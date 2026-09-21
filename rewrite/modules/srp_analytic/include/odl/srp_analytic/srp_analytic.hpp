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
#include <odl/macromodel/macromodel.hpp>

namespace odl::srp_analytic {

using SrpError = odl::Diagnostic;

/// SPEC-srp-analytic SRPA-R-001..006.  Both in the body frame throughout:
/// `sun_direction_body` is the Sun direction as seen in the satellite's own
/// body frame (the caller's attitude, not this module's), and the returned
/// force is in that same frame, in newtons -- dividing by the macromodel's
/// own cited mass to get an acceleration is the caller's.
[[nodiscard]] odl::Result<Vec3, SrpError>
srp_force(const macromodel::Macromodel& model, const macromodel::BodyDirection& sun_direction_body);

}  // namespace odl::srp_analytic
