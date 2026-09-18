#pragma once
// odl/time/leap_table.hpp — ΔAT as data, with an expiry that is enforced.
//
// SPEC-time.md §4.7.  The table is loaded from the IERS file declared in the
// manifest, never from a dependency's compiled-in copy: ERFA v2.0.1 dates from
// 2023-10-13 and its eraDat returns status +1, "dubious year", TOGETHER WITH A
// USABLE-LOOKING NUMBER, for years from 2028 (TIME-R-053).  A value returned
// with a warning the caller ignores is exactly the defect class this tree is
// organised against, so the tree does UTC↔TAI itself.
//
// The file carries its own expiry — "File expires on 28 June 2027" — and that is
// enforced (TIME-R-051).  A leap second is announced about six months ahead; a
// table fetched before the announcement cannot know about it; a missed leap
// second is 1 s, which is 7.5 km of along-track position at LEO, and it is
// silent.  Plan rule R12: exactly one named, per-run, logged escape hatch.

#include <odl/core/result.hpp>
#include <odl/time/duration.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odl::time {

using TimeError = odl::Diagnostic;

/// One row of the IERS table: ΔAT in force from `mjd_utc` onwards.
struct LeapEntry {
    std::int64_t mjd_utc;   ///< UTC Modified Julian Day the step takes effect
    std::int64_t delta_at;  ///< TAI − UTC, integral from 1972
};

/// Where a table came from, carried with it so a run can record it.
struct LeapProvenance {
    std::string source_id;   ///< manifest id or file path
    std::string sha256;      ///< of the bytes parsed, when known
    std::string bulletin;    ///< e.g. "IERS Bulletin C 72, July 2026"
};

class LeapTable {
public:
    /// Parse the IERS `Leap_Second.dat` format.  Refuses a file that is
    /// unparseable, non-monotonic in MJD, or non-integral in ΔAT at or after
    /// 1972 (TIME-F-005).
    static odl::Result<LeapTable, TimeError> parse(std::string_view text,
                                                   LeapProvenance provenance);

    [[nodiscard]] const std::vector<LeapEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] const LeapProvenance& provenance() const noexcept { return provenance_; }

    /// The UTC MJD after which this table may not be trusted, from the file's
    /// own "File expires on" line.  Zero if the file did not state one.
    [[nodiscard]] std::int64_t expiry_mjd() const noexcept { return expiry_mjd_; }
    [[nodiscard]] std::string_view expiry_text() const noexcept { return expiry_text_; }

    /// ΔAT in force on a UTC day.  Refuses before 1972-01-01 (TIME-F-002) and
    /// after the table's expiry unless the escape hatch is set (TIME-F-004).
    [[nodiscard]] odl::Result<std::int64_t, TimeError> delta_at_on(std::int64_t mjd_utc) const;

    /// The number of seconds inserted at the end of a UTC day: 0 normally, 1 on
    /// a day ending in a positive leap second, -1 for a negative one.
    [[nodiscard]] std::int64_t leap_at_end_of(std::int64_t mjd_utc) const noexcept;

    /// Plan rule R12's single named override for TIME-R-051.  Set explicitly per
    /// run, never by default, never from a configuration fall-through, and
    /// recorded in run provenance by the caller together with this table's hash.
    void assume_no_further_leap_seconds(bool on) noexcept { assume_no_further_ = on; }
    [[nodiscard]] bool assuming_no_further_leap_seconds() const noexcept {
        return assume_no_further_;
    }

    /// The first UTC MJD the table covers: 41317, i.e. 1972-01-01.
    static constexpr std::int64_t kFirstSupportedMjd = 41317;

private:
    LeapTable() = default;

    std::vector<LeapEntry> entries_;
    LeapProvenance         provenance_;
    std::int64_t           expiry_mjd_ = 0;
    std::string            expiry_text_;
    bool                   assume_no_further_ = false;
};

}  // namespace odl::time
