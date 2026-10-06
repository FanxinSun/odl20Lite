// odl/devkit/bigint.cpp — see bigint.hpp.

#include <odl/devkit/bigint.hpp>

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

namespace odl::devkit {

namespace {

using Limb = std::uint32_t;
using Mag = std::vector<Limb>;   // a magnitude: least significant limb first, no limb of zero at the top
constexpr unsigned kLimbBits = 32;
constexpr Limb kDecimalChunk = 1000000000U;   // 10**9, the largest power of ten in a limb
constexpr unsigned kDecimalChunkDigits = 9;

void trim(Mag& m) {
    while (!m.empty() && m.back() == 0) m.pop_back();
}

int compare_magnitudes(const Mag& a, const Mag& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (std::size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

Mag add_magnitudes(const Mag& a, const Mag& b) {
    const Mag& longer = a.size() >= b.size() ? a : b;
    const Mag& shorter = a.size() >= b.size() ? b : a;
    Mag out;
    out.reserve(longer.size() + 1);
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < longer.size(); ++i) {
        const std::uint64_t sum = std::uint64_t{longer[i]} + (i < shorter.size() ? shorter[i] : Limb{0}) + carry;
        out.push_back(static_cast<Limb>(sum));
        carry = sum >> kLimbBits;
    }
    if (carry != 0) out.push_back(static_cast<Limb>(carry));
    return out;
}

// a - b, for a >= b
Mag subtract_magnitudes(const Mag& a, const Mag& b) {
    Mag out;
    out.reserve(a.size());
    std::int64_t borrow = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const std::int64_t diff = std::int64_t{a[i]} - (i < b.size() ? std::int64_t{b[i]} : std::int64_t{0}) - borrow;
        borrow = diff < 0 ? 1 : 0;
        out.push_back(static_cast<Limb>(diff));   // modulo 2**32: that is what borrowing 2**32 does to a negative difference
    }
    trim(out);
    return out;
}

Mag multiply_magnitudes(const Mag& a, const Mag& b) {
    Mag out(a.size() + b.size(), 0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j) {
            const std::uint64_t cur = std::uint64_t{out[i + j]} + std::uint64_t{a[i]} * b[j] + carry;   // at most 2**64 - 1
            out[i + j] = static_cast<Limb>(cur);
            carry = cur >> kLimbBits;
        }
        for (std::size_t k = i + b.size(); carry != 0; ++k) {
            const std::uint64_t cur = std::uint64_t{out[k]} + carry;
            out[k] = static_cast<Limb>(cur);
            carry = cur >> kLimbBits;
        }
    }
    trim(out);
    return out;
}

// m = m * factor + addend, in place
void multiply_add_small(Mag& m, Limb factor, Limb addend) {
    std::uint64_t carry = addend;
    for (Limb& limb : m) {
        const std::uint64_t cur = std::uint64_t{limb} * factor + carry;
        limb = static_cast<Limb>(cur);
        carry = cur >> kLimbBits;
    }
    if (carry != 0) m.push_back(static_cast<Limb>(carry));
}

// m = m / divisor in place, the remainder returned; divisor != 0
Limb divide_small(Mag& m, Limb divisor) {
    std::uint64_t remainder = 0;
    for (std::size_t i = m.size(); i-- > 0;) {
        const std::uint64_t cur = (remainder << kLimbBits) | m[i];
        m[i] = static_cast<Limb>(cur / divisor);
        remainder = cur % divisor;
    }
    trim(m);
    return static_cast<Limb>(remainder);
}

Mag shift_magnitude_left(const Mag& a, std::size_t n) {
    if (a.empty()) return {};
    const std::size_t whole = n / kLimbBits;
    const auto bits = static_cast<unsigned>(n % kLimbBits);
    Mag out(whole, 0);
    out.reserve(whole + a.size() + 1);
    Limb carry = 0;
    for (const Limb x : a) {
        if (bits == 0) {
            out.push_back(x);
        } else {
            out.push_back(static_cast<Limb>((std::uint64_t{x} << bits) | carry));
            carry = x >> (kLimbBits - bits);
        }
    }
    if (carry != 0) out.push_back(carry);
    return out;
}

Mag shift_magnitude_right(const Mag& a, std::size_t n) {
    const std::size_t whole = n / kLimbBits;
    const auto bits = static_cast<unsigned>(n % kLimbBits);
    if (whole >= a.size()) return {};
    Mag out;
    out.reserve(a.size() - whole);
    for (std::size_t i = whole; i < a.size(); ++i) {
        std::uint64_t cur = a[i];
        if (bits != 0) {
            cur >>= bits;
            if (i + 1 < a.size()) cur |= std::uint64_t{a[i + 1]} << (kLimbBits - bits);
        }
        out.push_back(static_cast<Limb>(cur));
    }
    trim(out);
    return out;
}

// quotient and remainder of magnitudes; b != 0
void divide_magnitudes(const Mag& a, const Mag& b, Mag& quotient, Mag& remainder) {
    if (compare_magnitudes(a, b) < 0) {
        quotient.clear();
        remainder = a;
        return;
    }
    if (b.size() == 1) {
        quotient = a;
        const Limb r = divide_small(quotient, b.front());
        remainder.clear();
        if (r != 0) remainder.push_back(r);
        return;
    }
    // bit by bit, from the top: remainder = 2 * remainder + (the next bit of a); subtract b whenever it fits and set that bit of the quotient
    quotient.assign(a.size(), 0);
    remainder.clear();
    for (std::size_t i = a.size() * kLimbBits; i-- > 0;) {
        Limb carry = (a[i / kLimbBits] >> (i % kLimbBits)) & 1U;
        for (Limb& limb : remainder) {
            const Limb next = limb >> (kLimbBits - 1);
            limb = (limb << 1) | carry;
            carry = next;
        }
        if (carry != 0) remainder.push_back(carry);
        if (compare_magnitudes(remainder, b) >= 0) {
            remainder = subtract_magnitudes(remainder, b);
            quotient[i / kLimbBits] |= Limb{1} << (i % kLimbBits);
        }
    }
    trim(quotient);
}

}  // namespace

BigInt BigInt::from_magnitude(std::vector<Limb> limbs, bool negative) {
    trim(limbs);
    BigInt out;
    out.negative_ = negative && !limbs.empty();
    out.limbs_ = std::move(limbs);
    return out;
}

BigInt::BigInt(std::int64_t value) {
    // the magnitude as unsigned: negating INT64_MIN as a signed number would overflow
    const std::uint64_t magnitude = value < 0 ? ~static_cast<std::uint64_t>(value) + 1U : static_cast<std::uint64_t>(value);
    if (magnitude != 0) {
        limbs_.push_back(static_cast<Limb>(magnitude));
        if ((magnitude >> kLimbBits) != 0) limbs_.push_back(static_cast<Limb>(magnitude >> kLimbBits));
    }
    negative_ = value < 0;
}

BigInt BigInt::from_decimal(std::string_view text) {
    std::size_t i = 0;
    bool negative = false;
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
        negative = text.front() == '-';
        i = 1;
    }
    if (i == text.size()) throw std::invalid_argument("BigInt::from_decimal: no digits in '" + std::string(text) + "'");
    for (std::size_t k = i; k < text.size(); ++k) {
        if (text[k] < '0' || text[k] > '9') throw std::invalid_argument("BigInt::from_decimal: '" + std::string(text) + "' is not a decimal integer");
    }
    Mag m;
    while (i < text.size()) {
        const std::size_t take = std::min<std::size_t>(kDecimalChunkDigits, text.size() - i);
        Limb chunk = 0;
        Limb scale = 1;
        for (std::size_t k = 0; k < take; ++k) {
            chunk = chunk * 10U + static_cast<Limb>(text[i + k] - '0');
            scale *= 10U;
        }
        multiply_add_small(m, scale, chunk);
        i += take;
    }
    return from_magnitude(std::move(m), negative);
}

std::string BigInt::to_decimal() const {
    if (limbs_.empty()) return "0";
    Mag work = limbs_;
    std::vector<Limb> chunks;   // base 10**9, least significant first
    while (!work.empty()) chunks.push_back(divide_small(work, kDecimalChunk));
    std::string out = negative_ ? "-" : "";
    out += std::to_string(chunks.back());
    for (std::size_t i = chunks.size() - 1; i-- > 0;) {
        const std::string digits = std::to_string(chunks[i]);
        out.append(kDecimalChunkDigits - digits.size(), '0');
        out += digits;
    }
    return out;
}

std::size_t BigInt::bit_length() const noexcept {
    if (limbs_.empty()) return 0;
    return (limbs_.size() - 1) * kLimbBits + (kLimbBits - static_cast<unsigned>(std::countl_zero(limbs_.back())));
}

BigInt BigInt::abs() const { return from_magnitude(limbs_, false); }

bool BigInt::fits_int64() const noexcept {
    if (limbs_.size() > 2) return false;
    std::uint64_t magnitude = 0;
    for (std::size_t i = limbs_.size(); i-- > 0;) magnitude = (magnitude << kLimbBits) | limbs_[i];
    const auto largest = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    return negative_ ? magnitude <= largest + 1U : magnitude <= largest;
}

std::int64_t BigInt::to_int64() const {
    if (!fits_int64()) throw std::overflow_error("BigInt::to_int64: " + to_decimal() + " does not fit in 64 bits");
    std::uint64_t magnitude = 0;
    for (std::size_t i = limbs_.size(); i-- > 0;) magnitude = (magnitude << kLimbBits) | limbs_[i];
    return negative_ ? static_cast<std::int64_t>(~magnitude + 1U) : static_cast<std::int64_t>(magnitude);
}

BigInt BigInt::operator-() const { return from_magnitude(limbs_, !negative_); }

BigInt operator+(const BigInt& a, const BigInt& b) {
    if (a.negative_ == b.negative_) return BigInt::from_magnitude(add_magnitudes(a.limbs_, b.limbs_), a.negative_);
    const int order = compare_magnitudes(a.limbs_, b.limbs_);
    if (order == 0) return BigInt();
    if (order > 0) return BigInt::from_magnitude(subtract_magnitudes(a.limbs_, b.limbs_), a.negative_);
    return BigInt::from_magnitude(subtract_magnitudes(b.limbs_, a.limbs_), b.negative_);
}

BigInt operator-(const BigInt& a, const BigInt& b) { return a + (-b); }

BigInt operator*(const BigInt& a, const BigInt& b) { return BigInt::from_magnitude(multiply_magnitudes(a.limbs_, b.limbs_), a.negative_ != b.negative_); }

BigInt operator/(const BigInt& a, const BigInt& b) { return BigInt::divmod(a, b).quotient; }

BigInt operator%(const BigInt& a, const BigInt& b) { return BigInt::divmod(a, b).remainder; }

BigInt::DivMod BigInt::divmod(const BigInt& a, const BigInt& b) {
    if (b.is_zero()) throw std::domain_error("BigInt: division by zero");
    Mag q;
    Mag r;
    divide_magnitudes(a.limbs_, b.limbs_, q, r);
    if (a.negative_ == b.negative_) return {from_magnitude(std::move(q), false), from_magnitude(std::move(r), b.negative_)};
    // the signs differ: the quotient is the floor of a negative number, so an inexact one is one below the truncated one, and the remainder is what is left to the next multiple of b
    if (r.empty()) return {from_magnitude(std::move(q), true), BigInt()};
    return {from_magnitude(add_magnitudes(q, Mag{1}), true), from_magnitude(subtract_magnitudes(b.limbs_, r), b.negative_)};
}

BigInt BigInt::shifted_left(std::size_t n) const { return from_magnitude(shift_magnitude_left(limbs_, n), negative_); }

BigInt BigInt::shifted_right(std::size_t n) const {
    if (!negative_) return from_magnitude(shift_magnitude_right(limbs_, n), false);
    // floor(-m / 2**n) = -(floor((m - 1) / 2**n) + 1)
    const Mag below = subtract_magnitudes(limbs_, Mag{1});
    return from_magnitude(add_magnitudes(shift_magnitude_right(below, n), Mag{1}), true);
}

BigInt BigInt::gcd(const BigInt& a, const BigInt& b) {
    Mag x = a.limbs_;
    Mag y = b.limbs_;
    while (!y.empty()) {
        Mag q;
        Mag r;
        divide_magnitudes(x, y, q, r);
        x = std::move(y);
        y = std::move(r);
    }
    return from_magnitude(std::move(x), false);
}

BigInt BigInt::pow(const BigInt& base, unsigned exponent) {
    BigInt result(1);
    BigInt square = base;
    while (exponent != 0) {
        if ((exponent & 1U) != 0) result = result * square;
        exponent >>= 1U;
        if (exponent != 0) square = square * square;
    }
    return result;
}

std::strong_ordering operator<=>(const BigInt& a, const BigInt& b) noexcept {
    if (a.sign() != b.sign()) return a.sign() <=> b.sign();
    const int order = compare_magnitudes(a.limbs_, b.limbs_);
    const int signed_order = a.negative_ ? -order : order;
    return signed_order <=> 0;
}

}  // namespace odl::devkit
