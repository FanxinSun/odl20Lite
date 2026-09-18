#pragma once
// odl/integrators/stepper.hpp — RK4 and RKF7(8), over a plain vector.
//
// SPEC-integrators.md.  The steppers are generic over the state's width and the
// right-hand side, because the gate's two published checks are NOT orbits:
// Fehlberg's Example (53) is a coupled pair with a closed form, and the
// quadrature exhibition of INTG-A-005 is a scalar. A stepper that only knew
// about `State` could not be gated against either.
//
// THE ERROR ESTIMATE IS RETURNED, NOT CONSUMED INTERNALLY, and that is what lets
// INTG-A-005 show the estimate is EXACTLY ZERO on a quadrature problem while the
// true error is not. See SPEC-integrators §3.4: alpha_0 = alpha_11 = 0 and
// alpha_10 = alpha_12 = 1, so (134)'s four evaluations cancel in pairs whenever
// the right-hand side depends on x alone. The controller is blind there, not
// merely optimistic.

#include <odl/integrators/rkf78_coefficients.hpp>

#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <vector>

namespace odl::integrators {

template <std::size_t N>
using Vec = std::array<double, N>;

/// A RUNTIME-SIZED state, for the parameter sensitivities of L3 step 4.
using DynVec = std::vector<double>;

/// The steppers are generic over the CONTAINER, not over a compile-time width.
///
/// That distinction is the whole of L3 step 4's gate. A registry issues however
/// many parameters a caller declares, the sensitivity block is 6 x n wide, and
/// "registering a second parameter requires no change to the integrator" is only
/// true if the integrator never knew the width. Templating on `std::size_t N`
/// would have satisfied every test at n = 1 and n = 2 by RECOMPILING, which is a
/// change to the integrator wearing the costume of a template argument.
template <class V>
concept StateVector = requires(V v, const V& c, std::size_t i) {
    { c.size() } -> std::convertible_to<std::size_t>;
    { v[i] } -> std::convertible_to<double&>;
    { c[i] } -> std::convertible_to<const double&>;
};

/// One RKF7(8) step. `f(x, y) -> Vec<N>`. Returns the 7th-order result and the
/// (134) estimate; neither is combined with the other here.
///
/// THE 7TH-ORDER SOLUTION IS THE ONE THAT PROPAGATES, AND THAT IS A CHOICE WITH
/// A CONSEQUENCE. An 8th-order companion is computed at every step and used only
/// for the estimate. It is tempting to propagate with it instead -- "local
/// extrapolation" -- which costs nothing and gains an order, and every modern
/// embedded pair is used that way.
///
/// DO NOT, WITHOUT REPLACING THE GATE. SPEC-integrators INTG-A-004 compares the
/// accumulated errors on Fehlberg's Example (53) against his Table XI, and that
/// is the ONLY check tying this tableau to HIS method rather than merely to a
/// valid RK7(8) pair -- the order conditions cannot distinguish those, because
/// his derivation has free parameters that were chosen rather than forced.
/// Local extrapolation propagates a different solution, so the accumulated
/// errors would no longer be the quantity Table XI reports, and the gate would
/// go on passing its order-condition arm while having quietly stopped checking
/// the thing it was built for.
///
/// The measured order confirms which is in use: INTG-A-006 recovers slope 6.90
/// from fixed steps, not 7.9.
template <StateVector V>
struct StepResult {
    V y{};        ///< the propagating 7th-order solution
    V error{};    ///< (134): the truncation estimate, componentwise
};

template <StateVector V, class F>
StepResult<V> rkf78_step(F&& f, double x, const V& y, double h) {
    const std::size_t N = y.size();
    using namespace rkf78;
    std::array<V, kStages> k{};
    for (int s = 0; s < kStages; ++s) {
        V ys = y;
        for (int j = 0; j < s; ++j) {
            const double b = kBeta[static_cast<std::size_t>(s)][static_cast<std::size_t>(j)];
            if (b == 0.0) continue;
            for (std::size_t i = 0; i < N; ++i)
                ys[i] += h * b * k[static_cast<std::size_t>(j)][i];
        }
        k[static_cast<std::size_t>(s)] = f(x + kAlpha[static_cast<std::size_t>(s)] * h, ys);
    }
    StepResult<V> out;
    out.y = y;
    out.error = y;
    for (int s = 0; s < kStages; ++s) {
        const double c = kC[static_cast<std::size_t>(s)];
        if (c == 0.0) continue;
        for (std::size_t i = 0; i < N; ++i)
            out.y[i] += h * c * k[static_cast<std::size_t>(s)][i];
    }
    // (134): TE = (41/840)(f0 + f10 - f11 - f12) h.
    //
    // GROUPED PAIRWISE, AND THE GROUPING IS NOT COSMETIC. On a quadrature
    // problem the four evaluations are pairwise BIT-IDENTICAL -- alpha_0 =
    // alpha_11 and alpha_10 = alpha_12, and the y argument is ignored -- so the
    // algebraic cancellation is exact. Written as the report writes it,
    // (f0 + f10) - f11 - f12 rounds before it subtracts and returns about
    // 1.1e-16 rather than zero.
    //
    // That difference matters: an estimate of EXACTLY ZERO is honest about being
    // blind, and the controller's behaviour is then obviously degenerate. An
    // estimate of a few times 1e-18 is rounding noise WEARING THE SHAPE OF AN
    // ESTIMATE -- the controller responds to it, the numbers look plausible, and
    // nothing announces that the quantity is meaningless. See SPEC-integrators
    // §3.4 and INTG-A-005.
    //
    // It is also better conditioned in general: f0 is near f11 and f10 near f12
    // whenever the problem is near-quadrature, which is exactly when the naive
    // ordering loses the most.
    for (std::size_t i = 0; i < N; ++i)
        out.error[i] = kErrorWeight * h * ((k[0][i] - k[11][i]) + (k[10][i] - k[12][i]));
    return out;
}

/// One classical RK4 step.
template <StateVector V, class F>
V rk4_step(F&& f, double x, const V& y, double h) {
    const std::size_t N = y.size();
    V t = y;
    const V k1 = f(x, y);
    for (std::size_t i = 0; i < N; ++i) t[i] = y[i] + 0.5 * h * k1[i];
    const V k2 = f(x + 0.5 * h, t);
    for (std::size_t i = 0; i < N; ++i) t[i] = y[i] + 0.5 * h * k2[i];
    const V k3 = f(x + 0.5 * h, t);
    for (std::size_t i = 0; i < N; ++i) t[i] = y[i] + h * k3[i];
    const V k4 = f(x + h, t);
    V out = y;
    for (std::size_t i = 0; i < N; ++i)
        out[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    return out;
}

/// What a controlled integration did.  Accepted and rejected steps are counted
/// SEPARATELY (INTG-R-012): Fehlberg's Table XI reports evaluations exactly
/// equal to steps x substitutions for both methods -- 818 x 13 = 10634 and
/// 1423 x 17 = 24191 -- so his counts are of ACCEPTED steps, and a count that
/// conflated the two would not be comparable with his.
struct Record {
    std::size_t accepted = 0, rejected = 0, evaluations = 0;
    double h_min = 0.0, h_max = 0.0, worst_estimate = 0.0;
};

/// The controller's constants.  SPEC-integrators INTG-R-011 and INTG-Q-001:
/// Fehlberg gives the PROCEDURE in prose, not these numbers, so they are this
/// tree's and are named rather than buried.
struct Control {
    double safety = 0.9;
    double max_growth = 5.0;
    double max_shrink = 0.1;
    std::size_t max_rejections = 20;
};

}  // namespace odl::integrators
