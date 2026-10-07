// gradient_reference.cpp — reference tensors for SPEC-gravity's gradient acceptance rows, from the DEFINITION of the potential, by exact polynomial algebra.
//
// Plan L0 step 8, group C7, ported to C++ (the user's directive of 2026-10-06) from tools/gradient_reference.py, on the devkit's BigInt, Rational and Decimal.
//
// WHAT IS BEING CHECKED.  `ConventionalField::gradient_of` returns the Cartesian second-derivative tensor of the potential of ONE normalised coefficient (n, m, C or S, amplitude 1), computed by the recursion
// of SPEC-gravity §4.4 extended to the second derivative (§4.7).  Its reference must not use that recursion, or any formula of §4.7, or the test would compare the implementation with itself.
//
// THE DEFINITION, with no Condon-Shortley phase (GRAV-R-007), in the units GM = a_e = 1 scaled out:
//
//     V_nm = (GM a_e^n / r^(n+1)) Pbar_nm(z/r) [C cos(m lambda) + S sin(m lambda)],
//     Pbar_nm(u) = N_nm (1-u^2)^(m/2) P_n^(m)(u),   N_nm^2 = (2 - delta_0m)(2n+1)(n-m)!/(n+m)!   (TN36-6 (6.2b)).
//
// With (1-u^2)^(m/2) cos(m lambda) = Re (x + i y)^m / r^m this is a POLYNOMIAL over a power of r,
//
//     V_nm = GM a_e^n N_nm  q(x, y, z) / r^(2n+1),
//     q    = [ r^(n-m) P_n^(m)(z/r) ] * [ C Re (x+iy)^m + S Im (x+iy)^m ],
//
// and r^(n-m) P_n^(m)(z/r) = sum_j e_j z^(n-2j-m) (x^2+y^2+z^2)^j is a polynomial because P_n^(m) has the parity of n - m.  q is a homogeneous harmonic polynomial of degree n.  Its Hessian over r^s,
// s = 2n+1, is
//
//     d_ij (q / r^s) = q_ij r^-s - s (q_i x_j + q_j x_i + q delta_ij) r^-(s+2) + s (s+2) q x_i x_j r^-(s+4),
//
// which is exact calculus.  Everything but the final square roots (r, and N_nm) is exact rational arithmetic; those are taken at 70 digits.  The dimensionless reference emitted is R_ij = G_ij / (GM / a_e^3)
// at the point (x, y, z) given in METRES as the exact double the test will pass, so no position rounding enters.
//
// THE SECOND CHECK, `--check`.  The same tensors are assembled from the formulas SPEC-gravity §4.7 prescribes -- the local (radial, north, east) Hessian from P'_nm, dP'_nm/du and d2P'_nm/du2 and the
// Horner-form sums of §4.7 -- in 90-digit arithmetic and compared with the polynomial route.  That is how §4.7's formulas were verified before they were written down, and why the specification can say so.
// The two routes share nothing but the definition.
//
//   gradient_reference [--check] [--header PATH] [--verify PATH]
//   exit 0 emitted / verified   1 a value failed its two-precision agreement, or the two routes disagree, or the committed header differs   2 an argument error, or a file that cannot be read or written
//   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's product is a FILE, the generated header modules/gravity/tests/gradient_reference.hpp (25,539 bytes, 145 tensors, one version in git since L7 step 1).  The port wrote it
// byte for byte, against a substitution list registered before the comparison and of ONE entry (the generator's own name in line 1, `tools/gradient_reference.py` -> `.cpp`); and the check, whose result no file
// holds but SPEC-gravity §4.7 (lines 561-565: "worst relative disagreement 5.9 x 10^-84"), printed the leading digits 5.9 and the exponent -84: a number that is the rounding noise of a chain of decimal
// operations at 90 digits, and that only arithmetic rounding exactly where libmpdec rounds can reproduce.  (C7_proof_registration.txt, in this group's report files, holds the registration and the results.)
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * The exact polynomials are evaluated with integers: a point's coordinates are rationals with a power of two below them (they are doubles), so a polynomial's value is a sum of integers over one common
//     denominator, made once, where the Python made a Fraction of every term.  The values are the same exact rationals.  The polynomials of a coefficient are made once, not once for each of the ten
//     evaluations (five points, two precisions); the Python's dict keeps its insertion order and so does `Poly`, because tools/forcemodel_fd_sizing.cpp sums over it in floating point.
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--opt=value` is taken).  `-h` prints this tool's own text.  A file that is not UTF-8, a directory, a path
//     that cannot be written: REFUSED, exit 2, naming it (the Python died with a traceback, whose exit status 1 is the one it also used for "the header differs").  Text read back is compared as Python's
//     read_text() hands it back: universal newlines.  The path of `--header` is echoed as given (pathlib would have collapsed `./` and `//` in it).

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "gradient_reference.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>
#include <tuple>

namespace dk = odl::devkit;

namespace odl::tools::gradient_reference {

using dk::DecimalContext;
using dk::LocalContext;
using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "gradient_reference";

[[nodiscard]] std::size_t at(int index) { return static_cast<std::size_t>(index); }

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

/// Decimal(f.numerator) / Decimal(f.denominator) in the current context: the numerator and the denominator are exact, the quotient is rounded once.
[[nodiscard]] Decimal to_decimal(const Rational& f) { return Decimal(f.numerator()) / Decimal(f.denominator()); }

/// x ** e of the Python for an int e: the integer power of libmpdec.
[[nodiscard]] Decimal pw(const Decimal& x, std::int64_t e) { return x.pow(Decimal(e)); }

/// What a coefficient's tensors need, made once: q, its three first derivatives and its nine second derivatives.
struct CaseData {
    Poly q;
    std::array<Poly, 3> qi;
    std::array<std::array<Poly, 3>, 3> qij;
};

const CaseData& case_data(int n, int m, char kind) {
    thread_local std::map<std::tuple<int, int, char>, CaseData> cache;
    const auto key = std::make_tuple(n, m, kind);
    const auto found = cache.find(key);
    if (found != cache.end()) return found->second;
    CaseData data;
    data.q = solid_polynomial(n, m, kind);
    for (int a = 0; a < 3; ++a) data.qi[at(a)] = p_der(data.q, a);
    for (int a = 0; a < 3; ++a) {
        for (int b = 0; b < 3; ++b) data.qij[at(a)][at(b)] = p_der(data.qi[at(a)], b);
    }
    return cache.emplace(key, std::move(data)).first->second;
}

}  // namespace

// ============================================================================================================ exact polynomials in x, y, z: {(i, j, k): Fraction}

const Rational* Poly::find(const Key& key) const {
    const auto it = index_.find(key);
    return it == index_.end() ? nullptr : &terms_[it->second].second;
}

void Poly::reindex_from(std::size_t first) {
    for (std::size_t i = first; i < terms_.size(); ++i) index_[terms_[i].first] = i;
}

void Poly::accumulate(const Key& key, const Rational& value) {
    const auto it = index_.find(key);
    const bool present = it != index_.end();
    const Rational sum = present ? terms_[it->second].second + value : value;
    if (sum.is_zero()) {
        if (present) {
            const std::size_t gone = it->second;
            terms_.erase(std::next(terms_.begin(), static_cast<std::ptrdiff_t>(gone)));
            index_.erase(it);
            reindex_from(gone);
        }
    } else if (present) {
        terms_[it->second].second = sum;
    } else {
        index_.emplace(key, terms_.size());
        terms_.emplace_back(key, sum);
    }
}

void Poly::set(const Key& key, const Rational& value) {
    const auto it = index_.find(key);
    if (it != index_.end()) {
        terms_[it->second].second = value;
    } else {
        index_.emplace(key, terms_.size());
        terms_.emplace_back(key, value);
    }
}

bool operator==(const Poly& a, const Poly& b) {
    if (a.size() != b.size()) return false;
    for (const Poly::Term& term : a.terms()) {
        const Rational* other = b.find(term.first);
        if (other == nullptr || !(*other == term.second)) return false;
    }
    return true;
}

Poly p_add(const Poly& a, const Poly& b) {
    Poly out = a;
    for (const Poly::Term& term : b.terms()) out.accumulate(term.first, term.second);
    return out;
}

Poly p_mul(const Poly& a, const Poly& b) {
    Poly out;
    for (const Poly::Term& ta : a.terms()) {
        for (const Poly::Term& tb : b.terms()) {
            const Key key{ta.first[0] + tb.first[0], ta.first[1] + tb.first[1], ta.first[2] + tb.first[2]};
            out.accumulate(key, ta.second * tb.second);
        }
    }
    return out;
}

Poly p_scale(const Poly& a, const Rational& c) {
    Poly out;
    if (c.is_zero()) return out;
    for (const Poly::Term& term : a.terms()) out.set(term.first, term.second * c);
    return out;
}

Poly p_der(const Poly& a, int axis) {
    Poly out;
    for (const Poly::Term& term : a.terms()) {
        if (term.first[at(axis)] > 0) {
            Key key = term.first;
            --key[at(axis)];
            out.set(key, term.second * Rational(term.first[at(axis)]));
        }
    }
    return out;
}

Rational p_eval(const Poly& a, const std::array<Rational, 3>& pt) {
    if (a.empty()) return Rational(0);
    // A point is rational, x_a = N_a / D_a, so a term c x^i y^j z^k is c N^(i,j,k) / D^(i,j,k) and the sum is one integer over one denominator: the powers of each coordinate's numerator and denominator
    // (a power of two, when the point is a double, is a shift) are made once, and every term is the product of its coefficient's numerator, the other coefficients' denominators, and those powers.
    std::array<int, 3> top{0, 0, 0};
    for (const Poly::Term& term : a.terms()) {
        for (std::size_t axis = 0; axis < 3; ++axis) top[axis] = std::max(top[axis], term.first[axis]);
    }
    struct Axis {
        std::vector<BigInt> num;   // N^e, e = 0 .. top
        std::vector<BigInt> den;   // D^e when D is not a power of two (otherwise empty)
        bool dyadic = false;
        std::size_t shift = 0;     // log2(D) when it is
    };
    std::array<Axis, 3> axes;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        Axis& ax = axes[axis];
        const BigInt& n = pt[axis].numerator();
        const BigInt& d = pt[axis].denominator();
        ax.dyadic = d == BigInt(1).shifted_left(d.bit_length() - 1);
        ax.shift = d.bit_length() - 1;
        ax.num.push_back(BigInt(1));
        if (!ax.dyadic) ax.den.push_back(BigInt(1));
        for (int e = 1; e <= top[axis]; ++e) {
            ax.num.push_back(ax.num.back() * n);
            if (!ax.dyadic) ax.den.push_back(ax.den.back() * d);
        }
    }
    BigInt common(1);   // the least common multiple of the coefficients' denominators
    for (const Poly::Term& term : a.terms()) {
        const BigInt& d = term.second.denominator();
        common = common / BigInt::gcd(common, d) * d;
    }
    BigInt total(0);
    for (const Poly::Term& term : a.terms()) {
        BigInt t = term.second.numerator() * (common / term.second.denominator());
        std::size_t shift = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const Axis& ax = axes[axis];
            const int e = term.first[axis];
            t = t * ax.num[at(e)];
            if (ax.dyadic) shift += ax.shift * at(top[axis] - e);
            else t = t * ax.den[at(top[axis] - e)];
        }
        total += t.shifted_left(shift);
    }
    BigInt denominator = common;
    std::size_t shift = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (axes[axis].dyadic) shift += axes[axis].shift * at(top[axis]);
        else denominator = denominator * axes[axis].den[at(top[axis])];
    }
    return Rational(total, denominator.shifted_left(shift));
}

Poly p_pow(const Poly& a, int e) {
    Poly out = poly_one();
    for (int i = 0; i < e; ++i) out = p_mul(out, a);
    return out;
}

const Poly& poly_x() {
    static const Poly p = [] { Poly out; out.set({1, 0, 0}, Rational(1)); return out; }();
    return p;
}
const Poly& poly_y() {
    static const Poly p = [] { Poly out; out.set({0, 1, 0}, Rational(1)); return out; }();
    return p;
}
const Poly& poly_z() {
    static const Poly p = [] { Poly out; out.set({0, 0, 1}, Rational(1)); return out; }();
    return p;
}
const Poly& poly_one() {
    static const Poly p = [] { Poly out; out.set({0, 0, 0}, Rational(1)); return out; }();
    return p;
}
const Poly& poly_r2() {
    static const Poly p = p_add(p_add(p_mul(poly_x(), poly_x()), p_mul(poly_y(), poly_y())), p_mul(poly_z(), poly_z()));
    return p;
}

const Rational& earth_radius() {
    static const Rational ae(BigInt(63781363), BigInt(10));
    return ae;
}

Rational rational_from_double(double v) {
    if (!std::isfinite(v)) throw std::invalid_argument("rational_from_double: not a finite number");
    if (v == 0.0) return Rational(0);
    int exponent = 0;
    const double fraction = std::frexp(v, &exponent);   // v = fraction * 2^exponent, 0.5 <= |fraction| < 1
    const BigInt mantissa(static_cast<std::int64_t>(std::ldexp(fraction, 53)));   // an integer of at most 53 bits, exactly
    const int shift = exponent - 53;                    // v = mantissa * 2^shift
    if (shift >= 0) return Rational(mantissa.shifted_left(at(shift)));
    return Rational(mantissa, BigInt(1).shifted_left(at(-shift)));
}

Rational rational_pow(const Rational& base, int n) {
    Rational out(1);
    for (int i = 0; i < n; ++i) out = out * base;
    return out;
}

// ============================================================================================================ the definition

Rational norm_squared(int n, int m) {
    return Rational(factorial(n - m) * BigInt(2 * n + 1) * BigInt(m == 0 ? 1 : 2), factorial(n + m));
}

std::vector<std::pair<int, Rational>> legendre_derivative_coeffs(int n, int m) {
    std::vector<std::pair<int, Rational>> out;
    for (int j = 0; n - 2 * j - m >= 0; ++j) {
        const int e = n - 2 * j - m;
        BigInt numerator = comb(n, j) * comb(2 * n - 2 * j, n) * factorial(n - 2 * j);
        if (j % 2 == 1) numerator = -numerator;
        out.emplace_back(e, Rational(numerator, factorial(e) * BigInt::pow(BigInt(2), static_cast<unsigned>(n))));
    }
    return out;
}

Poly solid_polynomial(int n, int m, char kind) {
    Poly pi;
    for (const auto& [e, c] : legendre_derivative_coeffs(n, m)) {
        const int j = (n - m - e) / 2;
        pi = p_add(pi, p_scale(p_mul(p_pow(poly_z(), e), p_pow(poly_r2(), j)), c));
    }
    Poly a = poly_one();
    Poly b;
    for (int step = 0; step < m; ++step) {
        Poly next_a = p_add(p_mul(a, poly_x()), p_scale(p_mul(b, poly_y()), Rational(-1)));
        Poly next_b = p_add(p_mul(a, poly_y()), p_mul(b, poly_x()));
        a = std::move(next_a);
        b = std::move(next_b);
    }
    return p_mul(pi, kind == 'C' ? a : b);
}

Tensor reference_tensor(int n, int m, char kind, const std::array<Rational, 3>& pos, int prec) {
    LocalContext ctx(prec);
    const CaseData& data = case_data(n, m, kind);
    std::array<Decimal, 3> xs;
    for (std::size_t a = 0; a < 3; ++a) xs[a] = to_decimal(pos[a]);
    const Rational r2 = pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2];
    const Decimal r = to_decimal(r2).sqrt();
    const std::int64_t s = 2 * n + 1;
    const Decimal qv = to_decimal(p_eval(data.q, pos));
    std::array<Decimal, 3> qiv;
    for (std::size_t a = 0; a < 3; ++a) qiv[a] = to_decimal(p_eval(data.qi[a], pos));
    std::array<std::array<Decimal, 3>, 3> qijv;
    for (std::size_t a = 0; a < 3; ++a) {
        for (std::size_t b = 0; b < 3; ++b) qijv[a][b] = to_decimal(p_eval(data.qij[a][b], pos));
    }
    const Decimal norm = to_decimal(norm_squared(n, m)).sqrt();
    const Decimal scale = norm * to_decimal(rational_pow(earth_radius(), n + 3));
    Tensor out;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            const Decimal f = qijv[i][j] * pw(r, -s)
                              - Decimal(s) * (qiv[i] * xs[j] + qiv[j] * xs[i] + (i == j ? qv : Decimal(0))) * pw(r, -(s + 2))
                              + Decimal(s * (s + 2)) * qv * xs[i] * xs[j] * pw(r, -(s + 4));
            out[i][j] = scale * f;
        }
    }
    return out;
}

// ============================================================================================================ the formulas of SPEC-gravity §4.7, in Decimal, for the second check

Tensor local_formula_tensor(int n, int m, char kind, const std::array<Rational, 3>& pos, int prec) {
    LocalContext ctx(prec);
    std::array<Decimal, 3> xi;
    for (std::size_t a = 0; a < 3; ++a) xi[a] = to_decimal(pos[a] / earth_radius());
    const Decimal rho2 = pw(xi[0], 2) + pw(xi[1], 2);
    const Decimal r = (rho2 + pw(xi[2], 2)).sqrt();
    const Decimal u = xi[2] / r;
    const Decimal c = rho2.sqrt() / r;
    Decimal cl;
    Decimal sl;
    if (rho2 > Decimal(0)) {
        cl = xi[0] / rho2.sqrt();
        sl = xi[1] / rho2.sqrt();
    } else {
        cl = Decimal(1);
        sl = Decimal(0);
    }
    const Decimal norm = to_decimal(norm_squared(n, m)).sqrt();

    // P'_nm = N_nm P_n^(m)(u);  dP'/du = N_nm P_n^(m+1)(u);  d2P'/du2 = N_nm P_n^(m+2)(u)
    const auto poly = [&](int order_extra) {
        Decimal tot(0);
        for (const auto& [e, coef] : legendre_derivative_coeffs(n, m + order_extra)) tot = tot + to_decimal(coef) * (e != 0 ? pw(u, e) : Decimal(1));
        return norm * tot;
    };
    const Decimal p = poly(0);
    const Decimal p1 = poly(1);
    const Decimal p2 = poly(2);
    // cos(m lambda), sin(m lambda) by the exact angle-addition recursion
    Decimal cm(1);
    Decimal sm(0);
    for (int step = 0; step < m; ++step) {
        const Decimal next_cm = cm * cl - sm * sl;
        const Decimal next_sm = sm * cl + cm * sl;
        cm = next_cm;
        sm = next_sm;
    }
    const Decimal cc = kind == 'C' ? Decimal(1) : Decimal(0);
    const Decimal ss = kind == 'C' ? Decimal(0) : Decimal(1);
    const Decimal w = cc * cm + ss * sm;
    const Decimal wp = ss * cm - cc * sm;

    // c^k, with 0^0 = 1 and no negative power ever formed
    const auto cp = [&](int k) {
        if (k < 0) throw std::logic_error("cp: a negative power was asked for");
        return k == 0 ? Decimal(1) : pw(c, k);
    };

    const Decimal md(m);
    const Decimal A0 = cp(m) * p * w;
    const Decimal Bphi = w * (m >= 1 ? (-md * cp(m - 1) * u * p) : Decimal(0)) + w * cp(m + 1) * p1;
    const Decimal Blam = m >= 1 ? (md * cp(m - 1) * p * wp) : Decimal(0);
    const Decimal Cpp = w * (m >= 2 ? ((md * (md - Decimal(1))) * cp(m - 2) * u * u * p) : Decimal(0))
                        + w * (-md * cp(m) * p - (Decimal(2) * md + Decimal(1)) * cp(m) * u * p1 + cp(m + 2) * p2);
    const Decimal Cpl = md * wp * (m >= 2 ? (-(md - Decimal(1)) * cp(m - 2) * u * p) : Decimal(0)) + md * wp * cp(m) * p1;
    const Decimal Cll = w * (-md * cp(m) * p - u * cp(m) * p1) + w * (m >= 2 ? (-(md * (md - Decimal(1))) * cp(m - 2) * p) : Decimal(0));
    const Decimal g = (Decimal(1) / pw(r, 3)) * pw(Decimal(1) / r, n);   // GM / r^3 times (a_e / r)^n, GM = a_e = 1
    const Decimal nd(n);
    const Tensor H{{{g * (nd + Decimal(1)) * (nd + Decimal(2)) * A0, -g * (nd + Decimal(2)) * Bphi, -g * (nd + Decimal(2)) * Blam},
                    {-g * (nd + Decimal(2)) * Bphi, g * (Cpp - (nd + Decimal(1)) * A0), g * Cpl},
                    {-g * (nd + Decimal(2)) * Blam, g * Cpl, g * (Cll - (nd + Decimal(1)) * A0)}}};
    // T: columns e_r, e_north, e_east
    const Tensor T{{{c * cl, -u * cl, -sl}, {c * sl, -u * sl, cl}, {u, c, Decimal(0)}}};
    Tensor out;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            Decimal tot(0);
            for (std::size_t a = 0; a < 3; ++a) {
                for (std::size_t b = 0; b < 3; ++b) tot = tot + T[i][a] * H[a][b] * T[j][b];
            }
            out[i][j] = tot;
        }
    }
    return out;
}

std::array<Rational, 3> pos_fraction(const std::array<double, 3>& xi) {
    const double ae_d = earth_radius().to_double();
    return {rational_from_double(ae_d * xi[0]), rational_from_double(ae_d * xi[1]), rational_from_double(ae_d * xi[2])};
}

// max(abs(t[i][j]) for i in range(3) for j in range(3)) in the current context: abs() rounds, and the value of the largest is what is kept (the starting zero is below every abs())
Decimal max_abs(const Tensor& t) {
    Decimal best(0);
    for (const auto& row : t) {
        for (const Decimal& v : row) {
            const Decimal a = v.abs();
            if (a > best) best = a;
        }
    }
    return best;
}

Decimal worst_relative_difference(const Tensor& a, const Tensor& b, const Decimal& scale) {
    Decimal worst(0);
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            const Decimal relative = (+a[i][j] - +b[i][j]).abs() / scale;
            if (relative > worst) worst = relative;
        }
    }
    return worst;
}

Decimal largest_difference(const Tensor& form, const Tensor& ref) {
    Decimal biggest(0);
    for (std::size_t a = 0; a < 3; ++a) {
        for (std::size_t b = 0; b < 3; ++b) {
            const Decimal gap = (form[a][b] - ref[a][b]).abs();
            if (gap > biggest) biggest = gap;
        }
    }
    return biggest;
}

namespace {

/// "(Fraction(a, b), Fraction(c, d), Fraction(e, f))": repr of the tuple of Fractions.
std::string repr_point(const std::array<Rational, 3>& pos) {
    std::string out = "(";
    for (std::size_t a = 0; a < 3; ++a) {
        if (a != 0) out += ", ";
        out += "Fraction(" + pos[a].numerator().to_decimal() + ", " + pos[a].denominator().to_decimal() + ")";
    }
    return out + ")";
}

/// "(a, b, c)": repr of a tuple of floats.
std::string repr_floats(const std::array<double, 3>& v) { return "(" + dk::py_float_repr(v[0]) + ", " + dk::py_float_repr(v[1]) + ", " + dk::py_float_repr(v[2]) + ")"; }

}  // namespace

Tensor two_precision(int n, int m, char kind, const std::array<Rational, 3>& pos, int lo, int hi) {
    const Tensor a = reference_tensor(n, m, kind, pos, lo);
    const Tensor b = reference_tensor(n, m, kind, pos, hi);
    Decimal scale = max_abs(b);
    if (scale.is_zero()) scale = Decimal(1);
    Decimal worst;
    {
        LocalContext ctx(60);
        worst = worst_relative_difference(a, b, scale);
    }
    if (worst > Decimal::from_string("1e-50")) {
        throw TwoPrecisionError("(" + std::to_string(n) + "," + std::to_string(m) + "," + std::string(1, kind) + ") at " + repr_point(pos) + ": the " + std::to_string(lo) + "- and " + std::to_string(hi) +
                                "-digit tensors differ by " + worst.to_string());
    }
    return b;
}

// ============================================================================================================ the reference points

const std::vector<Case>& default_cases() {
    // Every term to degree 4, then a spread of higher ones: sectorial, near-sectorial, zonal, and interior, so the m-dependent nests (m = 0, 1, >= 2) and the degree factors are all exercised.
    static const std::vector<Case> cases = [] {
        std::vector<Case> out;
        for (int n = 2; n <= 4; ++n) {
            for (int m = 0; m <= n; ++m) {
                for (const char kind : {'C', 'S'}) {
                    if (!(m == 0 && kind == 'S')) out.push_back({n, m, kind});
                }
            }
        }
        for (const Case& c : {Case{6, 3, 'C'}, Case{6, 5, 'S'}, Case{10, 7, 'C'}, Case{10, 10, 'S'}, Case{20, 0, 'C'}, Case{20, 13, 'C'}, Case{36, 17, 'S'}, Case{36, 36, 'C'}}) out.push_back(c);
        return out;
    }();
    return cases;
}

const std::vector<std::array<double, 3>>& default_points() {
    // Positions in units of a_e, as exact binary doubles after multiplication by a_e: the equator on the x axis, a general mid-latitude point, a point in the southern hemisphere in another octant, the pole
    // itself, and a point a whisker off it (where cos(phi) is tiny and any 1/cos(phi) in an implementation would announce itself).
    static const std::vector<std::array<double, 3>> points = {{1.05, 0.0, 0.0}, {0.62, 0.55, 0.71}, {-0.43, 0.81, -0.74}, {0.0, 0.0, 1.07}, {1.0e-3, -2.0e-3, 1.1}};
    return points;
}

Settings default_settings() { return Settings{default_cases(), default_points(), 70, 110, 90}; }

// ============================================================================================================ the header

std::string header_text(const std::vector<Row>& rows) {
    std::string text;
    const auto line = [&](const std::string& s) { text += s + "\n"; };
    line("// GENERATED by tools/gradient_reference.cpp \xE2\x80\x94 do not edit.");
    line("//");
    line("// Reference second-derivative tensors for SPEC-gravity's gradient acceptance rows, computed from the DEFINITION");
    line("// of the potential of ONE normalised coefficient by exact polynomial algebra (a homogeneous harmonic polynomial");
    line("// over a power of r, differentiated exactly), never from \xC2\xA7" "4.4's recursion or \xC2\xA7" "4.7's formulas.  Each tensor was");
    line("// evaluated at 70 and at 110 digits and is emitted only if the two agree to 50.");
    line("//");
    line("// `g` is G_ij / (GM / a_e^3) for the potential of the single coefficient (n, m, kind) = 1 at the position (x, y, z)");
    line("// in metres \xE2\x80\x94 the exact double the test passes \xE2\x80\x94 in the order xx, xy, xz, yy, yz, zz.  The tensor is symmetric");
    line("// by calculus; the test compares all NINE entries of the implementation's, so a pair that disagrees fails.");
    line("#pragma once");
    line("");
    line("namespace odl::gravity::reference {");
    line("");
    line("struct GradientCase {");
    line("    int n;");
    line("    int m;");
    line("    char kind;          ///< 'C' or 'S'");
    line("    double x, y, z;     ///< metres");
    line("    double g[6];        ///< xx, xy, xz, yy, yz, zz of G / (GM / a_e^3)");
    line("};");
    line("");
    line("inline constexpr GradientCase kGradient[] = {");
    for (const Row& row : rows) {
        std::string hexes;
        for (const auto& [i, j] : {std::pair<std::size_t, std::size_t>{0, 0}, {0, 1}, {0, 2}, {1, 1}, {1, 2}, {2, 2}}) {
            if (!hexes.empty()) hexes += ", ";
            hexes += dk::py_float_hex(row.ref[i][j].to_double());
        }
        line("    {" + std::to_string(row.c.n) + ", " + std::to_string(row.c.m) + ", '" + std::string(1, row.c.kind) + "', " + dk::py_float_repr(row.pos[0]) + ", " + dk::py_float_repr(row.pos[1]) + ", " +
             dk::py_float_repr(row.pos[2]) + ", {" + hexes + "}},");
    }
    line("};");
    line("");
    line("}  // namespace odl::gravity::reference");
    return text;
}

// ============================================================================================================ the tool

namespace {

const char kUsageText[] = "usage: gradient_reference [-h] [--check] [--header HEADER] [--verify VERIFY]\n";

const char kHelpText[] =
    "\n"
    "Reference tensors for SPEC-gravity's gradient acceptance rows, from the DEFINITION of the potential, by exact polynomial algebra: the second derivative of the potential of one normalised\n"
    "coefficient (n, m, C or S), at the five reference positions, in 70 and in 110 digits, emitted only where the two agree to 50 (the header modules/gravity/tests/gradient_reference.hpp).\n"
    "\n"
    "options:\n"
    "  -h, --help       show this help and exit\n"
    "  --check          also verify SPEC-gravity §4.7's formulas against the definition (90 digits against 110)\n"
    "  --header HEADER  write a C++ header here instead of printing\n"
    "  --verify VERIFY  compare this committed header with what the generator emits; exit 1 if they differ\n"
    "\n"
    "exit codes: 0 emitted / verified   1 a value failed its two-precision agreement, or the two routes disagree, or the committed header differs\n"
    "            2 an argument error, or a file that cannot be read or written\n";

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        bool check = false;
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
            if (opt == "--header" || opt == "--verify") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (opt == "--header") {
                    header_given = value;
                    has_header = true;
                } else {
                    verify_given = value;
                    has_verify = true;
                }
            } else if (opt == "--check" && !has_value) {
                check = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        // the Python's own state: the default decimal context
        LocalContext fresh;
        fresh.context() = DecimalContext{};

        std::vector<Row> rows;
        Decimal worst_formula(0);
        for (const Case& c : settings.cases) {
            for (const std::array<double, 3>& xi : settings.points) {
                const std::array<Rational, 3> pos = pos_fraction(xi);
                Tensor ref;
                try {
                    ref = two_precision(c.n, c.m, c.kind, pos, settings.lo, settings.hi);
                } catch (const TwoPrecisionError& exc) {
                    io.err << "REFUSED  " << exc.what() << '\n';
                    return kFailed;
                }
                if (check) {
                    const Tensor form = local_formula_tensor(c.n, c.m, c.kind, pos, settings.formula);
                    Decimal diff;
                    {
                        LocalContext ctx(100);
                        Decimal scale = max_abs(ref);
                        if (scale.is_zero()) scale = Decimal(1);
                        diff = largest_difference(form, ref) / scale;
                    }
                    if (diff > worst_formula) worst_formula = diff;
                    if (diff > Decimal::from_string("1e-80")) {
                        io.err << "DISAGREE (" << c.n << "," << c.m << "," << c.kind << ") at " << repr_floats(xi) << ": formulas differ from the definition by " << diff.to_string() << '\n';
                        return kFailed;
                    }
                }
                const double ae_d = earth_radius().to_double();
                rows.push_back(Row{c, {xi[0] * ae_d, xi[1] * ae_d, xi[2] * ae_d}, ref});
            }
        }
        if (check) {
            io.err << "ok       \xC2\xA7" "4.7's formulas agree with the definition over " << rows.size() << " cases; worst relative disagreement " << worst_formula.format(".3E") << " (the formulas at "
                   << settings.formula << " digits, the definition at " << settings.hi << ")\n";
        }

        const std::string text = header_text(rows);
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
            io.err << "wrote " << rows.size() << " tensors to " << header_given << '\n';
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

}  // namespace odl::tools::gradient_reference

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::gradient_reference::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
