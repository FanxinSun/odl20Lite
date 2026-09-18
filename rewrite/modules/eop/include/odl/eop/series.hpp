#pragma once
// odl/eop/series.hpp — the loaded series, its coverage, and the query.
//
// SPEC-eop.md.  This module is deliberately NOT part of `time`: it parses files
// and holds a coverage policy, neither of which belongs in a dependency-free
// leaf.  That boundary was argued by the spec author and adopted, and plan §2
// was amended to match.

#include <odl/core/result.hpp>
#include <odl/eop/record.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace odl::eop {

using EopError = odl::Diagnostic;

struct Coverage {
    std::int64_t first_mjd = 0;   ///< already margined: see EOP-R-052
    std::int64_t last_mjd = 0;
    [[nodiscard]] bool empty() const noexcept { return last_mjd < first_mjd; }
};

/// One tabulated day.  `tai_at_0h` is computed once at load, so a query needs no
/// leap table: the spacing between consecutive rows is the actual length of that
/// UTC day, 86400 or 86401 SI seconds, and interpolating on it handles a leap
/// second without the query having to know about one.
struct EopRow {
    std::int64_t mjd = 0;
    std::int64_t tai_at_0h = 0;
    EopRecord    value{};
};

class EopSeries {
public:
    /// EOP 20 C04.  Whitespace-tokenised, not fixed-column: the declared format
    /// is fixed-width but nothing guarantees it survives a field widening, and
    /// the values are unambiguous when split (EOP-R-020).
    ///
    /// The leap table is needed at LOAD, not at query: each row is tabulated at
    /// 0h UTC and is converted once to an instant, so `at()` is then a pure
    /// function of the series.  SPEC-eop §5 put the table on the query instead;
    /// moving it here keeps the query signature the spec wanted.
    static odl::Result<EopSeries, EopError> load_c04(std::string_view text,
                                                      EopProvenance provenance,
                                                      const odl::time::LeapTable& leaps);

    /// finals2000A.all.  FIXED-WIDTH, and it must be: its numeric fields abut
    /// without separators, so whitespace tokenisation mis-parses it (EOP-R-024).
    /// This is the opposite of load_c04 and the difference is a property of the
    /// two formats, not an inconsistency.
    static odl::Result<EopSeries, EopError> load_finals2000a(std::string_view text,
                                                              EopProvenance provenance,
                                                              const odl::time::LeapTable& leaps);

    /// Prefer, per epoch: C04 final, then Bulletin B, then rapid, then predicted.
    /// Reports the discontinuity at the boundary and REFUSES one above the
    /// policy's threshold, because at that size it means mismatched products —
    /// different ITRF realisations, or a C04 from before a retroactive revision
    /// spliced against a current finals2000A.
    static odl::Result<EopSeries, EopError> splice(const EopSeries& final_series,
                                                    const EopSeries& rapid_series,
                                                    const EopPolicy& policy);

    [[nodiscard]] Coverage coverage() const noexcept;
    [[nodiscard]] const std::vector<EopRow>& rows() const noexcept { return rows_; }
    [[nodiscard]] const std::vector<EopProvenance>& provenance() const noexcept {
        return provenance_;
    }
    /// Rows the pre-1972 UTC policy excluded at load (SPEC-time TIME-R-040).
    [[nodiscard]] std::size_t rows_before_1972() const noexcept { return skipped_pre_1972_; }
    /// Rows excluded because they lie past the leap-second table's expiry.
    ///
    /// The two adopted specifications collide here and neither anticipated it:
    /// SPEC-time TIME-R-051 refuses UTC past the table's expiry, and
    /// finals2000A.all routinely PREDICTS about a year ahead — past it. Rather
    /// than refuse the whole file, the series is truncated at the horizon where
    /// UTC is still well defined and says how much it dropped, so the refusal a
    /// caller meets is EOP-F-007 naming the coverage rather than a leap-table
    /// error from three layers down.
    [[nodiscard]] std::size_t rows_past_leap_expiry() const noexcept { return skipped_expired_; }

    /// Interpolated and, by default, with the four sub-daily corrections restored.
    [[nodiscard]] odl::Result<EopRecord, EopError> at(const odl::time::Epoch& when,
                                                       const EopPolicy& policy) const;
    /// The interpolated series alone, `subdaily_applied` false.  Exists so that
    /// agreement with IERS daily values is expressible; `frames` refuses it.
    [[nodiscard]] odl::Result<EopRecord, EopError> raw_at(const odl::time::Epoch& when,
                                                           const EopPolicy& policy) const;

private:
    EopSeries() = default;
    [[nodiscard]] odl::Result<EopRecord, EopError> query(const odl::time::Epoch& when,
                                                          const EopPolicy& policy,
                                                          bool subdaily) const;

    std::vector<EopRow>        rows_;
    std::vector<EopProvenance> provenance_;
    std::size_t                skipped_pre_1972_ = 0;
    std::size_t                skipped_expired_ = 0;
};

}  // namespace odl::eop
