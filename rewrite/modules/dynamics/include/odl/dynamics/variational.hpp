#pragma once
// odl/dynamics/variational.hpp — the state transition matrix, integrated with
// the state.
//
// SPEC-stm.md.  With x = (r, v) and x' = (v, a):
//
//     A(t) = [  0      I   ]        dPhi/dt = A(t) Phi,  Phi(t0,t0) = I
//            [ da/dr  da/dv]
//
// PHI IS INTEGRATED ALONGSIDE THE STATE, not by a second pass over a stored
// trajectory (STM-R-001): 6 + 36 = 42 components advance together, so Phi is
// evaluated at exactly the states the trajectory visited.
//
// THE JACOBIAN NEEDS NO km<->m CROSSING, and that is worth saying because every
// other boundary in this tree does. da/dr has units of 1/s^2 and da/dv of 1/s,
// and BOTH ARE INVARIANT under the uniform scaling that takes metres to
// kilometres: scaling r and a by the same factor leaves da/dr unchanged. So A is
// the same matrix in either unit system and no conversion appears here. The
// crossing stays where SPEC-dynamics DYN-R-010 put it -- the two sites in
// dynamics.cpp -- and `ci.sh` gate 11 confirms this file adds none.
//
// THE DERIVATIVES ARE ANALYTIC, NEVER FINITE-DIFFERENCED (STM-R-003), because
// the gate is agreement with finite differences and a finite-differenced
// implementation checked against finite differences would pass while checking
// nothing: the two would share the step size, the cancellation and the
// truncation.

#include <odl/dynamics/force_set.hpp>
#include <odl/integrators/integrate.hpp>

#include <array>
#include <vector>

namespace odl::dyn {

using Mat6 = std::array<std::array<double, 6>, 6>;

[[nodiscard]] inline Mat6 identity6() noexcept {
    Mat6 m{};
    for (std::size_t i = 0; i < 6; ++i) m[i][i] = 1.0;
    return m;
}

struct StmSolution {
    frames::State<frames::Frame::GCRS> state;
    Mat6 phi{};
    odl::integrators::Record record{};
    /// The integral of tr(A) over the propagation, carried alongside Phi.
    ///
    /// LIOUVILLE, IN THE FORM THAT SURVIVES DRAG. d(det Phi)/dt = tr(A) det Phi,
    /// so det Phi = exp(integral of tr A dt). For the forces L3 has, tr(da/dv)
    /// is zero and this integral is zero and the determinant is 1 -- but writing
    /// the test as "det Phi == 1" would make it FALSE the moment drag arrives at
    /// L4 with tr(da/dv) < 0, and whoever met the failure would restrict it to
    /// conservative forces or delete it. That would lose the only check on Phi
    /// that involves no difference estimate, at exactly the moment the dynamics
    /// get harder.
    ///
    /// In this form the conservative case is the SPECIAL CASE where the integral
    /// vanishes, and at L4 the check gets STRONGER rather than merely surviving:
    /// with drag the determinant actually moves, so it verifies the dissipation
    /// rate instead of confirming a constant. One extra scalar in the
    /// variational state buys that.
    double integrated_trace = 0.0;
    /// Summed over forces that declared no velocity dependence (DYN-R-027).
    /// A Phi whose da/dv block is partly ABSENT is not the same object as one
    /// whose block is zero, and the bound is what says which (STM-R-011).
    double neglected_velocity_bound_per_s = 0.0;
};

/// State, Phi, AND a sensitivity column for every registered parameter.
///
/// L3 STEP 4, AND THE WHOLE LAYER EXISTS FOR THE SECOND HALF OF ITS GATE:
/// "registering a second parameter requires no change to the integrator". It is
/// easy to write a registry where adding a parameter works while something
/// downstream quietly knew the width all along -- and templating the integrator
/// on a compile-time size would have been exactly that, satisfying every test at
/// n = 1 and n = 2 BY RECOMPILING. The integrator takes a runtime-sized state and
/// `y0.size()` is the only answer available to it.
///
/// S = dx(t)/dp satisfies  dS/dt = A S + B,  S(t0) = 0,  with B = [0; da/dp],
/// because the INITIAL state does not depend on the parameters.
struct SensitivitySolution {
    frames::State<frames::Frame::GCRS> state;
    Mat6 phi{};
    /// Row-major 6 x n, n = registry.size(). Addressed by ParameterId, never by
    /// column number (DYN-R-005).
    std::vector<double> sensitivities;
    std::size_t width = 0;
    odl::integrators::Record record{};
    double integrated_trace = 0.0;
    double neglected_velocity_bound_per_s = 0.0;

    [[nodiscard]] odl::Result<std::array<double, 6>, DynError>
    column(const ParameterRegistry& reg, const ParameterId& id) const;
};

[[nodiscard]] odl::Result<SensitivitySolution, DynError>
propagate_with_sensitivities(const ForceSet& forces, const ParameterSet& params,
                             const ParameterRegistry& registry,
                             const frames::State<frames::Frame::GCRS>& x0, double seconds,
                             double tolerance,
                             odl::integrators::Control ctl = odl::integrators::Control{});

/// Propagate the state and Phi together over `seconds`.
[[nodiscard]] odl::Result<StmSolution, DynError>
propagate_with_stm(const ForceSet& forces, const ParameterSet& params,
                   const frames::State<frames::Frame::GCRS>& x0, double seconds,
                   double tolerance,
                   odl::integrators::Control ctl = odl::integrators::Control{});

/// Propagate the state alone, by the same route, so that a finite-difference
/// comparison against `propagate_with_stm` differences the SAME integration
/// rather than a different one.
[[nodiscard]] odl::Result<frames::State<frames::Frame::GCRS>, DynError>
propagate(const ForceSet& forces, const ParameterSet& params,
          const frames::State<frames::Frame::GCRS>& x0, double seconds, double tolerance,
          odl::integrators::Control ctl = odl::integrators::Control{});

}  // namespace odl::dyn
