#pragma once
// odl/frames/vector.hpp — a position or an acceleration that knows its frame.
//
// WHY HERE AND NOT IN THE MODULE THAT NEEDED IT.  `gravity` takes a position in
// the ITRS and returns an acceleration in the ITRS, and SPEC-gravity GRAV-R-050
// requires both to carry their frame in the type so that a GCRS position cannot
// be handed in by mistake.  The manager's ruling on EPH-Q-001 settled where such
// a type belongs: *the frame enumeration belongs to that specification, and a
// second spec extending it silently is how enumerations drift.*  The same
// argument applies to the things the enumeration labels.
//
// WHY NOT `State`.  A State is a position AND a velocity AT an epoch, in km.
// A point at which to evaluate a static field is none of those: it has no
// velocity, it needs no epoch (the field's epoch dependence is the field's, not
// the point's), and the geopotential's own published scaling parameters are in
// metres.  Reusing State would have meant inventing a velocity and an epoch to
// throw away, which is how a zero becomes a silent default.
//
// UNITS ARE IN THE ACCESSOR NAME, deliberately, and they differ from State's.
// State carries km because SPEC-ephemerides asks CALCEPH for km at every call
// site so that no conversion factor appears in this tree's source at all.  This
// header carries metres because EGM2008's a_e is published as 6378136.3 m.  The
// two are reconciled by the caller, in the open, with the unit named on both
// sides — never by a bare `.position()` flowing into a bare metres argument.

#include <odl/core/vec3.hpp>
#include <odl/frames/state.hpp>

namespace odl::frames {

/// A point, in metres, in a named frame.
template <Frame F>
class Position {
public:
    Position() = delete;   // a vector with no frame is not a position

    explicit constexpr Position(Vec3 metres) noexcept : r_(metres) {}

    [[nodiscard]] constexpr const Vec3& metres() const noexcept { return r_; }
    [[nodiscard]] double norm_m() const noexcept { return r_.norm(); }

    static constexpr Frame frame = F;
    static constexpr std::string_view frame_name = name_of(F);

private:
    Vec3 r_;
};

/// An acceleration, in m s^-2, in a named frame.
template <Frame F>
class Acceleration {
public:
    Acceleration() = delete;

    explicit constexpr Acceleration(Vec3 m_per_s2) noexcept : a_(m_per_s2) {}

    [[nodiscard]] constexpr const Vec3& metres_per_second_squared() const noexcept { return a_; }
    [[nodiscard]] double norm_m_per_s2() const noexcept { return a_.norm(); }

    static constexpr Frame frame = F;
    static constexpr std::string_view frame_name = name_of(F);

private:
    Vec3 a_;
};

using ItrsPosition     = Position<Frame::ITRS>;
using ItrsAcceleration = Acceleration<Frame::ITRS>;
using GcrsPosition     = Position<Frame::GCRS>;
using GcrsAcceleration = Acceleration<Frame::GCRS>;

}  // namespace odl::frames
