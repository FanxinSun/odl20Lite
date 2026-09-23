// attitude.cpp — SPEC-photon-pressure PHPR-R-004.

#include <odl/attitude/attitude.hpp>

#include <string>

namespace odl::attitude {
namespace {

/// sin of ~2e-4 arcsec -- see attitude.hpp's own reasoning for why this is
/// tight rather than generous.
inline constexpr double kMinAxisNorm = 1.0e-9;

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

}  // namespace

odl::Result<Mat3, AttitudeError>
nominal_yaw_steering(const Vec3& r_gcrs_m, const Vec3& sun_direction_gcrs) {
    const Vec3 r_hat = normalized(r_gcrs_m);
    const Vec3 s_hat = normalized(sun_direction_gcrs);
    const Vec3 z_body = -1.0 * r_hat;

    const Vec3 y_axis_raw = z_body.cross(s_hat);
    const double y_norm = y_axis_raw.norm();
    if (y_norm < kMinAxisNorm) {
        return odl::err(AttitudeError{"ATTD-F-001",
            "the Sun is within the nadir singularity's own tolerance of the nadir axis: "
            "the panel rotation axis (z_body x s_hat) has norm " + std::to_string(y_norm) +
            ", below " + std::to_string(kMinAxisNorm) + " -- undefined, not merely small"});
    }
    const Vec3 y_body = (1.0 / y_norm) * y_axis_raw;
    const Vec3 x_body = y_body.cross(z_body);

    Mat3 m;
    m.r[0] = {x_body.x, x_body.y, x_body.z};
    m.r[1] = {y_body.x, y_body.y, y_body.z};
    m.r[2] = {z_body.x, z_body.y, z_body.z};
    return m;
}

}  // namespace odl::attitude
