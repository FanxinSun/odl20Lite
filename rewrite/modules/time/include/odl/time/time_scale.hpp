#pragma once
// odl/time/time_scale.hpp — the scales, and what is deliberately not one.
//
// SPEC-time.md §3.3: UT1 IS NOT A MEMBER OF THIS ENUMERATION, and its absence is
// the point.  UT1 is the Earth's rotation angle expressed as time: not uniform,
// not predictable, known only where the IERS has published ΔUT1, and never the
// independent variable of a dynamical model.  With UT1 as an ordinary scale,
// `epoch_in(UT1)` is reachable from anywhere and silently defaults ΔUT1 to zero
// — an error of up to 0.9 s, which is 6.75 km of along-track position at LEO.
//
// So UT1 is produced by exactly one operation, `ut1_two_part_jd`, which demands
// ΔUT1 as an argument and returns something only `frames` can consume
// (TIME-R-001, TIME-R-002).

#include <string_view>

namespace odl::time {

enum class TimeScale {
    TAI,   ///< International Atomic Time. The internal scale (TIME-R-010).
    TT,    ///< Terrestrial Time. TT = TAI + 32.184 s exactly.
    TCG,   ///< Geocentric Coordinate Time.
    TDB,   ///< Barycentric Dynamical Time. The argument of the JPL ephemerides.
    TCB,   ///< Barycentric Coordinate Time.
    UTC,   ///< Coordinated Universal Time. NOT uniform; leap seconds.
    GPS,   ///< GPS system time. TAI − 19 s exactly.
};

[[nodiscard]] constexpr std::string_view name_of(TimeScale s) noexcept {
    switch (s) {
        case TimeScale::TAI: return "TAI";
        case TimeScale::TT:  return "TT";
        case TimeScale::TCG: return "TCG";
        case TimeScale::TDB: return "TDB";
        case TimeScale::TCB: return "TCB";
        case TimeScale::UTC: return "UTC";
        case TimeScale::GPS: return "GPS";
    }
    return "?";
}

/// True where the scale advances by SI seconds without interruption.  UTC does
/// not, which is why it is the only scale needing the leap table.
[[nodiscard]] constexpr bool is_uniform(TimeScale s) noexcept {
    return s != TimeScale::UTC;
}

/// An observer's position, for the diurnal term of TDB − TT (TIME-R-021).
/// Reaches about 2.1 µs, which is 16 mm of along-track position at LEO and is
/// therefore not negligible for SLR.
struct Site {
    double east_longitude_rad = 0.0;
    double u_km = 0.0;   ///< distance from the Earth's spin axis
    double v_km = 0.0;   ///< distance north of the equatorial plane
};

}  // namespace odl::time
