#include <odl/forcemodel/gravity_force.hpp>

#include <sstream>

namespace odl::forcemodel {

Gravity::Gravity(const gravity::GravityModel& model, EarthOrientation orientation, gravity::Degree degree,
                 gravity::Order order, GravityOptions options)
    : model_(&model), orientation_(std::move(orientation)), degree_(degree), order_(order), options_(std::move(options)) {}

dyn::ForceId Gravity::id() const { return dyn::ForceId{"gravity"}; }

const std::vector<dyn::ParameterId>& Gravity::consumes() const { return consumes_; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Gravity::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3&,
               const dyn::ParameterSet&, const dyn::ParameterRegistry& registry) const {
    auto orientation = orientation_.at(t);
    if (!orientation) return odl::err(passthrough("gravity", orientation.error()));
    const Mat3& m = orientation->rotation.m;                  // GCRS -> ITRS, the whole chain
    const Mat3 mt = m.transpose();

    auto field = model_->conventional(t, options_.extrapolate_secular);
    if (!field) return odl::err(passthrough("gravity", field.error()));

    const frames::ItrsPosition at{m.apply(r_m.metres())};
    auto a = field->acceleration(at, degree_, order_);
    if (!a) return odl::err(passthrough("gravity", a.error()));
    auto g = field->gradient(at, degree_, order_);
    if (!g) return odl::err(passthrough("gravity", g.error()));

    const Vec3 a_gcrs = mt.apply(a->metres_per_second_squared());
    const Mat3 da_dr = mt.times(g->per_second_squared()).times(m);        // mT G m

    SourceId sid("gravity");
    sid.add("model", "EGM2008_to2190_TideFree")
       .add("system", gravity::name_of(field->tide_system()) == std::string_view("zero tide") ? "zero-tide" : "tide-free")
       .add("N", std::to_string(degree_.value()))
       .add("M", std::to_string(order_.value()))
       .add("secular_pole", options_.extrapolate_secular ? "extrapolated" : "in-fit")
       .add("orientation", orientation_.describe());
    dyn::Provenance prov{sid.str(), sha256_list(options_.sources, {"egm2008-coefficients"}), false};

    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{a_gcrs},
        // `da/dv` is zero EXACTLY, not a bound on a nonzero term: the acceleration is a function of the GCRS position and the epoch alone.
        dyn::StateJacobian::no_velocity_dependence(da_dr, 0.0),
        dyn::ParameterJacobian(registry),
        prov,
    };
}

}  // namespace odl::forcemodel
