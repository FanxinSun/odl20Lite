#pragma once
// lm_controls.hpp — the two CONTROLS of L7 step 3 (SPEC-estimation.md §8.3), written in the test code so that the library under test is not its own witness:
//   * `gauss_newton`   — the plain Gauss–Newton iteration x += (A^T A)^-1 A^T r, every step taken, no damping (the control that must FAIL from at least one start, or the damped method's
//                        advantage is unproved).  The linear solve is step 2's scaled path, so that a failure is the iteration's and not the solver's.
//   * `dtu_unscaled`   — DTU's Algorithm 3.16 LITERALLY: the damped system (A^T A + mu I) h = -g in the parameters' own units, factored by plain Cholesky, stopping tests (3.15a)
//                        ||g||_inf <= eps1 and (3.15b) ||h|| <= eps2 (||x|| + eps2), mu_0 = tau max a_ii, the update (2.21).  It is the algorithm whose counts DTU prints (88, 175) and
//                        the "unscaled damping" the plan says did better than the scaled from two starts: measured here, not cited.
#include <odl/estimation/dense.hpp>
#include <odl/estimation/levenberg_marquardt.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace odl::estimation::fx {

struct ControlRun {
    std::vector<double> x;
    double objective = std::numeric_limits<double>::quiet_NaN();
    std::size_t iterations = 0;
    std::size_t rejected = 0;
    std::size_t passes = 0;                          ///< dtu_unscaled: DTU's k, every pass of the while loop (accepted or not)
    std::string ended;                               ///< "gradient", "step", "k_max", "non-finite", "refused"
    std::vector<double> mu_history;                  ///< dtu_unscaled: mu at each pass
    std::vector<std::vector<double>> iterates;       ///< dtu_unscaled: x after each ACCEPTED step (for the invariance comparison)
};

struct Normal {
    std::size_t n = 0;
    std::vector<double> a;                           ///< n x n full symmetric
    std::vector<double> g;                           ///< A^T r  (= -g of DTU's sign: DTU's g = J^T f = -A^T r for f = -r)
    double f = 0.0;                                  ///< (1/2) sum r^2
    bool ok = false;
};

[[nodiscard]] inline Normal normal_of(const LmEvaluation& e, std::size_t n) {
    Normal out;
    out.n = n;
    out.a.assign(n * n, 0.0);
    out.g.assign(n, 0.0);
    const std::size_t m = e.residual.size();
    double s = 0.0;
    for (std::size_t k = 0; k < m; ++k) {
        s += e.residual[k] * e.residual[k];
        for (std::size_t i = 0; i < n; ++i) {
            out.g[i] += e.jacobian[k * n + i] * e.residual[k];
            for (std::size_t j = 0; j < n; ++j) out.a[i * n + j] += e.jacobian[k * n + i] * e.jacobian[k * n + j];
        }
    }
    out.f = 0.5 * s;
    out.ok = std::isfinite(out.f);
    return out;
}

/// Plain Gauss–Newton, at most `kmax` steps, ending when the step is below 1e-12 of the iterate (scaled norms by step 2's own scaling are not used: the control is the plain iteration).
[[nodiscard]] inline ControlRun gauss_newton(const std::vector<std::string>& names, std::span<const double> x0, const LmModel& model, std::size_t kmax = 200) {
    const std::size_t n = names.size();
    ControlRun run;
    run.x.assign(x0.begin(), x0.end());
    for (std::size_t k = 0; k < kmax; ++k) {
        auto e = model(run.x, true);
        if (!e) { run.ended = "refused"; return run; }
        const Normal nm = normal_of(*e, n);
        run.objective = nm.f;
        if (!nm.ok) { run.ended = "non-finite"; return run; }
        auto ne = NormalEquations::from_normal_system(names, nm.a, nm.g, e->residual.size(), 2.0 * nm.f);
        if (!ne) { run.ended = "refused"; return run; }
        auto sol = ne->solve();
        if (!sol) { run.ended = "refused"; return run; }
        double hn = 0.0, xn = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            run.x[i] += sol->x[i];
            hn += sol->x[i] * sol->x[i];
            xn += run.x[i] * run.x[i];
            if (!std::isfinite(run.x[i])) { run.ended = "non-finite"; return run; }
        }
        run.iterations = k + 1;
        if (std::sqrt(hn) <= 1.0e-12 * (std::sqrt(xn) + 1.0e-12)) { run.ended = "step"; break; }
        run.ended = "k_max";
    }
    auto e = model(run.x, false);
    if (e) {
        double s = 0.0;
        for (double r : e->residual) s += r * r;
        run.objective = 0.5 * s;
    }
    return run;
}

/// DTU's Algorithm 3.16 as printed, unscaled.
[[nodiscard]] inline ControlRun dtu_unscaled(const std::vector<std::string>& names, std::span<const double> x0, const LmModel& model, double tau, double eps1, double eps2,
                                             std::size_t kmax) {
    const std::size_t n = names.size();
    ControlRun run;
    run.x.assign(x0.begin(), x0.end());
    auto e = model(run.x, true);
    if (!e) { run.ended = "refused"; return run; }
    Normal cur = normal_of(*e, n);
    run.objective = cur.f;
    double nu = 2.0;
    double amax = 0.0;
    for (std::size_t i = 0; i < n; ++i) amax = std::max(amax, cur.a[i * n + i]);
    double mu = tau * amax;
    auto ginf = [&](const Normal& nm) {
        double v = 0.0;
        for (double g : nm.g) v = std::max(v, std::fabs(g));
        return v;
    };
    bool found = ginf(cur) <= eps1;
    run.ended = found ? "gradient" : "k_max";
    std::size_t k = 0;
    while (!found && k < kmax) {
        ++k;
        run.mu_history.push_back(mu);
        std::vector<double> mat = cur.a;
        for (std::size_t i = 0; i < n; ++i) mat[i * n + i] += mu;
        const CholeskyResult c = cholesky_lower(mat, n);
        std::vector<double> h = cur.g;                 // DTU solves (A + mu I) h = -g with g = J^T f = -A^T r: so h solves (...) h = A^T r = cur.g
        bool solved = c.ok;
        if (solved) {
            forward_substitute(mat, n, h);
            backward_substitute_transposed(mat, n, h);
            for (double v : h) solved = solved && std::isfinite(v);
        }
        if (!solved) {
            mu *= nu;
            nu *= 2.0;
            ++run.rejected;
            if (!(mu < 1.0e300)) { run.ended = "refused"; break; }
            continue;
        }
        double hn = 0.0, xn = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            hn += h[i] * h[i];
            xn += run.x[i] * run.x[i];
        }
        hn = std::sqrt(hn);
        xn = std::sqrt(xn);
        if (hn <= eps2 * (xn + eps2)) { found = true; run.ended = "step"; break; }
        std::vector<double> xnew(n);
        for (std::size_t i = 0; i < n; ++i) xnew[i] = run.x[i] + h[i];
        auto en = model(xnew, true);
        double fnew = std::numeric_limits<double>::infinity();
        if (en) {
            double s = 0.0;
            for (double r : en->residual) s += r * r;
            fnew = 0.5 * s;
        }
        double hg = 0.0, hh = 0.0;                     // L(0) - L(h) = (1/2) h^T (mu h - g_DTU) = (1/2) h^T (mu h + A^T r)
        for (std::size_t i = 0; i < n; ++i) {
            hg += h[i] * cur.g[i];
            hh += h[i] * h[i];
        }
        const double predicted = 0.5 * (mu * hh + hg);
        const double rho = std::isfinite(fnew) && predicted > 0.0 ? (cur.f - fnew) / predicted : -1.0;
        if (rho > 0.0) {
            run.x = xnew;
            cur = normal_of(*en, n);
            run.objective = cur.f;
            run.iterates.push_back(run.x);
            ++run.iterations;
            found = ginf(cur) <= eps1;
            if (found) run.ended = "gradient";
            const double t = 2.0 * rho - 1.0;
            mu *= std::max(1.0 / 3.0, 1.0 - t * t * t);
            nu = 2.0;
        } else {
            mu *= nu;
            nu *= 2.0;
            ++run.rejected;
            if (!(mu < 1.0e300)) { run.ended = "refused"; break; }
        }
    }
    run.passes = k;
    if (!found && run.ended != "refused") run.ended = "k_max";
    return run;
}

}  // namespace odl::estimation::fx
