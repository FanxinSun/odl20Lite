// glonass_tests.cpp — SPEC-glonass-attitude.md §8, GLNY-A-001 through
// GLNY-A-006.

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

constexpr double kGlonassRadiusM = 25508e3;  ///< representative GLONASS MEO radius
constexpr double kDeg = std::numbers::pi / 180.0;
/// Kepler's third law at `kGlonassRadiusM` (GM=3.986004418e14 m^3/s^2) --
/// period ~11.26 h, matching GLONASS's own real ~11h16m period. USED for v's
/// own magnitude below, the SAME reasoning `galileo_tests.cpp`'s own
/// `kOmegaRadPerS` states: with v=omega*r along t_hat, |r x v|/|r|^2 = omega
/// EXACTLY, needed for GLNY-A-005/GLNY-A-006's own independent time
/// predictions to mean anything.
constexpr double kOmegaRadPerS = 1.5497e-4;
/// DIL11's own hardware rate and mean motion, independently restated here
/// (not `#include`d from the code under test) -- see attitude.cpp's own
/// `kGlonassHardwareYawRateRadPerS`/`kGlonassMuDotRadPerS`.
constexpr double kRateRadPerS = 0.25 * kDeg;
constexpr double kMuDotRadPerS = 0.00888 * kDeg;
constexpr double kEpsilon0Rad = 14.20 * kDeg;

struct OrbitFixture {
    Vec3 r_gcrs_m, v_gcrs_m_per_s, sun_gcrs;
};

/// Same construction as `attitude_tests.cpp`'s own `fixture_at`
/// (independently duplicated, the same small-fixture reasoning
/// `galileo_tests.cpp`'s own copy states): n_hat=Z, e0=X fixed, `mu_deg`
/// this tree's OWN convention (from midnight). DIL11's own mu IS this same
/// convention directly (its own Fig.1: "Midnight (mu=0deg)", "Noon
/// (mu=180deg)", matching `mu_rad`'s own header comment, "orbit angle from
/// midnight", exactly) -- unlike Galileo's own eta (from noon), no offset
/// is applied here.
[[nodiscard]] OrbitFixture fixture_at(double beta_deg, double mu_deg) {
    const double beta = beta_deg * kDeg;
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 r_hat = std::cos(mu) * e0 + std::sin(mu) * e1;
    const Vec3 t_hat = std::cos(mu) * e1 - std::sin(mu) * e0;
    const Vec3 s_hat = (-std::cos(beta)) * e0 + std::sin(beta) * n_hat;
    return {kGlonassRadiusM * r_hat, (kOmegaRadPerS * kGlonassRadiusM) * t_hat, s_hat};
}

[[nodiscard]] double wrap_pi(double a) noexcept {
    while (a > std::numbers::pi) a -= 2.0 * std::numbers::pi;
    while (a <= -std::numbers::pi) a += 2.0 * std::numbers::pi;
    return a;
}

[[nodiscard]] double max_component_diff(const Mat3& a, const Mat3& b) noexcept {
    double d = 0.0;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) d = std::max(d, std::abs(a.r[i][j] - b.r[i][j]));
    return d;
}

/// DIL11 Eq.1, "as printed" -- NOT this tree's own converted `psi_nominal`
/// (atan2(-tanb,-sinmu)): DIL11's own formula has NO negation on sin(mu).
/// Independently transcribed (a fresh reading of the source, not a call
/// into the code under test), used below to prove the pi-minus conversion
/// identity rather than assume it.
[[nodiscard]] double psi_dilssner_native(double beta, double mu) noexcept {
    return std::atan2(-std::tan(beta), std::sin(mu));
}

/// This file's own independent transcription of the SAME formula
/// `psi_nominal`/`frame_from_yaw` in attitude.cpp implement (GPS's own
/// convention, which DIL11 Sec.2.1 states it shares) -- used to build an
/// expected Mat3 without calling the code under test.
[[nodiscard]] Mat3 frame_from_psi_independent(const Vec3& r_hat, const Vec3& t_hat,
                                              const Vec3& n_hat, double psi) noexcept {
    const Vec3 z_body = -1.0 * r_hat;
    const Vec3 x_body = (-std::cos(psi)) * t_hat + (-std::sin(psi)) * n_hat;
    const Vec3 y_body = z_body.cross(x_body);
    Mat3 m;
    m.r[0] = {x_body.x, x_body.y, x_body.z};
    m.r[1] = {y_body.x, y_body.y, y_body.z};
    m.r[2] = {z_body.x, z_body.y, z_body.z};
    return m;
}

/// DIL11 Eq.2, "as printed" -- independently transcribed, the SAME
/// un-negated shape Eq.1 is; used only to fix the sign of DIL11's own
/// SIGN[R, psi_dot_n(mu_s)] term in the independent shadow/noon
/// reconstructions below.
[[nodiscard]] double psidot_dilssner_native(double beta, double mu) noexcept {
    const double t = std::tan(beta);
    const double s = std::sin(mu);
    return kMuDotRadPerS * t * std::cos(mu) / (s * s + t * t);
}

}  // namespace

// --- GLNY-A-001 ------------------------------------------------------------

TEST_CASE("GLNY-A-001  DIL11's own Eq.1 (native, un-negated sin(mu)) and "
          "this tree's own psi_nominal (KOUBA09's own converted form, "
          "DIL11 Sec.2.1's own stated shared convention) agree by exactly "
          "pi, at a spread of geometries -- and glonass_m_yaw_attitude, "
          "off both turns, returns EXACTLY what nominal_yaw_steering "
          "returns for the same (r, sun)",
          "[attitude][glonass]") {
    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {
        {45.0, 60.0}, {-30.0, 100.0}, {10.0, 150.0}, {-5.0, 220.0}, {20.0, 300.0}, {60.0, 45.0},
    };
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        // The pi-minus identity, independently, both sides transcribed
        // fresh (not reading attitude.cpp's own psi_nominal back). NOTE the
        // identity is ADDITIVE (atan2(y,-x) = pi - atan2(y,x), the standard
        // reflection-through-the-y-axis relation), not "the two differ by
        // pi": converted + native == pi (mod 2pi), not converted - native.
        const double native = psi_dilssner_native(c.beta_deg * kDeg, c.mu_deg * kDeg);
        const double converted = std::atan2(-std::tan(c.beta_deg * kDeg), -std::sin(c.mu_deg * kDeg));
        CHECK_THAT(wrap_pi(converted + native - std::numbers::pi), WithinAbs(0.0, 1.0e-9));

        // Off-turn (|beta|=45,60,30,... all >> beta_0=2.03deg and >>
        // epsilon_0=14.2deg for most; the two smallest-|beta| cases, 10 and
        // 5 deg, are still outside BOTH gates) reduces to nominal exactly.
        auto expected = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(expected.has_value());
        auto got = glonass_m_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        REQUIRE(got.has_value());
        CHECK(max_component_diff(*got, *expected) < 1.0e-9);
    }
}

// --- GLNY-A-002 --------------------------------------------------------------

TEST_CASE("GLNY-A-002  the shadow-crossing (midnight) turn matches an "
          "INDEPENDENT reconstruction of DIL11's own Eq.11-14: a full-rate "
          "ramp from shadow entry that reaches the nominal exit yaw and "
          "then holds there, constant, until actual shadow exit",
          "[attitude][glonass]") {
    struct Case { double beta_deg, mu_deg; const char* label; };
    const Case cases[] = {
        {1.0, -12.0, "ramp phase, well inside shadow entry"},
        {1.0, 0.0, "ramp/hold region, at orbit midnight"},
        {5.0, -5.0, "ramp phase, mid-latitude beta"},
        {-3.0, 4.0, "negative beta, hold region"},
    };
    for (const auto& c : cases) {
        const double beta = c.beta_deg * kDeg;
        const double cos_ratio = std::cos(kEpsilon0Rad) / std::cos(beta);
        REQUIRE(cos_ratio <= 1.0);  // every case above is inside eclipse season
        const double mu_e = std::acos(cos_ratio);
        const double mu_s = -mu_e;
        const double mu_query = c.mu_deg * kDeg;
        REQUIRE(mu_query >= mu_s);
        REQUIRE(mu_query <= mu_e);

        const double psi_s = psi_dilssner_native(beta, mu_s);
        const double psi_e = psi_dilssner_native(beta, mu_e);
        const double psidot_s = psidot_dilssner_native(beta, mu_s);
        const double ramp_dir = (psidot_s < 0.0) ? -kRateRadPerS : kRateRadPerS;
        const double mu_f = mu_s + (psi_e - psi_s) / ramp_dir * kMuDotRadPerS;
        const double psi_dilssner = (mu_query < mu_f) ? (psi_s + ramp_dir * (mu_query - mu_s) / kMuDotRadPerS)
                                                        : psi_e;
        const double psi_expected_tree = std::numbers::pi - psi_dilssner;

        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        const Vec3 n_hat{0.0, 0.0, 1.0};
        const Vec3 e0{1.0, 0.0, 0.0};
        const Vec3 e1 = n_hat.cross(e0);
        const Vec3 r_hat = std::cos(mu_query) * e0 + std::sin(mu_query) * e1;
        const Vec3 t_hat = std::cos(mu_query) * e1 - std::sin(mu_query) * e0;
        const Mat3 expected = frame_from_psi_independent(r_hat, t_hat, n_hat, wrap_pi(psi_expected_tree));

        auto got = glonass_m_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        REQUIRE(got.has_value());
        CHECK(max_component_diff(*got, expected) < 1.0e-6);
    }
}

// --- GLNY-A-003 --------------------------------------------------------------

TEST_CASE("GLNY-A-003  the noon turn matches an INDEPENDENT reconstruction "
          "of DIL11's own Eq.15-21: the four-iteration onset solve from "
          "DIL11's own stated 176.8deg seed, then a single ramp phase "
          "spanning the whole maneuver (no hold)",
          "[attitude][glonass]") {
    struct Case { double beta_deg, mu_deg; };
    // beta=0.001, not 0.0 exactly: at beta=0 the nominal yaw RATE (both
    // DIL11's own raw form and this tree's converted one) is exactly zero,
    // so DIL11's own stated rule ("the sign of the actual yaw rate is the
    // same as the sign of the nominal yaw rate at shadow entry") names no
    // unique direction there -- a genuine degeneracy in the source's own
    // rule, not a production defect (production's own tie-break, sign(0):=
    // +1, is documented at `glonass_m_shadow_turn`'s own header, the SAME
    // TYAW-R-007 convention already established for GPS) -- found this
    // session's own testing, avoided here rather than asserting one
    // arbitrary resolution is "the" answer.
    const Case cases[] = {{0.001, 178.0}, {1.0, 178.0}, {-1.5, 181.5}, {0.5, 177.5}};
    for (const auto& c : cases) {
        const double beta = c.beta_deg * kDeg;
        const double beta_abs = std::abs(beta);
        double mu_s = 176.8 * kDeg;
        for (int i = 0; i < 4; ++i) {
            const double s = std::sin(mu_s), cc = std::cos(mu_s);
            const double denom = beta_abs * beta_abs + s * s;
            const double num = std::atan(beta_abs / s) + beta_abs * mu_s * cc / denom +
                               std::numbers::pi * kRateRadPerS / kMuDotRadPerS - std::numbers::pi / 2.0;
            const double den = kRateRadPerS / kMuDotRadPerS + beta_abs * cc / denom;
            mu_s = num / den;
        }
        const double mu_e = 2.0 * std::numbers::pi - mu_s;
        const double mu_query = c.mu_deg * kDeg;
        REQUIRE(mu_query >= mu_s);
        REQUIRE(mu_query <= mu_e);

        const double psi_s = psi_dilssner_native(beta, mu_s);
        const double psidot_s = psidot_dilssner_native(beta, mu_s);
        const double ramp_dir = (psidot_s < 0.0) ? -kRateRadPerS : kRateRadPerS;
        const double psi_dilssner = psi_s + ramp_dir * (mu_query - mu_s) / kMuDotRadPerS;
        const double psi_expected_tree = std::numbers::pi - psi_dilssner;

        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        const Vec3 n_hat{0.0, 0.0, 1.0};
        const Vec3 e0{1.0, 0.0, 0.0};
        const Vec3 e1 = n_hat.cross(e0);
        const Vec3 r_hat = std::cos(mu_query) * e0 + std::sin(mu_query) * e1;
        const Vec3 t_hat = std::cos(mu_query) * e1 - std::sin(mu_query) * e0;
        const Mat3 expected = frame_from_psi_independent(r_hat, t_hat, n_hat, wrap_pi(psi_expected_tree));

        auto got = glonass_m_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        REQUIRE(got.has_value());
        CHECK(max_component_diff(*got, expected) < 1.0e-6);
    }
}

// --- GLNY-A-004 --------------------------------------------------------------

TEST_CASE("GLNY-A-004  the shadow turn's own hold phase is GENUINELY "
          "flat -- the INTRINSIC yaw angle psi (extracted through each "
          "point's own orbit triad, not the raw GCRS-frame Mat3, which "
          "rotates with orbital position regardless of psi) does not "
          "change between two query points both inside the hold region "
          "(mu_f < mu < mu_e) -- shown to be a real mechanism, not merely "
          "a wide ramp too shallow to notice",
          "[attitude][glonass][gate]") {
    auto extract_psi = [](const Mat3& m, double mu) {
        const Vec3 n_hat{0.0, 0.0, 1.0};
        const Vec3 t_hat{-std::sin(mu), std::cos(mu), 0.0};
        const Vec3 x{m.r[0][0], m.r[0][1], m.r[0][2]};
        return std::atan2(-x.dot(n_hat), -x.dot(t_hat));
    };
    // beta=1deg: mu_e=14.165deg, mu_s=-14.165deg (Eq.11), mu_f (the
    // ramp/hold boundary, solved by this file's own GLNY-A-002) =
    // -8.06deg -- -7deg and 14deg both sit inside (mu_f, mu_e), spanning
    // most of the hold region's own width.
    const auto a = fixture_at(1.0, -7.0);
    const auto b = fixture_at(1.0, 14.0);
    auto frame_a = glonass_m_yaw_attitude(a.r_gcrs_m, a.v_gcrs_m_per_s, a.sun_gcrs);
    auto frame_b = glonass_m_yaw_attitude(b.r_gcrs_m, b.v_gcrs_m_per_s, b.sun_gcrs);
    REQUIRE(frame_a.has_value());
    REQUIRE(frame_b.has_value());
    const double psi_a = extract_psi(*frame_a, -7.0 * kDeg);
    const double psi_b = extract_psi(*frame_b, 14.0 * kDeg);
    CHECK_THAT(wrap_pi(psi_b - psi_a), WithinAbs(0.0, 1.0e-6));

    // A deliberately-broken "no hold, keep tracking nominal" version (the
    // defect class this test exists to catch -- treating DIL11's own law
    // as "Kouba's family with new constants," continuing to track the
    // moving nominal curve instead of holding): psi_nominal itself swings
    // by about 168 deg between mu=-7deg and mu=14deg at beta=1deg, so a
    // law that forgot to hold would show a large step here, not a
    // near-zero one.
    const double psi_nom_a = std::atan2(-std::tan(1.0 * kDeg), -std::sin(-7.0 * kDeg));
    const double psi_nom_b = std::atan2(-std::tan(1.0 * kDeg), -std::sin(14.0 * kDeg));
    CHECK(std::abs(wrap_pi(psi_nom_b - psi_nom_a)) > 1.0);
}

// --- GLNY-A-005 --------------------------------------------------------------

TEST_CASE("GLNY-A-005  time direction (TYAW-A-012's own shape), shadow "
          "turn: true elapsed time to reach a REGISTERED milestone (the "
          "ramp/hold boundary mu_f) matches the geometry's own prediction "
          "-- shown firing on a reversed-velocity version",
          "[attitude][glonass][gate]") {
    const double beta_deg = 1.0;
    const double beta = beta_deg * kDeg;
    const double mu_e = std::acos(std::cos(kEpsilon0Rad) / std::cos(beta));
    const double mu_s = -mu_e;
    const double psi_s = psi_dilssner_native(beta, mu_s);
    const double psi_e = psi_dilssner_native(beta, mu_e);
    const double psidot_s = psidot_dilssner_native(beta, mu_s);
    const double ramp_dir = (psidot_s < 0.0) ? -kRateRadPerS : kRateRadPerS;
    const double mu_f = mu_s + (psi_e - psi_s) / ramp_dir * kMuDotRadPerS;

    // REGISTERED: starting at mu_s (t=0), mu_f is reached after
    // t_f = (mu_f - mu_s)/mu_dot of true elapsed time (mu_dot = the
    // fixture's own kOmegaRadPerS, since fixture_at's own v is built at
    // exactly that rate).
    const double t_f_s = (mu_f - mu_s) / kOmegaRadPerS;
    const double mu_at_t_f_deg = (mu_s + kOmegaRadPerS * t_f_s) / kDeg;
    REQUIRE_THAT(mu_at_t_f_deg, WithinAbs(mu_f / kDeg, 1.0e-9));

    const auto at_boundary = fixture_at(beta_deg, mu_at_t_f_deg);
    auto forward = glonass_m_yaw_attitude(at_boundary.r_gcrs_m, at_boundary.v_gcrs_m_per_s,
                                          at_boundary.sun_gcrs);
    REQUIRE(forward.has_value());

    // BROKEN: velocity reversed at mu_s, the SAME t_f elapsed -- a
    // satellite actually moving backwards would, after |t_f| of real
    // elapsed time, be at mu = mu_s - (mu_f - mu_s), well OUTSIDE the
    // shadow window on the wrong side (an independent ground truth, not
    // read from the code under test).
    const double mu_wrong_deg = (mu_s - (mu_f - mu_s)) / kDeg;
    const auto entry = fixture_at(beta_deg, mu_s / kDeg);
    const Vec3 v_reversed = -1.0 * entry.v_gcrs_m_per_s;
    const auto actually_reached = fixture_at(beta_deg, mu_wrong_deg);
    auto reversed = glonass_m_yaw_attitude(actually_reached.r_gcrs_m, v_reversed, actually_reached.sun_gcrs);
    REQUIRE(reversed.has_value());
    CHECK(max_component_diff(*forward, *reversed) > 0.1);
}

// --- GLNY-A-006 --------------------------------------------------------------

TEST_CASE("GLNY-A-006  rotation sense (TYAW-A-015's own shape), both "
          "turns: through each turn's own ramp region, the built law's "
          "own rate has the SAME SIGN as DIL11's own SIGN[R, psi_dot_n] "
          "rule states -- shown firing on a deliberately sense-flipped "
          "version",
          "[attitude][glonass][gate]") {
    auto extract_psi = [](const Mat3& m, double mu) {
        const Vec3 n_hat{0.0, 0.0, 1.0};
        const Vec3 t_hat{-std::sin(mu), std::cos(mu), 0.0};
        const Vec3 x{m.r[0][0], m.r[0][1], m.r[0][2]};
        return std::atan2(-x.dot(n_hat), -x.dot(t_hat));
    };
    // Shadow turn, well inside the ramp region (beta=1deg: mu_f is a
    // fraction of a degree past mu_s=-13.35deg -- probe near mu_s itself,
    // still on the ramp side, not at the hold plateau).
    {
        const double beta_deg = 1.0;
        constexpr double kMuProbeDeg = -13.0;
        constexpr double kStepDeg = 1.0e-3;
        const auto a = fixture_at(beta_deg, kMuProbeDeg - kStepDeg);
        const auto b = fixture_at(beta_deg, kMuProbeDeg + kStepDeg);
        auto frame_a = glonass_m_yaw_attitude(a.r_gcrs_m, a.v_gcrs_m_per_s, a.sun_gcrs);
        auto frame_b = glonass_m_yaw_attitude(b.r_gcrs_m, b.v_gcrs_m_per_s, b.sun_gcrs);
        REQUIRE(frame_a.has_value());
        REQUIRE(frame_b.has_value());
        const double d_built = wrap_pi(extract_psi(*frame_b, (kMuProbeDeg + kStepDeg) * kDeg) -
                                       extract_psi(*frame_a, (kMuProbeDeg - kStepDeg) * kDeg));

        const double beta = beta_deg * kDeg;
        const double mu_s = -std::acos(std::cos(kEpsilon0Rad) / std::cos(beta));
        const double psidot_s = psidot_dilssner_native(beta, mu_s);
        const double ramp_dir_dilssner = (psidot_s < 0.0) ? -kRateRadPerS : kRateRadPerS;
        // Expected tree-convention rate sign: d(psi_tree)/d(mu) =
        // -d(psi_dilssner)/d(mu), so the ramp's own tree-convention step
        // has the OPPOSITE sign from DIL11's own SIGN[R, psi_dot_n].
        const double d_expected = -ramp_dir_dilssner;
        CHECK(d_built * d_expected > 0.0);

        // BROKEN: sense-flipped (the wrong SIGN[] branch).
        const double d_broken = ramp_dir_dilssner;
        CHECK(d_built * d_broken < 0.0);
    }
    // Noon turn, well inside its own ramp region.
    {
        const double beta_deg = 1.0;
        constexpr double kMuProbeDeg = 178.5;
        constexpr double kStepDeg = 1.0e-3;
        const auto a = fixture_at(beta_deg, kMuProbeDeg - kStepDeg);
        const auto b = fixture_at(beta_deg, kMuProbeDeg + kStepDeg);
        auto frame_a = glonass_m_yaw_attitude(a.r_gcrs_m, a.v_gcrs_m_per_s, a.sun_gcrs);
        auto frame_b = glonass_m_yaw_attitude(b.r_gcrs_m, b.v_gcrs_m_per_s, b.sun_gcrs);
        REQUIRE(frame_a.has_value());
        REQUIRE(frame_b.has_value());
        const double d_built = wrap_pi(extract_psi(*frame_b, (kMuProbeDeg + kStepDeg) * kDeg) -
                                       extract_psi(*frame_a, (kMuProbeDeg - kStepDeg) * kDeg));

        const double beta = beta_deg * kDeg;
        double mu_s = 176.8 * kDeg;
        const double beta_abs = std::abs(beta);
        for (int i = 0; i < 4; ++i) {
            const double s = std::sin(mu_s), cc = std::cos(mu_s);
            const double denom = beta_abs * beta_abs + s * s;
            const double num = std::atan(beta_abs / s) + beta_abs * mu_s * cc / denom +
                               std::numbers::pi * kRateRadPerS / kMuDotRadPerS - std::numbers::pi / 2.0;
            const double den = kRateRadPerS / kMuDotRadPerS + beta_abs * cc / denom;
            mu_s = num / den;
        }
        const double psidot_s = psidot_dilssner_native(beta, mu_s);
        const double ramp_dir_dilssner = (psidot_s < 0.0) ? -kRateRadPerS : kRateRadPerS;
        const double d_expected = -ramp_dir_dilssner;
        CHECK(d_built * d_expected > 0.0);
        const double d_broken = ramp_dir_dilssner;
        CHECK(d_built * d_broken < 0.0);
    }
}
