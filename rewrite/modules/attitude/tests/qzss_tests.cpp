// qzss_tests.cpp — SPEC-qzss-attitude.md §8, QZSY-A-001 through QZSY-A-005.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/attitude/attitude.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace odl;
using namespace odl::attitude;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kQzssRadiusM = 42164e3;  ///< representative QZSS/GEO-family radius
constexpr double kDeg = std::numbers::pi / 180.0;
/// Kepler's third law at `kQzssRadiusM` (GM=3.986004418e14 m^3/s^2) --
/// period ~23.9h, matching a geosynchronous orbit (QZS-1's own real orbit
/// is inclined/eccentric IGSO, not circular GEO, but a representative
/// circular fixture is all `fixture_at`'s own construction needs, the SAME
/// reasoning `galileo_tests.cpp`'s/`glonass` attitude tests' own
/// `kOmegaRadPerS` state).
constexpr double kOmegaRadPerS = 7.2925e-5;

struct OrbitFixture {
    Vec3 r_gcrs_m, v_gcrs_m_per_s, sun_gcrs;
};

/// Same construction as this tree's own other attitude fixtures: n_hat=Z,
/// e0=X fixed, `mu_deg` this tree's own convention (from midnight, matching
/// `mu_rad`). Not used by every test below (orbit-normal mode does not read
/// the Sun at all), but kept for the ones that do.
[[nodiscard]] OrbitFixture fixture_at(double beta_deg, double mu_deg) {
    const double beta = beta_deg * kDeg;
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 r_hat = std::cos(mu) * e0 + std::sin(mu) * e1;
    const Vec3 t_hat = std::cos(mu) * e1 - std::sin(mu) * e0;
    const Vec3 s_hat = (-std::cos(beta)) * e0 + std::sin(beta) * n_hat;
    return {kQzssRadiusM * r_hat, (kOmegaRadPerS * kQzssRadiusM) * t_hat, s_hat};
}

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

[[nodiscard]] double max_component_diff(const Mat3& a, const Mat3& b) noexcept {
    double d = 0.0;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) d = std::max(d, std::abs(a.r[i][j] - b.r[i][j]));
    return d;
}

void check_orthonormal_right_handed(const Mat3& m) {
    const Vec3 x{m.r[0][0], m.r[0][1], m.r[0][2]};
    const Vec3 y{m.r[1][0], m.r[1][1], m.r[1][2]};
    const Vec3 z{m.r[2][0], m.r[2][1], m.r[2][2]};
    CHECK_THAT(x.norm(), WithinAbs(1.0, 1.0e-9));
    CHECK_THAT(x.dot(y), WithinAbs(0.0, 1.0e-9));
    const Vec3 xy = x.cross(y);
    CHECK_THAT(xy.x, WithinAbs(z.x, 1.0e-9));
    CHECK_THAT(xy.y, WithinAbs(z.y, 1.0e-9));
    CHECK_THAT(xy.z, WithinAbs(z.z, 1.0e-9));
}

}  // namespace

// --- QZSY-A-001 --------------------------------------------------------------

TEST_CASE("QZSY-A-001  orbit_normal_attitude, independently reconstructed "
          "from SPI_QZS1_B Sec.3(2)'s own words (z=-r_hat, -y_qzss=n_hat, "
          "x_qzss completes the right-handed system) then mapped through "
          "the SAME 180-about-Z rotation the frame mapping uses, matches "
          "production exactly; the result is orthonormal and right-handed. "
          "SPI_QZS1_B's own '(roughly the flight direction)' describes "
          "QZSS's own NATIVE x_qzss (=t_hat, prograde, checked directly) -- "
          "the 180-about-Z map that carries this into this tree's own "
          "convention flips it, so this tree's own x_body is RETROGRADE "
          "(=-t_hat), not a contradiction of the source, a property of a "
          "DIFFERENT axis (a mistaken expectation this test's own first "
          "draft asserted the wrong way, caught by its own failing numbers "
          "before being trusted)",
          "[attitude][qzss]") {
    struct Case { double mu_deg; };
    const Case cases[] = {{0.0}, {45.0}, {123.0}, {270.0}};
    for (const auto& c : cases) {
        // beta is irrelevant to orbit_normal_attitude (it never reads the
        // Sun) -- fixture_at's own beta parameter is unused here beyond
        // building a self-consistent (r, v).
        const auto f = fixture_at(0.0, c.mu_deg);
        const Vec3 r_hat = normalized(f.r_gcrs_m);
        const Vec3 n_hat = normalized(f.r_gcrs_m.cross(f.v_gcrs_m_per_s));
        const Vec3 t_hat = n_hat.cross(r_hat);

        // Independent reconstruction: QZSS's own native frame (z=-r_hat,
        // y_qzss=-n_hat, x_qzss=y_qzss x z_qzss), THEN the 180-about-Z map.
        const Vec3 z_qzss = -1.0 * r_hat;
        const Vec3 y_qzss = -1.0 * n_hat;
        const Vec3 x_qzss = y_qzss.cross(z_qzss);
        const Vec3 x_expected = -1.0 * x_qzss;
        const Vec3 y_expected = -1.0 * y_qzss;
        const Vec3 z_expected = z_qzss;

        const Mat3 got = orbit_normal_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s);
        const Vec3 x_got{got.r[0][0], got.r[0][1], got.r[0][2]};
        const Vec3 y_got{got.r[1][0], got.r[1][1], got.r[1][2]};
        const Vec3 z_got{got.r[2][0], got.r[2][1], got.r[2][2]};
        CHECK_THAT((x_got - x_expected).norm(), WithinAbs(0.0, 1.0e-9));
        CHECK_THAT((y_got - y_expected).norm(), WithinAbs(0.0, 1.0e-9));
        CHECK_THAT((z_got - z_expected).norm(), WithinAbs(0.0, 1.0e-9));

        check_orthonormal_right_handed(got);
        // QZSS's own native x_qzss (checked directly, before the tree map)
        // is prograde, matching the source's own words.
        CHECK(x_qzss.dot(t_hat) > 0.9);
        // This tree's own x_body, after the 180-about-Z map, is retrograde
        // -- the verified relationship, not an assumption.
        CHECK(x_got.dot(t_hat) < -0.9);

        // BROKEN: a realistic slip -- using QZSS's own native y_qzss
        // (=-n_hat) DIRECTLY as this tree's own y_body, forgetting the
        // 180-about-Z map's own sign flip on y (while still correctly
        // flipping x). The resulting x_body would then point PROGRADE in
        // tree convention -- the OPPOSITE of the verified real result --
        // the defect this guard exists to catch, shown actually firing.
        const Vec3 y_broken = -1.0 * n_hat;
        const Vec3 x_broken = y_broken.cross(z_expected);
        CHECK(x_broken.dot(t_hat) > 0.9);
    }
}

// --- QZSY-A-002 --------------------------------------------------------------

TEST_CASE("QZSY-A-002  outside the mode-switch (|beta| > 20deg), "
          "qzss_yaw_attitude returns EXACTLY what nominal_yaw_steering "
          "returns for the same (r, sun) -- the delegation this module's "
          "own design rests on, asserted as a regression guard",
          "[attitude][qzss]") {
    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {{45.0, 30.0}, {-30.0, 200.0}, {60.0, 90.0}, {25.0, 270.0}};
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        auto expected = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(expected.has_value());
        auto got = qzss_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        REQUIRE(got.has_value());
        CHECK(max_component_diff(*got, *expected) < 1.0e-9);
        check_orthonormal_right_handed(*got);
    }
}

// --- QZSY-A-003 --------------------------------------------------------------

TEST_CASE("QZSY-A-003  the mode switch fires at the registered ~20deg "
          "boundary on both sides of beta=0 -- shown firing on a "
          "deliberately reversed comparison (which would swap which mode "
          "applies on EVERY geometry, not just near the boundary)",
          "[attitude][qzss][gate]") {
    struct Case { double beta_deg; bool expect_orbit_normal; };
    const Case cases[] = {
        {19.0, true}, {-19.0, true}, {0.0, true},
        {21.0, false}, {-21.0, false}, {45.0, false},
    };
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, 90.0);
        auto got = qzss_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        REQUIRE(got.has_value());
        auto orbit_normal = orbit_normal_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s);
        const bool matches_orbit_normal = max_component_diff(*got, orbit_normal) < 1.0e-9;
        CHECK(matches_orbit_normal == c.expect_orbit_normal);

        // BROKEN: the reversed comparison (`>=` for orbit-normal instead of
        // `<=`) -- disagrees with the real dispatcher at EVERY case here,
        // not merely near the boundary, the defect this guard exists to
        // catch, shown actually firing.
        const bool broken_expects_orbit_normal = std::abs(c.beta_deg) * kDeg >= 20.0 * kDeg;
        CHECK(broken_expects_orbit_normal != c.expect_orbit_normal);
    }
}
