// jason_tests.cpp — SPEC-jason-attitude.md, JSAT-A-001..A-005.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/attitude/attitude.hpp>

#include <algorithm>
#include <cmath>

using namespace odl;
using namespace odl::attitude;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

/// An equatorial circular orbit (inclination 0, so n_hat = +Z exactly) --
/// chosen so beta = asin(s_hat.n_hat) = s_hat's own Z-component directly,
/// making beta-prime trivial to control precisely, the same simplification
/// `sentinel6_tests.cpp`'s own CircularOrbit makes for a different angle.
struct EquatorialOrbit {
    [[nodiscard]] static Vec3 r(double theta_rad) noexcept {
        return 7.0e6 * Vec3{std::cos(theta_rad), std::sin(theta_rad), 0.0};
    }
    [[nodiscard]] static Vec3 v(double theta_rad) noexcept {
        return 7500.0 * Vec3{-std::sin(theta_rad), std::cos(theta_rad), 0.0};
    }
};

[[nodiscard]] Vec3 sun_at_beta(double beta_rad) noexcept {
    return Vec3{std::cos(beta_rad), 0.0, std::sin(beta_rad)};
}

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

}  // namespace

// --- JSAT-A-002: fixed-yaw regime, both flight directions ------------------

TEST_CASE("JSAT-A-002  jason_attitude: fixed-yaw regime (|beta-prime| < 15 "
          "deg) builds X along-track (forward, beta-prime > 0) or "
          "anti-along-track (backward, beta-prime < 0), Y completing "
          "right-handed, Z nadir -- checked against an independently "
          "computed orbital triad, both signs of beta-prime",
          "[attitude][jason]") {
    for (double beta_deg : {5.0, -5.0, 10.0, -12.0}) {
        const double theta = 37.0 * kDegToRad;  // arbitrary
        const Vec3 r = EquatorialOrbit::r(theta), v = EquatorialOrbit::v(theta);
        const Vec3 sun = sun_at_beta(beta_deg * kDegToRad);

        JasonRegime regime{};
        auto m = jason_attitude(r, v, sun, &regime);
        REQUIRE(m.has_value());
        CHECK(regime == JasonRegime::FixedYaw);

        const Vec3 r_hat = normalized(r);
        const Vec3 n_hat = normalized(r.cross(v));
        const Vec3 t_hat = n_hat.cross(r_hat);
        const Vec3 x_body{m->r[0][0], m->r[0][1], m->r[0][2]};
        const Vec3 y_body{m->r[1][0], m->r[1][1], m->r[1][2]};
        const Vec3 z_body{m->r[2][0], m->r[2][1], m->r[2][2]};

        CHECK_THAT((z_body - (-1.0 * r_hat)).norm(), WithinAbs(0.0, 1.0e-9));
        if (beta_deg > 0.0) {
            CHECK_THAT((x_body - t_hat).norm(), WithinAbs(0.0, 1.0e-9));
            CHECK_THAT((y_body - (-1.0 * n_hat)).norm(), WithinAbs(0.0, 1.0e-9));
        } else {
            CHECK_THAT((x_body - (-1.0 * t_hat)).norm(), WithinAbs(0.0, 1.0e-9));
            CHECK_THAT((y_body - n_hat).norm(), WithinAbs(0.0, 1.0e-9));
        }
        // Orthonormal, right-handed regardless of branch.
        CHECK_THAT(m->determinant(), WithinAbs(1.0, 1.0e-9));
    }
}

// --- JSAT-A-001/A-003: yaw-steering reduces to nominal_yaw_steering --------

TEST_CASE("JSAT-A-001  jason_attitude: outside the fixed-yaw switch, "
          "beta-prime IS signed_beta_rad (checked via the regime boundary "
          "itself, not a separate exposed function -- no redundant public "
          "wrapper was built, jason.hpp's own header comment)",
          "[attitude][jason]") {
    const double theta = 12.0 * kDegToRad;
    const Vec3 r = EquatorialOrbit::r(theta), v = EquatorialOrbit::v(theta);
    // At beta=20deg (> the 15deg switch), the regime must be YawSteering;
    // at beta=10deg (< the switch), FixedYaw -- if beta-prime were computed
    // any other way (e.g. against r_hat instead of n_hat), this equatorial
    // geometry's own exact numbers would not line up this cleanly.
    JasonRegime regime_high{}, regime_low{};
    auto high = jason_attitude(r, v, sun_at_beta(20.0 * kDegToRad), &regime_high);
    auto low = jason_attitude(r, v, sun_at_beta(10.0 * kDegToRad), &regime_low);
    REQUIRE(high.has_value());
    REQUIRE(low.has_value());
    CHECK(regime_high == JasonRegime::YawSteering);
    CHECK(regime_low == JasonRegime::FixedYaw);
}

TEST_CASE("JSAT-A-003  jason_attitude: in the yaw-steering regime, EXACTLY "
          "reproduces nominal_yaw_steering(r, v, -sun) -- the negated-Sun "
          "reduction this file's own header comment derives from "
          "right-handedness, not a separately built frame construction",
          "[attitude][jason]") {
    const double theta = 200.0 * kDegToRad;
    const Vec3 r = EquatorialOrbit::r(theta), v = EquatorialOrbit::v(theta);
    const Vec3 sun = sun_at_beta(40.0 * kDegToRad);

    JasonRegime regime{};
    auto got = jason_attitude(r, v, sun, &regime);
    REQUIRE(got.has_value());
    CHECK(regime == JasonRegime::YawSteering);

    auto expected = nominal_yaw_steering(r, -1.0 * sun);
    REQUIRE(expected.has_value());
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            CHECK_THAT(got->r[i][j], WithinAbs(expected->r[i][j], 1.0e-12));

    // The source's own stated property, checked directly: +X points AWAY
    // from the real Sun (not toward it, the opposite of every GPS/Galileo/
    // QZSS yaw-steering law this tree has built).
    const Vec3 x_body{got->r[0][0], got->r[0][1], got->r[0][2]};
    CHECK(x_body.dot(normalized(sun)) < 0.0);
}

// --- JSAT-A-004: the mode-boundary guard shown firing -----------------------

TEST_CASE("JSAT-A-004  the guard shown firing: the fixed-yaw/yaw-steering "
          "dispatcher selects the correct regime on both sides of the 15 "
          "deg boundary and of beta-prime=0; a deliberately reversed "
          "threshold comparison disagrees with the real dispatcher at "
          "every geometry checked -- QZSY-A-003's own role, applied to "
          "this law's own different (approximate, no-hysteresis) switch",
          "[attitude][jason]") {
    const double theta = 88.0 * kDegToRad;
    const Vec3 r = EquatorialOrbit::r(theta), v = EquatorialOrbit::v(theta);
    const double betas_deg[] = {-40.0, -16.0, -14.0, -1.0, 1.0, 14.0, 16.0, 40.0};
    const JasonRegime expected[] = {
        JasonRegime::YawSteering, JasonRegime::YawSteering, JasonRegime::FixedYaw,
        JasonRegime::FixedYaw,    JasonRegime::FixedYaw,    JasonRegime::FixedYaw,
        JasonRegime::YawSteering, JasonRegime::YawSteering,
    };
    int disagreements = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        JasonRegime regime{};
        auto m = jason_attitude(r, v, sun_at_beta(betas_deg[i] * kDegToRad), &regime);
        REQUIRE(m.has_value());
        CHECK(regime == expected[i]);

        // The deliberately reversed comparison: '>' where production uses
        // '<' -- flips the verdict at every non-boundary point checked.
        const bool wrong_is_fixed_yaw = !(std::abs(betas_deg[i] * kDegToRad) < 15.0 * kDegToRad);
        const bool wrong_regime_is_fixed = wrong_is_fixed_yaw;
        const bool real_regime_is_fixed = (regime == JasonRegime::FixedYaw);
        if (wrong_regime_is_fixed != real_regime_is_fixed) ++disagreements;
    }
    CHECK(disagreements == 8);
}

// --- JSAT-A-005: the regime boundary is a genuine discontinuity ------------

TEST_CASE("JSAT-A-005  jason_attitude is genuinely DISCONTINUOUS at the "
          "regime boundary -- ramps and flips are NOT modelled (JSAT-R-004), "
          "proved by comparing the frame just below and just above the "
          "switch, not merely asserted in a comment: the jump is orders of "
          "magnitude larger than nominal_yaw_steering's own continuity "
          "gives for the SAME small geometry change within one regime",
          "[attitude][jason]") {
    const double theta = 55.0 * kDegToRad;
    const Vec3 r = EquatorialOrbit::r(theta), v = EquatorialOrbit::v(theta);

    JasonRegime below_regime{}, above_regime{};
    auto below = jason_attitude(r, v, sun_at_beta((15.0 - 0.01) * kDegToRad), &below_regime);
    auto above = jason_attitude(r, v, sun_at_beta((15.0 + 0.01) * kDegToRad), &above_regime);
    REQUIRE(below.has_value());
    REQUIRE(above.has_value());
    CHECK(below_regime == JasonRegime::FixedYaw);
    CHECK(above_regime == JasonRegime::YawSteering);

    const Vec3 x_below{below->r[0][0], below->r[0][1], below->r[0][2]};
    const Vec3 x_above{above->r[0][0], above->r[0][1], above->r[0][2]};
    const double jump_deg = std::acos(std::clamp(x_below.dot(x_above), -1.0, 1.0)) / kDegToRad;

    // A 0.02 deg change in beta-prime, entirely WITHIN one regime, moves
    // nominal_yaw_steering's own x_body by a comparably tiny amount --
    // the continuity baseline this test's own jump is measured against.
    auto within_regime_a = nominal_yaw_steering(r, sun_at_beta(40.0 * kDegToRad));
    auto within_regime_b = nominal_yaw_steering(r, sun_at_beta(40.02 * kDegToRad));
    REQUIRE(within_regime_a.has_value());
    REQUIRE(within_regime_b.has_value());
    const Vec3 xa{within_regime_a->r[0][0], within_regime_a->r[0][1], within_regime_a->r[0][2]};
    const Vec3 xb{within_regime_b->r[0][0], within_regime_b->r[0][1], within_regime_b->r[0][2]};
    const double smooth_change_deg = std::acos(std::clamp(xa.dot(xb), -1.0, 1.0)) / kDegToRad;

    CHECK(smooth_change_deg < 1.0);         // a smooth law barely moves
    CHECK(jump_deg > 10.0 * smooth_change_deg);  // the regime boundary jumps far more
}

// --- structural note: ATTD-F-001 is UNREACHABLE through this branch --------
//
// nominal_yaw_steering's own singularity requires the Sun within
// kMinAxisNorm of the NADIR axis (z_body = -r_hat), i.e. s_hat within a
// small angle epsilon of +/-r_hat. Since r_hat is ALWAYS perpendicular to
// n_hat (r_hat.n_hat = 0 exactly, orbit_triad's own construction), any
// s_hat within epsilon of +/-r_hat has |s_hat.n_hat| = O(epsilon) --
// |beta-prime| = O(epsilon), i.e. NEAR ZERO. Jason's own yaw-steering
// branch is reached only for |beta-prime| >= 15 deg (kJasonFixedYawSwitchRad,
// APPROXIMATE), far outside any such epsilon-neighbourhood -- so
// ATTD-F-001 is PROVABLY unreachable through `jason_attitude`'s own
// yaw-steering branch, a structural property of the two thresholds
// together (JSAT-Q-003 names this in the spec, not silently left untested
// because it happens to be hard to trigger).
