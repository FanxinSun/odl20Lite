// macromodel_tests.cpp — SPEC-macromodel §8, L4 step 2's gate.
//
// SCHEMA AND VALIDATION ONLY.  L4 step 2's review moved `srp_force` and every
// test that computed a force out to modules/srp_analytic (SPEC-srp-analytic
// SRPA-A-001..007) -- this file keeps exactly what is left: construction,
// citation enforcement, and the round trip a pure data type owes its own gate
// once the force that used to exercise it indirectly is gone.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/irradiance.hpp>
#include <odl/macromodel/macromodel.hpp>

#include <cmath>
#include <limits>

using namespace odl;
using namespace odl::macromodel;

namespace {

OpticalTriple triple(double a, double s, double d, const char* tag) {
    auto al = cited(a, tag);
    auto rh = cited(s, tag);
    auto de = cited(d, tag);
    REQUIRE(al.has_value());
    REQUIRE(rh.has_value());
    REQUIRE(de.has_value());
    return OpticalTriple{*al, *rh, *de};
}

}  // namespace

TEST_CASE("MCRM-A-011  the cannonball round-trip: store one spherical surface, read back every "
          "field with its citation, and get back exactly what was put in", "[macromodel][gate]") {
    // ADDED after the force law left this module (plan §5 constraint 7's own
    // point applied to a schema rather than a layer): the schema needs a gate
    // that does not depend on any force law existing at all, since a value
    // this general -- N surfaces of either kind -- could otherwise be fully
    // exercised only indirectly, through whichever consumer happens to call
    // it. A pure identity round trip is the schema's own cannonball.
    auto area = cited(2.5, "test-stated area");
    auto alpha = cited(0.2, "test-stated alpha");
    auto rho = cited(0.3, "test-stated rho");
    auto delta = cited(0.5, "test-stated delta");
    REQUIRE(area.has_value());
    REQUIRE(alpha.has_value());
    REQUIRE(rho.has_value());
    REQUIRE(delta.has_value());
    SphericalSurface sph{*area, *alpha, *rho, *delta, std::nullopt};

    auto mass = cited(1234.5, "test-stated mass");
    auto com = cited(Vec3{0.1, -0.2, 0.3}, "test-stated centre of mass");
    REQUIRE(mass.has_value());
    REQUIRE(com.has_value());

    MacromodelBuilder b;
    b.add_surface(sph).set_mass(*mass).set_centre_of_mass(*com);
    auto model = std::move(b).build();
    REQUIRE(model.has_value());

    REQUIRE(model->surfaces().size() == 1);
    const auto* read_back = std::get_if<SphericalSurface>(&model->surfaces().front());
    REQUIRE(read_back != nullptr);   // the kind survived the round trip

    CHECK(read_back->cross_section_area_m2.value() == 2.5);
    CHECK(read_back->cross_section_area_m2.citation() == "test-stated area");
    CHECK(read_back->absorptivity.value() == 0.2);
    CHECK(read_back->absorptivity.citation() == "test-stated alpha");
    CHECK(read_back->specular.value() == 0.3);
    CHECK(read_back->specular.citation() == "test-stated rho");
    CHECK(read_back->diffuse.value() == 0.5);
    CHECK(read_back->diffuse.citation() == "test-stated delta");

    CHECK(model->mass_kg().value() == 1234.5);
    CHECK(model->mass_kg().citation() == "test-stated mass");
    CHECK(model->centre_of_mass_m().value().x == 0.1);
    CHECK(model->centre_of_mass_m().value().y == -0.2);
    CHECK(model->centre_of_mass_m().value().z == 0.3);
    CHECK(model->centre_of_mass_m().citation() == "test-stated centre of mass");
}

TEST_CASE("MCRM-A-012  the flat-surface round trip, both normal modes", "[macromodel][gate]") {
    auto area = cited(4.0, "test-stated");
    auto alpha = cited(0.4, "test-stated");
    auto rho = cited(0.1, "test-stated");
    auto delta = cited(0.5, "test-stated");
    REQUIRE(area.has_value());
    REQUIRE(alpha.has_value());
    REQUIRE(rho.has_value());
    REQUIRE(delta.has_value());

    auto sp = flat_surface_sun_pointing(*area, *alpha, *rho, *delta);
    REQUIRE(sp.has_value());
    CHECK(sp->normal_mode() == NormalMode::sun_pointing);
    CHECK_FALSE(sp->body_fixed_normal().has_value());   // MCRM-R-002's pairing: no normal stored

    auto n = body_direction(Vec3{0.0, 0.0, 1.0});
    REQUIRE(n.has_value());
    auto bf = flat_surface_body_fixed(*area, *n, *alpha, *rho, *delta);
    REQUIRE(bf.has_value());
    CHECK(bf->normal_mode() == NormalMode::body_fixed);
    REQUIRE(bf->body_fixed_normal().has_value());
    CHECK(bf->body_fixed_normal()->vec().z == 1.0);
}

TEST_CASE("MCRM-A-006  citation enforcement fires, one case per field, and not on the adjacent "
          "fully-cited input", "[macromodel][gate]") {
    // the four surface-level fields: cited() itself refuses
    CHECK_FALSE(cited(1.0, "").has_value());
    CHECK_FALSE(cited(1.0, "   ").has_value());
    auto blank_area = cited(1.0, "");
    REQUIRE_FALSE(blank_area.has_value());
    CHECK(blank_area.error().id == "MCRM-F-001");

    // and it does NOT fire on the adjacent, properly-cited input
    auto good = cited(1.0, "test-stated");
    CHECK(good.has_value());

    // the two macromodel-level fields: build() refuses when unset
    {
        MacromodelBuilder b;   // neither mass nor COM set
        auto r = std::move(b).build();
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MCRM-F-003");
    }
    {
        MacromodelBuilder b;
        auto com = cited(Vec3{0, 0, 0}, "test-stated");
        REQUIRE(com.has_value());
        b.set_centre_of_mass(*com);   // mass still unset
        auto r = std::move(b).build();
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MCRM-F-003");
    }
    {
        // fully cited: build() succeeds
        MacromodelBuilder b;
        auto mass = cited(100.0, "test-stated");
        auto com = cited(Vec3{0, 0, 0}, "test-stated");
        REQUIRE(mass.has_value());
        REQUIRE(com.has_value());
        b.set_mass(*mass).set_centre_of_mass(*com);
        auto r = std::move(b).build();
        CHECK(r.has_value());
    }
}

TEST_CASE("MCRM-A-007  a non-unit BodyDirection is refused, and a unit one is not",
          "[macromodel][gate]") {
    auto not_unit = body_direction(Vec3{1.0, 1.0, 0.0});   // norm = sqrt(2)
    REQUIRE_FALSE(not_unit.has_value());
    CHECK(not_unit.error().id == "MCRM-F-002");

    auto zero = body_direction(Vec3{0.0, 0.0, 0.0});
    REQUIRE_FALSE(zero.has_value());
    CHECK(zero.error().id == "MCRM-F-002");

    // and it does NOT fire on the adjacent unit input
    auto unit = body_direction(Vec3{0.0, 0.0, 1.0});
    CHECK(unit.has_value());
}

TEST_CASE("MCRM-A-013  the band/face round trip, schema-only: BandedOptics.in() falls back "
          "correctly, FlatSurface.back() is absent/present correctly, on both surface kinds",
          "[macromodel][gate]") {
    auto area = cited(1.0, "test-stated");
    REQUIRE(area.has_value());
    const OpticalTriple front_vis = triple(0.1, 0.8, 0.1, "front-visible");
    const OpticalTriple front_ir = triple(0.5, 0.3, 0.2, "front-infrared");   // DISTINCT values

    // No front-infrared supplied: front(infrared) falls back to front(visible), value-equal.
    auto no_ir = flat_surface_body_fixed(*area, *body_direction(Vec3{0, 0, 1}),
                                         front_vis.absorptivity, front_vis.specular,
                                         front_vis.diffuse);
    REQUIRE(no_ir.has_value());
    CHECK(no_ir->front(Band::infrared).absorptivity.value() ==
         no_ir->front(Band::visible).absorptivity.value());
    CHECK(no_ir->front(Band::infrared).absorptivity.value() == 0.1);   // == visible, not infrared
    CHECK_FALSE(no_ir->back(Band::visible).has_value());   // no back supplied: one-sided
    CHECK_FALSE(no_ir->back(Band::infrared).has_value());

    // Front-infrared supplied, distinct from visible: front(infrared) returns IT, not visible.
    auto with_front_ir = flat_surface_body_fixed(*area, *body_direction(Vec3{0, 0, 1}),
                                                 front_vis.absorptivity, front_vis.specular,
                                                 front_vis.diffuse, front_ir);
    REQUIRE(with_front_ir.has_value());
    CHECK(with_front_ir->front(Band::infrared).absorptivity.value() == 0.5);
    CHECK(with_front_ir->front(Band::visible).absorptivity.value() == 0.1);   // unaffected

    // A back face, visible-only: back(visible) is IT; back(infrared) falls back to back-visible.
    const OpticalTriple back_vis = triple(0.9, 0.05, 0.05, "back-visible");
    auto with_back = flat_surface_body_fixed(*area, *body_direction(Vec3{0, 0, 1}),
                                             front_vis.absorptivity, front_vis.specular,
                                             front_vis.diffuse, std::nullopt,
                                             BandedOptics{back_vis, std::nullopt});
    REQUIRE(with_back.has_value());
    REQUIRE(with_back->back(Band::visible).has_value());
    CHECK(with_back->back(Band::visible)->absorptivity.value() == 0.9);
    REQUIRE(with_back->back(Band::infrared).has_value());
    CHECK(with_back->back(Band::infrared)->absorptivity.value() == 0.9);   // fallback to back-visible

    // A back face WITH its own infrared triple: back(infrared) returns IT, not back-visible.
    const OpticalTriple back_ir = triple(0.2, 0.1, 0.7, "back-infrared");
    auto with_back_both = flat_surface_body_fixed(*area, *body_direction(Vec3{0, 0, 1}),
                                                  front_vis.absorptivity, front_vis.specular,
                                                  front_vis.diffuse, std::nullopt,
                                                  BandedOptics{back_vis, back_ir});
    REQUIRE(with_back_both.has_value());
    REQUIRE(with_back_both->back(Band::infrared).has_value());
    CHECK(with_back_both->back(Band::infrared)->absorptivity.value() == 0.2);

    // SphericalSurface: the same fall-back, via its own .in(Band).
    auto alpha = cited(0.1, "test-stated"), rho = cited(0.8, "test-stated"),
        delta = cited(0.1, "test-stated");
    REQUIRE(alpha.has_value()); REQUIRE(rho.has_value()); REQUIRE(delta.has_value());
    SphericalSurface sph_no_ir{*area, *alpha, *rho, *delta, std::nullopt};
    CHECK(sph_no_ir.in(Band::infrared).absorptivity.value() == 0.1);   // falls back to visible
    SphericalSurface sph_with_ir{*area, *alpha, *rho, *delta, front_ir};
    CHECK(sph_with_ir.in(Band::infrared).absorptivity.value() == 0.5);   // its own stated triple
    CHECK(sph_with_ir.in(Band::visible).absorptivity.value() == 0.1);    // unaffected
}

TEST_CASE("MCRM-A-014  irradiance_w_per_m2 refuses a negative and a NaN value, does not refuse "
          "an adjacent valid one (zero included), and that valid one round-trips exactly",
          "[macromodel][gate]") {
    auto negative = irradiance_w_per_m2(-1.0);
    REQUIRE_FALSE(negative.has_value());
    CHECK(negative.error().id == "MCRM-F-006");

    auto nan_value = irradiance_w_per_m2(std::nan(""));
    REQUIRE_FALSE(nan_value.has_value());
    CHECK(nan_value.error().id == "MCRM-F-006");

    auto infinite = irradiance_w_per_m2(std::numeric_limits<double>::infinity());
    REQUIRE_FALSE(infinite.has_value());
    CHECK(infinite.error().id == "MCRM-F-006");

    // Not refused: zero, and a genuine flux density -- round-tripping exactly (MCRM-R-015).
    auto zero = irradiance_w_per_m2(0.0);
    REQUIRE(zero.has_value());
    CHECK(zero->watts_per_m2() == 0.0);

    auto real = irradiance_w_per_m2(1367.0);
    REQUIRE(real.has_value());
    CHECK(real->watts_per_m2() == 1367.0);
}
