#pragma once
// odl/estimation/levenberg_marquardt.hpp — nonlinear least squares by the Levenberg–Marquardt method (SPEC-estimation.md v0.4, L7 step 3; the manager's
// rulings of 2026-10-06 and 2026-10-31).
//
// THE ALGORITHM is DTU's Algorithm 3.16 (Madsen, Nielsen & Tingleff, IMM-3215, 2004, section 3.2) IN THE SCALED VARIABLES.  At every iterate the parameters are
// scaled by the exact powers of two of step 2 (EST-R-201), recomputed from the diagonal of that iterate's normal matrix N = A^T A; the damped step solves
// (N_s + mu I) y = b_s, which in the parameters' own units is (N + mu W) h = b with W = diag(2^(2 e_i)) — handed to step 2's `NormalEquations::from_normal_system`, so the
// scaled path stays the ONLY path and no factorisation is written here.  The gain ratio is taken on the WHOLE OBJECTIVE (the callback returns it: a prior added by step 4
// is part of it), against the decrease the quadratic model predicts, L(0) - L(h) = (1/2) h^T (mu W h + b).  The damping follows (2.21): on rho > 0 the step is taken and
// mu *= max(1/3, 1 - (2 rho - 1)^3), nu = 2; otherwise mu *= nu, nu *= 2.
//
// A FAILED TRIAL IS A REJECTED STEP.  A non-finite objective or residual, a Jacobian that is not finite, and a damped system that step 2 refuses all raise mu and the
// iteration continues; the run ends `refused` only when mu exceeds `mu_max`.
//
// A RESULT CARRIES A STATUS, never a bare "converged".  Every decision read from the matrix is made on SCALED quantities and is DIMENSIONLESS: the gradient test is the largest
// |cosine| between the residual and a column of the Jacobian, not a norm with units, so rescaling the residuals or the parameters by powers of two changes neither the iterates nor
// the status (tested).  A stop by the step test while that cosine is above `stalled_cosine` is `stalled`, not converged.

#include <odl/core/result.hpp>
#include <odl/estimation/normal_equations.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace odl::estimation {

/// What the model returns at a point x: everything WHITENED (divided by the observation's standard deviation), so that the objective is F = (1/2) sum r_k^2 and N = A^T A.
struct LmEvaluation {
    std::vector<double> residual;                    ///< r_k = (observed_k - computed_k) / sigma_k, m values
    std::vector<double> jacobian;                    ///< A_ki = d(computed_k)/d(x_i) / sigma_k, m x n row-major (the PARTIALS OF THE MODEL, so that N h = A^T r moves x towards the data)
    std::optional<double> objective;                 ///< F(x) when it is not (1/2) sum r^2 (a prior adds to it, step 4); empty: computed from `residual`
};

/// The model, as a callable.  It returns an evaluation or a refusal (a point outside the model's domain): a refusal is a REJECTED TRIAL, not an abort.  `with_jacobian` is false
/// for a trial point whose Jacobian is not yet needed (it is evaluated again only if the step is accepted).
using LmModel = std::function<odl::Result<LmEvaluation, EstimationRefusal>(std::span<const double> x, bool with_jacobian)>;

enum class LmStatus {
    converged_gradient,       ///< the dimensionless gradient test held: max |cos(r, a_i)| <= gradient_cosine
    converged_residual,       ///< the zero-residual test held: F <= residual_fraction * F(x0)
    converged_step,           ///< the step test held AND the gradient cosine is not above stalled_cosine
    stalled,                  ///< the step test held while the gradient cosine is above stalled_cosine: the iteration stopped moving without having arrived
    max_iterations,           ///< k_max reached
    refused,                  ///< mu exceeded mu_max, or the model refused the starting point, or a stop (the step test, k_max) came while the LATEST trial could not be evaluated; `message` says which
};

[[nodiscard]] const char* to_string(LmStatus s) noexcept;

struct LmOptions {
    double tau = 1.0e-3;                 ///< mu_0 = tau * max_i (N_s)_ii; the scaled diagonal lies in [1/2, 2), so this is tau * (a number of order one)
    double gradient_cosine = 1.0e-10;    ///< converged_gradient: the largest |cosine| between the residual and a Jacobian column, DIMENSIONLESS
    double residual_fraction = 1.0e-30;  ///< converged_residual: F <= residual_fraction * F(x0) (the zero-residual problems' own test; the cosine is indeterminate there)
    double step_tolerance = 1.0e-12;     ///< eps_2 of (3.14), applied to the SCALED step and iterate: ||h_s|| <= eps_2 (||x_s|| + eps_2)
    double stalled_cosine = 1.0e-4;      ///< a step stop with a gradient cosine above this is `stalled`
    std::size_t max_iterations = 1000;   ///< k_max (every pass of the loop, accepted or rejected)
    double mu_max = 1.0e16;              ///< the registered limit at which the run ends `refused`
};

/// One pass of the loop.
struct LmIteration {
    std::size_t k = 0;
    double objective = 0.0;              ///< F at the iterate BEFORE the trial
    double cosine = 0.0;                 ///< the gradient cosine at that iterate
    double mu = 0.0;                     ///< the damping used for the trial
    double rho = 0.0;                    ///< the gain ratio (NaN for a trial that was refused)
    double step_norm_scaled = 0.0;       ///< ||h_s||_2 (0 for a refused trial)
    bool accepted = false;
};

struct LmResult {
    LmStatus status = LmStatus::refused;
    std::string message;                 ///< what ended the run, with the numbers
    std::vector<std::string> names;
    std::vector<double> x;               ///< the last ACCEPTED iterate
    double objective = 0.0;              ///< F there
    double cosine = 0.0;                 ///< the gradient cosine there (NaN if never evaluated)
    std::size_t iterations = 0;          ///< passes of the loop
    std::size_t accepted_steps = 0;
    std::size_t rejected_steps = 0;
    double mu = 0.0;                     ///< the damping at the end
    std::vector<LmIteration> history;
    std::vector<std::vector<double>> accepted_iterates;   ///< x after each ACCEPTED step, in order (the starting point is not repeated)
    /// The UNDAMPED solve at x (step 2's Solution: cofactor, condition, sigma_0^2), when step 2 accepts the system; empty otherwise.
    std::optional<Solution> final_system;
};

/// Minimises F(x) = (1/2) sum r_k(x)^2 from `x0`.  The parameters are named (step 2 needs names); `observations` is m.
[[nodiscard]] LmResult levenberg_marquardt(const std::vector<std::string>& names, std::span<const double> x0, const LmModel& model, const LmOptions& options = {});

}  // namespace odl::estimation
