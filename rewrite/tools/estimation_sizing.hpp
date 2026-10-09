#pragma once
// tools/estimation_sizing.hpp — the FROZEN FIGURES of SPEC-estimation §6 (L7 step 2: the batch estimator, normal equations scaled by default), as numbers (plan L0 step 8, group C9).
//
// `run` is the whole tool: `estimation_sizing [--cache DIR] [--report] [--header PATH] [--verify PATH] [--spec-table] [--verify-spec PATH]`.  Exit 0 done (and, with --verify / --verify-spec, the committed text is
// exactly what the generator emits), 1 the committed text differs, 2 an argument error or an input file that is missing, cannot be read or is not the shape expected, 70 an error the tool did not anticipate.
//
// What the tool computes, for each dataset the estimator's gate is registered against (NIST StRD's eleven linear regression datasets, read from the PINNED CACHE and never copied into the tree; Demmel's worked
// example; the synthetic cases of SPEC-estimation §8.2): the rows the C++ tests build, as DOUBLES, by the recipe of §8.2 (the decimal parsed in x87 long double, the powers formed in long double by repeated
// multiplication, rounded to double once) with a checksum of their bit patterns; the exact (rational) normal equations, the exact solution, the exact inverse of the SCALED matrix, the eigenvalues of the
// unit-diagonal matrix A and of the scaled matrix (90-digit decimal Jacobi); and the derived bounds of SPEC-estimation §4/§6 with u = 2^-53:
//     gamma_k   = k u / (1 - k u)                                              chi_n = 1 / (1 - (n+1) u)
//     Theta_s   = chi_n [ (3 n^2 + n) u + n^3 u^2 ]                            the solve, scaled, in the 2-norm
//     Theta_N   = n gamma_m (1 + gamma_m) + Theta_s                            plus the forming error of N
//     B         = 4 ||Q_s||_1 ( Theta_N + sqrt(n) gamma_m ||r|| / ||D x|| )    the scaled forward error the solve certifies (code: eta_rep = 0)
// and the test-level versions that add the representation error of decimal data (eta_rep = 2.01 u) and the certified values' own last-digit uncertainty.

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/rational.hpp>
#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::estimation_sizing {

using odl::devkit::Decimal;
using odl::devkit::Rational;

/// An input that is missing, cannot be read or is not the shape this reads (the Python's FileNotFoundError, AttributeError, IndexError, InvalidOperation): exit 2.
struct BadInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// The context precision of every Decimal computation of the tool (the Python's `getcontext().prec = 90`).  The functions that compute in Decimal put it in force themselves (a LocalContext) and leave the caller's alone.
inline constexpr std::int64_t kPrecision = 90;

inline constexpr double kU = 0x1p-53;                  ///< the unit roundoff of IEEE binary64, round to nearest
inline constexpr double kEtaRep = 2.01 * kU;           ///< relative representation error of a decimal datum rounded to double through x87 long double (u + 4e-19, doubled for a product)
inline constexpr double kCertRelative = 1.0e-14;       ///< one unit of the 15th printed digit, worst case (leading digit 1): the certified values' own uncertainty

// ----------------------------------------------------------------------------------------------------------------------------------------- x87 long double, emulated exactly

/// Fraction(float): the double EXACTLY.  Throws BadInput for NaN and the infinities.
[[nodiscard]] Rational exact_rational(double v);
/// Fraction(Decimal): the decimal EXACTLY (coefficient * 10**exponent).  Throws BadInput for an exponent beyond +-100,000 (the Python would run out of memory).
[[nodiscard]] Rational rational_of(const Decimal& d);

/// The nearest value of `bits` significant bits to the exact rational q, ties to even (the Python's _round_sig).  Zero for zero.
[[nodiscard]] Rational round_sig(const Rational& q, unsigned bits);

/// An x87 extended-precision number (a 64-bit significand, round to nearest even): the exact rational value of the nearest such number.  Every result of * and / is the EXACT value of the operation
/// rounded to 64 significant bits, which is what the hardware returns under the default precision control.  No overflow, underflow or denormal handling: the data never come near them.
class Ld {
public:
    /// A decimal string, parsed exactly and rounded once (what a correctly rounded strtold does): "-6.860120914", "1e5".  Throws BadInput for text that is not a decimal number.
    explicit Ld(std::string_view decimal);
    /// A double or a machine integer: exact before the rounding.
    explicit Ld(double value);
    explicit Ld(std::int64_t value);
    explicit Ld(int value) : Ld(static_cast<std::int64_t>(value)) {}

    [[nodiscard]] const Rational& value() const noexcept { return q_; }
    /// float(self): round to nearest even at 53 bits -- the double rounding of `static_cast<double>(long double)`.
    [[nodiscard]] double to_double() const { return q_.to_double(); }

    friend Ld operator*(const Ld& a, const Ld& b);
    friend Ld operator/(const Ld& a, const Ld& b);

private:
    Ld() = default;
    Rational q_;
};

// ----------------------------------------------------------------------------------------------------------------------------------------- the bounds' constants

[[nodiscard]] double gamma_k(int k);                                ///< k u / (1 - k u)   (the Python's gamma; libm has a `gamma` of its own)
[[nodiscard]] double chi_n(int n);                                  ///< 1 / (1 - (n+1) u)
[[nodiscard]] double theta_solve(int n);                            ///< Theta_s
[[nodiscard]] double theta_n(int n, int m, double eta_rep = 0.0);   ///< Theta_N

// ----------------------------------------------------------------------------------------------------------------------------------------- the NIST files

struct NistEntry {
    const char* name;
    int parameters;
    int observations;
};
/// The eleven datasets, in the order the header and the table list them.
inline constexpr NistEntry kNist[] = {
    {"Norris", 2, 36},    {"Pontius", 3, 40},   {"NoInt1", 1, 11},    {"NoInt2", 1, 3},     {"Filip", 11, 82},    {"Longley", 7, 16},
    {"Wampler1", 6, 21},  {"Wampler2", 6, 21},  {"Wampler3", 6, 21},  {"Wampler4", 6, 21},  {"Wampler5", 6, 21},
};
inline constexpr std::size_t kNistCount = sizeof(kNist) / sizeof(kNist[0]);

/// What the tool reads of one file: the certified estimates and their standard deviations (the printed strings), the certified residual standard deviation, and the data rows (the tokens of every non-blank line).
struct NistFile {
    std::vector<std::string> cert, sd;
    std::string rsd;
    std::vector<std::vector<std::string>> rows;
};
/// The file's bytes as the Python read them: latin-1 (every byte one character), universal newlines.  `what` names the file in a refusal.
[[nodiscard]] NistFile parse_nist(std::string_view bytes, const std::string& what);
/// <cache>/nist-strd-lls-<name in lower case>/<name>.dat
[[nodiscard]] NistFile read_nist(const std::filesystem::path& cache, std::string_view name);

using DMatrix = std::vector<std::vector<double>>;
using RMatrix = std::vector<std::vector<Rational>>;
using DecMatrix = std::vector<std::vector<Decimal>>;
using Tokens = std::vector<std::vector<std::string>>;

/// The design matrix and the response of the recipe of SPEC-estimation §8.2, in rationals (the decimal data exactly) ...
struct ExactDesign {
    RMatrix x;
    std::vector<Rational> y;
};
/// ... and as the DOUBLES of the C++ test (long double recurrence, rounded once to double).
struct DoubleDesign {
    DMatrix x;
    std::vector<double> y;
};
[[nodiscard]] ExactDesign design_exact(std::string_view name, const Tokens& rows);
[[nodiscard]] DoubleDesign design_double(std::string_view name, const Tokens& rows);

/// The sum of the bit patterns of every entry of every row, then the response, in row order, modulo 2^64.
[[nodiscard]] std::uint64_t checksum(const DMatrix& x, const std::vector<double>& y);

// ----------------------------------------------------------------------------------------------------------------------------------------- exact linear algebra

/// Decimal(numerator) / Decimal(denominator) in the context in force: the 90-digit decimal of a rational.
[[nodiscard]] Decimal dec(const Rational& q);

/// The solution of M x = b by Gauss-Jordan elimination in rationals (the first non-zero pivot of each column).  Throws BadInput if M is singular.
[[nodiscard]] std::vector<Rational> exact_solve(const RMatrix& m, const std::vector<Rational>& b);
/// The inverse of M, by the same elimination.  Throws BadInput if M is singular.
[[nodiscard]] RMatrix exact_inverse(const RMatrix& m);

/// The eigenvalues of a symmetric matrix, ascending, by cyclic Jacobi rotations in 90-digit decimal arithmetic (at most 100 sweeps, to off-diagonal^2 <= diagonal^2 1e-160), and with `vectors` the
/// eigenvector of each (a vector per eigenvalue, in the same order).  Without them the rotations of the vector matrix are not made (they do not enter the matrix).
struct JacobiResult {
    std::vector<Decimal> values;
    DecMatrix vectors;
};
[[nodiscard]] JacobiResult jacobi_eigs(const DecMatrix& m, bool vectors = false);

/// The rule of SPEC-estimation EST-R-201: d = m 2^k, m in [0.5, 1); e = floor(k / 2); the scaled diagonal d 2^-2e lies in [0.5, 2).
[[nodiscard]] int scale_exponent(double d);

// ----------------------------------------------------------------------------------------------------------------------------------------- the double-precision recipe

/// The estimator's forming (EST-R-203): N-hat_ij = sum_k fl(a_ki a_kj), rows ascending, plain double sums; b-hat likewise.  N is returned full (the upper triangle copied from the lower).
struct NormalEquations {
    DMatrix n;
    std::vector<double> b;
};
[[nodiscard]] NormalEquations normal_double(const DMatrix& x, const std::vector<double>& y);

/// Where Demmel's Algorithm 2.1 stopped: the parameter index and the pivot value that was not positive and finite.
struct Breakdown {
    int index;
    double pivot;
};
/// Demmel's Algorithm 2.1 in plain double (EST-R-204): the lower factor if it completes, else where it broke down.
struct CholeskyResult {
    DMatrix l;
    std::optional<Breakdown> breakdown;
};
[[nodiscard]] CholeskyResult cholesky_double(const DMatrix& n);

// ----------------------------------------------------------------------------------------------------------------------------------------- a dataset's figures

/// What analyse() returns: the Python's dict, member for member (the names lower-cased).
struct DatasetFigures {
    std::string name;
    int n = 0, m = 0;
    std::vector<int> e;                          ///< the scale exponents
    std::uint64_t checksum = 0;
    std::vector<std::string> cert, sd;
    std::string rsd;
    std::optional<int> breakdown_index;
    std::optional<double> breakdown_pivot;
    double kappa = 0, lam_min_ns = 0, lam_max_ns = 0, lam_min_a = 0, q1 = 0;
    std::vector<double> kappa_i;                 ///< N_ii (N^-1)_ii
    double dx = 0, r = 0, digits_exact_vs_cert = 0, sigma0_exact = 0, abs_ax_x = 0;
    double theta_n_code = 0, theta_n_test = 0, lam_max_a = 0;
    std::vector<double> qdiag;                   ///< (N^-1)_ii in the units of the parameters
    double b_code = 0, b_test = 0;
    bool certified_no_digit = false;
    double tau = 0, b_q = 0, b_rho = 0, delta = 0, b_sigma = 0, b_pair = 0;
    std::vector<double> weakest_exact;           ///< the exact eigenvector of the smallest eigenvalue of N_s, its largest component positive
    double gap = 0, tau_s = 0, cos_min = 0;
    std::vector<double> rho_exact;               ///< the exact correlations, row-major upper triangle
};
[[nodiscard]] DatasetFigures analyse(const std::filesystem::path& cache, std::string_view name);

/// The control of EST-A-209: the ratio of the smallest to the largest Cholesky pivot of the UNSCALED normal matrix, in double (the decision rule U); `ratio` is none and `breakdown` the index if it breaks down.
struct RuleU {
    std::optional<double> ratio;
    std::optional<std::vector<double>> pivots;   ///< only the unscaled Pontius case keeps them
    std::optional<int> breakdown;
};
[[nodiscard]] RuleU predicted_unscaled_rule(const std::filesystem::path& cache, std::string_view name);

/// Pontius with every design column divided by its own 2-norm in extended precision, rounded to double: the SAME problem in different units.  The norms are sums of floats, CPython 3.12's compensated sum (py_sum).
struct EquilibratedRows {
    DMatrix x;
    std::vector<double> y, norms;
};
[[nodiscard]] EquilibratedRows equilibrated_pontius_rows(const std::filesystem::path& cache);
/// The rule U of an equilibrated problem: the ratio of the smallest to the largest pivot (the pivots are not kept).
[[nodiscard]] RuleU rule_u_of(const DMatrix& x, const std::vector<double>& y, bool keep_pivots);

// ----------------------------------------------------------------------------------------------------------------------------------------- Demmel's example, the synthetic problems, the hatch

struct DemmelFigures {
    std::vector<int> e;
    std::vector<double> p;
    double kappa_a = 0, kappa_h = 0, kappa_ns = 0, lam_min_a = 0, lam_max_a = 0, q1 = 0, theta_n = 0, b = 0, b_exact_a = 0, published_bound = 0;
    std::vector<double> published_vector, x;
};
[[nodiscard]] DemmelFigures demmel();

struct SynDependent {
    std::vector<int> e;
    DMatrix n;
    int breakdown_index = 0;
    double breakdown_pivot = 0;
    std::vector<double> dependency;
};
[[nodiscard]] SynDependent syn_dependent();

struct SynNearlyDependent {
    std::vector<int> e;
    double kappa = 0, lam_min_ns = 0, q1 = 0, b = 0, theta_n = 0;
};
[[nodiscard]] SynNearlyDependent syn_nearly_dependent();

/// Rows e_k - e_{k-1} (k = 1 .. n+1): N = tridiag(2, -1) exactly; lambda_min = 2 - 2 cos(pi/(n+1)).
struct Tridiagonal {
    int n = 0, m = 0;
    double theta_s = 0, lam_min_a = 0, b_col = 0;
};
[[nodiscard]] Tridiagonal tridiagonal(int n);
/// The n of the registered tridiagonal problems, in order.
inline constexpr int kTridiagonalSizes[] = {1, 2, 5, 10, 20, 40};

/// EST-A-210: Filip, certified yet beyond normal equations in double, with the five highest-order parameters ELIMINATED at their certified values.
struct HatchFigures {
    std::vector<int> free_indices, eliminated;
    int n_free = 0;
    std::vector<int> e;
    std::optional<int> breakdown_index;
    double kappa = 0, lam_min_a = 0, q1 = 0, dx = 0, r_orig = 0, r_mod = 0, mag = 0, de_v = 0;
    int dof = 0;
    double b_code = 0, b_test = 0, digits_exact_vs_cert = 0;
    std::uint64_t checksum_reduced = 0;
};
[[nodiscard]] HatchFigures hatch_filip(const std::filesystem::path& cache);

// ----------------------------------------------------------------------------------------------------------------------------------------- the report, the header, the table

struct Report {
    double u = kU, eta_rep = kEtaRep, cert_relative = kCertRelative;
    std::vector<DatasetFigures> datasets;        ///< in the order of kNist
    RuleU rule_u_pontius, rule_u_pontius_equilibrated;
    HatchFigures hatch;
    DemmelFigures demmel;
    SynDependent syn_dependent;
    SynNearlyDependent syn_nearly_dependent;
    std::vector<Tridiagonal> tridiagonal;        ///< in the order of kTridiagonalSizes
};
[[nodiscard]] Report build_report(const std::filesystem::path& cache);

/// float.hex() of a double, with the two spellings the header uses for an infinity ("HUGE_VAL", "-HUGE_VAL"); "0.0" for none.
[[nodiscard]] std::string hexf(double x);
[[nodiscard]] std::string hexf(const std::optional<double>& x);

/// The text of modules/estimation/tests/registered_figures.hpp, ending in a newline.
[[nodiscard]] std::string build_header(const Report& rep);

/// The specification's generated block, from the BEGIN marker to the END marker (no newline after the END marker).
inline constexpr char kBegin[] = "<!-- BEGIN estimation_sizing: generated by tools/estimation_sizing.cpp --spec-table; do not edit by hand -->";
inline constexpr char kEnd[] = "<!-- END estimation_sizing -->";
[[nodiscard]] std::string build_spec_table(const Report& rep);

/// `x` in three significant digits (`digits`) as the table writes it: "1.23 × 10⁻⁵", "0" for zero (or none), "∞" for an infinity.  Throws std::invalid_argument for a NaN.
[[nodiscard]] std::string sci(double x, int digits = 3);
[[nodiscard]] std::string sci(const std::optional<double>& x, int digits = 3);

/// The default report: the figures in the Python's layout.
void print_report(const Report& rep, std::ostream& out);

// ----------------------------------------------------------------------------------------------------------------------------------------- the tool

struct Settings {
    std::filesystem::path cache;   ///< <the tree>/data/cache unless --cache says otherwise
    /// Makes the report of a cache; EMPTY means build_report.  A test puts a function of its own here, so that the modes (--header, --verify, --spec-table, --verify-spec, the default report) are tested
    /// without the exact arithmetic of the real data.
    std::function<Report(const std::filesystem::path&)> make_report;
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::estimation_sizing
