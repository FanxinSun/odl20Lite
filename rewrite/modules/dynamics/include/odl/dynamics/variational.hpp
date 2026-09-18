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
    /// Summed over forces that declared no velocity dependence (DYN-R-027).
    /// A Phi whose da/dv block is partly ABSENT is not the same object as one
    /// whose block is zero, and the bound is what says which (STM-R-011).
    double neglected_velocity_bound_per_s = 0.0;
};

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
