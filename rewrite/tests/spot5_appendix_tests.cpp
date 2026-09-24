// spot5_appendix_tests.cpp — SPEC-srp-analytic.md SRPA-A-014..A-013, cross-
// module (odl::spacecraft + odl::srp_analytic), the same "linked here on
// purpose" reasoning `tests/l2_floors.cpp` states for its own three
// modules (L5 step 4's own rule-2 check: reproducing a published, source-
// provided numerical SRP example through this tree's own real kernel).
//
// Source: CNES, "DORIS satellites models implemented in POE processing,"
// SALP-NT-BORD-OP-16137-CN, Ed.1/Rev.20 (2026-09-09), Appendix 1 ("EXAMPLE
// OF COMPUTATION OF THE SOLAR RADIATION PRESSURE"), fetched directly
// 2026-09-24, SHA256
// c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619 (the
// SAME edition `modules/spacecraft`'s own `sentinel6.cpp`/`jason.cpp`
// cite). The appendix's own worked example is for SPOT-5's bus (NOT
// Sentinel-6 -- the manager's own ruling names this "the first published
// case in this tree that runs a real macromodel through the photon-
// pressure kernel," and SPOT-5 is not part of this tree's own constellation
// family, so its table is built HERE ONLY, not exposed from
// `odl::spacecraft`).
//
// THE APPENDIX'S OWN FORMULA IS NOT `srp_analytic::flat_force`'S FORMULA IN
// GENERAL -- a finding, not an oversight, worked out in full below (this
// file's own `general_srp_force`) and proved against SPOT-5's own 20
// printed test vectors before being trusted (SRPA-A-014). The appendix's
// own equation (quoted, OCR-flattened in the source's own text but
// re-derived from first principles and independently confirmed numerically,
// not trusted from the flattened rendering alone):
//
//   a = sum_i A_i (u.n_i) [ 2*Ks_i*(u.n_i)*n_i + Kd_i*(u.n_i + 2/3*n_i) + Ka_i*u ]
//
// where u is the direction FROM THE SATELLITE TOWARD THE SOURCE (this
// tree's own e_D convention EXACTLY, confirmed numerically below -- the
// appendix's own prose states the opposite, "positive from source to
// satellite," but the 20 printed test vectors settle it unambiguously,
// PROVENANCE.md's own L5 step 4 section), Ks/Kd/Ka the specular/diffuse/
// absorbed coefficients. Re-derived from first principles (radiation-
// momentum bookkeeping: absorbed photons transfer their full incident
// momentum; specularly reflected photons transfer twice the normal
// component; diffusely (Lambertian) reflected photons transfer their own
// incident momentum along u minus a mean 2/3 factor along the normal --
// this file's own derivation, not copied from RHS12) gives EXACTLY:
//
//   a = -A*(e_D.n) * [ (alpha+delta)*e_D + (2*delta/3 + 2*rho*(e_D.n))*n ]
//
// which is `srp_analytic::flat_force`'s OWN formula -- `(1-rho)*e_D +
// 2*(delta/3+rho*cos_theta)*n` -- if and ONLY if alpha+delta = 1-rho, i.e.
// alpha+rho+delta = 1 (energy conservation). SPOT-5's own Appendix-1 table
// does NOT conserve energy (its own six rows sum to 0.499-0.912, checked
// directly, SRPA-A-015).
//
// THIS FINDING IS NOW A GUARANTEED SCHEMA GUARD, NOT MERELY A REPORTED GAP
// (the manager's own review of this file's first version): `flat_surface_
// body_fixed`/`flat_surface_sun_pointing` (`modules/macromodel/include/odl/
// macromodel/macromodel.hpp`, MCRM-F-007) now refuse any triple more than
// 1% short of energy conservation, so a non-conserving macromodel (SPOT-5's
// own table, or a future one shaped like it) cannot reach `photon_force`
// silently -- SRPA-A-015 now proves the REFUSAL fires for every one of
// SPOT-5's six rows, through the exact factory call a real spacecraft-data
// file uses, rather than showing the kernel's own mismatched output (which
// is no longer reachable to demonstrate this way, since the data can no
// longer be built at all -- SRPA-A-015's own header comment keeps the
// original hand-worked-out numbers as the record of why the guard exists).
// Sentinel-6's OWN real macromodel (`sentinel6()`, `modules/spacecraft`)
// DOES conserve energy on every row (checked, `SPCR-A-026`) -- so for
// Sentinel-6, the two formulas are IDENTICAL, and SRPA-A-016 is the
// genuine, positive "real macromodel through the real kernel" deliverable,
// checked against `general_srp_force` applied to Sentinel-6's own data at
// the SAME 40-point (azimuth, elevation) sweep the appendix itself
// demonstrates. Together, SRPA-A-014 (the formula against the published
// case), SRPA-A-015 (the guard keeping non-conserving data out) and
// SRPA-A-016 (the real kernel against the formula, within its own scope)
// are how this tree validates `photon_force` against a published example:
// through the formula, within the scope the formula and the guard both
// state -- and no more than that (`SPEC-photon-pressure.md` §4.1).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/macromodel/irradiance.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/sentinel6.hpp>
#include <odl/srp_analytic/srp_analytic.hpp>

#include <cmath>
#include <vector>

using namespace odl;
using namespace odl::macromodel;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

/// Appendix 1's own words: "the above numbers have to be multiplied by
/// (1/(M*c))*W... to obtain the modelled solar radiation pressure
/// acceleration" -- i.e. the printed (ax,ay,az) columns are `area *
/// bracket` ALONE, with NEITHER the irradiance NOR the 1/c factor
/// `srp_analytic::flat_force`'s own `prefactor = area*irradiance/c`
/// applies. Passing THIS value as the kernel's own irradiance makes
/// `prefactor = area*c/c = area` exactly -- the unique choice that strips
/// the kernel's own physical scaling back to the appendix's own
/// dimensionless-in-irradiance convention, not an arbitrary large number.
/// Exact, by definition of the metre since 1983 (the same constant
/// `srp_analytic.cpp`'s own header comment names).
constexpr double kSpeedOfLightMPerS = 299792458.0;

struct RawFace {
    double area_m2;
    Vec3 normal;      // unit, "in sat ref frame"
    double alpha, rho, delta;   // absorbed, specular, diffuse -- AS PRINTED, not required to sum to 1
};

/// Appendix 1's own SPOT-5 table (6 body rows, panel excluded -- the
/// appendix's own words, "the SPOT-5 main body, solar panel excluded").
const std::vector<RawFace> kSpot5 = {
    {7.21,  Vec3{ 1.0, 0.0, 0.0}, -0.108, 0.3460, 0.2610},
    {7.21,  Vec3{-1.0, 0.0, 0.0},  0.394, 0.1610, 0.0510},
    {10.79, Vec3{ 0.0,-1.0, 0.0},  0.047, 0.4750, 0.3680},
    {10.79, Vec3{ 0.0, 1.0, 0.0},  0.071, 0.4570, 0.3660},
    {11.79, Vec3{ 0.0, 0.0, 1.0},  0.341, 0.3700, 0.2010},
    {11.79, Vec3{ 0.0, 0.0,-1.0},  0.240, 0.3930, 0.2620},
};

/// The appendix's own 20 registered (az, el) -> (ax, ay, az) triples,
/// Appendix 1's own printed table, quoted exactly.
struct ExampleRow { double az_deg, el_deg, ax, ay, az; };
const std::vector<ExampleRow> kExamples = {
    {0.0, -90.0, -0.000, 0.000, 17.245},   {0.0, -45.0, -6.893, 0.000, 9.600},
    {0.0, 0.0, -7.347, 0.000, 0.000},      {0.0, 45.0, -7.128, 0.000, -9.226},
    {0.0, 90.0, -0.000, 0.000, -16.695},
    {45.0, -90.0, -0.000, -0.000, 17.245}, {45.0, -45.0, -5.422, -7.329, 11.106},
    {45.0, 0.0, -6.291, -9.702, 0.000},    {45.0, 45.0, -5.588, -7.496, -10.732},
    {45.0, 90.0, -0.000, -0.000, -16.695},
    {90.0, -90.0, -0.000, -0.000, 17.245}, {90.0, -45.0, -0.000, -12.110, 11.407},
    {90.0, 0.0, -0.000, -17.210, 0.000},   {90.0, 45.0, -0.000, -12.345, -11.032},
    {90.0, 90.0, -0.000, -0.000, -16.695},
    {135.0, -90.0, 0.000, -0.000, 17.245}, {135.0, -45.0, 4.776, -7.855, 11.850},
    {135.0, 0.0, 5.296, -10.755, 0.000},   {135.0, 45.0, 4.943, -8.022, -11.476},
    {135.0, 90.0, 0.000, -0.000, -16.695},
    {180.0, -90.0, 0.000, -0.000, 17.245}, {180.0, -45.0, 5.898, -0.000, 10.653},
    {180.0, 0.0, 5.775, -0.000, 0.000},    {180.0, 45.0, 6.133, -0.000, -10.279},
    {180.0, 90.0, 0.000, -0.000, -16.695},
    {225.0, -90.0, 0.000, 0.000, 17.245},  {225.0, -45.0, 4.717, 7.900, 11.766},
    {225.0, 0.0, 5.177, 10.840, 0.000},    {225.0, 45.0, 4.884, 8.067, -11.392},
    {225.0, 90.0, 0.000, 0.000, -16.695},
    {270.0, -90.0, 0.000, 0.000, 17.245},  {270.0, -45.0, 0.000, 12.195, 11.288},
    {270.0, 0.0, 0.000, 17.375, 0.000},    {270.0, 45.0, 0.000, 12.431, -10.913},
    {270.0, 90.0, 0.000, 0.000, -16.695},
    {315.0, -90.0, -0.000, 0.000, 17.245}, {315.0, -45.0, -5.362, 7.374, 11.022},
    {315.0, 0.0, -6.172, 9.788, 0.000},    {315.0, 45.0, -5.529, 7.541, -10.648},
    {315.0, 90.0, -0.000, 0.000, -16.695},
};

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

[[nodiscard]] Vec3 u_from_az_el(double az_deg, double el_deg) noexcept {
    const double az = az_deg * kDegToRad, el = el_deg * kDegToRad;
    return Vec3{std::cos(el) * std::cos(az), std::cos(el) * std::sin(az), std::sin(el)};
}

/// This file's own independently-derived general formula (this file's own
/// header comment) -- NOT a call into `srp_analytic`, so it is a genuine,
/// separate check of that module's own kernel, not a tautology (plan §4
/// rule 5). `e_D` = direction from satellite TOWARD the source.
[[nodiscard]] Vec3 general_srp_force(const std::vector<RawFace>& faces, const Vec3& e_D) noexcept {
    Vec3 total{0.0, 0.0, 0.0};
    for (const RawFace& f : faces) {
        const double cos_theta = e_D.dot(f.normal);
        if (cos_theta <= 0.0) continue;
        const Vec3 bracket = (f.alpha + f.delta) * cos_theta * e_D +
                             (2.0 * (f.delta / 3.0 + f.rho * cos_theta)) * cos_theta * f.normal;
        total = total + (-f.area_m2) * bracket;
    }
    return total;
}

}  // namespace

// --- SRPA-A-014: the independently-derived formula matches all 20 --------

TEST_CASE("SRPA-A-014  general_srp_force, independently re-derived from "
          "radiation-momentum first principles (this file's own header "
          "comment, NOT copied from RHS12 or srp_analytic's own formula), "
          "reproduces ALL 20 of Appendix 1's own printed SPOT-5 test "
          "vectors to the precision they are printed",
          "[srp_analytic][spot5][gate]") {
    for (const ExampleRow& ex : kExamples) {
        const Vec3 e_D = u_from_az_el(ex.az_deg, ex.el_deg);
        const Vec3 got = general_srp_force(kSpot5, e_D);
        // The source prints its own output to 3 decimals; half that last
        // digit (0.0005) is the tightest honest tolerance a re-derivation
        // from the SAME 3-4 decimal input table can be held to.
        CHECK_THAT(got.x, WithinAbs(ex.ax, 5.0e-4));
        CHECK_THAT(got.y, WithinAbs(ex.ay, 5.0e-4));
        CHECK_THAT(got.z, WithinAbs(ex.az, 5.0e-4));
    }
}

// --- SRPA-A-015: SPOT-5 does not energy-conserve; MCRM-F-007 refuses it ----
//
// FIRST DRAFT OF THIS TEST (kept here as the record, not the executable
// claim any more): built SPOT-5's own non-conserving table through
// `flat_surface_body_fixed` directly, then showed the REAL kernel's own
// output ("-10.9592") did NOT match the appendix's own printed "-7.347" --
// a genuine ~49% gap, worked out by hand from `flat_force`'s own published
// formula before running it, matching the row's own energy gap
// (1-0.499=0.501) exactly. That finding is what led the manager to rule
// (this round's own review) that the SCHEMA itself must refuse a
// non-conserving surface (MCRM-F-007, `modules/macromodel/include/odl/
// macromodel/macromodel.hpp`) -- built in response, and it now refuses
// EVERY row of SPOT-5's own table below, which is what this test checks
// instead: not that the kernel mismatches non-conserving data (still true,
// but no longer reachable to demonstrate this way, since the data can no
// longer be built at all), but that IT CANNOT REACH THE KERNEL IN THE
// FIRST PLACE.

TEST_CASE("SRPA-A-015  SPOT-5's own six rows do NOT sum to 1 (checked "
          "directly, 0.499-0.912) -- and MCRM-F-007 now refuses every one "
          "of them at `flat_surface_body_fixed` itself, PROVING a non-"
          "conserving macromodel cannot reach the real "
          "srp_analytic::photon_force kernel silently, through the exact "
          "construction path a real caller would use (rule 5)",
          "[srp_analytic][spot5][gate]") {
    int refused = 0;
    for (const RawFace& f : kSpot5) {
        const double sum = f.alpha + f.rho + f.delta;
        CHECK(sum < 0.999);  // every row genuinely under 1, none accidentally conserving

        auto area = cited(f.area_m2, "Appendix 1, SPOT-5 table (test-local, not a tree constellation)");
        auto a = cited(f.alpha, "Appendix 1");
        auto s = cited(f.rho, "Appendix 1");
        auto d = cited(f.delta, "Appendix 1");
        REQUIRE(area.has_value());
        REQUIRE(a.has_value());
        REQUIRE(s.has_value());
        REQUIRE(d.has_value());
        auto n = body_direction(f.normal);
        REQUIRE(n.has_value());

        // The SAME call a real spacecraft-data file makes (jason.cpp's own
        // `build_row`, sentinel6.cpp's own `build_row`) -- not a bespoke
        // bypass of the schema's own factory.
        auto surf = flat_surface_body_fixed(*area, *n, *a, *s, *d);
        if (!surf.has_value()) {
            CHECK(surf.error().id == "MCRM-F-007");
            ++refused;
        }
    }
    // Every one of SPOT-5's six rows is refused -- none happens to sneak
    // through under the 1% tolerance (the closest, row 5, is 8.8% short).
    CHECK(refused == 6);

    // A genuinely CONSERVING row, adjacent to the six refused ones, is NOT
    // refused -- the guard catches the real defect, not merely every input.
    auto area = cited(7.21, "test-stated, conserving");
    auto a = cited(0.610, "test-stated"), s = cited(0.349, "test-stated"),
        d = cited(0.041, "test-stated");  // matches Sentinel-6's own +X row, sums to 1.000
    REQUIRE(area.has_value()); REQUIRE(a.has_value()); REQUIRE(s.has_value()); REQUIRE(d.has_value());
    auto n = body_direction(Vec3{1.0, 0.0, 0.0});
    REQUIRE(n.has_value());
    auto conserving = flat_surface_body_fixed(*area, *n, *a, *s, *d);
    CHECK(conserving.has_value());
}

// --- SRPA-A-016: Sentinel-6's REAL macromodel through the REAL kernel -----

TEST_CASE("SRPA-A-016  Sentinel-6's own REAL macromodel (sentinel6(), "
          "energy-conserving on every row, SPCR-A-026), run through the "
          "REAL srp_analytic::photon_force kernel, matches "
          "general_srp_force exactly at the SAME 40-point (az, el) sweep "
          "Appendix 1 itself demonstrates -- the genuine, positive 'real "
          "macromodel through the real kernel' deliverable the manager's "
          "own ruling asked for",
          "[srp_analytic][spot5][sentinel6][gate]") {
    auto model = odl::spacecraft::sentinel6();
    REQUIRE(model.has_value());

    // Build the SAME 12-row RawFace table sentinel6.cpp itself builds, for
    // this file's own INDEPENDENT general_srp_force -- read from the SAME
    // source table (Sec.16.3), not derived from the built Macromodel (which
    // would make this a tautology, plan §4 rule 5).
    const std::vector<RawFace> s6_faces = {
        {4.149,  Vec3{ 1.0, 0.0, 0.0}, 0.610, 0.349, 0.041},
        {3.941,  Vec3{-1.0, 0.0, 0.0}, 0.412, 0.546, 0.042},
        {11.83,  Vec3{ 0.0, 0.0, 1.0}, 0.413, 0.571, 0.016},
        {2.072,  Vec3{ 0.0, 0.0,-1.0}, 0.310, 0.660, 0.030},
        {8.65,   normalized(Vec3{0.0,  0.616, -0.788}), 0.545, 0.139, 0.316},
        {8.65,   normalized(Vec3{0.0, -0.616, -0.788}), 0.545, 0.139, 0.316},
        {3.76,   normalized(Vec3{0.0,  0.616,  0.788}), 0.823, 0.013, 0.164},
        {3.76,   normalized(Vec3{0.0, -0.616,  0.788}), 0.823, 0.013, 0.164},
        {1.329,  Vec3{ 0.0, 1.0, 0.0}, 0.454, 0.506, 0.040},
        {1.329,  Vec3{ 0.0,-1.0, 0.0}, 0.454, 0.506, 0.040},
        {0.92,   normalized(Vec3{0.469, 0.0, -0.833}), 0.920, 0.000, 0.080},
        {0.8123, Vec3{ 0.0, 0.0, 1.0}, 0.250, 0.190, 0.560},
    };
    for (const RawFace& f : s6_faces) {
        CHECK_THAT(f.alpha + f.rho + f.delta, WithinAbs(1.0, 1.0e-9));  // energy-conserving, unlike SPOT-5
    }

    auto irr = irradiance_w_per_m2(kSpeedOfLightMPerS);
    REQUIRE(irr.has_value());

    int checked = 0;
    for (double az : {0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0}) {
        for (double el : {-90.0, -45.0, 0.0, 45.0, 90.0}) {
            const Vec3 e_D = u_from_az_el(az, el);
            auto dir = body_direction(e_D);
            REQUIRE(dir.has_value());
            auto kernel_force = srp_analytic::photon_force(*model, *irr, Band::visible, *dir, *dir,
                                                            Vec3{0.0, 0.0, 0.0});
            REQUIRE(kernel_force.has_value());
            const Vec3 general = general_srp_force(s6_faces, e_D);
            CHECK_THAT(kernel_force->x, WithinAbs(general.x, 1.0e-9));
            CHECK_THAT(kernel_force->y, WithinAbs(general.y, 1.0e-9));
            CHECK_THAT(kernel_force->z, WithinAbs(general.z, 1.0e-9));
            ++checked;
        }
    }
    CHECK(checked == 40);
}
