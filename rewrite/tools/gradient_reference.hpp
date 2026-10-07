#pragma once
// tools/gradient_reference.hpp — reference tensors for SPEC-gravity's gradient acceptance rows, from the DEFINITION of the potential, by exact polynomial algebra (plan L0 step 8, group C7).
//
// `run` is the whole tool: `gradient_reference [--check] [--header PATH] [--verify PATH]`.  Exit 0 emitted / verified, 1 a value failed its two-precision agreement or the two routes disagree or the
// committed header differs, 2 an argument error or a file that cannot be read or written, 70 an error the tool did not anticipate.
//
// The pieces: the exact polynomial algebra over the rationals (the names of the Python: p_add p_mul p_scale p_der p_eval p_pow), the definition's solid polynomial, the reference tensor of the definition at a
// chosen precision, the same tensor from SPEC-gravity §4.7's formulas, the two-precision guard, and the text of the header.  tools/forcemodel_fd_sizing.cpp and tools/forcemodel_comparator_header.cpp use the
// polynomial algebra as the Python tools imported it; everything is a function of its arguments, so that a test hands the tool a smaller set of cases, a lower precision, or a damaged polynomial.

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/rational.hpp>
#include <odl/devkit/tool.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace odl::tools::gradient_reference {

using odl::devkit::BigInt;
using odl::devkit::Decimal;
using odl::devkit::Rational;

/// The exponents (i, j, k) of x^i y^j z^k.
using Key = std::array<int, 3>;

/// An exact polynomial in x, y, z: the Python's dict {(i, j, k): Fraction}.  Like a dict it KEEPS ITS INSERTION ORDER (a key that is set again keeps its place, a key removed and set again goes to the end), which
/// is what the order of a floating-point sum over its terms is made of in tools/forcemodel_fd_sizing.cpp, so it is kept here.  A term of zero is never stored.
class Poly {
public:
    using Term = std::pair<Key, Rational>;

    Poly() = default;

    [[nodiscard]] bool empty() const noexcept { return terms_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return terms_.size(); }
    [[nodiscard]] const std::vector<Term>& terms() const noexcept { return terms_; }
    /// dict.get(key): the coefficient, or nullptr.
    [[nodiscard]] const Rational* find(const Key& key) const;

    /// `s = out.get(key, 0) + value; if s == 0: out.pop(key, None) else: out[key] = s`: the one operation the algebra builds its results with.
    void accumulate(const Key& key, const Rational& value);
    /// `out[key] = value` (the caller never passes zero).
    void set(const Key& key, const Rational& value);

    /// dict equality: the same keys with the same values, in any order.
    friend bool operator==(const Poly& a, const Poly& b);

private:
    void reindex_from(std::size_t first);

    std::vector<Term> terms_;
    std::map<Key, std::size_t> index_;
};

[[nodiscard]] Poly p_add(const Poly& a, const Poly& b);
[[nodiscard]] Poly p_mul(const Poly& a, const Poly& b);
/// a * c for a number c; the empty polynomial when c is zero.
[[nodiscard]] Poly p_scale(const Poly& a, const Rational& c);
/// The derivative with respect to x (axis 0), y (1) or z (2).
[[nodiscard]] Poly p_der(const Poly& a, int axis);
/// The exact value at a point.
[[nodiscard]] Rational p_eval(const Poly& a, const std::array<Rational, 3>& pt);
/// a to a non-negative power, by repeated multiplication from the constant 1.
[[nodiscard]] Poly p_pow(const Poly& a, int e);

[[nodiscard]] const Poly& poly_x();
[[nodiscard]] const Poly& poly_y();
[[nodiscard]] const Poly& poly_z();
[[nodiscard]] const Poly& poly_one();
/// x^2 + y^2 + z^2.
[[nodiscard]] const Poly& poly_r2();

/// EGM2008's a_e in metres, the value ScalingParameters carries: 63781363/10.
[[nodiscard]] const Rational& earth_radius();

/// The double that float(Fraction) would hand back for the exact value of `v` as a Fraction: Fraction(v), exactly (a finite double only).
[[nodiscard]] Rational rational_from_double(double v);
/// Fraction ** n for a non-negative n.
[[nodiscard]] Rational rational_pow(const Rational& base, int n);

/// N_nm^2 = (2 - delta_0m)(2n+1)(n-m)!/(n+m)!, TN36-6 (6.2b), exactly.
[[nodiscard]] Rational norm_squared(int n, int m);
/// P_n^(m)(t) = sum_e coeff[e] t^e, exactly, from P_n(t) = 2^-n sum_j (-1)^j C(n,j) C(2n-2j,n) t^(n-2j): the pairs (e, coefficient) in the order of j, which is e falling by two from n - m (empty when m > n).
[[nodiscard]] std::vector<std::pair<int, Rational>> legendre_derivative_coeffs(int n, int m);
/// q / N_nm: [ r^(n-m) P_n^(m)(z/r) ] * [ Re or Im (x+iy)^m ] (kind 'C' or 'S'), exactly.  A homogeneous harmonic polynomial of degree n (the empty polynomial for 'S' with m = 0).
[[nodiscard]] Poly solid_polynomial(int n, int m, char kind);

/// A 3 x 3 tensor of decimals.
using Tensor = std::array<std::array<Decimal, 3>, 3>;

/// R_ij = G_ij / (GM / a_e^3) for the potential of the single coefficient (n, m, kind) = 1, at the exact point `pos` (metres), from the definition, in `prec` digits.
[[nodiscard]] Tensor reference_tensor(int n, int m, char kind, const std::array<Rational, 3>& pos, int prec);
/// The same tensor assembled from the formulas of SPEC-gravity §4.7 (the local radial-north-east Hessian from P'_nm, dP'_nm/du, d2P'_nm/du2, rotated), with GM = a_e = 1, in `prec` digits.
[[nodiscard]] Tensor local_formula_tensor(int n, int m, char kind, const std::array<Rational, 3>& pos, int prec);
/// The exact value of the DOUBLE that the C++ test forms as a_e * xi (one rounded multiplication per coordinate).
[[nodiscard]] std::array<Rational, 3> pos_fraction(const std::array<double, 3>& xi);

/// max(abs(t[i][j])) over the nine entries, in the current context (abs() rounds).
[[nodiscard]] Decimal max_abs(const Tensor& t);
/// max over the nine entries of |+a[i][j] - +b[i][j]| / scale: each entry is rounded to the current context before the difference (the comparison of two_precision, which runs it in a local 60 digits).
[[nodiscard]] Decimal worst_relative_difference(const Tensor& a, const Tensor& b, const Decimal& scale);
/// max over the nine entries of |form[i][j] - ref[i][j]|, in the current context (the comparison of --check, which runs it in a local 100 digits).
[[nodiscard]] Decimal largest_difference(const Tensor& form, const Tensor& ref);

/// Thrown by two_precision: its message is the Python's ValueError text.
struct TwoPrecisionError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
/// The tensor at `hi` digits, if it agrees with the one at `lo` digits to 50 digits of the tensor's largest entry; otherwise throws TwoPrecisionError.  Runs in the CURRENT decimal context for its
/// own arithmetic (the Python's default one: 28 digits for the scale, a local 60 for the comparison).
[[nodiscard]] Tensor two_precision(int n, int m, char kind, const std::array<Rational, 3>& pos, int lo = 70, int hi = 110);

/// A coefficient: degree, order and kind ('C' or 'S').
struct Case {
    int n;
    int m;
    char kind;
};

/// The 145 reference points of SPEC-gravity: the 29 coefficients and the five positions (in units of a_e), as the Python listed them.
[[nodiscard]] const std::vector<Case>& default_cases();
[[nodiscard]] const std::vector<std::array<double, 3>>& default_points();

/// What the tool computes.  The precisions are those of the guard (70 and 110 digits) and of the second route (90); a test lowers them to show a refusal.
struct Settings {
    std::vector<Case> cases;
    std::vector<std::array<double, 3>> points;
    int lo = 70;
    int hi = 110;
    int formula = 90;
};
[[nodiscard]] Settings default_settings();

/// One row of the header: the coefficient, the position in metres (the exact double) and the tensor at the higher precision.
struct Row {
    Case c;
    std::array<double, 3> pos;
    Tensor ref;
};
/// The text of the generated header.
[[nodiscard]] std::string header_text(const std::vector<Row>& rows);

/// The tool.  `argv` is its arguments WITHOUT the program name.  `run_on` does the work of `settings`; `run` is `run_on` the real ones.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::gradient_reference
