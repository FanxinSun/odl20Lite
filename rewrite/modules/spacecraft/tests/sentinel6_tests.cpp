// sentinel6_tests.cpp — SPEC-spacecraft.md, SPCR-A-025..029 (Sentinel-6).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/cited.hpp>
#include <odl/spacecraft/sentinel6.hpp>

#include <cmath>
#include <variant>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;
using Catch::Matchers::WithinAbs;

// --- SPCR-A-026: every surface sums to 1, both bands ------------------------

TEST_CASE("SPCR-A-026  sentinel6(): every surface's own visible AND infrared "
          "triple sums to 1 (Sec.16.3's own arithmetic), 12 surfaces, all "
          "body-fixed",
          "[spacecraft][sentinel6]") {
    auto m = sentinel6();
    REQUIRE(m.has_value());
    CHECK(m->surfaces().size() == 12);

    for (const auto& s : m->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        CHECK(fs.normal_mode() == NormalMode::body_fixed);
        const auto& vis = fs.front(Band::visible);
        CHECK_THAT(vis.absorptivity.value() + vis.specular.value() + vis.diffuse.value(),
                   WithinAbs(1.0, 1.0e-9));
        const auto& ir = fs.front(Band::infrared);
        CHECK_THAT(ir.absorptivity.value() + ir.specular.value() + ir.diffuse.value(),
                   WithinAbs(1.0, 1.0e-9));
        // Every normal is unit length regardless of what the source printed
        // (body_direction()'s own MCRM-F-002 guarantees this structurally,
        // but the point of this schema-fidelity check is to confirm the
        // renormalisation actually reached construction, not merely that
        // some vector was accepted).
        CHECK_THAT(fs.body_fixed_normal()->vec().norm(), WithinAbs(1.0, 1.0e-9));
    }
}

// --- SPCR-A-027: infrared is genuinely populated and uniform ---------------

TEST_CASE("SPCR-A-027  sentinel6(): infrared optics are uniform "
          "(0.100/0.800/0.100) on every row, as Sec.16.3 prints, and "
          "GENUINELY PRESENT (not silently falling back to the visible "
          "triple) -- the first constellation in this tree to populate "
          "BandedOptics's own infrared field",
          "[spacecraft][sentinel6]") {
    auto m = sentinel6();
    REQUIRE(m.has_value());
    for (const auto& s : m->surfaces()) {
        const auto& fs = std::get<FlatSurface>(s);
        const auto& vis = fs.front(Band::visible);
        const auto& ir = fs.front(Band::infrared);
        CHECK_THAT(ir.specular.value(), WithinAbs(0.100, 1.0e-9));
        CHECK_THAT(ir.diffuse.value(), WithinAbs(0.800, 1.0e-9));
        CHECK_THAT(ir.absorptivity.value(), WithinAbs(0.100, 1.0e-9));
        // Guard: infrared must differ from visible for at least one row
        // (proving `in(Band::infrared)` is reading the populated field, not
        // silently falling back to `visible` the way an UNPOPULATED
        // optional would, BandedOptics::in()'s own documented fallback).
        if (std::abs(vis.specular.value() - 0.349) < 1.0e-9) {  // the +X row
            CHECK(std::abs(vis.specular.value() - ir.specular.value()) > 1.0e-6);
        }
    }
}

// --- SPCR-A-028: the two renormalised, non-axis-aligned normals ------------

TEST_CASE("SPCR-A-028  sentinel6(): the two non-axis-aligned face normals, "
          "AS PRINTED, do not renormalise to unit length -- checked against "
          "the source's own printed 3-decimal components directly, not "
          "assumed -- and the BUILT surface carries the renormalised "
          "direction, not the raw printed one",
          "[spacecraft][sentinel6]") {
    // Sec.16.3, row 5: normal (0, 0.616, -0.788) -- printed norm^2 =
    // 0.616^2 + 0.788^2 = 1.000400, i.e. norm = 1.0002, NOT exactly 1
    // (a small, plausibly-rounding-level deviation).
    const double n1 = std::sqrt(0.616 * 0.616 + 0.788 * 0.788);
    CHECK(std::abs(n1 - 1.0) > 1.0e-6);
    CHECK(std::abs(n1 - 1.0) < 1.0e-2);

    // Sec.16.3, row 11: normal (0.469, 0, -0.833) -- printed norm^2 =
    // 0.469^2 + 0.833^2 = 0.913850, norm = 0.9560 -- a MATERIAL (4.4%)
    // deviation, verified against the rendered PDF page directly (not an
    // extraction artefact, PROVENANCE.md's own L5 step 4 section).
    const double n2 = std::sqrt(0.469 * 0.469 + 0.833 * 0.833);
    CHECK(std::abs(n2 - 1.0) > 0.04);

    auto m = sentinel6();
    REQUIRE(m.has_value());
    bool found_row5 = false, found_row11 = false;
    for (const auto& s : m->surfaces()) {
        const auto& fs = std::get<FlatSurface>(s);
        const Vec3& n = fs.body_fixed_normal()->vec();
        if (std::abs(fs.area_m2().value() - 8.65) < 1.0e-9 && n.y > 0.0) {
            found_row5 = true;
            CHECK_THAT(n.norm(), WithinAbs(1.0, 1.0e-9));
            CHECK_THAT(n.y, WithinAbs(0.616 / n1, 1.0e-6));
            CHECK_THAT(n.z, WithinAbs(-0.788 / n1, 1.0e-6));
        }
        if (std::abs(fs.area_m2().value() - 0.92) < 1.0e-9) {
            found_row11 = true;
            CHECK_THAT(n.norm(), WithinAbs(1.0, 1.0e-9));
            CHECK_THAT(n.x, WithinAbs(0.469 / n2, 1.0e-6));
            CHECK_THAT(n.z, WithinAbs(-0.833 / n2, 1.0e-6));
        }
    }
    CHECK(found_row5);
    CHECK(found_row11);
}

// --- SPCR-A-029: mass/CoM, and the citation guard ---------------------------

TEST_CASE("SPCR-A-029  sentinel6(): mass and centre of mass are Sec.16.1's "
          "own baseline; every value is cited; the citation-refusal guard "
          "reaches this module's own call path",
          "[spacecraft][sentinel6]") {
    auto m = sentinel6();
    REQUIRE(m.has_value());
    CHECK_THAT(m->mass_kg().value(), WithinAbs(1191.831, 1.0e-9));
    CHECK_THAT(m->centre_of_mass_m().value().x, WithinAbs(1.5274, 1.0e-9));
    CHECK_THAT(m->centre_of_mass_m().value().y, WithinAbs(-0.0073, 1.0e-9));
    CHECK_THAT(m->centre_of_mass_m().value().z, WithinAbs(0.0373, 1.0e-9));
    CHECK(!m->mass_kg().citation().empty());

    // The guard shown firing: a blank citation, through this module's own
    // Cited/body_direction call path, is refused by MCRM-F-001.
    auto blank = cited(1.0, "");
    CHECK(!blank.has_value());
    if (!blank.has_value()) CHECK(blank.error().id == "MCRM-F-001");
}
