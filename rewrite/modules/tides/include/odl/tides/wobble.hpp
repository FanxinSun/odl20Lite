#pragma once
// odl/tides/wobble.hpp — the wobble variables, and the pole they are measured from.
//
// TN36-7 (25):   m1 = xp - xs,   m2 = -(yp - ys)
//
// NOTE THE SIGN ON m2.  It is minus (yp - ys), not plus.  A sign error there is
// invisible in the pole tide's amplitude and inverts its phase (PERT-R-007).
//
// PERT-R-006 and GRAV-R-029: the secular pole (xs, ys) is `gravity`'s
// definition, consumed here and never restated.  Two definitions is how the
// static field ends up secular and the pole tide ends up mean.  There is no
// default and no zero: a pole tide computed against an implicit zero secular
// pole is wrong by the whole secular drift, which by 2026 is larger than the
// wobble itself.

#include <odl/core/result.hpp>
#include <odl/gravity/secular_pole.hpp>

namespace odl::tides {

using TidesError = odl::Diagnostic;

class Wobble {
public:
    Wobble() = delete;

    /// `xp`, `yp` are polar motion in RADIANS; `secular` is gravity's.
    [[nodiscard]] static Wobble from(double xp_rad, double yp_rad,
                                     const gravity::PoleCoordinates& secular) noexcept {
        return Wobble{xp_rad - secular.x_rad, -(yp_rad - secular.y_rad)};
    }

    [[nodiscard]] constexpr double m1_rad() const noexcept { return m1_; }
    [[nodiscard]] constexpr double m2_rad() const noexcept { return m2_; }
    [[nodiscard]] double m1_arcsec() const noexcept;
    [[nodiscard]] double m2_arcsec() const noexcept;

private:
    constexpr Wobble(double m1, double m2) noexcept : m1_(m1), m2_(m2) {}
    double m1_, m2_;
};

}  // namespace odl::tides
