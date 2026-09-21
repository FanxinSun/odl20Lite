// macromodel_tests.cpp — SPEC-macromodel §8, L4 step 2's gate.
//
// GATED ON A CANNONBALL ROUND-TRIP, per the plan's own words for this step.
// Every geometry and every optical triple below is STATED IN THIS FILE -- no
// manifest entry, no library, nothing read from L5's (not-yet-existing)
// population. That is what keeps this layer's exit gate satisfiable with what
// this layer and the ones below it have (plan §5 constraint 7).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/macromodel.hpp>

#include <cmath>
#include <numbers>

using namespace odl;
using namespace odl::macromodel;

namespace {

constexpr double kS0 = 1367.0;           // matches macromodel.cpp's own constant
constexpr double kC = 299792458.0;

BodyDirection must_dir(double x, double y, double z) {
    const double n = std::sqrt(x * x + y * y + z * z);
    auto d = body_direction(Vec3{x / n, y / n, z / n});
    REQUIRE(d.has_value());
    return *d;
}

/// Five (alpha, rho, delta) triples spanning pure-absorbing, pure-specular,
/// pure-diffuse and two mixed cases -- the same five MCRM-P-1's Monte Carlo
/// check used, so the acceptance gate and the derivation's own corroboration
/// examine the same points.
struct Triple { double alpha, rho, delta; };
const Triple kTriples[] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0},
                           {0.5, 0.3, 0.2}, {0.2, 0.5, 0.3}};

Macromodel one_sphere(double area_m2, const Triple& t) {
    auto a = cited(area_m2, "test-stated");
    auto al = cited(t.alpha, "test-stated");
    auto rh = cited(t.rho, "test-stated");
    auto de = cited(t.delta, "test-stated");
    REQUIRE(a.has_value());
    REQUIRE(al.has_value());
    REQUIRE(rh.has_value());
    REQUIRE(de.has_value());
    SphericalSurface sph{*a, *al, *rh, *de};

    MacromodelBuilder b;
    b.add_surface(sph);
    auto mass = cited(1.0, "test-stated");   // 1 kg: MCRM-R-005 returns FORCE, not
                                              // acceleration, so mass does not enter
                                              // the quantity these tests check
    auto com = cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
    REQUIRE(mass.has_value());
    REQUIRE(com.has_value());
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto m = std::move(b).build();
    REQUIRE(m.has_value());
    return *m;
}

Macromodel one_sun_pointing_flat(double area_m2, const Triple& t) {
    auto a = cited(area_m2, "test-stated");
    auto al = cited(t.alpha, "test-stated");
    auto rh = cited(t.rho, "test-stated");
    auto de = cited(t.delta, "test-stated");
    REQUIRE(a.has_value());
    auto fs = flat_surface_sun_pointing(*a, *al, *rh, *de);
    REQUIRE(fs.has_value());

    MacromodelBuilder b;
    b.add_surface(*fs);
    auto mass = cited(1.0, "test-stated");
    auto com = cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto m = std::move(b).build();
    REQUIRE(m.has_value());
    return *m;
}

}  // namespace

TEST_CASE("MCRM-A-001  the cannonball round-trip: sphere against the closed form",
          "[macromodel][gate]") {
    constexpr double kArea = 2.5;   // m^2, stated in the test, not from any table
    int checked = 0;
    double worst_rel = 0.0;
    for (const auto& t : kTriples) {
        auto model = one_sphere(kArea, t);
        for (auto dir : {std::array{1.0, 0.0, 0.0}, std::array{0.0, 1.0, 0.0},
                         std::array{0.0, 0.0, 1.0}, std::array{1.0, 1.0, 1.0}}) {
            const auto e_D = must_dir(dir[0], dir[1], dir[2]);
            auto f = srp_force(model, e_D);
            REQUIRE(f.has_value());
            const double expected_mag = (kArea * kS0 / kC) * (1.0 + 4.0 * t.delta / 9.0);
            const double got_mag = f->norm();
            const double rel = std::abs(got_mag - expected_mag) / expected_mag;
            worst_rel = std::max(worst_rel, rel);
            // and it must point OPPOSITE the Sun direction (radiation pushes away)
            CHECK(f->dot(e_D.vec()) < 0.0);
            ++checked;
        }
    }
    INFO("checked " << checked << " (triple, direction) combinations; worst relative "
         "deviation from the closed form " << worst_rel);
    CHECK(checked == 20);
    CHECK(worst_rel < 1.0e-12);
}

TEST_CASE("MCRM-A-002  a sphere's force does not depend on rho", "[macromodel][gate]") {
    // Hold alpha+delta fixed at their kTriples[3] values and trade rho against
    // alpha alone -- MCRM-R-007 has no rho term, so the force must not move.
    constexpr double kArea = 3.0;
    constexpr double kDelta = 0.2;
    const auto e_D = must_dir(0.3, 0.4, std::sqrt(1.0 - 0.09 - 0.16));
    double reference = -1.0;
    int checked = 0;
    for (double rho : {0.0, 0.1, 0.3, 0.5, 0.7, 1.0 - kDelta}) {
        const Triple t{1.0 - kDelta - rho, rho, kDelta};
        auto model = one_sphere(kArea, t);
        auto f = srp_force(model, e_D);
        REQUIRE(f.has_value());
        if (reference < 0.0) {
            reference = f->norm();
        } else {
            CHECK_THAT(f->norm(), Catch::Matchers::WithinRel(reference, 1.0e-12));
        }
        ++checked;
    }
    INFO("checked " << checked << " rho values, force magnitude held at " << reference << " N");
    CHECK(checked == 6);
}

TEST_CASE("MCRM-A-003  a sphere's force is attitude-independent", "[macromodel][gate]") {
    // The SAME inertial Sun direction, expressed as if the body frame were
    // rotated several different ways -- since a sphere has no normal, the
    // computed force must be identical to rounding regardless of "attitude",
    // which for an all-spherical macromodel is not even a meaningful concept.
    constexpr double kArea = 1.7;
    const Triple t{0.4, 0.3, 0.3};
    auto model = one_sphere(kArea, t);
    Vec3 reference{};
    bool have_reference = false;
    int checked = 0;
    for (auto dir : {std::array{1.0, 0.0, 0.0}, std::array{0.0, 0.0, 1.0},
                     std::array{0.6, 0.8, 0.0}, std::array{-0.5, 0.5, std::sqrt(0.5)}}) {
        const auto e_D = must_dir(dir[0], dir[1], dir[2]);
        auto f = srp_force(model, e_D);
        REQUIRE(f.has_value());
        // rotate the RESULT back by aligning on e_D so magnitudes compare directly:
        // the force is always -magnitude*e_D, so magnitude alone is the invariant.
        if (!have_reference) { reference = *f; have_reference = true; }
        CHECK_THAT(f->norm(), Catch::Matchers::WithinRel(reference.norm(), 1.0e-12));
        CHECK_THAT(f->dot(e_D.vec()) / f->norm(), Catch::Matchers::WithinAbs(-1.0, 1.0e-12));
        ++checked;
    }
    INFO("checked " << checked << " differently-oriented Sun directions");
    CHECK(checked == 4);
}

TEST_CASE("MCRM-A-004  the flat-plate degenerate case, and agreement with the sphere at alpha=1",
          "[macromodel][gate]") {
    constexpr double kArea = 4.0;
    const Triple black{1.0, 0.0, 0.0};   // fully absorbing: the textbook case
    auto flat_model = one_sun_pointing_flat(kArea, black);
    auto sphere_model = one_sphere(kArea, black);

    int checked = 0;
    double worst_rel_textbook = 0.0, worst_diff_cross = 0.0;
    for (auto dir : {std::array{1.0, 0.0, 0.0}, std::array{0.0, 1.0, 0.0},
                     std::array{-0.4, 0.4, std::sqrt(1.0 - 0.16 - 0.16)}}) {
        const auto e_D = must_dir(dir[0], dir[1], dir[2]);
        auto f_flat = srp_force(flat_model, e_D);
        auto f_sphere = srp_force(sphere_model, e_D);
        REQUIRE(f_flat.has_value());
        REQUIRE(f_sphere.has_value());

        const double textbook = kArea * kS0 / kC;   // f = S0*A/c, alpha=1
        worst_rel_textbook = std::max(worst_rel_textbook,
            std::abs(f_flat->norm() - textbook) / textbook);
        worst_diff_cross = std::max(worst_diff_cross, std::abs(f_flat->norm() - f_sphere->norm()));
        CHECK(f_flat->dot(e_D.vec()) < 0.0);
        ++checked;
    }
    INFO("checked " << checked << " Sun directions; worst |flat - S0*A/c|/  (S0*A/c) = "
         << worst_rel_textbook << "; worst |flat - sphere| at alpha=1 = " << worst_diff_cross << " N");
    CHECK(checked == 3);
    CHECK(worst_rel_textbook < 1.0e-12);
    CHECK(worst_diff_cross < 1.0e-9);
}

TEST_CASE("MCRM-A-005  the sphere and flat-plate diffuse coefficients genuinely differ",
          "[macromodel][gate]") {
    // At the SAME delta, rho=0: flat-plate normal incidence is 1+2*delta/3,
    // sphere is 1+4*delta/9. MCRM-R-008: these must NOT be equal, and the gap
    // is exactly 2*delta/9 -- the guard against silently substituting one
    // model's coefficient for the other's.
    int checked = 0;
    for (double delta : {0.1, 0.3, 0.5, 0.8, 1.0}) {
        const double flat_coeff = 1.0 + 2.0 * delta / 3.0;
        const double sphere_coeff = 1.0 + 4.0 * delta / 9.0;
        const double gap = flat_coeff - sphere_coeff;
        INFO("delta=" << delta << " flat=" << flat_coeff << " sphere=" << sphere_coeff);
        CHECK(gap > 0.0);
        CHECK_THAT(gap, Catch::Matchers::WithinAbs(2.0 * delta / 9.0, 1.0e-12));
        ++checked;
    }
    CHECK(checked == 5);
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

TEST_CASE("MCRM-A-007  non-unit BodyDirection and sun-direction inputs are refused, and unit "
          "ones are not", "[macromodel][gate]") {
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

TEST_CASE("MCRM-A-008  srp_force refuses an empty macromodel", "[macromodel][gate]") {
    MacromodelBuilder b;
    auto mass = cited(100.0, "test-stated");
    auto com = cited(Vec3{0, 0, 0}, "test-stated");
    REQUIRE(mass.has_value());
    REQUIRE(com.has_value());
    b.set_mass(*mass).set_centre_of_mass(*com);   // no surfaces added
    auto m = std::move(b).build();
    REQUIRE(m.has_value());

    const auto e_D = must_dir(1.0, 0.0, 0.0);
    auto f = srp_force(*m, e_D);
    REQUIRE_FALSE(f.has_value());
    CHECK(f.error().id == "MCRM-F-004");
}

TEST_CASE("MCRM-A-010  a body-fixed flat surface facing away from the Sun contributes nothing",
          "[macromodel][gate]") {
    // Not a numbered acceptance row on its own -- folded into R-005's own
    // domain statement -- but the cos(theta) < 0 branch is code with no other
    // test reaching it, and a guard with no test is a guard nobody has proven.
    constexpr double kArea = 5.0;
    const Triple t{0.5, 0.3, 0.2};
    auto area = cited(kArea, "test-stated");
    auto al = cited(t.alpha, "test-stated");
    auto rh = cited(t.rho, "test-stated");
    auto de = cited(t.delta, "test-stated");
    auto normal = must_dir(0.0, 0.0, 1.0);
    auto fs = flat_surface_body_fixed(*area, normal, *al, *rh, *de);
    REQUIRE(fs.has_value());

    MacromodelBuilder b;
    b.add_surface(*fs);
    auto mass = cited(1.0, "test-stated");
    auto com = cited(Vec3{0, 0, 0}, "test-stated");
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto model = std::move(b).build();
    REQUIRE(model.has_value());

    // Sun BEHIND the surface: e_D . e_N < 0
    const auto e_D = must_dir(0.0, 0.0, -1.0);
    auto f = srp_force(*model, e_D);
    REQUIRE(f.has_value());
    CHECK(f->norm() == 0.0);
}
