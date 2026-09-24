// srp_analytic_tests.cpp — SPEC-srp-analytic §8, relocated from
// modules/macromodel/tests/macromodel_tests.cpp (L4 step 2's review).
//
// EVERY GEOMETRY AND OPTICAL TRIPLE BELOW IS STATED IN THIS FILE -- no
// manifest entry, no library, nothing read from RS14's Tables 1-2 (L5's job).
// That is what keeps this layer's exit gate satisfiable with what this layer
// and the ones below it have (plan §5 constraint 7).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/macromodel.hpp>
#include <odl/srp_analytic/srp_analytic.hpp>

#include "srp_force_golden.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using namespace odl;
using namespace odl::macromodel;
using odl::srp_analytic::photon_force;
using odl::srp_analytic::srp_force;

namespace {

constexpr double kS0 = 1367.0;           // matches srp_analytic.cpp's own constant
constexpr double kC = 299792458.0;

BodyDirection must_dir(double x, double y, double z) {
    const double n = std::sqrt(x * x + y * y + z * z);
    auto d = body_direction(Vec3{x / n, y / n, z / n});
    REQUIRE(d.has_value());
    return *d;
}

struct Triple { double alpha, rho, delta; };
/// Five (alpha, rho, delta) triples spanning pure-absorbing, pure-specular,
/// pure-diffuse and two mixed cases -- the same five MCRM-P-1's Monte Carlo
/// check used.
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
    SphericalSurface sph{*a, *al, *rh, *de, std::nullopt};

    MacromodelBuilder b;
    b.add_surface(sph);
    auto mass = cited(1.0, "test-stated");   // 1 kg: R-006 returns FORCE, not
                                              // acceleration, so mass does not
                                              // enter the quantity checked here
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

/// PHPR-A-001's own construction: a body-fixed flat surface, normal +z.
Macromodel one_flat_body_fixed_z(double area_m2, const Triple& t) {
    auto a = cited(area_m2, "test-stated");
    auto al = cited(t.alpha, "test-stated");
    auto rh = cited(t.rho, "test-stated");
    auto de = cited(t.delta, "test-stated");
    auto normal = must_dir(0.0, 0.0, 1.0);
    auto fs = flat_surface_body_fixed(*a, normal, *al, *rh, *de);
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

/// PHPR-A-001's own construction: a body-fixed bus (+z), a sun-pointing
/// panel and a sphere, summed -- SRPA-R-008's own box-wing composition.
Macromodel phpr_box_wing(const Triple& bus_t, const Triple& panel_t) {
    auto bus_a = cited(3.0, "test-stated");
    auto bus_al = cited(bus_t.alpha, "test-stated");
    auto bus_rh = cited(bus_t.rho, "test-stated");
    auto bus_de = cited(bus_t.delta, "test-stated");
    auto bus_n = must_dir(0.0, 0.0, 1.0);
    auto bus = flat_surface_body_fixed(*bus_a, bus_n, *bus_al, *bus_rh, *bus_de);
    REQUIRE(bus.has_value());

    auto pan_a = cited(2.0, "test-stated");
    auto pan_al = cited(panel_t.alpha, "test-stated");
    auto pan_rh = cited(panel_t.rho, "test-stated");
    auto pan_de = cited(panel_t.delta, "test-stated");
    auto panel = flat_surface_sun_pointing(*pan_a, *pan_al, *pan_rh, *pan_de);
    REQUIRE(panel.has_value());

    auto sph_a = cited(0.5, "test-stated");
    auto sph_al = cited(0.3, "test-stated");
    auto sph_rh = cited(0.3, "test-stated");
    auto sph_de = cited(0.4, "test-stated");
    SphericalSurface sph{*sph_a, *sph_al, *sph_rh, *sph_de, std::nullopt};

    MacromodelBuilder b;
    b.add_surface(*bus).add_surface(*panel).add_surface(sph);
    auto mass = cited(500.0, "test-stated");
    auto com = cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto m = std::move(b).build();
    REQUIRE(m.has_value());
    return *m;
}

}  // namespace

TEST_CASE("SRPA-A-001  the cannonball round-trip: sphere against the closed form",
          "[srp_analytic][gate]") {
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
            CHECK(f->dot(e_D.vec()) < 0.0);   // radiation pushes AWAY from the Sun
            ++checked;
        }
    }
    INFO("checked " << checked << " (triple, direction) combinations; worst relative "
         "deviation from the closed form " << worst_rel);
    CHECK(checked == 20);
    CHECK(worst_rel < 1.0e-12);
}

TEST_CASE("SRPA-A-002  a sphere's force does not depend on rho", "[srp_analytic][gate]") {
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

TEST_CASE("SRPA-A-003  a sphere's force is attitude-independent", "[srp_analytic][gate]") {
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
        if (!have_reference) { reference = *f; have_reference = true; }
        CHECK_THAT(f->norm(), Catch::Matchers::WithinRel(reference.norm(), 1.0e-12));
        CHECK_THAT(f->dot(e_D.vec()) / f->norm(), Catch::Matchers::WithinAbs(-1.0, 1.0e-12));
        ++checked;
    }
    INFO("checked " << checked << " differently-oriented Sun directions");
    CHECK(checked == 4);
}

TEST_CASE("SRPA-A-004  the flat-plate degenerate case, and agreement with the sphere at alpha=1",
          "[srp_analytic][gate]") {
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
    INFO("checked " << checked << " Sun directions; worst |flat - S0*A/c|/(S0*A/c) = "
         << worst_rel_textbook << "; worst |flat - sphere| at alpha=1 = " << worst_diff_cross << " N");
    CHECK(checked == 3);
    CHECK(worst_rel_textbook < 1.0e-12);
    CHECK(worst_diff_cross < 1.0e-9);
}

TEST_CASE("SRPA-A-005  the flat-plate/sphere gap across rho, INCLUDING the specular case the "
          "original guard missed", "[srp_analytic][gate]") {
    // CORRECTED FROM MCRM-A-005 (SPEC-macromodel v1.0), which asserted this gap
    // at rho=0 ONLY -- where it is exactly 2*delta/9, small by construction
    // (<=2/9 for any delta<=1). The manager's ruling: the gap is rho + 2*delta/9
    // in general, so rho -- not delta -- is where "a factor of two in a
    // recovered area" actually lives, and a guard that zeroes rho tests the
    // one case where there ISN'T a large factor. A specular sail (rho=0.9,
    // delta=0) is exactly the shape of this project's one confirmed error on
    // real data (a reflectivity/specularity mix-up, LightSail-2), and the
    // ORIGINAL guard set rho=0 throughout -- it could not have seen that error
    // if it recurred, no matter how many delta values it swept.
    struct Case { double rho, delta, expected_ratio; };
    const Case cases[] = {
        {0.0, 0.3, (1.0 + 0.0 + 2.0*0.3/3.0) / (1.0 + 4.0*0.3/9.0)},   // diffuse-only: small
        {0.5, 0.2, 1.50},                                               // mixed panel
        {0.9, 0.0, 1.90},                                               // SPECULAR SAIL: the case
                                                                          // that matters and the
                                                                          // one the original test
                                                                          // never tried
        {0.9, 0.3, (1.0 + 0.9 + 2.0*0.3/3.0) / (1.0 + 4.0*0.3/9.0)},    // specular AND diffuse
    };
    int checked = 0;
    double worst_gap_err = 0.0;
    bool saw_specular_dominant = false;
    for (const auto& c : cases) {
        const double flat_coeff = 1.0 + c.rho + 2.0 * c.delta / 3.0;
        const double sphere_coeff = 1.0 + 4.0 * c.delta / 9.0;
        const double gap = flat_coeff - sphere_coeff;
        const double expected_gap = c.rho + 2.0 * c.delta / 9.0;
        INFO("rho=" << c.rho << " delta=" << c.delta << " flat=" << flat_coeff
             << " sphere=" << sphere_coeff << " gap=" << gap << " ratio=" << flat_coeff / sphere_coeff);
        CHECK(gap > 0.0);
        worst_gap_err = std::max(worst_gap_err, std::abs(gap - expected_gap));
        CHECK_THAT(gap, Catch::Matchers::WithinAbs(expected_gap, 1.0e-12));
        if (c.expected_ratio > 0.0)
            CHECK_THAT(flat_coeff / sphere_coeff, Catch::Matchers::WithinAbs(c.expected_ratio, 1.0e-6));
        if (c.rho >= 0.9 && flat_coeff / sphere_coeff > 1.5) saw_specular_dominant = true;
        ++checked;
    }
    INFO("checked " << checked << " (rho, delta) cases; worst gap-formula error " << worst_gap_err);
    CHECK(checked == 4);
    // THE GUARD THIS ROW EXISTS FOR: at least one case with substantial rho
    // must show a ratio well above what any rho=0 sweep could ever produce
    // (max 1 + 2/9 = 1.222 for delta<=1) -- asserted, not merely computed, so
    // that a future edit narrowing this test back to rho=0 fails loudly.
    CHECK(saw_specular_dominant);
}

TEST_CASE("SRPA-A-006  srp_force refuses an empty macromodel", "[srp_analytic][gate]") {
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
    CHECK(f.error().id == "SRPA-F-001");
}

TEST_CASE("SRPA-A-007  a body-fixed flat surface facing away from the Sun contributes nothing",
          "[srp_analytic][gate]") {
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

namespace {

/// Builds a Macromodel from an already-constructed list of surfaces, sharing
/// the same test-stated 1 kg mass and origin centre of mass every other
/// helper in this file uses (irrelevant to srp_force, which returns force).
Macromodel many_surfaces(std::vector<Surface> surfaces) {
    MacromodelBuilder b;
    for (auto& s : surfaces) b.add_surface(std::move(s));
    auto mass = cited(1.0, "test-stated");
    auto com = cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
    REQUIRE(mass.has_value());
    REQUIRE(com.has_value());
    b.set_mass(*mass).set_centre_of_mass(*com);
    auto m = std::move(b).build();
    REQUIRE(m.has_value());
    return *m;
}

Surface flat_body_fixed(double area, const Vec3& normal_dir, const Triple& t) {
    auto a = cited(area, "test-stated");
    auto al = cited(t.alpha, "test-stated");
    auto rh = cited(t.rho, "test-stated");
    auto de = cited(t.delta, "test-stated");
    auto n = must_dir(normal_dir.x, normal_dir.y, normal_dir.z);
    REQUIRE(a.has_value());
    auto fs = flat_surface_body_fixed(*a, n, *al, *rh, *de);
    REQUIRE(fs.has_value());
    return Surface{*fs};
}

Surface flat_sun_pointing(double area, const Triple& t) {
    auto a = cited(area, "test-stated");
    auto al = cited(t.alpha, "test-stated");
    auto rh = cited(t.rho, "test-stated");
    auto de = cited(t.delta, "test-stated");
    REQUIRE(a.has_value());
    auto fs = flat_surface_sun_pointing(*a, *al, *rh, *de);
    REQUIRE(fs.has_value());
    return Surface{*fs};
}

}  // namespace

TEST_CASE("SRPA-A-009  box-wing composition: a multi-surface force equals the sum of its "
          "surfaces' own forces", "[srp_analytic][gate]") {
    // A small RHS12-SHAPED macromodel -- one sun-pointing solar panel plus
    // three body-fixed bus faces (+X, +Z, -Z; SRPA-R-008a's own convention) --
    // with areas and optical properties STATED HERE, not read from RS14's
    // Tables 1-2: this gate is composition, not a populated library.
    const Triple panel{0.7, 0.05, 0.25}, plusX{0.5, 0.1, 0.4}, plusZ{0.44, 0.11, 0.45},
        minusZ{0.58, 0.08, 0.34};
    const double a_panel = 11.0, a_x = 2.7, a_z = 2.9, a_mz = 2.9;

    std::vector<double> areas{a_panel, a_x, a_z, a_mz};
    std::vector<Triple> triples{panel, plusX, plusZ, minusZ};
    std::vector<Vec3> normals{{}, {1, 0, 0}, {0, 0, 1}, {0, 0, -1}};   // panel's is unused

    int checked = 0;
    double worst_rel = 0.0;
    for (auto dir : {std::array{1.0, 0.0, 0.0}, std::array{0.3, 0.3, std::sqrt(1.0 - 0.09 - 0.09)},
                     std::array{-0.2, 0.6, std::sqrt(1.0 - 0.04 - 0.36)}}) {
        const auto e_D = must_dir(dir[0], dir[1], dir[2]);

        std::vector<Surface> all;
        all.push_back(flat_sun_pointing(a_panel, panel));
        all.push_back(flat_body_fixed(a_x, normals[1], plusX));
        all.push_back(flat_body_fixed(a_z, normals[2], plusZ));
        all.push_back(flat_body_fixed(a_mz, normals[3], minusZ));
        auto combined = many_surfaces(std::move(all));
        auto f_combined = srp_force(combined, e_D);
        REQUIRE(f_combined.has_value());

        Vec3 summed{0.0, 0.0, 0.0};
        // panel alone
        {
            auto m = many_surfaces({flat_sun_pointing(a_panel, panel)});
            auto f = srp_force(m, e_D);
            REQUIRE(f.has_value());
            summed = summed + *f;
        }
        for (int i = 1; i <= 3; ++i) {
            auto m = many_surfaces({flat_body_fixed(areas[static_cast<std::size_t>(i)],
                                                     normals[static_cast<std::size_t>(i)],
                                                     triples[static_cast<std::size_t>(i)])});
            auto f = srp_force(m, e_D);
            REQUIRE(f.has_value());
            summed = summed + *f;
        }

        const double diff = (*f_combined - summed).norm();
        const double scale = f_combined->norm();
        const double rel = scale > 0.0 ? diff / scale : diff;
        worst_rel = std::max(worst_rel, rel);
        ++checked;
    }
    INFO("checked " << checked << " Sun directions on a 4-surface box-wing-shaped macromodel; "
         "worst relative deviation of (combined force) from (sum of individual forces) = " << worst_rel);
    CHECK(checked == 3);
    CHECK(worst_rel < 1.0e-12);
}

TEST_CASE("SRPA-A-010  mixed lit/shadowed composition: only the lit surfaces contribute, "
          "within a sum", "[srp_analytic][gate]") {
    // Sun along +X. +X bus faces it (cos theta = 1); -X-facing would not, but
    // SRPA-R-008a's own convention has no -X surface -- so this uses +Z and
    // -Z instead, at a Sun direction that lights +Z and shadows -Z exactly by
    // construction (e_D has no -Z component to speak of, but -Z's own normal
    // is (0,0,-1), giving e_D.e_N = -e_D.z < 0 whenever e_D.z > 0).
    const Triple plusX{0.5, 0.1, 0.4}, plusZ{0.44, 0.11, 0.45}, minusZ{0.58, 0.08, 0.34};
    const double a_x = 2.7, a_z = 2.9, a_mz = 2.9;
    const auto e_D = must_dir(0.6, 0.0, 0.8);   // e_D.z = 0.8 > 0: lights +Z, shadows -Z

    std::vector<Surface> all;
    all.push_back(flat_body_fixed(a_x, Vec3{1, 0, 0}, plusX));
    all.push_back(flat_body_fixed(a_z, Vec3{0, 0, 1}, plusZ));
    all.push_back(flat_body_fixed(a_mz, Vec3{0, 0, -1}, minusZ));   // shadowed at this e_D
    auto combined = many_surfaces(std::move(all));
    auto f_combined = srp_force(combined, e_D);
    REQUIRE(f_combined.has_value());

    auto m_x = many_surfaces({flat_body_fixed(a_x, Vec3{1, 0, 0}, plusX)});
    auto m_z = many_surfaces({flat_body_fixed(a_z, Vec3{0, 0, 1}, plusZ)});
    auto m_mz = many_surfaces({flat_body_fixed(a_mz, Vec3{0, 0, -1}, minusZ)});
    auto f_x = srp_force(m_x, e_D);
    auto f_z = srp_force(m_z, e_D);
    auto f_mz = srp_force(m_mz, e_D);
    REQUIRE(f_x.has_value());
    REQUIRE(f_z.has_value());
    REQUIRE(f_mz.has_value());

    INFO("-Z's own contribution at this Sun direction: " << f_mz->norm() << " N (must be exactly 0)");
    CHECK(f_mz->norm() == 0.0);   // the case is not vacuous: -Z really is shadowed here
    CHECK(f_x->norm() > 0.0);     // and +X, +Z really are lit
    CHECK(f_z->norm() > 0.0);

    const Vec3 lit_only_sum = *f_x + *f_z;   // deliberately excludes f_mz, which is zero anyway
    const double diff = (*f_combined - lit_only_sum).norm();
    const double rel = diff / f_combined->norm();
    INFO("relative deviation of (3-surface combined force) from (sum of the two LIT surfaces "
         "alone) = " << rel);
    CHECK(rel < 1.0e-12);
}

TEST_CASE("SRPA-A-011  pure absorber at oblique incidence: force exactly P*A*cos(theta) "
          "along -e_D", "[srp_analytic][gate]") {
    // alpha=1 is the ONLY configuration this file had not tried at oblique
    // incidence: SRPA-A-004 used alpha=1 too, but sun_pointing (cos theta=1
    // always, e_N=e_D always). Here the surface is body_fixed with a STATED
    // normal, genuinely different from e_D, so cos theta is genuinely < 1 and
    // the -e_D/-e_N split (invisible in A-004) is exercised for the first time.
    constexpr double kArea = 3.3;
    const Triple black{1.0, 0.0, 0.0};
    const auto normal = must_dir(0.0, 0.0, 1.0);
    auto area = cited(kArea, "test-stated");
    auto al = cited(black.alpha, "test-stated");
    auto rh = cited(black.rho, "test-stated");
    auto de = cited(black.delta, "test-stated");
    REQUIRE(area.has_value());
    auto fs = flat_surface_body_fixed(*area, normal, *al, *rh, *de);
    REQUIRE(fs.has_value());
    auto model = many_surfaces({Surface{*fs}});

    int checked = 0;
    double worst_mag_rel = 0.0, worst_dir_dev = 0.0;
    for (double theta_deg : {15.0, 30.0, 45.0, 60.0, 75.0}) {
        const double theta = theta_deg * std::numbers::pi / 180.0;
        // e_D at angle theta from the z-axis normal, in the x-z plane
        const auto e_D = must_dir(std::sin(theta), 0.0, std::cos(theta));
        auto f = srp_force(model, e_D);
        REQUIRE(f.has_value());

        const double P = kS0 / kC;
        const double expected_mag = P * kArea * std::cos(theta);
        const Vec3 expected_dir{-e_D.vec().x, -e_D.vec().y, -e_D.vec().z};   // exactly -e_D

        const double mag_rel = std::abs(f->norm() - expected_mag) / expected_mag;
        const Vec3 got_dir = (1.0 / f->norm()) * (*f);
        const double dir_dev = (got_dir - expected_dir).norm();
        worst_mag_rel = std::max(worst_mag_rel, mag_rel);
        worst_dir_dev = std::max(worst_dir_dev, dir_dev);
        ++checked;
    }
    INFO("checked " << checked << " oblique incidence angles; worst magnitude relative error "
         << worst_mag_rel << "; worst direction deviation from exactly -e_D: " << worst_dir_dev);
    CHECK(checked == 5);
    CHECK(worst_mag_rel < 1.0e-12);
    CHECK(worst_dir_dev < 1.0e-12);
}

TEST_CASE("SRPA-A-012  pure specular reflector at oblique incidence: force exactly "
          "2*P*A*cos^2(theta) along -e_N", "[srp_analytic][gate]") {
    constexpr double kArea = 2.1;
    const Triple mirror{0.0, 1.0, 0.0};
    const Vec3 n_vec{0.0, 0.0, 1.0};
    const auto normal = must_dir(n_vec.x, n_vec.y, n_vec.z);
    auto area = cited(kArea, "test-stated");
    auto al = cited(mirror.alpha, "test-stated");
    auto rh = cited(mirror.rho, "test-stated");
    auto de = cited(mirror.delta, "test-stated");
    REQUIRE(area.has_value());
    auto fs = flat_surface_body_fixed(*area, normal, *al, *rh, *de);
    REQUIRE(fs.has_value());
    auto model = many_surfaces({Surface{*fs}});

    int checked = 0;
    double worst_mag_rel = 0.0, worst_dir_dev = 0.0;
    for (double theta_deg : {15.0, 30.0, 45.0, 60.0, 75.0}) {
        const double theta = theta_deg * std::numbers::pi / 180.0;
        const auto e_D = must_dir(std::sin(theta), 0.0, std::cos(theta));
        auto f = srp_force(model, e_D);
        REQUIRE(f.has_value());

        const double P = kS0 / kC;
        const double expected_mag = 2.0 * P * kArea * std::cos(theta) * std::cos(theta);
        const Vec3 expected_dir{-n_vec.x, -n_vec.y, -n_vec.z};   // exactly -e_N, NOT -e_D

        const double mag_rel = std::abs(f->norm() - expected_mag) / expected_mag;
        const Vec3 got_dir = (1.0 / f->norm()) * (*f);
        const double dir_dev = (got_dir - expected_dir).norm();
        worst_mag_rel = std::max(worst_mag_rel, mag_rel);
        worst_dir_dev = std::max(worst_dir_dev, dir_dev);
        ++checked;
    }
    INFO("checked " << checked << " oblique incidence angles; worst magnitude relative error "
         << worst_mag_rel << "; worst direction deviation from exactly -e_N: " << worst_dir_dev);
    CHECK(checked == 5);
    CHECK(worst_mag_rel < 1.0e-12);
    CHECK(worst_dir_dev < 1.0e-12);
}

namespace {

/// A latitude/longitude tessellation of a unit sphere into n*(2n) body-fixed
/// FlatSurfaces, each normal at its cell's centre and each area its cell's
/// EXACT solid angle -- SRPA-P-3's own scheme, whose discretisation error was
/// measured (not assumed) before this test was written.
Macromodel tessellated_sphere(int n_theta, const Triple& t) {
    std::vector<Surface> facets;
    facets.reserve(static_cast<std::size_t>(n_theta) * static_cast<std::size_t>(2 * n_theta));
    const int n_phi = 2 * n_theta;
    for (int i = 0; i < n_theta; ++i) {
        const double theta_lo = std::numbers::pi * i / n_theta;
        const double theta_hi = std::numbers::pi * (i + 1) / n_theta;
        const double theta_c = 0.5 * (theta_lo + theta_hi);
        const double dphi = 2.0 * std::numbers::pi / n_phi;
        const double cell_area = (std::cos(theta_lo) - std::cos(theta_hi)) * dphi;   // exact solid angle
        for (int j = 0; j < n_phi; ++j) {
            const double phi_c = (j + 0.5) * dphi;
            const Vec3 n{std::sin(theta_c) * std::cos(phi_c), std::sin(theta_c) * std::sin(phi_c),
                        std::cos(theta_c)};
            auto normal = body_direction(n);
            REQUIRE(normal.has_value());
            auto area = cited(cell_area, "test-stated: exact solid angle of this lat/lon cell");
            auto al = cited(t.alpha, "test-stated");
            auto rh = cited(t.rho, "test-stated");
            auto de = cited(t.delta, "test-stated");
            REQUIRE(area.has_value());
            auto fs = flat_surface_body_fixed(*area, *normal, *al, *rh, *de);
            REQUIRE(fs.has_value());
            facets.push_back(Surface{*fs});
        }
    }
    return many_surfaces(std::move(facets));
}

}  // namespace

TEST_CASE("SRPA-A-013  the tessellated-sphere cross-check, against SRPA-P-3's pre-registered "
          "convergence prediction", "[srp_analytic][gate]") {
    // SRPA-P-3: empirically second-order in n_theta, measured BEFORE this test
    // was written, at n_theta up to 128. The gate uses n=32 and n=64, well
    // inside the range the prediction was checked over, and reasserts the
    // convergence RATIO rather than trusting either resolution alone -- a bug
    // giving the specular term the wrong power of cos(theta) would move BOTH
    // resolutions' error to ~33% and the ratio check would still catch it even
    // if, by coincidence, one resolution's absolute error passed.
    const auto e_D = must_dir(0.0, 0.0, 1.0);
    struct Case { Triple t; double closed_form; const char* label; };
    const Case cases[] = {
        {{0.0, 1.0, 0.0}, 1.0, "pure specular (sharpest: no rho cancellation cushion)"},
        {{0.0, 0.0, 1.0}, 1.0 + 4.0 / 9.0, "pure diffuse"},
        {{0.2, 0.5, 0.3}, 1.0 + 4.0 * 0.3 / 9.0, "mixed"},
    };

    int checked = 0;
    for (const auto& c : cases) {
        auto model_32 = tessellated_sphere(32, c.t);
        auto model_64 = tessellated_sphere(64, c.t);
        auto f_32 = srp_force(model_32, e_D);
        auto f_64 = srp_force(model_64, e_D);
        REQUIRE(f_32.has_value());
        REQUIRE(f_64.has_value());

        const double coeff_32 = f_32->norm() / (kS0 / kC * std::numbers::pi);   // /(P * pi*r^2), r=1
        const double coeff_64 = f_64->norm() / (kS0 / kC * std::numbers::pi);
        const double err_32 = std::abs(coeff_32 - c.closed_form) / c.closed_form;
        const double err_64 = std::abs(coeff_64 - c.closed_form) / c.closed_form;
        const double ratio = err_32 / err_64;

        INFO(c.label << ": closed form " << c.closed_form << ", n=32 coeff " << coeff_32
             << " (err " << err_32 << "), n=64 coeff " << coeff_64 << " (err " << err_64
             << "), ratio " << ratio << " [SRPA-P-3 predicts ~4.0]");
        CHECK(err_32 < 5.0e-3);
        CHECK(err_64 < 1.5e-3);
        CHECK(ratio > 3.0);
        CHECK(ratio < 5.0);
        // direction check: the tessellated force must point along -e_D, same
        // as the smooth sphere's (SRPA-R-005) -- a swapped e_D/e_N term would
        // fail this even if some accidental magnitude cancellation hid in the
        // coefficient comparison above.
        const double dir_cos = f_32->dot(e_D.vec()) / f_32->norm();
        CHECK_THAT(dir_cos, Catch::Matchers::WithinAbs(-1.0, 1.0e-9));
        ++checked;
    }
    INFO("checked " << checked << " optical triples, each at two tessellation resolutions");
    CHECK(checked == 3);
}

// ---------------------------------------------------------------------------
// PHPR-A-001 (SPEC-photon-pressure §4.1, §8): srp_force's bit-identity
// through the photon_force kernel refactor -- against a value CAPTURED FROM
// CODE THAT NO LONGER EXISTS IN THIS TREE, not against a live call to the
// same wrapper being tested. The distinction matters and was caught in
// review, not found here first: "the old tests still pass" only proves
// agreement within THEIR tolerances (1e-12 relative and similar), which a
// reordered floating-point expression -- (A*I/c)*coeff*cos(theta) instead of
// A*(I*cos(theta)/c)*coeff, say -- would still satisfy while changing the
// last bit; and comparing srp_force against photon_force(model, 1367,
// visible, d, d, {0,0,0}) is comparing srp_force's own wrapper body against
// itself, which cannot fail regardless of what the underlying maths does
// (the shadow module's own configuration-cannot-fail-to-produce shape, one
// layer up).
//
// PROVEN BY INJECTION (plan §4 rule 5), not merely argued. flat_force's own
// return statement, `-(prefactor * cos_theta) * bracket`, was changed to
// `(-prefactor) * (cos_theta * bracket)` -- the same value, reassociated --
// rebuilt, and run: 42 of the 400 golden cases (61 of 3951 assertions)
// failed, every one at exactly the last representable bit (confirmed
// separately: `(A*I/c)` vs `A*(I/c)` alone, the more obvious reassociation,
// turned out NOT to diverge for this suite's own area/triple values --
// checked directly rather than assumed, before this one was tried and did).
// Reverted; the suite returned to 96587/13, clean. A test that cannot be
// made to fail by a real, small, realistic error is not a test of that
// error; this one now is, and this paragraph is where that was checked.
// srp_force_golden.hpp's own header records exactly how its 400
// values were captured and from which commit.
TEST_CASE("PHPR-A-001  srp_force is bit-identical to its own pre-refactor "
          "output, against a captured golden file, not a live tautology",
          "[srp_analytic][gate]") {
    using odl::srp_analytic::test::kSrpForceGolden;

    const std::array<double, 3> dirs[] = {
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0},
        {-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, -1.0},
        {0.577350269189626, 0.577350269189626, 0.577350269189626},
        {0.3, 0.3, std::sqrt(1.0 - 0.09 - 0.09)},
        {0.8, 0.1, std::sqrt(1.0 - 0.64 - 0.01)},
        {0.1, -0.6, std::sqrt(1.0 - 0.01 - 0.36)},
    };

    // BIT identity, not value identity: `==` treats -0.0 and +0.0 as equal,
    // so it would silently pass a reordering that flipped a zero's own sign
    // -- no force component's physics turns on that sign today, but the
    // claim this test makes is "the same bits", and bit_cast is what makes
    // that claim exactly true rather than true except at one edge.
    auto same_bits = [](double a, double b) {
        return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
    };

    std::size_t i = 0;
    auto check_one = [&](const Macromodel& model, const BodyDirection& dir) {
        REQUIRE(i < kSrpForceGolden.size());
        const auto& g = kSrpForceGolden[i];
        auto r = srp_force(model, dir);
        REQUIRE(r.has_value());
        INFO("case " << i << ": " << g.label);
        // Bit-for-bit -- this IS the point of the test (PHPR-A-001):
        // anything less would let a reordered expression through, which is
        // precisely the hazard this test exists to catch.
        CHECK(same_bits(r->x, g.x));
        CHECK(same_bits(r->y, g.y));
        CHECK(same_bits(r->z, g.z));
        ++i;
    };

    for (const auto& t : kTriples) {
        for (const auto& d : dirs) {
            auto dir = must_dir(d[0], d[1], d[2]);
            check_one(one_sphere(1.0, t), dir);
            check_one(one_flat_body_fixed_z(1.0, t), dir);
            check_one(one_sun_pointing_flat(1.0, t), dir);
        }
    }
    for (const auto& bus_t : kTriples) {
        for (const auto& panel_t : kTriples) {
            for (const auto& d : dirs) {
                auto dir = must_dir(d[0], d[1], d[2]);
                check_one(phpr_box_wing(bus_t, panel_t), dir);
            }
        }
    }
    REQUIRE(i == kSrpForceGolden.size());
}

namespace {

/// The area/mass this row's own three geometries share -- only the front/
/// back triples and the source direction vary between them.
constexpr double kA16AreaM2 = 1.0;

Vec3 photon_force_of(const Macromodel& model, Band band, const Vec3& source_dir) {
    auto irr = irradiance_w_per_m2(kS0);
    REQUIRE(irr.has_value());
    auto dir = must_dir(source_dir.x, source_dir.y, source_dir.z);
    auto f = photon_force(model, *irr, band, dir, dir, Vec3{0.0, 0.0, 0.0});
    REQUIRE(f.has_value());
    return *f;
}

}  // namespace

TEST_CASE("PHPR-A-016  PHPR-R-004a's two axes, face and band, checked separately then "
          "together",
          "[srp_analytic][gate]") {
    auto area = cited(kA16AreaM2, "test-stated");
    REQUIRE(area.has_value());
    auto normal = must_dir(0.0, 0.0, 1.0);   // front faces +z

    SECTION("(1) face: a two-sided surface lit from behind receives a force through the "
            "back triple; the same geometry on a one-sided surface receives nothing") {
        // Energy-conserving (MCRM-F-007); the front triple is never actually
        // read in this section (the source is BEHIND the front, cos_theta<0
        // zeroes it regardless of its own optics), so this choice changes
        // nothing this section's own claim depends on.
        auto front_a = cited(0.8, "front"), front_r = cited(0.1, "front"),
            front_d = cited(0.1, "front");
        auto back_a = cited(0.9, "back"), back_r = cited(0.05, "back"), back_d = cited(0.05, "back");
        REQUIRE(front_a.has_value()); REQUIRE(back_a.has_value());

        BandedOptics back{OpticalTriple{*back_a, *back_r, *back_d}, std::nullopt};
        auto two_sided = flat_surface_body_fixed(*area, normal, *front_a, *front_r, *front_d,
                                                  std::nullopt, back);
        REQUIRE(two_sided.has_value());
        auto one_sided = flat_surface_body_fixed(*area, normal, *front_a, *front_r, *front_d);
        REQUIRE(one_sided.has_value());

        MacromodelBuilder tb, ob;
        auto mass = cited(1.0, "test-stated");
        auto com_v = cited(Vec3{0, 0, 0}, "test-stated");
        REQUIRE(mass.has_value()); REQUIRE(com_v.has_value());
        tb.add_surface(*two_sided).set_mass(*mass).set_centre_of_mass(*com_v);
        ob.add_surface(*one_sided).set_mass(*mass).set_centre_of_mass(*com_v);
        auto two_sided_model = std::move(tb).build();
        auto one_sided_model = std::move(ob).build();
        REQUIRE(two_sided_model.has_value());
        REQUIRE(one_sided_model.has_value());

        // Source BEHIND the front normal: cos(theta) < 0 on the front.
        const Vec3 source_behind{0.0, 0.0, -1.0};
        const Vec3 f_two = photon_force_of(*two_sided_model, Band::visible, source_behind);
        const Vec3 f_one = photon_force_of(*one_sided_model, Band::visible, source_behind);
        INFO("two-sided |F| = " << f_two.norm() << ", one-sided |F| = " << f_one.norm());
        CHECK(f_two.norm() > 0.0);            // the back caught it
        CHECK(f_one.x == 0.0); CHECK(f_one.y == 0.0); CHECK(f_one.z == 0.0);   // unchanged: still unlit
    }

    SECTION("(2) band: a surface with a stated infrared triple different from its visible "
            "one gives a measurably different result under Band::infrared; a surface with "
            "none gives the SAME result under both bands") {
        // rho/delta, not alpha, are what flat_force actually reads (SRPA-R-001's
        // own two coefficients) -- a big swing needs to be in THOSE, not alpha.
        auto vis_a = cited(0.8, "vis"), vis_r = cited(0.1, "vis"), vis_d = cited(0.1, "vis");
        auto ir_a = cited(0.05, "ir"), ir_r = cited(0.9, "ir"), ir_d = cited(0.05, "ir");
        REQUIRE(vis_a.has_value()); REQUIRE(ir_a.has_value());

        OpticalTriple ir_triple{*ir_a, *ir_r, *ir_d};
        auto with_ir = flat_surface_body_fixed(*area, normal, *vis_a, *vis_r, *vis_d, ir_triple);
        REQUIRE(with_ir.has_value());
        auto without_ir = flat_surface_body_fixed(*area, normal, *vis_a, *vis_r, *vis_d);
        REQUIRE(without_ir.has_value());

        MacromodelBuilder wb, nb;
        auto mass = cited(1.0, "test-stated");
        auto com_v = cited(Vec3{0, 0, 0}, "test-stated");
        REQUIRE(mass.has_value()); REQUIRE(com_v.has_value());
        wb.add_surface(*with_ir).set_mass(*mass).set_centre_of_mass(*com_v);
        nb.add_surface(*without_ir).set_mass(*mass).set_centre_of_mass(*com_v);
        auto with_ir_model = std::move(wb).build();
        auto without_ir_model = std::move(nb).build();
        REQUIRE(with_ir_model.has_value());
        REQUIRE(without_ir_model.has_value());

        const Vec3 source_front{0.0, 0.0, 1.0};
        const Vec3 with_vis = photon_force_of(*with_ir_model, Band::visible, source_front);
        const Vec3 with_infra = photon_force_of(*with_ir_model, Band::infrared, source_front);
        INFO("with-triple: visible |F| = " << with_vis.norm() << ", infrared |F| = "
             << with_infra.norm());
        CHECK((with_infra - with_vis).norm() / with_vis.norm() > 0.1);   // measurably different

        const Vec3 without_vis = photon_force_of(*without_ir_model, Band::visible, source_front);
        const Vec3 without_infra = photon_force_of(*without_ir_model, Band::infrared, source_front);
        CHECK(without_vis.x == without_infra.x);   // exact fall-back, not merely close
        CHECK(without_vis.y == without_infra.y);
        CHECK(without_vis.z == without_infra.z);
    }

    SECTION("(3) both together: a two-sided surface with band-differing BACK optics only -- "
            "the front reads identically in both bands, the back does not") {
        // Energy-conserving (MCRM-F-007); the equality this section checks
        // (front_vis == front_ir, an exact fall-back) holds for any front
        // triple, so this choice changes nothing the section depends on.
        auto front_a = cited(0.8, "front"), front_r = cited(0.1, "front"),
            front_d = cited(0.1, "front");
        auto back_vis_a = cited(0.8, "back-vis"), back_vis_r = cited(0.1, "back-vis"),
            back_vis_d = cited(0.1, "back-vis");
        auto back_ir_a = cited(0.05, "back-ir"), back_ir_r = cited(0.9, "back-ir"),
            back_ir_d = cited(0.05, "back-ir");
        REQUIRE(front_a.has_value()); REQUIRE(back_vis_a.has_value()); REQUIRE(back_ir_a.has_value());

        BandedOptics back{OpticalTriple{*back_vis_a, *back_vis_r, *back_vis_d},
                          OpticalTriple{*back_ir_a, *back_ir_r, *back_ir_d}};
        // front_infrared left absent: the front's own visible/infrared MUST agree.
        auto surf = flat_surface_body_fixed(*area, normal, *front_a, *front_r, *front_d,
                                            std::nullopt, back);
        REQUIRE(surf.has_value());
        MacromodelBuilder b;
        auto mass = cited(1.0, "test-stated");
        auto com_v = cited(Vec3{0, 0, 0}, "test-stated");
        REQUIRE(mass.has_value()); REQUIRE(com_v.has_value());
        b.add_surface(*surf).set_mass(*mass).set_centre_of_mass(*com_v);
        auto model = std::move(b).build();
        REQUIRE(model.has_value());

        const Vec3 front_vis = photon_force_of(*model, Band::visible, Vec3{0.0, 0.0, 1.0});
        const Vec3 front_ir = photon_force_of(*model, Band::infrared, Vec3{0.0, 0.0, 1.0});
        CHECK(front_vis.x == front_ir.x);   // front: no infrared triple stated, exact fall-back
        CHECK(front_vis.y == front_ir.y);
        CHECK(front_vis.z == front_ir.z);

        const Vec3 back_vis = photon_force_of(*model, Band::visible, Vec3{0.0, 0.0, -1.0});
        const Vec3 back_ir = photon_force_of(*model, Band::infrared, Vec3{0.0, 0.0, -1.0});
        INFO("back: visible |F| = " << back_vis.norm() << ", infrared |F| = " << back_ir.norm());
        CHECK((back_ir - back_vis).norm() / back_vis.norm() > 0.1);   // back: stated, differs
    }
}
