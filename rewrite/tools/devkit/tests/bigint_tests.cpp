// The devkit's BigInt against values written down by hand, against 128-bit machine arithmetic on random operands (the exchange format is the decimal text, so that the two sides share no code), against a
// schoolbook multiplier on decimal digits (a different base, a different algorithm) and against identities that hold for any numbers (a == q*b + r, gcd of Fibonacci numbers).  The random operands come
// from std::mt19937_64 with a fixed seed: the engine's output is specified by the standard, so the numbers are the same on every platform.

#include <catch2/catch_test_macros.hpp>

#include "bignum_printers.hpp"

#include <odl/devkit/bigint.hpp>

#include <algorithm>
#include <compare>
#include <cstdint>
#include <limits>
#include <numeric>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using odl::devkit::BigInt;


namespace {

__extension__ typedef __int128 Int128;
__extension__ typedef unsigned __int128 UInt128;

// decimal text of a 128-bit integer, made here and independently of BigInt
std::string text(Int128 v) {
    if (v == 0) return "0";
    const bool negative = v < 0;
    UInt128 u = negative ? UInt128{0} - static_cast<UInt128>(v) : static_cast<UInt128>(v);
    std::string s;
    while (u != 0) {
        s.push_back(static_cast<char>('0' + static_cast<int>(u % 10)));
        u /= 10;
    }
    if (negative) s.push_back('-');
    std::reverse(s.begin(), s.end());
    return s;
}

BigInt big(Int128 v) { return BigInt::from_decimal(text(v)); }

// a random integer of at most `bits` bits (1..126), of either sign, with the engine's low-order bits as they come
Int128 random_int(std::mt19937_64& rng, unsigned bits) {
    UInt128 u = (static_cast<UInt128>(rng()) << 64) | rng();
    if (bits < 128) u &= (UInt128{1} << bits) - 1U;
    Int128 v = static_cast<Int128>(u);
    if ((rng() & 1U) != 0) v = -v;
    return v;
}

// Python's floor division of 128-bit integers
void floor_divmod(Int128 a, Int128 b, Int128& q, Int128& r) {
    q = a / b;
    r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        q -= 1;
        r += b;
    }
}

// the product of two non-negative integers written in decimal, by the schoolbook method on digits: no limbs, no carries between limbs, nothing shared with BigInt
std::string multiply_decimal(const std::string& a, const std::string& b) {
    std::vector<int> digits(a.size() + b.size(), 0);   // least significant first
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b.size(); ++j) {
            digits[i + j] += (a[a.size() - 1 - i] - '0') * (b[b.size() - 1 - j] - '0');
        }
    }
    int carry = 0;
    for (int& d : digits) {
        d += carry;
        carry = d / 10;
        d %= 10;
    }
    std::string out;
    for (std::size_t i = digits.size(); i-- > 0;) out.push_back(static_cast<char>('0' + digits[i]));
    const std::size_t first = out.find_first_not_of('0');
    return first == std::string::npos ? "0" : out.substr(first);
}

BigInt factorial(unsigned n) {
    BigInt out(1);
    for (unsigned k = 2; k <= n; ++k) out = out * BigInt(static_cast<std::int64_t>(k));
    return out;
}

}  // namespace

TEST_CASE("BigInt is built from machine integers and from decimal text, and prints both back", "[devkit][bigint]") {
    CHECK(BigInt().to_decimal() == "0");
    CHECK(BigInt(0).to_decimal() == "0");
    CHECK(BigInt(1).to_decimal() == "1");
    CHECK(BigInt(-1).to_decimal() == "-1");
    CHECK(BigInt(std::numeric_limits<std::int64_t>::max()).to_decimal() == "9223372036854775807");
    CHECK(BigInt(std::numeric_limits<std::int64_t>::min()).to_decimal() == "-9223372036854775808");   // negating the minimum as a signed number would overflow
    CHECK(BigInt(4294967295LL).to_decimal() == "4294967295");                                             // 2**32 - 1: one limb, full
    CHECK(BigInt(4294967296LL).to_decimal() == "4294967296");                                             // 2**32: two limbs
    CHECK(BigInt(-4294967296LL).to_decimal() == "-4294967296");
    CHECK(BigInt(1000000000LL).to_decimal() == "1000000000");                                             // exactly one decimal chunk
    CHECK(BigInt(999999999LL).to_decimal() == "999999999");
    CHECK(BigInt(1000000001LL).to_decimal() == "1000000001");                                             // the zero padding of the lower chunk
    CHECK(BigInt(1000000000000000000LL).to_decimal() == "1000000000000000000");

    // from text: signs, leading zeros, zero in both signs
    CHECK(BigInt::from_decimal("0") == BigInt());
    CHECK(BigInt::from_decimal("-0") == BigInt());
    CHECK(BigInt::from_decimal("+0") == BigInt());
    CHECK(BigInt::from_decimal("-0").sign() == 0);
    CHECK(BigInt::from_decimal("+5") == BigInt(5));
    CHECK(BigInt::from_decimal("-5") == BigInt(-5));
    CHECK(BigInt::from_decimal("000123") == BigInt(123));
    CHECK(BigInt::from_decimal("0000000000").is_zero());
    CHECK(BigInt::from_decimal("9223372036854775808").to_decimal() == "9223372036854775808");         // 2**63
    CHECK(BigInt::from_decimal("18446744073709551616").to_decimal() == "18446744073709551616");       // 2**64
    CHECK(BigInt::from_decimal("-18446744073709551616").to_decimal() == "-18446744073709551616");
    CHECK(BigInt::from_decimal("1000000000000000000000000000000").to_decimal() == "1000000000000000000000000000000");
    CHECK(BigInt::from_decimal("000000000123456789012345678901234567890").to_decimal() == "123456789012345678901234567890");

    // refused
    for (const char* bad : {"", "-", "+", "--1", "1-", "1_000", " 1", "1 ", "0x10", "1e3", "1.0", "\xEF\xBC\x91\xEF\xBC\x92", "a"}) {
        INFO("text '" << bad << "'");
        CHECK_THROWS_AS(BigInt::from_decimal(bad), std::invalid_argument);
    }
}

TEST_CASE("BigInt holds the large values written down by hand: powers of two and ten, and factorials", "[devkit][bigint]") {
    CHECK(BigInt::pow(2, 64).to_decimal() == "18446744073709551616");
    CHECK(BigInt::pow(2, 100).to_decimal() == "1267650600228229401496703205376");
    CHECK(BigInt::pow(2, 128).to_decimal() == "340282366920938463463374607431768211456");
    CHECK(BigInt::pow(10, 30).to_decimal() == "1000000000000000000000000000000");
    CHECK(factorial(20).to_decimal() == "2432902008176640000");
    CHECK(factorial(25).to_decimal() == "15511210043330985984000000");
    CHECK(factorial(30).to_decimal() == "265252859812191058636308480000000");
    // 100!: 158 digits, 24 trailing zeros (floor(100/5) + floor(100/25)), and the leading digits 9.33262154439441e157
    const std::string f100 = factorial(100).to_decimal();
    CHECK(f100.size() == 158);
    CHECK(f100.compare(0, 15, "933262154439441") == 0);
    CHECK(f100.find_last_not_of('0') == f100.size() - 25);
    // and it is divisible by 2**97 (Legendre: 50 + 25 + 12 + 6 + 3 + 1) and by no 2**98
    const BigInt f = factorial(100);
    CHECK((f % BigInt::pow(2, 97)).is_zero());
    CHECK_FALSE((f % BigInt::pow(2, 98)).is_zero());
}

TEST_CASE("BigInt addition, subtraction and multiplication agree with 128-bit arithmetic on random operands of every size", "[devkit][bigint]") {
    std::mt19937_64 rng(0x0D1F00D5EEDULL);
    for (int round = 0; round < 4000; ++round) {
        const unsigned bits = 1U + static_cast<unsigned>(rng() % 125U);   // 1 .. 125, so that a sum of two fits
        const Int128 a = random_int(rng, bits);
        const Int128 b = random_int(rng, 1U + static_cast<unsigned>(rng() % 125U));
        INFO(text(a) << " and " << text(b));
        CHECK((big(a) + big(b)).to_decimal() == text(a + b));
        CHECK((big(a) - big(b)).to_decimal() == text(a - b));
        const Int128 small_a = random_int(rng, 1U + static_cast<unsigned>(rng() % 62U));
        const Int128 small_b = random_int(rng, 1U + static_cast<unsigned>(rng() % 62U));
        CHECK((big(small_a) * big(small_b)).to_decimal() == text(small_a * small_b));
        CHECK((-big(a)).to_decimal() == text(-a));
    }
    // the corners of the limb arithmetic: carries across a whole run of full limbs, and a borrow across it
    const BigInt full = BigInt::pow(2, 96) - BigInt(1);   // three full limbs
    CHECK((full + BigInt(1)) == BigInt::pow(2, 96));
    CHECK((BigInt::pow(2, 96) - BigInt(1)) == full);
    CHECK((full * full + full + full + BigInt(1)) == BigInt::pow(2, 192));   // (x-1)^2 + 2(x-1) + 1 = x^2 with x = 2**96
    CHECK((full * BigInt(0)).is_zero());
    CHECK((BigInt(0) * full).is_zero());
    CHECK((full + (-full)).is_zero());
    CHECK((full - full).sign() == 0);
    CHECK((-full - full) == -(full + full));
    CHECK((BigInt(5) + BigInt(-7)) == BigInt(-2));
    CHECK((BigInt(-5) + BigInt(7)) == BigInt(2));
    CHECK((BigInt(-5) - BigInt(-5)).is_zero());
    CHECK((BigInt(-3) * BigInt(4)) == BigInt(-12));
    CHECK((BigInt(-3) * BigInt(-4)) == BigInt(12));
    BigInt acc(7);
    acc += BigInt(3);
    acc -= BigInt(4);
    acc *= BigInt(10);
    CHECK(acc == BigInt(60));
}

TEST_CASE("BigInt multiplication agrees with a schoolbook multiplier on decimal digits, for numbers of hundreds of digits", "[devkit][bigint]") {
    std::mt19937_64 rng(0xC0FFEE1234ULL);
    for (int round = 0; round < 200; ++round) {
        std::string a;
        std::string b;
        for (std::size_t i = 0, n = 1 + rng() % 400; i < n; ++i) a.push_back(static_cast<char>('0' + rng() % 10));
        for (std::size_t i = 0, n = 1 + rng() % 400; i < n; ++i) b.push_back(static_cast<char>('0' + rng() % 10));
        INFO(a << " * " << b);
        const std::string expected = multiply_decimal(a, b);
        const BigInt x = BigInt::from_decimal(a);
        const BigInt y = BigInt::from_decimal(b);
        CHECK((x * y).to_decimal() == expected);
        CHECK((y * x).to_decimal() == expected);
        // and the division undoes it when the divisor is not zero
        if (!y.is_zero()) {
            const BigInt::DivMod qr = BigInt::divmod(x * y, y);
            CHECK(qr.quotient == x);
            CHECK(qr.remainder.is_zero());
        }
    }
    // 99...9 squared is 99...98 00...01: carries through every limb
    const std::string nines(300, '9');
    const std::string expected = std::string(299, '9') + "8" + std::string(299, '0') + "1";
    CHECK((BigInt::from_decimal(nines) * BigInt::from_decimal(nines)).to_decimal() == expected);
}

TEST_CASE("BigInt divmod is Python's: floor division, the remainder takes the divisor's sign", "[devkit][bigint]") {
    struct Row {
        std::int64_t a, b, q, r;
    };
    // derived by hand from the definition a == q*b + r with r of the sign of b and |r| < |b|
    const Row table[] = {
        {7, 2, 3, 1},   {-7, 2, -4, 1}, {7, -2, -4, -1}, {-7, -2, 3, -1},  {6, 3, 2, 0},    {-6, 3, -2, 0}, {6, -3, -2, 0},
        {-6, -3, 2, 0}, {0, 5, 0, 0},   {0, -5, 0, 0},   {5, 7, 0, 5},     {-5, 7, -1, 2},  {5, -7, -1, -2}, {-5, -7, 0, -5},
        {1, 1, 1, 0},   {-1, 1, -1, 0}, {1, -1, -1, 0},  {-1, -1, 1, 0},   {100, 7, 14, 2},  {-100, 7, -15, 5}, {100, -7, -15, -5},
        {-100, -7, 14, -2},
    };
    for (const Row& row : table) {
        INFO(row.a << " divmod " << row.b);
        const BigInt::DivMod qr = BigInt::divmod(BigInt(row.a), BigInt(row.b));
        CHECK(qr.quotient == BigInt(row.q));
        CHECK(qr.remainder == BigInt(row.r));
        CHECK(BigInt(row.a) / BigInt(row.b) == BigInt(row.q));
        CHECK(BigInt(row.a) % BigInt(row.b) == BigInt(row.r));
    }
    CHECK_THROWS_AS(BigInt::divmod(BigInt(1), BigInt(0)), std::domain_error);
    CHECK_THROWS_AS(BigInt(0) / BigInt(0), std::domain_error);
    CHECK_THROWS_AS(BigInt(5) % BigInt(0), std::domain_error);
}

TEST_CASE("BigInt divmod agrees with 128-bit arithmetic on random operands, and satisfies a == q*b + r on huge ones", "[devkit][bigint]") {
    std::mt19937_64 rng(0xD1710D1710ULL);
    for (int round = 0; round < 6000; ++round) {
        const Int128 a = random_int(rng, 1U + static_cast<unsigned>(rng() % 126U));
        Int128 b = random_int(rng, 1U + static_cast<unsigned>(rng() % 126U));
        if (b == 0) b = 1;
        Int128 q = 0;
        Int128 r = 0;
        floor_divmod(a, b, q, r);
        INFO(text(a) << " divmod " << text(b));
        const BigInt::DivMod got = BigInt::divmod(big(a), big(b));
        CHECK(got.quotient.to_decimal() == text(q));
        CHECK(got.remainder.to_decimal() == text(r));
    }
    // divisors and dividends on the limb boundaries, where the one-limb and the many-limb paths meet
    for (const char* divisor : {"4294967295", "4294967296", "4294967297", "18446744073709551615", "18446744073709551616", "18446744073709551617", "79228162514264337593543950336", "340282366920938463463374607431768211456"}) {
        const BigInt d = BigInt::from_decimal(divisor);
        for (const BigInt& dividend : {d, d - BigInt(1), d + BigInt(1), d * d, d * d - BigInt(1), d * BigInt(12345) + d - BigInt(1), BigInt(0), BigInt(1)}) {
            for (const BigInt& sign : {BigInt(1), BigInt(-1)}) {
                const BigInt a = dividend * sign;
                const BigInt::DivMod qr = BigInt::divmod(a, d);
                INFO(a.to_decimal() << " divmod " << d.to_decimal());
                CHECK(qr.quotient * d + qr.remainder == a);
                CHECK(qr.remainder.sign() >= 0);   // the divisor is positive
                CHECK(qr.remainder < d);
            }
        }
    }
    // numbers of hundreds of digits, from the decimal generator
    for (int round = 0; round < 150; ++round) {
        std::string a;
        std::string b;
        for (std::size_t i = 0, n = 1 + rng() % 300; i < n; ++i) a.push_back(static_cast<char>('0' + rng() % 10));
        for (std::size_t i = 0, n = 1 + rng() % 200; i < n; ++i) b.push_back(static_cast<char>('0' + rng() % 10));
        BigInt x = BigInt::from_decimal(a);
        BigInt y = BigInt::from_decimal(b);
        if (y.is_zero()) y = BigInt(1);
        if ((rng() & 1U) != 0) x = -x;
        if ((rng() & 1U) != 0) y = -y;
        const BigInt::DivMod qr = BigInt::divmod(x, y);
        INFO(x.to_decimal() << " divmod " << y.to_decimal());
        CHECK(qr.quotient * y + qr.remainder == x);
        CHECK(qr.remainder.abs() < y.abs());
        CHECK((qr.remainder.is_zero() || qr.remainder.sign() == y.sign()));
    }
}

TEST_CASE("BigInt orders and compares by value, whatever the way the value was made", "[devkit][bigint]") {
    CHECK(BigInt(1) < BigInt(2));
    CHECK(BigInt(-2) < BigInt(-1));
    CHECK(BigInt(-1) < BigInt(0));
    CHECK(BigInt(0) < BigInt(1));
    CHECK(BigInt(-1) < BigInt(1));
    CHECK(BigInt::pow(2, 64) > BigInt::pow(2, 63));
    CHECK(-BigInt::pow(2, 64) < -BigInt::pow(2, 63));
    CHECK(BigInt::pow(2, 64) > BigInt(std::numeric_limits<std::int64_t>::max()));
    CHECK(-BigInt::pow(2, 64) < BigInt(std::numeric_limits<std::int64_t>::min()));
    CHECK(BigInt::pow(2, 64) - BigInt(1) < BigInt::pow(2, 64));
    CHECK(BigInt::pow(2, 64) <= BigInt::pow(2, 64));
    CHECK(BigInt::pow(2, 64) >= BigInt::pow(2, 64));
    CHECK(BigInt::from_decimal("18446744073709551616") == BigInt::pow(2, 64));
    CHECK(BigInt::from_decimal("-0") == BigInt(0) - BigInt(0));
    CHECK(BigInt(3) * BigInt(4) == BigInt(12));
    CHECK(BigInt(3) != BigInt(4));
    CHECK(BigInt(-3) != BigInt(3));
    CHECK((BigInt::pow(3, 40) <=> BigInt::pow(3, 40)) == std::strong_ordering::equal);
    CHECK((BigInt(1) <=> BigInt(2)) == std::strong_ordering::less);
    CHECK((BigInt(2) <=> BigInt(1)) == std::strong_ordering::greater);
}

TEST_CASE("BigInt shifts are multiplication and FLOOR division by powers of two", "[devkit][bigint]") {
    for (const std::size_t n : {0U, 1U, 31U, 32U, 33U, 63U, 64U, 65U, 95U, 96U, 97U, 200U}) {
        INFO("shift " << n);
        CHECK(BigInt(1).shifted_left(n) == BigInt::pow(2, static_cast<unsigned>(n)));
        CHECK(BigInt(-1).shifted_left(n) == -BigInt::pow(2, static_cast<unsigned>(n)));
        CHECK(BigInt(0).shifted_left(n).is_zero());
        CHECK(BigInt::pow(2, static_cast<unsigned>(n)).shifted_right(n) == BigInt(1));
        CHECK(BigInt::from_decimal("123456789012345678901234567890").shifted_left(n).shifted_right(n) == BigInt::from_decimal("123456789012345678901234567890"));
        CHECK(BigInt::from_decimal("-123456789012345678901234567890").shifted_left(n).shifted_right(n) == BigInt::from_decimal("-123456789012345678901234567890"));
    }
    // right shifts that lose bits: floor, so a negative number goes down
    CHECK(BigInt(5).shifted_right(1) == BigInt(2));
    CHECK(BigInt(-5).shifted_right(1) == BigInt(-3));
    CHECK(BigInt(4).shifted_right(1) == BigInt(2));
    CHECK(BigInt(-4).shifted_right(1) == BigInt(-2));
    CHECK(BigInt(1).shifted_right(1) == BigInt(0));
    CHECK(BigInt(-1).shifted_right(1) == BigInt(-1));
    CHECK(BigInt(5).shifted_right(100) == BigInt(0));
    CHECK(BigInt(-5).shifted_right(100) == BigInt(-1));
    CHECK(BigInt(0).shifted_right(5) == BigInt(0));
    CHECK(BigInt::pow(2, 70).shifted_right(70) == BigInt(1));
    CHECK((BigInt::pow(2, 70) - BigInt(1)).shifted_right(70) == BigInt(0));
    CHECK((-BigInt::pow(2, 70) - BigInt(1)).shifted_right(70) == BigInt(-2));
    // random: against floor division of 128-bit integers
    std::mt19937_64 rng(0x5417F7ULL);
    for (int round = 0; round < 3000; ++round) {
        const Int128 a = random_int(rng, 1U + static_cast<unsigned>(rng() % 126U));
        const std::size_t n = rng() % 126U;
        Int128 q = 0;
        Int128 r = 0;
        floor_divmod(a, Int128{1} << n, q, r);
        INFO(text(a) << " >> " << n);
        CHECK(big(a).shifted_right(n).to_decimal() == text(q));
    }
}

TEST_CASE("BigInt bit_length, parity and the 64-bit conversions", "[devkit][bigint]") {
    CHECK(BigInt(0).bit_length() == 0);
    CHECK(BigInt(1).bit_length() == 1);
    CHECK(BigInt(2).bit_length() == 2);
    CHECK(BigInt(3).bit_length() == 2);
    CHECK(BigInt(255).bit_length() == 8);
    CHECK(BigInt(256).bit_length() == 9);
    CHECK(BigInt(-256).bit_length() == 9);
    CHECK(BigInt(2147483648LL).bit_length() == 32);
    CHECK(BigInt(4294967295LL).bit_length() == 32);
    CHECK(BigInt(4294967296LL).bit_length() == 33);
    CHECK((BigInt::pow(2, 64) - BigInt(1)).bit_length() == 64);
    CHECK(BigInt::pow(2, 64).bit_length() == 65);
    CHECK(BigInt::pow(2, 1000).bit_length() == 1001);

    CHECK_FALSE(BigInt(0).is_odd());
    CHECK(BigInt(1).is_odd());
    CHECK(BigInt(-1).is_odd());
    CHECK_FALSE(BigInt(2).is_odd());
    CHECK(BigInt::pow(2, 70).shifted_right(0).is_odd() == false);
    CHECK((BigInt::pow(2, 70) + BigInt(1)).is_odd());

    CHECK(BigInt(std::numeric_limits<std::int64_t>::max()).fits_int64());
    CHECK(BigInt(std::numeric_limits<std::int64_t>::min()).fits_int64());
    CHECK_FALSE((BigInt(std::numeric_limits<std::int64_t>::max()) + BigInt(1)).fits_int64());
    CHECK_FALSE((BigInt(std::numeric_limits<std::int64_t>::min()) - BigInt(1)).fits_int64());
    CHECK_FALSE(BigInt::pow(2, 64).fits_int64());
    CHECK_FALSE((-BigInt::pow(2, 100)).fits_int64());
    CHECK(BigInt(std::numeric_limits<std::int64_t>::max()).to_int64() == std::numeric_limits<std::int64_t>::max());
    CHECK(BigInt(std::numeric_limits<std::int64_t>::min()).to_int64() == std::numeric_limits<std::int64_t>::min());
    CHECK(BigInt(0).to_int64() == 0);
    CHECK(BigInt(-12345).to_int64() == -12345);
    CHECK(BigInt(4294967296LL).to_int64() == 4294967296LL);
    CHECK_THROWS_AS((BigInt(std::numeric_limits<std::int64_t>::max()) + BigInt(1)).to_int64(), std::overflow_error);
    CHECK_THROWS_AS((BigInt(std::numeric_limits<std::int64_t>::min()) - BigInt(1)).to_int64(), std::overflow_error);
    CHECK(BigInt(-5).abs() == BigInt(5));
    CHECK(BigInt(5).abs() == BigInt(5));
    CHECK(BigInt(0).abs().is_zero());
    CHECK(BigInt(-5).sign() == -1);
    CHECK(BigInt(0).sign() == 0);
    CHECK(BigInt(5).sign() == 1);
}

TEST_CASE("BigInt gcd is never negative, and gcd(F(m), F(n)) = F(gcd(m, n)) for Fibonacci numbers", "[devkit][bigint]") {
    CHECK(BigInt::gcd(BigInt(0), BigInt(0)).is_zero());
    CHECK(BigInt::gcd(BigInt(0), BigInt(5)) == BigInt(5));
    CHECK(BigInt::gcd(BigInt(5), BigInt(0)) == BigInt(5));
    CHECK(BigInt::gcd(BigInt(0), BigInt(-5)) == BigInt(5));
    CHECK(BigInt::gcd(BigInt(-12), BigInt(18)) == BigInt(6));
    CHECK(BigInt::gcd(BigInt(12), BigInt(-18)) == BigInt(6));
    CHECK(BigInt::gcd(BigInt(-12), BigInt(-18)) == BigInt(6));
    CHECK(BigInt::gcd(BigInt(17), BigInt(5)) == BigInt(1));
    CHECK(BigInt::gcd(BigInt(7), BigInt(7)) == BigInt(7));
    CHECK(BigInt::gcd(BigInt::pow(2, 100), BigInt::pow(2, 60) * BigInt(3)) == BigInt::pow(2, 60));
    CHECK(BigInt::gcd(BigInt::pow(2, 100) * BigInt(3), BigInt::pow(2, 60) * BigInt(9)) == BigInt::pow(2, 60) * BigInt(3));
    // Fibonacci numbers, by addition alone
    std::vector<BigInt> fib = {BigInt(0), BigInt(1)};
    for (int i = 2; i <= 300; ++i) fib.push_back(fib[static_cast<std::size_t>(i - 1)] + fib[static_cast<std::size_t>(i - 2)]);
    CHECK(fib[100].to_decimal() == "354224848179261915075");   // F(100), a well-known value
    for (const auto& [m, n] : std::vector<std::pair<int, int>>{{150, 100}, {300, 200}, {299, 300}, {90, 60}, {120, 84}, {211, 107}, {64, 48}}) {
        const int g = std::gcd(m, n);
        INFO("F(" << m << "), F(" << n << ") has gcd F(" << g << ")");
        CHECK(BigInt::gcd(fib[static_cast<std::size_t>(m)], fib[static_cast<std::size_t>(n)]) == fib[static_cast<std::size_t>(g)]);
    }
}

TEST_CASE("BigInt pow", "[devkit][bigint]") {
    CHECK(BigInt::pow(3, 0) == BigInt(1));
    CHECK(BigInt::pow(0, 0) == BigInt(1));   // as 0 ** 0 is in Python
    CHECK(BigInt::pow(0, 5).is_zero());
    CHECK(BigInt::pow(1, 1000) == BigInt(1));
    CHECK(BigInt::pow(-2, 3) == BigInt(-8));
    CHECK(BigInt::pow(-2, 4) == BigInt(16));
    CHECK(BigInt::pow(-2, 64) == BigInt::pow(2, 64));
    CHECK(BigInt::pow(10, 18).to_decimal() == "1000000000000000000");
    BigInt repeated(1);
    for (int i = 0; i < 100; ++i) repeated = repeated * BigInt(7);
    CHECK(BigInt::pow(7, 100) == repeated);
    CHECK(BigInt::pow(7, 100).to_decimal() == repeated.to_decimal());
}
