// The devkit's Python float functions (pymath.hpp) and the 'e' and 'f' formats (pyfmt.hpp): sum() with Neumaier's compensation on vectors whose answer is derived by hand, math.ulp on its documented cases,
// hypot against an exact oracle that shares no code with it (the rounding of a square root is correct if and only if the exact sum of squares lies between the squares of the two midpoints), the special cases of
// float_pow, radians and degrees on the values Python gives, the domain errors, and the printf-based formats on the values Python prints.  Every expected value is DERIVED BEFORE the code ran (the derivations are in
// the comments) and none was taken from a run of it.  The random inputs come from std::mt19937_64 with a fixed seed.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/rational.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace odl::devkit;

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
const double kNaN = std::numeric_limits<double>::quiet_NaN();

double sum_of(std::vector<double> items) { return py_sum(items); }

// the words of the exception an action throws ("(nothing was thrown)" when it throws none)
template <typename Action>
std::string message_of(const Action& action) {
    try {
        action();
    } catch (const std::exception& exc) {
        return exc.what();
    }
    return "(nothing was thrown)";
}

// the exact rational of a finite double
Rational exact(double v) {
    if (v == 0.0) return Rational(0);
    int e2 = 0;
    const double f = std::frexp(std::fabs(v), &e2);
    Rational out{BigInt(static_cast<std::int64_t>(std::ldexp(f, 53)))};
    const int shift = e2 - 53;
    if (shift >= 0) out = out * Rational(BigInt::pow(BigInt(2), static_cast<unsigned>(shift)));
    else out = out / Rational(BigInt::pow(BigInt(2), static_cast<unsigned>(-shift)));
    return v < 0 ? -out : out;
}

}  // namespace

TEST_CASE("py_sum is sum() of floats in Python 3.12 and later: Neumaier's compensation, the first item added to the int 0", "[devkit][pymath]") {
    CHECK(sum_of({}) == 0.0);
    CHECK(sum_of({1.0, 2.0, 3.0}) == 6.0);
    CHECK(sum_of({5.0}) == 5.0);
    // the classical example: 1e100 + 1 + (-1e100) + 1.  Step by step (result, compensation): (1e100, 0); 1e100 + 1 = 1e100, the 1 is lost into the compensation (1e100 - 1e100) + 1 = 1; then -1e100 gives (0, 1 + 0), and
    // the 1 gives (1, 1 + 0); the end adds the compensation: 2.  The naive sum is 0.
    CHECK(sum_of({1e100, 1.0, -1e100, 1.0}) == 2.0);
    CHECK(sum_of({1.0, 1e100, 1.0, -1e100}) == 2.0);
    // 1e16 + 1 + 1: each 1 is a tie below the spacing of 2 at 1e16 and goes to the even neighbour, 1e16 itself; the compensation collects both; 1e16 + 2 is a double
    CHECK(sum_of({1e16, 1.0, 1.0}) == 1.0000000000000002e16);
    // ten copies of 0.1: the exact sum is 1.00000000000000005551..., whose nearest double is 1.0 (the naive sum is 0.9999999999999999)
    CHECK(sum_of(std::vector<double>(10, 0.1)) == 1.0);
    // a lone -0.0 added to the int 0 is +0.0, and so is a sum of negative zeros; a negative number alone stays
    CHECK_FALSE(std::signbit(sum_of({-0.0})));
    CHECK_FALSE(std::signbit(sum_of({-0.0, -0.0})));
    CHECK(sum_of({-3.5}) == -3.5);
    CHECK(sum_of({-1.0, 1.0}) == 0.0);
    CHECK_FALSE(std::signbit(sum_of({-1.0, 1.0})));
    // an infinite sum stays infinite (the NaN the compensation becomes must not be added), a NaN stays NaN
    CHECK(sum_of({kInf, 1.0}) == kInf);
    CHECK(sum_of({1.0, -kInf}) == -kInf);
    CHECK(std::isnan(sum_of({kNaN, 1.0})));
    CHECK(std::isnan(sum_of({kInf, -kInf})));
    // the sum of two items is the plain sum when nothing is lost, and the compensation is not added when it is zero
    CHECK(sum_of({0.5, 0.25}) == 0.75);
    CHECK(sum_of({1e308, 1e308}) == kInf);   // the overflow of a plain sum, not a NaN
}

TEST_CASE("py_ulp is math.ulp: the documented cases", "[devkit][pymath]") {
    CHECK(py_ulp(1.0) == std::ldexp(1.0, -52));
    CHECK(py_ulp(-1.0) == std::ldexp(1.0, -52));
    CHECK(py_ulp(0.5) == std::ldexp(1.0, -53));
    CHECK(py_ulp(2.0) == std::ldexp(1.0, -51));
    CHECK(py_ulp(1.0 - std::ldexp(1.0, -53)) == std::ldexp(1.0, -53));   // the largest double below one: the next one up is 1.0, a distance of 2**-53
    CHECK(py_ulp(0.0) == std::numeric_limits<double>::denorm_min());
    CHECK(py_ulp(-0.0) == std::numeric_limits<double>::denorm_min());
    CHECK(py_ulp(std::numeric_limits<double>::denorm_min()) == std::numeric_limits<double>::denorm_min());
    CHECK(py_ulp(kInf) == kInf);
    CHECK(py_ulp(-kInf) == kInf);
    CHECK(std::isnan(py_ulp(kNaN)));
    // the largest double: the distance to the one below it, 2**971
    CHECK(py_ulp(std::numeric_limits<double>::max()) == std::ldexp(1.0, 971));
    CHECK(py_ulp(std::ldexp(1.0, 1023)) == std::ldexp(1.0, 971));
    // the ulps the sizing tools take: 2 pi lies in [4, 8) (2**-50), 1.227e7 in [2**23, 2**24) (2**-29), 7.2e6 in [2**22, 2**23) (2**-30), 6.92e6 likewise, 1.5708 in [1, 2) (2**-52)
    CHECK(py_ulp(2.0 * std::numbers::pi) == std::ldexp(1.0, -50));
    CHECK(py_ulp(1.227e7) == std::ldexp(1.0, -29));
    CHECK(py_ulp(7.2e6) == std::ldexp(1.0, -30));
    CHECK(py_ulp(6.92e6) == std::ldexp(1.0, -30));
    CHECK(py_ulp(1.5707963267948966) == std::ldexp(1.0, -52));
}

TEST_CASE("py_hypot is the correctly rounded Euclidean norm, shown against the exact sum of squares", "[devkit][pymath]") {
    CHECK(py_hypot(3.0, 4.0) == 5.0);
    CHECK(py_hypot(-3.0, 4.0) == 5.0);
    CHECK(py_hypot(5.0, -12.0) == 13.0);
    CHECK(py_hypot(0.0, 7.5) == 7.5);
    CHECK(py_hypot(-7.5, 0.0) == 7.5);
    CHECK(py_hypot(0.0, 0.0) == 0.0);
    CHECK(py_hypot(1.0, 1.0) == std::sqrt(2.0));                                            // the correctly rounded root of 2
    CHECK(py_hypot(std::ldexp(1.0, 500), std::ldexp(1.0, 500)) == std::ldexp(std::sqrt(2.0), 500));   // a power of two scales exactly
    CHECK(py_hypot(std::ldexp(1.0, -300), std::ldexp(1.0, -300)) == std::ldexp(std::sqrt(2.0), -300));
    CHECK(py_hypot(1e300, 1e300) == py_hypot(1e300, -1e300));
    CHECK(py_hypot(kInf, kNaN) == kInf);                                                    // an infinity wins over a NaN
    CHECK(py_hypot(kNaN, -kInf) == kInf);
    CHECK(std::isnan(py_hypot(kNaN, 1.0)));
    CHECK(py_hypot(-kInf, 2.0) == kInf);
    // a result that overflows is Python's OverflowError: here a refusal (the result of two doubles near the maximum is beyond the range)
    CHECK_THROWS_AS(py_hypot(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()), std::range_error);
    // zero is no special case: the same path takes it (a mantissa of zero), and the answer is the other leg, in either place, of either sign, down to the smallest numbers
    CHECK(py_hypot(0.0, 1e308) == 1e308);
    CHECK(py_hypot(-0.0, std::numeric_limits<double>::denorm_min()) == std::numeric_limits<double>::denorm_min());
    CHECK(py_hypot(std::numeric_limits<double>::denorm_min(), 0.0) == std::numeric_limits<double>::denorm_min());
    CHECK_FALSE(std::signbit(py_hypot(-0.0, -0.0)));
    // a result below the normal range is refused, as the header says (a zero leg is not such a case: the other leg comes back as it is)
    CHECK_THROWS_AS(py_hypot(std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::denorm_min()), std::range_error);
    // THE STICKY BIT.  The integer root of S * 4**k is a floor: it cannot tell a square root that lies a hair above the midpoint of two doubles from one that lies exactly at it, nor a hair below from at it; the bit
    // under the root does.  1 + 2**-53 is the midpoint of 1 and 1 + 2**-52.  With y = 2**-26 (1 + 2**-52) (a double: 1 + 2**-52 is the double above 1, and a power of two scales it exactly), y**2 = 2**-52 (1 + 2**-51 + 2**-104)
    // and sqrt(1 + y**2) = 1 + 2**-53 + 2**-104 - 2**-107 + ...: ABOVE the midpoint by 2**-104, so the correctly rounded value is 1 + 2**-52 (a root of 70 bits sees nothing of a hair of 2**-104 and, without the sticky bit,
    // would be a tie and go to the even neighbour, 1).  With y = 2**-26 (1 - 2**-53) (the double below 1 times a power of two), y**2 = 2**-52 (1 - 2**-52 + 2**-106) and sqrt(1 + y**2) = 1 + 2**-53 - 2**-105 - ...:
    // BELOW the midpoint, so the value is 1.
    {
        const double above = std::ldexp(1.0 + std::ldexp(1.0, -52), -26);
        const double below = std::ldexp(1.0 - std::ldexp(1.0, -53), -26);
        const Rational mid = (exact(1.0) + exact(1.0 + std::ldexp(1.0, -52))) / Rational(2);
        CHECK(Rational(1) + exact(above) * exact(above) > mid * mid);   // the oracle's own statement of what is derived above
        CHECK(Rational(1) + exact(below) * exact(below) < mid * mid);
        CHECK(py_hypot(1.0, above) == 1.0 + std::ldexp(1.0, -52));
        CHECK(py_hypot(above, 1.0) == 1.0 + std::ldexp(1.0, -52));
        CHECK(py_hypot(1.0, below) == 1.0);
    }
    // AN EXACT TIE.  The Pythagorean triple of (m, n) = (75000001, 70000000) is (m**2 - n**2, 2 m n, m**2 + n**2) = (725000150000001, 10500000140000000, 10525000150000001).  Both legs are doubles (the first is below 2**53,
    // the second is even and below 2**54) and the hypotenuse, 54 bits whose last is 1, is the MIDPOINT of the doubles 10525000150000000 and 10525000150000002.  Ties go to the even mantissa, and (10525000150000000) / 2 =
    // 5262500075000000 is even: the answer is 10525000150000000, which an always-sticky root (a tie pushed up) would not give, and nor would one that takes an exact root for inexact.
    {
        const BigInt a(725000150000001);
        const BigInt b(10500000140000000);
        const BigInt c(10525000150000001);
        CHECK(a * a + b * b == c * c);
        CHECK(py_hypot(725000150000001.0, 10500000140000000.0) == 10525000150000000.0);
        CHECK(py_hypot(-10500000140000000.0, 725000150000001.0) == 10525000150000000.0);
    }
    // a million-fold of random pairs would be slow with rationals; 4,000 of them, of every relative size, against the exact oracle: r is the correct rounding of sqrt(S) when S lies between the squares of the
    // midpoints (r + below) / 2 and (r + above) / 2, which are rationals of the double's own neighbours
    std::mt19937_64 rng(0x4859504FULL);
    for (int round = 0; round < 4000; ++round) {
        const double a = std::ldexp(static_cast<double>(rng() >> 11) + 1.0, static_cast<int>(rng() % 120) - 90);
        const double b = std::ldexp(static_cast<double>(rng() >> 11) + 1.0, static_cast<int>(rng() % 120) - 90);
        const double r = py_hypot(a, b);
        const Rational s = exact(a) * exact(a) + exact(b) * exact(b);
        const Rational low = (exact(r) + exact(std::nextafter(r, 0.0))) / Rational(2);
        const Rational high = (exact(r) + exact(std::nextafter(r, kInf))) / Rational(2);
        INFO("hypot(" << a << ", " << b << ") = " << r);
        CHECK(low * low <= s);
        CHECK(s <= high * high);
        CHECK(r >= std::fmax(a, b));
    }
}

TEST_CASE("py_pow is CPython's float_pow: the special cases, the sign of an odd power, and the refusals", "[devkit][pymath]") {
    CHECK(py_pow(2.0, 10.0) == 1024.0);
    CHECK(py_pow(-2.0, 3.0) == -8.0);
    CHECK(py_pow(-2.0, 2.0) == 4.0);
    CHECK(py_pow(-2.0, -3.0) == -0.125);
    CHECK(py_pow(10.0, -3.0) == 0.001);
    CHECK(py_pow(2.0, 0.5) == std::sqrt(2.0));
    // x ** 0 is 1 whatever x is, 1 ** y is 1 whatever y is, a NaN is a NaN otherwise
    CHECK(py_pow(0.0, 0.0) == 1.0);
    CHECK(py_pow(kNaN, 0.0) == 1.0);
    CHECK(py_pow(kInf, 0.0) == 1.0);
    CHECK(py_pow(1.0, kNaN) == 1.0);
    CHECK(py_pow(1.0, kInf) == 1.0);
    CHECK(py_pow(-1.0, kInf) == 1.0);
    CHECK(std::isnan(py_pow(kNaN, 2.0)));
    CHECK(std::isnan(py_pow(2.0, kNaN)));
    CHECK(py_pow(0.0, 3.0) == 0.0);
    CHECK(py_pow(0.0, 0.5) == 0.0);
    CHECK(py_pow(kInf, 2.0) == kInf);
    CHECK(py_pow(2.0, kInf) == kInf);
    CHECK(py_pow(0.5, kInf) == 0.0);
    CHECK(py_pow(-1.0, 3.0) == -1.0);
    CHECK(py_pow(-1.0, 4.0) == 1.0);
    // the refusals: Python's ZeroDivisionError, and the complex result of a negative base and a fraction (a float is what these tools need)
    CHECK_THROWS_AS(py_pow(0.0, -1.0), std::domain_error);
    CHECK_THROWS_AS(py_pow(-0.0, -2.0), std::domain_error);
    CHECK_THROWS_AS(py_pow(-8.0, 1.0 / 3.0), std::domain_error);
    CHECK_THROWS_AS(py_pow(-2.0, 0.5), std::domain_error);
    // an overflow of finite numbers is OverflowError: refused
    CHECK_THROWS_AS(py_pow(10.0, 400.0), std::range_error);
    CHECK_THROWS_AS(py_pow(-10.0, 401.0), std::range_error);
    // and they say what Python says (or, for the complex case, what it would make of it)
    CHECK(message_of([] { (void)py_pow(0.0, -1.0); }) == "0.0 cannot be raised to a negative power");
    CHECK(message_of([] { (void)py_pow(-2.0, 0.5); }) == "a negative number raised to a power that is not an integer is complex in Python");
    CHECK(message_of([] { (void)py_pow(10.0, 400.0); }) == "math range error");
    // a call that the compiler could fold (a constant exponent of 2) goes to the C library all the same, and agrees with the product where that is exact: 6.6e6 ** 2 and 1.5e6 ** 2 are exact doubles
    CHECK(py_pow(6.6e6, 2.0) == 6.6e6 * 6.6e6);
    CHECK(py_pow(1.5e6, 2.0) == 1.5e6 * 1.5e6);
    CHECK(py_pow(6.6e6, 3.0) == 6.6e6 * 6.6e6 * 6.6e6);   // 103125**3 has 50 bits: exact in a double as well
}

TEST_CASE("py_radians and py_degrees are CPython's: x * (pi / 180) and x * (180 / pi), each constant rounded first", "[devkit][pymath]") {
    CHECK(py_radians(180.0) == std::numbers::pi);              // math.radians(180) == math.pi in Python
    CHECK(py_radians(90.0) == 1.5707963267948966);
    CHECK(py_radians(0.0) == 0.0);
    CHECK(py_radians(1.0) == 0.017453292519943295);
    CHECK(py_degrees(std::numbers::pi) == 180.0);
    CHECK(py_degrees(1.0) == 57.29577951308232);
    CHECK(py_degrees(0.0) == 0.0);
    CHECK(py_radians(-45.0) == -py_radians(45.0));
    // the product with the rounded constant, not a division: 30 degrees is 30 * 0.017453292519943295 = 0.5235987755982988 (the double nearest pi / 6 is 0.5235987755982988 as well)
    CHECK(py_radians(30.0) == 30.0 * 0.017453292519943295);
}

TEST_CASE("the C library's functions behind py_*: the values at the points everybody knows, and Python's domain errors", "[devkit][pymath]") {
    CHECK(py_sin(0.0) == 0.0);
    CHECK(py_cos(0.0) == 1.0);
    CHECK(py_tan(0.0) == 0.0);
    CHECK(py_asin(1.0) == 1.5707963267948966);
    CHECK(py_asin(0.0) == 0.0);
    CHECK(py_atan(1.0) == 0.7853981633974483);
    CHECK(py_atan2(1.0, 1.0) == 0.7853981633974483);
    CHECK(py_atan2(0.0, -1.0) == std::numbers::pi);
    CHECK(py_atan2(-0.0, -1.0) == -std::numbers::pi);
    CHECK(py_log10(1000.0) == 3.0);
    CHECK(py_log10(1.0) == 0.0);
    CHECK(py_log10(0.001) == -3.0);
    CHECK(py_log1p(0.0) == 0.0);
    CHECK(py_sqrt(2.0) == 1.4142135623730951);
    CHECK(py_sqrt(0.0) == 0.0);
    CHECK(py_sqrt(1e300) == 1e150);
    // domain errors (Python's ValueError) and an infinity from finite arguments (OverflowError)
    CHECK_THROWS_AS(py_sqrt(-1.0), std::domain_error);
    CHECK_THROWS_AS(py_asin(1.5), std::domain_error);
    CHECK_THROWS_AS(py_asin(-1.0000000000000002), std::domain_error);
    CHECK_THROWS_AS(py_log10(-1.0), std::domain_error);
    CHECK_THROWS_AS(py_log10(0.0), std::range_error);
    CHECK_THROWS_AS(py_log1p(-1.0), std::range_error);
    CHECK_THROWS_AS(py_log1p(-2.0), std::domain_error);
    CHECK_THROWS_AS(py_sin(kInf), std::domain_error);
    CHECK_THROWS_AS(py_cos(kInf), std::domain_error);
    CHECK_THROWS_AS(py_tan(kInf), std::domain_error);
    // the words of the errors: a ValueError is "math domain error" (a NaN from finite arguments, or an infinite argument of sin, cos and tan, or a square root of a negative number), an OverflowError "math range error"
    CHECK(message_of([] { (void)py_asin(1.5); }) == "math domain error");
    CHECK(message_of([] { (void)py_log10(-1.0); }) == "math domain error");
    CHECK(message_of([] { (void)py_log10(0.0); }) == "math range error");
    CHECK(message_of([] { (void)py_sqrt(-1.0); }) == "math domain error");
    CHECK(message_of([] { (void)py_sin(kInf); }) == "math domain error");
    CHECK(message_of([] { (void)py_cos(kInf); }) == "math domain error");
    CHECK(message_of([] { (void)py_tan(-kInf); }) == "math domain error");
    // a NaN in is a NaN out, with no error
    CHECK(std::isnan(py_sin(kNaN)));
    CHECK(std::isnan(py_log10(kNaN)));
    // identities that tie the functions to one another to the last place of a double that a library of 0.55 ulp would not keep: only at exact points
    CHECK(py_cos(std::numbers::pi) == -1.0);
    CHECK(py_sin(std::numbers::pi / 2.0) == 1.0);
}

TEST_CASE("py_format_e and py_format_f are format(x, '.Ne') and format(x, '.Nf'): the exactly rounded decimal expansion, and Python's words for the rest", "[devkit][pymath]") {
    CHECK(py_format_e(1.5e-5, 3) == "1.500e-05");
    CHECK(py_format_e(12345.678, 2) == "1.23e+04");
    CHECK(py_format_e(0.0, 4) == "0.0000e+00");
    CHECK(py_format_e(-0.0, 1) == "-0.0e+00");
    CHECK(py_format_e(1e100, 0) == "1e+100");
    CHECK(py_format_e(-2.5e-300, 5) == "-2.50000e-300");
    CHECK(py_format_e(9.5, 0) == "1e+01");                       // 9.5 is exact, and a tie goes to the even neighbour of the last digit kept: 10 (the digit before is 9, odd)
    CHECK(py_format_e(8.5, 0) == "8e+00");                       // an exact tie to the even digit
    CHECK(py_format_e(kInf, 3) == "inf");
    CHECK(py_format_e(-kInf, 3) == "-inf");
    CHECK(py_format_e(kNaN, 3) == "nan");
    CHECK(py_format_f(2.5, 0) == "2");                           // exact ties to even, as Python's format does
    CHECK(py_format_f(3.5, 0) == "4");
    CHECK(py_format_f(0.125, 2) == "0.12");
    CHECK(py_format_f(0.375, 2) == "0.38");
    CHECK(py_format_f(-0.0, 1) == "-0.0");
    CHECK(py_format_f(1e22, 0) == "10000000000000000000000");
    CHECK(py_format_f(5580552.4, 1) == "5580552.4");
    CHECK(py_format_f(3.14159, 3) == "3.142");
    CHECK(py_format_f(-29.0465, 4) == "-29.0465");
    CHECK(py_format_f(10.0, 0) == "10");
    CHECK(py_format_f(1e300, 0).size() == 301);                  // a number of 301 digits: the buffer is the size it needs
    CHECK(py_format_f(kInf, 2) == "inf");
    CHECK(py_format_f(-kInf, 2) == "-inf");
    CHECK(py_format_f(kNaN, 2) == "nan");
}
