// scale_tests.cpp — SPEC-estimation.md §8.2 EST-A-201 (the scale rule) and EST-A-202 (the scaling and the unscaling are exact).
#include "fixture.hpp"

#include <cmath>
#include <limits>

using namespace odl::estimation;

TEST_CASE("EST-A-201: the scale rule -- the exponent table, the interval [1/2, 2) over 10^4 values and at the boundaries, the refusals", "[estimation][scale]") {
    // (a) the table, derived by hand in the specification from frexp and e = floor(k/2)
    struct Row {
        double d;
        int e;
        double scaled;       // the scaled diagonal d 2^(-2e), exact where the literal is
        bool exact;
    };
    const Row table[] = {
        {1.0, 0, 1.0, true},       {2.0, 1, 0.5, true},       {3.0, 1, 0.75, true},      {4.0, 1, 1.0, true},      {8.0, 2, 0.5, true},
        {0.5, 0, 0.5, true},       {0.25, -1, 1.0, true},     {0.1, -2, 1.6, true},      {1.99, 0, 1.99, true},    {7.9, 1, 1.975, true},
        {0x1p-900, -450, 1.0, true}, {0x1p900, 450, 1.0, true}, {1e-300, -498, 0.6696928794914171, false}, {1e300, 498, 1.4932217896051503, false}};
    int rows = 0;
    for (const Row& r : table) {
        ++rows;
        CHECK(scale_exponent(r.d) == r.e);
        const double s = std::ldexp(r.d, -2 * r.e);
        if (r.exact) CHECK(s == r.scaled);
        else CHECK(std::fabs(s - r.scaled) <= 1e-15 * r.scaled);
        CHECK(s >= 0.5);
        CHECK(s < 2.0);
    }
    REQUIRE(rows == 14);
    fx::say("EST-A-201 (a): %d table rows checked", rows);

    // (b) 10^4 seeded values log-uniform over [2^-1000, 2^1000] and 18 boundary points
    std::mt19937_64 gen(20261006);
    int cases = 0, outside = 0;
    for (int i = 0; i < 10000; ++i) {
        const double mant = 1.0 + static_cast<double>(gen() >> 11) * 0x1p-53;
        const double d = std::ldexp(mant, fx::uniform_int(gen, -1000, 999));
        const int e = scale_exponent(d);
        const double s = std::ldexp(d, -2 * e);
        ++cases;
        if (!(s >= 0.5 && s < 2.0)) ++outside;
    }
    for (const int j : {-1000, -1, 0, 1, 2, 999}) {
        const double v = std::ldexp(1.0, j);
        for (const double d : {v, std::nextafter(v, 0.0), std::nextafter(v, std::numeric_limits<double>::infinity())}) {
            const double s = std::ldexp(d, -2 * scale_exponent(d));
            ++cases;
            if (!(s >= 0.5 && s < 2.0)) ++outside;
        }
    }
    REQUIRE(cases == 10018);
    CHECK(outside == 0);
    fx::say("EST-A-201 (b): %d values, %d outside [1/2, 2)", cases, outside);

    // (c) the refusals, through the estimator
    auto refuse_system = [](double d) {
        const std::vector<double> m = {d};
        const std::vector<double> b = {1.0};
        auto ne = NormalEquations::from_normal_system({"p"}, m, b, 1);
        REQUIRE(ne.has_value());
        return ne->solve();
    };
    int refusals = 0;
    for (const double d : {0.0, -1.0}) {
        const auto r = refuse_system(d);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == std::string_view("EST-F-201"));
        CHECK(r.error().parameter == "p");
        ++refusals;
    }
    for (const double d : {std::ldexp(1.0, -1001), std::ldexp(1.0, 1001)}) {
        const auto r = refuse_system(d);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == std::string_view("EST-F-206"));
        CHECK(r.error().parameter == "p");
        ++refusals;
    }
    {   // a diagonal that overflows to +inf: rows of entries 1e200
        auto ne = NormalEquations::create({"p"});
        REQUIRE(ne.has_value());
        const std::vector<double> row = {1e200};
        REQUIRE(ne->add(row, 1.0, 1.0).has_value());
        const auto r = ne->solve();
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == std::string_view("EST-F-206"));
        ++refusals;
    }
    {   // NaN offered to from_normal_system is EST-F-204
        const std::vector<double> m = {std::numeric_limits<double>::quiet_NaN()};
        const std::vector<double> b = {1.0};
        const auto ne = NormalEquations::from_normal_system({"p"}, m, b, 1);
        REQUIRE_FALSE(ne.has_value());
        CHECK(ne.error().id == std::string_view("EST-F-204"));
        ++refusals;
    }
    REQUIRE(refusals == 6);
    fx::say("EST-A-201 (c): %d refusals checked", refusals);
}

TEST_CASE("EST-A-202: the scaling is exact and the unscaling is exact -- 40 seeded problems, bitwise", "[estimation][scale]") {
    int completed = 0, cases = 0;
    for (int c = 0; c < 40; ++c) {
        ++cases;
        const std::size_t n = 2 + static_cast<std::size_t>(c % 8);
        std::mt19937_64 g(777 + static_cast<std::uint64_t>(c));
        std::vector<int> p(n);
        for (auto& v : p) v = fx::uniform_int(g, -30, 30);
        const fx::Rows rows = fx::well_posed_rows(20261006 + static_cast<std::uint64_t>(c), n, p);
        auto ne = fx::estimator_of_rows(rows, fx::names_of(n));
        const auto formed = ne.formed_system();
        REQUIRE(formed.has_value());
        const auto sol = ne.solve();
        if (!sol) {
            fx::say("EST-A-202: case %d (n = %zu) REFUSED %.*s: %s", c, n, static_cast<int>(sol.error().id.size()), sol.error().id.data(), sol.error().message.c_str());
            continue;
        }
        ++completed;
        REQUIRE(sol->free_parameters == n);
        bool all_exact = true;
        for (std::size_t i = 0; i < n; ++i) {
            const int ei = sol->scale_exponents[i];
            const double dii = sol->scaled_matrix[i * n + i];
            if (!(dii >= 0.5 && dii < 2.0)) all_exact = false;
            if (std::ldexp(sol->scaled_rhs[i], ei) != formed->rhs[i]) all_exact = false;
            if (std::ldexp(sol->scaled_solution[i], -ei) != sol->x[i]) all_exact = false;
            for (std::size_t j = 0; j < n; ++j) {
                const int ej = sol->scale_exponents[j];
                if (std::ldexp(sol->scaled_matrix[i * n + j], ei + ej) != formed->matrix[i * n + j]) all_exact = false;
                if (std::ldexp(sol->scaled_cofactor[i * n + j], -(ei + ej)) != sol->cofactor[i * n + j]) all_exact = false;
                if (sol->cofactor[i * n + j] != sol->cofactor[j * n + i]) all_exact = false;
            }
        }
        CHECK(all_exact);
    }
    REQUIRE(cases == 40);
    CHECK(completed == 40);
    fx::say("EST-A-202: %d problems, %d completed", cases, completed);
}
