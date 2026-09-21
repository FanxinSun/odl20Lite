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

#include <array>
#include <cmath>

using namespace odl;
using namespace odl::macromodel;
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
    SphericalSurface sph{*a, *al, *rh, *de};

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
