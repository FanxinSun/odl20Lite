// attitude_tests.cpp — SPEC-photon-pressure §4.2, §8 (PHPR-A-003).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/attitude/attitude.hpp>

#include <cmath>
#include <numbers>

using namespace odl;
using namespace odl::attitude;
using Catch::Matchers::WithinAbs;

namespace {

Vec3 normalized(const Vec3& v) {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

/// Checked at every stated case, not asserted once: the three axes are
/// mutually orthogonal, each unit length, and right-handed (x cross y = z) --
/// a law that silently returned a left-handed or non-orthogonal frame would
/// still "point roughly the right way" and pass a looser check.
void check_orthonormal_right_handed(const Mat3& m) {
    const Vec3 x{m.r[0][0], m.r[0][1], m.r[0][2]};
    const Vec3 y{m.r[1][0], m.r[1][1], m.r[1][2]};
    const Vec3 z{m.r[2][0], m.r[2][1], m.r[2][2]};
    CHECK_THAT(x.norm(), WithinAbs(1.0, 1.0e-12));
    CHECK_THAT(y.norm(), WithinAbs(1.0, 1.0e-12));
    CHECK_THAT(z.norm(), WithinAbs(1.0, 1.0e-12));
    CHECK_THAT(x.dot(y), WithinAbs(0.0, 1.0e-12));
    CHECK_THAT(y.dot(z), WithinAbs(0.0, 1.0e-12));
    CHECK_THAT(x.dot(z), WithinAbs(0.0, 1.0e-12));
    const Vec3 x_cross_y = x.cross(y);
    CHECK_THAT(x_cross_y.x, WithinAbs(z.x, 1.0e-12));
    CHECK_THAT(x_cross_y.y, WithinAbs(z.y, 1.0e-12));
    CHECK_THAT(x_cross_y.z, WithinAbs(z.z, 1.0e-12));
}

}  // namespace

TEST_CASE("PHPR-A-003  nominal yaw-steering reproduces the standard law at a stated "
          "equatorial, polar and eclipse-season geometry, orthonormal and right-handed",
          "[attitude][gate]") {
    struct Case { const char* label; Vec3 r_gcrs_m; Vec3 sun_gcrs; };
    const Case cases[] = {
        // Equatorial: satellite on the GCRS x-axis at GNSS-ish altitude, Sun
        // well off the orbital plane's own local vertical.
        {"equatorial", Vec3{2.656e7, 0.0, 0.0}, normalized(Vec3{0.3, 0.9, 0.2})},
        // Polar: satellite near the GCRS z-axis (a polar-orbit-like position).
        {"polar", Vec3{1.0e6, 2.0e5, 2.656e7}, normalized(Vec3{1.0, 0.2, 0.05})},
        // Eclipse-season-like: Sun nearly in the satellite's own orbital
        // plane (here, nearly perpendicular to r, the geometry that produces
        // eclipses for an equatorial orbit) but not exactly on the nadir axis.
        {"eclipse-season-like", Vec3{2.656e7, 0.0, 0.0}, normalized(Vec3{0.01, 1.0, 0.0})},
    };

    for (const auto& c : cases) {
        INFO(c.label);
        auto m = nominal_yaw_steering(c.r_gcrs_m, c.sun_gcrs);
        REQUIRE(m.has_value());
        check_orthonormal_right_handed(*m);

        const Vec3 r_hat = normalized(c.r_gcrs_m);
        const Vec3 z_body{m->r[2][0], m->r[2][1], m->r[2][2]};
        const Vec3 y_body{m->r[1][0], m->r[1][1], m->r[1][2]};
        // z_body = -r_hat exactly (RS09 §3.2.1's own nadir convention).
        CHECK_THAT(z_body.x, WithinAbs(-r_hat.x, 1.0e-12));
        CHECK_THAT(z_body.y, WithinAbs(-r_hat.y, 1.0e-12));
        CHECK_THAT(z_body.z, WithinAbs(-r_hat.z, 1.0e-12));
        // y_body (the panel rotation axis) is perpendicular to the Sun by
        // construction -- the property that makes it "the Sun-tracking
        // panels' own rotation axis" rather than an arbitrary third axis.
        CHECK_THAT(y_body.dot(c.sun_gcrs), WithinAbs(0.0, 1.0e-10));
    }
}

TEST_CASE("PHPR-A-003  nominal yaw-steering refuses at the Sun-on-nadir singularity, "
          "both ways, and does not over-refuse a case that only approaches it",
          "[attitude][gate]") {
    const Vec3 r_gcrs_m{2.656e7, 0.0, 0.0};
    const Vec3 nadir_direction = normalized(Vec3{-1.0, 0.0, 0.0});   // = -r_hat = z_body

    // Sun exactly ON the nadir axis: the panel rotation axis z_body x s_hat
    // is exactly zero, undefined, not merely small.
    auto refused = nominal_yaw_steering(r_gcrs_m, nadir_direction);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().id == "ATTD-F-001");

    // Sun exactly on the ANTI-nadir (zenith) axis: z_body x s_hat is also
    // exactly zero here -- the same singularity from the other side.
    auto refused_anti = nominal_yaw_steering(r_gcrs_m, Vec3{1.0, 0.0, 0.0});
    REQUIRE_FALSE(refused_anti.has_value());
    CHECK(refused_anti.error().id == "ATTD-F-001");

    // Adjacent, not degenerate: a Sun direction a full degree away from the
    // nadir axis is nowhere near this law's own tight tolerance (~2e-4
    // arcsec) and must succeed -- proven both ways, not only that the
    // refusal fires (plan §4 rule 5).
    const double one_degree = 1.0 * std::numbers::pi / 180.0;
    const Vec3 near_nadir_not_on_it =
        normalized(Vec3{-std::cos(one_degree), std::sin(one_degree), 0.0});
    auto succeeded = nominal_yaw_steering(r_gcrs_m, near_nadir_not_on_it);
    REQUIRE(succeeded.has_value());
    check_orthonormal_right_handed(*succeeded);
}
