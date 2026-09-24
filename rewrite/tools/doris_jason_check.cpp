// doris_jason_check.cpp — SPEC-jason-attitude.md's own real-data control,
// reproducible on demand. NOT part of the automatic gate, the SAME two
// reasons every other real-data control's own header names: scope and
// closure (no pinned data in CI; this is checked by hand, on demand).
//
// SOURCE: `doris.ign.fr`, anonymous FTP, confirmed genuinely open (no login
// challenge) this round -- CDDIS, the OTHER route `SATMOD` names, redirects
// to an EarthData OAuth login, refused per this project's own standing
// no-account discipline. Real Jason-3 body-attitude quaternions
// (`ancillary/quaternions/ja3/<year>/ja3qbody*.001`, columns headed
// "QISLEST1..4", NO stated component order or rotation convention) and real
// SP3-format orbit solutions (`products/orbits/gsc/ja3/`, a GSFC-produced
// SLR+DORIS dynamic orbit, satellite id "L39" inside the file, ITRF,
// `.Z`-compressed) -- both fetched and decompressed this round.
//
// FIRST VERSION OF THIS TOOL tried 12 axis/order/sense combinations against
// a nadir test and found none converging (13.6-170.0 deg, chaotic across
// epochs) -- the manager's own review named this trial-and-error (rule 7's
// own loophole: a comparator family widened after looking tells us little)
// and asked for rule 4 first. Found and read in full: `SALP-IF-M/IDS-
// EA15938-CN` v2 (30/06/2020), "JASON1&2&3 / Descriptions of the quaternion
// and solar panel files" (CNES, DOI 10.24400/312072/i04-2026.018),
// `ids-doris.org/resources/technical-documents/technical-note-jasons-
// quaternions-description.html`. It settles the frame (J2000, NOT ECEF --
// the earlier version's own spurious ECEF->GCRS step on the quaternion side
// is the likely cause of the earlier chaotic result, explained in full at
// `run_nadir`'s own header comment below), the component order (scalar
// first, confirming the earlier hypothesis) and the time scale (UTC). It
// does NOT state the rotation sense (body-to-J2000 or J2000-to-body) --
// the ONE genuinely remaining choice, checked both ways, a bounded second
// attempt per the manager's own instruction, not a re-opened search.
//
// MODE 1 (--nadir): TOPEX/Jason's own source states "Z always nadir"
// (`SPEC-jason-attitude.md` §3) -- a MODEL-INDEPENDENT fact checked
// directly, before trusting `jason_attitude`'s own construction, the SAME
// "verify independently before testing" discipline this project's own
// QZS-3 diagnosis used.
//
// MODE 2 (--compare): REGISTERED comparison against `jason_attitude`, for
// one Jason-3 day -- criterion and predictions stated BEFORE this mode
// reads a quaternion row for comparison (the nadir test above already read
// the file, but only to settle the rotation SENSE, a different question
// from whether `jason_attitude`'s own CONSTRUCTION is correct).
//
// RESULT (2026-09-24, GSFC/gsc's own SLR+DORIS L39=Jason-3 SP3, 2025-12-01/
// 12, matched against ja3qbody20251203220000_20251205020000.001): --nadir,
// DIRECT sense, Z axis: 0.60-1.56 deg across all 8 sampled epochs (X and Y
// both ~90 deg, as they must be when Z is genuinely nadir) -- a clean,
// consistent match, unlike the first version's chaotic 13.6-170.0 deg. The
// TRANSPOSE sense scattered (30.2-117.2 deg), clearly wrong. A SECOND,
// independent bug was found and fixed while chasing the first version's own
// stable-but-wrong ~150-155 deg result: `nearest_sp3` matched a query's own
// (hour, minute) against `e.t[i]`'s own ELAPSED hour/minute SINCE the arc's
// first epoch, not wall-clock time of day -- correct only on the SP3 arc's
// own first day, silently wrong on every later one. Fixed by matching on
// the query's own FULL calendar date/time, converted to UTC (rule 4) and
// compared by absolute elapsed TAI seconds from the SAME `t0` the ephemeris
// itself is built from.
//
// A THIRD, INDEPENDENT SOURCE OF RESIDUAL was found and fixed on the
// manager's own second review: even with both bugs above fixed, the
// residual (0.60-1.56 deg nadir, 0.14-1.52 deg comparison) was ~1000x the
// SAME pipeline's own floor on CODE's GNSS files (0.00003-0.0002 deg) --
// suspicious in its own right, and the manager's own hypothesis (an
// unapplied GPS-UTC offset, 18s, ~0.96 deg at this orbit's own ~0.0534
// deg/s rate) was checked directly: `time::TimeScale::GPS`/`UTC` were
// ALREADY correctly applied on each side (the SP3's own "%c" header line
// confirms "GPS" explicitly; the quaternion format doc states UTC), so the
// SPECIFIC "unapplied offset" mechanism was ruled out -- but the
// along-/cross-track decomposition the manager asked for (`nadir_at_shift`)
// found the REAL third bug anyway: `nearest_sp3`'s own NEAREST-1-MINUTE-
// SAMPLE selection (rather than interpolation) was itself introducing an
// error of the SAME size and SAME along-track-dominated shape an 18s
// offset would (confirmed directly: shifting the query by +/-18s and
// re-running the nearest-sample lookup moves the along-track component by
// ~0.86-1.07 deg while cross-track stays flat, ~0.01-0.15 deg throughout all
// three shifts -- the exact signature the manager named). Switching to
// EXACT interpolation (`interp_ephem`, linear between the two bracketing
// SP3 samples at the query's own precise elapsed time) collapses the
// nadir-only residual to 0.026-0.178 deg -- inside the manager's own
// predicted 0.1-0.2 deg range (Jason's own real pointing dynamics plus the
// geodetic-vs-geocentric nadir difference this tool does not correct for),
// confirming a genuine third bug, not an unexplained residual.
//
// --compare, DIRECT sense, criterion 2 deg, REGISTERED before this mode
// read a quaternion row, NOW USING THE SAME INTERPOLATION FIX: 12 epochs
// across 2025-12-03/05, beta-prime -75.3 to -78.5 deg throughout (no
// fixed-yaw window, |beta-prime|<15 deg, fell within this particular
// 2.5-day arc -- not chased further, the manager's own instruction to
// include one was conditional, "if one falls within reach"). ALL TWELVE
// MATCHED, 0.033-1.43 deg (tighter than the pre-interpolation 0.14-1.52
// deg, though not as uniformly tight as the pure nadir-only test above --
// the full yaw-steering comparison also depends on the Sun direction and
// the real yaw angle's own dynamics, a genuinely different, larger-
// degrees-of-freedom comparison than nadir alone, not chased down further
// this round given it is already comfortably inside criterion) -- strong,
// real-data confirmation of `jason_attitude`'s own yaw-steering
// construction, INCLUDING the "negate the Sun direction" frame mapping
// (`SPEC-jason-attitude.md` §3), previously DERIVED but unconfirmed. The
// fixed-yaw regime and its own construction remain UNCONFIRMED by real
// data -- no epoch this round's own reachable window exercised it.
//
// --compare's own printed table gained two columns this round (L5's exit-
// gate review, carried forward, not resolved): mu (argument of latitude)
// and the nominal yaw law's own local rate (deg/s, a central finite
// difference), alongside the existing residual -- so a follow-on can plot
// residual against rate directly, to tell whether it tracks TIMING
// sensitivity (a fast-turning law amplifies a small timing error) or
// something in the law's own functional form. Not investigated further
// this round; the per-epoch numbers are in the handover report.

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
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace odl;

namespace {

constexpr double kDeg = M_PI / 180.0;

std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.good()) { std::cerr << "cannot open " << p << "\n"; std::exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
Vec3 normalized(const Vec3& v) { double n = v.norm(); return Vec3{v.x / n, v.y / n, v.z / n}; }
double angle_deg(const Vec3& a, const Vec3& b) {
    return std::acos(std::clamp(normalized(a).dot(normalized(b)), -1.0, 1.0)) / kDeg;
}

// --- SP3, read once into an interpolated ephemeris ---------------------

// L6 step 1's own SP3-interpolation round (plan/subplan_L6/L6-1.md, ruled
// 2026-09-25): position at any query time is now `io::Sp3Ephemeris::
// position_km_at`, one order-10 Lagrange fit shared with every other tool
// this round re-points, replacing BOTH of this file's own OLD interpolation
// paths at once -- `interp_ephem`'s own linear blend between two bracketing
// samples (the fix that collapsed the along-track residual from ~1 deg to
// ~0.03-0.18 deg, per this file's own header account) AND `nearest_sp3`'s
// own cruder nearest-1-minute-sample selection, which `run_nadir` alone
// still used (a second place the same class of bug could live, now gone:
// every mode in this file reads position the SAME way). Velocity is a
// central difference of the SAME interpolant over `io::kVelocityStepS`
// (1 s), each side transformed ECEF->GCRS INDIVIDUALLY before differencing
// -- transforming a difference is not the same as differencing a
// transform, since GCRS is a time-dependent rotation of ECEF.
struct Ephem {
    time::Epoch t0;             ///< the absolute moment elapsed-seconds queries are measured from
    odl::io::Sp3Ephemeris sp3;  ///< ECEF; GCRS is a per-query transform, not precomputed (below)
};

Ephem build_ephem(const std::string& sp3path, const std::string& tag_id, const time::LeapTable& leaps) {
    auto parsed = odl::io::read_sp3(slurp(sp3path));
    if (!parsed.has_value()) { std::cerr << "SP3: " << parsed.error().id << " " << parsed.error().message << "\n"; std::exit(1); }
    auto sp3_eph = odl::io::Sp3Ephemeris::build(*parsed, tag_id);
    if (!sp3_eph.has_value()) { std::cerr << "Sp3Ephemeris: " << sp3_eph.error().id << " " << sp3_eph.error().message << "\n"; std::exit(1); }
    const auto& fe = sp3_eph->first_epoch();
    time::Calendar c{fe.year, fe.month, fe.day, fe.hour, fe.minute, fe.second};
    auto t0 = time::Epoch::from_calendar(time::TimeScale::GPS, c, leaps);
    if (!t0.has_value()) { std::cerr << "epoch: " << t0.error().message << "\n"; std::exit(1); }
    return Ephem{*t0, std::move(*sp3_eph)};
}

odl::Result<Vec3, odl::Diagnostic> ecef_to_gcrs(const time::Epoch& t, const Vec3& v_ecef_km,
                                                const time::LeapTable& leaps, const eop::EopSeries& c04) {
    auto eop_rec = c04.at(t, eop::EopPolicy{});
    if (!eop_rec.has_value()) return odl::err(eop_rec.error());
    frames::ItrsState itrs{t, v_ecef_km, Vec3{0, 0, 0}};
    auto g = frames::to_gcrs(itrs, *eop_rec, leaps);
    if (!g.has_value()) return odl::err(g.error());
    return g->position();
}

struct GcrsState { Vec3 r, v; };

/// Interpolated GCRS position AND velocity at an exact elapsed time
/// (`e.t0` plus `target_s`) -- the tree's one interpolation facility
/// (`io::Sp3Ephemeris`), used identically by every mode in this file.
odl::Result<GcrsState, odl::Diagnostic> state_at(const Ephem& e, double target_s,
                                                 const time::LeapTable& leaps, const eop::EopSeries& c04) {
    auto epoch_at = [&](double t_s) { return e.t0.add(time::Duration::from_seconds(t_s)); };

    auto r_ecef = e.sp3.position_km_at(target_s);
    if (!r_ecef.has_value()) return odl::err(r_ecef.error());
    auto r_gcrs = ecef_to_gcrs(epoch_at(target_s), *r_ecef, leaps, c04);
    if (!r_gcrs.has_value()) return odl::err(r_gcrs.error());

    auto r_minus_ecef = e.sp3.position_km_at(target_s - odl::io::kVelocityStepS);
    auto r_plus_ecef = e.sp3.position_km_at(target_s + odl::io::kVelocityStepS);
    if (!r_minus_ecef.has_value()) return odl::err(r_minus_ecef.error());
    if (!r_plus_ecef.has_value()) return odl::err(r_plus_ecef.error());
    auto g_minus = ecef_to_gcrs(epoch_at(target_s - odl::io::kVelocityStepS), *r_minus_ecef, leaps, c04);
    if (!g_minus.has_value()) return odl::err(g_minus.error());
    auto g_plus = ecef_to_gcrs(epoch_at(target_s + odl::io::kVelocityStepS), *r_plus_ecef, leaps, c04);
    if (!g_plus.has_value()) return odl::err(g_plus.error());
    Vec3 v_gcrs = (1.0 / (2.0 * odl::io::kVelocityStepS)) * (*g_plus - *g_minus);

    return GcrsState{*r_gcrs, v_gcrs};
}

// --- DORIS quaternion ancillary file ---------------------------------------

struct QRow { int y, mo, d, h, mi; double sec; double q1, q2, q3, q4; };

std::vector<QRow> read_qbody(const std::string& path) {
    std::vector<QRow> rows;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        // "2026/04/01 22:00:20.373  <bigint> <q1> 2007  <bigint> <q2> 2007  ..."
        std::string datepart, timepart;
        std::istringstream ss(line);
        ss >> datepart >> timepart;
        if (datepart.size() < 10 || timepart.size() < 8) continue;
        QRow row{};
        row.y = std::stoi(datepart.substr(0, 4));
        row.mo = std::stoi(datepart.substr(5, 2));
        row.d = std::stoi(datepart.substr(8, 2));
        row.h = std::stoi(timepart.substr(0, 2));
        row.mi = std::stoi(timepart.substr(3, 2));
        row.sec = std::stod(timepart.substr(6));
        long long junk;
        double q[4];
        for (int k = 0; k < 4; ++k) {
            ss >> junk >> q[k] >> junk;
            if (!ss) { ss.clear(); ss >> q[k]; }  // tolerate a non-integer quality code
        }
        row.q1 = q[0]; row.q2 = q[1]; row.q3 = q[2]; row.q4 = q[3];
        rows.push_back(row);
    }
    return rows;
}

/// The standard quaternion->DCM column, `q_s` the scalar part, `(qx,qy,qz)`
/// the vector part: the body axis this returns, expressed in the OTHER
/// frame, matching `orbex_qzss_check.cpp`'s own `xb_ecef` formula exactly
/// (that tool's own q0=scalar case) -- reused here for whichever component
/// the caller names as scalar, so both candidate orderings share one
/// formula rather than two hand-duplicated ones.
Vec3 body_z_axis_in_other_frame(double q_s, double qx, double qy, double qz) {
    // Third COLUMN of the standard DCM(q) -- body z expressed in the other frame.
    return Vec3{2 * (qx * qz + q_s * qy), 2 * (qy * qz - q_s * qx), 1 - 2 * (qx * qx + qy * qy)};
}
Vec3 body_x_axis_in_other_frame(double q_s, double qx, double qy, double qz) {
    return Vec3{1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy + q_s * qz), 2 * (qx * qz - q_s * qy)};
}
Vec3 body_y_axis_in_other_frame(double q_s, double qx, double qy, double qz) {
    return Vec3{2 * (qx * qy - q_s * qz), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz + q_s * qx)};
}

struct EnvBits {
    time::LeapTable leaps;
    eop::EopSeries c04;
    eph::Ephemeris ephem;
};

EnvBits load_env(const std::string& leappath) {
    auto leaps = time::LeapTable::parse(slurp(leappath), time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps.has_value()) { std::cerr << "leap: " << leaps.error().message << "\n"; std::exit(1); }
    auto c04 = eop::EopSeries::load_c04(slurp("data/cache/eop-c04-20/eopc04.1962-now"),
                                        eop::EopProvenance{"eop-c04-20", "", "", ""}, *leaps);
    if (!c04.has_value()) { std::cerr << "c04: " << c04.error().message << "\n"; std::exit(1); }
    auto ephem = eph::Ephemeris::open({"data/cache/de440s-spk/de440s.bsp"}, {});
    if (!ephem.has_value()) { std::cerr << "ephem: " << ephem.error().message << "\n"; std::exit(1); }
    return EnvBits{std::move(*leaps), std::move(*c04), std::move(*ephem)};
}

/// The query's own elapsed time from `e.t0`, TAI seconds, exact -- `shift_s`
/// added AFTER the UTC-to-TAI conversion, for testing whether an unaccounted
/// time-scale offset (the manager's own hypothesis for an earlier residual)
/// is present: a REAL such offset would need correcting BEFORE the
/// conversion in production code, but adding it after, here, in a
/// diagnostic-only function, tests the SAME numerical effect a
/// before-conversion fix would have, without implying one is believed to
/// exist. `nearest_sp3` (this function's own former sibling, a coarser,
/// nearest-1-minute-sample position lookup `run_nadir` alone still used)
/// and `interp_ephem` (a hand-rolled linear blend the rest of this file
/// used) are BOTH retired this round -- `state_at`, above, is now every
/// mode's own one route to a position, at this exact elapsed time.
double target_elapsed_s(const Ephem& e, int y, int mo, int d, int hh, int mm, double sec,
                        const time::LeapTable& leaps, double shift_s) {
    time::Calendar c;
    c.year = y; c.month = mo; c.day = d; c.hour = hh; c.minute = mm; c.second = sec;
    auto query = time::Epoch::from_calendar(time::TimeScale::UTC, c, leaps);
    if (!query.has_value()) { std::cerr << "epoch: " << query.error().message << "\n"; std::exit(1); }
    return static_cast<double>(query->tai_seconds() - e.t0.tai_seconds()) + shift_s;
}

/// RULE 4, applied per the manager's own instruction: `SALP-IF-M/IDS-
/// EA15938-CN` v2 (30/06/2020), "JASON1&2&3 / Descriptions of the
/// quaternion and solar panel files" (CNES, DOI 10.24400/312072/i04-
/// 2026.018), found at `ids-doris.org/resources/technical-documents/
/// technical-note-jasons-quaternions-description.html`, fetched and read
/// in full 2026-09-24 -- THE convention, quoted, not the 12-way guess this
/// tool's own earlier version tried:
///
///   "The quaternion files contain the 4 components of the spacecraft
///   attitude in the J2000 frame."  -- NOT ECEF. The earlier version of
///   this tool converted the quaternion-derived body axis from ECEF to
///   GCRS via `frames::to_gcrs`, the SAME step CODE's own ORBEX
///   quaternions genuinely need (confirmed correct there, 0.00003-0.0002
///   deg agreement) -- but DORIS's own quaternions need NO such step,
///   already being in an inertial (J2000 approx GCRS at this precision)
///   frame. Applying that spurious extra Earth-rotation transform is the
///   most likely reason NONE of the earlier version's twelve combinations
///   converged: Earth rotates ~15 deg/hour, and the earlier test's own
///   eight epochs spanned ~26 hours, so a wrongly-applied ECEF/GCRS
///   rotation would swing the predicted axis across tens of degrees as
///   time of day changes -- exactly the chaotic (13.6-170.0 deg) pattern
///   found, not a constant offset.
///
///   "Q = [Q0, Q1, Q2, Q3] where Q0 = scalar (real) part, and [Q1,Q2,Q3] =
///   vector (imaginary) part" ... "QISLEST1 ... Scalar (real) part" --
///   SCALAR FIRST, confirming this tool's own "scalar-first" hypothesis
///   (never the "scalar-last" one, now dropped).
///
///   "UT time ... UTC time of the packet" -- UTC, not GPS time (the SP3's
///   own convention, unrelated and unchanged -- SP3 epochs stay GPS time,
///   `time::TimeScale::GPS`, the standard for that format).
///
///   "only 5 parameters are useful (time and quaternion components), the 8
///   others (integers UI<n>) are useless and shall be skipped" -- confirms
///   this tool's own existing parsing (the big integers were always
///   ignored).
///
/// NOT STATED by this document: the rotation SENSE (does the quaternion
/// rotate body-frame coordinates INTO J2000, or J2000 coordinates INTO the
/// body frame?). This is the ONE remaining, genuinely unresolved choice --
/// checked both ways below, a bounded, single run, not a re-opened search.
/// The manager's own hypothesis: the residual (0.60-1.56 deg nadir,
/// 0.14-1.52 deg comparison) is the SIZE an unapplied GPS-UTC offset (18s
/// at Jason-3's own ~0.0534 deg/s orbital rate) would produce, ~1000x the
/// same pipeline's own floor on CODE's GNSS files. This function reports
/// the nadir residual at a STATED time shift, decomposed along-track
/// (`t_hat`) and cross-track (`n_hat`) -- a time error is almost all
/// along-track, so the split itself is part of the evidence, not only the
/// total. Interpolates the SP3 ephemeris at the EXACT shifted target time
/// (nearest-SAMPLE selection, the 1-minute SP3 grid, is too coarse to
/// resolve an 18s shift -- it would often not even change which sample is
/// nearest).
void nadir_at_shift(const Ephem& e, const std::vector<QRow>& q, const time::LeapTable& leaps,
                    const eop::EopSeries& c04, double shift_s, int max_epochs) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  shift = " << std::showpos << shift_s << std::noshowpos << " s\n";
    std::cout << "  epoch (UTC)       | total (deg) | along-track (deg) | cross-track (deg)\n";
    int checked = 0;
    for (std::size_t qi = 0; qi < q.size() && checked < max_epochs;
        qi += (q.size() / static_cast<std::size_t>(max_epochs) == 0
                   ? 1
                   : q.size() / static_cast<std::size_t>(max_epochs)),
        ++checked) {
        const QRow& row = q[qi];
        const double target = target_elapsed_s(e, row.y, row.mo, row.d, row.h, row.mi, row.sec,
                                                leaps, shift_s);
        const auto st = state_at(e, target, leaps, c04);
        if (!st.has_value()) { std::cerr << "  state_at: " << st.error().id << " " << st.error().message << "\n"; continue; }
        const Vec3 r_hat = normalized(st->r);
        const Vec3 nadir_gcrs = -1.0 * r_hat;
        const Vec3 n_hat = normalized(st->r.cross(st->v));
        const Vec3 t_hat = n_hat.cross(r_hat);

        Vec3 z_direct_gcrs = body_z_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        const double total = angle_deg(z_direct_gcrs, nadir_gcrs);

        // Small-angle projection of the error vector onto the local
        // along-/cross-track directions -- exact to O(total^3), utterly
        // negligible at a total error of order 1 deg (~1e-4 rad^3).
        const Vec3 err = z_direct_gcrs - nadir_gcrs;
        const double along_deg = (err.dot(t_hat)) / kDeg;
        const double cross_deg = (err.dot(n_hat)) / kDeg;

        std::cout << "  " << row.y << "/" << row.mo << "/" << row.d << " " << row.h << ":" << row.mi
                  << ":" << row.sec << " | " << total << " | " << along_deg << " | " << cross_deg << "\n";
    }
}

void run_nadir(const std::string& sp3path, const std::string& qpath, const std::string& leappath) {
    auto env = load_env(leappath);
    auto e = build_ephem(sp3path, "L39", env.leaps);
    auto q = read_qbody(qpath);
    if (q.empty()) { std::cerr << "no quaternion rows in " << qpath << "\n"; std::exit(1); }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "NADIR TEST (rule-4 convention: scalar-first, J2000/GCRS directly, UTC) -- "
                 "angle (deg) between the quaternion's own predicted body Z axis and -r_hat "
                 "(real nadir), the ONE remaining ambiguity (rotation sense), several epochs\n";
    std::cout << "epoch (UTC)       | direct  | transpose\n";

    int checked = 0;
    for (std::size_t qi = 0; qi < q.size() && checked < 8; qi += (q.size() / 8 == 0 ? 1 : q.size() / 8), ++checked) {
        const QRow& row = q[qi];
        // Position at this quaternion row's own EXACT wall-clock time
        // (state_at, the tree's one interpolation facility -- the SP3
        // reader's own minute grid and the quaternion file's own ~32s
        // cadence do not align, and no longer need to: this is no longer a
        // nearest-sample lookup).
        const double target = target_elapsed_s(e, row.y, row.mo, row.d, row.h, row.mi, row.sec, env.leaps, 0.0);
        const auto st = state_at(e, target, env.leaps, env.c04);
        if (!st.has_value()) { std::cerr << "state_at: " << st.error().id << " " << st.error().message << "\n"; continue; }
        Vec3 nadir_gcrs = -1.0 * normalized(st->r);

        // Q0 (scalar) = row.q1; [Q1,Q2,Q3] (vector) = row.q2,q3,q4 -- rule-4
        // confirmed, applied directly to GCRS, NO ECEF step.
        Vec3 z_direct_gcrs = body_z_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        Vec3 z_transp_gcrs = body_z_axis_in_other_frame(-row.q1, row.q2, row.q3, row.q4);

        double a1 = angle_deg(z_direct_gcrs, nadir_gcrs);
        double a2 = angle_deg(z_transp_gcrs, nadir_gcrs);
        std::cout << row.y << "/" << row.mo << "/" << row.d << " " << row.h << ":" << row.mi
                  << ":" << row.sec << " | " << a1 << " | " << a2 << "\n";

        // Z/direct is stable but clusters away from both 0 and 180 -- ADDED
        // when that was found: which of the three body axes (still under
        // the NOW rule-4-corrected frame/order/timescale, not a repeat of
        // the earlier, uncorrected 12-way sweep) is actually nadir in
        // DORIS's own telemetry-native frame, which may differ from "the
        // satellite reference frame" the ATTITUDE-LAW document names.
        Vec3 x_direct_gcrs = body_x_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        Vec3 y_direct_gcrs = body_y_axis_in_other_frame(row.q1, row.q2, row.q3, row.q4);
        std::cout << "    [direct sense, all axes] X:" << angle_deg(x_direct_gcrs, nadir_gcrs)
                  << " Y:" << angle_deg(y_direct_gcrs, nadir_gcrs) << " Z:" << a1 << "\n";
    }

    std::cout << "\nTIME-SHIFT DIAGNOSTIC (the manager's own hypothesis: an unapplied GPS-UTC "
                 "offset, ~18s at this orbit's own rate, is about the size of the residual "
                 "above) -- along-/cross-track decomposition at three shifts, interpolated "
                 "exactly, not nearest-sample:\n";
    nadir_at_shift(e, q, env.leaps, env.c04, 0.0, 8);
    nadir_at_shift(e, q, env.leaps, env.c04, 18.0, 8);
    nadir_at_shift(e, q, env.leaps, env.c04, -18.0, 8);
}

/// MODE 2, run only after `--nadir` shows one sense converging cleanly.
/// REGISTERED: the criterion below (2 deg, matching every other ORBEX-style
/// control's own `kMatchCriterionDeg`) is fixed BEFORE this function reads
/// a single quaternion row for comparison -- the nadir test above already
/// read the file, but only to settle which of the two rotation senses to
/// use, a different question from whether `jason_attitude`'s own
/// CONSTRUCTION is correct, which is what this mode alone tests.
constexpr double kMatchCriterionDeg = 2.0;

/// The manager's own third-review ask, carried forward as a diagnostic for a
/// follow-on: alongside beta and the residual, print mu (argument of
/// latitude, the SAME satellite-agnostic ascending-node angle
/// `sentinel6_argument_of_latitude_rad` already computes -- reused directly
/// here as a generic geometric quantity, not because Jason's own law reads
/// it) and the nominal yaw law's own LOCAL rate, so a later session can tell
/// whether the residual tracks how fast the LAW is turning (timing
/// sensitivity) rather than something in the law's own functional form.
///
/// A CENTRAL FINITE DIFFERENCE of jason_attitude's own body-x frame, over a
/// small +/-dt_s straight-line perturbation of r along the real v (v and the
/// Sun direction held fixed across the interval -- dt_s is small enough,
/// against both the orbital period (~112 min) and the Sun's own apparent
/// motion, that this is a diagnostic magnitude, not a production rate) --
/// the SAME "perturb, re-run the real public interface, difference the
/// OUTPUT FRAME" technique this tree's own TYAW-A-015/GALY-A-011 guards
/// already use, not a read of jason_attitude's own private internals.
double nominal_yaw_rate_deg_per_s(const Vec3& r, const Vec3& v, const Vec3& sun_dir) {
    constexpr double kDtS = 5.0;
    attitude::JasonRegime regime{};
    auto plus = attitude::jason_attitude(r + kDtS * v, v, sun_dir, &regime);
    auto minus = attitude::jason_attitude(r - kDtS * v, v, sun_dir, &regime);
    if (!plus.has_value() || !minus.has_value()) return 0.0;  // a regime boundary crossed within the step -- not expected at any epoch used here, all deep in yaw-steering
    Vec3 x_plus{plus->r[0][0], plus->r[0][1], plus->r[0][2]};
    Vec3 x_minus{minus->r[0][0], minus->r[0][1], minus->r[0][2]};
    return angle_deg(x_plus, x_minus) / (2.0 * kDtS);
}

void run_compare(const std::string& sp3path, const std::string& qpath, const std::string& leappath,
                 bool use_transpose) {
    auto env = load_env(leappath);
    auto e = build_ephem(sp3path, "L39", env.leaps);
    auto q = read_qbody(qpath);
    if (q.empty()) { std::cerr << "no quaternion rows in " << qpath << "\n"; std::exit(1); }

    std::cout << std::fixed << std::setprecision(5);
    std::cout << "REGISTERED COMPARISON against jason_attitude, criterion " << kMatchCriterionDeg
              << " deg, " << (use_transpose ? "transpose" : "direct") << " sense (settled by --nadir)\n";
    std::cout << "epoch (UTC)       | beta-prime | mu (deg) | regime      | nominal yaw rate "
                 "(deg/s) | angle to prediction (deg)\n";

    bool all_matched = true;
    int checked = 0;
    for (std::size_t qi = 0; qi < q.size() && checked < 12;
        qi += (q.size() / 12 == 0 ? 1 : q.size() / 12), ++checked) {
        const QRow& row = q[qi];
        // INTERPOLATED, not nearest-sample (the `--nadir` diagnostic found
        // nearest-1-minute-sample rounding was the dominant remaining
        // residual, ~0.86-1.07 deg along-track, once the frame/order/time-
        // scale bugs were already fixed -- this mode inherits that fix, now
        // through state_at's own order-10 fit rather than a linear blend).
        const double target = target_elapsed_s(e, row.y, row.mo, row.d, row.h, row.mi, row.sec,
                                                env.leaps, 0.0);
        const auto st = state_at(e, target, env.leaps, env.c04);
        if (!st.has_value()) { std::cerr << "state_at: " << st.error().id << " " << st.error().message << "\n"; continue; }
        const Vec3 r = st->r, v = st->v;

        time::Epoch t = [&] {
            time::Calendar c; c.year = row.y; c.month = row.mo; c.day = row.d;
            c.hour = row.h; c.minute = row.mi; c.second = row.sec;
            auto ep = time::Epoch::from_calendar(time::TimeScale::UTC, c, env.leaps);
            if (!ep.has_value()) { std::cerr << "epoch: " << ep.error().message << "\n"; std::exit(1); }
            return *ep;
        }();
        auto sun = env.ephem.geocentric_state(eph::Body::Sun, t, env.leaps);
        if (!sun.has_value()) { std::cerr << "sun: " << sun.error().message << "\n"; continue; }
        Vec3 sun_dir_km = sun->position() - r;

        attitude::JasonRegime regime{};
        auto pred = attitude::jason_attitude(r, v, sun_dir_km, &regime);
        if (!pred.has_value()) { std::cerr << "REFUSED: " << pred.error().message << "\n"; continue; }
        Vec3 x_pred{pred->r[0][0], pred->r[0][1], pred->r[0][2]};

        const double q_s = use_transpose ? -row.q1 : row.q1;
        Vec3 x_real_gcrs = body_x_axis_in_other_frame(q_s, row.q2, row.q3, row.q4);

        double n_hat_dot_s = normalized(r.cross(v)).dot(normalized(sun_dir_km));
        double beta_deg = std::asin(std::clamp(n_hat_dot_s, -1.0, 1.0)) / kDeg;
        double mu_deg = attitude::sentinel6_argument_of_latitude_rad(r, v) / kDeg;
        double rate_deg_s = nominal_yaw_rate_deg_per_s(r, v, sun_dir_km);
        double err = angle_deg(x_pred, x_real_gcrs);
        if (err > kMatchCriterionDeg) all_matched = false;
        std::cout << row.y << "/" << row.mo << "/" << row.d << " " << row.h << ":" << row.mi
                  << ":" << row.sec << " | " << beta_deg << " | " << mu_deg << " | "
                  << (regime == attitude::JasonRegime::FixedYaw ? "FixedYaw " : "YawSteering")
                  << " | " << rate_deg_s << " | " << err << "\n";
    }
    std::cout << (all_matched ? "ALL MATCHED within the registered criterion\n"
                              : "AT LEAST ONE EXCEEDED the registered criterion\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 5 && std::string(argv[1]) == "--nadir") {
        run_nadir(argv[2], argv[3], argv[4]);
        return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--compare") {
        const std::string sense = argv[5];
        if (sense != "direct" && sense != "transpose") {
            std::cerr << "sense must be 'direct' or 'transpose'\n"; return 1;
        }
        run_compare(argv[2], argv[3], argv[4], sense == "transpose");
        return 0;
    }
    std::cerr << "usage: doris_jason_check --nadir <sp3> <qbody> <leap_seconds_dat>\n"
              << "   or: doris_jason_check --compare <sp3> <qbody> <leap_seconds_dat> <direct|transpose>\n";
    return 1;
}
