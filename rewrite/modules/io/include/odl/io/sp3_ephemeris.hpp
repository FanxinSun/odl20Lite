#pragma once
// odl/io/sp3_ephemeris.hpp — interpolated satellite position from an SP3 file.
//
// SPEC-io-formats.md §3.8. plan/subplan_L6/L6-1.md, ruled 2026-09-25: the
// comparison tools' own three position-lookup defects (nearest-sample
// selection, elapsed time in place of a calendar date, an assumed frame)
// lived in their own per-tool interpolation code, and each tool still
// interpolated in its own way -- so interpolation is DATA ACCESS, one
// facility here, not re-implemented per tool or by `measmod`, which uses it.
//
// Like sp3.hpp, this module never constructs an odl::time::Epoch -- no
// LeapTable is available here (SPEC-io-formats.md §3.1). Time is measured as
// elapsed CALENDAR seconds from the ephemeris's own first sample, a pure
// function of the Calendar fields (`calendar_elapsed_seconds`, below): exact
// for a continuous time system -- TAI, GPS, the system every real file this
// tree holds actually uses -- and a stated, bounded approximation for a
// UTC-tagged file (off by at most the leap seconds actually crossed, and
// then only across the instant of the leap second itself), never silently
// assumed away.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/io/sp3.hpp>
#include <odl/time/epoch.hpp>  // odl::time::Calendar only; no Epoch or LeapTable used here

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace odl::io {

using Sp3EphemerisError = odl::Diagnostic;

/// Elapsed seconds from `from` to `to`, both read as plain calendar fields --
/// a proleptic-Gregorian day count plus time-of-day, no `LeapTable`. See the
/// file header above for exactly what this is exact for and what it is not.
[[nodiscard]] double calendar_elapsed_seconds(const time::Calendar& from, const time::Calendar& to);

/// The raw Lagrange fit through exactly the given `(t_s, position_km)`
/// points, evaluated at `t_query` -- no span, gap or manoeuvre policy at all.
/// `Sp3Ephemeris::position_km_at` is this function wrapped with window
/// selection and the refusal policy below; exposed separately (SPEC-io-
/// formats.md §3.8) so the accuracy self-check (`IOFM-A-030`/`031`) can
/// measure the polynomial's own error against a real sample deliberately
/// held out, without that sample's own absence being read as a data gap --
/// a different, and deliberately stricter, question `position_km_at` alone
/// answers for an arbitrary caller who does not already know the answer.
[[nodiscard]] odl::Vec3 lagrange_interpolate(
    const std::vector<std::pair<double, odl::Vec3>>& points, double t_query);

/// One satellite's own position time series from an `Sp3File`, ready for
/// repeated interpolation (`IOFM-R-003`). Built once; every `position_km_at`
/// call reuses the same samples and the same order.
class Sp3Ephemeris {
public:
    /// `target_order` is the Lagrange polynomial order this ephemeris tries
    /// to use -- default 10 (11 points), the order SPEC-io-formats.md §3.8's
    /// own cited sources converge on for both a 15-minute GNSS file and a
    /// 60-second LEO file. The order actually used (`order()`) is reduced to
    /// `sample_count() - 1` when the file offers fewer than
    /// `target_order + 1` epochs for this satellite (small or synthetic
    /// fixtures; every real file this tree holds offers far more).
    [[nodiscard]] static odl::Result<Sp3Ephemeris, Sp3EphemerisError>
        build(const Sp3File& file, const std::string& satellite_id, int target_order = 10);

    /// Interpolated position, kilometres, at `t_s` seconds since
    /// `first_epoch()`. Refuses rather than ever extrapolating
    /// (`IOFM-R-004`): outside the sampled span (`IOFM-F-009`), across a gap
    /// bracketing `t_s` (`IOFM-F-010`, `IOFM-R-005`), or when a manoeuvre-
    /// flagged sample falls inside the window this query would use
    /// (`IOFM-F-011`, `IOFM-R-005`).
    [[nodiscard]] odl::Result<odl::Vec3, Sp3EphemerisError> position_km_at(double t_s) const;

    [[nodiscard]] int order() const noexcept { return order_; }
    [[nodiscard]] std::size_t sample_count() const noexcept { return samples_.size(); }
    [[nodiscard]] const time::Calendar& first_epoch() const noexcept { return first_epoch_; }
    [[nodiscard]] double first_sample_seconds() const noexcept;
    [[nodiscard]] double last_sample_seconds() const noexcept;

private:
    struct Sample {
        double t_s;
        odl::Vec3 r_km;
        bool maneuver;
    };
    Sp3Ephemeris(std::vector<Sample> samples, int order, double nominal_interval_s,
                 time::Calendar first_epoch) noexcept
        : samples_(std::move(samples)), order_(order),
          nominal_interval_s_(nominal_interval_s), first_epoch_(first_epoch) {}

    std::vector<Sample> samples_;
    int order_;
    double nominal_interval_s_;
    time::Calendar first_epoch_;
};

/// The central-difference step `central_difference_velocity_km_s` uses by
/// default -- comfortably inside the shortest real sampling interval this
/// tree holds (60 s) and far enough from floating-point noise at km-scale
/// positions to be numerically clean. Also the right step for a caller
/// composing its own velocity in a DIFFERENT frame (e.g. GCRS): call
/// `position_km_at(t_s - kVelocityStepS)` / `(t_s + kVelocityStepS)`,
/// transform EACH to the target frame, then difference the two transformed
/// results -- transforming a difference is not the same as differencing a
/// transform when the frame is time-dependent, so that composition cannot
/// simply be folded into this module, which has no frame-transform code and
/// should not gain any.
inline constexpr double kVelocityStepS = 1.0;

/// Velocity, km/s, at `t_s` -- a central finite difference of
/// `position_km_at` over `h_s` (default `kVelocityStepS`). NOT an
/// `Sp3Ephemeris` member: its own stated scope (the ruling, SPEC-io-
/// formats.md §3.8) is position; this is that facility's own obvious
/// derivative, offered once here so a caller needing velocity at an
/// ARBITRARY query time (not only at a real sample, where the SP3 file's own
/// `V` record already states it) does not re-derive it per call site.
/// Refuses if `t_s - h_s` or `t_s + h_s` themselves refuse (near a span,
/// gap or manoeuvre boundary) -- see `position_km_at`.
[[nodiscard]] odl::Result<odl::Vec3, Sp3EphemerisError> central_difference_velocity_km_s(
    const Sp3Ephemeris& eph, double t_s, double h_s = kVelocityStepS);

}  // namespace odl::io
