#pragma once
// odl/drag/geodetic.hpp — WGS84 ITRS position to geodetic latitude, longitude,
// altitude.
//
// A UTILITY, NOT DRAG PHYSICS, kept local to this module because drag is its
// only consumer today (atmosphere::Place wants geodetic coordinates, and
// nothing else in this tree currently needs an ECEF-to-geodetic conversion).
// If a second consumer arrives, this is a candidate to move to `frames`, not
// before -- building it general purpose ahead of a second use is the same
// over-building the tree has refused elsewhere (plan §5 constraint 7's own
// argument, one level down from a layer to a utility).
//
// WGS84's own defining constants, not re-derived: a = 6378137.0 m is the
// semi-major axis SHDW already cites (`kEarthEquatorialRadiusM`) and
// `SPEC-gravity` GRAV-R-006 states the same way, from the pinned
// `wgs84-standard` manifest entry (NGA.STND.0036); b = 6356752.3142 m is
// Table 3.6 of the same entry, the same b and the same table `SPEC-gravity`'s
// own §9 provenance register cites for `GRAV-A-012`.

#include <odl/core/vec3.hpp>

#include <cmath>
#include <numbers>

namespace odl::drag {

inline constexpr double kWgs84SemiMajorM = 6378137.0;       ///< a, NGA.STND.0036
inline constexpr double kWgs84SemiMinorM = 6356752.3142;    ///< b, NGA.STND.0036 Table 3.6

struct Geodetic {
    double latitude_rad = 0.0;    ///< geodetic (not geocentric) latitude
    double longitude_rad = 0.0;
    double altitude_m = 0.0;      ///< height above the WGS84 ellipsoid
};

/// Bowring's iterative method, standard and simple to verify by round trip:
/// geodetic -> ECEF -> geodetic returns the input to within the iteration's
/// own tolerance. Converges in 2-3 iterations for any altitude a satellite
/// occupies (Bowring 1976; the method is textbook-standard, e.g. Vallado
/// *Fundamentals of Astrodynamics and Applications* Algorithm 12 gives the
/// same iteration). Verified here against trivial closed-form cases (the
/// equator and the poles, where the ellipsoid's own symmetry gives the answer
/// without iterating at all) rather than against a second worked example,
/// because those closed forms are exact identities, not a comparator that
/// could itself be wrong.
[[nodiscard]] inline Geodetic itrs_to_geodetic(const Vec3& r_itrs_m) noexcept {
    constexpr double a = kWgs84SemiMajorM, b = kWgs84SemiMinorM;
    constexpr double e2 = 1.0 - (b * b) / (a * a);         // first eccentricity squared
    constexpr double ep2 = (a * a) / (b * b) - 1.0;        // second eccentricity squared

    const double p = std::hypot(r_itrs_m.x, r_itrs_m.y);
    const double lon = std::atan2(r_itrs_m.y, r_itrs_m.x);

    if (p < 1.0) {
        // On (or within 1 m of) the polar axis: longitude is undefined and
        // latitude is exactly +-90 deg, the closed-form case Bowring's
        // iteration does not need and would divide by ~zero approaching.
        const double lat = r_itrs_m.z >= 0.0 ? std::numbers::pi / 2.0 : -std::numbers::pi / 2.0;
        const double alt = std::abs(r_itrs_m.z) - b;
        return Geodetic{lat, 0.0, alt};
    }

    // Bowring's starting value and iteration.
    double beta = std::atan2(r_itrs_m.z * a, p * b);
    double lat = 0.0;
    for (int i = 0; i < 6; ++i) {
        const double sin_b = std::sin(beta), cos_b = std::cos(beta);
        lat = std::atan2(r_itrs_m.z + ep2 * b * sin_b * sin_b * sin_b,
                         p - e2 * a * cos_b * cos_b * cos_b);
        beta = std::atan2(b * std::sin(lat), a * std::cos(lat));
    }
    const double sin_lat = std::sin(lat);
    const double N = a / std::sqrt(1.0 - e2 * sin_lat * sin_lat);   // prime vertical radius
    const double alt = (std::abs(std::cos(lat)) > 1.0e-12) ? (p / std::cos(lat) - N)
                                                            : (std::abs(r_itrs_m.z) - b);
    return Geodetic{lat, lon, alt};
}

/// The inverse, needed only to verify the round trip.
[[nodiscard]] inline Vec3 geodetic_to_itrs(const Geodetic& g) noexcept {
    constexpr double a = kWgs84SemiMajorM, b = kWgs84SemiMinorM;
    constexpr double e2 = 1.0 - (b * b) / (a * a);
    const double sin_lat = std::sin(g.latitude_rad), cos_lat = std::cos(g.latitude_rad);
    const double N = a / std::sqrt(1.0 - e2 * sin_lat * sin_lat);
    return Vec3{(N + g.altitude_m) * cos_lat * std::cos(g.longitude_rad),
                (N + g.altitude_m) * cos_lat * std::sin(g.longitude_rad),
                (N * (1.0 - e2) + g.altitude_m) * sin_lat};
}

}  // namespace odl::drag
