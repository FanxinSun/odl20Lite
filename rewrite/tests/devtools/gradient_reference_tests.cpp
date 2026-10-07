// tests/devtools/gradient_reference_tests.cpp — the gradient reference tensors of SPEC-gravity, from the definition (plan L0 step 8, group C7): the exact polynomial algebra with the order of its terms, the
// solid polynomials of degrees 2 and 3 by hand and the properties every one of the 29 coefficients must have (homogeneous, harmonic), the tensors against closed forms (derived below and evaluated with GNU
// bc, a host tool used to derive digits and never by the build), their symmetry, trace and homogeneity, the second route against the first, the two-precision guard and its refusal, the text of the header,
// the tool's command line and what it does to a file, and the real tree.  (ctests `gradient_reference.behaviour` and `gradient_reference.real_tree`; the Python tool had no test of its own, so these are
// the cases that show it CAN fail.)
//
// Every expectation is DERIVED BY HAND, from the definition, from the print statements and f-strings of gradient_reference.py, and from closed forms worked out in the comments, and not taken from running
// the port.

#include <catch2/catch_test_macros.hpp>

#include "bignum_printers.hpp"
#include "throwing_stream.hpp"

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/tool.hpp>

#include "gradient_reference.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace gr = odl::tools::gradient_reference;
using namespace odl::devkit;

namespace {

constexpr int kOk = 0, kFailed = 1, kArgument = 2, kInternal = 70;
constexpr const char* kCommittedHeader = "modules/gravity/tests/gradient_reference.hpp";

Rational q(std::int64_t n, std::int64_t d = 1) { return Rational(BigInt(n), BigInt(d)); }
Decimal D(const char* text) { return Decimal::from_string(text); }

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }
bool starts_with(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::size_t from = 0;
    while (from < text.size()) {
        const std::size_t nl = text.find('\n', from);
        if (nl == std::string::npos) {
            out.push_back(text.substr(from));
            break;
        }
        out.push_back(text.substr(from, nl - from));
        from = nl + 1;
    }
    return out;
}

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_on(const gr::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = gr::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_real(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = gr::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

/// two coefficients and the five positions: ten tensors
gr::Settings small_settings() {
    gr::Settings s = gr::default_settings();
    s.cases = {{2, 0, 'C'}, {2, 2, 'S'}};
    return s;
}

/// the terms of a polynomial, in order, as "i j k: coefficient"
std::vector<std::string> terms_of(const gr::Poly& p) {
    std::vector<std::string> out;
    for (const gr::Poly::Term& term : p.terms()) out.push_back(std::to_string(term.first[0]) + " " + std::to_string(term.first[1]) + " " + std::to_string(term.first[2]) + ": " + term.second.to_string());
    return out;
}

gr::Poly poly_of(std::initializer_list<std::pair<gr::Key, Rational>> terms) {
    gr::Poly p;
    for (const auto& [key, value] : terms) p.set(key, value);
    return p;
}

/// |got - want| <= 10^-digits |want| (or, for a zero, 10^-digits), in 300 digits
bool near(const Decimal& got, const Decimal& want, int digits) {
    LocalContext ctx(300);
    const Decimal tol = D(("1e-" + std::to_string(digits)).c_str());
    const Decimal gap = (got - want).abs();
    return want.is_zero() ? gap <= tol : gap <= tol * want.abs();
}

}  // namespace

// ======================================================================================================================================== the polynomial algebra

TEST_CASE("a Poly is the Python's dict: it keeps its insertion order, a key set again keeps its place, a key that sums to zero is removed and comes back at the end", "[gradient_reference][behaviour]") {
    gr::Poly p;
    p.accumulate({1, 0, 0}, q(1));
    p.accumulate({0, 1, 0}, q(2));
    p.accumulate({0, 0, 1}, q(3));
    CHECK(terms_of(p) == (std::vector<std::string>{"1 0 0: 1", "0 1 0: 2", "0 0 1: 3"}));
    p.accumulate({0, 1, 0}, q(5));   // 2 + 5 = 7, where it stood
    CHECK(terms_of(p) == (std::vector<std::string>{"1 0 0: 1", "0 1 0: 7", "0 0 1: 3"}));
    p.accumulate({0, 1, 0}, q(-7));   // 7 - 7 = 0: removed
    CHECK(terms_of(p) == (std::vector<std::string>{"1 0 0: 1", "0 0 1: 3"}));
    CHECK(p.find({0, 1, 0}) == nullptr);
    CHECK(p.size() == 2);
    p.accumulate({0, 1, 0}, q(4));   // and back, at the END
    CHECK(terms_of(p) == (std::vector<std::string>{"1 0 0: 1", "0 0 1: 3", "0 1 0: 4"}));
    p.accumulate({2, 0, 0}, q(0));   // zero added to a key that is not there stores nothing
    CHECK(p.size() == 3);
    CHECK(p.find({2, 0, 0}) == nullptr);
    p.accumulate({1, 0, 0}, q(0));   // zero added to a key that is there changes nothing
    CHECK(terms_of(p) == (std::vector<std::string>{"1 0 0: 1", "0 0 1: 3", "0 1 0: 4"}));
    // set replaces in place and appends a new key
    p.set({0, 0, 1}, q(9));
    p.set({0, 0, 0}, q(-1, 2));
    CHECK(terms_of(p) == (std::vector<std::string>{"1 0 0: 1", "0 0 1: 9", "0 1 0: 4", "0 0 0: -1/2"}));
    // the removal of the first and of the last term leaves the rest in order, and the index follows
    gr::Poly r = poly_of({{{1, 0, 0}, q(1)}, {{0, 1, 0}, q(1)}, {{0, 0, 1}, q(1)}});
    r.accumulate({1, 0, 0}, q(-1));
    CHECK(terms_of(r) == (std::vector<std::string>{"0 1 0: 1", "0 0 1: 1"}));
    CHECK(*r.find({0, 0, 1}) == q(1));
    r.accumulate({0, 0, 1}, q(-1));
    CHECK(terms_of(r) == (std::vector<std::string>{"0 1 0: 1"}));
    r.accumulate({0, 1, 0}, q(-1));
    CHECK(r.empty());
    r.accumulate({0, 1, 0}, q(2));   // an index that has lost every key still works
    CHECK(terms_of(r) == (std::vector<std::string>{"0 1 0: 2"}));
}

TEST_CASE("two Polys are equal as dicts are: the same keys with the same values, in any order", "[gradient_reference][behaviour]") {
    const gr::Poly a = poly_of({{{1, 0, 0}, q(1)}, {{0, 1, 0}, q(2)}});
    const gr::Poly b = poly_of({{{0, 1, 0}, q(2)}, {{1, 0, 0}, q(1)}});
    CHECK(a == b);
    CHECK_FALSE(a == poly_of({{{1, 0, 0}, q(1)}, {{0, 1, 0}, q(3)}}));      // another value
    CHECK_FALSE(a == poly_of({{{1, 0, 0}, q(1)}, {{0, 0, 1}, q(2)}}));      // another key
    CHECK_FALSE(a == poly_of({{{1, 0, 0}, q(1)}}));                         // fewer
    CHECK_FALSE(a == poly_of({{{1, 0, 0}, q(1)}, {{0, 1, 0}, q(2)}, {{0, 0, 1}, q(1)}}));   // more
    CHECK(gr::Poly{} == gr::Poly{});
}

TEST_CASE("p_add, p_mul, p_scale, p_der and p_pow: the algebra, and the order the terms come out in", "[gradient_reference][behaviour]") {
    const gr::Poly& x = gr::poly_x();
    const gr::Poly& y = gr::poly_y();
    const gr::Poly x_plus_y = gr::p_add(x, y);
    CHECK(terms_of(x_plus_y) == (std::vector<std::string>{"1 0 0: 1", "0 1 0: 1"}));
    const gr::Poly x_minus_y = gr::p_add(x, gr::p_scale(y, q(-1)));
    CHECK(terms_of(x_minus_y) == (std::vector<std::string>{"1 0 0: 1", "0 1 0: -1"}));
    // (x + y)(x - y) = x^2 - y^2: the products x*x, x*(-y), y*x, y*(-y) are met in that order, and the two cross terms cancel and are gone
    CHECK(terms_of(gr::p_mul(x_plus_y, x_minus_y)) == (std::vector<std::string>{"2 0 0: 1", "0 2 0: -1"}));
    // p_add leaves its arguments alone, and adds up what is in both: (x + y) + (-x) = y
    const gr::Poly minus_x = gr::p_scale(x, q(-1));
    CHECK(terms_of(gr::p_add(x_plus_y, minus_x)) == (std::vector<std::string>{"0 1 0: 1"}));
    CHECK(terms_of(x_plus_y) == (std::vector<std::string>{"1 0 0: 1", "0 1 0: 1"}));
    // p_scale by zero is the empty polynomial, by anything else multiplies every term and keeps the order
    CHECK(gr::p_scale(x_plus_y, q(0)).empty());
    CHECK(terms_of(gr::p_scale(x_plus_y, q(-3, 2))) == (std::vector<std::string>{"1 0 0: -3/2", "0 1 0: -3/2"}));
    // p_der: x^3 y + 2 z + 7
    const gr::Poly f = poly_of({{{3, 1, 0}, q(1)}, {{0, 0, 1}, q(2)}, {{0, 0, 0}, q(7)}});
    CHECK(terms_of(gr::p_der(f, 0)) == (std::vector<std::string>{"2 1 0: 3"}));
    CHECK(terms_of(gr::p_der(f, 1)) == (std::vector<std::string>{"3 0 0: 1"}));
    CHECK(terms_of(gr::p_der(f, 2)) == (std::vector<std::string>{"0 0 0: 2"}));
    CHECK(gr::p_der(gr::poly_one(), 0).empty());
    // (x + y)^3 = x^3 + 3 x^2 y + 3 x y^2 + y^3, the terms in the order the repeated multiplication meets them: (x+y)^2 is x^2, xy (1 and 1 added), y^2
    CHECK(terms_of(gr::p_pow(x_plus_y, 2)) == (std::vector<std::string>{"2 0 0: 1", "1 1 0: 2", "0 2 0: 1"}));
    CHECK(terms_of(gr::p_pow(x_plus_y, 3)) == (std::vector<std::string>{"3 0 0: 1", "2 1 0: 3", "1 2 0: 3", "0 3 0: 1"}));
    CHECK(gr::p_pow(x_plus_y, 0) == gr::poly_one());
    CHECK(terms_of(gr::p_pow(x_plus_y, 1)) == (std::vector<std::string>{"1 0 0: 1", "0 1 0: 1"}));
    CHECK(gr::p_pow(x_plus_y, -1) == gr::poly_one());   // range(-1) is empty
    // r^2 = x^2 + y^2 + z^2
    CHECK(terms_of(gr::poly_r2()) == (std::vector<std::string>{"2 0 0: 1", "0 2 0: 1", "0 0 2: 1"}));
    CHECK(terms_of(gr::poly_z()) == (std::vector<std::string>{"0 0 1: 1"}));
    CHECK(terms_of(gr::poly_one()) == (std::vector<std::string>{"0 0 0: 1"}));
}

TEST_CASE("p_eval is the exact value at a point: at a dyadic point, at one with denominators of every kind, with zeros and negatives", "[gradient_reference][behaviour]") {
    // x y z + x^2 - 3 at (3/4, -5/8, 1/2): -15/64 + 9/16 - 3 = (-15 + 36 - 192)/64 = -171/64
    const gr::Poly a = poly_of({{{1, 1, 1}, q(1)}, {{2, 0, 0}, q(1)}, {{0, 0, 0}, q(-3)}});
    CHECK(gr::p_eval(a, {q(3, 4), q(-5, 8), q(1, 2)}) == q(-171, 64));
    // 2 x^2 y - z^3/3 + 5 at (1/3, 2/5, -3/7): 4/45 + 9/343 + 5 = (1372 + 405 + 77175)/15435 = 78952/15435
    const gr::Poly b = poly_of({{{2, 1, 0}, q(2)}, {{0, 0, 3}, q(-1, 3)}, {{0, 0, 0}, q(5)}});
    CHECK(gr::p_eval(b, {q(1, 3), q(2, 5), q(-3, 7)}) == q(78952, 15435));
    // a mixture: x dyadic, y not: x^2 y^2 + x y at (1/2, 1/3, 9): 1/36 + 1/6 = 7/36
    const gr::Poly c = poly_of({{{2, 2, 0}, q(1)}, {{1, 1, 0}, q(1)}});
    CHECK(gr::p_eval(c, {q(1, 2), q(1, 3), q(9)}) == q(7, 36));
    // zeros and signs: x^2 + y z^2 - x y at (0, 2, -1) = 0 + 2 - 0 = 2;  and a coordinate of zero to the power of zero is one: 1 + x at (0, 0, 0) = 1
    const gr::Poly d = poly_of({{{2, 0, 0}, q(1)}, {{0, 1, 2}, q(1)}, {{1, 1, 0}, q(-1)}});
    CHECK(gr::p_eval(d, {q(0), q(2), q(-1)}) == q(2));
    CHECK(gr::p_eval(gr::p_add(gr::poly_one(), gr::poly_x()), {q(0), q(0), q(0)}) == q(1));
    // an odd power of a negative number
    CHECK(gr::p_eval(poly_of({{{3, 0, 0}, q(1)}}), {q(-5, 2), q(0), q(0)}) == q(-125, 8));
    // the empty polynomial is zero, and a polynomial whose coefficients have different denominators is summed over their least common multiple: x/4 + y/6 + z/10 at (1, 1, 1) = (15 + 10 + 6)/60
    CHECK(gr::p_eval(gr::Poly{}, {q(1), q(1), q(1)}) == q(0));
    CHECK(gr::p_eval(poly_of({{{1, 0, 0}, q(1, 4)}, {{0, 1, 0}, q(1, 6)}, {{0, 0, 1}, q(1, 10)}}), {q(1), q(1), q(1)}) == q(31, 60));
    // a value that comes out reduced: x^2 at (6/4, 0, 0) is 9/4
    CHECK(gr::p_eval(poly_of({{{2, 0, 0}, q(1)}}), {Rational(BigInt(6), BigInt(4)), q(0), q(0)}) == q(9, 4));
}

// ======================================================================================================================================== the definition

TEST_CASE("N_nm squared, the coefficients of P_n^(m), and the solid polynomials of degrees 2 and 3, by hand", "[gradient_reference][behaviour]") {
    // (2 - delta_0m)(2n+1)(n-m)!/(n+m)!
    CHECK(gr::norm_squared(2, 0) == q(5));          // 1 * 5 * 2!/2!
    CHECK(gr::norm_squared(2, 1) == q(5, 3));       // 2 * 5 * 1!/3!
    CHECK(gr::norm_squared(2, 2) == q(5, 12));      // 2 * 5 * 0!/4!
    CHECK(gr::norm_squared(3, 0) == q(7));
    CHECK(gr::norm_squared(1, 1) == q(3));          // 2 * 3 * 0!/2!
    CHECK(gr::norm_squared(4, 4) == q(1, 2240));    // 2 * 9 / 8!
    // P_n^(m)(t) = sum_e coeff[e] t^e: P_2 = (3t^2 - 1)/2, P_3' = (15t^2 - 3)/2, P_4 = (35t^4 - 30t^2 + 3)/8, P_4'' = (420t^2 - 60)/8, P_4''' = 105 t
    using Coeffs = std::vector<std::pair<int, Rational>>;
    CHECK(gr::legendre_derivative_coeffs(2, 0) == (Coeffs{{2, q(3, 2)}, {0, q(-1, 2)}}));
    CHECK(gr::legendre_derivative_coeffs(3, 1) == (Coeffs{{2, q(15, 2)}, {0, q(-3, 2)}}));
    CHECK(gr::legendre_derivative_coeffs(4, 0) == (Coeffs{{4, q(35, 8)}, {2, q(-15, 4)}, {0, q(3, 8)}}));
    CHECK(gr::legendre_derivative_coeffs(4, 2) == (Coeffs{{2, q(105, 2)}, {0, q(-15, 2)}}));
    CHECK(gr::legendre_derivative_coeffs(4, 3) == (Coeffs{{1, q(105)}}));
    CHECK(gr::legendre_derivative_coeffs(4, 4) == (Coeffs{{0, q(105)}}));   // P_4'''' = 105
    CHECK(gr::legendre_derivative_coeffs(4, 5).empty());                     // m > n: no term
    // what the exact arithmetic refuses: the factorial of a negative number, which an order above the degree asks for (the Python raised ValueError there)
    try {
        (void)gr::norm_squared(2, 3);
        FAIL("N_23 was computed");
    } catch (const std::invalid_argument& exc) {
        CHECK(std::string(exc.what()) == "factorial() not defined for negative values");
    }
    CHECK(gr::legendre_derivative_coeffs(0, 0) == (Coeffs{{0, q(1)}}));
    CHECK(gr::legendre_derivative_coeffs(1, 0) == (Coeffs{{1, q(1)}}));
    // q / N_nm = [ r^(n-m) P_n^(m)(z/r) ] * [ Re or Im (x+iy)^m ]
    CHECK(gr::solid_polynomial(2, 0, 'C') == poly_of({{{0, 0, 2}, q(1)}, {{2, 0, 0}, q(-1, 2)}, {{0, 2, 0}, q(-1, 2)}}));   // (3z^2 - r^2)/2
    CHECK(gr::solid_polynomial(2, 0, 'S').empty());
    CHECK(gr::solid_polynomial(2, 1, 'C') == poly_of({{{1, 0, 1}, q(3)}}));                          // 3 z x
    CHECK(gr::solid_polynomial(2, 1, 'S') == poly_of({{{0, 1, 1}, q(3)}}));                          // 3 z y
    CHECK(gr::solid_polynomial(2, 2, 'C') == poly_of({{{2, 0, 0}, q(3)}, {{0, 2, 0}, q(-3)}}));      // 3 (x^2 - y^2)
    CHECK(gr::solid_polynomial(2, 2, 'S') == poly_of({{{1, 1, 0}, q(6)}}));                          // 3 * 2 x y
    CHECK(gr::solid_polynomial(3, 0, 'C') == poly_of({{{0, 0, 3}, q(1)}, {{2, 0, 1}, q(-3, 2)}, {{0, 2, 1}, q(-3, 2)}}));   // (5z^3 - 3 z r^2)/2
    // (3, 1): r^2 (15 t^2 - 3)/2 with t = z/r is (12 z^2 - 3 x^2 - 3 y^2)/2, times x or y
    CHECK(gr::solid_polynomial(3, 1, 'C') == poly_of({{{1, 0, 2}, q(6)}, {{3, 0, 0}, q(-3, 2)}, {{1, 2, 0}, q(-3, 2)}}));
    CHECK(gr::solid_polynomial(3, 1, 'S') == poly_of({{{0, 1, 2}, q(6)}, {{2, 1, 0}, q(-3, 2)}, {{0, 3, 0}, q(-3, 2)}}));
    // (3, 2): 15 z (x^2 - y^2) and 15 z 2 x y
    CHECK(gr::solid_polynomial(3, 2, 'C') == poly_of({{{2, 0, 1}, q(15)}, {{0, 2, 1}, q(-15)}}));
    CHECK(gr::solid_polynomial(3, 2, 'S') == poly_of({{{1, 1, 1}, q(30)}}));
    // (3, 3): 15 Re (x+iy)^3 = 15 (x^3 - 3 x y^2) and 15 Im = 15 (3 x^2 y - y^3)
    CHECK(gr::solid_polynomial(3, 3, 'C') == poly_of({{{3, 0, 0}, q(15)}, {{1, 2, 0}, q(-45)}}));
    CHECK(gr::solid_polynomial(3, 3, 'S') == poly_of({{{2, 1, 0}, q(45)}, {{0, 3, 0}, q(-15)}}));
}

TEST_CASE("every solid polynomial of the 29 coefficients is homogeneous of degree n and harmonic: its Laplacian is exactly zero", "[gradient_reference][behaviour]") {
    for (const gr::Case& c : gr::default_cases()) {
        INFO("(" << c.n << "," << c.m << "," << c.kind << ")");
        const gr::Poly poly = gr::solid_polynomial(c.n, c.m, c.kind);
        REQUIRE_FALSE(poly.empty());
        for (const gr::Poly::Term& term : poly.terms()) CHECK(term.first[0] + term.first[1] + term.first[2] == c.n);
        gr::Poly laplacian;
        for (int axis = 0; axis < 3; ++axis) laplacian = gr::p_add(laplacian, gr::p_der(gr::p_der(poly, axis), axis));
        CHECK(laplacian.empty());
    }
    CHECK(gr::default_cases().size() == 29);   // every term to degree 4 (5 + 7 + 9 = ... 2 + 3 + 4 degrees, m = 0 with no S) and the eight higher ones
}

TEST_CASE("the 145 reference points are the 29 coefficients and the five positions, as the Python listed them", "[gradient_reference][behaviour]") {
    const std::vector<gr::Case>& cases = gr::default_cases();
    // degree 2: (2,0,C) (2,1,C) (2,1,S) (2,2,C) (2,2,S); degree 3: seven; degree 4: nine; then the eight
    CHECK(cases.size() == 29);
    CHECK(cases[0].n == 2);
    CHECK(cases[0].m == 0);
    CHECK(cases[0].kind == 'C');
    CHECK(cases[1].kind == 'C');
    CHECK(cases[2].kind == 'S');
    CHECK(cases[2].m == 1);
    CHECK(cases[5].n == 3);
    CHECK(cases[5].m == 0);
    CHECK(cases[12].n == 4);
    CHECK(cases[12].m == 0);
    CHECK(cases[20].n == 4);
    CHECK(cases[20].m == 4);
    CHECK(cases[20].kind == 'S');
    const std::vector<std::pair<int, std::pair<int, char>>> tail = {{6, {3, 'C'}}, {6, {5, 'S'}}, {10, {7, 'C'}}, {10, {10, 'S'}}, {20, {0, 'C'}}, {20, {13, 'C'}}, {36, {17, 'S'}}, {36, {36, 'C'}}};
    for (std::size_t k = 0; k < tail.size(); ++k) {
        CHECK(cases[21 + k].n == tail[k].first);
        CHECK(cases[21 + k].m == tail[k].second.first);
        CHECK(cases[21 + k].kind == tail[k].second.second);
    }
    const auto& points = gr::default_points();
    REQUIRE(points.size() == 5);
    CHECK(points[0] == (std::array<double, 3>{1.05, 0.0, 0.0}));
    CHECK(points[3] == (std::array<double, 3>{0.0, 0.0, 1.07}));
    CHECK(points[4] == (std::array<double, 3>{1.0e-3, -2.0e-3, 1.1}));
    CHECK(gr::default_settings().cases.size() * gr::default_settings().points.size() == 145);
}

TEST_CASE("exact rationals from doubles, and the exact positions of the test", "[gradient_reference][behaviour]") {
    CHECK(gr::rational_from_double(0.0) == q(0));
    CHECK(gr::rational_from_double(-0.0) == q(0));
    CHECK(gr::rational_from_double(1.0) == q(1));
    CHECK(gr::rational_from_double(0.5) == q(1, 2));
    CHECK(gr::rational_from_double(-2.75) == q(-11, 4));
    CHECK(gr::rational_from_double(6.0) == q(6));
    CHECK(gr::rational_from_double(1024.0) == q(1024));
    CHECK(gr::rational_from_double(9007199254740992.0) == Rational(BigInt(1).shifted_left(53)));   // 2^53
    CHECK(gr::rational_from_double(1.7976931348623157e308) == Rational((BigInt(1).shifted_left(53) - BigInt(1)).shifted_left(971)));   // the largest double: (2^53 - 1) 2^971
    // 0.1 is 3602879701896397 / 2^55, as everyone knows
    CHECK(gr::rational_from_double(0.1) == Rational(BigInt::from_decimal("3602879701896397"), BigInt(1).shifted_left(55)));
    CHECK(gr::rational_from_double(5e-324) == Rational(BigInt(1), BigInt(1).shifted_left(1074)));   // the smallest subnormal
    CHECK(gr::rational_from_double(2.2250738585072014e-308) == Rational(BigInt(1), BigInt(1).shifted_left(1022)));   // the smallest normal
    for (const double v : {3.141592653589793, -1e-300, 123456.789, 6378136.3, 0.30000000000000004}) CHECK(gr::rational_from_double(v).to_double() == v);
    CHECK_THROWS_AS(gr::rational_from_double(std::numeric_limits<double>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS(gr::rational_from_double(std::nan("")), std::invalid_argument);
    try {
        (void)gr::rational_from_double(std::numeric_limits<double>::infinity());
        FAIL("infinity was taken for a rational");
    } catch (const std::invalid_argument& exc) {
        CHECK(std::string(exc.what()) == "rational_from_double: not a finite number");
    }
    // Fraction ** n
    CHECK(gr::rational_pow(q(3, 2), 4) == q(81, 16));
    CHECK(gr::rational_pow(q(-2, 3), 3) == q(-8, 27));
    CHECK(gr::rational_pow(q(7, 5), 0) == q(1));
    CHECK(gr::rational_pow(q(0), 0) == q(1));
    CHECK(gr::earth_radius() == q(63781363, 10));
    // the position a test passes is the double a_e * xi, one rounded multiplication: the header's coordinates are those doubles
    const std::array<Rational, 3> p1 = gr::pos_fraction({1.05, 0.0, 0.0});
    CHECK(p1[0].to_double() == 6697043.115);
    CHECK(p1[1] == q(0));
    CHECK(p1[2] == q(0));
    const std::array<Rational, 3> p3 = gr::pos_fraction({-0.43, 0.81, -0.74});
    CHECK(p3[0].to_double() == -2742598.6089999997);
    CHECK(p3[1].to_double() == 5166290.403);
    CHECK(p3[2].to_double() == -4719820.862);
    const std::array<Rational, 3> p5 = gr::pos_fraction({1.0e-3, -2.0e-3, 1.1});
    CHECK(p5[0].to_double() == 6378.1363);
    CHECK(p5[1].to_double() == -12756.2726);
    CHECK(p5[2].to_double() == 7015949.930000001);
    // and each is exact: the denominator is a power of two
    for (const Rational& r : p3) CHECK(r.denominator() == BigInt(1).shifted_left(r.denominator().bit_length() - 1));
}

// ======================================================================================================================================== the tensors

TEST_CASE("the definition's tensor against closed forms: (2,0,C) at the pole, (2,2,C) on the equator, (3,0,C) at the pole", "[gradient_reference][behaviour]") {
    // Positions that are exact in binary, so that the closed forms below (evaluated with bc at 130 digits) are exact too:  a = 63781363/10,  z = 218387387/32 = 6824605.84375,  x = 53576345/8 = 6697043.125.
    // V = N q / r^(2n+1) and R = N a^(n+3) Hessian(q / r^(2n+1)).
    //  (2,0,C): q = z^2 - (x^2 + y^2)/2, on the z axis q / r^5 = z^-3, so R_zz = 12 N (a/z)^5 with N = sqrt 5; the tensor is traceless and axially symmetric, so R_xx = R_yy = -R_zz/2, and the rest is 0
    //  (2,2,C): q = 3 (x^2 - y^2), N = sqrt(5/12); on the x axis the second derivatives of q / r^5 are 36, -21, -15 times x^-5 (they sum to zero), and the off-diagonal ones vanish by symmetry
    //  (3,0,C): q = z^3 - 3 z (x^2 + y^2)/2, N = sqrt 7, on the z axis q / r^7 = z^-4: R_zz = 20 N (a/z)^6, R_xx = R_yy = -10 N (a/z)^6
    const std::array<Rational, 3> pole = {q(0), q(0), q(218387387, 32)};
    const std::array<Rational, 3> equator = {q(53576345, 8), q(0), q(0)};
    {
        const gr::Tensor t = gr::reference_tensor(2, 0, 'C', pole, 110);
        CHECK(near(t[2][2], D("19.1314267335747888910477778787388265429186648614971589754236649182457966532579722353160395648487324620393870812809681"), 100));
        CHECK(near(t[0][0], D("-9.5657133667873944455238889393694132714593324307485794877118324591228983266289861176580197824243662310196935406404840"), 100));
        CHECK(near(t[1][1], D("-9.5657133667873944455238889393694132714593324307485794877118324591228983266289861176580197824243662310196935406404840"), 100));
        for (const auto& [i, j] : {std::pair<std::size_t, std::size_t>{0, 1}, {0, 2}, {1, 2}, {1, 0}, {2, 0}, {2, 1}}) CHECK(t[i][j].is_zero());
    }
    {
        const gr::Tensor t = gr::reference_tensor(2, 2, 'C', equator, 110);
        CHECK(near(t[0][0], D("18.2075026283636139397606779074370760484303829971435814882020848679382494711991163789387499516887561707613422493144194"), 100));
        CHECK(near(t[1][1], D("-10.621043199878774798193728779338294361584390081667089201451216172963978858199484554380937471818441099610782978766744"), 100));
        CHECK(near(t[2][2], D("-7.5864594284848391415669491280987816868459929154764922867508686949742706129996318245578124798703150711505592705476747"), 100));
        for (const auto& [i, j] : {std::pair<std::size_t, std::size_t>{0, 1}, {0, 2}, {1, 2}, {1, 0}, {2, 0}, {2, 1}}) CHECK(t[i][j].is_zero());
    }
    {
        const gr::Tensor t = gr::reference_tensor(3, 0, 'C', pole, 110);
        CHECK(near(t[2][2], D("35.2595161603569496193459812760293652526124389813691494704611140195136430939971410173591742497487142320333299359753485"), 100));
        CHECK(near(t[0][0], D("-17.629758080178474809672990638014682626306219490684574735230557009756821546998570508679587124874357116016664967987674"), 100));
        CHECK(near(t[1][1], D("-17.629758080178474809672990638014682626306219490684574735230557009756821546998570508679587124874357116016664967987674"), 100));
    }
    // the S tensor of a zonal term is no tensor at all (the polynomial is empty): the reference is exactly zero
    const gr::Tensor zero = gr::reference_tensor(2, 0, 'S', pole, 110);
    for (const auto& row : zero) {
        for (const Decimal& v : row) CHECK(v.is_zero());
    }
}

TEST_CASE("the definition's tensor is symmetric, traceless (V is harmonic) and homogeneous of degree -(n+3) in the position, to the last digits of the precision", "[gradient_reference][behaviour]") {
    // a few coefficients of every kind at the mid-latitude point and at the point near the pole
    for (const gr::Case& c : {gr::Case{2, 1, 'S'}, gr::Case{3, 2, 'C'}, gr::Case{4, 4, 'S'}, gr::Case{6, 3, 'C'}, gr::Case{10, 7, 'C'}, gr::Case{20, 13, 'C'}}) {
        for (const std::array<double, 3>& xi : {std::array<double, 3>{0.62, 0.55, 0.71}, std::array<double, 3>{1.0e-3, -2.0e-3, 1.1}}) {
            INFO("(" << c.n << "," << c.m << "," << c.kind << ") at (" << xi[0] << ", " << xi[1] << ", " << xi[2] << ")");
            const std::array<Rational, 3> pos = gr::pos_fraction(xi);
            const gr::Tensor t = gr::reference_tensor(c.n, c.m, c.kind, pos, 110);
            LocalContext ctx(200);
            Decimal biggest = t[0][0].abs();
            for (const auto& row : t) {
                for (const Decimal& v : row) biggest = v.abs() > biggest ? v.abs() : biggest;
            }
            REQUIRE(biggest > D("1e-30"));
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = i + 1; j < 3; ++j) CHECK((t[i][j] - t[j][i]).abs() <= biggest * D("1e-100"));
            }
            CHECK((t[0][0] + t[1][1] + t[2][2]).abs() <= biggest * D("1e-100"));
            // twice the position (exact): the tensor falls by 2^(n+3)
            const std::array<Rational, 3> twice = {pos[0] * q(2), pos[1] * q(2), pos[2] * q(2)};
            const gr::Tensor t2 = gr::reference_tensor(c.n, c.m, c.kind, twice, 110);
            Decimal factor(1);
            for (int k = 0; k < c.n + 3; ++k) factor = factor * Decimal(2);
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = 0; j < 3; ++j) CHECK((t2[i][j] * factor - t[i][j]).abs() <= biggest * D("1e-100"));
            }
        }
    }
}

TEST_CASE("the second route, §4.7's formulas, agrees with the definition to the precision of the formulas", "[gradient_reference][behaviour]") {
    for (const gr::Case& c : {gr::Case{2, 0, 'C'}, gr::Case{3, 2, 'S'}, gr::Case{4, 4, 'C'}, gr::Case{6, 5, 'S'}}) {
        for (const std::array<double, 3>& xi : gr::default_points()) {
            INFO("(" << c.n << "," << c.m << "," << c.kind << ") at (" << xi[0] << ", " << xi[1] << ", " << xi[2] << ")");
            const std::array<Rational, 3> pos = gr::pos_fraction(xi);
            const gr::Tensor ref = gr::reference_tensor(c.n, c.m, c.kind, pos, 110);
            const gr::Tensor form = gr::local_formula_tensor(c.n, c.m, c.kind, pos, 90);
            LocalContext ctx(200);
            Decimal biggest = ref[0][0].abs();
            for (const auto& row : ref) {
                for (const Decimal& v : row) biggest = v.abs() > biggest ? v.abs() : biggest;
            }
            if (!biggest.is_zero()) REQUIRE(biggest > D("1e-30"));   // a sectorial term of order 3 and more has no tensor at all on the polar axis: zero in both routes
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = 0; j < 3; ++j) CHECK((form[i][j] - ref[i][j]).abs() <= biggest * D("1e-80"));
            }
        }
    }
    // at 30 digits the formulas are good to about 30 and no better: far from 1e-80 (so the check of the tool CAN fail)
    const std::array<Rational, 3> pos = gr::pos_fraction({0.62, 0.55, 0.71});
    const gr::Tensor ref = gr::reference_tensor(3, 2, 'S', pos, 110);
    const gr::Tensor coarse = gr::local_formula_tensor(3, 2, 'S', pos, 30);
    LocalContext ctx(200);
    Decimal worst(0);
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) worst = (coarse[i][j] - ref[i][j]).abs() > worst ? (coarse[i][j] - ref[i][j]).abs() : worst;
    }
    CHECK(worst > D("1e-60"));
    CHECK(worst < D("1e-25"));
}

TEST_CASE("the two-precision guard: the tensor at the higher precision when the two agree to 50 digits, a refusal that says where when they do not", "[gradient_reference][behaviour]") {
    const std::array<Rational, 3> pos = gr::pos_fraction({0.62, 0.55, 0.71});
    const gr::Tensor high = gr::reference_tensor(3, 2, 'S', pos, 110);
    const gr::Tensor got = gr::two_precision(3, 2, 'S', pos);
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) CHECK(got[i][j] == high[i][j]);   // the one at 110 digits itself, not a rounding of it
    }
    // at 10 and 20 digits the tensors differ by about 1e-10 relative: refused, with the Python's words
    try {
        (void)gr::two_precision(3, 2, 'S', pos, 10, 20);
        FAIL("a tensor evaluated at 10 and at 20 digits was accepted");
    } catch (const gr::TwoPrecisionError& exc) {
        const std::string what = exc.what();
        CHECK(starts_with(what, "(3,2,S) at (Fraction("));
        CHECK(contains(what, "): the 10- and 20-digit tensors differ by "));
        const std::string number = what.substr(what.rfind("differ by ") + 10);
        CHECK(D(number.c_str()) > D("1e-50"));
        CHECK(D(number.c_str()) < D("1e-5"));
    }
    // the tensor of the zero polynomial is zero at both precisions: its scale is zero and the guard takes one
    const gr::Tensor zero = gr::two_precision(2, 0, 'S', pos);
    for (const auto& row : zero) {
        for (const Decimal& v : row) CHECK(v.is_zero());
    }
}

// ======================================================================================================================================== the header

TEST_CASE("the comparisons of the guards look at each of the nine entries: max_abs, worst_relative_difference and largest_difference, entry by entry and in the current context", "[gradient_reference][behaviour]") {
    const Decimal zero = D("0");
    const auto alone = [&](std::size_t i, std::size_t j, const Decimal& v) {
        gr::Tensor t = {{{zero, zero, zero}, {zero, zero, zero}, {zero, zero, zero}}};
        t[i][j] = v;
        return t;
    };
    const gr::Tensor none = alone(0, 0, zero);
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            INFO("entry (" << i << "," << j << ")");
            CHECK(gr::max_abs(alone(i, j, D("-0.25"))) == D("0.25"));
            CHECK(gr::max_abs(alone(i, j, D("7"))) == D("7"));
            CHECK(gr::max_abs(alone(i, j, D("0.00001"))) == D("0.00001"));
            CHECK(gr::worst_relative_difference(alone(i, j, D("3")), none, D("4")) == D("0.75"));
            CHECK(gr::worst_relative_difference(none, alone(i, j, D("-3")), D("4")) == D("0.75"));
            CHECK(gr::largest_difference(alone(i, j, D("-2")), none) == D("2"));
            CHECK(gr::largest_difference(none, alone(i, j, D("5"))) == D("5"));
        }
    }
    // the largest of several, whichever it is, and zero when there is none
    gr::Tensor many = {{{D("1"), D("-9"), D("2")}, {D("3"), D("4"), D("-5")}, {D("6"), D("-7"), D("8")}}};
    CHECK(gr::max_abs(many) == D("9"));
    many[2][2] = D("-10");
    CHECK(gr::max_abs(many) == D("10"));
    CHECK(gr::max_abs(none).is_zero());
    CHECK(gr::worst_relative_difference(many, many, D("10")).is_zero());
    CHECK(gr::largest_difference(many, many).is_zero());
    gr::Tensor other = many;
    other[1][0] = D("3.5");
    other[0][2] = D("-1");
    CHECK(gr::largest_difference(many, other) == D("3"));   // |2 - (-1)| = 3 against |3 - 3.5| = 0.5
    CHECK(gr::worst_relative_difference(many, other, D("6")) == D("0.5"));
    // the relative difference rounds each entry to the context first, the largest difference does not: at five digits 1.000001 and 1.0 are the same number, but their difference is still 1e-6
    const gr::Tensor a = alone(1, 1, D("1.000001"));
    const gr::Tensor b = alone(1, 1, D("1.0"));
    CHECK(gr::worst_relative_difference(a, b, D("1")) == D("0.000001"));
    {
        LocalContext lc(5);
        CHECK(gr::worst_relative_difference(a, b, D("1")).is_zero());
        CHECK(gr::largest_difference(a, b) == D("0.000001"));
        CHECK(gr::largest_difference(alone(2, 0, D("1.00001234")), alone(2, 0, D("1.0"))) == D("0.00001234"));
    }
}

TEST_CASE("the text of the header: its fixed lines, one line a tensor with the doubles of the position and the hexadecimal floats of six entries, and its end", "[gradient_reference][behaviour]") {
    // entries, by hand: 1 = 0x1.0p+0, 1/2 = 0x1.0p-1, -1/4 = -0x1.0p-2, 3 = 0x1.8p+1, 0.1 = 0x1.999999999999ap-4 (the double nearest 0.1), -1 = -0x1.0p+0; and zeros of both signs
    gr::Tensor a = {{{D("1"), D("0.5"), D("-0.25")}, {D("0"), D("3"), D("0.1")}, {D("-0"), D("2"), D("-1")}}};
    gr::Tensor b = {{{D("-0"), D("0"), D("0")}, {D("0"), D("0"), D("0")}, {D("0"), D("0"), D("-0")}}};
    const std::vector<gr::Row> rows = {gr::Row{{2, 0, 'C'}, {1.5, -2.25, 0.0}, a}, gr::Row{{36, 17, 'S'}, {0.0, 6697043.115, -1e-05}, b}};
    const std::string text = gr::header_text(rows);
    const std::vector<std::string> lines = lines_of(text);
    REQUIRE(lines.size() == 23 + 2 + 3);
    CHECK(text.back() == '\n');
    CHECK_FALSE(text.ends_with("\n\n"));
    CHECK(lines[0] == "// GENERATED by tools/gradient_reference.cpp \xE2\x80\x94 do not edit.");
    CHECK(lines[1] == "//");
    CHECK(lines[4] == "// over a power of r, differentiated exactly), never from \xC2\xA7" "4.4's recursion or \xC2\xA7" "4.7's formulas.  Each tensor was");
    CHECK(lines[5] == "// evaluated at 70 and at 110 digits and is emitted only if the two agree to 50.");
    CHECK(lines[8] == "// in metres \xE2\x80\x94 the exact double the test passes \xE2\x80\x94 in the order xx, xy, xz, yy, yz, zz.  The tensor is symmetric");
    CHECK(lines[10] == "#pragma once");
    CHECK(lines[12] == "namespace odl::gravity::reference {");
    CHECK(lines[14] == "struct GradientCase {");
    CHECK(lines[17] == "    char kind;          ///< 'C' or 'S'");
    CHECK(lines[18] == "    double x, y, z;     ///< metres");
    CHECK(lines[19] == "    double g[6];        ///< xx, xy, xz, yy, yz, zz of G / (GM / a_e^3)");
    CHECK(lines[22] == "inline constexpr GradientCase kGradient[] = {");
    CHECK(lines[23] == "    {2, 0, 'C', 1.5, -2.25, 0.0, {0x1.0000000000000p+0, 0x1.0000000000000p-1, -0x1.0000000000000p-2, 0x1.8000000000000p+1, 0x1.999999999999ap-4, -0x1.0000000000000p+0}},");
    CHECK(lines[24] == "    {36, 17, 'S', 0.0, 6697043.115, -1e-05, {-0x0.0p+0, 0x0.0p+0, 0x0.0p+0, 0x0.0p+0, 0x0.0p+0, -0x0.0p+0}},");
    CHECK(lines[25] == "};");
    CHECK(lines[26] == "");
    CHECK(lines[27] == "}  // namespace odl::gravity::reference");
}

// ======================================================================================================================================== the tool

TEST_CASE("the tool with no flag prints the header and one more newline; --header writes it, says so on standard error and prints nothing; the same run twice is the same", "[gradient_reference][behaviour]") {
    const gr::Settings settings = small_settings();
    const Result printed = run_on(settings, {});
    REQUIRE(printed.code == kOk);
    CHECK(printed.err.empty());
    CHECK(printed.out.back() == '\n');
    CHECK(printed.out.ends_with("}  // namespace odl::gravity::reference\n\n"));   // print(text): the text's own newline and print's
    const std::vector<std::string> lines = lines_of(printed.out);
    CHECK(lines.size() == 23 + 10 + 3 + 1);
    CHECK(lines[23].starts_with("    {2, 0, 'C', 6697043.115, 0.0, 0.0, {"));   // the first of the ten rows: the first coefficient at the first position
    CHECK(lines[27].starts_with("    {2, 0, 'C', 6378.1363, -12756.2726, 7015949.930000001, {"));
    CHECK(lines[28].starts_with("    {2, 2, 'S', 6697043.115, 0.0, 0.0, {"));
    CHECK(run_on(settings, {}).out == printed.out);

    TempDir dir("odl-grad");
    const std::string file = (dir.path() / "out.hpp").string();
    const Result written = run_on(settings, {"--header", file});
    CHECK(written.code == kOk);
    CHECK(written.out.empty());
    CHECK(written.err == "wrote 10 tensors to " + file + "\n");
    CHECK(read_text(file) + "\n" == printed.out);
    // --header=PATH is taken as well
    const std::string second = (dir.path() / "second.hpp").string();
    CHECK(run_on(settings, {"--header=" + second}).code == kOk);
    CHECK(read_text(second) == read_text(file));
    // a path that cannot be written: refused, naming it
    const Result unwritable = run_on(settings, {"--header", (dir.path() / "no" / "such" / "dir.hpp").string()});
    CHECK(unwritable.code == kArgument);
    CHECK(contains(unwritable.err, "gradient_reference: "));
    CHECK(contains(unwritable.err, "dir.hpp"));
    CHECK(unwritable.out.empty());
}

TEST_CASE("--verify says whether a committed header is exactly what the generator emits, and wins over --header", "[gradient_reference][behaviour]") {
    const gr::Settings settings = small_settings();
    TempDir dir("odl-grad");
    const std::string file = (dir.path() / "committed.hpp").string();
    REQUIRE(run_on(settings, {"--header", file}).code == kOk);
    const Result same = run_on(settings, {"--verify", file});
    CHECK(same.code == kOk);
    CHECK(same.err == "ok       the committed header is exactly what the generator emits\n");
    CHECK(same.out.empty());
    // one character changed
    std::string text = read_text(file);
    text[text.find("kGradient") + 1] = 'X';
    write_text(file, text);
    const Result different = run_on(settings, {"--verify", file});
    CHECK(different.code == kFailed);
    CHECK(different.err == "MISMATCH the committed header differs from the generator's output\n");
    // a file with Windows line ends is read as Python's read_text reads it (universal newlines): the same header
    REQUIRE(run_on(settings, {"--header", file}).code == kOk);
    std::string with_crlf;
    for (const char ch : read_text(file)) {
        if (ch == '\n') with_crlf += '\r';
        with_crlf += ch;
    }
    write_text(file, with_crlf);
    CHECK(run_on(settings, {"--verify", file}).code == kOk);
    // --verify wins over --header: nothing is written
    const std::string other = (dir.path() / "other.hpp").string();
    CHECK(run_on(settings, {"--header", other, "--verify", file}).code == kOk);
    CHECK_FALSE(fs::exists(other));
    // a file that is not there, a directory, a file that is not UTF-8: refused, naming it
    const Result missing = run_on(settings, {"--verify", (dir.path() / "nowhere.hpp").string()});
    CHECK(missing.code == kArgument);
    CHECK(contains(missing.err, "nowhere.hpp"));
    const Result directory = run_on(settings, {"--verify", dir.path().string()});
    CHECK(directory.code == kArgument);
    write_text(file, "\xE9 not utf-8");
    const Result latin = run_on(settings, {"--verify", file});
    CHECK(latin.code == kArgument);
    CHECK(contains(latin.err, "committed.hpp"));
}

TEST_CASE("--check compares §4.7's formulas with the definition and says how well they agree; a formula at too low a precision is DISAGREE", "[gradient_reference][behaviour]") {
    const gr::Settings settings = small_settings();
    const Result checked = run_on(settings, {"--check"});
    CHECK(checked.code == kOk);
    CHECK(checked.out.find("kGradient") != std::string::npos);
    const std::string head = "ok       \xC2\xA7" "4.7's formulas agree with the definition over 10 cases; worst relative disagreement ";
    const std::size_t at = checked.err.find(head);
    REQUIRE(at != std::string::npos);
    CHECK(at == 0);
    const std::string tail = checked.err.substr(head.size());
    // "d.dddE-NN (the formulas at 90 digits, the definition at 110)\n": a mantissa of four digits and an exponent no larger than -80 (the threshold of the check itself)
    CHECK(tail.size() > 8);
    CHECK(tail[1] == '.');
    CHECK(tail.substr(5, 2) == "E-");
    const std::size_t space = tail.find(' ');
    REQUIRE(space != std::string::npos);
    CHECK(D(tail.substr(0, space).c_str()) < D("1E-80"));
    CHECK(tail.substr(space) == " (the formulas at 90 digits, the definition at 110)\n");
    // the default check is silent on standard error without --check
    CHECK(run_on(settings, {}).err.empty());

    // the formulas at 30 digits do not agree with the definition to 1e-80: DISAGREE, naming the case, the position as the tuple of floats, and by how much; exit 1 and nothing printed
    gr::Settings coarse = settings;
    coarse.formula = 30;
    const Result disagree = run_on(coarse, {"--check"});
    CHECK(disagree.code == kFailed);
    CHECK(disagree.out.empty());
    CHECK(starts_with(disagree.err, "DISAGREE (2,0,C) at (1.05, 0.0, 0.0): formulas differ from the definition by "));
    const std::string number = disagree.err.substr(disagree.err.rfind("by ") + 3);
    CHECK(number.back() == '\n');
    CHECK(D(number.substr(0, number.size() - 1).c_str()) > D("1e-80"));
    // without --check the formulas are never evaluated
    CHECK(run_on(coarse, {}).code == kOk);
}

TEST_CASE("a tensor that does not agree between the two precisions is REFUSED: standard error says which, the exit status is 1, and no header is written", "[gradient_reference][behaviour]") {
    gr::Settings settings = small_settings();
    settings.lo = 10;
    settings.hi = 20;
    TempDir dir("odl-grad");
    const std::string file = (dir.path() / "never.hpp").string();
    const Result refused = run_on(settings, {"--header", file});
    CHECK(refused.code == kFailed);
    CHECK(refused.out.empty());
    CHECK(starts_with(refused.err, "REFUSED  (2,0,C) at (Fraction("));
    CHECK(contains(refused.err, "): the 10- and 20-digit tensors differ by "));
    CHECK(refused.err.back() == '\n');
    CHECK_FALSE(fs::exists(file));
}

TEST_CASE("the command line: -h prints the usage and the options; an option that is not one, or without its argument, is an error of exit status 2", "[gradient_reference][behaviour]") {
    const gr::Settings settings = small_settings();
    const std::string usage = "usage: gradient_reference [-h] [--check] [--header HEADER] [--verify VERIFY]\n";
    static const char kHelp[] = R"HELP(usage: gradient_reference [-h] [--check] [--header HEADER] [--verify VERIFY]

Reference tensors for SPEC-gravity's gradient acceptance rows, from the DEFINITION of the potential, by exact polynomial algebra: the second derivative of the potential of one normalised
coefficient (n, m, C or S), at the five reference positions, in 70 and in 110 digits, emitted only where the two agree to 50 (the header modules/gravity/tests/gradient_reference.hpp).

options:
  -h, --help       show this help and exit
  --check          also verify SPEC-gravity §4.7's formulas against the definition (90 digits against 110)
  --header HEADER  write a C++ header here instead of printing
  --verify VERIFY  compare this committed header with what the generator emits; exit 1 if they differ

exit codes: 0 emitted / verified   1 a value failed its two-precision agreement, or the two routes disagree, or the committed header differs
            2 an argument error, or a file that cannot be read or written
)HELP";   // the text of the tool's own help, pinned
    for (const char* flag : {"-h", "--help"}) {
        const Result help = run_on(settings, {flag});
        CHECK(help.code == kOk);
        CHECK(starts_with(help.out, usage));
        CHECK(help.out == kHelp);
        CHECK(contains(help.out, "  --check          also verify SPEC-gravity \xC2\xA7" "4.7's formulas against the definition (90 digits against 110)\n"));
        CHECK(contains(help.out, "  --header HEADER  write a C++ header here instead of printing\n"));
        CHECK(contains(help.out, "  --verify VERIFY  compare this committed header with what the generator emits; exit 1 if they differ\n"));
        CHECK(contains(help.out, "exit codes: 0 emitted / verified"));
        CHECK(help.err.empty());
    }
    const Result unknown = run_on(settings, {"--frobnicate"});
    CHECK(unknown.code == kArgument);
    CHECK(unknown.err == usage + "gradient_reference: error: unrecognized arguments: --frobnicate\n");
    CHECK(unknown.out.empty());
    const Result stray = run_on(settings, {"stray"});
    CHECK(stray.code == kArgument);
    CHECK(contains(stray.err, "unrecognized arguments: stray"));
    const Result bare = run_on(settings, {"--header"});
    CHECK(bare.code == kArgument);
    CHECK(bare.err == usage + "gradient_reference: error: argument --header: expected one argument\n");
    const Result swallowed = run_on(settings, {"--verify", "--check"});
    CHECK(swallowed.code == kArgument);
    CHECK(contains(swallowed.err, "argument --verify: expected one argument"));
    // a word that begins with two dashes is an option, however many more follow: `---x` is not the argument of --verify
    const Result dashes = run_on(settings, {"--verify", "---x"});
    CHECK(dashes.code == kArgument);
    CHECK(dashes.err == usage + "gradient_reference: error: argument --verify: expected one argument\n");
    const Result valued = run_on(settings, {"--check=1"});
    CHECK(valued.code == kArgument);
    CHECK(contains(valued.err, "unrecognized arguments: --check=1"));
    // an option may come after the help: -h answers at once
    CHECK(run_on(settings, {"-h", "--frobnicate"}).code == kOk);
    CHECK(run_on(settings, {"--frobnicate", "-h"}).code == kArgument);
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[gradient_reference][behaviour]") {
    odl::devtools_testing::ThrowingStream out;
    std::ostringstream err;
    CHECK(gr::run_on(small_settings(), {}, Streams{out, err}) == kInternal);
    CHECK(err.str() == "gradient_reference: internal error: boom\n");
}

// ======================================================================================================================================== the real tree

TEST_CASE("the committed header IS what the tool writes, byte for byte, and --check on the tree reproduces SPEC-gravity §4.7's 5.9e-84", "[gradient_reference][real_tree]") {
    const fs::path header = fs::path(ODL_TREE_ROOT) / kCommittedHeader;
    const Result verified = run_real({"--verify", header.string()});
    CHECK(verified.err == "ok       the committed header is exactly what the generator emits\n");
    CHECK(verified.code == kOk);
    // the specification records "worst relative disagreement 5.9 x 10^-84" for the check over the 145 cases (SPEC-gravity.md §4.7): its two figures, which rounding of four digits gives
    const Result checked = run_real({"--check"});
    CHECK(checked.code == kOk);
    const std::string head = "ok       \xC2\xA7" "4.7's formulas agree with the definition over 145 cases; worst relative disagreement ";
    REQUIRE(starts_with(checked.err, head));
    const std::string tail = checked.err.substr(head.size());
    const Decimal worst = D(tail.substr(0, tail.find(' ')).c_str());
    CHECK(worst >= D("5.85E-84"));
    CHECK(worst < D("5.95E-84"));
    // 145 rows, and the line count of the committed file
    const std::vector<std::string> lines = lines_of(read_text(header));
    CHECK(lines.size() == 171);
    CHECK(lines[0] == "// GENERATED by tools/gradient_reference.cpp \xE2\x80\x94 do not edit.");
}
