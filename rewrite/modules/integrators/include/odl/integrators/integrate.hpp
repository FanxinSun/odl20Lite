#pragma once
// odl/integrators/integrate.hpp — the controlled integration.
//
// SPEC-integrators.md §4.  The integrator sees a right-hand side and a width and
// nothing about what any component MEANS (DYN-R-026), which is L3's exit gate.

#include <odl/core/result.hpp>
#include <odl/integrators/stepper.hpp>

#include <algorithm>
#include <string>

namespace odl::integrators {

using IntgError = odl::Diagnostic;

template <StateVector V>
struct Solution {
    V y{};
    double x = 0.0;
    Record record{};
};

/// Integrate from `x0` to `x1` with RKF7(8) under step-size control.
///
/// THE WIDTH IS THE STATE'S, NOT A TEMPLATE PARAMETER. Nothing in here knows how
/// many components there are; `y0.size()` is the only answer available to it.
template <StateVector V, class F>
[[nodiscard]] odl::Result<Solution<V>, IntgError>
integrate(F&& f, double x0, const V& y0, double x1, double tolerance,
          Control ctl = Control{}, double h_initial = 0.0) {
    const std::size_t N = y0.size();
    if (!(tolerance > 0.0)) {
        return odl::err(IntgError{"INTG-F-005",
            "the tolerance must be positive; got " + std::to_string(tolerance)});
    }
    if (x1 == x0) return Solution<V>{y0, x0, Record{}};

    const double span = x1 - x0;
    double h = (h_initial != 0.0) ? h_initial : span / 100.0;
    double x = x0;
    V y = y0;
    Record rec;
    rec.h_min = std::abs(h);
    rec.h_max = std::abs(h);
    std::size_t consecutive = 0;

    while ((span > 0.0) ? (x < x1) : (x > x1)) {
        if ((span > 0.0 && x + h > x1) || (span < 0.0 && x + h < x1)) h = x1 - x;
        if (std::abs(h) < std::abs(x) * 1e-15) {
            return odl::err(IntgError{"INTG-F-001",
                "the step " + std::to_string(h) + " has underflowed the abscissa " +
                std::to_string(x) + "'s resolution at tolerance " + std::to_string(tolerance) +
                "; the integration cannot advance."});
        }
        const auto step = rkf78_step<V>(f, x, y, h);
        rec.evaluations += rkf78::kStages;

        double est = 0.0;
        for (std::size_t i = 0; i < N; ++i) est = std::max(est, std::abs(step.error[i]));
        for (std::size_t i = 0; i < N; ++i) {
            if (!std::isfinite(step.y[i]) || !std::isfinite(step.error[i])) {
                return odl::err(IntgError{"INTG-F-004",
                    "component " + std::to_string(i) + " is not finite after a step at x = " +
                    std::to_string(x)});
            }
        }
        rec.worst_estimate = std::max(rec.worst_estimate, est);

        if (est <= tolerance || est == 0.0) {
            x += h;
            y = step.y;
            ++rec.accepted;
            consecutive = 0;
            rec.h_min = std::min(rec.h_min, std::abs(h));
            rec.h_max = std::max(rec.h_max, std::abs(h));
        } else {
            ++rec.rejected;
            if (++consecutive > ctl.max_rejections) {
                return odl::err(IntgError{"INTG-F-002",
                    std::to_string(consecutive) + " consecutive rejections at x = " +
                    std::to_string(x) + "; last estimate " + std::to_string(est) +
                    " against tolerance " + std::to_string(tolerance)});
            }
        }
        // the standard order-8 scaling on a 7th-order propagator's estimate
        double factor = ctl.safety;
        if (est > 0.0) factor = ctl.safety * std::pow(tolerance / est, 1.0 / 8.0);
        else           factor = ctl.max_growth;
        factor = std::clamp(factor, ctl.max_shrink, ctl.max_growth);
        h *= factor;
    }
    return Solution<V>{y, x, rec};
}

}  // namespace odl::integrators
