#pragma once
// odl/ephemerides/body.hpp — bodies as a closed enumeration.
//
// SPEC-ephemerides.md EPH-R-020: integers never cross this boundary, and §3.3
// says why. There are TWO numbering schemes and CALCEPH uses either depending on
// one flag, so the same integer means different bodies:
//
//   value   classic (CALCEPH default)   NAIF integer ID (CALCEPH_USE_NAIFID)
//     1     Mercury barycentre           Mercury barycentre     -- these agree
//     3     EARTH                        Earth-Moon barycentre  -- these do not
//    10     MOON                         SUN                    -- nor these
//
// `testpo.440` is in the CLASSIC scheme; SPK kernels are indexed by NAIF IDs.
// A mistake is not a factor of 1e8 that a magnitude check would find — it is a
// plausible vector for the wrong body.

#include <string_view>

namespace odl::eph {

enum class Body {
    MercuryBarycentre, VenusBarycentre, Earth, MarsBarycentre, JupiterBarycentre,
    SaturnBarycentre, UranusBarycentre, NeptuneBarycentre, PlutoBarycentre,
    Moon, Sun, SolarSystemBarycentre, EarthMoonBarycentre,
};

[[nodiscard]] constexpr std::string_view name_of(Body b) noexcept {
    switch (b) {
        case Body::MercuryBarycentre:    return "Mercury barycentre";
        case Body::VenusBarycentre:      return "Venus barycentre";
        case Body::Earth:                return "Earth";
        case Body::MarsBarycentre:       return "Mars barycentre";
        case Body::JupiterBarycentre:    return "Jupiter barycentre";
        case Body::SaturnBarycentre:     return "Saturn barycentre";
        case Body::UranusBarycentre:     return "Uranus barycentre";
        case Body::NeptuneBarycentre:    return "Neptune barycentre";
        case Body::PlutoBarycentre:      return "Pluto barycentre";
        case Body::Moon:                 return "Moon";
        case Body::Sun:                  return "Sun";
        case Body::SolarSystemBarycentre:return "solar-system barycentre";
        case Body::EarthMoonBarycentre:  return "Earth-Moon barycentre";
    }
    return "?";
}

/// The CLASSIC numbering, which is what CALCEPH expects without
/// CALCEPH_USE_NAIFID and what `testpo.440` uses. Written once, here, and
/// exercised by the gate — EPH-A-001 runs `testpo` cases through this mapping,
/// so the translation is tested rather than assumed (EPH-R-021).
[[nodiscard]] constexpr int classic_id(Body b) noexcept {
    switch (b) {
        case Body::MercuryBarycentre:     return 1;
        case Body::VenusBarycentre:       return 2;
        case Body::Earth:                 return 3;
        case Body::MarsBarycentre:        return 4;
        case Body::JupiterBarycentre:     return 5;
        case Body::SaturnBarycentre:      return 6;
        case Body::UranusBarycentre:      return 7;
        case Body::NeptuneBarycentre:     return 8;
        case Body::PlutoBarycentre:       return 9;
        case Body::Moon:                  return 10;
        case Body::Sun:                   return 11;
        case Body::SolarSystemBarycentre: return 12;
        case Body::EarthMoonBarycentre:   return 13;
    }
    return 0;
}

/// The inverse, for reading `testpo.440`. Returns false for 14-17, which are
/// nutations, librations and the time-scale differences rather than bodies.
[[nodiscard]] constexpr bool body_of_classic_id(int id, Body& out) noexcept {
    for (Body b : {Body::MercuryBarycentre, Body::VenusBarycentre, Body::Earth,
                   Body::MarsBarycentre, Body::JupiterBarycentre, Body::SaturnBarycentre,
                   Body::UranusBarycentre, Body::NeptuneBarycentre, Body::PlutoBarycentre,
                   Body::Moon, Body::Sun, Body::SolarSystemBarycentre,
                   Body::EarthMoonBarycentre}) {
        if (classic_id(b) == id) { out = b; return true; }
    }
    return false;
}

}  // namespace odl::eph
