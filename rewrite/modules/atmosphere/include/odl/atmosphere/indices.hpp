#pragma once
// odl/atmosphere/indices.hpp — the geomagnetic index conventions.
//
// SPEC-atmosphere ATMO-R-009.  These are TWO DIFFERENT REQUESTS, not one
// request with a flag.  The reference reads seven numbers only when SW(9) = -1;
// passing seven with the switch unset makes it read the first and silently
// ignore six, which is a wrong answer with no signal.
//
// They live in their own header because both the public interface and the model
// internals need them, and neither should have to include the other.

#include <array>

namespace odl::atmosphere {

struct DailyAp {
    double ap = 0.0;                ///< the daily Ap index
};

struct ThreeHourlyAp {
    /// (1) daily Ap; (2) 3-hourly at the epoch; (3)-(5) 3, 6 and 9 hours before;
    /// (6) mean of the eight from 12 to 33 hours before; (7) mean of the eight
    /// from 36 to 57 hours before.  The reference's own ordering.
    std::array<double, 7> ap{};
};

}  // namespace odl::atmosphere
