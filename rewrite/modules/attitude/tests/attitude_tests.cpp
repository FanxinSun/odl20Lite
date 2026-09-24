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

// --- SPEC-thrust-yaw test fixtures ------------------------------------

constexpr double kGpsRadiusM = 26561e3;
constexpr double kDeg = std::numbers::pi / 180.0;

struct OrbitFixture {
    Vec3 r_gcrs_m, v_gcrs_m_per_s, sun_gcrs;
};

/// An exact (r, v, sun) fixture for a CHOSEN (beta_deg, mu_deg), via a fixed
/// orbit normal n_hat = Z and in-plane reference e0 = X: r_hat(mu) =
/// cos(mu)*e0 + sin(mu)*e1, so that KOUBA09's own mu (measured from
/// midnight, positive prograde) comes out exactly as requested -- derived
/// and verified (PROVENANCE.md Sec.30.14) against `gps_yaw_attitude`'s own
/// beta/mu computation, not merely asserted. The sin(mu) terms here are
/// the OPPOSITE sign from an earlier version of this fixture (which matched
/// `mu_rad`'s own pre-fix, negated convention) -- `mu_rad`'s own fix is
/// what moved, this fixture's own JOB (produce the state whose mu is the
/// requested mu_deg) has not.
OrbitFixture fixture_at(double beta_deg, double mu_deg) {
    const double beta = beta_deg * kDeg;
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 r_hat = std::cos(mu) * e0 + std::sin(mu) * e1;
    const Vec3 t_hat = std::cos(mu) * e1 - std::sin(mu) * e0;
    const Vec3 s_hat = (-std::cos(beta)) * e0 + std::sin(beta) * n_hat;
    return {kGpsRadiusM * r_hat, 3000.0 * t_hat, s_hat};
}

/// Recovers the yaw angle psi from a returned body-x axis at a KNOWN mu,
/// inverting `frame_from_yaw`'s own x_body = -cos(psi)*t_hat - sin(psi)*n_hat
/// (attitude.cpp), using the SAME (n_hat, t_hat) `fixture_at` itself builds
/// (PROVENANCE.md Sec.30.14's own sign, matching the fixture above).
double recover_psi(const Vec3& x_body, double mu_deg) {
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 t_hat = std::cos(mu) * e1 - std::sin(mu) * e0;
    return std::atan2(-x_body.dot(n_hat), -x_body.dot(t_hat));
}

double max_component_diff(const Mat3& a, const Mat3& b) {
    double d = 0.0;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) d = std::max(d, std::abs(a.r[i][j] - b.r[i][j]));
    return d;
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

TEST_CASE("TYAW-A-001  the beta0 = atan(mu_dot/R) derived relation reproduces KOUBA09's "
          "own printed turn-onset thresholds for every rate-limited-ramp block",
          "[attitude][gate]") {
    // IIF's own night side no longer has a beta0 (TYAW-R-003, Shape E:
    // active whenever eclipsed, not when a hardware rate limit is first
    // reached) -- removed from this table, not merely left unchecked
    // (PROVENANCE.md Sec.30.10 records why).
    constexpr double kMuDot = 0.00836;  // KOUBA09 Eq. 6, deg/s
    struct Case { const char* label; double rate_deg_s; double printed_deg; double tol_deg; };
    const Case cases[] = {
        {"II/IIA fast end (KOUBA09 Table 1 max, PRN 27)", 0.134, 3.6, 0.05},
        {"II/IIA slow end (KOUBA09 Table 1 min, PRN 10)", 0.098, 4.9, 0.05},
        {"IIR (KOUBA09's single stated hardware rate)", 0.20, 2.4, 0.05},
    };
    for (const auto& c : cases) {
        INFO(c.label);
        const double beta0_deg = std::atan(kMuDot / c.rate_deg_s) / kDeg;
        CHECK_THAT(beta0_deg, WithinAbs(c.printed_deg, c.tol_deg));
    }
}

TEST_CASE("TYAW-A-002  II/IIA and IIR noon-turn continuity at hand-over: exact at onset "
          "by construction, and back to nominal well past the turn",
          "[attitude][gate]") {
    struct BlockCase { const char* label; GpsBlock block; HardwareYawRates rates; };
    const BlockCase blocks[] = {
        {"II/IIA", GpsBlock::II_IIA, HardwareYawRates{0.122, 0.122, 0.5, 0.0017}},
        {"IIR/IIR-M", GpsBlock::IIR_IIRM, HardwareYawRates{0.20, 0.20, 0.0, 0.0}},
    };
    const double beta_deg = 1.0;  // well inside beta0 for both blocks

    for (const auto& b : blocks) {
        INFO(b.label);
        const double beta0_deg = std::atan(0.00836 / b.rates.noon_deg_per_s) / kDeg;
        REQUIRE(beta_deg < beta0_deg);  // fixture sanity: this beta really triggers a turn
        const double half_width_deg = std::sqrt(beta0_deg * beta_deg - beta_deg * beta_deg);
        const double mu_s_deg = 180.0 - half_width_deg;

        // Exact at onset: KOUBA09's own construction starts the ramp AT
        // psi_n(mu_s), so the two agree by construction, not by an
        // independent computation happening to coincide.
        {
            auto f = fixture_at(beta_deg, mu_s_deg);
            auto turn = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, b.block, b.rates);
            auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
            REQUIRE(turn.has_value());
            REQUIRE(nom.has_value());
            CHECK(max_component_diff(*turn, *nom) < 1.0e-9);
        }

        // Well past the turn: KOUBA09's own stated "up to 30 min" (II/IIA)
        // and "up to 15 min" (IIR) both correspond to well under 20 deg of
        // mu at mu_dot -- 25 deg past center is a generous margin.
        {
            auto f = fixture_at(beta_deg, 180.0 + 25.0);
            auto turn = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, b.block, b.rates);
            auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
            REQUIRE(turn.has_value());
            REQUIRE(nom.has_value());
            CHECK(max_component_diff(*turn, *nom) < 1.0e-6);
        }
    }
}

TEST_CASE("TYAW-A-002b  the near-beta=0 regime the manager's own review found: the "
          "nominal law's own swing through a turn tends to 180 deg as beta shrinks, so "
          "catch-up must still be detected correctly there, not just at moderate beta",
          "[attitude][gate]") {
    // beta = 0.005 deg, R = 0.122 deg/s (II/IIA): ground-truth simulation of
    // KOUBA09's own operational catch-up rule (ATTD's own PROVENANCE record)
    // gives mu_s = 179.8601 deg, mu_e = 192.0531 deg -- a swing of ~177 deg,
    // close enough to 180 that the earlier (wrong, bounded) fix would have
    // handed back to nominal before this point was reached.
    const HardwareYawRates rates{0.122, 0.122, 0.5, 0.0017};
    const double beta_deg = 0.005;

    // Still active, just before the true catch-up (mu_e - 0.5 deg).
    {
        auto f = fixture_at(beta_deg, 191.553);
        auto turn = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::II_IIA, rates);
        auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(turn.has_value());
        REQUIRE(nom.has_value());
        CHECK(max_component_diff(*turn, *nom) > 1.0e-3);  // genuinely still diverged from nominal
    }

    // Past the true catch-up (mu_e + 5 deg): back to matching nominal.
    {
        auto f = fixture_at(beta_deg, 197.053);
        auto turn = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::II_IIA, rates);
        auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
        REQUIRE(turn.has_value());
        REQUIRE(nom.has_value());
        CHECK(max_component_diff(*turn, *nom) < 1.0e-6);
    }
}

TEST_CASE("TYAW-A-003  II/IIA shadow-crossing yaw rate never exceeds the hardware rate, "
          "spin-up phase included",
          "[attitude][gate]") {
    const HardwareYawRates rates{0.122, 0.122, 0.5, 0.0017};
    const double beta_deg = 0.5;  // deep eclipse season, well inside the 13.5 deg shadow half-angle
    const double R_rad_s = rates.noon_deg_per_s * kDeg;
    const double dmu_deg = 0.02;
    const double dt_s = (dmu_deg * kDeg) / (0.00836 * kDeg);

    double prev_psi = 0.0;
    bool have_prev = false;
    int checked = 0;

    // Swept range stays inside sqrt(13.5^2 - 0.5^2) =~ 13.49 deg, so every
    // sample is genuinely within the shadow-crossing window.
    for (double mu_deg = -13.0; mu_deg <= 13.0; mu_deg += dmu_deg) {
        auto f = fixture_at(beta_deg, mu_deg);
        auto res = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::II_IIA, rates);
        REQUIRE(res.has_value());
        const Vec3 x_body{res->r[0][0], res->r[0][1], res->r[0][2]};
        const double psi = recover_psi(x_body, mu_deg);
        if (have_prev) {
            double dpsi = psi - prev_psi;
            while (dpsi > std::numbers::pi) dpsi -= 2.0 * std::numbers::pi;
            while (dpsi < -std::numbers::pi) dpsi += 2.0 * std::numbers::pi;
            const double psidot = dpsi / dt_s;
            CHECK(std::abs(psidot) <= R_rad_s * 1.01);  // 1% margin for the finite difference
            ++checked;
        }
        prev_psi = psi;
        have_prev = true;
    }
    REQUIRE(checked > 1000);  // sanity: the sweep really covered the window densely
}

TEST_CASE("TYAW-A-004  IIF's own noon (Shape F) and shadow (Shape E) turn durations each "
          "checked against DIL10's own stated ceiling, not against each other",
          "[attitude][gate]") {
    // rates.night_deg_per_s is not read for IIF under Shape E (attitude.hpp).
    const HardwareYawRates rates{0.11, 0.0, 0.0, 0.0};

    // Noon (Shape F, TYAW-R-002/R-003 -- confirmed, not left as `evaluate_
    // turn_lead`, once mu_rad's own sign was fixed, PROVENANCE.md Sec.30.14):
    // DIL10's own "lasts about 27 minutes AT MOST" is the beta -> 0 LIMIT of
    // Shape F's own TOTAL duration (onset to catch-up) -- verified in
    // PROVENANCE.md Sec.30.10 to increase monotonically as beta shrinks,
    // approaching ~27.2 min, not a value at some unstated moderate beta.
    // beta = 0 EXACTLY refuses here (TYAW-A-006's own noon-side singularity:
    // the ramp's own half-width vanishes at beta = 0 exactly), so a very
    // small beta stands in for the limit DIL10 describes. The onset is
    // TYAW-P-1's own closed form; only the catch-up end needs a search
    // (mirroring TYAW-A-002's own construction).
    {
        const double beta_deg = 0.001;
        const double onset_deg = std::atan(0.00836 / 0.11) / kDeg;
        const double half_width_deg = std::sqrt(onset_deg * beta_deg - beta_deg * beta_deg);
        double d_found = -1.0;
        for (double d = 0.05; d < 30.0; d += 0.01) {
            auto f = fixture_at(beta_deg, 180.0 + d);
            auto turn = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::IIF, rates);
            auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
            REQUIRE(turn.has_value());
            REQUIRE(nom.has_value());
            if (max_component_diff(*turn, *nom) < 1.0e-6) { d_found = d; break; }
        }
        REQUIRE(d_found > 0.0);  // sanity: catch-up really found within the search range
        const double noon_duration_min = (half_width_deg + d_found) / 0.00836 / 60.0;
        INFO("noon total duration(min)=" << noon_duration_min);
        CHECK_THAT(noon_duration_min, WithinAbs(27.14, 1.5));  // DIL10's own "about 27...at most"
    }

    // Shadow (Shape E), at beta = 0 exactly (DIL10's own stated condition
    // for "about 55 minutes"): the active window is closed-form
    // (+-kShadowHalfAngleRad, TYAW-R-001's own E_sh borrowed for IIF, no
    // search needed for the window itself) -- checked here by confirming
    // the law is genuinely active (diverged from nominal) well inside it
    // and has fallen through to nominal exactly well outside it; continuity
    // AT the boundary follows from construction (TYAW-P-2, psi is defined
    // to equal the unwrapped nominal value there), not re-tested pointwise.
    {
        const double beta_deg = 0.0;
        auto inside = fixture_at(beta_deg, 10.0);
        auto outside = fixture_at(beta_deg, 13.6);
        auto turn_in = gps_yaw_attitude(inside.r_gcrs_m, inside.v_gcrs_m_per_s, inside.sun_gcrs,
                                         GpsBlock::IIF, rates);
        auto turn_out = gps_yaw_attitude(outside.r_gcrs_m, outside.v_gcrs_m_per_s, outside.sun_gcrs,
                                          GpsBlock::IIF, rates);
        auto nom_in = nominal_yaw_steering(inside.r_gcrs_m, inside.sun_gcrs);
        auto nom_out = nominal_yaw_steering(outside.r_gcrs_m, outside.sun_gcrs);
        REQUIRE(turn_in.has_value());
        REQUIRE(turn_out.has_value());
        REQUIRE(nom_in.has_value());
        REQUIRE(nom_out.has_value());
        CHECK(max_component_diff(*turn_in, *nom_in) > 1.0e-2);    // well inside: genuinely diverged
        CHECK(max_component_diff(*turn_out, *nom_out) < 1.0e-9);  // well outside: fallen through exactly

        // 2*E_sh/mu_dot, the ACTUAL implemented duration -- not the
        // registration paragraph's own illustrative "55.4 min" (Sec.4.3),
        // which used the raw point-source asin(R_E/a) = 13.897 deg, not the
        // widened kShadowHalfAngleRad = 13.5 deg this law actually uses
        // (PROVENANCE.md Sec.30.10).
        const double shadow_duration_min = 2.0 * 13.5 / 0.00836 / 60.0;
        CHECK(std::abs(shadow_duration_min - 55.0) < 0.05 * 55.0);  // DIL10's own "about 55 minutes"
    }
}

TEST_CASE("TYAW-A-004b  Shape E's own closed-form swing (attitude.cpp's "
          "evaluate_shadow_constant_rate, ATAN(sin(mu)/tan(beta))) matches an INDEPENDENT "
          "numerical integration of the nominal law's own rate across the shadow window, "
          "read through the psi it hands to gps_yaw_attitude, not re-derived from the same "
          "formula twice",
          "[attitude][gate]") {
    const HardwareYawRates rates{0.11, 0.0, 0.0, 0.0};  // night_deg_per_s unread for IIF

    // Independent numerical swing: a plain small-step nearest-branch
    // accumulation of psi_nominal itself (the SAME technique this project
    // already relies on between adjacent `wrap_near` calls elsewhere, not
    // the closed form under test) -- built here from `nominal_yaw_steering`
    // (a call this test does not otherwise exercise for IIF), not from
    // attitude.cpp's own internal psi_nominal, so the two routes genuinely
    // do not share code.
    auto numerical_swing_deg = [&](double beta_deg) {
        const double half_width_deg = std::sqrt(13.5 * 13.5 - beta_deg * beta_deg);
        constexpr int kSteps = 20000;
        const double step_deg = 2.0 * half_width_deg / kSteps;
        double mu_deg = -half_width_deg;
        auto psi_at = [&](double mu_d) {
            auto f = fixture_at(beta_deg, mu_d);
            auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
            REQUIRE(nom.has_value());  // beta != 0 here, so never the Sun-on-nadir singularity
            return recover_psi(Vec3{nom->r[0][0], nom->r[0][1], nom->r[0][2]}, mu_d) / kDeg;
        };
        double psi_walk = psi_at(mu_deg);
        const double psi_entry = psi_walk;
        for (int i = 0; i < kSteps; ++i) {
            mu_deg += step_deg;
            double psi_next = psi_at(mu_deg);
            while (psi_next - psi_walk > 180.0) psi_next -= 360.0;
            while (psi_next - psi_walk < -180.0) psi_next += 360.0;
            psi_walk = psi_next;
        }
        return psi_walk - psi_entry;
    };

    for (double beta_deg : {4.2173, 1.0, 0.1, 0.01}) {
        INFO("beta(deg)=" << beta_deg);
        const double numerical = numerical_swing_deg(beta_deg);

        // Read the closed form back out through the production function
        // itself: the swing is (psi at mu_exit) - (psi at mu_entry),
        // unwrapped, both read from gps_yaw_attitude(IIF) directly.
        const double half_width_deg = std::sqrt(13.5 * 13.5 - beta_deg * beta_deg);
        auto f_entry = fixture_at(beta_deg, -half_width_deg);
        auto f_exit = fixture_at(beta_deg, half_width_deg);
        auto turn_entry = gps_yaw_attitude(f_entry.r_gcrs_m, f_entry.v_gcrs_m_per_s, f_entry.sun_gcrs,
                                            GpsBlock::IIF, rates);
        auto turn_exit = gps_yaw_attitude(f_exit.r_gcrs_m, f_exit.v_gcrs_m_per_s, f_exit.sun_gcrs,
                                           GpsBlock::IIF, rates);
        REQUIRE(turn_entry.has_value());
        REQUIRE(turn_exit.has_value());
        const double psi_entry_deg =
            recover_psi(Vec3{turn_entry->r[0][0], turn_entry->r[0][1], turn_entry->r[0][2]},
                        -half_width_deg) / kDeg;
        double psi_exit_deg =
            recover_psi(Vec3{turn_exit->r[0][0], turn_exit->r[0][1], turn_exit->r[0][2]},
                        half_width_deg) / kDeg;
        while (psi_exit_deg - psi_entry_deg > 180.0) psi_exit_deg -= 360.0;
        while (psi_exit_deg - psi_entry_deg < -180.0) psi_exit_deg += 360.0;
        const double closed_form = psi_exit_deg - psi_entry_deg;

        CHECK_THAT(closed_form, WithinAbs(numerical, 1.0e-3));
    }
}

TEST_CASE("TYAW-A-005  IIIA is bit-identical to IIF at the same inputs -- the stopgap is "
          "exactly what it claims to be, not a fifth implementation",
          "[attitude][gate]") {
    // night_deg_per_s/yaw_bias_deg are not read for IIF/IIIA under Shape E
    // (attitude.hpp) -- left at 0 rather than a stale value that would
    // misleadingly suggest they still matter here.
    const HardwareYawRates rates{0.11, 0.0, 0.0, 0.0};
    struct Case { double beta_deg, mu_deg; };
    const Case cases[] = {{1.0, 180.0}, {1.0, 179.0}, {1.0, 0.0}, {5.0, 90.0}, {0.02, 0.0}};
    for (const auto& c : cases) {
        INFO("beta=" << c.beta_deg << " mu=" << c.mu_deg);
        auto f = fixture_at(c.beta_deg, c.mu_deg);
        auto iif = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::IIF, rates);
        auto iiia = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::IIIA, rates);
        REQUIRE(iif.has_value() == iiia.has_value());
        if (iif.has_value()) {
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) CHECK(iif->r[i][j] == iiia->r[i][j]);
        } else {
            CHECK(iif.error().id == iiia.error().id);
        }
    }
}

TEST_CASE("TYAW-A-012  mu genuinely increases with TRUE time along a REAL propagated "
          "trajectory, and a rate-limited turn's own midpoint falls AFTER the singularity "
          "in true elapsed time -- the independent-property guard against the mu-direction "
          "bug PROVENANCE.md Sec.30.14 records (both checks FAIL on the code as it stood "
          "before that fix, confirmed when the fix was made, not merely asserted after)",
          "[attitude][gate]") {
    // mu_rad itself is private (anonymous namespace); these are the two
    // OBSERVABLE consequences the manager's own review specified, tested
    // through the public interface alone, against a REAL propagated
    // trajectory (two-body RK4, self-contained here -- perturbations are
    // utterly negligible over the tens of minutes this needs).
    constexpr double kGM = 3.986004418e14;  // TYAW-P-5's own value, m^3/s^2 (SPEC-thrust-yaw §6)

    auto two_body_accel = [&](const Vec3& r) {
        const double r3 = std::pow(r.norm(), 3);
        return (-kGM / r3) * r;
    };
    auto rk4_step = [&](Vec3& r, Vec3& v, double dt) {
        auto k1v = v; auto k1a = two_body_accel(r);
        auto k2v = v + (dt / 2.0) * k1a; auto k2a = two_body_accel(r + (dt / 2.0) * k1v);
        auto k3v = v + (dt / 2.0) * k2a; auto k3a = two_body_accel(r + (dt / 2.0) * k2v);
        auto k4v = v + dt * k3a; auto k4a = two_body_accel(r + dt * k3v);
        r = r + (dt / 6.0) * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
        v = v + (dt / 6.0) * (k1a + 2.0 * k2a + 2.0 * k3a + k4a);
    };

    // A circular-ish GPS state with the Sun placed to give beta = 1 deg and
    // put true noon inside the propagated span -- the same geometry family
    // TYAW-A-002 already uses for IIR, so this is a realistic case, not a
    // contrived one.
    const double beta_deg = 1.0;
    auto f0 = fixture_at(beta_deg, 178.0);  // 2 deg of mu before noon, at t=0
    Vec3 r = f0.r_gcrs_m;
    Vec3 v = normalized(f0.v_gcrs_m_per_s) * std::sqrt(kGM / f0.r_gcrs_m.norm());  // circular speed
    const Vec3 sun_gcrs = f0.sun_gcrs;  // fixed direction over this short a span

    // Test 1: mu increases with true time between two states a few seconds
    // apart -- read through psi_nominal's own OBSERVABLE consequence at a
    // beta far from either turn (nominal_yaw_steering, no turn law involved,
    // so this isolates the geometry alone): recover psi at t and t+dt via
    // the SAME fixture-relative (n_hat,t_hat) this file's own recover_psi
    // uses, at a KNOWN, small mu that is NOT actually changing (n_hat,t_hat
    // recomputed fresh each step from the propagated r,v themselves, not
    // from a fixed mu) -- the observable is simpler: psi recovered THIS WAY
    // is, for the nominal law with x_sign=+1, monotonically related to mu
    // near mu=0 (psi_nominal(beta,mu,1) = ATAN2(-tan beta,-sin mu), strictly
    // decreasing in mu for small beta>0 as mu crosses 0) -- so confirming
    // psi's own value at t+dt is LESS than at t (for this small a step, mu
    // near 0, beta=1deg fixed) is confirming mu itself increased.
    {
        Vec3 r2 = r, v2 = v;
        // propagate to a state where mu is near 0 (midnight-ish geometry is
        // not needed; use noon, mu near 180, where the SAME local relation
        // holds by the identical algebra with mu_center=180) -- simpler:
        // step forward a few seconds from the ALREADY-near-noon f0 state.
        const double dt_s = 5.0;
        rk4_step(r2, v2, dt_s);
        auto psi_recover = [&](const Vec3& r_now, const Vec3& v_now) {
            const Vec3 r_hat = normalized(r_now);
            const Vec3 n_hat = normalized(r_now.cross(v_now));
            const Vec3 t_hat = n_hat.cross(r_hat);
            auto nom = gps_yaw_attitude(r_now, v_now, sun_gcrs, GpsBlock::IIF,
                                         HardwareYawRates{0.11, 0.0, 0.0, 0.0});
            REQUIRE(nom.has_value());
            const Vec3 xb{nom->r[0][0], nom->r[0][1], nom->r[0][2]};
            return std::atan2(-xb.dot(n_hat), -xb.dot(t_hat));
        };
        const double psi_t0 = psi_recover(r, v);
        const double psi_t1 = psi_recover(r2, v2);
        // near noon (mu approaching 180 from below, x_sign=1, beta>0): psi_n
        // = ATAN2(-tan beta, -sin mu) increases as mu increases toward 180
        // (sin mu decreasing toward 0 as mu->180 from below, for mu in
        // (90,180)) -- so psi increasing over this step is mu increasing.
        INFO("psi_t0(deg)=" << psi_t0 / kDeg << " psi_t1(deg)=" << psi_t1 / kDeg);
        CHECK(psi_t1 > psi_t0);
    }

    // Test 2: a rate-limited turn's own midpoint falls AFTER the geometric
    // singularity in TRUE ELAPSED TIME -- propagated across a real IIR noon
    // turn (KOUBA09's own LAG law, confirmed §4.2), no mu involved at all:
    // find the true-time moment r_hat is closest to -sun_hat (true noon),
    // and the true-time midpoint of gps_yaw_attitude's own active window,
    // and check the turn's own midpoint comes AFTER true noon, not before.
    {
        const HardwareYawRates iir_rates{0.20, 0.20, 0.0, 0.0};
        Vec3 rp = r, vp = v;
        double t_s = 0.0;
        double t_true_noon = -1.0, t_active_start = -1.0, t_active_end = -1.0;
        double best_alignment = -2.0;
        constexpr double kStepS = 2.0;
        for (int i = 0; i < 900; ++i) {  // 30 minutes, comfortably past IIR's own <=15 min
            const double align = normalized(rp).dot(normalized(sun_gcrs));  // NOON: r_hat aligns WITH the Sun
            if (align > best_alignment) { best_alignment = align; t_true_noon = t_s; }
            auto turn = gps_yaw_attitude(rp, vp, sun_gcrs, GpsBlock::IIR_IIRM, iir_rates);
            auto nom = nominal_yaw_steering(rp, sun_gcrs);
            REQUIRE(turn.has_value());
            REQUIRE(nom.has_value());
            const bool active = max_component_diff(*turn, *nom) > 1.0e-6;
            if (active && t_active_start < 0.0) t_active_start = t_s;
            if (active) t_active_end = t_s;
            rk4_step(rp, vp, kStepS);
            t_s += kStepS;
        }
        REQUIRE(t_active_start >= 0.0);  // sanity: a turn was actually found
        const double t_mid = (t_active_start + t_active_end) / 2.0;
        INFO("t_true_noon=" << t_true_noon << " t_active=[" << t_active_start << "," << t_active_end
                             << "] t_mid=" << t_mid);
        CHECK(t_mid > t_true_noon);  // the LAG law's own midpoint is AFTER true noon
    }
}

TEST_CASE("TYAW-A-013  KOUBA09's Eq.4 transcribed independently matches psi_nominal's own "
          "output through psi_tree = pi - psi_KOUBA09 -- the guard against the wrong-reason-"
          "comment bug PROVENANCE.md Sec.30.16 corrects (an earlier version of this section's "
          "own comments attributed psi_nominal's sign flip to compensating mu_rad, when the "
          "real cause is this tree's own yaw convention). x_sign=-1 (Eq.5, IIR) deliberately "
          "NOT exercised here -- TYAW-Q-007 (SPEC-thrust-yaw Sec.10) records why",
          "[attitude][gate]") {
    // KOUBA09's Eq.4 (x_sign=+1) exactly as printed -- written fresh here,
    // NOT calling psi_nominal (which is unreachable from this file
    // regardless: both live in attitude.cpp's own anonymous namespace).
    // This is the independent transcription the manager asked for, not a
    // restatement of production code under a different name.
    auto kouba_eq4_psi = [](double beta_rad, double mu_rad_) {
        return std::atan2(-std::tan(beta_rad), std::sin(mu_rad_));
    };
    auto wrapped_diff = [](double a, double b) { return std::atan2(std::sin(a - b), std::cos(a - b)); };

    // Sample points well away from any turn or shadow window at every beta
    // used (largest half-width in this grid is well under 45 deg), so
    // gps_yaw_attitude falls through to nominal_yaw_steering -- the SAME
    // off-turn fallthrough PROVENANCE.md Sec.30.8's own "confirmation by
    // shared formula" already relies on. x_sign=+1 covers II/IIA, IIF and
    // IIIA (TYAW-R-004's own bit-identical-to-IIF definition); IIR's own
    // x_sign=-1 is deliberately left untested here (TYAW-Q-007).
    const HardwareYawRates ii_iia_rates{0.11, 0.10, 0.5, 0.0017};

    double max_err_rad = 0.0;
    for (double beta_deg : {0.5, 2.0, 5.0, 15.0}) {
        for (double mu_deg : {45.0, 90.0, 135.0, 225.0, 270.0, 315.0}) {
            auto f = fixture_at(beta_deg, mu_deg);
            auto ii_iia = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs,
                                            GpsBlock::II_IIA, ii_iia_rates);
            REQUIRE(ii_iia.has_value());
            const double psi_tree =
                recover_psi(Vec3{ii_iia->r[0][0], ii_iia->r[0][1], ii_iia->r[0][2]}, mu_deg);
            const double psi_k = kouba_eq4_psi(beta_deg * kDeg, mu_deg * kDeg);
            const double err = std::abs(wrapped_diff(psi_tree, std::numbers::pi - psi_k));
            max_err_rad = std::max(max_err_rad, err);
        }
    }
    INFO("max wrapped |psi_tree - (pi - psi_KOUBA09_Eq4)| over the grid (rad) = " << max_err_rad);
    CHECK(max_err_rad < 1.0e-9);
}

TEST_CASE("TYAW-A-006  TYAW-F-001 fires at the Sun-on-zenith singularity outside any turn "
          "window, forwarded with ATTD-F-001's own id unchanged",
          "[attitude][gate]") {
    // Exactly at the noon point (mu = 180 deg, beta = 0 exactly): the Sun
    // must be on ZENITH (away from Earth, +r_hat), not nadir -- Sun-on-nadir
    // is the MIDNIGHT geometry (mu = 0), where II/IIA's own shadow-crossing
    // is always active at beta = 0 (its own width formula does not vanish
    // there the way the noon/midnight-turn one does). At the noon point,
    // every block's own noon-turn half-width formula DOES vanish at
    // beta = 0 (shown in PROVENANCE), so gps_yaw_attitude falls through to
    // nominal_yaw_steering here, which refuses at this exact geometry.
    const Vec3 r_gcrs_m{-2.656e7, 0.0, 0.0};
    const Vec3 v_gcrs_m_per_s{0.0, -3000.0, 0.0};
    const Vec3 zenith_direction{-1.0, 0.0, 0.0};
    const HardwareYawRates rates{0.122, 0.122, 0.5, 0.0017};

    for (auto block : {GpsBlock::II_IIA, GpsBlock::IIR_IIRM, GpsBlock::IIF, GpsBlock::IIIA}) {
        INFO(static_cast<int>(block));
        auto refused = gps_yaw_attitude(r_gcrs_m, v_gcrs_m_per_s, zenith_direction, block, rates);
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.error().id == "ATTD-F-001");
    }
}
