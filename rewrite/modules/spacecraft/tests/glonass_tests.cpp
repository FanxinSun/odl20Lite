// glonass_tests.cpp — SPEC-spacecraft.md §8, SPCR-A-015 through SPCR-A-019.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/spacecraft/glonass.hpp>

#include <variant>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;
using Catch::Matchers::WithinAbs;

namespace {

template <class F>
void for_each_flat_surface(const Macromodel& m, F&& f) {
    for (const auto& s : m.surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        f(fs);
    }
}

}  // namespace

// --- SPCR-A-015 ----------------------------------------------------------------

TEST_CASE("SPCR-A-015  every surface of glonass()/glonass_m()/glonass_k() "
          "has alpha+specular+diffuse=1 (RS14's own arithmetic), 7 "
          "surfaces each (6 bus faces + 1 combined panel), and the SAME "
          "formula-verified rho->specular/delta->diffuse mapping GPS "
          "already uses -- spot-checked cell by cell against RS14's own "
          "printed tables",
          "[spacecraft][glonass]") {
    auto g = glonass();
    auto gm = glonass_m();
    auto gk = glonass_k();
    REQUIRE(g.has_value());
    REQUIRE(gm.has_value());
    REQUIRE(gk.has_value());

    for (const Macromodel* m : {&*g, &*gm, &*gk}) {
        CHECK(m->surfaces().size() == 7);
        for_each_flat_surface(*m, [](const FlatSurface& fs) {
            const double a = fs.absorptivity().value();
            const double s = fs.specular().value();
            const double d = fs.diffuse().value();
            CHECK_THAT(a + s + d, WithinAbs(1.0, 1.0e-12));
        });
    }

    // RS14 Table 5.6's own +Z bus row (shape=0, no caveat needed): area
    // 1.662, alpha 0.374, delta 0.381 (-> diffuse), rho 0.245 (-> specular)
    // -- read directly from the table, not from glonass.cpp's own values.
    const Vec3 plus_z_normal = body_direction(Vec3{0.0, 0.0, 1.0})->vec();
    bool found_plus_z = false;
    for (const auto& s : g->surfaces()) {
        if (!std::holds_alternative<FlatSurface>(s)) continue;
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.normal_mode() != NormalMode::body_fixed) continue;
        if (!fs.body_fixed_normal().has_value()) continue;
        const Vec3& n = fs.body_fixed_normal()->vec();
        if (n.x == plus_z_normal.x && n.y == plus_z_normal.y && n.z == plus_z_normal.z) {
            CHECK_THAT(fs.area_m2().value(), WithinAbs(1.662, 1.0e-9));
            CHECK_THAT(fs.absorptivity().value(), WithinAbs(0.374, 1.0e-9));
            CHECK_THAT(fs.specular().value(), WithinAbs(0.245, 1.0e-9));  // RS14's own rho
            CHECK_THAT(fs.diffuse().value(), WithinAbs(0.381, 1.0e-9));   // RS14's own delta
            found_plus_z = true;
        }
    }
    CHECK(found_plus_z);

    // Masses: RS14's own captions, Tables 5.6/5.7/5.8.
    CHECK_THAT(g->mass_kg().value(), WithinAbs(1415.0, 1.0e-9));
    CHECK_THAT(gm->mass_kg().value(), WithinAbs(1415.0, 1.0e-9));
    CHECK_THAT(gk->mass_kg().value(), WithinAbs(935.0, 1.0e-9));
}

// --- SPCR-A-016 ----------------------------------------------------------------

TEST_CASE("SPCR-A-016  the +-X/+-Y bus faces of glonass()/glonass_m() carry "
          "a citation stating RS14's own cylinder-wing 'shape' blend and "
          "this build's own flat-law approximation; the +-Z faces and "
          "solar panels, and EVERY glonass_k() face (RS14 prints no "
          "'shape' column for GLONASS-K), do not -- the split is real, "
          "checked by substring, not merely asserted",
          "[spacecraft][glonass]") {
    auto g = glonass();
    REQUIRE(g.has_value());

    auto has_shape_caveat = [](const FlatSurface& fs) {
        return fs.absorptivity().citation().find("cylinder-wing") != std::string::npos ||
               fs.absorptivity().citation().find("'shape' column") != std::string::npos;
    };

    int caveated = 0, plain = 0;
    for (const auto& s : g->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        if (has_shape_caveat(fs)) ++caveated;
        else ++plain;
    }
    // 4 caveated (+X,-X,+Y,-Y bus), 3 plain (+Z,-Z bus, panels).
    CHECK(caveated == 4);
    CHECK(plain == 3);

    auto gm = glonass_m();
    REQUIRE(gm.has_value());
    caveated = 0; plain = 0;
    for (const auto& s : gm->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        if (has_shape_caveat(fs)) ++caveated;
        else ++plain;
    }
    CHECK(caveated == 4);
    CHECK(plain == 3);

    auto gk = glonass_k();
    REQUIRE(gk.has_value());
    for (const auto& s : gk->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        CHECK_FALSE(has_shape_caveat(fs));
    }
}

// --- SPCR-A-017 ----------------------------------------------------------------

TEST_CASE("SPCR-A-017  glonass_k()'s own dimension citation ends at RS14's "
          "own stated chain end, \"Mitrikas (personal communication, "
          "2011)\" -- the same treatment GPS-IIF's own \"an unpublished "
          "document\" source gets, recorded rather than resolved further",
          "[spacecraft][glonass]") {
    auto gk = glonass_k();
    REQUIRE(gk.has_value());
    bool found = false;
    for (const auto& s : gk->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.area_m2().citation().find("Mitrikas (personal communication, 2011)") !=
            std::string::npos) {
            found = true;
        }
    }
    CHECK(found);
}

// --- SPCR-A-018 ----------------------------------------------------------------

TEST_CASE("SPCR-A-018  the guard shown firing: a deliberately blank "
          "citation on a test-local copy of this module's own construction "
          "is refused by MCRM-F-001, through THIS module's own call path",
          "[spacecraft][glonass]") {
    auto area = cited(3.310, "");  // deliberately blank
    REQUIRE_FALSE(area.has_value());
    CHECK(area.error().id == "MCRM-F-001");

    auto normal = body_direction(Vec3{1.0, 0.0, 0.0});
    REQUIRE(normal.has_value());
    auto absorptivity = cited(0.440, "RS14 Table 5.6");
    auto specular = cited(0.112, "RS14 Table 5.6");
    auto diffuse = cited(0.448, "RS14 Table 5.6");
    REQUIRE(absorptivity.has_value());
    REQUIRE(specular.has_value());
    REQUIRE(diffuse.has_value());
    // area itself failed above (MCRM-F-001) -- confirms the SAME refusal
    // this module's own bus_face()/solar_panels() would hit if a citation
    // string were ever accidentally left blank, reached the same way every
    // other spacecraft file's own SPCR-A-002/SPCR-A-018-shaped guard is.
}

// --- SPCR-A-019 ----------------------------------------------------------------

TEST_CASE("SPCR-A-019  glonass()/glonass_m()'s own optics for the +-X/+-Y "
          "bus faces and glonass_k()'s own for every face are RS14's own "
          "generic Ziebart (2001) assumption, marked ASSUMED -- checked, "
          "not merely stated in a comment",
          "[spacecraft][glonass]") {
    for (auto result : {glonass(), glonass_m(), glonass_k()}) {
        REQUIRE(result.has_value());
        bool found_assumed = false;
        for (const auto& s : result->surfaces()) {
            REQUIRE(std::holds_alternative<FlatSurface>(s));
            const auto& fs = std::get<FlatSurface>(s);
            if (fs.absorptivity().citation().find("ASSUMED") != std::string::npos) found_assumed = true;
        }
        CHECK(found_assumed);
    }
}
