// SPEC-measmod.md §5.1 — the production station track, Earth orientation and Earth motion.

#include <odl/measmod/tracks.hpp>

#include <odl/core/units.hpp>
#include <odl/frames/transform.hpp>

#include <cmath>

namespace odl::measmod {

odl::Result<EarthOrientationSample, MeasError> EopEarthOrientation::at(const odl::time::Epoch& when) const {
    auto record = series_->at(when, policy_);
    if (!record) return odl::err(record.error());                       // the EOP series' own refusal, with its own id
    auto rot = odl::frames::gcrs_to_itrs(when, *record, *leaps_);
    if (!rot) return odl::err(rot.error());                              // the frames chain's own
    // `omega_rad_s` is in the intermediate (TIRS) components, after `pre`: the rotation vector in GCRS axes is preᵀ ω
    return EarthOrientationSample{rot->m, rot->pre.transpose().apply(rot->omega_rad_s)};
}

odl::Result<StationKinematics, MeasError> EarthFixedStation::at(const odl::time::Epoch& when) const {
    auto o = orientation_->at(when);
    if (!o) return odl::err(o.error());
    const odl::Mat3 to_gcrs = o->gcrs_to_itrs.transpose();
    StationKinematics k;
    k.position_m = to_gcrs.apply(itrs_position_m_);
    k.velocity_m_s = o->omega_gcrs_rad_s.cross(k.position_m);
    const double sl = std::sin(geodetic_.longitude_rad), cl = std::cos(geodetic_.longitude_rad);
    const double sp = std::sin(geodetic_.latitude_rad), cp = std::cos(geodetic_.latitude_rad);
    k.up = to_gcrs.apply(odl::Vec3{cp * cl, cp * sl, sp});
    k.up_rate_per_s = o->omega_gcrs_rad_s.cross(k.up);
    return k;
}

odl::Result<EarthMotionSample, MeasError> EphemerisEarthMotion::at(const odl::time::Epoch& when) const {
    auto s = ephemeris_->barycentric_state(odl::eph::Body::Earth, when, *leaps_);
    if (!s) return odl::err(s.error());                                  // the ephemeris' own refusal
    return EarthMotionSample{odl::metres_from_km(s->position()), odl::metres_from_km(s->velocity())};
}

}  // namespace odl::measmod
