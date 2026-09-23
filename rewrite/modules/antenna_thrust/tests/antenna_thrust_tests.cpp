// antenna_thrust_tests.cpp — SPEC-thrust-yaw §4.5, §8.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/antenna_thrust/antenna_thrust.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using namespace odl;
// NOT `using namespace odl::antenna_thrust;` -- that namespace's own name
// collides with the free function `antenna_thrust::antenna_thrust`, and
// opening both it and `odl` (already needed for Vec3/Mat3/Result) makes the
// bare call `antenna_thrust(...)` genuinely ambiguous between the namespace
// and the function (found by trying it, not anticipated). Calls below are
// qualified `antenna_thrust::antenna_thrust(...)` instead -- `antenna_thrust`
// alone already resolves to the namespace via `using namespace odl;`.
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

Vec3 normalized(const Vec3& v) {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

}  // namespace

TEST_CASE("TYAW-A-007  TYAW-F-002 fires on a negative and a NaN p_watts, and does not fire "
          "on the adjacent valid value (zero included)",
          "[antenna_thrust][gate]") {
    const Vec3 z_body = normalized(Vec3{0.3, -0.7, 0.6});

    auto negative = antenna_thrust::antenna_thrust(-1.0, z_body);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().id == "TYAW-F-002");

    auto nan_case = antenna_thrust::antenna_thrust(std::numeric_limits<double>::quiet_NaN(), z_body);
    REQUIRE_FALSE(nan_case.has_value());
    CHECK(nan_case.error().id == "TYAW-F-002");

    auto infinite_case =
        antenna_thrust::antenna_thrust(std::numeric_limits<double>::infinity(), z_body);
    REQUIRE_FALSE(infinite_case.has_value());
    CHECK(infinite_case.error().id == "TYAW-F-002");

    // The adjacent valid value, zero included: no refusal, and the returned
    // force is exactly zero at p_watts = 0.
    auto zero_case = antenna_thrust::antenna_thrust(0.0, z_body);
    REQUIRE(zero_case.has_value());
    CHECK_THAT(zero_case->norm(), WithinAbs(0.0, 1.0e-300));

    auto small_positive = antenna_thrust::antenna_thrust(1.0e-6, z_body);
    REQUIRE(small_positive.has_value());
}

TEST_CASE("TYAW-A-008  antenna_thrust returns exactly P/c along -z_body, at a stated P, "
          "for a non-axis-aligned boresight",
          "[antenna_thrust][gate]") {
    constexpr double kC = 299792458.0;
    const Vec3 z_body = normalized(Vec3{0.2, 0.6, -0.776});
    const double p_watts = 240.0;  // SVN63/G01, IGSMETA (SPEC-thrust-yaw §2), a plausibility anchor

    auto force = antenna_thrust::antenna_thrust(p_watts, z_body);
    REQUIRE(force.has_value());

    const double expected_mag = p_watts / kC;
    CHECK_THAT(force->norm(), WithinRel(expected_mag, 1.0e-12));

    const Vec3 expected_direction = -1.0 * z_body;
    const Vec3 got_direction = normalized(*force);
    CHECK_THAT(got_direction.x, WithinAbs(expected_direction.x, 1.0e-12));
    CHECK_THAT(got_direction.y, WithinAbs(expected_direction.y, 1.0e-12));
    CHECK_THAT(got_direction.z, WithinAbs(expected_direction.z, 1.0e-12));

    // Exact component match against the closed form, not just magnitude and
    // direction separately.
    const Vec3 expected = (-p_watts / kC) * z_body;
    CHECK_THAT(force->x, WithinAbs(expected.x, 1.0e-20));
    CHECK_THAT(force->y, WithinAbs(expected.y, 1.0e-20));
    CHECK_THAT(force->z, WithinAbs(expected.z, 1.0e-20));
}

TEST_CASE("TYAW-A-010  TYAW-P-5's own (1 + cos alpha)/2 average and the two stated "
          "overstatement figures, computed in code",
          "[antenna_thrust][gate]") {
    struct Case { double alpha_deg, expected_avg, expected_overstatement_pct; };
    const Case cases[] = {
        {13.9, 0.98536, 1.46},
        {20.0, 0.96985, 3.02},
    };
    for (const auto& c : cases) {
        INFO("alpha = " << c.alpha_deg << " deg");
        const double alpha_rad = c.alpha_deg * std::numbers::pi / 180.0;
        const double avg_cos_theta = (1.0 + std::cos(alpha_rad)) / 2.0;
        const double overstatement_pct = (1.0 - avg_cos_theta) * 100.0;
        CHECK_THAT(avg_cos_theta, WithinAbs(c.expected_avg, 5.0e-5));
        CHECK_THAT(overstatement_pct, WithinAbs(c.expected_overstatement_pct, 5.0e-3));
    }
}

TEST_CASE("TYAW-A-011  TYAW-R-006's own position-only claim: d(a)/d(v) declared exactly "
          "zero, and the analytic d(a)/d(r) matches an independent central finite "
          "difference of the whole plugin call",
          "[antenna_thrust][gate]") {
    antenna_thrust::AntennaThrust force(240.0, 1630.0);  // SVN63/G01-scale power, GPS IIF-scale mass

    CHECK(force.id().name == "antenna_thrust");
    CHECK(force.consumes().empty());

    // Unused by this force (TYAW-R-006) -- any valid epoch will do, no
    // LeapTable needed since GPS time (unlike UTC) has no leap-second
    // ambiguity to resolve.
    auto when_result = odl::time::Epoch::from_gps_week(2246, 0.0);
    REQUIRE(when_result.has_value());
    const auto when = *when_result;
    // A generic, non-axis-aligned GPS-altitude position: no single component
    // dominates, so no da_dr component is trivially near zero.
    const Vec3 r0{26561e3 * 0.6, 26561e3 * 0.3, 26561e3 * std::sqrt(1.0 - 0.36 - 0.09)};
    const frames::Position<frames::Frame::GCRS> r{r0};
    const Vec3 v{0.0, 3874.0, 0.0};  // unused by this force -- exercised anyway (TYAW-R-006)

    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);
    auto result = force.accel(when, r, v, params, reg);
    REQUIRE(result.has_value());

    const double a_norm = result->acceleration.norm_m_per_s2();
    INFO("|a| = " << a_norm << " m/s^2");
    CHECK(a_norm > 0.0);
    CHECK(a_norm < 1.0e-8);  // plausibility; TYAW-P-4's own order is 4.91e-10

    // d(a)/d(v): declared ABSENT (not a small nonzero bound), with the
    // neglected magnitude exactly 0 -- TYAW-R-006's own claim made concrete:
    // z_body does not depend on velocity at all, for any provider this tree
    // has, so there is no term to bound, however small.
    CHECK_FALSE(result->d_state.d_velocity().has_value());
    CHECK(result->d_state.neglected_velocity_bound_per_s() == 0.0);

    // d(a)/d(r): the analytic closed form against an INDEPENDENT central
    // finite difference of the whole plugin call (`PHPR-A-006`'s own
    // precedent), not a finite difference checked against another finite
    // difference.
    constexpr double kStepM = 100.0;
    const Vec3 axis_unit[3] = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    Mat3 fd_da_dr{};
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 dr = kStepM * axis_unit[static_cast<std::size_t>(axis)];
        auto plus = force.accel(when, frames::Position<frames::Frame::GCRS>{r0 + dr}, v, params, reg);
        auto minus = force.accel(when, frames::Position<frames::Frame::GCRS>{r0 - dr}, v, params, reg);
        REQUIRE(plus.has_value());
        REQUIRE(minus.has_value());
        const Vec3 dcol = (1.0 / (2.0 * kStepM)) *
            (plus->acceleration.metres_per_second_squared() -
             minus->acceleration.metres_per_second_squared());
        fd_da_dr.r[0][static_cast<std::size_t>(axis)] = dcol.x;
        fd_da_dr.r[1][static_cast<std::size_t>(axis)] = dcol.y;
        fd_da_dr.r[2][static_cast<std::size_t>(axis)] = dcol.z;
    }

    const Mat3& analytic = result->d_state.d_position();
    const double scale = 240.0 / (299792458.0 * 1630.0 * r0.norm());  // da_dr's own natural size
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) {
            INFO("i=" << i << " j=" << j << " analytic=" << analytic.r[i][j]
                       << " fd=" << fd_da_dr.r[i][j]);
            CHECK_THAT(analytic.r[i][j], WithinAbs(fd_da_dr.r[i][j], 1.0e-6 * scale));
        }
}
