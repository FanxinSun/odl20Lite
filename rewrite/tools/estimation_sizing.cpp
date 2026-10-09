// estimation_sizing.cpp — the FROZEN FIGURES of SPEC-estimation §6 (L7 step 2: the batch estimator, normal equations scaled by default).
//
// Plan L0 step 8, group C9, ported to C++ (the user's directive of 2026-10-06) from tools/estimation_sizing.py (deleted by the same commit: `git show 5cf568a:rewrite/tools/estimation_sizing.py`).
//
// The estimator's gate is registered BEFORE any code of it exists (plan §4 rule 7).  This tool is where the numbers the registration needs come from, in exact arithmetic, and it is run by two ctests
// (`estimation.sizing_header_matches_generator`, `estimation.spec_table_matches_generator`) so that no figure in the specification is a number nobody can regenerate (plan §4 rule 3).  What it computes is
// described in estimation_sizing.hpp.
//
//   estimation_sizing [--cache DIR] [--report] [--header PATH] [--verify PATH] [--spec-table] [--verify-spec PATH]
//   exit 0 done (and, with --verify / --verify-spec, the committed text is exactly what the generator emits)   1 the committed text differs from the generator's   2 an argument error, or an input file that is
//   missing, cannot be read or is not the shape expected   70 an error the tool did not anticipate
//
// THE ACCEPTANCE CONDITION (PROVENANCE.md section 41.8, recorded there at the maintainer's request): the committed modules/estimation/tests/registered_figures.hpp and the specification's generated table BYTE FOR BYTE,
// and independence of any interpreter.  Where the Python summed floats with CPython 3.12's compensated sum() this does the same compensated sum on purpose (devkit py_sum), and tests/devtools/estimation_sizing_tests.cpp
// shows, on the real data and on numbers made for it, what the interpreter had been deciding (GitHub's run 37485373306 failed on CPython 3.11, whose sum() is the plain one).  The x87 long double of the design recipe is
// EMULATED exactly on rationals (a 64-bit significand, ties to even), never the hardware's, so that the generator gives the same bytes on a host without x87.
//
// THE PROOF OF THE PORT (registered before any line of this file was written: C9_proof_registration.txt in the group's report files).  The tool's products are two committed texts, and the port printed each of
// them BYTE FOR BYTE on its first and only comparison, against a substitution list registered beforehand that named the generator's own name and nothing else: `--header` wrote
// modules/estimation/tests/registered_figures.hpp (24,404 bytes, 169 lines, sha256 7e94360f...) but for line 1 (".py" -> ".cpp"; 24,405 bytes; lines 2-169 identical, sha256 of those 85b36fc7...), and `--spec-table`
// printed the generated block of SPEC-estimation.md section 6.2 (38 lines, 5,524 bytes, sha256 08aa3f14...) but for its BEGIN marker (lines 2-38 identical, sha256 f8e2eb2a...).  The default report has no record of
// the Python's output: its figures are the header's, printed in the Python's layout and compared with them in the tests, and the columns the header does not hold come from a second method written there.  Each mode
// takes about 4.4 s (the Python's, tens of seconds); the exact elimination on Filip's 11 x 11 normal matrix needed no faster gcd than the devkit's.  C9_proof_result.txt holds the results as they came.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * The Python died with a traceback on a file that is not the shape it reads (no "Certified Values (lines A to B)" or "Data (lines A to B)" in the first twelve lines, no "Residual / Standard Deviation" in the
//     certified block, a data token that is not a number, a row with too few columns); this REFUSES, naming the file, exit 2.  A file that is missing, a directory, a file that cannot be read: REFUSED, exit 2, naming
//     it (the Python's FileNotFoundError was caught, with the OS's words, and exited 2; every other OSError was a traceback).  A singular matrix (the Python's StopIteration) is a refusal too.
//   * The Python's regular expressions are hand-written scanners over the file read as latin-1 (every byte one character) with universal newlines, with Python's `\s` (devkit is_py_space) and `\d` (the decimal digits
//     of latin-1 are ASCII's) and its leftmost-match order; each is tested against the semantics of the pattern it replaces.
//   * Both messages of --verify and --verify-spec go to STANDARD ERROR, as the Python's did (`print(..., file=sys.stderr)` takes the whole conditional expression), and as legendre_reference's port does.
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--cache=DIR` is taken).  `-h` prints this tool's own text.
//   * The Python ran the Jacobi iteration three times on the scaled matrix of a dataset (the eigenvalues, then again with the eigenvectors) and carried the eigenvector matrix through every rotation whether it was
//     asked for or not; the second run is the same computation as the first, so this runs it ONCE with the vectors and takes the eigenvalues from it, and the rotations of the vector matrix are made only when the
//     vectors are asked for (they never enter the matrix).  The elimination skips the multiplications by an entry that is exactly zero (x - f * 0 is x) and writes the zero that x - (x / p) * p is.  The exact
//     normal matrix is computed on one triangle and mirrored (a * b is b * a).  The hatch's reduced response was formed in double by the Python as well, with the b-hat that goes with it; no figure takes either
//     (the figures take N-hat's diagonal, for the scale exponents, and its Cholesky factor), so that dead computation is not repeated.  Every one of these gives the SAME numbers; the tests check the exact results
//     by their postconditions and the real data against the committed record.
//   * `Settings::make_report` (empty means build_report) lets a test put a report of its own in place of the exact arithmetic of the real data, so that the modes and their refusals are tested in microseconds; the two
//     ctests and `estimation_sizing.real_tree` run the real thing.  An error the tool did not anticipate is reported and exits 70 (the Python's traceback exited 1).

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>

#include "estimation_sizing.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <utility>

namespace dk = odl::devkit;

namespace odl::tools::estimation_sizing {

using dk::BigInt;
using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "estimation_sizing";
constexpr double kInfinity = std::numeric_limits<double>::infinity();

// 2 ** k exactly
Rational pow2(std::ptrdiff_t k) {
    const BigInt p = BigInt(1).shifted_left(static_cast<std::size_t>(k >= 0 ? k : -k));
    return k >= 0 ? Rational(p) : Rational(BigInt(1), p);
}

// k // 2 (Python's floor division: -3 // 2 is -2)
int floor_half(int k) { return k >= 0 ? k / 2 : -((-k + 1) / 2); }

// Decimal(token): the text of a datum, exactly
Decimal parse_decimal(std::string_view token) {
    try {
        return Decimal::from_python(token);
    } catch (const dk::DecimalError&) {
        throw BadInput("'" + std::string(token) + "' is not a decimal number");
    }
}

std::string join_ints(const std::vector<int>& v) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) out += (i == 0 ? "" : ", ") + std::to_string(v[i]);
    return out;
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------------------------------------------ exact numbers, x87 long double

Rational exact_rational(double v) {
    if (!std::isfinite(v)) throw BadInput("a number that is not finite has no exact value");
    if (v == 0.0) return Rational();
    int exponent = 0;
    const double fraction = std::frexp(std::fabs(v), &exponent);                  // |v| = fraction * 2^exponent, 0.5 <= fraction < 1
    const auto mantissa = static_cast<std::int64_t>(std::ldexp(fraction, 53));   // its significant bits, as an integer (exact: a double has at most 53)
    const Rational magnitude = Rational(mantissa) * pow2(static_cast<std::ptrdiff_t>(exponent) - 53);
    return v < 0.0 ? -magnitude : magnitude;
}

Rational rational_of(const Decimal& d) {
    if (d.is_zero()) return Rational();
    constexpr std::int64_t kLimit = 100000;
    if (d.exponent() > kLimit || d.exponent() < -kLimit) throw BadInput("the exponent of " + d.to_string() + " is beyond what this reads");
    BigInt coefficient = d.coefficient();
    if (d.is_negative()) coefficient = -coefficient;
    const auto digits = static_cast<unsigned>(d.exponent() >= 0 ? d.exponent() : -d.exponent());
    const BigInt scale = BigInt::pow(BigInt(10), digits);
    return d.exponent() >= 0 ? Rational(coefficient * scale) : Rational(coefficient, scale);
}

Rational round_sig(const Rational& q, unsigned bits) {
    if (q.is_zero()) return Rational();
    const bool negative = q.sign() < 0;
    const BigInt num = q.numerator().abs();
    const BigInt& den = q.denominator();
    auto e2 = static_cast<std::ptrdiff_t>(num.bit_length()) - static_cast<std::ptrdiff_t>(den.bit_length());   // floor(log2 a) is e2 or e2 - 1
    if (num.shifted_left(e2 < 0 ? static_cast<std::size_t>(-e2) : 0) < den.shifted_left(e2 > 0 ? static_cast<std::size_t>(e2) : 0)) --e2;
    const std::ptrdiff_t shift = e2 - (static_cast<std::ptrdiff_t>(bits) - 1);   // the unit in the last place of the result is 2**shift
    BigInt n;
    BigInt twice;
    BigInt half;
    if (shift >= 0) {
        half = den.shifted_left(static_cast<std::size_t>(shift));
        BigInt::DivMod qr = BigInt::divmod(num, half);
        n = std::move(qr.quotient);
        twice = qr.remainder + qr.remainder;
    } else {
        half = den;
        BigInt::DivMod qr = BigInt::divmod(num.shifted_left(static_cast<std::size_t>(-shift)), den);
        n = std::move(qr.quotient);
        twice = qr.remainder + qr.remainder;
    }
    if (twice > half || (twice == half && n.is_odd())) n = n + BigInt(1);
    const Rational magnitude = shift >= 0 ? Rational(n.shifted_left(static_cast<std::size_t>(shift))) : Rational(n, BigInt(1).shifted_left(static_cast<std::size_t>(-shift)));
    return negative ? -magnitude : magnitude;
}

Ld::Ld(std::string_view decimal) : q_(round_sig(rational_of(parse_decimal(decimal)), 64)) {}
Ld::Ld(double value) : q_(round_sig(exact_rational(value), 64)) {}
Ld::Ld(std::int64_t value) : q_(round_sig(Rational(value), 64)) {}

Ld operator*(const Ld& a, const Ld& b) {
    Ld out;
    out.q_ = round_sig(a.q_ * b.q_, 64);
    return out;
}

Ld operator/(const Ld& a, const Ld& b) {
    Ld out;
    out.q_ = round_sig(a.q_ / b.q_, 64);
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the bounds' constants

double gamma_k(int k) {
    const double ku = k * kU;
    return ku / (1.0 - ku);
}

double chi_n(int n) { return 1.0 / (1.0 - (n + 1) * kU); }

double theta_solve(int n) {
    const auto nn = static_cast<std::int64_t>(n);   // (the Python's integers never overflow; 64 bits hold 3 n^2 + n and n^3 up to n = 2,097,151, and the sizes in use are at most 40)
    return chi_n(n) * (static_cast<double>(3 * nn * nn + nn) * kU + static_cast<double>(nn * nn * nn) * kU * kU);
}

double theta_n(int n, int m, double eta_rep) { return n * (gamma_k(m) * (1.0 + gamma_k(m)) + eta_rep) + theta_solve(n); }

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the NIST files

namespace {

// bytes.decode("latin-1") as UTF-8: every byte one character
std::string latin1_to_utf8(std::string_view bytes) {
    std::string out;
    out.reserve(bytes.size());
    for (const char c : bytes) {
        const auto b = static_cast<unsigned char>(c);
        if (b < 0x80) {
            out += c;
        } else {
            out += static_cast<char>(0xC0U | (b >> 6U));
            out += static_cast<char>(0x80U | (b & 0x3FU));
        }
    }
    return out;
}

// --- the scanners of the Python's regular expressions (str patterns: `\s` is is_py_space; `\d` is an ASCII digit, because the text is latin-1 and the decimal digits of U+0000-U+00FF are ASCII's)

std::size_t skip_space(std::string_view s, std::size_t i) {   // \s*
    while (i < s.size()) {
        std::size_t after = 0;
        if (!dk::is_py_space(dk::code_point_at(s, i, after))) break;
        i = after;
    }
    return i;
}

std::size_t skip_non_space(std::string_view s, std::size_t i) {   // \S*
    while (i < s.size()) {
        std::size_t after = 0;
        if (dk::is_py_space(dk::code_point_at(s, i, after))) break;
        i = after;
    }
    return i;
}

std::size_t skip_digits(std::string_view s, std::size_t i) {   // \d*
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
    return i;
}

bool literal_at(std::string_view s, std::size_t i, std::string_view lit) { return i <= s.size() && s.compare(i, lit.size(), lit) == 0; }   // (compare() clamps a substring that runs past the end, and then differs)

// int(digits) for a slice: the value, saturated at 10^18 (a slice bound beyond the list is the list's end, whatever the size of the number; the product below cannot overflow)
std::int64_t count_of(std::string_view digits) {
    constexpr std::int64_t kHuge = 1'000'000'000'000'000'000;
    std::int64_t v = 0;
    for (const char c : digits) {
        if (v >= kHuge / 10) return kHuge;
        v = v * 10 + (c - '0');
    }
    return v;
}

// re.search(keyword + r"\s+\(lines (\d+) to (\d+)\)", text): the leftmost match's two numbers
bool search_lines_range(std::string_view text, std::string_view keyword, std::int64_t& first, std::int64_t& last) {
    for (std::size_t p = text.find(keyword); p != std::string_view::npos; p = text.find(keyword, p + 1)) {
        const std::size_t i = p + keyword.size();
        const std::size_t j = skip_space(text, i);
        if (j == i || !literal_at(text, j, "(lines ")) continue;
        const std::size_t a0 = j + 7;
        const std::size_t a1 = skip_digits(text, a0);
        if (a1 == a0 || !literal_at(text, a1, " to ")) continue;
        const std::size_t b0 = a1 + 4;
        const std::size_t b1 = skip_digits(text, b0);
        if (b1 == b0 || !literal_at(text, b1, ")")) continue;
        first = count_of(text.substr(a0, a1 - a0));
        last = count_of(text.substr(b0, b1 - b0));
        return true;
    }
    return false;
}

// re.match(r"\s+B(\d+)\s+(\S+)\s+(\S+)\s*$", line): the second and third groups
bool match_certified_line(std::string_view ln, std::string& estimate, std::string& deviation) {
    const std::size_t i = skip_space(ln, 0);
    if (i == 0 || i >= ln.size() || ln[i] != 'B') return false;
    const std::size_t d1 = skip_digits(ln, i + 1);
    if (d1 == i + 1) return false;
    const std::size_t t0 = skip_space(ln, d1);
    if (t0 == d1) return false;
    const std::size_t t1 = skip_non_space(ln, t0);
    if (t1 == t0) return false;
    const std::size_t u0 = skip_space(ln, t1);
    if (u0 == t1) return false;
    const std::size_t u1 = skip_non_space(ln, u0);
    if (u1 == u0 || skip_space(ln, u1) != ln.size()) return false;
    estimate = std::string(ln.substr(t0, t1 - t0));
    deviation = std::string(ln.substr(u0, u1 - u0));
    return true;
}

// re.search(r"Residual\s*\n\s*Standard Deviation\s+(\S+)", block): the group of the leftmost match.  The whitespace after "Residual" is one run (the greedy \s* takes all of it and gives back to the LAST
// newline of it); the literal must follow the run, and the run must hold a newline character.
bool search_residual_deviation(std::string_view block, std::string& value) {
    constexpr std::string_view kResidual = "Residual";
    constexpr std::string_view kDeviation = "Standard Deviation";
    for (std::size_t p = block.find(kResidual); p != std::string_view::npos; p = block.find(kResidual, p + 1)) {
        const std::size_t i = p + kResidual.size();
        const std::size_t j = skip_space(block, i);
        if (block.substr(i, j - i).find('\n') == std::string_view::npos || !literal_at(block, j, kDeviation)) continue;
        const std::size_t k = j + kDeviation.size();
        const std::size_t t0 = skip_space(block, k);
        if (t0 == k) continue;
        const std::size_t t1 = skip_non_space(block, t0);
        if (t1 == t0) continue;
        value = std::string(block.substr(t0, t1 - t0));
        return true;
    }
    return false;
}

// lines[start:stop] with Python's slice rules (a negative bound counts from the end, one beyond the list is the list's end)
std::vector<std::string> py_slice(const std::vector<std::string>& lines, std::int64_t start, std::int64_t stop) {
    const auto n = static_cast<std::int64_t>(lines.size());
    const auto clamp = [n](std::int64_t k) {
        if (k < 0) k = std::max<std::int64_t>(k + n, 0);
        return std::min(k, n);
    };
    start = clamp(start);
    stop = clamp(stop);
    std::vector<std::string> out;
    for (std::int64_t i = start; i < stop; ++i) out.push_back(lines[static_cast<std::size_t>(i)]);
    return out;
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) out += '\n';
        out += lines[i];
    }
    return out;
}

}  // namespace

NistFile parse_nist(std::string_view bytes, const std::string& what) {
    const std::vector<std::string> lines = dk::splitlines_py(dk::universal_newlines(latin1_to_utf8(bytes)));
    const std::string head = join_lines(py_slice(lines, 0, 12));
    std::int64_t c0 = 0;
    std::int64_t c1 = 0;
    std::int64_t d0 = 0;
    std::int64_t d1 = 0;
    if (!search_lines_range(head, "Certified Values", c0, c1)) throw BadInput(what + ": there is no \"Certified Values (lines A to B)\" in its first twelve lines");
    if (!search_lines_range(head, "Data", d0, d1)) throw BadInput(what + ": there is no \"Data (lines A to B)\" in its first twelve lines");
    NistFile out;
    const std::vector<std::string> certified = py_slice(lines, c0 - 1, c1);
    for (const std::string& ln : certified) {
        std::string estimate;
        std::string deviation;
        if (match_certified_line(ln, estimate, deviation)) {
            out.cert.push_back(std::move(estimate));
            out.sd.push_back(std::move(deviation));
        }
    }
    if (!search_residual_deviation(join_lines(certified), out.rsd)) throw BadInput(what + ": there is no \"Residual / Standard Deviation\" line in its certified values");
    for (const std::string& ln : py_slice(lines, d0 - 1, d1)) {
        std::vector<std::string> tokens = dk::split_py(ln);
        if (!tokens.empty()) out.rows.push_back(std::move(tokens));
    }
    return out;
}

NistFile read_nist(const std::filesystem::path& cache, std::string_view name) {
    std::string lower(name);
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    const std::filesystem::path path = cache / ("nist-strd-lls-" + lower) / (std::string(name) + ".dat");
    dk::Bytes bytes;
    try {
        bytes = dk::read_bytes(path);
    } catch (const std::runtime_error& exc) {
        throw BadInput(exc.what());
    }
    return parse_nist(dk::as_text(dk::ByteView{bytes.data(), bytes.size()}), path.string());
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the design recipe of SPEC-estimation 8.2

namespace {

// One row of the design for the dataset `name`, in the type V (rationals, or extended-precision numbers): the response is vals[0], the abscissa vals[1].
template <class V, class Mul>
std::vector<V> design_row(std::string_view name, const std::vector<V>& vals, const V& one, Mul mul) {
    const auto abscissa = [&]() -> const V& {
        if (vals.size() < 2) throw BadInput(std::string(name) + ": a data row has no abscissa (it has " + std::to_string(vals.size()) + " columns)");
        return vals[1];
    };
    std::vector<V> x;
    if (name == "NoInt1" || name == "NoInt2") {
        x.push_back(abscissa());
    } else if (name == "Longley") {
        x.push_back(one);
        for (std::size_t i = 1; i < vals.size(); ++i) x.push_back(vals[i]);
    } else if (name == "Norris") {
        x.push_back(one);
        x.push_back(abscissa());
    } else if (name == "Pontius") {
        x.push_back(one);
        x.push_back(abscissa());
        x.push_back(mul(abscissa(), abscissa()));
    } else {   // Filip (powers 0 .. 10) and Wampler1 .. 5 (powers 0 .. 5)
        const int top = name == "Filip" ? 10 : 5;
        x.push_back(one);
        V p = one;
        for (int k = 0; k < top; ++k) {
            p = mul(p, abscissa());
            x.push_back(p);
        }
    }
    return x;
}

}  // namespace

ExactDesign design_exact(std::string_view name, const Tokens& rows) {
    ExactDesign out;
    for (const std::vector<std::string>& r : rows) {
        std::vector<Rational> vals;
        for (const std::string& t : r) vals.push_back(rational_of(parse_decimal(t)));
        out.y.push_back(vals.at(0));
        out.x.push_back(design_row<Rational>(name, vals, Rational(1), [](const Rational& a, const Rational& b) { return a * b; }));
    }
    return out;
}

DoubleDesign design_double(std::string_view name, const Tokens& rows) {
    DoubleDesign out;
    for (const std::vector<std::string>& r : rows) {
        std::vector<Ld> vals;
        for (const std::string& t : r) vals.emplace_back(t);
        const std::vector<Ld> x = design_row<Ld>(name, vals, Ld(1), [](const Ld& a, const Ld& b) { return a * b; });
        std::vector<double> row;
        for (const Ld& v : x) row.push_back(v.to_double());
        out.x.push_back(std::move(row));
        out.y.push_back(vals.at(0).to_double());
    }
    return out;
}

std::uint64_t checksum(const DMatrix& x, const std::vector<double>& y) {
    std::uint64_t s = 0;
    for (std::size_t k = 0; k < x.size() && k < y.size(); ++k) {
        for (const double v : x[k]) s += std::bit_cast<std::uint64_t>(v);
        s += std::bit_cast<std::uint64_t>(y[k]);
    }
    return s;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ exact linear algebra

Decimal dec(const Rational& q) { return Decimal(q.numerator()) / Decimal(q.denominator()); }

std::vector<Rational> exact_solve(const RMatrix& m, const std::vector<Rational>& b) {
    const std::size_t n = m.size();
    RMatrix a(n);
    for (std::size_t i = 0; i < n; ++i) {
        a[i] = m[i];
        a[i].push_back(b[i]);
    }
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t piv = i;
        while (piv < n && a[piv][i].is_zero()) ++piv;
        if (piv == n) throw BadInput("the matrix is singular");
        std::swap(a[i], a[piv]);
        for (std::size_t r = 0; r < n; ++r) {
            if (r == i || a[r][i].is_zero()) continue;
            const Rational f = a[r][i] / a[i][i];
            a[r][i] = Rational();   // x - (x / p) * p
            for (std::size_t c = i + 1; c <= n; ++c) {
                if (!a[i][c].is_zero()) a[r][c] = a[r][c] - f * a[i][c];
            }
        }
    }
    std::vector<Rational> x;
    for (std::size_t i = 0; i < n; ++i) x.push_back(a[i][n] / a[i][i]);
    return x;
}

RMatrix exact_inverse(const RMatrix& m) {
    const std::size_t n = m.size();
    RMatrix a(n);
    for (std::size_t i = 0; i < n; ++i) {
        a[i] = m[i];
        for (std::size_t j = 0; j < n; ++j) a[i].push_back(i == j ? Rational(1) : Rational());
    }
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t piv = i;
        while (piv < n && a[piv][i].is_zero()) ++piv;
        if (piv == n) throw BadInput("the matrix is singular");
        std::swap(a[i], a[piv]);
        const Rational d = a[i][i];
        for (std::size_t c = i; c < 2 * n; ++c) {
            if (!a[i][c].is_zero()) a[i][c] = a[i][c] / d;
        }
        for (std::size_t r = 0; r < n; ++r) {
            if (r == i || a[r][i].is_zero()) continue;
            const Rational f = a[r][i];
            a[r][i] = Rational();   // x - f * 1
            for (std::size_t c = i + 1; c < 2 * n; ++c) {
                if (!a[i][c].is_zero()) a[r][c] = a[r][c] - f * a[i][c];
            }
        }
    }
    RMatrix q(n);
    for (std::size_t i = 0; i < n; ++i) q[i].assign(a[i].begin() + static_cast<std::ptrdiff_t>(n), a[i].end());
    return q;
}

JacobiResult jacobi_eigs(const DecMatrix& m, bool vectors) {
    const dk::LocalContext context(kPrecision);
    const std::size_t n = m.size();
    DecMatrix a = m;
    DecMatrix v(n, std::vector<Decimal>(n));
    for (std::size_t i = 0; i < n; ++i) v[i][i] = Decimal(1);
    const Decimal tolerance = Decimal(10).pow(Decimal(-160));
    const Decimal two(2);
    const Decimal one(1);
    for (int sweep = 0; sweep < 100; ++sweep) {
        Decimal off(0);
        Decimal dg(0);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) off = off + a[i][j].pow(two);
        }
        for (std::size_t i = 0; i < n; ++i) dg = dg + a[i][i].pow(two);
        if (off <= dg * tolerance) break;
        for (std::size_t p = 0; p + 1 < n; ++p) {
            for (std::size_t q = p + 1; q < n; ++q) {
                if (a[p][q].is_zero()) continue;
                const Decimal th = (a[q][q] - a[p][p]) / (two * a[p][q]);
                const Decimal t = Decimal(th >= Decimal(0) ? 1 : -1) / (th.abs() + (th * th + one).sqrt());
                const Decimal c = one / (t * t + one).sqrt();
                const Decimal s = t * c;
                for (std::size_t k = 0; k < n; ++k) {
                    const Decimal akp = a[k][p];
                    const Decimal akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                    if (vectors) {
                        const Decimal vkp = v[k][p];
                        const Decimal vkq = v[k][q];
                        v[k][p] = c * vkp - s * vkq;
                        v[k][q] = s * vkp + c * vkq;
                    }
                }
                for (std::size_t k = 0; k < n; ++k) {
                    const Decimal apk = a[p][k];
                    const Decimal aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
            }
        }
    }
    std::vector<std::size_t> order(n);
    for (std::size_t i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&a](std::size_t x, std::size_t y) { return a[x][x] < a[y][y]; });
    JacobiResult out;
    for (const std::size_t i : order) out.values.push_back(a[i][i]);
    if (vectors) {
        for (const std::size_t i : order) {
            std::vector<Decimal> column;
            for (std::size_t k = 0; k < n; ++k) column.push_back(v[k][i]);
            out.vectors.push_back(std::move(column));
        }
    }
    return out;
}

int scale_exponent(double d) {
    int k = 0;
    std::frexp(d, &k);
    return floor_half(k);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the double-precision recipe

NormalEquations normal_double(const DMatrix& x, const std::vector<double>& y) {
    const std::size_t m = x.size();
    const std::size_t n = x.at(0).size();
    NormalEquations out;
    out.n.assign(n, std::vector<double>(n, 0.0));
    out.b.assign(n, 0.0);
    for (std::size_t k = 0; k < m; ++k) {
        const std::vector<double>& a = x[k];
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j <= i; ++j) out.n[i][j] += a.at(i) * a.at(j);
            out.b[i] += a.at(i) * y.at(k);
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < i; ++j) out.n[j][i] = out.n[i][j];
    }
    return out;
}

CholeskyResult cholesky_double(const DMatrix& n_matrix) {
    const std::size_t n = n_matrix.size();
    CholeskyResult out;
    out.l.assign(n, std::vector<double>(n, 0.0));
    DMatrix& l = out.l;
    for (std::size_t i = 0; i < n; ++i) {
        double s = n_matrix[i][i];
        for (std::size_t k = 0; k < i; ++k) s -= l[i][k] * l[i][k];
        if (!(s > 0.0) || !std::isfinite(s)) {
            out.breakdown = Breakdown{static_cast<int>(i), s};
            out.l.clear();
            return out;
        }
        l[i][i] = dk::py_sqrt(s);
        for (std::size_t j = i + 1; j < n; ++j) {
            double t = n_matrix[j][i];
            for (std::size_t k = 0; k < i; ++k) t -= l[j][k] * l[i][k];
            l[j][i] = t / l[i][i];
        }
    }
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ a dataset's figures

namespace {

// float(dec(q).sqrt())
double sqrt_of(const Rational& q) { return dec(q).sqrt().to_double(); }

// the smallest, over the first `count` parameters, of the digits to which the exact solution agrees with the certified value (15 where they are equal), as the Python's generator computes them
double digits_vs_certified(const std::vector<Rational>& x, const std::vector<std::string>& cert, std::size_t count) {
    if (cert.size() < count) throw BadInput("the file certifies " + std::to_string(cert.size()) + " values, " + std::to_string(count) + " are wanted");
    double best = kInfinity;
    for (std::size_t i = 0; i < count; ++i) {
        const Decimal xi = dec(x[i]);
        const Decimal ci = parse_decimal(cert[i]);
        const double digits = xi != ci ? (-((xi - ci).copy_abs() / ci.copy_abs()).log10()).to_double() : 15.0;
        best = std::min(best, digits);
    }
    return best;
}

// max over the columns of the sum of the absolute values of the column: the 1-norm
Rational one_norm(const RMatrix& q) {
    Rational best;
    for (std::size_t j = 0; j < q.size(); ++j) {
        Rational column;
        for (std::size_t i = 0; i < q.size(); ++i) column += q[i][j].abs();
        if (j == 0 || column > best) best = column;
    }
    return best;
}

// N_s = D^-1 N D^-1 with D = diag(2^e): the power-of-two scaling of the code
RMatrix scaled(const RMatrix& n, const std::vector<int>& e) {
    RMatrix out(n.size(), std::vector<Rational>(n.size()));
    for (std::size_t i = 0; i < n.size(); ++i) {
        for (std::size_t j = 0; j < n.size(); ++j) out[i][j] = n[i][j] * pow2(-(e[i] + e[j]));
    }
    return out;
}

DecMatrix decimals_of(const RMatrix& q) {
    DecMatrix out(q.size(), std::vector<Decimal>(q.size()));
    for (std::size_t i = 0; i < q.size(); ++i) {
        for (std::size_t j = 0; j < q.size(); ++j) out[i][j] = dec(q[i][j]);
    }
    return out;
}

// the unit-diagonal matrix A = D^-1 N D^-1 with D = diag(sqrt(N_ii)), in 90 digits
DecMatrix unit_diagonal(const RMatrix& n) {
    std::vector<Decimal> dd;
    for (std::size_t i = 0; i < n.size(); ++i) dd.push_back(dec(n[i][i]).sqrt());
    DecMatrix out(n.size(), std::vector<Decimal>(n.size()));
    for (std::size_t i = 0; i < n.size(); ++i) {
        for (std::size_t j = 0; j < n.size(); ++j) out[i][j] = dec(n[i][j]) / (dd[i] * dd[j]);
    }
    return out;
}

DMatrix scaled_double(const DMatrix& n, const std::vector<int>& e) {
    DMatrix out(n.size(), std::vector<double>(n.size()));
    for (std::size_t i = 0; i < n.size(); ++i) {
        for (std::size_t j = 0; j < n.size(); ++j) out[i][j] = std::ldexp(n[i][j], -(e[i] + e[j]));
    }
    return out;
}

std::vector<int> scale_exponents(const DMatrix& n) {
    std::vector<int> e;
    for (std::size_t i = 0; i < n.size(); ++i) e.push_back(scale_exponent(n[i][i]));
    return e;
}

}  // namespace

DatasetFigures analyse(const std::filesystem::path& cache, std::string_view name) {
    const dk::LocalContext context(kPrecision);
    const NistFile file = read_nist(cache, name);
    const ExactDesign exact = design_exact(name, file.rows);
    const DoubleDesign dbl = design_double(name, file.rows);
    if (dbl.x.empty()) throw BadInput(std::string(name) + ": the file has no data rows");
    const std::size_t m = dbl.x.size();
    const std::size_t n = dbl.x[0].size();
    for (const std::vector<double>& row : dbl.x) {
        if (row.size() < n) throw BadInput(std::string(name) + ": a data row has fewer columns than the first");
    }
    const NormalEquations nh = normal_double(dbl.x, dbl.y);
    DatasetFigures out;
    out.name = std::string(name);
    out.n = static_cast<int>(n);
    out.m = static_cast<int>(m);
    out.e = scale_exponents(nh.n);
    const CholeskyResult chol = cholesky_double(scaled_double(nh.n, out.e));
    out.checksum = checksum(dbl.x, dbl.y);
    out.cert = file.cert;
    out.sd = file.sd;
    out.rsd = file.rsd;
    if (chol.breakdown) {
        out.breakdown_index = chol.breakdown->index;
        out.breakdown_pivot = chol.breakdown->pivot;
    }
    // exact quantities (the decimal data, the same power-of-two scaling as the code)
    RMatrix ne(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            Rational s;
            for (std::size_t k = 0; k < m; ++k) s += exact.x[k][i] * exact.x[k][j];
            ne[i][j] = s;
            ne[j][i] = s;
        }
    }
    std::vector<Rational> be(n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t k = 0; k < m; ++k) be[i] += exact.x[k][i] * exact.y[k];
    }
    const std::vector<Rational> x = exact_solve(ne, be);
    const RMatrix nse = scaled(ne, out.e);
    const RMatrix qse = exact_inverse(nse);
    const Rational q1 = one_norm(qse);
    const JacobiResult ev_a = jacobi_eigs(unit_diagonal(ne));
    const JacobiResult ev_ns = jacobi_eigs(decimals_of(nse), true);   // the eigenvalues, and the vectors of the weakest-direction figures below (the same computation)
    out.kappa = (ev_ns.values.back() / ev_ns.values.front()).to_double();
    out.lam_min_ns = ev_ns.values.front().to_double();
    out.lam_max_ns = ev_ns.values.back().to_double();
    out.lam_min_a = ev_a.values.front().to_double();
    out.q1 = dec(q1).to_double();
    for (std::size_t i = 0; i < n; ++i) out.kappa_i.push_back(dec(ne[i][i] * pow2(-2 * out.e[i]) * qse[i][i]).to_double());   // N_s,ii Q_s,ii = N_ii (N^-1)_ii
    Rational dx2;
    for (std::size_t i = 0; i < n; ++i) dx2 += ne[i][i] * x[i] * x[i];
    out.dx = sqrt_of(dx2);
    Rational y2;
    for (std::size_t k = 0; k < m; ++k) y2 += exact.y[k] * exact.y[k];
    out.r = sqrt_of(y2);
    out.digits_exact_vs_cert = digits_vs_certified(x, file.cert, n);
    // the residual at the exact solution: Omega, sigma0 (against the certified residual standard deviation) and the magnitudes the bounds need
    Rational omega;
    Rational magnitude2;
    for (std::size_t k = 0; k < m; ++k) {
        Rational fitted;
        Rational row_magnitude;
        for (std::size_t i = 0; i < n; ++i) {
            fitted += exact.x[k][i] * x[i];
            row_magnitude += exact.x[k][i].abs() * x[i].abs();
        }
        const Rational rho = exact.y[k] - fitted;
        omega += rho * rho;
        magnitude2 += row_magnitude * row_magnitude;
    }
    const int nu = out.m - out.n;
    out.sigma0_exact = sqrt_of(omega / Rational(nu));
    out.abs_ax_x = sqrt_of(magnitude2);
    // --- the derived bounds
    const double r = out.r;
    const double dx = out.dx;
    const double sqrt_n = dk::py_sqrt(static_cast<double>(n));
    const double tn0 = theta_n(out.n, out.m, 0.0);
    const double tnt = theta_n(out.n, out.m, kEtaRep);
    out.theta_n_code = tn0;
    out.theta_n_test = tnt;
    out.lam_max_a = ev_a.values.back().to_double();
    for (std::size_t i = 0; i < n; ++i) out.qdiag.push_back(dec(qse[i][i] * pow2(-2 * out.e[i])).to_double());   // Q_ii = (N^-1)_ii in the units of the parameters
    out.b_code = dx > 0 ? 4.0 * out.q1 * (tn0 + sqrt_n * gamma_k(out.m) * r / dx) : kInfinity;
    out.b_test = dx > 0 ? 4.0 * out.q1 * (tnt + sqrt_n * (gamma_k(out.m) + kEtaRep) * r / dx) + kCertRelative : kInfinity;
    out.certified_no_digit = out.b_code >= 0.5;
    const double ainv = 1.0 / out.lam_min_a;
    const double tau = tnt * ainv;
    out.tau = tau;
    out.b_q = tau < 1 ? tau / (1.0 - tau) : kInfinity;   // |Q-hat_ii / Q_ii - 1| <= B_Q
    out.b_rho = 3.0 * out.b_q;                           // |rho-hat_ij - rho_ij| <= B_rho
    // sigma0.  Omega(x-hat) = Omega + delta^2 (Pythagoras: the residual at the optimum is orthogonal to the columns), delta^2 = (x-hat - x)' N (x-hat - x)
    //   <= ||A^-1||_2 g0^2, g0 = Theta_N ||D x|| + sqrt(n) (gamma_m + eta_rep) ||r||  (N (x-hat - x) = Delta b - Delta N x-hat);  so
    //   sqrt(Omega-hat) - sqrt(Omega) <= min(delta, delta^2 / (2 sqrt(Omega))); the rounding of the residuals and of the sum of squares and the representation error add
    const double sq_omega = out.sigma0_exact * dk::py_sqrt(static_cast<double>(nu));
    const double g0 = tnt * dx + sqrt_n * (gamma_k(out.m) + kEtaRep) * r;
    const double delta = dk::py_sqrt(ainv) * g0;
    const double d_sqrt = sq_omega == 0.0 ? delta : std::min(delta, delta * delta / (2.0 * sq_omega));
    out.delta = delta;
    out.b_sigma = ((gamma_k(out.n + 1) + 2 * kU + kU * kU) * (r + out.abs_ax_x) + 0.5 * gamma_k(out.m + 1) * sq_omega + d_sqrt) / dk::py_sqrt(static_cast<double>(nu));
    // two solutions of the same problem in different units (EST-A-209): the sum of the two certified errors, the second with twice the representation error
    out.b_pair = dx > 0 ? (out.b_test - kCertRelative) + 4.0 * out.q1 * (theta_n(out.n, out.m, 2 * kEtaRep) + sqrt_n * (gamma_k(out.m) + 2 * kEtaRep) * r / dx) : kInfinity;
    // the weakest direction: the eigenvector of the smallest eigenvalue of N_s (exact), its gap, and the perturbation bound on its angle
    std::vector<Decimal> weakest = ev_ns.vectors.front();
    std::size_t big = 0;
    for (std::size_t i = 1; i < n; ++i) {
        if (weakest[i].abs() > weakest[big].abs()) big = i;
    }
    if (weakest[big] < Decimal(0)) {
        for (Decimal& t : weakest) t = -t;
    }
    for (const Decimal& t : weakest) out.weakest_exact.push_back(t.to_double());
    out.gap = n > 1 ? (ev_ns.values[0] / ev_ns.values[1]).to_double() : 0.0;   // lambda_2(Q_s) / lambda_1(Q_s)
    out.tau_s = 2.0 * tn0 / out.lam_min_ns;                                    // ||Q-hat_s - Q_s||_2 / ||Q_s||_2 <= tau_s (scaled diagonal below 2)
    out.cos_min = 0.0;
    if (n > 1 && out.tau_s / (1.0 - out.gap) < 1.0) {
        const double y = 1.0 - dk::py_pow(out.tau_s / (1.0 - out.gap), 2.0);
        out.cos_min = dk::py_sqrt(y > 0.0 ? y : 0.0);
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) out.rho_exact.push_back((dec(qse[i][j]) / (dec(qse[i][i]) * dec(qse[j][j])).sqrt()).to_double());
    }
    return out;
}

RuleU rule_u_of(const DMatrix& x, const std::vector<double>& y, bool keep_pivots) {
    const NormalEquations nh = normal_double(x, y);
    const CholeskyResult chol = cholesky_double(nh.n);
    RuleU out;
    if (chol.breakdown) {
        out.breakdown = chol.breakdown->index;
        return out;
    }
    std::vector<double> pivots;
    for (std::size_t i = 0; i < nh.n.size(); ++i) pivots.push_back(dk::py_pow(chol.l[i][i], 2.0));
    out.ratio = *std::min_element(pivots.begin(), pivots.end()) / *std::max_element(pivots.begin(), pivots.end());
    if (keep_pivots) out.pivots = pivots;
    return out;
}

RuleU predicted_unscaled_rule(const std::filesystem::path& cache, std::string_view name) {
    const NistFile file = read_nist(cache, name);
    const DoubleDesign dbl = design_double(name, file.rows);
    return rule_u_of(dbl.x, dbl.y, true);
}

EquilibratedRows equilibrated_pontius_rows(const std::filesystem::path& cache) {
    const NistFile file = read_nist(cache, "Pontius");
    const DoubleDesign dbl = design_double("Pontius", file.rows);
    const std::size_t n = dbl.x.at(0).size();
    EquilibratedRows out;
    for (std::size_t j = 0; j < n; ++j) {
        std::vector<double> squares;   // sum(float(LD(x) * LD(x))): CPython 3.12's compensated sum() of floats
        for (const std::vector<double>& row : dbl.x) squares.push_back((Ld(row[j]) * Ld(row[j])).to_double());
        out.norms.push_back(dk::py_sqrt(dk::py_sum(squares)));
    }
    for (const std::vector<double>& row : dbl.x) {
        std::vector<double> scaled_row;
        for (std::size_t j = 0; j < n; ++j) scaled_row.push_back(row[j] / out.norms[j]);
        out.x.push_back(std::move(scaled_row));
    }
    out.y = dbl.y;
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ Demmel's example

DemmelFigures demmel() {
    const dk::LocalContext context(kPrecision);
    const std::size_t n = 4;
    const auto frac = [](std::int64_t num, std::int64_t den) { return Rational(BigInt(num), BigInt(den)); };
    const RMatrix demmel_a = {{frac(1, 1), frac(-11, 100), frac(24, 100), frac(-34, 100)},
                              {frac(-11, 100), frac(1, 1), frac(7, 100), frac(30, 100)},
                              {frac(24, 100), frac(7, 100), frac(1, 1), frac(65, 100)},
                              {frac(-34, 100), frac(30, 100), frac(65, 100), frac(1, 1)}};
    const std::vector<Rational> demmel_b = {Rational(42), Rational(-26), Rational(24), Rational(34)};
    const std::vector<Rational> demmel_d = {Rational(1), Rational(BigInt::pow(BigInt(10), 5)), Rational(BigInt(1), BigInt::pow(BigInt(10), 10)), Rational(BigInt::pow(BigInt(10), 15))};
    const char* const demmel_p[] = {"-.3641849059026662", ".09299067506030982", ".7002799484168281", "-.6069020369959377"};
    const char* const scale_text[] = {"1", "1e5", "1e-10", "1e15"};
    RMatrix h(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) h[i][j] = demmel_d[i] * demmel_a[i][j] * demmel_d[j];
    }
    const std::vector<Rational> x = exact_solve(h, demmel_b);
    std::vector<Rational> y;
    for (std::size_t i = 0; i < n; ++i) y.push_back(demmel_d[i] * x[i]);
    Decimal sum_squares(0);
    for (const Rational& v : y) sum_squares = sum_squares + dec(v).pow(Decimal(2));
    const Decimal nrm = sum_squares.sqrt();
    DemmelFigures out;
    for (const Rational& v : y) out.p.push_back((dec(v) / nrm).to_double());
    // double matrix as the test builds it: H_ij in long double from the decimals, rounded once
    DMatrix hd(n, std::vector<double>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            hd[i][j] = (Ld(demmel_a[i][j].numerator().to_decimal()) / Ld(demmel_a[i][j].denominator().to_decimal()) * Ld(std::string_view(scale_text[i])) * Ld(std::string_view(scale_text[j]))).to_double();
        }
    }
    out.e = scale_exponents(hd);
    RMatrix hs(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) hs[i][j] = exact_rational(hd[i][j]) * pow2(-(out.e[i] + out.e[j]));
    }
    const RMatrix qs = exact_inverse(hs);
    const Rational q1 = one_norm(qs);
    const JacobiResult ev_a = jacobi_eigs(unit_diagonal(h));
    const JacobiResult ev_h = jacobi_eigs(decimals_of(h));
    const JacobiResult ev_ns = jacobi_eigs(decimals_of(hs));
    const double eta_h = 1.002 * kU;
    const double tn = static_cast<double>(n) * eta_h + theta_solve(static_cast<int>(n));
    out.kappa_a = (ev_a.values.back() / ev_a.values.front()).to_double();
    out.kappa_h = (ev_h.values.back() / ev_h.values.front()).to_double();
    out.kappa_ns = (ev_ns.values.back() / ev_ns.values.front()).to_double();
    out.lam_min_a = ev_a.values.front().to_double();
    out.lam_max_a = ev_a.values.back().to_double();
    out.q1 = dec(q1).to_double();
    out.theta_n = tn;
    out.b = 4.0 * dec(q1).to_double() * tn;
    out.b_exact_a = tn / ev_a.values.front().to_double();
    out.published_bound = 1.5e-14;
    for (const char* const p : demmel_p) out.published_vector.push_back(parse_decimal(p).to_double());
    for (const Rational& v : x) out.x.push_back(dec(v).to_double());
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ synthetic problems

SynDependent syn_dependent() {
    const std::vector<double> a = {1.0, 1.0, 1.0, 1.0};
    const std::vector<double> b = {1.0, 1.0, -1.0, -1.0};
    const std::vector<double>& c = a;
    DMatrix x;
    for (std::size_t k = 0; k < 4; ++k) x.push_back({a[k], b[k], c[k]});
    const std::vector<double> y = {1.0, 2.0, 3.0, 4.0};
    const NormalEquations ne = normal_double(x, y);
    SynDependent out;
    out.e = scale_exponents(ne.n);
    const CholeskyResult chol = cholesky_double(scaled_double(ne.n, out.e));
    if (!chol.breakdown) throw std::logic_error("SYN-dependent: the factorisation of the scaled matrix did not break down");
    out.n = ne.n;
    out.breakdown_index = chol.breakdown->index;
    out.breakdown_pivot = chol.breakdown->pivot;
    out.dependency = {1.0, 0.0};
    return out;
}

SynNearlyDependent syn_nearly_dependent() {
    const dk::LocalContext context(kPrecision);
    const double h = std::ldexp(1.0, -23);
    const std::vector<double> a1 = {1.0, 1.0, 1.0, 1.0};
    const std::vector<double> a2 = {1.0 + h, 1.0 - h, 1.0, 1.0};
    DMatrix x;
    for (std::size_t k = 0; k < 4; ++k) x.push_back({a1[k], a2[k]});
    const std::vector<double> y = {1.0, 0.0, 0.0, 0.0};
    const NormalEquations ne = normal_double(x, y);
    SynNearlyDependent out;
    out.e = scale_exponents(ne.n);
    RMatrix n_exact(2, std::vector<Rational>(2));
    for (std::size_t i = 0; i < 2; ++i) {
        for (std::size_t j = 0; j < 2; ++j) n_exact[i][j] = exact_rational(ne.n[i][j]);
    }
    const RMatrix ns = scaled(n_exact, out.e);
    const RMatrix qs = exact_inverse(ns);
    const Rational q1 = one_norm(qs);
    const JacobiResult ev = jacobi_eigs(decimals_of(ns));
    std::vector<double> squares;
    for (const double v : y) squares.push_back(v * v);
    const double r = dk::py_sqrt(dk::py_sum(squares));
    std::vector<Rational> b_exact;
    for (const double v : ne.b) b_exact.push_back(exact_rational(v));
    const std::vector<Rational> sol = exact_solve(n_exact, b_exact);
    Rational dx2;
    for (std::size_t i = 0; i < 2; ++i) dx2 += n_exact[i][i] * sol[i] * sol[i];
    const double dx = sqrt_of(dx2);
    const double tn = theta_n(2, 4, 0.0);
    out.kappa = (ev.values.back() / ev.values.front()).to_double();
    out.lam_min_ns = ev.values.front().to_double();
    out.q1 = dec(q1).to_double();
    out.b = 4.0 * dec(q1).to_double() * (tn + dk::py_sqrt(2.0) * gamma_k(4) * r / dx);
    out.theta_n = tn;
    return out;
}

Tridiagonal tridiagonal(int n) {
    const double lam_min = 2.0 - 2.0 * dk::py_cos(std::numbers::pi / (n + 1));
    Tridiagonal out;
    out.n = n;
    out.m = n + 1;
    out.theta_s = theta_solve(n);
    out.lam_min_a = lam_min / 2.0;   // the diagonal is 2: A = N / 2
    out.b_col = theta_solve(n) / out.lam_min_a;
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the hatch

HatchFigures hatch_filip(const std::filesystem::path& cache) {
    const dk::LocalContext context(kPrecision);
    const NistFile file = read_nist(cache, "Filip");
    const ExactDesign exact = design_exact("Filip", file.rows);
    const DoubleDesign dbl = design_double("Filip", file.rows);
    const std::size_t m = dbl.x.size();
    const std::size_t nfull = 11;
    const std::vector<std::size_t> eliminated = {6, 7, 8, 9, 10};
    const std::vector<std::size_t> free_set = {0, 1, 2, 3, 4, 5};
    const std::size_t nf = free_set.size();
    const std::size_t ne_count = eliminated.size();
    if (file.cert.size() < nfull) throw BadInput("Filip: the file certifies " + std::to_string(file.cert.size()) + " values, eleven are wanted");
    std::vector<Rational> v_exact;
    for (const std::size_t j : eliminated) v_exact.push_back(rational_of(parse_decimal(file.cert[j])));
    // exact reduced problem
    std::vector<Rational> rprime(m);
    for (std::size_t k = 0; k < m; ++k) {
        Rational eliminated_part;
        for (std::size_t i = 0; i < ne_count; ++i) eliminated_part += exact.x[k][eliminated[i]] * v_exact[i];
        rprime[k] = exact.y[k] - eliminated_part;
    }
    RMatrix nff(nf, std::vector<Rational>(nf));
    for (std::size_t a = 0; a < nf; ++a) {
        for (std::size_t b = 0; b <= a; ++b) {
            Rational s;
            for (std::size_t k = 0; k < m; ++k) s += exact.x[k][free_set[a]] * exact.x[k][free_set[b]];
            nff[a][b] = s;
            nff[b][a] = s;
        }
    }
    std::vector<Rational> bf(nf);
    for (std::size_t a = 0; a < nf; ++a) {
        for (std::size_t k = 0; k < m; ++k) bf[a] += exact.x[k][free_set[a]] * rprime[k];
    }
    const std::vector<Rational> x = exact_solve(nff, bf);
    // the double recipe: the forming of N_FF as everywhere.  (The Python also formed the reduced response r'_k = r_k - sum_e a_ke v_e in double, in E order, and b_F from it; no figure uses either -- the figures
    // take N-hat's diagonal, for the scale exponents, and its Cholesky factor -- so the plain response is passed here and the dead computation is not repeated.)
    DMatrix xf;
    for (std::size_t k = 0; k < m; ++k) {
        std::vector<double> row;
        for (const std::size_t i : free_set) row.push_back(dbl.x[k][i]);
        xf.push_back(std::move(row));
    }
    const NormalEquations nh = normal_double(xf, dbl.y);
    HatchFigures out;
    out.e = scale_exponents(nh.n);
    const CholeskyResult chol = cholesky_double(scaled_double(nh.n, out.e));
    const RMatrix nse = scaled(nff, out.e);
    const RMatrix qse = exact_inverse(nse);
    const Rational q1 = one_norm(qse);
    const JacobiResult ev_a = jacobi_eigs(unit_diagonal(nff));
    const JacobiResult ev_ns = jacobi_eigs(decimals_of(nse));
    Rational dx2;
    for (std::size_t i = 0; i < nf; ++i) dx2 += nff[i][i] * x[i] * x[i];
    const double dx = sqrt_of(dx2);
    Rational y2;
    Rational rp2;
    Rational mag2;
    for (std::size_t k = 0; k < m; ++k) {
        y2 += exact.y[k] * exact.y[k];
        rp2 += rprime[k] * rprime[k];
        Rational row_magnitude;
        for (std::size_t i = 0; i < ne_count; ++i) row_magnitude += exact.x[k][eliminated[i]].abs() * v_exact[i].abs();
        mag2 += row_magnitude * row_magnitude;
    }
    const double r_orig = sqrt_of(y2);
    const double r_mod = sqrt_of(rp2);
    const double mag = sqrt_of(mag2);
    // D_E v from the FULL exact normal matrix diagonal
    Decimal dev2(0);
    for (std::size_t i = 0; i < ne_count; ++i) {
        Rational column2;
        for (std::size_t k = 0; k < m; ++k) column2 += exact.x[k][eliminated[i]] * exact.x[k][eliminated[i]];
        const Decimal d_e = dec(column2).sqrt();
        dev2 = dev2 + (d_e * dec(v_exact[i])).pow(Decimal(2));
    }
    const double de_v = dev2.sqrt().to_double();
    const int nf_int = static_cast<int>(nf);
    const int m_int = static_cast<int>(m);
    const int ne_int = static_cast<int>(ne_count);
    const double tn0 = theta_n(nf_int, m_int, 0.0);
    const double tnt = theta_n(nf_int, m_int, kEtaRep);
    const double sqrt_nf = dk::py_sqrt(static_cast<double>(nf_int));
    const double bterm0 = sqrt_nf * (gamma_k(m_int) * r_mod + gamma_k(2 * ne_int) * (r_orig + mag)) / dx;
    const double bterm_t = sqrt_nf * ((gamma_k(m_int) + kEtaRep) * r_mod + (gamma_k(2 * ne_int) + kEtaRep) * (r_orig + mag)) / dx;
    const double cert_term = dk::py_sqrt(static_cast<double>(nf_int * ne_int)) * kCertRelative * de_v / dx;
    for (const std::size_t i : free_set) out.free_indices.push_back(static_cast<int>(i));
    for (const std::size_t i : eliminated) out.eliminated.push_back(static_cast<int>(i));
    out.n_free = nf_int;
    if (chol.breakdown) out.breakdown_index = chol.breakdown->index;
    out.kappa = (ev_ns.values.back() / ev_ns.values.front()).to_double();
    out.lam_min_a = ev_a.values.front().to_double();
    out.q1 = dec(q1).to_double();
    out.dx = dx;
    out.r_orig = r_orig;
    out.r_mod = r_mod;
    out.mag = mag;
    out.de_v = de_v;
    out.dof = m_int - nf_int;
    out.b_code = 4.0 * dec(q1).to_double() * (tn0 + bterm0);
    out.b_test = 4.0 * dec(q1).to_double() * (tnt + bterm_t + cert_term) + kCertRelative;
    out.digits_exact_vs_cert = digits_vs_certified(x, file.cert, nf);
    out.checksum_reduced = checksum(dbl.x, dbl.y);
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the report

Report build_report(const std::filesystem::path& cache) {
    const dk::LocalContext context(kPrecision);
    Report rep;
    for (const NistEntry& entry : kNist) rep.datasets.push_back(analyse(cache, entry.name));
    rep.rule_u_pontius = predicted_unscaled_rule(cache, "Pontius");
    const EquilibratedRows eq = equilibrated_pontius_rows(cache);
    rep.rule_u_pontius_equilibrated = rule_u_of(eq.x, eq.y, false);
    rep.hatch = hatch_filip(cache);
    rep.demmel = demmel();
    rep.syn_dependent = syn_dependent();
    rep.syn_nearly_dependent = syn_nearly_dependent();
    for (const int n : kTridiagonalSizes) rep.tridiagonal.push_back(tridiagonal(n));
    return rep;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the header

std::string hexf(double x) {
    if (std::isinf(x)) return x > 0 ? "HUGE_VAL" : "-HUGE_VAL";
    return dk::py_float_hex(x);
}

std::string hexf(const std::optional<double>& x) { return x ? hexf(*x) : "0.0"; }

namespace {

// "{" + the values (hexadecimal floats) + "}", padded with 0.0 to `width` places; and the same for integers, padded with 0
std::string arr_hex(const std::vector<double>& v, std::size_t width) {
    std::string out = "{";
    for (std::size_t i = 0; i < std::max(width, v.size()); ++i) out += (i == 0 ? "" : ", ") + (i < v.size() ? hexf(v[i]) : hexf(0.0));
    return out + "}";
}

std::string arr_ints(const std::vector<int>& v, std::size_t width) {
    std::string out = "{";
    for (std::size_t i = 0; i < std::max(width, v.size()); ++i) out += (i == 0 ? "" : ", ") + (i < v.size() ? std::to_string(v[i]) : std::string("0"));
    return out + "}";
}

std::string index_or_minus_one(const std::optional<int>& i) { return std::to_string(i ? *i : -1); }

}  // namespace

std::string build_header(const Report& rep) {
    std::vector<std::string> L = {
        "// GENERATED by tools/estimation_sizing.cpp -- do not edit.",
        "//",
        "// The FROZEN FIGURES of SPEC-estimation 6 (L7 step 2), registered before any code of the estimator existed: the rows' bit checksums, the exact condition numbers, the",
        "// scale exponents, the derived bounds, the predictions of the refusals.  Every double is a hexadecimal float, so the header carries the generator's values exactly.",
        "#pragma once",
        "",
        "#include <cmath>",
        "",
        "namespace odl::estimation::registered {",
        "",
        "inline constexpr int kMaxN = 11;",
        "inline constexpr double kUnitRoundoff = 0x1p-53;",
        "inline constexpr double kEtaRep = " + hexf(rep.eta_rep) + ";              // 2.01 u: a decimal datum rounded to double through long double, doubled for a product",
        "inline constexpr double kCertifiedRelative = " + hexf(rep.cert_relative) + ";   // one unit of the 15th printed digit, worst case",
        "",
        "struct NistFigures {",
        "    const char* name;",
        "    int n, m;",
        "    unsigned long long checksum;      ///< sum of the bit patterns of every design entry and response, in row order, mod 2^64",
        "    int e[kMaxN];                     ///< the scale exponents (EST-R-201)",
        "    double kappa, lam_min_a, lam_max_a, q1, dx_norm, r_norm;",
        "    double theta_n_code, theta_n_test, b_code, b_test, b_q, b_rho, b_sigma, sigma0_exact;",
        "    double b_pair, gap, tau_s, cos_min;   ///< EST-A-209 (two units), EST-A-212 (the weakest direction's angle bound)",
        "    double kappa_i[kMaxN];",
        "    double qdiag[kMaxN];              ///< (N^-1)_ii in the units of the parameters",
        "    double weakest_exact[kMaxN];      ///< the exact eigenvector of the smallest eigenvalue of N_s, largest component positive",
        "    double rho_exact[kMaxN * (kMaxN - 1) / 2];   ///< the exact correlations, row-major upper triangle",
        "    int breakdown_index;              ///< -1: the Cholesky factorisation of the scaled matrix completes",
        "    double breakdown_pivot;",
        "};",
        "",
        "inline constexpr NistFigures kNist[] = {",
    };
    for (const DatasetFigures& d : rep.datasets) {
        L.push_back("    {\"" + d.name + "\", " + std::to_string(d.n) + ", " + std::to_string(d.m) + ", " + std::to_string(d.checksum) + "ULL, " + arr_ints(d.e, 11) + ",");
        L.push_back("     " + hexf(d.kappa) + ", " + hexf(d.lam_min_a) + ", " + hexf(d.lam_max_a) + ", " + hexf(d.q1) + ", " + hexf(d.dx) + ", " + hexf(d.r) + ",");
        L.push_back("     " + hexf(d.theta_n_code) + ", " + hexf(d.theta_n_test) + ", " + hexf(d.b_code) + ", " + hexf(d.b_test) + ", " + hexf(d.b_q) + ", " + hexf(d.b_rho) + ", " + hexf(d.b_sigma) + ", " +
                    hexf(d.sigma0_exact) + ",");
        L.push_back("     " + hexf(d.b_pair) + ", " + hexf(d.gap) + ", " + hexf(d.tau_s) + ", " + hexf(d.cos_min) + ",");
        L.push_back("     " + arr_hex(d.kappa_i, 11) + ",");
        L.push_back("     " + arr_hex(d.qdiag, 11) + ",");
        L.push_back("     " + arr_hex(d.weakest_exact, 11) + ",");
        L.push_back("     " + arr_hex(d.rho_exact, 55) + ",");
        L.push_back("     " + index_or_minus_one(d.breakdown_index) + ", " + hexf(d.breakdown_pivot) + "},");
    }
    L.push_back("};");
    L.push_back("");
    const HatchFigures& ht = rep.hatch;
    L.push_back("/// Filip with B6 .. B10 ELIMINATED at their certified values (EST-A-210).");
    L.push_back("struct HatchFigures { int n_free, dof; int e[kMaxN]; int breakdown_index; double kappa, lam_min_a, q1, dx_norm, r_orig, r_mod, magnitude, d_e_v, b_code, b_test, digits_exact_vs_certified; };");
    L.push_back("inline constexpr HatchFigures kHatchFilip = {" + std::to_string(ht.n_free) + ", " + std::to_string(ht.dof) + ", " + arr_ints(ht.e, 11) + ", " + index_or_minus_one(ht.breakdown_index) + ",");
    L.push_back("    " + hexf(ht.kappa) + ", " + hexf(ht.lam_min_a) + ", " + hexf(ht.q1) + ", " + hexf(ht.dx) + ", " + hexf(ht.r_orig) + ", " + hexf(ht.r_mod) + ", " + hexf(ht.mag) + ", " + hexf(ht.de_v) + ",");
    L.push_back("    " + hexf(ht.b_code) + ", " + hexf(ht.b_test) + ", " + hexf(ht.digits_exact_vs_cert) + "};");
    L.push_back("");
    L.push_back("/// The unscaled decision rule U of EST-A-209: min pivot / max pivot of the Cholesky factor of the UNSCALED normal matrix.");
    L.push_back("inline constexpr double kRuleUPontius = " + hexf(rep.rule_u_pontius.ratio) + ";");
    L.push_back("inline constexpr double kRuleUPontiusEquilibrated = " + hexf(rep.rule_u_pontius_equilibrated.ratio) + ";");
    L.push_back("");
    const DemmelFigures& dm = rep.demmel;
    L.push_back("/// Demmel's worked example (LAPACK Working Note 14, page 4): H = D A D, D = diag(1, 1e5, 1e-10, 1e15), b = (42, -26, 24, 34).");
    L.push_back("struct DemmelFigures { int e[4]; double p[4]; double kappa_a, lam_min_a, lam_max_a, kappa_h, kappa_ns, q1, theta_n, b, b_exact_a, published_bound; };");
    L.push_back("inline constexpr DemmelFigures kDemmel = {" + arr_ints(dm.e, 4) + ", " + arr_hex(dm.p, 4) + ",");
    L.push_back("    " + hexf(dm.kappa_a) + ", " + hexf(dm.lam_min_a) + ", " + hexf(dm.lam_max_a) + ", " + hexf(dm.kappa_h) + ", " + hexf(dm.kappa_ns) + ", " + hexf(dm.q1) + ", " + hexf(dm.theta_n) + ", " +
                hexf(dm.b) + ", " + hexf(dm.b_exact_a) + ", " + hexf(dm.published_bound) + "};");
    L.push_back("");
    const SynDependent& sd = rep.syn_dependent;
    const SynNearlyDependent& sn = rep.syn_nearly_dependent;
    L.push_back("/// SYN-dependent: three parameters, the third a DUPLICATE of the first (EST-A-211).");
    L.push_back("inline constexpr int kSynDependentBreakdownIndex = " + std::to_string(sd.breakdown_index) + ";");
    L.push_back("inline constexpr double kSynDependentBreakdownPivot = " + hexf(sd.breakdown_pivot) + ";");
    L.push_back("/// SYN-nearly-dependent: two parameters whose columns differ by 2^-23 in two entries (EST-A-211).");
    L.push_back("inline constexpr double kSynNearKappa = " + hexf(sn.kappa) + ";");
    L.push_back("inline constexpr double kSynNearLamMin = " + hexf(sn.lam_min_ns) + ";");
    L.push_back("inline constexpr double kSynNearQ1 = " + hexf(sn.q1) + ";");
    L.push_back("inline constexpr double kSynNearB = " + hexf(sn.b) + ";");
    L.push_back("");
    L.push_back("/// The tridiagonal(2, -1) problems of EST-A-203: n, Theta_s(n), lambda_min(A), the per-column bound Theta_s / lambda_min(A).");
    L.push_back("struct TridiagonalFigures { int n; double theta_s, lam_min_a, b_col; };");
    L.push_back("inline constexpr TridiagonalFigures kTridiagonal[] = {");
    for (const Tridiagonal& t : rep.tridiagonal) L.push_back("    {" + std::to_string(t.n) + ", " + hexf(t.theta_s) + ", " + hexf(t.lam_min_a) + ", " + hexf(t.b_col) + "},");
    L.push_back("};");
    L.push_back("");
    L.push_back("}  // namespace odl::estimation::registered");
    L.push_back("");
    return join_lines(L);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the specification's table

std::string sci(double x, int digits) {
    if (x == 0.0) return "0";
    if (std::isinf(x)) return "∞";
    if (std::isnan(x)) throw std::invalid_argument("sci: not a number");
    const std::string s = dk::py_format_e(x, digits - 1);
    const std::size_t epos = s.find('e');
    const int e = std::stoi(s.substr(epos + 1));
    static const char* const kSuperscript[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string sup;
    for (const char c : std::to_string(e)) sup += c == '-' ? "⁻" : kSuperscript[c - '0'];
    return e != 0 ? s.substr(0, epos) + " × 10" + sup : s.substr(0, epos);
}

std::string sci(const std::optional<double>& x, int digits) { return x ? sci(*x, digits) : "0"; }

std::string build_spec_table(const Report& rep) {
    std::vector<std::string> L = {kBegin, "",
         "**The registered figures, per dataset** (NIST StRD linear regression, read from the pinned cache; `m` rows, `n` parameters; the design recipe of §8.2; `u = 2⁻⁵³`). "
         "`e` are the scale exponents of `EST-R-201`; `κ` is the exact `κ₂(N_s)` of the power-of-two-scaled matrix; `κᵢ = Nᵢᵢ (N⁻¹)ᵢᵢ`; `q₁ = ‖Q_s‖₁`; `Θ_N` the forming-and-solving "
         "constant of `EST-R-206` at η = 0; `B`(code) the relative error the solve certifies; `B`(test) the same with the representation error of decimal data and the certified "
         "values' own last digit added; `B_Q` the bound on `|Q̂ᵢᵢ/Qᵢᵢ − 1|`; `B_σ` the bound on `|σ̂₀ − σ₀|`; the last column what the factorisation of the scaled matrix does.", "",
         "| dataset | `n × m` | `e` | `κ` | `min κᵢ` – `max κᵢ` | `q₁` | `Θ_N` | `B` (code) | `B` (test) | `B_Q` | `B_σ` | outcome |",
         "|---|---|---|---|---|---|---|---|---|---|---|---|"};
    for (const DatasetFigures& d : rep.datasets) {
        std::string outcome;
        std::string bc;
        std::string bt;
        std::string bq;
        std::string bs;
        if (d.breakdown_index) {
            outcome = "**EST-F-202 at `B" + std::to_string(*d.breakdown_index) + "`**, pivot " + sci(d.breakdown_pivot, 2);
            bc = bt = bq = bs = "—";
        } else {
            outcome = "completes";
            bc = sci(d.b_code);
            bt = sci(d.b_test);
            bq = sci(d.b_q);
            bs = sci(d.b_sigma);
        }
        const double kmin = *std::min_element(d.kappa_i.begin(), d.kappa_i.end());
        const double kmax = *std::max_element(d.kappa_i.begin(), d.kappa_i.end());
        L.push_back("| " + d.name + " | " + std::to_string(d.n) + " × " + std::to_string(d.m) + " | " + join_ints(d.e) + " | " + sci(d.kappa) + " | " + sci(kmin) + " – " + sci(kmax) + " | " + sci(d.q1) + " | " +
                    sci(d.theta_n_code) + " | " + bc + " | " + bt + " | " + bq + " | " + bs + " | " + outcome + " |");
    }
    const HatchFigures& ht = rep.hatch;
    const std::vector<int> hatch_e(ht.e.begin(), ht.e.begin() + std::min<std::ptrdiff_t>(ht.n_free, static_cast<std::ptrdiff_t>(ht.e.size())));
    L.push_back("");
    L.push_back("**The hatch (`EST-A-210`):** Filip with `B6 … B10` eliminated at their certified values — `n_free` = " + std::to_string(ht.n_free) + ", `e` = " + join_ints(hatch_e) + ", the factorisation completes, "
                "`κ` = " + sci(ht.kappa) + ", `q₁` = " + sci(ht.q1) + ", `B` (code) = " + sci(ht.b_code) + ", `B` (test) = " + sci(ht.b_test) + ", ν = " + std::to_string(ht.dof) + "; the exact reduced solution agrees with the "
                "certified `B0 … B5` to **" + dk::py_format_f(ht.digits_exact_vs_cert, 1) + " digits**.");
    L.push_back("");
    L.push_back("**The unscaled decision `U` (`EST-A-209`)** — the smallest over the largest pivot of the Cholesky factor of the UNSCALED normal matrix, in double: Pontius " + sci(rep.rule_u_pontius.ratio, 3) + "; "
                "the same problem with every design column divided by its own norm " + sci(rep.rule_u_pontius_equilibrated.ratio, 3) + ". `U` refuses below `n·2⁻⁵²` (" + sci(3 * std::ldexp(1.0, -52), 3) + " at n = 3).");
    L.push_back("");
    const DemmelFigures& dm = rep.demmel;
    L.push_back("**Demmel's example (`EST-A-208`):** `e` = " + join_ints(dm.e) + "; **exact** κ₂(A) = " + dk::py_format_f(dm.kappa_a, 4) + " (λ_max = " + dk::py_format_f(dm.lam_max_a, 4) + ", λ_min = " +
                dk::py_format_f(dm.lam_min_a, 4) + ") — the page prints "
                "“κ(A) ≈ 2.0”, which is **not** the 2-norm condition number of the printed A, and the gate does not use it; κ₂(H) = " + sci(dm.kappa_h) + " (the page: ≈ 10⁵⁰), κ₂(N_s) = " + dk::py_format_f(dm.kappa_ns, 4) + ", "
                "`q₁` = " + dk::py_format_f(dm.q1, 4) + ", `Θ_N` = " + sci(dm.theta_n) + " (η_H = 1.002 u for the matrix entries, none for b), `B` = " + sci(dm.b) + " (and " + sci(dm.b_exact_a) +
                " with ‖A⁻¹‖₂ in place of 4 q₁); "
                "the printed bound is " + sci(dm.published_bound, 2) + ".");
    L.push_back("");
    const SynDependent& sd = rep.syn_dependent;
    const SynNearlyDependent& sn = rep.syn_nearly_dependent;
    L.push_back("**Synthetic (`EST-A-211`):** `SYN-dependent` — `e` = " + join_ints(sd.e) + ", the third pivot is exactly " + dk::py_format_g(sd.breakdown_pivot) + ", refusal at index " + std::to_string(sd.breakdown_index) +
                " "
                "with the dependency `c ≈ 1·a + 0·b`; `SYN-nearly-dependent` — `e` = " + join_ints(sn.e) + ", κ = " + sci(sn.kappa) + ", λ_min(N_s) = " + sci(sn.lam_min_ns) + ", "
                "`q₁` = " + sci(sn.q1) + ", `B` = " + dk::py_format_f(sn.b, 3) + " (≥ ½: EST-F-203).");
    L.push_back("");
    L.push_back("**The tridiagonal problems (`EST-A-203`),** rows `eₖ − eₖ₋₁`: `n`, `Θ_s(n)`, `λ_min(A) = (1 − cos(π/(n+1)))`, the per-column bound `B_col = Θ_s / λ_min(A)`:");
    L.push_back("");
    L.push_back("| `n` | `Θ_s` | `λ_min(A)` | `B_col` |");
    L.push_back("|---|---|---|---|");
    for (const Tridiagonal& t : rep.tridiagonal) L.push_back("| " + std::to_string(t.n) + " | " + sci(t.theta_s) + " | " + sci(t.lam_min_a) + " | " + sci(t.b_col) + " |");
    L.push_back("");
    L.push_back(kEnd);
    return join_lines(L);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the report

namespace {

// repr(list of ints), repr(list of floats): "[1, 2]", "[0.5, 1.0]"
std::string repr_ints(const std::vector<int>& v) { return "[" + join_ints(v) + "]"; }

std::string repr_floats(const std::vector<double>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) out += (i == 0 ? "" : ", ") + dk::py_float_repr(v[i]);
    return out + "]";
}

// repr(dict) of a rule U record: {'ratio': ..., 'pivots': [...], 'breakdown': None}
std::string repr_rule_u(const RuleU& r) {
    std::string out = "{'ratio': " + (r.ratio ? dk::py_float_repr(*r.ratio) : std::string("None"));
    if (r.pivots) out += ", 'pivots': " + repr_floats(*r.pivots);
    out += ", 'breakdown': " + (r.breakdown ? std::to_string(*r.breakdown) : std::string("None")) + "}";
    return out;
}

std::string e_fmt(double v, std::size_t width, int precision) { return dk::pad_left(dk::py_format_e(v, precision), width); }
std::string f_fmt(double v, std::size_t width, int precision) { return dk::pad_left(dk::py_format_f(v, precision), width); }

}  // namespace

void print_report(const Report& rep, std::ostream& out) {
    using dk::pad_left;
    using dk::pad_right;
    out << "u = 2^-53 = " << dk::py_format_e(rep.u, 4) << "; eta_rep = " << dk::py_format_e(rep.eta_rep, 4) << "; certified values' own uncertainty = " << dk::py_format_e(rep.cert_relative, 0) << "\n\n";
    out << pad_right("dataset", 9) << ' ' << pad_left("n", 2) << ' ' << pad_left("m", 3) << ' ' << pad_left("kappa(N_s)", 11) << ' ' << pad_left("lam_min(A)", 11) << ' ' << pad_left("||Q_s||_1", 10) << ' '
        << pad_left("Theta_N", 9) << ' ' << pad_left("B (code)", 10) << ' ' << pad_left("B (test)", 10) << ' ' << pad_left("B_Q", 9) << ' ' << pad_left("B_sigma", 9) << "  cert.digits  refusal\n";
    for (const DatasetFigures& d : rep.datasets) {
        std::string ref = "-";
        if (d.breakdown_index) ref = "EST-F-202 at B" + std::to_string(*d.breakdown_index) + " (pivot " + dk::py_format_e(*d.breakdown_pivot, 3) + ")";
        if (!d.breakdown_index && d.certified_no_digit) ref = "EST-F-003";
        out << pad_right(d.name, 9) << ' ' << pad_left(std::to_string(d.n), 2) << ' ' << pad_left(std::to_string(d.m), 3) << ' ' << e_fmt(d.kappa, 11, 4) << ' ' << e_fmt(d.lam_min_a, 11, 4) << ' '
            << e_fmt(d.q1, 10, 3) << ' ' << e_fmt(d.theta_n_code, 9, 2) << ' ' << e_fmt(d.b_code, 10, 3) << ' ' << e_fmt(d.b_test, 10, 3) << ' ' << e_fmt(d.b_q, 9, 2) << ' ' << e_fmt(d.b_sigma, 9, 2) << "  "
            << f_fmt(d.digits_exact_vs_cert, 6, 1) << "      " << ref << '\n';
    }
    out << "\nscale exponents e (EST-R-201) and per-parameter kappa_i = N_ii (N^-1)_ii:\n";
    for (const DatasetFigures& d : rep.datasets) {
        out << "  " << pad_right(d.name, 9) << " e = " << repr_ints(d.e) << "   kappa_i = [";
        for (std::size_t i = 0; i < d.kappa_i.size(); ++i) out << (i == 0 ? "" : ", ") << '\'' << dk::py_format_g(d.kappa_i[i], 4) << '\'';
        out << "]\n";
    }
    out << "\nrule U (the unscaled decision of EST-A-209): ratio of the smallest to the largest Cholesky pivot of the UNSCALED N:\n";
    out << "  " << pad_right("Pontius", 24) << ' ' << repr_rule_u(rep.rule_u_pontius) << '\n';
    out << "  " << pad_right("Pontius_equilibrated", 24) << ' ' << repr_rule_u(rep.rule_u_pontius_equilibrated) << '\n';
    const HatchFigures& ht = rep.hatch;
    out << "\nthe hatch (Filip, B6 .. B10 eliminated at the certified values): e = " << repr_ints(ht.e) << ", breakdown = " << (ht.breakdown_index ? std::to_string(*ht.breakdown_index) : std::string("None"))
        << ", kappa(N_s) = " << dk::py_format_e(ht.kappa, 4) << ", ||Q_s||_1 = " << dk::py_format_e(ht.q1, 3) << ", B(code) = " << dk::py_format_e(ht.b_code, 3) << ", B(test) = "
        << dk::py_format_e(ht.b_test, 3) << ", dof = " << ht.dof << ", exact reduced solution vs certified: " << dk::py_format_f(ht.digits_exact_vs_cert, 1) << " digits\n";
    const DemmelFigures& dm = rep.demmel;
    out << "\nDemmel's example: e = " << repr_ints(dm.e) << ", kappa(A) = " << dk::py_format_f(dm.kappa_a, 4) << " (lambda_max = " << dk::py_format_f(dm.lam_max_a, 4) << "), kappa(H) = "
        << dk::py_format_e(dm.kappa_h, 4) << ", kappa(N_s) = " << dk::py_format_f(dm.kappa_ns, 4) << ", ||Q_s||_1 = " << dk::py_format_e(dm.q1, 4) << ", Theta_N = " << dk::py_format_e(dm.theta_n, 3)
        << ", B = " << dk::py_format_e(dm.b, 3) << " (with 4 ||Q_s||_1) and " << dk::py_format_e(dm.b_exact_a, 3) << " (with ||A^-1||_2), published bound " << dk::py_format_e(dm.published_bound, 1) << '\n';
    const SynDependent& sd = rep.syn_dependent;
    out << "\nSYN-dependent: e = " << repr_ints(sd.e) << ", breakdown at parameter index " << sd.breakdown_index << " with pivot " << dk::py_float_repr(sd.breakdown_pivot) << ", dependency c ~ "
        << repr_floats(sd.dependency) << " (a, b)\n";
    const SynNearlyDependent& sn = rep.syn_nearly_dependent;
    out << "SYN-nearly-dependent: e = " << repr_ints(sn.e) << ", kappa(N_s) = " << dk::py_format_e(sn.kappa, 4) << ", lam_min(N_s) = " << dk::py_format_e(sn.lam_min_ns, 4) << ", ||Q_s||_1 = "
        << dk::py_format_e(sn.q1, 4) << ", Theta_N = " << dk::py_format_e(sn.theta_n, 3) << ", B = " << dk::py_format_f(sn.b, 3) << '\n';
    out << "\ntridiagonal (rows e_k - e_{k-1}): n, Theta_s, lambda_min(A), B_col = Theta_s / lambda_min(A):\n";
    for (const Tridiagonal& t : rep.tridiagonal) {
        out << "  n = " << pad_left(std::to_string(t.n), 3) << "  Theta_s = " << dk::py_format_e(t.theta_s, 3) << "  lam_min(A) = " << dk::py_format_e(t.lam_min_a, 4) << "  B_col = "
            << dk::py_format_e(t.b_col, 3) << '\n';
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------ the tool

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

Settings default_settings() {
    Settings settings;
    settings.cache = default_root() / "data" / "cache";
    return settings;
}

namespace {

const char kUsageText[] = "usage: estimation_sizing [-h] [--cache CACHE] [--report] [--header HEADER] [--verify VERIFY] [--spec-table] [--verify-spec VERIFY_SPEC]\n";

const char kHelpText[] =
    "\n"
    "The FROZEN FIGURES of SPEC-estimation section 6 (L7 step 2): the registered header modules/estimation/tests/registered_figures.hpp and the specification's generated table, from exact arithmetic on\n"
    "the pinned NIST files.  With none of --header, --verify, --spec-table and --verify-spec the figures are printed as a report.\n"
    "\n"
    "options:\n"
    "  -h, --help                show this help and exit\n"
    "  --cache CACHE             the pinned cache (default: data/cache of the tree this tool was built from)\n"
    "  --report                  print the report (the default; no effect)\n"
    "  --header HEADER           write the C++ header of frozen figures\n"
    "  --verify VERIFY           exit 1 unless this committed header is exactly what the generator emits\n"
    "  --spec-table              print the specification's generated table block\n"
    "  --verify-spec VERIFY_SPEC exit 1 unless this specification's generated block is exactly what the generator emits\n"
    "\n"
    "exit codes: 0 done (and, with a --verify option, the committed text is exactly what the generator emits)   1 the committed text differs from the generator's\n"
    "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected   70 an error the tool did not anticipate\n";

// pathlib's str(Path(p)): no empty and no "." components, no trailing slash; "." for what is left of nothing.
std::string path_text(const std::string& given) {
    std::string out;
    const bool absolute = !given.empty() && given.front() == '/';
    std::size_t i = 0;
    while (i <= given.size()) {
        const std::size_t slash = std::min(given.find('/', i), given.size());
        const std::string part = given.substr(i, slash - i);
        if (!part.empty() && part != ".") {
            if (!out.empty() || absolute) out += '/';
            out += part;
        }
        i = slash + 1;
    }
    if (out.empty()) return absolute ? "/" : ".";
    return out;
}

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        std::filesystem::path cache = settings.cache;
        bool spec_table = false;
        std::optional<std::string> header;
        std::optional<std::string> verify;
        std::optional<std::string> verify_spec;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--cache" || opt == "--header" || opt == "--verify" || opt == "--verify-spec") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (opt == "--cache") {
                    cache = value;
                } else if (opt == "--header") {
                    header = value;
                } else if (opt == "--verify") {
                    verify = value;
                } else {
                    verify_spec = value;
                }
            } else if ((opt == "--report" || opt == "--spec-table") && !has_value) {
                if (opt == "--spec-table") spec_table = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        Report rep;
        try {
            rep = settings.make_report ? settings.make_report(cache) : build_report(cache);
        } catch (const BadInput& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
        if (spec_table) {
            io.out << build_spec_table(rep) << '\n';
            return kOk;
        }
        if (verify_spec) {
            std::string doc;
            try {
                doc = dk::universal_newlines(dk::read_text(*verify_spec));
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            const std::size_t i = doc.find(kBegin);
            const std::size_t j = doc.find(kEnd);
            const bool ok = i != std::string::npos && j != std::string::npos && j > i && doc.compare(i, j + std::string_view(kEnd).size() - i, build_spec_table(rep)) == 0;
            io.err << (ok ? "ok       the specification's generated block is exactly what the generator emits"
                          : "MISMATCH the specification's generated block differs from the generator's output (or is missing)")
                   << '\n';
            return ok ? kOk : kFailed;
        }
        if (verify) {
            const std::string text = build_header(rep);
            std::string committed;
            try {
                committed = dk::universal_newlines(dk::read_text(*verify));
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            const bool ok = committed == text;
            io.err << (ok ? "ok       the committed header is exactly what the generator emits"
                          : "MISMATCH the committed header differs from the generator's output (a figure moved: the registration is not what it was)")
                   << '\n';
            return ok ? kOk : kFailed;
        }
        if (header) {
            const std::string text = build_header(rep);
            try {
                dk::write_text(*header, text);
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            io.err << "wrote " << dk::splitlines_py(text).size() << " lines to " << path_text(*header) << '\n';
            return kOk;
        }
        print_report(rep, io.out);
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::estimation_sizing

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::estimation_sizing::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
