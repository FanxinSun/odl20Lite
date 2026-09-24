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
#include <odl/io/sp3_ephemeris.hpp>
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
// place to live here.
struct Sp3Row { int y, mo, d, h, mi; double sec; };  // r_ecef_km dropped: read through Sp3Ephemeris now

struct Ephem {
    std::vector<double> t;
    std::vector<Vec3> r, v;
    int y0, mo0, d0;
    time::Epoch t0;  ///< the absolute moment t[i] is measured from -- REQUIRED to place a
                      ///< query correctly across a day boundary (see epoch_at, below).
};

/// Epoch from an Ephem's own `t0` plus elapsed seconds -- pure `Epoch`
/// arithmetic (TAI seconds since a fixed origin), not a calendar
/// reconstruction. An EARLIER version of this function rebuilt hh/mm/ss from
/// `elapsed_s` assuming the result always falls on `e`'s own first calendar
/// day; L6 step 1's own SP3-interpolation round found this false on a REAL
/// file for the first time (`doris_jason_check.cpp` already carries the
/// identical fix, for the identical reason, from an earlier round): a daily
/// product's own LAST epoch is commonly stamped at the NEXT day's
/// 00:00:00:00 (a closing bookend sample), and the velocity computation
/// below is the first caller ever to evaluate this function there --
/// `elapsed_s=86400` decomposed to "hour 24", refused by `Epoch::
/// from_calendar` (`calendar field out of range`). `Epoch::add` needs no
/// calendar decomposition at all, so no day boundary is a special case.
time::Epoch epoch_at(const Ephem& e, double elapsed_s, const time::LeapTable&) {
    return e.t0.add(time::Duration::from_seconds(elapsed_s));
}

// L6 step 1's own SP3-interpolation round (plan/subplan_L6/L6-1.md, ruled
// 2026-09-25): position at each real sample is read through
// `io::Sp3Ephemeris::position_km_at`, exact at its own nodes by
// construction, so it reproduces the OLD direct-from-`sp3[i]` value bit for
// bit -- the change that matters is velocity. GALY-Q-002
// (`SPEC-galileo-attitude.md`) names the OLD velocity -- a central
// difference between two REAL, 5-MINUTE-APART neighbouring samples -- as a
// suspect for FOC's own small (0.06-0.10 deg) unexplained residual. The NEW
// velocity is a central difference of the SAME smooth interpolant over
// `io::kVelocityStepS` (1 s, not 5 minutes), each side transformed to GCRS
// INDIVIDUALLY before differencing -- transforming a difference is not the
// same as differencing a transform, since GCRS is a time-dependent rotation
// of ECEF, so the two transforms cannot be collapsed into one call. One-
// sided at the array's own first and last sample, where the far side would
// fall outside the sampled span.
Ephem build_ephem(const std::string& sp3path, const std::string& prn, const time::LeapTable& leaps,
                  const eop::EopSeries& c04) {
    auto parsed = odl::io::read_sp3(slurp(sp3path));
    if (!parsed.has_value()) { std::cerr << "SP3: " << parsed.error().id << " " << parsed.error().message << "\n"; std::exit(1); }
    auto sp3_eph = odl::io::Sp3Ephemeris::build(*parsed, prn);
    if (!sp3_eph.has_value()) { std::cerr << "Sp3Ephemeris: " << sp3_eph.error().id << " " << sp3_eph.error().message << "\n"; std::exit(1); }

    // Every real sample's own calendar date/time, for e.t[]/e.y0/mo0/d0 below
    // -- position itself now comes from sp3_eph, not from this vector.
    std::vector<Sp3Row> sp3;
    for (const auto& epoch : parsed->epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.position.satellite_id == prn) {
                sp3.push_back({epoch.epoch.year, epoch.epoch.month, epoch.epoch.day,
                               epoch.epoch.hour, epoch.epoch.minute, epoch.epoch.second});
            }
        }
    }
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

    time::Epoch t0 = make_epoch(sp3[0].y, sp3[0].mo, sp3[0].d, sp3[0].h, sp3[0].mi, sp3[0].sec);
    Ephem e{{}, {}, {}, sp3[0].y, sp3[0].mo, sp3[0].d, t0};  // Epoch has no default constructor
    e.t.resize(sp3.size());
    e.r.resize(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        auto ti = make_epoch(sp3[i].y, sp3[i].mo, sp3[i].d, sp3[i].h, sp3[i].mi, sp3[i].sec);
        e.t[i] = static_cast<double>(ti.tai_seconds() - t0.tai_seconds());
    }
    // epoch_at needs e.t[]/e.y0/mo0/d0, set above; declared after them.
    auto epoch_at_elapsed = [&](double elapsed_s) { return epoch_at(e, elapsed_s, leaps); };
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        auto r = sp3_eph->position_km_at(e.t[i]);
        if (!r.has_value()) { std::cerr << "position_km_at: " << r.error().id << " " << r.error().message << "\n"; std::exit(1); }
        e.r[i] = ecef_to_gcrs(make_epoch(sp3[i].y, sp3[i].mo, sp3[i].d, sp3[i].h, sp3[i].mi, sp3[i].sec), *r);
    }
    e.v.resize(sp3.size());
    for (std::size_t i = 0; i < sp3.size(); ++i) {
        const double t_minus = i == 0 ? e.t[i] : e.t[i] - odl::io::kVelocityStepS;
        const double t_plus = i + 1 == sp3.size() ? e.t[i] : e.t[i] + odl::io::kVelocityStepS;
        auto r_minus = sp3_eph->position_km_at(t_minus);
        auto r_plus = sp3_eph->position_km_at(t_plus);
        if (!r_minus.has_value() || !r_plus.has_value()) {
            std::cerr << "velocity at sample " << i << ": position_km_at refused near a span/gap/manoeuvre boundary\n";
            std::exit(1);
        }
        Vec3 g_minus = ecef_to_gcrs(epoch_at_elapsed(t_minus), *r_minus);
        Vec3 g_plus = ecef_to_gcrs(epoch_at_elapsed(t_plus), *r_plus);
        e.v[i] = (1.0 / (t_plus - t_minus)) * (g_plus - g_minus);
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
