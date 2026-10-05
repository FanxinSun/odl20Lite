#pragma once
// odl/frames/state.hpp — a state whose frame is part of its type.
//
// SPEC-frames.md FRAME-R-003, FRAME-R-004, and L1's exit gate: a state cannot be
// reinterpreted between frames, ENFORCED BY THE TYPE SYSTEM rather than by a
// test.
//
// WHY THIS SHAPE AND NOT A RUNTIME TAG.  The idiomatic C++ answer is a `Frame`
// enum member on a State struct — which is almost certainly the predecessor's
// shape, and which makes FRAME-R-004 unenforceable, because a tag that is a
// field is a tag anyone can assign.  With the frame as a template parameter
// there is no assignment to make: State<Frame::TEME> and State<Frame::GCRS> are
// unrelated types, no conversion exists between them, and the only way from one
// to the other is a transform function that does the rotation.
//
// The cost of getting this wrong is not subtle.  A frame error is a PURE
// ROTATION: magnitudes are preserved, so every check based on altitude, speed,
// energy or angular momentum passes, and only the direction is wrong.  The
// GCRS-to-TEME rotation grows at about 1.7 km per year of along-track
// displacement at 7000 km, so by now a TEME state mistaken for a GCRS one is
// displaced by tens of kilometres with nothing in its magnitude to show it.

#include <odl/core/units.hpp>
#include <odl/core/vec3.hpp>
#include <odl/time/epoch.hpp>

#include <string_view>

namespace odl::frames {

/// FRAME-R-033's own bound on the pole angle a TEME conversion may be wrong by: 0.1 arcsec, the
/// difference between the IAU-76/80 realisation of the true pole and the IAU 2006/2000A CIP that
/// identifying PEF with TIRS imports. The requirement's figure, not one fitted to T-01 (which
/// measured 50.2 mas against Horizons' conversion, inside it).
inline constexpr double kTemeFloorRad = 0.1 * 3.14159265358979323846 / (180.0 * 3600.0);

enum class Frame {
    BCRS,   ///< Barycentric Celestial Reference System: ICRS axes, solar-system
            ///< barycentre origin. The frame of the planetary ephemerides.
            ///<
            ///< NOT just another member. BCRS <-> GCRS is a TRANSLATION, not a
            ///< rotation — every other transformation here is an orthogonal
            ///< matrix and an angular velocity — and BCRS is TDB-based where
            ///< GCRS is TT-based. SPEC-frames §3.5, FRAME-R-027..029: there is
            ///< deliberately NO `to_gcrs(State<BCRS>, eop, leaps)`, because a
            ///< function shaped like its neighbours gets used like them.
    GCRS,   ///< Geocentric Celestial Reference System. The inertial frame of the dynamics.
    CIRS,   ///< Celestial Intermediate Reference System: CIP as z, CIO as x.
    TIRS,   ///< Terrestrial Intermediate Reference System: CIP as z, TIO as x.
    ITRS,   ///< International Terrestrial Reference System, as realised by the EOP series.
    TEME,   ///< True Equator, Mean Equinox: SGP4's output frame, and nothing else's.
};

[[nodiscard]] constexpr std::string_view name_of(Frame f) noexcept {
    switch (f) {
        case Frame::BCRS: return "BCRS";
        case Frame::GCRS: return "GCRS";
        case Frame::CIRS: return "CIRS";
        case Frame::TIRS: return "TIRS";
        case Frame::ITRS: return "ITRS";
        case Frame::TEME: return "TEME";
    }
    return "?";
}

template <Frame F>
class State {
public:
    State() = delete;   // a state with no epoch and no frame is not a state

    State(odl::time::Epoch when, Vec3 position_km, Vec3 velocity_km_s) noexcept
        : epoch_(when), r_(position_km), v_(velocity_km_s) {}

    [[nodiscard]] const odl::time::Epoch& epoch() const noexcept { return epoch_; }
    [[nodiscard]] const Vec3& position() const noexcept { return r_; }   ///< km
    [[nodiscard]] const Vec3& velocity() const noexcept { return v_; }   ///< km/s

    static constexpr Frame frame = F;
    static constexpr std::string_view frame_name = name_of(F);

    /// FRAME-R-033 (v1.11). A TEME state carries a floor for the one thing the frame's definition
    /// leaves open: WHICH POLE a conversion assumes -- the observed one (with the IERS celestial-pole
    /// offsets) or the model's (without) -- a choice a TLE's producer does not document. The floor is
    /// the spec's own angle, 0.1 arcsec, times THIS state's radius: 3.39 m at 7000 km, 20.4 m at
    /// geostationary radius. It is a bound for a comparator whose convention is unknown, not an
    /// irreducible property: against a named convention the difference is predicted and removed (T-01,
    /// IOSG-A-011: Horizons' conversion of a TLE applies no offsets, this tree's chain differs from it by
    /// 50.2 mas, and a chain with its convention reproduces it to 2 mm).
    /// [Superseded 2026-10-06, kept visible: "A TEME state carries an uncertainty floor it cannot be rid
    /// of", and a constant 3.0 m for every TEME state -- a low-orbit figure the stated angle gives as
    /// 20.4 m at geostationary radius.]
    [[nodiscard]] double frame_uncertainty_floor_m() const noexcept {
        return F == Frame::TEME ? kTemeFloorRad * odl::metres_from_km(r_.norm()) : 0.0;
    }

    /// FRAME-R-028.  The BCRS <-> GCRS step is a translation and says so: it
    /// takes the Earth's barycentric state as an EXPLICIT argument, so a caller
    /// must have obtained it from `ephemerides` and cannot get a silent zero.
    /// Deliberately not named `to_gcrs`, and deliberately not taking an EOP
    /// record — nothing about it resembles the rotations.
    ///
    /// FRAME-R-029: the epoch is unchanged and is NOT rescaled. The TDB/TT
    /// relativistic scaling between the two systems, L_B = 1.55e-8 (2.3 m on an
    /// astronomical unit), is not applied here; a consumer needing
    /// TDB-compatible lengths must say so. See FRAME-Q-006.
    template <Frame G = F>
    [[nodiscard]] auto translated_by(const State<Frame::BCRS>& earth_in_bcrs) const noexcept
        -> State<Frame::GCRS>
        requires (G == Frame::BCRS)
    {
        return State<Frame::GCRS>{epoch_, r_ - earth_in_bcrs.position(),
                                  v_ - earth_in_bcrs.velocity()};
    }

private:
    odl::time::Epoch epoch_;
    Vec3 r_;
    Vec3 v_;
};

using BcrsState = State<Frame::BCRS>;
using GcrsState = State<Frame::GCRS>;
using ItrsState = State<Frame::ITRS>;
using TemeState = State<Frame::TEME>;

}  // namespace odl::frames
