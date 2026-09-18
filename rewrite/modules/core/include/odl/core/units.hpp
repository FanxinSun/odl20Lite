#pragma once
// odl/core/units.hpp — the ONE place kilometres and metres meet.
//
// WHY THIS FILE EXISTS, and it is a condition rather than a convenience.
// `SPEC-frames.md` §3.6 puts `State` in kilometres — because `SPEC-ephemerides`
// asks CALCEPH for km at every call site, so that no conversion factor appears
// in this tree's source at all — and `Position<F>` / `Acceleration<F>` in
// metres, because EGM2008 publishes its reference radius as 6378136.3 m. Each
// module uses the unit its normative source publishes, which is the same
// principle reaching two answers rather than an inconsistency.
//
// But the split is only safe if the CROSSING is named. Somewhere a sum of
// accelerations in m s^-2 becomes the derivative of a State in km, and if that
// is left to arrive with the first force it arrives once per force: one
// conversion site becomes six, and five of them are somebody's afternoon. So it
// is named here, before there is more than one caller, with its own test, and
// `FRAME-R-062` binds `SPEC-dynamics` to state WHERE the crossing happens before
// any force is integrated.
//
// There is no operator, no implicit conversion and no generic `convert<>`. A
// caller writes the function's name, and the name says which way it goes.

#include <odl/core/vec3.hpp>

namespace odl {

/// Exactly, by definition of the kilometre.
inline constexpr double kMetresPerKilometre = 1000.0;

[[nodiscard]] constexpr double metres_from_km(double km) noexcept {
    return km * kMetresPerKilometre;
}
[[nodiscard]] constexpr double km_from_metres(double m) noexcept {
    return m / kMetresPerKilometre;
}
[[nodiscard]] constexpr Vec3 metres_from_km(const Vec3& km) noexcept {
    return Vec3{metres_from_km(km.x), metres_from_km(km.y), metres_from_km(km.z)};
}
[[nodiscard]] constexpr Vec3 km_from_metres(const Vec3& m) noexcept {
    return Vec3{km_from_metres(m.x), km_from_metres(m.y), km_from_metres(m.z)};
}

/// THE CROSSING. A force model's acceleration, in m s^-2, expressed in the units
/// the state vector is integrated in. Spelt out rather than folded into
/// `km_from_metres` so that a reader of the integrator sees what is being
/// converted and not merely that something is.
[[nodiscard]] constexpr Vec3 state_accel_km_s2_from_m_s2(const Vec3& a_m_s2) noexcept {
    return km_from_metres(a_m_s2);
}

/// The same crossing in the other direction, for a position handed from the
/// state vector to a force model.
[[nodiscard]] constexpr Vec3 field_position_m_from_state_km(const Vec3& r_km) noexcept {
    return metres_from_km(r_km);
}

}  // namespace odl
