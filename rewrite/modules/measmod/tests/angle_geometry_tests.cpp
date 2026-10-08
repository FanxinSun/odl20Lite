// angle_geometry_tests.cpp — SPEC-measmod.md §6.2 (v) and §8.7: MEAS-A-096g, the angle gate's geometry rule. No model is run: the line of sight of each of the four gates is selected from the
// geometry alone (angle_scenario.hpp), and the selection, the class and the visibility score are what the specification states. This file is committed WITH the frozen sizing, before the
// code of the gate itself exists.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "angle_scenario.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using odl::Vec3;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kRho = 1.2e6;                       // m: the slant range of the sizing
constexpr double kSpeed = 7500.0;                    // m/s
double ulp_of(double x) { return std::nextafter(std::abs(x), std::numeric_limits<double>::infinity()) - std::abs(x); }
const char* direction_name(double radial) { return radial == 0.0 ? "across" : "along"; }

/// The geometry the specification states for each gate (SPEC-measmod §6.2 (v)).
struct GeometryFrozen {
    const char* name;
    AngleGate gate;
    double expected_declination_deg, expected_score;
};
const GeometryFrozen kGeometric = {"Geometric", AngleGate::Geometric, 31.01, 0.6548};
const GeometryFrozen kAstrometric = {"Astrometric", AngleGate::Astrometric, 31.01, 0.6548};
const GeometryFrozen kRefracted = {"ApparentRefracted", AngleGate::ApparentRefracted, 30.0, 0.4853};
const GeometryFrozen kOfDate = {"Astrometric, true equator and equinox of date", AngleGate::AstrometricOfDate, 30.95, 0.6549};

}  // namespace

// ---- MEAS-A-096g: the geometry rule (no model is run) -----------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-096g  the angle gate's geometry rule selects the line of sight of each of the four gates from the geometry alone (no model is run): azimuth 0 degrees at elevation 30, "
          "in the class (declination or elevation at most 39.5 degrees) with the visibility score the specification states; the target in the sizing's binade of the ulp; the first normal "
          "point's weather and its refraction constants",
          "[measmod][anglegeom]") {
    for (const GeometryFrozen* fz : {&kGeometric, &kAstrometric, &kRefracted, &kOfDate}) {
        for (double radial : {0.0, 1.0}) {
            const AngleScenario sc(fz->gate, 30.0, kRho, kSpeed, radial);
            INFO(fz->name << ", " << direction_name(radial));
            CHECK(sc.azimuth_deg() == 0);
            CHECK(std::abs(sc.declination_deg()) <= AngleScenario::kMaxDeclination_deg);
            CHECK(sc.score() >= AngleScenario::kMinScore);
            CHECK_THAT(sc.declination_deg(), WithinAbs(fz->expected_declination_deg, 0.01));
            CHECK_THAT(sc.score(), WithinAbs(fz->expected_score, 5e-4));
            CHECK_THAT(sc.target_position().norm(), WithinRel(7.053e6, 1e-3));
            const Vec3 r = sc.target_position();
            CHECK(ulp_of(std::max({std::abs(r.x), std::abs(r.y), std::abs(r.z)})) <= ulp_of(7.2e6));           // the sizing's operand ulp bounds this target's
            CHECK_THAT(sc.slant_range_at_emission(), WithinRel(radial == 0.0 ? 1.2e6 : 1.19997e6, 1e-4));
        }
    }
    const AngleScenario sc(AngleGate::ApparentRefracted, 30.0, kRho, kSpeed, 0.0);
    const AngleAtmosphere atm = sc.atmosphere();
    CHECK_THAT(atm.atmosphere.pressure_hpa, WithinAbs(976.70, 1e-9));
    CHECK_THAT(atm.atmosphere.temperature_c, WithinAbs(37.15, 1e-9));
    CHECK_THAT(atm.atmosphere.relative_humidity, WithinAbs(0.12, 1e-12));
    CHECK_THAT(atm.wavelength_um, WithinAbs(0.532, 1e-12));
    auto c = refraction_constants(atm);
    REQUIRE(c.has_value());
    CHECK_THAT(c->a, WithinRel(2.484383e-4, 1e-6));                 // the 60-digit evaluation of tools/measmod_fd_sizing.cpp
    CHECK_THAT(c->b, WithinRel(-3.123796e-7, 1e-6));
}
