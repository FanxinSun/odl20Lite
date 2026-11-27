#pragma once
// odl/forcemodel/scaled_force.hpp — the SRP-scale carrier (SPEC-estimation.md v0.7 EST-R-901; ruled by odl Committee 2026-11-27, EST-Q-010 (a)).
//
// `Srp` consumes no registered parameter (PHPR-R-005, ruled) and stays so.  The frozen `G-*` fits fit an SRP scale, and `joint_covariance` needs it as a registered parameter in registry order.  So the
// scale is carried by a DECORATOR over any dyn::Force, here, in `forcemodel` (which depends on `dyn`; no L4 code changes), serving the cannonball and the box-wing fit alike:
//
//   a            = c * a_inner
//   d(a)/d(r)    = c * (the inner force's)         d(a)/d(v) = c * (the inner force's), the declared neglected-velocity bound times |c|
//   d(a)/d(p_k)  = c * (the inner force's own parameter column)
//   d(a)/d(c)    = a_inner          (the acceleration at c = 1: the acceleration is exactly linear in c)
//   consumes()   = {c} followed by the inner force's own
//
// `c` is the multiplier of the template's nominal A*C_R/m (the `G-*` convention, oracle/ORACLE.md section 6).  With c = 1 the decorated force IS the inner one, bit for bit (a product by 1.0 is exact).

#include <odl/dynamics/force.hpp>
#include <odl/forcemodel/common.hpp>

#include <memory>
#include <vector>

namespace odl::forcemodel {

class ScaledForce final : public dyn::Force {
public:
    /// EST-F-902: a null inner force; a `c` the registry did not issue; an inner force that already consumes `c`.
    [[nodiscard]] static odl::Result<std::shared_ptr<ScaledForce>, ForceModelError>
    create(std::shared_ptr<const dyn::Force> inner, dyn::ParameterId c, const dyn::ParameterRegistry& registry);

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override { return consumes_; }
    /// EST-F-902 at the call: a non-finite value of `c`.  The inner force's refusal is passed on unchanged.
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s, const dyn::ParameterSet& params,
          const dyn::ParameterRegistry& registry) const override;

private:
    ScaledForce(std::shared_ptr<const dyn::Force> inner, dyn::ParameterId c, std::vector<dyn::ParameterId> consumes) : inner_(std::move(inner)), c_(c), consumes_(std::move(consumes)) {}

    std::shared_ptr<const dyn::Force> inner_;
    dyn::ParameterId c_;
    std::vector<dyn::ParameterId> consumes_;     // c first, then the inner force's
};

}  // namespace odl::forcemodel
