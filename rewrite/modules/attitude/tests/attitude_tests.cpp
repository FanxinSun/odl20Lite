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
/// cos(mu)*e0 - sin(mu)*e1, so that KOUBA09's own mu (measured from
/// midnight, positive prograde) comes out exactly as requested -- derived
/// and verified (PROVENANCE) against `gps_yaw_attitude`'s own beta/mu
/// computation, not merely asserted.
OrbitFixture fixture_at(double beta_deg, double mu_deg) {
    const double beta = beta_deg * kDeg;
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 r_hat = std::cos(mu) * e0 - std::sin(mu) * e1;
    const Vec3 t_hat = std::cos(mu) * e1 + std::sin(mu) * e0;
    const Vec3 s_hat = (-std::cos(beta)) * e0 + std::sin(beta) * n_hat;
    return {kGpsRadiusM * r_hat, 3000.0 * t_hat, s_hat};
}

/// Recovers the yaw angle psi from a returned body-x axis at a KNOWN mu,
/// inverting `frame_from_yaw`'s own x_body = -cos(psi)*t_hat - sin(psi)*n_hat
/// (attitude.cpp), using the SAME (n_hat, t_hat) `fixture_at` itself builds.
double recover_psi(const Vec3& x_body, double mu_deg) {
    const double mu = mu_deg * kDeg;
    const Vec3 n_hat{0.0, 0.0, 1.0};
    const Vec3 e0{1.0, 0.0, 0.0};
    const Vec3 e1 = n_hat.cross(e0);
    const Vec3 t_hat = std::cos(mu) * e1 + std::sin(mu) * e0;
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

TEST_CASE("TYAW-A-001  the beta0 = atan(mu_dot/R) derived relation reproduces KOUBA09's/DIL10's "
          "own printed turn-onset thresholds for all four stated hardware rates",
          "[attitude][gate]") {
    constexpr double kMuDot = 0.00836;  // KOUBA09 Eq. 6, deg/s
    struct Case { const char* label; double rate_deg_s; double printed_deg; double tol_deg; };
    const Case cases[] = {
        {"II/IIA fast end (KOUBA09 Table 1 max, PRN 27)", 0.134, 3.6, 0.05},
        {"II/IIA slow end (KOUBA09 Table 1 min, PRN 10)", 0.098, 4.9, 0.05},
        {"IIR (KOUBA09's single stated hardware rate)", 0.20, 2.4, 0.05},
        {"IIF night (DIL10 ~0.06 deg/s, cross-checked against eclips.f)", 0.06, 8.0, 0.10},
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

TEST_CASE("TYAW-A-004  IIF's own two-rate law: the noon turn is measurably shorter than "
          "the midnight/shadow turn, at the same beta",
          "[attitude][gate]") {
    const HardwareYawRates rates{0.11, 0.06, -0.7, 0.0};
    const double beta_deg = 1.0;

    auto extent_deg = [&](double mu_center_deg) {
        for (double d = 0.05; d < 20.0; d += 0.05) {
            auto f = fixture_at(beta_deg, mu_center_deg + d);
            auto turn = gps_yaw_attitude(f.r_gcrs_m, f.v_gcrs_m_per_s, f.sun_gcrs, GpsBlock::IIF, rates);
            auto nom = nominal_yaw_steering(f.r_gcrs_m, f.sun_gcrs);
            REQUIRE(turn.has_value());
            REQUIRE(nom.has_value());
            if (max_component_diff(*turn, *nom) < 1.0e-6) return d;  // caught up
        }
        return 20.0;  // not reached in practice; see the ratio check below
    };

    const double noon_extent = extent_deg(180.0);
    const double night_extent = extent_deg(0.0);
    INFO("noon extent(deg)=" << noon_extent << " night extent(deg)=" << night_extent);
    REQUIRE(noon_extent < 20.0);
    REQUIRE(night_extent < 20.0);
    CHECK(noon_extent < night_extent);
    // Order-of-magnitude consistent with the ~1.8x rate ratio (0.11/0.06),
    // not asserted exact: the durations depend on the whole ramp-vs-nominal
    // shape, not simply the rate ratio.
    CHECK(night_extent > 1.2 * noon_extent);
}

TEST_CASE("TYAW-A-005  IIIA is bit-identical to IIF at the same inputs -- the stopgap is "
          "exactly what it claims to be, not a fifth implementation",
          "[attitude][gate]") {
    const HardwareYawRates rates{0.11, 0.06, -0.7, 0.0};
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
