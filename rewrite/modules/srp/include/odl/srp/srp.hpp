#pragma once
// odl/srp/srp.hpp — solar radiation pressure, as a dyn::Force plugin.
//
// SPEC-photon-pressure PHPR-R-005..R-007. The gap this closes: L4 steps 1
// and 3 built the eclipse shadow function and the SRP force law and no step
// composed them into a force the propagator can use -- the only dyn::Force
// in the tree was Drag, and L4's exit gate cannot be reached without this
// one.

#include <odl/attitude/attitude.hpp>
#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/force.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <vector>

namespace odl::srp {

/// `dyn::Force`'s frozen surface (SPEC-dynamics), the same shape `Drag`
/// already takes: fixed physical properties bound at construction (the
/// `Macromodel`, and a reference to a long-lived `Ephemeris` this class does
/// not own -- `Ephemeris` is move-only, SPEC-ephemerides's own choice, and a
/// caller who wants several forces reading the same kernel constructs one
/// `Ephemeris` and references it from each). Consumes no registered
/// parameter (PHPR-R-005): unlike drag's single, poorly-known C_D, the
/// macromodel's own per-surface optical triples already carry what SRP needs
/// to know, and a C_R-shaped scalar on top of them would be a second,
/// competing definition of the same quantity -- not built until a real need
/// for one is found, `DYN-Q-001`'s own terms applied to not adding a thing
/// rather than to adding one.
class Srp final : public dyn::Force {
public:
    Srp(macromodel::Macromodel model, const eph::Ephemeris& ephemeris, odl::time::LeapTable leaps);

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
         const Vec3& v_m_per_s, const dyn::ParameterSet& params,
         const dyn::ParameterRegistry& registry) const override;

private:
    /// `accel_only`'s own result: the acceleration, plus d(acceleration)/
    /// d(velocity), ANALYTIC (`srp_analytic::photon_force_and_velocity_
    /// jacobian`, PHPR-R-010), already rotated into GCRS. Computed at every
    /// call -- including the six position-bumped ones `accel()` makes to
    /// form d(a)/d(r) by finite difference, which use only `.acceleration`
    /// -- rather than forked into two near-duplicate pipelines: the extra
    /// per-surface outer-product terms `photon_force_and_velocity_jacobian`
    /// computes over `photon_force` cost nothing worth a second code path
    /// that could drift from this one.
    struct AccelAndVelocityJacobian {
        Vec3 acceleration;
        Mat3 d_acceleration_d_velocity;   ///< GCRS frame, s^-1
    };

    /// Everything `accel()` does short of assembling d(a)/d(r) -- PHPR-R-005
    /// through R-010's own pipeline (Sun state, distance-scaled irradiance,
    /// velocity relative to the Sun, attitude, shadow, the kernel), called
    /// once for the true state and six more times, bumped in position only,
    /// to form d(a)/d(r) by central difference (`PHPR-A-006` checks this
    /// function's own analytic d(a)/d(v) against an independent finite
    /// difference; d(a)/d(r) has no analytic form this pipeline's own
    /// distance-scaled irradiance, shadow function and attitude law make
    /// tractable to derive, so it stays a finite difference, `PHPR-P-6`).
    /// Not a public surface: `dyn::Force::accel` is.
    [[nodiscard]] odl::Result<AccelAndVelocityJacobian, dyn::DynError>
    accel_only(const odl::time::Epoch& t, const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s) const;

    macromodel::Macromodel model_;
    const eph::Ephemeris& ephemeris_;
    odl::time::LeapTable leaps_;
    std::vector<dyn::ParameterId> consumes_;   // always empty; PHPR-R-005
};

}  // namespace odl::srp
