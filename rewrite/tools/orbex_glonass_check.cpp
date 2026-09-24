// orbex_glonass_check.cpp — SPEC-glonass-attitude.md's own real-data
// control, reproducible on demand.
//
// NOT part of the automatic gate, the SAME two reasons `orbex_galileo_
// check.cpp`'s own header names: scope (a handful of satellites, one day)
// and closure. GLONASS-M's own yaw law (DIL11's shadow-crossing and
// noon-turn maneuvers) had NO real-data check at all before this one --
// verified only against an independent transcription of DIL11's own
// equations before this program ran.
//
// WHAT THIS PROGRAM DOES: for a named (PRN, day, hh:mm), reads the real
// attitude quaternion from CODE's own ORBEX file, converts its own body-X
// axis to GCRS, and compares it against `odl::attitude::glonass_m_yaw_
// attitude`'s own prediction at the SAME real (r, v, Sun) -- built from the
// SAME day's own SP3 positions and a real Sun ephemeris, not a synthetic
// fixture. The SAME pinned CODE MGEX files `orbex_galileo_check.cpp` uses
// (multi-GNSS: GPS, GLONASS, Galileo, BeiDou, QZSS all in the one file
// pair) -- no separate fetch needed.
//
// REGISTERED BEFORE READING THE ATTITUDE FILE (this session's own record,
// kept here rather than only in the report): `--scan` mode (position data
// only, no attitude read) was used first against `orbex_galileo_check.cpp`'s
// own day (2023-10-07), which turned out to have NO GLONASS-M satellite
// below beta=18.5deg all day -- outside even the shadow turn's own
// eclipse-season window (|beta|<14.2deg), so no turn is active for any
// satellite that day at all. Two further days were scanned the same way
// (position only) before any attitude file was touched: 2023-09-23 (DOY
// 266, nearer the equinox, minimum beta ~9.25deg -- inside the shadow
// window but not the noon-turn's own tighter one, beta_0=2.03deg) and
// 2023-09-09 (DOY 252), which found R19 and R20 both crossing beta
// essentially 0 -- DIL11's own "sharp midnight-turn" case, Sec.4.1 -- and
// so inside BOTH turns' own windows. The criterion, stated before any
// quaternion was read: the angle between the predicted and the real x_body
// (GCRS, both unit vectors) is BELOW 3 DEGREES -- looser than Galileo's own
// 2-degree criterion (`orbex_galileo_check.cpp`), because GLONASS-M's own
// turns are FASTER and more ABRUPT (a 0.25 deg/s hardware rate producing a
// full 180-degree swing in 12 minutes, against Galileo's own much gentler
// 5656-second cosine ramp), so this program's own approximations (beta held
// at its own single-epoch value; the satellite's own state linearly
// interpolated between 5-minute SP3 points) were expected to cost more
// precision here.
//
// RESULT (2026-09-24, all four REGISTERED before reading, none adjusted
// after, DOY 252/2023-09-09): R20 shadow-crossing (06:10, mu=0.88deg,
// beta=-0.045deg): 0.0007deg. R20 noon turn (11:45, mu=179.14deg,
// beta=0.116deg): 0.026deg. R19 shadow-crossing (04:35, mu=3.20deg,
// beta=0.0091deg): 0.0008deg. R19 noon turn (10:00, mu=176.18deg,
// beta=0.166deg): 0.0022deg. All four far inside the registered 3-degree
// criterion, matching to hundredths of a degree or better -- the loose
// criterion this header's own reasoning above anticipated was not needed:
// the FIRST real-data confirmation either DIL11 mechanism (ramp-then-hold
// shadow-crossing, and the four-iteration noon-turn onset) has had, for two
// independent satellites each, PROVENANCE.md §33's own account.
//
// PINNED SOURCE FILES: the SAME day and file pair `orbex_galileo_check.cpp`
// covers is NOT what this control uses (see above) -- this control's own
// day is 2023-09-09 (DOY 252):
//
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20232520000_01D_05M_ORB.SP3.gz
//   curl -sO https://zhw-b.s3.cloud.switch.ch/aiub/CODE_MGEX/CODE/2023/COD0MGXFIN_20232520000_01D_30S_ATT.OBX.gz
//   gunzip -k COD0MGXFIN_20232520000_01D_05M_ORB.SP3.gz
//   gunzip -k COD0MGXFIN_20232520000_01D_30S_ATT.OBX.gz
//
// LICENCE BASIS: same as `orbex_galileo_check.cpp`'s own header (IGS's own
// open-data terms of use, PROVENANCE.md §30.11).
//
// BUILD (from the repo root, after `cmake --build build`):
//
//   g++ -std=c++20 -O2 -isystem build/_deps/tl-expected-src/include \
//       -I modules/core/include -I modules/time/include -I modules/eop/include \
//       -I modules/ephemerides/include -I modules/frames/include -I modules/attitude/include \
//       -I build/_deps/calceph-src/include -I build/_deps/erfa-src/src \
//       tools/orbex_glonass_check.cpp \
//       build/modules/attitude/libodl_attitude.a build/modules/frames/libodl_frames.a \
//       build/modules/eop/libodl_eop.a build/modules/ephemerides/libodl_ephemerides.a \
//       build/modules/time/libodl_time.a build/_deps/calceph-build/src/libcalceph.a \
//       build/liberfa.a -o /tmp/orbex_glonass_check
//
// RUN (compare mode):
//
//   /tmp/orbex_glonass_check COD0MGXFIN_20232800000_01D_05M_ORB.SP3 \
//                            COD0MGXFIN_20232800000_01D_30S_ATT.OBX \
//                            <data/cache>/iers-leap-seconds/Leap_Second.dat \
//                            <data/cache>/eop-c04-20/eopc04.1962-now \
//                            <data/cache>/de440s-spk/de440s.bsp
//
// (scan mode: --scan <sp3> <prn> <leap> in place of the five arguments.)

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
constexpr double kMatchCriterionDeg = 3.0;  ///< registered before any file was read, this header

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
/// file for the first time (`orbex_galileo_check.cpp`/`doris_jason_check.cpp`
/// already carry the identical fix, for the identical reason): a daily
/// product's own LAST epoch is commonly stamped at the NEXT day's
/// 00:00:00:00 (a closing bookend sample), and the new velocity computation
/// below is the first caller to evaluate this function there --
/// `elapsed_s=86400` decomposed to "hour 24", refused by `Epoch::
/// from_calendar`. `Epoch::add` needs no calendar decomposition at all, so
/// no day boundary is a special case.
time::Epoch epoch_at(const Ephem& e, double elapsed_s, const time::LeapTable&) {
    return e.t0.add(time::Duration::from_seconds(elapsed_s));
}

// L6 step 1's own SP3-interpolation round (plan/subplan_L6/L6-1.md, ruled
// 2026-09-25): position at each real sample is read through
// `io::Sp3Ephemeris::position_km_at`, exact at its own nodes by
// construction, so it reproduces the OLD direct-from-`sp3[i]` value bit for
// bit -- the change that matters is velocity, now a central difference of
// the SAME smooth interpolant over `io::kVelocityStepS` (1 s, not the up to
// several minutes between two real neighbouring samples), each side
// transformed to GCRS INDIVIDUALLY before differencing -- transforming a
// difference is not the same as differencing a transform, since GCRS is a
// time-dependent rotation of ECEF. One-sided at the array's own first and
// last sample, where the far side would fall outside the sampled span.
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

    std::cout << "idx  hh:mm  beta_deg  mu_deg\n";
    for (std::size_t i = 0; i < e.t.size(); ++i) {
        auto ti = epoch_at(e, e.t[i], *leaps);
        auto sun = ephem->geocentric_state(eph::Body::Sun, ti, *leaps);
        if (!sun.has_value()) continue;
        Vec3 sun_dir_km = sun->position() - (1.0 / 1000.0) * e.r[i];
        Vec3 s_hat = normalized(sun_dir_km);
        Vec3 r_hat = normalized(e.r[i]), n_hat = normalized(e.r[i].cross(e.v[i])), t_hat = n_hat.cross(r_hat);
        double beta_deg = std::asin(std::clamp(s_hat.dot(n_hat), -1.0, 1.0)) / kDeg;
        // this tree's own mu (KOUBA09-true, from midnight), matching
        // attitude.cpp's own mu_rad -- NEGATED atan2, the same fix that
        // function's own header records.
        Vec3 u_midnight = -1.0 * normalized(s_hat - s_hat.dot(n_hat) * n_hat);
        double mu_deg = -std::atan2(t_hat.dot(u_midnight), r_hat.dot(u_midnight)) / kDeg;
        int hh = static_cast<int>(e.t[i]) / 3600, mm = (static_cast<int>(e.t[i]) / 60) % 60;
        std::cout << i << "  " << hh << ":" << mm << "  " << beta_deg << "  " << mu_deg << "\n";
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

    struct Check { const char* prn; int hh, mm; };
    // REGISTERED via --scan (position data only, no attitude read) on
    // 2023-09-09 (DOY 252), a deep GLONASS eclipse-season day: R19/R20 both
    // cross beta essentially 0 (DIL11's own "sharp midnight-turn" case).
    const Check checks[] = {
        {"R20", 6, 10},    // mu=0.88deg, beta=-0.045deg -- shadow-crossing (midnight)
        {"R20", 11, 45},   // mu=179.14deg, beta=0.116deg -- noon-turn
        {"R19", 4, 35},    // mu=3.20deg, beta=0.0091deg -- shadow-crossing, 2nd satellite
        {"R19", 10, 0},    // mu=176.18deg, beta=0.166deg -- noon-turn, 2nd satellite
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
        auto pred = attitude::glonass_m_yaw_attitude(e.r[idx], e.v[idx], sun_dir_km);
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
    std::cerr << "usage: orbex_glonass_check <sp3> <att> <leap_seconds_dat>\n"
              << "       (data/cache/eop-c04-20 and data/cache/de440s-spk read from the repo root)\n"
              << "   or: orbex_glonass_check --scan <sp3> <prn> <leap_seconds_dat>\n";
    return 1;
}
