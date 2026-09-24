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
/// of numbers. `night_deg_per_s` is read for `GpsBlock::II_IIA` (`TYAW-R-001`'s
/// own shadow-crossing target rate) and `GpsBlock::IIR_IIRM` (`TYAW-R-002`'s
/// own night rate, identical to noon); NOT read for `GpsBlock::IIF`
/// (`TYAW-R-003`'s own night side is Shape E, the shadow-crossing regime --
/// its own single constant rate is computed from beta and the fixed shadow
/// half-angle, not supplied by the caller, since during actual eclipse there
/// is no hardware rate limit being chased). `spin_up_deg_per_s2` and
/// `yaw_bias_deg` are read only for `GpsBlock::II_IIA` (`TYAW-R-001`'s own
/// shadow-crossing spin phase and its own bias-determined turn direction,
/// needed there because the solar sensor has lost the Sun; IIF's own Shape E
/// direction is fully determined by the nominal law at shadow entry/exit,
/// needing no separate bias, and IIR's turn is sensor-guided throughout,
/// never losing the Sun); ignored otherwise.
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

// --- SPEC-galileo-attitude: GALY-R-001..R-005 ------------------------------

/// Which of GSC's own two printed laws applies -- one mathematical family
/// (GALY-R-001's own shared orbital-frame Sun projection), differing in the
/// near-singularity handling each of GSC's own sections gives: IOV (§3.1.1)
/// substitutes a smooth auxiliary Sun vector; FOC (§3.1.2) uses its own
/// "modified yaw steering law" inside its own named near-colinearity region
/// (built 2026-09-24, the manager's own ruling -- an earlier version
/// refused there instead, `GALY-F-001`, now retired).
enum class GalileoBlock { IOV, FOC };

/// GALY-R-001..R-004. GSC's own yaw-steering law ("Galileo Satellite
/// Metadata" §3), reduced to this tree's own established
/// `nominal_yaw_steering` wherever GSC's own formula is, algebraically, the
/// SAME nadir-pointing/Sun-tracking geometry that function already builds
/// (GALY-A-004 proves the reduction: IOV's own primary formula and FOC's
/// own formula are IDENTICAL, both equal to `nominal_yaw_steering`'s own
/// implicit angle, once expressed in the SAME orbital-frame Sun projection)
/// -- so this function's own new work is exactly the two blocks' own
/// DEVIATIONS from that shared nominal law, not a re-derivation of the
/// frame-building formula itself:
///  - `IOV`: near the Sun/nadir-axis singularity (GSC's own named region,
///    `|x_sun| < sinβx, |y_sun| < sinβy`), substitutes GSC's own smooth
///    "auxiliary Sun reference vector" (§3.1.1) for the real Sun direction
///    before calling `nominal_yaw_steering` -- continuous by construction at
///    the region's own boundary (`GALY-A-006`).
///  - `FOC`: GSC's own "modified yaw steering law" (§3.1.2) inside its own
///    named near-colinearity switch-over region (|β| < 4.1°, colinearity
///    angle ε < 10°) -- built via a CLOSED-FORM window entry (`GALY-A-009`
///    proves GSC's own colinearity angle ε depends on μ alone, not β, so
///    the window's own entry μ is a fixed constant, never a remembered
///    crossing), GSC's own primary formula outside it.
///
/// Returns M_gcrs_to_body in THIS TREE's own convention (matching
/// `nominal_yaw_steering`/`gps_yaw_attitude`), not GSC's own native frame --
/// `odl::spacecraft::galileo_frame_from_mechanical` is the (unrelated, data-
/// side) mapping for the macromodel's own face normals; this function
/// already emits the ANTEX-convention frame directly, verified two ways
/// (`GALY-A-004`/`GALY-A-005`) against GSC's own two printed forms.
[[nodiscard]] odl::Result<Mat3, AttitudeError>
galileo_yaw_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                     const Vec3& sun_direction_gcrs, GalileoBlock block);

/// Exposed for `GALY-A-004`/`GALY-A-005`/`GALY-A-007`'s own verification
/// only -- `galileo_yaw_attitude` itself never reads this scalar, only the
/// frame it implies (through `nominal_yaw_steering`). GSC's own §3.1 NATIVE
/// yaw angle (its own printed atan2 form, IOV and FOC alike -- GALY-A-004
/// proves they are algebraically the same function), BEFORE IOV's own
/// auxiliary-vector substitution: the manager's own instruction is to check
/// against the source's own two printed forms directly, which needs the
/// angle itself, not only the body frame it produces.
[[nodiscard]] double galileo_native_yaw_angle_pre_substitution(
    const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s, const Vec3& sun_direction_gcrs) noexcept;

// --- SPEC-glonass-attitude: GLNY-R-001..R-004 ------------------------------

/// GLNY-R-001..R-004. GLONASS-M's own yaw-attitude law, Dilssner et al.
/// (2011), "The GLONASS-M satellite yaw-attitude model," Adv. Space Res.
/// 47:160-171 (`DIL11`) -- covers GLONASS-M ONLY, the paper's own stated
/// scope (its own title, and its own text: "hardly anything in this field
/// is known about the second spacecraft-generation," i.e. -M specifically;
/// original GLONASS and GLONASS-K are not covered by this or any other
/// source found this session, `SPEC-glonass-attitude.md` §2). Built from
/// `DIL11`'s own words for EACH turn, not assumed to be GPS's own KOUBA09
/// family with new constants (the manager's own explicit instruction,
/// echoing the IIF night-turn precedent, `SPEC-thrust-yaw.md` `TYAW-R-003`):
///  - **shadow-crossing (midnight)**: a closed-form geometric window
///    (`DIL11` Eq.11, no rate-threshold search), a full-hardware-rate ramp
///    from shadow entry that reaches the nominal exit yaw and then HOLDS
///    there (constant) until actual shadow exit -- a genuinely different
///    mechanism from `DIL11`'s own noon turn, confirmed against `DIL11`'s
///    own real SVN724 data (Fig.5).
///  - **noon turn**: an iterative onset-angle solve (`DIL11` Eq.16-20, its
///    own published four-iteration method, reproduced exactly, not replaced
///    by an from-scratch exact solve), then a SINGLE ramp phase spanning the
///    whole maneuver -- no hold, unlike the shadow turn (`DIL11`'s own
///    Eq.16 symmetry and Fig.6).
///  - **off-turn**: `DIL11`'s own Eq.1 states it uses "the axis conventions
///    of the GPS Block II/IIA satellites" -- algebraically KOUBA09's own
///    Eq.4 "as printed" once converted the SAME way `psi_nominal` above
///    already is, so this reduces EXACTLY to this tree's own existing
///    `psi_nominal`/`frame_from_yaw`, reused directly (PROVED, not assumed:
///    `GLNY-A-001`'s own 200000-point numerical check, `SPEC-glonass-
///    attitude.md` §3).
///
/// A stateless provider, the same shape `gps_yaw_attitude`/
/// `galileo_yaw_attitude` already are: every quantity is computable from the
/// CURRENT `(r_gcrs_m, v_gcrs_m_per_s, sun_direction_gcrs)` alone. Refuses
/// (`GLNY-F-001`, forwarded unchanged from `ATTD-F-001`) exactly where
/// `nominal_yaw_steering` itself would, outside both turns.
[[nodiscard]] odl::Result<Mat3, AttitudeError>
glonass_m_yaw_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                       const Vec3& sun_direction_gcrs);

// --- SPEC-qzss-attitude: QZSY-R-001..R-004 ---------------------------------

/// QZSY-R-002. The "orbit-normal" attitude law (SPI_QZS1_B Sec.3(2), QZSS's
/// own low-|beta| eclipse-season mode): z_body=-r_hat (nadir), y_body=n_hat
/// (the orbit normal -- FIXED, never tracking the Sun), x_body=-t_hat
/// (completing the right-handed system, "roughly the flight direction").
/// Named GENERICALLY, built REUSABLY (the manager's own instruction) for a
/// future BeiDou "zero-bias" mode (`SPEC-spacecraft.md`'s own BeiDou entry
/// names the SAME mechanism) -- takes no Sun direction at all, a genuine
/// property of this law, not an omitted parameter. Never fails: no
/// nadir-style singularity (r and v are never parallel for a real orbit),
/// so a bare `Mat3`, not a `Result`.
[[nodiscard]] Mat3 orbit_normal_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s) noexcept;

/// QZSY-R-001/R-003/R-004. QZSS's own two-mode law (`SPI_QZS1_B` §3,
/// Cabinet Office): `orbit_normal_attitude` for `|beta| <=
/// kQzssBetaSwitchRad` (APPROXIMATE, ~20deg, the source's own stated
/// figure -- the manager's own ruling: the real switch is COMMANDED, not a
/// pure function of beta, unlike GPS's/GLONASS-M's own rate-derived
/// onsets); `nominal_yaw_steering` otherwise -- QZSS's own yaw-steering
/// mode reduces to it EXACTLY (proved, `SPEC-qzss-attitude.md` §3, the SAME
/// "deviation from an already-trusted law" pattern Galileo's and
/// GLONASS-M's own nominal laws already are), so no separate frame
/// construction is built for it. Refuses (`QZSY-F-001`, forwarded
/// unchanged from `ATTD-F-001`) exactly where `nominal_yaw_steering` itself
/// would, in the yaw-steering branch only -- `orbit_normal_attitude` never
/// refuses.
[[nodiscard]] odl::Result<Mat3, AttitudeError>
qzss_yaw_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s, const Vec3& sun_direction_gcrs);

}  // namespace odl::attitude
