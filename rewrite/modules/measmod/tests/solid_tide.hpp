#pragma once
// solid_tide.hpp — the TEST-SIDE solid Earth tide helper of G5's envelope (SPEC-measmod.md §8.9, MEAS-A-101): the IERS Step-1 degree-2 term, TN36 chapter 7 equation (5),
//
//   Δr = Σ_{j = Moon, Sun}  (GM_j R_e⁴)/(GM_⊕ R_j³) · { h₂ r̂ [3 (R̂_j·r̂)² − 1]/2 + 3 l₂ (R̂_j·r̂) [R̂_j − (R̂_j·r̂) r̂] },
//
// with the NOMINAL degree-2 Love and Shida numbers h₂ = 0.6078, l₂ = 0.0847 — and nothing else. It is NOT a model term: the model's `Applied` record still says "station displacement: not
// modelled" (ruling R2), and nothing in `include/` or `src/` reads this file. It centres the envelope on the signed range effect of an omitted term; the terms it omits (the latitude
// dependence of h₂ and l₂, degree 3, the out-of-phase parts, the frequency corrections of Step 2) are its stated uncertainty, sized in SPEC-measmod §8.9 before the helper met the four
// published cases of the IERS routine's prolog.
//
// Constants: TN36 chapter 1, Table 1.1 — R_e = 6 378 136.6 m (the Earth's equatorial radius a_E), GM_⊕ = 3.986 004 418 × 10¹⁴ m³ s⁻², the heliocentric GM_☉ = 1.327 124 420 99 × 10²⁰ m³ s⁻²,
// the Moon-Earth mass ratio μ = 0.012 300 037 1.

#include <odl/core/vec3.hpp>

#include <cmath>

namespace odl::measmod::testing {

struct TideConstants {
    double h2 = 0.6078;                  // the nominal degree-2 Love number (TN36-7 Table 7.2)
    double l2 = 0.0847;                  // the nominal degree-2 Shida number
    double re_m = 6378136.6;             // a_E, TN36 Table 1.1
    double gm_earth = 3.986004418e14;    // m^3 s^-2
    double gm_sun = 1.32712442099e20;    // m^3 s^-2
    double moon_earth_mass_ratio = 0.0123000371;
};

/// The IERS Step-1 degree-2 displacement of a station at `station_m` (any frame, metres: only its direction enters) for the Moon at `moon_m` and the Sun at `sun_m` (the SAME frame, metres, geocentric).
/// The displacement is in that frame, metres.
inline odl::Vec3 solid_tide_step1_degree2(const odl::Vec3& station_m, const odl::Vec3& moon_m, const odl::Vec3& sun_m, const TideConstants& c = TideConstants{}) {
    const odl::Vec3 rhat = (1.0 / station_m.norm()) * station_m;
    odl::Vec3 total{};
    struct Body {
        const odl::Vec3* position;
        double gm_ratio;                 // GM_j / GM_earth
    };
    const Body bodies[2] = {{&moon_m, c.moon_earth_mass_ratio}, {&sun_m, c.gm_sun / c.gm_earth}};
    for (const Body& b : bodies) {
        const double rj = b.position->norm();
        const odl::Vec3 rj_hat = (1.0 / rj) * (*b.position);
        const double cosz = rj_hat.dot(rhat);
        const double scale = b.gm_ratio * std::pow(c.re_m / rj, 3.0) * c.re_m;                       // GM_j R_e^4 / (GM_E R_j^3)
        const odl::Vec3 radial = (c.h2 * (3.0 * cosz * cosz - 1.0) / 2.0) * rhat;
        const odl::Vec3 transverse = (3.0 * c.l2 * cosz) * (rj_hat - cosz * rhat);
        total = total + scale * (radial + transverse);
    }
    return total;
}

/// The range effect of a station displacement `delta_m` on the one-way-equivalent range to a satellite along the unit vector `to_satellite` (station → satellite): the omitted displacement
/// moves the station, so the true range exceeds the modelled one by −ĝ·Δ. This is the signed centre of G5's envelope.
inline double range_effect_of_displacement(const odl::Vec3& to_satellite_unit, const odl::Vec3& delta_m) { return -to_satellite_unit.dot(delta_m); }

}  // namespace odl::measmod::testing
