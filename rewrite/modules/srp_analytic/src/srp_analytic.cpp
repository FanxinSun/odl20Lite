// srp_analytic.cpp — SPEC-srp-analytic §4, generalised by SPEC-photon-pressure
// §4.1 into the shared kernel both SRP and ERP call.
//
// Relocated from odl::macromodel::macromodel.cpp (L4 step 2's review); the
// mathematics is unchanged, only its home.  Both force laws are re-derived in
// this file's header comments, not only cited: plan rule 8's converse (an
// independent, closed-form derivation exists for each, so RHS12's own
// legibility does not matter and none was sought).
//
// THE ABERRATION TERM (PHPR-R-010), BLS79 Eq. (5): m*v_dot = (S*A/c)*Q_pr*
// [(1 - rdot/c)*S_hat - v/c], S_hat the unit vector ALONG THE INCIDENT BEAM
// (source to particle).  This file's own e_D points the OTHER way (particle
// to source, so the steady force -coeff*e_D pushes AWAY from the source, as
// it must) -- S_hat = -e_D, and BLS79's own rdot = v.S_hat = -(v.e_D), so
// (1 - rdot/c) = (1 + (v.e_D)/c).  Substituting:
//
//   F = -(S*A/c)*Q_pr*[(1 + (v.e_D)/c)*e_D + v/c]
//
// which at v=0 is exactly the pre-existing steady force (PHPR-R-003's
// bit-identity), and whose two pieces are individually sensible: the Doppler
// factor enhances or reduces the steady pressure the way moving toward or
// away from the source must, and the "-v/c" piece is a force opposing v --
// Poynting-Robertson drag, the same direction regardless of e_D.
//
// SPHERE: Q_pr = 1 + 4*delta/9 (SRPA-R-003/R-005's own existing coefficient)
// IS Q_pr in BLS79's own sense: an isotropically-scattering sphere has
// Q_pr = Q_abs + Q_sca (BLS79's own stated case), and rho already does not
// enter a sphere's net force at all (SRPA-A-002), so the diffuse-only
// coefficient already computed is exactly this Q_pr, exact per BLS79.
//
// FLAT SURFACE: BLS79 treats one isotropic "particle", not a plate with two
// distinct directional responses (SRPA-R-001's e_D-weight (1-rho), e_N-weight
// 2*(delta/3+rho*cos_theta)).  The natural, minimal generalisation used here
// applies the SAME Doppler factor to the whole steady bracket (the incoming
// photon flux is enhanced by the same factor regardless of what happens to
// it after arriving) and a single combined drag term, using the STEADY
// force's own two coefficients summed as the drag's effective Q_pr -- stated
// as an approximation, not an independent flat-plate relativistic
// rederivation (none was found accessible: KVP10 derives the same
// normal-incidence case BLS79 already covers exactly, not the general
// oblique case).  It is EXACT at the one geometry this tree's own
// sun-tracking panels always have -- normal incidence (cos_theta = 1) -- and
// at a perfectly specular reflector there (rho=1, delta=0) reduces to
// Q_pr = 2, BLS79's own "perfectly backscatters" case: twice the drag of
// absorption, not the same, which is the case PHPR-R-010 names as this
// tree's own target spacecraft.  Bounded by the term's own size, of order
// PHPR-P-5's ~1.5e-9 m/s^2, where it is not exact.

#include <odl/srp_analytic/srp_analytic.hpp>

#include <cmath>

namespace odl::srp_analytic {
namespace {

using odl::macromodel::Band;
using odl::macromodel::FlatSurface;
using odl::macromodel::NormalMode;
using odl::macromodel::OpticalTriple;
using odl::macromodel::Surface;
using odl::macromodel::SphericalSurface;

/// RHS12 Eq. (6)'s own value, 1367 W/m^2.
inline constexpr double kSolarIrradianceAt1AuWPerM2 = 1367.0;
/// Exact, by definition of the metre since 1983.
inline constexpr double kSpeedOfLightMPerS = 299792458.0;

/// d(force)/d(v_rel) is a 3x3 matrix regardless of surface kind, always of
/// the shape -k*(a (x) b + s*I) for some scalar k, vectors a/b and scalar s
/// -- both surface kinds below build one this way, verified analytically
/// (this file's own header comment) and numerically (a finite-difference
/// cross-check, recorded in PROVENANCE.md's own step-5 entry, to machine
/// precision) before being trusted as PHPR-A-006's own production Jacobian,
/// not merely its check.
Mat3 outer_plus_scaled_identity(const Vec3& a, const Vec3& b, double scale, double k) {
    const double av[3] = {a.x, a.y, a.z};
    const double bv[3] = {b.x, b.y, b.z};
    Mat3 m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = k * (av[i] * bv[j] + (i == j ? scale : 0.0));
    return m;
}

struct ForceAndJacobian {
    Vec3 force;
    Mat3 d_force_d_v;   ///< PHPR-R-010's own analytic Jacobian, not a finite difference
};

/// SRPA-R-001 (RHS12 Eq. 6) generalised by PHPR-R-010: this file's own header
/// comment derives the aberration terms from BLS79 Eq. (5).  `v_rel` is
/// velocity relative to the irradiance SOURCE, in body frame; the zero
/// vector reproduces the pre-existing steady force exactly (PHPR-R-003).
/// cos_theta < 0 (surface facing away from the source) contributes nothing
/// -- stated in the equation's own domain, not bolted on as a special case.
///
/// d(F)/d(v): F is LINEAR in v (BLS79 Eq. 5's own shape, kept through the
/// generalisation), so the Jacobian does not depend on v at all -- exact,
/// not a local linearisation. F's own v-dependent part is
/// -(prefactor*cos_theta/c)*[(v.e_D)*steady_direction + drag_q_pr*v], so
/// d/dv is -(prefactor*cos_theta/c)*[steady_direction (x) e_D + drag_q_pr*I].
ForceAndJacobian flat_force(double area_m2, double irradiance_w_m2, double rho, double delta,
                            const Vec3& e_D, const Vec3& e_N, const Vec3& v_rel) {
    const double cos_theta = e_D.dot(e_N);
    if (!(cos_theta > 0.0)) return ForceAndJacobian{Vec3{0.0, 0.0, 0.0}, Mat3{}};
    const double prefactor = area_m2 * irradiance_w_m2 / kSpeedOfLightMPerS;
    const double e_D_coeff = (1.0 - rho);
    const double e_N_coeff = 2.0 * (delta / 3.0 + rho * cos_theta);
    const double doppler = 1.0 + e_D.dot(v_rel) / kSpeedOfLightMPerS;
    const Vec3 steady_direction = e_D_coeff * e_D + e_N_coeff * e_N;
    const double drag_q_pr = e_D_coeff + e_N_coeff;
    const Vec3 bracket = (doppler * steady_direction) + (drag_q_pr / kSpeedOfLightMPerS) * v_rel;
    const Vec3 force = -(prefactor * cos_theta) * bracket;
    const Mat3 jac = outer_plus_scaled_identity(steady_direction, e_D, drag_q_pr,
                                                -(prefactor * cos_theta) / kSpeedOfLightMPerS);
    return ForceAndJacobian{force, jac};
}

/// SRPA-R-003 generalised by PHPR-R-010, exact per BLS79 (this file's own
/// header comment): Q_pr = 1 + 4*delta/9, rho absent (SRPA-A-002). d(F)/d(v)
/// = -(prefactor*Q_pr/c)*[e_D (x) e_D + I], the same linear-in-v reasoning
/// as `flat_force`'s own comment, with steady_direction = e_D and
/// drag_q_pr = Q_pr.
ForceAndJacobian spherical_force(double area_m2, double irradiance_w_m2, double delta,
                                 const Vec3& e_D, const Vec3& v_rel) {
    const double q_pr = 1.0 + 4.0 * delta / 9.0;
    const double prefactor = area_m2 * irradiance_w_m2 / kSpeedOfLightMPerS;
    const double doppler = 1.0 + e_D.dot(v_rel) / kSpeedOfLightMPerS;
    const Vec3 bracket = (doppler * e_D) + (1.0 / kSpeedOfLightMPerS) * v_rel;
    const Vec3 force = -(prefactor * q_pr) * bracket;
    const Mat3 jac = outer_plus_scaled_identity(e_D, e_D, 1.0, -(prefactor * q_pr) / kSpeedOfLightMPerS);
    return ForceAndJacobian{force, jac};
}

}  // namespace

odl::Result<PhotonForceAndVelocityJacobian, SrpError>
photon_force_and_velocity_jacobian(const macromodel::Macromodel& model,
                                   macromodel::IrradianceWPerM2 irradiance, Band band,
                                   const macromodel::BodyDirection& source_direction_body,
                                   const macromodel::BodyDirection& sun_direction_body,
                                   const Vec3& velocity_relative_to_source_body_m_per_s) {
    if (model.surfaces().empty())
        return odl::err(SrpError{"SRPA-F-001",
                         "photon_force called on a macromodel with zero surfaces: nothing to "
                         "evaluate"});

    const Vec3& e_D = source_direction_body.vec();
    const Vec3& sun_e_D = sun_direction_body.vec();
    const Vec3& v_rel = velocity_relative_to_source_body_m_per_s;
    const double irr = irradiance.watts_per_m2();
    Vec3 total_force{0.0, 0.0, 0.0};
    Mat3 total_jac{};
    for (const Surface& s : model.surfaces()) {
        if (const auto* flat = std::get_if<FlatSurface>(&s)) {
            const Vec3 front_normal = (flat->normal_mode() == NormalMode::sun_pointing)
                                          ? sun_e_D
                                          : flat->body_fixed_normal()->vec();
            const OpticalTriple& front = flat->front(band);
            const auto front_fj = flat_force(flat->area_m2().value(), irr, front.specular.value(),
                                             front.diffuse.value(), e_D, front_normal, v_rel);
            total_force = total_force + front_fj.force;
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) total_jac.r[i][j] += front_fj.d_force_d_v.r[i][j];
            // PHPR-R-004a: the back face, reversed normal, evaluated only
            // when the source is behind the front (flat_force's own
            // cos_theta < 0 domain already zeroes the front in that case).
            if (const auto back = flat->back(band)) {
                const auto back_fj = flat_force(flat->area_m2().value(), irr, back->specular.value(),
                                                back->diffuse.value(), e_D, -1.0 * front_normal, v_rel);
                total_force = total_force + back_fj.force;
                for (std::size_t i = 0; i < 3; ++i)
                    for (std::size_t j = 0; j < 3; ++j) total_jac.r[i][j] += back_fj.d_force_d_v.r[i][j];
            }
        } else {
            const auto& sph = std::get<SphericalSurface>(s);
            const OpticalTriple triple = sph.in(band);
            const auto fj = spherical_force(sph.cross_section_area_m2.value(), irr,
                                            triple.diffuse.value(), e_D, v_rel);
            total_force = total_force + fj.force;
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) total_jac.r[i][j] += fj.d_force_d_v.r[i][j];
        }
    }
    return PhotonForceAndVelocityJacobian{total_force, total_jac};
}

odl::Result<Vec3, SrpError>
photon_force(const macromodel::Macromodel& model, macromodel::IrradianceWPerM2 irradiance, Band band,
            const macromodel::BodyDirection& source_direction_body,
            const macromodel::BodyDirection& sun_direction_body,
            const Vec3& velocity_relative_to_source_body_m_per_s) {
    // A thin extraction of `photon_force_and_velocity_jacobian`'s own force
    // member, not a parallel computation -- mechanically preserves
    // PHPR-R-003's bit-identity (the force expression is computed exactly
    // once, in `flat_force`/`spherical_force`, whichever function is called).
    auto fj = photon_force_and_velocity_jacobian(model, irradiance, band, source_direction_body,
                                                  sun_direction_body,
                                                  velocity_relative_to_source_body_m_per_s);
    if (!fj.has_value()) return odl::err(fj.error());
    return fj->force;
}

odl::Result<Vec3, SrpError>
srp_force(const macromodel::Macromodel& model, const macromodel::BodyDirection& sun_direction_body) {
    auto irr = macromodel::irradiance_w_per_m2(kSolarIrradianceAt1AuWPerM2);
    // kSolarIrradianceAt1AuWPerM2 is a positive literal constant; this can
    // only fail if that literal itself is ever made negative or non-finite,
    // which is a compile-time fact this file's own header states, not a
    // runtime possibility -- REQUIRE-shaped, not a caller-facing refusal.
    if (!irr.has_value())
        return odl::err(SrpError{"SRPA-F-001", "unreachable: kSolarIrradianceAt1AuWPerM2 is invalid"});
    return photon_force(model, *irr, Band::visible, sun_direction_body, sun_direction_body,
                        Vec3{0.0, 0.0, 0.0});
}

}  // namespace odl::srp_analytic
