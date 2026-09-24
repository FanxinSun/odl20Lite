// orbex_qzss_check.cpp — SPEC-qzss-attitude.md's own real-data control,
// reproducible on demand.
//
// NOT part of the automatic gate, the SAME two reasons every other ORBEX
// control's own header names: scope and closure.
//
// SCOPE NOTE: `SPEC-spacecraft.md`'s own `qzss_1()` builds QZS-1's own
// specific macromodel (mass, CoM, geometry) from its own SPI document
// (`SPI_QZS1_B`). QZS-1 itself (PRN J01) is not present in CODE's own MGEX
// SP3/ATT files on the days checked -- decommissioned/replaced by QZS-1R
// by 2023, per this session's own scan. The ATTITUDE LAW and FRAME MAPPING
// this program checks (`odl::attitude::qzss_yaw_attitude`,
// `odl::spacecraft::qzss_frame_from_native`) are a DIFFERENT, constellation
// -wide fact from one satellite's own geometry -- QZS-1's own SPI document
// states the law and frame convention for the QZSS bus family generally,
// not as a QZS-1-specific idiosyncrasy, so this control uses QZS-2 (J02)
// and QZS-4 (J04), the two other IGSO-type QZSS satellites present in the
// SAME pinned CODE files, to check that shared law and mapping against
// real data -- NOT to validate QZS-1's own specific mass/CoM/geometry,
// which this program does not touch. QZS-3 (J03, GEO, a different orbit
// type from QZS-1/2/4's own IGSO) is deliberately NOT used here, to keep
// the comparison to satellites of the SAME orbit family QZS-1 itself flies.
//
// WHAT THIS PROGRAM DOES: for a named (PRN, day, hh:mm), reads the real
// attitude quaternion from CODE's own ORBEX file, converts its own body-X
// axis to GCRS, and compares it against `odl::attitude::qzss_yaw_attitude`'s
// own prediction at the SAME real (r, v, Sun).
//
// REGISTERED BEFORE READING THE ATTITUDE FILE: `--scan` mode (position data
// only) was run across FOUR days -- the three already pinned for the
// GLONASS control (2023-10-07, 2023-09-23, 2023-09-09) plus one more
// (2023-12-16, chosen to sample a different point in QZSS's own slow beta
// cycle) -- for J02, J03 and J04. QZSS's own beta moves far more slowly
// than GLONASS's own (a geosynchronous orbit, not a ~11h16m MEO cycle), so
// a satellite at a given beta regime stays there for entire days at a
// time, but NO crossing below beta=20deg was found for J02 or J04 (the two
// OTHER IGSO-type satellites besides QZS-1) on any of the four days --
// closest was 21.7deg (J02, 2023-12-16) -- so this control checks
// YAW-STEERING MODE ONLY; the orbit-normal mode's own real-data check was
// SOUGHT, NOT COMPLETED (`SPEC-qzss-attitude.md` §10's own open question),
// relying instead on the mode's own tight algebraic verification
// (`QZSY-A-001`, an independent reconstruction proving both the
// construction and its own right-handedness). The criterion, stated before
// any quaternion was read: the angle between predicted and real x_body is
// BELOW 2 DEGREES (GPS's/Galileo's own tightness, since QZSS's own
// yaw-steering mode reduces to the SAME `nominal_yaw_steering` law those
// already use).
//
// RESULT (2026-09-24, all four REGISTERED before reading, none adjusted
// after, 2023-10-07/DOY 280): J02 00:00 (beta=31.4deg): 0.00004deg. J02
// 12:00 (beta=31.1deg): 0.00003deg. J04 00:00 (beta=-40.3deg): 0.00019deg.
// J04 12:00 (beta=-40.4deg): 0.00018deg. All four far inside the
// registered criterion, an order of magnitude tighter than GPS's/Galileo's
// own best real-data agreement -- strong confirmation that the 180-about-Z
// frame mapping (`odl::spacecraft::qzss_frame_from_native`, hypothesised
// from SPI_QZS1_B's own stated Sun-hemisphere property and structural
// analogy to Galileo's own case) is correct, for two independent
// satellites, PROVENANCE.md §33's own account.
//
// PINNED SOURCE FILES: the SAME CODE MGEX day `orbex_glonass_check.cpp`'s
// own sibling day pins (2023-10-07, the ORIGINAL day this whole session's
// own Galileo control also used), read from the same already-fetched local
// copy -- not repeated here.
//
// BUILD (from the repo root, after `cmake --build build`):
//
//   g++ -std=c++20 -O2 -isystem build/_deps/tl-expected-src/include \
//       -I modules/core/include -I modules/time/include -I modules/eop/include \
//       -I modules/ephemerides/include -I modules/frames/include -I modules/attitude/include \
//       -I build/_deps/calceph-src/include -I build/_deps/erfa-src/src \
//       tools/orbex_qzss_check.cpp \
//       build/modules/attitude/libodl_attitude.a build/modules/frames/libodl_frames.a \
//       build/modules/eop/libodl_eop.a build/modules/ephemerides/libodl_ephemerides.a \
//       build/modules/time/libodl_time.a build/_deps/calceph-build/src/libcalceph.a \
//       build/liberfa.a -o /tmp/orbex_qzss_check
//
// RUN (compare mode): same argument shape as `orbex_glonass_check.cpp`.
// (scan mode: --scan <sp3> <prn> <leap> in place of the three arguments.)

#include <odl/attitude/attitude.hpp>
#include <odl/core/vec3.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/transform.hpp>
#include <odl/frames/vector.hpp>
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

struct Sp3Row { int y, mo, d, h, mi; double sec; Vec3 r_ecef_km; };
std::vector<Sp3Row> read_sp3(const std::string& path, const std::string& prn) {
    std::vector<Sp3Row> rows;
    std::ifstream f(path);
    std::string line;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    double sec = 0.0;
    const std::string tag = "P" + prn;
    while (std::getline(f, line)) {
        if (line.size() > 1 && line[0] == '*') {
            std::istringstream ss(line.substr(1));
            ss >> y >> mo >> d >> h >> mi >> sec;
        } else if (line.compare(0, tag.size(), tag) == 0) {
            std::istringstream ss(line.substr(tag.size()));
            double x, yy, z, clk;
            ss >> x >> yy >> z >> clk;
            rows.push_back({y, mo, d, h, mi, sec, Vec3{x, yy, z}});
        }
    }
    return rows;
}

struct Ephem {
    std::vector<double> t;
    std::vector<Vec3> r, v;
    int y0, mo0, d0;
};

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

    std::cout << "idx  hh:mm  beta_deg\n";
    for (std::size_t i = 0; i < e.t.size(); ++i) {
        auto ti = epoch_at(e, e.t[i], *leaps);
        auto sun = ephem->geocentric_state(eph::Body::Sun, ti, *leaps);
        if (!sun.has_value()) continue;
        Vec3 sun_dir_km = sun->position() - (1.0 / 1000.0) * e.r[i];
        Vec3 s_hat = normalized(sun_dir_km);
        Vec3 n_hat = normalized(e.r[i].cross(e.v[i]));
        double beta_deg = std::asin(std::clamp(s_hat.dot(n_hat), -1.0, 1.0)) / kDeg;
        int hh = static_cast<int>(e.t[i]) / 3600, mm = (static_cast<int>(e.t[i]) / 60) % 60;
        std::cout << i << "  " << hh << ":" << mm << "  " << beta_deg << "\n";
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

    struct Check { const char* prn; int hh, mm; const char* label; };
    // REGISTERED via --scan (position data only) across four days spanning
    // most of a year (2023-10-07, 09-23, 09-09, 12-16): QZSS's own beta
    // moves far more slowly than GLONASS's own (geosynchronous, not a fast
    // MEO cycle), and J02/J04 (the two OTHER IGSO-type QZSS satellites
    // besides QZS-1) never dropped below beta=21.7deg on any of the four --
    // no orbit-normal-mode crossing was found for an IGSO satellite in the
    // days checked (this header's own "not completed" note). All four
    // checks below are yaw-steering-mode crossings instead, at two
    // satellites and two epochs each, 2023-10-07 (DOY 280).
    const Check checks[] = {
        {"J02", 0, 0, "yaw-steering, beta=31.4deg"},
        {"J02", 12, 0, "yaw-steering, beta=31.1deg"},
        {"J04", 0, 0, "yaw-steering, beta=-40.3deg"},
        {"J04", 12, 0, "yaw-steering, beta=-40.4deg"},
    };

    bool all_matched = true;
    for (const auto& chk : checks) {
        auto e = build_ephem(sp3path, chk.prn, *leaps, *c04);
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
        auto pred = attitude::qzss_yaw_attitude(e.r[idx], e.v[idx], sun_dir_km);
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
        std::cout << chk.prn << " " << chk.hh << ":" << chk.mm << " (" << chk.label << ")  angle="
                  << angle_deg << " deg  " << (matches ? "MATCHES" : "DOES NOT MATCH")
                  << " the registered " << kMatchCriterionDeg << "-deg criterion\n";
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
    std::cerr << "usage: orbex_qzss_check <sp3> <att> <leap_seconds_dat>\n"
              << "   or: orbex_qzss_check --scan <sp3> <prn> <leap_seconds_dat>\n";
    return 1;
}
