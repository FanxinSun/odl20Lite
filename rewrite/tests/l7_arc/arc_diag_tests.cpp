// arc_diag_tests.cpp — L7 step 3, part 2: ARC-X-008, the manager's DIAGNOSTIC of the noise-run stop (L7_step3_registration.txt, amendment 7, registered with its prediction BEFORE it ran).  A test of a hypothesis: the
// library is used as it is (the continuation sets the library's own `step_tolerance` to 0); nothing is changed and no fix is proposed.  Hidden ([.diag]).  The one command:
//     ./tests/odl_l7_arc_tests "ARC-X-008*"
#include "arc_runs.hpp"

using namespace l7arc;

namespace {

/// Eigenvalues of a symmetric n x n matrix (row-major) by cyclic Jacobi rotations, ascending.
std::vector<double> eigenvalues(std::vector<double> a, std::size_t n) {
    for (int sweep = 0; sweep < 100; ++sweep) {
        double off = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) off += a[i * n + j] * a[i * n + j];
        if (off < 1e-30) break;
        for (std::size_t p = 0; p < n; ++p)
            for (std::size_t q = p + 1; q < n; ++q) {
                if (std::abs(a[p * n + q]) < 1e-300) continue;
                const double theta = (a[q * n + q] - a[p * n + p]) / (2.0 * a[p * n + q]);
                const double t = (theta >= 0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0), s = t * c;
                for (std::size_t k = 0; k < n; ++k) {
                    const double akp = a[k * n + p], akq = a[k * n + q];
                    a[k * n + p] = c * akp - s * akq;
                    a[k * n + q] = s * akp + c * akq;
                }
                for (std::size_t k = 0; k < n; ++k) {
                    const double apk = a[p * n + k], aqk = a[q * n + k];
                    a[p * n + k] = c * apk - s * aqk;
                    a[q * n + k] = s * apk + c * aqk;
                }
            }
    }
    std::vector<double> e(n);
    for (std::size_t i = 0; i < n; ++i) e[i] = a[i * n + i];
    std::sort(e.begin(), e.end());
    return e;
}

/// The whitened normal matrix N = A^T A (own units, row-major 6 x 6) at x.
std::vector<double> normal_at(const ArcModel& arc, const std::array<double, 6>& x) {
    auto ev = arc.evaluate(x, true);
    REQUIRE(ev.has_value());
    std::vector<double> n(36, 0.0);
    for (std::size_t k = 0; k < arc.count(); ++k)
        for (std::size_t i = 0; i < 6; ++i)
            for (std::size_t j = 0; j < 6; ++j) n[i * 6 + j] += (ev->rows[k * 6 + i] / kRangeSigmaM) * (ev->rows[k * 6 + j] / kRangeSigmaM);
    return n;
}

double quad(const std::vector<double>& n, const std::array<double, 6>& h) {
    double s = 0.0;
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j) s += h[i] * n[i * 6 + j] * h[j];
    return s;
}

void print_system(const char* label, const odl::estimation::LmResult& r) {
    if (!r.final_system) { std::printf("ARCDIAG %s final_system: none\n", label); return; }
    const auto& s = *r.final_system;
    std::printf("ARCDIAG %s system: kappa_est=%.6e kappa_up=%.6e B=%.3e\n", label, s.condition.condition_estimate, s.condition.condition_upper, s.certified_relative_error);
    const auto ev = eigenvalues(s.scaled_matrix, s.free_parameters);
    std::printf("ARCDIAG %s scaled eigenvalues:", label);
    for (double e : ev) std::printf(" %.6e", e);
    std::printf("\nARCDIAG %s formal sigma (sqrt diag cofactor, own units):", label);
    for (std::size_t i = 0; i < 6; ++i) std::printf(" %.4e", std::sqrt(s.cofactor[i * 6 + i]));
    std::printf("\n");
    std::fflush(stdout);
}

/// Continue from `x0` with the step test OFF; print every iterate; return the final result.
odl::estimation::LmResult continuation(const char* label, const ArcModel& arc, const std::array<double, 6>& x0, std::array<double, 6>* x_end) {
    odl::estimation::LmOptions o;
    o.step_tolerance = 0.0;
    o.max_iterations = 200;
    const auto l = run_lm(arc, std::array<double, 6>{}, x0, o);          // the truth argument is unused here (errors are reported separately)
    const auto& r = l.result;
    std::printf("ARCDIAG %s continuation: status=%s iterations=%zu accepted=%zu rejected=%zu F=%.6f cosine=%.3e mu_end=%.3e evals=%zu wall_s=%.0f msg=\"%s\"\n", label, odl::estimation::to_string(r.status), r.iterations,
                r.accepted_steps, r.rejected_steps, r.objective, r.cosine, r.mu, l.evaluations, l.seconds, r.message.c_str());
    std::size_t acc = 0;
    std::array<double, 6> prev = x0;
    for (const auto& it : r.history) {
        double decrement = std::nan("");
        std::array<double, 6> move{};
        if (it.accepted) {
            const auto& xn = r.accepted_iterates[acc++];
            std::array<double, 6> cur{xn[0], xn[1], xn[2], xn[3], xn[4], xn[5]};
            for (std::size_t i = 0; i < 6; ++i) move[i] = cur[i] - prev[i];
            decrement = quad(normal_at(arc, prev), move);
            prev = cur;
        }
        std::printf("ARCDIAG %s k=%zu %s F=%.6f cosine=%.3e mu=%.3e rho=%.3f ||h_s||=%.3e newton_decrement=%.3e move=[%.3e %.3e %.3e %.3e %.3e %.3e]\n", label, it.k, it.accepted ? "acc" : "rej", it.objective, it.cosine, it.mu, it.rho,
                    it.step_norm_scaled, decrement, move[0], move[1], move[2], move[3], move[4], move[5]);
        std::fflush(stdout);
    }
    if (x_end) *x_end = {r.x[0], r.x[1], r.x[2], r.x[3], r.x[4], r.x[5]};
    print_system(label, r);
    return r;
}

}  // namespace

TEST_CASE("ARC-X-008  the noise-run diagnostic", "[.diag]") {
    ArcForces forces;
    const ArcDesign d = gate_arc_design(3.0);
    const auto truth = cartesian_of(d);
    const auto grid = state_grid(forces, state_from(epoch_of(d), truth), d.days * 86400.0, kArcGridStepS);
    ArcModel noisy(forces, d, grid, kArcGridStepS);
    noisy.generate_data(truth, 20251220ULL, kRangeSigmaM);
    REQUIRE(noisy.data_hash() == "7e2530c6808e176f");
    // (1) the noise run again: it must reproduce
    const auto start = start_of(LadderKind::P, d, truth, kRungsPGate[2]);
    const auto l0 = run_lm(noisy, truth, start);
    print_lm("gate-noeclipse-noise/P", kRungsPGate[2], l0, noisy.count());
    std::printf("ARCDIAG original run: status=%s iterations=%zu F=%.6f cosine=%.4e mu_end=%.4e\n", odl::estimation::to_string(l0.result.status), l0.result.iterations, l0.result.objective, l0.result.cosine, l0.result.mu);
    REQUIRE(l0.result.status == odl::estimation::LmStatus::stalled);
    print_system("stalled-point", l0.result);
    const std::array<double, 6> x_stalled{l0.result.x[0], l0.result.x[1], l0.result.x[2], l0.result.x[3], l0.result.x[4], l0.result.x[5]};
    // (2) the continuation, step test off
    std::array<double, 6> x_end{};
    const auto r = continuation("from-stalled", noisy, x_stalled, &x_end);
    // total move in formal sigma and the Mahalanobis distance
    if (r.final_system) {
        std::array<double, 6> mv{};
        for (std::size_t i = 0; i < 6; ++i) mv[i] = x_end[i] - x_stalled[i];
        std::printf("ARCDIAG total move from the stalled point (own units): [%.4e %.4e %.4e %.4e %.4e %.4e]\n", mv[0], mv[1], mv[2], mv[3], mv[4], mv[5]);
        std::printf("ARCDIAG total move in formal sigma:");
        double nrm = 0.0;
        for (std::size_t i = 0; i < 6; ++i) { const double sg = std::sqrt(r.final_system->cofactor[i * 6 + i]); std::printf(" %.4f", mv[i] / sg); nrm += (mv[i] / sg) * (mv[i] / sg); }
        std::printf("   (norm %.4f)\n", std::sqrt(nrm));
        std::printf("ARCDIAG Mahalanobis distance^2 of the move (Delta' N Delta, N at the end): %.4f (6 dof)\n", quad(normal_at(noisy, x_end), mv));
        std::fflush(stdout);
    }
    // (3) the control: the same continuation from the truth with the seeded data
    (void)continuation("control-from-truth", noisy, truth, nullptr);
}
