// gate_tests.cpp — SPEC-measmod.md §6.2 (the finite-difference gate, G1, frozen before any run; Amendment A1 of 2026-10-06, made after the first run's miss) and §8.4:
// MEAS-A-042 … MEAS-A-046 for the range.
//
// The gate: central differences of the model's OUTPUT over a trajectory family that varies the target's state at the nominal bounce epoch by ±h e_k, TIME-FIXED,
// re-solving the light time for every varied trajectory, at five sizes h ∈ {10, 30, 100, 300, 1000} m for the position directions and four, {0.01, 0.1, 1, 10} m/s, for
// the velocity directions. The band B is the largest spread of the five position estimates over the three components.
//   (a) B_pred/10 <= B <= 10 B_pred                              — neither degenerate nor wild
//   (b) |f̂ₖ(h) − aₖ| <= εₖ(h) for every size and component       — the analytic row within the first-principles error of every estimate
//   (c) |f̂ₖ(h_v)| <= ν/h_v and aₖ = 0 exactly for the velocity columns
// with ε(h) = h² F(h)/6 + ν/h. THE SIZING IS THE AMENDED ONE (Amendment A1; the first sizing missed (b) in 7 of 8 cases and stands as first written in the specification):
//   F(h) = F_geo(h) + 1.01 F_trop(h) is a bound over the stencil for the whole modelled range, F_geo(h) = 1.1547005 (1 + κ)³ / (ρ (1 − κ) − h)², κ = (v + 2 v_s)/c;
//   ν = the first sizing's three ulp of the target's components, plus — in the REAL-CHAIN configuration only — the Earth-rotation angle's floor,
//   ν_chain = 3 u R⊥ |ĝ·ê| w, u = 2⁻⁴⁵ rad, w = ½ in event 2 (only the down leg's station epoch moves) and 1 in event 1.
// Both configurations are asserted: the REAL CHAIN (the model L7 will use) with the amended ν, and an EXACT RIGID ROTATION of that chain's own matrix (round-off only)
// with the first ν. The target's velocity is run in two directions per geometry (across the line of sight, and along it).
// The literals below are the specification's amended frozen numbers (tools/measmod_fd_sizing.cpp --scan --check reproduces them): they were written and committed
// BEFORE this file was built, and the test recomputes them from the formulas and asserts they agree.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/measmod/lighttime.hpp>
#include <odl/measmod/range.hpp>
#include <odl/measmod/shapiro.hpp>

#include "scenario.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <string>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using odl::Vec3;
using odl::time::Epoch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr std::array<double, 5> kHs = {{10.0, 30.0, 100.0, 300.0, 1000.0}};
constexpr std::array<double, 4> kHvs = {{0.01, 0.1, 1.0, 10.0}};
using Estimates = std::array<std::array<double, 5>, 3>;

constexpr double kUlpEra = 1.0 / 35184372088832.0;               // 2^-45 rad (2^45 = 35184372088832): one ulp of an Earth-rotation angle between 128 and 256 rad (2019 - 2039)
constexpr double kStationSpeedMax = 7.2921150e-5 * 6378137.0;    // m/s: the equatorial surface speed, which no station exceeds

enum class Config { RealChain, ExactRigid };
const char* config_name(Config c) { return c == Config::RealChain ? "real chain" : "exact rigid rotation"; }
const StationTrack& station_of(const Scenario& sc, Config c) { return c == Config::RealChain ? sc.station() : sc.rigid_station(); }
const char* event_name(EpochEvent e) { return e == EpochEvent::GroundTransmit ? "event 2 (the transmit time)" : "event 1 (the bounce time)"; }
const char* direction_name(double radial) { return radial == 0.0 ? "across" : "along"; }

/// A geometry of SPEC-measmod §6.2, with its first sizing (kept), and the numbers of Amendment A1.
struct Frozen {
    const char* name;
    double rho;                              ///< the slant range the sizing assumed, metres
    double operand_m;                        ///< the target's geocentric distance the first noise bound was sized on, metres
    double nu_printed;                       ///< the first sizing's per-evaluation noise bound as printed: three ulp of that operand, metres
    double f_first;                          ///< the first sizing's F = 1.1547/rho^2 as printed
    std::array<double, 5> eps_first;         ///< the first sizing's eps(h) as printed
    double elevation_deg, azimuth_rule_deg, target_m, speed_m_s;
    // Amendment A1
    std::array<double, 5> f_trop;            ///< F_trop(h): the troposphere's and Shapiro's third-derivative supremum over the stencil, m^-2 (tools/measmod_fd_sizing.cpp --scan)
    double nu_chain_lit[2];                  ///< [0] event 2, [1] event 1: the chain's bound of one evaluation, metres
    std::array<double, 5> eps_rigid, eps_real2, eps_real1;
    double b_pred_rigid, b_pred_real2, b_pred_real1;
};

const Frozen kLageos = {"LAGEOS-like", 6.6e6, 1.227e7, 5.59e-9, 2.651e-14, {{5.59e-10, 1.90e-10, 1.00e-10, 4.16e-10, 4.42e-9}}, 52.0, 105.0, 1.227e7, 5700.0,
                        {{9.6904e-20, 9.6905e-20, 9.6910e-20, 9.6924e-20, 9.6973e-20}}, {3.7910e-08, 7.5821e-08},
                        {{5.5924e-10, 1.9024e-10, 1.0007e-10, 4.1633e-10, 4.4255e-09}}, {{4.3503e-09, 1.4539e-09, 4.7917e-10, 5.4270e-10, 4.4634e-09}},
                        {{8.1413e-09, 2.7176e-09, 8.5827e-10, 6.6907e-10, 4.5013e-09}}, 4.4255e-09, 4.4634e-09, 8.1413e-09};
const Frozen kLeo = {"LEO-like", 1.5e6, 6.92e6, 2.79e-9, 5.132e-13, {{2.88e-10, 1.70e-10, 8.83e-10, 7.71e-9, 8.55e-8}}, 15.0, 0.0, 6.92e6, 7500.0,
                     {{7.7299e-16, 7.7314e-16, 7.7365e-16, 7.7512e-16, 7.8028e-16}}, {2.2981e-07, 4.5961e-07},
                     {{2.8796e-10, 1.7024e-10, 8.8481e-10, 7.7232e-09, 8.5794e-08}}, {{2.3269e-08, 7.8305e-09, 3.1829e-09, 8.4892e-09, 8.6024e-08}},
                     {{4.6249e-08, 1.5491e-08, 5.4809e-09, 9.2553e-09, 8.6253e-08}}, 8.5794e-08, 8.6024e-08, 8.6253e-08};

constexpr double kStationAxisDistance = 5580552.4;               // m: Yarragadee's system reference point's distance from the Earth's axis (tools/measmod_reference.cpp's registry section)
constexpr double kPi = 3.14159265358979323846;

double ulp_of(double x) { return std::nextafter(std::abs(x), std::numeric_limits<double>::infinity()) - std::abs(x); }
double nu_first(const Frozen& f) { return 3.0 * ulp_of(f.operand_m); }

double kappa_of(const Frozen& f) { return (f.speed_m_s + 2.0 * kStationSpeedMax) / kSpeedOfLight_m_s; }
double f_geo(const Frozen& f, double h) {
    const double k = kappa_of(f), d = f.rho * (1.0 - k) - h;
    return 1.1547005383792515 * std::pow(1.0 + k, 3.0) / (d * d);
}
double f_amended(const Frozen& f, std::size_t i) { return f_geo(f, kHs[i]) + 1.01 * f.f_trop[i]; }
double eps_amended(const Frozen& f, std::size_t i, double nu) { return kHs[i] * kHs[i] * f_amended(f, i) / 6.0 + nu / kHs[i]; }
double g_east_of(const Frozen& f) { return std::cos(f.elevation_deg * kPi / 180.0) * std::abs(std::cos(f.azimuth_rule_deg * kPi / 180.0)); }
double nu_chain_of(double r_perp, double g_east, EpochEvent ev) { return 3.0 * kUlpEra * r_perp * g_east * (ev == EpochEvent::GroundTransmit ? 0.5 : 1.0); }
double nu_of(const Frozen& f, Config c, EpochEvent ev, double r_perp, double g_east) { return nu_first(f) + (c == Config::RealChain ? nu_chain_of(r_perp, g_east, ev) : 0.0); }
std::array<double, 5> eps_table(const Frozen& f, double nu) {
    std::array<double, 5> e{};
    for (std::size_t i = 0; i < 5; ++i) e[i] = eps_amended(f, i, nu);
    return e;
}
double b_pred_of(const std::array<double, 5>& eps) { return *std::max_element(eps.begin(), eps.end()); }

/// Everything one run of the gate on one geometry in one configuration produces, and the two candidate rows a negative control uses.
struct Outcome {
    Estimates fhat{};
    std::array<std::array<double, 4>, 3> fhat_v{};
    Vec3 analytic, geometric, other_formula;
    double rel_geometric = 0.0, rel_other = 0.0;       ///< ‖wrong row − the model's row‖ / ‖the model's row‖
    std::array<double, 3> analytic_v{};
    double band = 0.0;
    double worst_b = 0.0;                              ///< max over k, i of |f̂ − a| / ε
    double worst_c = 0.0;                              ///< max over k, i of |f̂_v| / (ν/h_v)
    bool inside_raw_spread = true;
    double slant_up = 0.0;
    double target_radius = 0.0;
    double target_largest_component = 0.0;
    int azimuth_deg = 0;
    double geometry_score = 0.0;
    double g_east = 0.0;                               ///< |ĝ·ê| measured on the scenario
    double r_perp = 0.0;                               ///< the station's distance from the Earth's axis, metres
    double nu = 0.0;                                   ///< the configuration's noise bound for this case
    std::array<double, 5> eps{};                       ///< the amended ε(h) of this case and configuration
};

/// The violation of criterion (b) by a candidate row: max over k, i of |f̂ − a_candidate| / ε.
double violation(const Estimates& fhat, const Vec3& candidate, const std::array<double, 5>& eps) {
    const double a[3] = {candidate.x, candidate.y, candidate.z};
    double worst = 0.0;
    for (std::size_t k = 0; k < 3; ++k)
        for (std::size_t i = 0; i < kHs.size(); ++i) worst = std::max(worst, std::abs(fhat[k][i] - a[k]) / eps[i]);
    return worst;
}

double relative_difference(const Vec3& wrong, const Vec3& right) { return (wrong - right).norm() / right.norm(); }

/// The gate on the geometry of `fz` in configuration `cfg`: nominal solve, then the central differences of range_m() over the drift family. `radial_fraction` is the part
/// of the speed along the line of sight.
Outcome run_gate(const Frozen& fz, EpochEvent event, double radial_fraction, Config cfg) {
    Outcome o;
    const Scenario sc(first_real_observation(), event, fz.elevation_deg, fz.rho, fz.speed_m_s, radial_fraction);
    const StationTrack& station = station_of(sc, cfg);
    const RangeObservation& obs = sc.obs();
    auto nominal = model_range(obs, station, sc.trajectory(sc.tag()));
    REQUIRE(nominal.has_value());
    const Epoch t_star = nominal->bounce_tt();            // the epoch of the variation: the nominal bounce
    for (std::size_t k = 0; k < 3; ++k) o.analytic_v[k] = nominal->partials().d[0][k + 3];
    o.analytic = Vec3{nominal->partials().d[0][0], nominal->partials().d[0][1], nominal->partials().d[0][2]};
    o.geometric = 0.5 * (nominal->applied().up.direction + nominal->applied().down.direction);
    o.slant_up = nominal->applied().up.geometric_range_m;
    o.target_radius = sc.target_position().norm();
    const Vec3 r = sc.target_position();
    o.target_largest_component = std::max({std::abs(r.x), std::abs(r.y), std::abs(r.z)});
    o.azimuth_deg = sc.azimuth_deg();
    o.geometry_score = sc.geometry_score();
    o.g_east = std::abs(sc.line_of_sight().dot(sc.east()));
    o.r_perp = std::hypot(obs.site.srp_itrs_m.x, obs.site.srp_itrs_m.y);
    o.nu = nu_of(fz, cfg, event, o.r_perp, o.g_east);
    o.eps = eps_table(fz, o.nu);

    // the very solution the model's row was made from, assembled with the OTHER event's formula (a negative control of MEAS-A-044)
    auto solved = solve_two_way(event, obs.epoch, station, sc.trajectory(sc.tag()), model_extra_for(obs));
    REQUIRE(solved.has_value());
    const Vec3 same = range_partial_position(*solved, event);
    CHECK_THAT(same.x, WithinAbs(o.analytic.x, 1e-15));
    CHECK_THAT(same.y, WithinAbs(o.analytic.y, 1e-15));
    CHECK_THAT(same.z, WithinAbs(o.analytic.z, 1e-15));
    o.other_formula = range_partial_position(*solved, event == EpochEvent::GroundTransmit ? EpochEvent::Bounce : EpochEvent::GroundTransmit);
    o.rel_geometric = relative_difference(o.geometric, o.analytic);
    o.rel_other = relative_difference(o.other_formula, o.analytic);

    auto y = [&](const Vec3& dr, const Vec3& dv) {
        auto m = model_range(obs, station, sc.trajectory(t_star, dr, dv));
        REQUIRE(m.has_value());
        return m->range_m();
    };
    const Vec3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const double a[3] = {o.analytic.x, o.analytic.y, o.analytic.z};
    for (std::size_t k = 0; k < 3; ++k) {
        double lo = std::numeric_limits<double>::infinity(), hi = -lo;
        for (std::size_t i = 0; i < kHs.size(); ++i) {
            const double h = kHs[i];
            o.fhat[k][i] = (y(h * e[k], {}) - y(-h * e[k], {})) / (2.0 * h);
            lo = std::min(lo, o.fhat[k][i]);
            hi = std::max(hi, o.fhat[k][i]);
            o.worst_b = std::max(o.worst_b, std::abs(o.fhat[k][i] - a[k]) / o.eps[i]);
        }
        o.band = std::max(o.band, hi - lo);
        if (a[k] < lo || a[k] > hi) o.inside_raw_spread = false;
        for (std::size_t i = 0; i < kHvs.size(); ++i) {
            const double hv = kHvs[i];
            o.fhat_v[k][i] = (y({}, hv * e[k]) - y({}, -hv * e[k])) / (2.0 * hv);
            o.worst_c = std::max(o.worst_c, std::abs(o.fhat_v[k][i]) / (o.nu / hv));
        }
    }
    return o;
}

void report(const std::string& what, const Outcome& o, const Frozen& fz) {
    (void)fz;
    WARN(std::setprecision(4) << what << ": nu " << o.nu << ", band B = " << o.band << " (B_pred " << b_pred_of(o.eps) << ", ratio " << o.band / b_pred_of(o.eps)
                              << "); worst |f̂ − a|/ε = " << o.worst_b << "; worst |f̂_v|/(ν/h_v) = " << o.worst_c << "; analytic row inside the raw spread: "
                              << (o.inside_raw_spread ? "yes" : "no") << "; azimuth " << o.azimuth_deg << " deg (score " << o.geometry_score << "), |g.e| " << o.g_east
                              << ", up-leg slant range " << o.slant_up << " m, target at " << o.target_radius << " m");
}

/// "the test holds the frozen figures as literals AND recomputes them from the formulas and asserts they agree" (MEAS-A-042): the first sizing, kept, and Amendment A1.
void check_frozen_figures(const Frozen& fz) {
    // the first sizing, as it was frozen
    CHECK_THAT(fz.f_first, WithinRel(1.1547005383792515 / (fz.rho * fz.rho), 5e-4));
    for (std::size_t i = 0; i < kHs.size(); ++i) CHECK_THAT(kHs[i] * kHs[i] * fz.f_first / 6.0 + fz.nu_printed / kHs[i], WithinRel(fz.eps_first[i], 0.01));
    CHECK_THAT(nu_first(fz), WithinRel(fz.nu_printed, 2e-3));
    // Amendment A1: the chain's floor and its carriage through the range
    CHECK_THAT(kUlpEra, WithinRel(std::ldexp(1.0, -45), 1e-12));
    CHECK_THAT(kUlpEra * kStationAxisDistance, WithinRel(1.586e-7, 2e-3));                                            // MEAS-P-22
    CHECK_THAT(nu_chain_of(kStationAxisDistance, g_east_of(fz), EpochEvent::GroundTransmit), WithinRel(fz.nu_chain_lit[0], 2e-3));
    CHECK_THAT(nu_chain_of(kStationAxisDistance, g_east_of(fz), EpochEvent::Bounce), WithinRel(fz.nu_chain_lit[1], 2e-3));
    // the amended ε(h) of the three columns and their B_pred
    const auto rigid = eps_table(fz, nu_first(fz));
    const auto real2 = eps_table(fz, nu_first(fz) + nu_chain_of(kStationAxisDistance, g_east_of(fz), EpochEvent::GroundTransmit));
    const auto real1 = eps_table(fz, nu_first(fz) + nu_chain_of(kStationAxisDistance, g_east_of(fz), EpochEvent::Bounce));
    for (std::size_t i = 0; i < kHs.size(); ++i) {
        CHECK_THAT(rigid[i], WithinRel(fz.eps_rigid[i], 2e-3));
        CHECK_THAT(real2[i], WithinRel(fz.eps_real2[i], 2e-3));
        CHECK_THAT(real1[i], WithinRel(fz.eps_real1[i], 2e-3));
    }
    CHECK_THAT(b_pred_of(rigid), WithinRel(fz.b_pred_rigid, 2e-3));
    CHECK_THAT(b_pred_of(real2), WithinRel(fz.b_pred_real2, 2e-3));
    CHECK_THAT(b_pred_of(real1), WithinRel(fz.b_pred_real1, 2e-3));
    // F over the stencil is never below F at its centre, and the troposphere's share is what the specification says (3.7e-6 of F_geo at 52 deg, 1.5e-3 at 15 deg)
    for (std::size_t i = 0; i < kHs.size(); ++i) CHECK(f_amended(fz, i) > 1.1547005383792515 / (fz.rho * fz.rho));
    CHECK_THAT(fz.f_trop[0] / f_geo(fz, 0.0), WithinRel(fz.elevation_deg > 30.0 ? 3.66e-6 : 1.51e-3, 0.02));
}

/// The gate's three criteria on one outcome, against the amended sizing of its configuration.
void check_criteria(const Frozen& fz, const Outcome& o) {
    // the geometry is the one the table names, at the azimuth the frozen rule selects
    CHECK_THAT(o.slant_up, WithinRel(fz.rho, 0.01));
    CHECK_THAT(o.target_radius, WithinRel(fz.target_m, 0.01));
    CHECK(o.azimuth_deg == static_cast<int>(fz.azimuth_rule_deg));
    CHECK_THAT(o.g_east, WithinRel(g_east_of(fz), 5e-3));
    CHECK_THAT(o.r_perp, WithinRel(kStationAxisDistance, 2e-8));
    // the first noise bound bounds three ulp of the largest component of THIS target (the components, not the radius, are what the position sums add)
    CHECK(3.0 * ulp_of(o.target_largest_component) <= nu_first(fz) * 1.005);
    // (a) the band is neither degenerate nor wild
    const double b_pred = b_pred_of(o.eps);
    CHECK(o.band >= b_pred / 10.0);
    CHECK(o.band <= b_pred * 10.0);
    // (b) the analytic row lies within the first-principles error of every estimate
    CHECK(o.worst_b <= 1.0);
    // (c) the velocity columns: exactly zero, and every estimate within the noise bound
    for (std::size_t k = 0; k < 3; ++k) CHECK(o.analytic_v[k] == 0.0);
    CHECK(o.worst_c <= 1.0);
}

void gate_in_configuration(const Frozen& fz, Config cfg) {
    for (EpochEvent ev : {EpochEvent::GroundTransmit, EpochEvent::Bounce}) {
        for (double radial : {0.0, 1.0}) {
            INFO(fz.name << ", " << config_name(cfg) << ", " << event_name(ev) << ", " << direction_name(radial) << " the line of sight");
            const Outcome o = run_gate(fz, ev, radial, cfg);
            report(std::string(fz.name) + ", " + config_name(cfg) + ", " + (ev == EpochEvent::GroundTransmit ? "event 2" : "event 1") + ", " + direction_name(radial), o, fz);
            check_criteria(fz, o);
        }
    }
}

}  // namespace

TEST_CASE("MEAS-A-042  G1, range, LAGEOS-like (slant range 6.6e6 m, target at 1.227e7 m, elevation 52 deg, speed 5.7 km/s across the line of sight and along it; the full "
          "model with atmosphere and Shapiro; the Earth-fixed station), events 1 and 2, in both configurations of Amendment A1 (the real chain; an exact rigid rotation): "
          "criteria (a), (b), (c) of section 6.2 against the amended sizing",
          "[measmod][gate][range]") {
    SECTION("the amended figures: the literals and the formulas agree") { check_frozen_figures(kLageos); }
    SECTION("the real chain, amended nu") { gate_in_configuration(kLageos, Config::RealChain); }
    SECTION("an exact rigid rotation, nu as first frozen") { gate_in_configuration(kLageos, Config::ExactRigid); }
}

TEST_CASE("MEAS-A-043  G1, range, LEO-like (slant range 1.5e6 m, target at 6.92e6 m, elevation 15 deg, so the mapping function's derivative matters; speed 7.5 km/s, "
          "both directions), both events, in both configurations: the same criteria",
          "[measmod][gate][range]") {
    SECTION("the amended figures: the literals and the formulas agree") { check_frozen_figures(kLeo); }
    SECTION("the real chain, amended nu") { gate_in_configuration(kLeo, Config::RealChain); }
    SECTION("an exact rigid rotation, nu as first frozen") { gate_in_configuration(kLeo, Config::ExactRigid); }
}

namespace {

void controls_in_configuration(Config cfg) {
    for (const Frozen* fz : {&kLageos, &kLeo}) {
        for (EpochEvent ev : {EpochEvent::GroundTransmit, EpochEvent::Bounce}) {
            for (double radial : {1.0, 0.0}) {
                INFO(fz->name << ", " << config_name(cfg) << ", " << event_name(ev) << ", " << direction_name(radial) << " the line of sight");
                const Outcome o = run_gate(*fz, ev, radial, cfg);
                const double v_right = violation(o.fhat, o.analytic, o.eps);
                const double v_geometric = violation(o.fhat, o.geometric, o.eps);
                const double v_other = violation(o.fhat, o.other_formula, o.eps);
                WARN(std::setprecision(4) << fz->name << ", " << config_name(cfg) << ", " << event_name(ev) << ", " << direction_name(radial) << ": violation of (b) in units of epsilon — "
                                          << "the model's row " << v_right << "; the geometric row " << v_geometric << " (differs from the model's by " << o.rel_geometric
                                          << " relative); the other event's formula " << v_other << " (differs by " << o.rel_other << ")");
                // the correct row passes the assertion every candidate is held to ...
                CHECK(v_right <= 1.0);
                // ... every wrong row fails it ...
                CHECK(v_geometric > 1.0);
                CHECK(v_other > 1.0);
                // ... a wrong row that is wrong at the level of v/c (1.9e-5, 2.5e-5) or more fails it by at least 1000 times epsilon, whichever configuration ...
                if (o.rel_geometric >= 1e-5) CHECK(v_geometric >= 1000.0);
                if (o.rel_other >= 1e-5) CHECK(v_other >= 1000.0);
                // ... and in the rigid configuration both wrong rows of every along-the-line-of-sight case fail by at least 1000 times epsilon, as first written
                if (cfg == Config::ExactRigid && radial == 1.0) {
                    CHECK(v_geometric >= 1000.0);
                    CHECK(v_other >= 1000.0);
                }
            }
        }
    }
}

}  // namespace

TEST_CASE("MEAS-A-044  the gate can fail (rule 5), in both configurations of Amendment A1: the model's row passes (b); the geometric row 1/2 (g_u + g_d) and the row of the "
          "other event's formula each fail it; every wrong row that differs from the model's by 1e-5 relative or more fails by at least 1000 times epsilon; in the rigid "
          "configuration both wrong rows of every along-the-line-of-sight case do",
          "[measmod][gate][range]") {
    SECTION("the real chain, amended nu") { controls_in_configuration(Config::RealChain); }
    SECTION("an exact rigid rotation, nu as first frozen") { controls_in_configuration(Config::ExactRigid); }
}

namespace {

void boundary_in_configuration(Config cfg) {
    const Frozen& fz = kLageos;
    for (EpochEvent ev : {EpochEvent::GroundTransmit, EpochEvent::Bounce}) {
        INFO(config_name(cfg) << ", " << event_name(ev));
        const Scenario sc(first_real_observation(), ev, fz.elevation_deg, fz.rho, fz.speed_m_s, 1.0);
        const StationTrack& station = station_of(sc, cfg);
        const double r_perp = std::hypot(sc.obs().site.srp_itrs_m.x, sc.obs().site.srp_itrs_m.y);
        const double nu = nu_of(fz, cfg, ev, r_perp, std::abs(sc.line_of_sight().dot(sc.east())));
        const auto eps = eps_table(fz, nu);
        auto nominal = model_range(sc.obs(), station, sc.trajectory(sc.tag()));
        REQUIRE(nominal.has_value());
        const Epoch t_b = nominal->bounce_tt();
        const Epoch t0 = t_b.add(odl::time::Duration::from_seconds(-600.0));         // the state is parametrised 600 s before the bounce
        const double dt = t_b.difference(t0).to_seconds();
        CHECK_THAT(dt, WithinAbs(600.0, 1e-9));
        const Vec3 a{nominal->partials().d[0][0], nominal->partials().d[0][1], nominal->partials().d[0][2]};
        const double ak[3] = {a.x, a.y, a.z};
        // Phi(t_b, t0) = [[I, dt I], [0, I]] is exact for the drift (the acceleration is the nominal's for every varied trajectory): the mapped row is [a, dt a].
        // The differences are taken with respect to the state AT t0: positions by h, velocities by h/dt, so that the displacement at t_b is h in both.
        auto y = [&](const Vec3& dr, const Vec3& dv) {
            auto m = model_range(sc.obs(), station, sc.trajectory(t0, dr, dv));
            REQUIRE(m.has_value());
            return m->range_m();
        };
        const Vec3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        Estimates fr{};
        double worst_r = 0.0, worst_v = 0.0;
        for (std::size_t k = 0; k < 3; ++k) {
            for (std::size_t i = 0; i < kHs.size(); ++i) {
                const double h = kHs[i];
                fr[k][i] = (y(h * e[k], {}) - y(-h * e[k], {})) / (2.0 * h);
                const double hv = h / dt;
                const double fv = (y({}, hv * e[k]) - y({}, -hv * e[k])) / (2.0 * hv);
                worst_r = std::max(worst_r, std::abs(fr[k][i] - ak[k]) / eps[i]);
                worst_v = std::max(worst_v, std::abs(fv - dt * ak[k]) / (dt * eps[i]));
            }
        }
        WARN(std::setprecision(4) << config_name(cfg) << ", " << event_name(ev) << ": nu " << nu << ", worst |f̂_r0 − a|/ε = " << worst_r << ", worst |f̂_v0 − dt a|/(dt ε) = " << worst_v);
        CHECK(worst_r <= 1.0);
        CHECK(worst_v <= 1.0);

        if (ev == EpochEvent::GroundTransmit) {
            // DOUBLE-COUNTING (event 2 only: with event 1 the tag IS the bounce, and the bounce epoch depends on nothing). A row that exported the bounce epoch's
            // dependence dt_b/dx in ADDITION to the folded-in one and was mapped by L7 with the same Phi would add (dR/dt_b) (dt_b/dx) once more. dR/dt_b is the
            // range rate along the nominal trajectory (the model's output differenced over its own tag), dt_b/dx = a_u/(c − a_u.v_b).
            auto solved = solve_two_way(ev, sc.obs().epoch, station, sc.trajectory(sc.tag()), model_extra_for(sc.obs()));
            REQUIRE(solved.has_value());
            const Vec3 g_t = (1.0 / (kSpeedOfLight_m_s - solved->up.a.dot(solved->target_velocity_m_s))) * solved->up.a;
            RangeObservation later = sc.obs(), earlier = sc.obs();
            const double step = 1e-3;                                                  // seconds: the range rate is differenced over ±1 ms
            later.epoch = sc.obs().epoch.add(odl::time::Duration::from_seconds(step));
            earlier.epoch = sc.obs().epoch.add(odl::time::Duration::from_seconds(-step));
            auto r_plus = model_range(later, station, sc.trajectory(sc.tag()));
            auto r_minus = model_range(earlier, station, sc.trajectory(sc.tag()));
            REQUIRE(r_plus.has_value());
            REQUIRE(r_minus.has_value());
            const double range_rate = (r_plus->range_m() - r_minus->range_m()) / (2.0 * step);
            const Vec3 doubled = a + range_rate * g_t;
            const double rel = (doubled - a).norm() / a.norm();
            WARN(std::setprecision(4) << config_name(cfg) << ": range rate " << range_rate << " m/s, |dt_b/dx| " << g_t.norm() << " s/m: the double-counted row differs from the model's by "
                                      << rel << " relative; it violates (b) by " << violation(fr, doubled, eps) << " epsilon, the model's row by " << violation(fr, a, eps));
            CHECK_THAT(rel, WithinRel(5700.0 / kSpeedOfLight_m_s, 0.15));              // the range rate over c: 1.9e-5, the station's own radial velocity being the rest
            CHECK(violation(fr, doubled, eps) >= 1000.0);
            CHECK(violation(fr, a, eps) <= 1.0);
        }
    }
}

}  // namespace

TEST_CASE("MEAS-A-045  the boundary with L7, in both configurations of Amendment A1: with a drift trajectory parametrised at an epoch t0 of its own, the finite difference of "
          "the model with respect to the state at t0 equals the row mapped with Phi(t_b, t0) — the model's row is the whole of the model's contribution — and a row that also "
          "exported dt_b/dx and was mapped the same way would double-count by the range rate over c, and fails (b) by at least 1000 times epsilon",
          "[measmod][gate][range]") {
    SECTION("the real chain, amended nu") { boundary_in_configuration(Config::RealChain); }
    SECTION("an exact rigid rotation, nu as first frozen") { boundary_in_configuration(Config::ExactRigid); }
}

TEST_CASE("MEAS-A-046  the Earth-orientation chain's output noise floor: the real chain's station position minus an exact rigid rotation of its own matrix at its own rate, at "
          "601 epochs 1e-10 s apart about the tag, is at most 3 u R_perp (u = 2^-45 rad) at every epoch and at least 0.5 u R_perp at the peak",
          "[measmod][gate][range]") {
    const Scenario sc(first_real_observation(), EpochEvent::GroundTransmit, 15.0, 1.5e6, 7500.0, 0.0);
    const double r_perp = std::hypot(sc.obs().site.srp_itrs_m.x, sc.obs().site.srp_itrs_m.y);
    const double one_ulp = kUlpEra * r_perp;                                           // MEAS-P-22
    CHECK_THAT(one_ulp, WithinRel(1.586e-7, 2e-3));
    double peak = 0.0, sum2 = 0.0;
    int n = 0;
    for (int k = -300; k <= 300; ++k) {
        const Epoch t = sc.tag().add(odl::time::Duration::from_seconds(1e-10 * k));
        auto real = sc.station().at(t);
        auto rigid = sc.rigid_station().at(t);
        REQUIRE(real.has_value());
        REQUIRE(rigid.has_value());
        const double d = (real->position_m - rigid->position_m).norm();
        peak = std::max(peak, d);
        sum2 += d * d;
        ++n;
        CHECK(d <= 3.0 * one_ulp);                                                     // at every epoch
    }
    WARN(std::setprecision(4) << "the chain's floor: peak " << peak << " m = " << peak / one_ulp << " u R_perp (one ulp " << one_ulp << " m, three ulp " << 3.0 * one_ulp << " m), rms "
                              << std::sqrt(sum2 / n) << " m over " << n << " epochs");
    CHECK(peak <= 3.0 * one_ulp);
    CHECK(peak >= 0.5 * one_ulp);                                                      // the floor exists, and the bound is able to fail
}
