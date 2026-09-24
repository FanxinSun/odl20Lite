#pragma once
// odl/io/iod.hpp — the IOD optical observation format, as read.
//
// SPEC-io-formats.md §3.5. Source: Lewis, G. D., "IOD Observation Format
// Description", Version 0, 10 October 1998, clarified 24 February 2002
// (`IODFMT`, satobs.org/position/IODformat.html). Fixed-column, 80 characters.

#include <odl/core/result.hpp>

#include <optional>
#include <string>

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
[[nodiscard]] odl::Result<std::string, IodError> write_iod(const IodObservation& obs);

[[nodiscard]] bool operator==(const IodObservation&, const IodObservation&) noexcept;

}  // namespace odl::io
