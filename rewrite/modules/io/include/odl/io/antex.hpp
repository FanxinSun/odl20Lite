#pragma once
// odl/io/antex.hpp — ANTEX 1.4, the antenna exchange format, as read.
//
// SPEC-io-formats.md §3.7. Source: Rothacher, M., Schmid, R. (TU München),
// "ANTEX: The Antenna Exchange Format, Version 1.4", 15 September 2010
// (`ANTEX14`, files.igs.org/pub/data/format/antex14.txt). RINEX-style: data
// in columns 1-60, a record-type LABEL in columns 61-80 for every record
// except the phase-pattern data rows themselves (§4 of `ANTEX14`).

#include <odl/core/result.hpp>

#include <optional>
#include <string>
#include <vector>

namespace odl::io {

using AntexError = odl::Diagnostic;

enum class AntexPcvType { Absolute, Relative };  ///< header PCV TYPE, 'A'/'R'

struct AntexHeader {
    double version = 0.0;
    char satellite_system = 'M';
    AntexPcvType pcv_type = AntexPcvType::Absolute;
    std::string reference_antenna;   ///< blank if not relative
};

/// One frequency section (`START OF FREQUENCY`...`END OF FREQUENCY`): the
/// mean-phase-centre eccentricity plus the non-azimuth-dependent pattern
/// (always present) and, when the antenna's own `DAZI > 0`, the azimuth-
/// dependent grid. RMS sections (`START OF FREQ RMS`...) are NOT parsed --
/// this reader's own stated scope (§3.7): uncertainty of a calibration this
/// tree does not yet consume.
struct AntexFrequency {
    std::string code;               ///< e.g. "G01"
    double north_mm = 0.0, east_mm = 0.0, up_mm = 0.0;  ///< PCO
    std::vector<double> noazi_mm;   ///< NOAZI row, ZEN1..ZEN2 step DZEN
    /// Each entry: (azimuth_deg, values ZEN1..ZEN2) -- empty if DAZI == 0.
    std::vector<std::pair<double, std::vector<double>>> azimuth_grid_mm;
};

struct AntexAntenna {
    std::string antenna_type;       ///< TYPE / SERIAL NO, first 20 columns, trimmed
    std::string serial_or_sat_code; ///< TYPE / SERIAL NO, second field, trimmed (blank = all representatives)
    double dazi_deg = 0.0;
    double zen1_deg = 0.0, zen2_deg = 0.0, dzen_deg = 0.0;
    int num_frequencies = 0;
    std::vector<AntexFrequency> frequencies;
};

struct AntexFile {
    AntexHeader header;
    std::vector<AntexAntenna> antennas;
};

[[nodiscard]] odl::Result<AntexFile, AntexError> read_antex(std::string_view text);
[[nodiscard]] odl::Result<std::string, AntexError> write_antex(const AntexFile& file);

[[nodiscard]] bool operator==(const AntexHeader&, const AntexHeader&) noexcept;
[[nodiscard]] bool operator==(const AntexFrequency&, const AntexFrequency&) noexcept;
[[nodiscard]] bool operator==(const AntexAntenna&, const AntexAntenna&) noexcept;
[[nodiscard]] bool operator==(const AntexFile&, const AntexFile&) noexcept;

}  // namespace odl::io
