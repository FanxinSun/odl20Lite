#pragma once
// odl/shadow/perspective.hpp — LI19's PPM: the shadow function by perspective
// projection, an ELLIPSOIDAL Earth, and optionally the atmosphere.
//
// SPEC-shadow.md §4.  This is the other member of the family `conical.hpp`
// implements, and it differs in every one of the five choices that file names:
//
//   the Earth's figure   AN ELLIPSOID     -- x^T A x = 1, A = diag(a^-2,a^-2,b^-2).
//                                           LI19 §1: "Earth is closer to an
//                                           ellipsoid than a sphere", which is
//                                           this model's entire purpose.
//   the Sun              A SPHERE, hence a CIRCLE on the image plane (LI19 eq 28)
//   which states         SUNLIGHT, PENUMBRA, UMBRA.  There is NO annular branch
//                                           here: `conical.hpp` has one because
//                                           the SECM's geometry admits one, and
//                                           this model reaches the same states
//                                           through areas rather than through
//                                           angular radii.
//   Fs in the penumbra   THE TRUE OCCULTED-AREA RATIO, as the SECM -- but of a
//                                           CIRCLE against a CONIC, not against
//                                           another circle.
//   the atmosphere       SELECTABLE       -- `Atmosphere::none` is LI19's PPM,
//                                           `Atmosphere::linear_toa` its PPM_atm.
//
// THE FRAME IS NOT INTERCHANGEABLE WITH `conical.hpp`'s, and that is why it is
// in the type (plan §5 constraint 10).  A = diag(a^-2, a^-2, b^-2) is the Earth
// ellipsoid ONLY in an Earth-fixed frame; in GCRS the same matrix is an
// ellipsoid fixed in inertial space, which is not a body.  `conical` takes GCRS
// because a sphere is a sphere in every frame and the question never arises.
// The signatures differ so that the mistake cannot compile.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/frames/vector.hpp>
#include <odl/shadow/conical.hpp>

namespace odl::shadow {

/// WGS 84.  `kEarthRadiusM` in `conical.hpp` is this same equatorial value, so
/// the two models differ in the POLAR radius alone and a comparison between them
/// measures oblateness and nothing else.
inline constexpr double kEarthEquatorialRadiusM = 6378137.0;
inline constexpr double kEarthPolarRadiusM      = 6356752.314245;

/// LI19 §2.6: "The interactions between the solar radiation and the atmosphere
/// above the stratosphere (at about 50 km altitude) are negligibly small for SRP
/// modelling (Robertson, 2015)."  The TOA is that ellipsoid, wrapping the solid
/// Earth at constant height.
inline constexpr double kAtmosphereThicknessM = 50.0e3;

enum class Atmosphere {
    none,        ///< LI19's PPM
    linear_toa,  ///< LI19's PPM_atm: f(h) linear in depth, 1 at the TOA, 0 at the solid Earth
};

/// What the Earth's silhouette turned out to be.  REPORTED, because it is the
/// one thing about this model that surprises a reader of the conical one, it is
/// not recoverable from Fs, and a run that never produced a hyperbola has not
/// exercised half the code (LI19 eq 24).
enum class Silhouette {
    absent,     ///< the image plane is in front of the Earth: LI19's "no image", full phase
    ellipse,    ///< |B| > 0
    hyperbola,  ///< |B| < 0 -- the PARTIAL-image case, and it is not exotic
};

struct PerspectiveResult {
    double fraction = 1.0;
    State state = State::sunlight;
    Silhouette silhouette = Silhouette::absent;
};

/// Fs for a satellite at `sat`, Sun's centre at `sun`, both EARTH-FIXED and in
/// metres.
[[nodiscard]] odl::Result<PerspectiveResult, ShadowError>
perspective(const frames::Position<frames::Frame::ITRS>& sun,
            const frames::Position<frames::Frame::ITRS>& sat,
            Atmosphere atmosphere = Atmosphere::none);

namespace detail {

/// The same model over a STATED ellipsoid, which is how the degeneracy is
/// demonstrated rather than asserted: at a == b this must reproduce `conical`,
/// whose agreement with an independent integration is already gated.
///
/// It is in `detail` and not on the public surface for SHDW-R-005's reason -- a
/// caller free to vary the occulting body's shape is a caller free to make this
/// a different model.  Designed general, demonstrated degenerate; the generality
/// is the test's, not the caller's.
[[nodiscard]] odl::Result<PerspectiveResult, ShadowError>
perspective_on(const Vec3& sun_m, const Vec3& sat_m, double equatorial_m, double polar_m,
               Atmosphere atmosphere, double image_plane_distance_m);

}  // namespace detail
}  // namespace odl::shadow
