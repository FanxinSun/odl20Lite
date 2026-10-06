#pragma once
// odl/io/crd.hpp — the ILRS Consolidated laser Ranging Data format, as read.
//
// SPEC-io-formats.md §3.3. Source: Ricklefs, R. L., for the ILRS Data Format
// and Procedures Working Group, "Consolidated Laser Ranging Data Format
// (CRD)", Version 2.00/2.01, 19 September 2019 (`CRD2`). FREE FORMAT (`CRD2`
// §0's own words): fields are whitespace-delimited, not column-positioned.

#include <odl/core/result.hpp>
#include <odl/time/time_scale.hpp>

#include <optional>
#include <string>
#include <vector>

namespace odl::io {

using CrdError = odl::Diagnostic;

/// `CRD2`'s own twenty-eight record types (§1-§4). Header/session/range kinds
/// are parsed field by field; the rest are carried opaque (SPEC-io-formats.md
/// §3.3 -- this module's own stated scope, not every consumer's need).
enum class CrdRecordKind {
    Format, Station, Target, Session, Prediction, EndOfSession, EndOfFile,   // H1,H2,H3,H4,H5,H8,H9
    Config0, Config1, Config2, Config3, Config4, Config5, Config6, Config7, // C0-C7, opaque
    FullRateRange, NormalPointRange, RangeSupplement,                       // 10, 11, 12
    Meteorological, SkyQuality,                                            // 20, 21
    Angles,                                                                // 30, opaque
    Calibration40, Calibration41, Calibration42,                           // 40-42, opaque
    SessionStatistics, Compatibility,                                      // 50, 60, opaque
    UserDefined,                                                           // 9X, opaque
    Comment,                                                               // 00
};

struct CrdFormatHeader {          // H1
    int version = 0;
    int year = 0, month = 0, day = 0, hour = 0;
};

struct CrdStationHeader {         // H2
    std::string name;
    int system_id = 0, system_number = 0, occupancy = 0;
    int epoch_time_scale = 0;
    std::string network;
};

struct CrdTargetHeader {          // H3
    std::string name;
    long ilrs_id = 0;
    std::string sic;              ///< "na" for non-ILRS targets; kept as text, never coerced to a number
    std::string norad_id;         ///< likewise "na" is legitimate
    int spacecraft_time_scale = 0;
    int target_class = 0;
    std::string target_location;  ///< "na" is legitimate (§3.3 of CRD2)
};

struct CrdSessionHeader {         // H4
    int data_type = 0;
    int start_year = 0, start_month = 0, start_day = 0, start_hour = 0, start_minute = 0, start_second = 0;
    std::optional<int> end_year, end_month, end_day, end_hour, end_minute, end_second;  ///< "na" if unavailable
    int release_flag = 0;
    bool tropo_applied = false, com_applied = false, receive_amp_applied = false;
    // Remaining H4 flags (transmit amplitude, receive time bias, transmit
    // time bias, range type indicator, data quality alert) are carried
    // verbatim, tokenised, since this round's own consumer does not need them
    // interpreted individually.
    std::vector<std::string> remaining_fields;
};

/// Full-rate (`10`) or normal-point (`11`) range record -- `CRD2`'s own two
/// numeric range types, structurally identical enough (both epoch + time-of-
/// flight + configuration id + epoch event, `11` carrying additional
/// normal-pointing statistics) to share one struct, `kind` naming which.
struct CrdRangeRecord {
    CrdRecordKind kind = CrdRecordKind::FullRateRange;
    double seconds_of_day = 0.0;
    double time_of_flight_s = 0.0;
    std::string config_id;
    int epoch_event = 0;
    /// `10`'s own remaining fields (filter flag, detector channel, stop
    /// number, receive/transmit amplitude) OR `11`'s own (window length,
    /// number of ranges, RMS, skew, kurtosis, peak-mean, return rate,
    /// detector channel, signal-to-noise) -- tokenised, per-kind meaning
    /// documented in `CRD2` §3.1/§3.2, not re-modelled field by field here
    /// (this reader's own consumer, L6 step 4, needs epoch/time-of-flight/
    /// config id/epoch event, not the per-return statistics).
    std::vector<std::string> remaining_fields;
};

/// A record type this reader does not parse field by field (§3.3): the type
/// tag plus every remaining whitespace-delimited token, verbatim.
struct CrdOpaqueRecord {
    CrdRecordKind kind = CrdRecordKind::Comment;
    std::vector<std::string> fields;
};

struct CrdFile {
    CrdFormatHeader format;
    std::vector<CrdStationHeader> stations;   ///< one file may combine several passes (CRD2 §1.2.2)
    std::vector<CrdTargetHeader> targets;
    std::vector<CrdSessionHeader> sessions;
    std::vector<CrdRangeRecord> ranges;
    std::vector<CrdOpaqueRecord> opaque;      ///< every other record, in file order, kind-tagged
};

/// Record `20` (SPEC-io-formats.md §3.3.1, `CRD2` §3.4): seconds of day, surface pressure in MILLIBAR, temperature in KELVIN,
/// relative humidity in PER CENT, and the origin of the values (0 measured, 1 interpolated).
struct CrdMeteorology {
    double seconds_of_day = 0.0;
    double pressure_mbar = 0.0;
    double temperature_k = 0.0;
    double relative_humidity_percent = 0.0;
    int origin = 0;
};

/// Record `C0` (detail type 0): the transmit wavelength in nanometres and the system configuration id that record `11`'s
/// field 4 names; the component ids that follow are carried verbatim.
struct CrdConfig0 {
    std::string config_id;
    double wavelength_nm = 0.0;
    std::vector<std::string> remaining_fields;
};

/// One `H4`…`H8` block (a session or pass segment, `CRD2` §1.4) with the `H2`, `H3` and `C0` records in force for it
/// (SPEC-io-formats.md §3.3.1, IOFM-R-008): header-level `C0`s inherited from before the first `H4`, then the block's own.
struct CrdPass {
    CrdStationHeader station;
    CrdTargetHeader target;
    CrdSessionHeader session;
    /// The scale of every epoch of the pass, RESOLVED when the pass was built (IOFM-R-010): `H2` field 6 = 3, 4 or 7
    /// (UTC(USNO), UTC(GPS), UTC(BIPM)) is `TimeScale::UTC`; any other code was refused (IOFM-F-015).
    odl::time::TimeScale time_scale = odl::time::TimeScale::UTC;
    std::vector<CrdConfig0> configs;
    std::vector<CrdRangeRecord> ranges;          ///< records `10` and `11`, in file order
    std::vector<CrdMeteorology> meteorology;     ///< records `20`, in file order
    std::vector<CrdOpaqueRecord> other;          ///< every other record of the block, in file order

    /// IOFM-R-009: field 3 of the `C0` whose field 4 is `config_id`, nanometres. IOFM-F-014 for an id with no `C0` in force.
    [[nodiscard]] odl::Result<double, CrdError> wavelength_nm(std::string_view config_id) const;
};

/// IOFM-R-008 … -R-010: the file as passes. IOFM-F-013 for a structure that is not `H4`…`H8` blocks under an `H2` and an `H3`,
/// IOFM-F-015 for an `H2` time-scale code other than 3, 4, 7. The flat `read_crd` is unchanged and still lenient.
[[nodiscard]] odl::Result<std::vector<CrdPass>, CrdError> read_crd_passes(std::string_view text);

[[nodiscard]] odl::Result<CrdFile, CrdError> read_crd(std::string_view text);
[[nodiscard]] odl::Result<std::string, CrdError> write_crd(const CrdFile& file);

[[nodiscard]] bool operator==(const CrdFormatHeader&, const CrdFormatHeader&) noexcept;
[[nodiscard]] bool operator==(const CrdStationHeader&, const CrdStationHeader&) noexcept;
[[nodiscard]] bool operator==(const CrdTargetHeader&, const CrdTargetHeader&) noexcept;
[[nodiscard]] bool operator==(const CrdSessionHeader&, const CrdSessionHeader&) noexcept;
[[nodiscard]] bool operator==(const CrdRangeRecord&, const CrdRangeRecord&) noexcept;
[[nodiscard]] bool operator==(const CrdOpaqueRecord&, const CrdOpaqueRecord&) noexcept;
[[nodiscard]] bool operator==(const CrdFile&, const CrdFile&) noexcept;

}  // namespace odl::io
