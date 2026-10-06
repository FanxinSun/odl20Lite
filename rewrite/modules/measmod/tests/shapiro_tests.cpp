// shapiro_tests.cpp — SPEC-measmod.md §8.3, MEAS-A-025 and MEAS-A-026 (G4).
//
// G4: the Shapiro term against an independent evaluation, LABELLED NOT PUBLISHED: TN36 §11.2 prints the formula (11.17) and no number.
// The expected values are tools/measmod_reference.py's: 60-digit arithmetic at the very doubles this test passes, and the partials
// by the closed form AND by a 60-digit central difference that had to agree before either was emitted.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/measmod/shapiro.hpp>

#include "measmod_reference.hpp"

#include <cmath>

using namespace odl::measmod;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("MEAS-A-025  G4, the Shapiro term (2GM/c^2) ln[(r1+r2+rho)/(r1+r2-rho)] at three geometries equals an independent 60-digit "
          "evaluation; labelled not published",
          "[measmod][shapiro]") {
    CHECK_THAT(2.0 * kGmEarthTt_m3_s2 / (kSpeedOfLight_m_s * kSpeedOfLight_m_s), WithinRel(ref::shapiro_two_gm_over_c2_m, 1e-15));
    struct Geom { double r1, r2, rho, delay; const char* name; };
    const Geom geoms[] = {
        {ref::shapiro_zenith_r1_m, ref::shapiro_zenith_r2_m, ref::shapiro_zenith_rho_m, ref::shapiro_zenith_delay_m, "LAGEOS-like, zenith"},
        {ref::shapiro_el10_r1_m, ref::shapiro_el10_r2_m, ref::shapiro_el10_rho_m, ref::shapiro_el10_delay_m, "LAGEOS-like, 10 degrees"},
        {ref::shapiro_leo30_r1_m, ref::shapiro_leo30_r2_m, ref::shapiro_leo30_rho_m, ref::shapiro_leo30_delay_m, "LEO-like, 30 degrees"},
    };
    for (const Geom& g : geoms) {
        INFO(g.name);
        auto s = shapiro_leg(g.r1, g.r2, g.rho);
        REQUIRE(s.has_value());
        CHECK_THAT(s->delay_m, WithinAbs(g.delay, 1e-12));
    }
    // the magnitudes the specification quotes (MEAS-P-6): 5.80 mm at zenith and 9.88 mm at 10 degrees for a LAGEOS-like geometry; 2.25 mm for the LEO case
    CHECK_THAT(shapiro_leg(ref::shapiro_zenith_r1_m, ref::shapiro_zenith_r2_m, ref::shapiro_zenith_rho_m)->delay_m, WithinAbs(5.80e-3, 0.005e-3));
    CHECK_THAT(shapiro_leg(ref::shapiro_el10_r1_m, ref::shapiro_el10_r2_m, ref::shapiro_el10_rho_m)->delay_m, WithinAbs(9.88e-3, 0.005e-3));
    CHECK_THAT(shapiro_leg(ref::shapiro_leo30_r1_m, ref::shapiro_leo30_r2_m, ref::shapiro_leo30_rho_m)->delay_m, WithinAbs(2.25e-3, 0.005e-3));
}

TEST_CASE("MEAS-A-026  the Shapiro term's partials equal the independent high-precision derivatives; the term is symmetric in r1 and r2, "
          "positive, tends to 0 with rho, and a leg through the Earth refuses",
          "[measmod][shapiro]") {
    struct Geom { double r1, r2, rho, d_rho, d_r; };
    const Geom geoms[] = {
        {ref::shapiro_zenith_r1_m, ref::shapiro_zenith_r2_m, ref::shapiro_zenith_rho_m, ref::shapiro_zenith_d_rho, ref::shapiro_zenith_d_r},
        {ref::shapiro_el10_r1_m, ref::shapiro_el10_r2_m, ref::shapiro_el10_rho_m, ref::shapiro_el10_d_rho, ref::shapiro_el10_d_r},
        {ref::shapiro_leo30_r1_m, ref::shapiro_leo30_r2_m, ref::shapiro_leo30_rho_m, ref::shapiro_leo30_d_rho, ref::shapiro_leo30_d_r},
    };
    for (const Geom& g : geoms) {
        auto s = shapiro_leg(g.r1, g.r2, g.rho);
        REQUIRE(s.has_value());
        CHECK_THAT(s->d_rho, WithinRel(g.d_rho, 1e-10));
        CHECK_THAT(s->d_r1, WithinRel(g.d_r, 1e-10));
        CHECK_THAT(s->d_r2, WithinRel(g.d_r, 1e-10));
        CHECK(s->d_rho > 0.0);                                            // a longer leg is delayed more
        CHECK(s->d_r1 < 0.0);                                             // and a higher end delays it less: the leg is less deep in the well
        // symmetric in r1 <-> r2: the delay is exactly the same
        auto swapped = shapiro_leg(g.r2, g.r1, g.rho);
        REQUIRE(swapped.has_value());
        CHECK(swapped->delay_m == s->delay_m);
        CHECK(s->delay_m > 0.0);
    }
    // tends to 0 with rho: for r1 = r2 = r and a short leg the delay is (2GM/c^2) 2 rho / (2 r) to the leading order
    const double k = 2.0 * kGmEarthTt_m3_s2 / (kSpeedOfLight_m_s * kSpeedOfLight_m_s), r = 7.0e6;
    for (double rho : {1.0e3, 1.0, 1.0e-3}) {
        auto s = shapiro_leg(r, r, rho);
        REQUIRE(s.has_value());
        CHECK_THAT(s->delay_m, WithinRel(k * rho / r, 1e-6));
    }
    CHECK(shapiro_leg(r, r, 0.0)->delay_m == 0.0);
    // a leg whose geometry has no finite value refuses (a path through the Earth's centre, a non-positive radius, a negative separation)
    for (auto bad : {shapiro_leg(7.0e6, 7.0e6, 1.4e7), shapiro_leg(7.0e6, 7.0e6, 2.0e7), shapiro_leg(0.0, 7.0e6, 1.0e6), shapiro_leg(-1.0, 7.0e6, 1.0e6),
                     shapiro_leg(7.0e6, 7.0e6, -1.0), shapiro_leg(std::nan(""), 7.0e6, 1.0e6)}) {
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().id == "MEAS-F-010");
    }
    // the adjacent legs that do have a value (just under r1 + r2) give a large but finite delay
    CHECK(shapiro_leg(7.0e6, 7.0e6, 1.39999e7).has_value());
}
