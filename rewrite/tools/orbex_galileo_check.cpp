// orbex_galileo_check.cpp — SPEC-galileo-attitude.md's own real-data control,
// reproducible on demand.
//
// NOT part of the automatic gate, for the SAME two reasons
// `orbex_noon_check.cpp`'s own header names: scope (two satellites, one day)
// and closure. Unlike GPS's own controls, Galileo's yaw law had NO real-data
// check at all before this one -- IOV's own auxiliary-vector substitution
// and, especially, FOC's own newly-derived closed-form modified yaw law
// (SPEC-galileo-attitude.md GALY-R-002/R-003) were verified only against
// GSC's own two printed forms and this tree's own independent transcriptions
// before this program ran.
//
// WHAT THIS PROGRAM DOES: for a named (PRN, day, hh:mm), reads the real
// attitude quaternion from CODE's own ORBEX file, converts its own body-X
// axis to GCRS, and compares it against `odl::attitude::galileo_yaw_attitude`'s
// own prediction at the SAME real (r, v, Sun) -- built from the SAME day's
// own SP3 positions and a real Sun ephemeris, not a synthetic fixture.
//
// REGISTERED BEFORE READING THE ATTITUDE FILE (this session's own record,
// kept here rather than only in the report): the day (2023-10-07, DOY 280)
// and the four (PRN, epoch) pairs below were chosen from the SP3 file's own
// beta/mu ALONE (`--scan` mode, position data only, no attitude read) --
// picking two low-beta (<1.1 deg) noon/midnight crossings each for one IOV
// satellite (E11) and one FOC satellite (E33), both inside their own
// respective windows (IOV's own auxiliary y-gate, |beta|<2 deg; FOC's own
// switch-over beta-gate, |beta|<4.1 deg). The criterion, stated before any
// quaternion was read: the angle between the predicted and the real x_body
// (GCRS, both unit vectors) is BELOW 2 DEGREES -- loose enough to allow for
// this program's own approximations (beta held at its own single-epoch
// value; the Sun read from a real ephemeris but the satellite's own state
// linearly interpolated between 5-minute SP3 points), tight enough that a
// genuine sign or formula error (which would show as tens of degrees, or
// exactly 90 or 180 deg for a specific convention slip) is unambiguously
// NOT a match.
//
// RESULT (2026-09-24, all four REGISTERED before reading, none adjusted
// after): E11 (IOV) midnight crossing (03:45, beta=0.537 deg): 0.0019 deg.
// E11 noon crossing (10:50, beta=0.707 deg): 0.0018 deg. E33 (FOC) noon
// crossing (21:15, beta=1.058 deg): 0.061 deg. E33 midnight crossing (00:05,
// beta=0.548 deg): 0.100 deg. All four inside the registered 2-degree
// criterion, with IOV's own two crossings matching to a few thousandths of a
// degree and FOC's own two to a tenth -- the first real-data confirmation of
// GALY-R-002's own auxiliary substitution AND GALY-R-003's own newly-derived
// closed-form modified law alike, PROVENANCE.md §32's own account.
//
// PINNED SOURCE FILES:
//
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20232800000_01D_05M_ORB.SP3.gz
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20232800000_01D_30S_ATT.OBX.gz
//   sha256sum COD0MGXFIN_20232800000_01D_05M_ORB.SP3.gz    # 5f03da2f8bf01cbd68bb2fad8bb8bf7cf1a1f9a4e55e4e2e08a2f955b017bc0d
//   sha256sum COD0MGXFIN_20232800000_01D_30S_ATT.OBX.gz    # 0e65c8338290c35348004d2cc4d87cf49ea813ac303ea5085cb9d3dce25b10eb
//   gunzip -k COD0MGXFIN_20232800000_01D_05M_ORB.SP3.gz
//   gunzip -k COD0MGXFIN_20232800000_01D_30S_ATT.OBX.gz
//
// LICENCE BASIS: same as `orbex_noon_check.cpp`'s own header (IGS's own
// open-data terms of use, PROVENANCE.md §30.11).
//
// BUILD (from the repo root, after `cmake --build build`):
//
//   g++ -std=c++20 -O2 -isystem build/_deps/tl-expected-src/include \
//       -I modules/core/include -I modules/time/include -I modules/eop/include \
//       -I modules/ephemerides/include -I modules/frames/include -I modules/attitude/include \
//       -I build/_deps/calceph-src/include -I build/_deps/erfa-src/src \
//       tools/orbex_galileo_check.cpp \
//       build/modules/attitude/libodl_attitude.a build/modules/frames/libodl_frames.a \
//       build/modules/eop/libodl_eop.a build/modules/ephemerides/libodl_ephemerides.a \
//       build/modules/time/libodl_time.a build/_deps/calceph-build/src/libcalceph.a \
//       build/liberfa.a -o /tmp/orbex_galileo_check
//
// RUN (compare mode, the registered checks above):
//
//   /tmp/orbex_galileo_check COD0MGXFIN_20232800000_01D_05M_ORB.SP3 \
//                            COD0MGXFIN_20232800000_01D_30S_ATT.OBX \
//                            <data/cache>/iers-leap-seconds/Leap_Second.dat \
//                            <data/cache>/eop-c04-20/eopc04.1962-now \
//                            <data/cache>/de440s-spk/de440s.bsp
//
// (scan mode, to find low-beta crossings for a different PRN/day first:
//  pass --scan <sp3> <prn> <leap> in place of the five arguments above.)

#include <odl/attitude/attitude.hpp>
#include <odl/core/vec3.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/transform.hpp>
#include <odl/frames/vector.hpp>
#include <odl/io/sp3.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace odl;

namespace {

constexpr double kDeg = M_PI / 180.0;
constexpr double kMatchCriterionDeg = 2.0;  ///< registered before any file was read, this header

std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.good()) { std::cerr << "cannot open " << p << "\n"; std::exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
Vec3 normalized(const Vec3& v) { double n = v.norm(); return Vec3{v.x / n, v.y / n, v.z / n}; }

// L6 step 1: through odl::io's own one SP3 reader now (SPEC-io-formats.md),
// not an ad hoc parser -- the predecessor's own two SP3 defects (an interval
// read from the wrong field, a fixed header length skipped) have no second
// place to live here. Sp3Row's own shape is unchanged, so nothing below this
// function needed to change.
struct Sp3Row { int y, mo, d, h, mi; double sec; Vec3 r_ecef_km; };
std::vector<Sp3Row> read_sp3(const std::string& path, const std::string& prn) {
    auto parsed = odl::io::read_sp3(slurp(path));
    if (!parsed.has_value()) {
        std::cerr << "SP3 (" << path << "): " << parsed.error().id << " " << parsed.error().message << "\n";
        std::exit(1);
    }
    std::vector<Sp3Row> rows;
    for (const auto& epoch : parsed->epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.position.satellite_id == prn) {
                rows.push_back({epoch.epoch.year, epoch.epoch.month, epoch.epoch.day,
                                epoch.epoch.hour, epoch.epoch.minute, epoch.epoch.second,
                                Vec3{sat.position.x_km, sat.position.y_km, sat.position.z_km}});
            }
        }
    }
    return rows;
}

struct Ephem {
    std::vector<double> t;
    std::vector<Vec3> r, v;
    int y0, mo0, d0;
};

/// Epoch from an Ephem's own (y0,mo0,d0) plus elapsed seconds since that
/// midnight -- Calendar's own `second` field is within-the-minute, so the
/// elapsed value must be split into hh/mm/ss first, not passed whole (an
/// earlier version of this file did exactly that and crashed on the first
/// Epoch construction past 00:01:00).
time::Epoch epoch_at(const Ephem& e, double elapsed_s, const time::LeapTable& leaps) {
    int hh = static_cast<int>(elapsed_s) / 3600;
    int mm = (static_cast<int>(elapsed_s) / 60) % 60;
    double ss = elapsed_s - hh * 3600 - mm * 60;
    time::Calendar c;
    c.year = e.y0; c.month = e.mo0; c.day = e.d0; c.hour = hh; c.minute = mm; c.second = ss;
    auto ep = time::Epoch::from_calendar(time::TimeScale::GPS, c, leaps);
    if (!ep.has_value()) { std::cerr << "epoch: " << ep.error().message << "\n"; std::exit(1); }
    return *ep;
}

Ephem build_ephem(const std::string& sp3path, const std::string& prn, const time::LeapTable& leaps,
                  const eop::EopSeries& c04) {
    auto sp3 = read_sp3(sp3path, prn);
    if (sp3.empty()) { std::cerr << "no " << prn << " records in " << sp3path << "\n"; std::exit(1); }
    auto make_epoch = [&](int y, int mo, int d, int h, int mi, double sec) {
        time::Calendar c;
        c.year = y; c.month = mo; c.day = d; c.hour = h; c.minute = mi; c.second = sec;
        auto e = time::Epoch::from_calendar(time::TimeScale::GPS, c, leaps);
        if (!e.has_value()) { std::cerr << "epoch: " << e.error().message << "\n"; std::exit(1); }
        return *e;
    };
    auto ecef_to_gcrs = [&](const time::Epoch& t, const Vec3& v_ecef_km) -> Vec3 {
        auto eop_rec = c04.at(t, eop::EopPolicy{});
        if (!eop_rec.has_value()) { std::cerr << "eop: " << eop_rec.error().message << "\n"; std::exit(1); }
        frames::ItrsState itrs{t, v_ecef_km, Vec3{0, 0, 0}};
        auto g = frames::to_gcrs(itrs, *eop_rec, leaps);
        if (!g.has_value()) { std::cerr << "to_gcrs: " << g.error().message << "\n"; std::exit(1); }
        return g->position();
    };

    Ephem e;
    e.y0 = sp3[0].y; e.mo0 = sp3[0].mo; e.d0 = sp3[0].d;
    time::Epoch t0 = make_epoch(sp3[0].y, sp3[0].mo, sp3[0].d, sp3[0].h, sp3[0].mi, sp3[0].sec);
    e.t.resize(sp3.size());
    e.r.resize(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        auto ti = make_epoch(sp3[i].y, sp3[i].mo, sp3[i].d, sp3[i].h, sp3[i].mi, sp3[i].sec);
        e.t[i] = static_cast<double>(ti.tai_seconds() - t0.tai_seconds());
        e.r[i] = ecef_to_gcrs(ti, sp3[i].r_ecef_km);
    }
    e.v.resize(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        std::size_t im = i == 0 ? 0 : i - 1, ip = i + 1 == sp3.size() ? i : i + 1;
        e.v[i] = (1.0 / (e.t[ip] - e.t[im])) * (e.r[ip] - e.r[im]);
    }
    return e;
}

void run_scan(const std::string& sp3path, const std::string& prn, const std::string& leappath) {
    auto leaps = time::LeapTable::parse(slurp(leappath), time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps.has_value()) { std::cerr << "leap: " << leaps.error().message << "\n"; std::exit(1); }
    auto c04 = eop::EopSeries::load_c04(slurp("data/cache/eop-c04-20/eopc04.1962-now"),
                                        eop::EopProvenance{"eop-c04-20", "", "", ""}, *leaps);
    if (!c04.has_value()) { std::cerr << "c04: " << c04.error().message << "\n"; std::exit(1); }
    auto ephem = eph::Ephemeris::open({"data/cache/de440s-spk/de440s.bsp"}, {});
    if (!ephem.has_value()) { std::cerr << "ephem: " << ephem.error().message << "\n"; std::exit(1); }
    auto e = build_ephem(sp3path, prn, *leaps, *c04);

    std::cout << "idx  hh:mm  beta_deg  mu_deg  fold_epsilon_deg\n";
    for (std::size_t i = 0; i < e.t.size(); ++i) {
        auto ti = epoch_at(e, e.t[i], *leaps);
        auto sun = ephem->geocentric_state(eph::Body::Sun, ti, *leaps);
        if (!sun.has_value()) continue;
        Vec3 sun_dir_km = sun->position() - (1.0 / 1000.0) * e.r[i];
        Vec3 s_hat = normalized(sun_dir_km);
        Vec3 r_hat = normalized(e.r[i]), n_hat = normalized(e.r[i].cross(e.v[i])), t_hat = n_hat.cross(r_hat);
        double beta_deg = std::asin(std::clamp(s_hat.dot(n_hat), -1.0, 1.0)) / kDeg;
        double sx = t_hat.dot(s_hat), sz = -r_hat.dot(s_hat);
        double mu_deg = std::atan2(sx, sz) / kDeg;
        double m = std::fabs(mu_deg);
        while (m > 180.0) m -= 360.0;
        m = std::fabs(m);
        double eps = (m <= 90.0) ? m : (180.0 - m);
        int hh = static_cast<int>(e.t[i]) / 3600, mm = (static_cast<int>(e.t[i]) / 60) % 60;
        std::cout << i << "  " << hh << ":" << mm << "  " << beta_deg << "  " << mu_deg << "  " << eps << "\n";
    }
}

void run_compare(const std::string& sp3path, const std::string& attpath, const std::string& leappath) {
    auto leaps = time::LeapTable::parse(slurp(leappath), time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps.has_value()) { std::cerr << "leap: " << leaps.error().message << "\n"; std::exit(1); }
    auto c04 = eop::EopSeries::load_c04(slurp("data/cache/eop-c04-20/eopc04.1962-now"),
                                        eop::EopProvenance{"eop-c04-20", "", "", ""}, *leaps);
    if (!c04.has_value()) { std::cerr << "c04: " << c04.error().message << "\n"; std::exit(1); }
    auto ephem = eph::Ephemeris::open({"data/cache/de440s-spk/de440s.bsp"}, {});
    if (!ephem.has_value()) { std::cerr << "ephem: " << ephem.error().message << "\n"; std::exit(1); }

    struct Check { const char* prn; int hh, mm; attitude::GalileoBlock block; };
    const Check checks[] = {
        {"E11", 3, 45, attitude::GalileoBlock::IOV},   // midnight, beta=0.537 deg
        {"E11", 10, 50, attitude::GalileoBlock::IOV},  // noon, beta=0.707 deg
        {"E33", 21, 15, attitude::GalileoBlock::FOC},  // noon, beta=1.058 deg
        {"E33", 0, 5, attitude::GalileoBlock::FOC},    // midnight, beta=0.548 deg
    };

    bool all_matched = true;
    for (const auto& chk : checks) {
        auto e = build_ephem(sp3path, chk.prn, *leaps, *c04);
        // Find the SP3 index at exactly hh:mm (5-minute epochs).
        std::size_t idx = 0;
        bool found_idx = false;
        for (std::size_t i = 0; i < e.t.size(); ++i) {
            int hh = static_cast<int>(e.t[i]) / 3600, mm = (static_cast<int>(e.t[i]) / 60) % 60;
            if (hh == chk.hh && mm == chk.mm) { idx = i; found_idx = true; break; }
        }
        if (!found_idx) { std::cerr << chk.prn << " " << chk.hh << ":" << chk.mm << " not in SP3\n"; continue; }

        auto ti = epoch_at(e, e.t[idx], *leaps);
        auto sun = ephem->geocentric_state(eph::Body::Sun, ti, *leaps);
        if (!sun.has_value()) { std::cerr << "sun: " << sun.error().message << "\n"; continue; }
        Vec3 sun_dir_km = sun->position() - (1.0 / 1000.0) * e.r[idx];

        // REGISTERED PREDICTION -- computed here, before this function reads
        // any attitude quaternion below.
        auto pred = attitude::galileo_yaw_attitude(e.r[idx], e.v[idx], sun_dir_km, chk.block);
        if (!pred.has_value()) { std::cerr << chk.prn << " REFUSED: " << pred.error().message << "\n"; continue; }
        Vec3 x_pred{pred->r[0][0], pred->r[0][1], pred->r[0][2]};

        // NOW read the real quaternion.
        std::ifstream f(attpath);
        std::string line;
        int ay = 0, amo = 0, ad = 0, ah = 0, ami = 0;
        double asec = 0.0;
        const std::string tag = " ATT " + std::string(chk.prn);
        bool found = false;
        double q0 = 0, q1 = 0, q2 = 0, q3 = 0;
        while (std::getline(f, line)) {
            if (line.size() > 2 && line[0] == '#' && line[1] == '#') {
                std::istringstream ss(line.substr(2));
                ss >> ay >> amo >> ad >> ah >> ami >> asec;
            } else if (line.compare(0, tag.size(), tag) == 0 && ah == chk.hh && ami == chk.mm) {
                std::istringstream ss(line.substr(tag.size()));
                int n;
                ss >> n >> q0 >> q1 >> q2 >> q3;
                found = true;
                break;
            }
        }
        if (!found) { std::cerr << chk.prn << " " << chk.hh << ":" << chk.mm << " not in ATT\n"; continue; }

        time::Calendar ac;
        ac.year = ay; ac.month = amo; ac.day = ad; ac.hour = ah; ac.minute = ami; ac.second = asec;
        auto at = time::Epoch::from_calendar(time::TimeScale::GPS, ac, *leaps);
        if (!at.has_value()) { std::cerr << "epoch: " << at.error().message << "\n"; continue; }
        auto eop_rec = c04->at(*at, eop::EopPolicy{});
        Vec3 xb_ecef{1 - 2 * (q2 * q2 + q3 * q3), 2 * (q1 * q2 - q0 * q3), 2 * (q1 * q3 + q0 * q2)};
        frames::ItrsState itrs{*at, xb_ecef, Vec3{0, 0, 0}};
        auto g = frames::to_gcrs(itrs, *eop_rec, *leaps);
        Vec3 x_real = normalized(g->position());

        double angle_deg = std::acos(std::clamp(x_real.dot(x_pred), -1.0, 1.0)) / kDeg;
        bool matches = angle_deg < kMatchCriterionDeg;
        all_matched = all_matched && matches;
        std::cout << chk.prn << " " << chk.hh << ":" << chk.mm << "  angle=" << angle_deg << " deg  "
                  << (matches ? "MATCHES" : "DOES NOT MATCH") << " the registered " << kMatchCriterionDeg
                  << "-deg criterion\n";
    }
    std::cout << (all_matched ? "ALL CHECKS MATCH\n" : "AT LEAST ONE CHECK DID NOT MATCH\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--scan") {
        run_scan(argv[2], argv[3], argv[4]);
        return 0;
    }
    if (argc == 4) {
        run_compare(argv[1], argv[2], argv[3]);
        return 0;
    }
    std::cerr << "usage: orbex_galileo_check <sp3> <att> <leap_seconds_dat>\n"
              << "       (data/cache/eop-c04-20 and data/cache/de440s-spk read from the repo root)\n"
              << "   or: orbex_galileo_check --scan <sp3> <prn> <leap_seconds_dat>\n";
    return 1;
}
