// g5_comparison_tests.cpp — SPEC-measmod.md §8.9: MEAS-A-100 (G5, the registered comparison of the range model with one real pass, run ONCE against the committed envelope) and
// MEAS-A-100b (the comparison's own arithmetic, run freely).
//
// The rule (§8.9, "The comparison's rule"): the model's residual `observed − modelled` of every normal point of the chosen pass — no bias removed, no outlier rejected — passes iff
// |r_i − c_i| ≤ w_i, against the envelope committed in e40d83b and recomputed here by the same function. OUTSIDE IS A FINDING AND THE STEP DOES NOT CLOSE ON IT: the test fails and stays
// red until the manager rules. What is printed beyond the rule (the statistics of r − c; the number of points at which each assembly defect, applied to the actual residual, would have
// left the envelope) is descriptive and is not a criterion.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "g5_envelope.hpp"

#include <cstdio>
#include <sstream>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using namespace odl::measmod::testing::g5;
using Catch::Matchers::WithinAbs;

namespace {

/// The comparison's rule: a normal point passes iff its residual lies within `halfwidth` of the envelope's centre, the edges included.
bool inside(double residual_m, double centre_m, double halfwidth_m) { return std::abs(residual_m - centre_m) <= halfwidth_m; }

}  // namespace

TEST_CASE("MEAS-A-100b  the comparison's arithmetic: a residual on the edge of the window is inside, one a hair beyond it is not, the centre's sign is the sign of the residual, and a defect that "
          "enlarges the modelled range lowers the residual by its size",
          "[measmod][g5][comparisonlogic]") {
    CHECK(inside(0.3, 0.1, 0.2));                      // on the upper edge
    CHECK(inside(-0.1, 0.1, 0.2));                     // on the lower edge
    CHECK_FALSE(inside(0.3000001, 0.1, 0.2));
    CHECK_FALSE(inside(-0.1000001, 0.1, 0.2));
    // a station 0.2 m higher than modelled, with the satellite overhead, makes the true range 0.2 m shorter: observed minus modelled = -0.2 = the centre
    CHECK_THAT(range_effect_of_displacement(Vec3{0.0, 0.0, 1.0}, Vec3{0.0, 0.0, 0.2}), WithinAbs(-0.2, 1e-18));
    CHECK(inside(-0.2, -0.2, 0.0));
    // a defect that enlarges the modelled range by 0.4924 m (the centre-of-mass sign) moves a residual that sat on the centre to 0.4924 m below it: outside a window of half-width 0.27 m
    const double c = -0.3, w = 0.27, delta = 0.4924;
    CHECK(inside(c, c, w));
    CHECK_FALSE(inside(c - delta, c, w));
    // and the shifted residual stays inside only if the clean one lay more than (delta - w) above the centre: the window's top (2 w - delta) wide
    CHECK(inside(c + (delta - w) + 1e-9 - delta, c, w));
    CHECK_FALSE(inside(c + (delta - w) - 1e-9 - delta, c, w));
}

TEST_CASE("MEAS-A-100  G5, the registered check, run once: the model's residuals (observed - modelled, no bias removed, no outlier rejected) over every normal point of the chosen real pass "
          "lie within the committed envelope; outside is a finding and the step does not close on it; the result is printed with the envelope beside it",
          "[measmod][g5][comparison][pregistered]") {
    const PassChoice& pc = chosen();
    REQUIRE(pc.observations.size() == static_cast<std::size_t>(kChosenNormalPoints));
    const Envelope env = build_envelope();                          // the committed envelope, by the same function
    const std::vector<DefectRow> defects = assembly_defects();
    REQUIRE(env.points.size() == pc.observations.size());
    const std::size_t n = pc.observations.size();

    std::vector<double> residual(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const RangeObservation& obs = pc.observations[i];
        const EarthFixedStation station(obs.site.srp_itrs_m, obs.site.srp_geodetic, orientation());
        auto m = model_range(obs, station, target());
        REQUIRE(m.has_value());
        residual[i] = m->residual_m();                                // observed - modelled, the one-way-equivalent range, metres
    }

    {
        std::ostringstream o;
        char b[400];
        o << "G5 COMPARISON (MEAS-A-100), run once; the envelope of MEAS-A-101 (commit e40d83b) beside it. Millimetres; r = observed - modelled, no bias removed; rule: |r - c| <= w.\n";
        o << "   i  UTC s-of-day  elev deg        r        c        w    r - c   (r - c)/w  margin w-|r-c|  verdict\n";
        for (std::size_t i = 0; i < n; ++i) {
            const NpEnvelope& p = env.points[i];
            const double d = residual[i] - p.centre_m;
            std::snprintf(b, sizeof b, "  %2zu  %11.3f  %8.2f  %7.1f  %7.1f  %7.1f  %7.1f  %9.3f  %14.1f  %s\n", i, p.seconds_of_day, p.elevation_deg, residual[i] * 1e3, p.centre_m * 1e3, p.w_m * 1e3,
                          d * 1e3, d / p.w_m, (p.w_m - std::abs(d)) * 1e3, inside(residual[i], p.centre_m, p.w_m) ? "inside" : "OUTSIDE");
            o << b;
        }
        double mean = 0.0, sq = 0.0, lo = 1e9, hi = -1e9, ratio = 0.0, rmean = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double d = residual[i] - env.points[i].centre_m;
            mean += d;
            sq += d * d;
            lo = std::min(lo, d);
            hi = std::max(hi, d);
            ratio = std::max(ratio, std::abs(d) / env.points[i].w_m);
            rmean += residual[i];
        }
        mean /= static_cast<double>(n);
        rmean /= static_cast<double>(n);
        const double rms = std::sqrt(sq / static_cast<double>(n));
        double var = 0.0;
        for (std::size_t i = 0; i < n; ++i) var += (residual[i] - env.points[i].centre_m - mean) * (residual[i] - env.points[i].centre_m - mean);
        const double sd = std::sqrt(var / static_cast<double>(n - 1));
        std::snprintf(b, sizeof b, "  r - c: mean %.1f mm, standard deviation about the mean %.1f mm, rms %.1f mm, smallest %.1f, largest %.1f; largest |r - c|/w = %.3f; mean r = %.1f mm\n", mean * 1e3, sd * 1e3,
                      rms * 1e3, lo * 1e3, hi * 1e3, ratio, rmean * 1e3);
        o << b;
        o << "  PREDICTED before the run (SPEC-measmod §8.9; predictions, not criteria): |r - c| <= 80 mm everywhere (largest ratio <= 0.30), mean of r - c within +-50 mm, neighbouring points within 30 mm, "
             "r between -450 and -120 mm\n";
        WARN(o.str());
    }
    {
        std::ostringstream o;
        char b[500];
        o << "G5's POWER ON THIS PASS'S ACTUAL RESIDUALS (descriptive): each assembly defect of the envelope's table applied to the residual (the defective modelled range is larger by the defect's size, so the "
             "residual is lower by it), and the number of the 18 points at which that residual would have left the envelope:\n";
        for (const DefectRow& d : defects) {
            int out = 0;
            for (std::size_t i = 0; i < n; ++i) {
                if (!d.delta_m[i] || !inside(residual[i] - *d.delta_m[i], env.points[i].centre_m, env.points[i].w_m)) ++out;
            }
            std::snprintf(b, sizeof b, "  %-46s would be OUTSIDE at %2d of %zu points\n", d.name.c_str(), out, n);
            o << b;
        }
        WARN(o.str());
    }

    // the rule
    for (std::size_t i = 0; i < n; ++i) {
        INFO("normal point " << i << ": r = " << residual[i] * 1e3 << " mm, centre " << env.points[i].centre_m * 1e3 << " mm, half-width " << env.points[i].w_m * 1e3 << " mm");
        CHECK(inside(residual[i], env.points[i].centre_m, env.points[i].w_m));
    }
}
