#pragma once
// odl/frames/local.hpp — RTN and DYB.
//
// These are ROTATIONS, not frames a state is stored in, and that is why they are
// not members of `Frame`.  SPEC-frames FRAME-R-043: RTN is for expressing
// differences, covariances and empirical accelerations at an epoch; its basis
// rotates at the orbital rate, which is not a small quantity, so it is not a
// frame to integrate in and the type system should not offer it as one.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>

namespace odl::frames {

using FrameError = odl::Diagnostic;

/// Radial, Transverse, Normal — in that order, and under that name only.
///
///   e_R = r / |r|                  radial, outward
///   e_N = (r x v) / |r x v|        along the orbit angular momentum
///   e_T = e_N x e_R                completing the right-handed set
///
/// FRAME-R-041: no RSW, RIC, NTW or QSW aliases are offered. Those names denote
/// overlapping but not identical conventions in the literature and an alias is
/// an invitation to a silent axis swap; a source using another name is
/// translated explicitly where it is read.
///
/// FRAME-R-042: e_T IS NOT THE VELOCITY DIRECTION except on a circular orbit.
/// The angle between them is the flight-path angle, arctan[e sin nu/(1+e cos nu)],
/// which reaches 24° at e = 0.7.
[[nodiscard]] odl::Result<Mat3, FrameError> rtn_basis(const Vec3& r, const Vec3& v);

/// Sun-oriented, for the empirical radiation-pressure models.
///
///   e_D = (r_Sun - r_sat) / |...|        SPACECRAFT TOWARDS THE SUN
///   e_Y = (e_D x e_r) / |...|            the solar-panel rotation axis
///   e_B = e_D x e_Y
///
/// FRAME-R-050: the sense of e_D is stated at every point a DYB component is
/// reported, because the opposite convention is also in use and the sign error
/// it produces is a sign error in the estimated SRP scale — which a fit absorbs
/// without complaint and which surfaces only when a coefficient is compared
/// against a published one.
///
/// The primary sources for this frame (Beutler et al. 1994; Arnold et al. 2015)
/// could not be obtained, so the convention here is stated from first principles
/// rather than inherited. See FRAME-Q-002.
[[nodiscard]] odl::Result<Mat3, FrameError> dyb_basis(const Vec3& r_sat, const Vec3& r_sun);

}  // namespace odl::frames
