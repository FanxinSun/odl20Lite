#pragma once
// odl/frames/transform.hpp — GCRS <-> ITRS and TEME, by IAU 2006/2000A.
//
// SPEC-frames.md §4.1.  The chain, in the direction this interface exposes:
//
//   M_GCRS_to_ITRS(t) = W(t) . R(t) . Q(t)
//     Q  GCRS  -> CIRS   precession-nutation and frame bias   TN36 eq. (5.10)
//     R  CIRS  -> TIRS   Earth rotation, R3(ERA)              TN36 eq. (5.5)
//     W  TIRS  -> ITRS   polar motion                         TN36 eq. (5.3)
//
// TN36 eq. (5.1) writes the inverse; the two are transposes.
//
// eraC2t06a would do the whole chain in one call and IS NOT USED (FRAME-R-017):
// it takes no dX, dY, so it cannot apply the observed celestial pole offsets,
// which are a few tenths of a milliarcsecond — about 10 mm at 7000 km. It is
// called in the test suite instead, with dX = dY = 0, as an independent check of
// the composition.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/eop/record.hpp>
#include <odl/frames/state.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

namespace odl::frames {

using FrameError = odl::Diagnostic;

/// A rotation between two frames at an epoch, with the angular velocity of the
/// target frame relative to the source.  Returned together because the velocity
/// transformation is not a matrix product and a 6x6 would suggest it were.
struct Rotation {
    Mat3 m;                 ///< source -> target, the whole chain
    Mat3 pre;               ///< source -> the spinning intermediate frame (TIRS / PEF)
    Mat3 post;              ///< that intermediate -> target (polar motion); m = post . pre
    Vec3 omega_rad_s{};     ///< Earth rotation, IN INTERMEDIATE COMPONENTS

    /// The split is not decoration.  The Earth spins about the CIP, which is the
    /// z-axis of TIRS and NOT quite the z-axis of ITRS — polar motion separates
    /// them by about 0.3 arcsec.  So the transport term omega x r must be formed
    /// in the intermediate frame and then rotated, not formed in the target
    /// frame with omega assumed along its z.  Getting that wrong is invisible in
    /// position and worth about 1 mm/s in velocity at 10000 km, which is how it
    /// was found: it was the residual left on Vallado's published example after
    /// everything else had been accounted for.
};

/// The composed chain, GCRS -> ITRS.
[[nodiscard]] odl::Result<Rotation, FrameError> gcrs_to_itrs(
    const odl::time::Epoch& when, const odl::eop::EopRecord& eop,
    const odl::time::LeapTable& leaps);

/// TEME -> ITRS, per Vallado, Crawford, Hujsak & Kelso (AIAA 2006-6753) eq. (1):
/// r_PEF = R3(theta_GMST82) r_TEME, then polar motion to ITRS. The paper's
/// recommendation is exactly this — "rotate to PEF using GMST, and then rotate
/// to other standard coordinate frames" — and the equinox-based route through
/// TOD is NOT implemented, because the paper enumerates three independent
/// ambiguities in it (how many nutation terms, whether the post-1996 kinematic
/// terms are in the equation of the equinoxes, which small-angle approximations)
/// and there is no public basis for choosing among them.
[[nodiscard]] odl::Result<Rotation, FrameError> teme_to_itrs(
    const odl::time::Epoch& when, const odl::eop::EopRecord& eop,
    const odl::time::LeapTable& leaps);

// --- state transforms ------------------------------------------------------
// One named function per ordered pair.  There is no generic "convert to frame X"
// taking a runtime tag, because that is the operation FRAME-R-004 forbids.

[[nodiscard]] odl::Result<ItrsState, FrameError> to_itrs(
    const GcrsState& s, const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps);
[[nodiscard]] odl::Result<GcrsState, FrameError> to_gcrs(
    const ItrsState& s, const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps);
[[nodiscard]] odl::Result<ItrsState, FrameError> to_itrs(
    const TemeState& s, const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps);
[[nodiscard]] odl::Result<TemeState, FrameError> to_teme(
    const ItrsState& s, const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps);
[[nodiscard]] odl::Result<GcrsState, FrameError> to_gcrs(
    const TemeState& s, const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps);

/// What model this is, emitted with every run (FRAME-R-002). A numeric baseline
/// frozen under one model version must not be compared against another without
/// the comparison recording both.
struct ModelVersion {
    std::string_view precession = "IAU 2006";
    std::string_view nutation = "IAU 2000A";
    std::string_view origin = "CIO";
    std::string_view erfa;
};
[[nodiscard]] ModelVersion model_version() noexcept;

}  // namespace odl::frames
