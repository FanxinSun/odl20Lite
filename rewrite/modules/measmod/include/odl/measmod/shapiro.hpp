#pragma once
// odl/measmod/shapiro.hpp — the relativistic range term (the gravitational delay of a light leg).
//
// SPEC-measmod.md §4.4 (MEAS-R-024), §7 (MEAS-F-010), §8.3 (G4).
//
// TN36 §11.2 equation (11.17), with the Earth the only body ("for near-Earth satellites … the only body to be
// considered is the Earth"): Δ = (2 GM⊕/c²) ln[(r₁ + r₂ + ρ)/(r₁ + r₂ − ρ)], r₁ and r₂ the geocentric distances of the
// two ends of the leg and ρ their separation. GM⊕ is the TT-compatible value (TN36 §1.1), because every length and time
// of the range model is TT-compatible; c is the defining constant.

#include <odl/core/result.hpp>

namespace odl::measmod {

using MeasError = odl::Diagnostic;

/// TN36 Table 1.1 / §1.1: GM⊕, TT-compatible, m³ s⁻² (the TCG-compatible value 3.986 004 418 × 10¹⁴ × (1 − L_G)).
inline constexpr double kGmEarthTt_m3_s2 = 3.986004415e14;
/// The speed of light, m s⁻¹ (defining).
inline constexpr double kSpeedOfLight_m_s = 299792458.0;

struct ShapiroLeg {
    double delay_m = 0.0;   ///< the extra path of the leg, metres (the equation's time of propagation is delay_m / c)
    double d_rho = 0.0;     ///< ∂delay/∂ρ
    double d_r1 = 0.0;      ///< ∂delay/∂r₁ (= ∂delay/∂r₂)
    double d_r2 = 0.0;      ///< ∂delay/∂r₂
};

/// MEAS-F-010 when r₁ or r₂ is not positive, ρ is negative, or r₁ + r₂ ≤ ρ: a leg that passes through the Earth's centre
/// (the logarithm's argument is not finite) is a target below the horizon.
[[nodiscard]] odl::Result<ShapiroLeg, MeasError> shapiro_leg(double r1_m, double r2_m, double rho_m);

}  // namespace odl::measmod
