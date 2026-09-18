#pragma once
// odl/tides/solid_earth.hpp — TN36-6 §6.2, in the Conventions' three steps.
//
//   Step 1  (6.6) and (6.7): the time-domain evaluation with nominal, frequency
//           independent Love numbers, from the Sun's and Moon's BODY-FIXED
//           positions.  Degrees 2 and 3, and the degree-4 changes the degree-2
//           tides make through k2m^(+).
//   Step 2  (6.8a)-(6.8e): the frequency-dependent corrections, summed over the
//           constituents of Tables 6.5a, 6.5b and 6.5c.
//   Step 3  (6.13)-(6.14): the permanent part removed, because this tree's
//           background field is ZERO TIDE and leaving it in counts the permanent
//           deformation twice.
//
// PERT-R-012: Step 1 is evaluated in the ITRS.  Using GCRS coordinates for the
// Sun and Moon gives a field that rotates with the sky instead of with the
// Earth, and is wrong by the whole of the diurnal signal.

#include <odl/core/result.hpp>
#include <odl/eop/fundamental_arguments.hpp>
#include <odl/frames/vector.hpp>
#include <odl/tides/increments.hpp>

namespace odl::tides {

/// TN36-6 Table 6.3.  PERT-R-011: the anelastic column is the default and the
/// elastic one is offered for comparison, never silently.
enum class LoveNumbers { Anelastic, Elastic };

[[nodiscard]] constexpr const char* name_of(LoveNumbers l) noexcept {
    return l == LoveNumbers::Anelastic ? "anelastic" : "elastic";
}

class SolidEarthTide {
public:
    SolidEarthTide() = delete;

    /// All three steps.  `sun` and `moon` are geocentric positions in the ITRS.
    [[nodiscard]] static odl::Result<TideIncrements, TidesError>
    increments(const frames::Position<frames::Frame::ITRS>& sun,
               const frames::Position<frames::Frame::ITRS>& moon,
               const eop::tides::Arguments& args,
               LoveNumbers love = LoveNumbers::Anelastic,
               TideSystem target = TideSystem::ZeroTide);

    /// Step 1 alone.
    [[nodiscard]] static odl::Result<TideIncrements, TidesError>
    step1(const frames::Position<frames::Frame::ITRS>& sun,
          const frames::Position<frames::Frame::ITRS>& moon, LoveNumbers love);

    /// Step 2 alone, over every constituent of Tables 6.5a/b/c.
    [[nodiscard]] static TideIncrements step2(const eop::tides::Arguments& args);

    /// Step 2 restricted to ONE constituent of one table, which is what
    /// PERT-A-001 needs: evaluated at theta_f = 0 it must return that row's two
    /// printed amplitudes.
    [[nodiscard]] static odl::Result<TideIncrements, TidesError>
    step2_one(int band, std::size_t row, double theta_f);

    /// How many constituents each band carries, so that a test can report its
    /// denominator instead of asserting one.
    [[nodiscard]] static std::size_t constituents(int band) noexcept;

    /// TN36-6 (6.14): the permanent deformation, A0 H0 k20.
    [[nodiscard]] static double permanent_c20(LoveNumbers love) noexcept;

    /// (6.8c) and (6.8d).
    static constexpr double kA0 = 4.4228e-8;        ///< m^-1
    static constexpr double kA1 = -3.1274e-8;       ///< m^-1, (-1)^1 * 3.1274e-8
    static constexpr double kA2 = 3.1274e-8;        ///< m^-1, (-1)^2 * 3.1274e-8
    /// (6.14)'s H0, the Cartwright-Tayler amplitude of the permanent tide.
    static constexpr double kH0 = -0.31460;         ///< m
};

}  // namespace odl::tides
