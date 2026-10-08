// The devkit's Decimal against what Python's decimal module (libmpdec) is documented to do, and against oracles that share no code with it: exact rational arithmetic (Rational, tested apart) for the
// correct rounding of + - * / and of the square root, hand-derived cases of the General Decimal Arithmetic Specification's rules (the ideal exponent of a quotient and of a root, the exponent of a sum, the sign of a
// zero, the layout of str() and of format()), and digits of irrational numbers computed by GNU bc 1.07.1 (`scale=160; sqrt(2)`, `scale=160; l(2)/l(10)`), a host tool that names no part of the build.  Every
// expected value was derived BEFORE the code ran and none was taken from a run of it.  The random operands come from std::mt19937_64 with a fixed seed.

#include <catch2/catch_test_macros.hpp>

#include "bignum_printers.hpp"

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/decimal.hpp>
#include <odl/devkit/rational.hpp>

#include <cmath>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using odl::devkit::BigInt;
using odl::devkit::Decimal;
using odl::devkit::DecimalContext;
using odl::devkit::DecimalDivisionByZero;
using odl::devkit::DecimalError;
using odl::devkit::DecimalInvalidOperation;
using odl::devkit::DecimalOverflow;
using odl::devkit::getcontext;
using odl::devkit::LocalContext;
using odl::devkit::Rational;

namespace {

// every case starts from Python's default context and leaves it so
struct ContextReset {
    ContextReset() { getcontext() = DecimalContext{}; }
    ~ContextReset() { getcontext() = DecimalContext{}; }
};

Decimal D(const char* text) { return Decimal::from_string(text); }
std::string S(const Decimal& d) { return d.to_string(); }

BigInt pow10b(unsigned k) { return BigInt::pow(BigInt(10), k); }
Rational pow10r(int k) { return k >= 0 ? Rational(pow10b(static_cast<unsigned>(k))) : Rational(BigInt(1), pow10b(static_cast<unsigned>(-k))); }

// the exact value of a Decimal
Rational value_of(const Decimal& d) {
    const Rational magnitude = Rational(d.coefficient()) * pow10r(static_cast<int>(d.exponent()));
    return d.is_negative() ? -magnitude : magnitude;
}

int digits_of(const BigInt& n) { return n.is_zero() ? 1 : static_cast<int>(n.to_decimal().size()); }

// the rational rounded to `prec` significant digits, half-even, exactly (the oracle of every correctly rounded operation): scale it into [10**(prec-1), 10**prec), take the integer part, and look at the rest
Rational rounded(const Rational& q, int prec) {
    if (q.is_zero()) return Rational(0);   // (a copy of q would be the same zero; g++ 13 reads that copy as a possible null dereference)
    const Rational a = q.abs();
    int e = digits_of(a.numerator()) - digits_of(a.denominator()) - prec;   // within a factor of ten of the right one
    for (;;) {
        const Rational scaled = a / pow10r(e);
        if (scaled < pow10r(prec - 1)) {
            --e;
        } else if (scaled >= pow10r(prec)) {
            ++e;
        } else {
            BigInt n = BigInt::divmod(scaled.numerator(), scaled.denominator()).quotient;
            const Rational rest = scaled - Rational(n);
            const Rational twice = rest + rest;
            if (twice > Rational(1) || (twice == Rational(1) && n.is_odd())) n = n + BigInt(1);
            const Rational out = Rational(n) * pow10r(e);
            return q.sign() < 0 ? -out : out;
        }
    }
}

Decimal random_decimal(std::mt19937_64& rng, int max_digits, int exp_lo, int exp_hi) {
    const int digits = 1 + static_cast<int>(rng() % static_cast<std::uint64_t>(max_digits));
    BigInt coefficient;
    switch (rng() % 10) {
        case 0: coefficient = pow10b(static_cast<unsigned>(digits - 1)); break;   // a power of ten
        case 1: coefficient = BigInt::from_decimal("5" + std::string(static_cast<std::size_t>(digits - 1), '0')); break;   // half of the next power
        case 2: coefficient = BigInt::from_decimal(std::string(static_cast<std::size_t>(digits), '9')); break;   // all nines
        case 3: coefficient = BigInt(); break;                                                                    // zero
        default: {
            std::string text(1, static_cast<char>('1' + rng() % 9));
            for (int i = 1; i < digits; ++i) text.push_back(static_cast<char>('0' + rng() % 10));
            coefficient = BigInt::from_decimal(text);
        }
    }
    const auto exponent = static_cast<std::int64_t>(exp_lo) + static_cast<std::int64_t>(rng() % static_cast<std::uint64_t>(exp_hi - exp_lo + 1));
    return Decimal::from_parts((rng() & 1U) != 0, coefficient, exponent);
}

}  // namespace

TEST_CASE("Decimal is read from text and printed as Python's str() prints it", "[devkit][decimal]") {
    ContextReset reset;
    // text -> str(): the rules of the to-scientific-string of the specification
    const std::vector<std::pair<const char*, const char*>> table = {
        {"0", "0"},           {"0.00", "0.00"},       {"-0", "-0"},           {"123", "123"},         {"-123.456", "-123.456"}, {"0.000001", "0.000001"},   {"0.0000001", "1E-7"},
        {"1E+2", "1E+2"},     {"1e2", "1E+2"},        {"123E+3", "1.23E+5"},  {"12.50", "12.50"},    {"1.0E-5", "0.000010"},   {".5", "0.5"},              {"5.", "5"},
        {"+7", "7"},          {"00012", "12"},        {"1E+0", "1"},          {"0E+5", "0E+5"},       {"0E-10", "0E-10"},       {"-1.5E+24", "-1.5E+24"},   {"100", "100"},
        {"1.00E+2", "100"},   {"0.1", "0.1"},       {"10E-1", "1.0"},       {"1000000", "1000000"}, {"1E+6", "1E+6"},         {"-1E-7", "-1E-7"},         {"12345E-8", "0.00012345"},
        {"12345E-12", "1.2345E-8"},
    };
    for (const auto& [text, expected] : table) {
        INFO("text " << text);
        CHECK(S(D(text)) == expected);
    }
    // the parts: the zero keeps its exponent, the sign of a zero is kept, leading zeros go, trailing zeros stay
    CHECK(D("0.00").exponent() == -2);
    CHECK(D("0.00").is_zero());
    CHECK(D("-0").is_negative());
    CHECK(D("-0").is_zero());
    CHECK(D("1.500").coefficient() == BigInt(1500));
    CHECK(D("1.500").exponent() == -3);
    CHECK(D("000.0120").coefficient() == BigInt(120));
    CHECK(D("000.0120").exponent() == -4);
    CHECK(D("12E+3").coefficient() == BigInt(12));
    CHECK(D("12E+3").exponent() == 3);
    // exact machine integers
    CHECK(S(Decimal(0)) == "0");
    CHECK(S(Decimal(-5)) == "-5");
    CHECK(S(Decimal(std::numeric_limits<std::int64_t>::min())) == "-9223372036854775808");
    CHECK(S(Decimal(BigInt::pow(2, 100))) == "1267650600228229401496703205376");   // no rounding of a constructor's argument, whatever the precision
    // digits, adjusted exponent, integer-ness
    CHECK(D("0").digits() == 1);
    CHECK(D("1234").digits() == 4);
    CHECK(D("1234E-2").adjusted() == 1);
    CHECK(D("1E-7").adjusted() == -7);
    CHECK(D("0.00").adjusted() == -2);
    CHECK(D("2").is_integer());
    CHECK(D("2.0").is_integer());
    CHECK(D("1E+3").is_integer());
    CHECK(D("0.00").is_integer());
    CHECK_FALSE(D("2.5").is_integer());
    CHECK_FALSE(D("1E-3").is_integer());
    // refused
    for (const char* bad : {"", ".", "e5", "1e", "1e+", "--1", "1.2.3", "0x10", " 1", "1 ", "NaN", "Infinity", "-", "+", "1_000", "1E+9999999999999999", "1,5", "E", "+.", "1e5.5"}) {
        INFO("text '" << bad << "'");
        CHECK_THROWS_AS(Decimal::from_string(bad), DecimalInvalidOperation);
    }
    CHECK_THROWS_AS(Decimal::from_parts(false, BigInt(-1), 0), std::invalid_argument);
    // the capitals of the context
    getcontext().capitals = false;
    CHECK(S(D("1E+2")) == "1e+2");
    CHECK(S(D("1E-7")) == "1e-7");
}

TEST_CASE("Decimal's context is Python's: the defaults, localcontext restoring what it found, and getcontext().prec set for good", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(getcontext().prec == 28);
    CHECK(getcontext().emax == 999999);
    CHECK(getcontext().emin == -999999);
    CHECK(getcontext().capitals);
    {
        LocalContext lc(6);
        CHECK(getcontext().prec == 6);
        CHECK(S(Decimal(1) / Decimal(7)) == "0.142857");
        {
            LocalContext inner;   // a copy of the one in force
            CHECK(getcontext().prec == 6);
            inner.context().prec = 3;
            CHECK(S(Decimal(1) / Decimal(7)) == "0.143");
        }
        CHECK(getcontext().prec == 6);   // the inner one's change is gone
        lc.context().prec = 9;
        CHECK(S(Decimal(1) / Decimal(7)) == "0.142857143");
    }
    CHECK(getcontext().prec == 28);
    CHECK(S(Decimal(1) / Decimal(7)) == "0.1428571428571428571428571429");
    // Python's `getcontext().prec = 60` outside any block stays in force, and a localcontext made afterwards copies it and puts it back
    getcontext().prec = 60;
    {
        LocalContext lc;
        CHECK(getcontext().prec == 60);
        lc.context().prec = 5;
        CHECK(S(+D("123456.789")) == "1.2346E+5");
    }
    CHECK(getcontext().prec == 60);
    // an exception leaves the context restored too
    try {
        LocalContext lc(3);
        (void)(Decimal(1) / Decimal(0));
    } catch (const DecimalDivisionByZero&) {
    }
    CHECK(getcontext().prec == 60);
}

TEST_CASE("Decimal addition and subtraction: the exponent of the result, the sign of a zero, the rounding, and the amounts too small to see", "[devkit][decimal]") {
    ContextReset reset;
    // the examples of the specification (add, subtract), at precision 9, and the exponent rule: the smaller of the two
    {
        LocalContext lc(9);
        const std::vector<std::tuple<const char*, const char*, const char*, const char*>> table = {   // a, b, a + b, a - b
            {"12", "7.00", "19.00", "5.00"},
            {"1E+2", "1E+4", "1.01E+4", "-9.9E+3"},
            {"1.3", "1.07", "2.37", "0.23"},
            {"1.3", "1.30", "2.60", "0.00"},
            {"1.3", "1.3", "2.6", "0.0"},
            {"1.3", "-1.07", "0.23", "2.37"},
            {"-1.3", "1.3", "0.0", "-2.6"},
            {"-1.3", "-1.3", "-2.6", "0.0"},
            {"0", "0", "0", "0"},
            {"0E+5", "0E-2", "0.00", "0.00"},
            {"1.5", "0E+3", "1.5", "1.5"},
            {"123456789", "1", "123456790", "123456788"},
            {"123456789", "0.5", "123456790", "123456788"},   // 123456789.5 -> a tie, to the even digit
            {"123456788", "0.5", "123456788", "123456788"},   // 123456788.5 -> 123456788 (the digit before the tie is even); 123456787.5 -> 123456788 (odd: up)
            {"999999999", "1", "1.00000000E+9", "999999998"},
            {"1E+1000", "1E-1000", "1.00000000E+1000", "1.00000000E+1000"},
        };
        for (const auto& [a, b, sum, difference] : table) {
            INFO(a << " and " << b);
            CHECK(S(D(a) + D(b)) == sum);
            CHECK(S(D(a) - D(b)) == difference);
        }
    }
    // the sign of a zero: the operands' when they agree, + when they differ or when two non-zero amounts cancel
    CHECK(S(D("0") + D("0")) == "0");
    CHECK(S(D("0") + D("-0")) == "0");
    CHECK(S(D("-0") + D("0")) == "0");
    CHECK(S(D("-0") + D("-0")) == "-0");
    CHECK(S(D("0") - D("0")) == "0");
    CHECK(S(D("0") - D("-0")) == "0");
    CHECK(S(D("-0") - D("0")) == "-0");
    CHECK(S(D("-0") - D("-0")) == "0");
    CHECK(S(D("5") - D("5")) == "0");
    CHECK(S(D("-5") + D("5")) == "0");
    CHECK(S(D("-5") - D("-5")) == "0");
    CHECK_FALSE((D("5") - D("5")).is_negative());
    CHECK((D("-0") + D("-0")).is_negative());
    // a zero added to a number is the number, padded toward the zero's exponent: to Python's 28 digits here
    CHECK(S(D("1.5") + D("0E-10")) == "1.5000000000");
    CHECK(S(D("1.5") - D("0E-10")) == "1.5000000000");
    CHECK(S(D("0E-10") + D("1.5")) == "1.5000000000");
    CHECK(S(D("0E-10") - D("1.5")) == "-1.5000000000");
    CHECK(S(D("1E+100") + D("0E-100")) == "1.000000000000000000000000000E+100");
    CHECK(S(D("-1E+100") - D("0E-100")) == "-1.000000000000000000000000000E+100");
    // and only as far as the precision has room
    {
        LocalContext lc(3);
        CHECK(S(D("1E+100") + D("0E-100")) == "1.00E+100");
        CHECK(S(D("12") + D("0E-100")) == "12.0");
        CHECK(S(D("12345") + D("0")) == "1.23E+4");
        CHECK(S(D("12355") + D("0")) == "1.24E+4");
        CHECK(S(D("12345") + D("0E-3")) == "1.23E+4");
    }
    // ties go to the even digit, and a nonzero amount far below the last digit breaks a tie that would otherwise be one
    {
        LocalContext lc(5);
        CHECK(S(D("1.00005") + D("0")) == "1.0000");       // 1.0000|5: the digit before is 0, even
        CHECK(S(D("1.00015") + D("0")) == "1.0002");       // 1.0001|5: the digit before is 1, odd: up
        CHECK(S(D("1.00005") + D("1E-100")) == "1.0001");  // above the tie, whatever the amount
        CHECK(S(D("1.00005") - D("1E-100")) == "1.0000");  // below it
        CHECK(S(D("1.00015") + D("1E-100")) == "1.0002");
        CHECK(S(D("1.00015") - D("1E-100")) == "1.0001");
        CHECK(S(D("1.00005") + D("-1E-100")) == "1.0000");
        CHECK(S(D("-1.00005") + D("-1E-100")) == "-1.0001");
        CHECK(S(D("-1.00005") - D("-1E-100")) == "-1.0000");
        CHECK(S(D("1") + D("1E-100")) == "1.0000");        // padded to the precision
        CHECK(S(D("1") - D("1E-100")) == "1.0000");        // 0.99999... rounds back to one
        CHECK(S(D("1E-100") + D("1")) == "1.0000");
        CHECK(S(D("1E-100") - D("1")) == "-1.0000");
        CHECK(S(D("123456") + D("1E-3")) == "1.2346E+5");  // 123456.001
        CHECK(S(D("123455") + D("1E+100")) == "1.0000E+100");
        CHECK(S(D("123455") - D("1E+100")) == "-1.0000E+100");
    }
    // a subtraction that borrows from a power of ten: 1000 - tiny is 999.999..., which rounds to the three digits 1.00E+3 again; with the amount large enough it does not
    {
        LocalContext lc(3);
        CHECK(S(D("1000") - D("1E-100")) == "1.00E+3");
        CHECK(S(D("1000") - D("0.4")) == "1.00E+3");   // 999.6
        CHECK(S(D("1000") - D("0.6")) == "999");       // 999.4
        CHECK(S(D("1000") - D("0.5")) == "1.00E+3");   // 999.5: a tie between 999 and 1000: the even digit is that of 1000 (the last of 100 is 0)
        CHECK(S(D("1000") - D("0.50001")) == "999");
        CHECK(S(D("-1000") + D("1E-100")) == "-1.00E+3");
    }
    // the carry of a rounding into a new digit
    {
        LocalContext lc(3);
        CHECK(S(D("999") + D("1")) == "1.00E+3");
        CHECK(S(D("999.5") + D("0")) == "1.00E+3");
        CHECK(S(D("-999.5") + D("0")) == "-1.00E+3");
        CHECK(S(D("99.95") + D("0")) == "100");   // 99.95 -> 100 (three digits: the carry gives 100, exponent 0)
    }
    // and the results of any exponents are within half a unit of the last digit of the exact value: see the random test below
}

TEST_CASE("Decimal + - * / and sqrt are correctly rounded: random operands of every shape against exact rational arithmetic", "[devkit][decimal]") {
    ContextReset reset;
    std::mt19937_64 rng(0xDEC1A15EEDULL);
    const int precisions[] = {1, 2, 3, 5, 9, 17, 28, 40};
    for (int round = 0; round < 6000; ++round) {
        const int prec = precisions[rng() % 8];
        LocalContext lc(prec);
        // operands of up to 45 digits whose exponents differ by up to 140 places: sums that need the whole precision, sums where one operand is only a sticky amount, borrows from powers of ten
        const Decimal a = random_decimal(rng, 45, -70, 70);
        const Decimal b = random_decimal(rng, 45, -70, 70);
        const Rational qa = value_of(a);
        const Rational qb = value_of(b);
        INFO("prec " << prec << ": " << S(a) << " and " << S(b));
        const Decimal sum = a + b;
        CHECK(value_of(sum) == rounded(qa + qb, prec));
        CHECK(sum.digits() <= prec);
        const Decimal difference = a - b;
        CHECK(value_of(difference) == rounded(qa - qb, prec));
        CHECK(difference.digits() <= prec);
        const Decimal product = a * b;
        CHECK(value_of(product) == rounded(qa * qb, prec));
        CHECK(product.digits() <= prec);
        if (!b.is_zero()) {
            const Decimal quotient = a / b;
            CHECK(value_of(quotient) == rounded(qa / qb, prec));
            CHECK(quotient.digits() <= prec);
        }
        // the sign of a zero result: of the product by the signs, of the quotient too, of an exact cancellation +
        if (product.is_zero()) CHECK(product.is_negative() == (a.is_negative() != b.is_negative()));
        if (!a.is_zero() && a == b) CHECK_FALSE(difference.is_negative());
    }
}

TEST_CASE("Decimal multiplication: exponents add, a zero keeps the sum of the exponents and the xor of the signs", "[devkit][decimal]") {
    ContextReset reset;
    {
        LocalContext lc(9);
        CHECK(S(D("1.20") * D("3")) == "3.60");
        CHECK(S(D("7") * D("3")) == "21");
        CHECK(S(D("0.9") * D("0.8")) == "0.72");
        CHECK(S(D("0.9") * D("-0")) == "-0.0");
        CHECK(S(D("654321") * D("654321")) == "4.28135971E+11");
        CHECK(S(D("-3") * D("-4")) == "12");
        CHECK(S(D("1E+5") * D("1E-3")) == "1E+2");
        CHECK(S(D("0E+5") * D("2E+7")) == "0E+12");
        CHECK(S(D("0") * D("-5")) == "-0");
        CHECK(S(D("-0") * D("5")) == "-0");
        CHECK(S(D("-0") * D("-5")) == "0");
        CHECK(S(D("0.00") * D("0.00")) == "0.0000");
    }
    // ties
    {
        LocalContext lc(2);
        CHECK(S(D("1.5") * D("1.5")) == "2.2");    // 2.25 -> the digit before the tie is 2, even
        CHECK(S(D("2.5") * D("1.5")) == "3.8");    // 3.75 -> 3.8 (7 is odd: up)
        CHECK(S(D("4.5") * D("1.1")) == "5.0");    // 4.95 -> 5.0 (9 odd -> up and carries)
        CHECK(S(D("-1.5") * D("1.5")) == "-2.2");
    }
}

TEST_CASE("Decimal division: the ideal exponent, exact and inexact results, the sign and the errors", "[devkit][decimal]") {
    ContextReset reset;
    {
        LocalContext lc(9);
        const std::vector<std::tuple<const char*, const char*, const char*>> table = {   // the examples of the specification (divide)
            {"1", "3", "0.333333333"},   {"2", "3", "0.666666667"},     {"5", "2", "2.5"},          {"1", "10", "0.1"},          {"12", "12", "1"},
            {"8.00", "2", "4.00"},       {"2.400", "2.0", "1.20"},      {"1000", "100", "10"},      {"1000", "1", "1000"},       {"2.40E+6", "2", "1.20E+6"},
            {"100", "0.1", "1.00E+3"},   {"1.00", "3", "0.333333333"},  {"0", "5", "0"},            {"0.00", "5", "0.00"},       {"0", "5.00", "0E+2"},
            {"-0", "5", "-0"},           {"0", "-5", "-0"},             {"-0", "-5", "0"},          {"1", "-4", "-0.25"},        {"-1", "-4", "0.25"},
            {"1E+10", "1E+3", "1E+7"},   {"1E-10", "1E+3", "1E-13"},    {"7", "7.00", "1"},         {"7.00", "7", "1.00"},       {"10", "4", "2.5"},
            {"22", "7", "3.14285714"},   {"1", "81", "0.0123456790"},   {"123456789012", "1", "1.23456789E+11"},
        };
        for (const auto& [a, b, expected] : table) {
            INFO(a << " / " << b);
            CHECK(S(D(a) / D(b)) == expected);
        }
    }
    {
        LocalContext lc(3);
        CHECK(S(D("1200000") / D("1")) == "1.20E+6");   // exact, but more digits than the precision: it is rounded, the zeros stay
        CHECK(S(D("1") / D("3")) == "0.333");
        CHECK(S(D("2") / D("3")) == "0.667");
        CHECK(S(D("1") / D("8")) == "0.125");
        CHECK(S(D("1") / D("16")) == "0.0625");
        CHECK(S(D("1") / D("32")) == "0.0312");        // 0.03125: a tie between 0.0312 and 0.0313 -> even
        CHECK(S(D("3") / D("32")) == "0.0938");        // 0.09375: a tie -> 0.0938 (7 odd)
    }
    CHECK_THROWS_AS(D("1") / D("0"), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("-1") / D("-0"), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("0") / D("0"), DecimalInvalidOperation);
    CHECK_THROWS_AS(D("0") / D("-0"), DecimalInvalidOperation);
    // at 28 digits the value of Python's documentation
    CHECK(S(Decimal(1) / Decimal(7)) == "0.1428571428571428571428571429");
    CHECK(S(Decimal(2) / Decimal(3)) == "0.6666666666666666666666666667");
    CHECK(S(Decimal(1) / Decimal(3)) == "0.3333333333333333333333333333");
    // a huge dividend and a one-digit divisor, and a divisor longer than the dividend
    {
        LocalContext lc(5);
        CHECK(S(D("123456789012345678901234567890") / D("7")) == "1.7637E+28");
        CHECK(S(D("7") / D("123456789012345678901234567890")) == "5.6700E-29");
    }
}

TEST_CASE("Decimal square root: correctly rounded, with the ideal exponent of an exact root and the sign of a zero", "[devkit][decimal]") {
    ContextReset reset;
    {
        LocalContext lc(9);
        const std::vector<std::pair<const char*, const char*>> table = {   // the examples of the specification (square-root)
            {"0", "0"},           {"-0", "-0"},         {"0.0", "0.0"},        {"0.00", "0.0"},       {"0E+5", "0E+2"},     {"0E+4", "0E+2"},      {"0E-5", "0.000"},    {"1", "1"},
            {"1.0", "1.0"},       {"1.00", "1.0"},      {"2", "1.41421356"},   {"2.5", "1.58113883"}, {"100", "10"},        {"1E+2", "1E+1"},     {"0.25", "0.5"},       {"0.04", "0.2"},
            {"1E-2", "0.1"},      {"1E+3", "31.6227766"}, {"4", "2"},          {"16.0000", "4.00"},   {"4E+4", "2E+2"},    {"9", "3"},            {"0.01", "0.1"},      {"1E+4", "1E+2"},
            {"123456789", "11111.1111"}, {"15241578750190521", "123456789"},   {"0.5", "0.707106781"},
        };
        for (const auto& [x, expected] : table) {
            INFO("sqrt " << x);
            CHECK(S(D(x).sqrt()) == expected);
        }
    }
    CHECK(S(Decimal(2).sqrt()) == "1.414213562373095048801688724");   // Python's documented value at 28 digits
    CHECK(D("-0").sqrt().is_negative());
    CHECK_THROWS_AS(D("-1").sqrt(), DecimalInvalidOperation);
    CHECK_THROWS_AS(D("-1E-10").sqrt(), DecimalInvalidOperation);
    // digits of irrational roots, from bc: `scale=160; sqrt(2)` etc., rounded to the precision under test
    struct Row {
        const char* radicand;
        const char* digits;   // the root, to 120 places or more
    };
    const std::vector<Row> roots = {
        {"2", "1.4142135623730950488016887242096980785696718753769480731766797379907324784621070388503875343276415727350138462309122970249248360558507372126441214970999358314132"},
        {"5", "2.2360679774997896964091736687312762354406183596115257242708972454105209256378048994144144083787822749695081761507737835042532677244470738635863601215334527088667"},
        {"10", "3.1622776601683793319988935444327185337195551393252168268575048527925944386392382213442481083793002951873472841528400551485488560304538800146905195967001539033449"},
        {"0.5", "0.7071067811865475244008443621048490392848359376884740365883398689953662392310535194251937671638207863675069231154561485124624180279253686063220607485499679157066"},
    };
    for (const Row& row : roots) {
        for (const int prec : {1, 5, 28, 40, 70, 90, 110}) {
            LocalContext lc(prec);
            const Decimal root = D(row.radicand).sqrt();
            INFO("sqrt(" << row.radicand << ") at " << prec << " digits");
            CHECK(root.digits() == prec);
            CHECK(value_of(root) == rounded(value_of(D(row.digits)), prec));
        }
    }
}

TEST_CASE("Decimal square root agrees with the rounding of the exact root, by integer square roots, for random radicands", "[devkit][decimal]") {
    ContextReset reset;
    std::mt19937_64 rng(0x5917D001ULL);
    for (int round = 0; round < 1500; ++round) {
        const int prec = static_cast<int>(1 + rng() % 40);
        LocalContext lc(prec);
        Decimal x = random_decimal(rng, 60, -40, 40).copy_abs();
        if (x.is_zero()) continue;
        const Decimal root = x.sqrt();
        INFO("sqrt(" << S(x) << ") at " << prec);
        // the oracle: the exponent made even, 10**(2m) scaled in so that the integer root has at least prec + 5 digits; the exact root is that integer when the scaled radicand is a perfect square, and otherwise a
        // little above it, so that adding a half to the integer puts it on the right side of every rounding boundary
        BigInt c = x.coefficient();
        std::int64_t e = x.exponent();
        if (e % 2 != 0) {
            c = c * BigInt(10);
            e -= 1;
        }
        const int m = prec + 5 + 120;
        const BigInt radicand = c * pow10b(static_cast<unsigned>(2 * m));
        const BigInt r = BigInt::isqrt(radicand);
        const Rational scale = pow10r(static_cast<int>(e / 2 - m));
        const Rational exact_or_above = (r * r == radicand) ? Rational(r) * scale : (Rational(r) + Rational(BigInt(1), BigInt(2))) * scale;
        CHECK(value_of(root) == rounded(exact_or_above, prec));
        CHECK(root.digits() <= prec);
    }
}

TEST_CASE("Decimal unary plus, minus and abs are operations of the context; copy_abs and copy_negate are not", "[devkit][decimal]") {
    ContextReset reset;
    {
        LocalContext lc(9);
        CHECK(S(+D("1.3")) == "1.3");
        CHECK(S(+D("1.30")) == "1.30");
        CHECK(S(+D("12345678912")) == "1.23456789E+10");
        CHECK(S(+D("-12345678912")) == "-1.23456789E+10");
        CHECK(S(-D("1.3")) == "-1.3");
        CHECK(S(-D("-1.3")) == "1.3");
        CHECK(S(-D("12345678912")) == "-1.23456789E+10");
        CHECK(S(D("-12345678912").abs()) == "1.23456789E+10");
        CHECK(S(D("12345678912").abs()) == "1.23456789E+10");
        CHECK(S(D("-2.50").abs()) == "2.50");
        CHECK(S(D("12345678912").copy_abs()) == "12345678912");   // no rounding
        CHECK(S(D("-12345678912").copy_abs()) == "12345678912");
        CHECK(S(D("12345678912").copy_negate()) == "-12345678912");
        CHECK(S(D("-1.50").copy_negate()) == "1.50");
    }
    // the sign of a zero
    CHECK(S(-D("0")) == "0");     // -(+0) is +0, not -0
    CHECK(S(-D("-0")) == "0");
    CHECK(S(+D("-0")) == "0");
    CHECK(S(+D("0.00")) == "0.00");
    CHECK(S(D("-0").abs()) == "0");
    CHECK(S(D("0").abs()) == "0");
    CHECK(S(D("0").copy_negate()) == "-0");
    CHECK(S(D("-0").copy_negate()) == "0");
    CHECK(S(D("-0").copy_abs()) == "0");
}

TEST_CASE("Decimal compares by value, whatever the exponents and the sign of a zero", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(D("1.0") == D("1.00"));
    CHECK(D("1E+2") == D("100"));
    CHECK(D("-0") == D("0"));
    CHECK(D("0.00") == D("0E+10"));
    CHECK(D("1E+2") > D("99"));
    CHECK(D("99.99999999999999999999999999999999") < D("1E+2"));
    CHECK(D("-1") < D("0"));
    CHECK(D("-0") < D("1E-1000"));
    CHECK(D("-1E-1000") < D("-0"));
    CHECK(D("0.5") < D("1"));
    CHECK(D("2") > D("1.9999999999"));
    CHECK(D("-2") < D("-1.9999999999"));
    CHECK(D("123456789012345678901234567890") > D("123456789012345678901234567889.9"));
    CHECK(D("1E+1000") > D("9.99999999999E+999"));
    CHECK(D("1E-1000") < D("1E-999"));
    CHECK(D("5E+3") == D("5000"));
    CHECK_FALSE(D("5E+3") == D("5001"));
    CHECK(D("5E+3") != D("5001"));
    CHECK((D("3") <=> D("3.0")) == std::strong_ordering::equal);
    CHECK((D("3") <=> D("4")) == std::strong_ordering::less);
    CHECK((D("-3") <=> D("-4")) == std::strong_ordering::greater);
    // truth
    CHECK(static_cast<bool>(D("0.1")));
    CHECK(static_cast<bool>(D("-1E-100")));
    CHECK_FALSE(static_cast<bool>(D("0")));
    CHECK_FALSE(static_cast<bool>(D("-0")));
    CHECK_FALSE(static_cast<bool>(D("0E+100")));
    // random: against the exact rationals
    std::mt19937_64 rng(0xC04BA2EULL);
    for (int round = 0; round < 3000; ++round) {
        const Decimal a = random_decimal(rng, 30, -50, 50);
        const Decimal b = random_decimal(rng, 30, -50, 50);
        INFO(S(a) << " vs " << S(b));
        CHECK((a <=> b) == (value_of(a) <=> value_of(b)));
        CHECK((a == b) == (value_of(a) == value_of(b)));
    }
}

TEST_CASE("Decimal power with an integer exponent: the cases written by hand, then within a unit of the exact value for random ones", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(S(D("2").pow(D("10"))) == "1024");
    CHECK(S(D("1.5").pow(D("2"))) == "2.25");
    CHECK(S(D("1.5").pow(D("3"))) == "3.375");
    CHECK(S(D("0.1").pow(D("3"))) == "0.001");
    CHECK(S(D("10").pow(D("-2"))) == "0.01");
    CHECK(S(D("2").pow(D("-1"))) == "0.5");
    CHECK(S(D("2").pow(D("-3"))) == "0.125");
    CHECK(S(D("3").pow(D("40"))) == "12157665459056928801");
    CHECK(S(D("-2").pow(D("3"))) == "-8");
    CHECK(S(D("-2").pow(D("2"))) == "4");
    CHECK(S(D("-2").pow(D("-3"))) == "-0.125");
    CHECK(S(D("7.5").pow(D("0"))) == "1");
    CHECK(S(D("-7").pow(D("0"))) == "1");
    CHECK(S(D("4").pow(D("2.0"))) == "16");      // an exponent that is an integer value, however written
    CHECK(S(D("4").pow(D("2E+0"))) == "16");
    CHECK(S(D("2").pow(D("1E+1"))) == "1024");
    CHECK(S(D("2").pow(D("1"))) == "2");
    CHECK(S(D("1E+3").pow(D("2"))) == "1E+6");
    CHECK(S(D("1E-3").pow(D("2"))) == "0.000001");
    CHECK(S(D("12.5").pow(D("4"))) == "24414.0625");
    // one: 1, 1.0, 1.000 to a power give one with as many zeros after the point as the power times the base's digits there, up to the precision
    CHECK(S(D("1").pow(D("5"))) == "1");
    CHECK(S(D("1.000").pow(D("3"))) == "1.000000000");
    CHECK(S(D("1.000").pow(D("-2"))) == "1");
    CHECK(S(D("1.00").pow(D("1000"))) == "1.000000000000000000000000000");   // 28 digits
    // the zeros stop at the precision, however the power compares with it: 26, 27 (one below the digits), 28 (the digits) and 29 zeros wanted, at 28 digits
    CHECK(S(D("1.0").pow(D("26"))) == "1." + std::string(26, '0'));
    CHECK(S(D("1.0").pow(D("27"))) == "1." + std::string(27, '0'));
    CHECK(S(D("1.0").pow(D("28"))) == "1." + std::string(27, '0'));
    CHECK(S(D("1.0").pow(D("29"))) == "1." + std::string(27, '0'));
    CHECK(D("1.0").pow(D("28")).digits() == 28);
    {
        LocalContext lc(5);
        CHECK(S(D("1.0").pow(D("3"))) == "1.000");
        CHECK(S(D("1.0").pow(D("4"))) == "1.0000");
        CHECK(S(D("1.0").pow(D("5"))) == "1.0000");
        CHECK(S(D("1.0").pow(D("6"))) == "1.0000");
        CHECK(S(D("1.00").pow(D("2"))) == "1.0000");
    }
    // the largest exponent taken has 40 bits (2^40 - 1), the next one is refused whatever the base, and so is one that is no int64
    CHECK(S(D("1").pow(D("1099511627775"))) == "1");
    CHECK_THROWS_AS(D("1").pow(D("1099511627776")), DecimalOverflow);
    CHECK_THROWS_AS(D("2").pow(D("1099511627776")), DecimalOverflow);
    CHECK_THROWS_AS(D("1").pow(D("1E+30")), DecimalOverflow);
    // zero
    CHECK(S(D("0").pow(D("5"))) == "0");
    CHECK(S(D("-0").pow(D("3"))) == "-0");
    CHECK(S(D("-0").pow(D("2"))) == "0");
    CHECK_THROWS_AS(D("0").pow(D("-1")), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("0").pow(D("0")), DecimalInvalidOperation);
    // CHANGED in group C8: a fraction in the exponent was refused (not implemented); it is the real power now, whose own case is below.  Here: 4 ** 0.5 and 4 ** -1.5, which are 2 and 1/8, at 28 digits
    CHECK(S(D("4").pow(D("0.5"))) == "2.000000000000000000000000000");
    CHECK(S(D("4").pow(D("-1.5"))) == "0.1250000000000000000000000000");
    // the precision: a result of more digits is rounded to it
    {
        LocalContext lc(2);
        CHECK(S(D("7").pow(D("3"))) == "3.4E+2");
        CHECK(S(D("3").pow(D("-1"))) == "0.33");
    }
    {
        LocalContext lc(5);
        CHECK(S(D("3").pow(D("-1"))) == "0.33333");
        CHECK(S(D("2").pow(D("20"))) == "1.0486E+6");
    }
    // a random check against the exact value, which a result made by square-and-multiply in a few more digits than the context's keeps within 0.6 of a unit of the last digit (libmpdec's own bound:
    // 0.1 unit before the final rounding, half a unit in it)
    std::mt19937_64 rng(0x90E75EEDULL);
    for (int round = 0; round < 1500; ++round) {
        const int prec = static_cast<int>(2 + rng() % 40);
        LocalContext lc(prec);
        Decimal base = random_decimal(rng, 30, -20, 20).copy_abs();
        if (base.is_zero()) continue;
        const int n = static_cast<int>(rng() % 25) - 12;
        if (n == 0) continue;
        const Decimal result = base.pow(Decimal(n));
        INFO(S(base) << " ** " << n << " at " << prec << " digits: " << S(result));
        Rational exact = Rational(1);
        for (int i = 0; i < std::abs(n); ++i) exact = exact * value_of(base);
        if (n < 0) exact = Rational(1) / exact;
        const Rational error = (value_of(result) - exact).abs();
        const Rational unit = pow10r(static_cast<int>(result.adjusted()) - prec + 1);   // the last digit of the result
        CHECK(error <= unit * Rational(BigInt(6), BigInt(10)));
        CHECK(result.digits() <= prec);
    }
}

TEST_CASE("Decimal power: the General Decimal Arithmetic Specification's own examples of an integer power, at the precision it uses", "[devkit][decimal]") {
    ContextReset reset;
    // SOURCE (rule 4; read 2026-10-07, no account): the General Decimal Arithmetic Specification, https://speleotrove.com/decimal/daops.html#refpower.  For a second operand that is an integer and a result that can
    // be expressed exactly within the precision, "its exponent will be that which would result if the operation were calculated by repeated multiplication (if the second operand is negative then the reciprocal of
    // the first operand is used, with the absolute value of the second operand determining the multiplications)"; and for the rest: "Inexact finite results should be correctly rounded, but may be up to 1 ulp
    // (unit in last place) in error."  The section states NO working precision and no algorithm.  Its examples (precision 9 in the specification's own examples) are these, and the port gives each of them.
    LocalContext lc(9);
    CHECK(S(D("2").pow(D("3"))) == "8");
    CHECK(S(D("-2").pow(D("3"))) == "-8");
    CHECK(S(D("2").pow(D("-3"))) == "0.125");
    CHECK(S(D("1.7").pow(D("8"))) == "69.7575744");   // 1.7 ** 8 = 69.75757441 exactly: inexact at nine digits, and here the correct rounding and "up to 1 ulp" coincide
    // the exponent rule, in the specification's words: an exact result has the exponent of repeated multiplication, which is the sum of the exponents of the factors (the reciprocal's, for a negative power)
    CHECK(S(D("1.50").pow(D("2"))) == "2.2500");        // 1.50 * 1.50: two digits after the point, twice
    CHECK(S(D("1.5").pow(D("3"))) == "3.375");
    CHECK(S(D("0.50").pow(D("-2"))) == "4");           // 1 / 0.50 = 2, then 2 * 2 (the reciprocal's own exponent is the ideal one of a quotient: 0)
    CHECK(S(D("10").pow(D("-2"))) == "0.01");
    CHECK(S(D("100").pow(D("2"))) == "10000");
    CHECK(S(D("1E+2").pow(D("2"))) == "1E+4");
    // WHAT THESE DO AND DO NOT PIN.  Where the result is exact, the exponent and the digits are the specification's.  Where it is inexact, the specification allows 1 ulp and so does NOT settle the last digit: the
    // port's working precision is libmpdec's as its author remembers it (below), and that digit is pinned ONLY against what Python recorded -- the worst relative disagreement SPEC-gravity 4.7 records, which the
    // port reproduces, and the committed artefacts of group C7, which it wrote byte for byte -- and against the case that follows, never against a specification that is silent on it.
}

TEST_CASE("Decimal power: the working precision of libmpdec's integer power, pinned where one digit more or less changes the last digit of the result", "[devkit][decimal]") {
    ContextReset reset;
    // The port's rule is libmpdec's _mpd_qpow_int as the port's author remembers it (this tree has not read libmpdec's source): the products of the square-and-multiply are made at prec + digits(exponent) +
    // the exponent's own exponent + 2 digits, one more when the exponent is negative (the reciprocal of the base is taken first, at that precision), and the result is rounded to prec digits at the end.  The
    // specification (the case above) states no working precision and allows an inexact result to be 1 ulp in error, so it does not decide the cases below.  The
    // cases are those found by running the rule with the working precision changed by one (by two where the exponent is written with an exponent of its own, 1E+1 or 10.0): the values are the port's own, and
    // in five of them they are NOT the correctly rounded ones (a rule with a few extra digits gives such a value in about four cases of a thousand: 235 of 60,000 random bases and exponents).  They pin the rule against a change of it; they do not prove it, and the LAST DIGIT of such a result is
    // pinned only against Python's recorded outputs (the reproduction of SPEC-gravity 4.7's 5.9e-84 and the committed headers), because the specification leaves it open.
    const auto power = [](const char* base, const char* exponent, int prec) {
        LocalContext lc(prec);
        return S(D(base).pow(D(exponent)));
    };
    CHECK(power("1.867442", "14", 6) == "6272.77");                                   // the correctly rounded value is 6272.78
    CHECK(power("7.4964070", "6", 28) == "177467.5464925043153251721392");             // ... 391
    CHECK(power("3.45", "-2", 28) == "0.08401596303297626549044318421");               // ... 420
    CHECK(power("1.760067873", "10.0", 5) == "285.30");                                // 285.29
    CHECK(power("8.920855", "-20.0", 5) == "9.8145E-20");                              // 9.8146E-20
    CHECK(power("6.54787119114", "-5", 6) == "0.0000830805");                          // here the rule and the correct rounding agree, and a digit less of working precision does not
    CHECK(power("4.061810", "-30", 5) == "5.4753E-19");
    CHECK(power("3.770048", "1E+1", 6) == "580054");
}

TEST_CASE("Decimal base-10 logarithm: exact for powers of ten, otherwise the correctly rounded value, against digits computed by bc", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(S(D("1000").log10()) == "3");
    CHECK(S(D("0.001").log10()) == "-3");
    CHECK(S(D("1").log10()) == "0");
    CHECK(S(D("1.000").log10()) == "0");
    CHECK(S(D("1E+5").log10()) == "5");
    CHECK(S(D("10").log10()) == "1");
    CHECK(S(D("100.0").log10()) == "2");
    CHECK(S(D("1E-400").log10()) == "-400");
    CHECK(S(D("1E+999").log10()) == "999");
    CHECK_FALSE(D("1000").log10().is_negative());
    CHECK_FALSE(D("1").log10().is_negative());
    CHECK_THROWS_AS(D("0").log10(), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("-0").log10(), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("-5").log10(), DecimalInvalidOperation);
    // the digits, from bc 1.07.1: `scale=160; l(x)/l(10)`, as many as are used here (the 120 first are right, bc truncates)
    struct Row {
        const char* x;
        const char* digits;
    };
    const std::vector<Row> rows = {
        {"2", "0.3010299956639811952137388947244930267681898814621085413104274611271081892744245094869272521181861720406844771914309953790947678811335235059996923337046955750644"},
        {"3", "0.4771212547196624372950279032551153092001288641906958648298656403052291527836611230429683556476163015104646927682520458935629691422252273512903437060715294099332"},
        {"7", "0.8450980400142568307122162585926361934835723963239654065036349537182534399020791660661115278474885733414243100753543455862416061706836189250857975837884420769977"},
        {"0.5", "-0.3010299956639811952137388947244930267681898814621085413104274611271081892744245094869272521181861720406844771914309953790947678811335235059996923337046955750644"},
        {"123456789", "8.0915149771692704475183336230595472585150733389446664302923522309638357236426840965424903354282490714591977064292483065730693861907022223547607755491396888409765"},
        {"1.0000001", "0.0000000434294460188529180136701973587794716184534126294027719584836883711874705191488380350743571468116989978072630134373400982032758758016544152893438274634244"},
        {"9.999999", "0.9999999565705496382022629537898114058361764793483806795963114641548661901944696020708781376954487102221296682943081390406238987003573144147525742328743482202335"},
        {"0.00123", "-2.9100948885606020681955602467767038912693575019756170461627776138152581777935518151443602702941965259335890055056202599938151366748576466576754804206369506567974"},
        {"5", "0.6989700043360188047862611052755069732318101185378914586895725388728918107255754905130727478818138279593155228085690046209052321188664764940003076662953044249354"},
        {"11", "1.0413926851582250407501999712430242417067021904664530945965390186797530322332493475712947863857311790034748129922161815941725785197150074357685893432217927791197"},
    };
    for (const Row& row : rows) {
        for (const int prec : {1, 6, 17, 28, 60, 80, 110}) {
            LocalContext lc(prec);
            const Decimal logarithm = D(row.x).log10();
            INFO("log10(" << row.x << ") at " << prec << " digits: " << S(logarithm));
            CHECK(logarithm.digits() == prec);
            CHECK(value_of(logarithm) == rounded(value_of(D(row.digits)), prec));
        }
    }
    // a value of 60 digits and an adjusted exponent of 457, as legendre_reference takes the logarithm of (bc: `x=1.234567890123456789012345678901234567890123456789012345678901; l(x)/l(10)`, to which 457 is added)
    {
        LocalContext lc(80);
        const Decimal x = D("1.234567890123456789012345678901234567890123456789012345678901E+457");
        const Decimal logarithm = x.log10();
        CHECK(logarithm.digits() == 80);
        CHECK(logarithm.adjusted() == 2);   // 457.09...
        const Rational expected = Rational(457) + value_of(D("0.0915149772126998957108302782343211743723014330732879824751514822248243789784720374196375955937465698859627161932746778834457226332182384591"));
        CHECK(value_of(logarithm) == rounded(expected, 80));
    }
    // near one, where the value is small and the digits needed are many: log10(1 + 1e-50) = 1e-50 / ln(10) (to 1e-100)
    {
        LocalContext lc(20);
        const Decimal logarithm = D("1.00000000000000000000000000000000000000000000000001").log10();
        CHECK(S(logarithm) == "4.3429448190325182765E-51");   // 1e-50 / 2.302585092994045684017991454684364207601101488628772976033327900967572609677352480235997205089598298
    }
}

TEST_CASE("Decimal converts to double as float(Decimal) does: correctly rounded, whatever the length", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(D("0.1").to_double() == 0.1);
    CHECK(D("-0").to_double() == 0.0);
    CHECK(std::signbit(D("-0").to_double()));
    CHECK_FALSE(std::signbit(D("0").to_double()));
    CHECK(D("1E+400").to_double() == std::numeric_limits<double>::infinity());
    CHECK(D("-1E+400").to_double() == -std::numeric_limits<double>::infinity());
    CHECK(D("1E-400").to_double() == 0.0);
    CHECK(D("4.9E-324").to_double() == std::numeric_limits<double>::denorm_min());
    CHECK(D("1.7976931348623157E+308").to_double() == std::numeric_limits<double>::max());
    CHECK(D("9007199254740993").to_double() == 9007199254740992.0);   // 2**53 + 1 is a tie between 2**53 and 2**53 + 2: the even mantissa
    CHECK(D("9007199254740995").to_double() == 9007199254740996.0);   // a tie again: ...994 has the odd mantissa, ...996 the even
    CHECK(D("9007199254740993.000000000000001").to_double() == 9007199254740994.0);   // just above the tie
    CHECK(D("0.3971478906347806").to_double() == 0.3971478906347806);
    CHECK(D("6378136.3").to_double() == 6378136.3);
    // a coefficient of thousands of digits
    {
        const std::string long_digits = "1" + std::string(5000, '0');
        CHECK(Decimal::from_string(long_digits + "E-5000").to_double() == 1.0);
        CHECK(Decimal::from_string("0." + std::string(400, '0') + "1").to_double() == 0.0);
        CHECK(Decimal::from_string("0.1" + std::string(5000, '0') + "1").to_double() == 0.1000000000000000055511151231257827);   // the double nearest 0.1 (the digits beyond are far below half an ulp)
    }
}

TEST_CASE("Decimal format: .<n>e, .<n>E, .<n>f as Python lays them out, rounding half-even", "[devkit][decimal]") {
    ContextReset reset;
    const std::vector<std::tuple<const char*, const char*, const char*>> table = {   // number, specification, text
        {"12345.678", ".3e", "1.235e+4"},       {"12345.678", ".0e", "1e+4"},            {"1", ".17e", "1.00000000000000000e+0"},   {"5.9E-84", ".3E", "5.900E-84"},
        {"9.9996", ".3e", "1.000e+1"},          {"9.9995", ".3e", "1.000e+1"},           {"9.9985", ".3e", "9.998e+0"},             {"0", ".3e", "0.000e+3"},
        {"-1.5", ".1e", "-1.5e+0"},             {"-1.55", ".1e", "-1.6e+0"},             {"1.25", ".1e", "1.2e+0"},                 {"1.35", ".1e", "1.4e+0"},
        {"123456789E+10", ".2E", "1.23E+18"},   {"0.00012345", ".2e", "1.23e-4"},         {"0.00012355", ".3E", "1.236E-4"},         {"1E-7", ".0e", "1e-7"},
        {"1.235", ".2f", "1.24"},               {"1.225", ".2f", "1.22"},                {"2.675", ".2f", "2.68"},                  {"0.125", ".2f", "0.12"},
        {"-1.5", ".0f", "-2"},                  {"2.5", ".0f", "2"},                     {"3.5", ".0f", "4"},                       {"0.5", ".0f", "0"},
        {"-0.5", ".0f", "-0"},                  {"1E-30", ".17f", "0.00000000000000000"}, {"-1E-30", ".3f", "-0.000"},              {"1E+3", ".2f", "1000.00"},
        {"1.5E+2", ".1f", "150.0"},             {"0", ".3f", "0.000"},                   {"-0", ".2f", "-0.00"},                    {"0.4", ".0f", "0"},
        {"0.0005", ".3f", "0.000"},             {"0.0015", ".3f", "0.002"},              {"0.0025", ".3f", "0.002"},                {"123.456", ".10f", "123.4560000000"},
        {"99.9995", ".3f", "100.000"},          {"1E+20", ".0f", "100000000000000000000"}, {"0.1", ".24F", "0.100000000000000000000000"},
    };
    for (const auto& [number, spec, expected] : table) {
        INFO(number << " " << spec);
        CHECK(D(number).format(spec) == expected);
    }
    // the digits the legendre tool prints, from a sixty-digit value of sqrt(5)/2 and its logarithm
    CHECK(D("1.118033988749894848204586834365638117720309179805762862135").format(".17e") == "1.11803398874989485e+0");
    CHECK(D("-1.118033988749894848204586834365638117720309179805762862135").format(".17e") == "-1.11803398874989485e+0");
    CHECK(D("2.23606797749978969640917366873127623544061835961152572427").format(".24f") == "2.236067977499789696409174");
    CHECK(D("0.04845500650402820503").format(".17f") == "0.04845500650402821");
    // a precision with a nine in it, with two digits, and a hundred places
    CHECK(D("123456789012").format(".9e") == "1.234567890e+11");
    CHECK(D("3.14159").format(".19f") == "3.1415900000000000000");
    CHECK(D("9.5").format(".9F") == "9.500000000");
    CHECK(D("1.5").format(".99f") == "1.5" + std::string(98, '0'));
    CHECK(D("0").format(".2e") == "0.00e+2");
    CHECK(D("0").format(".0e") == "0e+0");
    // errors
    for (const char* bad : {"", "e", ".e", ".3", ".3g", "10.3e", ".3x", ".-3e", "+.3e", ".3%", "010.3f"}) {
        INFO("spec '" << bad << "'");
        CHECK_THROWS_AS(D("1.5").format(bad), DecimalError);
    }
}

TEST_CASE("Decimal division of numbers of thousands of digits is correctly rounded, and so is the square root", "[devkit][decimal]") {
    ContextReset reset;
    std::mt19937_64 rng(0x9460DECULL);
    const auto digits = [&](std::size_t n) {
        std::string s(1, static_cast<char>('1' + rng() % 9));
        for (std::size_t i = 1; i < n; ++i) s.push_back(static_cast<char>('0' + rng() % 10));
        return s;
    };
    for (const auto& [prec, na, nb] : std::vector<std::tuple<int, std::size_t, std::size_t>>{{9460, 13000, 12950}, {9460, 20, 20}, {9160, 13000, 4}, {300, 4000, 500}}) {
        LocalContext lc(prec);
        const Decimal a = Decimal::from_string(digits(na));
        const Decimal b = Decimal::from_string(digits(nb));
        INFO(prec << " digits: a has " << na << ", b has " << nb);
        const Decimal q = a / b;
        CHECK(q.digits() == prec);
        CHECK(value_of(q) == rounded(value_of(a) / value_of(b), prec));
        const Decimal root = a.sqrt();
        CHECK(root.digits() == prec);
        // root**2 is within a half unit of the last digit on each side of a (the root is irrational unless a is a square, and the tests above show correct rounding at the small sizes)
        const Rational unit = pow10r(static_cast<int>(root.exponent()));
        const Rational lower = value_of(root) - unit / Rational(2);
        const Rational upper = value_of(root) + unit / Rational(2);
        CHECK(lower * lower <= value_of(a));
        CHECK(value_of(a) <= upper * upper);
    }
}

TEST_CASE("Decimal results outside the exponent range are refused, and the operations of the tools stay inside it", "[devkit][decimal]") {
    ContextReset reset;
    CHECK_THROWS_AS(D("1E+999999") * D("10"), DecimalOverflow);
    CHECK_NOTHROW(D("1E+999998") * D("10"));
    CHECK_THROWS_AS(D("1E+999999") + D("9E+999999"), DecimalOverflow);
    CHECK_THROWS_AS(D("1E-999999") / D("10"), DecimalError);   // underflow is not implemented
    getcontext().emax = 5;
    CHECK_THROWS_AS(D("1E+5") * D("10"), DecimalOverflow);
    CHECK_NOTHROW(D("1E+4") * D("10"));
    getcontext().emax = 999999;
    getcontext().emin = -5;
    CHECK_THROWS_AS(D("1E-5") / D("10"), DecimalError);
    CHECK_NOTHROW(D("1E-4") / D("10"));
}

TEST_CASE("Decimal::from_string reads the view it is given and never a character beyond it, and takes exponents up to 15 digits", "[devkit][decimal]") {
    ContextReset reset;
    // the views end in the middle of a longer text, so a character read beyond the end is a character of the text and not a terminator
    const auto of = [](std::string_view whole, std::size_t length) { return Decimal::from_string(whole.substr(0, length)); };
    CHECK(S(of("123", 2)) == "12");          // not the 3 that follows
    CHECK(S(of("12.5", 2)) == "12");         // not the point
    CHECK(S(of("1.23", 3)) == "1.2");        // not the 3 after the fraction
    CHECK(S(of("12e5", 2)) == "12");         // not the exponent
    CHECK(S(of("1e56", 3)) == "1E+5");       // not the 6
    CHECK(S(of("-12", 2)) == "-1");
    CHECK(S(of("1-", 1)) == "1");
    CHECK_THROWS_AS(of("1e+5", 2), DecimalInvalidOperation);   // an exponent mark with nothing after it, whatever follows the view
    CHECK_THROWS_AS(of("1e5", 2), DecimalInvalidOperation);
    CHECK_THROWS_AS(of("+12", 1), DecimalInvalidOperation);
    CHECK_THROWS_AS(of("12", 0), DecimalInvalidOperation);
    // the exponent of 15 digits is the largest, of either sign: it is only parsed, and the arithmetic refuses what it cannot hold
    CHECK(S(D("1E+999999999999999")) == "1E+999999999999999");
    CHECK(S(D("1E-999999999999999")) == "1E-999999999999999");
    CHECK(S(D("1E+000999999999999999")) == "1E+999999999999999");   // (the leading zeros are no digits of value)
    CHECK_THROWS_AS(D("1E+1000000000000000"), DecimalInvalidOperation);
    CHECK_THROWS_AS(D("1E-1000000000000000"), DecimalInvalidOperation);
    CHECK_THROWS_AS(D("1E+99999999999999999999"), DecimalInvalidOperation);
}

TEST_CASE("Decimal's exceptions say what went wrong, in words that name the operation", "[devkit][decimal]") {
    ContextReset reset;
    const auto message = [](const auto& action) -> std::string {
        try {
            action();
        } catch (const std::exception& exc) {
            return exc.what();
        }
        return "(nothing was thrown)";
    };
    CHECK(message([] { (void)(D("0") / D("0")); }) == "Decimal: 0 / 0 is undefined");
    CHECK(message([] { (void)(D("1") / D("0")); }) == "Decimal: division by zero");
    CHECK(message([] { (void)D("-1").sqrt(); }) == "Decimal: the square root of a negative number");
    CHECK(message([] { (void)Decimal::from_parts(false, BigInt(-1), 0); }) == "Decimal::from_parts: the coefficient is negative");
    CHECK(message([] { (void)D("abc"); }) == "Decimal: 'abc' is not a number");
    CHECK(message([] { (void)D("-2").pow(D("0.5")); }) == "Decimal: a negative number to a power that is not an integer");   // CHANGED in group C8 (it was: only integer exponents are implemented)
    CHECK(message([] { (void)D("0").pow(D("0")); }) == "Decimal: 0 ** 0 is undefined");
    CHECK(message([] { (void)D("0").pow(D("-1")); }) == "Decimal: 0 ** a negative number");
    CHECK(message([] { (void)D("2").pow(D("1099511627776")); }) == "Decimal::pow: the exponent is too large");
    CHECK(message([] { (void)D("0").log10(); }) == "Decimal: the logarithm of zero");
    CHECK(message([] { (void)D("-5").log10(); }) == "Decimal: the logarithm of a negative number");
    CHECK(message([] { (void)D("1.5").format(".e"); }) == "Decimal::format: only the specifications .<precision>e, .E, .f and .F are implemented, not '.e'");
    CHECK(message([] { (void)D("1.5").format(".3g"); }) == "Decimal::format: the type 'g' is not implemented");
    CHECK(message([] { (void)D("1.5").format(".x3e"); }) == "Decimal::format: '.x3e' is not a precision");
    CHECK(message([] { (void)D("1.5").format(".1000001f"); }) == "Decimal::format: the precision is too large");
    CHECK(message([] { (void)(D("1E+999999") * D("10")); }) == "Decimal: the result is beyond Emax");
    CHECK(message([] { (void)(D("1E-999999") / D("10")); }) == "Decimal: the result is below Emin (underflow is not implemented)");
    // the words of the group C8 additions (from_double, the natural logarithm, the exponential, the real power's zero base)
    CHECK(message([] { (void)Decimal::from_double(std::numeric_limits<double>::infinity()); }) == "Decimal: NaN and the infinities are not implemented");
    CHECK(message([] { (void)Decimal::from_double(std::numeric_limits<double>::quiet_NaN()); }) == "Decimal: NaN and the infinities are not implemented");
    CHECK(message([] { (void)D("0").pow(D("-0.5")); }) == "Decimal: 0 ** a negative number");
    CHECK(message([] { (void)D("0").ln(); }) == "Decimal: the logarithm of zero");
    CHECK(message([] { (void)D("-1").ln(); }) == "Decimal: the logarithm of a negative number");
    CHECK(message([] { (void)D("1E+7").exp(); }) == "Decimal: exp of a number beyond the exponent range");
    CHECK(message([] { (void)D("-1E+7").exp(); }) == "Decimal: exp of a number beyond the exponent range underflows (underflow is not implemented)");
    CHECK_THROWS_AS(D("1E+7").exp(), DecimalOverflow);
    CHECK_THROWS_AS(D("-1E+7").exp(), DecimalError);
}

TEST_CASE("Decimal addition and subtraction of operands built around the rounding boundaries: a tie, a unit either side of it, a power of ten, and a sticky amount at every depth below", "[devkit][decimal]") {
    ContextReset reset;
    std::mt19937_64 rng(0xB0A7D0DEULL);
    const int precisions[] = {1, 2, 3, 4, 5, 8, 17, 28};
    for (int round = 0; round < 40000; ++round) {
        const int prec = precisions[rng() % 8];
        LocalContext lc(prec);
        // the larger operand: `prec` digits that are kept and `extra` that are dropped, the dropped ones a tie, a unit either side of one, nothing, a unit, all nines or noise; or a power of ten of that length
        const int extra = 1 + static_cast<int>(rng() % 8);
        std::string kept(1, static_cast<char>('1' + rng() % 9));
        for (int i = 1; i < prec; ++i) kept.push_back(static_cast<char>('0' + rng() % 10));
        const BigInt half = BigInt::from_decimal("5" + std::string(static_cast<std::size_t>(extra - 1), '0'));
        BigInt dropped;
        switch (rng() % 7) {
            case 0: dropped = BigInt(0); break;
            case 1: dropped = BigInt(1); break;
            case 2: dropped = half - BigInt(1); break;
            case 3: dropped = half; break;
            case 4: dropped = half + BigInt(1); break;
            case 5: dropped = pow10b(static_cast<unsigned>(extra)) - BigInt(1); break;
            default: {
                std::string noise;
                for (int i = 0; i < extra; ++i) noise.push_back(static_cast<char>('0' + rng() % 10));
                dropped = BigInt::from_decimal(noise);
            }
        }
        BigInt big = BigInt::from_decimal(kept + std::string(static_cast<std::size_t>(extra), '0')) + dropped;
        if (rng() % 8 == 0) big = pow10b(static_cast<unsigned>(prec + extra - 1));
        const std::int64_t exponent = static_cast<std::int64_t>(rng() % 41) - 20;
        // the smaller: one or two digits, from far below the last digit of the larger to just above the rounding position
        const BigInt small(static_cast<std::int64_t>(1 + rng() % 99));
        const std::int64_t offset = static_cast<std::int64_t>(rng() % static_cast<std::uint64_t>(extra + 11)) - (extra + 8);
        const Decimal a = Decimal::from_parts((rng() & 1U) != 0, big, exponent);
        const Decimal b = Decimal::from_parts((rng() & 1U) != 0, small, exponent + offset);
        const Rational qa = value_of(a);
        const Rational qb = value_of(b);
        INFO("prec " << prec << ": " << S(a) << " and " << S(b));
        CHECK(value_of(a + b) == rounded(qa + qb, prec));
        CHECK(value_of(b + a) == rounded(qa + qb, prec));
        CHECK(value_of(a - b) == rounded(qa - qb, prec));
        CHECK(value_of(b - a) == rounded(qb - qa, prec));
    }
}

TEST_CASE("Decimal::from_double is Decimal(float): the double exactly, by Python's route (group C8)", "[devkit][decimal]") {
    ContextReset reset;
    // the value the Python documentation gives for Decimal(0.1) (the decimal module's FAQ: the exact value of the double nearest 0.1)
    CHECK(S(Decimal::from_double(0.1)) == "0.1000000000000000055511151231257827021181583404541015625");
    CHECK(S(Decimal::from_double(0.5)) == "0.5");
    CHECK(S(Decimal::from_double(-2.5)) == "-2.5");
    CHECK(S(Decimal::from_double(0.75)) == "0.75");
    CHECK(S(Decimal::from_double(1.0)) == "1");                   // a whole number is its integer, exponent 0
    CHECK(S(Decimal::from_double(4.0e6)) == "4000000");
    CHECK(S(Decimal::from_double(-6378137.0)) == "-6378137");
    CHECK(S(Decimal::from_double(1e22)) == "10000000000000000000000");   // 10**22 is the largest power of ten a double holds exactly
    CHECK(S(Decimal::from_double(1e23)) == "99999999999999991611392");   // 10**23 is not one: the double nearest it is 9.999999999999999161e22
    CHECK(Decimal::from_double(4.0e6).exponent() == 0);
    CHECK(Decimal::from_double(1e23).exponent() == 0);
    CHECK(Decimal::from_double(0.5).exponent() == -1);
    CHECK(Decimal::from_double(0.1).exponent() == -55);             // 2**-55 times an odd number: 55 digits after the point
    // zero keeps its sign, and has exponent 0
    CHECK(S(Decimal::from_double(0.0)) == "0");
    CHECK(S(Decimal::from_double(-0.0)) == "-0");
    CHECK(Decimal::from_double(0.0).exponent() == 0);
    CHECK(Decimal::from_double(-0.0).is_negative());
    // the context does not round a constructor's argument: a 60-digit-prec context leaves the 55 digits of 0.1 whole, and a 5-digit one does too
    {
        LocalContext lc(5);
        CHECK(Decimal::from_double(0.1).digits() == 55);
    }
    // NaN and the infinities are not implemented
    CHECK_THROWS_AS(Decimal::from_double(std::numeric_limits<double>::infinity()), DecimalError);
    CHECK_THROWS_AS(Decimal::from_double(-std::numeric_limits<double>::infinity()), DecimalError);
    CHECK_THROWS_AS(Decimal::from_double(std::numeric_limits<double>::quiet_NaN()), DecimalError);
    // the smallest subnormal is 2**-1074: n = 1, k = 1074, the coefficient 5**1074 (751 digits) and the exponent -1074
    {
        const Decimal tiny = Decimal::from_double(std::numeric_limits<double>::denorm_min());
        CHECK(tiny.exponent() == -1074);
        CHECK(tiny.coefficient() == BigInt::pow(BigInt(5), 1074));
        CHECK(tiny.digits() == 751);
    }
    // random doubles of every magnitude: the exact rational of the bits is the value, and float() brings the double back
    std::mt19937_64 rng(0xD0B1ECULL);
    for (int round = 0; round < 3000; ++round) {
        double x = 0.0;
        switch (rng() % 4) {
            case 0: x = std::ldexp(static_cast<double>(rng() >> 11), static_cast<int>(rng() % 200) - 150); break;   // 53 random bits, scaled
            case 1: x = std::ldexp(static_cast<double>(rng() >> 11), -1074 + static_cast<int>(rng() % 60)); break;   // subnormal and the first normals
            case 2: x = static_cast<double>(static_cast<std::int64_t>(rng() >> 12)); break;                         // a whole number
            default: x = std::ldexp(static_cast<double>(rng() >> 44), static_cast<int>(rng() % 2000) - 1000); break;   // few bits, far from one
        }
        if ((rng() & 1U) != 0) x = -x;
        if (x == 0.0 || !std::isfinite(x)) continue;
        int e2 = 0;
        const double f = std::frexp(std::fabs(x), &e2);
        const BigInt m(static_cast<std::int64_t>(std::ldexp(f, 53)));
        Rational exact(m);   // m * 2**(e2 - 53)
        const int shift = e2 - 53;
        if (shift >= 0) exact = exact * Rational(BigInt::pow(BigInt(2), static_cast<unsigned>(shift)));
        else exact = exact / Rational(BigInt::pow(BigInt(2), static_cast<unsigned>(-shift)));
        if (x < 0) exact = -exact;
        const Decimal d = Decimal::from_double(x);
        INFO("x = " << x);
        CHECK(value_of(d) == exact);
        CHECK(d.to_double() == x);
        CHECK(d.is_negative() == (x < 0));
    }
}

TEST_CASE("Decimal::from_python is Decimal(str): from_string with the white space Python strips from both ends (group C8)", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(S(Decimal::from_python("3.14")) == "3.14");
    CHECK(S(Decimal::from_python("  3.14 ")) == "3.14");
    CHECK(S(Decimal::from_python("   7456.731201")) == "7456.731201");   // an SP3 field's slice: F14.6 with its leading blanks
    CHECK(S(Decimal::from_python("\t-12.50E+3\n")) == "-1.250E+4");
    CHECK(S(Decimal::from_python("\xE3\x80\x80" "1" "\xE2\x80\x83")) == "1");   // IDEOGRAPHIC SPACE and EM SPACE are white space to str.strip()
    CHECK(S(Decimal::from_python("0.00")) == "0.00");                            // a zero keeps its exponent
    // what the grammar refuses stays refused: nothing, blanks, a blank inside, an underscore, a letter, NaN and Infinity (not implemented)
    for (const char* text : {"", " ", "\n\t", "1 2", "1_0", "x", "1x", "NaN", "Infinity", "--1", "1e", ".", "+"}) {
        INFO("\"" << text << "\"");
        CHECK_THROWS_AS(Decimal::from_python(text), DecimalInvalidOperation);
    }
}

TEST_CASE("Decimal natural logarithm: exactly zero for one, otherwise the correctly rounded value, against digits computed by bc (group C8)", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(S(D("1").ln()) == "0");
    CHECK(S(D("1.000").ln()) == "0");
    CHECK(S(D("1E+0").ln()) == "0");
    CHECK(S(D("10E-1").ln()) == "0");   // the coefficient 10 with the exponent -1: the number one
    CHECK_FALSE(D("1").ln().is_negative());
    CHECK(D("1.000").ln().exponent() == 0);
    CHECK_THROWS_AS(D("0").ln(), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("-0").ln(), DecimalDivisionByZero);
    CHECK_THROWS_AS(D("-5").ln(), DecimalInvalidOperation);
    // the digits, from bc 1.07.1: `scale=500; l(x)`, the first 170 significant digits (bc truncates; the digits used here are rounded from them)
    struct Row {
        const char* x;
        const char* digits;
    };
    const std::vector<Row> rows = {
        {"2", "0.69314718055994530941723212145817656807550013436025525412068000949339362196969471560586332699641868754200148102057068573368552023575813055703267075163507596193072757082837"},
        {"3", "1.0986122886681096913952452369225257046474905578227494517346943336374942932186089668736157548137320887879700290659578657423680042259305198210528018707672774106031627691833"},
        {"7", "1.9459101490553133051053527434431797296370847295818611884593901499375798627520692677876584985878715269930616942058511409117237522576777868431489580951639007759078244681042"},
        {"0.5", "-0.69314718055994530941723212145817656807550013436025525412068000949339362196969471560586332699641868754200148102057068573368552023575813055703267075163507596193072757082837"},
        {"10", "2.3025850929940456840179914546843642076011014886287729760333279009675726096773524802359972050895982983419677840422862486334095254650828067566662873690987816894829072083255"},
        {"123456789", "18.631401766168018033193933347963204209713681841020401975185089945092217467256351777656116420190302695478476528765284320420283948035771542201960273496247272978207797688120"},
        {"1.0000001", "0.000000099999995000000333333308333335333333166666680952379702381063492053492064401154317821075513374799089865756459506466344700995027853232181834349564631098785339622598535030716"},
        {"9.999999", "2.3025849929940406840176581213260308722677679886287586903177921865707472028519547457481793840140849221143064897143856707384933957875152709848009133369944680419241188122781"},
        {"0.00123", "-6.7007411095978109248279486634618877449816064159218137948126822609403901387624959283440531292622725918838784017500988855779289919352476079230246233822868719317271567741871"},
        {"5", "1.6094379124341003746007593332261876395256013542685177219126478914741789877076577646301338780931796107999663030217155628997240052293246761996336166174637057275521796374971"},
        {"11", "2.3978952727983705440619435779651292998217068539374171752185677091305736239132367130750547080026347914147157258881379985222555691585957873953553023908011080650516419066806"},
        {"1E+100", "230.25850929940456840179914546843642076011014886287729760333279009675726096773524802359972050895982983419677840422862486334095254650828067566662873690987816894829072083255"},
        {"1E-100", "-230.25850929940456840179914546843642076011014886287729760333279009675726096773524802359972050895982983419677840422862486334095254650828067566662873690987816894829072083255"},
        {"1.5", "0.40546510810816438197801311546434913657199042346249419761401432414410067124891425126775242781731340124596854804538718000868248399017238926402013111913220144867243519835500"},
        {"0.999999", "-0.0000010000005000003333335833335333335000001428572678572539683539683448774282107551338265623932290557290520525781963661220600849060011650779995779304625251973466498649764416787"},
        {"1234567890123456789012345678901234567890", "90.011539649083434237756668443178827978681086321845699565551588375086968381539992946936315492364675340914874659473892515790216971941080731700795938404066037775310325528636"},
        {"3E-30", "-67.978940501153260829144498403608400523385554101040439829265142695389683997101965440206300397874216861471063492202629593259917759726553682878935819202196173273884053480583"},
        {"99999.99999", "11.512925464870228420084957273421487704672149109810529546833306004837863034101048114215700311051166094874442094813696755432476559504338520407817347046404042690948773325871"},
        {"1.000000000000000000000000000000000000000000000000000000000001", "0.00000000000000000000000000000000000000000000000000000000000099999999999999999999999999999999999999999999999999999999999950000000000000000000000000000000000000000000000000000000000033333333333333333333333333333333333333333333333333"},
    };
    for (const Row& row : rows) {
        for (const int prec : {1, 6, 17, 28, 60, 80, 110}) {
            LocalContext lc(prec);
            const Decimal logarithm = D(row.x).ln();
            INFO("ln(" << row.x << ") at " << prec << " digits: " << S(logarithm));
            CHECK(logarithm.digits() == prec);
            CHECK(value_of(logarithm) == rounded(value_of(D(row.digits)), prec));
        }
    }
    // ln 10 at 60 digits: the constant measmod_reference.py's LN10 and its port take (bc: l(10) = 2.30258509299404568401799145468436420760110148862877297603332790...)
    {
        LocalContext lc(60);
        CHECK(S(D("10").ln()) == "2.30258509299404568401799145468436420760110148862877297603333");
    }
    // the same value from any spelling of the argument
    {
        LocalContext lc(40);
        CHECK(D("2").ln() == D("2.000").ln());
        CHECK(D("2").ln() == D("20E-1").ln());
        CHECK(S(D("2").ln()) == S(D("0.2E+1").ln()));
    }
    // near one, where the value is small and the digits needed are many: ln(1 + 1e-50) = 1e-50 - 5e-101 + 3.3e-151 = 9.99999999999999999999999999999999999999999999999995e-51, which rounds up across a power of ten
    {
        LocalContext lc(20);
        CHECK(S(D("1.00000000000000000000000000000000000000000000000001").ln()) == "1.0000000000000000000E-50");
    }
    // exp and ln undo each other: exp(ln(x)) at 40 digits is x for a number of up to 30, ln taken at 60
    {
        for (const char* text : {"2", "0.5", "1234.5678", "9.99999", "1E+30", "1E-30", "3.141592653589793238", "0.0000123", "77777777.7777777"}) {
            Decimal l;
            {
                LocalContext lc(60);
                l = D(text).ln();
            }
            LocalContext lc(40);
            INFO(text);
            CHECK(l.exp() == D(text));
        }
    }
}

TEST_CASE("Decimal exponential: exactly one for zero, otherwise the correctly rounded value, against digits computed by bc (group C8)", "[devkit][decimal]") {
    ContextReset reset;
    CHECK(S(D("0").exp()) == "1");
    CHECK(S(D("-0").exp()) == "1");
    CHECK(S(D("0.000").exp()) == "1");
    CHECK(S(D("0E+5").exp()) == "1");
    CHECK_FALSE(D("-0").exp().is_negative());
    CHECK(D("0.000").exp().exponent() == 0);
    // the digits, from bc 1.07.1: `scale=500; e(x)`, the first 170 significant digits
    struct Row {
        const char* x;
        const char* digits;
    };
    const std::vector<Row> rows = {
        {"1", "2.7182818284590452353602874713526624977572470936999595749669676277240766303535475945713821785251664274274663919320030599218174135966290435729003342952605956307381323286279"},
        {"-1", "0.36787944117144232159552377016146086744581113103176783450783680169746149574489980335714727434591964374662732527684399520824697579279012900862665358949409878309219436737733"},
        {"0.5", "1.6487212707001281468486507878141635716537761007101480115750793116406610211942156086327765200563666430028666377563077970046711669752196091598409714524900597969294226590984"},
        {"10", "22026.465794806716516957900645284244366353512618556781074235426355225202818570792575199120968164525895451555501092457836652423291606522895166222480137728972873485577837847"},
        {"-10", "0.000045399929762484851535591515560550610237918088866564969259071305650999421614302281652525004545947782321708055089686028492945199117244520388837183347709414567560990909217007"},
        {"2.302585092994045684", "9.9999999999999999998200854531563579239906035759205678936114998052084928589447316266812804094452616602010020694289740536785003478752116847949083322949612823622644159444062"},
        {"100", "26881171418161354484126255515800135873611118.773741922415191608615280287034909564914158871097219845710811670879190576068697597709761868233548459638929871966089629133626120"},
        {"-100", "0.000000000000000000000000000000000000000000037200759760208359629596958038631183373588922923767819671206138766632904758958157181571187786422814966019356176423110698002479856420525356002661856882839075574388191160228"},
        {"0.0000001", "1.0000001000000050000001666666708333334166666680555555753968256448412725970017912257498096039783583186521554578257207233108357494776559211143810433709657789468036004871776"},
        {"700.5", "16721859620674985572410360793021203111449422613713041352496415947763492920228458780798191734909370650694357627813606284245351984509828816930484835636341464796075951298528000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
        {"1E-10", "1.0000000001000000000050000000001666666666708333333334166666666680555555555753968253970734126984154541446208388447971783810325477013020415798354166409723112352080613696023"},
        {"25.123456789", "81466423360.659567100614618629185046394906977937360268318976865317357297364102066199481175929289103539109785176313645028137550813755725253747516989586444318154537243673973"},
        {"-37.5", "0.000000000000000051755550058018685348510907057388299460248104678145500561957503671442602597463484130220279253843521862233653392645260025972558035918301596523908742378930299995747005812592"},
        {"0.001", "1.0010005001667083416680557539930583115630762005807014602285146744603597482514482984127182260041532609430682188720950993420636786961196238409723309055317683119235561984980"},
        {"5E+2", "14035922178528374107397703328409120821806021155655454250255643688895552313943821922640079350083432091928424275200106921063850971260135477134602885117174978399377774315602000000000000000000000000000000000000000000000000"},
        {"-5E+2", "0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000071245764067412855315491573771227552469277568761942948865653601614200717910222153210446982918595510098003514378639591600655964550091345194704347052793081514786824519849433"},
        {"0.693147180559945309417232121458", "1.9999999999999999999999999999996468638489997312794894917586400121894980418817582821761091025115748056319987747314226051995624804710400289454188489318832609951030804657179"},
        {"1E-50", "1.0000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000500000000000000000000000000000000000000000000000001666666666666666666"},
        {"-1E-50", "0.99999999999999999999999999999999999999999999999999000000000000000000000000000000000000000000000000004999999999999999999999999999999999999999999999999983333333333333333333"},
    };
    for (const Row& row : rows) {
        for (const int prec : {1, 6, 17, 28, 60, 80, 110}) {
            LocalContext lc(prec);
            const Decimal exponential = D(row.x).exp();
            INFO("exp(" << row.x << ") at " << prec << " digits: " << S(exponential));
            CHECK(exponential.digits() == prec);
            CHECK(value_of(exponential) == rounded(value_of(D(row.digits)), prec));
        }
    }
    // e at 60 digits, and the argument of the vapour-pressure formula's 10**x, x ln 10 (the shape measmod_reference.py's d_pow10 takes)
    {
        LocalContext lc(60);
        CHECK(S(D("1").exp()) == "2.71828182845904523536028747135266249775724709369995957496697");   // bc: e(1) = 2.718281828459045235360287471352662497757247093699959574966967627...
    }
    // beyond the exponent range: a number of 10**7 or more, and an argument whose power of ten is beyond Emax (3e6 / ln 10 = 1.3e6 > 999999)
    CHECK_THROWS_AS(D("1E+7").exp(), DecimalOverflow);
    CHECK_THROWS_AS(D("3E+6").exp(), DecimalOverflow);
    CHECK_THROWS_AS(D("-1E+7").exp(), DecimalError);
    CHECK_THROWS_AS(D("-3E+6").exp(), DecimalError);
    // but not one that is merely large: exp(2000) = 10**868.6 is a number with 869 digits before the point
    {
        LocalContext lc(20);
        CHECK(D("2000").exp().adjusted() == 868);
        CHECK(D("-2000").exp().adjusted() == -869);
    }
}

TEST_CASE("Decimal power with a fraction in the exponent: exp(y ln x) in libmpdec's working precision, against bc, and the specification's own example (group C8)", "[devkit][decimal]") {
    ContextReset reset;
    // WHAT IS PUBLISHED, AND WHAT THIS PINS.  The Python documentation (Context.power, https://docs.python.org/3/library/decimal.html, read 2026-10-08, no account): "Changed in version 3.3: The C module computes power()
    // in terms of the correctly rounded exp() and ln() functions. The result is well-defined but only “almost always correctly rounded”."  So exp(y ln x) is the published method; its working precision (here max(digits(x),
    // prec) + 4 + 19) is NOT published: it is an implementation detail, the port's author remembers it and libmpdec's source has not been read (class EMU of the rule-5 record).  As for the integer power (above), the digits
    // are therefore pinned ONLY by the committed artefacts and by bc, and not by the rule: the rows below (bc), the specification's own example, and -- the one caller of this branch is `refco` of tools/measmod_fd_sizing.cpp --
    // the Python's recorded output of the sizing (the refraction's A and B of the first normal point's weather, to seven digits) and ERFA's published eraRefco case (to 1e-15 and 1e-18 absolute), both asserted in the
    // tests of that tool.  A result that is not the correctly rounded one is possible by the documentation's own words ("almost always"); the rows below have none.
    // SOURCE: the General Decimal Arithmetic Specification's example of this section, https://speleotrove.com/decimal/daops.html#refpower (read 2026-10-07): power('10', '0.301029996') = '2.00000000' at precision 9
    {
        LocalContext lc(9);
        CHECK(S(D("10").pow(D("0.301029996"))) == "2.00000000");
    }
    // the digits, from bc 1.07.1: `scale=500; e(y * l(x))`, the first 170 significant digits.  The rule is exp(y ln x) in max(digits(x), prec) + 23 digits: a result that is not the correctly rounded one needs 23 nines or
    // zeros in a row after the last digit kept, and none of these has them.  The last digit is pinned against bc and NOT against Python's own, which the port's author has not seen (libmpdec's source is unread).
    struct Row {
        const char* x;
        const char* y;
        const char* digits;
    };
    const std::vector<Row> rows = {
        {"10", "0.301029996", "2.0000000015474237823316723103696200865549458094424732636769052252025759502872720080748828132256939523756642929873306882891255180276948826877997597558634984103864522676039"},
        {"2", "0.5", "1.4142135623730950488016887242096980785696718753769480731766797379907324784621070388503875343276415727350138462309122970249248360558507372126441214970999358314132226659275"},
        {"7.5", "0.3333333333333333333333333333333333", "1.9574338205844317977124680302058355809978638536018058443894898339359265575280682562189125161764981812580827103603379507207165554887022138661191428655210013515968673218224"},
        {"0.3", "-0.5", "1.8257418583505537115232326093360071131758156499932775140896481657749775904090757793361947872129020858824227531428379741469066929960882186297157084201399645016885622754676"},
        {"1234.5678", "2.5", "53553438.871132430115602062509946722860562074757276906255866599477552050026155696408236123603294503905943593071279103252923256450967996617235751520936603497663220533134274"},
        {"1.0000001", "-1.5", "0.99999985000001874999781250024609372292969043261687329104900970106590307997470802084372066813585732740839122920984019643692520371085224379743634660465425298893730822055592"},
        {"99999", "0.1234567891234567", "4.1426992856807713501540375626739402145001624083409431384216832262242146980143862221136636268675633386146421310219296716207848802696792522290246949863979926738777264810436"},
        {"3", "10.5", "102275.86813613463530550217901958025739560570743224916570606734632965719271544773338960460118182877114507004802443835040721669528791459314492134944633366066957193537725059"},
        {"10", "7.5", "31622776.601683793319988935444327185337195551393252168268575048527925944386392382213442481083793002951873472841528400551485488560304538800146905195967001539033449216571792"},
        {"10", "-0.123456789", "0.75256360427293398052865578043717651049382001217835761721659367173022129228196850780007364211901788572983715991415736381431577480960162412931086531722759486640807148810680"},
        {"0.5", "0.75", "0.59460355750136053335874998528023795764648604623190870650950111235973333411345857993539067226906883685801869738738460659303186318089492387839268043126900888753507575570177"},
        {"123456789", "0.9999999", "123456558.98291061510357466919388871286830554241608962694738179442289712382239379955184980477002771741060990263440278445791097187394678257074706574204434436013455469037262"},
    };
    for (const Row& row : rows) {
        for (const int prec : {1, 6, 17, 28, 60}) {
            LocalContext lc(prec);
            const Decimal powered = D(row.x).pow(D(row.y));
            INFO(row.x << " ** " << row.y << " at " << prec << " digits: " << S(powered));
            CHECK(powered.digits() == prec);
            CHECK(value_of(powered) == rounded(value_of(D(row.digits)), prec));
        }
    }
    // the cases of the rule: a negative base, zero, one, an exponent so small that the result rounds to one
    CHECK_THROWS_AS(D("-4").pow(D("0.5")), DecimalInvalidOperation);
    CHECK_THROWS_AS(D("-4").pow(D("-1.5")), DecimalInvalidOperation);
    CHECK(S(D("0").pow(D("0.5"))) == "0");
    CHECK_FALSE(D("0").pow(D("0.5")).is_negative());
    CHECK_THROWS_AS(D("0").pow(D("-0.5")), DecimalDivisionByZero);
    {
        LocalContext lc(5);
        CHECK(S(D("1").pow(D("0.5"))) == "1.0000");
        CHECK(S(D("1.00").pow(D("-2.5"))) == "1.0000");
        CHECK(S(D("1E+0").pow(D("0.5"))) == "1.0000");
    }
    {
        LocalContext lc(10);
        CHECK(S(D("7").pow(D("1E-40"))) == "1.000000000");   // 7 ** 1e-40 = 1 + 1.9e-40, which a working precision of 33 digits cannot tell from one: one, with prec - 1 zeros
        CHECK(D("7").pow(D("1E-40")).digits() == 10);
    }
    // an exponent that is an integer however written goes to the integer rule, and a fraction to this one: 4 ** 0.5 is not 2 (it is 2 with prec digits: the rule has no notion of an exact result)
    {
        LocalContext lc(6);
        CHECK(S(D("4").pow(D("0.5"))) == "2.00000");
        CHECK(S(D("4").pow(D("2.0"))) == "16");
    }
    // a base with a positive exponent (1E+1 is ten written with a coefficient of one): it is not "exactly one", and the rule gives its root (bc: sqrt(10) = 3.16227766016837933199889354443...); 1E+2 gives 10 with prec digits
    {
        LocalContext lc(6);
        CHECK(S(D("1E+1").pow(D("0.5"))) == "3.16228");
        CHECK(S(D("1E+2").pow(D("0.5"))) == "10.0000");
        CHECK(S(D("1E+1").pow(D("-0.5"))) == "0.316228");
    }
}
