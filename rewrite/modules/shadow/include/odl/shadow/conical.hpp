#pragma once
// odl/shadow/conical.hpp — the eclipse shadow function, spherical Earth, conical.
//
// SPEC-shadow.md.  "CONICAL SHADOW" IS A FAMILY, NOT A MODEL, and two correct
// implementations of different members disagree by a factor nobody can find.
// This is Li, Ziebart, Bhattarai & Harrison (2019)'s SECM, and its five choices
// are named here beside the code that makes them:
//
//   the Earth's figure   SPHERE               -- the S in SECM; the ellipsoid is
//                                               the PPM's entire purpose
//   the Sun              A DISC of finite angular radius, never a point source
//   which states         UMBRA, PENUMBRA AND ANNULAR -- the paper's Fig. 2: "all
//                                               the possible eclipse states
//                                               including annular umbra", against
//                                               the CYM which "can only describe
//                                               umbra"
//   Fs in the penumbra   THE TRUE OCCULTED-AREA RATIO -- "the ratio of the
//                                               unblocked solar disk area to the
//                                               area of the whole solar disk".
//                                               NOT linear in the occulted
//                                               fraction and NOT a smoothstep.
//   the atmosphere       NONE                 -- SECM_atm is a separate variant
//
// A reader who needs a different member of the family must change this file and
// will see which choice they are changing.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/frames/vector.hpp>

namespace odl::shadow {

using ShadowError = odl::Diagnostic;

/// The model's own constants. They are NOT arguments: a caller free to vary the
/// occulting radius is a caller free to make this a different model (SHDW-R-005).
inline constexpr double kEarthRadiusM = 6378137.0;
inline constexpr double kSunRadiusM   = 6.957e8;

/// Which state the geometry is in. Reported alongside Fs so that a consumer can
/// tell "fully lit" from "penumbral but nearly lit", which the number alone does
/// not distinguish at the boundary.
enum class State { sunlight, penumbra, umbra, annular };

struct ShadowResult {
    double fraction = 1.0;     ///< Fs, in [0,1]
    State state = State::sunlight;
};

/// Fs for a satellite at `sat`, with the Sun's centre at `sun`, both geocentric
/// and in metres.
[[nodiscard]] odl::Result<ShadowResult, ShadowError>
conical(const frames::Position<frames::Frame::GCRS>& sun,
        const frames::Position<frames::Frame::GCRS>& sat);

/// THE UMBRA CONE'S APEX, and the reason the annular branch above cannot be
/// reached from any orbit in this plan (SHDW-R-002, SHDW-P-1).
///
///     L = Re * d / (Rs - Re) = 1.3842e6 km
///
/// An annular eclipse OF THE SUN BY THE EARTH needs the satellite BEYOND that,
/// where the umbra has closed to a point. GNSS at 26 560 km is at 1.9% of it and
/// the umbra there is still 6 256 km across; the apex is 52x further out. The
/// branch is implemented because it is in the model, and it is exercised only
/// from a SYNTHETIC geometry -- see SHDW-A-005, whose name says so. A branch that
/// passes because nothing reaches it is the guard that cannot fire in a
/// different hat, and the next reader must be able to tell design from neglect.
[[nodiscard]] double umbra_apex_distance_m() noexcept;

}  // namespace odl::shadow
