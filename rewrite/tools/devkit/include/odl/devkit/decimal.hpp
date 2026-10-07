#pragma once
// odl/devkit/decimal.hpp — decimal floating point: the part of Python's `decimal` module (libmpdec, the General Decimal Arithmetic Specification) that the generators which compute in high precision use
// (plan L0 step 8, group C7: tools/gradient_reference.cpp, legendre_reference.cpp and forcemodel_comparator_header.cpp, which evaluate definitions at 40 to 9,460 digits and print what two precisions agree on).
//
// A Decimal is a sign, an integer coefficient and an exponent (value = (-1)**sign * coefficient * 10**exponent) and keeps all three, because Python's does and what a tool prints depends on them: the exponent of a
// quotient (the IDEAL exponent: an exact result has its trailing zeros removed down to it, an inexact one fills the precision), the exponent of a sum (the smaller of the two), the sign of a zero (a product, a quotient
// and a square root of a negative zero are negative zeros; x + (-x) is +0).  Every operation is evaluated in the CURRENT CONTEXT (getcontext(): the precision in digits, ROUND_HALF_EVEN, Emax/Emin; localcontext() is
// LocalContext) and is CORRECTLY ROUNDED — the exact result rounded once to the precision — except the integer power, which is libmpdec's: square-and-multiply in a working precision a few digits larger than the
// context's, rounded to the context at the end, which is what Python computes and what a result that is the noise of the last digits (the worst disagreement of two routes at 90 digits) is made of.
//
// NOT here, because no tool of this group uses it: NaN and the infinities, subnormals and the traps of Underflow, rounding modes other than ROUND_HALF_EVEN, a power with a non-integer exponent, ln and exp, the
// comparison of a Decimal with a float.  The first use adds the case, with its proof.  Anything outside what is here throws (DecimalError and its subclasses), as Python raises.

#include <odl/devkit/bigint.hpp>

#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace odl::devkit {

/// What Python raises as decimal.DecimalException (and what this does not implement raises too).
class DecimalError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
/// decimal.InvalidOperation: the square root of a negative number, 0/0, the logarithm of a number that is not positive, a string that is not a number.
class DecimalInvalidOperation : public DecimalError {
public:
    using DecimalError::DecimalError;
};
/// decimal.DivisionByZero: x/0, 0 ** -n.
class DecimalDivisionByZero : public DecimalError {
public:
    using DecimalError::DecimalError;
};
/// decimal.Overflow: a result whose adjusted exponent exceeds Emax.
class DecimalOverflow : public DecimalError {
public:
    using DecimalError::DecimalError;
};

/// The part of decimal.Context that is used: Python's defaults are the defaults here.
struct DecimalContext {
    std::int64_t prec = 28;        ///< significant digits of a result
    std::int64_t emax = 999999;    ///< the largest adjusted exponent of a result
    std::int64_t emin = -999999;   ///< the smallest adjusted exponent of a normal result
    bool capitals = true;          ///< 'E' (not 'e') in str()
};

/// decimal.getcontext(): the current context of this thread.  Assigning to its members is Python's `getcontext().prec = 60`.
[[nodiscard]] DecimalContext& getcontext() noexcept;

/// `with decimal.localcontext() as ctx:` — the current context is saved, left in force (a copy to change through context()), and restored when this goes out of scope.
class LocalContext {
public:
    LocalContext();
    explicit LocalContext(std::int64_t prec);
    ~LocalContext();
    LocalContext(const LocalContext&) = delete;
    LocalContext& operator=(const LocalContext&) = delete;
    [[nodiscard]] DecimalContext& context() noexcept { return getcontext(); }

private:
    DecimalContext saved_;
};

class Decimal {
public:
    Decimal() = default;   // 0
    Decimal(std::int64_t value);   // NOLINT(google-explicit-constructor): Decimal(int), and Python lets an int stand beside a Decimal (`2 * x`); exact, whatever the context
    explicit Decimal(const BigInt& value);   // exact: the context's precision does not round a constructor's argument, as in Python
    /// decimal.Decimal("-12.5e3"): an optional sign, digits with an optional point (at least one digit), an optional exponent (e or E, an optional sign, digits).  Exact (the coefficient keeps its digits, the exponent
    /// counts the digits after the point); a zero keeps its exponent (Decimal("0.00") has exponent -2).  Throws DecimalInvalidOperation on anything else (Python also takes NaN, Infinity and underscores).
    [[nodiscard]] static Decimal from_string(std::string_view text);
    /// The number with this sign, coefficient (non-negative) and exponent, as it is, rounded to nothing.
    [[nodiscard]] static Decimal from_parts(bool negative, BigInt coefficient, std::int64_t exponent);

    [[nodiscard]] bool is_negative() const noexcept { return negative_; }   ///< the sign bit: true for -0
    [[nodiscard]] bool is_zero() const noexcept { return coefficient_.is_zero(); }
    [[nodiscard]] const BigInt& coefficient() const noexcept { return coefficient_; }   ///< the magnitude, as_tuple()'s digits
    [[nodiscard]] std::int64_t exponent() const noexcept { return exponent_; }
    [[nodiscard]] std::int64_t digits() const;     ///< the number of digits of the coefficient (1 for a zero)
    [[nodiscard]] std::int64_t adjusted() const;   ///< exponent + digits - 1: the exponent of the leading digit
    [[nodiscard]] bool is_integer() const;         ///< the value is an integer (Decimal("2.0") is, Decimal("2.5") is not)

    // The operations, each in the current context and correctly rounded (the power: see above).  The names are Python's.
    friend Decimal operator+(const Decimal& a, const Decimal& b);
    friend Decimal operator-(const Decimal& a, const Decimal& b);
    friend Decimal operator*(const Decimal& a, const Decimal& b);
    /// Throws DecimalDivisionByZero (x/0) or DecimalInvalidOperation (0/0).
    friend Decimal operator/(const Decimal& a, const Decimal& b);
    [[nodiscard]] Decimal operator-() const;   ///< Decimal.__neg__: 0 - x, except that -(+0) and -(-0) are +0
    [[nodiscard]] Decimal operator+() const;   ///< Decimal.__pos__: the number rounded to the context's precision (`+x`); -0 becomes +0
    [[nodiscard]] Decimal abs() const;         ///< abs(x): the operation, which rounds to the context (copy_abs() does not)
    [[nodiscard]] Decimal sqrt() const;        ///< correctly rounded; the ideal exponent of an exact root is floor(exponent / 2).  Throws DecimalInvalidOperation for a negative number (not for -0, whose root is -0).
    /// self ** exponent for an exponent that is an integer (Decimal("-4") and Decimal("4.0") are; Decimal("0.5") throws DecimalError, which this does not implement).  libmpdec's algorithm: in a working
    /// precision of prec + digits(exponent) + exp(exponent) + 2 (+1 for a negative exponent), the reciprocal for a negative exponent and then the left-to-right square-and-multiply, rounded to the context at the end.
    [[nodiscard]] Decimal pow(const Decimal& exponent) const;
    /// The base-10 logarithm, correctly rounded; exact (an integer) when the number is a power of ten.  Throws DecimalInvalidOperation unless the number is positive.
    [[nodiscard]] Decimal log10() const;

    [[nodiscard]] Decimal copy_abs() const;      ///< the same number without its sign, in no context
    [[nodiscard]] Decimal copy_negate() const;   ///< the same number with the sign turned, in no context (-0 and +0 swap)

    /// Numeric equality and order: 1.0 == 1.00, +0 == -0, 1E+2 > 99.
    friend bool operator==(const Decimal& a, const Decimal& b);
    friend std::strong_ordering operator<=>(const Decimal& a, const Decimal& b);
    explicit operator bool() const noexcept { return !is_zero(); }   ///< Decimal.__bool__: not zero

    /// float(Decimal): the double nearest to the exact value (ties to even), as float(str(d)) is; -0 gives -0.0, a value beyond the range gives an infinity, a tiny one a subnormal or zero (no error).
    [[nodiscard]] double to_double() const;
    /// str(Decimal): the General Decimal Arithmetic to-scientific-string, "0.001", "1E-7", "1.23E+5", "-0", "12.50".
    [[nodiscard]] std::string to_string() const;
    /// format(d, spec) for the specifications `.<precision>e`, `.<precision>E` and `.<precision>f` (and F): rounded half-even to precision + 1 significant digits (e) or to precision places (f), the exponent of
    /// an e-format with its sign and as many digits as it has ("1.235e+4", "5.9E-84"), the point and the digits as Python lays them out.  Any other specification throws DecimalError.
    [[nodiscard]] std::string format(std::string_view spec) const;

private:
    friend struct DecimalOps;   // decimal.cpp: the operations, which also run in a context of their own (the working context of the power)

    bool negative_ = false;
    BigInt coefficient_;   // never negative
    std::int64_t exponent_ = 0;
};

}  // namespace odl::devkit
