// odl/devkit/decimal.cpp — see decimal.hpp.

#include <odl/devkit/decimal.hpp>

#include <odl/devkit/pytext.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace odl::devkit {

namespace {

thread_local DecimalContext g_context;

// 10 ** k, k >= 0, kept: a decimal is rounded by dividing by a power of ten and a tool does that thousands of times by the same few
const BigInt& pow10(std::int64_t k) {
    static thread_local std::map<std::int64_t, BigInt> cache;
    auto it = cache.find(k);
    if (it == cache.end()) it = cache.emplace(k, BigInt::pow(BigInt(10), static_cast<unsigned>(k))).first;
    return it->second;
}

// the number of decimal digits of a non-negative integer, 1 for zero
std::int64_t digit_count(const BigInt& c) {
    if (c.is_zero()) return 1;
    // 2**(bits-1) <= c < 2**bits, so the digit count is within one of floor((bits - 1) log10(2)) + 1; the estimate is corrected against the powers of ten
    auto d = static_cast<std::int64_t>(static_cast<double>(c.bit_length() - 1) * 0.30102999566398119521) + 1;
    while (c >= pow10(d)) ++d;
    while (d > 1 && c < pow10(d - 1)) --d;
    return d;
}

// floor(a / b) for b > 0
std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && (a < 0)) --q;
    return q;
}

// Rounds the coefficient to at most `prec` digits, half-even: the digits dropped are compared with half a unit of the last digit kept, and an exact tie goes to the even digit.  `exp` follows.
void round_half_even(BigInt& coef, std::int64_t& exp, std::int64_t prec) {
    const std::int64_t d = digit_count(coef);
    if (d <= prec) return;
    const std::int64_t k = d - prec;
    BigInt::DivMod qr = BigInt::divmod(coef, pow10(k));
    const std::strong_ordering order = (qr.remainder + qr.remainder) <=> pow10(k);
    if (order > 0 || (order == 0 && qr.quotient.is_odd())) qr.quotient = qr.quotient + BigInt(1);
    exp += k;
    coef = std::move(qr.quotient);
    if (digit_count(coef) > prec) {   // 99...9 rounded up to 100...0: one digit too many, and the dropped digit is a zero
        coef = coef / BigInt(10);
        exp += 1;
    }
}

}  // namespace

// The operations, as functions of a context, so that the power can run its multiplications in a working context of its own.
struct DecimalOps {
    // the number, rounded to the context's precision and checked against its exponent range
    static Decimal finish(bool negative, BigInt coef, std::int64_t exp, const DecimalContext& ctx) {
        round_half_even(coef, exp, ctx.prec);
        if (!coef.is_zero()) {
            const std::int64_t adjusted = exp + digit_count(coef) - 1;
            if (adjusted > ctx.emax) throw DecimalOverflow("Decimal: the result is beyond Emax");
            if (adjusted < ctx.emin) throw DecimalError("Decimal: the result is below Emin (underflow is not implemented)");
        }
        return Decimal::from_parts(negative, std::move(coef), exp);
    }

    static Decimal add(const Decimal& a, const Decimal& b, bool flip_b, const DecimalContext& ctx) {
        const bool sa = a.negative_;
        const bool sb = flip_b ? !b.negative_ : b.negative_;
        const BigInt& ca = a.coefficient_;
        const BigInt& cb = b.coefficient_;
        const std::int64_t ea = a.exponent_;
        const std::int64_t eb = b.exponent_;
        if (ca.is_zero() && cb.is_zero()) {
            // the exponent is the smaller; the sign is the operands' when they agree and + when they differ (ROUND_HALF_EVEN)
            return Decimal::from_parts(sa == sb ? sa : false, BigInt(), std::min(ea, eb));
        }
        if (ca.is_zero() || cb.is_zero()) {
            // the other number, padded with zeros toward the smaller exponent as far as the precision has room for them
            const bool a_zero = ca.is_zero();
            const BigInt& c = a_zero ? cb : ca;
            const std::int64_t e = a_zero ? eb : ea;
            const std::int64_t e_zero = a_zero ? ea : eb;
            BigInt coef = c;
            std::int64_t exp = e;
            if (e_zero < e) {
                const std::int64_t room = std::max<std::int64_t>(ctx.prec - digit_count(c), 0);
                const std::int64_t shift = std::min(e - e_zero, room);
                coef = c * pow10(shift);
                exp = e - shift;
            }
            return finish(a_zero ? sb : sa, std::move(coef), exp, ctx);
        }
        const std::int64_t adj_a = ea + digit_count(ca) - 1;
        const std::int64_t adj_b = eb + digit_count(cb) - 1;
        const bool a_big = adj_a >= adj_b;   // the operand whose leading digit is the higher
        const BigInt& cbig = a_big ? ca : cb;
        const std::int64_t ebig = a_big ? ea : eb;
        const bool sbig = a_big ? sa : sb;
        const bool ssmall = a_big ? sb : sa;
        const std::int64_t adj_big = a_big ? adj_a : adj_b;
        const std::int64_t adj_small = a_big ? adj_b : adj_a;
        // The smaller operand matters only as a "sticky" amount below everything that can decide the rounding when its leading digit is below both the last digit of the larger operand and the half-unit position of
        // the result's last digit (one digit lower still when a subtraction borrows from a power of ten): then it is replaced by a unit at a position under both, which rounds exactly as the true amount would.
        const std::int64_t lowbound = std::min(ebig, adj_big - ctx.prec - 1);
        if (adj_small < lowbound) {
            const std::int64_t q = lowbound - 1;
            BigInt value = cbig * pow10(ebig - q);
            value = (sbig == ssmall) ? value + BigInt(1) : value - BigInt(1);
            return finish(sbig, std::move(value), q, ctx);
        }
        const std::int64_t e_min = std::min(ea, eb);
        BigInt va = ca * pow10(ea - e_min);
        BigInt vb = cb * pow10(eb - e_min);
        if (sa) va = -va;
        if (sb) vb = -vb;
        const BigInt sum = va + vb;
        return finish(sum.sign() < 0, sum.abs(), e_min, ctx);   // an exact zero is +0
    }

    static Decimal multiply(const Decimal& a, const Decimal& b, const DecimalContext& ctx) {
        return finish(a.negative_ != b.negative_, a.coefficient_ * b.coefficient_, a.exponent_ + b.exponent_, ctx);
    }

    static Decimal divide(const Decimal& a, const Decimal& b, const DecimalContext& ctx) {
        if (b.is_zero()) {
            if (a.is_zero()) throw DecimalInvalidOperation("Decimal: 0 / 0 is undefined");
            throw DecimalDivisionByZero("Decimal: division by zero");
        }
        const bool negative = a.negative_ != b.negative_;
        const std::int64_t ideal = a.exponent_ - b.exponent_;
        if (a.is_zero()) return Decimal::from_parts(negative, BigInt(), ideal);
        // a quotient of at least prec + 1 digits, with the remainder known: a nonzero remainder makes it inexact, and a digit 1 put after the quotient (a "sticky" digit) makes the rounding see that
        const std::int64_t la = digit_count(a.coefficient_);
        const std::int64_t lb = digit_count(b.coefficient_);
        const std::int64_t s = std::max<std::int64_t>(0, ctx.prec + 1 + lb - la);
        BigInt::DivMod qr = BigInt::divmod(a.coefficient_ * pow10(s), b.coefficient_);
        BigInt q = std::move(qr.quotient);
        std::int64_t e = ideal - s;
        if (qr.remainder.is_zero()) {
            // exact: the trailing zeros come off, down to the ideal exponent
            while (e < ideal) {
                BigInt::DivMod tenth = BigInt::divmod(q, BigInt(10));
                if (!tenth.remainder.is_zero()) break;
                q = std::move(tenth.quotient);
                ++e;
            }
        } else {
            q = q * BigInt(10) + BigInt(1);
            e -= 1;
        }
        return finish(negative, std::move(q), e, ctx);
    }

    static Decimal square_root(const Decimal& a, const DecimalContext& ctx) {
        if (a.negative_ && !a.is_zero()) throw DecimalInvalidOperation("Decimal: the square root of a negative number");
        const std::int64_t ideal = floor_div(a.exponent_, 2);
        if (a.is_zero()) return Decimal::from_parts(a.negative_, BigInt(), ideal);
        BigInt c = a.coefficient_;
        std::int64_t e = a.exponent_;
        if (e % 2 != 0) {   // an odd exponent: one more digit in the coefficient makes it even
            c = c * BigInt(10);
            e -= 1;
        }
        // root digits: the radicand c * 100**k has at least 2 (prec + 1) digits, so the root has at least prec + 1
        const std::int64_t d = digit_count(c);
        const std::int64_t k = std::max<std::int64_t>(0, (2 * (ctx.prec + 1) - d + 1) / 2);
        const BigInt radicand = c * pow10(2 * k);
        BigInt root = BigInt::isqrt(radicand);
        std::int64_t exp = e / 2 - k;
        if (root * root == radicand) {
            while (exp < ideal) {
                BigInt::DivMod tenth = BigInt::divmod(root, BigInt(10));
                if (!tenth.remainder.is_zero()) break;
                root = std::move(tenth.quotient);
                ++exp;
            }
        } else {
            root = root * BigInt(10) + BigInt(1);
            exp -= 1;
        }
        return finish(false, std::move(root), exp, ctx);
    }
};

DecimalContext& getcontext() noexcept { return g_context; }

LocalContext::LocalContext() : saved_(g_context) {}
LocalContext::LocalContext(std::int64_t prec) : saved_(g_context) { g_context.prec = prec; }
LocalContext::~LocalContext() { g_context = saved_; }

Decimal::Decimal(std::int64_t value) {
    negative_ = value < 0;
    coefficient_ = BigInt(value).abs();
}

Decimal::Decimal(const BigInt& value) : negative_(value.sign() < 0), coefficient_(value.abs()) {}

Decimal Decimal::from_parts(bool negative, BigInt coefficient, std::int64_t exponent) {
    if (coefficient.sign() < 0) throw std::invalid_argument("Decimal::from_parts: the coefficient is negative");
    Decimal out;
    out.negative_ = negative;
    out.coefficient_ = std::move(coefficient);
    out.exponent_ = exponent;
    return out;
}

Decimal Decimal::from_string(std::string_view text) {
    const std::string shown(text);
    const auto invalid = [&] { return DecimalInvalidOperation("Decimal: '" + shown + "' is not a number"); };
    std::size_t i = 0;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        negative = text[i] == '-';
        ++i;
    }
    std::string digits;
    std::int64_t fraction_digits = 0;
    bool seen_digit = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        digits.push_back(text[i++]);
        seen_digit = true;
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            digits.push_back(text[i++]);
            ++fraction_digits;
            seen_digit = true;
        }
    }
    if (!seen_digit) throw invalid();
    std::int64_t exponent = 0;
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        bool exponent_negative = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
            exponent_negative = text[i] == '-';
            ++i;
        }
        if (i >= text.size()) throw invalid();
        std::int64_t value = 0;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
            value = value * 10 + (text[i++] - '0');
            if (value > 999999999999999LL) throw invalid();
        }
        exponent = exponent_negative ? -value : value;
    }
    if (i != text.size()) throw invalid();
    return from_parts(negative, BigInt::from_decimal(digits), exponent - fraction_digits);
}

Decimal Decimal::from_python(std::string_view text) { return from_string(strip_py(text)); }

Decimal Decimal::from_double(double value) {
    if (!std::isfinite(value)) throw DecimalError("Decimal: NaN and the infinities are not implemented");
    const bool negative = std::signbit(value);
    const double magnitude = std::fabs(value);
    if (magnitude == 0.0) return from_parts(negative, BigInt(), 0);
    int binary_exponent = 0;
    const double fraction = std::frexp(magnitude, &binary_exponent);   // magnitude = fraction * 2**binary_exponent, 0.5 <= fraction < 1: exact
    auto mantissa = static_cast<std::uint64_t>(std::ldexp(fraction, 53));   // an integer below 2**53: exact
    std::int64_t exponent = static_cast<std::int64_t>(binary_exponent) - 53;
    while ((mantissa & 1U) == 0) {   // lowest terms, as float.as_integer_ratio() gives them: n / d, d a power of two (the mantissa is not zero)
        mantissa >>= 1U;
        ++exponent;
    }
    const BigInt n(static_cast<std::int64_t>(mantissa));
    if (exponent >= 0) return from_parts(negative, n.shifted_left(static_cast<std::size_t>(exponent)), 0);   // a whole number: its integer, exponent 0
    const auto k = static_cast<unsigned>(-exponent);
    return from_parts(negative, n * BigInt::pow(BigInt(5), k), -static_cast<std::int64_t>(k));   // n / 2**k = n * 5**k / 10**k
}

std::int64_t Decimal::digits() const { return digit_count(coefficient_); }

std::int64_t Decimal::adjusted() const { return exponent_ + digit_count(coefficient_) - 1; }

bool Decimal::is_integer() const {
    if (exponent_ >= 0 || coefficient_.is_zero()) return true;
    return (coefficient_ % pow10(-exponent_)).is_zero();
}

Decimal operator+(const Decimal& a, const Decimal& b) { return DecimalOps::add(a, b, false, getcontext()); }

Decimal operator-(const Decimal& a, const Decimal& b) { return DecimalOps::add(a, b, true, getcontext()); }

Decimal operator*(const Decimal& a, const Decimal& b) { return DecimalOps::multiply(a, b, getcontext()); }

Decimal operator/(const Decimal& a, const Decimal& b) { return DecimalOps::divide(a, b, getcontext()); }

Decimal Decimal::operator-() const {
    // 0 - x except for the sign of a zero: -(+0) and -(-0) are +0 (ROUND_HALF_EVEN)
    if (is_zero()) return DecimalOps::finish(false, BigInt(), exponent_, getcontext());
    return DecimalOps::finish(!negative_, coefficient_, exponent_, getcontext());
}

Decimal Decimal::operator+() const {
    if (is_zero()) return DecimalOps::finish(false, BigInt(), exponent_, getcontext());
    return DecimalOps::finish(negative_, coefficient_, exponent_, getcontext());
}

Decimal Decimal::abs() const { return negative_ ? -*this : +*this; }

Decimal Decimal::sqrt() const { return DecimalOps::square_root(*this, getcontext()); }

Decimal Decimal::copy_abs() const { return from_parts(false, coefficient_, exponent_); }

Decimal Decimal::copy_negate() const { return from_parts(!negative_, coefficient_, exponent_); }

Decimal Decimal::pow(const Decimal& exponent) const {
    const DecimalContext& ctx = getcontext();
    if (!exponent.is_integer()) {
        // a power with a fraction in the exponent (libmpdec's _mpd_qpow_real as the author remembers it): the base must be positive; exp(y ln x) in max(digits of x, prec) + 4 + 19 digits, rounded to the context
        if (is_zero()) {
            if (exponent.negative_) throw DecimalDivisionByZero("Decimal: 0 ** a negative number");
            return from_parts(false, BigInt(), 0);
        }
        if (negative_) throw DecimalInvalidOperation("Decimal: a negative number to a power that is not an integer");
        if (exponent_ <= 0 && coefficient_ == pow10(-exponent_)) {   // exactly one (1, 1.0, 1.00): one with prec - 1 zeros after it, as any power of one has
            return from_parts(false, pow10(ctx.prec - 1), -(ctx.prec - 1));
        }
        Decimal powered;
        {
            LocalContext working(std::max<std::int64_t>(digits(), ctx.prec) + 4 + 19);
            powered = (ln() * exponent).exp();
        }
        return DecimalOps::finish(false, powered.coefficient_, powered.exponent_, ctx);
    }
    const bool exponent_negative = exponent.negative_ && !exponent.is_zero();
    // the exponent as an integer
    BigInt n = exponent.coefficient_;
    if (exponent.exponent_ > 0) n = n * pow10(exponent.exponent_);
    else if (exponent.exponent_ < 0) n = n / pow10(-exponent.exponent_);
    const bool result_negative = negative_ && n.is_odd();
    if (is_zero()) {
        if (n.is_zero()) throw DecimalInvalidOperation("Decimal: 0 ** 0 is undefined");
        if (exponent_negative) throw DecimalDivisionByZero("Decimal: 0 ** a negative number");
        return from_parts(result_negative, BigInt(), 0);
    }
    if (n.is_zero()) return from_parts(false, BigInt(1), 0);
    if (!n.fits_int64() || n.bit_length() > 40) throw DecimalOverflow("Decimal::pow: the exponent is too large");
    // a base that is exactly +1 (1, 1.0, 1.000): the result is one, with as many zeros after it as the exponent times the base's digits after the point, up to the precision
    if (!negative_ && exponent_ <= 0 && coefficient_ == pow10(-exponent_)) {
        if (exponent_negative) return from_parts(false, BigInt(1), 0);
        const BigInt wanted = n * BigInt(-exponent_);
        const std::int64_t shift = wanted > BigInt(ctx.prec - 1) ? ctx.prec - 1 : wanted.to_int64();
        return from_parts(false, pow10(shift), -shift);
    }
    // libmpdec's working context: more digits than the result's, half-even, the same exponent range
    DecimalContext work = ctx;
    work.prec = ctx.prec + (exponent.digits() + exponent.exponent_ + 2) + (exponent_negative ? 1 : 0);
    Decimal base = copy_abs();
    if (exponent_negative) base = DecimalOps::divide(Decimal(1), base, work);
    const std::uint64_t count = static_cast<std::uint64_t>(n.abs().to_int64());
    Decimal result = base;
    for (std::uint64_t bit = std::uint64_t{1} << (63 - static_cast<unsigned>(std::countl_zero(count))); (bit >>= 1U) != 0;) {
        result = DecimalOps::multiply(result, result, work);
        if ((count & bit) != 0) result = DecimalOps::multiply(result, base, work);
    }
    return DecimalOps::finish(result_negative, result.coefficient_, result.exponent_, ctx);
}

bool operator==(const Decimal& a, const Decimal& b) { return (a <=> b) == 0; }

std::strong_ordering operator<=>(const Decimal& a, const Decimal& b) {
    const int sign_a = a.is_zero() ? 0 : (a.negative_ ? -1 : 1);
    const int sign_b = b.is_zero() ? 0 : (b.negative_ ? -1 : 1);
    if (sign_a != sign_b) return sign_a <=> sign_b;
    if (sign_a == 0) return std::strong_ordering::equal;
    // the same sign, neither zero: by the position of the leading digit, then digit by digit
    std::strong_ordering magnitude = std::strong_ordering::equal;
    const std::int64_t adjusted_a = a.adjusted();
    const std::int64_t adjusted_b = b.adjusted();
    if (adjusted_a != adjusted_b) {
        magnitude = adjusted_a <=> adjusted_b;
    } else {
        const std::int64_t e_min = std::min(a.exponent_, b.exponent_);
        magnitude = (a.coefficient_ * pow10(a.exponent_ - e_min)) <=> (b.coefficient_ * pow10(b.exponent_ - e_min));
    }
    return sign_a < 0 ? 0 <=> magnitude : magnitude;
}

double Decimal::to_double() const {
    const std::string text = (negative_ ? "-" : "") + coefficient_.to_decimal() + "e" + std::to_string(exponent_);
    return std::strtod(text.c_str(), nullptr);   // glibc's strtod rounds correctly, to nearest even, at any length
}

std::string Decimal::to_string() const {
    // the General Decimal Arithmetic's to-scientific-string: plain notation when the exponent is not positive and the adjusted exponent is at least -6, otherwise one digit before the point and an exponent
    const std::string digits_text = coefficient_.to_decimal();
    const std::int64_t length = static_cast<std::int64_t>(digits_text.size());
    const std::int64_t left_digits = exponent_ + length;
    std::int64_t dot_place = 1;
    if (exponent_ <= 0 && left_digits > -6) dot_place = left_digits;
    std::string int_part;
    std::string frac_part;
    if (dot_place <= 0) {
        int_part = "0";
        frac_part = "." + std::string(static_cast<std::size_t>(-dot_place), '0') + digits_text;
    } else if (dot_place >= length) {
        int_part = digits_text + std::string(static_cast<std::size_t>(dot_place - length), '0');
    } else {
        int_part = digits_text.substr(0, static_cast<std::size_t>(dot_place));
        frac_part = "." + digits_text.substr(static_cast<std::size_t>(dot_place));
    }
    std::string exp_part;
    if (left_digits != dot_place) {
        const std::int64_t shown = left_digits - dot_place;
        exp_part = std::string(getcontext().capitals ? "E" : "e") + (shown < 0 ? "-" : "+") + std::to_string(shown < 0 ? -shown : shown);
    }
    return std::string(negative_ ? "-" : "") + int_part + frac_part + exp_part;
}

namespace {

// Python's Decimal._rescale for a finite number, rounding half-even: the coefficient and exponent with exponent new_exp (digits are padded with zeros, or dropped with rounding)
void rescale(BigInt& coef, std::int64_t& exp, std::int64_t new_exp) {
    if (coef.is_zero()) {
        exp = new_exp;
        return;
    }
    if (exp >= new_exp) {
        coef = coef * pow10(exp - new_exp);
        exp = new_exp;
        return;
    }
    const std::int64_t k = new_exp - exp;   // digits to drop
    BigInt::DivMod qr = BigInt::divmod(coef, pow10(k));
    const std::strong_ordering order = (qr.remainder + qr.remainder) <=> pow10(k);
    if (order > 0 || (order == 0 && qr.quotient.is_odd())) qr.quotient = qr.quotient + BigInt(1);
    coef = std::move(qr.quotient);
    exp = new_exp;
}

}  // namespace

namespace {

// atanh(num / den) * 10**scale for |num / den| < 1/3, by the series t + t**3/3 + t**5/5 + ... in fixed point (an integer is the value times 10**scale; every division floors, so each term is off by
// less than one unit and the sum by less than twice the number of terms, which `terms` counts)
BigInt atanh_fixed(const BigInt& num, const BigInt& den, std::int64_t scale, std::int64_t& terms) {
    const BigInt& unit = pow10(scale);
    const BigInt t = (num.abs() * unit) / den;   // atanh is odd: the series is summed for the magnitude (a floor of a negative term would never reach zero)
    const BigInt t_squared = (t * t) / unit;
    BigInt power = t;
    BigInt sum;
    for (std::int64_t j = 0; !power.is_zero(); ++j) {
        sum = sum + power / BigInt(2 * j + 1);
        power = (power * t_squared) / unit;
        ++terms;
    }
    return num.sign() < 0 ? -sum : sum;
}

}  // namespace

Decimal Decimal::log10() const {
    const DecimalContext& ctx = getcontext();
    if (is_zero()) throw DecimalDivisionByZero("Decimal: the logarithm of zero");
    if (negative_) throw DecimalInvalidOperation("Decimal: the logarithm of a negative number");
    // x = c * 10**e = m * 10**E with m = c / 10**(d-1) in [1, 10) and E = e + d - 1
    const std::int64_t d = digit_count(coefficient_);
    const std::int64_t big_e = exponent_ + d - 1;
    const BigInt& lead = pow10(d - 1);
    if (coefficient_ == lead) return DecimalOps::finish(big_e < 0, BigInt(big_e).abs(), 0, ctx);   // a power of ten: the exponent, exactly
    // Otherwise the logarithm is irrational and has prec digits.  log10(m) = ln(m) / ln(10) in fixed point, ln(m) = k ln 2 + 2 atanh((y - 1) / (y + 1)) with y = m / 2**k near 1; the rounding of the value
    // is compared at both ends of its error bound and the working digits are doubled until the two ends round alike.
    double approx = 0.0;
    {
        const std::string head = coefficient_.to_decimal().substr(0, 15);
        approx = std::strtod(head.c_str(), nullptr) / std::pow(10.0, static_cast<double>(head.size() - 1));   // m to about 15 digits
    }
    const int k = approx < 1.4142 ? 0 : approx < 2.8284 ? 1 : approx < 5.6568 ? 2 : 3;
    for (std::int64_t guard = 10;; guard *= 2) {
        if (guard > 100000) throw DecimalError("Decimal::log10: no rounding could be decided");
        const std::int64_t scale = ctx.prec + guard + digit_count(BigInt(big_e).abs());
        std::int64_t terms = 0;
        const BigInt ln2 = BigInt(2) * atanh_fixed(BigInt(1), BigInt(3), scale, terms);
        const BigInt ln10 = BigInt(3) * ln2 + BigInt(2) * atanh_fixed(BigInt(1), BigInt(9), scale, terms);   // 10 = 2**3 * 5/4
        const BigInt denominator = lead * BigInt::pow(BigInt(2), static_cast<unsigned>(k));
        const BigInt ln_y = BigInt(2) * atanh_fixed(coefficient_ - denominator, coefficient_ + denominator, scale, terms);
        const BigInt ln_m = BigInt(k) * ln2 + ln_y;
        const BigInt fraction = (ln_m * pow10(scale)) / ln10;   // log10(m), in [0, 1)
        const BigInt value = BigInt(big_e) * pow10(scale) + fraction;
        const BigInt error = BigInt(16 * (terms + 16));   // each series is off by less than twice its terms; the sums and the quotient add a few times that
        const auto rounded = [&](const BigInt& v) {
            BigInt magnitude = v.abs();
            std::int64_t exp = -scale;
            round_half_even(magnitude, exp, ctx.prec);
            return std::pair<std::pair<bool, BigInt>, std::int64_t>{{v.sign() < 0, std::move(magnitude)}, exp};
        };
        const auto low = rounded(value - error);
        const auto high = rounded(value + error);
        if (low.first.first == high.first.first && low.first.second == high.first.second && low.second == high.second && !low.first.second.is_zero()) {
            return DecimalOps::finish(low.first.first, low.first.second, low.second, ctx);
        }
    }
}

namespace {

// ln 2 and ln 10 times 10**scale, from ln 2 = 2 atanh(1/3) and ln 10 = 3 ln 2 + 2 atanh(1/9) (10 = 2**3 * 5/4); `terms` counts the series' terms, as it does for atanh_fixed
struct Logs {
    BigInt ln2;
    BigInt ln10;
};

Logs logs_fixed(std::int64_t scale, std::int64_t& terms) {
    Logs out;
    out.ln2 = BigInt(2) * atanh_fixed(BigInt(1), BigInt(3), scale, terms);
    out.ln10 = BigInt(3) * out.ln2 + BigInt(2) * atanh_fixed(BigInt(1), BigInt(9), scale, terms);
    return out;
}

// the k in 0 .. 3 for which m / 2**k lies in [0.7071, 1.4142], m = coefficient / 10**(digits-1) in [1, 10): the series of atanh((y - 1)/(y + 1)) then converges at better than one digit in six terms
int halvings_of(const BigInt& coefficient) {
    const std::string head = coefficient.to_decimal().substr(0, 15);
    const double approx = std::strtod(head.c_str(), nullptr) / std::pow(10.0, static_cast<double>(head.size() - 1));   // m to about 15 digits
    return approx < 1.4142 ? 0 : approx < 2.8284 ? 1 : approx < 5.6568 ? 2 : 3;
}

// The number `value * 10**unit_exponent` rounded to the context's precision, when the two ends of its error bound (`error` units of the last place) round to the same number, which is then returned finished;
// nothing when they do not, and the caller works with more digits.  A result that is zero is never decided (a bound that reaches across zero has no sign).
std::optional<Decimal> decided(const BigInt& value, std::int64_t unit_exponent, const BigInt& error, const DecimalContext& ctx) {
    const auto rounded = [&](const BigInt& v) {
        BigInt magnitude = v.abs();
        std::int64_t exp = unit_exponent;
        round_half_even(magnitude, exp, ctx.prec);
        return std::tuple<bool, BigInt, std::int64_t>{v.sign() < 0, std::move(magnitude), exp};
    };
    const auto low = rounded(value - error);
    const auto high = rounded(value + error);
    if (std::get<0>(low) == std::get<0>(high) && std::get<1>(low) == std::get<1>(high) && std::get<2>(low) == std::get<2>(high) && !std::get<1>(low).is_zero()) {
        return DecimalOps::finish(std::get<0>(low), std::get<1>(low), std::get<2>(low), ctx);
    }
    return std::nullopt;
}

}  // namespace

Decimal Decimal::ln() const {
    const DecimalContext& ctx = getcontext();
    if (is_zero()) throw DecimalDivisionByZero("Decimal: the logarithm of zero");
    if (negative_) throw DecimalInvalidOperation("Decimal: the logarithm of a negative number");
    // x = c * 10**e = m * 10**E with m = c / 10**(d-1) in [1, 10) and E = e + d - 1:  ln x = E ln 10 + k ln 2 + 2 atanh((y - 1) / (y + 1)), y = m / 2**k near one
    const std::int64_t d = digit_count(coefficient_);
    const std::int64_t big_e = exponent_ + d - 1;
    const BigInt& lead = pow10(d - 1);
    if (big_e == 0 && coefficient_ == lead) return from_parts(false, BigInt(), 0);   // exactly one, however it is spelt: ln(1) = 0, exact, exponent 0
    const int k = halvings_of(coefficient_);
    for (std::int64_t guard = 10;; guard *= 2) {
        if (guard > 100000) throw DecimalError("Decimal::ln: no rounding could be decided");
        const std::int64_t scale = ctx.prec + guard + digit_count(BigInt(big_e).abs());
        std::int64_t terms = 0;
        const Logs logs = logs_fixed(scale, terms);
        const BigInt denominator = lead * BigInt::pow(BigInt(2), static_cast<unsigned>(k));
        const BigInt ln_y = BigInt(2) * atanh_fixed(coefficient_ - denominator, coefficient_ + denominator, scale, terms);
        const BigInt value = BigInt(big_e) * logs.ln10 + BigInt(k) * logs.ln2 + ln_y;
        const BigInt error = BigInt(16 * (terms + 16)) * (BigInt(big_e).abs() + BigInt(2));   // each series is off by less than twice its terms; E ln 10 repeats the error of ln 10 E times
        if (const std::optional<Decimal> result = decided(value, -scale, error, ctx)) return *result;
    }
}

Decimal Decimal::exp() const {
    const DecimalContext& ctx = getcontext();
    if (is_zero()) return from_parts(false, BigInt(1), 0);   // exp(0) = 1, exact, exponent 0, whatever the zero
    if (adjusted() > 6) {   // |x| of 10**7 or more is far beyond the exponent range of any context
        if (!negative_) throw DecimalOverflow("Decimal: exp of a number beyond the exponent range");
        throw DecimalError("Decimal: exp of a number beyond the exponent range underflows (underflow is not implemented)");
    }
    // x = q ln 10 + r with q the integer nearest x / ln 10 (found from a 40-digit ln 10: any integer within one of it would serve), |r| <= 1.16 or so: exp(x) = 10**q * exp(r), exp(r) by its series
    std::int64_t unused_terms = 0;
    const BigInt ln10_coarse = logs_fixed(40, unused_terms).ln10;
    const BigInt x_coarse = exponent_ + 40 >= 0 ? coefficient_ * pow10(exponent_ + 40) : coefficient_ / pow10(-(exponent_ + 40));
    const BigInt x_signed = negative_ ? -x_coarse : x_coarse;
    const BigInt q = (x_signed * BigInt(2) + ln10_coarse) / (ln10_coarse * BigInt(2));   // floor(x / ln 10 + 1/2)
    const BigInt q_magnitude = q.abs();
    const std::int64_t q_int = q.to_int64();
    for (std::int64_t guard = 10;; guard *= 2) {
        if (guard > 100000) throw DecimalError("Decimal::exp: no rounding could be decided");
        const std::int64_t scale = ctx.prec + guard + digit_count(q_magnitude) + 2;
        const BigInt& unit = pow10(scale);
        std::int64_t ln_terms = 0;
        const BigInt ln10 = logs_fixed(scale, ln_terms).ln10;
        const BigInt x_fixed = exponent_ + scale >= 0 ? coefficient_ * pow10(exponent_ + scale) : coefficient_ / pow10(-(exponent_ + scale));
        const BigInt r = (negative_ ? -x_fixed : x_fixed) - q * ln10;
        const BigInt r_magnitude = r.abs();
        // exp(r) = 1 + r + r**2/2! + ...: the terms are made for |r| (they are then never negative, and each floor reaches zero) and added with the sign r**n has
        BigInt term = unit;
        BigInt sum = unit;
        std::int64_t exp_terms = 0;
        for (std::int64_t n = 1;; ++n) {
            term = (term * r_magnitude) / (unit * BigInt(n));
            if (term.is_zero()) break;
            sum = (r.sign() < 0 && n % 2 == 1) ? sum - term : sum + term;
            ++exp_terms;
        }
        // the error of r is |q| times that of ln 10 (and a unit for x itself); exp(r) <= 3.2 times it; the series adds at most two units a term
        const BigInt error = BigInt(4) * (q_magnitude + BigInt(2)) * BigInt(16 * (ln_terms + 16)) + BigInt(8 * (exp_terms + 16));
        if (const std::optional<Decimal> result = decided(sum, q_int - scale, error, ctx)) return *result;
    }
}

std::string Decimal::format(std::string_view spec) const {
    // `.<precision><type>` with type e, E, f or F: the part of Python's format specification language that the tools use
    if (spec.size() < 3 || spec.front() != '.') throw DecimalError("Decimal::format: only the specifications .<precision>e, .E, .f and .F are implemented, not '" + std::string(spec) + "'");
    const char type = spec.back();
    if (type != 'e' && type != 'E' && type != 'f' && type != 'F') throw DecimalError("Decimal::format: the type '" + std::string(1, type) + "' is not implemented");
    std::int64_t precision = 0;
    for (std::size_t i = 1; i + 1 < spec.size(); ++i) {
        if (spec[i] < '0' || spec[i] > '9') throw DecimalError("Decimal::format: '" + std::string(spec) + "' is not a precision");
        precision = precision * 10 + (spec[i] - '0');
        if (precision > 1000000) throw DecimalError("Decimal::format: the precision is too large");
    }
    const bool scientific = type == 'e' || type == 'E';
    BigInt coef = coefficient_;
    std::int64_t exp = exponent_;
    if (scientific) {
        if (!coef.is_zero()) {
            // round to precision + 1 significant digits (a second pass when the rounding carried into a new digit)
            const std::int64_t before = exp + digit_count(coef) - 1;
            rescale(coef, exp, before + 1 - (precision + 1));
            const std::int64_t after = exp + digit_count(coef) - 1;
            if (after != before) rescale(coef, exp, after + 1 - (precision + 1));
        }
    } else {
        rescale(coef, exp, -precision);
    }
    std::string digits_text = coef.to_decimal();
    const std::int64_t length = static_cast<std::int64_t>(digits_text.size());
    const std::int64_t left_digits = exp + length;
    std::int64_t dot_place = 1;
    if (!scientific) dot_place = left_digits;
    else if (coef.is_zero()) dot_place = 1 - precision;
    // (Python's layout has a third case, a point beyond the last digit; e, E, f and F never reach it: an f has been rescaled to the exponent -precision, so its point is at or before the last digit, and an e
    // puts the point after the first of its precision + 1 digits, or, for a zero, of the one digit "0")
    std::string int_part;
    std::string frac_part;
    if (dot_place < 0) {
        int_part = "0";
        frac_part = std::string(static_cast<std::size_t>(-dot_place), '0') + digits_text;
    } else {
        int_part = digits_text.substr(0, static_cast<std::size_t>(dot_place));
        if (int_part.empty()) int_part = "0";
        frac_part = digits_text.substr(static_cast<std::size_t>(dot_place));
    }
    const std::int64_t shown_exponent = left_digits - dot_place;
    std::string out = std::string(negative_ ? "-" : "") + int_part;
    if (!frac_part.empty()) out += "." + frac_part;
    if (shown_exponent != 0 || scientific) out += std::string(1, type == 'E' ? 'E' : 'e') + (shown_exponent < 0 ? "-" : "+") + std::to_string(shown_exponent < 0 ? -shown_exponent : shown_exponent);
    return out;
}

}  // namespace odl::devkit
