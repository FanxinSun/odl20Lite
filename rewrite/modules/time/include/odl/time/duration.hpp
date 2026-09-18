#pragma once
// odl/time/duration.hpp — an interval of SI seconds.
//
// SPEC-time.md TIME-R-013: an interval is a signed pair of the same shape as an
// epoch, and interval arithmetic is exact in the whole-second component.
//
// The fraction is always in [0,1), including for negative durations: -0.5 s is
// (whole = -1, fraction = 0.5).  Floor semantics rather than truncation, so
// there is exactly one representation of each value and no sign special case
// anywhere in the arithmetic.

#include <cmath>
#include <cstdint>

namespace odl::time {

class Duration {
public:
    constexpr Duration() noexcept = default;

    /// Whole seconds plus a fraction, renormalised so that fraction ∈ [0,1).
    static Duration from_parts(std::int64_t whole, double fraction) noexcept {
        const double k = std::floor(fraction);
        return Duration{whole + static_cast<std::int64_t>(k), fraction - k};
    }
    static Duration from_seconds(double s) noexcept { return from_parts(0, s); }
    static constexpr Duration zero() noexcept { return Duration{}; }

    [[nodiscard]] constexpr std::int64_t whole_seconds() const noexcept { return whole_; }
    [[nodiscard]] constexpr double fraction() const noexcept { return fraction_; }

    /// Lossy above a few times 2^53 seconds; fine for any interval this tree sees.
    [[nodiscard]] double to_seconds() const noexcept {
        return static_cast<double>(whole_) + fraction_;
    }

    friend Duration operator+(Duration a, Duration b) noexcept {
        return from_parts(a.whole_ + b.whole_, a.fraction_ + b.fraction_);
    }
    friend Duration operator-(Duration a, Duration b) noexcept {
        return from_parts(a.whole_ - b.whole_, a.fraction_ - b.fraction_);
    }
    friend Duration operator-(Duration a) noexcept {
        return from_parts(-a.whole_, -a.fraction_);
    }
    friend constexpr bool operator==(Duration a, Duration b) noexcept {
        return a.whole_ == b.whole_ && a.fraction_ == b.fraction_;
    }
    friend constexpr bool operator<(Duration a, Duration b) noexcept {
        return a.whole_ != b.whole_ ? a.whole_ < b.whole_ : a.fraction_ < b.fraction_;
    }
    friend constexpr bool operator!=(Duration a, Duration b) noexcept { return !(a == b); }
    friend constexpr bool operator>(Duration a, Duration b) noexcept { return b < a; }
    friend constexpr bool operator<=(Duration a, Duration b) noexcept { return !(b < a); }
    friend constexpr bool operator>=(Duration a, Duration b) noexcept { return !(a < b); }

private:
    constexpr Duration(std::int64_t whole, double fraction) noexcept
        : whole_(whole), fraction_(fraction) {}

    std::int64_t whole_ = 0;
    double       fraction_ = 0.0;
};

}  // namespace odl::time
