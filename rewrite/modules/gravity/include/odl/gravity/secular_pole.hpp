#pragma once
// odl/gravity/secular_pole.hpp — ONE definition of the secular pole, exported.
//
// SPEC-gravity GRAV-R-029, and the condition the manager attached to the ruling
// on GRAV-Q-005: *the pole model is defined once, here, and L2 step 3's pole
// tide consumes that definition rather than restating it.  Two definitions is
// how step 2 ends up secular and step 3 ends up mean, and the inconsistency
// would be worth about what the substitution itself is worth* — which §3.7 of
// the specification measures at sixty-one times the truncation error the same
// field accepts.
//
// So this header is part of `gravity`'s public surface for a reason that has
// nothing to do with `gravity` needing it to be.  The four constants appear in
// exactly one place in this tree, checked by GRAV-A-028.
//
// TN36-7 §7.1.4 (21), update of 2018-02-01.  This REPLACED the "mean pole" of
// earlier Conventions; the edition matters, and the manifest pins it by hash.

#include <odl/core/result.hpp>
#include <odl/time/epoch.hpp>

namespace odl::gravity {

using GravityError = odl::Diagnostic;

/// The coordinates of the secular pole, in radians.
struct PoleCoordinates {
    double x_rad = 0.0;
    double y_rad = 0.0;
};

class SecularPole {
public:
    /// TN36-7 (21): xs = 55.0 + 1.677 (t - 2000) mas, ys = 320.5 + 3.460 (t - 2000) mas,
    /// with t the date in years of 365.25 days.
    static constexpr double kX0Mas = 55.0;
    static constexpr double kXRateMasPerYear = 1.677;
    static constexpr double kY0Mas = 320.5;
    static constexpr double kYRateMasPerYear = 3.460;

    /// The span of the least-squares fit TN36-7 §7.1.4 states, and the only
    /// published validity span this tree has for any part of the conventional
    /// model's epoch dependence (GRAV-Q-008).
    static constexpr double kFitFirstYear = 1900.0;
    static constexpr double kFitLastYear  = 2017.0;

    /// GRAV-R-024: the argument is TT, in years of 365.25 days from J2000.0.
    /// GRAV-F-006 refuses outside the fit unless the single named override of
    /// R-ERR-3 is set for this call, in which case the caller has said so and
    /// the run's provenance records it.
    [[nodiscard]] static odl::Result<PoleCoordinates, GravityError>
    at(const odl::time::Epoch& tt, bool extrapolate_secular_terms_beyond_fit = false);

    /// The same, as the plain linear model, for a caller that already holds the
    /// date in years — used by the pole tide of L2 step 3 so that it consumes
    /// this definition rather than restating the four constants.
    [[nodiscard]] static PoleCoordinates at_years(double years_of_365_25_from_2000) noexcept;

    /// TT years of 365.25 days measured from 2000.0, the argument TN36-7 (21)
    /// and TN36-6 (6.4) are both written in.
    [[nodiscard]] static double years_from_2000(const odl::time::Epoch& tt) noexcept;

private:
    SecularPole() = delete;
};

}  // namespace odl::gravity
