#pragma once
// odl/devkit/pymath.hpp — Python's float functions, as the tools that reproduce a Python tool's printed numbers use them (plan L0 step 8, group C8: tools/measmod_fd_sizing.cpp and
// tools/measmod_height_sensitivity.cpp, whose output is a few digits of a long chain of double-precision operations).
//
// What a printed number depends on is every rounding in the chain, so each function here does what Python's does, and where Python calls the C library the C library is called, THROUGH A FUNCTION OF THIS FILE
// COMPILED APART (pymath.cpp), so that no compiler can fold or merge it: GCC turns pow(x, 2.0) into x * x, and a sin and a cos of one argument into one call of sincos, and neither is promised to give the last
// place the separate calls of the Python gave.  Everything is compiled without fused multiply-add (-ffp-contract=off, tools/CMakeLists.txt).
//
// The domain errors are Python's: where math.sqrt(-1) raises ValueError ("math domain error") these throw std::domain_error, and where a finite argument gives an infinity (OverflowError) std::range_error.
// None is meant to happen in a tool whose inputs are fixed; the refusal is what stops a defect that would make it happen from printing a number.

#include <cstddef>
#include <span>

namespace odl::devkit {

/// math.radians(x) and math.degrees(x) as CPython computes them: x * (pi / 180) and x * (180 / pi), the two constants rounded to double first.
[[nodiscard]] double py_radians(double degrees);
[[nodiscard]] double py_degrees(double radians);

/// sum(items) of floats, Python 3.12 and later: the first item is added to the int 0 (so a -0.0 alone gives +0.0), the rest by Neumaier's improved Kahan-Babuska summation, and the compensation is added at the
/// end when it is finite and not zero.  An EMPTY sum is the int 0 in Python: it is 0.0 here, and a caller whose result depends on the difference (the sign of -sum(()) / x) says so.
[[nodiscard]] double py_sum(std::span<const double> items);

/// math.ulp(x): the value of the least significant bit of x (the distance to the next float away from zero; for the largest float, to the one below it); NaN for NaN, +inf for an infinity, the smallest
/// subnormal for zero, ulp(-x) == ulp(x).
[[nodiscard]] double py_ulp(double x);

/// math.hypot(x, y): the Euclidean norm, CORRECTLY ROUNDED -- computed in exact integer arithmetic.  Python's own is "within 1 ulp" and correctly rounded in all but a very few cases; a result outside the normal range
/// throws std::range_error (no tool of this tree meets one; a zero leg returns the other leg whatever it is).  An infinity gives +inf even beside a NaN, a NaN gives NaN, as Python's do.
[[nodiscard]] double py_hypot(double x, double y);

/// x ** y of two floats, CPython's float_pow: the special cases (y == 0 and x == 1 give 1.0, NaN, infinities) and then the C library's pow() of the magnitude, negated for a negative base and an odd integer
/// exponent.  Where Python would return a complex number (a negative base and an exponent that is not an integer) or raise ZeroDivisionError (0.0 to a negative power) this throws std::domain_error.
[[nodiscard]] double py_pow(double x, double y);

// the C library's functions, called as the math module calls them
[[nodiscard]] double py_sin(double x);
[[nodiscard]] double py_cos(double x);
[[nodiscard]] double py_tan(double x);
[[nodiscard]] double py_asin(double x);
[[nodiscard]] double py_atan(double x);
[[nodiscard]] double py_atan2(double y, double x);
[[nodiscard]] double py_log1p(double x);
[[nodiscard]] double py_log10(double x);
[[nodiscard]] double py_sqrt(double x);

}  // namespace odl::devkit
