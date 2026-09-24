// ecom_tests.cpp — SPEC-ecom.md §8, ECOM-A-001 through ECOM-A-009.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/ecom/ecom.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>

using namespace odl;
using namespace odl::ecom;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kGpsRadiusM = 26561e3;
constexpr double kGpsSpeedM_s = 3874.0;

Vec3 normalized(const Vec3& v) {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

Vec3 random_unit(std::mt19937_64& rng) {
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    Vec3 v;
    do {
        v = Vec3{u(rng), u(rng), u(rng)};
    } while (v.norm() < 1.0e-6);
    return normalized(v);
}

/// Rodrigues' formula: a proper rotation by `theta` about `axis_hat`,
/// verified as a rotation (orthogonal, det +1) only by construction here --
/// ECOM-A-002 itself is what puts it to work.
Mat3 rotation_from_axis_angle(const Vec3& axis_hat, double theta) {
    Mat3 k;
    k.r[0][0] = 0.0;        k.r[0][1] = -axis_hat.z; k.r[0][2] =  axis_hat.y;
    k.r[1][0] =  axis_hat.z; k.r[1][1] = 0.0;        k.r[1][2] = -axis_hat.x;
    k.r[2][0] = -axis_hat.y; k.r[2][1] =  axis_hat.x; k.r[2][2] = 0.0;
    const Mat3 k2 = k.times(k);
    const double s = std::sin(theta), c = std::cos(theta);
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            out.r[i][j] = ((i == j) ? 1.0 : 0.0) + s * k.r[i][j] + (1.0 - c) * k2.r[i][j];
    return out;
}

/// Signed angle FROM `node_hat` TO `vec_hat`, going around `normal_hat` --
/// ECOM-A-005's own independent (r, v, sun) -> (u, u_s) construction, built
/// fresh here and never calling into this module's own Delta-u at all.
double angle_from_node(const Vec3& vec_hat, const Vec3& node_hat, const Vec3& normal_hat) {
    const double c = vec_hat.dot(node_hat);
    const double s = normal_hat.dot(node_hat.cross(vec_hat));
    return std::atan2(s, c);
}

struct Fixture {
    Vec3 r, v, sun;
};

/// A well-posed (r, v, sun) triple, rejection-sampled rather than
/// special-cased: r x v well away from the equatorial normal (so
/// ECOM-A-005's own ascending-node construction is well-posed), and the Sun
/// well away from both ECOM-F-001 (e_D || e_r) and ECOM-F-002 (s_hat ||
/// n_hat), so "well away" is a genuine margin rather than a coordinate
/// coincidence. `v` is a generic direction, not constrained perpendicular
/// to `r` -- ECOM-R-002 assumes nothing more than r x v != 0.
Fixture random_fixture(std::mt19937_64& rng) {
    for (;;) {
        const Vec3 r_hat = random_unit(rng);
        const Vec3 v_dir = random_unit(rng);
        const Vec3 n_raw = r_hat.cross(v_dir);
        if (n_raw.norm() < 0.3) continue;            // r, v too near-parallel
        const Vec3 n_hat = normalized(n_raw);
        if (std::abs(n_hat.z) > 0.94) continue;       // near-equatorial orbital plane
        const Vec3 sun_hat = random_unit(rng);
        if (std::abs(sun_hat.dot(r_hat)) > 0.98) continue;  // near ECOM-F-001
        if (std::abs(sun_hat.dot(n_hat)) > 0.94) continue;  // near ECOM-F-002
        return Fixture{kGpsRadiusM * r_hat, kGpsSpeedM_s * v_dir, sun_hat};
    }
}

}  // namespace

// --- ECOM-A-001 ------------------------------------------------------------

TEST_CASE("ECOM-A-001  the D/Y/B frame is orthonormal, right-handed, e_D points toward the "
          "Sun, and e_Y matches ARN15 Eq.1's own sign exactly, not just any perpendicular",
          "[ecom]") {
    std::mt19937_64 rng(20260924);
    const EcomOrder order{0, 0};
    const EcomCoefficients coeffs{};  // frame fields do not depend on coefficient values at all

    for (int trial = 0; trial < 200; ++trial) {
        const Fixture f = random_fixture(rng);
        auto result = ecom_acceleration(f.r, f.v, f.sun, order, coeffs);
        REQUIRE(result.has_value());

        const Vec3& e_D = result->e_D;
        const Vec3& e_Y = result->e_Y;
        const Vec3& e_B = result->e_B;

        CHECK_THAT(e_D.norm(), WithinAbs(1.0, 1.0e-13));
        CHECK_THAT(e_Y.norm(), WithinAbs(1.0, 1.0e-13));
        CHECK_THAT(e_B.norm(), WithinAbs(1.0, 1.0e-13));
        CHECK_THAT(e_D.dot(e_Y), WithinAbs(0.0, 1.0e-12));
        CHECK_THAT(e_Y.dot(e_B), WithinAbs(0.0, 1.0e-12));
        CHECK_THAT(e_D.dot(e_B), WithinAbs(0.0, 1.0e-12));
        const Vec3 cross_dy = e_D.cross(e_Y);
        CHECK_THAT(cross_dy.x, WithinAbs(e_B.x, 1.0e-12));
        CHECK_THAT(cross_dy.y, WithinAbs(e_B.y, 1.0e-12));
        CHECK_THAT(cross_dy.z, WithinAbs(e_B.z, 1.0e-12));

        // e_D points toward the Sun: dotted with the actual satellite->Sun
        // argument this call was given, positive.
        CHECK(e_D.dot(f.sun) > 0.0);

        // e_Y matches Eq.1's OWN sign, -(e_r x e_D) -- checked against the
        // specific signed formula, not merely "some perpendicular". Eq.1's
        // own formula is the NORMALIZED cross product (its own "/|...|"),
        // so this test normalizes too, or the comparison would be checking
        // magnitude (which varies with the r_hat/e_D angle) rather than
        // direction.
        const Vec3 r_hat = normalized(f.r);
        const Vec3 raw_e_Y = r_hat.cross(e_D);
        const Vec3 expected_e_Y = (-1.0 / raw_e_Y.norm()) * raw_e_Y;
        CHECK_THAT(e_Y.x, WithinAbs(expected_e_Y.x, 1.0e-12));
        CHECK_THAT(e_Y.y, WithinAbs(expected_e_Y.y, 1.0e-12));
        CHECK_THAT(e_Y.z, WithinAbs(expected_e_Y.z, 1.0e-12));
    }

    // Guard: the WRONG sign (+normalize(r_hat x e_D), missing Eq.1's own
    // minus) is substantially different from what the check above passed --
    // proving the SIGN specifically is discriminated (both candidates are
    // unit vectors here, so this isolates sign from normalization).
    const Fixture f = random_fixture(rng);
    auto result = ecom_acceleration(f.r, f.v, f.sun, order, coeffs);
    REQUIRE(result.has_value());
    const Vec3 r_hat = normalized(f.r);
    const Vec3 wrong_e_Y = normalized(r_hat.cross(result->e_D));
    const Vec3 d = result->e_Y - wrong_e_Y;
    CHECK(d.norm() > 1.0);
}

// --- ECOM-A-002 ------------------------------------------------------------

TEST_CASE("ECOM-A-002  Delta-u is independent of the coordinate system: rotating r, v and the "
          "Sun direction by a common arbitrary rotation leaves Delta-u unchanged",
          "[ecom]") {
    std::mt19937_64 rng(20260924 + 2);
    std::uniform_real_distribution<double> theta_dist(0.0, 2.0 * std::numbers::pi);
    const EcomOrder order{0, 0};
    const EcomCoefficients coeffs{};

    for (int trial = 0; trial < 300; ++trial) {
        const Fixture f = random_fixture(rng);
        auto base = ecom_acceleration(f.r, f.v, f.sun, order, coeffs);
        REQUIRE(base.has_value());

        const Mat3 rot = rotation_from_axis_angle(random_unit(rng), theta_dist(rng));
        auto rotated = ecom_acceleration(rot.apply(f.r), rot.apply(f.v), rot.apply(f.sun), order, coeffs);
        REQUIRE(rotated.has_value());

        CHECK_THAT(rotated->delta_u_rad, WithinAbs(base->delta_u_rad, 1.0e-12));
    }

    // Guard: rotating ONLY (r, v) and leaving the Sun direction fixed
    // breaks the relative geometry, and Delta-u changes substantially --
    // proving the invariance above is a property of a CONSISTENT rotation
    // applied to every input, not an accident of Delta-u ignoring its own
    // arguments.
    const Fixture g = random_fixture(rng);
    auto g_base = ecom_acceleration(g.r, g.v, g.sun, order, coeffs);
    REQUIRE(g_base.has_value());
    const Mat3 rot = rotation_from_axis_angle(random_unit(rng), 1.3);
    auto g_partial = ecom_acceleration(rot.apply(g.r), rot.apply(g.v), g.sun, order, coeffs);
    REQUIRE(g_partial.has_value());
    CHECK(std::abs(g_partial->delta_u_rad - g_base->delta_u_rad) > 1.0e-3);
}

// --- ECOM-A-003 / ECOM-A-004 -------------------------------------------------

TEST_CASE("ECOM-A-003  D(Delta_u + pi) = D(Delta_u) for every truncation order and every "
          "non-trivial coefficient set -- the even-harmonic-only structure of Eq.5",
          "[ecom]") {
    std::mt19937_64 rng(20260924 + 3);
    std::uniform_real_distribution<double> du_dist(-std::numbers::pi, std::numbers::pi);
    std::uniform_real_distribution<double> c_dist(-50.0, 50.0);

    for (int n_D : {0, 1, 2, 3}) {
        const EcomOrder order{n_D, 0};
        for (int trial = 0; trial < 50; ++trial) {
            EcomCoefficients c;
            c.D0 = c_dist(rng);
            for (int i = 0; i < n_D; ++i) {
                c.D_even_c.push_back(c_dist(rng));
                c.D_even_s.push_back(c_dist(rng));
            }
            const double du = du_dist(rng);
            CHECK_THAT(d_of(order, c, du + std::numbers::pi), WithinAbs(d_of(order, c, du), 1.0e-12));
        }
    }

    // Guard: the SAME non-trivial coefficients at a pi/2 offset instead of
    // pi do NOT generally agree -- proving the check is discriminating on
    // the OFFSET, not vacuously true for D constant or for any two points.
    const EcomOrder order{2, 0};
    EcomCoefficients c;
    c.D0 = 3.0;
    c.D_even_c = {7.0, -4.0};
    c.D_even_s = {2.0, 5.0};
    const double du = 0.4;
    CHECK(std::abs(d_of(order, c, du + std::numbers::pi / 2.0) - d_of(order, c, du)) > 1.0);
}

TEST_CASE("ECOM-A-004  B(Delta_u + pi) - B0 = -(B(Delta_u) - B0) for every truncation order "
          "and every non-trivial coefficient set -- the odd-harmonic-only structure of Eq.5",
          "[ecom]") {
    std::mt19937_64 rng(20260924 + 4);
    std::uniform_real_distribution<double> du_dist(-std::numbers::pi, std::numbers::pi);
    std::uniform_real_distribution<double> c_dist(-50.0, 50.0);

    for (int n_B : {0, 1, 2, 3}) {
        const EcomOrder order{0, n_B};
        for (int trial = 0; trial < 50; ++trial) {
            EcomCoefficients c;
            c.B0 = c_dist(rng);
            for (int i = 0; i < n_B; ++i) {
                c.B_odd_c.push_back(c_dist(rng));
                c.B_odd_s.push_back(c_dist(rng));
            }
            const double du = du_dist(rng);
            const double lhs = b_of(order, c, du + std::numbers::pi) - c.B0;
            const double rhs = -(b_of(order, c, du) - c.B0);
            CHECK_THAT(lhs, WithinAbs(rhs, 1.0e-12));
        }
    }

    // Guard: the SAME non-trivial coefficients at a pi/2 offset do NOT
    // generally satisfy the anti-symmetry.
    const EcomOrder order{0, 2};
    EcomCoefficients c;
    c.B0 = -1.5;
    c.B_odd_c = {6.0, 3.0};
    c.B_odd_s = {-2.0, 4.0};
    const double du = -0.9;
    const double lhs = b_of(order, c, du + std::numbers::pi / 2.0) - c.B0;
    const double rhs = -(b_of(order, c, du) - c.B0);
    CHECK(std::abs(lhs - rhs) > 1.0);
}

// --- ECOM-A-005 --------------------------------------------------------------

TEST_CASE("ECOM-A-005  the reduction: D4B1-family output at n_D=0, n_B=1 matches Eq.4 written "
          "in u, with Bc/Bs rotated from B1c/B1s by u_s -- both u and u_s computed fresh via "
          "an independently-built ascending-node construction, never through this module's own "
          "Delta-u",
          "[ecom]") {
    std::mt19937_64 rng(20260924 + 5);
    std::uniform_real_distribution<double> c_dist(-1.0, 1.0);  // O(1): matches the spec's own
                                                                 // "1e-9 rad-equivalent" tolerance
    const EcomOrder order{0, 1};

    for (int trial = 0; trial < 100; ++trial) {
        const Fixture f = random_fixture(rng);

        EcomCoefficients c;
        c.D0 = c_dist(rng);
        c.Y0 = c_dist(rng);
        c.B0 = c_dist(rng);
        c.B_odd_c = {c_dist(rng)};
        c.B_odd_s = {c_dist(rng)};

        auto result = ecom_acceleration(f.r, f.v, f.sun, order, c);
        REQUIRE(result.has_value());
        const double b_production = b_of(order, c, result->delta_u_rad);

        // Independent node-based construction of u and u_s -- not calling
        // into this module's own geometry at all.
        const Vec3 r_hat = normalized(f.r);
        const Vec3 n_hat = normalized(f.r.cross(f.v));
        const Vec3 node_hat = normalized(Vec3{0.0, 0.0, 1.0}.cross(n_hat));
        const double u = angle_from_node(r_hat, node_hat, n_hat);
        const Vec3 sun_hat = normalized(f.sun);
        const Vec3 s_proj = normalized(sun_hat - sun_hat.dot(n_hat) * n_hat);
        const double u_s = angle_from_node(s_proj, node_hat, n_hat);

        const double bc = c.B_odd_c[0] * std::cos(u_s) - c.B_odd_s[0] * std::sin(u_s);
        const double bs = c.B_odd_c[0] * std::sin(u_s) + c.B_odd_s[0] * std::cos(u_s);
        const double b_eq4 = c.B0 + bc * std::cos(u) + bs * std::sin(u);

        CHECK_THAT(b_eq4, WithinAbs(b_production, 1.0e-9));
    }

    // Guard: the WRONG rotation (sin/cos roles swapped) does NOT generally
    // reproduce the same value -- proving the specific formula is checked,
    // not just "some rotation by u_s".
    const Fixture f = random_fixture(rng);
    EcomCoefficients c;
    c.D0 = 0.2;
    c.Y0 = 0.1;
    c.B0 = 0.4;
    c.B_odd_c = {0.9};
    c.B_odd_s = {-0.6};
    auto result = ecom_acceleration(f.r, f.v, f.sun, order, c);
    REQUIRE(result.has_value());
    const double b_production = b_of(order, c, result->delta_u_rad);

    const Vec3 r_hat = normalized(f.r);
    const Vec3 n_hat = normalized(f.r.cross(f.v));
    const Vec3 node_hat = normalized(Vec3{0.0, 0.0, 1.0}.cross(n_hat));
    const double u = angle_from_node(r_hat, node_hat, n_hat);
    const Vec3 sun_hat = normalized(f.sun);
    const Vec3 s_proj = normalized(sun_hat - sun_hat.dot(n_hat) * n_hat);
    const double u_s = angle_from_node(s_proj, node_hat, n_hat);

    const double bc_wrong = c.B_odd_c[0] * std::sin(u_s) - c.B_odd_s[0] * std::cos(u_s);
    const double bs_wrong = c.B_odd_c[0] * std::cos(u_s) + c.B_odd_s[0] * std::sin(u_s);
    const double b_wrong = c.B0 + bc_wrong * std::cos(u) + bs_wrong * std::sin(u);
    CHECK(std::abs(b_wrong - b_production) > 1.0e-3);
}

// --- ECOM-A-006 --------------------------------------------------------------

TEST_CASE("ECOM-A-006  every coefficient's own analytic sensitivity column matches a central "
          "finite difference of the full acceleration -- exact, since the acceleration is "
          "linear in every coefficient",
          "[ecom]") {
    std::mt19937_64 rng(20260924 + 6);
    std::uniform_real_distribution<double> c_dist(-3.0, 3.0);
    constexpr double h = 1.0e-4;

    for (int n_D : {0, 1, 2}) {
        for (int n_B : {0, 1, 2}) {
            const EcomOrder order{n_D, n_B};
            for (int trial = 0; trial < 20; ++trial) {
                const Fixture f = random_fixture(rng);
                EcomCoefficients c;
                c.D0 = c_dist(rng);
                for (int i = 0; i < n_D; ++i) {
                    c.D_even_c.push_back(c_dist(rng));
                    c.D_even_s.push_back(c_dist(rng));
                }
                c.Y0 = c_dist(rng);
                c.B0 = c_dist(rng);
                for (int i = 0; i < n_B; ++i) {
                    c.B_odd_c.push_back(c_dist(rng));
                    c.B_odd_s.push_back(c_dist(rng));
                }

                auto centre = ecom_acceleration(f.r, f.v, f.sun, order, c);
                REQUIRE(centre.has_value());

                auto check_column = [&](double& slot, const Vec3& analytic) {
                    const double orig = slot;
                    slot = orig + h;
                    auto plus = ecom_acceleration(f.r, f.v, f.sun, order, c);
                    slot = orig - h;
                    auto minus = ecom_acceleration(f.r, f.v, f.sun, order, c);
                    slot = orig;
                    REQUIRE(plus.has_value());
                    REQUIRE(minus.has_value());
                    const Vec3 numeric =
                        (1.0 / (2.0 * h)) * (plus->acceleration_m_s2 - minus->acceleration_m_s2);
                    CHECK_THAT(numeric.x, WithinAbs(analytic.x, 1.0e-8));
                    CHECK_THAT(numeric.y, WithinAbs(analytic.y, 1.0e-8));
                    CHECK_THAT(numeric.z, WithinAbs(analytic.z, 1.0e-8));
                };

                check_column(c.D0, centre->d_coefficients.D0);
                for (int i = 0; i < n_D; ++i) {
                    const auto idx = static_cast<std::size_t>(i);
                    check_column(c.D_even_c[idx], centre->d_coefficients.D_even_c[idx]);
                    check_column(c.D_even_s[idx], centre->d_coefficients.D_even_s[idx]);
                }
                check_column(c.Y0, centre->d_coefficients.Y0);
                check_column(c.B0, centre->d_coefficients.B0);
                for (int i = 0; i < n_B; ++i) {
                    const auto idx = static_cast<std::size_t>(i);
                    check_column(c.B_odd_c[idx], centre->d_coefficients.B_odd_c[idx]);
                    check_column(c.B_odd_s[idx], centre->d_coefficients.B_odd_s[idx]);
                }
            }
        }
    }

    // Guard: D0's own column (e_D) and Y0's own column (e_Y) are, except by
    // coincidence, different directions -- proving this check discriminates
    // between columns rather than accepting any of them interchangeably.
    const Fixture f = random_fixture(rng);
    const EcomOrder order{0, 0};
    EcomCoefficients c;
    c.D0 = 1.0;
    c.Y0 = 1.0;
    c.B0 = 1.0;
    auto centre = ecom_acceleration(f.r, f.v, f.sun, order, c);
    REQUIRE(centre.has_value());
    const Vec3 d = centre->d_coefficients.D0 - centre->d_coefficients.Y0;
    CHECK(d.norm() > 0.5);
}

// --- ECOM-A-007 --------------------------------------------------------------

TEST_CASE("ECOM-A-007  the analytic velocity Jacobian matches a central finite difference of "
          "the acceleration with respect to v",
          "[ecom]") {
    std::mt19937_64 rng(20260924 + 7);
    std::uniform_real_distribution<double> c_dist(-3.0, 3.0);
    constexpr double h = 1.0e-3;  // m/s, tiny next to ~3900 m/s orbital speeds

    for (int n_D : {0, 1, 2}) {
        for (int n_B : {0, 1, 2}) {
            const EcomOrder order{n_D, n_B};
            for (int trial = 0; trial < 15; ++trial) {
                const Fixture f = random_fixture(rng);
                EcomCoefficients c;
                c.D0 = c_dist(rng);
                for (int i = 0; i < n_D; ++i) {
                    c.D_even_c.push_back(c_dist(rng));
                    c.D_even_s.push_back(c_dist(rng));
                }
                c.Y0 = c_dist(rng);
                c.B0 = c_dist(rng);
                for (int i = 0; i < n_B; ++i) {
                    c.B_odd_c.push_back(c_dist(rng));
                    c.B_odd_s.push_back(c_dist(rng));
                }

                auto centre = ecom_acceleration(f.r, f.v, f.sun, order, c);
                REQUIRE(centre.has_value());

                for (std::size_t axis = 0; axis < 3; ++axis) {
                    Vec3 bump{};
                    if (axis == 0) bump.x = h;
                    else if (axis == 1) bump.y = h;
                    else bump.z = h;
                    auto plus = ecom_acceleration(f.r, f.v + bump, f.sun, order, c);
                    auto minus = ecom_acceleration(f.r, f.v - bump, f.sun, order, c);
                    REQUIRE(plus.has_value());
                    REQUIRE(minus.has_value());
                    const Vec3 numeric =
                        (1.0 / (2.0 * h)) * (plus->acceleration_m_s2 - minus->acceleration_m_s2);
                    const Vec3 analytic{centre->d_velocity.r[0][axis], centre->d_velocity.r[1][axis],
                                         centre->d_velocity.r[2][axis]};
                    CHECK_THAT(numeric.x, WithinAbs(analytic.x, 5.0e-6));
                    CHECK_THAT(numeric.y, WithinAbs(analytic.y, 5.0e-6));
                    CHECK_THAT(numeric.z, WithinAbs(analytic.z, 5.0e-6));
                }
            }
        }
    }

    // Guard: twice the analytic value does NOT match the finite difference
    // -- proving this check is sensitive to the actual VALUE, not merely to
    // a Jacobian of the right shape being present.
    const Fixture f = random_fixture(rng);
    const EcomOrder order{1, 1};
    EcomCoefficients c;
    c.D0 = 1.0;
    c.D_even_c = {0.6};
    c.D_even_s = {-0.4};
    c.Y0 = 0.3;
    c.B0 = 0.5;
    c.B_odd_c = {0.8};
    c.B_odd_s = {-0.2};
    auto centre = ecom_acceleration(f.r, f.v, f.sun, order, c);
    REQUIRE(centre.has_value());

    const Vec3 bump{h, 0.0, 0.0};
    auto plus = ecom_acceleration(f.r, f.v + bump, f.sun, order, c);
    auto minus = ecom_acceleration(f.r, f.v - bump, f.sun, order, c);
    REQUIRE(plus.has_value());
    REQUIRE(minus.has_value());
    const Vec3 numeric = (1.0 / (2.0 * h)) * (plus->acceleration_m_s2 - minus->acceleration_m_s2);
    const Vec3 analytic{centre->d_velocity.r[0][0], centre->d_velocity.r[1][0], centre->d_velocity.r[2][0]};
    const Vec3 wrong = 2.0 * analytic;
    const Vec3 diff = numeric - wrong;
    CHECK((std::abs(diff.x) + std::abs(diff.y) + std::abs(diff.z)) > 1.0e-6);
}

// --- ECOM-A-008 --------------------------------------------------------------

TEST_CASE("ECOM-A-008  ECOM-F-001 and ECOM-F-002 fire exactly at their own stated "
          "degeneracies, and not just near them",
          "[ecom]") {
    const EcomOrder order{0, 0};
    const EcomCoefficients c{};

    // ECOM-F-001: the spacecraft exactly on the Earth-Sun line, e_D || e_r.
    {
        const Vec3 r{kGpsRadiusM, 0.0, 0.0};
        const Vec3 v{0.0, kGpsSpeedM_s, 500.0};
        const Vec3 sun_on_line{1.0, 0.0, 0.0};
        auto result = ecom_acceleration(r, v, sun_on_line, order, c);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().id == "ECOM-F-001");
    }
    // Guard: a Sun direction merely near the line, not on it, does not fire
    // -- the boundary is a real tolerance, not an over-broad net.
    {
        const Vec3 r{kGpsRadiusM, 0.0, 0.0};
        const Vec3 v{0.0, kGpsSpeedM_s, 500.0};
        const Vec3 sun_near_line = normalized(Vec3{1.0, 0.05, 0.0});
        auto result = ecom_acceleration(r, v, sun_near_line, order, c);
        REQUIRE(result.has_value());
    }

    // ECOM-F-002: the Sun exactly on the orbit normal, beta = +-90 deg.
    {
        const Vec3 r{kGpsRadiusM, 0.0, 0.0};
        const Vec3 v{0.0, kGpsSpeedM_s, 0.0};  // n_hat = r x v, exactly +Z
        const Vec3 sun_on_normal{0.0, 0.0, 1.0};
        auto result = ecom_acceleration(r, v, sun_on_normal, order, c);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().id == "ECOM-F-002");
    }
    // Guard: a Sun direction merely near the normal does not fire.
    {
        const Vec3 r{kGpsRadiusM, 0.0, 0.0};
        const Vec3 v{0.0, kGpsSpeedM_s, 0.0};
        const Vec3 sun_near_normal = normalized(Vec3{0.05, 0.0, 1.0});
        auto result = ecom_acceleration(r, v, sun_near_normal, order, c);
        REQUIRE(result.has_value());
    }
}

// --- ECOM-A-009 --------------------------------------------------------------

TEST_CASE("ECOM-A-009  d4b1_order() names CODE's own operational configuration, n_D=2, n_B=1",
          "[ecom]") {
    const EcomOrder order = d4b1_order();
    CHECK(order.n_D == 2);
    CHECK(order.n_B == 1);

    // Guard: a DIFFERENT order (D2B0, say) does not satisfy this check --
    // proving it discriminates the specific configuration, not merely "some
    // order was returned".
    const EcomOrder wrong{2, 0};
    CHECK_FALSE((wrong.n_D == order.n_D && wrong.n_B == order.n_B));
}
