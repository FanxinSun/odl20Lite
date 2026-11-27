#include <odl/forcemodel/scaled_force.hpp>

#include <cmath>

namespace odl::forcemodel {

namespace {

[[nodiscard]] Mat3 times(double c, const Mat3& m) noexcept {
    Mat3 out;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) out.r[i][j] = c * m.r[i][j];
    return out;
}

}  // namespace

odl::Result<std::shared_ptr<ScaledForce>, ForceModelError>
ScaledForce::create(std::shared_ptr<const dyn::Force> inner, dyn::ParameterId c, const dyn::ParameterRegistry& registry) {
    if (!inner) return odl::err(ForceModelError{"EST-F-902", "ScaledForce: the wrapped force is null"});
    if (!registry.issued(c)) return odl::err(ForceModelError{"EST-F-902", "ScaledForce: the scale parameter was not issued by the registry given"});
    std::vector<dyn::ParameterId> consumes{c};
    for (const dyn::ParameterId& p : inner->consumes()) {
        if (p == c) return odl::err(ForceModelError{"EST-F-902", "ScaledForce: the wrapped force '" + inner->id().name + "' already consumes the scale parameter"});
        consumes.push_back(p);
    }
    return std::shared_ptr<ScaledForce>(new ScaledForce(std::move(inner), c, std::move(consumes)));
}

dyn::ForceId ScaledForce::id() const { return dyn::ForceId{inner_->id().name + ".scaled"}; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
ScaledForce::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s, const dyn::ParameterSet& params,
                   const dyn::ParameterRegistry& registry) const {
    auto ev = inner_->accel(t, r_m, v_m_per_s, params, registry);
    if (!ev) return odl::err(ev.error());
    const auto cv = params.value(c_);
    if (!cv) return odl::err(cv.error());
    const double c = *cv;
    if (!std::isfinite(c)) return odl::err(dyn::DynError{"EST-F-902", "ScaledForce '" + id().name + "': the value of the scale parameter is not finite"});

    const Vec3 a_inner = ev->acceleration.metres_per_second_squared();
    const dyn::StateJacobian& sj = ev->d_state;
    const dyn::StateJacobian scaled_state = sj.d_velocity() ? dyn::StateJacobian::with_velocity(times(c, sj.d_position()), times(c, *sj.d_velocity()))
                                                            : dyn::StateJacobian::no_velocity_dependence(times(c, sj.d_position()), std::fabs(c) * sj.neglected_velocity_bound_per_s());
    dyn::ParameterJacobian dp{registry};
    for (const dyn::ParameterId& p : inner_->consumes()) {
        const auto col = ev->d_parameters.column(p);
        if (!col) return odl::err(col.error());
        if (auto put = dp.set_column(p, Vec3{c * col->x, c * col->y, c * col->z}); !put) return odl::err(put.error());
    }
    if (auto put = dp.set_column(c_, a_inner); !put) return odl::err(put.error());      // d(a)/d(c) = the acceleration at c = 1

    dyn::ForceEvaluation out{frames::Acceleration<frames::Frame::GCRS>{Vec3{c * a_inner.x, c * a_inner.y, c * a_inner.z}}, scaled_state, std::move(dp), std::move(ev->provenance)};
    return out;
}

}  // namespace odl::forcemodel
