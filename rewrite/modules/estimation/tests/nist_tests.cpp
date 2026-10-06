// nist_tests.cpp — SPEC-estimation.md §8.2 EST-A-206 (layer c: the certified values), EST-A-207 (Filip is refused, and says why) and EST-A-208 (Demmel's
// worked example).  The certified numbers are NIST's (NIST/ITL Statistical Reference Datasets, Standard Reference Database 140,
// https://www.itl.nist.gov/div898/strd/), read from the pinned files; none is copied into the tree.
#include "fixture.hpp"

#include <cmath>
#include <map>

using namespace odl::estimation;
using fx::ld;

TEST_CASE("EST-A-206: layer (c) -- the certified values of ten NIST linear datasets: estimates, residual standard deviation, standard deviations", "[estimation][nist]") {
    // the predicted digits (rule 7, the middle form; NOT criteria): exploratory runs of PROVENANCE section 40.7
    const std::map<std::string, double> predicted = {{"Norris", 11.5}, {"Pontius", 11.5}, {"NoInt1", 14.0}, {"NoInt2", 14.5}, {"Longley", 8.0},
                                                     {"Wampler1", 6.0}, {"Wampler2", 9.0}, {"Wampler3", 6.0}, {"Wampler4", 6.0}, {"Wampler5", 6.0}};
    int cases = 0;
    for (const std::string& nm : fx::served()) {
        ++cases;
        const fx::NistData d = fx::load_nist(nm);
        const registered::NistFigures& f = fx::figures(nm);
        REQUIRE(d.checksum() == f.checksum);
        REQUIRE(d.n == static_cast<std::size_t>(f.n));
        REQUIRE(d.m == static_cast<std::size_t>(f.m));
        auto ne = fx::estimator_of(d);
        const auto formed = ne.formed_system();
        const auto sol = ne.solve();
        REQUIRE((formed.has_value() && sol.has_value()));
        for (std::size_t i = 0; i < d.n; ++i) CHECK(sol->scale_exponents[i] == f.e[i]);
        // (i) the scaled difference from the certified estimates, D = diag(sqrt(N-hat_ii))
        const std::vector<ld> dd = fx::d_of(*formed);
        const ld phi = fx::scaled_difference(dd, sol->x, d.certified);
        CHECK(phi <= static_cast<ld>(f.b_test));
        // (ii) the certified relative error the solve reports equals the registered B (code) to 1e-3
        CHECK(std::fabs(sol->certified_relative_error - f.b_code) <= 1.0e-3 * f.b_code);
        // (iii) sigma_0 against the certified residual standard deviation
        const auto s2 = sol->sigma0_squared();
        REQUIRE(s2.has_value());
        const double sigma0 = std::sqrt(*s2);
        const double rsd = static_cast<double>(d.certified_rsd);
        CHECK(std::fabs(sigma0 - rsd) <= f.b_sigma + 1.0e-14 * rsd);
        // (iv) the standard deviations of the estimates
        double worst_sd = 0.0;
        for (std::size_t i = 0; i < d.n; ++i) {
            const double sd = sigma0 * std::sqrt(sol->cofactor[i * d.n + i]);
            const double sd_cert = static_cast<double>(d.certified_sd[i]);
            const double bound = std::sqrt(f.qdiag[i]) * (f.b_sigma * std::sqrt(1.0 + f.b_q) + f.sigma0_exact * f.b_q) + 1.0e-14 * sd_cert;
            CHECK(std::fabs(sd - sd_cert) <= bound);
            if (bound > 0.0) worst_sd = std::max(worst_sd, std::fabs(sd - sd_cert) / bound);
        }
        // (v) printed: the digits agreed with the certified estimates (NIST's LRE), beside the predicted digits
        double min_lre = 15.0;
        for (std::size_t i = 0; i < d.n; ++i) {
            const ld c = d.certified[i];
            if (c == 0.0L) continue;
            const ld r = std::fabs((static_cast<ld>(sol->x[i]) - c) / c);
            min_lre = std::min(min_lre, r == 0.0L ? 15.0 : static_cast<double>(-std::log10(r)));
        }
        const double want = predicted.at(nm);
        fx::say("EST-A-206: %-9s n = %2zu m = %3zu  phi_cert = %.3Le (bound B(test) = %.3e, ratio %.2Le)  B(code) = %.4e (registered %.4e)  sigma0 = %.9g (RSD cert %.9g, diff %.2e vs bound %.2e)  "
                "worst SD ratio to bound %.2e  digits (min LRE) %.1f  [predicted >= %.1f: %s]",
                nm.c_str(), d.n, d.m, phi, f.b_test, phi / static_cast<ld>(f.b_test), sol->certified_relative_error, f.b_code, sigma0, rsd, std::fabs(sigma0 - rsd), f.b_sigma, worst_sd,
                min_lre, want, min_lre >= want ? "MET" : "MISSED");
    }
    REQUIRE(cases == 10);
    fx::say("EST-A-206: %d datasets", cases);
}

TEST_CASE("EST-A-207: Filip, certified yet beyond normal equations in double, is REFUSED -- EST-F-202 at B9, with the dependency", "[estimation][nist]") {
    const fx::NistData d = fx::load_nist("Filip");
    const registered::NistFigures& f = fx::figures("Filip");
    REQUIRE(d.checksum() == f.checksum);
    auto ne = fx::estimator_of(d);
    const auto sol = ne.solve();
    REQUIRE_FALSE(sol.has_value());
    const EstimationRefusal& r = sol.error();
    CHECK(r.id == std::string_view("EST-F-202"));
    CHECK(r.parameter == "B9");
    CHECK(r.value < 0.0);
    CHECK(r.message.find("B9") != std::string::npos);
    CHECK(r.message.find("the condition number is not available") != std::string::npos);
    // the weakest direction: its last component (B9) positive, zero after it; kappa unavailable
    REQUIRE(r.condition.weakest_direction.size() == d.n);
    CHECK(r.condition.weakest_direction[9] > 0.0);
    CHECK(r.condition.weakest_direction[10] == 0.0);
    CHECK(r.condition.condition_estimate == 0.0);
    CHECK(r.condition.condition_upper == 0.0);
    // the predictions (not criteria): the pivot within a factor 20 of -2.2e-12 and the dependency's three largest members B5, B7, B6
    const bool band = r.value <= -2.2e-12 / 20.0 && r.value >= -2.2e-12 * 20.0;
    std::vector<std::size_t> idx;
    for (std::size_t i = 0; i < 9; ++i) idx.push_back(i);
    std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return std::fabs(r.condition.weakest_direction[a]) > std::fabs(r.condition.weakest_direction[b]);
    });
    const bool order = idx[0] == 5 && idx[1] == 7 && idx[2] == 6;
    fx::say("EST-A-207: refused %.*s at %s, pivot %.4e (registered at the tool's replication: %.4e)  [prediction: pivot in the band %s; dependency B5, B7, B6 %s]",
            static_cast<int>(r.id.size()), r.id.data(), r.parameter.c_str(), r.value, f.breakdown_pivot, band ? "MET" : "MISSED", order ? "MET" : "MISSED");
    fx::say("EST-A-207: %s", r.message.c_str());
    fx::say("EST-A-207: the exact facts beside it: kappa_2(N_s) = %.4e, lambda_min(A) = %.4e against Demmel's eps (n^2 + n) = %.4e (DEM89 Thm 3.2)", f.kappa, f.lam_min_a,
            0x1p-52 * (11.0 * 11.0 + 11.0));
    CHECK(f.breakdown_index == 9);
}

TEST_CASE("EST-A-208: Demmel's worked example (DEM89 p. 4) through the system mode", "[estimation][nist]") {
    const fx::DemmelSystem dm = fx::demmel_system();
    auto ne = NormalEquations::from_normal_system({"x1", "x2", "x3", "x4"}, dm.h, dm.b, 4);
    REQUIRE(ne.has_value());
    const auto sol = ne->solve();
    REQUIRE(sol.has_value());
    const registered::DemmelFigures& g = registered::kDemmel;
    for (std::size_t i = 0; i < 4; ++i) CHECK(sol->scale_exponents[i] == g.e[i]);
    // the scaled error against the printed exact scaled solution p = D x / ||D x||: ||p - D x-hat / ||D x-hat||||_2
    const std::array<ld, 4> dd = fx::demmel_d();
    ld nrm = 0.0L;
    std::array<ld, 4> w{};
    for (std::size_t i = 0; i < 4; ++i) {
        w[i] = dd[i] * static_cast<ld>(sol->x[i]);
        nrm += w[i] * w[i];
    }
    nrm = std::sqrt(nrm);
    ld err2 = 0.0L;
    for (std::size_t i = 0; i < 4; ++i) {
        const ld t = static_cast<ld>(g.p[i]) - w[i] / nrm;
        err2 += t * t;
    }
    const ld err = std::sqrt(err2);
    CHECK(err <= static_cast<ld>(g.b));
    // the diagnostics: kappa_est within [0.5, 1.001] of the exact kappa_2(N_s); kappa_up above it
    CHECK(sol->condition.condition_estimate >= 0.5 * g.kappa_ns);
    CHECK(sol->condition.condition_estimate <= 1.001 * g.kappa_ns);
    CHECK(sol->condition.condition_upper >= 0.999 * g.kappa_ns);
    fx::say("EST-A-208: scaled error %.3Le (registered bound %.3e; the page's printed bound %.1e, MET: %s; the page's computed solution's error was 7.6e-17)", err, g.b, g.published_bound,
            err <= static_cast<ld>(g.published_bound) ? "yes" : "no");
    fx::say("EST-A-208: e = %d %d %d %d (registered %d %d %d %d); kappa_est = %.4f, kappa_up = %.4f, exact kappa_2(N_s) = %.4f; exact kappa_2(A) = %.4f (the page prints ~2.0), kappa_2(H) = %.4e (the page: ~1e50)",
            sol->scale_exponents[0], sol->scale_exponents[1], sol->scale_exponents[2], sol->scale_exponents[3], g.e[0], g.e[1], g.e[2], g.e[3], sol->condition.condition_estimate,
            sol->condition.condition_upper, g.kappa_ns, g.kappa_a, g.kappa_h);
}
