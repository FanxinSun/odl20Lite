// hatch_tests.cpp — SPEC-estimation.md §8.2 EST-A-210 (the one hatch: elimination of named parameters at stated values) and EST-A-211 (the refusals, each with
// its content).
#include "fixture.hpp"

#include <cmath>
#include <limits>

using namespace odl::estimation;
using fx::ld;

namespace {

bool same_bits(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Rows added to a fresh estimator of the named parameters.
NormalEquations with_rows(const std::vector<std::string>& names, const std::vector<std::vector<double>>& rows, const std::vector<double>& r) {
    auto made = NormalEquations::create(names);
    if (!made) throw std::runtime_error("create: " + made.error().message);
    NormalEquations ne = std::move(*made);
    for (std::size_t k = 0; k < rows.size(); ++k) {
        auto a = ne.add(rows[k], r[k], 1.0);
        if (!a) throw std::runtime_error("add: " + a.error().message);
    }
    return ne;
}

const fx::Synthetic& problem(const std::vector<fx::Synthetic>& all, const char* label) {
    for (const fx::Synthetic& s : all)
        if (s.label == label) return s;
    throw std::runtime_error(std::string("no synthetic problem ") + label);
}

}  // namespace

TEST_CASE("EST-A-210: the hatch -- Filip with B6 .. B10 eliminated at their certified values, the identity, the refusals", "[estimation][hatch]") {
    // (a) Filip, certified yet beyond normal equations in double, with the five highest-order parameters held at their certified values
    {
        const fx::NistData d = fx::load_nist("Filip");
        REQUIRE(d.checksum() == fx::figures("Filip").checksum);
        const registered::HatchFigures& h = registered::kHatchFilip;
        auto ne = fx::estimator_of(d);
        const SolveOptions opt = fx::filip_hatch_options(d);
        const auto sol = ne.solve(opt);
        REQUIRE(sol.has_value());
        REQUIRE(sol->free_index.size() == 6);
        for (std::size_t i = 0; i < 6; ++i) CHECK(sol->free_index[i] == i);
        CHECK(sol->degrees_of_freedom == 76);
        for (std::size_t i = 0; i < 6; ++i) CHECK(sol->scale_exponents[i] == h.e[i]);
        CHECK(sol->condition.condition_estimate >= 0.99 * h.kappa);
        CHECK(sol->condition.condition_estimate <= 1.01 * h.kappa);
        CHECK(std::fabs(sol->certified_relative_error - h.b_code) <= 1.0e-3 * h.b_code);
        bool exact_values = true, zero_rows = true;
        for (std::size_t j = 6; j <= 10; ++j) {
            if (!same_bits(sol->x[j], opt.eliminate[j - 6].value)) exact_values = false;
            if (sol->eliminated[j] != 1) exact_values = false;
            for (std::size_t i = 0; i < d.n; ++i)
                if (sol->cofactor[j * d.n + i] != 0.0 || sol->cofactor[i * d.n + j] != 0.0) zero_rows = false;
        }
        CHECK(exact_values);
        CHECK(zero_rows);
        // the free ones against the certified estimates: the scaled difference with D from the reduced system, <= B(test) of the hatch
        const FormedSystem reduced = fx::reduced_system(d, opt);
        const std::vector<ld> dd = fx::d_of(reduced);
        std::vector<double> xf(6);
        std::vector<ld> cf(6);
        for (std::size_t i = 0; i < 6; ++i) {
            xf[i] = sol->x[i];
            cf[i] = d.certified[i];
        }
        const ld phi = fx::scaled_difference(dd, xf, cf);
        CHECK(phi <= static_cast<ld>(h.b_test));
        double min_lre = 15.0;
        for (std::size_t i = 0; i < 6; ++i) {
            const ld r = std::fabs((static_cast<ld>(sol->x[i]) - d.certified[i]) / d.certified[i]);
            min_lre = std::min(min_lre, r == 0.0L ? 15.0 : static_cast<double>(-std::log10(r)));
        }
        // the provenance literal, built from its definition
        std::vector<std::pair<std::string, double>> elim;
        for (std::size_t j = 6; j <= 10; ++j) elim.emplace_back(d.parameter_names[j], opt.eliminate[j - 6].value);
        const std::string want = fx::provenance_literal(11, 6, 82, elim, std::vector<int>(h.e, h.e + 6));
        CHECK(sol->provenance == want);
        fx::say("EST-A-210 (a): Filip/hatch  free %zu  nu = %td  kappa_est = %.4e (exact %.4e)  B = %.4e (registered %.4e)  phi_cert = %.3Le (bound B(test) %.3e)  digits of B0..B5 %.1f [predicted >= 6]",
                sol->free_index.size(), sol->degrees_of_freedom, sol->condition.condition_estimate, h.kappa, sol->certified_relative_error, h.b_code, phi, h.b_test, min_lre);
        fx::say("EST-A-210 (a): %s", sol->provenance.c_str());
    }

    // (b) the identity: eliminating p2 at 1.5 equals, bit for bit, the hand-reduced three-parameter problem (residual r' = r - a2 * 1.5 by successive subtraction)
    {
        const std::vector<std::vector<double>> rows = {{1, 0, 1, 2}, {1, 1, 0, 1}, {1, 2, 1, 0}, {1, 3, 2, 1}, {1, 4, 0, 3}, {1, 5, 1, 1}, {1, 6, 2, 2}, {1, 7, 0, 0}, {1, 8, 1, 3}};
        const std::vector<double> r = {3, 2, 5, 4, 9, 7, 6, 8, 10};
        REQUIRE(rows.size() == 9);
        auto ne = with_rows({"p0", "p1", "p2", "p3"}, rows, r);
        SolveOptions opt;
        opt.eliminate.push_back({"p2", 1.5});
        const auto e = ne.solve(opt);
        REQUIRE(e.has_value());
        std::vector<std::vector<double>> reduced;
        std::vector<double> rp;
        for (std::size_t k = 0; k < rows.size(); ++k) {
            reduced.push_back({rows[k][0], rows[k][1], rows[k][3]});
            double acc = r[k];
            acc = acc - rows[k][2] * 1.5;
            rp.push_back(acc);
        }
        auto nr = with_rows({"p0", "p1", "p3"}, reduced, rp);
        const auto h = nr.solve();
        REQUIRE(h.has_value());
        bool same = same_bits(e->x[0], h->x[0]) && same_bits(e->x[1], h->x[1]) && same_bits(e->x[3], h->x[2]) && same_bits(e->x[2], 1.5);   // h has 3 entries: p3 is h->x[2]
        const std::size_t map[3] = {0, 1, 3};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) same = same && same_bits(e->cofactor[map[i] * 4 + map[j]], h->cofactor[i * 3 + j]);
        REQUIRE(e->scaled_matrix.size() == h->scaled_matrix.size());
        for (std::size_t i = 0; i < e->scaled_matrix.size(); ++i) same = same && same_bits(e->scaled_matrix[i], h->scaled_matrix[i]);
        for (std::size_t i = 0; i < e->scaled_rhs.size(); ++i) same = same && same_bits(e->scaled_rhs[i], h->scaled_rhs[i]);
        for (std::size_t i = 0; i < e->scaled_solution.size(); ++i) same = same && same_bits(e->scaled_solution[i], h->scaled_solution[i]);
        for (std::size_t i = 0; i < e->scaled_cofactor.size(); ++i) same = same && same_bits(e->scaled_cofactor[i], h->scaled_cofactor[i]);
        same = same && same_bits(e->condition.condition_estimate, h->condition.condition_estimate) && same_bits(e->condition.condition_upper, h->condition.condition_upper);
        for (std::size_t i = 0; i < 3; ++i)
            same = same && same_bits(e->condition.parameter_conditions[map[i]], h->condition.parameter_conditions[i]) &&
                   same_bits(e->condition.weakest_direction[map[i]], h->condition.weakest_direction[i]);
        same = same && e->degrees_of_freedom == h->degrees_of_freedom;
        CHECK(same);
        // the one thing that is NOT identical: B carries the elimination's own rounding term (EST-R-206's gamma_2ne (rho_0 + mu_E)), so it is larger
        CHECK(e->certified_relative_error > h->certified_relative_error);
        fx::say("EST-A-210 (b): eliminating p2 at 1.5 equals the hand-reduced problem bit for bit (x, cofactor, scaled system, diagnostics): %s; B %.4e against %.4e (larger by the elimination term)",
                same ? "yes" : "NO", e->certified_relative_error, h->certified_relative_error);
    }

    // (c) the refusals EST-F-204 of the hatch
    {
        const std::vector<std::vector<double>> rows = {{1, 0}, {1, 1}, {1, 2}};
        const std::vector<double> r = {1, 2, 4};
        auto ne = with_rows({"a", "b"}, rows, r);
        int refused = 0;
        auto expect = [&](const SolveOptions& o) {
            const auto s = ne.solve(o);
            REQUIRE_FALSE(s.has_value());
            CHECK(s.error().id == std::string_view("EST-F-204"));
            ++refused;
        };
        SolveOptions unknown;
        unknown.eliminate.push_back({"zz", 1.0});
        expect(unknown);
        SolveOptions repeated;
        repeated.eliminate.push_back({"a", 1.0});
        repeated.eliminate.push_back({"a", 2.0});
        expect(repeated);
        SolveOptions nan;
        nan.eliminate.push_back({"a", kNaN});
        expect(nan);
        SolveOptions all;
        all.eliminate.push_back({"a", 1.0});
        all.eliminate.push_back({"b", 1.0});
        expect(all);
        const std::vector<double> m = {2.0, 0.0, 0.0, 2.0};
        const std::vector<double> b = {1.0, 1.0};
        auto sys = NormalEquations::from_normal_system({"a", "b"}, m, b, 3);
        REQUIRE(sys.has_value());
        SolveOptions one;
        one.eliminate.push_back({"a", 1.0});
        const auto s = sys->solve(one);
        REQUIRE_FALSE(s.has_value());
        CHECK(s.error().id == std::string_view("EST-F-204"));
        ++refused;
        CHECK(refused == 5);
        fx::say("EST-A-210 (c): %d hatch refusals", refused);
    }
}

TEST_CASE("EST-A-211: the refusals, each with its content -- 25 cases", "[estimation][hatch]") {
    int cases = 0;
    const std::vector<fx::Synthetic> syn = fx::refusal_problems();
    REQUIRE(syn.size() == 6);
    auto refused_with = [&](const fx::Synthetic& p) {
        const auto ne = fx::estimator_of_rows(p.rows, p.names);
        auto s = ne.solve();
        REQUIRE_FALSE(s.has_value());
        CHECK(s.error().id == std::string_view(p.id));
        CHECK(s.error().parameter == p.parameter);
        return s;
    };

    {   // (a) SYN-zero: the column q is zero
        ++cases;
        const auto s = refused_with(problem(syn, "SYN-zero"));
        CHECK(s.error().message.rfind("no information about q (", 0) == 0);      // q and only q
    }
    {   // (a') no rows at all: every parameter has no information, and the message names every one of them
        ++cases;
        const auto s = refused_with(problem(syn, "no-rows"));
        CHECK(s.error().message.find("u, v") != std::string::npos);
    }
    {   // (a'') two zero columns: both named
        ++cases;
        const auto s = refused_with(problem(syn, "two-zero-columns"));
        CHECK(s.error().message.find("b, c") != std::string::npos);
    }
    {   // (b) SYN-dependent: c is a duplicate of a; the third pivot is exactly 0
        ++cases;
        const fx::Synthetic& p = problem(syn, "SYN-dependent");
        const auto s = refused_with(p);
        CHECK(p.names[registered::kSynDependentBreakdownIndex] == s.error().parameter);
        CHECK(same_bits(s.error().value, registered::kSynDependentBreakdownPivot));
        CHECK(s.error().value == 0.0);
        CHECK(s.error().message.find("a (1)") != std::string::npos);
        const auto& w = s.error().condition.weakest_direction;
        REQUIRE(w.size() == 3);
        const double r2 = std::sqrt(0.5);
        CHECK(std::fabs(w[0] + r2) <= 1.0e-15);
        CHECK(std::fabs(w[1]) <= 1.0e-15);
        CHECK(std::fabs(w[2] - r2) <= 1.0e-15);
    }
    {   // (c) SYN-near: columns (1,1,1,1) and (1 + 2^-23, 1 - 2^-23, 1, 1); the factorisation completes (the pivot is 2^-47) and the solve certifies no digit
        ++cases;
        const auto s = refused_with(problem(syn, "SYN-near"));
        const double B = s.error().value;
        CHECK(B >= 1.0);
        CHECK(B <= 10.0);
        CHECK(B >= 0.5);
        CHECK(s.error().condition.condition_estimate >= 1.0e14);
        CHECK(s.error().condition.condition_estimate <= 1.0e15);
        const auto& w = s.error().condition.weakest_direction;
        REQUIRE(w.size() == 2);
        CHECK(w[0] * w[1] < 0.0);
        CHECK(std::fabs(w[0]) >= 0.3);
        CHECK(std::fabs(w[1]) >= 0.3);
        fx::say("EST-A-211 (c): SYN-near refused EST-F-203: B = %.4f (predicted %.4f; the registered exact kappa %.4e, lambda_min %.4e, q1 %.4e); kappa_est = %.4e; weakest direction (%.4f, %.4f)", B,
                registered::kSynNearB, registered::kSynNearKappa, registered::kSynNearLamMin, registered::kSynNearQ1, s.error().condition.condition_estimate, w[0], w[1]);
    }
    {   // (d) EST-F-204: invalid input, 15 cases
        auto ne = with_rows({"a", "b"}, {{1, 0}, {1, 1}, {1, 2}}, {1, 2, 4});
        auto add_refused = [&](std::span<const double> row, double res, double sigma) {
            ++cases;
            auto a = ne.add(row, res, sigma);
            REQUIRE_FALSE(a.has_value());
            CHECK(a.error().id == std::string_view("EST-F-204"));
            return a;
        };
        const std::vector<double> good = {1.0, 3.0};
        const std::vector<double> short_row = {1.0};
        const std::vector<double> inf_row = {1.0, kInf};
        const std::vector<double> neg_inf_row = {-kInf, 1.0};
        {
            const auto a = add_refused(short_row, 1.0, 1.0);                                       // 1: a row of the wrong length -- the row's index is named
            CHECK(a.error().message.find("observation row 3") != std::string::npos);
        }
        (void)add_refused(good, kNaN, 1.0);              // 2: a NaN residual
        (void)add_refused(inf_row, 1.0, 1.0);            // 3: an infinite row entry
        (void)add_refused(neg_inf_row, 1.0, 1.0);        // 4: a negative infinite row entry
        (void)add_refused(good, 1.0, 0.0);               // 5: sigma = 0
        (void)add_refused(good, 1.0, -1.0);              // 6: sigma = -1
        (void)add_refused(good, 1.0, kNaN);              // 7: sigma = NaN
        (void)add_refused(good, 1.0, kInf);              // 8: sigma = +inf
        ++cases;                                         // 9: an empty name
        {
            const auto m = NormalEquations::create({"a", ""});
            REQUIRE_FALSE(m.has_value());
            CHECK(m.error().id == std::string_view("EST-F-204"));
        }
        ++cases;                                         // 10: a duplicate name
        {
            const auto m = NormalEquations::create({"a", "b", "a"});
            REQUIRE_FALSE(m.has_value());
            CHECK(m.error().id == std::string_view("EST-F-204"));
            CHECK(m.error().parameter == "a");
        }
        ++cases;                                         // 11: no parameters
        {
            const auto m = NormalEquations::create({});
            REQUIRE_FALSE(m.has_value());
            CHECK(m.error().id == std::string_view("EST-F-204"));
        }
        const std::vector<double> m3 = {1, 0, 0, 1, 0, 0};
        const std::vector<double> b2 = {1, 1};
        const std::vector<double> m4 = {1, 0, 0, 1};
        const std::vector<double> b3 = {1, 1, 1};
        const std::vector<double> bnan = {1.0, kNaN};
        ++cases;                                         // 12: a system matrix of the wrong size
        {
            const auto m = NormalEquations::from_normal_system({"a", "b"}, m3, b2, 3);
            REQUIRE_FALSE(m.has_value());
            CHECK(m.error().id == std::string_view("EST-F-204"));
        }
        ++cases;                                         // 13: a right-hand side of the wrong size
        {
            const auto m = NormalEquations::from_normal_system({"a", "b"}, m4, b3, 3);
            REQUIRE_FALSE(m.has_value());
            CHECK(m.error().id == std::string_view("EST-F-204"));
        }
        ++cases;                                         // 14: a non-finite right-hand side
        {
            const auto m = NormalEquations::from_normal_system({"a", "b"}, m4, bnan, 3);
            REQUIRE_FALSE(m.has_value());
            CHECK(m.error().id == std::string_view("EST-F-204"));
        }
        ++cases;                                         // 15: a row added to a system-mode estimator
        {
            auto sys = NormalEquations::from_normal_system({"a", "b"}, m4, b2, 3);
            REQUIRE(sys.has_value());
            const auto a = sys->add(good, 1.0, 1.0);
            REQUIRE_FALSE(a.has_value());
            CHECK(a.error().id == std::string_view("EST-F-204"));
        }
    }
    {   // (e) EST-F-205: nu = 0, and a system without a residual square sum
        auto ne = with_rows({"a", "b", "c"}, {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {1, 2, 3});
        const auto s = ne.solve();
        REQUIRE(s.has_value());
        CHECK(s->degrees_of_freedom == 0);
        ++cases;
        {
            const auto sc = s->scaled_covariance();
            REQUIRE_FALSE(sc.has_value());
            CHECK(sc.error().id == std::string_view("EST-F-205"));
        }
        ++cases;
        {
            const auto s2 = s->sigma0_squared();
            REQUIRE_FALSE(s2.has_value());
            CHECK(s2.error().id == std::string_view("EST-F-205"));
        }
        CHECK(s->formal_covariance().size() == 9);       // the formal covariance works
        const std::vector<double> m = {1, 0, 0, 1};
        const std::vector<double> b = {1, 1};
        auto sys = NormalEquations::from_normal_system({"a", "b"}, m, b, 5);
        REQUIRE(sys.has_value());
        const auto t = sys->solve();
        REQUIRE(t.has_value());
        ++cases;
        {
            const auto s2 = t->sigma0_squared();
            REQUIRE_FALSE(s2.has_value());
            CHECK(s2.error().id == std::string_view("EST-F-205"));
        }
        ++cases;
        {
            const auto sc = t->scaled_covariance();
            REQUIRE_FALSE(sc.has_value());
            CHECK(sc.error().id == std::string_view("EST-F-205"));
        }
    }
    {   // (f) EST-F-206: a column multiplied by 2^600, so N_ii ~ 2^1200 overflows
        ++cases;
        (void)refused_with(problem(syn, "overflow-column"));
    }
    CHECK(cases == 25);
    fx::say("EST-A-211: %d refusal cases", cases);
}
