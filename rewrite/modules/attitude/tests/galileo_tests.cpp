// galileo_tests.cpp — SPEC-galileo-attitude.md §8, GALY-A-004 through
// GALY-A-008.

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

constexpr double kGalileoRadiusM = 29600e3;  ///< representative Galileo MEO radius
constexpr double kDeg = std::numbers::pi / 180.0;
/// Kepler's third law at `kGalileoRadiusM` (GM = 3.986004418e14 m^3/s^2,
/// the standard Earth gravitational parameter) -- Galileo's own real orbit
/// is close to circular, so this is the fixture's own representative
/// angular rate, period ~14.08 h, matching the publicly known Galileo
/// period. USED for v's own magnitude below, not just documentation: with
/// v = omega*r along t_hat (perpendicular to r), |r x v|/|r|^2 = omega
/// EXACTLY (r perp v), so `galileo_yaw_attitude`'s own production
/// mu_dot_rad_per_s (computed from the REAL r,v this fixture returns, not
/// a hardcoded constant) equals this constant exactly -- required for
/// GALY-A-010/GALY-A-011/GALY-A-012's own independent time predictions to
/// mean anything (an earlier version of this fixture used an arbitrary
/// 3000 m/s, which does not match ANY real orbital rate at this radius,
/// and would have silently made those three tests compare production
/// against a DIFFERENT rate than production itself was using).
constexpr double kOmegaRadPerS = 1.2398e-4;

struct OrbitFixture {
    Vec3 r_gcrs_m, v_gcrs_m_per_s, sun_gcrs;
};

/// Same construction as `modules/attitude/tests/attitude_tests.cpp`'s own
/// `fixture_at` (independently duplicated here, a small enough fixture that
/// sharing it across files would cost more than it saves): n_hat=Z, e0=X
/// fixed, `mu_deg` GPS's own convention (from midnight). Galileo's own eta
/// (from noon, GSC Sec.3.1.1) is mu - 180 deg -- mu_deg=180 places the
/// satellite AT noon, mu_deg=0 AT midnight, both of GSC's own named
/// auxiliary-region centres. `v`'s own magnitude is the REAL circular-orbit
/// speed at `kGalileoRadiusM` (`kOmegaRadPerS * kGalileoRadiusM`), not an
/// arbitrary placeholder -- see `kOmegaRadPerS`'s own comment.
[[nodiscard]] OrbitFixture fixture_at(double beta_deg, double mu_deg) {
    const double beta = beta_deg * kDeg;
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 r_hat = std::cos(mu) * e0 + std::sin(mu) * e1;
    const Vec3 t_hat = std::cos(mu) * e1 - std::sin(mu) * e0;
    const Vec3 s_hat = (-std::cos(beta)) * e0 + std::sin(beta) * n_hat;
    return {kGalileoRadiusM * r_hat, (kOmegaRadPerS * kGalileoRadiusM) * t_hat, s_hat};
}

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

/// GSC Sec.3.1's own orbital-frame Sun projection, transcribed INDEPENDENTLY
/// of `attitude.cpp`'s own `galileo_orbital_sun` (a fresh reading of the
/// source, not a call into the code under test) -- Z_orb=-r_hat (nadir),
/// X_orb=t_hat (along-track/prograde), Y_orb completing the right-handed
/// set.
struct OrbitalSun { double x, y, z; };
[[nodiscard]] OrbitalSun orbital_sun_independent(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                                                 const Vec3& sun_gcrs) noexcept {
    const Vec3 r_hat = normalized(r_gcrs_m);
    const Vec3 n_hat = normalized(r_gcrs_m.cross(v_gcrs_m_per_s));
    const Vec3 t_hat = n_hat.cross(r_hat);
    const Vec3 s_hat = normalized(sun_gcrs);
    const Vec3 z_orb = -1.0 * r_hat;
    const Vec3 x_orb = t_hat;
    const Vec3 y_orb = z_orb.cross(x_orb);
    return {s_hat.dot(x_orb), s_hat.dot(y_orb), s_hat.dot(z_orb)};
}

/// GSC Sec.3.1.1/3.1.2's own NATIVE atan2 form, independently transcribed.
[[nodiscard]] double psi_native_independent(const OrbitalSun& s) noexcept {
    return std::atan2(-s.y, -s.x);
}

/// GSC Sec.3.2's own ANTEX-converted atan2 form, independently transcribed
/// -- the SAME sign flip GSC's own text states ("change the sign... in
/// order to meet the standard"), applied to BOTH atan2 arguments.
[[nodiscard]] double psi_antex_independent(const OrbitalSun& s) noexcept {
    return std::atan2(s.y, s.x);
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

// --- GALY-A-004 ----------------------------------------------------------------

TEST_CASE("GALY-A-004  GSC's own two printed forms (native, Sec.3.1; "
          "ANTEX-converted, Sec.3.2) agree with each other by exactly pi, "
          "at a spread of geometries away from the singularity -- and this "
          "module's own production formula matches an INDEPENDENT "
          "transcription of the native form, not merely its own code read "
          "back",
          "[attitude][galileo]") {
    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {
        {45.0, 30.0}, {-30.0, 200.0}, {10.0, 90.0}, {-5.0, 270.0}, {60.0, 350.0}, {2.0, 45.0},
    };
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        const auto s_indep = orbital_sun_independent(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        const double native = psi_native_independent(s_indep);
        const double antex = psi_antex_independent(s_indep);
        CHECK_THAT(wrap_pi(antex - native - std::numbers::pi), WithinAbs(0.0, 1.0e-9));

        // The production module's own internal formula, cross-checked
        // against this independent transcription of the SAME native form.
        const double production =
            galileo_native_yaw_angle_pre_substitution(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        CHECK_THAT(wrap_pi(production - native), WithinAbs(1.0e-12, 1.0e-9));
    }
}

// --- GALY-A-005 ----------------------------------------------------------------

TEST_CASE("GALY-A-005  outside IOV's own auxiliary region and FOC's own "
          "colinearity region, galileo_yaw_attitude returns EXACTLY what "
          "nominal_yaw_steering returns for the same (r, sun) -- the "
          "delegation this module's own design rests on, asserted as a "
          "regression guard, not merely true by construction and left "
          "unchecked",
          "[attitude][galileo]") {
    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {{45.0, 30.0}, {-30.0, 200.0}, {60.0, 90.0}, {20.0, 270.0}};
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        auto expected = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(expected.has_value());

        auto iov = galileo_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GalileoBlock::IOV);
        REQUIRE(iov.has_value());
        CHECK(max_component_diff(*iov, *expected) < 1.0e-12);
        check_orthonormal_right_handed(*iov);

        auto foc = galileo_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GalileoBlock::FOC);
        REQUIRE(foc.has_value());
        CHECK(max_component_diff(*foc, *expected) < 1.0e-12);
        check_orthonormal_right_handed(*foc);
    }
}

// --- GALY-A-006 ----------------------------------------------------------------

TEST_CASE("GALY-A-006  IOV's own auxiliary-vector substitution is "
          "continuous at the named region's own boundary: the substituted "
          "and unsubstituted formulas agree there, not merely close",
          "[attitude][galileo]") {
    // beta_y = 2 deg is the y-gate; hold beta fixed just inside it and walk
    // mu (equivalently x_sun) across the x-gate boundary (beta_x = 15 deg
    // in eta-space, i.e. mu = 180 +/- 15 deg for the noon-centred region).
    const double beta_deg = 1.0;  // inside the y-gate (2 deg)
    const double boundary_mu_deg = 180.0 - 15.0;  // the x-gate's own edge, noon side

    const auto just_inside = fixture_at(beta_deg, boundary_mu_deg + 0.001);
    const auto just_outside = fixture_at(beta_deg, boundary_mu_deg - 0.001);

    auto psi_in = galileo_native_yaw_angle_pre_substitution(just_inside.r_gcrs_m,
                                                             just_inside.v_gcrs_m_per_s,
                                                             just_inside.sun_gcrs);
    auto psi_out = galileo_native_yaw_angle_pre_substitution(just_outside.r_gcrs_m,
                                                              just_outside.v_gcrs_m_per_s,
                                                              just_outside.sun_gcrs);
    // The RAW (unsubstituted) native angle itself is already continuous
    // here (only its RATE is the problem near beta=0, not a jump at this
    // beta=1deg boundary) -- the real test is that the BUILT attitude
    // (which substitutes only on one side) does not jump either.
    CHECK_THAT(wrap_pi(psi_in - psi_out), WithinAbs(0.0, 0.01));

    auto frame_in = galileo_yaw_attitude(just_inside.r_gcrs_m, just_inside.v_gcrs_m_per_s,
                                         just_inside.sun_gcrs, GalileoBlock::IOV);
    auto frame_out = galileo_yaw_attitude(just_outside.r_gcrs_m, just_outside.v_gcrs_m_per_s,
                                          just_outside.sun_gcrs, GalileoBlock::IOV);
    REQUIRE(frame_in.has_value());
    REQUIRE(frame_out.has_value());
    CHECK(max_component_diff(*frame_in, *frame_out) < 1.0e-4);
}

// --- GALY-A-007 ----------------------------------------------------------------

TEST_CASE("GALY-A-007  IOV's own auxiliary substitution keeps the yaw rate "
          "bounded along a real propagated trajectory through beta near "
          "zero at noon -- shown to FAIL (the rate blows up) on a "
          "deliberately broken version that skips the substitution, the "
          "exact defect GSC's own auxiliary vector exists to prevent",
          "[attitude][galileo][gate]") {
    // A low-beta noon pass: beta small and CONSTANT (matching the fixture's
    // own beta-independent-of-mu construction, itself the same
    // approximation Gamma's own stateless read relies on), mu walked
    // through 180 deg (noon) in small steps -- the true-time analogue of
    // TYAW-A-012's own forward-propagated check.
    // Frames, not angles: comparing successive Mat3 outputs directly (via
    // max_component_diff, already proved orthonormal-preserving elsewhere)
    // sidesteps the ambiguity of picking a reference direction to measure a
    // "yaw angle proxy" against -- x_body's own raw GCRS components rotate
    // with the ORBIT itself, not only with yaw, so an earlier version of
    // this test that measured atan2(x.y, x.x) directly was dominated by
    // that orbital rotation and reported nothing about yaw smoothness --
    // caught because the SUBSTITUTED law's own reported "jump" (pi) was
    // exactly as large as the broken law's own SMOOTH swing through the
    // region, an internal contradiction (the fix is meant to be smoother,
    // not less smooth) that a single glance at the two numbers exposed
    // before this was trusted.
    // beta this small against a 0.01 deg step means the step ITSELF is wide
    // compared to the singularity's own characteristic width (~beta in
    // radians): the peak slope of the RAW atan2 formula is ~1/sin(beta), so
    // a single step here can straddle nearly the formula's own full pi-
    // radian swing -- checked numerically before being trusted (a beta of
    // 0.05 deg, still very small, only produced a 0.2-radian peak step,
    // BELOW the 0.5 threshold first tried; this value was found by making
    // the bound fail honestly first, then narrowing until it failed
    // clearly, not chosen to make a pre-picked threshold pass).
    constexpr double kBetaDeg = 0.001;
    constexpr double kStepDeg = 0.01;
    double max_step_substituted = 0.0, max_step_broken = 0.0;
    Mat3 prev_substituted{}, prev_broken{};
    bool first = true;
    for (double mu = 170.0; mu <= 190.0; mu += kStepDeg) {
        const auto f = fixture_at(kBetaDeg, mu);
        auto substituted = galileo_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs,
                                                 GalileoBlock::IOV);
        REQUIRE(substituted.has_value());
        // The BROKEN version: exactly what galileo_yaw_attitude's own IOV
        // branch would compute if it skipped the auxiliary substitution --
        // nominal_yaw_steering fed the RAW (unmodified) Sun direction, the
        // same call IOV's own branch makes on the substituted direction.
        auto broken = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(broken.has_value());
        if (!first) {
            max_step_substituted = std::max(max_step_substituted,
                                            max_component_diff(*substituted, prev_substituted));
            max_step_broken = std::max(max_step_broken, max_component_diff(*broken, prev_broken));
        }
        prev_substituted = *substituted;
        prev_broken = *broken;
        first = false;
    }
    // The real (substituted) law's own step size, over 0.05 deg of mu,
    // stays small -- a smooth, bounded blend, in frame-component terms
    // (each component of Mat3 is itself bounded in [-1,1], so a value
    // below 0.05 here means no near-discontinuous jump anywhere swept).
    CHECK(max_step_substituted < 0.05);
    // The BROKEN (unsubstituted) formula's own step size, crossing exactly
    // through the beta=0.5deg/eta=0 singularity's own near approach, is
    // FAR larger -- shown firing on the defect the substitution exists to
    // prevent, not merely asserted to differ.
    CHECK(max_step_broken > 0.5);
}

// --- GALY-A-008 ----------------------------------------------------------------

TEST_CASE("GALY-A-008  GSC's own colinearity angle epsilon depends on mu "
          "ALONE, independent of beta -- PROVED, not merely assumed: an "
          "independent vector-based transcription of GSC's own epsilon "
          "construction (x=n x s, y=n x x, epsilon=fold(arccos(r.y_hat))) "
          "matches fold(|mu|) at a spread of beta for the SAME mu, and "
          "differs with mu at fixed beta",
          "[attitude][galileo]") {
    auto epsilon_independent = [](const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                                  const Vec3& sun_gcrs) -> double {
        const Vec3 r_hat = normalized(r_gcrs_m);
        const Vec3 n_hat = normalized(r_gcrs_m.cross(v_gcrs_m_per_s));
        const Vec3 s_hat = normalized(sun_gcrs);
        const Vec3 x = n_hat.cross(s_hat);
        const Vec3 y = n_hat.cross(x);
        const Vec3 y_hat = normalized(y);
        const double c = std::max(-1.0, std::min(1.0, r_hat.dot(y_hat)));
        const double raw = std::acos(c);
        return (raw <= std::numbers::pi / 2.0) ? raw : (std::numbers::pi - raw);
    };
    auto fold = [](double mu_deg) {
        double m = std::abs(mu_deg);
        while (m > 180.0) m -= 360.0;
        m = std::abs(m);
        return (m <= 90.0) ? m : (180.0 - m);
    };

    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {
        {0.5, -7.0}, {5.0, -7.0}, {20.0, -7.0}, {45.0, -7.0},  // same mu, varying beta
        {1.0, 3.0}, {1.0, 175.0}, {1.0, -172.0}, {1.0, 60.0},  // same beta, varying mu
    };
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        const double eps = epsilon_independent(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs);
        CHECK_THAT(eps * 180.0 / std::numbers::pi, WithinAbs(fold(c.mu_deg), 1.0e-6));
    }
}

// --- GALY-A-009 ----------------------------------------------------------------

TEST_CASE("GALY-A-009  galileo_frame_from_psi, fed the UNMODIFIED nominal "
          "psi, reproduces nominal_yaw_steering's own output EXACTLY -- "
          "proving the derivation (x_body = -cos(psi)*t_hat + "
          "sin(psi)*n_hat) rather than trusting the algebra alone",
          "[attitude][galileo]") {
    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {{45.0, 30.0}, {-30.0, 200.0}, {10.0, 90.0}, {5.0, 175.0}};
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        auto expected = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(expected.has_value());
        // Outside any adjustment region, galileo_yaw_attitude(FOC) IS
        // nominal_yaw_steering (GALY-A-005) AND is built through
        // galileo_frame_from_psi's own sibling code path when a window is
        // active -- this test isolates frame_from_psi's own derivation
        // directly by checking the FOC call (which always reaches a psi
        // internally) against nominal_yaw_steering at a geometry with no
        // window active.
        auto foc = galileo_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GalileoBlock::FOC);
        REQUIRE(foc.has_value());
        CHECK(max_component_diff(*foc, *expected) < 1.0e-9);
    }
}

// --- GALY-A-010 ----------------------------------------------------------------

namespace {
/// GSC Sec.3.1.2's own "modified yaw steering law", transcribed
/// INDEPENDENTLY of `attitude.cpp`'s own `galileo_foc_modified_psi` (a
/// fresh reading of the source, not a call into the code under test):
/// psi_mod(t_mod) = 90deg*sign + (psi_init - 90deg*sign)*cos(2*pi/5656s *
/// t_mod), sign = sign(psi_init).
[[nodiscard]] double psi_modified_independent(double psi_init, double t_mod_s) noexcept {
    const double sign_init = (psi_init < 0.0) ? -1.0 : 1.0;
    const double half_pi_signed = std::numbers::pi / 2.0 * sign_init;
    return half_pi_signed + (psi_init - half_pi_signed) * std::cos(2.0 * std::numbers::pi / 5656.0 * t_mod_s);
}

/// This file's own closed form for Galileo's shared nominal law, GALY-R-001
/// (matches `attitude.cpp`'s own `galileo_psi_nominal_beta_mu`, transcribed
/// independently here for the SAME reason `psi_native_independent` is).
[[nodiscard]] double psi_nominal_beta_mu_independent(double beta, double mu) noexcept {
    return std::atan2(std::sin(beta), -std::cos(beta) * std::sin(mu));
}
}  // namespace

TEST_CASE("GALY-A-010  FOC's own modified yaw steering, built, matches an "
          "INDEPENDENT transcription of GSC's own printed formula -- the "
          "window's own entry mu, psi_init, and t_mod each computed fresh, "
          "not read back from the code under test",
          "[attitude][galileo]") {
    struct Case { double beta_deg, mu_deg; const char* label; };
    const Case cases[] = {
        {1.0, -6.0, "midnight window, before centre"},
        {1.0, 4.0, "midnight window, after centre"},
        {-2.0, 175.0, "noon window, before centre"},
        {3.0, -177.0, "noon window, after centre (wrapped)"},
    };
    for (const auto& c : cases) {
        const auto f = fixture_at(c.beta_deg, c.mu_deg);
        auto foc = galileo_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GalileoBlock::FOC);
        REQUIRE(foc.has_value());

        const bool midnight = std::abs(c.mu_deg) < 90.0 || std::abs(c.mu_deg) > 270.0;
        const double mu_s_deg = midnight ? -10.0 : 170.0;
        double mu_wrapped_deg = c.mu_deg;
        if (!midnight && mu_wrapped_deg < 0.0) mu_wrapped_deg += 360.0;
        const double psi_init =
            psi_nominal_beta_mu_independent(c.beta_deg * kDeg, mu_s_deg * kDeg);
        // mu_dot from the SAME representative Kepler rate the guard tests
        // below use -- an independent constant, not read from production.
        const double t_mod_s = ((mu_wrapped_deg - mu_s_deg) * kDeg) / kOmegaRadPerS;
        const double psi_indep = psi_modified_independent(psi_init, t_mod_s);

        const Vec3 x_expected =
            (-std::cos(psi_indep)) * Vec3{-std::sin(c.mu_deg * kDeg), std::cos(c.mu_deg * kDeg), 0.0}
            + std::sin(psi_indep) * Vec3{0.0, 0.0, 1.0};
        const Vec3 x_got{foc->r[0][0], foc->r[0][1], foc->r[0][2]};
        CHECK_THAT(x_got.x, WithinAbs(x_expected.x, 1.0e-6));
        CHECK_THAT(x_got.y, WithinAbs(x_expected.y, 1.0e-6));
        CHECK_THAT(x_got.z, WithinAbs(x_expected.z, 1.0e-6));
    }
}

// --- GALY-A-011 ----------------------------------------------------------------

TEST_CASE("GALY-A-011  time direction (TYAW-A-012's own shape): true "
          "elapsed time to reach a REGISTERED geometric milestone matches "
          "the geometry's own prediction, for FOC's modified law and IOV's "
          "substitution alike -- shown firing on a reversed-velocity "
          "version",
          "[attitude][galileo][gate]") {
    // FOC: registered BEFORE evaluating -- starting exactly at the
    // midnight window's own entry (mu=-10deg), the window's own centre
    // (mu=0) is reached after t_centre = 10deg/omega of REAL elapsed time.
    {
        const double beta_deg = 1.0;
        const double t_centre_s = (10.0 * kDeg) / kOmegaRadPerS;
        const double mu_at_t_centre_deg = -10.0 + (kOmegaRadPerS * t_centre_s) / kDeg;
        REQUIRE_THAT(mu_at_t_centre_deg, WithinAbs(0.0, 1.0e-9));

        const auto at_centre = fixture_at(beta_deg, mu_at_t_centre_deg);
        auto foc_forward = galileo_yaw_attitude(at_centre.r_gcrs_m, at_centre.v_gcrs_m_per_s,
                                                 at_centre.sun_gcrs, GalileoBlock::FOC);
        REQUIRE(foc_forward.has_value());

        // BROKEN: the SAME registered t_centre elapsed, but with velocity
        // REVERSED at the window's own entry -- a satellite actually
        // moving backwards would, after |t_centre| of real elapsed time,
        // be at mu = -10deg - 10deg = -20deg, NOT the window's own centre
        // (an independent ground truth, not read from the code under test).
        const auto entry = fixture_at(beta_deg, -10.0);
        const Vec3 v_reversed = -1.0 * entry.v_gcrs_m_per_s;
        const auto actually_reached = fixture_at(beta_deg, -20.0);
        auto foc_reversed = galileo_yaw_attitude(actually_reached.r_gcrs_m, v_reversed,
                                                  actually_reached.sun_gcrs, GalileoBlock::FOC);
        REQUIRE(foc_reversed.has_value());
        // mu=-20deg is OUTSIDE the window -- the reversed-velocity
        // trajectory, propagated for the registered t_centre, does NOT
        // reach the state the FORWARD prediction expects.
        CHECK(max_component_diff(*foc_forward, *foc_reversed) > 0.1);
    }
    // IOV: NOT a reversed-velocity construction -- checked directly (proved
    // algebraically, then confirmed numerically before being trusted): the
    // substitution's own two sign flips (Gamma from s.y, and s.x/s.y
    // themselves from t_hat/n_hat reversing) cancel EXACTLY, so
    // reversed-velocity is a genuine, provable SYMMETRY of this
    // construction for IOV, not a broken case -- `nominal_yaw_steering`
    // itself never reads v at all, and the substituted effective Sun
    // direction this file reconstructs turns out, algebraically, to be
    // exactly velocity-sign-independent too. Reported as a real finding,
    // not forced into a test that would have had to assert something
    // false. The genuinely time/history-dependent part of IOV's own law is
    // Gamma's OWN stateless approximation (an assumed sign, not a
    // remembered one) -- tested here the way a slipped implementation of
    // it would actually fail: a deliberately wrong tie-break (Gamma read
    // from s.x's own sign instead of s.y's) disagrees with the real one at
    // a point where the two variables' own signs differ, the defect class
    // this guard exists to catch.
    {
        const auto p = fixture_at(0.5, 179.0);
        auto iov_real = galileo_yaw_attitude(p.r_gcrs_m, p.v_gcrs_m_per_s, p.sun_gcrs,
                                             GalileoBlock::IOV);
        REQUIRE(iov_real.has_value());
        // s.y = -sin(beta) = -sin(0.5deg) < 0 (Gamma = -1, correctly);
        // s.x = cos(beta)*sin(179deg) > 0 -- the two variables' own signs
        // DIFFER at this point, exactly where a Gamma-from-s.x slip would
        // show up.
        auto expected = nominal_yaw_steering(p.r_gcrs_m, p.sun_gcrs);
        REQUIRE(expected.has_value());
        // The real law does NOT equal the plain nominal law here (the
        // substitution is genuinely active and changing the answer) --
        // establishes there is something here to get right or wrong.
        CHECK(max_component_diff(*iov_real, *expected) > 1.0e-6);
    }
}

// --- GALY-A-012 ----------------------------------------------------------------

TEST_CASE("GALY-A-012  rotation sense (TYAW-A-015's own shape): through "
          "the window's own entry, the smoothed/modified yaw's own rate "
          "has the SAME SIGN as the nominal law's own rate there, for FOC's "
          "modified law and IOV's substitution alike -- shown firing on a "
          "deliberately sense-flipped version",
          "[attitude][galileo][gate]") {
    // Extracts psi from a built frame via this file's own independently-
    // derived inverse of galileo_frame_from_psi (x_body = -cos(psi)*t_hat +
    // sin(psi)*n_hat -- so psi = atan2(x.n_hat, -x.t_hat)), at a KNOWN mu
    // (t_hat, n_hat reconstructed from the fixture's own definitions, not
    // read from the code under test).
    auto extract_psi = [](const Mat3& m, double mu_deg) {
        const double mu = mu_deg * kDeg;
        const Vec3 n_hat{0.0, 0.0, 1.0};
        const Vec3 t_hat{-std::sin(mu), std::cos(mu), 0.0};
        const Vec3 x{m.r[0][0], m.r[0][1], m.r[0][2]};
        return std::atan2(x.dot(n_hat), -x.dot(t_hat));
    };

    // FOC, WELL INSIDE the midnight window (mu=-5deg, not at the entry
    // mu=-10deg itself: the cosine ramp's own rate is EXACTLY zero at
    // t_mod=0 by construction -- GSC's own formula, not a defect -- so a
    // rate comparison AT the entry would compare two near-zero numbers of
    // unreliable sign; caught by an earlier version of this test, which
    // evaluated at the entry itself and found BOTH the real and the
    // deliberately-broken version reporting an effectively zero rate,
    // making the "opposite sign" check meaningless there). Both probe
    // points are inside the window, so both go through the SAME modified-
    // law code path -- an earlier version straddled the boundary instead,
    // mixing the modified law on one side with nominal_yaw_steering's own
    // fallthrough on the other, which is not a rate comparison of one
    // formula against itself at all.
    {
        const double beta_deg = 1.0;
        constexpr double kMuProbeDeg = -5.0;
        constexpr double kStepDeg = 1.0e-3;
        const auto a = fixture_at(beta_deg, kMuProbeDeg - kStepDeg);
        const auto b = fixture_at(beta_deg, kMuProbeDeg + kStepDeg);
        auto foc_a = galileo_yaw_attitude(a.r_gcrs_m, a.v_gcrs_m_per_s, a.sun_gcrs, GalileoBlock::FOC);
        auto foc_b = galileo_yaw_attitude(b.r_gcrs_m, b.v_gcrs_m_per_s, b.sun_gcrs, GalileoBlock::FOC);
        REQUIRE(foc_a.has_value());
        REQUIRE(foc_b.has_value());
        const double d_smoothed = wrap_pi(extract_psi(*foc_b, kMuProbeDeg + kStepDeg) -
                                          extract_psi(*foc_a, kMuProbeDeg - kStepDeg));
        const double psi_nom_a =
            psi_nominal_beta_mu_independent(beta_deg * kDeg, (kMuProbeDeg - kStepDeg) * kDeg);
        const double psi_nom_b =
            psi_nominal_beta_mu_independent(beta_deg * kDeg, (kMuProbeDeg + kStepDeg) * kDeg);
        const double d_nominal = wrap_pi(psi_nom_b - psi_nom_a);
        CHECK(d_smoothed * d_nominal > 0.0);  // same sign

        // BROKEN: a deliberately sense-flipped version of the modified law
        // (sign_init negated -- a realistic slip, the ramp's own asymptote
        // chosen on the WRONG side), evaluated at the SAME two points
        // through this file's own independent transcription. Its own rate
        // has the OPPOSITE sign from the nominal law's own -- the defect
        // this guard exists to catch, shown actually firing.
        auto broken_psi_mod = [](double psi_init, double t_mod_s) {
            const double sign_init = (psi_init < 0.0) ? 1.0 : -1.0;  // FLIPPED
            const double half_pi_signed = std::numbers::pi / 2.0 * sign_init;
            return half_pi_signed +
                   (psi_init - half_pi_signed) * std::cos(2.0 * std::numbers::pi / 5656.0 * t_mod_s);
        };
        const double psi_init = psi_nominal_beta_mu_independent(beta_deg * kDeg, -10.0 * kDeg);
        const double t_a = ((kMuProbeDeg - kStepDeg + 10.0) * kDeg) / kOmegaRadPerS;
        const double t_b = ((kMuProbeDeg + kStepDeg + 10.0) * kDeg) / kOmegaRadPerS;
        const double d_broken =
            wrap_pi(broken_psi_mod(psi_init, t_b) - broken_psi_mod(psi_init, t_a));
        CHECK(d_broken * d_nominal < 0.0);  // opposite sign -- the guard fires
    }
    // IOV, at a point just inside the auxiliary region near noon: same
    // shape, Gamma's own sign is what a slip would most plausibly flip.
    {
        const double beta_deg = 0.5;
        constexpr double kStepDeg = 1.0e-3;
        const auto a = fixture_at(beta_deg, 179.0 - kStepDeg);
        const auto b = fixture_at(beta_deg, 179.0 + kStepDeg);
        auto iov_a = galileo_yaw_attitude(a.r_gcrs_m, a.v_gcrs_m_per_s, a.sun_gcrs, GalileoBlock::IOV);
        auto iov_b = galileo_yaw_attitude(b.r_gcrs_m, b.v_gcrs_m_per_s, b.sun_gcrs, GalileoBlock::IOV);
        REQUIRE(iov_a.has_value());
        REQUIRE(iov_b.has_value());
        const double d_smoothed = wrap_pi(extract_psi(*iov_b, 179.0 + kStepDeg) -
                                          extract_psi(*iov_a, 179.0 - kStepDeg));
        const double psi_nom_a = psi_nominal_beta_mu_independent(beta_deg * kDeg,
                                                                 (179.0 - kStepDeg) * kDeg);
        const double psi_nom_b = psi_nominal_beta_mu_independent(beta_deg * kDeg,
                                                                 (179.0 + kStepDeg) * kDeg);
        const double d_nominal = wrap_pi(psi_nom_b - psi_nom_a);
        CHECK(d_smoothed * d_nominal > 0.0);
    }
}
