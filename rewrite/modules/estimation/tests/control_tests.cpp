// control_tests.cpp — SPEC-estimation.md §8.2 EST-A-214 (the controls: every layer can fail) and EST-A-215 (system mode, provenance, determinism).
// The controls of layers (a) and (b) are inside EST-A-204 and EST-A-205 (kernel_tests.cpp); here are the control of layer (c), the unscaling control,
// the refusals' control, and the properties of the system mode and of the provenance.
#include "fixture.hpp"

#include <odl/estimation/dense.hpp>

#include <cmath>
#include <limits>
#include <map>

using namespace odl::estimation;
using fx::ld;

namespace {

bool same_bits(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }

bool same_vec(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!same_bits(a[i], b[i])) return false;
    return true;
}

/// Every numeric field, the exponents, the provenance and the optional residual square sum, bit for bit.
bool same_solution(const Solution& a, const Solution& b) {
    bool same = same_vec(a.x, b.x) && same_vec(a.scaled_matrix, b.scaled_matrix) && same_vec(a.scaled_rhs, b.scaled_rhs) && same_vec(a.scaled_solution, b.scaled_solution) &&
                same_vec(a.scaled_cofactor, b.scaled_cofactor) && same_vec(a.cofactor, b.cofactor) && same_vec(a.condition.parameter_conditions, b.condition.parameter_conditions) &&
                same_vec(a.condition.weakest_direction, b.condition.weakest_direction) && same_bits(a.condition.condition_estimate, b.condition.condition_estimate) &&
                same_bits(a.condition.condition_upper, b.condition.condition_upper) && same_bits(a.certified_relative_error, b.certified_relative_error);
    same = same && a.scale_exponents == b.scale_exponents && a.free_index == b.free_index && a.eliminated == b.eliminated && a.provenance == b.provenance &&
           a.observations == b.observations && a.free_parameters == b.free_parameters && a.degrees_of_freedom == b.degrees_of_freedom &&
           a.weighted_rss.has_value() == b.weighted_rss.has_value();
    if (a.weighted_rss && b.weighted_rss) same = same && same_bits(*a.weighted_rss, *b.weighted_rss);
    return same;
}

}  // namespace

TEST_CASE("EST-A-214: the controls -- layer (c) sees a gross defect, the unscaling is checked, the refusals are not bypassable", "[estimation][control]") {
    // The predicted ratios phi_cert / B(test) of the injected defect (the response entry of largest |y| times 10), computed before any group ran by the exact
    // arithmetic of the sizing tool (PROVENANCE 40.8's evidence: control_power_check): printed beside the measured ones, NOT criteria.
    const std::map<std::string, double> predicted = {{"Norris", 2.629e12},   {"Pontius", 4.437e11},   {"NoInt1", 3.174e13},   {"NoInt2", 2.117e14},  {"Longley", 3.442e6},
                                                     {"Wampler1", 9.133e9},   {"Wampler2", 1.260e10}, {"Wampler3", 9.135e9},  {"Wampler4", 9.311e9}, {"Wampler5", 2.164e9}};
    int cases = 0;
    for (const std::string& nm : fx::served()) {
        ++cases;
        fx::NistData d = fx::load_nist(nm);
        const registered::NistFigures& f = fx::figures(nm);
        REQUIRE(d.checksum() == f.checksum);
        std::size_t kstar = 0;
        for (std::size_t k = 1; k < d.m; ++k)
            if (std::fabs(d.response[k]) > std::fabs(d.response[kstar])) kstar = k;
        d.response[kstar] *= 10.0;                                           // the injected defect
        auto ne = fx::estimator_of(d);
        const auto formed = ne.formed_system();
        const auto sol = ne.solve();
        REQUIRE((formed.has_value() && sol.has_value()));                    // the defect leaves the design alone: the solve completes
        const std::vector<ld> dd = fx::d_of(*formed);
        const ld phi = fx::scaled_difference(dd, sol->x, d.certified);
        const ld ratio = phi / static_cast<ld>(f.b_test);
        CHECK(ratio >= 1.0e3L);
        fx::say("EST-A-214 (c): %-9s the entry %2zu times 10: phi_cert = %.3Le against B(test) = %.3e: ratio %.3Le [predicted %.3e; criterion >= 1e3]", nm.c_str(), kstar, phi, f.b_test, ratio,
                predicted.at(nm));
    }
    REQUIRE(cases == 10);

    // (d) the unscaling forgotten: y (the scaled solution) in place of x at Pontius, e = 3, 23, 45
    {
        const fx::NistData d = fx::load_nist("Pontius");
        const registered::NistFigures& f = fx::figures("Pontius");
        REQUIRE(d.checksum() == f.checksum);
        auto ne = fx::estimator_of(d);
        const auto formed = ne.formed_system();
        const auto sol = ne.solve();
        REQUIRE((formed.has_value() && sol.has_value()));
        REQUIRE(sol->scale_exponents.size() == 3);
        CHECK(sol->scale_exponents[0] == 3);
        CHECK(sol->scale_exponents[1] == 23);
        CHECK(sol->scale_exponents[2] == 45);
        const std::vector<ld> dd = fx::d_of(*formed);
        const ld good = fx::scaled_difference(dd, sol->x, d.certified);
        const ld bad = fx::scaled_difference(dd, sol->scaled_solution, d.certified);
        const ld ratio = bad / static_cast<ld>(f.b_test);
        CHECK(good <= static_cast<ld>(f.b_test));
        CHECK(ratio > 1.0e3L);
        fx::say("EST-A-214 (d): Pontius, e = %d %d %d: phi_cert(x) = %.3Le (<= B(test) %.3e), phi_cert(y, the unscaling forgotten) = %.3Le: ratio to B(test) %.3Le [predicted 2.388e22; criterion > 1e3]",
                sol->scale_exponents[0], sol->scale_exponents[1], sol->scale_exponents[2], good, f.b_test, bad, ratio);
    }

    // (e) the refusals are not bypassable, the hatch is the only way through and it is logged, and the refusals are not blanket
    const std::vector<fx::Synthetic> syn = fx::refusal_problems();
    REQUIRE(syn.size() == 6);
    int problems = 0, still_refused = 0, through_the_hatch = 0, neighbours = 0;
    for (const fx::Synthetic& p : syn) {
        ++problems;
        const NormalEquations ne = fx::estimator_of_rows(p.rows, p.names);
        // (i) a refusal -- a Result error, never a value -- and the same one the second time (nothing was learned, nothing was changed)
        const auto s1 = ne.solve();
        const auto s2 = ne.solve();
        REQUIRE_FALSE(s1.has_value());
        REQUIRE_FALSE(s2.has_value());
        CHECK(s1.error().id == std::string_view(p.id));
        CHECK(s1.error().parameter == p.parameter);
        CHECK(s2.error().id == s1.error().id);
        CHECK(s2.error().parameter == s1.error().parameter);
        CHECK(s2.error().message == s1.error().message);
        CHECK(same_bits(s2.error().value, s1.error().value));
        // (ii) an elimination that does not touch the cause is the same refusal
        if (!p.unrelated.empty()) {
            SolveOptions o;
            o.eliminate.push_back({p.unrelated, 0.0});
            const auto s = ne.solve(o);
            REQUIRE_FALSE(s.has_value());
            CHECK(s.error().id == s1.error().id);
            CHECK(s.error().parameter == s1.error().parameter);
            ++still_refused;
        }
        // (iii) the elimination of the cause is the one way through, and the provenance says so
        if (!p.cause.empty()) {
            SolveOptions o;
            o.eliminate.push_back({p.cause, 0.0});
            const auto s = ne.solve(o);
            REQUIRE(s.has_value());
            CHECK(s->provenance.find("eliminated=" + p.cause + "=0;") != std::string::npos);
            ++through_the_hatch;
        }
        // (iv) the neighbour (the cause removed) completes: the refusals are conditions, not blanket
        {
            const NormalEquations nb = fx::estimator_of_rows(p.neighbour, p.names);
            const auto s = nb.solve();
            REQUIRE(s.has_value());
            CHECK(s->certified_relative_error < 1.0e-3);
            fx::say("EST-A-214 (e): %-16s refused %.*s twice, identically; the neighbour completes with B = %.3e", p.label.c_str(), static_cast<int>(s1.error().id.size()), s1.error().id.data(),
                    s->certified_relative_error);
            ++neighbours;
        }
    }
    CHECK(problems == 6);
    CHECK(still_refused == 4);
    CHECK(through_the_hatch == 4);
    CHECK(neighbours == 6);
    fx::say("EST-A-214 (e): %d problems, %d refused again under an unrelated elimination, %d completed through the logged hatch, %d neighbours completed", problems, still_refused, through_the_hatch,
            neighbours);
}

TEST_CASE("EST-A-215: system mode, provenance, determinism", "[estimation][control]") {
    // (a) from_normal_system given the N-hat, b-hat of Norris's rows
    const fx::NistData d = fx::load_nist("Norris");
    const registered::NistFigures& f = fx::figures("Norris");
    REQUIRE(d.checksum() == f.checksum);
    auto ne = fx::estimator_of(d);
    const auto formed = ne.formed_system();
    const auto rows_sol = ne.solve();
    REQUIRE((formed.has_value() && rows_sol.has_value()));
    const std::size_t n = d.n;
    {
        auto sys = NormalEquations::from_normal_system(d.parameter_names, formed->matrix, formed->rhs, d.m);
        REQUIRE(sys.has_value());
        const auto sol = sys->solve();
        REQUIRE(sol.has_value());
        CHECK(same_vec(sol->x, rows_sol->x));
        CHECK(same_vec(sol->scaled_solution, rows_sol->scaled_solution));
        CHECK(same_vec(sol->scaled_cofactor, rows_sol->scaled_cofactor));
        CHECK(same_vec(sol->scaled_matrix, rows_sol->scaled_matrix));
        CHECK(sol->scale_exponents == rows_sol->scale_exponents);
        for (std::size_t i = 0; i < n; ++i) CHECK(sol->scale_exponents[i] == f.e[i]);
        // B without the forming terms: 4 q1 Theta_s(n)
        const double q1 = norm_one(sol->scaled_cofactor, n);
        const ld want = 4.0L * static_cast<ld>(q1) * fx::theta_s(n);
        CHECK(std::fabs(static_cast<ld>(sol->certified_relative_error) - want) <= 1.0e-12L * want);
        CHECK(sol->certified_relative_error < rows_sol->certified_relative_error);
        // no residual square sum: Omega is empty and the scaled covariance is refused
        CHECK_FALSE(sol->weighted_rss.has_value());
        const auto sc = sol->scaled_covariance();
        REQUIRE_FALSE(sc.has_value());
        CHECK(sc.error().id == std::string_view("EST-F-205"));
        // with the residual square sum supplied: Omega = sum r^2 - b'x within 1e-6 relative of the rows'
        ld rss_ld = 0.0L;
        for (const double y : d.response) rss_ld += static_cast<ld>(y) * static_cast<ld>(y);
        const double rss = static_cast<double>(rss_ld);
        auto sys2 = NormalEquations::from_normal_system(d.parameter_names, formed->matrix, formed->rhs, d.m, rss);
        REQUIRE(sys2.has_value());
        const auto sol2 = sys2->solve();
        REQUIRE(sol2.has_value());
        REQUIRE(sol2->weighted_rss.has_value());
        REQUIRE(rows_sol->weighted_rss.has_value());
        const double rel = std::fabs(*sol2->weighted_rss - *rows_sol->weighted_rss) / *rows_sol->weighted_rss;
        CHECK(rel <= 1.0e-6);
        CHECK(sol2->scaled_covariance().has_value());
        fx::say("EST-A-215 (a): system mode at Norris is bit-identical to the rows' in x, y, Q_s and e; B = %.4e (rows %.4e; 4 q1 Theta_s = %.4Le); Omega: rows %.9g, system %.9g, relative difference %.2e (the cancellation "
                "of sum r^2 - b'x is %.1f orders: ||r||^2 = %.3e) [criterion <= 1e-6; predicted near 1e-10]",
                sol->certified_relative_error, rows_sol->certified_relative_error, want, *rows_sol->weighted_rss, *sol2->weighted_rss, rel, std::log10(static_cast<double>(rss_ld) / *rows_sol->weighted_rss),
                static_cast<double>(rss_ld));
    }

    // (b) the provenance of Norris's solve and of EST-A-210's equal EST-R-212's literal with its values, built here from the definition
    {
        const std::string want = fx::provenance_literal(2, 2, 36, {}, std::vector<int>(f.e, f.e + 2));
        CHECK(rows_sol->provenance == want);
        fx::say("EST-A-215 (b): %s", rows_sol->provenance.c_str());
        const fx::NistData fd = fx::load_nist("Filip");
        REQUIRE(fd.checksum() == fx::figures("Filip").checksum);
        auto fne = fx::estimator_of(fd);
        const SolveOptions opt = fx::filip_hatch_options(fd);
        const auto fsol = fne.solve(opt);
        REQUIRE(fsol.has_value());
        std::vector<std::pair<std::string, double>> elim;
        for (std::size_t j = 6; j <= 10; ++j) elim.emplace_back(fd.parameter_names[j], opt.eliminate[j - 6].value);
        const std::string fwant = fx::provenance_literal(11, 6, 82, elim, std::vector<int>(registered::kHatchFilip.e, registered::kHatchFilip.e + 6));
        CHECK(fsol->provenance == fwant);
        fx::say("EST-A-215 (b): %s", fsol->provenance.c_str());
    }

    // (c) solving twice gives bit-identical solutions: no hidden state (five problems: Norris, Pontius, Longley, Wampler1, the Filip hatch)
    {
        int twice = 0;
        for (const std::string nm : {"Norris", "Pontius", "Longley", "Wampler1"}) {
            const fx::NistData dd = fx::load_nist(nm);
            auto e = fx::estimator_of(dd);
            const auto a = e.solve();
            const auto b = e.solve();
            REQUIRE((a.has_value() && b.has_value()));
            CHECK(same_solution(*a, *b));
            ++twice;
        }
        {
            const fx::NistData fd = fx::load_nist("Filip");
            auto e = fx::estimator_of(fd);
            const SolveOptions opt = fx::filip_hatch_options(fd);
            const auto a = e.solve(opt);
            const auto b = e.solve(opt);
            REQUIRE((a.has_value() && b.has_value()));
            CHECK(same_solution(*a, *b));
            ++twice;
        }
        CHECK(twice == 5);
        fx::say("EST-A-215 (c): %d problems solved twice, bit-identical", twice);
    }
}
