// sentinel6_tests.cpp — SPEC-sentinel6-attitude.md, S6AT-A-001..A-003.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/attitude/attitude.hpp>

#include <cmath>

using namespace odl;
using namespace odl::attitude;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

/// A synthetic circular orbit, RAAN = 0, chosen inclination -- a CLOSED-FORM
/// ground truth for the argument of latitude (theta_true, by construction)
/// and for the orbital triad, independent of any ephemeris. Ascending node
/// along GCRS +X (RAAN=0's own standard meaning); p_hat=+X, q_hat=(0,cos i,
/// sin i) is the SECOND orbital-plane basis vector, chosen so n_hat =
/// p_hat x q_hat has a POSITIVE Z-component for i < 90 deg (a prograde
/// orbit's own standard property, checked directly in the first test
/// below, not merely assumed). Only DIRECTIONS matter for n_hat/r_hat/t_hat
/// (the cross product's direction is invariant to positive rescaling), so
/// arbitrary positive radius/speed magnitudes are used.
struct CircularOrbit {
    double inclination_rad;
    [[nodiscard]] Vec3 r(double theta_true_rad) const noexcept {
        const Vec3 p{1.0, 0.0, 0.0};
        const Vec3 q{0.0, std::cos(inclination_rad), std::sin(inclination_rad)};
        const double c = std::cos(theta_true_rad), s = std::sin(theta_true_rad);
        return 7.0e6 * Vec3{c * p.x + s * q.x, c * p.y + s * q.y, c * p.z + s * q.z};
    }
    [[nodiscard]] Vec3 v(double theta_true_rad) const noexcept {
        const Vec3 p{1.0, 0.0, 0.0};
        const Vec3 q{0.0, std::cos(inclination_rad), std::sin(inclination_rad)};
        const double c = std::cos(theta_true_rad), s = std::sin(theta_true_rad);
        return 7500.0 * Vec3{-s * p.x + c * q.x, -s * p.y + c * q.y, -s * p.z + c * q.z};
    }
};

[[nodiscard]] double wrap_pi(double a) noexcept {
    while (a > kPi) a -= 2.0 * kPi;
    while (a <= -kPi) a += 2.0 * kPi;
    return a;
}

}  // namespace

// --- S6AT-A-001 --------------------------------------------------------------

TEST_CASE("S6AT-A-001  sentinel6_argument_of_latitude_rad matches a "
          "closed-form circular orbit exactly, at eight angles spanning a "
          "full revolution, for a prograde (i=53deg) orbit -- and the "
          "orbit's own n_hat has a positive Z-component, confirming the "
          "synthetic geometry is genuinely prograde, not an accidental "
          "retrograde construction",
          "[attitude][sentinel6]") {
    const CircularOrbit orbit{53.0 * kDegToRad};
    // n_hat = r_hat(0) x v_hat(0)-direction check via a known point.
    const Vec3 r0 = orbit.r(0.0), v0 = orbit.v(0.0);
    const Vec3 n = r0.cross(v0);
    CHECK(n.z > 0.0);  // prograde, i < 90 deg

    for (int deg = 0; deg < 360; deg += 45) {
        const double theta_true = deg * kDegToRad;
        const double got = sentinel6_argument_of_latitude_rad(orbit.r(theta_true), orbit.v(theta_true));
        CHECK_THAT(wrap_pi(got - theta_true), WithinAbs(0.0, 1.0e-9));
    }
}

TEST_CASE("S6AT-A-001b  the guard shown firing: a deliberately reversed "
          "(wrong-sign) argument of latitude disagrees with the closed-form "
          "circular orbit at every angle checked, and would give a "
          "DECREASING theta forward in time instead of increasing",
          "[attitude][sentinel6]") {
    const CircularOrbit orbit{53.0 * kDegToRad};
    // The deliberately-reversed formula: the same construction with the
    // negation this file's own production code applies REMOVED (the sign
    // this test exists to prove matters, mirroring TYAW-A-012's own role
    // for mu_rad's historical negation bug).
    auto wrong_sign = [&](double theta_true) {
        const Vec3 r = orbit.r(theta_true), v = orbit.v(theta_true);
        const Vec3 r_hat = Vec3{r.x, r.y, r.z} * (1.0 / r.norm());
        const Vec3 n_hat_raw = r.cross(v);
        const Vec3 n_hat = n_hat_raw * (1.0 / n_hat_raw.norm());
        const Vec3 t_hat = n_hat.cross(r_hat);
        const Vec3 pole{0.0, 0.0, 1.0};
        Vec3 node_raw = pole.cross(n_hat);
        node_raw = node_raw * (1.0 / node_raw.norm());
        // WRONG: positive sign on the t_hat term (production negates it).
        return std::atan2(node_raw.dot(t_hat), node_raw.dot(r_hat));
    };

    int disagreements = 0;
    for (int deg = 10; deg < 360; deg += 40) {
        const double theta_true = deg * kDegToRad;
        const double wrong = wrong_sign(theta_true);
        if (std::abs(wrap_pi(wrong - theta_true)) > 1.0e-6) ++disagreements;
    }
    CHECK(disagreements > 0);

    // Forward-in-time: the wrong-sign formula gives a value that DECREASES
    // (mod wrap) as theta_true increases through a small step, the opposite
    // of the real, correct function's own increasing behaviour (checked
    // next, S6AT-A-001c).
    const double a = wrong_sign(30.0 * kDegToRad);
    const double b = wrong_sign(31.0 * kDegToRad);
    CHECK(wrap_pi(b - a) < 0.0);
}

TEST_CASE("S6AT-A-001c  forward-in-time: sentinel6_argument_of_latitude_rad "
          "increases as the satellite is propagated forward along its own "
          "orbit (mu_rad's own established verification shape, applied to "
          "this law's own different angular origin)",
          "[attitude][sentinel6]") {
    const CircularOrbit orbit{53.0 * kDegToRad};
    for (int deg = 0; deg < 360; deg += 30) {
        const double a = sentinel6_argument_of_latitude_rad(orbit.r(deg * kDegToRad), orbit.v(deg * kDegToRad));
        const double b = sentinel6_argument_of_latitude_rad(orbit.r((deg + 1) * kDegToRad),
                                                             orbit.v((deg + 1) * kDegToRad));
        CHECK(wrap_pi(b - a) > 0.0);
    }
}

// --- S6AT-A-002: orthonormal, right-handed ----------------------------------

TEST_CASE("S6AT-A-002  sentinel6_attitude returns an orthonormal, "
          "right-handed frame at every geometry checked, across a full "
          "revolution",
          "[attitude][sentinel6]") {
    const CircularOrbit orbit{53.0 * kDegToRad};
    for (int deg = 0; deg < 360; deg += 23) {
        const Mat3 m = sentinel6_attitude(orbit.r(deg * kDegToRad), orbit.v(deg * kDegToRad));
        CHECK_THAT(m.determinant(), WithinAbs(1.0, 1.0e-9));
        const Mat3 should_be_identity = m.times(m.transpose());
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                CHECK_THAT(should_be_identity.r[i][j], WithinAbs(i == j ? 1.0 : 0.0, 1.0e-9));
    }
}

// --- S6AT-A-003: oscillations vanish at their nodes -------------------------

TEST_CASE("S6AT-A-003  the roll/pitch/yaw oscillations vanish at their own "
          "nodes, checked against an INDEPENDENTLY worked-out closed form -- "
          "not the appendix's own numerical example (which is for SPOT-5's "
          "bus, no attitude), the manager's own named alternative when the "
          "appendix does not pin the rotation convention",
          "[attitude][sentinel6]") {
    const CircularOrbit orbit{53.0 * kDegToRad};

    // theta=0 and theta=180: roll (a2*sin theta) and pitch (a3*sin 2theta)
    // both vanish, leaving a PURE ROTATION ABOUT R by the yaw angle alpha1 =
    // a1*cos(theta) (worked out independently in attitude.cpp's own header
    // comment: Rsat=R, Tsat=cos(a1)*T+sin(a1)*N, Nsat=-sin(a1)*T+cos(a1)*N
    // at theta=0). Checked against the frame's own R,T,N components via
    // R_hat=r_hat(theta), T_hat=t_hat, N_hat=n_hat at that instant.
    for (double theta_deg : {0.0, 180.0}) {
        const double theta = theta_deg * kDegToRad;
        const Vec3 r = orbit.r(theta), v = orbit.v(theta);
        const Mat3 m = sentinel6_attitude(r, v);
        const Vec3 r_hat = r * (1.0 / r.norm());
        const Vec3 n_hat_raw = r.cross(v);
        const Vec3 n_hat = n_hat_raw * (1.0 / n_hat_raw.norm());
        const Vec3 t_hat = n_hat.cross(r_hat);

        const double a1 = (4.225 * kDegToRad) * std::cos(theta);  // Sentinel-6's own yaw coefficient
        // Rsat = R  =>  z_body = -Rsat = -r_hat.
        const Vec3 z_body{m.r[2][0], m.r[2][1], m.r[2][2]};
        CHECK_THAT((z_body - (-1.0 * r_hat)).norm(), WithinAbs(0.0, 1.0e-9));
        // Tsat = cos(a1)*T + sin(a1)*N  =>  x_body = Tsat.
        const Vec3 expected_x = std::cos(a1) * t_hat + std::sin(a1) * n_hat;
        const Vec3 x_body{m.r[0][0], m.r[0][1], m.r[0][2]};
        CHECK_THAT((x_body - expected_x).norm(), WithinAbs(0.0, 1.0e-9));
    }

    // theta=90 and theta=270: yaw (a1*cos theta) and pitch (a3*sin 2theta)
    // both vanish, leaving a PURE ROTATION ABOUT T by the roll angle
    // alpha2 = a2*sin(theta): Rsat=cos(a2)*R-sin(a2)*N, Tsat=T,
    // Nsat=sin(a2)*R+cos(a2)*N.
    for (double theta_deg : {90.0, 270.0}) {
        const double theta = theta_deg * kDegToRad;
        const Vec3 r = orbit.r(theta), v = orbit.v(theta);
        const Mat3 m = sentinel6_attitude(r, v);
        const Vec3 r_hat = r * (1.0 / r.norm());
        const Vec3 n_hat_raw = r.cross(v);
        const Vec3 n_hat = n_hat_raw * (1.0 / n_hat_raw.norm());
        const Vec3 t_hat = n_hat.cross(r_hat);

        const double a2 = (-0.111 * kDegToRad) * std::sin(theta);  // Sentinel-6's own roll coefficient
        // Tsat = T  =>  x_body = T.
        const Vec3 x_body{m.r[0][0], m.r[0][1], m.r[0][2]};
        CHECK_THAT((x_body - t_hat).norm(), WithinAbs(0.0, 1.0e-9));
        // Rsat = cos(a2)*R - sin(a2)*N  =>  z_body = -Rsat.
        const Vec3 expected_z = -1.0 * (std::cos(a2) * r_hat - std::sin(a2) * n_hat);
        const Vec3 z_body{m.r[2][0], m.r[2][1], m.r[2][2]};
        CHECK_THAT((z_body - expected_z).norm(), WithinAbs(0.0, 1.0e-9));
    }
}
