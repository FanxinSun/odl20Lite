#pragma once
// odl/devkit/bigint.hpp — arbitrary-precision integers: the part of Python's `int` that the tools which prove things in exact arithmetic use (plan L0 step 8, group C6: tools/rk_coefficients.cpp,
// whose order conditions are identities over the rationals and whose intermediate numerators outgrow every machine integer).
//
// Written here because every piece of the devkit is: a generator does not link a library it did not write, and the number types the tools need are few.  Sign and magnitude, 32-bit limbs, the
// schoolbook algorithms; division by a number of more than one limb is Knuth's algorithm D (group C7: the decimal arithmetic of decimal.hpp divides numbers of thousands of digits, and the long
// division bit by bit that group C6 used was slower by the number of bits; tests/ keeps that division as its independent oracle).  Multiplication is still the schoolbook method: the sizes in use
// (a few thousand limbs at the most) make Karatsuba a thing to add the day a tool is seen waiting for it.
//
// The semantics are Python's where Python has a word for them: `//` and `%` are FLOOR division and the remainder takes the divisor's sign, `>>` floors, `gcd` is never negative.

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odl::devkit {

class BigInt {
public:
    BigInt() = default;   // zero
    BigInt(std::int64_t value);   // NOLINT(google-explicit-constructor): Python's int(): machine integers convert, and `2 * x` should read as it does there

    /// An optional sign and one or more decimal digits (ASCII), nothing else: no space, no underscore (Python's int() takes both; no tool writes them).  Throws std::invalid_argument.
    [[nodiscard]] static BigInt from_decimal(std::string_view text);
    [[nodiscard]] std::string to_decimal() const;

    [[nodiscard]] bool is_zero() const noexcept { return limbs_.empty(); }
    [[nodiscard]] int sign() const noexcept { return limbs_.empty() ? 0 : (negative_ ? -1 : 1); }
    [[nodiscard]] bool is_odd() const noexcept { return !limbs_.empty() && (limbs_.front() & 1U) != 0; }
    /// int.bit_length(): the number of bits of the magnitude, 0 for zero.
    [[nodiscard]] std::size_t bit_length() const noexcept;
    [[nodiscard]] BigInt abs() const;

    [[nodiscard]] bool fits_int64() const noexcept;
    /// Throws std::overflow_error when the value does not fit.
    [[nodiscard]] std::int64_t to_int64() const;

    [[nodiscard]] BigInt operator-() const;
    friend BigInt operator+(const BigInt& a, const BigInt& b);
    friend BigInt operator-(const BigInt& a, const BigInt& b);
    friend BigInt operator*(const BigInt& a, const BigInt& b);
    BigInt& operator+=(const BigInt& b) { return *this = *this + b; }
    BigInt& operator-=(const BigInt& b) { return *this = *this - b; }
    BigInt& operator*=(const BigInt& b) { return *this = *this * b; }

    /// Python's divmod: the quotient is the FLOOR of the true quotient and the remainder has the sign of the divisor (or is zero), so that a == quotient * b + remainder.  Throws std::domain_error
    /// when `b` is zero.
    struct DivMod;
    [[nodiscard]] static DivMod divmod(const BigInt& a, const BigInt& b);
    friend BigInt operator/(const BigInt& a, const BigInt& b);
    friend BigInt operator%(const BigInt& a, const BigInt& b);

    /// a * 2**n, and floor(a / 2**n) (Python's << and >>; the right shift of a negative number rounds toward minus infinity).
    [[nodiscard]] BigInt shifted_left(std::size_t n) const;
    [[nodiscard]] BigInt shifted_right(std::size_t n) const;

    /// math.gcd: never negative, gcd(0, 0) == 0.
    [[nodiscard]] static BigInt gcd(const BigInt& a, const BigInt& b);
    /// base ** exponent.
    [[nodiscard]] static BigInt pow(const BigInt& base, unsigned exponent);
    /// math.isqrt: the floor of the square root of a non-negative number (Newton's iteration from above).  Throws std::domain_error for a negative one.
    [[nodiscard]] static BigInt isqrt(const BigInt& n);

    friend bool operator==(const BigInt& a, const BigInt& b) noexcept { return a.negative_ == b.negative_ && a.limbs_ == b.limbs_; }
    friend std::strong_ordering operator<=>(const BigInt& a, const BigInt& b) noexcept;

private:
    using Limb = std::uint32_t;
    // the value with this magnitude (high zero limbs are dropped) and this sign (ignored for a zero magnitude)
    [[nodiscard]] static BigInt from_magnitude(std::vector<Limb> limbs, bool negative);

    std::vector<Limb> limbs_;   // the magnitude, least significant limb first, no limb of zero at the top
    bool negative_ = false;     // never true for zero
};

/// The result of BigInt::divmod.
struct BigInt::DivMod {
    BigInt quotient;
    BigInt remainder;
};

}  // namespace odl::devkit
