#pragma once
// odl/relativity/correction.hpp — TN36-10 (10.12), the relativistic correction
// to the acceleration of an Earth satellite in the GCRS.
//
// Three terms, and they are kept separable on purpose (PERT-R-042): TN36-10
// §10.3 states their magnitudes separately, the acceptance tests check them
// separately, and "how much is frame dragging worth at this altitude" is a
// question the module should be able to answer rather than one an analyst has
// to answer by editing it.
//
// SIZES, from TN36-10 §10.3, so that a reader knows what is being added:
//   Schwarzschild    a few parts in 1e10 (high orbits) to 1e9 (low) of the main
//                    Newtonian acceleration. Neglecting it while adjusting orbit
//                    parameters appears as a 4 mm reduction in orbit radius.
//   Lense-Thirring   1e-11 to 1e-12; orbital-plane precession 0.8 mas/yr
//                    (geostationary) to 180 mas/yr (low orbit).
//   de Sitter        1e-11 to 1e-12; 19 mas/yr, INDEPENDENT of orbit height.
//
// The relativistic effect of the Earth's oblateness is NOT included, as
// TN36-10 §10.3 says of its own equation (PERT-R-044).

#include <odl/core/result.hpp>
#include <odl/frames/state.hpp>
#include <odl/frames/vector.hpp>

#include <vector>

namespace odl::relativity {

using RelativityError = odl::Diagnostic;

/// PPN parameters.  Both are 1 in general relativity.  They are named rather
/// than folded into the arithmetic because a PPN test is the only reason
/// (10.12) is written with them, and folding them away removes the reason
/// (PERT-R-040).
class PpnParameters {
public:
    PpnParameters() = delete;
    [[nodiscard]] static constexpr PpnParameters general_relativity() noexcept {
        return PpnParameters{1.0, 1.0};
    }
    /// PERT-F-009: they must be stated.  There is no silent default.
    [[nodiscard]] static odl::Result<PpnParameters, RelativityError> of(double beta, double gamma);

    [[nodiscard]] constexpr double beta() const noexcept { return beta_; }
    [[nodiscard]] constexpr double gamma() const noexcept { return gamma_; }
    [[nodiscard]] constexpr bool is_general_relativity() const noexcept {
        return beta_ == 1.0 && gamma_ == 1.0;
    }

private:
    constexpr PpnParameters(double b, double g) noexcept : beta_(b), gamma_(g) {}
    double beta_, gamma_;
};

enum class Term { Schwarzschild, LenseThirring, DeSitter };

[[nodiscard]] constexpr const char* name_of(Term t) noexcept {
    switch (t) {
        case Term::Schwarzschild: return "Schwarzschild";
        case Term::LenseThirring: return "Lense-Thirring";
        case Term::DeSitter:      return "de Sitter";
    }
    return "?";
}

/// Which of the three to include.  Three named flags, not an integer
/// (PERT-R-072): a bitmask invites arithmetic on it.
struct Terms {
    bool schwarzschild = true;
    bool lense_thirring = true;
    bool de_sitter = true;
    [[nodiscard]] static constexpr Terms all() noexcept { return {}; }
    [[nodiscard]] static constexpr Terms only(Term t) noexcept {
        return {t == Term::Schwarzschild, t == Term::LenseThirring, t == Term::DeSitter};
    }
};

struct NamedAcceleration {
    Term term;
    Vec3 a_m_s2;
};

class Correction {
public:
    Correction() = delete;

    /// TN36-10 (10.12).
    ///
    /// `sat` is geocentric — position and velocity about the Earth.
    /// `earth_about_sun` is R and Rdot of (10.12): **the Earth with respect to
    /// the SUN**, not the barycentre and not the Earth with respect to anything
    /// else. PERT-R-041: the two appear in adjacent lines of one equation and
    /// substituting one for the other changes the de Sitter term by orders of
    /// magnitude. It is an explicit argument for the reason FRAME-R-028 makes
    /// the barycentric translation one: a caller must have obtained it.
    [[nodiscard]] static odl::Result<frames::Acceleration<frames::Frame::GCRS>, RelativityError>
    acceleration(const frames::State<frames::Frame::GCRS>& sat,
                 const frames::State<frames::Frame::BCRS>& earth_about_sun,
                 Terms terms = Terms::all(),
                 PpnParameters ppn = PpnParameters::general_relativity());

    /// The same, term by term, in the order Schwarzschild, Lense-Thirring,
    /// de Sitter.  Only the terms `terms` selects appear.
    [[nodiscard]] static odl::Result<std::vector<NamedAcceleration>, RelativityError>
    by_term(const frames::State<frames::Frame::GCRS>& sat,
            const frames::State<frames::Frame::BCRS>& earth_about_sun,
            Terms terms = Terms::all(),
            PpnParameters ppn = PpnParameters::general_relativity());

    /// TN36-1 and TN36-10 §10.3.  Exposed so that a test can state what it is
    /// checking against rather than repeating the number.
    static constexpr double kC = 299792458.0;                 ///< m/s, defining
    static constexpr double kGmEarth = 3.986004418e14;        ///< m^3/s^2, TN36-1
    static constexpr double kGmSun = 1.32712442099e20;        ///< m^3/s^2, TN36-1
    /// |J| ~ 9.8e8 m^2/s, TN36-10 §10.3, which gives the MAGNITUDE only. The
    /// direction is the Earth's rotation axis; it is taken as the GCRS z-axis,
    /// and the difference between that and the true pole is of order 1e-5 rad
    /// on a term that is itself 1e-11 of the main acceleration.
    static constexpr double kEarthAngularMomentumPerMass = 9.8e8;
};

}  // namespace odl::relativity
