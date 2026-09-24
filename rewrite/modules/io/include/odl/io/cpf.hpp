#pragma once
// odl/io/cpf.hpp — the ILRS Consolidated Prediction Format, as read.
//
// SPEC-io-formats.md §3.4. Source: Ricklefs, R. L., for the ILRS Predictions
// Formats Study Group, "Consolidated Laser Target Prediction Format",
// Version 2, 28 February 2018 (`CPF2`). FREE FORMAT, whitespace-tokenised.

#include <odl/core/result.hpp>

#include <string>
#include <vector>

namespace odl::io {

using CpfError = odl::Diagnostic;

enum class CpfDirectionFlag { Common, Transmit, Receive };  ///< `CPF2`'s own 0/1/2

struct CpfHeader1 {   // H1
    int version = 0;
    std::string source;
    int prod_year = 0, prod_month = 0, prod_day = 0, prod_hour = 0;
    int sequence_number = 0;
    int sub_daily_sequence_number = 0;
    std::string target_name;
    std::string notes;
};

struct CpfHeader2 {   // H2
    long ilrs_id = 0;
    long sic = 0;
    long norad_id = 0;
    int start_year = 0, start_month = 0, start_day = 0, start_hour = 0, start_minute = 0, start_second = 0;
    int end_year = 0, end_month = 0, end_day = 0, end_hour = 0, end_minute = 0, end_second = 0;
    int table_step_s = 0;
    int compatible_with_tiv = 0;
    int target_class = 0;
    int reference_frame = 0;
    int rotational_angle_type = 0;
    int com_correction = 0;
    int target_location = 0;
};

/// Position record (`10`): geocentric X/Y/Z at (MJD, seconds-of-day UTC).
struct CpfPositionRecord {
    CpfDirectionFlag direction = CpfDirectionFlag::Common;
    int mjd = 0;
    double seconds_of_day = 0.0;
    int leap_second_flag = 0;
    double x_m = 0.0, y_m = 0.0, z_m = 0.0;
};

/// A record type this reader does not parse field by field (H3-H5, 20/30/
/// .../99, comments) -- tag plus tokens verbatim, the same scope decision
/// SPEC-io-formats.md §3.3 already states for CRD.
struct CpfOpaqueRecord {
    std::string tag;
    std::vector<std::string> fields;
};

struct CpfFile {
    CpfHeader1 header1;
    CpfHeader2 header2;
    std::vector<CpfPositionRecord> positions;
    std::vector<CpfOpaqueRecord> opaque;
};

[[nodiscard]] odl::Result<CpfFile, CpfError> read_cpf(std::string_view text);
[[nodiscard]] odl::Result<std::string, CpfError> write_cpf(const CpfFile& file);

[[nodiscard]] bool operator==(const CpfHeader1&, const CpfHeader1&) noexcept;
[[nodiscard]] bool operator==(const CpfHeader2&, const CpfHeader2&) noexcept;
[[nodiscard]] bool operator==(const CpfPositionRecord&, const CpfPositionRecord&) noexcept;
[[nodiscard]] bool operator==(const CpfOpaqueRecord&, const CpfOpaqueRecord&) noexcept;
[[nodiscard]] bool operator==(const CpfFile&, const CpfFile&) noexcept;

}  // namespace odl::io
