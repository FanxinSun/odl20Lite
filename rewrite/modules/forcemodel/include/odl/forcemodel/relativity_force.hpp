#pragma once
// odl/forcemodel/relativity_force.hpp — ONE term of TN36-10 (10.12) as a dyn::Force (SPEC-forcemodel.md FMOD-R-006).
//
// The three terms are separable by design (PERT-R-042) and differ by two orders, so each is its own plugin and the ranking reads them
// apart.  The acceleration is `relativity::Correction::by_term` for the one term, with the satellite's state crossed to the L2 API's
// kilometres BY NAME, once per call, and the Earth about the Sun built from the typed `State<GCRS>` of the Sun, negated (no crossing).
// The Jacobians are closed forms; every term depends on the satellite's velocity, and each plugin says so (`with_velocity`).

#include <odl/dynamics/force.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/forcemodel/common.hpp>
#include <odl/relativity/correction.hpp>

#include <string>
#include <vector>

namespace odl::forcemodel {

class Relativity final : public dyn::Force {
public:
    /// The ephemeris and the leap table are borrowed and must outlive the plugin.
    Relativity(relativity::Term term, const eph::Ephemeris& ephemeris, const time::LeapTable& leaps,
               relativity::PpnParameters ppn = relativity::PpnParameters::general_relativity(),
               std::vector<SourceRecord> sources = {});

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s,
          const dyn::ParameterSet& params, const dyn::ParameterRegistry& registry) const override;

    [[nodiscard]] relativity::Term term() const noexcept { return term_; }

private:
    relativity::Term term_;
    const eph::Ephemeris* ephemeris_;
    const time::LeapTable* leaps_;
    relativity::PpnParameters ppn_;
    std::vector<SourceRecord> sources_;
    std::vector<dyn::ParameterId> consumes_;
};

}  // namespace odl::forcemodel
