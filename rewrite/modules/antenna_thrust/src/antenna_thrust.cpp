// antenna_thrust.cpp — SPEC-thrust-yaw TYAW-R-005..R-006.

#include <odl/antenna_thrust/antenna_thrust.hpp>

#include <cmath>
#include <cstddef>
#include <string>

namespace odl::antenna_thrust {
namespace {

/// CODATA exact value.
inline constexpr double kSpeedOfLightMPerS = 299792458.0;

}  // namespace

odl::Result<Vec3, ThrustError> antenna_thrust(double p_watts, const Vec3& z_body_gcrs_unit) {
    if (!(p_watts >= 0.0) || !std::isfinite(p_watts)) {
        return odl::err(ThrustError{"TYAW-F-002",
            "antenna_thrust called with a negative or non-finite p_watts: " +
            std::to_string(p_watts)});
    }
    return (-p_watts / kSpeedOfLightMPerS) * z_body_gcrs_unit;
}

AntennaThrust::AntennaThrust(double p_watts, double mass_kg)
    : p_watts_(p_watts), mass_kg_(mass_kg), consumes_{} {}

dyn::ForceId AntennaThrust::id() const { return dyn::ForceId{"antenna_thrust"}; }

const std::vector<dyn::ParameterId>& AntennaThrust::consumes() const { return consumes_; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
AntennaThrust::accel(const odl::time::Epoch&, const frames::Position<frames::Frame::GCRS>& r_m,
                     const Vec3&, const dyn::ParameterSet&, const dyn::ParameterRegistry&) const {
    const Vec3 r_gcrs_m = r_m.metres();
    const double r_norm = r_gcrs_m.norm();
    const Vec3 r_hat{r_gcrs_m.x / r_norm, r_gcrs_m.y / r_norm, r_gcrs_m.z / r_norm};

    // z_body = -r_hat (geocentric nadir, TYAW-R-006 -- every attitude law
    // this tree has, so this force reads it directly rather than calling
    // one).
    const Vec3 z_body{-r_hat.x, -r_hat.y, -r_hat.z};

    auto force = antenna_thrust(p_watts_, z_body);
    if (!force.has_value())
        return odl::err(dyn::DynError{force.error().id, force.error().message});

    const Vec3 a_gcrs = (1.0 / mass_kg_) * (*force);

    // d(a)/d(r) = (P/(m*c*|r|)) * (I - r_hat (x) r_hat^T), the standard
    // derivative of a normalised vector -- analytic, not a finite
    // difference of a function differentiable on sight (the L3 trap this
    // tree's own discipline names, `Srp::accel_only`'s own earlier remaining
    // finite difference existing only because ITS OWN pipeline, unlike this
    // one, has no closed form to take instead). a = (P/(m*c)) * r_hat
    // exactly: the force is (-P/c)*z_body = (-P/c)*(-r_hat) = (P/c)*r_hat.
    const double scale = p_watts_ / (kSpeedOfLightMPerS * mass_kg_ * r_norm);
    const double rh[3] = {r_hat.x, r_hat.y, r_hat.z};
    Mat3 da_dr;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            da_dr.r[i][j] = scale * ((i == j ? 1.0 : 0.0) - rh[i] * rh[j]);

    dyn::ParameterJacobian pj(dyn::ParameterRegistry{});
    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{a_gcrs},
        // d(a)/d(v) = 0 EXACTLY, not a bound on a nonzero term: z_body does
        // not depend on velocity at all, for any provider this tree has
        // (TYAW-R-006) -- there is no mechanism, however small, for it to.
        dyn::StateJacobian::no_velocity_dependence(da_dr, 0.0),
        pj,
        std::nullopt,
    };
}

}  // namespace odl::antenna_thrust
