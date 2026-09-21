// macromodel.cpp — SPEC-macromodel §4.
//
// Two force laws, one per surface kind, summed.  Both are re-derived in the
// header comments next to the code that implements them, not only cited: plan
// rule 8's converse (an independent, closed-form derivation exists for each,
// so an implementation's legibility does not matter and none was sought).

#include <odl/macromodel/macromodel.hpp>

#include <cmath>

namespace odl::macromodel {
namespace {

/// RHS12 Eq. (6)'s own value, 1367 W/m^2.
inline constexpr double kSolarIrradianceAt1AuWPerM2 = 1367.0;
/// Exact, by definition of the metre since 1983.
inline constexpr double kSpeedOfLightMPerS = 299792458.0;

/// RHS12 Eq. (6), MCRM-R-005: the flat-surface force law, exactly as printed.
/// cos_theta < 0 (surface facing away from the Sun) contributes nothing --
/// stated in the equation's own domain, not bolted on as a special case.
Vec3 flat_force(double area_m2, double alpha, double rho, double delta,
                const Vec3& e_D, const Vec3& e_N) {
    (void)alpha;   // alpha does not appear in Eq. 6 directly -- only through
                    // rho and delta, since alpha+rho+delta need not be 1
                    // (SPEC-macromodel §3, RHS12 §4) and Eq. 6 is stated in
                    // rho and delta alone.
    const double cos_theta = e_D.dot(e_N);
    if (!(cos_theta > 0.0)) return Vec3{0.0, 0.0, 0.0};
    const double scale = -(area_m2 * kSolarIrradianceAt1AuWPerM2 / kSpeedOfLightMPerS) * cos_theta;
    const double e_D_coeff = (1.0 - rho);
    const double e_N_coeff = 2.0 * (delta / 3.0 + rho * cos_theta);
    return scale * (e_D_coeff * e_D + e_N_coeff * e_N);
}

/// MCRM-R-007: the spherical-surface force law, derived in SPEC-macromodel
/// §4.3 by integrating Eq. (6) over the illuminated hemisphere.  rho does not
/// appear -- MCRM-R-007's own point, and MCRM-A-002 asserts it stays absent.
Vec3 spherical_force(double area_m2, double delta, const Vec3& e_D) {
    const double coeff = 1.0 + 4.0 * delta / 9.0;
    return -(area_m2 * kSolarIrradianceAt1AuWPerM2 / kSpeedOfLightMPerS) * coeff * e_D;
}

}  // namespace

odl::Result<Vec3, MacromodelError>
srp_force(const Macromodel& model, const BodyDirection& sun_direction_body) {
    if (model.surfaces().empty())
        return odl::err(MacromodelError{"MCRM-F-004",
                         "srp_force called on a macromodel with zero surfaces: nothing to "
                         "evaluate"});

    const Vec3& e_D = sun_direction_body.vec();
    Vec3 total{0.0, 0.0, 0.0};
    for (const Surface& s : model.surfaces()) {
        if (const auto* flat = std::get_if<FlatSurface>(&s)) {
            const Vec3 e_N = (flat->normal_mode() == NormalMode::sun_pointing)
                                 ? e_D
                                 : flat->body_fixed_normal()->vec();
            total = total + flat_force(flat->area_m2().value(), flat->absorptivity().value(),
                                       flat->specular().value(), flat->diffuse().value(), e_D, e_N);
        } else {
            const auto& sph = std::get<SphericalSurface>(s);
            total = total + spherical_force(sph.cross_section_area_m2.value(), sph.diffuse.value(), e_D);
        }
    }
    return total;
}

}  // namespace odl::macromodel
