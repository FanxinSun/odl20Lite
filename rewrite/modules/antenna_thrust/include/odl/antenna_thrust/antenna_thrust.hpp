#pragma once
// odl/antenna_thrust/antenna_thrust.hpp — antenna-thrust recoil, as a
// dyn::Force plugin.
//
// SPEC-thrust-yaw TYAW-R-005..R-006. L4 step 6's second half (the first is
// odl::attitude's own gps_yaw_attitude, added beside this): the recoil of
// the radiated navigation signal, P/c along the boresight, with P a
// caller-supplied value -- per-satellite transmit power is L5's population
// data (DYN-Q-001's L4/L5 split), so this force LAW takes it as an argument
// rather than a file-local table.
//
// TYAW-R-006, MADE CONCRETE (manager's own review): every attitude law this
// tree has sets z_body to geocentric nadir and never moves it -- yaw is by
// definition a rotation ABOUT z_body, not of it. So this force is a function
// of POSITION ALONE: no Sun ephemeris, no yaw-attitude call, exactly zero
// velocity dependence, and an ANALYTIC position Jacobian, not the
// Ephemeris/AttitudeProvider-plumbed, finite-differenced design this file
// carried before that review (kept out of the tree's own history rather than
// silently dropped -- PROVENANCE records why it changed).

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/force.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/time/epoch.hpp>

#include <vector>

namespace odl::antenna_thrust {

using ThrustError = odl::Diagnostic;

/// TYAW-R-005's own corrected law: magnitude P/c as a stated UPPER BOUND
/// (not exact -- TYAW-P-5 quantifies the gap, 1.46-3.02% at realistic GPS
/// beam widths, and why it is left unmodelled), direction exactly along
/// -z_body_gcrs_unit (recoil, opposite the +z_body boresight
/// `SPEC-macromodel`'s own body-frame convention already fixes) -- exact
/// for any pattern axisymmetric about the boresight, regardless of its own
/// shape.
///
/// Takes the boresight axis directly, a unit vector, rather than a full
/// `Mat3` frame: this law needs exactly one thing, the direction +z_body
/// points in, and a "frame" parameter invites a caller to build one that is
/// not a complete, valid frame and pass it anyway, silently correct only
/// because nothing reads the other rows (manager's own review, PROVENANCE).
///
/// Refuses (TYAW-F-002) on a negative or non-finite `p_watts`. Does not
/// itself validate that `z_body_gcrs_unit` is unit length: every caller in
/// this tree either builds it directly from a normalised position (this
/// module's own `AntennaThrust`) or an `odl::attitude` provider's own
/// already-orthonormal frame.
[[nodiscard]] odl::Result<Vec3, ThrustError>
antenna_thrust(double p_watts, const Vec3& z_body_gcrs_unit);

/// `dyn::Force`'s frozen surface -- SIMPLER than `Srp`/`Erp`'s own shape on
/// purpose (TYAW-R-006): no `Macromodel`, no `Ephemeris`, no attitude
/// dependency at all, since z_body is geocentric nadir for every law this
/// tree has and this force reads it straight from the position `accel()`
/// already receives. `p_watts` and `mass_kg` are the only per-satellite
/// population this force needs (`DYN-Q-001`'s own L4/L5 split).
class AntennaThrust final : public dyn::Force {
public:
    AntennaThrust(double p_watts, double mass_kg);

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
         const Vec3& v_m_per_s, const dyn::ParameterSet& params,
         const dyn::ParameterRegistry& registry) const override;

private:
    double p_watts_;
    double mass_kg_;
    std::vector<dyn::ParameterId> consumes_;   // always empty; the same reasoning PHPR-R-005 gives
};

}  // namespace odl::antenna_thrust
