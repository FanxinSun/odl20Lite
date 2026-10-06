// angle_gate_tests.cpp — SPEC-measmod.md §6.2 (v) (the angle gate, frozen 2026-10-06 BEFORE any of this code was written; tools/measmod_fd_sizing.py --scan --check reproduces its numbers)
// and §8.7: MEAS-A-096 … -099 (the gate for right ascension and declination Astrometric, Geometric, azimuth and elevation ApparentRefracted, and
// Astrometric of date), run ONCE each, alone, with the log kept: the cases carry the tag [pregistered], which an unfiltered run of the binary would execute.
//
// The gate: central differences of the model's OUTPUTS (two: a right ascension or azimuth, wrapped, and a declination or elevation) over a trajectory family that varies the target's state at
// the nominal EMISSION epoch by ±h e_k, TIME-FIXED, re-solving the light time for every varied trajectory, at five sizes h ∈ {10, 30, 100, 300, 1000} m for the position directions and
// four, {0.01, 0.1, 1, 10} m/s, for the velocity directions.
//   (a) B_pred/10 <= B <= 10 B_pred          B the largest spread of the five estimates over both outputs and the three components
//   (b) |f̂(h) − a| <= ε(h)                    for each output, component and size
//   (c) |f̂(h_v)| <= ν_a/h_v and a = 0 exactly for the velocity columns
// with ε(h) = h² F_a(h)/6 + ν_a/h, F_a(h) = F_pure(h) + F_extra/ρ³, ν_a = 3 [ulp(2π) + ulp(|r|)/ρ] in BOTH configurations (the chain's floor is zero: the observer, the orientation and the
// Earth's motion are evaluated at the observation epoch only, MEAS-A-089). Both configurations of Amendment A1 are run: the real chain and an exact rigid rotation of its own matrix whose
// reference epoch lies 0.01 s before the observation epoch; they must agree within 2 ν_a/h + 3 δs/(ρ(1 − κ) − h)². The target's velocity is run across the line of sight and along it.
// The literals below are the specification's frozen numbers; the test recomputes them from the formulas and asserts they agree.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "angle_scenario.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using odl::Mat3;
using odl::Vec3;
using odl::time::Epoch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC = 299792458.0;
constexpr std::array<double, 5> kHs = {{10.0, 30.0, 100.0, 300.0, 1000.0}};
constexpr std::array<double, 4> kHvs = {{0.01, 0.1, 1.0, 10.0}};
using Estimates = std::array<std::array<std::array<double, 5>, 3>, 2>;           // [output][component][size]
using EstimatesV = std::array<std::array<std::array<double, 4>, 3>, 2>;          // [output][component][velocity size]

constexpr double kRho = 1.2e6;                       // m: the slant range of the sizing
constexpr double kSpeed = 7500.0;                    // m/s
constexpr double kNuAlit = 4.9928e-15;               // rad: the frozen noise bound (MEAS-P-25, -26)
constexpr double kDeltaS = 1.0e-6;                   // m: the bound of the observers' difference the gate asserts before it compares configurations
constexpr double kObserversMax = 1.0e-6;             // m

double ulp_of(double x) { return std::nextafter(std::abs(x), std::numeric_limits<double>::infinity()) - std::abs(x); }
double nu_a() { return 3.0 * (ulp_of(2.0 * kPi) + ulp_of(7.2e6) / kRho); }
double f_pure(double h) { return 2.0 / (std::pow(kRho - h, 3.0) * std::pow(std::cos(40.0 * kPi / 180.0 + std::asin(h / kRho)), 3.0)); }

/// One group of the frozen table: the extra term and the specification's literals.
struct AngleFrozen {
    const char* name;
    AngleGate gate;
    double f_extra;                                  ///< F_extra / rho^3: the fraction of 1/rho^3 added to F_pure
    std::array<double, 5> f_a_lit, eps_lit;          ///< the specification's F_a(h), rad m^-3, and epsilon(h), rad m^-1
    double b_pred_lit;
};
const AngleFrozen kGeometric = {"Geometric", AngleGate::Geometric, 2.62e-4, {{2.57496e-18, 2.57519e-18, 2.57602e-18, 2.57839e-18, 2.58671e-18}},
                                {{5.4220e-16, 5.5271e-16, 4.3433e-15, 3.8693e-14, 4.3112e-13}}, 4.3112e-13};
const AngleFrozen kAstrometric = {"Astrometric", AngleGate::Astrometric, 1.43e-3, {{2.57563e-18, 2.57587e-18, 2.57670e-18, 2.57907e-18, 2.58739e-18}},
                                  {{5.4221e-16, 5.5281e-16, 4.3444e-15, 3.8703e-14, 4.3124e-13}}, 4.3124e-13};
const AngleFrozen kRefracted = {"ApparentRefracted", AngleGate::ApparentRefracted, 9.74e-2, {{2.63117e-18, 2.63141e-18, 2.63224e-18, 2.63461e-18, 2.64292e-18}},
                                {{5.4314e-16, 5.6114e-16, 4.4370e-15, 3.9536e-14, 4.4049e-13}}, 4.4049e-13};
const AngleFrozen kOfDate = {"Astrometric, true equator and equinox of date", AngleGate::AstrometricOfDate, 1.43e-3, {{2.57563e-18, 2.57587e-18, 2.57670e-18, 2.57907e-18, 2.58739e-18}},
                             {{5.4221e-16, 5.5281e-16, 4.3444e-15, 3.8703e-14, 4.3124e-13}}, 4.3124e-13};
constexpr std::array<double, 5> kAgreementLit = {{1.0007e-15, 3.3494e-16, 1.0194e-16, 3.5370e-17, 1.2073e-17}};

double f_a(const AngleFrozen& fz, std::size_t i) { return f_pure(kHs[i]) + fz.f_extra / (kRho * kRho * kRho); }
double eps_of(const AngleFrozen& fz, std::size_t i) { return kHs[i] * kHs[i] * f_a(fz, i) / 6.0 + nu_a() / kHs[i]; }
double kappa() { return kSpeed / kC; }
double agreement_tolerance(std::size_t i) { return 2.0 * nu_a() / kHs[i] + 3.0 * kDeltaS / std::pow(kRho * (1.0 - kappa()) - kHs[i], 2.0); }

const char* direction_name(double radial) { return radial == 0.0 ? "across" : "along"; }

// ---- the model's output, and the row assembled in the test from the term functions (for the controls) -----------------------------------------------------------------------------------

std::array<double, 2> evaluate(const AngleScenario& sc, AngleConfig cfg, const DriftTrajectory& tr) {
    const AngleObservation obs = sc.observation();
    if (sc.gate() == AngleGate::ApparentRefracted) {
        auto m = model_azel(obs, sc.station(cfg), tr, sc.orientation(cfg));
        REQUIRE(m.has_value());
        return {m->azimuth_rad(), m->elevation_rad()};
    }
    auto m = model_radec(obs, sc.station(cfg), tr, sc.motion());
    REQUIRE(m.has_value());
    return {m->ra_rad(), m->dec_rad()};
}

std::array<Vec3, 2> model_row(const AngleScenario& sc, AngleConfig cfg, std::array<double, 3>* velocity_part0 = nullptr, std::array<double, 3>* velocity_part1 = nullptr) {
    const AngleObservation obs = sc.observation();
    auto take = [&](const auto& m) {
        if (velocity_part0)
            for (std::size_t k = 0; k < 3; ++k) (*velocity_part0)[k] = m.partials().d[0][k + 3];
        if (velocity_part1)
            for (std::size_t k = 0; k < 3; ++k) (*velocity_part1)[k] = m.partials().d[1][k + 3];
        return std::array<Vec3, 2>{Vec3{m.partials().d[0][0], m.partials().d[0][1], m.partials().d[0][2]}, Vec3{m.partials().d[1][0], m.partials().d[1][1], m.partials().d[1][2]}};
    };
    if (sc.gate() == AngleGate::ApparentRefracted) {
        auto m = model_azel(obs, sc.station(cfg), sc.trajectory(), sc.orientation(cfg));
        REQUIRE(m.has_value());
        return take(*m);
    }
    auto m = model_radec(obs, sc.station(cfg), sc.trajectory(), sc.motion());
    REQUIRE(m.has_value());
    return take(*m);
}

Mat3 outer(const Vec3& a, const Vec3& b) {
    const double av[3] = {a.x, a.y, a.z}, bv[3] = {b.x, b.y, b.z};
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = av[i] * bv[j];
    return m;
}
Mat3 minus(const Mat3& a, const Mat3& b) {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = a.r[i][j] - b.r[i][j];
    return m;
}
Mat3 times_scalar(const Mat3& a, double s) {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = s * a.r[i][j];
    return m;
}
Vec3 row_times(const Vec3& r, const Mat3& m) {
    return Vec3{r.x * m.r[0][0] + r.y * m.r[1][0] + r.z * m.r[2][0], r.x * m.r[0][1] + r.y * m.r[1][1] + r.z * m.r[2][1], r.x * m.r[0][2] + r.y * m.r[1][2] + r.z * m.r[2][2]};
}

/// What a control leaves out of the row of MEAS-R-060 (all true: the model's row).
struct RowOptions {
    bool annual_jacobian = true;        ///< D' of the Astrometric reduction
    bool dzo_dzv = true;                ///< the refraction's dz_o/dz_v
    bool light_time_shift = true;       ///< K = I - v n^T/(c + n.v): the emission epoch's dependence on the displacement
    bool frame_rotation = true;         ///< the rotation of the of-date frame inside the row
    bool diurnal_jacobian = true;       ///< A' of the diurnal aberration
};

/// The position part of the row, [d output / d r] for the two outputs, assembled here from the term functions and the geometric facts of solve_emission — the specification's
/// "Angles" paragraph of MEAS-R-060, written out again in the test, with the options of a control.
std::array<Vec3, 2> assemble_row(const AngleScenario& sc, AngleConfig cfg, const RowOptions& opt) {
    const Epoch t_o = sc.observation_epoch();
    auto s = sc.station(cfg).at(t_o);
    REQUIRE(s.has_value());
    auto em = solve_emission(t_o, s->position_m, sc.trajectory());
    REQUIRE(em.has_value());
    const Vec3 n = em->direction, v = em->target_velocity_m_s;
    const Mat3 identity = Mat3::identity();
    const Mat3 k = opt.light_time_shift ? minus(identity, times_scalar(outer(v, n), 1.0 / (kC + n.dot(v)))) : identity;
    const Mat3 p = minus(identity, outer(n, n));
    const Mat3 jg = times_scalar(p.times(k), 1.0 / em->range_m);

    if (sc.gate() != AngleGate::ApparentRefracted) {
        Vec3 n_red = n;
        Mat3 j_red = identity;
        if (sc.gate() == AngleGate::Astrometric || sc.gate() == AngleGate::AstrometricOfDate) {
            auto earth = sc.motion().at(t_o);
            REQUIRE(earth.has_value());
            const Vec3 beta = (1.0 / kC) * earth->velocity_m_s;
            n_red = aberration_remove(n, beta);
            if (opt.annual_jacobian) j_red = aberration_remove_jacobian(n, beta);
        }
        const Mat3 frame = (sc.gate() == AngleGate::AstrometricOfDate && opt.frame_rotation) ? sc.frame_matrix() : identity;
        const Vec3 m = frame.apply(n_red);
        const Mat3 j = frame.times(j_red).times(jg);
        const double h = std::hypot(m.x, m.y), m2 = m.dot(m);
        const Vec3 row_ra = (1.0 / (h * h)) * Vec3{-m.y, m.x, 0.0};
        const Vec3 row_dec = (1.0 / (h * m2)) * Vec3{-m.z * m.x, -m.z * m.y, h * h};
        return {row_times(row_ra, j), row_times(row_dec, j)};
    }

    // azimuth and elevation
    auto refco = refraction_constants(*sc.observation().atmosphere);
    REQUIRE(refco.has_value());
    auto sample = sc.orientation(cfg).at(t_o);
    REQUIRE(sample.has_value());
    const Vec3 beta_d = (1.0 / kC) * s->velocity_m_s;
    const Vec3 n_app = aberration_apply(n, beta_d);
    const Mat3 j_red = opt.diurnal_jacobian ? aberration_apply_jacobian(n, beta_d) : identity;
    const double sl = std::sin(sc.site().location.longitude_rad), cl = std::cos(sc.site().location.longitude_rad);
    const double sp = std::sin(sc.site().location.latitude_rad), cp = std::cos(sc.site().location.latitude_rad);
    Mat3 enu{};
    enu.r[0] = {-sl, cl, 0.0};
    enu.r[1] = {-sp * cl, -sp * sl, cp};
    enu.r[2] = {cp * cl, cp * sl, sp};
    const Mat3 to_local = enu.times(sample->gcrs_to_itrs);
    const Vec3 loc = to_local.apply(n_app);
    const double h = std::hypot(loc.x, loc.y);
    const double z_v = std::atan2(h, loc.z), z_o = refracted_zenith_distance(z_v, *refco);
    const double dzo = opt.dzo_dzv ? refracted_zenith_derivative(z_o, *refco) : 1.0;
    const Mat3 j = to_local.times(j_red).times(jg);
    const Vec3 row_e{j.r[0][0], j.r[0][1], j.r[0][2]}, row_n{j.r[1][0], j.r[1][1], j.r[1][2]}, row_u{j.r[2][0], j.r[2][1], j.r[2][2]};
    const Vec3 g_az = (1.0 / (h * h)) * (loc.y * row_e - loc.x * row_n);
    const Vec3 dh = (1.0 / h) * (loc.x * row_e + loc.y * row_n);
    const Vec3 g_zv = (1.0 / (h * h + loc.z * loc.z)) * (loc.z * dh - h * row_u);
    return {g_az, -dzo * g_zv};
}

// ---- the gate ---------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct AngleOutcome {
    Estimates fhat{};
    EstimatesV fhat_v{};
    std::array<Vec3, 2> analytic{};
    std::array<std::array<double, 3>, 2> analytic_v{};
    double band = 0.0;
    double worst_b = 0.0;
    int worst_b_out = 0, worst_b_k = 0;
    std::size_t worst_b_i = 0;
    double worst_c = 0.0;
    bool inside_raw_spread = true;
    std::array<double, 5> eps{};
};

double violation(const Estimates& fhat, const std::array<Vec3, 2>& candidate, const std::array<double, 5>& eps) {
    double worst = 0.0;
    for (std::size_t o = 0; o < 2; ++o) {
        const double a[3] = {candidate[o].x, candidate[o].y, candidate[o].z};
        for (std::size_t k = 0; k < 3; ++k)
            for (std::size_t i = 0; i < kHs.size(); ++i) worst = std::max(worst, std::abs(fhat[o][k][i] - a[k]) / eps[i]);
    }
    return worst;
}

AngleOutcome run_gate(const AngleFrozen& fz, const AngleScenario& sc, AngleConfig cfg) {
    AngleOutcome o;
    for (std::size_t i = 0; i < 5; ++i) o.eps[i] = eps_of(fz, i);
    o.analytic = model_row(sc, cfg, &o.analytic_v[0], &o.analytic_v[1]);
    auto y = [&](const Vec3& dr, const Vec3& dv) { return evaluate(sc, cfg, sc.trajectory(dr, dv)); };
    auto difference = [&](std::size_t out, const std::array<double, 2>& hi, const std::array<double, 2>& lo, double two_h) {
        const double d = hi[out] - lo[out];
        return (out == 0 ? wrap_to_pi(d) : d) / two_h;
    };
    const Vec3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (std::size_t out = 0; out < 2; ++out) {
        const double a[3] = {o.analytic[out].x, o.analytic[out].y, o.analytic[out].z};
        for (std::size_t k = 0; k < 3; ++k) {
            double lo = std::numeric_limits<double>::infinity(), hi = -lo;
            for (std::size_t i = 0; i < kHs.size(); ++i) {
                const double h = kHs[i];
                o.fhat[out][k][i] = difference(out, y(h * e[k], {}), y(-h * e[k], {}), 2.0 * h);
                lo = std::min(lo, o.fhat[out][k][i]);
                hi = std::max(hi, o.fhat[out][k][i]);
                const double v = std::abs(o.fhat[out][k][i] - a[k]) / o.eps[i];
                if (v > o.worst_b) {
                    o.worst_b = v;
                    o.worst_b_out = static_cast<int>(out);
                    o.worst_b_k = static_cast<int>(k);
                    o.worst_b_i = i;
                }
            }
            o.band = std::max(o.band, hi - lo);
            if (a[k] < lo || a[k] > hi) o.inside_raw_spread = false;
            for (std::size_t i = 0; i < kHvs.size(); ++i) {
                const double hv = kHvs[i];
                o.fhat_v[out][k][i] = difference(out, y({}, hv * e[k]), y({}, -hv * e[k]), 2.0 * hv);
                o.worst_c = std::max(o.worst_c, std::abs(o.fhat_v[out][k][i]) / (nu_a() / hv));
            }
        }
    }
    return o;
}

std::string label(const AngleFrozen& fz, AngleConfig cfg, double radial) {
    std::ostringstream s;
    s << fz.name << ", " << angle_config_name(cfg) << ", " << direction_name(radial) << " the line of sight";
    return s.str();
}

void report(const std::string& what, const AngleOutcome& o) {
    double b_pred = *std::max_element(o.eps.begin(), o.eps.end());
    WARN(std::setprecision(4) << what << ": band B = " << o.band << " (B_pred " << b_pred << ", ratio " << o.band / b_pred << "); worst |f̂ − a|/ε = " << o.worst_b << " (output " << o.worst_b_out
                              << ", component " << o.worst_b_k << ", h = " << kHs[o.worst_b_i] << " m); worst |f̂_v|/(ν_a/h_v) = " << o.worst_c << "; analytic row inside the raw spread: "
                              << (o.inside_raw_spread ? "yes" : "no"));
}

/// "the test holds the frozen figures as literals AND recomputes them from the formulas and asserts they agree" (MEAS-A-096 …).
void check_frozen_figures(const AngleFrozen& fz) {
    CHECK_THAT(nu_a(), WithinRel(kNuAlit, 1e-4));
    CHECK_THAT(ulp_of(7.2e6), WithinRel(std::ldexp(1.0, -30), 1e-12));
    CHECK_THAT(ulp_of(2.0 * kPi), WithinRel(std::ldexp(1.0, -50), 1e-12));
    for (std::size_t i = 0; i < kHs.size(); ++i) {
        CHECK_THAT(f_a(fz, i), WithinRel(fz.f_a_lit[i], 1e-4));
        CHECK_THAT(eps_of(fz, i), WithinRel(fz.eps_lit[i], 1e-3));
        CHECK_THAT(agreement_tolerance(i), WithinRel(kAgreementLit[i], 1e-3));
        CHECK(f_a(fz, i) > f_pure(kHs[i]));
    }
    double b_pred = 0.0;
    for (std::size_t i = 0; i < kHs.size(); ++i) b_pred = std::max(b_pred, eps_of(fz, i));
    CHECK_THAT(b_pred, WithinRel(fz.b_pred_lit, 1e-3));
    CHECK_THAT(f_pure(10.0), WithinRel(2.5748e-18, 1e-4));            // MEAS-P-27's family
    CHECK_THAT(f_pure(1000.0), WithinRel(2.5866e-18, 1e-4));
}

/// The gate's three criteria on one outcome, against the frozen sizing.
void check_criteria(const AngleFrozen& fz, const AngleOutcome& o) {
    const double b_pred = *std::max_element(o.eps.begin(), o.eps.end());
    CHECK(o.band >= b_pred / 10.0);                                  // (a)
    CHECK(o.band <= b_pred * 10.0);
    CHECK(o.worst_b <= 1.0);                                         // (b)
    for (std::size_t out = 0; out < 2; ++out)
        for (std::size_t k = 0; k < 3; ++k) CHECK(o.analytic_v[out][k] == 0.0);     // (c)
    CHECK(o.worst_c <= 1.0);
    (void)fz;
}

struct Control {
    const char* name;
    RowOptions options;
    double multiple;
    bool across_only;
    /// false: computed and logged beside the others, NOT asserted. The control without the diurnal aberration's Jacobian had no power at the geometry the rule selected (0.47 epsilon for
    /// the 100 asked: the line of sight is perpendicular to the station's velocity, so the Jacobian's first-order effect vanishes); SPEC-measmod 6.2 (v), amended after the run by the
    /// manager's ruling (plan/subplan_L6/L6-4.md, 7baf3ad): the run stands as run, and the control's claim moves to MEAS-A-098b (angle_diurnal_tests.cpp).
    bool asserted = true;
    bool applies_to(AngleGate g) const {
        if (!options.annual_jacobian) return g == AngleGate::Astrometric || g == AngleGate::AstrometricOfDate;
        if (!options.dzo_dzv) return g == AngleGate::ApparentRefracted;
        if (!options.frame_rotation) return g == AngleGate::AstrometricOfDate;
        if (!options.diurnal_jacobian) return g == AngleGate::ApparentRefracted;
        return true;                                                 // the emission-epoch shift: every gate
    }
};

std::vector<Control> controls() {
    std::vector<Control> c;
    RowOptions o;
    o = RowOptions{}; o.annual_jacobian = false; c.push_back({"without the annual aberration's Jacobian D'", o, 1.0e3, false});
    o = RowOptions{}; o.dzo_dzv = false; c.push_back({"without dz_o/dz_v", o, 1.0e3, false});
    o = RowOptions{}; o.light_time_shift = false; c.push_back({"without the emission-epoch shift", o, 1.0e3, true});
    o = RowOptions{}; o.frame_rotation = false; c.push_back({"without the frame's rotation", o, 1.0e3, false});
    o = RowOptions{}; o.diurnal_jacobian = false; c.push_back({"without the diurnal aberration's Jacobian", o, 1.0e2, false, false});
    return c;
}

/// The whole gate of one group: both velocity directions, both configurations, the agreement of the configurations, and the controls.
void gate_group(const AngleFrozen& fz) {
    for (double radial : {0.0, 1.0}) {
        const AngleScenario sc(fz.gate, 30.0, kRho, kSpeed, radial);
        INFO(fz.name << ", " << direction_name(radial) << " the line of sight");
        CHECK(sc.azimuth_deg() == 0);
        CHECK_THAT(sc.slant_range_at_emission(), WithinRel(kRho, 1e-4));

        // the observers of the two configurations differ by at most 1e-6 m at the observation epoch: asserted before anything is compared
        const double ds = (sc.station_position(AngleConfig::RealChain) - sc.station_position(AngleConfig::ExactRigid)).norm();
        WARN(std::setprecision(4) << fz.name << ", " << direction_name(radial) << ": the real and rigid observers differ by " << ds << " m at the observation epoch (asserted below " << kObserversMax << " m)");
        REQUIRE(ds <= kObserversMax);

        std::array<AngleOutcome, 2> outcomes;
        for (AngleConfig cfg : {AngleConfig::RealChain, AngleConfig::ExactRigid}) {
            INFO(label(fz, cfg, radial));
            const AngleOutcome o = run_gate(fz, sc, cfg);
            outcomes[cfg == AngleConfig::RealChain ? 0 : 1] = o;
            report(label(fz, cfg, radial), o);
            check_criteria(fz, o);

            // the reconstruction of the row from the term functions agrees with the model's partials, before any control is read
            const std::array<Vec3, 2> rebuilt = assemble_row(sc, cfg, RowOptions{});
            for (std::size_t out = 0; out < 2; ++out) {
                CHECK_THAT((rebuilt[out] - o.analytic[out]).norm() / o.analytic[out].norm(), WithinAbs(0.0, 1e-9));
            }
            const double v_model = violation(o.fhat, o.analytic, o.eps);
            CHECK(v_model <= 1.0);

            // the controls (rule 5)
            for (const Control& c : controls()) {
                if (!c.applies_to(fz.gate)) continue;
                if (c.across_only && radial != 0.0) continue;
                const std::array<Vec3, 2> wrong = assemble_row(sc, cfg, c.options);
                const double rel = std::max((wrong[0] - o.analytic[0]).norm() / o.analytic[0].norm(), (wrong[1] - o.analytic[1]).norm() / o.analytic[1].norm());
                const double v = violation(o.fhat, wrong, o.eps);
                WARN(std::setprecision(4) << label(fz, cfg, radial) << ", control " << c.name << ": the row differs from the model's by " << rel << " of itself and fails (b) by " << v
                                          << " ε (" << (c.asserted ? "asked: " : "logged, not asserted — asked as first written: ") << c.multiple << ")");
                if (c.asserted) CHECK(v >= c.multiple);
            }
        }

        // the two configurations agree within the written tolerance
        double worst = 0.0;
        for (std::size_t out = 0; out < 2; ++out)
            for (std::size_t k = 0; k < 3; ++k)
                for (std::size_t i = 0; i < kHs.size(); ++i)
                    worst = std::max(worst, std::abs(outcomes[0].fhat[out][k][i] - outcomes[1].fhat[out][k][i]) / agreement_tolerance(i));
        WARN(std::setprecision(4) << fz.name << ", " << direction_name(radial) << ": the real chain and the rigid rotation agree to " << worst << " of the tolerance 2 ν_a/h + 3 δs/ρ'^2");
        CHECK(worst <= 1.0);
        // the analytic rows of the two configurations agree to arithmetic noise too
        for (std::size_t out = 0; out < 2; ++out) CHECK((outcomes[0].analytic[out] - outcomes[1].analytic[out]).norm() <= 1e-9 * outcomes[0].analytic[out].norm());
    }
}

}  // namespace

// ---- the gates --------------------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-096  G1, right ascension and declination, Astrometric (rho = 1.2e6 m, 30 deg elevation, 7.5 km/s across and along, the Earth's velocity from de440s), in both configurations of "
          "Amendment A1: criteria (a), (b), (c) of section 6.2 against the frozen angle sizing of 6.2 (v), the two configurations' agreement, and the controls",
          "[measmod][gate][anglegate][pregistered]") {
    SECTION("the frozen figures: the literals and the formulas agree") { check_frozen_figures(kAstrometric); }
    SECTION("the gate") { gate_group(kAstrometric); }
}

TEST_CASE("MEAS-A-097  G1, right ascension and declination, Geometric: the same", "[measmod][gate][anglegate][pregistered]") {
    SECTION("the frozen figures: the literals and the formulas agree") { check_frozen_figures(kGeometric); }
    SECTION("the gate") { gate_group(kGeometric); }
}

TEST_CASE("MEAS-A-098  G1, azimuth and elevation, ApparentRefracted (30 deg elevation; the diurnal aberration, the local frame and the refraction's dz_o/dz_v in the row): the same",
          "[measmod][gate][anglegate][pregistered]") {
    SECTION("the frozen figures: the literals and the formulas agree") { check_frozen_figures(kRefracted); }
    SECTION("the gate") { gate_group(kRefracted); }
}

TEST_CASE("MEAS-A-099  G1 for 'of date' (Astrometric, the true equator and equinox of the date: the frame's rotation inside the row), and the gate can fail (rule 5) for angles: the controls of "
          "6.2 (v) fail by their stated multiples in every gate and both configurations while the correct row passes",
          "[measmod][gate][anglegate][pregistered]") {
    SECTION("the frozen figures: the literals and the formulas agree") { check_frozen_figures(kOfDate); }
    SECTION("the gate") { gate_group(kOfDate); }
}
