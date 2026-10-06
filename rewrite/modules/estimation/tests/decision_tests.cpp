// decision_tests.cpp — SPEC-estimation.md §8.2 EST-A-209 (unit invariance, bit for bit, and the control that shows it matters), EST-A-212 (the diagnostics
// against exact values) and EST-A-213 (the covariance).
#include "fixture.hpp"

#include <odl/estimation/dense.hpp>

#include <cmath>

using namespace odl::estimation;
using fx::ld;

namespace {

/// The decision rule U of EST-A-209: min pivot / max pivot of the Cholesky factor of the UNSCALED normal matrix, in double.  Returns the ratio, or 0 on a breakdown.
double unscaled_pivot_ratio(const FormedSystem& f) {
    std::vector<double> l = f.matrix;
    const CholeskyResult ch = cholesky_lower(l, f.n);
    if (!ch.ok) return 0.0;
    double lo = std::numeric_limits<double>::infinity(), hi = 0.0;
    for (std::size_t i = 0; i < f.n; ++i) {
        const double p = l[i * f.n + i] * l[i * f.n + i];
        lo = std::min(lo, p);
        hi = std::max(hi, p);
    }
    return lo / hi;
}

bool u_refuses(double ratio, std::size_t n) { return ratio < static_cast<double>(n) * 0x1p-52; }

/// The dataset's rows with column j multiplied by 2^(p_j) (exact), as a NistData.
fx::NistData rescale_pow2(const fx::NistData& d, const std::vector<int>& p) {
    fx::NistData out = d;
    for (std::size_t k = 0; k < d.m; ++k)
        for (std::size_t j = 0; j < d.n; ++j) out.design[k * d.n + j] = std::ldexp(d.design[k * d.n + j], p[j]);
    return out;
}

bool same_bits(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }

bool same_vector(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!same_bits(a[i], b[i]) && !(std::isnan(a[i]) && std::isnan(b[i]))) return false;
    return true;
}

}  // namespace

TEST_CASE("EST-A-209: unit invariance, bit for bit, and the control that shows it matters", "[estimation][decision]") {
    int cases = 0;
    // (a) Pontius and Wampler1, 8 exponent vectors each (the first p = -e, the second 0, six seeded), Filip with 4
    for (const std::string nm : {"Pontius", "Wampler1"}) {
        const fx::NistData d = fx::load_nist(nm);
        REQUIRE(d.checksum() == fx::figures(nm).checksum);
        auto base_ne = fx::estimator_of(d);
        const auto base = base_ne.solve();
        REQUIRE(base.has_value());
        std::vector<std::vector<int>> vectors;
        {
            std::vector<int> minus_e(d.n);
            for (std::size_t j = 0; j < d.n; ++j) minus_e[j] = -base->scale_exponents[j];
            vectors.push_back(minus_e);
            vectors.push_back(std::vector<int>(d.n, 0));
            for (int c = 0; c < 6; ++c) {
                std::mt19937_64 g(31337 + static_cast<std::uint64_t>(c) + (nm == "Pontius" ? 0U : 100U));
                std::vector<int> p(d.n);
                for (auto& v : p) v = fx::uniform_int(g, -35, 35);
                vectors.push_back(p);
            }
        }
        REQUIRE(vectors.size() == 8);
        for (std::size_t c = 0; c < vectors.size(); ++c) {
            ++cases;
            const std::vector<int>& p = vectors[c];
            const fx::NistData r = rescale_pow2(d, p);
            auto ne = fx::estimator_of(r);
            const auto sol = ne.solve();
            REQUIRE(sol.has_value());
            bool exact = true;
            for (std::size_t j = 0; j < d.n; ++j) {
                if (sol->scale_exponents[j] != base->scale_exponents[j] + p[j]) exact = false;
                if (!same_bits(sol->x[j], std::ldexp(base->x[j], -p[j]))) exact = false;
                for (std::size_t i = 0; i < d.n; ++i)
                    if (!same_bits(sol->cofactor[i * d.n + j], std::ldexp(base->cofactor[i * d.n + j], -(p[i] + p[j])))) exact = false;
            }
            if (!same_vector(sol->scaled_matrix, base->scaled_matrix) || !same_vector(sol->scaled_rhs, base->scaled_rhs) ||
                !same_vector(sol->scaled_solution, base->scaled_solution) || !same_vector(sol->scaled_cofactor, base->scaled_cofactor))
                exact = false;
            if (!same_bits(sol->condition.condition_estimate, base->condition.condition_estimate) || !same_bits(sol->condition.condition_upper, base->condition.condition_upper) ||
                !same_vector(sol->condition.parameter_conditions, base->condition.parameter_conditions) || !same_vector(sol->condition.weakest_direction, base->condition.weakest_direction))
                exact = false;
            for (std::size_t i = 0; i < d.n; ++i)
                for (std::size_t j = 0; j < d.n; ++j)
                    if (!same_bits(sol->correlation(i, j), base->correlation(i, j))) exact = false;
            if (!same_bits(sol->certified_relative_error, base->certified_relative_error) || !same_bits(*sol->weighted_rss, *base->weighted_rss)) exact = false;
            CHECK(exact);
            // the control (c), Pontius only: U on p = -e (vector 0) accepts, on p = 0 (vector 1) refuses; the estimator completes in both (REQUIRE above)
            if (nm == "Pontius" && c < 2) {
                const auto formed = ne.formed_system();
                REQUIRE(formed.has_value());
                const double ratio = unscaled_pivot_ratio(*formed);
                fx::say("EST-A-209 (c): Pontius rescaled by p = %s: U's pivot ratio %.4e -> U %s", c == 0 ? "-e" : "0", ratio, u_refuses(ratio, d.n) ? "REFUSES (rank-deficient)" : "accepts");
                if (c == 0) CHECK_FALSE(u_refuses(ratio, d.n));
                else CHECK(u_refuses(ratio, d.n));
            }
        }
    }
    {   // Filip: the same refusal under 4 rescalings
        const fx::NistData d = fx::load_nist("Filip");
        auto base_ne = fx::estimator_of(d);
        const auto base = base_ne.solve();
        REQUIRE_FALSE(base.has_value());
        for (int c = 0; c < 4; ++c) {
            ++cases;
            std::mt19937_64 g(8675309 + static_cast<std::uint64_t>(c));
            std::vector<int> p(d.n);
            for (auto& v : p) v = fx::uniform_int(g, -35, 35);
            const fx::NistData r = rescale_pow2(d, p);
            auto ne = fx::estimator_of(r);
            const auto sol = ne.solve();
            REQUIRE_FALSE(sol.has_value());
            CHECK(sol.error().id == base.error().id);
            CHECK(sol.error().parameter == base.error().parameter);
            CHECK(same_bits(sol.error().value, base.error().value));
            CHECK(same_vector(sol.error().condition.weakest_direction, base.error().condition.weakest_direction));
            CHECK(sol.error().message == base.error().message);
        }
    }
    REQUIRE(cases == 20);
    fx::say("EST-A-209 (a): %d rescalings compared bit for bit", cases);

    // (b) a rescale that is not a power of two: the columns of Pontius by 1e6, 1e-6, 1e3
    {
        const fx::NistData d = fx::load_nist("Pontius");
        const registered::NistFigures& f = fx::figures("Pontius");
        auto ne = fx::estimator_of(d);
        const auto formed = ne.formed_system();
        const auto base = ne.solve();
        REQUIRE((formed.has_value() && base.has_value()));
        const double factor[3] = {1.0e6, 1.0e-6, 1.0e3};
        fx::NistData r = d;
        for (std::size_t k = 0; k < d.m; ++k)
            for (std::size_t j = 0; j < d.n; ++j) r.design[k * d.n + j] = d.design[k * d.n + j] * factor[j];
        auto ne2 = fx::estimator_of(r);
        const auto resc = ne2.solve();
        REQUIRE(resc.has_value());
        std::vector<double> back(d.n);
        for (std::size_t j = 0; j < d.n; ++j) back[j] = resc->x[j] * factor[j];                  // x'_j f_j = x_j
        std::vector<ld> ref(d.n);
        for (std::size_t j = 0; j < d.n; ++j) ref[j] = static_cast<ld>(base->x[j]);
        const std::vector<ld> dd = fx::d_of(*formed);
        const ld diff = fx::scaled_difference(dd, back, ref);
        CHECK(diff <= static_cast<ld>(f.b_pair));
        fx::say("EST-A-209 (b): Pontius rescaled by 1e6, 1e-6, 1e3: the two solutions differ by %.3Le (bound B_pair = %.3e)", diff, f.b_pair);
    }

    // (c) the control's two registered figures: U on Pontius and on the same rows with every design column divided by its own norm
    {
        const fx::NistData d = fx::load_nist("Pontius");
        auto ne = fx::estimator_of(d);
        const auto formed = ne.formed_system();
        REQUIRE(formed.has_value());
        const double ratio = unscaled_pivot_ratio(*formed);
        CHECK(std::fabs(ratio - registered::kRuleUPontius) <= 1.0e-9 * registered::kRuleUPontius);
        CHECK(u_refuses(ratio, d.n));
        fx::NistData q = d;
        std::vector<double> norms(d.n, 0.0);
        for (std::size_t j = 0; j < d.n; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < d.m; ++k) s += static_cast<double>(static_cast<ld>(d.design[k * d.n + j]) * static_cast<ld>(d.design[k * d.n + j]));
            norms[j] = std::sqrt(s);
        }
        for (std::size_t k = 0; k < d.m; ++k)
            for (std::size_t j = 0; j < d.n; ++j) q.design[k * d.n + j] = d.design[k * d.n + j] / norms[j];
        auto neq = fx::estimator_of(q);
        const auto formed_q = neq.formed_system();
        const auto sol_q = neq.solve();
        REQUIRE((formed_q.has_value() && sol_q.has_value()));
        const double ratio_q = unscaled_pivot_ratio(*formed_q);
        CHECK(std::fabs(ratio_q - registered::kRuleUPontiusEquilibrated) <= 1.0e-9 * registered::kRuleUPontiusEquilibrated);
        CHECK_FALSE(u_refuses(ratio_q, d.n));
        fx::say("EST-A-209 (c): U's pivot ratio: Pontius %.4e (registered %.4e: REFUSES), equilibrated %.4e (registered %.4e: accepts); the estimator completes on both; ratio of the two %.2e",
                ratio, registered::kRuleUPontius, ratio_q, registered::kRuleUPontiusEquilibrated, ratio_q / ratio);
    }
}

TEST_CASE("EST-A-212: the diagnostics against exact values -- kappa, the per-parameter condition numbers, the weakest direction", "[estimation][decision]") {
    int cases = 0;
    for (const std::string nm : {"Pontius", "Longley", "Wampler1"}) {
        ++cases;
        const fx::NistData d = fx::load_nist(nm);
        const registered::NistFigures& f = fx::figures(nm);
        REQUIRE(d.checksum() == f.checksum);
        auto ne = fx::estimator_of(d);
        const auto sol = ne.solve();
        REQUIRE(sol.has_value());
        const ConditionReport& c = sol->condition;
        CHECK(c.condition_estimate >= 0.5 * f.kappa);
        CHECK(c.condition_estimate <= 1.001 * f.kappa);
        CHECK(c.condition_upper >= 0.999 * f.kappa);
        for (std::size_t i = 0; i < d.n; ++i) CHECK(std::fabs(c.parameter_conditions[i] / f.kappa_i[i] - 1.0) <= f.b_q);
        double cosine = 0.0;
        for (std::size_t i = 0; i < d.n; ++i) cosine += c.weakest_direction[i] * f.weakest_exact[i];
        CHECK(std::fabs(cosine) >= f.cos_min);
        fx::say("EST-A-212: %-9s kappa_est/kappa = %.6f  kappa_up/kappa = %.3f  worst kappa_i relative error %.2e (bound B_Q %.2e)  |cos| of the weakest direction = %.12f (registered >= %.12f)",
                nm.c_str(), c.condition_estimate / f.kappa, c.condition_upper / f.kappa,
                [&] { double w = 0; for (std::size_t i = 0; i < d.n; ++i) w = std::max(w, std::fabs(c.parameter_conditions[i] / f.kappa_i[i] - 1.0)); return w; }(), f.b_q, std::fabs(cosine), f.cos_min);
        if (nm == "Pontius") {      // GEO1's 'condition numbers' N_ii (N^-1)_ii: 11.07, 76.41, 41.16
            CHECK(std::fabs(c.parameter_conditions[0] - 11.07) < 0.01);
            CHECK(std::fabs(c.parameter_conditions[1] - 76.41) < 0.01);
            CHECK(std::fabs(c.parameter_conditions[2] - 41.16) < 0.01);
        }
        if (nm == "Longley") {      // the weakest direction is the intercept B0 against the year B6
            CHECK(std::fabs(c.weakest_direction[0] + 0.690) < 0.001);
            CHECK(std::fabs(c.weakest_direction[6] - 0.723) < 0.001);
        }
    }
    {   // Demmel's example: kappa only
        ++cases;
        const fx::DemmelSystem dm = fx::demmel_system();
        auto ne = NormalEquations::from_normal_system({"x1", "x2", "x3", "x4"}, dm.h, dm.b, 4);
        REQUIRE(ne.has_value());
        const auto sol = ne->solve();
        REQUIRE(sol.has_value());
        const registered::DemmelFigures& g = registered::kDemmel;
        CHECK(sol->condition.condition_estimate >= 0.5 * g.kappa_ns);
        CHECK(sol->condition.condition_estimate <= 1.001 * g.kappa_ns);
        CHECK(sol->condition.condition_upper >= 0.999 * g.kappa_ns);
        fx::say("EST-A-212: Demmel    kappa_est/kappa = %.6f  kappa_up/kappa = %.3f", sol->condition.condition_estimate / g.kappa_ns, sol->condition.condition_upper / g.kappa_ns);
    }
    REQUIRE(cases == 4);
}

TEST_CASE("EST-A-213: the covariance -- the cofactor and the correlations against exact values, the formal and the scaled", "[estimation][decision]") {
    int cases = 0;
    for (const std::string& nm : fx::served()) {
        ++cases;
        const fx::NistData d = fx::load_nist(nm);
        const registered::NistFigures& f = fx::figures(nm);
        REQUIRE(d.checksum() == f.checksum);
        auto ne = fx::estimator_of(d);
        const auto sol = ne.solve();
        REQUIRE(sol.has_value());
        const std::size_t n = d.n;
        double worst_q = 0.0, worst_rho = 0.0;
        bool symmetric = true, unit = true;
        std::size_t at = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const double qi = sol->cofactor[i * n + i];
            worst_q = std::max(worst_q, std::fabs(qi / f.qdiag[i] - 1.0));
            if (sol->correlation(i, i) != 1.0) unit = false;
            for (std::size_t j = i + 1; j < n; ++j) {
                if (sol->cofactor[i * n + j] != sol->cofactor[j * n + i]) symmetric = false;
                const double rho = sol->correlation(i, j);
                if (std::fabs(rho) > 1.0) unit = false;
                if (sol->correlation(j, i) != rho) symmetric = false;
                worst_rho = std::max(worst_rho, std::fabs(rho - f.rho_exact[at]));
                ++at;
            }
        }
        CHECK(worst_q <= f.b_q);
        CHECK(worst_rho <= f.b_rho);
        CHECK(symmetric);
        CHECK(unit);
        // the formal covariance is the cofactor; the scaled covariance is sigma_0^2 times it, elementwise and bitwise
        CHECK(same_vector(sol->formal_covariance(), sol->cofactor));
        const auto sc = sol->scaled_covariance();
        const auto s2 = sol->sigma0_squared();
        REQUIRE((sc.has_value() && s2.has_value()));
        bool product = true;
        for (std::size_t i = 0; i < sc->size(); ++i)
            if (!same_bits((*sc)[i], *s2 * sol->cofactor[i])) product = false;
        CHECK(product);
        CHECK(sol->degrees_of_freedom == static_cast<std::ptrdiff_t>(d.m) - static_cast<std::ptrdiff_t>(n));
        if (nm == "Pontius") {   // BRN52's standard deviation of a difference as an identity: sqrt(Q00 - 2 Q01 + Q11) = sqrt(c' Q c), c = (1, -1, 0)
            const ld direct = static_cast<ld>(sol->cofactor[0]) - 2.0L * static_cast<ld>(sol->cofactor[1]) + static_cast<ld>(sol->cofactor[4]);
            ld cq = 0.0L;
            const ld c[3] = {1.0L, -1.0L, 0.0L};
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) cq += c[i] * static_cast<ld>(sol->cofactor[i * 3 + j]) * c[j];
            CHECK(std::fabs(direct - cq) <= 4.0L * 0x1p-52L * std::max(std::fabs(direct), std::fabs(cq)) + 4.0L * 0x1p-52L * static_cast<ld>(sol->cofactor[0]));
        }
        fx::say("EST-A-213: %-9s worst |Q-hat_ii / Q_ii - 1| = %.2e (bound B_Q %.2e)  worst |rho-hat - rho| = %.2e (bound B_rho %.2e)", nm.c_str(), worst_q, f.b_q, worst_rho, f.b_rho);
    }
    REQUIRE(cases == 10);
}
