// The devkit's Rational against values written down by hand, against identities that hold for any fractions, and -- for the conversion to double, which the generated header of group C6 prints with %.17g
// -- against an oracle made in a different way: the quotient's decimal digits by long division in 128-bit arithmetic and strtod, which rounds a decimal string correctly.  The random operands come from
// std::mt19937_64 with a fixed seed (the engine's output is specified by the standard).

#include <catch2/catch_test_macros.hpp>

#include "bignum_printers.hpp"

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/rational.hpp>

#include <bit>
#include <compare>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <string>

using odl::devkit::BigInt;
using odl::devkit::Rational;

namespace {

__extension__ typedef unsigned __int128 UInt128;
__extension__ typedef __int128 Int128;

// the double nearest to num/den (both positive, below 2**63), made another way: the decimal digits of the quotient by long division, one more digit if any remainder is left (the value is then above the
// truncated text), and strtod.  With 80 fractional digits the text is within 1e-60 of the value, and no fraction with a denominator below 2**63 lies that close to the middle of two doubles unless it IS the
// middle, whose expansion ends within 62 digits and so is exact.
double nearest_double(std::uint64_t num, std::uint64_t den) {
    std::string digits = std::to_string(num / den) + ".";
    UInt128 remainder = num % den;
    for (int i = 0; i < 80 && remainder != 0; ++i) {
        remainder *= 10;
        digits.push_back(static_cast<char>('0' + static_cast<int>(remainder / den)));
        remainder %= den;
    }
    if (remainder != 0) digits.push_back('1');
    return std::strtod(digits.c_str(), nullptr);
}

std::string g17(double x) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.17g", x);
    return buffer;
}

std::uint64_t bits_of(double x) { return std::bit_cast<std::uint64_t>(x); }

Rational q(std::int64_t n, std::int64_t d) { return Rational(BigInt(n), BigInt(d)); }

}  // namespace

TEST_CASE("Rational is always in lowest terms with a positive denominator", "[devkit][rational]") {
    CHECK(q(6, -4).to_string() == "-3/2");
    CHECK(q(-6, -4).to_string() == "3/2");
    CHECK(q(-6, 4).to_string() == "-3/2");
    CHECK(q(6, 4).to_string() == "3/2");
    CHECK(q(0, -7).to_string() == "0");
    CHECK(q(0, 7).to_string() == "0");
    CHECK(q(8, 2).to_string() == "4");
    CHECK(q(-8, 2).to_string() == "-4");
    CHECK(q(1, 1).to_string() == "1");
    CHECK(q(41, 840).to_string() == "41/840");
    CHECK(Rational(BigInt::pow(2, 70), BigInt::pow(2, 72)).to_string() == "1/4");
    CHECK(Rational(BigInt::pow(2, 70) * BigInt(3), BigInt::pow(2, 72) * BigInt(9)).to_string() == "1/12");
    CHECK(Rational(5).to_string() == "5");
    CHECK(Rational(-5).to_string() == "-5");
    CHECK(Rational().to_string() == "0");
    CHECK(Rational(BigInt(7)).to_string() == "7");
    CHECK(q(6, -4).numerator() == BigInt(-3));
    CHECK(q(6, -4).denominator() == BigInt(2));
    CHECK(q(0, -7).denominator() == BigInt(1));
    CHECK(q(2, 4) == q(1, 2));
    CHECK(q(-2, -4) == q(1, 2));
    CHECK(q(1, 2) != q(1, 3));
    CHECK(q(1, 2) != q(-1, 2));
    CHECK(q(0, 3) == Rational(0));
    CHECK_THROWS_AS(Rational(BigInt(1), BigInt(0)), std::domain_error);
    CHECK_THROWS_AS(Rational(BigInt(0), BigInt(0)), std::domain_error);
}

TEST_CASE("Rational arithmetic is exact: the sums, products and quotients written down by hand", "[devkit][rational]") {
    CHECK(q(1, 2) + q(1, 3) == q(5, 6));
    CHECK(q(1, 2) - q(1, 3) == q(1, 6));
    CHECK(q(1, 3) - q(1, 2) == q(-1, 6));
    CHECK(q(2, 3) * q(3, 4) == q(1, 2));
    CHECK(q(-2, 3) * q(3, 4) == q(-1, 2));
    CHECK(q(-2, 3) * q(-3, 4) == q(1, 2));
    CHECK(q(1, 3) / q(2, 9) == q(3, 2));
    CHECK(q(1, 3) / q(-2, 9) == q(-3, 2));
    CHECK(q(-1, 3) / q(-2, 9) == q(3, 2));
    CHECK(q(1, 2) - q(1, 2) == Rational(0));
    CHECK((q(1, 2) - q(1, 2)).is_zero());
    CHECK(q(1, 2) + q(1, 2) == Rational(1));
    CHECK(-q(1, 2) == q(-1, 2));
    CHECK(-Rational(0) == Rational(0));
    CHECK(q(-3, 7).abs() == q(3, 7));
    CHECK(q(3, 7).abs() == q(3, 7));
    CHECK(q(0, 7).abs().is_zero());
    CHECK(q(-3, 7).sign() == -1);
    CHECK(q(3, 7).sign() == 1);
    CHECK(Rational().sign() == 0);
    CHECK(Rational(2) * q(1, 2) == Rational(1));
    CHECK(Rational(1) / q(41, 840) == q(840, 41));
    CHECK_THROWS_AS(q(1, 2) / Rational(0), std::domain_error);
    CHECK_THROWS_AS(Rational(1) / q(0, 5), std::domain_error);
    Rational acc(1);
    acc += q(1, 2);
    acc -= q(1, 4);
    acc *= Rational(4);
    CHECK(acc == Rational(5));
    // the tenth harmonic number, a classic: 1 + 1/2 + ... + 1/10 = 7381/2520
    Rational h(0);
    for (std::int64_t k = 1; k <= 10; ++k) h += q(1, k);
    CHECK(h == q(7381, 2520));
    CHECK(h.to_string() == "7381/2520");
    // the exact sum of 1/7 seven times is 1, which a double would not promise
    Rational seven(0);
    for (int k = 0; k < 7; ++k) seven += q(1, 7);
    CHECK(seven == Rational(1));
    // and a product that outgrows 64 bits: (1/3)^50 = 1 / 3^50
    Rational power(1);
    for (int k = 0; k < 50; ++k) power *= q(1, 3);
    CHECK(power == Rational(BigInt(1), BigInt::pow(3, 50)));
    CHECK(power.denominator().to_decimal() == "717897987691852588770249");   // 3**50
}

TEST_CASE("Rational arithmetic obeys the field identities on random fractions", "[devkit][rational]") {
    std::mt19937_64 rng(0xF1E1DULL);
    const auto random_fraction = [&rng]() {
        const auto n = static_cast<std::int64_t>(rng() % 2000001) - 1000000;
        const auto d = static_cast<std::int64_t>(rng() % 1000000) + 1;
        return q(n, d);
    };
    for (int round = 0; round < 1500; ++round) {
        const Rational a = random_fraction();
        const Rational b = random_fraction();
        const Rational c = random_fraction();
        INFO(a << ", " << b << ", " << c);
        CHECK((a + b) - b == a);
        CHECK(a + b == b + a);
        CHECK(a * b == b * a);
        CHECK(a * (b + c) == a * b + a * c);
        CHECK(a - a == Rational(0));
        if (!b.is_zero()) CHECK((a * b) / b == a);
        // the order agrees with the cross products, which are integers that fit 128 bits
        const bool less = static_cast<Int128>(a.numerator().to_int64()) * b.denominator().to_int64() < static_cast<Int128>(b.numerator().to_int64()) * a.denominator().to_int64();
        CHECK((a < b) == less);
        CHECK((a == b) == (a - b).is_zero());
        CHECK(((a < b) || (a == b) || (a > b)));
    }
    CHECK(q(1, 3) < q(1, 2));
    CHECK(q(-1, 2) < q(-1, 3));
    CHECK(q(-1, 3) < q(1, 1000000));
    CHECK(q(2, 4) <= q(1, 2));
    CHECK(q(2, 4) >= q(1, 2));
    CHECK(q(3, 2) > q(1, 1));
    CHECK((q(1, 3) <=> q(1, 3)) == std::strong_ordering::equal);
    CHECK((q(1, 3) <=> q(1, 2)) == std::strong_ordering::less);
}

TEST_CASE("Rational to_double is the nearest double: the values that matter, written as bit patterns and as %.17g", "[devkit][rational]") {
    // exact in binary
    CHECK(Rational(0).to_double() == 0.0);
    CHECK(Rational(1).to_double() == 1.0);
    CHECK(Rational(-1).to_double() == -1.0);
    CHECK(q(1, 2).to_double() == 0.5);
    CHECK(q(3, 8).to_double() == 0.375);
    CHECK(q(-5, 4).to_double() == -1.25);
    CHECK(q(25, 16).to_double() == 1.5625);
    CHECK(Rational(BigInt::pow(2, 1000)).to_double() == std::ldexp(1.0, 1000));
    CHECK(Rational(BigInt(1), BigInt::pow(2, 1000)).to_double() == std::ldexp(1.0, -1000));
    CHECK(Rational(BigInt::pow(2, 1023)).to_double() == std::ldexp(1.0, 1023));
    CHECK(Rational(BigInt(1), BigInt::pow(2, 1022)).to_double() == std::ldexp(1.0, -1022));   // the smallest normal double
    CHECK(Rational(BigInt(1) , BigInt(1)).to_double() == 1.0);
    // the classic inexact ones: the doubles nearest to 1/3, 2/3, 1/10 and 1/5
    CHECK(bits_of(q(1, 3).to_double()) == 0x3FD5555555555555ULL);
    CHECK(bits_of(q(2, 3).to_double()) == 0x3FE5555555555555ULL);
    CHECK(bits_of(q(1, 10).to_double()) == 0x3FB999999999999AULL);
    CHECK(bits_of(q(1, 5).to_double()) == 0x3FC999999999999AULL);
    CHECK(g17(q(1, 3).to_double()) == "0.33333333333333331");
    CHECK(g17(q(2, 3).to_double()) == "0.66666666666666663");
    CHECK(g17(q(1, 10).to_double()) == "0.10000000000000001");
    CHECK(g17(q(1, 5).to_double()) == "0.20000000000000001");
    CHECK(g17(q(-1, 3).to_double()) == "-0.33333333333333331");
    // the first row of Fehlberg's table: 2/27 and 41/840, 17 significant digits, as the generated header prints them
    CHECK(g17(q(2, 27).to_double()) == "0.07407407407407407");
    CHECK(g17(q(41, 840).to_double()) == "0.04880952380952381");
    CHECK(g17(q(5, 12).to_double()) == "0.41666666666666669");
    CHECK(g17(q(-25, 108).to_double()) == "-0.23148148148148148");
}

TEST_CASE("Rational to_double rounds ties to even, and above and below the middle the right way", "[devkit][rational]") {
    const BigInt two53 = BigInt::pow(2, 53);
    // 1 + 2**-53 is exactly between 1 and 1 + 2**-52: ties to even gives 1
    CHECK(Rational(two53 + BigInt(1), two53).to_double() == 1.0);
    // 1 + 3 * 2**-53 is exactly between 1 + 2**-52 (an odd last bit) and 1 + 2**-51 (even): the even one
    CHECK(Rational(two53 + BigInt(3), two53).to_double() == 1.0 + std::ldexp(1.0, -51));
    // a hair above the first middle goes up, a hair below goes down
    const BigInt tiny = BigInt::pow(2, 200);
    CHECK(Rational(two53 * tiny + tiny + BigInt(1), two53 * tiny).to_double() == 1.0 + std::ldexp(1.0, -52));
    CHECK(Rational(two53 * tiny + tiny - BigInt(1), two53 * tiny).to_double() == 1.0);
    // every middle between consecutive doubles in [1, 2): 1 + (2k + 1) * 2**-53 goes to the even neighbour, k or k + 1
    for (std::int64_t k = 0; k < 600; ++k) {
        const Rational middle(two53 + BigInt(2 * k + 1), two53);
        const std::int64_t even = k % 2 == 0 ? k : k + 1;
        INFO("k = " << k);
        CHECK(middle.to_double() == 1.0 + std::ldexp(static_cast<double>(even), -52));
    }
    // the same ties at another exponent, and negative: -(1 + 2**-53) * 2**10 is -1024
    CHECK(Rational(-(two53 + BigInt(1)) * BigInt::pow(2, 10), two53).to_double() == -1024.0);
    CHECK(Rational(-(two53 + BigInt(3)) * BigInt::pow(2, 10), two53).to_double() == -std::ldexp(1.0 + std::ldexp(1.0, -51), 10));
    // 2**53 + 1 is exactly between 2**53 and 2**53 + 2; ties to even gives 2**53; 2**53 + 3 gives 2**53 + 4
    CHECK(Rational(two53 + BigInt(1)).to_double() == 9007199254740992.0);
    CHECK(Rational(two53 + BigInt(3)).to_double() == 9007199254740996.0);
    // 2**54 - 1 is exactly between 2**54 - 2 (an odd last bit) and 2**54 (even): the even one
    CHECK(Rational(BigInt::pow(2, 54) - BigInt(1)).to_double() == 18014398509481984.0);
}

TEST_CASE("Rational to_double agrees with an oracle of another kind on random fractions of every size", "[devkit][rational]") {
    std::mt19937_64 rng(0xD0B1E5ULL);
    for (int round = 0; round < 5000; ++round) {
        const std::uint64_t num = 1 + (rng() >> (1 + rng() % 62));   // 1 .. 2**63
        const std::uint64_t den = 1 + (rng() >> (1 + rng() % 62));
        const std::uint64_t n = num >> 1;                            // below 2**63, as the oracle needs
        const std::uint64_t d = den >> 1;
        if (n == 0 || d == 0) continue;
        const double expected = nearest_double(n, d);
        INFO(n << "/" << d);
        CHECK(q(static_cast<std::int64_t>(n), static_cast<std::int64_t>(d)).to_double() == expected);
        CHECK(q(-static_cast<std::int64_t>(n), static_cast<std::int64_t>(d)).to_double() == -expected);
        // scaling by a power of two does not change which double is nearest, in the normal range
        const std::size_t shift = rng() % 700;
        CHECK(Rational(BigInt(static_cast<std::int64_t>(n)).shifted_left(shift), BigInt(static_cast<std::int64_t>(d))).to_double() == std::ldexp(expected, static_cast<int>(shift)));
        CHECK(Rational(BigInt(static_cast<std::int64_t>(n)), BigInt(static_cast<std::int64_t>(d)).shifted_left(shift)).to_double() == std::ldexp(expected, -static_cast<int>(shift)));
    }
}

TEST_CASE("Rational to_double refuses what a normal double cannot hold", "[devkit][rational]") {
    CHECK_THROWS_AS(Rational(BigInt::pow(2, 1024)).to_double(), std::range_error);
    CHECK_THROWS_AS(Rational(-BigInt::pow(2, 1024)).to_double(), std::range_error);
    CHECK_THROWS_AS(Rational(BigInt(1), BigInt::pow(2, 1023)).to_double(), std::range_error);
    CHECK_THROWS_AS(Rational(BigInt(1), BigInt::pow(2, 1100)).to_double(), std::range_error);
    CHECK_THROWS_AS(Rational(BigInt::pow(10, 400)).to_double(), std::range_error);
    // the largest double, (2**53 - 1) * 2**971, is held; the middle between it and 2**1024 rounds to the even one, which is 2**1024 itself, and overflows; below the middle it is held
    const BigInt largest = (BigInt::pow(2, 53) - BigInt(1)).shifted_left(971);
    CHECK(Rational(largest).to_double() == 1.7976931348623157e308);
    const BigInt middle = (BigInt::pow(2, 54) - BigInt(1)).shifted_left(970);
    CHECK_THROWS_AS(Rational(middle).to_double(), std::range_error);
    CHECK(Rational(middle - BigInt(1)).to_double() == 1.7976931348623157e308);
    // the message names the value
    try {
        (void)Rational(BigInt::pow(2, 1024)).to_double();
        FAIL("no exception");
    } catch (const std::range_error& e) {
        CHECK(std::string(e.what()).find("too large for a double") != std::string::npos);
    }
    try {
        (void)Rational(BigInt(1), BigInt::pow(2, 1023)).to_double();
        FAIL("no exception");
    } catch (const std::range_error& e) {
        CHECK(std::string(e.what()).find("too small for a normal double") != std::string::npos);
    }
}
