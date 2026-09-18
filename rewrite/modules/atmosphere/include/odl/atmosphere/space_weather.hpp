#pragma once
// odl/atmosphere/space_weather.hpp — the only input in this tree that moves.
//
// SPEC-atmosphere.md §4.3–§4.5.  Everything else L2 reads is frozen: EGM2008,
// FES2004, DE440, the Conventions.  This is not, and it moves in three ways a
// hash cannot tell apart — extension, REVISION of rows already present, and (in
// sources that carry it) forecast.  The policy is not an exception for moving
// data; it is the ordinary mechanism applied honestly:
//
//   * the manifest pins a snapshot by SHA-256 and THE LOADER HAS NO NETWORK PATH
//     (ATMO-R-025), so the gate is reproducible the same way every other gate is;
//   * the snapshot's identity is part of every answer, and two results from
//     different snapshots REFUSE comparison, because revision means they may
//     differ at an epoch both cover (ATMO-R-024, ATMO-F-013);
//   * updating is a manifest change, which plan §5 constraint 9 already makes
//     re-run every gate that consumed the entry.
//
// COVERAGE IS A SET, NOT AN INTERVAL (ATMO-R-021).  v1.0 of the specification
// wrote it as [first + 40, last − 40] and measurement showed that is wrong: the
// GFZ file's F10.7 column carries 655 interior gaps in 459 runs, the last in
// 2026, so 25 breaks fall inside the otherwise usable span.  Membership is
// decided per epoch, from that epoch's own required window.

#include <odl/atmosphere/atmosphere.hpp>
#include <odl/core/result.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odl::atmosphere {

/// A calendar day, which is all this model's time handling needs: NRLMSISE-00
/// ignores the year entirely and takes a day number and seconds of UT.
struct Day {
    int year = 0, month = 0, day = 0;
    friend bool operator==(const Day&, const Day&) = default;
    friend auto operator<=>(const Day&, const Day&) = default;
};

/// What a load found, reported rather than summarised: a count without its
/// denominator is what hid 868 ephemeris cases at L2 step 1.
struct TableSummary {
    std::size_t rows = 0;
    std::size_t f107_present = 0;       ///< rows whose F10.7obs is not the sentinel
    std::size_t f107_sentinel = 0;      ///< rows carrying -1.0
    std::size_t usable_days = 0;        ///< epochs with a complete required window
    std::size_t usable_breaks = 0;      ///< discontinuities inside the usable span
    Day first{}, last{};
    Day first_usable{}, last_usable{};
    std::size_t by_class[4] = {0, 0, 0, 0};   ///< indexed by DataClass
};

/// The result of checking a redistributed column against its issuing authority.
struct VerificationReport {
    std::size_t overlap_days = 0;
    std::size_t exact = 0;
    std::size_t differing = 0;
    double worst_difference_sfu = 0.0;
    std::size_t duplicate_timestamps = 0;   ///< dates carrying two readings at the same time
    std::size_t missing_noon = 0;           ///< dates with no local-noon reading at all
    Day first{}, last{};
};

/// The GFZ table: Kp, ap, Ap and the DRAO F10.7 it redistributes.
class SpaceWeatherTable {
public:
    /// Bytes, not a path — the tree's loader convention.  `declared_columns` is
    /// the manifest entry's own list, and a column outside it cannot be read
    /// (ATMO-R-032, ATMO-F-016): the GFZ file's sunspot column is CC BY-NC 4.0
    /// inside a CC BY 4.0 file, so "this tree reads no sunspot number" has to be
    /// enforced rather than asserted.
    [[nodiscard]] static odl::Result<SpaceWeatherTable, AtmoError>
    load(std::string_view bytes, std::string snapshot_id, std::string snapshot_sha256,
         const std::vector<std::string>& declared_columns);

    /// The sample NRLMSISE-00 needs for this day, or a refusal naming what is
    /// missing.  F10.7 is the PREVIOUS day's observed value and F10.7A is the
    /// centred 81-day mean, computed here and never read from a derived column.
    [[nodiscard]] odl::Result<SpaceWeather, AtmoError>
    sample(const Day& d, bool require_verified = false) const;

    /// Check the redistributed F10.7 against the issuing authority's own table.
    /// `drao_bytes` is `fluxtable.txt`.  Rows inside the reported window are then
    /// `Verification::verified_against_issuer`.
    [[nodiscard]] odl::Result<VerificationReport, AtmoError>
    verify_against_issuer(std::string_view drao_bytes);

    [[nodiscard]] const TableSummary& summary() const noexcept { return summary_; }
    [[nodiscard]] bool usable(const Day& d) const noexcept;

    /// Read a named column for a day.  Refuses a column the manifest does not
    /// declare, whatever the file contains.
    [[nodiscard]] odl::Result<double, AtmoError>
    column(const Day& d, std::string_view name) const;

private:
    struct Row {
        Day day{};
        double kp[8]{}, ap[8]{}, Ap = 0.0;
        double f107_obs = -1.0, f107_adj = -1.0;
        DataClass cls = DataClass::definitive;
    };
    std::vector<Row> rows_;
    std::vector<std::string> declared_;
    std::string snapshot_id_, snapshot_sha256_;
    TableSummary summary_{};
    bool verified_window_ = false;
    Day verified_first_{}, verified_last_{};

    [[nodiscard]] long index_of(const Day& d) const noexcept;
};

}  // namespace odl::atmosphere
