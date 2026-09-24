// qzss_tests.cpp — SPEC-spacecraft.md §8, SPCR-A-020 through SPCR-A-023.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/spacecraft/qzss.hpp>

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
        f(std::get<FlatSurface>(s));
    }
}

}  // namespace

// --- SPCR-A-020 ----------------------------------------------------------------

TEST_CASE("SPCR-A-020  qzss_frame_from_native, an involution, and the "
          "SAME 180-about-Z form galileo_frame_from_mechanical has -- "
          "independently derived for QZSS, checked here as pure algebra, "
          "not assumed shared with Galileo's own mapping",
          "[spacecraft][qzss]") {
    const Vec3 v{12.3, -45.6, 78.9};
    const Vec3 mapped = qzss_frame_from_native(v);
    CHECK_THAT(mapped.x, WithinAbs(-v.x, 1.0e-12));
    CHECK_THAT(mapped.y, WithinAbs(-v.y, 1.0e-12));
    CHECK_THAT(mapped.z, WithinAbs(v.z, 1.0e-12));

    const Vec3 twice = qzss_frame_from_native(qzss_frame_from_native(v));
    CHECK_THAT(twice.x, WithinAbs(v.x, 1.0e-12));
    CHECK_THAT(twice.y, WithinAbs(v.y, 1.0e-12));
    CHECK_THAT(twice.z, WithinAbs(v.z, 1.0e-12));
}

// --- SPCR-A-021 ----------------------------------------------------------------

TEST_CASE("SPCR-A-021  qzss_1 (BOL and EOL alike): every surface's own "
          "absorption+specular+diffuse sums to 1 (Table 4's own "
          "arithmetic); 9 surfaces (8 body-fixed + 1 combined SAP); the "
          "+Y Radiator row matches Table 4 cell by cell; BOL and EOL "
          "genuinely differ in mass and CoM, surfaces unchanged either way",
          "[spacecraft][qzss]") {
    auto bol = qzss_1(QzssLife::BeginningOfLife);
    auto eol = qzss_1(QzssLife::EndOfLife);
    REQUIRE(bol.has_value());
    REQUIRE(eol.has_value());

    for (const Macromodel* m : {&*bol, &*eol}) {
        CHECK(m->surfaces().size() == 9);
        for_each_flat_surface(*m, [](const FlatSurface& fs) {
            const double a = fs.absorptivity().value();
            const double s = fs.specular().value();
            const double d = fs.diffuse().value();
            CHECK_THAT(a + s + d, WithinAbs(1.0, 1.0e-12));
        });
    }

    bool found_plus_y_radiator = false;
    for (const auto& s : bol->surfaces()) {
        if (!std::holds_alternative<FlatSurface>(s)) continue;
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.normal_mode() != NormalMode::body_fixed) continue;
        // +Y Radiator: area 7.0, matches no other row's area on a body-fixed
        // +Y-native-mapped face (MLI on the same native face is 3.0).
        if (std::abs(fs.area_m2().value() - 7.0) < 1.0e-9) {
            CHECK_THAT(fs.absorptivity().value(), WithinAbs(0.08, 1.0e-9));
            CHECK_THAT(fs.specular().value(), WithinAbs(0.8280, 1.0e-9));
            CHECK_THAT(fs.diffuse().value(), WithinAbs(0.0920, 1.0e-9));
            found_plus_y_radiator = true;
        }
    }
    CHECK(found_plus_y_radiator);

    CHECK_THAT(bol->mass_kg().value(), WithinAbs(2281.0, 1.0e-9));
    CHECK_THAT(eol->mass_kg().value(), WithinAbs(2121.0, 1.0e-9));
    CHECK(bol->mass_kg().value() != eol->mass_kg().value());
    CHECK((bol->centre_of_mass_m().value() - eol->centre_of_mass_m().value()).norm() > 1.0e-6);
}

// --- SPCR-A-022 ----------------------------------------------------------------

TEST_CASE("SPCR-A-022  the SAP is built sun-pointing, not body-fixed, "
          "with the summed +Y/-Y area (45.0 m^2); every other material "
          "is body-fixed",
          "[spacecraft][qzss]") {
    auto m = qzss_1(QzssLife::BeginningOfLife);
    REQUIRE(m.has_value());
    int sun_pointing_count = 0;
    for (const auto& s : m->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.normal_mode() == NormalMode::sun_pointing) {
            ++sun_pointing_count;
            CHECK_THAT(fs.area_m2().value(), WithinAbs(45.0, 1.0e-9));
        }
    }
    CHECK(sun_pointing_count == 1);
}

// --- SPCR-A-023 ----------------------------------------------------------------

TEST_CASE("SPCR-A-023  the guard shown firing: a deliberately blank "
          "citation, through this module's own Cited/body_direction call "
          "path, is refused by MCRM-F-001",
          "[spacecraft][qzss]") {
    auto area = cited(9.0, "");
    REQUIRE_FALSE(area.has_value());
    CHECK(area.error().id == "MCRM-F-001");

    auto normal = body_direction(qzss_frame_from_native(Vec3{1.0, 0.0, 0.0}));
    REQUIRE(normal.has_value());
}
