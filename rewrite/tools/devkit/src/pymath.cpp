// odl/devkit/pymath.cpp — see pymath.hpp.  This file is compiled on its own and calls the C library through the functions below, so that nothing is folded or merged at a call site in another file.

#include <odl/devkit/pymath.hpp>

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/rational.hpp>

#include <math.h>   // NOLINT(modernize-deprecated-headers): ::pow, ::sin ... the C library's, whose addresses are taken

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace odl::devkit {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;   // the two constants as CPython rounds them: pi / 180.0 and 180.0 / pi, each a double
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

// a result that is NaN or infinite from finite arguments is Python's ValueError or OverflowError
double checked(double result, bool arguments_finite) {
    if (arguments_finite && std::isnan(result)) throw std::domain_error("math domain error");
    if (arguments_finite && std::isinf(result)) throw std::range_error("math range error");
    return result;
}

// v > 0 finite as an integer mantissa below 2**53 and a power of two: v = mantissa * 2**exponent, exactly
void decompose(double v, std::uint64_t& mantissa, int& exponent) {
    int e2 = 0;
    const double fraction = std::frexp(v, &e2);   // 0.5 <= fraction < 1
    mantissa = static_cast<std::uint64_t>(std::ldexp(fraction, 53));
    exponent = e2 - 53;
}

}  // namespace

double py_radians(double degrees) { return degrees * kDegToRad; }

double py_degrees(double radians) { return radians * kRadToDeg; }

double py_sum(std::span<const double> items) {
    if (items.empty()) return 0.0;
    double result = 0.0 + items.front();   // the int 0 plus the first float: +0.0 + -0.0 is +0.0
    double compensation = 0.0;
    for (std::size_t i = 1; i < items.size(); ++i) {
        const double x = items[i];
        const double t = result + x;
        if (std::fabs(result) >= std::fabs(x)) compensation += (result - t) + x;
        else compensation += (x - t) + result;
        result = t;
    }
    if (compensation != 0.0 && std::isfinite(compensation)) result += compensation;   // (a compensation that is infinite or NaN must not turn an infinite sum into NaN)
    return result;
}

double py_ulp(double x) {
    if (std::isnan(x)) return x;
    x = std::fabs(x);
    if (std::isinf(x)) return x;
    const double up = std::nextafter(x, std::numeric_limits<double>::infinity());
    if (std::isinf(up)) return x - std::nextafter(x, -std::numeric_limits<double>::infinity());   // the largest float: the distance to the one below
    return up - x;
}

double py_hypot(double x, double y) {
    x = std::fabs(x);
    y = std::fabs(y);
    if (std::isinf(x) || std::isinf(y)) return std::numeric_limits<double>::infinity();
    if (std::isnan(x) || std::isnan(y)) return std::numeric_limits<double>::quiet_NaN();
    if (x == 0.0) return y;   // a zero leg: the other one, whatever it is (the general path would take the same value through a rational that cannot be converted back when the leg is subnormal)
    if (y == 0.0) return x;
    std::uint64_t mx = 0;
    std::uint64_t my = 0;
    int ex = 0;
    int ey = 0;
    decompose(x, mx, ex);
    decompose(y, my, ey);
    const int e0 = std::min(ex, ey);
    // x**2 + y**2 = S * 2**(2 e0), S an integer
    const BigInt sum = (BigInt(static_cast<std::int64_t>(mx)) * BigInt(static_cast<std::int64_t>(mx))).shifted_left(static_cast<std::size_t>(2 * (ex - e0))) +
                       (BigInt(static_cast<std::int64_t>(my)) * BigInt(static_cast<std::int64_t>(my))).shifted_left(static_cast<std::size_t>(2 * (ey - e0)));
    // sqrt(S) = sqrt(S * 4**k) / 2**k: with S * 4**k of at least 140 bits the integer root has at least 70, far more than the 53 of a double and its rounding bit
    const std::size_t bits = sum.bit_length();
    const std::size_t k = bits >= 140 ? 0 : (140 - bits + 1) / 2;
    const BigInt radicand = sum.shifted_left(2 * k);
    const BigInt root = BigInt::isqrt(radicand);
    const bool exact = root * root == radicand;
    // the root with a STICKY bit under it: 2 root + 1 when the square root is not an integer, so that it can never stand at the midpoint of two doubles, and the rounding of it is the rounding of the true value
    const BigInt scaled = root.shifted_left(1) + BigInt(exact ? 0 : 1);
    const long long exponent = static_cast<long long>(e0) - static_cast<long long>(k) - 1;
    const Rational value = exponent >= 0 ? Rational(scaled.shifted_left(static_cast<std::size_t>(exponent))) : Rational(scaled, BigInt(1).shifted_left(static_cast<std::size_t>(-exponent)));
    return value.to_double();   // correctly rounded, ties to even (an exact result is a double or a tie)
}

double py_pow(double x, double y) {
    if (y == 0.0) return 1.0;   // x ** 0 is 1, whatever x is (NaN and 0.0 included)
    if (std::isnan(x)) return x;
    if (std::isnan(y)) return x == 1.0 ? 1.0 : y;
    bool negate = false;
    if (x == 0.0 && std::isfinite(y) && y < 0.0) throw std::domain_error("0.0 cannot be raised to a negative power");
    if (x < 0.0 && std::isfinite(x)) {
        if (std::isfinite(y) && y != std::floor(y)) throw std::domain_error("a negative number raised to a power that is not an integer is complex in Python");
        negate = std::isfinite(y) && std::fmod(std::fabs(y), 2.0) == 1.0;   // an odd integer exponent
        x = -x;
    }
    static double (*volatile call)(double, double) = &::pow;   // the C library's pow, called through a pointer the compiler cannot see through
    double r = call(x, y);
    if (negate) r = -r;
    return checked(r, std::isfinite(x) && std::isfinite(y));
}

double py_sin(double x) {
    static double (*volatile call)(double) = &::sin;
    if (std::isinf(x)) throw std::domain_error("math domain error");   // Python: math.sin(inf) raises ValueError
    return checked(call(x), std::isfinite(x));
}

double py_cos(double x) {
    static double (*volatile call)(double) = &::cos;
    if (std::isinf(x)) throw std::domain_error("math domain error");
    return checked(call(x), std::isfinite(x));
}

double py_tan(double x) {
    static double (*volatile call)(double) = &::tan;
    if (std::isinf(x)) throw std::domain_error("math domain error");
    return checked(call(x), std::isfinite(x));
}

double py_asin(double x) {
    static double (*volatile call)(double) = &::asin;
    return checked(call(x), std::isfinite(x));
}

double py_acos(double x) {
    static double (*volatile call)(double) = &::acos;
    return checked(call(x), std::isfinite(x));
}

double py_atan(double x) {
    static double (*volatile call)(double) = &::atan;
    return call(x);
}

double py_atan2(double y, double x) {
    static double (*volatile call)(double, double) = &::atan2;
    return call(y, x);
}

double py_log(double x) {
    static double (*volatile call)(double) = &::log;
    return checked(call(x), std::isfinite(x));
}

double py_log1p(double x) {
    static double (*volatile call)(double) = &::log1p;
    return checked(call(x), std::isfinite(x));
}

double py_log10(double x) {
    static double (*volatile call)(double) = &::log10;
    return checked(call(x), std::isfinite(x));
}

double py_sqrt(double x) {
    if (x < 0.0) throw std::domain_error("math domain error");
    return std::sqrt(x);   // IEEE: exact, the same wherever it is computed
}

}  // namespace odl::devkit
