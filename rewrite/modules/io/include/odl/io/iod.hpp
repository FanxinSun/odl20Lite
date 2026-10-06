#pragma once
// odl/io/iod.hpp — the IOD optical observation format, as read.
//
// SPEC-io-formats.md §3.5. Source: Lewis, G. D., "IOD Observation Format
// Description", Version 0, 10 October 1998, clarified 24 February 2002
// (`IODFMT`, satobs.org/position/IODformat.html). Fixed-column, 80 characters.

#include <odl/core/result.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace odl::io {

using IodError = odl::Diagnostic;

/// `IODFMT`'s own seven angle-format codes (column 45): three RA/DEC shapes
/// (1,2,3,7) at decreasing angular resolution, three AZ/EL shapes (4,5,6)
/// likewise. Each implies a DIFFERENT internal sub-structure for the 14-
/// column angle field (cols 48-61) -- decoding that sub-structure into a
/// uniform RA/DEC or AZ/EL degree pair is this format's own first real
/// consumer's job (L8's optical campaigns, not yet built, `IOFM-Q-004`), so
/// this reader validates the code and preserves the raw 14 columns rather
/// than guessing at a precision no consumer has named yet.
enum class IodAngleFormat { RaDecArcsec = 1, RaDecArcmin = 2, RaDecDeg = 3,
                            AzElArcsec = 4, AzElArcmin = 5, AzElDeg = 6, RaDecMixed = 7 };

struct IodObservation {
    std::string designation;          ///< cols 1-15, trimmed: object number + international designation, as one field
    int station_number = 0;           ///< cols 17-20
    std::optional<char> station_status;  ///< col 22, absent if blank
    int year = 0, month = 0, day = 0;    ///< cols 24-31 (YYYYMMDD)
    int hour = 0, minute = 0;            ///< cols 32-35
    double second = 0.0;                 ///< cols 36-40, SS.sss (millisecond precision)
    std::string time_uncertainty;        ///< cols 42-43, raw mantissa-exponent text (§3.5 -- not decoded, no consumer yet)
    IodAngleFormat angle_format = IodAngleFormat::RaDecArcsec;  ///< col 45
    std::optional<int> epoch_code;       ///< col 46: 0/blank=of-date, 1=1855 ... 6=2050
    std::string angle_raw;               ///< cols 48-61, raw 14 columns, shape depends on angle_format
    std::string positional_uncertainty;  ///< cols 63-64, raw (§3.5 -- not decoded)
    std::optional<char> optical_behavior;  ///< col 66
    std::optional<double> visual_magnitude;  ///< cols 67-70 (sign + 3 digits, implied decimal before the last)
    std::string magnitude_uncertainty;     ///< cols 72-73, raw (§3.5 -- not decoded)
    std::string flash_period;              ///< cols 75-80, raw (§3.5 -- not decoded, scaling not established this round)
};

[[nodiscard]] odl::Result<IodObservation, IodError> read_iod(std::string_view line);

// ---- v1.1 (SPEC-io-formats.md §3.5.1): the decoded angles and uncertainties ------------------------------------------------------------------------------------------

/// Whether a line's two angles are right ascension and declination (formats 1, 2, 3, 7) or azimuth and elevation (formats 4, 5, 6).
enum class IodAngleKind { RaDec, AzEl };

/// The two angles of an IOD line, radians: right ascension (or azimuth) in [0, 2π), declination (or elevation) in [−π/2, π/2].
struct IodAngles {
    IodAngleKind kind = IodAngleKind::RaDec;
    double first_rad = 0.0;
    double second_rad = 0.0;
};

/// IOFM-R-011: columns 48–61 by the format of column 45, blanks as zeros. IOFM-F-016: a non-digit that is not a blank in a digit position, a sign that is not `+` or `-`, a minutes
/// or seconds field of 60 or more, an hours field of 24 or more, an azimuth above 360° or a declination or elevation above 90°, a format code that is blank or not one of the seven.
/// The first overload takes the format code as the character of column 45, so that a blank one can be refused as the line it came from would have it.
[[nodiscard]] odl::Result<IodAngles, IodError> decode_iod_angles(char format_code, std::string_view angle_raw);
[[nodiscard]] odl::Result<IodAngles, IodError> decode_iod_angles(const IodObservation& obs);

/// IOFM-R-012: `MX` = mantissa digit and exponent digit, valued M × 10^(X−8): seconds for the time uncertainty (columns 42–43), and for the positional uncertainty (columns 63–64)
/// seconds, arcminutes or degrees of arc by the format, returned in radians. Two blanks (an empty field) mean "not reported". IOFM-F-017: a digit and a blank, or a non-digit.
[[nodiscard]] odl::Result<std::optional<double>, IodError> decode_iod_time_uncertainty(const IodObservation& obs);
[[nodiscard]] odl::Result<std::optional<double>, IodError> decode_iod_position_uncertainty(const IodObservation& obs);
[[nodiscard]] odl::Result<std::string, IodError> write_iod(const IodObservation& obs);

[[nodiscard]] bool operator==(const IodObservation&, const IodObservation&) noexcept;

}  // namespace odl::io
