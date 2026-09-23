#pragma once
// odl/srp_analytic/srp_analytic.hpp — the direct SRP force, cannonball and
// flat plate.
//
// SPEC-srp-analytic.md.  RELOCATED FROM odl::macromodel, not new: L4 step 2's
// review found `srp_force` built INSIDE the schema module, which put physics
// in what L5 designs as data and made every consumer of the macromodel
// library link an SRP force (PERT-Q-001's own precedent against exactly
// that).  This is "the force plugin reading the macromodel" the manager's
// verdict describes -- it depends on odl::macromodel, and odl::macromodel
// does not depend on it, so a caller who only wants the schema (L5's own
// population code, say) never links a force law at all.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/irradiance.hpp>
#include <odl/macromodel/macromodel.hpp>

namespace odl::srp_analytic {

using SrpError = odl::Diagnostic;

/// `photon_force_and_velocity_jacobian`'s own result: the force
/// `photon_force` itself returns, plus its exact derivative w.r.t. the same
/// call's `velocity_relative_to_source_body_m_per_s` parameter.
struct PhotonForceAndVelocityJacobian {
    Vec3 force;
    Mat3 d_force_d_velocity;
};

/// PHPR-R-001/R-002/R-004a/R-004b/R-010 (SPEC-photon-pressure §4.1).  The
/// shared photon-pressure kernel: SRP and ERP both call this, never
/// `flat_force`/`spherical_force` directly.
///
/// `irradiance`/`band`: the caller's, not a file-local constant (PHPR-R-001)
/// -- SRP supplies the Sun's own, distance-scaled; ERP supplies the Earth's,
/// per cap element, `band` selecting which of a surface's optical triples
/// applies (PHPR-R-004b: SRP and albedo are `Band::visible`, the Earth's own
/// emission is `Band::infrared`).
///
/// `source_direction_body`/`sun_direction_body`: two different directions on
/// purpose (PHPR-R-002). The former is the illumination geometry -- `cos
/// theta`, and `e_D` in every force term. The latter resolves ONLY a
/// sun-tracking surface's own normal (`NormalMode::sun_pointing`), since a
/// solar panel tracks the Sun regardless of which body's radiation is being
/// computed. For SRP's own call they are the identical vector;
/// `SphericalSurface`s ignore `sun_direction_body` entirely, having no normal
/// to resolve.
///
/// `velocity_relative_to_source_body_m_per_s`: the spacecraft's velocity
/// RELATIVE TO THE IRRADIANCE SOURCE, in body frame -- named for what it is
/// (plan §5 constraint 10) because it is not the spacecraft's own frame
/// velocity in general. For SRP the source is the Sun, so this is
/// v_sat,GCRS - v_sun,GCRS (Earth's own ~29.8 km/s heliocentric motion
/// dominates it); for ERP the source is the Earth, so this is v_sat,GCRS
/// unchanged. Zero exactly reproduces the pre-aberration force
/// (`PHPR-R-003`); a caller with no velocity to supply passes the zero
/// vector, not an approximation of one.
///
/// THIS IS ALSO BOX-WING'S ENTIRE FORCE LAW (SRPA-R-008): RHS12's box-wing is
/// a satellite bus (several body-fixed FlatSurfaces) plus solar panels (a
/// sun-pointing FlatSurface), and this function already sums over however
/// many surfaces a Macromodel holds. Box-wing needed no new physics, only the
/// proof that summation is correct for N > 1 -- SRPA-A-009 and SRPA-A-010.
[[nodiscard]] odl::Result<Vec3, SrpError>
photon_force(const macromodel::Macromodel& model, macromodel::IrradianceWPerM2 irradiance,
            macromodel::Band band, const macromodel::BodyDirection& source_direction_body,
            const macromodel::BodyDirection& sun_direction_body,
            const Vec3& velocity_relative_to_source_body_m_per_s);

/// PHPR-R-007/R-010: the same kernel as `photon_force`, additionally
/// returning the EXACT derivative of the returned force with respect to
/// `velocity_relative_to_source_body_m_per_s`, ANALYTIC rather than a finite
/// difference of this same call. `photon_force` is implemented as a thin
/// extraction of this function's own `.force` member (not a parallel
/// computation), so the two never disagree and PHPR-R-003's bit-identity
/// carries over unchanged.
///
/// Per-surface, each force law is exactly LINEAR in the velocity parameter
/// (BLS79 Eq. 5's own shape, kept through PHPR-R-010's generalisation: every
/// direction and coefficient the law uses -- e_D, e_N, steady_direction,
/// drag_q_pr, cos_theta -- is a function of geometry and optical properties
/// alone, never of velocity), so the Jacobian is a CONSTANT matrix -- exact
/// at every velocity, not a local linearisation, and independent of the
/// `velocity_relative_to_source_body_m_per_s` value actually passed in:
///
///   flat:      d(F)/d(v) = -(prefactor*cos_theta/c) * [steady_direction (x) e_D + drag_q_pr*I]
///   spherical: d(F)/d(v) = -(prefactor*q_pr/c)       * [e_D (x) e_D + I]
///
/// (x) is the outer product, I the 3x3 identity; both derived in this file's
/// own .cpp comments and verified, before this function existed, against an
/// independent central finite difference of the force formula in a
/// standalone numerical check (max abs difference ~1e-19, i.e. exact to
/// floating-point noise) -- the record `PHPR-A-006` now checks this
/// function's own production Jacobian against, not one finite difference
/// against another (plan §4 rule 5's own tautology trap, caught here by the
/// project's own manager). A surface with cos_theta <= 0 (and any absent
/// back face) contributes the zero matrix, matching `flat_force`'s own
/// zero-force domain. Every surface's own contribution -- front, back,
/// spherical -- sums linearly, the same summation `photon_force` itself
/// performs (SRPA-A-009/A-010's own N>1 proof applies unchanged: summing
/// constant Jacobians is exactly as valid as summing the forces they came
/// from).
[[nodiscard]] odl::Result<PhotonForceAndVelocityJacobian, SrpError>
photon_force_and_velocity_jacobian(const macromodel::Macromodel& model,
                                   macromodel::IrradianceWPerM2 irradiance, macromodel::Band band,
                                   const macromodel::BodyDirection& source_direction_body,
                                   const macromodel::BodyDirection& sun_direction_body,
                                   const Vec3& velocity_relative_to_source_body_m_per_s);

/// PHPR-R-003.  `srp_force(model, d) == photon_force(model,
/// kSolarIrradianceAt1AuWPerM2, Band::visible, d, d, {0,0,0})`, bit-identical
/// -- proved by `PHPR-A-001` against a value CAPTURED FROM THE PRE-REFACTOR
/// SOURCE (`srp_force_golden.hpp`, commit ea9462f), not by comparing this
/// wrapper against the call it makes: that comparison is a tautology (it
/// cannot fail, whatever the maths does) and `SPEC-srp-analytic`'s own
/// existing tests compare against TOLERANCES, which a reordered
/// floating-point expression can satisfy while changing the last bit --
/// neither is the claim this line makes. Kept, unchanged in signature or
/// behaviour, so every pre-existing caller and every existing
/// `SPEC-srp-analytic` §8 test is unaffected by this file's generalisation.
[[nodiscard]] odl::Result<Vec3, SrpError>
srp_force(const macromodel::Macromodel& model, const macromodel::BodyDirection& sun_direction_body);

}  // namespace odl::srp_analytic
