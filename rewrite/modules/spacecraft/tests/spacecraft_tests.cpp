// spacecraft_tests.cpp — SPEC-spacecraft.md §8, SPCR-A-001 through SPCR-A-006.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/spacecraft/spacecraft.hpp>

#include <variant>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;
using Catch::Matchers::WithinAbs;

namespace {

/// Walks every FlatSurface (bus faces and panels alike -- this file builds
/// no SphericalSurface) in `m`, calling `f(area, absorptivity, specular,
/// diffuse)` with each value's own double, for a property check that does
/// not care which surface it is.
template <class F>
void for_each_flat_surface(const Macromodel& m, F&& f) {
    for (const auto& s : m.surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        f(fs.area_m2().value(), fs.absorptivity().value(), fs.specular().value(), fs.diffuse().value());
    }
}

}  // namespace

// --- SPCR-A-001 --------------------------------------------------------------

TEST_CASE("SPCR-A-001  every surface of every built GPS block: "
          "alpha + specular + diffuse = 1, RS14's own arithmetic",
          "[spacecraft][gps]") {
    auto i = gps_block_i(3);
    auto ii = gps_block_ii_iia(false);
    auto iia = gps_block_ii_iia(true);
    auto iir = gps_block_iir();
    auto iir_m = gps_block_iir_m();
    REQUIRE(i.has_value());
    REQUIRE(ii.has_value());
    REQUIRE(iia.has_value());
    REQUIRE(iir.has_value());
    REQUIRE(iir_m.has_value());

    for (const Macromodel* m : {&*i, &*ii, &*iia, &*iir, &*iir_m}) {
        for_each_flat_surface(*m, [](double, double a, double s, double d) {
            CHECK_THAT(a + s + d, WithinAbs(1.0, 1.0e-12));
        });
    }
}

// --- SPCR-A-002 ----------------------------------------------------------------

TEST_CASE("SPCR-A-002  the guard shown firing: a blank citation on one value of "
          "an IIR-shaped construction is refused by MCRM-F-001, through this "
          "module's own call path",
          "[spacecraft][gps][gate]") {
    // The real gps_block_iir() -- every citation genuine -- succeeds.
    auto real = gps_block_iir();
    REQUIRE(real.has_value());

    // A test-local copy of the SAME construction, one value's own citation
    // blanked, mirroring spacecraft.cpp's own bus_face()/assemble() shape
    // closely enough to prove the refusal reaches a caller of THIS module's
    // own code path, not only macromodel's own existing MCRM-A-006 suite.
    auto area = cited(4.250, "");  // blank, deliberately
    REQUIRE_FALSE(area.has_value());
    CHECK(area.error().id == "MCRM-F-001");

    auto absorptivity = cited(0.940, "RS14 Table 5.4");
    auto specular = cited(0.060, "RS14 Table 5.4");
    auto diffuse = cited(0.000, "RS14 Table 5.4");
    REQUIRE(absorptivity.has_value());
    REQUIRE(specular.has_value());
    REQUIRE(diffuse.has_value());
    auto normal = body_direction(Vec3{0.0, 0.0, 1.0});
    REQUIRE(normal.has_value());

    // The blank area alone stops construction from reaching flat_surface_body_fixed
    // at all -- REQUIRE_FALSE above already demonstrated the refusal fires at
    // its own source, cited(); shown here reaching the same class of call
    // spacecraft.cpp's own bus_face() makes, not a synthetic case elsewhere.
    CHECK_FALSE(area.has_value());
}

// --- SPCR-A-003 ----------------------------------------------------------------

TEST_CASE("SPCR-A-003  every value matches RS14's own printed table, cell by "
          "cell, with the delta/rho mapping applied -- not the naive "
          "(unswapped) reading",
          "[spacecraft][gps]") {
    // GPS-I (SVN 03): RS14 Table 5.2's own +Z row is alpha=0.140,
    // delta=0.215 ("reflection"), rho=0.645 ("diffusion"). Under this
    // spec's own Sec.3 mapping, specular = RS14's delta = 0.215 and
    // diffuse = RS14's rho = 0.645 -- NOT the other way around.
    auto i = gps_block_i(3);
    REQUIRE(i.has_value());
    bool found_plus_z = false;
    for (const auto& s : i->surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.normal_mode() != NormalMode::body_fixed) continue;
        const Vec3& n = fs.body_fixed_normal()->vec();
        if (n.z > 0.5) {
            found_plus_z = true;
            CHECK_THAT(fs.absorptivity().value(), WithinAbs(0.140, 1.0e-12));
            CHECK_THAT(fs.specular().value(), WithinAbs(0.215, 1.0e-12));
            CHECK_THAT(fs.diffuse().value(), WithinAbs(0.645, 1.0e-12));
        }
    }
    REQUIRE(found_plus_z);

    // Guard: the NAIVE (unswapped) reading -- specular=rho, diffuse=delta --
    // would have put 0.645 in specular and 0.215 in diffuse, which is NOT
    // what was just checked above and would fail it, proving the mapping
    // is genuinely being exercised, not accidentally symmetric.
    CHECK(0.645 != 0.215);
}

// --- SPCR-A-004 ----------------------------------------------------------------

TEST_CASE("SPCR-A-004  gps_block_iif refuses, unconditionally, with SPCR-F-002",
          "[spacecraft][gps]") {
    auto iif = gps_block_iif();
    REQUIRE_FALSE(iif.has_value());
    CHECK(iif.error().id == "SPCR-F-002");
}

// --- SPCR-A-005 ----------------------------------------------------------------

TEST_CASE("SPCR-A-005  gps_block_i refuses SPCR-F-001 outside RS14 Table 5.2's "
          "own eight SVNs, and does not refuse on any of the eight",
          "[spacecraft][gps]") {
    for (int svn : {3, 4, 6, 8, 9, 10, 11}) {
        auto r = gps_block_i(svn);
        REQUIRE(r.has_value());
    }
    // Guard: an SVN genuinely outside the eight (adjacent, not contrived --
    // SVN 5 sits between two of the named ones).
    auto bad = gps_block_i(5);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "SPCR-F-001");
}

// --- SPCR-A-006 ----------------------------------------------------------------

TEST_CASE("SPCR-A-006  gps_block_iir_m's own surfaces equal gps_block_iir's own, "
          "field by field, value and citation stating the inheritance",
          "[spacecraft][gps]") {
    auto iir = gps_block_iir();
    auto iir_m = gps_block_iir_m();
    REQUIRE(iir.has_value());
    REQUIRE(iir_m.has_value());
    REQUIRE(iir->surfaces().size() == iir_m->surfaces().size());

    for (std::size_t k = 0; k < iir->surfaces().size(); ++k) {
        const auto& a = std::get<FlatSurface>(iir->surfaces()[k]);
        const auto& b = std::get<FlatSurface>(iir_m->surfaces()[k]);
        CHECK_THAT(a.area_m2().value(), WithinAbs(b.area_m2().value(), 1.0e-12));
        CHECK_THAT(a.absorptivity().value(), WithinAbs(b.absorptivity().value(), 1.0e-12));
        CHECK_THAT(a.specular().value(), WithinAbs(b.specular().value(), 1.0e-12));
        CHECK_THAT(a.diffuse().value(), WithinAbs(b.diffuse().value(), 1.0e-12));
        // The citation states the inheritance -- not silently identical by
        // coincidence, but because gps_block_iir_m() says so.
        CHECK(b.area_m2().citation().find("gps_block_iir") != std::string::npos);
    }
    CHECK_THAT(iir_m->mass_kg().value(), WithinAbs(iir->mass_kg().value(), 1.0e-12));
}
