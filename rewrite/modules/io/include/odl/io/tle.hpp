#pragma once
// odl/io/tle.hpp — the NORAD Two-Line Element set, as read.
//
// SPEC-io-formats.md §3.2/§8. Column layout: CelesTrak, "NORAD Two-Line
// Element Set Format" (`TLEFMT`, celestrak.org/NORAD/documentation/tle-fmt.php).
// Propagation itself (SGP4) is L6 step 3's own module, not here.

#include <odl/core/result.hpp>

#include <optional>
#include <string>

namespace odl::io {

using TleError = odl::Diagnostic;

/// One TLE, both lines' own fields. Angles in degrees, matching the format's
/// own printed unit -- converting to radians is a propagator concern
/// (L6 step 3), not this reader's.
struct Tle {
    int satellite_number = 0;              ///< line 1, cols 3-7; line 2, cols 3-7 (must agree)
    char classification = 'U';             ///< line 1, col 8
    /// line 1, cols 10-11/12-14/15-17. A SYNTHETIC or test satellite carries
    /// no real launch designator at all -- `STR3` §13's own sample element
    /// set (satellite 88888) leaves these blank, not zero-filled -- so
    /// `nullopt` is a legitimate, distinct value from `0`, not a parse
    /// failure this reader refuses.
    std::optional<int> intl_designator_year;
    std::optional<int> intl_designator_number;
    std::string intl_designator_piece;     ///< line 1, cols 15-17, trimmed (empty if absent)
    int epoch_year = 0;                    ///< line 1, cols 19-20 (2-digit)
    double epoch_day = 0.0;                ///< line 1, cols 21-32, day-of-year + fraction
    double mean_motion_dot = 0.0;          ///< line 1, cols 34-43, rev/day^2
    double mean_motion_ddot = 0.0;         ///< line 1, cols 45-52, rev/day^3 (decimal-assumed + exponent)
    double bstar = 0.0;                    ///< line 1, cols 54-61, 1/earth-radii (decimal-assumed + exponent)
    int ephemeris_type = 0;                ///< line 1, col 63
    int element_number = 0;                ///< line 1, cols 65-68
    double inclination_deg = 0.0;          ///< line 2, cols 9-16
    double raan_deg = 0.0;                 ///< line 2, cols 18-25
    double eccentricity = 0.0;             ///< line 2, cols 27-33, decimal assumed
    double arg_perigee_deg = 0.0;          ///< line 2, cols 35-42
    double mean_anomaly_deg = 0.0;         ///< line 2, cols 44-51
    double mean_motion_rev_per_day = 0.0;  ///< line 2, cols 53-63
    long revolution_number = 0;            ///< line 2, cols 64-68
};

/// `TLEFMT`'s own modulo-10 checksum: digits sum at their face value, '-'
/// counts as 1, every other character (letters, '.', '+', blanks) as 0.
/// `line_without_checksum` is the 68-character line body, checksum digit
/// excluded.
[[nodiscard]] int tle_checksum(std::string_view line_without_checksum) noexcept;

[[nodiscard]] odl::Result<Tle, TleError> read_tle(std::string_view line1, std::string_view line2);
[[nodiscard]] odl::Result<std::pair<std::string, std::string>, TleError> write_tle(const Tle& tle);

[[nodiscard]] bool operator==(const Tle& a, const Tle& b) noexcept;

}  // namespace odl::io
