// kernel_tests.cpp — SPEC-estimation.md §8.2 EST-A-203 (the kernels against closed forms), EST-A-204 (layer a: the forming) and EST-A-205 (layer b: the solve).
#include "fixture.hpp"

#include <odl/estimation/dense.hpp>

#include <cmath>

using namespace odl::estimation;
using fx::ld;

namespace {

/// The scaled difference ||D (a - b)||_2 / ||D a||_2 (normalised by the ESTIMATE, as EST-A-203 states it), in long double.
ld scaled_diff_by_estimate(const std::vector<ld>& d, std::span<const double> a, std::span<const ld> b) {
    ld num = 0.0L, den = 0.0L;
    for (std::size_t i = 0; i < d.size(); ++i) {
        const ld t = d[i] * (static_cast<ld>(a[i]) - b[i]);
        num += t * t;
        const ld u = d[i] * static_cast<ld>(a[i]);
        den += u * u;
    }
    return std::sqrt(num) / std::sqrt(den);
}

const registered::TridiagonalFigures& tri_figures(int n) {
    for (const auto& t : registered::kTridiagonal)
        if (t.n == n) return t;
    throw std::runtime_error("no registered tridiagonal figures for n = " + std::to_string(n));
}

}  // namespace

TEST_CASE("EST-A-203: the kernels against closed forms -- tridiag(2, -1), six sizes", "[estimation][kernel]") {
    int cases = 0;
    for (const int n : {1, 2, 5, 10, 20, 40}) {
        ++cases;
        const std::size_t nn = static_cast<std::size_t>(n);
        const fx::Rows rows = fx::tridiagonal_rows(nn);
        auto ne = fx::estimator_of_rows(rows, fx::names_of(nn));
        const auto formed = ne.formed_system();
        REQUIRE(formed.has_value());
        // (a) the forming is tridiag(2, -1) exactly
        bool exact = true;
        for (std::size_t i = 0; i < nn; ++i)
            for (std::size_t j = 0; j < nn; ++j) {
                const double want = i == j ? 2.0 : (i + 1 == j || j + 1 == i) ? -1.0 : 0.0;
                if (formed->matrix[i * nn + j] != want) exact = false;
            }
        CHECK(exact);
        const auto sol = ne.solve();
        REQUIRE(sol.has_value());
        const std::vector<ld> d = fx::d_of(*formed);
        const registered::TridiagonalFigures& reg = tri_figures(n);
        // (b) the solution against the exact (1, 2, ..., n)
        std::vector<ld> xstar(nn);
        for (std::size_t i = 0; i < nn; ++i) xstar[i] = static_cast<ld>(i + 1);
        const ld phi = scaled_diff_by_estimate(d, sol->x, xstar);
        CHECK(phi <= static_cast<ld>(reg.b_col));
        // (c) each column of the cofactor against Q_ij = min(i,j) (n+1-max(i,j)) / (n+1) (1-based), in long double
        ld worst_col = 0.0L;
        for (std::size_t j = 0; j < nn; ++j) {
            std::vector<double> qhat(nn);
            std::vector<ld> q(nn);
            for (std::size_t i = 0; i < nn; ++i) {
                qhat[i] = sol->cofactor[i * nn + j];
                const ld lo = static_cast<ld>(std::min(i, j) + 1), hi = static_cast<ld>(std::max(i, j) + 1);
                q[i] = lo * (static_cast<ld>(n) + 1.0L - hi) / (static_cast<ld>(n) + 1.0L);
            }
            const ld c = scaled_diff_by_estimate(d, qhat, q);
            worst_col = std::max(worst_col, c);
            CHECK(c <= static_cast<ld>(reg.b_col));
        }
        // (d) the kernel on the UNSCALED matrix: |(L L^T - N)_ij| <= (n+1) u chi_n sqrt(N_ii N_jj) = (n+1) u chi_n 2  (DEM89 Lemma 2.1), in long double
        std::vector<double> l(nn * nn, 0.0);
        for (std::size_t i = 0; i < nn; ++i) {
            l[i * nn + i] = 2.0;
            if (i > 0) l[i * nn + i - 1] = -1.0;
        }
        const CholeskyResult ch = cholesky_lower(l, nn);
        REQUIRE(ch.ok);
        ld worst_back = 0.0L, worst_factor = 0.0L;
        for (std::size_t i = 0; i < nn; ++i)
            for (std::size_t j = 0; j <= i; ++j) {
                ld s = 0.0L;
                for (std::size_t k = 0; k <= j; ++k) s += static_cast<ld>(l[i * nn + k]) * static_cast<ld>(l[j * nn + k]);
                const ld want = i == j ? 2.0L : (i == j + 1) ? -1.0L : 0.0L;
                worst_back = std::max(worst_back, std::fabs(s - want));
            }
        for (std::size_t i = 0; i < nn; ++i) {            // the closed-form factor (informational): L_ii = sqrt((i+1)/i), L_{i+1,i} = -sqrt(i/(i+1)), 1-based
            const ld lii = std::sqrt(static_cast<ld>(i + 2) / static_cast<ld>(i + 1));
            worst_factor = std::max(worst_factor, std::fabs((static_cast<ld>(l[i * nn + i]) - lii) / lii));
            if (i > 0) {
                const ld lsub = -std::sqrt(static_cast<ld>(i) / static_cast<ld>(i + 1));
                worst_factor = std::max(worst_factor, std::fabs((static_cast<ld>(l[i * nn + i - 1]) - lsub) / lsub));
            }
        }
        const ld back_bound = static_cast<ld>(nn + 1) * fx::kU * fx::chi_n(nn) * 2.0L;
        CHECK(worst_back <= back_bound);
        fx::say("EST-A-203: n = %2d  phi(x) = %.3Le  worst column %.3Le  (bound B_col = %.3e)  Lemma-2.1 backward %.3Le (bound %.3Le)  factor vs closed form (informational) %.3Le",
                n, phi, worst_col, reg.b_col, worst_back, back_bound, worst_factor);
    }
    REQUIRE(cases == 6);
}

TEST_CASE("EST-A-204: layer (a) -- the forming against a long double accumulation of the same doubles, and its control", "[estimation][kernel]") {
    int cases = 0;
    auto check = [&](const std::string& label, std::span<const double> a, std::span<const double> r, std::size_t n, bool drop_last_row) {
        auto ne = NormalEquations::create(fx::names_of(n));
        REQUIRE(ne.has_value());
        for (std::size_t k = 0; k < r.size(); ++k) REQUIRE(ne->add(std::span<const double>(&a[k * n], n), r[k], 1.0).has_value());
        const auto formed = ne->formed_system();
        REQUIRE(formed.has_value());
        const std::size_t m = r.size();
        const std::size_t mc = drop_last_row ? m - 1 : m;                           // the control: the comparator without the last row
        const fx::LdSystem ld_sys = fx::form_long_double(a.subspan(0, mc * n), r.subspan(0, mc), n);
        const fx::ld bound = fx::gamma_k(m) * (1.0L + fx::gamma_k(m));
        fx::ld worst = 0.0L, worst_b = 0.0L, rnorm = 0.0L;
        for (std::size_t k = 0; k < m; ++k) rnorm += static_cast<fx::ld>(r[k]) * static_cast<fx::ld>(r[k]);
        rnorm = std::sqrt(rnorm);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j <= i; ++j) {
                const fx::ld diff = std::fabs(static_cast<fx::ld>(formed->matrix[i * n + j]) - ld_sys.N[i * n + j]);
                worst = std::max(worst, diff / std::sqrt(ld_sys.N[i * n + i] * ld_sys.N[j * n + j]));
            }
            const fx::ld db = std::fabs(static_cast<fx::ld>(formed->rhs[i]) - ld_sys.b[i]);
            worst_b = std::max(worst_b, db / (fx::gamma_k(m) * std::sqrt(ld_sys.N[i * n + i]) * rnorm));
        }
        if (!drop_last_row) {
            CHECK(worst <= bound);
            CHECK(worst_b <= 1.0L);                                                // |b-hat - b'| <= gamma_m sqrt(N_ii) ||r||
            fx::say("EST-A-204: %-10s m = %2zu  worst N error / sqrt(NN) = %.3Le of the bound %.3Le (ratio %.3Lf)   worst b error / bound = %.3Lf", label.c_str(), m, worst, bound,
                    worst / bound, worst_b);
        }
        return worst / bound;
    };
    for (const char* nm : {"Norris", "Pontius", "Longley", "Wampler5", "Filip"}) {
        const fx::NistData d = fx::load_nist(nm);
        REQUIRE(d.checksum() == fx::figures(nm).checksum);
        check(nm, d.design, d.response, d.n, false);
        ++cases;
        if (std::string(nm) == "Pontius") {      // the control: the comparator's last row dropped exceeds the bound by >= 1e3
            const fx::ld ratio = check("Pontius*", d.design, d.response, d.n, true);
            fx::say("EST-A-204 control: the comparator without the last row differs by %.3Le times the bound", ratio);
            CHECK(ratio >= 1.0e3L);
        }
    }
    for (int c = 0; c < 20; ++c) {
        const std::size_t n = 2 + static_cast<std::size_t>(c % 5);
        const std::size_t m = 10 + static_cast<std::size_t>((c * 3) % 21);
        const fx::Rows rows = fx::random_double_rows(4242 + static_cast<std::uint64_t>(c), n, m);
        check("random" + std::to_string(c), rows.a, rows.r, rows.n, false);
        ++cases;
    }
    REQUIRE(cases == 25);
    fx::say("EST-A-204: %d cases", cases);
}

TEST_CASE("EST-A-205: layer (b) -- the scaled backward error with a long double residual, 38 cases, and its control", "[estimation][kernel]") {
    int cases = 0, control_cases = 0;
    fx::ld worst_over_u = 0.0L;
    auto judge = [&](const std::string& label, const FormedSystem& f, const std::vector<double>& x, std::size_t n_free, bool control_applies) {
        ++cases;
        const fx::ld omega = fx::backward_error(f, x);
        const fx::ld bound = fx::theta_s(n_free);
        CHECK(omega <= bound);
        worst_over_u = std::max(worst_over_u, omega / fx::kU);
        if (control_applies && n_free >= 2) {          // the control: the component of largest |D x| perturbed by 1e-8 relative fails by >= 1e3 Theta_s
            const std::vector<fx::ld> dd = fx::d_of(f);
            std::size_t big = 0;
            for (std::size_t i = 1; i < n_free; ++i)
                if (dd[i] * std::fabs(static_cast<fx::ld>(x[i])) > dd[big] * std::fabs(static_cast<fx::ld>(x[big]))) big = i;
            std::vector<double> bad = x;
            bad[big] *= 1.0 + 1.0e-8;
            const fx::ld ratio = fx::backward_error(f, bad) / bound;
            CHECK(ratio >= 1.0e3L);
            ++control_cases;
        }
        fx::say("EST-A-205: %-12s n = %2zu  omega = %.3Le  = %.3Lf u  (bound Theta_s = %.3Le = %.1Lf u)", label.c_str(), n_free, omega, omega / fx::kU, bound, bound / fx::kU);
    };
    for (const std::string& nm : fx::served()) {
        const fx::NistData d = fx::load_nist(nm);
        auto ne = fx::estimator_of(d);
        const auto formed = ne.formed_system();
        const auto sol = ne.solve();
        REQUIRE((formed.has_value() && sol.has_value()));
        judge(nm, *formed, sol->x, d.n, true);
    }
    for (const int n : {1, 2, 5, 10, 20, 40}) {
        const std::size_t nn = static_cast<std::size_t>(n);
        auto ne = fx::estimator_of_rows(fx::tridiagonal_rows(nn), fx::names_of(nn));
        const auto formed = ne.formed_system();
        const auto sol = ne.solve();
        REQUIRE((formed.has_value() && sol.has_value()));
        judge("tridiag" + std::to_string(n), *formed, sol->x, nn, true);
    }
    {   // Demmel's example (EST-A-208), through the system mode
        const fx::DemmelSystem dm = fx::demmel_system();
        auto ne = NormalEquations::from_normal_system({"x1", "x2", "x3", "x4"}, dm.h, dm.b, 4);
        REQUIRE(ne.has_value());
        const auto formed = ne->formed_system();
        const auto sol = ne->solve();
        REQUIRE((formed.has_value() && sol.has_value()));
        judge("Demmel", *formed, sol->x, 4, true);
    }
    {   // the hatch run (EST-A-210): the reduced system formed here independently of the estimator, the solution the estimator's
        const fx::NistData d = fx::load_nist("Filip");
        auto ne = fx::estimator_of(d);
        const SolveOptions opt = fx::filip_hatch_options(d);
        const auto sol = ne.solve(opt);
        REQUIRE(sol.has_value());
        const FormedSystem reduced = fx::reduced_system(d, opt);
        std::vector<double> xf;
        for (const std::size_t i : sol->free_index) xf.push_back(sol->x[i]);
        judge("Filip/hatch", reduced, xf, sol->free_index.size(), true);
    }
    for (int c = 0; c < 20; ++c) {
        const std::size_t n = 2 + static_cast<std::size_t>(c % 8);
        std::mt19937_64 g(991 + static_cast<std::uint64_t>(c));
        std::vector<int> p(n);
        for (auto& v : p) v = fx::uniform_int(g, -30, 30);
        const fx::Rows rows = fx::well_posed_rows(555 + static_cast<std::uint64_t>(c), n, p);
        auto ne = fx::estimator_of_rows(rows, fx::names_of(n));
        const auto formed = ne.formed_system();
        const auto sol = ne.solve();
        REQUIRE((formed.has_value() && sol.has_value()));
        judge("random" + std::to_string(c), *formed, sol->x, n, true);
    }
    REQUIRE(cases == 38);
    fx::say("EST-A-205: %d cases; the largest scaled backward error is %.3Lf u (prediction: <= 4 u); the control ran %d times", cases, worst_over_u, control_cases);
}
