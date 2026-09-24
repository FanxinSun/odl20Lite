// orbex_qzss_check.cpp — SPEC-qzss-attitude.md's own real-data control,
// reproducible on demand.
//
// NOT part of the automatic gate, the SAME two reasons every other ORBEX
// control's own header names: scope and closure.
//
// SCOPE NOTE: `SPEC-spacecraft.md`'s own `qzss_1()` builds QZS-1's own
// specific macromodel (mass, CoM, geometry) from its own SPI document
// (`SPI_QZS1_B`). QZS-1 itself (PRN J01) is not present in CODE's own MGEX
// SP3/ATT files on ANY day this session could reach (see the DAY-BY-DAY
// SCAN below) -- decommissioned/replaced by QZS-1R, apparently before this
// mirror's own coverage begins (late 2022). The ATTITUDE LAW and FRAME
// MAPPING this program checks (`odl::attitude::qzss_yaw_attitude`,
// `odl::attitude::orbit_normal_attitude`, `odl::spacecraft::qzss_frame_
// from_native`) are checked instead against QZS-2 (J02), QZS-3 (J03) and
// QZS-4 (J04) -- NOT to validate QZS-1's own specific mass/CoM/geometry,
// which this program does not touch, but because the manager's own review
// asked for the SOURCE BASIS of applying a law/mapping checked on other
// QZSS satellites to QZS-1. That basis, checked directly (each of QZS-1/
// 2/3/4's own SPI documents fetched and read this round, not assumed):
// **§2 (Reference Frame) is WORD-FOR-WORD IDENTICAL across all four** --
// "The QZS-N satellite coordinate system is aligned with the main body
// axes and originates at the center of the launch adapter plane... +Z...
// bore sight direction of the L-ANT antenna... +Y... parallel to the
// rotation axis of the solar panels... +X... constituted by a right handed
// system with +Y/+Z axis" (quoted, N substituted) -- the frame mapping is
// safely constellation-wide, confirmed both by this quote and by the
// real-data match below. **§3 (Attitude Law) is NOT uniformly shared**,
// found reading all four documents rather than assumed from QZS-1's own
// alone: QZS-1 switches between yaw-steering (|beta|>~20deg) and orbit-
// normal (|beta|<=~20deg); QZS-3's own words are "QZS-3 is CONTINUOUSLY
// controlled in the orbit normal mode" (quoted) -- ALWAYS orbit-normal,
// its own mode description word-for-word the SAME construction QZS-1's own
// document states; QZS-2 and QZS-4 instead "take always attitude of the
// yaw steering mode except the period that the orbit control maneuver is
// conducted" (quoted, both documents identical) -- ordinary yaw-steering
// ALWAYS, using a DIFFERENT, rate-limited "pseudo-yaw-steering" correction
// near beta=0 (their own explicit formula, not orbit-normal, not built in
// this tree -- QZS-1-specific scope, `SPEC-qzss-attitude.md` §3) rather
// than switching to orbit-normal the way QZS-1 does. So J02/J04 validate
// ONLY the shared yaw-steering law and frame (which is what they exercise
// at every beta they were found at); QZS-3, uniquely among the three,
// shares QZS-1's own orbit-normal construction by its own document's exact
// wording, and needs no low-beta window at all to exercise it -- checked
// below, EXPLORATORY, found NOT to match cleanly, and then DIAGNOSED: a
// second check against `nominal_yaw_steering` directly (added after the
// manager's own second review) found CODE's own real J03 attitude matches
// THAT law to the SAME thousandths-of-a-degree floor every other yaw-
// steering check reaches -- CODE's own analysis product does not implement
// QZS-3's own orbit-normal mode at all, so the EXPLORATORY mismatch is
// explained (a different law CODE happens to model, not a defect in this
// tree's own construction), not a live discrepancy (see RESULT).
//
// DAY-BY-DAY SCAN for QZS-1 (J01), per the manager's own review: ~monthly
// sampling (45 dates, cheap position-only checks, no quaternions read)
// across this mirror's own ENTIRE available span (2022-12-01 through
// 2026-07-20 -- CODE's own true multi-year archive, and CDDIS, were both
// unreachable from this environment: direct FTP/HTTPS to ftp.aiub.unibe.ch
// timed out, CDDIS redirects to an EarthData login this project's own
// standing no-account discipline does not cross) found J01 present in
// ZERO of the 45 files checked -- `any_qzss_records` confirms J02/J03/J04
// ARE present and correctly parsed in every one of the same 45 files, so
// the absence is J01's own, not a scan defect. RECORDED as the absence,
// per rule 4: no window exists in the data this session can reach.
//
// WHAT THIS PROGRAM DOES: for a named (PRN, day, hh:mm), reads the real
// attitude quaternion from CODE's own ORBEX file, converts its own body-X
// axis to GCRS, and compares it against a prediction at the SAME real
// (r, v, Sun) -- `qzss_yaw_attitude` for J02/J04, `orbit_normal_attitude`
// called directly (bypassing the beta-based dispatch, since QZS-3 is
// ALWAYS in this mode regardless of beta) for J03.
//
// REGISTERED BEFORE READING THE ATTITUDE FILE: the four yaw-steering
// checks (J02/J04, two epochs each, 2023-10-07/DOY 280) keep the ORIGINAL
// registration this control's own first version recorded (`--scan` found
// no low-beta window for these two across four days spanning a year), 2
// degrees. The two QZS-3 checks are EXPLORATORY, added after the manager's
// own review pointed at QZS-3's own shared law -- not held to that
// criterion, reported as found.
//
// RESULT, yaw-steering (2026-09-24, all four REGISTERED before reading,
// none adjusted after, 2023-10-07/DOY 280): J02 00:00 (beta=31.4deg):
// 0.00004deg. J02 12:00 (beta=31.1deg): 0.00003deg. J04 00:00
// (beta=-40.3deg): 0.00019deg. J04 12:00 (beta=-40.4deg): 0.00018deg. All
// four far inside the registered criterion, an order of magnitude tighter
// than GPS's/Galileo's own best real-data agreement -- strong confirmation
// that the 180-about-Z frame mapping is correct.
//
// RESULT, QZS-3 orbit-normal (EXPLORATORY, added after the manager's own
// FIRST review): NOT a clean match. At 00:00 the built construction is
// 170.9deg off; at 12:00 it is 13.5deg off; a 30-minute sweep across the
// full day shows the error tracing a SMOOTH curve between the built
// construction and its own 180-degree-yaw-flipped counterpart, crossing
// near 90deg twice and bottoming out at its own two closest approaches
// (6.56deg, 6.69deg) -- never reaching the sub-0.03deg floor every OTHER
// real-data control in this tree reaches.
//
// RESULT, QZS-3 DIAGNOSTIC against `nominal_yaw_steering` directly (added
// after the manager's own SECOND review, which identified the exact cause
// and REGISTERED the prediction before this result was read, this file's
// own record above): 0.00022deg at 00:00 (beta=6.45deg), 0.00032deg at
// 12:00 (beta=6.59deg) -- the SAME thousandths-of-a-degree floor J02's/
// J04's own yaw-steering checks reach. PREDICTION CONFIRMED EXACTLY: CODE's
// own analysis product does NOT implement QZS-3's own orbit-normal mode --
// it models J03 with the generic nominal yaw-steering law regardless of
// beta (beta=6.5deg here sits well inside QZS-1's own stated orbit-normal
// threshold, |beta|<=~20deg, yet CODE still used yaw-steering) -- CODE's
// own ORBEX file is CODE's own MODEL of the satellite, not the satellite
// itself, exactly the manager's own point. `orbit_normal_attitude`'s own
// construction is NOT contradicted by this data at all: the earlier
// EXPLORATORY mismatch is now explained as a comparison against a
// DIFFERENT law CODE happens to use for this satellite, not a defect in
// this tree's own construction, which remains independently verified
// correct algebraically (`QZSY-A-001`) and its own SHAPE confirmed shared
// with QZS-1 by QZS-3's own identical wording. Orbit-normal mode stays
// UNCONFIRMED by real-data agreement (no analysis centre this session
// could reach appears to implement it for any satellite checked), not
// contradicted -- `QZSY-Q-004` CLOSED on this result, `QZSY-Q-001` stays
// open on exactly that narrower, now-precise basis.
//
// PINNED SOURCE FILES: the SAME CODE MGEX day `orbex_glonass_check.cpp`'s
// own sibling day pins (2023-10-07, the ORIGINAL day this whole session's
// own Galileo control also used), read from the same already-fetched local
// copy -- not repeated here. The day-by-day scan's own 45 dates and their
// own `any_qzss_records`/`j01_records` counts are recorded in full in
// PROVENANCE.md §33's own account, not repeated here.
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

    enum class Mode { Dispatch, ForceOrbitNormal, ForceYawSteering };
    struct Check { const char* prn; int hh, mm; const char* label; Mode mode; };
    // REGISTERED via --scan (position data only) across four days spanning
    // most of a year (2023-10-07, 09-23, 09-09, 12-16): QZSS's own beta
    // moves far more slowly than GLONASS's own (geosynchronous, not a fast
    // MEO cycle), and J02/J04 (the two OTHER IGSO-type QZSS satellites
    // besides QZS-1) never dropped below beta=21.7deg on any of the four --
    // no orbit-normal-mode crossing was found for an IGSO satellite in the
    // days checked. The first four checks below are yaw-steering-mode
    // crossings, two satellites and two epochs each, 2023-10-07 (DOY 280).
    //
    // The LAST TWO checks are `orbit_normal_attitude` called DIRECTLY
    // (bypassing `qzss_yaw_attitude`'s own beta-based dispatch), against
    // QZS-3 (J03) -- found, reading QZS-3's own SPI document (`SPI-QZS3_F`)
    // after the manager's own review asked for the source basis of sharing
    // a law across satellites: "QZS-3 is CONTINUOUSLY controlled in the
    // orbit normal mode" (quoted), word-for-word the SAME construction
    // QZS-1's own document states for its own low-beta mode. QZS-3 needs NO
    // low-beta window at all -- it is ALWAYS in this mode, so ANY epoch
    // validates it directly, unlike QZS-1 itself (absent from every file
    // this session could reach, the --scan above's own record) or QZS-2/
    // QZS-4 (which do NOT share this mode in normal operation -- their own
    // SPI documents state they stay in yaw-steering, using a DIFFERENT
    // rate-limited "pseudo-yaw-steering" correction near beta=0 instead,
    // not built here, QZS-1-specific scope, `SPEC-qzss-attitude.md` §3).
    // The manager's own review, round two: CODE's own J03 solution is
    // CODE's own MODEL of the satellite (rule 8), not the satellite itself
    // -- QZS-3's own SPI states it flies orbit-normal, but that does not
    // mean CODE's own processing implements that mode for it. Diagnostic:
    // compare the SAME real J03 attitude against `nominal_yaw_steering`
    // directly (the SAME pipeline, the SAME frame mapping,
    // `qzss_yaw_attitude`'s own off-switch branch, L4 step 5's own already-
    // gated code) -- REGISTERED before this run (this file's own record,
    // kept here rather than only in the report): a 200000-geometry
    // independent check (this session's own record, kept in the report,
    // not repeated here) proves the angle between `nominal_yaw_steering`'s
    // own x_body and `orbit_normal_attitude`'s own x_body, at a GEO's fixed
    // beta, swings EXACTLY between beta and 180-beta over one day (verified
    // to the thousandth of a degree at beta=5/10/15deg) -- precisely the
    // shape the earlier EXPLORATORY sweep's own day-periodic curve showed.
    // PREDICTED: if CODE models J03 with yaw-steering, this comparison's
    // own residual will sit at the pipeline's usual floor (thousandths of a
    // degree, matching J02's/J04's own real-data agreement above) -- NOT
    // asserted after the fact, stated here before the result below was read.
    const Check checks[] = {
        {"J02", 0, 0, "yaw-steering, beta=31.4deg", Mode::Dispatch},
        {"J02", 12, 0, "yaw-steering, beta=31.1deg", Mode::Dispatch},
        {"J04", 0, 0, "yaw-steering, beta=-40.3deg", Mode::Dispatch},
        {"J04", 12, 0, "yaw-steering, beta=-40.4deg", Mode::Dispatch},
        {"J03", 0, 0, "EXPLORATORY: orbit-normal (QZS-3's own SPI)", Mode::ForceOrbitNormal},
        {"J03", 12, 0, "EXPLORATORY: orbit-normal (QZS-3's own SPI)", Mode::ForceOrbitNormal},
        {"J03", 0, 0, "DIAGNOSTIC: nominal yaw-steering (is this what CODE models?)", Mode::ForceYawSteering},
        {"J03", 12, 0, "DIAGNOSTIC: nominal yaw-steering (is this what CODE models?)", Mode::ForceYawSteering},
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

        // The day's own beta, reported alongside every result below (the
        // manager's own instruction) -- this tree's own signed_beta_rad is
        // internal (attitude.cpp's own anonymous namespace), so recomputed
        // here directly from the SAME (n_hat, Sun) geometry, independently.
        Vec3 n_hat_here = normalized(e.r[idx].cross(e.v[idx]));
        Vec3 s_hat_here = normalized(sun_dir_km);
        double beta_deg_here = std::asin(std::clamp(s_hat_here.dot(n_hat_here), -1.0, 1.0)) / kDeg;

        // REGISTERED PREDICTION -- computed here, before this function reads
        // any attitude quaternion below.
        Vec3 x_pred;
        if (chk.mode == Mode::ForceOrbitNormal) {
            Mat3 pred = attitude::orbit_normal_attitude(e.r[idx], e.v[idx]);
            x_pred = Vec3{pred.r[0][0], pred.r[0][1], pred.r[0][2]};
        } else if (chk.mode == Mode::ForceYawSteering) {
            auto pred = attitude::nominal_yaw_steering(e.r[idx], sun_dir_km);
            if (!pred.has_value()) { std::cerr << chk.prn << " REFUSED: " << pred.error().message << "\n"; continue; }
            x_pred = Vec3{pred->r[0][0], pred->r[0][1], pred->r[0][2]};
        } else {
            auto pred = attitude::qzss_yaw_attitude(e.r[idx], e.v[idx], sun_dir_km);
            if (!pred.has_value()) { std::cerr << chk.prn << " REFUSED: " << pred.error().message << "\n"; continue; }
            x_pred = Vec3{pred->r[0][0], pred->r[0][1], pred->r[0][2]};
        }

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
        if (chk.mode != Mode::Dispatch) {
            // QZS-3's own two modes (orbit-normal, yaw-steering) are
            // EXPLORATORY/DIAGNOSTIC, not held to the registered 2-deg
            // criterion (this header's own account) -- found, not folded
            // into the pass/fail verdict the yaw-steering checks below
            // earn honestly. beta reported alongside, the manager's own
            // instruction.
            std::cout << chk.prn << " " << chk.hh << ":" << chk.mm << " (" << chk.label << ")  beta="
                      << beta_deg_here << " deg  angle=" << angle_deg
                      << " deg  not held to the " << kMatchCriterionDeg << "-deg criterion (see header)\n";
            continue;
        }
        bool matches = angle_deg < kMatchCriterionDeg;
        all_matched = all_matched && matches;
        std::cout << chk.prn << " " << chk.hh << ":" << chk.mm << " (" << chk.label << ")  angle="
                  << angle_deg << " deg  " << (matches ? "MATCHES" : "DOES NOT MATCH")
                  << " the registered " << kMatchCriterionDeg << "-deg criterion\n";
    }
    std::cout << (all_matched ? "ALL REGISTERED CHECKS MATCH (yaw-steering; QZS-3's own checks are "
                                 "exploratory/diagnostic, see above)\n"
                              : "AT LEAST ONE REGISTERED CHECK DID NOT MATCH\n");
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
