#include <odl/frames/local.hpp>

#include <cmath>
#include <string>

namespace odl::frames {
namespace {

Mat3 rows_of(const Vec3& a, const Vec3& b, const Vec3& c) noexcept {
    Mat3 m;
    m.r[0] = {a.x, a.y, a.z};
    m.r[1] = {b.x, b.y, b.z};
    m.r[2] = {c.x, c.y, c.z};
    return m;
}

}  // namespace

odl::Result<Mat3, FrameError> rtn_basis(const Vec3& r, const Vec3& v) {
    const double rn = r.norm();
    const Vec3 h = r.cross(v);
    const double hn = h.norm();
    // FRAME-F-004.  Rectilinear motion, or a zero vector: the normal is not
    // merely small, it is undefined, and picking an arbitrary one is how a
    // covariance ends up rotated into a direction nobody chose.
    if (rn <= 0.0 || hn <= 1e-12 * rn * v.norm()) {
        return odl::err(FrameError{"FRAME-F-004",
            "RTN basis is undefined: |r| = " + std::to_string(rn) + ", |v| = " +
            std::to_string(v.norm()) + ", |r x v| = " + std::to_string(hn) +
            ", threshold 1e-12 |r||v|. The motion is rectilinear or a vector is zero, so the "
            "orbit normal does not exist. There is no fallback normal to pick."});
    }
    const Vec3 eR = (1.0 / rn) * r;
    const Vec3 eN = (1.0 / hn) * h;
    const Vec3 eT = eN.cross(eR);
    return rows_of(eR, eT, eN);
}

odl::Result<Mat3, FrameError> dyb_basis(const Vec3& r_sat, const Vec3& r_sun) {
    const Vec3 to_sun = r_sun - r_sat;          // FRAME-R-050: SPACECRAFT -> SUN
    const double dn = to_sun.norm();
    const double rn = r_sat.norm();
    if (dn <= 0.0 || rn <= 0.0) {
        return odl::err(FrameError{"FRAME-F-005",
            "DYB basis is undefined: |r_sat| = " + std::to_string(rn) +
            ", |r_Sun - r_sat| = " + std::to_string(dn)});
    }
    const Vec3 eD = (1.0 / dn) * to_sun;
    const Vec3 eR = (1.0 / rn) * r_sat;
    const Vec3 y = eD.cross(eR);
    const double yn = y.norm();
    // FRAME-F-005: the spacecraft exactly on the Earth-Sun line. Reachable, and
    // the returned basis would otherwise be arbitrary.
    if (yn <= 1e-10) {
        return odl::err(FrameError{"FRAME-F-005",
            "DYB basis is undefined: the spacecraft lies on the Earth-Sun line, "
            "|e_D x e_r| = " + std::to_string(yn) + ", threshold 1e-10. The solar-panel "
            "rotation axis has no direction there."});
    }
    const Vec3 eY = (1.0 / yn) * y;
    const Vec3 eB = eD.cross(eY);
    return rows_of(eD, eY, eB);
}

}  // namespace odl::frames
