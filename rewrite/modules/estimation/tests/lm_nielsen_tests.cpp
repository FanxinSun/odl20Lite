// lm_nielsen_tests.cpp — SPEC-estimation.md §8.3: NIELSEN'S TWENTY PROBLEMS (IMM-REP-1999-05, the Appendix), the held-out test of L7 step 3.
//   EST-A-330  the FIXTURE: each problem is checked against what the Appendix prints (its exact solution or its printed minimiser and F), by structure where it prints no minimiser, and its forward-mode
//              Jacobian against central differences -- BEFORE any fit uses the problem;
//   EST-A-331  the HELD-OUT comparison (registered in L7_step3_registration.txt before it was run; run ONCE): over the 29 instances of the report's Tables 3.3a-b that the document supports (Problem 17's
//              responses are not in it), the library at tau = 1e-3 (A), the library at the registered candidate tau = 1 (B), and DTU's Algorithm 3.16 unscaled at tau = 1e-3 (C): reached / not reached against the
//              printed F(x*), and the pass counts.  REPORTED, not gated: the manager rules on any change of default.
#include "lm_controls.hpp"
#include "nielsen.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <functional>

using namespace odl::estimation;
using namespace odl::estimation::fx;

namespace {

// the problem as a fit callback: residual r = f (DTU's f), Jacobian = -df/dx (this library's `jacobian` is the partials of the MODEL, residual = observed - model)
LmModel nielsen_model(int pno, std::size_t m, std::size_t n) {
    return [pno, m, n](std::span<const double> x, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(m);
        if (jac) {
            std::vector<Dual> xd(n), f(m);
            for (std::size_t j = 0; j < n; ++j) xd[j] = Dual::variable(x[j], j);
            nielsen_f<Dual>(pno, m, n, xd.data(), f.data());
            e.jacobian.assign(m * n, 0.0);
            for (std::size_t i = 0; i < m; ++i) {
                e.residual[i] = f[i].v;
                for (std::size_t j = 0; j < n; ++j) e.jacobian[i * n + j] = -f[i].g[j];
            }
        } else {
            std::vector<double> f(m);
            nielsen_f<double>(pno, m, n, x.data(), f.data());
            for (std::size_t i = 0; i < m; ++i) e.residual[i] = f[i];
        }
        return e;
    };
}

double nielsen_F(int pno, std::size_t m, std::size_t n, const std::vector<double>& x) {
    std::vector<double> f(m);
    nielsen_f<double>(pno, m, n, x.data(), f.data());
    double s = 0.0;
    for (double v : f) s += v * v;
    return 0.5 * s;
}

std::vector<std::string> pnames(std::size_t n) {
    std::vector<std::string> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back("x" + std::to_string(i + 1));
    return v;
}

}  // namespace

TEST_CASE("EST-A-330: Nielsen's problems are the Appendix's problems -- exact solutions, printed minimisers, structure, and forward-mode Jacobians", "[estimation][lm][nielsen][fixture]") {
    // (a) the linear problems at their exact minimisers: F(x*) of the Appendix's formulae, which also reproduce Table 3.3b's three digits
    for (const auto& [m, n] : std::vector<std::pair<std::size_t, std::size_t>>{{8, 8}, {32, 16}}) {
        const double dm = static_cast<double>(m);
        INFO("m = " << m << ", n = " << n);
        CHECK(std::fabs(nielsen_F(1, m, n, std::vector<double>(n, -1.0)) - 0.5 * (dm - static_cast<double>(n))) <= 1.0e-12);
        std::vector<double> x2(n, 0.0);
        x2[0] = 3.0 / (2.0 * dm + 1.0);                                       // the sum j x_j = 3/(2m+1)
        CHECK(std::fabs(nielsen_F(2, m, n, x2) - dm * (dm - 1.0) / (4.0 * (2.0 * dm + 1.0))) <= 1.0e-12);
        std::vector<double> x3(n, 0.0);
        x3[1] = 3.0 / (2.0 * dm - 3.0) / 2.0;                                  // the sum b x_b = 3/(2m-3), b = 2 for the only nonzero entry
        CHECK(std::fabs(nielsen_F(3, m, n, x3) - (dm * dm + 3.0 * dm - 6.0) / (4.0 * (2.0 * dm - 3.0))) <= 1.0e-12);
    }
    CHECK(std::fabs(nielsen_F(2, 8, 8, [] { std::vector<double> x(8, 0.0); x[0] = 3.0 / 17.0; return x; }()) - 0.824) < 5.0e-4);   // Table 3.3b: 8.24e-01
    CHECK(std::fabs(nielsen_F(3, 32, 16, [] { std::vector<double> x(16, 0.0); x[1] = 3.0 / 61.0 / 2.0; return x; }()) - 4.57) < 5.0e-3);   // 4.57e+00

    // (b) the zero-residual problems at their stated solutions
    CHECK(nielsen_F(4, 2, 2, {1.0, 1.0}) == 0.0);
    CHECK(nielsen_F(5, 3, 3, {1.0, 0.0, 0.0}) == 0.0);
    CHECK(nielsen_F(6, 4, 4, {0.0, 0.0, 0.0, 0.0}) == 0.0);
    CHECK(nielsen_F(12, 5, 3, {1.0, 10.0, 1.0}) < 1.0e-28);
    CHECK(nielsen_F(12, 5, 3, {10.0, 1.0, -1.0}) < 1.0e-28);
    CHECK(nielsen_F(12, 10, 3, {3.0, 3.0, 0.0}) < 1.0e-28);                       // x1 = x2 = alpha, x3 = 0
    for (std::size_t n : {5u, 10u}) {
        CHECK(nielsen_F(16, n, n, std::vector<double>(n, 1.0)) == 0.0);          // alpha = 1 is a root of n a^n - (n+1) a^(n-1) + 1
        std::vector<double> local(n, 0.0);
        local[n - 1] = static_cast<double>(n + 1);
        CHECK(std::fabs(nielsen_F(16, n, n, local) - 0.5) <= 1.0e-12);            // the local minimum the Appendix names
    }

    // (c) the printed minimisers and F(x*): the digits printed
    CHECK(std::fabs(nielsen_F(7, 2, 2, {11.4128, -0.896805}) - 24.4921) <= 1.0e-4 * 24.4921);
    CHECK(std::fabs(nielsen_F(8, 15, 3, {0.082411, 1.133036, 2.343695}) - 4.10744e-3) <= 1.0e-3 * 4.10744e-3);
    CHECK(std::fabs(nielsen_F(9, 11, 4, {0.192807, 0.191282, 0.123057, 0.136062}) - 1.53753e-4) <= 2.0e-3 * 1.53753e-4);
    // Meyer is ill-conditioned: a unit in the last printed digit of x moves F by about 1e-3 relative (probed), so these two are held to 2e-3, and the problem is the one EST-A-320 verified against DTU's counts
    CHECK(std::fabs(nielsen_F(10, 16, 3, {0.00560964, 6181.35, 345.224}) - 43.9729) <= 2.0e-3 * 43.9729);
    CHECK(std::fabs(nielsen_F(13, 10, 2, {0.257825, 0.257825}) - 62.1811) <= 1.0e-4 * 62.1811);
    CHECK(std::fabs(nielsen_F(14, 20, 4, {-11.5944, 13.2036, -0.4034, 0.2368}) - 4.29112e4) <= 1.0e-3 * 4.29112e4);
    CHECK(std::fabs(nielsen_F(18, 45, 4, {-4.0, -5.0, 4.0, -4.0}) - 5.0e-3) <= 2.0e-2 * 5.0e-3);   // generated so that this is the least-squares solution; rounding the y's to 6 decimals perturbs it
    CHECK(std::fabs(nielsen_F(19, 45, 2, {-4.0, -5.0}) - 5.0e-3) <= 2.0e-2 * 5.0e-3);
    CHECK(std::fabs(nielsen_F(20, 16, 3, {2.481778, 6.18135, 3.45224}) - 4.39729e-5) <= 3.0e-3 * 4.39729e-5);   // Table 3.3b prints 4.40e-05 (the Appendix's exponent, read from its image, was misread by one: 43.97e-6)

    // (d) the two problems whose minimiser the Appendix does not print: Chebyquad by structure, Watson by the series it fits
    {
        // y_i = int_0^1 T_i(x) dx, 0 for odd i and -1/(i^2-1) for even i: against a numerical quadrature of the shifted polynomial cos(i arccos(2x-1))
        for (std::size_t i = 1; i <= 18; ++i) {
            const std::size_t steps = 200000;
            double s = 0.0;
            for (std::size_t k = 0; k < steps; ++k) {
                const double x = (static_cast<double>(k) + 0.5) / static_cast<double>(steps);   // midpoint rule: smooth integrand after the change of variable, error ~ 1e-10
                s += std::cos(static_cast<double>(i) * std::acos(2.0 * x - 1.0));
            }
            s /= static_cast<double>(steps);
            const double printed = (i % 2 == 1) ? 0.0 : -1.0 / (static_cast<double>(i * i) - 1.0);
            INFO("T_" << i);
            CHECK(std::fabs(s - printed) <= 1.0e-7);
        }
        // the recurrence inside nielsen_f against cos(i arccos z): residual at a single x_j for n = 1 is T_i(x)/1 - y_i
        for (double x : {0.13, 0.5, 0.77}) {
            std::vector<double> f(6);
            nielsen_f<double>(15, 6, 1, &x, f.data());
            for (std::size_t i = 1; i <= 6; ++i) {
                const double y = (i % 2 == 1) ? 0.0 : -1.0 / (static_cast<double>(i * i) - 1.0);
                INFO("x = " << x << ", i = " << i);
                CHECK(std::fabs(f[i - 1] + y - std::cos(static_cast<double>(i) * std::acos(2.0 * x - 1.0))) <= 1.0e-12);
            }
        }
        // Watson: f_i is y'(t) - y(t)^2 - 1 for y = sum x_j t^(j-1); the Taylor series of tan t solves y' = 1 + y^2, so at its coefficients the residual at t = 1/29 is the truncation error, tiny, and f_30, f_31 vanish
        std::vector<double> tan_series = {0.0, 1.0, 0.0, 1.0 / 3.0, 0.0, 2.0 / 15.0, 0.0, 17.0 / 315.0, 0.0, 62.0 / 2835.0, 0.0, 1382.0 / 155925.0};
        std::vector<double> f(31);
        nielsen_f<double>(11, 31, 12, tan_series.data(), f.data());
        CHECK(std::fabs(f[0]) <= 1.0e-12);
        CHECK(f[29] == 0.0);
        CHECK(f[30] == 0.0);
        // and it is NOT a trivial residual: a perturbed series is far from zero
        tan_series[3] += 0.1;
        nielsen_f<double>(11, 31, 12, tan_series.data(), f.data());
        CHECK(std::fabs(f[0]) > 1.0e-4);
    }

    // (e) the forward-mode Jacobian against central differences, at the Appendix's start and at a displaced point, for every instance
    for (const NielsenCase& c : nielsen_cases()) {
        if (!nielsen_available(c.pno)) continue;
        const std::vector<double> x0 = nielsen_x0(c.pno, c.n);
        std::vector<double> displaced = x0;
        for (std::size_t j = 0; j < displaced.size(); ++j) displaced[j] = x0[j] * 1.03 + 0.02 * (static_cast<double>(j % 3) - 1.0);
        const LmModel model = nielsen_model(c.pno, c.m, c.n);
        for (const std::vector<double>* b : std::initializer_list<const std::vector<double>*>{&x0, &displaced}) {
            const auto e = model(*b, true);
            REQUIRE(e.has_value());
            for (std::size_t j = 0; j < c.n; ++j) {
                const double h = 6.0e-6 * std::max(std::fabs((*b)[j]), 1.0e-3);
                std::vector<double> bp = *b, bm = *b;
                bp[j] += h;
                bm[j] -= h;
                std::vector<double> fp(c.m), fm(c.m);
                nielsen_f<double>(c.pno, c.m, c.n, bp.data(), fp.data());
                nielsen_f<double>(c.pno, c.m, c.n, bm.data(), fm.data());
                double diff2 = 0.0, norm2 = 0.0, fmax = 0.0;
                for (std::size_t i = 0; i < c.m; ++i) {
                    const double fd = -(fp[i] - fm[i]) / (2.0 * h);                // the library's Jacobian is the partials of the model: -df/dx
                    const double an = e->jacobian[i * c.n + j];
                    diff2 += (fd - an) * (fd - an);
                    norm2 += an * an;
                    fmax = std::max(fmax, std::max(std::fabs(fp[i]), std::fabs(fm[i])));
                }
                const double floor = 4.0 * 1.1102230246251565e-16 * fmax * std::sqrt(static_cast<double>(c.m)) / h;
                INFO("Pno " << c.pno << " (" << c.m << ", " << c.n << "), parameter " << j);
                CHECK(std::sqrt(diff2) <= 1.0e-6 * std::sqrt(norm2) + floor);
            }
        }
    }
}

// EST-A-331 -- the HELD-OUT comparison.  Registered in L7_step3_registration.txt (H1) before it was run; NOT GATED: it prints, and the manager rules.
TEST_CASE("EST-A-331: Nielsen's problems, held out: the library at tau 1e-3 and 1, DTU's unscaled algorithm at 1e-3 -- reached or not, and the passes", "[estimation][lm][nielsen][heldout]") {
    std::size_t reach[3] = {0, 0, 0}, total = 0, all_three = 0;
    std::size_t passes[3] = {0, 0, 0};
    for (const NielsenCase& c : nielsen_cases()) {
        if (!nielsen_available(c.pno)) {
            std::printf("NIELSEN Pno %2d (%2zu,%2zu) OMITTED: the Appendix's responses for this problem are not in the document\n", c.pno, c.m, c.n);
            continue;
        }
        ++total;
        const std::vector<double> x0 = nielsen_x0(c.pno, c.n);
        const LmModel model = nielsen_model(c.pno, c.m, c.n);
        const std::vector<std::string> names = pnames(c.n);
        const double f0 = nielsen_F(c.pno, c.m, c.n, x0);
        auto reached = [&](double f) { return std::isfinite(f) && (c.f_printed > 0.0 ? f <= c.f_printed * (1.0 + 1.0e-2) : f <= 1.0e-14 * f0); };
        LmOptions oa;                                 // tau = 1e-3, the default
        LmOptions ob;
        ob.tau = 1.0;
        const LmResult a = levenberg_marquardt(names, x0, model, oa);
        const LmResult b = levenberg_marquardt(names, x0, model, ob);
        const ControlRun cc = dtu_unscaled(names, x0, model, 1.0e-3, 0.0, 1.0e-12, 1000);
        const bool ra = reached(a.objective), rb = reached(b.objective), rc = reached(cc.objective);
        std::printf("NIELSEN Pno %2d (%2zu,%2zu) F*=%.3e | A tau1e-3: %-7s k=%4zu %-18s F=%.3e | B tau1: %-7s k=%4zu %-18s F=%.3e | C unscaled 1e-3: %-7s k=%4zu ended=%-8s F=%.3e\n", c.pno, c.m, c.n,
                    c.f_printed, ra ? "reached" : "NOT", a.iterations, to_string(a.status), a.objective, rb ? "reached" : "NOT", b.iterations, to_string(b.status), b.objective, rc ? "reached" : "NOT",
                    cc.passes, cc.ended.c_str(), cc.objective);
        reach[0] += ra ? 1 : 0;
        reach[1] += rb ? 1 : 0;
        reach[2] += rc ? 1 : 0;
        if (ra && rb && rc) {
            ++all_three;
            passes[0] += a.iterations;
            passes[1] += b.iterations;
            passes[2] += cc.passes;
        }
    }
    std::printf("NIELSEN TOTAL over %zu instances: reached A (library tau 1e-3) %zu, B (library tau 1) %zu, C (DTU unscaled 1e-3) %zu; reached by all three: %zu, passes summed over those: A %zu, B %zu, C %zu\n", total,
                reach[0], reach[1], reach[2], all_three, passes[0], passes[1], passes[2]);
    CHECK(total == 29);
}
