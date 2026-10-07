// legendre_reference.cpp — reference values for GRAV-A-003, from the DEFINITION.
//
// Plan L0 step 8, group C7, ported to C++ (the user's directive of 2026-10-06) from tools/legendre_reference.py, on the devkit's BigInt, Rational and Decimal.
//
// SPEC-gravity §4.4's recursion is derived from TN36-6 (6.2b) with DLMF 14.6 and 14.10, and GRAV-A-003 checks the double-precision implementation of it against the definition evaluated in at least 50-digit
// arithmetic.  This produces those values.  It must not use the recursion, or the test would compare the implementation with itself.
//
// THE DEFINITION, with no Condon-Shortley phase (GRAV-R-007):
//
//     P_n^m(x) = (1 - x^2)^(m/2) d^m/dx^m P_n(x)
//
//     P_n(x) = 2^-n sum_j (-1)^j C(n,j) C(2n-2j, n) x^(n-2j)
//
// so, differentiating m times and dividing out the (1 - x^2)^(m/2) that SPEC-gravity factors away,
//
//     Pbar'_nm(u) = N_nm 2^-n sum_j (-1)^j C(n,j) C(2n-2j,n) (n-2j)!/(n-2j-m)! u^(n-2j-m)
//
// over j with n - 2j - m >= 0, and N_nm from TN36-6 (6.2b).
//
// WHY THE PRECISION IS SET SO HIGH.  The terms of that sum reach about 2^n/sqrt(pi n) -- at n = 2190, some 10^659 -- and cancel down to a result of order 10.  Roughly 660 digits are destroyed by cancellation
// before the answer appears, so 50 digits of output needs well over 700 of working precision.  Nothing warns you about this: at 50 digits the sum returns confident nonsense.  The guard is that this tool
// computes every value at two precisions and refuses to emit any value whose two evaluations disagree.
//
//   legendre_reference [--header PATH] [--verify PATH]
//   exit 0 emitted / the committed header is what the generator emits   1 a value failed its two-precision agreement check, or the committed header differs   2 an argument error, or a file that cannot be read
//   or written   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's product is a FILE, the generated header modules/gravity/tests/legendre_reference.hpp (7,969 bytes, 113 lines, one version in git since GRAV-A-003).  The port wrote it
// byte for byte, against a substitution list registered before the comparison and of ONE entry (the generator's own name in line 1, `tools/legendre_reference.py` -> `.cpp`).  No committed record held that
// header against its generator before this group (the other three had a ctest that regenerated them); `--verify` is new, as gradient_reference's, and is the ctest
// gravity.legendre_reference_header_matches_its_generator.  (C7_proof_registration.txt, in this group's report files, holds the registration and the results.)
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * THE ORDER OF THE EVALUATION.  The Python formed x ** e for every term (a power to the 2190th at 9,460 digits, a thousand times a value, a hundred values: a long time here, where its arithmetic is a
//     library of numbers-theoretic transforms).  The sum is evaluated here by Horner's rule in x^2 (one multiplication by a number of at most 32 digits for each term), and the integer coefficients by the
//     ratio of two consecutive ones (exact, by the definition of the sum).  The rounding noise of a different order of operations is different noise, at the 1e-8000th place: the values emitted are the
//     60-digit roundings of two evaluations that agree to 50 digits and so do not depend on it, and the comparison with the committed file says whether that is so.  The two-precision guard is the tool's own
//     and unchanged: a test lowers the precision and shows it REFUSE.
//   * The global context's precision, which the Python set to 60 for the rest of the run, is a local context of 60 digits here: nothing after it depends on the global value.
//   * N_nm and 2^n are made once for each precision of a degree and order, not once for each u (they do not depend on u).
//   * `--verify PATH` is new.  The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--opt=value` is taken).  `-h` prints this tool's own text.  A file that is not
//     UTF-8, a directory, a path that cannot be written: REFUSED, exit 2, naming it (the Python died with a traceback).  Text read back is compared as Python's read_text() hands it back: universal newlines.
//     The path of `--header` is echoed as given.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "legendre_reference.hpp"

#include <iostream>
#include <map>
#include <tuple>

namespace dk = odl::devkit;

namespace odl::tools::legendre_reference {

using dk::DecimalContext;
using dk::LocalContext;
using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "legendre_reference";

[[nodiscard]] BigInt factorial(int n) {
    if (n < 0) throw std::invalid_argument("factorial() not defined for negative values");
    BigInt out(1);
    for (int i = 2; i <= n; ++i) out *= BigInt(i);
    return out;
}

/// math.comb(n, k) for 0 <= k <= n: the product of (n - k + i) / i, each step an exact division.
[[nodiscard]] BigInt comb(int n, int k) {
    if (n < 0 || k < 0 || k > n) throw std::invalid_argument("comb(): the arguments are out of range");
    BigInt out(1);
    for (int i = 1; i <= k; ++i) out = out * BigInt(n - k + i) / BigInt(i);
    return out;
}

/// N_nm and 2^n at `precision` digits, each made once (the caller's context is that precision).
struct Scale {
    Decimal norm;
    Decimal two_n;
};

const Scale& scale_of(int n, int m, int precision) {
    thread_local std::map<std::tuple<int, int, int>, Scale> cache;
    const auto key = std::make_tuple(n, m, precision);
    const auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    Scale scale;
    const Rational nsq = norm_squared(n, m);
    scale.norm = (Decimal(nsq.numerator()) / Decimal(nsq.denominator())).sqrt();
    scale.two_n = Decimal(2).pow(Decimal(n));
    return cache.emplace(key, std::move(scale)).first->second;
}

}  // namespace

Rational norm_squared(int n, int m) {
    return Rational(factorial(n - m) * BigInt(2 * n + 1) * BigInt(m == 0 ? 1 : 2), factorial(n + m));
}

std::vector<BigInt> coefficients(int n, int m) {
    std::vector<BigInt> out;
    if (n < 0 || m < 0 || n - m < 0) return out;
    // j = 0:  C(2n, n) n!/(n-m)!;   then each coefficient from the one before it, coeff_(j+1) = -coeff_j (n-2j-m)(n-2j-m-1) / (2 (j+1) (2n-2j-1)),
    // which is exact because both are integers (the ratio of the definition's consecutive terms, worked out from C(n,j+1)/C(n,j), C(2n-2j-2,n)/C(2n-2j,n) and the ratio of the falling factorials)
    BigInt coeff = comb(2 * n, n);
    for (int i = 0; i < m; ++i) coeff *= BigInt(n - i);
    out.push_back(coeff);
    for (int j = 0; n - 2 * (j + 1) - m >= 0; ++j) {
        const std::int64_t divisor = 2 * static_cast<std::int64_t>(j + 1) * (2 * static_cast<std::int64_t>(n) - 2 * j - 1);
        const BigInt::DivMod qr = BigInt::divmod(coeff * BigInt(n - 2 * j - m) * BigInt(n - 2 * j - m - 1), BigInt(divisor));
        if (!qr.remainder.is_zero()) throw std::logic_error("coefficients(): the ratio of two consecutive coefficients was not exact");
        coeff = -qr.quotient;
        out.push_back(coeff);
    }
    return out;
}

Decimal pbar_factored(int n, int m, std::string_view u, int precision) {
    LocalContext ctx(precision);
    const Decimal x = Decimal::from_string(u);
    const Scale& scale = scale_of(n, m, precision);
    const std::vector<BigInt> coeffs = coefficients(n, m);
    // total = sum_j coeff_j x^(n-2j-m), and with y = x^2 and J the last j,  total = x^(n-2J-m) (...((coeff_0 y + coeff_1) y + coeff_2) ... y + coeff_J);  the list is never empty: j = 0 is always there (m <= n)
    const Decimal y = x * x;
    Decimal s(coeffs.front());
    for (std::size_t j = 1; j < coeffs.size(); ++j) s = s * y + Decimal(coeffs[j]);
    const int lowest = n - 2 * static_cast<int>(coeffs.size() - 1) - m;   // the exponent of x that remains: 0 or 1
    const Decimal total = lowest == 0 ? s : s * x;
    return +(scale.norm * total / scale.two_n);
}

Decimal value(int n, int m, std::string_view u, const Precisions& precisions) {
    const int lo = precisions.per_degree * n + precisions.base;
    const int hi = lo + precisions.gap;
    const Decimal a = pbar_factored(n, m, u, lo);
    const Decimal b = pbar_factored(n, m, u, hi);
    LocalContext ctx(60);
    const Decimal a60 = +a;
    const Decimal b60 = +b;
    if (a.is_zero() && b.is_zero()) return Decimal(0);
    if (b60.is_zero() || ((a60 - b60) / b60).abs() > Decimal::from_string("1e-50")) {
        throw RefusedError("Pbar'_" + std::to_string(n) + "," + std::to_string(m) + "(u=" + std::string(u) + ") does not agree between " + std::to_string(lo) + " and " + std::to_string(hi) +
                           " digits: " + a60.to_string() + " vs " + b60.to_string());
    }
    return b60;
}

Settings default_settings() {
    // (n, m) pairs, chosen to span the model: low degrees with closed forms, the sectorial and zonal extremes at full degree, and the interior.
    // sin(phi) values.  0 is the equator; the last is close enough to the pole that the unfactored function has underflowed a double for every order above 176.
    return Settings{{{2, 0}, {2, 1}, {2, 2}, {4, 3}, {10, 7}, {60, 30}, {360, 360}, {1000, 500}, {1500, 1499}, {2190, 0}, {2190, 1}, {2190, 1095}, {2190, 2190}},
                    {"0", "0.3971478906347806", "0.7071067811865476", "0.9396926207859084", "0.9999984769132877", "1"},
                    Precisions{}};
}

namespace {

const char kUsageText[] = "usage: legendre_reference [-h] [--header HEADER] [--verify VERIFY]\n";

const char kHelpText[] =
    "\n"
    "Reference values for GRAV-A-003, from the DEFINITION of the associated Legendre function: Pbar'_nm(u) at 13 (n, m) pairs and six values of sin(phi), each evaluated at two precisions and emitted only if\n"
    "the two agree to 50 significant digits (the header modules/gravity/tests/legendre_reference.hpp).\n"
    "\n"
    "options:\n"
    "  -h, --help       show this help and exit\n"
    "  --header HEADER  write a C++ header here instead of printing\n"
    "  --verify VERIFY  compare this committed header with what the generator emits; exit 1 if they differ\n"
    "\n"
    "exit codes: 0 emitted / the committed header is what the generator emits   1 a value failed its two-precision agreement check, or the committed header differs\n"
    "            2 an argument error, or a file that cannot be read or written\n";

struct Row {
    int n;
    int m;
    std::string u;
    Decimal v;
};

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        std::string header_given;
        std::string verify_given;
        bool has_header = false;
        bool has_verify = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value_text;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value_text = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--header" || opt == "--verify") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value_text = argv[++i];
                }
                if (opt == "--header") {
                    header_given = value_text;
                    has_header = true;
                } else {
                    verify_given = value_text;
                    has_verify = true;
                }
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        // the Python's own state: the default decimal context
        LocalContext fresh;
        fresh.context() = DecimalContext{};

        std::vector<Row> rows;
        for (const auto& [n, m] : settings.points) {
            for (const std::string& u : settings.sines) {
                Decimal v;
                try {
                    v = value(n, m, u, settings.precisions);
                } catch (const RefusedError& exc) {
                    io.err << "REFUSED  " << exc.what() << '\n';
                    return kFailed;
                }
                io.err << "ok       Pbar'_" << n << "," << m << "(u=" << u << ") = " << v.to_string() << '\n';
                rows.push_back(Row{n, m, u, v});
            }
        }

        std::string text;
        const auto line = [&](const std::string& s) {
            text += s;
            text += '\n';
        };
        line("// GENERATED by tools/legendre_reference.cpp — do not edit.");
        line("//");
        line("// Reference values for GRAV-A-003, computed from the DEFINITION of the");
        line("// associated Legendre function in high-precision decimal arithmetic, never");
        line("// from SPEC-gravity §4.4's recursion.  A test whose reference came from the");
        line("// same recursion it is testing would pass for a recursion that is wrong.");
        line("//");
        line("// Each value was evaluated at two precisions and is emitted only if the two");
        line("// agree to 50 significant digits; at degree 2190 about 660 digits are lost to");
        line("// cancellation before the answer appears, and a naive 50-digit evaluation");
        line("// returns confident nonsense rather than failing.");
        line("#pragma once");
        line("");
        line("namespace odl::gravity::reference {");
        line("");
        line("// Most of these values do not fit in a double: max over m of");
        line("// Pbar'_{2190,m}(1) is 10^457.864, which is 10^150 past the largest double.");
        line("// So each row carries BOTH the direct value, where there is one, and the");
        line("// base-10 logarithm of its magnitude with its sign, which there always is.");
        line("// A row with representable = false is checked through the scaled path, not");
        line("// skipped: the values that do not fit are exactly the ones worth checking.");
        line("struct LegendreValue {");
        line("    int n;");
        line("    int m;");
        line("    double sin_phi;");
        line("    bool representable;");
        line("    double pbar_factored;   ///< Pbar_nm(sin phi) / cos^m(phi), or 0 if it does not fit");
        line("    double log10_abs;       ///< log10 |Pbar'_nm|, or 0 when the value is 0");
        line("    double sign;            ///< +1, -1 or 0");
        line("};");
        line("");
        line("inline constexpr LegendreValue kLegendre[] = {");
        for (const Row& row : rows) {
            const std::string head = "    {" + std::to_string(row.n) + ", " + std::to_string(row.m) + ", " + row.u + ", ";
            if (row.v.is_zero()) {
                line(head + "true, 0.0, 0.0, 0.0},");
                continue;
            }
            const std::string sign = row.v > Decimal(0) ? "1" : "-1";
            Decimal l10;
            {
                LocalContext ctx(80);
                l10 = row.v.abs().log10();
            }
            const bool fits = Decimal::from_string("-300") < l10 && l10 < Decimal::from_string("300");
            const std::string direct = fits ? row.v.format(".17e") : "0.0";
            line(head + (fits ? "true" : "false") + ", " + direct + ", " + l10.format(".17f") + ", " + sign + ".0},");
        }
        line("};");
        line("");
        line("}  // namespace odl::gravity::reference");

        if (has_verify) {
            std::string committed;
            try {
                committed = dk::universal_newlines(dk::read_text(verify_given));
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            const bool ok = committed == text;
            io.err << (ok ? "ok       the committed header is exactly what the generator emits" : "MISMATCH the committed header differs from the generator's output") << '\n';
            return ok ? kOk : kFailed;
        }
        if (has_header) {
            try {
                dk::write_text(header_given, text);
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            io.err << "\nwrote " << rows.size() << " values to " << header_given << '\n';
        } else {
            io.out << text << '\n';
        }
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::legendre_reference

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::legendre_reference::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
