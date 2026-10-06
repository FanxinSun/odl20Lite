#pragma once
// odl/forcemodel/third_body_force.hpp — the attraction of ONE body as a dyn::Force (SPEC-forcemodel.md FMOD-R-004, -005).
//
// The acceleration is `thirdbody::Attraction::by_body` for the one body, so L2's value IS the plugin's value; the Jacobian is the tidal
// tensor in closed form, `mu [ 3 d d^T / |d|^5 - I / |d|^3 ]` with d = s - r (the indirect term does not depend on the satellite).  The
// plugin is in the GCRS throughout: no rotation, and the only crossing is the named one at the ephemeris boundary.

#include <odl/dynamics/force.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/forcemodel/common.hpp>
#include <odl/thirdbody/attraction.hpp>

#include <memory>
#include <string>
#include <vector>

namespace odl::forcemodel {

/// The lower-case name a body goes by in a ForceId (`sun`, `moon`, `mercury`, `venus`, `mars`, `jupiter`, `saturn`, `uranus`, `neptune`,
/// `pluto`); an empty view for a body that is not a third body (the Earth, the barycentres).
[[nodiscard]] std::string_view third_body_name(eph::Body b) noexcept;

class ThirdBody final : public dyn::Force {
public:
    /// The ephemeris, the GM table and the leap table are borrowed and must outlive the plugin.
    ThirdBody(eph::Body body, const eph::Ephemeris& ephemeris, const thirdbody::GravitationalParameters& gm,
              const time::LeapTable& leaps, std::vector<SourceRecord> sources = {});

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s,
          const dyn::ParameterSet& params, const dyn::ParameterRegistry& registry) const override;

    [[nodiscard]] eph::Body body() const noexcept { return body_; }

private:
    eph::Body body_;
    const eph::Ephemeris* ephemeris_;
    const thirdbody::GravitationalParameters* gm_;
    const time::LeapTable* leaps_;
    std::vector<SourceRecord> sources_;
    std::vector<dyn::ParameterId> consumes_;
};

/// One plugin per body of a STATED list (`PERT-R-052`); **refuses a list without the Sun or without the Moon** (`PERT-F-013`), naming which
/// is missing; refuses a body that is not a third body.
[[nodiscard]] odl::Result<std::vector<std::shared_ptr<ThirdBody>>, ForceModelError>
third_bodies(const std::vector<eph::Body>& bodies, const eph::Ephemeris& ephemeris,
             const thirdbody::GravitationalParameters& gm, const time::LeapTable& leaps,
             const std::vector<SourceRecord>& sources = {});

}  // namespace odl::forcemodel
