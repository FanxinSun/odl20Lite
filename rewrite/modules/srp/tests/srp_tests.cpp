// srp_tests.cpp — SPEC-photon-pressure §4.3, §8.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/core/units.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/srp/srp.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string_view>

using namespace odl;
using namespace odl::macromodel;
using odl::srp::Srp;
using Catch::Matchers::WithinRel;

namespace {

std::string slurp(const char* p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

const odl::time::LeapTable& leaps() {
    static const auto t = odl::time::LeapTable::parse(
        slurp(ODL_LEAP_SECOND_FILE), odl::time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    REQUIRE(t.has_value());
    return *t;
}

const eph::Ephemeris& ephemeris() {
    static const auto e = eph::Ephemeris::open({ODL_DE440S_BSP}, {});
    REQUIRE(e.has_value());
    return *e;
}

odl::time::Epoch epoch_at(int year, int month, int day, double hour) {
    odl::time::Calendar c;
    c.year = year; c.month = month; c.day = day;
    c.hour = static_cast<int>(hour); c.minute = 0; c.second = 0.0;
    auto e = odl::time::Epoch::from_calendar(odl::time::TimeScale::UTC, c, leaps());
    REQUIRE(e.has_value());
    return *e;
}

/// A one-panel, sun-pointing macromodel -- deliberately the simplest real
/// (non-cannonball) box-wing case, since srp_analytic's own suite already
/// covers the cannonball and full box-wing cases exhaustively.
Macromodel one_panel_model() {
    auto area = cited(4.0, "test-stated");
    auto absorptivity = cited(0.1, "test-stated");
    auto specular = cited(0.8, "test-stated");
    auto diffuse = cited(0.1, "test-stated");
    REQUIRE(area.has_value());
    REQUIRE(absorptivity.has_value());
    REQUIRE(specular.has_value());
    REQUIRE(diffuse.has_value());
    auto panel = flat_surface_sun_pointing(*area, *absorptivity, *specular, *diffuse);
    REQUIRE(panel.has_value());

    MacromodelBuilder b;
    b.add_surface(*panel);
    auto mass = cited(500.0, "test-stated");
    auto com = cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
    REQUIRE(mass.has_value());
    REQUIRE(com.has_value());
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto m = std::move(b).build();
    REQUIRE(m.has_value());
    return *m;
}

/// A cannonball -- `PHPR-A-006`'s own SECOND macromodel, added so the
/// sphere's own analytic Jacobian branch (`outer_plus_scaled_identity(e_D,
/// e_D, 1.0, ...)`, srp_analytic.cpp's own `spherical_force`) is checked by
/// something CI runs, not only by the scratch Python derivation that
/// verified it before either branch existed in production code -- the flat
/// branch alone (`one_panel_model`) never exercises this one.
Macromodel one_sphere_model() {
    auto area = cited(1.0, "test-stated");
    auto absorptivity = cited(0.3, "test-stated");
    auto specular = cited(0.3, "test-stated");
    auto diffuse = cited(0.4, "test-stated");
    REQUIRE(area.has_value());
    REQUIRE(absorptivity.has_value());
    REQUIRE(specular.has_value());
    REQUIRE(diffuse.has_value());
    SphericalSurface sph{*area, *absorptivity, *specular, *diffuse, std::nullopt};

    MacromodelBuilder b;
    b.add_surface(sph);
    auto mass = cited(500.0, "test-stated");
    auto com = cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
    REQUIRE(mass.has_value());
    REQUIRE(com.has_value());
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto m = std::move(b).build();
    REQUIRE(m.has_value());
    return *m;
}

}  // namespace

TEST_CASE("PHPR-A-015  Srp::accel is wired end-to-end at a stated LEO case: "
          "dyn::Force reached, a non-degenerate result, right order of magnitude",
          "[srp][gate]") {
    const Srp force(one_panel_model(), ephemeris(), leaps());

    CHECK(force.id().name == "srp");
    CHECK(force.consumes().empty());   // PHPR-R-005: no registered parameter

    const auto when = epoch_at(2015, 6, 21, 12.0);
    // A ~300 km circular-ish equatorial state -- not exactly circular (this
    // test does not need Kepler's equation, only a state that is not
    // degenerate), stated directly rather than propagated.
    const double r_m = odl::metres_from_km(6378.137 + 300.0);
    const double v_circ_m_s = std::sqrt(3.986004418e14 / r_m);   // stated GM, EGM2008's own
    const frames::Position<frames::Frame::GCRS> r{Vec3{r_m, 0.0, 0.0}};
    const Vec3 v{0.0, v_circ_m_s, 0.0};

    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);

    auto result = force.accel(when, r, v, params, reg);
    REQUIRE(result.has_value());

    const double a_norm = result->acceleration.norm_m_per_s2();
    // Order of magnitude only (this row's own tolerance, PHPR-A-015): SRP on
    // a 4 m^2/500 kg panel is of order 1367/(3e8) * (4/500) ~ 3.6e-8 m/s^2
    // at 1 au, times whatever this geometry's own shadow/incidence factors
    // are -- comfortably within two orders of magnitude either side.
    INFO("|a| = " << a_norm << " m/s^2");
    CHECK(a_norm > 1.0e-10);
    CHECK(a_norm < 1.0e-6);

    // The velocity Jacobian is with_velocity, not no_velocity_dependence
    // (PHPR-R-007) -- and it is not all zero, which would be the same
    // defect as declaring the wrong kind outright.
    REQUIRE(result->d_state.d_velocity().has_value());
    const Mat3& da_dv = *result->d_state.d_velocity();
    double da_dv_norm = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) da_dv_norm += da_dv.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *
                                                   da_dv.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
    CHECK(da_dv_norm > 0.0);
}

namespace {

/// The same stated ~300 km equatorial state `PHPR-A-015` uses -- not
/// circular exactly, only non-degenerate, and shared here so `PHPR-A-006`
/// and `PHPR-A-017` measure the same geometry.
struct LeoState {
    frames::Position<frames::Frame::GCRS> r;
    Vec3 v;
};

LeoState leo_state() {
    const double r_m = odl::metres_from_km(6378.137 + 300.0);
    const double v_circ_m_s = std::sqrt(3.986004418e14 / r_m);
    return LeoState{frames::Position<frames::Frame::GCRS>{Vec3{r_m, 0.0, 0.0}}, Vec3{0.0, v_circ_m_s, 0.0}};
}

}  // namespace

namespace {

/// `PHPR-A-006`'s own shared check, run once per surface kind below: an
/// INDEPENDENT central finite difference of the whole plugin call, not the
/// same computation repeated (plan rule 5's own tautology trap, the shape
/// `PHPR-A-001`'s bit-identity proof and `DYN-Q-002` both already caught
/// elsewhere in this tree) -- this loop never reads `d_state` at the
/// bumped points, only `.acceleration`, so it cannot be checking
/// `accel_only`'s own analytic Jacobian against itself, only against six
/// independent evaluations of the force it differentiates.
void check_analytic_velocity_jacobian(const Macromodel& model, std::string_view label) {
    INFO(label);
    const Srp force(model, ephemeris(), leaps());
    const auto when = epoch_at(2015, 6, 21, 12.0);
    const LeoState s = leo_state();

    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);

    auto centre = force.accel(when, s.r, s.v, params, reg);
    REQUIRE(centre.has_value());
    REQUIRE(centre->d_state.d_velocity().has_value());
    const Mat3& analytic = *centre->d_state.d_velocity();

    // This test's own, unrelated to production: per-surface, PHPR-R-010's
    // own force law is EXACTLY AFFINE in v_rel (this file's own header
    // comment on `photon_force_and_velocity_jacobian` states why), and nothing
    // else `accel_only` computes along the way (attitude, shadow, distance-
    // scaled irradiance) depends on v at all -- so central-difference
    // TRUNCATION error is exactly zero at any step size here, and the only
    // error is floating-point cancellation in `plus - minus`, which SHRINKS
    // as the step grows. A larger step is therefore strictly better for
    // this particular check, not a tradeoff the way it is for `PHPR-A-017`'s
    // own d(a)/d(r) sweep (where the pipeline is NOT affine in r); 10 m/s
    // is still five orders of magnitude below c and three below this
    // orbit's own ~7.7 km/s, comfortably inside where nothing else in the
    // pipeline could plausibly respond nonlinearly to it.
    constexpr double kTestVelocityStepMPerS = 10.0;
    const Vec3 axis_unit[3] = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    Mat3 fd{};
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 dv = kTestVelocityStepMPerS * axis_unit[static_cast<std::size_t>(axis)];
        auto plus = force.accel(when, s.r, s.v + dv, params, reg);
        auto minus = force.accel(when, s.r, s.v - dv, params, reg);
        REQUIRE(plus.has_value());
        REQUIRE(minus.has_value());
        const Vec3 dcol = (1.0 / (2.0 * kTestVelocityStepMPerS)) *
            (plus->acceleration.metres_per_second_squared() - minus->acceleration.metres_per_second_squared());
        fd.r[0][static_cast<std::size_t>(axis)] = dcol.x;
        fd.r[1][static_cast<std::size_t>(axis)] = dcol.y;
        fd.r[2][static_cast<std::size_t>(axis)] = dcol.z;
    }

    double max_abs_diff = 0.0, max_abs_analytic = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            max_abs_diff = std::max(max_abs_diff, std::abs(analytic.r[i][j] - fd.r[i][j]));
            max_abs_analytic = std::max(max_abs_analytic, std::abs(analytic.r[i][j]));
        }
    }
    INFO("max|analytic - fd| = " << max_abs_diff << ", max|analytic entry| = " << max_abs_analytic);
    REQUIRE(max_abs_analytic > 0.0);
    CHECK(max_abs_diff / max_abs_analytic < 1.0e-6);
}

}  // namespace

TEST_CASE("PHPR-A-006  Srp::accel's own analytic d(a)/d(v) (PHPR-R-010) matches an "
          "INDEPENDENT central finite difference of the whole plugin call, on BOTH the "
          "flat-surface and spherical branches of the analytic Jacobian",
          "[srp][gate]") {
    // Two macromodels, not one: `photon_force_and_velocity_jacobian`'s own
    // two branches (flat_force's steady_direction/drag_q_pr form; spherical_
    // force's e_D-outer-e_D/q_pr form) are different closed forms, and a
    // panel-only model never reaches the sphere one -- the manager's own
    // finding: the sphere branch had been checked only in the scratch Python
    // derivation before either branch existed in code, never by a test CI
    // runs.
    check_analytic_velocity_jacobian(one_panel_model(), "flat surface (sun-pointing panel)");
    check_analytic_velocity_jacobian(one_sphere_model(), "spherical surface");
}

TEST_CASE("PHPR-A-017  kPositionStepM is SIZED, not guessed (plan rule 7): a step "
          "sweep of Srp::accel's own remaining finite difference, d(a)/d(r)",
          "[srp][gate]") {
    // Independent of `accel_only`'s own internal kPositionStepM entirely:
    // this sweep differences the PUBLIC `accel()` call's `.acceleration` at
    // ITS OWN chosen steps, exactly the way `accel_only`'s six bumps do
    // internally, but from outside and at many step sizes -- so it measures
    // the true pipeline's own error-vs-step shape, not an assumption about
    // it.  Expected shape (plan rule 7): truncation error ~h^2 falling as h
    // shrinks, until round-off (~machine-epsilon * |a| / h) takes over, with
    // a broad plateau of near-minimum, near-stable estimates in between --
    // production's own 100 m should sit inside that plateau, not at either
    // ragged end.
    const Srp force(one_panel_model(), ephemeris(), leaps());
    const auto when = epoch_at(2015, 6, 21, 12.0);
    const LeoState s = leo_state();

    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);
    const Vec3 x_hat{1.0, 0.0, 0.0};

    // One representative column (d(a)/d(r_x)) is enough to see the
    // truncation/round-off trade-off; the law is the same isotropic-in-r
    // pipeline (ephemeris lookup, 1/r^2 irradiance, shadow, attitude) along
    // every axis, so one axis's own shape stands for all three.
    constexpr int kNumSteps = 8;
    constexpr double kSteps[kNumSteps] = {1.0e4, 1.0e3, 3.0e2, 1.0e2, 3.0e1, 1.0e1, 1.0, 1.0e-1};
    double estimates[kNumSteps];
    for (int i = 0; i < kNumSteps; ++i) {
        const double h = kSteps[i];
        auto plus = force.accel(when, frames::Position<frames::Frame::GCRS>{s.r.metres() + h * x_hat}, s.v,
                                params, reg);
        auto minus = force.accel(when, frames::Position<frames::Frame::GCRS>{s.r.metres() - h * x_hat}, s.v,
                                 params, reg);
        REQUIRE(plus.has_value());
        REQUIRE(minus.has_value());
        const Vec3 dcol = (1.0 / (2.0 * h)) * (plus->acceleration.metres_per_second_squared() -
                                               minus->acceleration.metres_per_second_squared());
        estimates[i] = dcol.norm();
        INFO("h=" << h << " m  |d(a)/d(r_x)| = " << estimates[i]);
    }

    // The plateau claim: 100 m (index 3) agrees with its neighbours (30 m,
    // index 4; 1000 m, index 2) far more closely than the sweep's own
    // extremes disagree with each other -- direct evidence 100 m is not
    // sitting on the truncation-dominated slope (where large-h neighbours
    // would disagree with it by a lot, ~h^2-shaped) or in round-off noise
    // (where small-h neighbours would disagree unpredictably).
    const double at_100 = estimates[3];
    REQUIRE(at_100 > 0.0);
    const double rel_to_30 = std::abs(estimates[4] - at_100) / at_100;
    const double rel_to_1000 = std::abs(estimates[2] - at_100) / at_100;
    INFO("relative change 100m->30m: " << rel_to_30 << ", 100m->1000m: " << rel_to_1000);
    CHECK(rel_to_30 < 1.0e-3);
    CHECK(rel_to_1000 < 1.0e-3);

    // The extremes of this sweep are each far LESS self-consistent with
    // their own nearest neighbour than 100 m is with its neighbours above --
    // the plateau has visible edges, not a flat line all the way out.
    const double edge_low_rel = std::abs(estimates[kNumSteps - 1] - estimates[kNumSteps - 2]) /
        estimates[kNumSteps - 2];
    const double edge_high_rel = std::abs(estimates[0] - estimates[1]) / estimates[1];
    INFO("edge (small h) relative step-to-step change: " << edge_low_rel
         << ", edge (large h): " << edge_high_rel);
    CHECK(edge_low_rel > rel_to_30);
}

TEST_CASE("PHPR-A-004  the SRP plugin's distance-scaled irradiance: perihelion and aphelion "
          "differ from the fixed-1367 baseline by the stated +3.43%/-3.26%, and Srp::accel's "
          "own force magnitude scales by very nearly the same ratio",
          "[srp][gate]") {
    // Two dates near Earth's own perihelion (~Jan 3) and aphelion (~Jul 4);
    // the exact day does not matter, only that each sits close to its own
    // extremum (PHPR-P-5/A-004's own stated figures are themselves close-to,
    // not exact-day, values).
    const auto near_perihelion = epoch_at(2015, 1, 3, 0.0);
    const auto near_aphelion = epoch_at(2015, 7, 4, 0.0);

    auto sun_at = [](const odl::time::Epoch& t) {
        auto s = ephemeris().geocentric_state(eph::Body::Sun, t, leaps());
        REQUIRE(s.has_value());
        return odl::metres_from_km(s->position());
    };
    const Vec3 r_sun_peri = sun_at(near_perihelion);
    const Vec3 r_sun_aph = sun_at(near_aphelion);
    const double au_m = odl::metres_from_km(eph::Ephemeris::kAstronomicalUnitKm);
    const double ratio_peri = (au_m / r_sun_peri.norm()) * (au_m / r_sun_peri.norm()) - 1.0;
    const double ratio_aph = (au_m / r_sun_aph.norm()) * (au_m / r_sun_aph.norm()) - 1.0;
    INFO("perihelion (au/r)^2 - 1 = " << ratio_peri << ", aphelion = " << ratio_aph);
    CHECK(std::abs(ratio_peri - 0.0343) < 0.003);
    CHECK(std::abs(ratio_aph - (-0.0326)) < 0.003);

    // Cross-check: Srp::accel's own force magnitude, at the SAME relative
    // geometry (panel sun-pointing, satellite displaced perpendicular to the
    // Sun direction so neither date's own geometry is eclipsed), scales by
    // very nearly this same ratio between the two dates -- proving the
    // plugin actually APPLIES the distance scaling, not merely that Earth's
    // own orbit has this eccentricity (a fact independent of this tree's
    // own code, checked above only to state the baseline this row cites).
    auto force_norm_at = [&](const odl::time::Epoch& t, const Vec3& r_sun_m) {
        const Vec3 sun_hat = (1.0 / r_sun_m.norm()) * r_sun_m;
        Vec3 perp = sun_hat.cross(Vec3{0.0, 0.0, 1.0});
        perp = (1.0 / perp.norm()) * perp;
        const double r_m = odl::metres_from_km(6378.137 + 300.0);
        const frames::Position<frames::Frame::GCRS> r{r_m * perp};
        const double v_circ_m_s = std::sqrt(3.986004418e14 / r_m);
        const Vec3 v = v_circ_m_s * sun_hat.cross(perp);   // roughly circular, direction immaterial here
        const Srp force(one_panel_model(), ephemeris(), leaps());
        dyn::ParameterRegistry reg;
        dyn::ParameterSet params(reg);
        auto result = force.accel(t, r, v, params, reg);
        REQUIRE(result.has_value());
        return result->acceleration.norm_m_per_s2();
    };
    const double a_peri = force_norm_at(near_perihelion, r_sun_peri);
    const double a_aph = force_norm_at(near_aphelion, r_sun_aph);
    const double measured_ratio = (a_peri / a_aph) - 1.0;
    const double expected_ratio = (1.0 + ratio_peri) / (1.0 + ratio_aph) - 1.0;
    INFO("a_peri=" << a_peri << " a_aph=" << a_aph << " measured (a_peri/a_aph - 1)=" <<
         measured_ratio << " expected=" << expected_ratio);
    CHECK(std::abs(measured_ratio - expected_ratio) < 1.0e-6);
}

TEST_CASE("PHPR-A-005  the SRP plugin's shadow factor: fired (a satellite placed nearly "
          "directly behind Earth from the Sun) and shown not to fire (placed nearly directly "
          "sunward)", "[srp][gate]") {
    const auto when = epoch_at(2015, 6, 21, 12.0);
    auto sun_state = ephemeris().geocentric_state(eph::Body::Sun, when, leaps());
    REQUIRE(sun_state.has_value());
    const Vec3 r_sun_m = odl::metres_from_km(sun_state->position());
    const Vec3 sun_hat = (1.0 / r_sun_m.norm()) * r_sun_m;
    const double r_m = odl::metres_from_km(6378.137 + 300.0);
    const double v_circ_m_s = std::sqrt(3.986004418e14 / r_m);

    const Srp force(one_panel_model(), ephemeris(), leaps());
    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);

    Vec3 perp = sun_hat.cross(Vec3{0.0, 1.0, 0.0});
    if (perp.norm() < 1.0e-6) perp = sun_hat.cross(Vec3{1.0, 0.0, 0.0});
    perp = (1.0 / perp.norm()) * perp;
    // A small (~5.7 deg) offset from the exact Sun/anti-Sun line -- deep
    // enough into each hemisphere to be unambiguously lit or shadowed (the
    // umbra's own angular half-width at LEO altitude, atan(R_E/r) ~ 44 deg,
    // is far wider than this), while clear of `ATTD-F-001`'s own
    // Sun-on-nadir-axis singularity, which the EXACT line sits on: a
    // satellite exactly sunward or anti-sunward is exactly the noon/midnight
    // geometry SS4.2's own small-beta note names as where the ideal law's
    // required yaw rate diverges -- not this row's own concern, so this test
    // deliberately does not sit on that line either.
    constexpr double kOffsetRad = 0.1;
    const Vec3 sunlit_dir = std::cos(kOffsetRad) * sun_hat + std::sin(kOffsetRad) * perp;
    const Vec3 eclipse_dir = std::cos(kOffsetRad) * (-1.0 * sun_hat) + std::sin(kOffsetRad) * perp;

    // Attitude (`nominal_yaw_steering`) depends on r and the Sun direction
    // only, never on v -- any non-degenerate velocity is fine here; both
    // branches use the same simple one.
    const Vec3 v = v_circ_m_s * perp;

    // SUNWARD (offset): unambiguously lit.
    {
        const frames::Position<frames::Frame::GCRS> r{r_m * sunlit_dir};
        auto result = force.accel(when, r, v, params, reg);
        REQUIRE(result.has_value());
        const double a_norm = result->acceleration.norm_m_per_s2();
        INFO("sunlit |a| = " << a_norm);
        CHECK(a_norm > 1.0e-10);   // PHPR-A-015's own plausibility floor
    }
    // ANTI-SUNWARD (offset): deep umbra.
    {
        const frames::Position<frames::Frame::GCRS> r{r_m * eclipse_dir};
        auto result = force.accel(when, r, v, params, reg);
        REQUIRE(result.has_value());
        const double a_norm = result->acceleration.norm_m_per_s2();
        INFO("eclipsed |a| = " << a_norm);
        CHECK(a_norm == 0.0);   // shadow::conical's own umbra branch: fraction == 0.0 exactly
    }
}
