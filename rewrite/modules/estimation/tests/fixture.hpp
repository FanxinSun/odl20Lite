#pragma once
// The fixture of SPEC-estimation.md §8.2: the NIST StRD linear-regression datasets as the doubles the estimator is fed, by the recipe the sizing tool
// (tools/estimation_sizing.py) uses, so that every registered figure applies to the very rows of the test.  The files are read from the PINNED CACHE
// (the manifest's `nist-strd-lls-<name>` entries); nothing of NIST's is copied into the tree, and the certified numbers are read from the files at run
// time and quoted in the specification as facts with NIST's citation.

#include <odl/estimation/normal_equations.hpp>

#include "registered_figures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef ODL_MANIFEST_CACHE_ROOT
#error "ODL_MANIFEST_CACHE_ROOT must name the manifest's cache directory (the module's CMakeLists defines it)"
#endif

namespace odl::estimation::fx {

using ld = long double;

struct NistData {
    std::string name;
    std::size_t n = 0, m = 0;
    std::vector<std::string> parameter_names;
    std::vector<double> design;                      ///< m x n, row-major, the doubles of the recipe
    std::vector<double> response;                    ///< m
    std::vector<ld> certified, certified_sd;         ///< parsed by strtold from the file's own certified block
    ld certified_rsd = 0.0L;
    std::vector<std::string> certified_text;

    [[nodiscard]] std::uint64_t checksum() const {
        std::uint64_t s = 0;
        for (std::size_t k = 0; k < m; ++k) {
            for (std::size_t i = 0; i < n; ++i) s += std::bit_cast<std::uint64_t>(design[k * n + i]);
            s += std::bit_cast<std::uint64_t>(response[k]);
        }
        return s;
    }
};

[[nodiscard]] inline std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

[[nodiscard]] inline std::vector<std::string> tokens(const std::string& line) {
    std::istringstream is(line);
    std::vector<std::string> out;
    for (std::string t; is >> t;) out.push_back(t);
    return out;
}

/// Reads `Certified Values  (lines a to b)` style ranges from the file's own header.
[[nodiscard]] inline std::pair<int, int> header_range(const std::vector<std::string>& lines, const std::string& key) {
    for (std::size_t i = 0; i < lines.size() && i < 14; ++i) {
        const std::size_t at = lines[i].find(key);
        if (at == std::string::npos) continue;
        const std::size_t lp = lines[i].find("(lines ", at);
        if (lp == std::string::npos) continue;
        int a = 0, b = 0;
        if (std::sscanf(lines[i].c_str() + lp, "(lines %d to %d)", &a, &b) == 2) return {a, b};
    }
    throw std::runtime_error("no '" + key + "' range in the header of the NIST file");
}

[[nodiscard]] inline NistData load_nist(const std::string& name) {
    const std::string path = std::string(ODL_MANIFEST_CACHE_ROOT) + "/nist-strd-lls-" + lower(name) + "/" + name + ".dat";
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open the pinned NIST file " + path);
    std::vector<std::string> lines;
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    const auto [c0, c1] = header_range(lines, "Certified Values");
    const auto [d0, d1] = header_range(lines, "Data");
    NistData d;
    d.name = name;
    for (int i = c0 - 1; i < c1; ++i) {
        const auto t = tokens(lines[static_cast<std::size_t>(i)]);
        if (t.size() == 3 && t[0].size() >= 2 && t[0][0] == 'B' && std::isdigit(static_cast<unsigned char>(t[0][1]))) {
            d.certified_text.push_back(t[1]);
            d.certified.push_back(std::strtold(t[1].c_str(), nullptr));
            d.certified_sd.push_back(std::strtold(t[2].c_str(), nullptr));
        }
        if (t.size() == 1 && t[0] == "Residual" && i + 1 < c1) {
            const auto u = tokens(lines[static_cast<std::size_t>(i) + 1]);
            if (u.size() == 3 && u[0] == "Standard") d.certified_rsd = std::strtold(u[2].c_str(), nullptr);
        }
    }
    for (int i = d0 - 1; i < d1; ++i) {
        const auto t = tokens(lines[static_cast<std::size_t>(i)]);
        if (t.empty()) continue;
        std::vector<ld> v;
        for (const auto& s : t) v.push_back(std::strtold(s.c_str(), nullptr));
        const ld y = v[0];
        std::vector<ld> row;
        if (name == "NoInt1" || name == "NoInt2") {
            row = {v[1]};
        } else if (name == "Longley") {
            row = {1.0L};
            for (std::size_t j = 1; j < v.size(); ++j) row.push_back(v[j]);
        } else if (name == "Norris") {
            row = {1.0L, v[1]};
        } else if (name == "Pontius") {
            row = {1.0L, v[1], v[1] * v[1]};
        } else {                                      // Filip (degree 10) and Wampler1 .. 5 (degree 5): repeated multiplication in long double
            const int degree = name == "Filip" ? 10 : 5;
            ld p = 1.0L;
            row.push_back(p);
            for (int k = 0; k < degree; ++k) {
                p = p * v[1];
                row.push_back(p);
            }
        }
        if (d.n == 0) d.n = row.size();
        for (const ld r : row) d.design.push_back(static_cast<double>(r));
        d.response.push_back(static_cast<double>(y));
        ++d.m;
    }
    for (std::size_t i = 0; i < d.n; ++i) d.parameter_names.push_back("B" + std::to_string(name == "NoInt1" || name == "NoInt2" ? 1 : i));
    return d;
}

/// The estimator of a dataset's rows, sigma = 1 for every row (a weight is stated, even 1).
[[nodiscard]] inline NormalEquations estimator_of(const NistData& d) {
    auto made = NormalEquations::create(d.parameter_names);
    if (!made) throw std::runtime_error("create: " + made.error().message);
    NormalEquations ne = std::move(*made);
    for (std::size_t k = 0; k < d.m; ++k) {
        auto r = ne.add(std::span<const double>(&d.design[k * d.n], d.n), d.response[k], 1.0);
        if (!r) throw std::runtime_error("add: " + r.error().message);
    }
    return ne;
}

[[nodiscard]] inline const registered::NistFigures& figures(std::string_view name) {      // by value: a literal argument makes no temporary the reference could dangle from
    for (const auto& f : registered::kNist)
        if (name == f.name) return f;
    throw std::runtime_error("no registered figures for " + std::string(name));
}

/// The ten datasets the certified-value gate is registered against (Filip is registered as refused).
[[nodiscard]] inline const std::vector<std::string>& served() {
    static const std::vector<std::string> v = {"Norris", "Pontius", "NoInt1", "NoInt2", "Longley", "Wampler1", "Wampler2", "Wampler3", "Wampler4", "Wampler5"};
    return v;
}


// ---------------------------------------------------------------------------------------------------------------------------- the comparators
// Layers (a) and (b) are checked against EXTENDED-PRECISION evaluations of the same doubles (x87 long double, u' = 2^-64, 2^-11 of u), and
// the comparator's own error is stated in the specification (at most 5e-4 of each bound).

inline constexpr ld kU = 0x1p-53L;

[[nodiscard]] inline ld gamma_k(std::size_t k) {
    const ld ku = static_cast<ld>(k) * kU;
    return ku / (1.0L - ku);
}
[[nodiscard]] inline ld chi_n(std::size_t n) { return 1.0L / (1.0L - static_cast<ld>(n + 1) * kU); }
/// Theta_s(n) of SPEC-estimation 3.4.
[[nodiscard]] inline ld theta_s(std::size_t n) {
    const ld nn = static_cast<ld>(n);
    return chi_n(n) * ((3.0L * nn * nn + nn) * kU + nn * nn * nn * kU * kU);
}

struct LdSystem {
    std::size_t n = 0;
    std::vector<ld> N, b;
};

/// N' and b' accumulated in long double from the (already whitened) double rows, EST-R-203's order.
[[nodiscard]] inline LdSystem form_long_double(std::span<const double> rows, std::span<const double> resid, std::size_t n) {
    LdSystem s;
    s.n = n;
    s.N.assign(n * n, 0.0L);
    s.b.assign(n, 0.0L);
    for (std::size_t k = 0; k < resid.size(); ++k) {
        const double* a = &rows[k * n];
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j <= i; ++j) s.N[i * n + j] += static_cast<ld>(a[i]) * static_cast<ld>(a[j]);
            s.b[i] += static_cast<ld>(a[i]) * static_cast<ld>(resid[k]);
        }
    }
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) s.N[i * n + j] = s.N[j * n + i];
    return s;
}

/// D = diag(sqrt(N-hat_ii)) in long double.
[[nodiscard]] inline std::vector<ld> d_of(const FormedSystem& f) {
    std::vector<ld> d(f.n);
    for (std::size_t i = 0; i < f.n; ++i) d[i] = std::sqrt(static_cast<ld>(f.matrix[i * f.n + i]));
    return d;
}

/// The scaled difference ||D (a - ref)||_2 / ||D ref||_2, in long double.
[[nodiscard]] inline ld scaled_difference(const std::vector<ld>& d, std::span<const double> a, std::span<const ld> ref) {
    ld num = 0.0L, den = 0.0L;
    for (std::size_t i = 0; i < d.size(); ++i) {
        const ld t = d[i] * (static_cast<ld>(a[i]) - ref[i]);
        num += t * t;
        const ld u = d[i] * ref[i];
        den += u * u;
    }
    return std::sqrt(num) / std::sqrt(den);
}

/// The scaled backward error omega = ||D^-1 (b-hat - N-hat x)||_2 / ||D x||_2 with the residual evaluated in long double from the doubles.
[[nodiscard]] inline ld backward_error(const FormedSystem& f, std::span<const double> x) {
    const std::vector<ld> d = d_of(f);
    ld num = 0.0L, den = 0.0L;
    for (std::size_t i = 0; i < f.n; ++i) {
        ld r = static_cast<ld>(f.rhs[i]);
        for (std::size_t j = 0; j < f.n; ++j) r -= static_cast<ld>(f.matrix[i * f.n + j]) * static_cast<ld>(x[j]);
        num += (r / d[i]) * (r / d[i]);
        const ld t = d[i] * static_cast<ld>(x[i]);
        den += t * t;
    }
    return std::sqrt(num) / std::sqrt(den);
}

// ---------------------------------------------------------------------------------------------------------------------------- seeded problems
// Deterministic: std::mt19937_64's sequence is specified by the standard; no <random> distribution is used (their algorithms are not).

[[nodiscard]] inline int uniform_int(std::mt19937_64& g, int lo, int hi) {
    return lo + static_cast<int>(g() % static_cast<std::uint64_t>(hi - lo + 1));
}

struct Rows {
    std::size_t n = 0;
    std::vector<double> a;                           ///< m x n row-major
    std::vector<double> r;                           ///< m
    [[nodiscard]] std::size_t m() const { return r.size(); }
};

/// EST-A-202's problems: the n unit rows, then 2n seeded integer rows in [-8, 8]; every entry of column j multiplied by 2^(p_j).
[[nodiscard]] inline Rows well_posed_rows(std::uint64_t seed, std::size_t n, const std::vector<int>& p) {
    std::mt19937_64 g(seed);
    Rows out;
    out.n = n;
    auto push = [&](const std::vector<double>& row) {
        for (std::size_t j = 0; j < n; ++j) out.a.push_back(std::ldexp(row[j], p[j]));
        out.r.push_back(static_cast<double>(uniform_int(g, -100, 100)));
    };
    for (std::size_t i = 0; i < n; ++i) {
        std::vector<double> row(n, 0.0);
        row[i] = 1.0;
        push(row);
    }
    for (std::size_t k = 0; k < 2 * n; ++k) {
        std::vector<double> row(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) row[j] = static_cast<double>(uniform_int(g, -8, 8));
        push(row);
    }
    return out;
}

/// EST-A-204's problems: entries +-2^(U[-20, 20]) (random mantissa), n <= 6, m <= 30.
[[nodiscard]] inline Rows random_double_rows(std::uint64_t seed, std::size_t n, std::size_t m) {
    std::mt19937_64 g(seed);
    Rows out;
    out.n = n;
    auto draw = [&] {
        const double mant = 1.0 + static_cast<double>(g() >> 11) * 0x1p-53;
        const double sign = (g() & 1U) ? 1.0 : -1.0;
        return sign * std::ldexp(mant, uniform_int(g, -20, 20));
    };
    for (std::size_t k = 0; k < m; ++k) {
        for (std::size_t j = 0; j < n; ++j) out.a.push_back(draw());
        out.r.push_back(draw());
    }
    return out;
}

[[nodiscard]] inline NormalEquations estimator_of_rows(const Rows& rows, const std::vector<std::string>& names) {
    auto made = NormalEquations::create(names);
    if (!made) throw std::runtime_error("create: " + made.error().message);
    NormalEquations ne = std::move(*made);
    for (std::size_t k = 0; k < rows.m(); ++k) {
        auto r = ne.add(std::span<const double>(&rows.a[k * rows.n], rows.n), rows.r[k], 1.0);
        if (!r) throw std::runtime_error("add: " + r.error().message);
    }
    return ne;
}

[[nodiscard]] inline std::vector<std::string> names_of(std::size_t n, const std::string& prefix = "p") {
    std::vector<std::string> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back(prefix + std::to_string(i));
    return v;
}

/// The rows e_k - e_{k-1} (k = 1 .. n+1): N = tridiag(2, -1) exactly; the response r = (1, 1, ..., 1, -n) has the exact solution (1, 2, ..., n).
[[nodiscard]] inline Rows tridiagonal_rows(std::size_t n) {
    Rows out;
    out.n = n;
    for (std::size_t k = 0; k <= n; ++k) {
        std::vector<double> row(n, 0.0);
        if (k < n) row[k] = 1.0;                      // +e_k
        if (k > 0) row[k - 1] = -1.0;                 // -e_{k-1}
        for (const double v : row) out.a.push_back(v);
        out.r.push_back(k == n ? -static_cast<double>(n) : 1.0);
    }
    return out;
}


// ---------------------------------------------------------------------------------------------------------------------------- the synthetic refusals
// The six refusal-producing problems of EST-A-211 (a, a', a'', b, c, f).  ONE list, used by [hatch] (the content of each refusal) and by [control]
// (that no path returns a value, that the hatch is the only way through and that the refusals are not blanket: each problem's NEIGHBOUR, the
// cause removed, completes).

[[nodiscard]] inline Rows rows_of(const std::vector<std::vector<double>>& a, const std::vector<double>& r) {
    Rows out;
    out.n = a.empty() ? 0 : a[0].size();
    for (const auto& row : a)
        for (const double v : row) out.a.push_back(v);
    out.r = r;
    return out;
}

struct Synthetic {
    std::string label;
    std::string id;                                  ///< the refusal solve() returns
    std::string parameter;                           ///< the parameter it names ("" when it names none)
    std::vector<std::string> names;
    Rows rows;                                       ///< the problem
    Rows neighbour;                                  ///< the same shape with the cause removed: its solve completes
    std::string cause;                               ///< a parameter whose elimination removes the cause ("" when no single one does)
    std::string unrelated;                           ///< a parameter whose elimination does NOT touch the cause ("" when there is none)
};

[[nodiscard]] inline std::vector<Synthetic> refusal_problems() {
    const double h = std::ldexp(1.0, -23);
    const double big = std::ldexp(1.0, 600);
    std::vector<Synthetic> v;
    // (a) SYN-zero: rows (1, 0, k), k = 1 .. 4; the neighbour has the column q = k^2
    v.push_back({"SYN-zero", "EST-F-201", "q", {"p", "q", "r"}, rows_of({{1, 0, 1}, {1, 0, 2}, {1, 0, 3}, {1, 0, 4}}, {1, 2, 3, 4}),
                 rows_of({{1, 1, 1}, {1, 4, 2}, {1, 9, 3}, {1, 16, 4}}, {1, 2, 3, 4}), "q", "p"});
    // (a') no rows at all: every parameter is without information; the neighbour has one unit row each
    v.push_back({"no-rows", "EST-F-201", "u", {"u", "v"}, rows_of({}, {}), rows_of({{1, 0}, {0, 1}}, {1, 2}), "", ""});
    // (a'') two zero columns, named together; no single elimination removes the cause
    v.push_back({"two-zero-columns", "EST-F-201", "b", {"a", "b", "c"}, rows_of({{1, 0, 0}, {2, 0, 0}, {3, 0, 0}}, {1, 2, 3}),
                 rows_of({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {1, 2, 3}), "", "a"});
    // (b) SYN-dependent: c is a duplicate of a; the neighbour's columns are mutually orthogonal
    v.push_back({"SYN-dependent", "EST-F-202", "c", {"a", "b", "c"}, rows_of({{1, 1, 1}, {1, 1, 1}, {1, -1, 1}, {1, -1, 1}}, {1, 2, 3, 4}),
                 rows_of({{1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1}}, {1, 2, 3, 4}), "c", "b"});
    // (c) SYN-near: columns (1,1,1,1) and (1 + 2^-23, 1 - 2^-23, 1, 1); the neighbour uses 2^-10
    const double h10 = std::ldexp(1.0, -10);
    v.push_back({"SYN-near", "EST-F-203", "", {"a1", "a2"}, rows_of({{1, 1.0 + h}, {1, 1.0 - h}, {1, 1}, {1, 1}}, {1, 0, 0, 0}),
                 rows_of({{1, 1.0 + h10}, {1, 1.0 - h10}, {1, 1}, {1, 1}}, {1, 0, 0, 0}), "a2", ""});
    // (f) a column multiplied by 2^600: N_bb ~ 2^1200 overflows; the neighbour multiplies by 2^50
    v.push_back({"overflow-column", "EST-F-206", "b", {"a", "b"}, rows_of({{1, big}, {2, big}}, {1, 2}),
                 rows_of({{1, std::ldexp(1.0, 50)}, {2, std::ldexp(1.0, 50)}}, {1, 2}), "b", "a"});
    return v;
}

[[nodiscard]] inline std::string g17(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

/// EST-R-212's literal, built here from its own definition (the tests do not call the estimator to build what they check).
[[nodiscard]] inline std::string provenance_literal(std::size_t n, std::size_t free_count, std::size_t m, const std::vector<std::pair<std::string, double>>& eliminated,
                                                    const std::vector<int>& exponents) {
    std::string p = "estimation=batch-normal-equations;scaling=powers-of-two;parameters=" + std::to_string(n) + ";free=" + std::to_string(free_count) +
                    ";observations=" + std::to_string(m) + ";eliminated=";
    if (eliminated.empty()) p += "none";
    for (std::size_t i = 0; i < eliminated.size(); ++i) p += std::string(i ? "," : "") + eliminated[i].first + "=" + g17(eliminated[i].second);
    p += ";exponents=";
    for (std::size_t i = 0; i < exponents.size(); ++i) p += std::string(i ? "," : "") + std::to_string(exponents[i]);
    p += ";";
    return p;
}

// ---------------------------------------------------------------------------------------------------------------------------- Demmel's example
// DEM89 page 4: H = D A D, D = diag(1, 1e5, 1e-10, 1e15), A and b as printed.  H_ij is formed in long double from the decimals and rounded to double
// once (the same recipe as the sizing tool's, so that the registered e, kappa and B apply to these very doubles).

struct DemmelSystem {
    std::vector<double> h;                           ///< 4 x 4 row-major, full symmetric
    std::vector<double> b;
};

[[nodiscard]] inline std::array<ld, 4> demmel_d() {
    return {std::strtold("1", nullptr), std::strtold("1e5", nullptr), std::strtold("1e-10", nullptr), std::strtold("1e15", nullptr)};
}

[[nodiscard]] inline DemmelSystem demmel_system() {
    const char* a[4][4] = {{"1", "-0.11", "0.24", "-0.34"}, {"-0.11", "1", "0.07", "0.30"}, {"0.24", "0.07", "1", "0.65"}, {"-0.34", "0.30", "0.65", "1"}};
    const std::array<ld, 4> d = demmel_d();
    DemmelSystem s;
    s.h.assign(16, 0.0);
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) s.h[i * 4 + j] = static_cast<double>(std::strtold(a[i][j], nullptr) * d[i] * d[j]);
    s.b = {42.0, -26.0, 24.0, 34.0};
    return s;
}

// ---------------------------------------------------------------------------------------------------------------------------- the hatch

/// Filip with B6 .. B10 eliminated at their certified values (the nearest doubles of the printed decimals).
[[nodiscard]] inline SolveOptions filip_hatch_options(const NistData& d) {
    SolveOptions o;
    for (std::size_t j = 6; j <= 10; ++j) o.eliminate.push_back({d.parameter_names[j], std::strtod(d.certified_text[j].c_str(), nullptr)});
    return o;
}

/// The reduced normal system of EST-R-209 formed HERE, independently of the estimator: the residual r' = r - sum_e a_e v_e left to right by successive
/// subtraction, then the plain double sums of EST-R-203 over the free parameters.
[[nodiscard]] inline FormedSystem reduced_system(const NistData& d, const SolveOptions& opt) {
    std::vector<char> elim(d.n, 0);
    std::vector<double> val(d.n, 0.0);
    for (const Elimination& e : opt.eliminate)
        for (std::size_t i = 0; i < d.n; ++i)
            if (d.parameter_names[i] == e.name) {
                elim[i] = 1;
                val[i] = e.value;
            }
    std::vector<std::size_t> free;
    for (std::size_t i = 0; i < d.n; ++i)
        if (!elim[i]) free.push_back(i);
    FormedSystem f;
    f.n = free.size();
    f.matrix.assign(f.n * f.n, 0.0);
    f.rhs.assign(f.n, 0.0);
    for (std::size_t k = 0; k < d.m; ++k) {
        const double* a = &d.design[k * d.n];
        double acc = d.response[k];
        for (std::size_t i = 0; i < d.n; ++i)
            if (elim[i]) acc = acc - a[i] * val[i];
        for (std::size_t ii = 0; ii < f.n; ++ii) {
            const double ai = a[free[ii]];
            for (std::size_t jj = 0; jj <= ii; ++jj) f.matrix[ii * f.n + jj] += ai * a[free[jj]];
            f.rhs[ii] += ai * acc;
        }
    }
    for (std::size_t i = 0; i < f.n; ++i)
        for (std::size_t j = i + 1; j < f.n; ++j) f.matrix[i * f.n + j] = f.matrix[j * f.n + i];
    return f;
}

/// Prints one line of a result (always visible in the log, whichever way the comparison falls).  The `format` attribute makes the compiler check every
/// format string against its arguments at each call site.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
inline void say(const char* format, ...) {
    std::va_list ap;
    va_start(ap, format);
    std::vprintf(format, ap);
    va_end(ap);
    std::printf("\n");
}

inline std::string id_of(const EstimationRefusal& e) { return std::string(e.id); }

}  // namespace odl::estimation::fx
