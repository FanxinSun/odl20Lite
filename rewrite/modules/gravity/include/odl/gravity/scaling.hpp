#pragma once
// odl/gravity/scaling.hpp — GM and a_e, which belong to the model and not to
// the world.
//
// SPEC-gravity GRAV-R-004: the scaling parameters are properties OF THE
// COEFFICIENT SET.  They must travel with it and must not be read from a general
// constants table.  GRAV-R-006 names the specific trap: WGS 84 publishes
// GM = 3986004.418e8 m^3 s^-2, which is the TCG-compatible value wearing a
// different name, and it is the most available wrong constant in this subject.
// Using it with EGM2008's coefficients costs 108.91 mm at 7331 km over one
// revolution, secularly — see GRAV-P-4, where half-a-T-squared IS the right
// instrument because a constant fractional error in GM does accumulate.
//
// The type therefore has no default constructor and no setters.  There is one
// factory per published, matched pair, and a checked one for anything else.

#include <odl/core/result.hpp>

#include <string>
#include <string_view>

namespace odl::gravity {

using GravityError = odl::Diagnostic;

/// Which time scale a GM value is compatible with (TN36-6 §6.1).
enum class GmCompatibility { TT, TCG, TDB };

[[nodiscard]] constexpr std::string_view name_of(GmCompatibility c) noexcept {
    switch (c) {
        case GmCompatibility::TT:  return "TT";
        case GmCompatibility::TCG: return "TCG";
        case GmCompatibility::TDB: return "TDB";
    }
    return "?";
}

class ScalingParameters {
public:
    ScalingParameters() = delete;

    /// The pair TN36-6 §6.1 and the EGM2008 README both publish, and the pair
    /// GRAV-R-005 selects because this tree integrates in a TT-based GCRS.
    [[nodiscard]] static ScalingParameters egm2008_tt_compatible() noexcept {
        return ScalingParameters{3.986004415e14, 6378136.3, GmCompatibility::TT,
                                 "TN36-6 §6.1 / EGM2008 README (2), TT-compatible"};
    }

    /// TN36-6 §6.1's TCG-compatible GM, for a caller working in TCG.  Offered so
    /// that the legitimate case does not have to go through the checked factory
    /// and so that GRAV-A-017 has something to compare against.
    [[nodiscard]] static ScalingParameters egm2008_tcg_compatible() noexcept {
        return ScalingParameters{3.986004418e14, 6378136.3, GmCompatibility::TCG,
                                 "TN36-6 §6.1, TCG-compatible"};
    }

    /// Anything else.  GRAV-F-005: a GM that is not one of the model's own is
    /// refused, naming both values, both sources and which time scale each GM is
    /// compatible with.
    [[nodiscard]] static odl::Result<ScalingParameters, GravityError>
    checked(double gm_m3_s2, double ae_m, GmCompatibility compat, std::string source);

    [[nodiscard]] double gm_m3_s2() const noexcept { return gm_; }
    [[nodiscard]] double ae_m() const noexcept { return ae_; }
    [[nodiscard]] GmCompatibility compatibility() const noexcept { return compat_; }
    [[nodiscard]] const std::string& source() const noexcept { return source_; }

private:
    ScalingParameters(double gm, double ae, GmCompatibility c, std::string src)
        : gm_(gm), ae_(ae), compat_(c), source_(std::move(src)) {}

    double gm_;
    double ae_;
    GmCompatibility compat_;
    std::string source_;
};

}  // namespace odl::gravity
