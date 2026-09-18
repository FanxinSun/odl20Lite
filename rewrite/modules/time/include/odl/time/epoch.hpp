#pragma once
// odl/time/epoch.hpp — an instant, and every conversion the pipeline needs.
//
// SPEC-time.md §4.1–§4.2.  Storage is TAI, as (i64 whole SI seconds since
// 1958-01-01T00:00:00 TAI, f64 fraction in [0,1)).  The reasoning, which the
// specification makes quantitative:
//
//   representation                 spacing     along-track at 7.5 km/s
//   f64 Julian Date                40 µs       0.30 m      disqualified
//   f64 Modified Julian Date       0.63 µs     4.7 mm      disqualified
//   i64 seconds + f64 fraction     0.11 fs     8e-16 m     chosen
//
// The two-part f64 JD is adequate per call but is NOT CANONICAL — the same
// instant has unboundedly many splittings, so equality, ordering and hashing are
// all ill-defined on it, and every one of those is needed by the layers above.
// It is the right form only at the ERFA call boundary, where TIME-R-014 requires
// the integral Julian day first and the day fraction second.
//
// Epoch is OPAQUE: there is no way to build one from a bare number.  Every
// constructor names a scale (TIME-R-003), because the most expensive defect this
// pipeline has seen was a JPL Horizons table read as UTC when it was TDB — 69 s,
// which is 518 km along track at LEO, and an orbit fit converges on it and
// reports a plausible residual.

#include <odl/core/result.hpp>
#include <odl/time/duration.hpp>
#include <odl/time/leap_table.hpp>
#include <odl/time/time_scale.hpp>

#include <cstdint>
#include <optional>

namespace odl::time {

/// A calendar breakdown in some named scale.  `second` may reach 60 (never 61)
/// in UTC on a day ending in a positive leap second.
struct Calendar {
    int    year = 0;
    int    month = 0;
    int    day = 0;
    int    hour = 0;
    int    minute = 0;
    double second = 0.0;
};

/// A two-part Julian date for an ERFA call: `day` integral, `fraction` in [0,1).
struct TwoPartJd {
    double day = 0.0;
    double fraction = 0.0;
    [[nodiscard]] double sum() const noexcept { return day + fraction; }
};

class Epoch {
public:
    Epoch() = delete;   // an epoch with no stated scale of origin is not a thing

    // --- construction, always naming a scale --------------------------------

    static odl::Result<Epoch, TimeError> from_calendar(TimeScale scale, const Calendar& c,
                                                       const LeapTable& leaps);
    static odl::Result<Epoch, TimeError> from_two_part_jd(TimeScale scale, double part1,
                                                          double part2, const LeapTable& leaps);
    /// GPS week and seconds-of-week.  Refuses a bare 10-bit week (TIME-F-006):
    /// the rollover ambiguity is 1024 weeks, and guessing the nearest one is how
    /// an epoch silently lands 19.6 years away.
    static odl::Result<Epoch, TimeError> from_gps_week(std::int64_t week, double seconds_of_week);

    // --- rendering ----------------------------------------------------------

    [[nodiscard]] odl::Result<Calendar, TimeError> calendar(TimeScale scale,
                                                            const LeapTable& leaps,
                                                            int decimals = 9) const;
    [[nodiscard]] odl::Result<TwoPartJd, TimeError> two_part_jd(TimeScale scale,
                                                                const LeapTable& leaps) const;

    // --- the scales ---------------------------------------------------------

    [[nodiscard]] odl::Result<Duration, TimeError> delta_at(const LeapTable& leaps) const;
    /// TDB − TT.  Cannot fail.  `site` supplies the diurnal term, which reaches
    /// about 2.1 µs; omit it for a geocentric epoch.
    [[nodiscard]] Duration tdb_minus_tt(std::optional<Site> site = std::nullopt) const;

    /// The ONLY route to a UT1 value (TIME-R-001).  ΔUT1 must be passed in, from
    /// the `eop` module with its coverage and quality policy already applied;
    /// nothing here may fetch it (TIME-R-002).  The leap table is needed because
    /// UT1 = UTC + ΔUT1 and UTC = TAI − ΔAT: ΔUT1 is published against UTC, so
    /// the conversion cannot skip it.
    [[nodiscard]] odl::Result<TwoPartJd, TimeError> ut1_two_part_jd(
        Duration dut1, const LeapTable& leaps) const;

    // --- arithmetic: total, exact, and needing no table ---------------------
    // This is the payoff of storing TAI.  A difference is always the elapsed SI
    // seconds, including across a leap second (TIME-R-034).

    [[nodiscard]] Epoch    add(Duration d) const noexcept;
    [[nodiscard]] Duration difference(const Epoch& earlier) const noexcept;

    friend constexpr bool operator==(const Epoch& a, const Epoch& b) noexcept {
        return a.seconds_ == b.seconds_ && a.fraction_ == b.fraction_;
    }
    friend constexpr bool operator<(const Epoch& a, const Epoch& b) noexcept {
        return a.seconds_ != b.seconds_ ? a.seconds_ < b.seconds_ : a.fraction_ < b.fraction_;
    }
    friend constexpr bool operator!=(const Epoch& a, const Epoch& b) noexcept { return !(a == b); }
    friend constexpr bool operator>(const Epoch& a, const Epoch& b) noexcept { return b < a; }
    friend constexpr bool operator<=(const Epoch& a, const Epoch& b) noexcept { return !(b < a); }
    friend constexpr bool operator>=(const Epoch& a, const Epoch& b) noexcept { return !(a < b); }

    /// TAI seconds since 1958-01-01T00:00:00 TAI.  Exposed for tests and for the
    /// module's own use; it is not a way to build an Epoch.
    [[nodiscard]] constexpr std::int64_t tai_seconds() const noexcept { return seconds_; }
    [[nodiscard]] constexpr double       tai_fraction() const noexcept { return fraction_; }

    /// The MJD of 1958-01-01, the representation origin.
    static constexpr std::int64_t kOriginMjd = 36204;

private:
    Epoch(std::int64_t seconds, double fraction) noexcept
        : seconds_(seconds), fraction_(fraction) {}
    static Epoch from_tai_parts(std::int64_t whole, double fraction) noexcept;

    std::int64_t seconds_;
    double       fraction_;
};

}  // namespace odl::time
