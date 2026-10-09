// tests/devtools/estimation_sizing_tests.cpp — the frozen figures of SPEC-estimation section 6 (plan L0 step 8, group C9): the x87 long double emulated on rationals against the machine's own, the exact
// elimination by its postconditions, the 90-digit Jacobi iteration against closed forms, the compensated sum of CPython 3.12, the reading of the NIST files (every pattern the Python searched with),
// the design recipe, the header and the table and the report by hand-derived text, the command line, and the real data against the committed record.
// (ctests `estimation_sizing.behaviour` and `estimation_sizing.real_tree`; the old ctest names estimation.sizing_header_matches_generator and estimation.spec_table_matches_generator run the tool itself.)
//
// Every expectation is DERIVED BY HAND from the statements of estimation_sizing.py (read, never run), or comes from a SECOND METHOD written here apart from the tool (the machine's own long double, a Gauss-Jordan
// elimination in 500-digit decimals, closed forms), or from the committed record (modules/estimation/tests/registered_figures.hpp and the specification's table, produced by the Python); none is taken from
// running the port to see what it says.

#include <catch2/catch_test_macros.hpp>

#include "throwing_stream.hpp"

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/decimal.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/rational.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "estimation_sizing.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace es = odl::tools::estimation_sizing;
using namespace odl::devkit;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

constexpr bool kHostHasX87 = (LDBL_MANT_DIG == 64);   // x86-64 Linux: long double IS the x87 extended precision the design recipe was written in

Rational q(std::int64_t num, std::int64_t den = 1) { return Rational(BigInt(num), BigInt(den)); }

Rational pow2r(long k) { return k >= 0 ? Rational(BigInt(1).shifted_left(static_cast<std::size_t>(k))) : Rational(BigInt(1), BigInt(1).shifted_left(static_cast<std::size_t>(-k))); }

// a long double, exactly (a 64-bit significand): the SECOND METHOD of the emulation, which reads the machine's own result
Rational exact_of(long double v) {
    if (v == 0.0L) return Rational();
    int e = 0;
    const long double f = std::frexp(std::fabs(v), &e);
    const auto m = static_cast<unsigned long long>(std::ldexp(f, 64));
    const Rational magnitude = Rational(BigInt::from_decimal(std::to_string(m))) * pow2r(e - 64);
    return v < 0.0L ? -magnitude : magnitude;
}

std::uint64_t bits_of(double v) { return std::bit_cast<std::uint64_t>(v); }

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const es::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = es::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

// the words of the exception an action throws ("(nothing was thrown)" when it throws none)
template <typename Action>
std::string message_of(const Action& action) {
    try {
        action();
    } catch (const std::exception& exc) {
        return exc.what();
    }
    return "(nothing was thrown)";
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

bool ends_with(const std::string& text, const std::string& tail) { return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0; }

std::string slurp(const fs::path& p) { return read_text(p); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the x87 emulation

TEST_CASE("round_sig: the nearest value of the given number of significant bits, ties to the even significand, signs kept", "[estimation_sizing][behaviour]") {
    // three significant bits: the values 1.00b = 1, 1.01b = 5/4, 1.10b = 3/2, 1.11b = 7/4 in [1, 2) (spacing 1/4)
    CHECK(es::round_sig(q(1), 3) == q(1));
    CHECK(es::round_sig(q(9, 8), 3) == q(1));      // 1.125: a tie between 1 (100b, even) and 5/4 (101b): the even one
    CHECK(es::round_sig(q(11, 8), 3) == q(3, 2));  // 1.375: a tie between 5/4 (101b) and 3/2 (110b, even): the even one
    CHECK(es::round_sig(q(13, 8), 3) == q(3, 2));  // 1.625: a tie between 3/2 (110b, even) and 7/4 (111b): the even one
    CHECK(es::round_sig(q(15, 8), 3) == q(2));     // 1.875: a tie between 7/4 (111b) and 2 (1000b read at three bits: 100b, even): 2
    CHECK(es::round_sig(q(19, 16), 3) == q(5, 4)); // 1.1875: nearer to 5/4 (distance 1/16) than to 1 (3/16)
    CHECK(es::round_sig(q(17, 16), 3) == q(1));    // 1.0625: nearer to 1
    CHECK(es::round_sig(q(37, 32), 3) == q(5, 4)); // 1.15625: just above the tie 9/8 = 36/32, so up
    CHECK(es::round_sig(q(35, 32), 3) == q(1));    // 1.09375: just below the tie, so down
    CHECK(es::round_sig(q(-9, 8), 3) == q(-1));
    CHECK(es::round_sig(q(-11, 8), 3) == q(-3, 2));
    CHECK(es::round_sig(q(-37, 32), 3) == q(-5, 4));
    // below one: the same pattern scaled by a power of two
    CHECK(es::round_sig(q(9, 64), 3) == q(1, 8));
    CHECK(es::round_sig(q(11, 64), 3) == q(3, 16));
    CHECK(es::round_sig(q(7, 1024), 3) == q(7, 1024));
    // values that already have the bits are unchanged, and so is zero
    CHECK(es::round_sig(Rational(), 64) == Rational());
    CHECK(es::round_sig(pow2r(-70), 64) == pow2r(-70));
    CHECK(es::round_sig(pow2r(200), 64) == pow2r(200));
    CHECK(es::round_sig(q(-3, 4), 2) == q(-3, 4));
    // one bit: the powers of two; 3/2 is a tie between 1 and 2 (1 is the even one: 1 = 1b with the exponent 0, 2 = 1b with the exponent 1: both "100b" -- the tie goes to the even last bit of the
    // integer significand, and a one-bit significand is always 1, odd: the floor() + (twice == half and n odd) rule rounds the tie UP)
    CHECK(es::round_sig(q(3, 2), 1) == q(2));
    CHECK(es::round_sig(q(5, 4), 1) == q(1));
    CHECK(es::round_sig(q(7, 4), 1) == q(2));
    // 64 bits: 1/3 is 0xAAAAAAAAAAAAAAAB * 2^-65 (the pattern 1010...1010 rounded up by the next bits 1010...)
    CHECK(es::round_sig(q(1, 3), 64) == Rational(BigInt::from_decimal("12297829382473034411"), BigInt(1).shifted_left(65)));
    // 2^64 + 1 is a tie at 64 bits (spacing 2): to the even significand, 2^64; 2^64 + 3 is a tie between ...01 (odd) and ...10 (even): up
    const BigInt two64 = BigInt(1).shifted_left(64);
    CHECK(es::round_sig(Rational(two64 + BigInt(1)), 64) == Rational(two64));
    CHECK(es::round_sig(Rational(two64 + BigInt(3)), 64) == Rational(two64 + BigInt(4)));
    CHECK(es::round_sig(Rational(two64 + BigInt(2)), 64) == Rational(two64 + BigInt(2)));
    // 53 bits: the doubles
    CHECK(es::round_sig(q(1, 10), 53).to_double() == 0.1);
    CHECK(es::round_sig(q(1, 3), 53).to_double() == 1.0 / 3.0);
    // a carry into the next power of two: 2 - 2^-70 at 64 bits is 2
    CHECK(es::round_sig(q(2) - pow2r(-70), 64) == q(2));
}

TEST_CASE("exact_rational and rational_of: Fraction(float) and Fraction(Decimal), exactly", "[estimation_sizing][behaviour]") {
    CHECK(es::exact_rational(1.0) == q(1));
    CHECK(es::exact_rational(-2.5) == q(-5, 2));
    CHECK(es::exact_rational(0.1) == Rational(BigInt::from_decimal("3602879701896397"), BigInt::from_decimal("36028797018963968")));
    CHECK(es::exact_rational(0.0) == Rational());
    CHECK(es::exact_rational(-0.0) == Rational());
    CHECK(es::exact_rational(std::numeric_limits<double>::denorm_min()) == pow2r(-1074));
    CHECK(es::exact_rational(std::numeric_limits<double>::max()) == Rational(BigInt(1).shifted_left(1024) - BigInt(1).shifted_left(971)));
    CHECK(es::exact_rational(0x1p-53) == pow2r(-53));
    CHECK(es::exact_rational(0x1.fffffffffffffp+0) == q(2) - pow2r(-52));
    CHECK(message_of([] { (void)es::exact_rational(std::numeric_limits<double>::infinity()); }) == "a number that is not finite has no exact value");
    CHECK(message_of([] { (void)es::exact_rational(std::numeric_limits<double>::quiet_NaN()); }) == "a number that is not finite has no exact value");

    CHECK(es::rational_of(Decimal::from_string("1.5")) == q(3, 2));
    CHECK(es::rational_of(Decimal::from_string("-0.125")) == q(-1, 8));
    CHECK(es::rational_of(Decimal::from_string("1e5")) == q(100000));
    CHECK(es::rational_of(Decimal::from_string("1E-3")) == q(1, 1000));
    CHECK(es::rational_of(Decimal::from_string("0.00")) == Rational());
    CHECK(es::rational_of(Decimal::from_string("-0")) == Rational());
    CHECK(es::rational_of(Decimal::from_string("12345678901234567890.5")) == Rational(BigInt::from_decimal("24691357802469135781"), BigInt(2)));
    CHECK(es::rational_of(Decimal::from_string("-6.860120914")) == Rational(BigInt::from_decimal("-6860120914"), BigInt::from_decimal("1000000000")));
    CHECK(es::rational_of(Decimal::from_string("1e100000")) == Rational(BigInt::pow(BigInt(10), 100000)));
    CHECK(es::rational_of(Decimal::from_string("-1e-100000")) == Rational(BigInt(-1), BigInt::pow(BigInt(10), 100000)));
    CHECK(message_of([] { (void)es::rational_of(Decimal::from_string("1e100001")); }) == "the exponent of 1E+100001 is beyond what this reads");
    CHECK(message_of([] { (void)es::rational_of(Decimal::from_string("1e-100001")); }) == "the exponent of 1E-100001 is beyond what this reads");
}

TEST_CASE("Ld: a decimal string is parsed exactly and rounded once; * and / are the exact result rounded to 64 bits", "[estimation_sizing][behaviour]") {
    // by hand
    CHECK(es::Ld("0.5").value() == q(1, 2));
    CHECK(es::Ld("-6").value() == q(-6));
    CHECK(es::Ld("1e5").value() == q(100000));
    CHECK(es::Ld("1e-10").value() == es::round_sig(q(1, 10000000000), 64));
    CHECK(es::Ld(std::string_view("0.1")).value() == es::round_sig(q(1, 10), 64));
    CHECK(es::Ld(0.1).value() == es::exact_rational(0.1));   // a double has 53 bits: exact before the rounding to 64
    CHECK(es::Ld(7).value() == q(7));
    CHECK(es::Ld(std::int64_t{-7}).value() == q(-7));
    CHECK((es::Ld("0.1") * es::Ld("0.1")).value() == es::round_sig(es::round_sig(q(1, 10), 64) * es::round_sig(q(1, 10), 64), 64));
    CHECK((es::Ld("1") / es::Ld("3")).value() == Rational(BigInt::from_decimal("12297829382473034411"), BigInt(1).shifted_left(65)));
    CHECK((es::Ld("1") / es::Ld("3")).to_double() == 1.0 / 3.0);
    CHECK(es::Ld("0.1").to_double() == 0.1);
    // a string that is not a number is refused, with the text in the message
    CHECK(contains(message_of([] { (void)es::Ld(std::string_view("12x")); }), "'12x' is not a decimal number"));
    CHECK(contains(message_of([] { (void)es::Ld(std::string_view("")); }), "is not a decimal number"));
    CHECK(contains(message_of([] { (void)es::Ld(std::string_view("1_0")); }), "is not a decimal number"));
}

TEST_CASE("Ld against the machine's own long double (control K1): 0 differences in parsing, products, quotients, squares, chains and the rounding to double", "[estimation_sizing][behaviour]") {
    if (!kHostHasX87) {
        SKIP("long double is not the x87 extended precision on this host");
    }
    std::mt19937_64 rng(20261009);
    const auto random_decimal = [&rng] {
        std::string s;
        if (rng() % 2 == 0) s += '-';
        const int digits = 1 + static_cast<int>(rng() % 19);
        const int point = static_cast<int>(rng() % static_cast<std::uint64_t>(digits + 1));
        for (int i = 0; i < digits; ++i) {
            if (i == point && i != 0) s += '.';
            s += static_cast<char>('0' + (i == 0 ? 1 + rng() % 9 : rng() % 10));
        }
        if (rng() % 3 == 0) s += "e" + std::to_string(static_cast<int>(rng() % 11) - 5);   // (so that the tenth power of any of them is a normal double)
        return s;
    };
    std::size_t compared = 0;
    std::size_t differences = 0;
    const auto check = [&](const Rational& emulated, long double hardware) {
        ++compared;
        if (!(emulated == exact_of(hardware))) ++differences;
    };
    for (int i = 0; i < 9000; ++i) {
        const std::string a = random_decimal();
        const std::string b = random_decimal();
        const long double ha = std::strtold(a.c_str(), nullptr);
        const long double hb = std::strtold(b.c_str(), nullptr);
        const es::Ld ea(a);
        const es::Ld eb(b);
        check(ea.value(), ha);
        check((ea * eb).value(), ha * hb);
        check((ea / eb).value(), ha / hb);
        check((ea * ea).value(), ha * ha);
        // the chain of the recipe: p = p * x, ten times
        es::Ld p(1);
        long double hp = 1.0L;
        for (int k = 0; k < 10; ++k) {
            p = p * ea;
            hp = hp * ha;
        }
        check(p.value(), hp);
        // the double rounding of static_cast<double>(long double)
        ++compared;
        if (bits_of(p.to_double()) != bits_of(static_cast<double>(hp))) ++differences;
        ++compared;
        if (bits_of((ea / eb).to_double()) != bits_of(static_cast<double>(ha / hb))) ++differences;
    }
    CHECK(compared == 9000U * 7U);
    CHECK(differences == 0U);
    // the ties at the decimal-to-binary conversion, which no random string reaches: 2^64 + 1 and 2^64 + 3 are exact in decimal and are ties in 64 bits
    for (const char* text : {"18446744073709551617", "18446744073709551619", "18446744073709551618", "36893488147419103233", "9223372036854775809"}) {
        CHECK(es::Ld(std::string_view(text)).value() == exact_of(std::strtold(text, nullptr)));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the bounds' constants

TEST_CASE("the constants of the bounds: gamma_k, chi_n, theta_solve and theta_n, by hand and against the same formulas in 90 digits", "[estimation_sizing][behaviour]") {
    const double u = 0x1p-53;
    CHECK(es::kU == u);
    CHECK(es::kEtaRep == 2.01 * u);
    CHECK(es::kCertRelative == 1.0e-14);
    // gamma_1 = u / (1 - u): 1 - u is exact, the quotient 2^-53 (1 + 2^-53 + ...) is just above the tie between 2^-53 and its successor, so it is the successor
    CHECK(es::gamma_k(1) == 0x1.0000000000001p-53);
    CHECK(es::gamma_k(0) == 0.0);
    // chi_1 = 1 / (1 - 2u) = 1 + 2u + 4u^2 + ...: 1 - 2u is exact (0x1.ffffffffffffep-1); the quotient is 1 + 2^-52 + 2^-104, just above the double 1 + 2^-52
    CHECK(es::chi_n(1) == 0x1.0000000000001p+0);
    // against the formulas evaluated in 90 digits, to a few units of the last place
    LocalContext ctx(90);
    const Decimal du = Decimal::from_string("1") / Decimal(BigInt(1).shifted_left(53));
    const auto close = [](double got, const Decimal& want, double ulps) {
        const double w = want.to_double();
        return std::fabs(got - w) <= ulps * std::ldexp(1.0, std::ilogb(w) - 52);
    };
    for (const int k : {1, 2, 3, 4, 16, 21, 40, 82}) {
        const Decimal ku = Decimal(k) * du;
        CHECK(close(es::gamma_k(k), ku / (Decimal(1) - ku), 2));
    }
    for (const int n : {1, 2, 3, 6, 7, 11, 40}) {
        const Decimal chi = Decimal(1) / (Decimal(1) - Decimal(n + 1) * du);
        CHECK(close(es::chi_n(n), chi, 2));
        const Decimal ts = chi * (Decimal(3 * n * n + n) * du + Decimal(n * n * n) * du * du);
        CHECK(close(es::theta_solve(n), ts, 4));
        for (const int m : {n + 1, 21, 36, 82}) {
            for (const double eta : {0.0, es::kEtaRep, 2 * es::kEtaRep}) {
                const Decimal gm = Decimal(m) * du / (Decimal(1) - Decimal(m) * du);
                const Decimal tn = Decimal(n) * (gm * (Decimal(1) + gm) + Decimal::from_double(eta)) + ts;
                CHECK(close(es::theta_n(n, m, eta), tn, 8));
            }
        }
    }
    // theta_n has eta_rep = 0 by default; the two arguments are the parameters and the observations in that order
    CHECK(es::theta_n(3, 40) == es::theta_n(3, 40, 0.0));
    CHECK(es::theta_n(3, 40) != es::theta_n(40, 3));
    // Theta_s(1) by hand: chi_1 (4u + u^2), about 4.44e-16 (the first line of the tridiagonal table)
    CHECK(es::theta_solve(1) == 0x1.0000000000001p-51);
    // (4u = 2^-51 is exact, u^2 = 2^-106 is an eighth of the unit in its last place and is lost, and chi_1 = 1 + 2^-52 makes 2^-51 (1 + 2^-52), a double)
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the scale exponent

TEST_CASE("scale_exponent: frexp's exponent floor-divided by two, so that the scaled diagonal d 2^-2e lies in [0.5, 2)", "[estimation_sizing][behaviour]") {
    // frexp(d) = (m, k) with m in [0.5, 1): 1.0 = (0.5, 1), 2.0 = (0.5, 2), 3.0 = (0.75, 2), 0.5 = (0.5, 0), 0.25 = (0.5, -1), 0.125 = (0.5, -2), 0.1 = (0.8, -3), 0.001 = (0.512, -9), 1024 = (0.5, 11)
    struct Row {
        double d;
        int expected;
    };
    const Row rows[] = {{1.0, 0}, {2.0, 1}, {3.0, 1}, {3.9999, 1}, {4.0, 1}, {0.5, 0}, {0.75, 0}, {0.25, -1}, {0.125, -1}, {0.0625, -2}, {0.1, -2}, {0.001, -5},
                        {1024.0, 5}, {1023.0, 5}, {0.0, 0}, {-8.0, 2}, {-0.25, -1}, {1e300, 498}, {1e-300, -498}};
    for (const Row& r : rows) {
        INFO("d = " << r.d);
        CHECK(es::scale_exponent(r.d) == r.expected);
    }
    // the point of it: for d > 0 the scaled diagonal d 2^-2e lies in [0.5, 2)
    std::mt19937_64 rng(7);
    for (int i = 0; i < 2000; ++i) {
        const double d = std::ldexp(0.5 + static_cast<double>(rng() % 1000000) / 2000000.0, static_cast<int>(rng() % 200) - 100);
        const double scaled = std::ldexp(d, -2 * es::scale_exponent(d));
        INFO("d = " << d);
        CHECK((scaled >= 0.5 && scaled < 2.0));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the NIST files

namespace {

// a file in the layout of the NIST files, small: the certified block is lines 7 to 12, the data lines 14 to 16, as the file's own lines 5 and 6 say
std::vector<std::string> mini_lines() {
    return {"NIST/ITL StRD",                                              // 1
            "Dataset Name:  Mini (Mini.dat)",                             // 2
            "",                                                           // 3
            "File Format:   ASCII",                                       // 4
            "               Certified Values  (lines 7 to 12)",          // 5
            "               Data              (lines 14 to 16)",         // 6
            "     Parameter          Estimate             of Estimate",   // 7
            "        B0        -0.5     0.25",                            // 8
            "        B1         2.0E-01      3E+00",                     // 9
            "",                                                           // 10
            "     Residual",                                              // 11
            "     Standard Deviation   0.75",                             // 12
            "Data:       y          x",                                   // 13
            "  1.5   2.5",                                                // 14
            "  3.0   4.0",                                                // 15
            "  4.5   6.0"};                                               // 16
}

std::string joined(const std::vector<std::string>& lines, const std::string& eol = "\r\n") {
    std::string out;
    for (const std::string& l : lines) out += l + eol;
    return out;
}

es::NistFile parsed(const std::vector<std::string>& lines, const std::string& eol = "\r\n") { return es::parse_nist(joined(lines, eol), "mini.dat"); }

std::string refusal(const std::string& text) { return message_of([&] { (void)es::parse_nist(text, "mini.dat"); }); }

const char kNoCertified[] = "mini.dat: there is no \"Certified Values (lines A to B)\" in its first twelve lines";
const char kNoData[] = "mini.dat: there is no \"Data (lines A to B)\" in its first twelve lines";
const char kNoResidual[] = "mini.dat: there is no \"Residual / Standard Deviation\" line in its certified values";

using Strings = std::vector<std::string>;

}  // namespace

TEST_CASE("parse_nist: the certified values, their deviations, the residual deviation and the data rows of a file in the NIST layout, whatever the line ends", "[estimation_sizing][behaviour]") {
    for (const std::string& eol : {std::string("\r\n"), std::string("\n"), std::string("\r")}) {
        INFO("eol of " << eol.size() << " bytes, first " << static_cast<int>(eol[0]));
        const es::NistFile f = parsed(mini_lines(), eol);
        CHECK(f.cert == Strings{"-0.5", "2.0E-01"});
        CHECK(f.sd == Strings{"0.25", "3E+00"});
        CHECK(f.rsd == "0.75");
        REQUIRE(f.rows.size() == 3U);
        CHECK(f.rows[0] == Strings{"1.5", "2.5"});
        CHECK(f.rows[1] == Strings{"3.0", "4.0"});
        CHECK(f.rows[2] == Strings{"4.5", "6.0"});
    }
    // a last line without a line end is a line; an empty text has no ranges
    {
        std::string text = joined(mini_lines());
        text.resize(text.size() - 2);
        CHECK(es::parse_nist(text, "x").rows.size() == 3U);
    }
    CHECK(refusal("") == kNoCertified);
}

TEST_CASE("parse_nist: 'Certified Values (lines A to B)' and 'Data (lines A to B)' are searched for in the first twelve lines, leftmost match, with \\s+ before the parenthesis", "[estimation_sizing][behaviour]") {
    // a decoy before the real one: the first occurrence does not match, the search goes on to the next
    {
        Strings l = mini_lines();
        l[2] = "Certified Values are not repeated here; Data (lines x to y) neither";
        const es::NistFile f = parsed(l);
        CHECK(f.cert.size() == 2U);
        CHECK(f.rows.size() == 3U);
    }
    // the first MATCH wins: two ranges for the certified values, the first is the one read
    {
        Strings l = mini_lines();
        l[2] = "Certified Values (lines 9 to 9)";   // only B1's line: one certified value, and no 'Residual'
        CHECK(refusal(joined(l)) == kNoResidual);
    }
    // line 1 is in the head too (the head is lines 1 to 12, not 2 to 12)
    {
        Strings l = mini_lines();
        l[4] = "filler";
        l[0] = "Certified Values (lines 7 to 12)";
        CHECK(parsed(l).cert.size() == 2U);
        l = mini_lines();
        l[5] = "filler";
        l[0] = "Data (lines 14 to 16)";
        CHECK(parsed(l).rows.size() == 3U);
    }
    // line 12 is still in the head and line 13 is not
    {
        Strings l(20);
        for (std::size_t i = 0; i < l.size(); ++i) l[i] = "filler";
        l[10] = "Data (lines 18 to 20)";
        l[11] = "Certified Values (lines 14 to 17)";
        l[13] = "  B0 1 2";
        l[14] = "Residual";
        l[15] = "Standard Deviation 3";
        l[17] = "5 6";
        l[18] = "";
        l[19] = "";
        const es::NistFile f = parsed(l);
        CHECK(f.cert == Strings{"1"});
        CHECK(f.rsd == "3");
        CHECK(f.rows == std::vector<Strings>{{"5", "6"}});
        l[11] = "filler";
        l[12] = "Certified Values (lines 14 to 17)";   // the thirteenth line
        CHECK(refusal(joined(l)) == kNoCertified);
        l[12] = "filler";
        l[11] = "Certified Values (lines 14 to 17)";
        l[10] = "filler";
        l[12] = "Data (lines 18 to 20)";   // 'Data' on the thirteenth line
        CHECK(refusal(joined(l)) == kNoData);
    }
    // what the patterns need: whitespace (one or more) between the keyword and the parenthesis, "(lines ", digits, " to ", digits, ")"
    const std::vector<std::pair<std::string, bool>> certified_spellings = {
        {"Certified Values  (lines 7 to 12)", true},   {"Certified Values\t(lines 7 to 12)", true},    {"Certified Values (lines 7 to 12)", true},
        {"Certified Values(lines 7 to 12)", false},    {"Certified Values (lines 7 to 12", false},     {"Certified Values (line 7 to 12)", false},
        {"Certified Values ( lines 7 to 12)", false},  {"Certified Values (lines 7 to)", false},       {"Certified Values (lines 7  to 12)", false},
        {"Certified Values (lines 7 to 12 )", false},  {"certified values (lines 7 to 12)", false},    {"Certified Values (lines a to 12)", false},
        {"Certified Values (lines 7-12)", false},      {"Certified Values (lines 07 to 012)", true},   {"Certified Values\xa0(lines 7 to 12)", true},
        {"Certified Values (lines  to 12)", false},   {"Certified Values (lines 7 to )", false},
        {"Certified Values\x1f(lines 7 to 12)", true}};
    for (const auto& [spelling, found] : certified_spellings) {
        Strings l = mini_lines();
        l[4] = "               " + spelling;
        INFO("spelling: " << spelling);
        if (found) {
            CHECK(parsed(l).cert.size() == 2U);
        } else {
            CHECK(refusal(joined(l)) == kNoCertified);
        }
    }
    // the same for the data
    for (const auto& [spelling, found] : std::vector<std::pair<std::string, bool>>{{"Data (lines 14 to 16)", true}, {"Data\t\t(lines 14 to 16)", true}, {"Data(lines 14 to 16)", false},
                                                                                      {"data (lines 14 to 16)", false}, {"Data (lines 14 to 16", false}, {"Data (lines 14 until 16)", false}, {"Data (lines  to 16)", false}, {"Data (lines 14 to )", false}}) {
        Strings l = mini_lines();
        l[5] = "               " + spelling;
        INFO("spelling: " << spelling);
        if (found) {
            CHECK(parsed(l).rows.size() == 3U);
        } else {
            CHECK(refusal(joined(l)) == kNoData);
        }
    }
}

TEST_CASE("parse_nist: a certified line is \\s+ B digits \\s+ token \\s+ token \\s*; the whitespace is Python's (latin-1 includes 0x1f, 0x85 as a line end, 0xa0)", "[estimation_sizing][behaviour]") {
    const std::vector<std::pair<std::string, std::optional<std::pair<std::string, std::string>>>> table = {
        {"        B0        -0.5     0.25", std::make_pair("-0.5", "0.25")},
        {"\tB1\t2\t3", std::make_pair("2", "3")},
        {" B10 1E+3 2   ", std::make_pair("1E+3", "2")},
        {"  B12   1   2", std::make_pair("1", "2")},
        {"  B1 abc def", std::make_pair("abc", "def")},
        {" B0  -0.5  0.25 \t", std::make_pair("-0.5", "0.25")},
        {" B0 -0.5 0.25\xa0", std::make_pair("-0.5", "0.25")},
        {" B0\xa0-0.5\xa0" "0.25", std::make_pair("-0.5", "0.25")},
        {"\x1f B0 -0.5 0.25", std::make_pair("-0.5", "0.25")},
        {"\xa0" "B0 -0.5 0.25", std::make_pair("-0.5", "0.25")},
        {"B0 -0.5 0.25", std::nullopt},         // no whitespace first
        {" b0 -0.5 0.25", std::nullopt},        // lower case
        {" B -0.5 0.25", std::nullopt},         // no digits
        {" B0-0.5 0.25", std::nullopt},         // no whitespace after the digits
        {" B0 -0.5", std::nullopt},             // one token
        {" B0", std::nullopt},
        {" B0   ", std::nullopt},             // the first token is missing: only white space after the digits
        {" B0 -0.5   ", std::nullopt},        // the second token is missing: only white space after the first
        {" B0 -0.5 0.25 9", std::nullopt},      // three tokens
        {" X0 -0.5 0.25", std::nullopt},
        {" B0x -0.5 0.25", std::nullopt},
        {" BB0 -0.5 0.25", std::nullopt},
        {"", std::nullopt},
        {"   ", std::nullopt}};
    for (const auto& [line, expected] : table) {
        Strings l = mini_lines();
        l[7] = line;
        INFO("line: [" << line << "]");
        const es::NistFile f = parsed(l);
        if (expected) {
            // B1's line (line 9) matches as well, so the first entries are ours
            REQUIRE(f.cert.size() == 2U);
            CHECK(f.cert[0] == expected->first);
            CHECK(f.sd[0] == expected->second);
            CHECK(f.cert[1] == "2.0E-01");
        } else {
            CHECK(f.cert == Strings{"2.0E-01"});
            CHECK(f.sd == Strings{"3E+00"});
        }
    }
    // a line outside the certified block is not read, whatever it looks like
    {
        Strings l = mini_lines();
        l[12] = "  B7 9 9";
        CHECK(parsed(l).cert.size() == 2U);
    }
    // the parameter headings of the real files are not certified lines
    CHECK(parsed(mini_lines()).cert.size() == 2U);
}

TEST_CASE("parse_nist: 'Residual', then white space with a newline in it, then 'Standard Deviation', white space and a token -- the leftmost occurrence that fits", "[estimation_sizing][behaviour]") {
    const auto rsd_of = [](const Strings& lines_10_to_12) {
        Strings l = mini_lines();
        l[9] = lines_10_to_12[0];
        l[10] = lines_10_to_12[1];
        l[11] = lines_10_to_12[2];
        return message_of([&] { (void)parsed(l); }) == "(nothing was thrown)" ? parsed(l).rsd : std::string("REFUSED");
    };
    CHECK(rsd_of({"", "     Residual", "     Standard Deviation   0.75"}) == "0.75");
    CHECK(rsd_of({"", "Residual", "Standard Deviation 7E-3"}) == "7E-3");
    CHECK(rsd_of({"Residual", "", "  Standard Deviation 0.75"}) == "0.75");                        // a blank line between: the run of white space has two newlines in it
    CHECK(rsd_of({"Residual   ", "   ", "Standard Deviation\t0.75"}) == "0.75");
    CHECK(rsd_of({"Residual Sum of Squares 26.6", "Residual", "Standard Deviation 0.75"}) == "0.75");   // the first occurrence has no newline in its white space: the search goes on
    CHECK(rsd_of({"Residual", "Degrees of Freedom", "Standard Deviation 0.75"}) == "REFUSED");           // ... and when the word after the newline is another, it does not fit
    CHECK(rsd_of({"", "Residual Standard Deviation 0.75", ""}) == "REFUSED");                           // on ONE line there is no newline between them
    CHECK(rsd_of({"", "Residual\xa0" "Standard Deviation 0.75", ""}) == "REFUSED");
    CHECK(rsd_of({"", "Residual", "Standard Deviation"}) == "REFUSED");                                  // no value
    CHECK(rsd_of({"", "Residual", "Standard Deviation    "}) == "REFUSED");                              // white space and no value
    CHECK(rsd_of({"", "Residual", "Standard Deviation0.75"}) == "REFUSED");                              // no white space before the value
    CHECK(rsd_of({"", "Residual", "standard deviation 0.75"}) == "REFUSED");
    CHECK(rsd_of({"", "residual", "Standard Deviation 0.75"}) == "REFUSED");
    CHECK(rsd_of({"", "Residual", "Standard  Deviation 0.75"}) == "REFUSED");
    CHECK(rsd_of({"", "Residual", "Standard Deviation   0.75   R-Squared 0.99"}) == "0.75");           // the first token only
    CHECK(rsd_of({"", "Residual", "  \t Standard Deviation 0.75"}) == "0.75");
    CHECK(rsd_of({"", "Residual:", "Standard Deviation 0.75"}) == "REFUSED");                            // "Residual" must be followed by white space only
    CHECK(rsd_of({"", "xResidual", "Standard Deviation 0.75"}) == "0.75");                               // ... but nothing is required BEFORE it
    // the value that follows is outside the certified block: not seen
    {
        Strings l = mini_lines();
        l[11] = "Residual";
        l[12] = "Standard Deviation 0.75";   // line 13, beyond the block's last line (12)
        CHECK(message_of([&] { (void)parsed(l); }) == kNoResidual);
    }
    // CRLF between the two lines (the usual case) -- universal newlines make it one newline
    CHECK(parsed(mini_lines(), "\r\n").rsd == "0.75");
}

TEST_CASE("parse_nist: the line ranges are Python slices -- a stop beyond the file is its end, a start of 0 counts from the end, huge numbers are fine", "[estimation_sizing][behaviour]") {
    {
        Strings l = mini_lines();
        l[4] = "Certified Values  (lines 7 to 99)";
        CHECK(parsed(l).rsd == "0.75");
        l[4] = "Certified Values  (lines 7 to 99999999999999999999999)";
        CHECK(parsed(l).rsd == "0.75");
        l[4] = "Certified Values  (lines 9 to 12)";   // from B1: the first certified line is gone
        CHECK(parsed(l).cert == Strings{"2.0E-01"});
        l[4] = "Certified Values  (lines 8 to 9)";    // no 'Residual'
        CHECK(refusal(joined(l)) == kNoResidual);
        l[4] = "Certified Values  (lines 0 to 12)";   // lines[-1:12]: the last line to the twelfth: empty
        CHECK(refusal(joined(l)) == kNoResidual);
        l[4] = "Certified Values  (lines 1 to 12)";   // lines[0:12]: includes the two header lines 5 and 6, which are no certified lines
        CHECK(parsed(l).cert.size() == 2U);
        l[4] = "Certified Values  (lines 12 to 7)";   // stop before start: empty
        CHECK(refusal(joined(l)) == kNoResidual);
    }
    {
        Strings l = mini_lines();
        l[5] = "Data  (lines 14 to 99)";
        CHECK(parsed(l).rows.size() == 3U);
        l[5] = "Data  (lines 15 to 16)";
        CHECK(parsed(l).rows == std::vector<Strings>{{"3.0", "4.0"}, {"4.5", "6.0"}});
        l[5] = "Data  (lines 14 to 14)";
        CHECK(parsed(l).rows == std::vector<Strings>{{"1.5", "2.5"}});
        l[5] = "Data  (lines 17 to 30)";
        CHECK(parsed(l).rows.empty());
        l[5] = "Data  (lines 0 to 3)";   // lines[-1:3]: empty
        CHECK(parsed(l).rows.empty());
        l[5] = "Data  (lines 99999999999999999999999 to 99999999999999999999999)";
        CHECK(parsed(l).rows.empty());
        // the numbers of 17, 18, 19 and 20 digits are beyond any file, as bounds either way: nothing overflows on the way to being clamped
        for (const char* big : {"99999999999999999", "999999999999999999", "9999999999999999999", "99999999999999999999", "100000000000000000", "1000000000000000000", "9223372036854775807", "9223372036854775808"}) {
            l[5] = std::string("Data  (lines ") + big + " to " + big + ")";
            CHECK(parsed(l).rows.empty());
            l[5] = std::string("Data  (lines 14 to ") + big + ")";
            CHECK(parsed(l).rows.size() == 3U);
            l[5] = std::string("Data  (lines ") + big + " to 16)";
            CHECK(parsed(l).rows.empty());
        }
        l[5] = "Data  (lines 13 to 14)";   // the heading line 13 has tokens too: they are rows (the tool takes every non-blank line of the range)
        CHECK(parsed(l).rows == std::vector<Strings>{{"Data:", "y", "x"}, {"1.5", "2.5"}});
    }
}

TEST_CASE("parse_nist: data rows are the tokens of the non-blank lines of the range; the text is latin-1 and the lines are Python's splitlines()", "[estimation_sizing][behaviour]") {
    // blank, whitespace-only and non-breaking-space-only lines are skipped; tabs and 0xa0 separate tokens
    {
        Strings l = mini_lines();
        l.push_back("");
        l.push_back("   \t ");
        l.push_back("\xa0\xa0");
        l.push_back("7.5\t8.5");
        l.push_back("9.5\xa0" "10.5");
        l[5] = "Data  (lines 14 to 21)";
        const es::NistFile f = parsed(l);
        REQUIRE(f.rows.size() == 5U);
        CHECK(f.rows[3] == Strings{"7.5", "8.5"});
        CHECK(f.rows[4] == Strings{"9.5", "10.5"});
    }
    // a byte above 0x7f in a line that is not read changes nothing
    {
        Strings l = mini_lines();
        l[1] = "Dataset Name:  Mini caf\xe9 (Mini.dat)";
        CHECK(parsed(l).cert.size() == 2U);
    }
    // ... and in a line that is read it is ONE character, kept: each byte of latin-1 becomes the code point of the same number (the token holds the UTF-8 of U+0080, U+00E9 and U+00FF)
    {
        Strings l = mini_lines();
        l.push_back("5.5\x80" "x 6.5\xe9 7.5\xff");
        l[5] = "Data  (lines 14 to 17)";
        const es::NistFile f = parsed(l);
        REQUIRE(f.rows.size() == 4U);
        CHECK(f.rows[3] == Strings{"5.5\xc2\x80" "x", "6.5\xc3\xa9", "7.5\xc3\xbf"});
    }
    // a character that ends a line for splitlines() ends it here: the file below has one line fewer before the block than the numbers of its header say, and one of these makes up the difference
    for (const std::string& sep : {std::string("\x0b"), std::string("\x0c"), std::string("\x1c"), std::string("\x1d"), std::string("\x1e"), std::string("\x85")}) {
        Strings l = mini_lines();
        l.erase(l.begin() + 2);   // the blank line 3
        l[0] = "NIST/ITL" + sep + "StRD";
        INFO("separator " << static_cast<int>(static_cast<unsigned char>(sep[0])));
        const es::NistFile f = parsed(l);
        CHECK(f.cert == Strings{"-0.5", "2.0E-01"});
        CHECK(f.rsd == "0.75");
        CHECK(f.rows.size() == 3U);
    }
    // ... and one that does not end a line does not: the numbers of the header are then one line out, and the data block "lines 14 to 16" holds two of the three rows, the first being line 13
    for (const std::string& sep : {std::string("\x1f"), std::string("\xa0"), std::string(" "), std::string("\t")}) {
        Strings l = mini_lines();
        l.erase(l.begin() + 2);
        l[0] = "NIST/ITL" + sep + "StRD";
        INFO("separator " << static_cast<int>(static_cast<unsigned char>(sep[0])));
        CHECK(parsed(l).rows == std::vector<Strings>{{"3.0", "4.0"}, {"4.5", "6.0"}});
    }
    // an 0x85 directly before the line end makes an empty line of its own (U+0085 and "\n" are two terminators; only "\r\n" is one)
    {
        Strings l = mini_lines();
        l.erase(l.begin() + 2);
        const es::NistFile f = es::parse_nist("NIST/ITL StRD\x85\r\n" + joined(Strings(l.begin() + 1, l.end())), "x");
        CHECK(f.rsd == "0.75");
    }
}

TEST_CASE("read_nist: <cache>/nist-strd-lls-<lower case name>/<Name>.dat, and a file that cannot be read is refused, naming it", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    fs::create_directories(cache / "nist-strd-lls-noint1");
    write_text(cache / "nist-strd-lls-noint1" / "NoInt1.dat", joined(mini_lines()));
    const es::NistFile f = es::read_nist(cache, "NoInt1");
    CHECK(f.rsd == "0.75");
    CHECK(f.cert.size() == 2U);
    // the file is read as bytes: a file that is not UTF-8 is no problem
    {
        Strings l = mini_lines();
        l[1] = "caf\xe9";
        write_text(cache / "nist-strd-lls-noint1" / "NoInt1.dat", joined(l));   // (the bytes it is given)
        CHECK(es::read_nist(cache, "NoInt1").rsd == "0.75");
        // upper-case letters of the name become lower case, the first and the last of the alphabet too
        fs::create_directories(cache / "nist-strd-lls-azaz");
        write_text(cache / "nist-strd-lls-azaz" / "AZaz.dat", joined(l));
        CHECK(es::read_nist(cache, "AZaz").rsd == "0.75");
    }
    const std::string missing = message_of([&] { (void)es::read_nist(cache, "Norris"); });
    CHECK(contains(missing, "cannot read"));
    CHECK(contains(missing, (cache / "nist-strd-lls-norris" / "Norris.dat").string()));
    fs::create_directories(cache / "nist-strd-lls-pontius" / "Pontius.dat");   // a directory where the file should be
    const std::string directory = message_of([&] { (void)es::read_nist(cache, "Pontius"); });
    CHECK(contains(directory, "cannot read"));
    CHECK(contains(directory, "not a regular file"));
    // a file that is not in the NIST layout is refused with the file's path
    write_text(cache / "nist-strd-lls-noint1" / "NoInt1.dat", "nothing of the layout\n");
    const std::string bad = message_of([&] { (void)es::read_nist(cache, "NoInt1"); });
    CHECK(contains(bad, (cache / "nist-strd-lls-noint1" / "NoInt1.dat").string()));
    CHECK(contains(bad, "Certified Values"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the design recipe

namespace {

es::Tokens rows_of(std::initializer_list<Strings> rows) { return es::Tokens(rows); }

}  // namespace

TEST_CASE("design_exact: the response is the first column, the design is the recipe of the dataset in rationals", "[estimation_sizing][behaviour]") {
    {   // NoInt: the abscissa alone
        const es::ExactDesign d = es::design_exact("NoInt1", rows_of({{"130", "60"}, {"131", "61"}}));
        CHECK(d.y == std::vector<Rational>{q(130), q(131)});
        CHECK(d.x == es::RMatrix{{q(60)}, {q(61)}});
    }
    {   // Longley: a one, then every other column
        const es::ExactDesign d = es::design_exact("Longley", rows_of({{"60323", "83.0", "234289", "2356", "1590", "107608", "1947"}}));
        CHECK(d.y == std::vector<Rational>{q(60323)});
        CHECK(d.x == es::RMatrix{{q(1), q(83), q(234289), q(2356), q(1590), q(107608), q(1947)}});
        CHECK(es::design_exact("Longley", rows_of({{"5"}})).x == es::RMatrix{{q(1)}});   // (no abscissa is read, so none is needed)
    }
    {   // Norris: one, x
        const es::ExactDesign d = es::design_exact("Norris", rows_of({{"0.1", "100"}}));
        CHECK(d.y == std::vector<Rational>{q(1, 10)});
        CHECK(d.x == es::RMatrix{{q(1), q(100)}});
    }
    {   // Pontius: one, x, x^2
        const es::ExactDesign d = es::design_exact("Pontius", rows_of({{"7", "0.5"}, {"8", "-3"}}));
        CHECK(d.x == es::RMatrix{{q(1), q(1, 2), q(1, 4)}, {q(1), q(-3), q(9)}});
    }
    {   // Wampler1 .. 5 (and any other name): one, x, ..., x^5
        for (const char* name : {"Wampler1", "Wampler2", "Wampler3", "Wampler4", "Wampler5", "Anything"}) {
            const es::ExactDesign d = es::design_exact(name, rows_of({{"1", "2"}}));
            CHECK(d.x == es::RMatrix{{q(1), q(2), q(4), q(8), q(16), q(32)}});
        }
    }
    {   // Filip: one, x, ..., x^10
        const es::ExactDesign d = es::design_exact("Filip", rows_of({{"0.8", "-3"}}));
        REQUIRE(d.x.size() == 1U);
        REQUIRE(d.x[0].size() == 11U);
        CHECK(d.y == std::vector<Rational>{q(4, 5)});
        Rational p = q(1);
        for (const Rational& entry : d.x[0]) {
            CHECK(entry == p);
            p = p * q(-3);
        }
        CHECK(d.x[0][10] == q(59049));
    }
    {   // decimal strings are exact, with signs, exponents and leading points
        const es::ExactDesign d = es::design_exact("Norris", rows_of({{"-.5", "1E+2"}, {"+2.50e-1", "0.1"}}));
        CHECK(d.y == std::vector<Rational>{q(-1, 2), q(1, 4)});
        CHECK(d.x == es::RMatrix{{q(1), q(100)}, {q(1), q(1, 10)}});
    }
    // refusals: no abscissa (except Longley), a token that is not a number
    CHECK(contains(message_of([&] { (void)es::design_exact("Norris", rows_of({{"1"}})); }), "Norris: a data row has no abscissa (it has 1 columns)"));
    CHECK(contains(message_of([&] { (void)es::design_exact("Filip", rows_of({{"1"}})); }), "Filip: a data row has no abscissa"));
    CHECK(contains(message_of([&] { (void)es::design_exact("NoInt1", rows_of({{"1"}})); }), "NoInt1: a data row has no abscissa"));
    CHECK(contains(message_of([&] { (void)es::design_exact("Pontius", rows_of({{"1"}})); }), "Pontius: a data row has no abscissa"));
    CHECK(contains(message_of([&] { (void)es::design_exact("Wampler1", rows_of({{"1"}})); }), "Wampler1: a data row has no abscissa"));
    CHECK(contains(message_of([&] { (void)es::design_exact("Norris", rows_of({{"1", "abc"}})); }), "'abc' is not a decimal number"));
    CHECK(contains(message_of([&] { (void)es::design_exact("Norris", rows_of({{"1", "2", "NaN"}})); }), "'NaN' is not a decimal number"));
    CHECK(es::design_exact("Norris", rows_of({})).x.empty());
}

TEST_CASE("design_double: the abscissa parsed in extended precision, the powers formed there by repeated multiplication, each rounded to double once", "[estimation_sizing][behaviour]") {
    // exactly representable cases, whatever the host
    {
        const es::DoubleDesign d = es::design_double("Wampler1", rows_of({{"3", "2"}}));
        CHECK(d.y == std::vector<double>{3.0});
        CHECK(d.x == es::DMatrix{{1.0, 2.0, 4.0, 8.0, 16.0, 32.0}});
        const es::DoubleDesign p = es::design_double("Pontius", rows_of({{"1", "0.5"}}));
        CHECK(p.x == es::DMatrix{{1.0, 0.5, 0.25}});
        const es::DoubleDesign n = es::design_double("NoInt2", rows_of({{"2", "-4"}}));
        CHECK(n.x == es::DMatrix{{-4.0}});
        const es::DoubleDesign l = es::design_double("Longley", rows_of({{"1", "2", "3"}}));
        CHECK(l.x == es::DMatrix{{1.0, 2.0, 3.0}});
    }
    CHECK(contains(message_of([&] { (void)es::design_double("Norris", rows_of({{"1"}})); }), "Norris: a data row has no abscissa"));
    CHECK(contains(message_of([&] { (void)es::design_double("Norris", rows_of({{"1", "x1"}})); }), "'x1' is not a decimal number"));
    if (!kHostHasX87) {
        SKIP("long double is not the x87 extended precision on this host");
    }
    // the machine's own recipe, written again: strtold, long double products, one rounding to double
    const std::vector<std::string> abscissae = {"0.1", "-6.860120914", "1.1", "3.141592653589793238", "7", "-0.07", "12345.6789", "0.3", "-2.5E-3", "99.99999999999"};
    for (const char* name : {"Norris", "Pontius", "Filip", "Wampler1", "Wampler5", "NoInt1", "Longley"}) {
        for (const std::string& x : abscissae) {
            const long double hx = std::strtold(x.c_str(), nullptr);
            const long double hy = std::strtold("0.3", nullptr);
            const es::DoubleDesign d = es::design_double(name, rows_of({{"0.3", x}}));
            REQUIRE(d.x.size() == 1U);
            std::vector<double> expected;
            const std::string n = name;
            if (n == "NoInt1") {
                expected = {static_cast<double>(hx)};
            } else if (n == "Longley") {
                expected = {1.0, static_cast<double>(hx)};
            } else if (n == "Norris") {
                expected = {1.0, static_cast<double>(hx)};
            } else if (n == "Pontius") {
                expected = {1.0, static_cast<double>(hx), static_cast<double>(hx * hx)};
            } else {
                long double p = 1.0L;
                expected.push_back(1.0);
                for (int k = 0; k < (n == "Filip" ? 10 : 5); ++k) {
                    p = p * hx;
                    expected.push_back(static_cast<double>(p));
                }
            }
            INFO(name << " x = " << x);
            REQUIRE(d.x[0].size() == expected.size());
            for (std::size_t i = 0; i < expected.size(); ++i) CHECK(bits_of(d.x[0][i]) == bits_of(expected[i]));
            REQUIRE(d.y.size() == 1U);
            CHECK(bits_of(d.y[0]) == bits_of(static_cast<double>(hy)));
        }
    }
    // the recipe is NOT plain double arithmetic: somewhere among these abscissae a power formed in extended precision and rounded once differs from the same power formed in double
    {
        std::size_t differing = 0;
        for (const std::string& x : abscissae) {
            const es::DoubleDesign d = es::design_double("Filip", rows_of({{"1", x}}));
            double p = 1.0;
            const double dx = std::strtod(x.c_str(), nullptr);
            for (int k = 1; k <= 10; ++k) {
                p = p * dx;
                if (bits_of(p) != bits_of(d.x.at(0).at(static_cast<std::size_t>(k)))) ++differing;
            }
        }
        CHECK(differing > 10U);
    }
}

TEST_CASE("checksum: the bit patterns of every entry and of the response, row by row, summed modulo 2^64", "[estimation_sizing][behaviour]") {
    // 1.0 = 0x3FF0..., 2.0 = 0x4000..., 3.0 = 0x4008...
    CHECK(es::checksum({{1.0, 2.0}}, {3.0}) == 0xBFF8000000000000ULL);
    CHECK(es::checksum({{1.0}, {2.0}}, {3.0, 0.0}) == 0x3FF0000000000000ULL + 0x4000000000000000ULL + 0x4008000000000000ULL);
    CHECK(es::checksum({}, {}) == 0ULL);
    CHECK(es::checksum({{0.0}}, {0.0}) == 0ULL);
    CHECK(es::checksum({{-0.0}}, {0.0}) == 0x8000000000000000ULL);
    // wrapping: -1.0 = 0xBFF0..., -2.0 = 0xC000...; the sum is 0x17FF0... and the carry is dropped
    CHECK(es::checksum({{-1.0}}, {-2.0}) == 0x7FF0000000000000ULL);
    // the response counts, and so does every entry of every row
    CHECK(es::checksum({{1.0}}, {2.0}) == 0x3FF0000000000000ULL + 0x4000000000000000ULL);
    CHECK(es::checksum({{1.0, 1.0}}, {1.0}) == 3 * 0x3FF0000000000000ULL);
    CHECK(es::checksum({{1.0}, {1.0}}, {1.0, 1.0}) == 4 * 0x3FF0000000000000ULL);
    // a row with no response is ignored (zip stops at the shorter)
    CHECK(es::checksum({{1.0}, {2.0}}, {1.0}) == 2 * 0x3FF0000000000000ULL);
    CHECK(es::checksum({{1.0}}, {1.0, 5.0}) == 2 * 0x3FF0000000000000ULL);
    // order does not matter for a sum, the values do: the same bits in other places change nothing, a different bit does
    CHECK(es::checksum({{1.0, 2.0}}, {3.0}) == es::checksum({{3.0, 1.0}}, {2.0}));
    CHECK(es::checksum({{1.0, 2.0}}, {3.0}) != es::checksum({{1.0, 2.0}}, {3.0000000000000004}));
    // NaN and infinity have bit patterns too
    CHECK(es::checksum({{std::numeric_limits<double>::infinity()}}, {0.0}) == 0x7FF0000000000000ULL);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- exact linear algebra

namespace {

std::vector<Rational> times(const es::RMatrix& m, const std::vector<Rational>& x) {
    std::vector<Rational> out(m.size());
    for (std::size_t i = 0; i < m.size(); ++i) {
        for (std::size_t j = 0; j < x.size(); ++j) out[i] += m[i][j] * x[j];
    }
    return out;
}

es::RMatrix times(const es::RMatrix& a, const es::RMatrix& b) {
    es::RMatrix out(a.size(), std::vector<Rational>(b[0].size()));
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b[0].size(); ++j) {
            for (std::size_t k = 0; k < b.size(); ++k) out[i][j] += a[i][k] * b[k][j];
        }
    }
    return out;
}

es::RMatrix identity(std::size_t n) {
    es::RMatrix out(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) out[i][i] = q(1);
    return out;
}

std::uint64_t binom(unsigned n, unsigned k) {
    std::uint64_t r = 1;
    for (unsigned i = 1; i <= k; ++i) r = r * (n - k + i) / i;
    return r;
}

es::RMatrix hilbert(std::size_t n) {
    es::RMatrix h(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) h[i][j] = q(1, static_cast<std::int64_t>(i + j + 1));
    }
    return h;
}

// the inverse of the Hilbert matrix has the integer entries (-1)^(i+j) (i+j-1) C(n+i-1, n-j) C(n+j-1, n-i) C(i+j-2, i-1)^2 (1-based)
es::RMatrix hilbert_inverse(unsigned n) {
    es::RMatrix h(n, std::vector<Rational>(n));
    for (unsigned i = 1; i <= n; ++i) {
        for (unsigned j = 1; j <= n; ++j) {
            const auto c = static_cast<std::int64_t>((i + j - 1) * binom(n + i - 1, n - j) * binom(n + j - 1, n - i) * binom(i + j - 2, i - 1) * binom(i + j - 2, i - 1));
            h[i - 1][j - 1] = q(((i + j) % 2 == 0) ? c : -c);
        }
    }
    return h;
}

// a matrix with positive diagonal dominance and fractional entries, from a seed: never singular
es::RMatrix random_matrix(std::mt19937_64& rng, std::size_t n) {
    es::RMatrix m(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) m[i][j] = q(static_cast<std::int64_t>(rng() % 11) - 5, 1 + static_cast<std::int64_t>(rng() % 7));
        m[i][i] += q(40 + static_cast<std::int64_t>(rng() % 5));
    }
    return m;
}

}  // namespace

TEST_CASE("dec: Decimal(numerator) / Decimal(denominator) in the context in force", "[estimation_sizing][behaviour]") {
    LocalContext ctx(90);
    CHECK(es::dec(q(7, 4)).to_string() == "1.75");
    CHECK(es::dec(q(-2)).to_string() == "-2");
    CHECK(es::dec(Rational()).to_string() == "0");
    CHECK(es::dec(q(1, 3)).to_string() == "0." + std::string(90, '3'));
    CHECK(es::dec(q(-2, 3)).to_string() == "-0." + std::string(89, '6') + "7");
    CHECK(es::dec(q(1, 8)).to_double() == 0.125);
    {
        LocalContext narrow(5);
        CHECK(es::dec(q(1, 3)).to_string() == "0.33333");
    }
    // a numerator of more than the context's digits is still exact before the division
    const Rational big(BigInt::pow(BigInt(10), 100) + BigInt(1), BigInt(1));
    CHECK(es::dec(big).to_string().size() > 90U);
}

TEST_CASE("exact_solve: Gauss-Jordan in rationals, the first non-zero pivot of each column; the answer is the exact solution (checked by N x == b)", "[estimation_sizing][behaviour]") {
    // 2x2 by hand: 2x + y = 3, x + 3y = 5 -> x = 4/5, y = 7/5
    {
        const std::vector<Rational> x = es::exact_solve({{q(2), q(1)}, {q(1), q(3)}}, {q(3), q(5)});
        CHECK(x == std::vector<Rational>{q(4, 5), q(7, 5)});
    }
    // a zero on the diagonal: the rows are exchanged
    {
        const std::vector<Rational> x = es::exact_solve({{q(0), q(1)}, {q(1), q(0)}}, {q(2), q(3)});
        CHECK(x == std::vector<Rational>{q(3), q(2)});
    }
    // a pivot that is zero only after the first elimination
    {
        const es::RMatrix a = {{q(1), q(1), q(1)}, {q(1), q(1), q(2)}, {q(1), q(2), q(3)}};
        const std::vector<Rational> b = {q(6), q(9), q(14)};
        const std::vector<Rational> x = es::exact_solve(a, b);
        CHECK(times(a, x) == b);
        CHECK(x == std::vector<Rational>{q(1), q(2), q(3)});
    }
    // 1x1, and a zero right-hand side
    CHECK(es::exact_solve({{q(4)}}, {q(2)}) == std::vector<Rational>{q(1, 2)});
    CHECK(es::exact_solve({{q(4), q(1)}, {q(1), q(4)}}, {q(0), q(0)}) == std::vector<Rational>{Rational(), Rational()});
    // the Hilbert matrix: x = H^-1 b with b = H (1, 2, ..., n) is (1, 2, ..., n) exactly
    for (std::size_t n = 2; n <= 8; ++n) {
        const es::RMatrix h = hilbert(n);
        std::vector<Rational> truth;
        for (std::size_t i = 0; i < n; ++i) truth.push_back(q(static_cast<std::int64_t>(i + 1)));
        const std::vector<Rational> x = es::exact_solve(h, times(h, truth));
        CHECK(x == truth);
    }
    // random matrices: the residual is exactly zero
    std::mt19937_64 rng(11);
    for (int trial = 0; trial < 12; ++trial) {
        const std::size_t n = 2 + rng() % 6;
        const es::RMatrix a = random_matrix(rng, n);
        std::vector<Rational> b(n);
        for (Rational& v : b) v = q(static_cast<std::int64_t>(rng() % 100) - 50, 1 + static_cast<std::int64_t>(rng() % 9));
        const std::vector<Rational> x = es::exact_solve(a, b);
        CHECK(times(a, x) == b);
    }
    // a row that is a multiple of another: singular
    CHECK(message_of([] { (void)es::exact_solve({{q(1), q(2)}, {q(2), q(4)}}, {q(1), q(2)}); }) == "the matrix is singular");
    CHECK(message_of([] { (void)es::exact_solve({{q(0), q(0)}, {q(0), q(1)}}, {q(1), q(2)}); }) == "the matrix is singular");
    CHECK(message_of([] { (void)es::exact_solve({{q(0)}}, {q(1)}); }) == "the matrix is singular");
}

TEST_CASE("exact_inverse: the same elimination on [M | I]; N Q == I exactly, and the Hilbert matrix's inverse is the integer matrix of the closed form", "[estimation_sizing][behaviour]") {
    CHECK(es::exact_inverse({{q(2), q(1)}, {q(1), q(3)}}) == es::RMatrix{{q(3, 5), q(-1, 5)}, {q(-1, 5), q(2, 5)}});
    CHECK(es::exact_inverse({{q(0), q(1)}, {q(1), q(0)}}) == es::RMatrix{{q(0), q(1)}, {q(1), q(0)}});
    CHECK(es::exact_inverse({{q(4)}}) == es::RMatrix{{q(1, 4)}});
    CHECK(es::exact_inverse(identity(4)) == identity(4));
    CHECK(es::exact_inverse({{q(2), q(0)}, {q(0), q(1, 3)}}) == es::RMatrix{{q(1, 2), q(0)}, {q(0), q(3)}});
    // Hilbert: n = 4 is the well-known matrix
    CHECK(es::exact_inverse(hilbert(4)) == es::RMatrix{{q(16), q(-120), q(240), q(-140)}, {q(-120), q(1200), q(-2700), q(1680)}, {q(240), q(-2700), q(6480), q(-4200)}, {q(-140), q(1680), q(-4200), q(2800)}});
    for (unsigned n = 1; n <= 8; ++n) CHECK(es::exact_inverse(hilbert(n)) == hilbert_inverse(n));
    // random matrices: both products are the identity, and the inverse of the inverse is the matrix
    std::mt19937_64 rng(13);
    for (int trial = 0; trial < 8; ++trial) {
        const std::size_t n = 2 + rng() % 5;
        const es::RMatrix a = random_matrix(rng, n);
        const es::RMatrix inv = es::exact_inverse(a);
        CHECK(times(a, inv) == identity(n));
        CHECK(times(inv, a) == identity(n));
        CHECK(es::exact_inverse(inv) == a);
    }
    CHECK(message_of([] { (void)es::exact_inverse({{q(1), q(2)}, {q(2), q(4)}}); }) == "the matrix is singular");
    CHECK(message_of([] { (void)es::exact_inverse({{q(0)}}); }) == "the matrix is singular");
    CHECK(message_of([] { (void)es::exact_inverse({{q(1), q(0)}, {q(0), q(0)}}); }) == "the matrix is singular");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- Jacobi, 90 digits

namespace {

// pi to 110 digits by Machin's formula, cos by its Taylor series: the SECOND METHOD for the spectrum of tridiagonal(2, -1)
Decimal atan_inverse(int x, std::int64_t digits) {
    LocalContext ctx(digits + 10);
    const Decimal x2 = Decimal(x) * Decimal(x);
    Decimal power = Decimal(1) / Decimal(x);
    Decimal sum = power;
    for (int k = 1; k < 400; ++k) {
        power = power / x2;
        const Decimal term = power / Decimal(2 * k + 1);
        sum = (k % 2 == 1) ? sum - term : sum + term;
        if (term.is_zero() || term.adjusted() < -(digits + 8)) break;
    }
    return sum;
}

Decimal pi_decimal(std::int64_t digits) {
    LocalContext ctx(digits + 10);
    return Decimal(16) * atan_inverse(5, digits) - Decimal(4) * atan_inverse(239, digits);
}

Decimal cos_decimal(const Decimal& a, std::int64_t digits) {
    LocalContext ctx(digits + 10);
    const Decimal a2 = a * a;
    Decimal term(1);
    Decimal sum(1);
    for (int k = 1; k < 400; ++k) {
        term = -(term * a2) / Decimal((2 * k - 1) * (2 * k));
        sum = sum + term;
        if (term.is_zero() || term.adjusted() < -(digits + 8)) break;
    }
    return sum;
}

Decimal dabs(const Decimal& d) { return d.copy_abs(); }

es::DecMatrix decimal_matrix(const std::vector<std::vector<std::int64_t>>& m) {
    es::DecMatrix out;
    for (const auto& row : m) {
        std::vector<Decimal> r;
        for (const std::int64_t v : row) r.emplace_back(v);
        out.push_back(std::move(r));
    }
    return out;
}

// |A v - lambda v|_max
Decimal residual(const es::DecMatrix& a, const std::vector<Decimal>& v, const Decimal& lambda) {
    Decimal worst(0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        Decimal r(0);
        for (std::size_t j = 0; j < a.size(); ++j) r = r + a[i][j] * v[j];
        r = r - lambda * v[i];
        if (dabs(r) > worst) worst = dabs(r);
    }
    return worst;
}

}  // namespace

TEST_CASE("the second method for the spectrum: pi and cos to 110 digits agree with published values", "[estimation_sizing][behaviour]") {
    LocalContext ctx(120);
    // pi's first 50 decimals, and cos(pi/3) = 1/2, cos(pi/2) = 0, cos(pi) = -1
    CHECK(pi_decimal(100).format(".50f") == "3.14159265358979323846264338327950288419716939937511");   // (50 places, rounded: ...9375105 -> ...937511)
    const Decimal tolerance = Decimal::from_string("1e-105");
    CHECK(dabs(cos_decimal(pi_decimal(110) / Decimal(3), 110) - Decimal::from_string("0.5")) < tolerance);
    CHECK(dabs(cos_decimal(pi_decimal(110) / Decimal(2), 110)) < tolerance);
    CHECK(dabs(cos_decimal(pi_decimal(110), 110) + Decimal(1)) < tolerance);
    CHECK(dabs(cos_decimal(Decimal(0), 110) - Decimal(1)) < tolerance);
}

TEST_CASE("jacobi_eigs: the eigenvalues of a symmetric matrix in ascending order, to 90 digits (control K3)", "[estimation_sizing][behaviour]") {
    LocalContext ctx(90);
    const Decimal tolerance = Decimal::from_string("1e-80");
    // [[2,1],[1,2]]: 1 and 3; the eigenvector of 1 is (1, -1)/sqrt(2) up to sign
    {
        const es::JacobiResult r = es::jacobi_eigs(decimal_matrix({{2, 1}, {1, 2}}), true);
        REQUIRE(r.values.size() == 2U);
        CHECK(dabs(r.values[0] - Decimal(1)) < tolerance);
        CHECK(dabs(r.values[1] - Decimal(3)) < tolerance);
        REQUIRE(r.vectors.size() == 2U);
        const Decimal half_sqrt2 = Decimal(2).sqrt() / Decimal(2);
        CHECK(dabs(dabs(r.vectors[0][0]) - half_sqrt2) < tolerance);
        CHECK(dabs(dabs(r.vectors[0][1]) - half_sqrt2) < tolerance);
        CHECK(dabs(r.vectors[0][0] + r.vectors[0][1]) < tolerance);   // (1, -1)
        CHECK(dabs(dabs(r.vectors[1][0]) - half_sqrt2) < tolerance);
        CHECK(dabs(r.vectors[1][0] - r.vectors[1][1]) < tolerance);   // (1, 1)
        // the signs are the ones the rotation gives: th = (2 - 2) / (2 * 1) = 0 is taken as non-negative, so t = +1, c = s = 1/sqrt(2), V = [[c, s], [-s, c]]; its column 0 belongs to the eigenvalue 1
        // (the vector (c, -s)) and its column 1 to 3 (the vector (s, c))
        CHECK(r.vectors[0][0] > Decimal(0));
        CHECK(r.vectors[0][1] < Decimal(0));
        CHECK(r.vectors[1][0] > Decimal(0));
        CHECK(r.vectors[1][1] > Decimal(0));
    }
    // a negative off-diagonal: [[1,-2],[-2,1]] has -1 and 3
    {
        const es::JacobiResult r = es::jacobi_eigs(decimal_matrix({{1, -2}, {-2, 1}}));
        REQUIRE(r.values.size() == 2U);
        CHECK(dabs(r.values[0] + Decimal(1)) < tolerance);
        CHECK(dabs(r.values[1] - Decimal(3)) < tolerance);
        CHECK(r.vectors.empty());
    }
    // an unequal diagonal: [[5,2],[2,1]] has 3 +- sqrt(8)
    {
        const es::JacobiResult r = es::jacobi_eigs(decimal_matrix({{5, 2}, {2, 1}}));
        REQUIRE(r.values.size() == 2U);
        CHECK(dabs(r.values[0] - (Decimal(3) - Decimal(8).sqrt())) < tolerance);
        CHECK(dabs(r.values[1] - (Decimal(3) + Decimal(8).sqrt())) < tolerance);
    }
    // a diagonal matrix is its own answer, sorted: the vectors are the unit vectors, and a tie keeps the order of the indices
    {
        const es::JacobiResult r = es::jacobi_eigs(decimal_matrix({{3, 0, 0}, {0, 1, 0}, {0, 0, 2}}), true);
        CHECK(r.values == std::vector<Decimal>{Decimal(1), Decimal(2), Decimal(3)});
        CHECK(r.vectors == es::DecMatrix{{Decimal(0), Decimal(1), Decimal(0)}, {Decimal(0), Decimal(0), Decimal(1)}, {Decimal(1), Decimal(0), Decimal(0)}});
        const es::JacobiResult t = es::jacobi_eigs(decimal_matrix({{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}), true);
        CHECK(t.vectors == es::DecMatrix{{Decimal(1), Decimal(0), Decimal(0)}, {Decimal(0), Decimal(1), Decimal(0)}, {Decimal(0), Decimal(0), Decimal(1)}});
        const es::JacobiResult one = es::jacobi_eigs(decimal_matrix({{7}}), true);
        CHECK(one.values == std::vector<Decimal>{Decimal(7)});
        CHECK(one.vectors == es::DecMatrix{{Decimal(1)}});
        CHECK(es::jacobi_eigs(es::DecMatrix{}).values.empty());
    }
    // the tridiagonal(2, -1) matrices: lambda_k = 2 - 2 cos(k pi / (n + 1)), k = 1 .. n, ascending
    const Decimal pi = pi_decimal(110);
    for (int n = 2; n <= 7; ++n) {
        std::vector<std::vector<std::int64_t>> m(static_cast<std::size_t>(n), std::vector<std::int64_t>(static_cast<std::size_t>(n), 0));
        for (std::size_t i = 0; i < m.size(); ++i) {
            m[i][i] = 2;
            if (i + 1 < m.size()) m[i][i + 1] = m[i + 1][i] = -1;
        }
        const es::DecMatrix a = decimal_matrix(m);
        const es::JacobiResult r = es::jacobi_eigs(a, true);
        REQUIRE(r.values.size() == static_cast<std::size_t>(n));
        REQUIRE(r.vectors.size() == static_cast<std::size_t>(n));
        Decimal trace(0);
        for (int k = 1; k <= n; ++k) {
            const Decimal want = Decimal(2) - Decimal(2) * cos_decimal(Decimal(k) * pi / Decimal(n + 1), 100);
            const Decimal& got = r.values[static_cast<std::size_t>(k - 1)];
            INFO("n = " << n << ", k = " << k);
            CHECK(dabs(got - want) / want < tolerance);
            CHECK(residual(a, r.vectors[static_cast<std::size_t>(k - 1)], got) < Decimal::from_string("1e-70"));
            trace = trace + got;
        }
        CHECK(dabs(trace - Decimal(2 * n)) < tolerance);   // the sum of the eigenvalues is the trace
    }
    // the Hilbert matrix of order 5: the sum of the eigenvalues is the trace, their product the determinant 1/266716800000, the sum of their squares the sum of the squares of the entries
    {
        es::DecMatrix h(5, std::vector<Decimal>(5));
        Decimal trace(0);
        Decimal frobenius(0);
        for (std::size_t i = 0; i < 5; ++i) {
            for (std::size_t j = 0; j < 5; ++j) {
                h[i][j] = Decimal(1) / Decimal(static_cast<std::int64_t>(i + j + 1));
                frobenius = frobenius + h[i][j] * h[i][j];
            }
            trace = trace + h[i][i];
        }
        const es::JacobiResult r = es::jacobi_eigs(h, true);
        REQUIRE(r.values.size() == 5U);
        REQUIRE(r.vectors.size() == 5U);
        Decimal sum(0);
        Decimal product(1);
        Decimal squares(0);
        for (const Decimal& v : r.values) {
            sum = sum + v;
            product = product * v;
            squares = squares + v * v;
        }
        CHECK(dabs(sum - trace) < tolerance);
        CHECK(dabs(squares - frobenius) < tolerance);
        CHECK(dabs(product - Decimal(1) / Decimal(266716800000LL)) / (Decimal(1) / Decimal(266716800000LL)) < Decimal::from_string("1e-70"));
        for (std::size_t k = 0; k < 5; ++k) {
            INFO("Hilbert 5, eigenpair " << k);
            CHECK(residual(h, r.vectors[k], r.values[k]) < Decimal::from_string("1e-80"));
            // orthonormal
            for (std::size_t l = 0; l <= k; ++l) {
                Decimal dot(0);
                for (std::size_t i = 0; i < 5; ++i) dot = dot + r.vectors[k][i] * r.vectors[l][i];
                CHECK(dabs(dot - Decimal(k == l ? 1 : 0)) < tolerance);
            }
        }
        for (std::size_t k = 1; k < 5; ++k) CHECK(r.values[k - 1] < r.values[k]);
        // the convergence test is RELATIVE to the matrix: the same matrix times 10^-60 and times 10^60 has the same spectrum times the same factor (a threshold with an absolute part would stop the first at once)
        for (const char* factor : {"1e-60", "1e60", "1e-200"}) {
            es::DecMatrix scaled_h = h;
            for (auto& row : scaled_h) {
                for (Decimal& entry : row) entry = entry * Decimal::from_string(factor);
            }
            const es::JacobiResult sr = es::jacobi_eigs(scaled_h);
            REQUIRE(sr.values.size() == 5U);
            for (std::size_t k = 0; k < 5; ++k) CHECK(dabs(sr.values[k] - r.values[k] * Decimal::from_string(factor)) / (r.values[k] * Decimal::from_string(factor)) < Decimal::from_string("1e-70"));
        }
        // without the vectors the eigenvalues are the very same decimals
        CHECK(es::jacobi_eigs(h, false).values == r.values);
    }
    // the matrix is not modified, and the context in force is the caller's again afterwards
    {
        LocalContext narrow(12);
        const es::DecMatrix before = decimal_matrix({{2, 1}, {1, 2}});
        const es::DecMatrix copy = before;
        (void)es::jacobi_eigs(before);
        CHECK(before == copy);
        CHECK(getcontext().prec == 12);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the double-precision recipe

TEST_CASE("normal_double: N_ij = sum over rows ascending of fl(a_ki a_kj) in plain double, the upper triangle copied from the lower", "[estimation_sizing][behaviour]") {
    // by hand: X = [[1,2],[3,4]], y = [5,6]: N = [[1+9, 2+12],[2+12, 4+16]], b = [1*5+3*6, 2*5+4*6]
    {
        const es::NormalEquations ne = es::normal_double({{1.0, 2.0}, {3.0, 4.0}}, {5.0, 6.0});
        CHECK(ne.n == es::DMatrix{{10.0, 14.0}, {14.0, 20.0}});
        CHECK(ne.b == std::vector<double>{23.0, 34.0});
    }
    // three columns, one row: the outer product
    {
        const es::NormalEquations ne = es::normal_double({{1.0, 2.0, 3.0}}, {2.0});
        CHECK(ne.n == es::DMatrix{{1.0, 2.0, 3.0}, {2.0, 4.0, 6.0}, {3.0, 6.0, 9.0}});
        CHECK(ne.b == std::vector<double>{2.0, 4.0, 6.0});
    }
    // the order of the additions is the rows' order, and each is a plain double addition: 1e16 + 1 is lost (a tie to the even 1e16) at each of the four additions, and the whole sum is 2e16
    {
        const es::NormalEquations ne = es::normal_double({{1.0e8}, {1.0}, {1.0}, {1.0}, {1.0}, {1.0e8}}, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        CHECK(ne.n[0][0] == 2.0e16);
        // (an exact sum would be 2e16 + 4, which is a double: the plain sum is 4 short)
        CHECK(ne.n[0][0] != 2.0e16 + 4.0);
    }
    // the same recipe written out for products that round: the sums of 0.1 * 0.3 and 0.7 * 0.7 in row order
    {
        const es::DMatrix x = {{0.1, 0.7}, {0.3, 0.2}, {0.7, 0.9}};
        const std::vector<double> y = {0.5, 0.25, 0.125};
        double n00 = 0.0;
        double n10 = 0.0;
        double n11 = 0.0;
        double b0 = 0.0;
        double b1 = 0.0;
        for (std::size_t k = 0; k < 3; ++k) {
            n00 += x[k][0] * x[k][0];
            n10 += x[k][1] * x[k][0];
            n11 += x[k][1] * x[k][1];
            b0 += x[k][0] * y[k];
            b1 += x[k][1] * y[k];
        }
        const es::NormalEquations ne = es::normal_double(x, y);
        CHECK(bits_of(ne.n[0][0]) == bits_of(n00));
        CHECK(bits_of(ne.n[1][0]) == bits_of(n10));
        CHECK(bits_of(ne.n[0][1]) == bits_of(n10));
        CHECK(bits_of(ne.n[1][1]) == bits_of(n11));
        CHECK(bits_of(ne.b[0]) == bits_of(b0));
        CHECK(bits_of(ne.b[1]) == bits_of(b1));
    }
    // columns beyond the first row's length are not read; a row shorter than the first is an error of the caller (an exception, not a wrong answer)
    CHECK(es::normal_double({{1.0, 2.0}, {3.0, 4.0, 99.0}}, {1.0, 1.0}).n == es::DMatrix{{10.0, 14.0}, {14.0, 20.0}});
    CHECK_THROWS(es::normal_double({{1.0, 2.0}, {3.0}}, {1.0, 1.0}));
    CHECK_THROWS(es::normal_double({}, {}));
}

TEST_CASE("cholesky_double: Demmel's Algorithm 2.1 in plain double; a pivot that is not positive and finite stops it with its index and value", "[estimation_sizing][behaviour]") {
    // the classical example: A = [[25,15,-5],[15,18,0],[-5,0,11]] = L L^T with L = [[5,0,0],[3,3,0],[-1,1,3]]
    {
        const es::CholeskyResult c = es::cholesky_double({{25.0, 15.0, -5.0}, {15.0, 18.0, 0.0}, {-5.0, 0.0, 11.0}});
        REQUIRE(!c.breakdown);
        CHECK(c.l == es::DMatrix{{5.0, 0.0, 0.0}, {3.0, 3.0, 0.0}, {-1.0, 1.0, 3.0}});
    }
    // [[4,2],[2,3]]: L = [[2,0],[1,sqrt(2)]]
    {
        const es::CholeskyResult c = es::cholesky_double({{4.0, 2.0}, {2.0, 3.0}});
        REQUIRE(!c.breakdown);
        REQUIRE(c.l.size() == 2U);
        CHECK(c.l[0][0] == 2.0);
        CHECK(c.l[1][0] == 1.0);
        CHECK(c.l[1][1] == std::sqrt(2.0));
        CHECK(c.l[0][1] == 0.0);
    }
    CHECK(es::cholesky_double({{9.0}}).l == es::DMatrix{{3.0}});
    // the breakdowns: [[1,1],[1,1]] has the pivot 1 - 1 = 0 at index 1; [[1,2],[2,1]] the pivot 1 - 4 = -3
    {
        const es::CholeskyResult c = es::cholesky_double({{1.0, 1.0}, {1.0, 1.0}});
        REQUIRE(c.breakdown);
        CHECK(c.breakdown->index == 1);
        CHECK(c.breakdown->pivot == 0.0);
        CHECK(c.l.empty());
        const es::CholeskyResult d = es::cholesky_double({{1.0, 2.0}, {2.0, 1.0}});
        REQUIRE(d.breakdown);
        CHECK(d.breakdown->index == 1);
        CHECK(d.breakdown->pivot == -3.0);
    }
    // the very first pivot: zero, negative, NaN, infinite
    for (const double first : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        const es::CholeskyResult c = es::cholesky_double({{first}});
        REQUIRE(c.breakdown);
        CHECK(c.breakdown->index == 0);
        if (std::isnan(first)) {
            CHECK(std::isnan(c.breakdown->pivot));
        } else {
            CHECK(c.breakdown->pivot == first);
        }
    }
    // a third-parameter breakdown reports index 2 and the value after the subtractions
    {
        const es::CholeskyResult c = es::cholesky_double({{1.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 1.0}});
        REQUIRE(c.breakdown);
        CHECK(c.breakdown->index == 2);
        CHECK(c.breakdown->pivot == 0.0);
    }
    // the factor reproduces the matrix to rounding on a well-conditioned example (the Hilbert matrix of order 4 in double)
    {
        es::DMatrix h(4, std::vector<double>(4));
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = 0; j < 4; ++j) h[i][j] = 1.0 / static_cast<double>(i + j + 1);
        }
        const es::CholeskyResult c = es::cholesky_double(h);
        REQUIRE(!c.breakdown);
        REQUIRE(c.l.size() == 4U);
        for (std::size_t i = 0; i < 4; ++i) {
            for (std::size_t j = 0; j <= i; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k <= j; ++k) s += c.l[i][k] * c.l[j][k];
                CHECK(std::fabs(s - h[i][j]) < 1e-15);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the compensated sum

TEST_CASE("py_sum, CPython 3.12's compensated sum(): on numbers where the compensated and the plain sums differ it shows what the interpreter had been deciding", "[estimation_sizing][behaviour]") {
    const auto plain = [](const std::vector<double>& v) {
        double s = 0.0;
        for (const double x : v) s += x;
        return s;
    };
    // 1e16, 1, -1e16, 1: the plain sum loses the first 1 (a tie to the even 1e16) and gets 1; the exact sum, and Neumaier's, is 2
    {
        const std::vector<double> v = {1e16, 1.0, -1e16, 1.0};
        CHECK(plain(v) == 1.0);
        CHECK(py_sum(v) == 2.0);
    }
    // 1, 1e100, 1, -1e100: plain 0, exact 2
    {
        const std::vector<double> v = {1.0, 1e100, 1.0, -1e100};
        CHECK(plain(v) == 0.0);
        CHECK(py_sum(v) == 2.0);
    }
    // the squares of a column that is 1e8 and four ones: the plain sum stays at 1e16 (each 1 is a tie to even), the compensated sum is 1e16 + 4, a double
    {
        const std::vector<double> v = {1e16, 1.0, 1.0, 1.0, 1.0};
        CHECK(plain(v) == 1e16);
        CHECK(py_sum(v) == 1e16 + 4.0);
        CHECK(std::sqrt(py_sum(v)) != std::sqrt(plain(v)));
    }
    // where nothing is lost they agree
    {
        const std::vector<double> v = {0.5, 0.25, 0.125, 4.0};
        CHECK(plain(v) == py_sum(v));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- rule U and the equilibrated rows on a synthetic cache

namespace {

// a NIST-layout file for the dataset `name` with the given data rows (y x), whatever the certified block says
std::string synthetic_dat(const std::vector<std::string>& data_rows, const std::vector<std::string>& cert_lines = {"        B0        -0.5     0.25", "        B1         2.0E-01      3E+00"}) {
    std::vector<std::string> l = {"NIST/ITL StRD", "Dataset Name:  Synthetic", "", "File Format:   ASCII",
                                  "               Certified Values  (lines 7 to " + std::to_string(9 + cert_lines.size()) + ")",
                                  "               Data              (lines " + std::to_string(11 + cert_lines.size()) + " to " + std::to_string(10 + cert_lines.size() + data_rows.size()) + ")",
                                  "     Parameter          Estimate             of Estimate"};
    for (const std::string& c : cert_lines) l.push_back(c);
    l.push_back("     Residual");
    l.push_back("     Standard Deviation   0.75");
    l.push_back("Data:       y          x");
    for (const std::string& r : data_rows) l.push_back(r);
    std::string out;
    for (const std::string& s : l) out += s + "\r\n";
    return out;
}

void put_dataset(const fs::path& cache, const std::string& name, const std::string& text) {
    std::string lower = name;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    fs::create_directories(cache / ("nist-strd-lls-" + lower));
    write_text(cache / ("nist-strd-lls-" + lower) / (name + ".dat"), text);
}

// the second method of rule U: the normal matrix and its Cholesky pivots in plain double, written out again
struct PlainRuleU {
    bool broke = false;
    int index = -1;
    std::vector<double> pivots;
};

PlainRuleU plain_rule_u(const es::DMatrix& x) {
    const std::size_t n = x[0].size();
    std::vector<std::vector<double>> a(n, std::vector<double>(n, 0.0));
    for (const auto& row : x) {
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j <= i; ++j) a[i][j] += row[i] * row[j];
        }
    }
    PlainRuleU out;
    std::vector<std::vector<double>> l(n, std::vector<double>(n, 0.0));
    for (std::size_t i = 0; i < n; ++i) {
        double s = a[i][i];
        for (std::size_t k = 0; k < i; ++k) s -= l[i][k] * l[i][k];
        if (!(s > 0.0) || std::isinf(s)) {
            out.broke = true;
            out.index = static_cast<int>(i);
            return out;
        }
        l[i][i] = std::sqrt(s);
        for (std::size_t j = i + 1; j < n; ++j) {
            double t = a[j][i];
            for (std::size_t k = 0; k < i; ++k) t -= l[j][k] * l[i][k];
            l[j][i] = t / l[i][i];
        }
    }
    for (std::size_t i = 0; i < n; ++i) out.pivots.push_back(l[i][i] * l[i][i]);
    return out;
}

}  // namespace

TEST_CASE("predicted_unscaled_rule: min pivot / max pivot of the unscaled Cholesky factor, in double, or the index of the breakdown", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    // Pontius-like rows with x = 1, 2, 3, 4: the design [1, x, x^2] has the integer normal matrix [[4,10,30],[10,30,100],[30,100,354]]
    put_dataset(cache, "Pontius", synthetic_dat({"  2.0  1.0", "  3.0  2.0", "  5.0  3.0", "  4.0  4.0"}));
    const es::RuleU r = es::predicted_unscaled_rule(cache, "Pontius");
    const PlainRuleU plain = plain_rule_u(es::design_double("Pontius", rows_of({{"2", "1"}, {"3", "2"}, {"5", "3"}, {"4", "4"}})).x);
    REQUIRE(!plain.broke);
    REQUIRE(plain.pivots.size() == 3U);
    REQUIRE(r.ratio.has_value());
    REQUIRE(r.pivots.has_value());
    CHECK(!r.breakdown.has_value());
    REQUIRE(r.pivots->size() == 3U);
    for (std::size_t i = 0; i < 3; ++i) CHECK(bits_of((*r.pivots)[i]) == bits_of(plain.pivots[i]));
    CHECK(*r.ratio == *std::min_element(plain.pivots.begin(), plain.pivots.end()) / *std::max_element(plain.pivots.begin(), plain.pivots.end()));
    // by hand: the first pivot is 4 (= 2^2), the second 30 - 25 = 5, the third 354 - 225 - 125 = 4, up to rounding in the square roots
    CHECK(std::fabs((*r.pivots)[0] - 4.0) < 1e-14);
    CHECK(std::fabs((*r.pivots)[1] - 5.0) < 1e-13);
    CHECK(std::fabs((*r.pivots)[2] - 4.0) < 1e-12);
    // a design whose third column duplicates the first (Longley: a one, then the other columns as they are): the rows [1, +-1, 1] give N = [[4,0,4],[0,4,0],[4,0,4]], the pivots 4, 4 and 4 - 4 - 0 = 0, all in exact doubles
    put_dataset(cache, "Longley", synthetic_dat({"  1.0  1.0  1.0", "  2.0  -1.0  1.0", "  3.0  1.0  1.0", "  4.0  -1.0  1.0"}));
    const es::RuleU dup = es::predicted_unscaled_rule(cache, "Longley");
    CHECK(!dup.ratio.has_value());
    CHECK(!dup.pivots.has_value());
    REQUIRE(dup.breakdown.has_value());
    CHECK(*dup.breakdown == 2);
    // a file that does not exist is refused with the OS's words
    CHECK(contains(message_of([&] { (void)es::predicted_unscaled_rule(cache, "Norris"); }), "cannot read"));
}

TEST_CASE("equilibrated_pontius_rows: every design column divided by its own 2-norm, the norm's sum of squares CPython 3.12's compensated sum -- the 3.11 plain sum would give another number", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    // x = 1e8, 1, 1, 1, 1: the column x has the squares 1e16, 1, 1, 1, 1; the compensated sum is 1e16 + 4 (a double), the plain sum 1e16
    put_dataset(cache, "Pontius", synthetic_dat({"  1.0  1e8", "  2.0  1", "  3.0  1", "  4.0  1", "  5.0  1"}));
    const es::EquilibratedRows eq = es::equilibrated_pontius_rows(cache);
    REQUIRE(eq.norms.size() == 3U);
    CHECK(eq.norms[0] == std::sqrt(5.0));
    CHECK(eq.norms[1] == std::sqrt(10000000000000004.0));
    CHECK(eq.norms[1] != 1e8);   // the plain sum would have given exactly 1e8
    const double x2_squared = (es::Ld(1e16) * es::Ld(1e16)).to_double();   // the squares of the column x^2: 1e32 (rounded as the recipe rounds it), 1, 1, 1, 1
    CHECK(eq.norms[2] == std::sqrt(py_sum(std::vector<double>{x2_squared, 1.0, 1.0, 1.0, 1.0})));
    CHECK(eq.y == std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0});
    REQUIRE(eq.x.size() == 5U);
    CHECK(eq.x[0][0] == 1.0 / eq.norms[0]);
    CHECK(eq.x[0][1] == 1e8 / eq.norms[1]);
    CHECK(eq.x[1][1] == 1.0 / eq.norms[1]);
    CHECK(eq.x[0][2] == 1e16 / eq.norms[2]);
    CHECK(eq.x[3][2] == 1.0 / eq.norms[2]);
    // the rule on those rows is the one of the rows
    const es::RuleU r = es::rule_u_of(eq.x, eq.y, false);
    const PlainRuleU plain = plain_rule_u(eq.x);
    if (plain.broke) {
        REQUIRE(r.breakdown.has_value());
        CHECK(*r.breakdown == plain.index);
        CHECK(!r.ratio.has_value());
    } else {
        REQUIRE(r.ratio.has_value());
        CHECK(!r.pivots.has_value());   // the equilibrated record has no pivots
        CHECK(*r.ratio == *std::min_element(plain.pivots.begin(), plain.pivots.end()) / *std::max_element(plain.pivots.begin(), plain.pivots.end()));
    }
    CHECK(es::rule_u_of(eq.x, eq.y, true).pivots.has_value() == !plain.broke);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- analyse on small datasets: a second method

namespace {

struct Obs {
    std::string y;
    std::string x;
};

bool same_double(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) return true;
    return a == b;   // (0.0 == -0.0; infinities equal themselves)
}

Rational rat(const std::string& s) { return es::rational_of(Decimal::from_string(s)); }

// The figures of a dataset with ONE or TWO parameters (NoInt: x; Norris: 1, x) from closed forms: Cramer's rule, the 2x2 inverse, the 2x2 eigenvalues and eigenvector -- in rationals and 90-digit decimals, and the floating
// chain of the bounds written out again from the Python's formulas.  Only for data whose normal matrix is exact in double (small integers), so that the scale exponents and the checksum can be written down.
es::DatasetFigures second_method(const std::string& name, const std::vector<Obs>& obs, bool intercept, const std::vector<std::string>& cert) {
    LocalContext ctx(90);
    es::DatasetFigures f;
    f.name = name;
    const std::size_t n = intercept ? 2 : 1;
    const std::size_t m = obs.size();
    f.n = static_cast<int>(n);
    f.m = static_cast<int>(m);
    std::vector<std::vector<Rational>> X;
    std::vector<Rational> y;
    es::DMatrix xd;
    std::vector<double> yd;
    for (const Obs& o : obs) {
        const Rational xv = rat(o.x);
        X.push_back(intercept ? std::vector<Rational>{q(1), xv} : std::vector<Rational>{xv});
        xd.push_back(intercept ? std::vector<double>{1.0, xv.to_double()} : std::vector<double>{xv.to_double()});
        y.push_back(rat(o.y));
        yd.push_back(y.back().to_double());
    }
    f.checksum = es::checksum(xd, yd);
    es::RMatrix N(n, std::vector<Rational>(n));
    std::vector<Rational> b(n);
    for (std::size_t k = 0; k < m; ++k) {
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) N[i][j] += X[k][i] * X[k][j];
            b[i] += X[k][i] * y[k];
        }
    }
    std::vector<Rational> sol;
    if (n == 1) {
        sol = {b[0] / N[0][0]};
    } else {
        const Rational det = N[0][0] * N[1][1] - N[0][1] * N[0][1];
        sol = {(b[0] * N[1][1] - N[0][1] * b[1]) / det, (N[0][0] * b[1] - N[0][1] * b[0]) / det};
    }
    for (std::size_t i = 0; i < n; ++i) f.e.push_back(es::scale_exponent(N[i][i].to_double()));
    es::RMatrix Ns(n, std::vector<Rational>(n));
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) Ns[i][j] = N[i][j] * pow2r(-(f.e[i] + f.e[j]));
    }
    es::RMatrix Q(n, std::vector<Rational>(n));
    if (n == 1) {
        Q[0][0] = q(1) / Ns[0][0];
    } else {
        const Rational det = Ns[0][0] * Ns[1][1] - Ns[0][1] * Ns[0][1];
        Q = {{Ns[1][1] / det, -Ns[0][1] / det}, {-Ns[0][1] / det, Ns[0][0] / det}};
    }
    Rational q1;
    for (std::size_t j = 0; j < n; ++j) {
        Rational col;
        for (std::size_t i = 0; i < n; ++i) col += Q[i][j].abs();
        if (j == 0 || col > q1) q1 = col;
    }
    f.q1 = es::dec(q1).to_double();
    // the spectra: A = [1] or [[1, rho], [rho, 1]]; N_s = [[a, c], [c, d]]
    std::vector<Decimal> eig_a;
    std::vector<Decimal> eig_ns;
    if (n == 1) {
        eig_a = {Decimal(1)};
        eig_ns = {es::dec(Ns[0][0])};
    } else {
        const Decimal rho = es::dec(N[0][1]) / (es::dec(N[0][0]) * es::dec(N[1][1])).sqrt();
        eig_a = {Decimal(1) - rho.copy_abs(), Decimal(1) + rho.copy_abs()};
        const Decimal a = es::dec(Ns[0][0]);
        const Decimal d = es::dec(Ns[1][1]);
        const Decimal c = es::dec(Ns[0][1]);
        const Decimal root = ((a - d) * (a - d) + Decimal(4) * c * c).sqrt();
        eig_ns = {((a + d) - root) / Decimal(2), ((a + d) + root) / Decimal(2)};
    }
    f.kappa = (eig_ns.back() / eig_ns.front()).to_double();
    f.lam_min_ns = eig_ns.front().to_double();
    f.lam_max_ns = eig_ns.back().to_double();
    f.lam_min_a = eig_a.front().to_double();
    f.lam_max_a = eig_a.back().to_double();
    for (std::size_t i = 0; i < n; ++i) {
        f.kappa_i.push_back(es::dec(Ns[i][i] * Q[i][i]).to_double());
        f.qdiag.push_back(es::dec(Q[i][i] * pow2r(-2 * f.e[i])).to_double());
    }
    Rational dx2;
    for (std::size_t i = 0; i < n; ++i) dx2 += N[i][i] * sol[i] * sol[i];
    f.dx = es::dec(dx2).sqrt().to_double();
    Rational y2;
    Rational omega;
    Rational mag2;
    for (std::size_t k = 0; k < m; ++k) {
        y2 += y[k] * y[k];
        Rational fitted;
        Rational mag;
        for (std::size_t i = 0; i < n; ++i) {
            fitted += X[k][i] * sol[i];
            mag += X[k][i].abs() * sol[i].abs();
        }
        omega += (y[k] - fitted) * (y[k] - fitted);
        mag2 += mag * mag;
    }
    f.r = es::dec(y2).sqrt().to_double();
    const int nu = f.m - f.n;
    f.sigma0_exact = es::dec(omega / q(nu)).sqrt().to_double();
    f.abs_ax_x = es::dec(mag2).sqrt().to_double();
    double digits = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < n; ++i) {
        const Decimal xi = es::dec(sol[i]);
        const Decimal ci = Decimal::from_string(cert[i]);
        digits = std::min(digits, xi != ci ? (-((xi - ci).copy_abs() / ci.copy_abs()).log10()).to_double() : 15.0);
    }
    f.digits_exact_vs_cert = digits;
    f.cert = cert;
    // the weakest direction and the rest of the chain
    if (n == 1) {
        f.weakest_exact = {1.0};
        f.gap = 0.0;
    } else {
        const Decimal a = es::dec(Ns[0][0]);
        const Decimal c = es::dec(Ns[0][1]);
        const Decimal lam = eig_ns.front();
        Decimal v0 = c;
        Decimal v1 = lam - a;
        const Decimal norm = (v0 * v0 + v1 * v1).sqrt();
        v0 = v0 / norm;
        v1 = v1 / norm;
        if (v1.copy_abs() > v0.copy_abs() ? v1 < Decimal(0) : v0 < Decimal(0)) {
            v0 = -v0;
            v1 = -v1;
        }
        f.weakest_exact = {v0.to_double(), v1.to_double()};
        f.gap = (eig_ns[0] / eig_ns[1]).to_double();
        f.rho_exact.push_back((es::dec(Q[0][1]) / (es::dec(Q[0][0]) * es::dec(Q[1][1])).sqrt()).to_double());
    }
    const double inf = std::numeric_limits<double>::infinity();
    const double sqrt_n = std::sqrt(static_cast<double>(n));
    const double tn0 = es::theta_n(f.n, f.m, 0.0);
    const double tnt = es::theta_n(f.n, f.m, es::kEtaRep);
    f.theta_n_code = tn0;
    f.theta_n_test = tnt;
    const double dx = f.dx;
    const double r = f.r;
    f.b_code = dx > 0 ? 4.0 * f.q1 * (tn0 + sqrt_n * es::gamma_k(f.m) * r / dx) : inf;
    f.b_test = dx > 0 ? 4.0 * f.q1 * (tnt + sqrt_n * (es::gamma_k(f.m) + es::kEtaRep) * r / dx) + es::kCertRelative : inf;
    f.certified_no_digit = f.b_code >= 0.5;
    const double ainv = 1.0 / f.lam_min_a;
    f.tau = tnt * ainv;
    f.b_q = f.tau < 1 ? f.tau / (1.0 - f.tau) : inf;
    f.b_rho = 3.0 * f.b_q;
    const double sq_omega = f.sigma0_exact * std::sqrt(static_cast<double>(nu));
    const double g0 = tnt * dx + sqrt_n * (es::gamma_k(f.m) + es::kEtaRep) * r;
    f.delta = std::sqrt(ainv) * g0;
    const double d_sqrt = sq_omega == 0.0 ? f.delta : std::min(f.delta, f.delta * f.delta / (2.0 * sq_omega));
    f.b_sigma = ((es::gamma_k(f.n + 1) + 2 * es::kU + es::kU * es::kU) * (r + f.abs_ax_x) + 0.5 * es::gamma_k(f.m + 1) * sq_omega + d_sqrt) / std::sqrt(static_cast<double>(nu));
    f.b_pair = dx > 0 ? (f.b_test - es::kCertRelative) + 4.0 * f.q1 * (es::theta_n(f.n, f.m, 2 * es::kEtaRep) + sqrt_n * (es::gamma_k(f.m) + 2 * es::kEtaRep) * r / dx) : inf;
    f.tau_s = 2.0 * tn0 / f.lam_min_ns;
    f.cos_min = 0.0;
    if (n > 1 && f.tau_s / (1.0 - f.gap) < 1.0) {
        const double z = f.tau_s / (1.0 - f.gap);
        const double y1 = 1.0 - py_pow(z, 2.0);   // float ** 2 is the C library's pow, as in Python
        f.cos_min = std::sqrt(y1 > 0.0 ? y1 : 0.0);
    }
    return f;
}

void check_figures(const es::DatasetFigures& got, const es::DatasetFigures& want) {
    const auto num = [](const char* field, double g, double w) {
        INFO("field " << field << ": got " << g << ", want " << w);
        CHECK(same_double(g, w));
    };
    const auto vec = [&](const char* field, const std::vector<double>& g, const std::vector<double>& w) {
        INFO("field " << field);
        REQUIRE(g.size() == w.size());
        for (std::size_t i = 0; i < g.size(); ++i) num(field, g[i], w[i]);
    };
    CHECK(got.name == want.name);
    CHECK(got.n == want.n);
    CHECK(got.m == want.m);
    CHECK(got.e == want.e);
    CHECK(got.checksum == want.checksum);
    CHECK(got.cert == want.cert);
    num("kappa", got.kappa, want.kappa);
    num("lam_min_ns", got.lam_min_ns, want.lam_min_ns);
    num("lam_max_ns", got.lam_max_ns, want.lam_max_ns);
    num("lam_min_a", got.lam_min_a, want.lam_min_a);
    num("lam_max_a", got.lam_max_a, want.lam_max_a);
    num("q1", got.q1, want.q1);
    vec("kappa_i", got.kappa_i, want.kappa_i);
    vec("qdiag", got.qdiag, want.qdiag);
    num("dx", got.dx, want.dx);
    num("r", got.r, want.r);
    num("digits_exact_vs_cert", got.digits_exact_vs_cert, want.digits_exact_vs_cert);
    num("sigma0_exact", got.sigma0_exact, want.sigma0_exact);
    num("abs_ax_x", got.abs_ax_x, want.abs_ax_x);
    num("theta_n_code", got.theta_n_code, want.theta_n_code);
    num("theta_n_test", got.theta_n_test, want.theta_n_test);
    num("b_code", got.b_code, want.b_code);
    num("b_test", got.b_test, want.b_test);
    CHECK(got.certified_no_digit == want.certified_no_digit);
    num("tau", got.tau, want.tau);
    num("b_q", got.b_q, want.b_q);
    num("b_rho", got.b_rho, want.b_rho);
    num("delta", got.delta, want.delta);
    num("b_sigma", got.b_sigma, want.b_sigma);
    num("b_pair", got.b_pair, want.b_pair);
    vec("weakest_exact", got.weakest_exact, want.weakest_exact);
    num("gap", got.gap, want.gap);
    num("tau_s", got.tau_s, want.tau_s);
    num("cos_min", got.cos_min, want.cos_min);
    vec("rho_exact", got.rho_exact, want.rho_exact);
    CHECK(!got.breakdown_index.has_value());
}

std::vector<std::string> default_cert() { return {"-0.5", "2.0E-01"}; }

std::string rows_text(const std::vector<Obs>& obs) {
    std::vector<std::string> rows;
    for (const Obs& o : obs) rows.push_back("  " + o.y + "  " + o.x);
    return synthetic_dat(rows);
}

}  // namespace

TEST_CASE("analyse: a one-parameter dataset (NoInt-like) agrees with the closed forms", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    // x = 1, 2, 3 and y = 2, 4, 7: N = 14, b = 31, the solution 31/14, Omega = 69 - 961/14 = 5/14, nu = 2; N has the frexp (0.875, 4), so e = 2 and N_s = 14/16 = 7/8
    const std::vector<Obs> obs = {{"2", "1"}, {"4", "2"}, {"7", "3"}};
    put_dataset(cache, "NoInt1", rows_text(obs));
    const es::DatasetFigures got = es::analyse(cache, "NoInt1");
    const es::DatasetFigures want = second_method("NoInt1", obs, false, default_cert());
    check_figures(got, want);
    // the hand-derived values the second method must agree with, so that a mistake in it is not shared
    CHECK(got.e == std::vector<int>{2});
    CHECK(got.kappa == 1.0);
    CHECK(got.lam_min_a == 1.0);
    CHECK(got.lam_max_a == 1.0);
    CHECK(got.lam_min_ns == 0.875);
    CHECK(got.lam_max_ns == 0.875);
    CHECK(got.q1 == 8.0 / 7.0);
    CHECK(got.kappa_i == std::vector<double>{1.0});
    CHECK(got.qdiag == std::vector<double>{1.0 / 14.0});
    CHECK(got.gap == 0.0);
    CHECK(got.cos_min == 0.0);
    CHECK(got.weakest_exact == std::vector<double>{1.0});
    CHECK(got.rho_exact.empty());
    CHECK(got.r == std::sqrt(69.0));
    CHECK(std::fabs(got.dx - 31.0 / std::sqrt(14.0)) < 4e-16 * got.dx);
    CHECK(std::fabs(got.abs_ax_x - (31.0 / 14.0) * std::sqrt(14.0)) < 4e-16 * got.abs_ax_x);
    CHECK(std::fabs(got.sigma0_exact - std::sqrt(5.0 / 28.0)) < 1e-16);
    CHECK(!got.breakdown_index.has_value());
    CHECK(!got.breakdown_pivot.has_value());
    CHECK(got.rsd == "0.75");
    CHECK(got.sd == Strings{"0.25", "3E+00"});
    CHECK(got.certified_no_digit == false);
    CHECK(got.b_code < 1e-13);
}

TEST_CASE("analyse: a two-parameter dataset (Norris-like) agrees with the closed forms", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    // x = 1..4, y = 1, 3, 2, 5: N = [[4,10],[10,30]], b = (11, 33), the solution (0, 11/10); e = (1, 2) and N_s = [[1, 5/4], [5/4, 15/8]]
    const std::vector<Obs> obs = {{"1", "1"}, {"3", "2"}, {"2", "3"}, {"5", "4"}};
    put_dataset(cache, "Norris", rows_text(obs));
    const es::DatasetFigures got = es::analyse(cache, "Norris");
    check_figures(got, second_method("Norris", obs, true, default_cert()));
    CHECK(got.e == std::vector<int>{1, 2});
    CHECK(got.n == 2);
    CHECK(got.m == 4);
    CHECK(got.kappa_i.size() == 2U);
    REQUIRE(got.weakest_exact.size() == 2U);
    CHECK(got.rho_exact.size() == 1U);
    CHECK(std::fabs(got.sigma0_exact - std::sqrt(2.7 / 2.0)) < 4e-16 * got.sigma0_exact);   // Omega = 2.7, nu = 2
    // the exact solution (0, 11/10) is printed as the digits against the certified (-0.5, 0.2): at B1 the relative distance is 4.5, so -log10(4.5)
    CHECK(std::fabs(got.digits_exact_vs_cert + std::log10(4.5)) < 1e-14);
    // the weakest direction of N_s has its larger component positive
    const double big = std::fabs(got.weakest_exact[0]) >= std::fabs(got.weakest_exact[1]) ? got.weakest_exact[0] : got.weakest_exact[1];
    CHECK(big > 0.0);
    CHECK(std::fabs(got.weakest_exact[0] * got.weakest_exact[0] + got.weakest_exact[1] * got.weakest_exact[1] - 1.0) < 1e-15);
}

TEST_CASE("analyse: an exact fit (Omega = 0) takes the delta branch of B_sigma; a zero response makes the bounds that divide by ||D x|| infinite", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    {   // y = 2 x exactly: the solution is 2, Omega = 0, so sq_omega = 0.0 and d_sqrt is delta itself
        const std::vector<Obs> obs = {{"2", "1"}, {"4", "2"}, {"6", "3"}};
        put_dataset(cache, "NoInt1", rows_text(obs));
        const es::DatasetFigures got = es::analyse(cache, "NoInt1");
        check_figures(got, second_method("NoInt1", obs, false, default_cert()));
        CHECK(got.sigma0_exact == 0.0);
        CHECK(got.delta > 0.0);
        CHECK(got.b_sigma > 0.0);
        CHECK(got.dx == 2.0 * std::sqrt(14.0));
        CHECK(got.digits_exact_vs_cert < 0.0);   // |2 - (-0.5)| / 0.5 = 5
        CHECK(std::fabs(got.digits_exact_vs_cert + std::log10(5.0)) < 1e-14);
    }
    {   // y = 0: the solution is zero, ||D x|| = 0
        const std::vector<Obs> obs = {{"0", "1"}, {"0", "2"}, {"0", "3"}};
        put_dataset(cache, "NoInt2", rows_text(obs));
        const es::DatasetFigures got = es::analyse(cache, "NoInt2");
        check_figures(got, second_method("NoInt2", obs, false, default_cert()));
        CHECK(got.dx == 0.0);
        CHECK(got.r == 0.0);
        CHECK(std::isinf(got.b_code));
        CHECK(std::isinf(got.b_test));
        CHECK(std::isinf(got.b_pair));
        CHECK(got.certified_no_digit);
        CHECK(got.b_sigma == 0.0);
        CHECK(got.sigma0_exact == 0.0);
        CHECK(got.abs_ax_x == 0.0);
        CHECK(got.digits_exact_vs_cert == 0.0);   // |0 - (-0.5)| / 0.5 = 1, log10 = 0
    }
    {   // a solution equal to the certified value: 15 digits
        TempDir td2("odl-est");
        // y = -0.5 x exactly (the first certified value, -0.5)
        const std::vector<Obs> obs = {{"-0.5", "1"}, {"-1", "2"}, {"-1.5", "3"}};
        put_dataset(td2.path(), "NoInt1", rows_text(obs));
        const es::DatasetFigures got = es::analyse(td2.path(), "NoInt1");
        CHECK(got.digits_exact_vs_cert == 15.0);
        check_figures(got, second_method("NoInt1", obs, false, default_cert()));
    }
}

TEST_CASE("analyse: a Cholesky breakdown of the scaled matrix is reported with its index and pivot, exactly as a plain double Cholesky written out here finds it; the exact figures are still computed", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    // x = offset + k for k = 1 .. 4 with the offset so large that (1, x) agree to the last bits of a double: whether the scaled matrix's second pivot comes out positive (garbage), zero or negative depends on the
    // rounding, so the second method here decides it for each offset -- the same normal matrix, the same power-of-two scaling and the same Cholesky, written again -- and the tool must agree in every case
    std::size_t broke = 0;
    std::size_t completed = 0;
    for (const std::int64_t offset : {100000000LL, 300000000LL, 1000000000LL, 3000000000LL, 10000000000LL, 100000000000LL, 1000000000000LL, 100000000000000LL}) {
        const std::vector<std::int64_t> xs = {offset + 1, offset + 2, offset + 3, offset + 4};
        const std::vector<std::string> ys = {"1.0", "2.0", "4.0", "3.0"};
        std::vector<std::string> rows;
        std::vector<std::vector<double>> design;
        for (std::size_t k = 0; k < 4; ++k) {
            rows.push_back("  " + ys[k] + "  " + std::to_string(xs[k]));
            design.push_back({1.0, static_cast<double>(xs[k])});
        }
        put_dataset(cache, "Norris", synthetic_dat(rows));
        // the second method: N in plain double, e by frexp, the scaled matrix by ldexp, the Cholesky of a 2x2
        double n00 = 0.0;
        double n10 = 0.0;
        double n11 = 0.0;
        for (const auto& a : design) {
            n00 += a[0] * a[0];
            n10 += a[1] * a[0];
            n11 += a[1] * a[1];
        }
        int k0 = 0;
        int k1 = 0;
        std::frexp(n00, &k0);
        std::frexp(n11, &k1);
        const int e0 = k0 >= 0 ? k0 / 2 : -((-k0 + 1) / 2);
        const int e1 = k1 >= 0 ? k1 / 2 : -((-k1 + 1) / 2);
        const double s00 = std::ldexp(n00, -(e0 + e0));
        const double s10 = std::ldexp(n10, -(e0 + e1));
        const double s11 = std::ldexp(n11, -(e1 + e1));
        const double l00 = std::sqrt(s00);
        const double l10 = s10 / l00;
        const double pivot = s11 - l10 * l10;
        const bool breakdown = !(pivot > 0.0);
        INFO("offset " << offset << ": plain pivot " << pivot);
        const es::DatasetFigures got = es::analyse(cache, "Norris");
        CHECK(got.e == std::vector<int>{e0, e1});
        if (breakdown) {
            ++broke;
            REQUIRE(got.breakdown_index.has_value());
            REQUIRE(got.breakdown_pivot.has_value());
            CHECK(*got.breakdown_index == 1);
            CHECK(bits_of(*got.breakdown_pivot) == bits_of(pivot));
        } else {
            ++completed;
            CHECK(!got.breakdown_index.has_value());
            CHECK(!got.breakdown_pivot.has_value());
        }
        // whatever the double Cholesky did, the exact quantities say the columns are almost dependent
        CHECK(got.kappa > 1e10);
        CHECK(got.lam_min_a < 1e-8);
        CHECK(got.n == 2);
        CHECK(got.certified_no_digit);   // B(code) is huge
    }
    // both branches were reached with these offsets
    CHECK(broke >= 1U);
    CHECK(completed >= 1U);
}

TEST_CASE("analyse: a solution of size below 1 still has finite bounds; a weakest direction whose two components are equal in size keeps the sign of the FIRST", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    {   // y = 0.125 x exactly: ||D x|| = sqrt(14) / 8 = 0.47, between 0 and 1 -- the bounds that divide by it are finite
        const std::vector<Obs> obs = {{"0.125", "1"}, {"0.25", "2"}, {"0.375", "3"}};
        put_dataset(cache, "NoInt1", rows_text(obs));
        const es::DatasetFigures got = es::analyse(cache, "NoInt1");
        check_figures(got, second_method("NoInt1", obs, false, default_cert()));
        CHECK(got.dx > 0.0);
        CHECK(got.dx < 1.0);
        CHECK(std::isfinite(got.b_code));
        CHECK(std::isfinite(got.b_test));
        CHECK(std::isfinite(got.b_pair));
        CHECK(got.b_code > 0.0);
    }
    {   // x = (1, 1, 1, -1): N = [[4, 2], [2, 4]], N_s = [[1, 1/2], [1/2, 1]] (e = (1, 1)), the eigenvalues 1/2 and 3/2, the weakest direction (1, -1)/sqrt(2): two components of the SAME size, and the first is positive
        const std::vector<Obs> obs = {{"1", "1"}, {"2", "1"}, {"4", "1"}, {"3", "-1"}};
        put_dataset(cache, "Norris", rows_text(obs));
        const es::DatasetFigures got = es::analyse(cache, "Norris");
        check_figures(got, second_method("Norris", obs, true, default_cert()));
        CHECK(got.e == std::vector<int>{1, 1});
        CHECK(got.lam_min_ns == 0.5);
        CHECK(got.lam_max_ns == 1.5);
        REQUIRE(got.weakest_exact.size() == 2U);
        CHECK(got.weakest_exact[0] > 0.0);
        CHECK(got.weakest_exact[1] < 0.0);
        CHECK(got.weakest_exact[0] == -got.weakest_exact[1]);
        CHECK(std::fabs(got.weakest_exact[0] - std::sqrt(0.5)) < 2e-16);
        CHECK(std::fabs(got.gap - 1.0 / 3.0) < 1e-16);
    }
}

TEST_CASE("analyse: the flag B(code) >= 1/2 and the angle bound of the weakest direction, at offsets that cross their thresholds", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    // x = offset + s k, k = 1 .. 4, with the spacing s = 1 and s = 3: B and the ratio tau_s / (1 - gap) grow as the square of the offset (and fall as the square of s), so a sweep of offsets 0.15 per cent apart crosses every threshold of the code and meets every window between
    // two of them (a window of 5 per cent in the ratio, of a factor 3 in B); what each value must be is written here from the Python's formulas, from the values the analysis itself reports
    std::size_t in_b_window = 0;
    std::size_t in_angle_window = 0;
    std::size_t angle_defined = 0;
    std::size_t angle_zero = 0;
    for (const std::int64_t spacing : {1, 3}) {
        double offset = 1.0e6;
        for (int step = 0; step < 3200; ++step, offset *= 1.0015) {
            const auto base = static_cast<std::int64_t>(offset);
            std::vector<std::string> rows;
            for (std::int64_t k = 1; k <= 4; ++k) rows.push_back("  " + std::to_string(k) + ".0  " + std::to_string(base + spacing * k));
            put_dataset(cache, "Norris", synthetic_dat(rows));
            const es::DatasetFigures got = es::analyse(cache, "Norris");
            INFO("offset " << base << ": B(code) " << got.b_code << ", tau_s " << got.tau_s << ", gap " << got.gap);
            CHECK(got.certified_no_digit == (got.b_code >= 0.5));
            if (got.b_code >= 0.5 && got.b_code < 1.5) ++in_b_window;
            const double z = got.tau_s / (1.0 - got.gap);
            if (z < 1.0) {
                const double y = 1.0 - py_pow(z, 2.0);
                ++angle_defined;
                CHECK(got.cos_min == std::sqrt(y));
                if (y > 0.0 && y <= 0.1) ++in_angle_window;
            } else {
                ++angle_zero;
                CHECK(got.cos_min == 0.0);
            }
        }
    }   // (the spacings: x = offset + k reaches the window of B(code), x = offset + 3 k that of the angle bound, between the jumps of the scale exponents)
    CHECK(in_b_window >= 1U);        // B(code) between 1/2 and 3/2 happened
    CHECK(in_angle_window >= 1U);    // 0 < 1 - z^2 <= 1/10 happened
    CHECK(angle_defined >= 1U);
    CHECK(angle_zero >= 1U);
}

TEST_CASE("analyse: what it refuses -- no rows, rows shorter than the first, fewer certified values than parameters, a certified value that is not a number, a singular matrix, as many rows as parameters", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    put_dataset(cache, "Norris", synthetic_dat({}));
    CHECK(contains(message_of([&] { (void)es::analyse(cache, "Norris"); }), "Norris: the file has no data rows"));
    put_dataset(cache, "Longley", synthetic_dat({"  1.0  2.0  3.0", "  1.0  2.0"}));
    CHECK(contains(message_of([&] { (void)es::analyse(cache, "Longley"); }), "Longley: a data row has fewer columns than the first"));
    put_dataset(cache, "Longley", synthetic_dat({"  1.0  2.0  3.0", "  2.0  1.0  5.0", "  4.0  3.0  2.0", "  3.0  2.0  2.0"}, {"        B0        -0.5     0.25", "        B1         2.0E-01      3E+00"}));
    CHECK(contains(message_of([&] { (void)es::analyse(cache, "Longley"); }), "the file certifies 2 values, 3 are wanted"));
    put_dataset(cache, "NoInt1", synthetic_dat({"  2.0  1.0", "  4.0  2.0", "  7.0  3.0"}, {"        B0        abc     0.25", "        B1         2.0E-01      3E+00"}));
    CHECK(contains(message_of([&] { (void)es::analyse(cache, "NoInt1"); }), "'abc' is not a decimal number"));
    // two equal columns: the exact normal matrix is singular
    put_dataset(cache, "Longley", synthetic_dat({"  1.0  1.0  1.0", "  2.0  2.0  2.0", "  4.0  3.0  3.0", "  3.0  5.0  5.0"}, {"  B0 1 2", "  B1 1 2", "  B2 1 2"}));
    CHECK(contains(message_of([&] { (void)es::analyse(cache, "Longley"); }), "the matrix is singular"));
    // as many rows as parameters: nu = 0, and the Python's ZeroDivisionError is a division by zero here too
    put_dataset(cache, "Norris", synthetic_dat({"  1.0  1.0", "  3.0  2.0"}));
    CHECK_THROWS_AS(es::analyse(cache, "Norris"), std::domain_error);
    // the file is not there
    CHECK(contains(message_of([&] { (void)es::analyse(cache, "Pontius"); }), "cannot read"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- Demmel, the synthetic problems, the tridiagonal ones

TEST_CASE("demmel: the worked example of LAPACK Working Note 14 -- the solution solves H x = b, p is a unit vector, the published numbers are carried", "[estimation_sizing][behaviour]") {
    const es::DemmelFigures d = es::demmel();
    // H = D A D with the published A (decimals), D = diag(1, 1e5, 1e-10, 1e15), b = (42, -26, 24, 34): the exact residual of the solution in double is within rounding
    const std::int64_t a_num[4][4] = {{100, -11, 24, -34}, {-11, 100, 7, 30}, {24, 7, 100, 65}, {-34, 30, 65, 100}};
    const Rational scale[4] = {q(1), q(100000), q(1, 10000000000), Rational(BigInt::pow(BigInt(10), 15))};
    const std::int64_t b[4] = {42, -26, 24, 34};
    REQUIRE(d.x.size() == 4U);
    for (std::size_t i = 0; i < 4; ++i) {
        Rational row;
        Rational magnitude;
        for (std::size_t j = 0; j < 4; ++j) {
            const Rational h = scale[i] * q(a_num[i][j], 100) * scale[j];
            row += h * es::exact_rational(d.x[j]);
            magnitude += (h * es::exact_rational(d.x[j])).abs();
        }
        const Rational residual = (row - q(b[i])).abs();
        INFO("row " << i);
        CHECK(residual.to_double() <= 1e-14 * magnitude.to_double());
    }
    // p = D x / |D x|: a unit vector
    REQUIRE(d.p.size() == 4U);
    double norm2 = 0.0;
    for (const double v : d.p) norm2 += v * v;
    CHECK(std::fabs(norm2 - 1.0) < 1e-15);
    // the scale exponents of the double matrix H_ii = D_i^2 (diagonal 1): 1, 1e10, 1e-20, 1e30 -> frexp exponents 1, 34, -66, 100 -> e = 0, 17, -33, 50
    CHECK(d.e == std::vector<int>{0, 17, -33, 50});
    CHECK(d.published_bound == 1.5e-14);
    CHECK(d.published_vector == std::vector<double>{-0.3641849059026662, 0.09299067506030982, 0.7002799484168281, -0.6069020369959377});
    // the page's own statements: kappa_2(H) = 3.11e50 (the page: about 1e50); the exact 2-norm condition number of the printed A is 12.8476, lambda_max 1.7671 (the page's "2.0" is not it)
    CHECK(std::fabs(d.kappa_a - 12.8476) < 5e-5);
    CHECK(std::fabs(d.lam_max_a - 1.7671) < 5e-5);
    CHECK(std::fabs(d.kappa_h / 3.11e50 - 1.0) < 2e-3);   // (PROVENANCE 40.8: 3.11e50, and the page's own "about 1e50")
    CHECK(d.lam_min_a < d.lam_max_a);
    CHECK(std::fabs(d.lam_max_a / d.lam_min_a - d.kappa_a) < 1e-12 * d.kappa_a);
    // the bound: Theta_N = 4 eta_H + Theta_s(4) with eta_H = 1.002 u; B = 4 q1 Theta_N and the other one divides Theta_N by lambda_min(A)
    CHECK(d.theta_n == 4.0 * (1.002 * es::kU) + es::theta_solve(4));
    CHECK(d.b == 4.0 * d.q1 * d.theta_n);
    CHECK(d.b_exact_a == d.theta_n / d.lam_min_a);
}

TEST_CASE("syn_dependent, syn_nearly_dependent and tridiagonal: closed forms", "[estimation_sizing][behaviour]") {
    {   // columns a = (1,1,1,1), b = (1,1,-1,-1), c = a: N = [[4,0,4],[0,4,0],[4,0,4]]; frexp(4) = (0.5, 3), e = 1 each, N_s = N/4; the pivots 1, 1, 1 - 1 - 0 = 0
        const es::SynDependent s = es::syn_dependent();
        CHECK(s.e == std::vector<int>{1, 1, 1});
        CHECK(s.n == es::DMatrix{{4.0, 0.0, 4.0}, {0.0, 4.0, 0.0}, {4.0, 0.0, 4.0}});
        CHECK(s.breakdown_index == 2);
        CHECK(s.breakdown_pivot == 0.0);
        CHECK(s.dependency == std::vector<double>{1.0, 0.0});
    }
    {   // columns (1,1,1,1) and (1+h, 1-h, 1, 1), h = 2^-23: N = [[4,4],[4,4+2^-45]], b = (1, 1+h); e = (1, 1); N_s = [[1,1],[1,1+d]] with d = 2^-47
        LocalContext ctx(90);
        const es::SynNearlyDependent s = es::syn_nearly_dependent();
        CHECK(s.e == std::vector<int>{1, 1});
        const Decimal d = Decimal::from_string("1") / Decimal(BigInt(1).shifted_left(47));
        const Decimal root = (Decimal(4) + d * d).sqrt();
        const Decimal lam_min = ((Decimal(2) + d) - root) / Decimal(2);
        const Decimal lam_max = ((Decimal(2) + d) + root) / Decimal(2);
        CHECK(s.lam_min_ns == lam_min.to_double());
        CHECK(s.kappa == (lam_max / lam_min).to_double());
        CHECK(s.q1 == static_cast<double>((std::uint64_t{1} << 48) + 1));   // (2 + d) / d = 2^48 + 1
        CHECK(s.theta_n == es::theta_n(2, 4, 0.0));
        // B = 4 q1 (Theta_N + sqrt(2) gamma_4 r / ||D x||) with r = 1 and the exact solution x = (-4194303.75, 4194304) of N x = b
        Rational dx2 = q(4) * (Rational(BigInt(-4194303)) - q(3, 4)) * (Rational(BigInt(-4194303)) - q(3, 4)) * q(1);
        dx2 = dx2 + (q(4) + pow2r(-45)) * q(4194304) * q(4194304);
        const double dx = es::dec(dx2).sqrt().to_double();
        CHECK(s.b == 4.0 * static_cast<double>((std::uint64_t{1} << 48) + 1) * (es::theta_n(2, 4, 0.0) + std::sqrt(2.0) * es::gamma_k(4) * 1.0 / dx));
        CHECK(s.b > 0.5);   // EST-F-203
    }
    for (const int n : es::kTridiagonalSizes) {
        const es::Tridiagonal t = es::tridiagonal(n);
        INFO("n = " << n);
        CHECK(t.n == n);
        CHECK(t.m == n + 1);
        CHECK(t.theta_s == es::theta_solve(n));
        CHECK(t.lam_min_a == (2.0 - 2.0 * std::cos(3.141592653589793 / (n + 1))) / 2.0);
        CHECK(t.b_col == es::theta_solve(n) / t.lam_min_a);
        // lambda_min(A) = 1 - cos(pi / (n + 1)) to rounding
        LocalContext ctx(100);
        const Decimal want = Decimal(1) - cos_decimal(pi_decimal(100) / Decimal(n + 1), 90);
        CHECK(std::fabs(t.lam_min_a - want.to_double()) <= 3e-16);
    }
    CHECK(es::tridiagonal(1).lam_min_a == 1.0 - 0x1p-53);   // cos(pi/2) = 6.1e-17 in double: 2 - 2 * 6.1e-17 rounds to 2 - 2^-52, and half of it is 1 - 2^-53
    CHECK(std::size(es::kTridiagonalSizes) == 6U);
}

TEST_CASE("hatch_filip: refuses a file with fewer than eleven certified values, naming the number", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    const fs::path cache = td.path();
    put_dataset(cache, "Filip", synthetic_dat({"  1.0  0.5", "  2.0  0.25", "  3.0  0.75"}));
    CHECK(contains(message_of([&] { (void)es::hatch_filip(cache); }), "Filip: the file certifies 2 values, eleven are wanted"));
    CHECK(contains(message_of([&] { (void)es::hatch_filip(td.path() / "nowhere"); }), "cannot read"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- hexf and sci

TEST_CASE("hexf: float.hex(), with the header's two spellings of an infinity and '0.0' for none", "[estimation_sizing][behaviour]") {
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(es::hexf(1.0) == "0x1.0000000000000p+0");
    CHECK(es::hexf(0.0) == "0x0.0p+0");
    CHECK(es::hexf(-0.0) == "-0x0.0p+0");
    CHECK(es::hexf(0.5) == "0x1.0000000000000p-1");
    CHECK(es::hexf(-1.5) == "-0x1.8000000000000p+0");
    CHECK(es::hexf(0x1.fffffffffffffp+1023) == "0x1.fffffffffffffp+1023");
    CHECK(es::hexf(std::numeric_limits<double>::denorm_min()) == "0x0.0000000000001p-1022");
    CHECK(es::hexf(inf) == "HUGE_VAL");
    CHECK(es::hexf(-inf) == "-HUGE_VAL");
    CHECK(es::hexf(std::optional<double>{}) == "0.0");
    CHECK(es::hexf(std::optional<double>{0.5}) == "0x1.0000000000000p-1");
    CHECK(es::hexf(std::optional<double>{inf}) == "HUGE_VAL");
    CHECK(es::hexf(std::optional<double>{0.0}) == "0x0.0p+0");
}

TEST_CASE("sci: three significant digits as the specification's table writes them -- '1.23 × 10⁻⁵', no power for exponent 0, '0' for zero or none, '∞' for an infinity", "[estimation_sizing][behaviour]") {
    CHECK(es::sci(0.0) == "0");
    CHECK(es::sci(-0.0) == "0");
    CHECK(es::sci(std::numeric_limits<double>::infinity()) == "∞");
    CHECK(es::sci(-std::numeric_limits<double>::infinity()) == "∞");
    CHECK(es::sci(1.0) == "1.00");
    CHECK(es::sci(123456.0) == "1.23 × 10⁵");
    CHECK(es::sci(0.000123) == "1.23 × 10⁻⁴");
    CHECK(es::sci(-5.0e-12) == "-5.00 × 10⁻¹²");
    CHECK(es::sci(9.996) == "1.00 × 10¹");        // %.2e rounds up into the next power of ten
    CHECK(es::sci(0.99999) == "1.00");             // ... and %.2e of this is 1.00e+00: exponent 0
    CHECK(es::sci(1e100) == "1.00 × 10¹⁰⁰");
    CHECK(es::sci(1e-100) == "1.00 × 10⁻¹⁰⁰");
    CHECK(es::sci(std::numeric_limits<double>::denorm_min()) == "4.94 × 10⁻³²⁴");
    CHECK(es::sci(2.5) == "2.50");
    CHECK(es::sci(1.5e5, 2) == "1.5 × 10⁵");
    CHECK(es::sci(1.5e5, 1) == "2 × 10⁵");          // %.0e: a tie to the even digit
    CHECK(es::sci(2.0 / 3.0, 4) == "6.667 × 10⁻¹");
    CHECK(es::sci(3.0 * std::ldexp(1.0, -52), 3) == "6.66 × 10⁻¹⁶");
    CHECK(es::sci(std::optional<double>{}) == "0");
    CHECK(es::sci(std::optional<double>{2.5}) == "2.50");
    CHECK(es::sci(std::optional<double>{-0.5}, 2) == "-5.0 × 10⁻¹");
    CHECK_THROWS_AS(es::sci(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    CHECK(message_of([] { (void)es::sci(std::numeric_limits<double>::quiet_NaN()); }) == "sci: not a number");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the header, the table and the report on a made-up report

namespace {

es::DatasetFigures alpha() {
    es::DatasetFigures d;
    d.name = "Alpha";
    d.n = 2;
    d.m = 3;
    d.e = {1, -2};
    d.checksum = 12345;
    d.kappa = 100.0;
    d.lam_min_a = 0.5;
    d.lam_max_a = 2.0;
    d.q1 = 8.0;
    d.dx = 0.25;
    d.r = 0.75;
    d.theta_n_code = 0x1p-50;
    d.theta_n_test = 0x1p-49;
    d.b_code = 0.125;
    d.b_test = 0.25;
    d.b_q = 0.0625;
    d.b_rho = 0.1875;
    d.b_sigma = 4.0;
    d.sigma0_exact = 0.75;
    d.b_pair = 1.5;
    d.gap = 0.5;
    d.tau_s = 0.25;
    d.cos_min = 0.875;
    d.kappa_i = {1.0, 2.5};
    d.qdiag = {0.5, 0.25};
    d.weakest_exact = {0.5, -0.5};
    d.rho_exact = {0.5};
    d.digits_exact_vs_cert = 12.34;
    return d;
}

es::DatasetFigures beta() {
    es::DatasetFigures d = alpha();
    d.name = "Beta";
    d.n = 1;
    d.m = 2;
    d.e = {5};
    d.checksum = 18446744073709551615ULL;
    d.kappa = 1.0;
    d.q1 = 0.5;
    d.theta_n_code = 2.0e-16;
    d.b_code = d.b_test = d.b_q = std::numeric_limits<double>::infinity();
    d.kappa_i = {3.0};
    d.qdiag = {2.0};
    d.weakest_exact = {1.0};
    d.rho_exact = {};
    d.breakdown_index = 1;
    d.breakdown_pivot = -0.5;
    d.digits_exact_vs_cert = 15.0;
    return d;
}

es::DatasetFigures gamma_dataset() {
    es::DatasetFigures d = alpha();
    d.name = "Gamma";
    d.certified_no_digit = true;
    d.digits_exact_vs_cert = -0.04;
    return d;
}

es::Report made_up_report() {
    es::Report rep;
    rep.u = 0x1p-53;
    rep.eta_rep = 0.5;
    rep.cert_relative = 0.25;
    rep.datasets = {alpha(), beta(), gamma_dataset()};
    rep.rule_u_pontius.ratio = 0.5;
    rep.rule_u_pontius.pivots = std::vector<double>{4.0, 2.0, 1.0};
    rep.rule_u_pontius_equilibrated.breakdown = 1;
    es::HatchFigures& h = rep.hatch;
    h.free_indices = {0, 1};
    h.eliminated = {2};
    h.n_free = 2;
    h.e = {3, 6, 9};
    h.kappa = 1e10;
    h.lam_min_a = 0.25;
    h.q1 = 2.5e5;
    h.dx = 2.0;
    h.r_orig = 3.0;
    h.r_mod = 0.5;
    h.mag = 1.5;
    h.de_v = 0.75;
    h.dof = 76;
    h.b_code = 0.001;
    h.b_test = 0.002;
    h.digits_exact_vs_cert = 11.04;
    es::DemmelFigures& dm = rep.demmel;
    dm.e = {0, 1, 2, 3};
    dm.p = {0.5, -0.5, 0.25, 0.75};
    dm.kappa_a = 12.5;
    dm.lam_min_a = 0.125;
    dm.lam_max_a = 1.75;
    dm.kappa_h = 3e50;
    dm.kappa_ns = 12.25;
    dm.q1 = 11.5;
    dm.theta_n = 6.25e-15;
    dm.b = 2.5e-13;
    dm.b_exact_a = 5e-14;
    dm.published_bound = 1.5e-14;
    rep.syn_dependent.e = {1, 1, 1};
    rep.syn_dependent.breakdown_index = 2;
    rep.syn_dependent.breakdown_pivot = 0.0;
    rep.syn_dependent.dependency = {1.0, 0.0};
    rep.syn_nearly_dependent.e = {1, 1};
    rep.syn_nearly_dependent.kappa = 5e14;
    rep.syn_nearly_dependent.lam_min_ns = 3.5e-15;
    rep.syn_nearly_dependent.q1 = 2.8e14;
    rep.syn_nearly_dependent.b = 2.75;
    rep.syn_nearly_dependent.theta_n = 2.5e-15;
    rep.tridiagonal = {es::Tridiagonal{1, 2, 4.0e-16, 1.0, 4.0e-16}, es::Tridiagonal{40, 41, 5.0e-13, 3.0e-3, 1.5e-10}};
    return rep;
}

Strings lines_of(const std::string& text) { return splitlines_py(text); }

std::string zeros(std::size_t k) {
    std::string out;
    for (std::size_t i = 0; i < k; ++i) out += ", 0x0.0p+0";
    return out;
}

}  // namespace

TEST_CASE("build_header on a made-up report: every line is the Python's f-string with the report's numbers, padded arrays, -1 for a Cholesky that completes, HUGE_VAL, 0.0 for none", "[estimation_sizing][behaviour]") {
    const es::Report rep = made_up_report();
    const std::string text = es::build_header(rep);
    CHECK(text.back() == '\n');
    CHECK(text.size() > 2U);
    CHECK(text[text.size() - 2] != '\n');   // one newline at the end, not two
    const Strings l = lines_of(text);
    REQUIRE(l.size() == 93U);
    CHECK(l[0] == "// GENERATED by tools/estimation_sizing.cpp -- do not edit.");
    CHECK(l[1] == "//");
    CHECK(l[4] == "#pragma once");
    CHECK(l[6] == "#include <cmath>");
    CHECK(l[8] == "namespace odl::estimation::registered {");
    CHECK(l[10] == "inline constexpr int kMaxN = 11;");
    CHECK(l[11] == "inline constexpr double kUnitRoundoff = 0x1p-53;");
    CHECK(l[12] == "inline constexpr double kEtaRep = 0x1.0000000000000p-1;              // 2.01 u: a decimal datum rounded to double through long double, doubled for a product");
    CHECK(l[13] == "inline constexpr double kCertifiedRelative = 0x1.0000000000000p-2;   // one unit of the 15th printed digit, worst case");
    CHECK(l[15] == "struct NistFigures {");
    CHECK(l[26] == "    double rho_exact[kMaxN * (kMaxN - 1) / 2];   ///< the exact correlations, row-major upper triangle");
    CHECK(l[29] == "};");
    CHECK(l[31] == "inline constexpr NistFigures kNist[] = {");
    // Alpha: nine lines
    CHECK(l[32] == "    {\"Alpha\", 2, 3, 12345ULL, {1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},");
    CHECK(l[33] == "     0x1.9000000000000p+6, 0x1.0000000000000p-1, 0x1.0000000000000p+1, 0x1.0000000000000p+3, 0x1.0000000000000p-2, 0x1.8000000000000p-1,");
    CHECK(l[34] == "     0x1.0000000000000p-50, 0x1.0000000000000p-49, 0x1.0000000000000p-3, 0x1.0000000000000p-2, 0x1.0000000000000p-4, 0x1.8000000000000p-3, 0x1.0000000000000p+2, 0x1.8000000000000p-1,");
    CHECK(l[35] == "     0x1.8000000000000p+0, 0x1.0000000000000p-1, 0x1.0000000000000p-2, 0x1.c000000000000p-1,");
    CHECK(l[36] == "     {0x1.0000000000000p+0, 0x1.4000000000000p+1" + zeros(9) + "},");
    CHECK(l[37] == "     {0x1.0000000000000p-1, 0x1.0000000000000p-2" + zeros(9) + "},");
    CHECK(l[38] == "     {0x1.0000000000000p-1, -0x1.0000000000000p-1" + zeros(9) + "},");
    CHECK(l[39] == "     {0x1.0000000000000p-1" + zeros(54) + "},");
    CHECK(l[40] == "     -1, 0.0},");
    // Beta: a checksum of 64 bits, a Cholesky that broke at index 1 with pivot -0.5, infinities
    CHECK(l[41] == "    {\"Beta\", 1, 2, 18446744073709551615ULL, {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},");
    CHECK(l[42] == "     0x1.0000000000000p+0, 0x1.0000000000000p-1, 0x1.0000000000000p+1, 0x1.0000000000000p-1, 0x1.0000000000000p-2, 0x1.8000000000000p-1,");
    CHECK(l[43] == "     " + es::hexf(2.0e-16) + ", 0x1.0000000000000p-49, HUGE_VAL, HUGE_VAL, HUGE_VAL, 0x1.8000000000000p-3, 0x1.0000000000000p+2, 0x1.8000000000000p-1,");
    CHECK(l[44] == "     0x1.8000000000000p+0, 0x1.0000000000000p-1, 0x1.0000000000000p-2, 0x1.c000000000000p-1,");
    CHECK(l[45] == "     {0x1.8000000000000p+1" + zeros(10) + "},");
    CHECK(l[46] == "     {0x1.0000000000000p+1" + zeros(10) + "},");
    CHECK(l[47] == "     {0x1.0000000000000p+0" + zeros(10) + "},");
    CHECK(l[48] == "     {0x0.0p+0" + zeros(54) + "},");
    CHECK(l[49] == "     1, -0x1.0000000000000p-1},");
    // Gamma follows
    CHECK(l[50] == "    {\"Gamma\", 2, 3, 12345ULL, {1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0},");
    CHECK(l[58] == "     -1, 0.0},");
    CHECK(l[59] == "};");
    CHECK(l[60] == "");
    // the hatch
    CHECK(l[61] == "/// Filip with B6 .. B10 ELIMINATED at their certified values (EST-A-210).");
    CHECK(l[62] == "struct HatchFigures { int n_free, dof; int e[kMaxN]; int breakdown_index; double kappa, lam_min_a, q1, dx_norm, r_orig, r_mod, magnitude, d_e_v, b_code, b_test, digits_exact_vs_certified; };");
    CHECK(l[63] == "inline constexpr HatchFigures kHatchFilip = {2, 76, {3, 6, 9, 0, 0, 0, 0, 0, 0, 0, 0}, -1,");
    CHECK(l[64] == "    " + es::hexf(1e10) + ", " + es::hexf(0.25) + ", " + es::hexf(2.5e5) + ", " + es::hexf(2.0) + ", " + es::hexf(3.0) + ", " + es::hexf(0.5) + ", " + es::hexf(1.5) + ", " + es::hexf(0.75) + ",");
    CHECK(l[65] == "    " + es::hexf(0.001) + ", " + es::hexf(0.002) + ", " + es::hexf(11.04) + "};");
    // the rule U: none is 0.0
    CHECK(l[67] == "/// The unscaled decision rule U of EST-A-209: min pivot / max pivot of the Cholesky factor of the UNSCALED normal matrix.");
    CHECK(l[68] == "inline constexpr double kRuleUPontius = 0x1.0000000000000p-1;");
    CHECK(l[69] == "inline constexpr double kRuleUPontiusEquilibrated = 0.0;");
    // Demmel
    CHECK(l[71] == "/// Demmel's worked example (LAPACK Working Note 14, page 4): H = D A D, D = diag(1, 1e5, 1e-10, 1e15), b = (42, -26, 24, 34).");
    CHECK(l[73] == "inline constexpr DemmelFigures kDemmel = {{0, 1, 2, 3}, {0x1.0000000000000p-1, -0x1.0000000000000p-1, 0x1.0000000000000p-2, 0x1.8000000000000p-1},");
    CHECK(l[74] == "    " + es::hexf(12.5) + ", " + es::hexf(0.125) + ", " + es::hexf(1.75) + ", " + es::hexf(3e50) + ", " + es::hexf(12.25) + ", " + es::hexf(11.5) + ", " + es::hexf(6.25e-15) + ", " + es::hexf(2.5e-13) + ", " +
                   es::hexf(5e-14) + ", " + es::hexf(1.5e-14) + "};");
    // the synthetic problems
    CHECK(l[77] == "inline constexpr int kSynDependentBreakdownIndex = 2;");
    CHECK(l[78] == "inline constexpr double kSynDependentBreakdownPivot = 0x0.0p+0;");
    CHECK(l[79] == "/// SYN-nearly-dependent: two parameters whose columns differ by 2^-23 in two entries (EST-A-211).");
    CHECK(l[80] == "inline constexpr double kSynNearKappa = " + es::hexf(5e14) + ";");
    CHECK(l[81] == "inline constexpr double kSynNearLamMin = " + es::hexf(3.5e-15) + ";");
    CHECK(l[82] == "inline constexpr double kSynNearQ1 = " + es::hexf(2.8e14) + ";");
    CHECK(l[83] == "inline constexpr double kSynNearB = " + es::hexf(2.75) + ";");
    // the tridiagonal ones, and the end
    CHECK(l[87] == "inline constexpr TridiagonalFigures kTridiagonal[] = {");
    CHECK(l[88] == "    {1, " + es::hexf(4.0e-16) + ", 0x1.0000000000000p+0, " + es::hexf(4.0e-16) + "},");
    CHECK(l[89] == "    {40, " + es::hexf(5.0e-13) + ", " + es::hexf(3.0e-3) + ", " + es::hexf(1.5e-10) + "},");
    CHECK(l[90] == "};");
    CHECK(l[91] == "");
    CHECK(l[92] == "}  // namespace odl::estimation::registered");
    // a hatch whose Cholesky broke at 4 writes the index, and a breakdown pivot of none is 0.0
    es::Report r2 = rep;
    r2.hatch.breakdown_index = 4;
    CHECK(lines_of(es::build_header(r2)).at(63) == "inline constexpr HatchFigures kHatchFilip = {2, 76, {3, 6, 9, 0, 0, 0, 0, 0, 0, 0, 0}, 4,");
    // an array longer than its width is not cut (the Python only pads)
    es::Report r3 = rep;
    r3.datasets[0].e = std::vector<int>(13, 7);
    CHECK(lines_of(es::build_header(r3)).at(32) == "    {\"Alpha\", 2, 3, 12345ULL, {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7},");
}

TEST_CASE("build_spec_table on a made-up report: the markers, the table rows (the outcome of a breakdown, the dashes), and the paragraphs, each as the Python's f-string writes it", "[estimation_sizing][behaviour]") {
    const es::Report rep = made_up_report();
    const std::string text = es::build_spec_table(rep);
    CHECK(text.back() != '\n');   // the END marker ends it; the Python's print adds the newline
    const Strings l = lines_of(text);
    REQUIRE(l.size() == 26U);
    CHECK(l[0] == "<!-- BEGIN estimation_sizing: generated by tools/estimation_sizing.cpp --spec-table; do not edit by hand -->");
    CHECK(l[0] == es::kBegin);
    CHECK(l[1] == "");
    CHECK(l[2].rfind("**The registered figures, per dataset** (NIST StRD linear regression, read from the pinned cache; `m` rows, `n` parameters; the design recipe of §8.2; `u = 2⁻⁵³`). `e` are the scale exponents of `EST-R-201`;", 0) == 0U);
    CHECK(l[2].size() > 500U);
    CHECK(ends_with(l[2], "`B_Q` the bound on `|Q̂ᵢᵢ/Qᵢᵢ − 1|`; `B_σ` the bound on `|σ̂₀ − σ₀|`; the last column what the factorisation of the scaled matrix does."));
    CHECK(l[3] == "");
    CHECK(l[4] == "| dataset | `n × m` | `e` | `κ` | `min κᵢ` – `max κᵢ` | `q₁` | `Θ_N` | `B` (code) | `B` (test) | `B_Q` | `B_σ` | outcome |");
    CHECK(l[5] == "|---|---|---|---|---|---|---|---|---|---|---|---|");
    CHECK(l[6] == "| Alpha | 2 × 3 | 1, -2 | 1.00 × 10² | 1.00 – 2.50 | 8.00 | 8.88 × 10⁻¹⁶ | 1.25 × 10⁻¹ | 2.50 × 10⁻¹ | 6.25 × 10⁻² | 4.00 | completes |");
    CHECK(l[7] == "| Beta | 1 × 2 | 5 | 1.00 | 3.00 – 3.00 | 5.00 × 10⁻¹ | 2.00 × 10⁻¹⁶ | — | — | — | — | **EST-F-202 at `B1`**, pivot -5.0 × 10⁻¹ |");
    CHECK(l[8] == "| Gamma | 2 × 3 | 1, -2 | 1.00 × 10² | 1.00 – 2.50 | 8.00 | 8.88 × 10⁻¹⁶ | 1.25 × 10⁻¹ | 2.50 × 10⁻¹ | 6.25 × 10⁻² | 4.00 | completes |");
    CHECK(l[9] == "");
    CHECK(l[10] == "**The hatch (`EST-A-210`):** Filip with `B6 … B10` eliminated at their certified values — `n_free` = 2, `e` = 3, 6, the factorisation completes, `κ` = 1.00 × 10¹⁰, `q₁` = 2.50 × 10⁵, "
                   "`B` (code) = 1.00 × 10⁻³, `B` (test) = 2.00 × 10⁻³, ν = 76; the exact reduced solution agrees with the certified `B0 … B5` to **11.0 digits**.");
    CHECK(l[11] == "");
    CHECK(l[12] == "**The unscaled decision `U` (`EST-A-209`)** — the smallest over the largest pivot of the Cholesky factor of the UNSCALED normal matrix, in double: Pontius 5.00 × 10⁻¹; the same problem with every "
                   "design column divided by its own norm 0. `U` refuses below `n·2⁻⁵²` (6.66 × 10⁻¹⁶ at n = 3).");
    CHECK(l[13] == "");
    CHECK(l[14] == "**Demmel's example (`EST-A-208`):** `e` = 0, 1, 2, 3; **exact** κ₂(A) = 12.5000 (λ_max = 1.7500, λ_min = 0.1250) — the page prints “κ(A) ≈ 2.0”, which is **not** the 2-norm condition number of "
                   "the printed A, and the gate does not use it; κ₂(H) = 3.00 × 10⁵⁰ (the page: ≈ 10⁵⁰), κ₂(N_s) = 12.2500, `q₁` = 11.5000, `Θ_N` = 6.25 × 10⁻¹⁵ (η_H = 1.002 u for the matrix entries, none for b), "
                   "`B` = 2.50 × 10⁻¹³ (and 5.00 × 10⁻¹⁴ with ‖A⁻¹‖₂ in place of 4 q₁); the printed bound is 1.5 × 10⁻¹⁴.");
    CHECK(l[15] == "");
    CHECK(l[16] == "**Synthetic (`EST-A-211`):** `SYN-dependent` — `e` = 1, 1, 1, the third pivot is exactly 0, refusal at index 2 with the dependency `c ≈ 1·a + 0·b`; `SYN-nearly-dependent` — `e` = 1, 1, "
                   "κ = 5.00 × 10¹⁴, λ_min(N_s) = 3.50 × 10⁻¹⁵, `q₁` = 2.80 × 10¹⁴, `B` = 2.750 (≥ ½: EST-F-203).");
    CHECK(l[17] == "");
    CHECK(l[18] == "**The tridiagonal problems (`EST-A-203`),** rows `eₖ − eₖ₋₁`: `n`, `Θ_s(n)`, `λ_min(A) = (1 − cos(π/(n+1)))`, the per-column bound `B_col = Θ_s / λ_min(A)`:");
    CHECK(l[19] == "");
    CHECK(l[20] == "| `n` | `Θ_s` | `λ_min(A)` | `B_col` |");
    CHECK(l[21] == "|---|---|---|---|");
    CHECK(l[22] == "| 1 | 4.00 × 10⁻¹⁶ | 1.00 | 4.00 × 10⁻¹⁶ |");
    CHECK(l[23] == "| 40 | 5.00 × 10⁻¹³ | 3.00 × 10⁻³ | 1.50 × 10⁻¹⁰ |");
    CHECK(l[24] == "");
    CHECK(l[25] == "<!-- END estimation_sizing -->");
    CHECK(l[25] == es::kEnd);
    // a hatch with all of its exponents shows them all when n_free says so; the n_free-th and later are cut
    es::Report r2 = rep;
    r2.hatch.n_free = 3;
    CHECK(contains(lines_of(es::build_spec_table(r2)).at(10), "`n_free` = 3, `e` = 3, 6, 9, the factorisation"));
    r2.hatch.n_free = 1;
    CHECK(contains(lines_of(es::build_spec_table(r2)).at(10), "`n_free` = 1, `e` = 3, the factorisation"));
    // an infinite bound is the symbol, a zero is '0'
    r2 = rep;
    r2.datasets[0].b_sigma = std::numeric_limits<double>::infinity();
    r2.datasets[0].b_q = 0.0;
    CHECK(contains(lines_of(es::build_spec_table(r2)).at(6), "| 0 | ∞ | completes |"));
}

TEST_CASE("print_report on a made-up report: every line of the Python's report with its field widths, the three kinds of refusal text, the dictionaries as Python prints them", "[estimation_sizing][behaviour]") {
    const es::Report rep = made_up_report();
    std::ostringstream out;
    es::print_report(rep, out);
    const std::string text = out.str();
    CHECK(text.back() == '\n');
    const Strings l = lines_of(text);
    REQUIRE(l.size() == 26U);
    CHECK(l[0] == "u = 2^-53 = 1.1102e-16; eta_rep = 5.0000e-01; certified values' own uncertainty = 2e-01");
    CHECK(l[1] == "");
    CHECK(l[2] == std::string("dataset  ") + " " + " n" + " " + "  m" + " " + " kappa(N_s)" + " " + " lam_min(A)" + " " + " ||Q_s||_1" + " " + "  Theta_N" + " " + "  B (code)" + " " + "  B (test)" + " " + "      B_Q" +
                      " " + "  B_sigma" + "  cert.digits  refusal");
    CHECK(l[3] == std::string("Alpha    ") + " " + " 2" + " " + "  3" + " " + " 1.0000e+02" + " " + " 5.0000e-01" + " " + " 8.000e+00" + " " + " 8.88e-16" + " " + " 1.250e-01" + " " + " 2.500e-01" + " " + " 6.25e-02" +
                      " " + " 4.00e+00" + "  " + "  12.3" + "      " + "-");
    CHECK(l[4] == std::string("Beta     ") + " " + " 1" + " " + "  2" + " " + " 1.0000e+00" + " " + " 5.0000e-01" + " " + " 5.000e-01" + " " + " 2.00e-16" + " " + "       inf" + " " + "       inf" + " " + "      inf" +
                      " " + " 4.00e+00" + "  " + "  15.0" + "      " + "EST-F-202 at B1 (pivot -5.000e-01)");
    CHECK(l[5] == std::string("Gamma    ") + " " + " 2" + " " + "  3" + " " + " 1.0000e+02" + " " + " 5.0000e-01" + " " + " 8.000e+00" + " " + " 8.88e-16" + " " + " 1.250e-01" + " " + " 2.500e-01" + " " + " 6.25e-02" +
                      " " + " 4.00e+00" + "  " + "  -0.0" + "      " + "EST-F-003");
    CHECK(l[6] == "");
    CHECK(l[7] == "scale exponents e (EST-R-201) and per-parameter kappa_i = N_ii (N^-1)_ii:");
    CHECK(l[8] == "  Alpha     e = [1, -2]   kappa_i = ['1', '2.5']");
    CHECK(l[9] == "  Beta      e = [5]   kappa_i = ['3']");
    CHECK(l[10] == "  Gamma     e = [1, -2]   kappa_i = ['1', '2.5']");
    CHECK(l[11] == "");
    CHECK(l[12] == "rule U (the unscaled decision of EST-A-209): ratio of the smallest to the largest Cholesky pivot of the UNSCALED N:");
    CHECK(l[13] == "  Pontius                  {'ratio': 0.5, 'pivots': [4.0, 2.0, 1.0], 'breakdown': None}");
    CHECK(l[14] == "  Pontius_equilibrated     {'ratio': None, 'breakdown': 1}");
    CHECK(l[15] == "");
    CHECK(l[16] == "the hatch (Filip, B6 .. B10 eliminated at the certified values): e = [3, 6, 9], breakdown = None, kappa(N_s) = 1.0000e+10, ||Q_s||_1 = 2.500e+05, B(code) = 1.000e-03, B(test) = 2.000e-03, dof = 76, "
                   "exact reduced solution vs certified: 11.0 digits");
    CHECK(l[17] == "");
    CHECK(l[18] == "Demmel's example: e = [0, 1, 2, 3], kappa(A) = 12.5000 (lambda_max = 1.7500), kappa(H) = 3.0000e+50, kappa(N_s) = 12.2500, ||Q_s||_1 = 1.1500e+01, Theta_N = 6.250e-15, B = 2.500e-13 (with 4 ||Q_s||_1) "
                   "and 5.000e-14 (with ||A^-1||_2), published bound 1.5e-14");
    CHECK(l[19] == "");
    CHECK(l[20] == "SYN-dependent: e = [1, 1, 1], breakdown at parameter index 2 with pivot 0.0, dependency c ~ [1.0, 0.0] (a, b)");
    CHECK(l[21] == "SYN-nearly-dependent: e = [1, 1], kappa(N_s) = 5.0000e+14, lam_min(N_s) = 3.5000e-15, ||Q_s||_1 = 2.8000e+14, Theta_N = 2.500e-15, B = 2.750");
    CHECK(l[22] == "");
    CHECK(l[23] == "tridiagonal (rows e_k - e_{k-1}): n, Theta_s, lambda_min(A), B_col = Theta_s / lambda_min(A):");
    CHECK(l[24] == "  n =   1  Theta_s = 4.000e-16  lam_min(A) = 1.0000e+00  B_col = 4.000e-16");
    CHECK(l[25] == "  n =  40  Theta_s = 5.000e-13  lam_min(A) = 3.0000e-03  B_col = 1.500e-10");
    // the other shapes of the rule U record: a breakdown of the unscaled Pontius case has no pivots; the hatch's breakdown index is printed; a pivot of the SYN problem is printed as repr
    es::Report r2 = rep;
    r2.rule_u_pontius = es::RuleU{};
    r2.rule_u_pontius.breakdown = 2;
    r2.rule_u_pontius_equilibrated = es::RuleU{};
    r2.rule_u_pontius_equilibrated.ratio = 0.024293380344446936;
    r2.hatch.breakdown_index = 3;
    r2.syn_dependent.breakdown_pivot = -1.4e-17;
    std::ostringstream out2;
    es::print_report(r2, out2);
    const Strings m = lines_of(out2.str());
    REQUIRE(m.size() == 26U);
    CHECK(m[13] == "  Pontius                  {'ratio': None, 'breakdown': 2}");
    CHECK(m[14] == "  Pontius_equilibrated     {'ratio': 0.024293380344446936, 'breakdown': None}");
    CHECK(contains(m[16], ", breakdown = 3, "));
    CHECK(contains(m[20], "with pivot -1.4e-17,"));
    // a dataset whose Cholesky broke AND whose bound is large is a breakdown, not EST-F-003
    es::Report r3 = rep;
    r3.datasets[1].certified_no_digit = true;
    std::ostringstream out3;
    es::print_report(r3, out3);
    CHECK(lines_of(out3.str()).at(4).rfind("Beta     ", 0) == 0U);
    CHECK(contains(lines_of(out3.str())[4], "EST-F-202 at B1 (pivot -5.000e-01)"));
    CHECK(!contains(lines_of(out3.str())[4], "EST-F-003"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the command line

namespace {

struct Seen {
    int calls = 0;
    fs::path cache;
};

es::Settings made_up_settings(Seen& seen) {
    es::Settings s;
    s.cache = "/the/default/cache";
    s.make_report = [&seen](const fs::path& cache) {
        ++seen.calls;
        seen.cache = cache;
        return made_up_report();
    };
    return s;
}

}  // namespace

TEST_CASE("the modes of the tool, on a made-up report: the report (default and --report), --spec-table, --header, and which of them wins", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    Seen seen;
    const es::Settings settings = made_up_settings(seen);
    const es::Report rep = made_up_report();
    std::ostringstream expected_report;
    es::print_report(rep, expected_report);
    for (const std::vector<std::string>& args : {std::vector<std::string>{}, std::vector<std::string>{"--report"}}) {
        seen = Seen{};
        const Result r = run_with(settings, args);
        CHECK(r.code == 0);
        CHECK(r.out == expected_report.str());
        CHECK(r.err.empty());
        CHECK(seen.calls == 1);
        CHECK(seen.cache == fs::path("/the/default/cache"));
    }
    // --cache DIR and --cache=DIR reach the report
    {
        seen = Seen{};
        CHECK(run_with(settings, {"--cache", "/elsewhere"}).code == 0);
        CHECK(seen.cache == fs::path("/elsewhere"));
        seen = Seen{};
        CHECK(run_with(settings, {"--cache=/other"}).code == 0);
        CHECK(seen.cache == fs::path("/other"));
    }
    // --spec-table prints the block and a newline (the Python's print)
    {
        const Result r = run_with(settings, {"--spec-table"});
        CHECK(r.code == 0);
        CHECK(r.out == es::build_spec_table(rep) + "\n");
        CHECK(r.err.empty());
    }
    // --header writes the file, says how many lines on the error stream, with the path the way pathlib spells it
    {
        const fs::path target = td.path() / "registered.hpp";
        const Result r = run_with(settings, {"--header", target.string()});
        CHECK(r.code == 0);
        CHECK(r.out.empty());
        const std::string text = es::build_header(rep);
        CHECK(slurp(target) == text);
        CHECK(r.err == "wrote " + std::to_string(lines_of(text).size()) + " lines to " + target.string() + "\n");
        const Result r2 = run_with(settings, {"--header", td.path().string() + "//./again.hpp"});
        CHECK(r2.code == 0);
        CHECK(r2.err == "wrote " + std::to_string(lines_of(text).size()) + " lines to " + (td.path() / "again.hpp").string() + "\n");
        const Result r3 = run_with(settings, {"--header=" + (td.path() / "third.hpp").string()});
        CHECK(r3.code == 0);
        CHECK(slurp(td.path() / "third.hpp") == text);
        // pathlib's spelling of RELATIVE paths too: no "." component, no empty one, no trailing slash (the working directory is the scratch one for the moment)
        {
            struct Cwd {
                fs::path before = fs::current_path();
                explicit Cwd(const fs::path& to) { fs::current_path(to); }
                ~Cwd() { fs::current_path(before); }
            };
            fs::create_directories(td.path() / "sub");
            const Cwd in_scratch(td.path());
            const std::size_t line_count = lines_of(text).size();
            const std::string prefix = "wrote " + std::to_string(line_count) + " lines to ";
            CHECK(run_with(settings, {"--header", "plain.hpp"}).err == prefix + "plain.hpp\n");
            CHECK(run_with(settings, {"--header", "./dot.hpp"}).err == prefix + "dot.hpp\n");
            CHECK(run_with(settings, {"--header", "sub//./deep.hpp"}).err == prefix + "sub/deep.hpp\n");
            CHECK(run_with(settings, {"--header", "./sub/./../up.hpp"}).err == prefix + "sub/../up.hpp\n");
            CHECK(slurp(td.path() / "sub" / "deep.hpp") == text);
            CHECK(slurp(td.path() / "up.hpp") == text);
            // a path that names a directory is not a file to write
            const Result onto_directory = run_with(settings, {"--header", "sub/"});
            CHECK(onto_directory.code == 2);
            CHECK(contains(onto_directory.err, "cannot write"));
            const Result empty = run_with(settings, {"--header", ""});
            CHECK(empty.code == 2);
        }
        // a directory that does not exist is refused, naming the file
        const Result bad = run_with(settings, {"--header", (td.path() / "nowhere" / "x.hpp").string()});
        CHECK(bad.code == 2);
        CHECK(contains(bad.err, "estimation_sizing: cannot write"));
        CHECK(contains(bad.err, "nowhere"));
        CHECK(bad.out.empty());
    }
    // the precedence of the Python's if-chain: --spec-table, then --verify-spec, then --verify, then --header, then the report
    {
        const fs::path target = td.path() / "never.hpp";
        const Result r = run_with(settings, {"--header", target.string(), "--spec-table"});
        CHECK(r.out == es::build_spec_table(rep) + "\n");
        CHECK(!fs::exists(target));
        const fs::path header_file = td.path() / "h.hpp";
        write_text(header_file, es::build_header(rep));
        const Result v = run_with(settings, {"--header", target.string(), "--verify", header_file.string()});
        CHECK(v.code == 0);
        CHECK(!fs::exists(target));
        CHECK(v.err == "ok       the committed header is exactly what the generator emits\n");
        const fs::path spec_file = td.path() / "s.md";
        write_text(spec_file, "x\n" + es::build_spec_table(rep) + "\ny\n");
        const Result w = run_with(settings, {"--verify", header_file.string(), "--verify-spec", spec_file.string()});
        CHECK(w.code == 0);
        CHECK(w.err == "ok       the specification's generated block is exactly what the generator emits\n");
        const Result both = run_with(settings, {"--verify-spec", spec_file.string(), "--spec-table"});
        CHECK(both.out == es::build_spec_table(rep) + "\n");
        CHECK(both.err.empty());
    }
}

TEST_CASE("--verify and --verify-spec: ok on standard ERROR (as the Python's print(..., file=sys.stderr)), MISMATCH and exit 1 otherwise, exit 2 for a file that cannot be read", "[estimation_sizing][behaviour]") {
    TempDir td("odl-est");
    Seen seen;
    const es::Settings settings = made_up_settings(seen);
    const es::Report rep = made_up_report();
    const std::string header = es::build_header(rep);
    const std::string table = es::build_spec_table(rep);
    const std::string ok_header = "ok       the committed header is exactly what the generator emits\n";
    const std::string bad_header = "MISMATCH the committed header differs from the generator's output (a figure moved: the registration is not what it was)\n";
    const std::string ok_spec = "ok       the specification's generated block is exactly what the generator emits\n";
    const std::string bad_spec = "MISMATCH the specification's generated block differs from the generator's output (or is missing)\n";
    const auto verify = [&](const std::string& content) {
        const fs::path f = td.path() / "committed.hpp";
        write_text(f, content);
        return run_with(settings, {"--verify", f.string()});
    };
    const auto verify_spec = [&](const std::string& content) {
        const fs::path f = td.path() / "spec.md";
        write_text(f, content);
        return run_with(settings, {"--verify-spec", f.string()});
    };
    {
        const Result r = verify(header);
        CHECK(r.code == 0);
        CHECK(r.out.empty());
        CHECK(r.err == ok_header);
    }
    {   // one byte changed
        std::string changed = header;
        changed[header.find("12345")] = '9';
        const Result r = verify(changed);
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == bad_header);
    }
    CHECK(verify(header.substr(0, header.size() - 1)).code == 1);   // the final newline missing
    CHECK(verify(header + "\n").code == 1);
    CHECK(verify("").code == 1);
    // universal newlines, as Path.read_text reads: a committed file with CRLF or CR line ends is the same text
    {
        std::string crlf;
        for (const char c : header) crlf += (c == '\n') ? std::string("\r\n") : std::string(1, c);
        CHECK(verify(crlf).code == 0);
        std::string cr;
        for (const char c : header) cr += (c == '\n') ? '\r' : c;
        CHECK(verify(cr).code == 0);
    }
    // a file that is missing, a directory, bytes that are not UTF-8
    {
        const Result missing = run_with(settings, {"--verify", (td.path() / "nowhere.hpp").string()});
        CHECK(missing.code == 2);
        CHECK(contains(missing.err, "estimation_sizing: cannot read"));
        CHECK(contains(missing.err, "nowhere.hpp"));
        const Result dir = run_with(settings, {"--verify", td.path().string()});
        CHECK(dir.code == 2);
        CHECK(contains(dir.err, "not a regular file"));
        write_text(td.path() / "latin1.hpp", "caf\xe9\n");
        const Result latin = run_with(settings, {"--verify", (td.path() / "latin1.hpp").string()});
        CHECK(latin.code == 2);
        CHECK(contains(latin.err, "not well-formed UTF-8"));
    }
    // the block of the specification: found between the first BEGIN and the first END
    {
        const Result r = verify_spec("# title\n\nprose\n" + table + "\nmore prose\n");
        CHECK(r.code == 0);
        CHECK(r.out.empty());
        CHECK(r.err == ok_spec);
    }
    CHECK(verify_spec(table).code == 0);           // nothing around it, no newline after the END marker
    CHECK(verify_spec(table + "\n").code == 0);
    CHECK(verify_spec("a\r\n" + table + "\r\nb\r\n").code == 0);
    {
        std::string changed = table;
        changed[table.find("| Alpha |") + 3] = 'X';
        const Result r = verify_spec("x\n" + changed + "\n");
        CHECK(r.code == 1);
        CHECK(r.err == bad_spec);
    }
    CHECK(verify_spec("nothing here\n").code == 1);                              // neither marker
    CHECK(verify_spec(table.substr(0, table.size() - 5) + "\n").code == 1);      // END marker damaged
    CHECK(verify_spec(table.substr(table.find('\n') + 1)).code == 1);            // BEGIN marker missing
    CHECK(verify_spec(std::string(es::kEnd) + "\n" + table).code == 1);          // an END marker before the BEGIN: the FIRST END is the one looked for, and it is before the BEGIN
    CHECK(verify_spec(table + "\n" + es::kEnd + "\n").code == 0);                // a second END later changes nothing
    CHECK(verify_spec(std::string(es::kBegin) + "\n" + table).code == 1);        // a second BEGIN before it: find() is the first BEGIN, and the block it opens (to the END) is not the table
    CHECK(verify_spec("<!-- BEGIN estimation_sizing: generated by tools/estimation_sizing.py --spec-table; do not edit by hand -->\n" + table.substr(table.find('\n') + 1)).code == 1);   // the old marker
    {
        const Result missing = run_with(settings, {"--verify-spec", (td.path() / "nowhere.md").string()});
        CHECK(missing.code == 2);
        CHECK(contains(missing.err, "cannot read"));
        CHECK(contains(missing.err, "nowhere.md"));
    }
}

TEST_CASE("the command line: options by their exact names, the value in the next argument or after '=', -h; everything else is refused with the usage line and exit 2", "[estimation_sizing][behaviour]") {
    Seen seen;
    const es::Settings settings = made_up_settings(seen);
    const std::string usage = "usage: estimation_sizing [-h] [--cache CACHE] [--report] [--header HEADER] [--verify VERIFY] [--spec-table] [--verify-spec VERIFY_SPEC]\n";
    // the whole text of -h, pinned (it is documentation, and this is its snapshot: a change of the words is a change of this test)
    const std::string help = usage +
        "\n"
        "The FROZEN FIGURES of SPEC-estimation section 6 (L7 step 2): the registered header modules/estimation/tests/registered_figures.hpp and the specification's generated table, from exact arithmetic on\n"
        "the pinned NIST files.  With none of --header, --verify, --spec-table and --verify-spec the figures are printed as a report.\n"
        "\n"
        "options:\n"
        "  -h, --help                show this help and exit\n"
        "  --cache CACHE             the pinned cache (default: data/cache of the tree this tool was built from)\n"
        "  --report                  print the report (the default; no effect)\n"
        "  --header HEADER           write the C++ header of frozen figures\n"
        "  --verify VERIFY           exit 1 unless this committed header is exactly what the generator emits\n"
        "  --spec-table              print the specification's generated table block\n"
        "  --verify-spec VERIFY_SPEC exit 1 unless this specification's generated block is exactly what the generator emits\n"
        "\n"
        "exit codes: 0 done (and, with a --verify option, the committed text is exactly what the generator emits)   1 the committed text differs from the generator's\n"
        "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected   70 an error the tool did not anticipate\n";
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_with(settings, {flag});
        CHECK(r.code == 0);
        CHECK(r.out.rfind(usage, 0) == 0U);
        CHECK(contains(r.out, "--verify-spec"));
        CHECK(r.out == help);
        CHECK(r.err.empty());
        CHECK(seen.calls == 0);
    }
    // -h wins over what comes after it
    CHECK(run_with(settings, {"-h", "--frobnicate"}).code == 0);
    const auto refused = [&](const std::vector<std::string>& args, const std::string& what) {
        const Result r = run_with(settings, args);
        INFO("arguments: " << args.size() << " first: " << (args.empty() ? "" : args[0]));
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == usage + "estimation_sizing: error: " + what + "\n");
    };
    refused({"--frobnicate"}, "unrecognized arguments: --frobnicate");
    refused({"extra"}, "unrecognized arguments: extra");
    refused({"--rep"}, "unrecognized arguments: --rep");           // no abbreviations
    refused({"--report=1"}, "unrecognized arguments: --report=1");   // a flag takes no value
    refused({"--spec-table=1"}, "unrecognized arguments: --spec-table=1");
    refused({"--header"}, "argument --header: expected one argument");
    refused({"--verify"}, "argument --verify: expected one argument");
    refused({"--verify-spec"}, "argument --verify-spec: expected one argument");
    refused({"--cache"}, "argument --cache: expected one argument");
    refused({"--header", "--verify", "x"}, "argument --header: expected one argument");   // an option is not a value
    refused({"--cache", "--report"}, "argument --cache: expected one argument");
    refused({"--cache", "---x"}, "argument --cache: expected one argument");   // (three dashes begin with two)
    refused({"--cache", "--"}, "argument --cache: expected one argument");
    CHECK(seen.calls == 0);
    // an empty value after '=' is a value
    {
        const Result r = run_with(settings, {"--cache="});
        CHECK(r.code == 0);
        CHECK(seen.cache.empty());
    }
}

TEST_CASE("errors: a report that cannot be made (BadInput) is a refusal, exit 2; anything else is an error the tool did not anticipate, exit 70; the real tool refuses a cache without the files", "[estimation_sizing][behaviour]") {
    es::Settings s;
    s.cache = "/c";
    s.make_report = [](const fs::path&) -> es::Report { throw es::BadInput("Norris.dat: nothing to read"); };
    {
        const Result r = run_with(s, {});
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == "estimation_sizing: Norris.dat: nothing to read\n");
    }
    for (const char* mode : {"--spec-table", "--report"}) {
        CHECK(run_with(s, {mode}).code == 2);
    }
    s.make_report = [](const fs::path&) -> es::Report { throw std::logic_error("a defect"); };
    {
        const Result r = run_with(s, {});
        CHECK(r.code == 70);
        CHECK(r.err == "estimation_sizing: internal error: a defect\n");
    }
    // the output stream refuses: the last-resort handler
    {
        Seen seen;
        const es::Settings ok = made_up_settings(seen);
        odl::devtools_testing::ThrowingStream out;
        std::ostringstream err;
        const int code = es::run_on(ok, {}, Streams{out, err});
        CHECK(code == 70);
        CHECK(contains(err.str(), "estimation_sizing: internal error: boom"));
    }
    // the real thing on a cache that has nothing in it
    {
        TempDir td("odl-est");
        std::ostringstream out;
        std::ostringstream err;
        const int code = es::run({"--cache", td.path().string()}, Streams{out, err});
        CHECK(code == 2);
        CHECK(out.str().empty());
        CHECK(contains(err.str(), "estimation_sizing: cannot read"));
        CHECK(contains(err.str(), (td.path() / "nist-strd-lls-norris" / "Norris.dat").string()));
        // a cache with Norris but not Pontius: the first file that is missing, in the order of the table
        fs::create_directories(td.path() / "nist-strd-lls-norris");
        write_text(td.path() / "nist-strd-lls-norris" / "Norris.dat", joined(mini_lines()));
        std::ostringstream err2;
        std::ostringstream out2;
        // (the mini file has one parameter's worth of data: its analysis may refuse before the second file is read, but the refusal names a file or a fact, and the exit status is 2 either way)
        const int code2 = es::run({"--cache", td.path().string()}, Streams{out2, err2});
        CHECK(code2 == 2);
        CHECK(contains(err2.str(), (td.path() / "nist-strd-lls-pontius" / "Pontius.dat").string()));
    }
    CHECK(es::default_settings().cache == es::default_root() / "data" / "cache");
    CHECK(!es::default_settings().make_report);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the real data

namespace {

const fs::path& tree_root() {
    static const fs::path root = es::default_root();
    return root;
}

const es::Report& real_report() {
    static const es::Report rep = es::build_report(es::default_settings().cache);
    return rep;
}

std::string committed_header_text() { return universal_newlines(read_text(tree_root() / "modules" / "estimation" / "tests" / "registered_figures.hpp")); }

std::string committed_spec_block() {
    const std::string doc = universal_newlines(read_text(tree_root() / "spec" / "SPEC-estimation.md"));
    const std::size_t i = doc.find(es::kBegin);
    const std::size_t j = doc.find(es::kEnd);
    REQUIRE(i != std::string::npos);
    REQUIRE(j != std::string::npos);
    REQUIRE(j > i);
    return doc.substr(i, j + std::string(es::kEnd).size() - i);
}

// one token of the committed header as a number: a hexadecimal float, 0.0, HUGE_VAL
double number_of(const std::string& token) {
    if (token == "HUGE_VAL") return std::numeric_limits<double>::infinity();
    if (token == "-HUGE_VAL") return -std::numeric_limits<double>::infinity();
    return std::strtod(token.c_str(), nullptr);
}

// the tokens of the initializer that follows `marker` in the header, up to its closing "};": braces and commas are separators
Strings initializer_tokens(const std::string& text, const std::string& marker) {
    const std::size_t at = text.find(marker);
    REQUIRE(at != std::string::npos);
    const std::size_t from = at + marker.size();
    const std::size_t to = text.find("};", from);
    REQUIRE(to != std::string::npos);
    std::string body = text.substr(from, to - from);
    for (char& c : body) {
        if (c == '{' || c == '}' || c == ',') c = ' ';
    }
    return split_py(body);
}

struct CommittedDataset {
    std::string name;
    int n = 0, m = 0;
    std::uint64_t checksum = 0;
    std::vector<int> e;
    double kappa = 0, lam_min_a = 0, lam_max_a = 0, q1 = 0, dx_norm = 0, r_norm = 0;
    double theta_n_code = 0, theta_n_test = 0, b_code = 0, b_test = 0, b_q = 0, b_rho = 0, b_sigma = 0, sigma0_exact = 0;
    double b_pair = 0, gap = 0, tau_s = 0, cos_min = 0;
    std::vector<double> kappa_i, qdiag, weakest_exact, rho_exact;
    int breakdown_index = -1;
    double breakdown_pivot = 0;
};

std::vector<CommittedDataset> committed_datasets(const std::string& text) {
    const Strings t = initializer_tokens(text, "inline constexpr NistFigures kNist[] = {");
    std::vector<CommittedDataset> out;
    std::size_t at = 0;
    const auto next = [&]() -> const std::string& {
        REQUIRE(at < t.size());
        return t[at++];
    };
    const auto next_double = [&] { return number_of(next()); };
    const auto next_array = [&](std::size_t count) {
        std::vector<double> v;
        for (std::size_t i = 0; i < count; ++i) v.push_back(next_double());
        return v;
    };
    while (at < t.size()) {
        CommittedDataset d;
        const std::string quoted = next();
        d.name = quoted.substr(1, quoted.size() - 2);
        d.n = std::stoi(next());
        d.m = std::stoi(next());
        const std::string& ck = next();
        d.checksum = std::stoull(ck.substr(0, ck.size() - 3));   // "...ULL"
        for (int i = 0; i < 11; ++i) d.e.push_back(std::stoi(next()));
        d.kappa = next_double();
        d.lam_min_a = next_double();
        d.lam_max_a = next_double();
        d.q1 = next_double();
        d.dx_norm = next_double();
        d.r_norm = next_double();
        d.theta_n_code = next_double();
        d.theta_n_test = next_double();
        d.b_code = next_double();
        d.b_test = next_double();
        d.b_q = next_double();
        d.b_rho = next_double();
        d.b_sigma = next_double();
        d.sigma0_exact = next_double();
        d.b_pair = next_double();
        d.gap = next_double();
        d.tau_s = next_double();
        d.cos_min = next_double();
        d.kappa_i = next_array(11);
        d.qdiag = next_array(11);
        d.weakest_exact = next_array(11);
        d.rho_exact = next_array(55);
        d.breakdown_index = std::stoi(next());
        d.breakdown_pivot = next_double();
        out.push_back(std::move(d));
    }
    return out;
}

double committed_constant(const std::string& text, const std::string& name) {
    const std::size_t at = text.find(" " + name + " = ");
    REQUIRE(at != std::string::npos);
    const std::size_t from = at + name.size() + 4;
    const std::size_t to = text.find(';', from);
    return number_of(text.substr(from, to - from));
}

// ---- the second method: the normal equations and their solution in 600-digit decimals, exact for these data and solved with pivoting

Decimal dec_token(const std::string& s) { return Decimal::from_string(s); }

// the design of a dataset in exact decimals: the recipe of SPEC-estimation 8.2 written again
std::vector<Decimal> decimal_design_row(const std::string& name, const Strings& tokens) {
    const Decimal x = tokens.size() > 1 ? dec_token(tokens[1]) : Decimal(0);
    if (name == "NoInt1" || name == "NoInt2") return {x};
    if (name == "Norris") return {Decimal(1), x};
    if (name == "Pontius") return {Decimal(1), x, x * x};
    if (name == "Longley") {
        std::vector<Decimal> row = {Decimal(1)};
        for (std::size_t i = 1; i < tokens.size(); ++i) row.push_back(dec_token(tokens[i]));
        return row;
    }
    std::vector<Decimal> row = {Decimal(1)};
    Decimal p(1);
    for (int k = 0; k < (name == "Filip" ? 10 : 5); ++k) {
        p = p * x;
        row.push_back(p);
    }
    return row;
}

std::vector<Decimal> solve_decimal(std::vector<std::vector<Decimal>> a, std::vector<Decimal> b) {
    const std::size_t n = a.size();
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t piv = i;
        for (std::size_t r = i + 1; r < n; ++r) {
            if (a[r][i].copy_abs() > a[piv][i].copy_abs()) piv = r;
        }
        std::swap(a[i], a[piv]);
        std::swap(b[i], b[piv]);
        for (std::size_t r = i + 1; r < n; ++r) {
            const Decimal f = a[r][i] / a[i][i];
            for (std::size_t c = i; c < n; ++c) a[r][c] = a[r][c] - f * a[i][c];
            b[r] = b[r] - f * b[i];
        }
    }
    std::vector<Decimal> x(n);
    for (std::size_t ii = n; ii-- > 0;) {
        Decimal s = b[ii];
        for (std::size_t c = ii + 1; c < n; ++c) s = s - a[ii][c] * x[c];
        x[ii] = s / a[ii][ii];
    }
    return x;
}

// min over the first `count` parameters of -log10(|x_i - c_i| / |c_i|), as the Python's generator computes it, from the solution of the normal equations of the given rows (y is column 0 of each token row,
// `reduced` subtracts the eliminated columns 6 .. 10 at their certified values first)
double second_method_digits(const std::string& name, const es::NistFile& file, bool reduced) {
    LocalContext ctx(600);
    const std::size_t count = reduced ? 6 : decimal_design_row(name, file.rows.at(0)).size();
    std::vector<std::vector<Decimal>> a(count, std::vector<Decimal>(count, Decimal(0)));
    std::vector<Decimal> b(count, Decimal(0));
    for (const Strings& tokens : file.rows) {
        const std::vector<Decimal> row = decimal_design_row(name, tokens);
        Decimal y = dec_token(tokens[0]);
        if (reduced) {
            for (std::size_t e = 6; e <= 10; ++e) y = y - row.at(e) * dec_token(file.cert.at(e));
        }
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = 0; j < count; ++j) a[i][j] = a[i][j] + row[i] * row[j];
            b[i] = b[i] + row[i] * y;
        }
    }
    const std::vector<Decimal> x = solve_decimal(a, b);
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < count; ++i) {
        const Decimal c = dec_token(file.cert.at(i));
        LocalContext narrow(90);
        const Decimal xi = +x[i];   // the 90-digit decimal of the exact solution, which is what the Python's generator compares (Wampler1 .. 5: their solution IS 1, exactly: 15.0)
        if (xi == c) {
            best = std::min(best, 15.0);
            continue;
        }
        const Decimal rel = (xi - c).copy_abs() / c.copy_abs();
        best = std::min(best, (-rel.log10()).to_double());
    }
    return best;
}

// the machine's own recipe for the double rows of a dataset (x87 long double): strtold, products in long double, one rounding to double; the SECOND METHOD of the design
es::DMatrix hardware_design(const std::string& name, const es::NistFile& file) {
    es::DMatrix out;
    for (const Strings& tokens : file.rows) {
        std::vector<long double> v;
        for (const std::string& t : tokens) v.push_back(std::strtold(t.c_str(), nullptr));
        std::vector<double> row;
        if (name == "NoInt1" || name == "NoInt2") {
            row = {static_cast<double>(v[1])};
        } else if (name == "Norris") {
            row = {1.0, static_cast<double>(v[1])};
        } else if (name == "Pontius") {
            row = {1.0, static_cast<double>(v[1]), static_cast<double>(v[1] * v[1])};
        } else if (name == "Longley") {
            row = {1.0};
            for (std::size_t i = 1; i < v.size(); ++i) row.push_back(static_cast<double>(v[i]));
        } else {
            long double p = 1.0L;
            row = {1.0};
            for (int k = 0; k < (name == "Filip" ? 10 : 5); ++k) {
                p = p * v[1];
                row.push_back(static_cast<double>(p));
            }
        }
        out.push_back(std::move(row));
    }
    return out;
}

}  // namespace

TEST_CASE("the real NIST files: the counts of the headers, the certified strings, the data -- read by hand from the files", "[estimation_sizing][real_tree]") {
    const fs::path cache = es::default_settings().cache;
    for (const es::NistEntry& entry : es::kNist) {
        const es::NistFile f = es::read_nist(cache, entry.name);
        INFO(entry.name);
        CHECK(f.rows.size() == static_cast<std::size_t>(entry.observations));
        CHECK(f.cert.size() == static_cast<std::size_t>(entry.parameters));
        CHECK(f.sd.size() == f.cert.size());
        CHECK(!f.rsd.empty());
        const es::DoubleDesign d = es::design_double(entry.name, f.rows);
        REQUIRE(!d.x.empty());
        for (const auto& row : d.x) CHECK(row.size() == static_cast<std::size_t>(entry.parameters));
        CHECK(d.x.size() == static_cast<std::size_t>(entry.observations));
    }
    // Norris, as the file prints it (lines 31 to 46 and 61 to 96 of Norris.dat)
    const es::NistFile norris = es::read_nist(cache, "Norris");
    CHECK(norris.cert == Strings{"-0.262323073774029", "1.00211681802045"});
    CHECK(norris.sd == Strings{"0.232818234301152", "0.429796848199937E-03"});
    CHECK(norris.rsd == "0.884796396144373");
    REQUIRE(norris.rows.size() == 36U);
    for (const Strings& row : norris.rows) CHECK(row.size() == 2U);
    // the names of the files, in the order the header and the table list them
    const char* const names[] = {"Norris", "Pontius", "NoInt1", "NoInt2", "Filip", "Longley", "Wampler1", "Wampler2", "Wampler3", "Wampler4", "Wampler5"};
    REQUIRE(es::kNistCount == 11U);
    for (std::size_t i = 0; i < 11; ++i) CHECK(std::string(es::kNist[i].name) == names[i]);
}

TEST_CASE("the real report has the shape of the table: the eleven datasets in order, the six tridiagonal problems, the hatch of Filip's last five", "[estimation_sizing][real_tree]") {
    const es::Report& rep = real_report();
    REQUIRE(rep.datasets.size() == 11U);
    for (std::size_t i = 0; i < 11; ++i) {
        CHECK(rep.datasets[i].name == es::kNist[i].name);
        CHECK(rep.datasets[i].n == es::kNist[i].parameters);
        CHECK(rep.datasets[i].m == es::kNist[i].observations);
        CHECK(rep.datasets[i].e.size() == static_cast<std::size_t>(es::kNist[i].parameters));
        CHECK(rep.datasets[i].kappa_i.size() == static_cast<std::size_t>(es::kNist[i].parameters));
        CHECK(rep.datasets[i].rho_exact.size() == static_cast<std::size_t>(es::kNist[i].parameters * (es::kNist[i].parameters - 1) / 2));
        CHECK(rep.datasets[i].weakest_exact.size() == static_cast<std::size_t>(es::kNist[i].parameters));
    }
    REQUIRE(rep.tridiagonal.size() == 6U);
    const int sizes[] = {1, 2, 5, 10, 20, 40};
    for (std::size_t i = 0; i < 6; ++i) CHECK(rep.tridiagonal[i].n == sizes[i]);
    CHECK(rep.hatch.free_indices == std::vector<int>{0, 1, 2, 3, 4, 5});
    CHECK(rep.hatch.eliminated == std::vector<int>{6, 7, 8, 9, 10});
    CHECK(rep.hatch.n_free == 6);
    CHECK(rep.hatch.dof == 82 - 6);
    CHECK(rep.hatch.checksum_reduced == rep.datasets[4].checksum);   // the full Filip design, as the dataset's own
    CHECK(rep.u == es::kU);
    CHECK(rep.eta_rep == es::kEtaRep);
    CHECK(rep.cert_relative == es::kCertRelative);
    // Filip alone has a Cholesky breakdown among the eleven; only it has a bound that is not below 0.5 ... and B(code) < 0.5 for the ten served
    for (const es::DatasetFigures& d : rep.datasets) {
        INFO(d.name);
        CHECK(d.breakdown_index.has_value() == (d.name == "Filip"));
        CHECK(d.certified_no_digit == (d.name == "Filip"));
    }
}

TEST_CASE("T1 the generated header is, byte for byte, the committed modules/estimation/tests/registered_figures.hpp, and the specification's block is the generated table", "[estimation_sizing][real_tree]") {
    const es::Report& rep = real_report();
    const std::string header = es::build_header(rep);
    const std::string committed = committed_header_text();
    CHECK(header.size() == committed.size());
    CHECK(header == committed);
    CHECK(lines_of(header).size() == 169U);
    // T2: the block of the specification
    const std::string table = es::build_spec_table(rep);
    const std::string block = committed_spec_block();
    CHECK(table.size() == block.size());
    CHECK(table == block);
    CHECK(lines_of(table).size() == 38U);
}

TEST_CASE("the exact solution and the exact inverse of every real dataset, by their postconditions (control K2): N x == b and N_s Q_s == I, exactly", "[estimation_sizing][real_tree]") {
    const fs::path cache = es::default_settings().cache;
    const es::Report& rep = real_report();
    REQUIRE(rep.datasets.size() == 11U);
    for (std::size_t d = 0; d < 11; ++d) {
        const char* name = es::kNist[d].name;
        INFO(name);
        const es::NistFile f = es::read_nist(cache, name);
        const es::ExactDesign design = es::design_exact(name, f.rows);
        const std::size_t n = design.x[0].size();
        es::RMatrix N(n, std::vector<Rational>(n));
        std::vector<Rational> b(n);
        for (std::size_t k = 0; k < design.x.size(); ++k) {
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = 0; j < n; ++j) N[i][j] += design.x[k][i] * design.x[k][j];
                b[i] += design.x[k][i] * design.y[k];
            }
        }
        const std::vector<Rational> x = es::exact_solve(N, b);
        CHECK(times(N, x) == b);
        // the scaled matrix with the exponents of the report, and its inverse
        es::RMatrix Ns(n, std::vector<Rational>(n));
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) Ns[i][j] = N[i][j] * pow2r(-(rep.datasets[d].e[i] + rep.datasets[d].e[j]));
        }
        const es::RMatrix Q = es::exact_inverse(Ns);
        CHECK(times(Ns, Q) == identity(n));
        CHECK(times(Q, Ns) == identity(n));
    }
}

TEST_CASE("the rows of every real dataset are, bit for bit, the machine's own long double recipe (the emulation against x87 on the real data)", "[estimation_sizing][real_tree]") {
    if (!kHostHasX87) {
        SKIP("long double is not the x87 extended precision on this host");
    }
    const fs::path cache = es::default_settings().cache;
    for (const es::NistEntry& entry : es::kNist) {
        INFO(entry.name);
        const es::NistFile f = es::read_nist(cache, entry.name);
        const es::DoubleDesign d = es::design_double(entry.name, f.rows);
        const es::DMatrix hardware = hardware_design(entry.name, f);
        REQUIRE(d.x.size() == hardware.size());
        std::size_t differing = 0;
        for (std::size_t k = 0; k < hardware.size(); ++k) {
            REQUIRE(d.x[k].size() == hardware[k].size());
            for (std::size_t i = 0; i < hardware[k].size(); ++i) differing += bits_of(d.x[k][i]) != bits_of(hardware[k][i]) ? 1U : 0U;
            if (bits_of(d.y.at(k)) != bits_of(static_cast<double>(std::strtold(f.rows.at(k).at(0).c_str(), nullptr)))) ++differing;
        }
        CHECK(differing == 0U);
    }
}

TEST_CASE("T4 the default report on the real data: every printed figure is the committed record's (the header's hexadecimal floats) in the Python's layout; the columns the header does not hold are from a second method", "[estimation_sizing][real_tree]") {
    const es::Report& rep = real_report();
    const std::string header = committed_header_text();
    const std::vector<CommittedDataset> committed = committed_datasets(header);
    REQUIRE(committed.size() == 11U);
    REQUIRE(rep.datasets.size() == 11U);
    const fs::path cache = es::default_settings().cache;
    std::ostringstream out;
    es::print_report(rep, out);
    const Strings l = lines_of(out.str());
    REQUIRE(l.size() == 46U);
    const double u = committed_constant(header, "kUnitRoundoff");
    const double eta = committed_constant(header, "kEtaRep");
    const double cert = committed_constant(header, "kCertifiedRelative");
    CHECK(l[0] == "u = 2^-53 = " + py_format_e(u, 4) + "; eta_rep = " + py_format_e(eta, 4) + "; certified values' own uncertainty = " + py_format_e(cert, 0));
    CHECK(l[1] == "");
    CHECK(l[2] == "dataset    n   m  kappa(N_s)  lam_min(A)  ||Q_s||_1   Theta_N   B (code)   B (test)       B_Q   B_sigma  cert.digits  refusal");
    const auto e_fmt = [](double v, std::size_t width, int precision) { return pad_left(py_format_e(v, precision), width); };
    for (std::size_t i = 0; i < 11; ++i) {
        const CommittedDataset& c = committed[i];
        INFO(c.name);
        const double digits = second_method_digits(c.name, es::read_nist(cache, c.name), false);
        std::string ref = "-";
        if (c.breakdown_index >= 0) ref = "EST-F-202 at B" + std::to_string(c.breakdown_index) + " (pivot " + py_format_e(c.breakdown_pivot, 3) + ")";
        else if (c.b_code >= 0.5) ref = "EST-F-003";
        CHECK(l[3 + i] == pad_right(c.name, 9) + " " + pad_left(std::to_string(c.n), 2) + " " + pad_left(std::to_string(c.m), 3) + " " + e_fmt(c.kappa, 11, 4) + " " + e_fmt(c.lam_min_a, 11, 4) + " " +
                              e_fmt(c.q1, 10, 3) + " " + e_fmt(c.theta_n_code, 9, 2) + " " + e_fmt(c.b_code, 10, 3) + " " + e_fmt(c.b_test, 10, 3) + " " + e_fmt(c.b_q, 9, 2) + " " + e_fmt(c.b_sigma, 9, 2) + "  " +
                              pad_left(py_format_f(digits, 1), 6) + "      " + ref);
        // the digits themselves, not only their one decimal place
        CHECK(rep.datasets[i].digits_exact_vs_cert == digits);
        // the scale exponents and kappa_i lines
        std::string e_text = "[";
        std::string k_text = "[";
        for (int k = 0; k < c.n; ++k) {
            e_text += (k == 0 ? "" : ", ") + std::to_string(c.e[static_cast<std::size_t>(k)]);
            k_text += (k == 0 ? "'" : ", '") + py_format_g(c.kappa_i[static_cast<std::size_t>(k)], 4) + "'";
        }
        CHECK(l[16 + i] == "  " + pad_right(c.name, 9) + " e = " + e_text + "]   kappa_i = " + k_text + "]");
    }
    CHECK(l[15] == "scale exponents e (EST-R-201) and per-parameter kappa_i = N_ii (N^-1)_ii:");
    CHECK(l[14] == "");
    CHECK(l[27] == "");
    CHECK(l[28] == "rule U (the unscaled decision of EST-A-209): ratio of the smallest to the largest Cholesky pivot of the UNSCALED N:");
    // rule U: the ratios are the record's; the pivots of the unscaled Pontius case are the plain-double Cholesky of the machine's own rows, written again
    {
        const es::NistFile pontius = es::read_nist(cache, "Pontius");
        const PlainRuleU plain = plain_rule_u(hardware_design("Pontius", pontius));
        REQUIRE(!plain.broke);
        std::string pivots = "[";
        for (std::size_t i = 0; i < plain.pivots.size(); ++i) pivots += (i == 0 ? "" : ", ") + py_float_repr(plain.pivots[i]);
        const double ratio = committed_constant(header, "kRuleUPontius");
        CHECK(*std::min_element(plain.pivots.begin(), plain.pivots.end()) / *std::max_element(plain.pivots.begin(), plain.pivots.end()) == ratio);
        CHECK(l[29] == "  Pontius                  {'ratio': " + py_float_repr(ratio) + ", 'pivots': " + pivots + "], 'breakdown': None}");
        CHECK(l[30] == "  Pontius_equilibrated     {'ratio': " + py_float_repr(committed_constant(header, "kRuleUPontiusEquilibrated")) + ", 'breakdown': None}");
    }
    // the hatch
    {
        const Strings t = initializer_tokens(header, "inline constexpr HatchFigures kHatchFilip = {");
        REQUIRE(t.size() == 25U);
        std::string e_text = "[";
        for (std::size_t i = 0; i < static_cast<std::size_t>(std::stoi(t[0])); ++i) e_text += (i == 0 ? "" : ", ") + t[2 + i];
        e_text += "]";
        const double digits = second_method_digits("Filip", es::read_nist(cache, "Filip"), true);
        CHECK(rep.hatch.digits_exact_vs_cert == digits);
        CHECK(std::stoi(t[1]) == 76);
        CHECK(l[32] == "the hatch (Filip, B6 .. B10 eliminated at the certified values): e = " + e_text + ", breakdown = None, kappa(N_s) = " + py_format_e(number_of(t[14]), 4) + ", ||Q_s||_1 = " +
                           py_format_e(number_of(t[16]), 3) + ", B(code) = " + py_format_e(number_of(t[22]), 3) + ", B(test) = " + py_format_e(number_of(t[23]), 3) + ", dof = " + t[1] +
                           ", exact reduced solution vs certified: " + py_format_f(digits, 1) + " digits");
    }
    // Demmel, the synthetic problems and the tridiagonal ones, from the record
    {
        const Strings t = initializer_tokens(header, "inline constexpr DemmelFigures kDemmel = {");
        REQUIRE(t.size() == 18U);
        CHECK(l[34] == "Demmel's example: e = [" + t[0] + ", " + t[1] + ", " + t[2] + ", " + t[3] + "], kappa(A) = " + py_format_f(number_of(t[8]), 4) + " (lambda_max = " + py_format_f(number_of(t[10]), 4) + "), kappa(H) = " +
                           py_format_e(number_of(t[11]), 4) + ", kappa(N_s) = " + py_format_f(number_of(t[12]), 4) + ", ||Q_s||_1 = " + py_format_e(number_of(t[13]), 4) + ", Theta_N = " +
                           py_format_e(number_of(t[14]), 3) + ", B = " + py_format_e(number_of(t[15]), 3) + " (with 4 ||Q_s||_1) and " + py_format_e(number_of(t[16]), 3) + " (with ||A^-1||_2), published bound " +
                           py_format_e(number_of(t[17]), 1));
        CHECK(l[36] == "SYN-dependent: e = [1, 1, 1], breakdown at parameter index " + std::to_string(static_cast<int>(committed_constant(header, "kSynDependentBreakdownIndex"))) + " with pivot " +
                           py_float_repr(committed_constant(header, "kSynDependentBreakdownPivot")) + ", dependency c ~ [1.0, 0.0] (a, b)");
        CHECK(l[37] == "SYN-nearly-dependent: e = [1, 1], kappa(N_s) = " + py_format_e(committed_constant(header, "kSynNearKappa"), 4) + ", lam_min(N_s) = " + py_format_e(committed_constant(header, "kSynNearLamMin"), 4) +
                           ", ||Q_s||_1 = " + py_format_e(committed_constant(header, "kSynNearQ1"), 4) + ", Theta_N = " + py_format_e(rep.syn_nearly_dependent.theta_n, 3) + ", B = " +
                           py_format_f(committed_constant(header, "kSynNearB"), 3));
        const Strings tr = initializer_tokens(header, "inline constexpr TridiagonalFigures kTridiagonal[] = {");
        REQUIRE(tr.size() == 24U);
        CHECK(l[39] == "tridiagonal (rows e_k - e_{k-1}): n, Theta_s, lambda_min(A), B_col = Theta_s / lambda_min(A):");
        for (std::size_t i = 0; i < 6; ++i) {
            CHECK(l[40 + i] == "  n = " + pad_left(tr[4 * i], 3) + "  Theta_s = " + py_format_e(number_of(tr[4 * i + 1]), 3) + "  lam_min(A) = " + py_format_e(number_of(tr[4 * i + 2]), 4) + "  B_col = " +
                               py_format_e(number_of(tr[4 * i + 3]), 3));
        }
    }
    CHECK(l[31] == "");
    CHECK(l[33] == "");
    CHECK(l[35] == "");
    CHECK(l[38] == "");
}

TEST_CASE("control K4: on the real Pontius data the plain and the compensated sums of a column of squares differ, the committed figure is the compensated one, and the plain one gives another (the failure of GitHub's run 37485373306)", "[estimation_sizing][real_tree]") {
    const fs::path cache = es::default_settings().cache;
    const std::string header = committed_header_text();
    const es::NistFile pontius = es::read_nist(cache, "Pontius");
    const es::DoubleDesign d = es::design_double("Pontius", pontius.rows);
    REQUIRE(!d.x.empty());
    const std::size_t n = d.x[0].size();
    std::vector<double> compensated_norms;
    std::vector<double> plain_norms;
    std::size_t columns_that_differ = 0;
    for (std::size_t j = 0; j < n; ++j) {
        std::vector<double> squares;
        for (const auto& row : d.x) squares.push_back((es::Ld(row[j]) * es::Ld(row[j])).to_double());
        double plain = 0.0;
        for (const double s : squares) plain += s;
        const double compensated = py_sum(squares);
        columns_that_differ += bits_of(plain) != bits_of(compensated) ? 1U : 0U;
        plain_norms.push_back(std::sqrt(plain));
        compensated_norms.push_back(std::sqrt(compensated));
    }
    // PREDICTION P-E8 (registered before the code was written): at least one column's sums differ in the last bit
    CHECK(columns_that_differ >= 1U);
    const auto ratio_with = [&](const std::vector<double>& norms) {
        es::DMatrix x;
        for (const auto& row : d.x) {
            std::vector<double> scaled_row;
            for (std::size_t j = 0; j < n; ++j) scaled_row.push_back(row[j] / norms[j]);
            x.push_back(std::move(scaled_row));
        }
        const PlainRuleU r = plain_rule_u(x);
        REQUIRE(!r.broke);
        return *std::min_element(r.pivots.begin(), r.pivots.end()) / *std::max_element(r.pivots.begin(), r.pivots.end());
    };
    const double committed = committed_constant(header, "kRuleUPontiusEquilibrated");
    const double with_compensated = ratio_with(compensated_norms);
    const double with_plain = ratio_with(plain_norms);
    CHECK(with_compensated == committed);
    CHECK(with_plain != committed);
    // and the tool's own equilibrated rows are the compensated ones
    const es::EquilibratedRows eq = es::equilibrated_pontius_rows(cache);
    REQUIRE(eq.norms.size() == n);
    for (std::size_t j = 0; j < n; ++j) CHECK(bits_of(eq.norms[j]) == bits_of(compensated_norms[j]));
}

TEST_CASE("the tool, whole, on the real cache: the default report is the report, and the mode outputs are the committed files'", "[estimation_sizing][real_tree]") {
    std::ostringstream expected;
    es::print_report(real_report(), expected);
    std::ostringstream out;
    std::ostringstream err;
    const int code = es::run({}, Streams{out, err});
    CHECK(code == 0);
    CHECK(err.str().empty());
    CHECK(out.str() == expected.str());
    // --verify and --verify-spec on the committed files: ok, exit 0, the message on the error stream
    const Result v = run_with(es::default_settings(), {"--verify", (tree_root() / "modules" / "estimation" / "tests" / "registered_figures.hpp").string()});
    CHECK(v.code == 0);
    CHECK(v.err == "ok       the committed header is exactly what the generator emits\n");
    const Result s = run_with(es::default_settings(), {"--verify-spec", (tree_root() / "spec" / "SPEC-estimation.md").string()});
    CHECK(s.code == 0);
    CHECK(s.err == "ok       the specification's generated block is exactly what the generator emits\n");
}
}  // namespace
