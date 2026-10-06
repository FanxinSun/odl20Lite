// odl/devkit/rational.cpp — see rational.hpp.

#include <odl/devkit/rational.hpp>

#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace odl::devkit {

Rational::Rational(BigInt numerator, BigInt denominator) {
    if (denominator.is_zero()) throw std::domain_error("Rational: the denominator is zero");
    if (denominator.sign() < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    const BigInt divisor = BigInt::gcd(numerator, denominator);   // at least 1: the denominator is not zero
    numerator_ = numerator / divisor;
    denominator_ = denominator / divisor;
}

Rational Rational::abs() const {
    Rational out;
    out.numerator_ = numerator_.abs();
    out.denominator_ = denominator_;
    return out;
}

std::string Rational::to_string() const {
    if (denominator_ == BigInt(1)) return numerator_.to_decimal();
    return numerator_.to_decimal() + "/" + denominator_.to_decimal();
}

double Rational::to_double() const {
    if (numerator_.is_zero()) return 0.0;
    const BigInt n = numerator_.abs();
    const BigInt& d = denominator_;
    // n/d lies strictly between 2**(e-1) and 2**(e+1).  Scale by 2**k, k = 55 - e, so that the integer quotient q = floor(n * 2**k / d) lies in [2**54, 2**56): 55 or 56 bits, two or three more than the
    // 53 a double keeps.  The remainder of that division says whether anything was lost below q.
    const auto e = static_cast<std::ptrdiff_t>(n.bit_length()) - static_cast<std::ptrdiff_t>(d.bit_length());
    const std::ptrdiff_t k = 55 - e;
    BigInt::DivMod qr = k >= 0 ? BigInt::divmod(n.shifted_left(static_cast<std::size_t>(k)), d) : BigInt::divmod(n, d.shifted_left(static_cast<std::size_t>(-k)));
    const std::uint64_t q = static_cast<std::uint64_t>(qr.quotient.to_int64());
    const bool sticky = !qr.remainder.is_zero();
    const auto extra = static_cast<unsigned>(qr.quotient.bit_length()) - 53U;   // the 2 or 3 low bits that do not fit
    std::uint64_t kept = q >> extra;
    const std::uint64_t low = q & ((std::uint64_t{1} << extra) - 1U);
    const std::uint64_t half = std::uint64_t{1} << (extra - 1U);
    // to nearest, ties to even: above half rounds up, below half rounds down, and exactly half rounds up when anything was lost below it (the true value is above the middle) or when the kept part is odd
    if (low > half || (low == half && (sticky || (kept & 1U) != 0))) ++kept;   // may carry into 2**53, which is a double all the same
    const std::ptrdiff_t exponent = static_cast<std::ptrdiff_t>(extra) - k;     // value = kept * 2**exponent
    const auto top = static_cast<std::ptrdiff_t>(std::bit_width(kept)) + exponent - 1;   // the binary exponent of the result
    if (top > 1023) throw std::range_error("Rational::to_double: " + to_string() + " is too large for a double");
    if (top < -1022) throw std::range_error("Rational::to_double: " + to_string() + " is too small for a normal double");
    const double magnitude = std::ldexp(static_cast<double>(kept), static_cast<int>(exponent));
    return numerator_.sign() < 0 ? -magnitude : magnitude;
}

Rational Rational::operator-() const {
    Rational out;
    out.numerator_ = -numerator_;
    out.denominator_ = denominator_;
    return out;
}

Rational operator+(const Rational& a, const Rational& b) {
    return Rational(a.numerator_ * b.denominator_ + b.numerator_ * a.denominator_, a.denominator_ * b.denominator_);
}

Rational operator-(const Rational& a, const Rational& b) { return a + (-b); }

Rational operator*(const Rational& a, const Rational& b) { return Rational(a.numerator_ * b.numerator_, a.denominator_ * b.denominator_); }

Rational operator/(const Rational& a, const Rational& b) {
    if (b.numerator_.is_zero()) throw std::domain_error("Rational: division by zero");
    return Rational(a.numerator_ * b.denominator_, a.denominator_ * b.numerator_);
}

std::strong_ordering operator<=>(const Rational& a, const Rational& b) {
    // the denominators are positive, so cross-multiplying keeps the order
    return (a.numerator_ * b.denominator_) <=> (b.numerator_ * a.denominator_);
}

}  // namespace odl::devkit
