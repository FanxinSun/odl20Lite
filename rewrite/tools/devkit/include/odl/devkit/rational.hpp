#pragma once
// odl/devkit/rational.hpp — exact rational numbers: the part of Python's `fractions.Fraction` that the tools which prove things in exact arithmetic use (plan L0 step 8, group C6:
// tools/rk_coefficients.cpp, whose order conditions are identities over the rationals).
//
// A Rational is always in lowest terms with a positive denominator, so that two equal values have the same numerator and denominator and `==` is a comparison of integers, as Fraction's is.  The
// conversion to double is CORRECTLY ROUNDED (to nearest, ties to even), as `float(Fraction)` is: Python divides the two integers with int.__truediv__, which rounds correctly, and this does the
// same with the integers of bigint.hpp.  The tools print the doubles with %.17g, which round-trips, so a conversion that was wrong in the last place would show in every file they write.

#include <odl/devkit/bigint.hpp>

#include <compare>
#include <cstdint>
#include <string>
#include <utility>

namespace odl::devkit {

class Rational {
public:
    Rational() = default;   // zero
    Rational(std::int64_t value) : numerator_(value), denominator_(1) {}   // NOLINT(google-explicit-constructor): Fraction(int); `2 * x` should read as it does there
    explicit Rational(BigInt value) : numerator_(std::move(value)), denominator_(1) {}
    /// numerator / denominator in lowest terms (Fraction(a, b)).  Throws std::domain_error when the denominator is zero.
    Rational(BigInt numerator, BigInt denominator);

    [[nodiscard]] const BigInt& numerator() const noexcept { return numerator_; }
    [[nodiscard]] const BigInt& denominator() const noexcept { return denominator_; }
    [[nodiscard]] int sign() const noexcept { return numerator_.sign(); }
    [[nodiscard]] bool is_zero() const noexcept { return numerator_.is_zero(); }
    [[nodiscard]] Rational abs() const;

    /// str(Fraction): "3/4", "-3/4", and "5" when the denominator is 1.
    [[nodiscard]] std::string to_string() const;

    /// float(Fraction): the double nearest to the value, ties to the even one.  Throws std::range_error when the value is too large for a double or so small that the nearest double is not normal (the tools
    /// never meet either, and the rounding of a subnormal is a thing this does not claim).
    [[nodiscard]] double to_double() const;

    [[nodiscard]] Rational operator-() const;
    friend Rational operator+(const Rational& a, const Rational& b);
    friend Rational operator-(const Rational& a, const Rational& b);
    friend Rational operator*(const Rational& a, const Rational& b);
    /// Throws std::domain_error when `b` is zero.
    friend Rational operator/(const Rational& a, const Rational& b);
    Rational& operator+=(const Rational& b) { return *this = *this + b; }
    Rational& operator-=(const Rational& b) { return *this = *this - b; }
    Rational& operator*=(const Rational& b) { return *this = *this * b; }

    friend bool operator==(const Rational& a, const Rational& b) noexcept { return a.numerator_ == b.numerator_ && a.denominator_ == b.denominator_; }
    friend std::strong_ordering operator<=>(const Rational& a, const Rational& b);

private:
    BigInt numerator_;
    BigInt denominator_{1};   // always positive
};

}  // namespace odl::devkit
