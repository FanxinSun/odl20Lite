#pragma once
// odl/forcemodel/gravity_force.hpp — the conventional EGM2008 field as a dyn::Force (SPEC-forcemodel.md FMOD-R-003).
//
// The field is fixed in the ITRS.  The plugin rotates the GCRS position into it with `m = gcrs_to_itrs(t, record, leaps).m`, evaluates the
// field there, and returns `mT a(m r)`; its Jacobian is `mT G m` with `G` the field's own tensor (SPEC-gravity §4.7).  The rotation depends
// on time only, so no Coriolis term arises and `da/dv` is EXACTLY zero, declared with the bound 0.

#include <odl/dynamics/force.hpp>
#include <odl/forcemodel/common.hpp>
#include <odl/forcemodel/earth_orientation.hpp>
#include <odl/gravity/field.hpp>

#include <string>
#include <vector>

namespace odl::forcemodel {

struct GravityOptions {
    /// GRAV-F-006's single override: the secular-pole fit ends in 2017, and an arc after it needs this.  Recorded in the provenance.
    bool extrapolate_secular = false;
    std::vector<SourceRecord> sources;     ///< the manifest ids and hashes the caller read (`egm2008-coefficients`)
};

class Gravity final : public dyn::Force {
public:
    /// `model` is borrowed and must outlive the plugin.
    Gravity(const gravity::GravityModel& model, EarthOrientation orientation, gravity::Degree degree, gravity::Order order,
            GravityOptions options = {});

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s,
          const dyn::ParameterSet& params, const dyn::ParameterRegistry& registry) const override;

    [[nodiscard]] std::string describe_orientation() const { return orientation_.describe(); }
    [[nodiscard]] gravity::Degree degree() const noexcept { return degree_; }
    [[nodiscard]] gravity::Order order() const noexcept { return order_; }

private:
    const gravity::GravityModel* model_;
    EarthOrientation orientation_;
    gravity::Degree degree_;
    gravity::Order order_;
    GravityOptions options_;
    std::vector<dyn::ParameterId> consumes_;     // always empty (FMOD-R-002)
};

}  // namespace odl::forcemodel
