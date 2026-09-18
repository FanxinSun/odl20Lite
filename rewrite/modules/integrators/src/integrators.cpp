// integrators.cpp — SPEC-integrators.
//
// The steppers and the controller are templates over the state's width, because
// the gate's published checks are not orbits (SPEC-integrators §1). This file
// exists so the module is a library rather than headers alone, and carries the
// one thing that is not generic: the tableau's own self-consistency, asserted at
// compile time so that a regenerated coefficient file cannot quietly disagree
// with the truncation term it is paired with.

#include <odl/integrators/integrate.hpp>

namespace odl::integrators {
namespace {

using namespace rkf78;

constexpr double sum(const std::array<double, kStages>& v) {
    double s = 0.0;
    for (double x : v) s += x;
    return s;
}

// Both weight vectors sum to 1. In the rationals this is exact; in doubles it is
// exact to rounding, and the EXACT statement is tools/rk_coefficients.py's.
static_assert(sum(kC)    > 1.0 - 1e-15 && sum(kC)    < 1.0 + 1e-15,
              "INTG-A-001: the 7th-order weights must sum to 1");
static_assert(sum(kCHat) > 1.0 - 1e-15 && sum(kCHat) < 1.0 + 1e-15,
              "INTG-A-001: the 8th-order weights must sum to 1");

// SPEC-integrators §3.4, asserted where the stepper can see it: the estimator's
// four abscissae coincide in pairs, which is WHY it vanishes on a quadrature
// problem. If a regenerated tableau ever broke this, (134) would no longer be
// the difference it is written as.
static_assert(kAlpha[0] == kAlpha[11], "alpha_0 == alpha_11");
static_assert(kAlpha[10] == kAlpha[12], "alpha_10 == alpha_12");

}  // namespace
}  // namespace odl::integrators
