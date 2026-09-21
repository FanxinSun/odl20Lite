#pragma once
// odl/macromodel/body_direction.hpp — a unit vector, tagged as living in the
// satellite's OWN body-fixed frame.
//
// SPEC-macromodel §3: the body frame is not one of frames::Frame's cases --
// every satellite has its own, so there is no shared enum value to check
// against the way FRAME-R checks GCRS against ITRS.  What CAN be checked, and
// is: that this is a unit vector and not some other Vec3 wearing the wrong
// hat.  A silently-renormalised "unit vector" is exactly the plausible-but-
// wrong number plan §5 constraint 4 exists to refuse rather than approximate,
// so construction REFUSES a non-unit input instead of fixing it up.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/macromodel/cited.hpp>

#include <cmath>
#include <string>

namespace odl::macromodel {

/// A unit 3-vector, body-frame.  Opaque: `body_direction()` is the only way in.
class BodyDirection {
public:
    [[nodiscard]] const Vec3& vec() const noexcept { return v_; }

private:
    friend odl::Result<BodyDirection, MacromodelError> body_direction(Vec3 v);

    explicit BodyDirection(Vec3 v) noexcept : v_(v) {}
    Vec3 v_;
};

/// MCRM-F-002.  1e-9 relative is generous against a caller's own construction
/// rounding (e.g. building a normal from spherical angles) while still catching
/// a vector that was never normalised at all, which is the failure this guards.
[[nodiscard]] inline odl::Result<BodyDirection, MacromodelError> body_direction(Vec3 v) {
    const double n = v.norm();
    if (std::abs(n - 1.0) > 1e-9) {
        return odl::err(MacromodelError{"MCRM-F-002",
            "a body-frame direction was not unit length: (" + std::to_string(v.x) + ", " +
            std::to_string(v.y) + ", " + std::to_string(v.z) + "), norm " +
            std::to_string(n) + " (expected 1)"});
    }
    return BodyDirection(v);
}

}  // namespace odl::macromodel
