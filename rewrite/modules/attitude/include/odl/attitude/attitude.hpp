#pragma once
// odl/attitude/attitude.hpp — the ideal nominal yaw-steering law.
//
// SPEC-photon-pressure PHPR-R-004. `SRPA-Q-001` named "computing
// sun_direction_body from an orbit position" as a different module's job --
// this is that module, built because L4 step 5's own two plugins (SRP, ERP)
// both need it from the day they exist, not a speculative generality.
//
// RULED (PHPR-Q-002): this module carries the IDEAL nominal law only -- a
// pure function of instantaneous geometry, nadir-pointing +Z, Sun-tracking
// solar panels, no attitude HISTORY to consult because there is none to
// carry. Everything beyond ideal nominal -- noon/midnight turns, where the
// law below is singular or nearly so, the constellation-specific laws,
// antenna thrust -- is L4 step 6's ("thrust-yaw", `../plan/subplan_L4/L4-6.md`),
// which adds providers to this module. It does not rebuild this law.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>

namespace odl::attitude {

using AttitudeError = odl::Diagnostic;

/// PHPR-R-004. The standard nadir-pointing, Sun-tracking-panel law used
/// throughout RS09/RS12/RHS12 ("ensuring that the navigation antennas always
/// point to the geocenter and that the solar panels always point to the
/// Sun," RS09 §3.2.1):
///
///   z_body = -r_hat                          (nadir)
///   y_body = (z_body x s_hat) / |z_body x s_hat|   (panel rotation axis,
///                                                    perpendicular to the Sun)
///   x_body = y_body x z_body
///
/// `r_gcrs_m` and `sun_direction_gcrs` need not be pre-normalised -- both are
/// normalised internally, so a caller passing a GCRS position in metres and
/// a Sun direction that is only approximately unit length gets the same
/// answer a caller who normalised first would.
///
/// Returns M_gcrs_to_body (`core/vec3.hpp`'s own `Mat3` convention: row i is
/// axis i of the TARGET frame, expressed in the SOURCE frame's components,
/// so `result.apply(v_gcrs)` gives `v`'s components in this body frame) --
/// not a bespoke frame type, because `Mat3` already is one and a second
/// definition of the same idea is the fault this tree's own `NormalMode`
/// comment (`odl/macromodel/macromodel.hpp`) already names for a different
/// pair of quantities.
///
/// Refuses (`ATTD-F-001`) when the Sun direction is within `kMinAxisNorm` of
/// the nadir axis (`|z_body x s_hat|` below it): the panel rotation axis is
/// then UNDEFINED, not merely small, and returning a near-singular frame
/// would be silently wrong rather than visibly absent. The threshold is
/// tight (~2 x 10^-4 arcsec) on purpose: this is the IDEAL law, and the
/// genuinely close approaches to this singularity that real missions
/// encounter (noon/midnight turns) are L4 step 6's own case to handle with a
/// non-refusing, purpose-built law -- refusing too early here would make
/// this module's own refusal fire on cases step 6 exists to answer, not
/// only on true degeneracy.
[[nodiscard]] odl::Result<Mat3, AttitudeError>
nominal_yaw_steering(const Vec3& r_gcrs_m, const Vec3& sun_direction_gcrs);

/// SPEC-thrust-yaw TYAW-R-001..R-004. Which GPS block's own law applies --
/// four laws sharing one mathematical shape (`KOUBA09`'s own ATAN2 turn
/// form), differing only in the parameters `HardwareYawRates` carries and
/// (IIR/IIR-M only) the absence of a shadow-crossing law. `IIIA` selects
/// `TYAW-R-004`'s own IIF-law stopgap, not a fifth implementation -- see
/// `TYAW-A-005`, which checks this is bit-identical to `IIF` at the same
/// inputs.
enum class GpsBlock { II_IIA, IIR_IIRM, IIF, IIIA };

/// The per-block constants `gps_yaw_attitude` needs, supplied by the caller
/// rather than compiled in -- `PHPR-R-001`'s own reasoning (a caller-supplied
/// irradiance, not a file-local constant) applied here to a different family
/// of numbers. `night_deg_per_s` is distinct from `noon_deg_per_s` only for
/// `GpsBlock::IIF` (`TYAW-R-003`'s own two measured rates, `DIL10`); every
/// other block states the same value in both, since `KOUBA09`'s own II/IIA
/// and IIR laws use one rate for every turn kind. `spin_up_deg_per_s2` is
/// read only for `GpsBlock::II_IIA` (`TYAW-R-001`'s own shadow-crossing spin
/// phase); ignored otherwise.
struct HardwareYawRates {
    double noon_deg_per_s;
    double night_deg_per_s;
    double yaw_bias_deg;
    double spin_up_deg_per_s2;
};

/// SPEC-thrust-yaw TYAW-R-001..R-007. A stateless provider (TYAW-R-007's own
/// header comment): every quantity this needs is computable from the CURRENT
/// epoch's own (`r_gcrs_m`, `v_gcrs_m_per_s`, `sun_direction_gcrs`) alone, no
/// history of a prior call required -- `KOUBA09`'s own turn-timing equations
/// are closed-form in the current beta and mu, reformulated here in terms of
/// mu directly (mu evolves linearly with time at the orbit's own mean
/// motion, so a time-since-turn-start and an angle-since-turn-start carry
/// the same information, and the angle needs no remembered reference epoch).
///
/// Outside any turn or shadow-crossing regime, returns exactly what
/// `nominal_yaw_steering(r_gcrs_m, sun_direction_gcrs)` returns (the two
/// must agree at every hand-over, `TYAW-P-2`) -- this function's own
/// refusal (`ATTD-F-001`, forwarded unchanged) is that same call's own
/// refusal, reached the same way.
///
/// The beta-near-zero turn-start sign ambiguity (`TYAW-Q-002`, RULED:
/// carry, with a stated convention): uses sign(beta) at the CURRENT query
/// instant, with sign(0) taken as +1 -- a fixed, deterministic tie-break,
/// not remembered state, matching this function's own overall
/// statelessness rather than breaking it for this one case.
[[nodiscard]] odl::Result<Mat3, AttitudeError>
gps_yaw_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                 const Vec3& sun_direction_gcrs, GpsBlock block, const HardwareYawRates& rates);

}  // namespace odl::attitude
