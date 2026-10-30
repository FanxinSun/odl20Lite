// levenberg_marquardt.cpp — see odl/estimation/levenberg_marquardt.hpp.  Plain double, no contraction (the module is built with -ffp-contract=off): the order of the sums below is
// the specification's (SPEC-estimation.md v0.4), so that the iterates are the same on every compiler that honours IEEE.
#include <odl/estimation/levenberg_marquardt.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace odl::estimation {

namespace {

struct Point {
    std::vector<double> residual;
    std::vector<double> jacobian;
    double objective = 0.0;
    // derived
    std::vector<double> normal;                      // n x n, LOWER triangle filled, full symmetric after mirror
    std::vector<double> rhs;                         // b = A^T r
    double residual_norm = 0.0;                      // sqrt(sum r^2)
    double cosine = 0.0;                             // max_i |b_i| / (sqrt(N_ii) ||r||)
};

bool finite_all(const std::vector<double>& v) {
    for (double d : v)
        if (!std::isfinite(d)) return false;
    return true;
}

// Forms everything the loop reads from an evaluation; false (and `why`) when the evaluation cannot be used.
bool derive(Point& p, std::size_t n, LmEvaluation&& e, bool with_jacobian, std::string& why) {
    if (!finite_all(e.residual)) {
        why = "the residual is not finite";
        return false;
    }
    double sum = 0.0;
    for (double r : e.residual) sum += r * r;
    p.residual = std::move(e.residual);
    p.residual_norm = std::sqrt(sum);
    p.objective = e.objective ? *e.objective : 0.5 * sum;
    if (!std::isfinite(p.objective)) {
        why = "the objective is not finite";
        return false;
    }
    if (!with_jacobian) return true;
    const std::size_t m = p.residual.size();
    if (e.jacobian.size() != m * n) {
        why = "the Jacobian has " + std::to_string(e.jacobian.size()) + " entries; " + std::to_string(m) + " x " + std::to_string(n) + " were expected";
        return false;
    }
    if (!finite_all(e.jacobian)) {
        why = "the Jacobian is not finite";
        return false;
    }
    p.jacobian = std::move(e.jacobian);
    p.normal.assign(n * n, 0.0);
    p.rhs.assign(n, 0.0);
    for (std::size_t k = 0; k < m; ++k) {
        const double* row = &p.jacobian[k * n];
        for (std::size_t i = 0; i < n; ++i) {
            p.rhs[i] += row[i] * p.residual[k];
            for (std::size_t j = 0; j <= i; ++j) p.normal[i * n + j] += row[i] * row[j];
        }
    }
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) p.normal[i * n + j] = p.normal[j * n + i];
    p.cosine = 0.0;
    if (p.residual_norm > 0.0) {
        for (std::size_t i = 0; i < n; ++i) {
            const double d = p.normal[i * n + i];
            if (d > 0.0) p.cosine = std::max(p.cosine, std::fabs(p.rhs[i]) / (std::sqrt(d) * p.residual_norm));
        }
    }
    return true;
}

// The exact scale exponent of a diagonal entry; a zero (or unusable) diagonal is scaled by 1.
int exponent_of(double diagonal) noexcept { return diagonal > 0.0 && std::isfinite(diagonal) ? scale_exponent(diagonal) : 0; }

}  // namespace

const char* to_string(LmStatus s) noexcept {
    switch (s) {
        case LmStatus::converged_gradient: return "converged_gradient";
        case LmStatus::converged_residual: return "converged_residual";
        case LmStatus::converged_step: return "converged_step";
        case LmStatus::stalled: return "stalled";
        case LmStatus::max_iterations: return "max_iterations";
        case LmStatus::refused: return "refused";
    }
    return "?";
}

LmResult levenberg_marquardt(const std::vector<std::string>& names, std::span<const double> x0, const LmModel& model, const LmOptions& opt) {
    const std::size_t n = names.size();
    LmResult out;
    out.names = names;
    out.cosine = std::numeric_limits<double>::quiet_NaN();
    out.x.assign(x0.begin(), x0.end());
    if (n == 0 || x0.size() != n) {
        out.message = "the parameter list has " + std::to_string(n) + " names and the start " + std::to_string(x0.size()) + " values";
        return out;
    }
    auto evaluate = [&](std::span<const double> x, bool with_jacobian, Point& p, std::string& why) {
        auto e = model(x, with_jacobian);
        if (!e) {
            why = "the model refused: " + e.error().message;
            return false;
        }
        return derive(p, n, std::move(*e), with_jacobian, why);
    };

    Point cur;
    std::string why;
    if (!evaluate(out.x, true, cur, why)) {
        out.message = "the starting point cannot be used: " + why;
        return out;
    }
    const double f0 = cur.objective;
    const std::size_t m = cur.residual.size();
    out.objective = cur.objective;
    out.cosine = cur.cosine;

    // mu_0 = tau * max_i of the scaled diagonal (N_s)_ii = N_ii 2^(-2 e_i), which lies in [1/2, 2)
    double scaled_max = 0.0;
    std::vector<int> e(n, 0);
    auto rescale = [&]() {
        scaled_max = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            e[i] = exponent_of(cur.normal[i * n + i]);
            scaled_max = std::max(scaled_max, std::ldexp(cur.normal[i * n + i], -2 * e[i]));
        }
    };
    rescale();
    double mu = opt.tau * (scaled_max > 0.0 ? scaled_max : 1.0);
    double nu = 2.0;
    out.mu = mu;

    auto finish_system = [&]() {
        std::vector<double> mat = cur.normal;
        auto ne = NormalEquations::from_normal_system(names, mat, cur.rhs, m, 2.0 * (cur.objective));
        if (!ne) return;
        auto sol = ne->solve();
        if (sol) out.final_system = std::move(*sol);
    };

    std::size_t k = 0;
    // THE LATEST TRIAL COULD NOT BE EVALUATED (the model refused it, its numbers were not finite, or step 2 refused the damped system): until a trial IS evaluated, no stop may be reported
    // as arrival -- a stop in that state is `refused`, whichever test fired (the manager's ruling of 2026-10-31).
    bool unevaluated = false;
    std::string unevaluated_why;
    for (;;) {
        // arrival tests at the current iterate
        if (cur.objective <= opt.residual_fraction * f0) {
            out.status = LmStatus::converged_residual;
            out.message = "F = " + std::to_string(cur.objective) + " <= " + std::to_string(opt.residual_fraction) + " F(x0)";
            break;
        }
        if (cur.cosine <= opt.gradient_cosine) {
            out.status = LmStatus::converged_gradient;
            out.message = "the largest |cosine| between the residual and a Jacobian column is " + std::to_string(cur.cosine);
            break;
        }
        if (k >= opt.max_iterations) {
            if (unevaluated) {
                out.status = LmStatus::refused;
                out.message = "k_max = " + std::to_string(opt.max_iterations) + " reached while the latest trial could not be evaluated: " + unevaluated_why;
                break;
            }
            out.status = LmStatus::max_iterations;
            out.message = "k_max = " + std::to_string(opt.max_iterations) + " reached";
            break;
        }
        ++k;
        out.iterations = k;
        LmIteration rec;
        rec.k = k;
        rec.objective = cur.objective;
        rec.cosine = cur.cosine;
        rec.mu = mu;
        rec.rho = std::numeric_limits<double>::quiet_NaN();

        // the damped system (N + mu W) h = b, W = diag(2^(2 e_i))
        std::vector<double> damped = cur.normal;
        for (std::size_t i = 0; i < n; ++i) damped[i * n + i] += std::ldexp(mu, 2 * e[i]);
        auto ne = NormalEquations::from_normal_system(names, damped, cur.rhs, m, 2.0 * cur.objective);
        std::optional<Solution> sol;
        std::string refusal;
        if (ne) {
            auto s = ne->solve();
            if (s) sol = std::move(*s);
            else refusal = std::string(s.error().id) + ": " + s.error().message;
        } else {
            refusal = std::string(ne.error().id) + ": " + ne.error().message;
        }
        bool accepted = false;
        if (!sol) {
            unevaluated = true;
            unevaluated_why = "the damped system was refused: " + refusal;
        }
        if (sol) {
            const std::vector<double>& h = sol->x;
            // the step test, on the SCALED step and iterate (y = 2^e x)
            double hs = 0.0, xs = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const double a = std::ldexp(h[i], e[i]);
                const double b = std::ldexp(out.x[i], e[i]);
                hs += a * a;
                xs += b * b;
            }
            hs = std::sqrt(hs);
            xs = std::sqrt(xs);
            rec.step_norm_scaled = hs;
            if (hs <= opt.step_tolerance * (xs + opt.step_tolerance)) {
                if (unevaluated) {
                    out.status = LmStatus::refused;
                    out.message = "the step test held (||h_s|| = " + std::to_string(hs) + ") but the latest trial could not be evaluated: " + unevaluated_why;
                    out.history.push_back(rec);
                    break;
                }
                if (cur.cosine > opt.stalled_cosine) {
                    out.status = LmStatus::stalled;
                    out.message = "the step test held (||h_s|| = " + std::to_string(hs) + ") while the gradient cosine is " + std::to_string(cur.cosine) + " > " +
                                  std::to_string(opt.stalled_cosine);
                } else {
                    out.status = LmStatus::converged_step;
                    out.message = "the step test held (||h_s|| = " + std::to_string(hs) + "), gradient cosine " + std::to_string(cur.cosine);
                }
                out.history.push_back(rec);
                break;
            }
            // the trial
            std::vector<double> xn(n);
            for (std::size_t i = 0; i < n; ++i) xn[i] = out.x[i] + h[i];
            Point trial;
            std::string trial_why;
            if (evaluate(xn, false, trial, trial_why)) {
                unevaluated = false;
                double hb = 0.0, hwh = 0.0;
                for (std::size_t i = 0; i < n; ++i) {
                    hb += h[i] * cur.rhs[i];
                    hwh += h[i] * h[i] * std::ldexp(1.0, 2 * e[i]);
                }
                const double predicted = 0.5 * (hb + mu * hwh);
                if (predicted > 0.0 && std::isfinite(predicted)) {
                    rec.rho = (cur.objective - trial.objective) / predicted;
                    if (rec.rho > 0.0) {
                        Point next;
                        std::string next_why;
                        if (evaluate(xn, true, next, next_why)) {
                            accepted = true;
                            out.x = xn;
                            out.accepted_iterates.push_back(xn);
                            cur = std::move(next);
                            rescale();
                        } else {
                            unevaluated = true;
                            unevaluated_why = "the Jacobian at the accepted trial could not be evaluated: " + next_why;
                        }
                    }
                }
            } else {
                unevaluated = true;
                unevaluated_why = "the trial could not be evaluated: " + trial_why;
            }
        }
        rec.accepted = accepted;
        out.history.push_back(rec);
        if (accepted) {
            ++out.accepted_steps;
            const double t = 2.0 * rec.rho - 1.0;
            mu *= std::max(1.0 / 3.0, 1.0 - t * t * t);
            nu = 2.0;
        } else {
            ++out.rejected_steps;
            mu *= nu;
            nu *= 2.0;
            if (!(mu <= opt.mu_max)) {
                out.status = LmStatus::refused;
                out.message = "every trial was rejected and mu = " + std::to_string(mu) + " exceeds mu_max = " + std::to_string(opt.mu_max) +
                              (refusal.empty() ? std::string() : "; the last refusal of the damped system: " + refusal);
                break;
            }
        }
        out.mu = mu;
    }
    out.objective = cur.objective;
    out.cosine = cur.cosine;
    out.mu = mu;
    finish_system();
    return out;
}

}  // namespace odl::estimation
