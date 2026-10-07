// tests/devtools/unitcheck_tests.cpp — the register of every factor of a thousand in production code (SPEC-dynamics DYN-R-040): its three scanners against the semantics of the regular
// expressions they replace, what it prints, what it refuses, which files it reads, on synthetic trees; and the real tree, recounted another way, with defects injected into a copy of the real
// sources.  (ctests `unitcheck.behaviour` and `unitcheck.real_tree`; the Python tool had no test of its own, so these are the cases that show it CAN fail.)
//
// Every expectation is DERIVED BY HAND from the patterns and the print statements of unitcheck.py, not taken from running the port.  Where the port deliberately differs (the path RELATIVE to the
// root is tested for `build` and `tests`; no source to search is refused with exit 2) the case says so.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/tool.hpp>

#include "throwing_stream.hpp"
#include "unitcheck.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
namespace uc = odl::tools::unitcheck;

namespace {

constexpr int kOk = 0, kFailed = 1, kArgument = 2;
constexpr const char* kHome = "modules/core/include/odl/core/units.hpp";

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = uc::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// `{:{w}d}` and `{:<{w}s}` of Python, for text that is ASCII
std::string right(std::size_t n, std::size_t width) {
    const std::string s = std::to_string(n);
    return std::string(width - std::min(width, s.size()), ' ') + s;
}
std::string left(const std::string& s, std::size_t width) { return s + std::string(width - std::min(width, s.size()), ' '); }

struct Counts {
    std::size_t sources = 0, home = 0, crossings = 0, declared = 0, unaccounted = 0, misplaced = 0, suffixed = 0;
};

// the eight lines of the denominator, from the print statements: the labels are 34, 35 and (five times) 51 characters wide before the number, the number is 5 wide; the last line is group C7's
std::string denominator(const Counts& c) {
    const std::size_t n = c.home + c.crossings + c.declared + c.unaccounted + c.misplaced + c.suffixed;
    return "\n" + left("  production sources searched", 34) + right(c.sources, 5) + "\n" + "  literals of value 1000 or 1/1000 " + right(n, 5) + "\n" + "    in " + left(kHome, 44) + " " + right(c.home, 5) + "\n" +
           left("    annotated UNIT-CROSSING", 51) + right(c.crossings, 5) + "\n" + left("    annotated NOT-A-UNIT-CROSSING", 51) + right(c.declared, 5) + "\n" +
           left("    ACCOUNTED FOR BY NEITHER", 51) + right(c.unaccounted, 5) + "\n" + left("    naming km OUTSIDE core/units.hpp", 51) + right(c.misplaced, 5) + "\n" +
           left("    with a user-defined or library suffix (listed)", 51) + right(c.suffixed, 5) + "\n";
}

const char kVerdict[] = "\nok       every factor of a thousand in production code is accounted for\n";
const char kHeading[] = "THE REGISTER \xE2\x80\x94 every literal whose value is exactly 1000 or 0.001,\nin production sources (tests excluded).\n\n";

const char kMisplacedTrailer[] =
    "\n  Annotating it does not make it legal. SPEC-frames FRAME-R-062 and\n"
    "  SPEC-dynamics DYN-R-010 give the km<->m crossing ONE site, and this is not it:\n"
    "  call metres_from_km / km_from_metres, or state_accel_km_s2_from_m_s2 and\n"
    "  field_position_m_from_state_km if what you are crossing is a state quantity.\n"
    "  One conversion site becomes six, and five of them are somebody's afternoon.\n";

const char kUnaccountedTrailer[] =
    "\n  Every factor of a thousand in production code is either the km<->m crossing\n"
    "  in core/units.hpp, or says what it is. Add ONE of:\n"
    "      // UNIT-CROSSING: <from> -> <to>\n"
    "      // NOT-A-UNIT-CROSSING: <what it is>\n"
    "  on that line or the one directly above it. If it IS a km<->m scaling, it does not belong here at all:\n"
    "  call core/units.hpp, which is what SPEC-frames FRAME-R-062 and SPEC-dynamics\n"
    "  DYN-R-010 require and what this gate exists to keep true.\n";

// a row of the register: the crossing's own (line number first) and the annotated ones (file and line first)
std::string home_row(std::size_t line, const std::string& literal, const std::string& text) { return "    " + right(line, 5) + "  " + left(literal, 8) + " " + text + "\n"; }
std::string annotated_row(const std::string& file, std::size_t line, const std::string& literal, const std::string& what) {
    return "    " + file + ":" + std::to_string(line) + "  " + left(literal, 8) + " " + what + "\n";
}
std::string unaccounted_row(const std::string& file, std::size_t line, const std::string& literal, const std::string& text) {
    return "  " + file + ":" + std::to_string(line) + "  [" + literal + "]  " + text + "\n";
}

struct Tree {
    TempDir td;
    fs::path root;
    // `below`: the tree's root is this directory below the temporary one (to put it under directories with telling names)
    explicit Tree(const std::string& below = "") : root(below.empty() ? td.path() : td.path() / below) { fs::create_directories(root); }
    void write(const std::string& relative, const std::string& content) {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, content);
    }
    [[nodiscard]] Result run(std::vector<std::string> extra = {}) const {
        std::vector<std::string> args = {"--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        return run_tool(args);
    }
};

std::string seen(std::string_view line) {
    const std::optional<uc::scan::Marker> m = uc::scan::marker(line);
    return m ? m->kind + "|" + m->what : std::string("none");
}

std::string tokens(std::string_view code) {
    std::string out;
    for (const std::string& t : uc::scan::number_tokens(code)) out += (out.empty() ? "" : "|") + t;
    return out;
}

}  // namespace

// ======================================================================================================================================== the scanners

TEST_CASE("marker: the name after a //, not glued to a word character, a colon, and the text without its trailing white space", "[unitcheck][behaviour][scan]") {
    CHECK(seen("x = 1000;  // UNIT-CROSSING: g/cm^3 -> kg/m^3") == "UNIT-CROSSING|g/cm^3 -> kg/m^3");
    CHECK(seen("// NOT-A-UNIT-CROSSING: a date radix") == "NOT-A-UNIT-CROSSING|a date radix");
    CHECK(seen("//UNIT-CROSSING: m -> mm") == "UNIT-CROSSING|m -> mm");                // a word boundary lies between the slash and the U
    CHECK(seen("/// UNIT-CROSSING: m -> mm") == "UNIT-CROSSING|m -> mm");
    CHECK(seen("// FOO-UNIT-CROSSING: x") == "UNIT-CROSSING|x");                        // a hyphen is no word character
    CHECK(seen("// UNIT-CROSSING : spaced colon") == "UNIT-CROSSING|spaced colon");
    CHECK(seen("// UNIT-CROSSING:tight") == "UNIT-CROSSING|tight");
    CHECK(seen("// UNIT-CROSSING:   padded   \t") == "UNIT-CROSSING|padded");
    CHECK(seen("// UNIT-CROSSING: a  b\t c") == "UNIT-CROSSING|a  b\t c");              // inside the text nothing is trimmed
    CHECK(seen("// UNIT-CROSSING: a: b") == "UNIT-CROSSING|a: b");
    CHECK(seen("// UNIT-CROSSING: x\xC2\xA0") == "UNIT-CROSSING|x");                     // NO-BREAK SPACE is white space to \s
    CHECK(seen("// UNIT-CROSSING: x \xE3\x80\x80") == "UNIT-CROSSING|x");                // IDEOGRAPHIC SPACE too
    CHECK(seen("// \xC2\xB7UNIT-CROSSING: dot") == "UNIT-CROSSING|dot");                 // a middle dot is no word character
    CHECK(seen("// UNIT-CROSSING: caf\xC3\xA9 -> m") == "UNIT-CROSSING|caf\xC3\xA9 -> m");
    // the leftmost name that is followed by a colon is the one read, and the text runs to the end of the line
    CHECK(seen("// UNIT-CROSSING: first // NOT-A-UNIT-CROSSING: second") == "UNIT-CROSSING|first // NOT-A-UNIT-CROSSING: second");
    CHECK(seen("// NOT-A-UNIT-CROSSING nothing here // UNIT-CROSSING: m -> mm") == "UNIT-CROSSING|m -> mm");
    CHECK(seen("a // b // UNIT-CROSSING: x") == "UNIT-CROSSING|x");
    CHECK(seen("// xNOT-A-UNIT-CROSSING: y") == "UNIT-CROSSING|y");                      // no boundary before the N, but one before the UNIT inside it
    // a blank is a text when nothing else follows the colon (the regular expression backtracks the white space to give `.+?` one character)
    CHECK(seen("// UNIT-CROSSING: ") == "UNIT-CROSSING| ");
    CHECK(seen("// UNIT-CROSSING:   ") == "UNIT-CROSSING| ");
    CHECK(seen("// UNIT-CROSSING:\t") == "UNIT-CROSSING|\t");
    CHECK(seen("// UNIT-CROSSING: \xC2\xA0") == "UNIT-CROSSING|\xC2\xA0");                      // the last blank is a whole code point, not its last byte
    CHECK(seen("// UNIT-CROSSING:\xE3\x80\x80\xE3\x80\x80") == "UNIT-CROSSING|\xE3\x80\x80");
    // white space of any script around the colon is white space to \s
    CHECK(seen("// UNIT-CROSSING\xC2\xA0: x") == "UNIT-CROSSING|x");
    CHECK(seen("// UNIT-CROSSING\xE3\x80\x80\xE3\x80\x80: x") == "UNIT-CROSSING|x");
    CHECK(seen("// UNIT-CROSSING:\xC2\xA0x") == "UNIT-CROSSING|x");
    CHECK(seen("// UNIT-CROSSING:\xE3\x80\x80\xC2\xA0y") == "UNIT-CROSSING|y");
    // no marker
    CHECK(seen("") == "none");
    CHECK(seen("UNIT-CROSSING: x") == "none");                                           // no //
    CHECK(seen("UNIT-CROSSING: x // y") == "none");                                      // the name comes before the //
    CHECK(seen("/* UNIT-CROSSING: x */") == "none");
    CHECK(seen("// UNIT-CROSSING:") == "none");                                          // nothing after the colon
    CHECK(seen("// UNIT-CROSSING") == "none");
    CHECK(seen("// UNIT-CROSSING x: y") == "none");
    CHECK(seen("// unit-crossing: x") == "none");                                        // the case counts
    CHECK(seen("// xUNIT-CROSSING: y") == "none");
    CHECK(seen("// _UNIT-CROSSING: y") == "none");
    CHECK(seen("// 9UNIT-CROSSING: y") == "none");
    CHECK(seen("// \xC3\xA9UNIT-CROSSING: y") == "none");                                // an accented letter is a word character
    CHECK(seen("// UNIT-CROSSINGx: y") == "none");
    CHECK(seen("// NOT-A-UNIT-CROSSINGx: y") == "none");
    CHECK(seen("// NOT-UNIT-CROSSING: y") == "UNIT-CROSSING|y");                         // only the two names count
}

TEST_CASE("number_tokens: the numbers of a line of code, as NUMBER's lookbehind, lookahead and backtracking leave them", "[unitcheck][behaviour][scan]") {
    CHECK(tokens("") == "");
    CHECK(tokens("no digits at all") == "");
    CHECK(tokens("x = 1000;") == "1000");
    CHECK(tokens("a = 1e3 + 1.0E+3 - 1000. * 0.001 / 1.0e-03") == "1e3|1.0E+3|1000.|0.001|1.0e-03");
    CHECK(tokens("1.e3 1000e5 0.1e4 01000") == "1.e3|1000e5|0.1e4|01000");
    CHECK(tokens("(1000) -1000 +1e3 a[1000] f(1000,1e3)") == "1000|1000|1e3|1000|1000|1e3");
    CHECK(tokens("1000 1000") == "1000|1000");
    CHECK(tokens("1,000") == "1|000");
    CHECK(tokens("9999999999999999999999 1000") == "9999999999999999999999|1000");
    // glued to a word character or a point on either side: no token (and no shorter one is carved out of it)
    CHECK(tokens("1000f") == "");                        // not C++ (an integer literal has no f suffix): a letter glued to the number, no token
    CHECK(tokens("1000.0f") == "1000.0f");               // CHANGED in group C6: a standard suffix is part of the literal (these were invisible to the Python's NUMBER)
    CHECK(tokens("1000u 1000UL 1e3f 1e3L") == "1000u|1000UL|1e3f|1e3L");
    CHECK(tokens("x1000") == "");
    CHECK(tokens("_1000") == "");
    CHECK(tokens("1000_") == "1000_");                   // CHANGED in group C7: an underscore begins a user-defined suffix (here the suffix `_`): a literal of the user's own, which the register lists
    CHECK(tokens("1_000") == "1_000");                   // CHANGED in group C7: the number 1 with the user-defined suffix `_000` (Python's separator is not C++'s): a token, whose value is 1
    CHECK(tokens("a.1000") == "");
    CHECK(tokens("1000.f") == "1000.f");                 // CHANGED in group C6, like 1000.0f
    CHECK(tokens("1000..") == "");
    CHECK(tokens("1000.0.0") == "");
    CHECK(tokens("1.5.3") == "");
    CHECK(tokens("1.0e3.5") == "");
    CHECK(tokens("1e") == "");
    CHECK(tokens("1e+") == "");
    CHECK(tokens("1e3e") == "");
    CHECK(tokens("0x3e8") == "0x3e8");                  // CHANGED in group C7: hexadecimal is read (the Python's NUMBER found the 3 glued to the x and refused it)
    CHECK(tokens(".5") == "");
    CHECK(tokens("1000.5") == "1000.5");
    // the exponent is taken when it can be, and given back when what follows it makes the token invalid
    CHECK(tokens("5e3") == "5e3");
    CHECK(tokens("5e3 x") == "5e3");
    // digits of any script are digits to \d, and a letter, a digit or a superscript of any script is a word character to \w
    CHECK(tokens("\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0") == "\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0");      // ARABIC-INDIC DIGITS one zero zero zero
    CHECK(tokens("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90") == "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90");   // FULLWIDTH DIGITS
    CHECK(tokens("\xC3\xA9" "1000") == "");                                                     // e acute before
    CHECK(tokens("1000" "\xC3\xA9") == "");                                                     // e acute after
    CHECK(tokens("1000" "\xC2\xB2") == "");                                                     // SUPERSCRIPT TWO is alphanumeric to Python
    CHECK(tokens("\xC2\xB7" "1000") == "1000");                                                 // MIDDLE DOT is not
    CHECK(tokens("2\xC3\x97" "1000") == "2|1000");                                              // MULTIPLICATION SIGN is not
    CHECK(tokens("\xFF" "1000") == "1000");                                                    // a byte that is not UTF-8 is no word and no point
    CHECK(tokens("1000" "\xFF") == "1000");
}

TEST_CASE("number_tokens: the standard suffixes of C++ are part of the literal and no other letters are (group C6)", "[unitcheck][behaviour][scan]") {
    // after an INTEGER literal: u, l, ul, lu, ll, ull, llu in either case (`ll` and `LL` keep their case)
    for (const char* literal : {"1000u", "1000U", "1000l", "1000L", "1000ul", "1000UL", "1000uL", "1000Ul", "1000lu", "1000LU", "1000lU", "1000Lu", "1000ll", "1000LL", "1000ull", "1000ULL", "1000uLL",
                                "1000Ull", "1000llu", "1000LLU", "1000llU", "1000LLu", "1'000u"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // after a FLOATING one (a point or an exponent): f, F, l, L
    for (const char* literal : {"1000.0f", "1000.0F", "1000.0l", "1000.0L", "1000.f", "1000.F", "1000.L", "1e3f", "1e3F", "1e3l", "1e3L", "1.0e+3f", "1.0E-3F", "0.001f", "1.e3f", "1000.0e0L"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // anything else leaves the number glued to a word character, which refuses the token: a suffix of the other kind (not C++), `lL` (not a suffix), a C++23 one, and the letters of an exponent that is not one.
    // (The user-defined and library suffixes -- `1000_km`, the chrono `1000ms` -- were here until group C7, which reads them as a category of their own: the next case.)
    for (const char* text : {"1000f", "1000F", "1000.0u", "1000.0U", "1000.0ul", "1e3u", "1e3ll", "1000lL", "1000Ll", "1000uu", "1000ulu", "1000lul", "1000ullu", "1000fl", "1000.0ff", "1000.0fl", "1000z", "1000uz",
                             "1000f16", "1000.0f32", "1000e", "1000ee3", "1000ma", "1000.0fx"}) {
        INFO(text);
        CHECK(tokens(text) == "");
    }
    // the lookahead still holds after a suffix, and so does the lookbehind before the literal
    CHECK(tokens("1000u.") == "");
    CHECK(tokens("1000.0f.5") == "");
    CHECK(tokens("1000ul_") == "");
    CHECK(tokens("1000.0fx") == "");
    CHECK(tokens("x1000u") == "");
    CHECK(tokens("a.1000.0f") == "");
    // in a line of code, with other literals around
    CHECK(tokens("float a = 1000.0f * 2u + 1e3L;") == "1000.0f|2u|1e3L");
    CHECK(tokens("f(1000.0f,1000u)") == "1000.0f|1000u");
    CHECK(tokens("v[0] = 1000UL; v[1] = 10ull;") == "0|1000UL|1|10ull");
}

TEST_CASE("number_tokens: digit separators are part of the number when they stand between two digits (group C6)", "[unitcheck][behaviour][scan]") {
    for (const char* literal : {"1'000", "1'000.0", "1'000.", "0.0'01", "1'0'0'0", "1'000'000", "1'000.000'1", "1e1'0", "1'0e2", "1.0e+0'3", "1'000u", "1'000.0f", "1'0e-1'0L", "00'1.0e-3"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // an apostrophe that does not stand between two digits is not a separator: the number ends before it
    CHECK(tokens("1''000") == "1|000");
    CHECK(tokens("1'") == "1");
    CHECK(tokens("1'x") == "1");
    CHECK(tokens("1000'") == "1000");
    CHECK(tokens("'1000") == "1000");
    CHECK(tokens("1'.5") == "1");                      // the 5 follows a point: no token begins there
    CHECK(tokens("1.'5") == "1.|5");                   // a point then an apostrophe: `1.` is a literal, and the 5 after the apostrophe (which is not between digits) is a number of its own
    CHECK(tokens("1e'3") == "3");                      // an exponent needs a digit, so the e stays glued to the 1 (no token there); the 3 after the apostrophe is a number of its own
    // character literals of digits are their own numbers, as they always were
    CHECK(tokens("c == '1'") == "1");
    CHECK(tokens("'0','1'") == "0|1");
    // a letter before the first digit refuses the token there, and the digits after the apostrophe begin a token of their own (the lookbehind allows an apostrophe), as in the old regular expression
    CHECK(tokens("x1'000") == "000");
    // in a line of code
    CHECK(tokens("long d = 1'000, e = 1'000'000 / 1'000;") == "1'000|1'000'000|1'000");
    // glued to a word character or a point after it: still no token
    CHECK(tokens("1'000x") == "");
    CHECK(tokens("1'000.0.0") == "");
}

TEST_CASE("number_tokens: hexadecimal, binary, octal and hexadecimal floating literals are numbers as C++ reads them, with the same separators and suffixes (group C7)", "[unitcheck][behaviour][scan]") {
    // integers of the three other radixes, with an integer suffix and digit separators
    for (const char* literal : {"0x3E8", "0X3e8", "0x3E8u", "0x3E8UL", "0x3'E8", "0xFFFF'FFFF", "0x0", "0xdeadBEEF", "0x1f", "0x1ul", "0xfu", "0xEL", "0b1111101000", "0B1111101000", "0b11'1110'1000", "0b1u", "0b1ull",
                                "01750", "0", "00", "01'750", "0777L", "0'7"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // hexadecimal floating literals: ONE number, whose exponent is not a number of its own
    for (const char* literal : {"0x1p-1000", "0x1p+1000", "0x1.F4p9", "0x.8p1", "0x1.p3", "0X1P3", "0x1.8p1f", "0x1p1L", "0x1'0p1'0", "0xA.Bp-3", "0x1p0"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // leading zeros of a FLOATING literal are decimal
    for (const char* literal : {"089.5", "08e1", "00.5", "09.", "01000.0", "01e3"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // the exponent of a hexadecimal floating literal is not the number 1000 after a sign: the misreading of the Python's NUMBER that the register carried (normal_equations.cpp's two rows)
    CHECK(tokens("constexpr double lo = 0x1p-1000, hi = 0x1p+1000;") == "0x1p-1000|0x1p+1000");
    CHECK(tokens("0x3e8 + 0b101 + 0777") == "0x3e8|0b101|0777");
    // what is no literal: a prefix without digits, a hexadecimal floating literal without its exponent, a digit out of the radix, glued letters and points
    for (const char* text : {"0x", "0X", "0x.", "0xg", "0xG1", "0x1g", "0x1.8", "0x1.8f", "0x1p", "0x1p+", "0x1p-x", "0b", "0B", "0b2", "0b12", "0b1012", "089", "078", "0x10.5", "0x1p3.5", "0x10x", "x0x10", "a.0x10",
                             "00x10", "0xx", "5x10", "1x1", "9b1", "7B101", "1X2"}) {
        INFO(text);
        CHECK(tokens(text) == "");
    }
    CHECK(tokens("0'8") == "8");   // 8 is no octal digit, so no literal begins at the 0; the 8 after the apostrophe is a number of its own, as after any apostrophe
    // a hexadecimal digit is never a suffix, and a letter that is not one begins one: `0x1f` is 31 and `0x1ul` is one, `0x1fl` is 0x1f with the suffix l
    CHECK(tokens("0x1f 0x1fl 0x1ful") == "0x1f|0x1fl|0x1ful");
    // an integer or a floating literal starting with 0 followed by x or b but not a prefix: `0b` needs a binary digit, and `0e3`, `0.5` are decimal
    CHECK(tokens("0e3 0.5 0f") == "0e3|0.5");   // `0f` is no literal: f is not a suffix of an integer
}

TEST_CASE("number_tokens: a user-defined or library suffix is part of the literal, and only those (group C7)", "[unitcheck][behaviour][scan]") {
    // a user-defined suffix begins with an underscore
    for (const char* literal : {"1000_km", "1000.0_km", "1e3_m", "0x3E8_km", "0b1111101000_km", "01750_km", "1'000_km", "0.001_m", "1000_", "1_000", "5_km", "1000_km_", "1000_Km2", "1.5e-3_s", "0x1p3_x", "0b1_", "0x1p3_"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // the library's: exactly h, min, s, ms, us, ns, d, y, i, il, if
    for (const char* literal : {"1000ms", "1000s", "1000min", "1000h", "1000us", "1000ns", "1000d", "1000y", "1000i", "1000il", "1000if", "1.5s", "0.001s", "1e3ms", "1000.0il", "0x3E8s", "0b101h", "07d"}) {
        INFO(literal);
        CHECK(tokens(literal) == literal);
    }
    // any other letters after a number are no suffix of C++: the number stays glued to them and is no token (the library's are lower case and whole: `1000I`, `1000sec` are not)
    for (const char* text : {"1000ma", "1000sec", "1000mins", "1000hh", "1000D", "1000S", "1000MS", "1000I", "1000Y", "1000H", "1000m", "1000n", "1000t", "1000ifl", "1000ili", "1000k", "1000a", "1000km", "1000kg", "1000cm",
                             "1000ul_km", "1000u_km", "1.0f_km", "1e3L_x", "0x3E8ul_km"}) {
        INFO(text);
        CHECK(tokens(text) == "");
    }
    // the lookahead still holds after the suffix: a point or a word character after it refuses the token (`1000_km.count()` is one pp-number in C++, whatever it means)
    CHECK(tokens("1000_km.") == "");
    CHECK(tokens("1000_km.count()") == "");
    CHECK(tokens("1000ms.") == "");
    CHECK(tokens("x1000_km") == "");
    CHECK(tokens("a.1000ms") == "");
    // in a line of code, beside other literals
    CHECK(tokens("auto d = 1000_km + 2.5_m * 3u;") == "1000_km|2.5_m|3u");
    CHECK(tokens("auto t = 1000ms + 2s;") == "1000ms|2s");
    // user_suffix_of: the suffix, and nothing for a standard one or none
    CHECK(uc::scan::user_suffix_of("1000_km") == "_km");
    CHECK(uc::scan::user_suffix_of("1000ms") == "ms");
    CHECK(uc::scan::user_suffix_of("0x3E8_m") == "_m");
    CHECK(uc::scan::user_suffix_of("1e3_m") == "_m");
    CHECK(uc::scan::user_suffix_of("1000.0_f") == "_f");
    CHECK(uc::scan::user_suffix_of("1000il") == "il");
    CHECK(uc::scan::user_suffix_of("1000").empty());
    CHECK(uc::scan::user_suffix_of("1000u").empty());
    CHECK(uc::scan::user_suffix_of("1000.0f").empty());
    CHECK(uc::scan::user_suffix_of("0x3E8").empty());
    CHECK(uc::scan::user_suffix_of("1000f").empty());     // no literal
    CHECK(uc::scan::user_suffix_of("1000_km ").empty());  // not one literal wholly
    CHECK(uc::scan::user_suffix_of("not a number").empty());
}

TEST_CASE("number_tokens: a separator belongs to a literal of any radix only between two digits, and what stands after a literal is left to the scanner (group C7, found by rule 5)", "[unitcheck][behaviour][scan]") {
    // an apostrophe after the last digit is no separator -- it opens a character literal, or nothing -- and so is not taken, in any radix and in the digits of a binary exponent
    CHECK(tokens("x = 0x3E8';") == "0x3E8");
    CHECK(tokens("x = 0b101';") == "0b101");
    CHECK(tokens("x = 0x1p3';") == "0x1p3");
    CHECK(tokens("x = 0x1.F4p9';") == "0x1.F4p9");
    CHECK(tokens("c = 1000';") == "1000");
    CHECK(tokens("c = 1e3';") == "1e3");
    CHECK(tokens("c = 0x3E8'g';") == "0x3E8");   // before a letter that is no digit of the radix: the apostrophe is not between two digits
    // a separator between two digits is taken, and the literal ends where its digits end, whatever follows
    CHECK(tokens("x = 0x3'E8; y = 7;") == "0x3'E8|7");
    CHECK(tokens("x = 0b11'1110'1000 + 1;") == "0b11'1110'1000|1");
    CHECK(tokens("x = 0x1p1'0 + 1;") == "0x1p1'0|1");
    CHECK(tokens("x = 0x1.F4'0p9 + 2;") == "0x1.F4'0p9|2");
    CHECK(tokens("x = 0x1'F.4p9 + 3;") == "0x1'F.4p9|3");
    CHECK(tokens("x = 1'000 + 4;") == "1'000|4");
    CHECK(tokens("x = 0x3'E8, y = 0b1'0, z = 0x1p1'0;") == "0x3'E8|0b1'0|0x1p1'0");
    // two apostrophes in a row, and one before the digits, are no separator
    CHECK(tokens("0x3''E8") == "0x3");    // 0x3, then an apostrophe pair and E8: the E8 is glued to the apostrophe, not to a digit
    CHECK(tokens("0x'3E8") == "3E8");     // no digit before the apostrophe: no literal at the 0; the 3E8 after an apostrophe is a number of its own, as after any apostrophe
    CHECK(tokens("0b'101") == "101");
    // a hexadecimal floating literal needs a digit in its mantissa, before or after the point, and its exponent: none of these is a literal
    CHECK(tokens("0x.P-3") == "3");   // no literal swallows the exponent, so its digits after the sign are a number of their own
    for (const char* text : {"0x.p1", "0xp1", "0x.p", "0x.'p1", "0X.P1"}) {
        INFO(text);
        CHECK(tokens(text) == "");
    }
}

TEST_CASE("is_thousand: float(token) is exactly 1000.0 or exactly 0.001, however it is spelt", "[unitcheck][behaviour][scan]") {
    for (const char* spelling : {"1000", "1000.0", "1000.", "1e3", "1E3", "1.0e3", "1.0E+3", "1e+3", "10e2", "100e1", "1000e0", "0.1e4", "01000.0", "0001000.000", "1.0e+03", "0.001", "1e-3", "1E-3", "1.0e-03",
                                 "0.0010", "000.001", "10e-4", "100e-5", "0.00100000000000000000000001", "1000.0000000000000000000000000001"}) {
        INFO(spelling);
        CHECK(uc::scan::is_thousand(spelling));
    }
    for (const char* spelling : {"1001", "999.9999999", "1000.000000001", "100", "10000", "1e4", "1e2", "1e-2", "1e-4", "0.01", "0.0001", "0.0010000001", "1e999", "1e-999", "0", "0.0", "1", "1e0",
                                 "999", "1000.5", "01000", "0100", "00"}) {   // 01000 is octal: 512
        INFO(spelling);
        CHECK_FALSE(uc::scan::is_thousand(spelling));
    }
    CHECK(uc::scan::is_thousand("\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0"));                               // ARABIC-INDIC DIGITS: float() reads them
    CHECK(uc::scan::is_thousand("0\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0"));                             // an ASCII 0 and then ARABIC-INDIC DIGITS: not an octal literal (the digits are not ASCII), the decimal 01000 = 1000
    CHECK(tokens("0\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0") == "0\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0");
    CHECK_FALSE(uc::scan::is_thousand("1000x"));                                                    // float() refuses what is not wholly a number
    CHECK(uc::scan::is_thousand("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"));                // FULLWIDTH DIGITS
}

TEST_CASE("is_thousand: the separators and the standard suffix of a literal are its spelling and its type, not its value (group C6)", "[unitcheck][behaviour][scan]") {
    for (const char* spelling : {"1000u", "1000U", "1000UL", "1000ull", "1000LLU", "1000.0f", "1000.f", "1000.0F", "1000.0L", "1e3f", "1e3L", "1.0E+3f", "0.001f", "0.001L", "1.0e-03F", "1'000", "1'000.0", "1'000.f",
                                 "10'00", "1'0'0'0", "1'0e2", "1'0e2L", "0.0'01", "00'1.0e-3", "1'000u", "1'000ull", "0.0'01f", "1e0'3", "100e0'1"}) {
        INFO(spelling);
        CHECK(uc::scan::is_thousand(spelling));
    }
    for (const char* spelling : {"1'001", "1'000'0", "0'001", "999u", "1001UL", "1000.5f", "0.0011f", "1e0'4", "1'0e3", "1000f", "1000.0u", "1000lL", "1000ee3", "1'000x", "1'", "'1000", "1''000",
                                 "1000 ", "1000u ", "1'000'", "1.'5", "1000.0.0"}) {
        INFO(spelling);
        CHECK_FALSE(uc::scan::is_thousand(spelling));
    }
}

TEST_CASE("is_thousand: the value of a hexadecimal, binary, octal or hexadecimal floating literal is C++'s, and a suffix does not change it (group C7)", "[unitcheck][behaviour][scan]") {
    for (const char* spelling : {"0x3E8", "0x3e8", "0X3E8", "0x3E8u", "0x3E8UL", "0x3'E8", "0x03E8", "0x0000'03E8", "0b1111101000", "0B1111101000", "0b11'1110'1000", "0b1111101000ull", "01750", "0'1750", "01750u",
                                 "001750", "0x1.F4p9", "0x1.F4p+9", "0x1F4p1", "0x7D0p-1", "0x3E8p0", "0x3E8.0p0", "0x.FA0p10", "0x1.F4p9f", "0x1.F4p9L", "0X1.F4P9", "0x1.F4p0'9"}) {
        INFO(spelling);
        CHECK(uc::scan::is_thousand(spelling));
    }
    for (const char* spelling : {"0x3E7", "0x3E9", "0b1111101001", "01751", "01000", "0x1p-1000", "0x1p+1000", "0x1.F4p8", "0x1.F4p10", "0x0p0", "0x1p0", "0x1000", "0x1.0p-10", "0x1p100000000", "0x1p-100000",
                                 "0b0", "089", "0x", "0b2", "0x1p", "0x1.8", "0xp1",
                                 // a value no double can hold, or only as a subnormal one, is not a thousand and is no error: 2^1024, 2^-1074 (the smallest subnormal), 2^-1075, the largest double, and the guards on the size of a shift
                                 "0x1p1024", "0x1p-1074", "0x1p-1075", "0x1.fffffffffffffp1023", "0x1p6000", "0x1p-6000", "0x1p6001", "0x1p-6001"}) {
        INFO(spelling);
        CHECK_FALSE(uc::scan::is_thousand(spelling));
    }
    // 0.001 written in hexadecimal: the double nearest to it is what the decimal 0.001 means, so its exact spelling is one, and the doubles on either side are not
    CHECK(uc::scan::is_thousand("0x1.0624dd2f1a9fcp-10"));
    CHECK_FALSE(uc::scan::is_thousand("0x1.0624dd2f1a9fdp-10"));
    CHECK_FALSE(uc::scan::is_thousand("0x1.0624dd2f1a9fbp-10"));
    // a user-defined or library suffix leaves the value of the number: it is the literal of a thousand of something
    for (const char* spelling : {"1000_km", "1000ms", "0.001_m", "1e3s", "0x3E8_km", "1'000_km", "01750_m", "0b1111101000_km", "1000.0_km", "1e-3_m", "1000min", "0x1.F4p9_m", "0.001h"}) {
        INFO(spelling);
        CHECK(uc::scan::is_thousand(spelling));
    }
    for (const char* spelling : {"1001_km", "1_000", "5_km", "999ms", "0x3E7_km", "01000_km", "1000_km ", "1000f", "1000ma"}) {
        INFO(spelling);
        CHECK_FALSE(uc::scan::is_thousand(spelling));
    }
}

namespace {

// the spans of code of each line, in brackets, a line with none as a dash: the lines of a file lexed in order
std::string code_of(const std::vector<std::string>& lines) {
    std::string out;
    uc::scan::LexState state;
    for (const std::string& line : lines) {
        const uc::scan::LexedLine lexed = uc::scan::lex_line(line, state);
        std::string shown;
        for (const uc::scan::CodeSpan& span : lexed.code) shown += "[" + line.substr(span.begin, span.end - span.begin) + "]";
        out += (out.empty() ? "" : "|") + (shown.empty() ? std::string("-") : shown);
    }
    return out;
}

}  // namespace

TEST_CASE("lex_line: the spans of a line that are not comment, whatever the comment is and wherever it begins (group C7)", "[unitcheck][behaviour][scan]") {
    CHECK(code_of({""}) == "-");
    CHECK(code_of({"int a = 1;"}) == "[int a = 1;]");
    CHECK(code_of({"// c"}) == "-");
    CHECK(code_of({"x // c"}) == "[x ]");
    CHECK(code_of({"/* c */ x"}) == "[ x]");
    CHECK(code_of({"/* a */"}) == "-");
    CHECK(code_of({"/**/ a"}) == "[ a]");
    CHECK(code_of({"a /* c */ b"}) == "[a ][ b]");
    CHECK(code_of({"a /* c */ b /* d */ e"}) == "[a ][ b ][ e]");
    CHECK(code_of({"a /*/ b */ c"}) == "[a ][ c]");                     // `/*/` is still open
    CHECK(code_of({"/* a // b */ c"}) == "[ c]");                        // a // inside a block comment is nothing
    CHECK(code_of({"a // b /* c", "x"}) == "[a ]|[x]");                  // a /* inside a line comment opens nothing
    // across lines
    CHECK(code_of({"a /* c", " inside */ b"}) == "[a ]|[ b]");
    CHECK(code_of({"x /* a", "b */"}) == "[x ]|-");
    CHECK(code_of({"/*", " * 1000", " */ x"}) == "-|-|[ x]");
    CHECK(code_of({"/* open", "   1000 begins with neither", "// nor with slashes", "*/"}) == "-|-|-|-");
    // strings and characters are code, and hide what looks like a comment
    CHECK(code_of({"s = \"/* not a comment */\";"}) == "[s = \"/* not a comment */\";]");
    CHECK(code_of({"s = \"x\"; // c"}) == "[s = \"x\"; ]");
    CHECK(code_of({"c = '/*'; d"}) == "[c = '/*'; d]");
    CHECK(code_of({"R\"(", "text // not", ")\"; // c"}) == "[R\"(]|[text // not]|[)\"; ]");
    CHECK(code_of({"s = \"abc\\", "//def\"; x // c"}) == "[s = \"abc\\]|[//def\"; x ]");
    // a line that begins with * outside a comment is code
    CHECK(code_of({" * 1000;"}) == "[ * 1000;]");
    CHECK(code_of({"double x = a", "    * 1000.0;"}) == "[double x = a]|[    * 1000.0;]");
    // the comment_start of old is where the line comment begins
    uc::scan::LexState state;
    CHECK(uc::scan::lex_line("a /* c */ b // d", state).comment_at == 12);
    CHECK(uc::scan::lex_line("no comment", state).comment_at == std::string::npos);
    CHECK(uc::scan::lex_line("/* open", state).comment_at == std::string::npos);
    CHECK(state.mode == uc::scan::LexState::Mode::BlockComment);
}

TEST_CASE("lex_line: what an identifier and a number are made of -- the digits, the letters and the underscore, to the ends of their ranges, and nothing just outside them (group C7, found by rule 5)", "[unitcheck][behaviour][scan]") {
    const auto comment_at = [](const std::string& line) {
        uc::scan::LexState state;
        return uc::scan::lex_line(line, state).comment_at;
    };
    // an apostrophe between a digit and any character of an identifier is a digit separator: it opens no character literal, and the comment after it is found
    for (const char* next : {"0", "5", "9", "a", "m", "z", "A", "M", "Z", "_"}) {
        const std::string line = std::string("x = 1'") + next + "; // c";
        INFO(line);
        CHECK(comment_at(line) == line.find("//"));
    }
    // and before a character that is none -- the neighbours of the ranges: / : @ [ ` { -- the apostrophe opens a character literal, which a // inside it does not end
    for (const char* next : {"/", ":", "@", "[", "`", "{", " ", "(", "-"}) {
        const std::string line = std::string("x = 1'") + next + " // c'";
        INFO(line);
        CHECK(comment_at(line) == std::string::npos);
    }
    // a raw string opens after exactly the prefixes R, LR, uR, UR and u8R, and not after an identifier that ends in one of them, whatever letter the identifier begins with: in `aR"(a"b)" 1000 // c` the quote
    // after the b ends an ORDINARY string, so that the comment is inside the next one; in `R"(a"b)" 1000 // c` the string is raw and ends at `)"`
    for (const char* ident : {"R", "LR", "uR", "UR", "u8R"}) {
        const std::string line = std::string(ident) + "\"(a\"b)\" 1000 // c";
        INFO(line);
        CHECK(comment_at(line) == line.find("//"));
    }
    for (const char* ident : {"aR", "zR", "ZR", "AR", "_R", "a9R", "x_R", "Z8R", "uuR", "LLR", "u7R", "U8R"}) {
        const std::string line = std::string(ident) + "\"(a\"b)\" 1000 // c";
        INFO(line);
        CHECK(comment_at(line) == std::string::npos);
    }
}

TEST_CASE("comment_start: the first // that is not inside a string, a character literal, a raw string or a block comment (group C6)", "[unitcheck][behaviour][scan]") {
    const auto where = [](const std::string& line) {
        uc::scan::LexState state;
        const std::size_t at = uc::scan::comment_start(line, state);
        return at == std::string::npos ? std::string("none") : line.substr(at);
    };
    // the plain cases
    CHECK(where("") == "none");
    CHECK(where("int a = 1;") == "none");
    CHECK(where("int a = 1; // c") == "// c");
    CHECK(where("// c") == "// c");
    CHECK(where("a = b / c; // d") == "// d");
    CHECK(where("a //* b */ c") == "//* b */ c");           // `//` comes first: a line comment, not a block comment
    CHECK(where("// say \"hi") == "// say \"hi");
    // inside a string literal
    CHECK(where("s = \"http://x\"; // real") == "// real");
    CHECK(where("s = \"//\";") == "none");
    CHECK(where("s = \"a\\\"//b\"; // c") == "// c");           // an escaped quote does not end the string
    CHECK(where("s = \"a\\\\\"; // c") == "// c");              // an escaped backslash does not escape the quote after it
    CHECK(where("s = \"\xC3\xA9//x\"; // c") == "// c");        // a multi-byte character inside the string
    CHECK(where("s = \"/*\"; // c") == "// c");                  // a /* inside a string opens no comment
    CHECK(where("s = L\"//\"; u8\"//\"; // c") == "// c");       // encoding prefixes of an ordinary string
    // inside a character literal
    CHECK(where("c = '\"'; // c") == "// c");                     // the quote inside a character literal opens no string
    CHECK(where("c = '//'; // c") == "// c");
    CHECK(where("c = '\\''; // c") == "// c");                    // an escaped quote
    CHECK(where("c = '/'; d = '/'; // c") == "// c");
    // digit separators and numbers
    CHECK(where("a = 1'000; // c") == "// c");                    // the apostrophe of a separator opens no character literal
    CHECK(where("a = 1'000 + '/'; // c") == "// c");
    CHECK(where("a = 0x1'F; b = 0b1'0; // c") == "// c");
    CHECK(where("a = 1'000.5e+1'0; // c") == "// c");
    CHECK(where("a = .5e+3 + 3.; // c") == "// c");
    CHECK(where("a = 0xAB'CD; // c") == "// c");                  // letters before the apostrophe: a hexadecimal number
    CHECK(where("a = 9'000; // c") == "// c");                    // the digit 9 begins a number like the others
    CHECK(where("a = 0'7; // c") == "// c");
    CHECK(where("a = 1'; // c") == "none");                       // an apostrophe that no letter or digit follows is no separator: it opens a character literal, which this line never closes
    CHECK(where("a = 1000' + 1; // c") == "none");
    CHECK(where("a = 1'000;") == "none");
    // raw string literals: every prefix, delimiters, a closer that is not the right one, and the things that are not raw
    CHECK(where("s = R\"(//)\"; // c") == "// c");
    CHECK(where("s = R\"x(//)x\"; // c") == "// c");
    CHECK(where("s = R\"x(a)\"b//)x\"; // c") == "// c");         // `)"` is no closer when the delimiter is x: the string goes on to `)x"`
    CHECK(where("s = LR\"(//)\"; uR\"(//)\"; UR\"(//)\"; u8R\"(//)\"; // c") == "// c");
    // each prefix, with a quote inside the raw string (an ordinary string would end there, and the `//` after it would be the comment)
    CHECK(where("s = R\"(a\"//)\"; // c") == "// c");
    CHECK(where("s = LR\"(a\"//)\"; // c") == "// c");
    CHECK(where("s = uR\"(a\"//)\"; // c") == "// c");
    CHECK(where("s = UR\"(a\"//)\"; // c") == "// c");
    CHECK(where("s = u8R\"(a\"//)\"; // c") == "// c");
    CHECK(where("s = R\"x(a\"//)x\"; // c") == "// c");
    // an identifier longer than a prefix before the quote (a letter, an underscore, a multi-byte letter) makes it an ordinary string: it ends at the second quote and the `//` after it is the comment
    CHECK(where("s = fooR\"(a\"//)\"; // c") == "//)\"; // c");
    CHECK(where("s = foo_R\"(a\"//)\"; // c") == "//)\"; // c");
    CHECK(where("s = \xC3\xA9R\"(a\"//)\"; // c") == "//)\"; // c");
    CHECK(where("s = R\"0123456789abcdef(//)0123456789abcdef\"; // c") == "// c");   // a delimiter of sixteen characters
    CHECK(where("s = R\"0123456789abcdef(a\"//)0123456789abcdef\"; // c") == "// c");
    CHECK(where("s = R\"0123456789abcde(a\"//)0123456789abcde\"; // c") == "// c");   // fifteen
    // characters that no delimiter may hold, each making the text an ordinary string that ends at the next quote
    CHECK(where("s = R\"a)b(\"; // c") == "// c");
    CHECK(where("s = R\"a\\b(\"; // c") == "// c");
    CHECK(where("s = R\"a\tb(\"; // c") == "// c");
    CHECK(where("s = R\"a\vb(\"; // c") == "// c");
    CHECK(where("s = R\"a\fb(\"; // c") == "// c");
    CHECK(where("s = fooR\"(//\" x; // c") == "// c");            // `fooR` is no prefix: an ordinary string, which the first quote after `(//` closes
    CHECK(where("s = R \"(//\" x; // c") == "// c");              // a space after the R: not a raw string either
    CHECK(where("s = R\"a b(\"; // c") == "// c");                // a space in the delimiter: not raw, so the string ends at the next quote
    CHECK(where("s = R\"0123456789abcdefg(\"; // c") == "// c");   // a delimiter of seventeen: not raw
    CHECK(where("s = R\"(//)\"") == "none");
    // inside a block comment (on one line)
    CHECK(where("a = b /* x // y */ + c; // d") == "// d");
    CHECK(where("a = b /* x // y */ + c;") == "none");
    CHECK(where("/* a */ b; /* c */ d; // e") == "// e");
    CHECK(where("a /*/ // b */ c; // d") == "// d");               // `/*/` is still open: its slash is not the end of a comment
    CHECK(where("a /**/ b; // c") == "// c");
    CHECK(where("a /* x *//* y */ b; // c") == "// c");             // the slash that closes a comment is not the first slash of a `//` that follows it
    // a quote is just a character after a // (the comment starts first) and inside a block comment
    CHECK(where("a = 1; /* don't */ b = 2; // c") == "// c");
    CHECK(where("a = 1; /* \" */ b = 2; // c") == "// c");
    // what is not closed ends with its line
    CHECK(where("s = \"abc // y") == "none");
    CHECK(where("c = 'a // y") == "none");

    // across lines
    const auto trail = [](const std::vector<std::string>& lines) {
        std::string out;
        uc::scan::LexState state;
        for (const std::string& line : lines) {
            const std::size_t at = uc::scan::comment_start(line, state);
            out += (out.empty() ? "" : "|") + (at == std::string::npos ? std::string("none") : line.substr(at));
        }
        return out;
    };
    CHECK(trail({"auto s = R\"sql(", "select 1 // not a comment", ")sql\"; // real"}) == "none|none|// real");
    CHECK(trail({"auto s = R\"(", ")\"; /* x", "*/ // c"}) == "none|none|// c");
    CHECK(trail({"/* open", " // inside", " close */ x; // real"}) == "none|none|// real");
    CHECK(trail({"/*/ // x", "*/ y; // z"}) == "none|// z");
    CHECK(trail({"/* a */ b; /* c", "d */ e; // f"}) == "none|// f");
    CHECK(trail({"s = \"abc\\", "//def\"; // real"}) == "none|// real");             // a backslash at the end of the line carries the string over
    CHECK(trail({"s = \"abc\\", "def\\", "//ghi\"; // real"}) == "none|none|// real");
    CHECK(trail({"s = \"abc // y", "z; // c"}) == "none|// c");                       // an ordinary string without a backslash ends with its line
    CHECK(trail({"c = 'a // y", "z; // c"}) == "none|// c");
    CHECK(trail({"c = '\\", "// c"}) == "none|// c");                                // a character literal is not carried over
    CHECK(trail({"s = \"abc\\\"", "// real"}) == "none|// real");                    // the backslash escapes the quote: the line does not end on a splice, the string is just unterminated
    CHECK(trail({"s = R\"abc", "// real"}) == "none|// real");                       // no `(`: not a raw string
    CHECK(trail({"// a comment \\", "// and the next line is not part of it for this register"}) == "// a comment \\|// and the next line is not part of it for this register");
    CHECK(trail({"auto s = R\"(", "x", "y"}) == "none|none|none");                   // an unterminated raw string runs to the end of the file
}

TEST_CASE("the scanners read the view they are given and nothing beyond it, whatever the larger text holds after the view (group C6)", "[unitcheck][behaviour][scan]") {
    // The register hands the scanners views into bigger texts (`line.substr(0, comment_at)` is followed in memory by the `//` it was cut at), so a read one past the end of a view is a read of the text around it.  Here the
    // character after the view is the one that would change the answer: a suffix letter, a point, a slash that completes a `//`, a star that completes a `/*`.
    const auto first = [](const std::string& whole, std::size_t n) { return std::string_view(whole).substr(0, n); };
    CHECK(uc::scan::is_thousand(first("1000u", 4)));        // a suffix after the digits is not the view's
    CHECK(uc::scan::is_thousand(first("1000L", 4)));
    CHECK(uc::scan::is_thousand(first("1000.0f", 6)));      // nor is the suffix of a floating literal
    CHECK(uc::scan::is_thousand(first("1000.5", 4)));       // a point after the digits is not the view's: with it the literal would be `1000.`, longer than the view
    CHECK(uc::scan::is_thousand(first("1'000'5", 5)));      // nor is a separator after them
    CHECK(tokens(first("1000.5", 4)) == "1000");
    CHECK(tokens(first("1'000'5", 5)) == "1'000");
    CHECK(tokens(first("1000u", 4)) == "1000");
    {
        const std::string whole = "a //";
        uc::scan::LexState state;
        CHECK(uc::scan::comment_start(first(whole, 3), state) == std::string::npos);   // the slash that ends the view is not the first of a `//`
        CHECK(state.mode == uc::scan::LexState::Mode::Code);
    }
    {
        const std::string whole = "a /*";
        uc::scan::LexState state;
        CHECK(uc::scan::comment_start(first(whole, 3), state) == std::string::npos);   // ... nor of a `/*`: no block comment is opened
        CHECK(state.mode == uc::scan::LexState::Mode::Code);
    }
}

TEST_CASE("LexState: where a line starts (group C6)", "[unitcheck][behaviour][scan]") {
    uc::scan::LexState state;
    CHECK(state.mode == uc::scan::LexState::Mode::Code);
    (void)uc::scan::comment_start("auto s = R\"d(", state);
    CHECK(state.mode == uc::scan::LexState::Mode::RawString);
    CHECK(state.raw_delimiter == "d");
    (void)uc::scan::comment_start("text )d\" /* open", state);
    CHECK(state.mode == uc::scan::LexState::Mode::BlockComment);
    (void)uc::scan::comment_start("close */ s = \"x\\", state);
    CHECK(state.mode == uc::scan::LexState::Mode::String);
    (void)uc::scan::comment_start("y\";", state);
    CHECK(state.mode == uc::scan::LexState::Mode::Code);
}

TEST_CASE("names_km: the kilometre pattern, case-insensitively and Unicode-aware", "[unitcheck][behaviour][scan]") {
    for (const char* text : {"km", "KM", "Km", "kM", "m -> km", "km -> m", "(km)", "km.", "km/s", "a km b", "kilomet", "kilometre", "KILOMETERS", "Kilometres to metres", "megakilometres", "kilometr",
                             "x-km-y", "km, m"}) {
        INFO(text);
        CHECK(uc::scan::names_km(text));
    }
    for (const char* text : {"", "m", "m -> mm", "km2", "xkm", "kmx", "km_s", "mkm", "1km", "km1", "kilomtr", "kilo", "milli", "k m", "g/cm^3 -> kg/m^3", "mas -> rad"}) {
        INFO(text);
        CHECK_FALSE(uc::scan::names_km(text));
    }
    // IGNORECASE reads three non-ASCII characters as ASCII letters of the pattern: the Kelvin sign as k, and the capital I with a dot above and the dotless i as i
    CHECK(uc::scan::names_km("\xE2\x84\xAA" "m"));                                       // KELVIN SIGN, m
    CHECK(uc::scan::names_km("m -> \xE2\x84\xAA" "M"));
    CHECK(uc::scan::names_km("\xE2\x84\xAA" "ilometres"));
    CHECK(uc::scan::names_km("k\xC4\xB0" "lomet"));                                      // CAPITAL I WITH DOT ABOVE
    CHECK(uc::scan::names_km("K\xC4\xB1" "LOMET"));                                      // DOTLESS I
    CHECK_FALSE(uc::scan::names_km("kilo\xC3\xA9" "tre"));
    CHECK_FALSE(uc::scan::names_km("\xE2\x84\xAA\xE2\x84\xAA"));                  // two Kelvin signs are KK, not km
    CHECK_FALSE(uc::scan::names_km("km" "\xC3\xA9"));                                    // a letter after the m: no boundary
    CHECK_FALSE(uc::scan::names_km("\xC3\xA9" "km"));
    CHECK(uc::scan::names_km("\xC2\xB7" "km" "\xC2\xB7"));                               // a middle dot on each side: boundaries
}

// ======================================================================================================================================== what is printed

TEST_CASE("the register: the crossing's own literals, each annotated one with what it says, the denominator, the verdict -- every line derived from the print statements", "[unitcheck][behaviour]") {
    Tree t;
    t.write(kHome, "// the one crossing\nconstexpr double kMetresPerKm = 1000.0;\nconstexpr double kKmPerMetre = 1e-3; // reverse\n");
    t.write("modules/a/a.cpp", "double x = y * 1e3;  // UNIT-CROSSING: mas -> rad\n// NOT-A-UNIT-CROSSING: a date radix\nint radix = 1000;\nint ok = 999 + 1001;\n");
    // a one-line block comment, a block comment of three lines, a line comment: none of them is read (group C7: a line is skipped when the lexer says it is comment, and not for the way it begins)
    t.write("modules/b/include/b.hpp", "/* 1000 in a block comment line is skipped */\n/* 1000 in a block comment\n * 1000 continuation\n */\n// 1000 in a line comment\ndouble t = 0.001; // NOT-A-UNIT-CROSSING: a tolerance\n");
    const Counts c{3, 2, 1, 2, 0, 0};
    const Result loud = t.run();
    CHECK(loud.code == kOk);
    CHECK(loud.err.empty());
    CHECK(loud.out == std::string(kHeading) + "  the crossing itself, in " + kHome + ":\n" + home_row(2, "1000.0", "constexpr double kMetresPerKm = 1000.0;") +
                          home_row(3, "1e-3", "constexpr double kKmPerMetre = 1e-3; // reverse") + "\n  unit conversions, 1, each naming what it converts:\n" +
                          annotated_row("modules/a/a.cpp", 1, "1e3", "mas -> rad") + "\n  not conversions at all, 2:\n" + annotated_row("modules/a/a.cpp", 3, "1000", "a date radix") +
                          annotated_row("modules/b/include/b.hpp", 6, "0.001", "a tolerance") + "\n  user-defined or library suffixes, 0, listed and never judged:\n" + denominator(c) + kVerdict);
    const Result quiet = t.run({"--quiet"});
    CHECK(quiet.code == kOk);
    CHECK(quiet.out == denominator(c) + kVerdict);
    CHECK(quiet.err.empty());
}

TEST_CASE("a factor of a thousand that is accounted for by neither is UNACCOUNTED: exit 1, the row, the first 90 code points of its line, the instructions", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/a.cpp",
            "int a = 1000;\ndouble b = 0.001;\n// NOT-A-UNIT-CROSSING: fine\ndouble c = 1e3;\ndouble d = 1000.;\n    double e = 1000.0;" + std::string(100, ' ') + "// not a marker\nx = \"" + std::string(90, 'q') + " 1e3\";\n");
    // line 4 is accounted for by the marker above it; lines 1, 2 and 5 have none (a line above that is code is no marker), 6 is cut at 90 code points (the white space counts), 7 likewise
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 1, 5, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/a.cpp", 1, "1000", "int a = 1000;") + unaccounted_row("modules/a/a.cpp", 2, "0.001", "double b = 0.001;") +
                       unaccounted_row("modules/a/a.cpp", 5, "1000.", "double d = 1000.;") + unaccounted_row("modules/a/a.cpp", 6, "1000.0", "double e = 1000.0;" + std::string(72, ' ')) +
                       unaccounted_row("modules/a/a.cpp", 7, "1e3", "x = \"" + std::string(85, 'q')) + kUnaccountedTrailer);
}

TEST_CASE("an annotation is not a permit: a UNIT-CROSSING that names kilometres outside core/units.hpp is refused, and only that is printed", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/a.cpp", "double a = 1000.0;  // UNIT-CROSSING: m -> km\ndouble b = 1000.0;  // UNIT-CROSSING: kilometres to metres\ndouble c = 1000.0;  // UNIT-CROSSING: km2 -> m2\nint u = 1000;\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 1, 0, 1, 2}));
    CHECK(r.err == "\nA KM<->M CROSSING OUTSIDE core/units.hpp:\n  modules/a/a.cpp:1  annotated 'm -> km'\n  modules/a/a.cpp:2  annotated 'kilometres to metres'\n" + std::string(kMisplacedTrailer));
}

TEST_CASE("the crossing's home is exempt whatever its lines say, and a kilometre annotation is refused anywhere else", "[unitcheck][behaviour]") {
    Tree t;
    t.write(kHome, "double a = 1000.0;\ndouble b = 1000.0;  // UNIT-CROSSING: m -> km\n");
    t.write("modules/core/include/odl/core/other_units.hpp", "double c = 1000.0;  // UNIT-CROSSING: m -> km\n");   // another file of the same directory
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{2, 2, 0, 0, 0, 1}));
    CHECK(r.err == "\nA KM<->M CROSSING OUTSIDE core/units.hpp:\n  modules/core/include/odl/core/other_units.hpp:1  annotated 'm -> km'\n" + std::string(kMisplacedTrailer));
}

// ======================================================================================================================================== which lines, which literals, which marker

TEST_CASE("the marker is on the line or on the line directly above it that BEGINS with // -- one line of lookback, no more; a marker on the line itself wins", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/lb.cpp",
            "// NOT-A-UNIT-CROSSING: above\n"                      // 1
            "int a = 1000;\n"                                      // 2  declared (above)
            "int b = 1000;\n"                                      // 3  the line above is code: unaccounted
            "// NOT-A-UNIT-CROSSING: stale\n"                      // 4
            "\n"                                                   // 5
            "int c = 1000;\n"                                      // 6  a blank line between: unaccounted
            "int d = 5; // NOT-A-UNIT-CROSSING: about d\n"         // 7
            "int e = 1000;\n"                                      // 8  the line above BEGINS with code, whatever it ends with: unaccounted
            "// NOT-A-UNIT-CROSSING: above f\n"                    // 9
            "int f = 1000; // UNIT-CROSSING: g -> kg\n"            // 10 the marker on the line wins over the one above: a conversion
            "int g = 1000; /* NOT-A-UNIT-CROSSING: block */\n"     // 11 a block comment is no marker: unaccounted
            "   // NOT-A-UNIT-CROSSING: indented above\n"          // 12
            "int h = 1000;\n");                                    // 13 declared (the line above begins with // after white space)
    const Result r = t.run();
    CHECK(r.code == kFailed);
    CHECK(contains(r.out, "\n  unit conversions, 1, each naming what it converts:\n" + annotated_row("modules/a/lb.cpp", 10, "1000", "g -> kg") + "\n  not conversions at all, 2:\n" +
                              annotated_row("modules/a/lb.cpp", 2, "1000", "above") + annotated_row("modules/a/lb.cpp", 13, "1000", "indented above") + "\n  user-defined or library suffixes, 0, listed and never judged:\n" + denominator(Counts{1, 0, 1, 2, 4, 0})));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/lb.cpp", 3, "1000", "int b = 1000;") + unaccounted_row("modules/a/lb.cpp", 6, "1000", "int c = 1000;") +
                       unaccounted_row("modules/a/lb.cpp", 8, "1000", "int e = 1000;") + unaccounted_row("modules/a/lb.cpp", 11, "1000", "int g = 1000; /* NOT-A-UNIT-CROSSING: block */") +
                       kUnaccountedTrailer);
}

TEST_CASE("literals are matched by VALUE: every spelling of 1000 and 1/1000 is one, no other number is; two on a line are two rows", "[unitcheck][behaviour]") {
    const std::vector<std::string> spellings = {"1e3", "1E3", "1.0e+3", "1000.", "1000.0", "0.001", "1.0e-03", "1E-3", "1e+3", "10e2", "100e1", "01000.0", "1000e0", "0.1e4", "0x3E8", "0X3e8u", "0b1111101000", "01750", "0x1.F4p9"};
    std::string annotated;
    for (const std::string& s : spellings) annotated += "double v = " + s + "; // NOT-A-UNIT-CROSSING: spelling\n";
    annotated += "double w = 1e3 + 1000; // NOT-A-UNIT-CROSSING: two on one line\n";
    Tree t;
    t.write("modules/a/spelt.cpp", annotated);
    // none of these is a factor of a thousand (and the last ones are not tokens at all: a suffix of the wrong kind, a user-defined or library suffix, a letter or a point is glued to them; the standard
    // suffixes and digit separators are read since group C6, and have their own cases below)
    t.write("modules/a/other.cpp",
            "double v[] = {1000.5, 999.99, 1e4, 1e-4, 100, 0.01, 1001, 0.0010000001, 1e2, 1000.000000001, 1e999};\n"
            "float f = 1000f; double u = 1000.0u; double x1000 = 0; double y = a.1000; double z = 1_000; int o = 01000; int h[] = {0x3E7, 0x3E9, 0b1111101001, 01751, 0x1000};\n"
            "constexpr double lo = 0x1p-1000, hi = 0x1p+1000, e = 0x1.F4p8, g = 0x1.F4p10; // the exponents are not the number 1000\n"
            "unsigned w[] = {1001u, 999UL, 1'001, 1'000'0, 0.0011f, 1000.5f, 10'000.0f};\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kOk);
    CHECK(r.out == denominator(Counts{2, 0, 0, spellings.size() + 2, 0, 0}) + kVerdict);
    const Result loud = t.run();
    for (std::size_t i = 0; i < spellings.size(); ++i) CHECK(contains(loud.out, annotated_row("modules/a/spelt.cpp", i + 1, spellings[i], "spelling")));
    CHECK(contains(loud.out, annotated_row("modules/a/spelt.cpp", spellings.size() + 1, "1e3", "two on one line") + annotated_row("modules/a/spelt.cpp", spellings.size() + 1, "1000", "two on one line")));
}

TEST_CASE("what is not looked at: comment, as the lexer says it -- a line comment, a block comment of any number of lines whatever its lines begin with -- and nothing else (group C7: the Python skipped any line that began with //, * or /*)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/skips.cpp",
            "// 1000\n"                                                     // 1  a line comment
            "   // 1000\n"                                                  // 2
            "/* 1000 */\n"                                                  // 3  a block comment on one line
            "int a = 5; // 1000\n"                                          // 4
            "const char* u = \"http://x\"; int n = 1000;\n"                 // 5  the // inside the string is no comment (group C6): the 1000 after it IS read
            "int b = 5; /* 1000 */\n"                                       // 6  CHANGED in group C7: a block comment hides what it holds wherever it begins (the Python read this 1000)
            "/* 1000\n"                                                     // 7  a block comment of four lines:
            " * 1000 continues\n"                                           // 8
            "   1000 and again, beginning with neither (the Python read this one)\n"   // 9
            " */\n"                                                         // 10
            "\t\xE3\x80\x80" "int c = 5;\n");                               // 11
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 0, 1, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/skips.cpp", 5, "1000", "const char* u = \"http://x\"; int n = 1000;") + kUnaccountedTrailer);
}

TEST_CASE("what is read around a comment: a line that begins with * outside a comment is code, and so is what follows a block comment on the line it closes on (group C7: the fourth blind spot, fixed)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/around.cpp",
            "double x = a\n"                                               // 1
            "    * 1000.0;\n"                                              // 2  a multiplication continued on a line that begins with *: code
            "/* c */ double y = 1000.0;\n"                                 // 3  the statement after a block comment that opens the line
            "/* open\n"                                                    // 4
            "   1000 inside\n"                                             // 5  comment
            " */ int z = 1000;\n"                                          // 6  the block closes: the rest is code
            "int w = 5; /* 1000 */ int v = 1000; /* 1000\n"                // 7  one comment closed on the line, the code between, and a second that opens
            "1000 still inside */ int u = 1000;\n"                         // 8  the second closes: the rest is code
            "int q = 7;\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 0, 5, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/around.cpp", 2, "1000.0", "* 1000.0;") + unaccounted_row("modules/a/around.cpp", 3, "1000.0", "/* c */ double y = 1000.0;") +
                       unaccounted_row("modules/a/around.cpp", 6, "1000", "*/ int z = 1000;") + unaccounted_row("modules/a/around.cpp", 7, "1000", "int w = 5; /* 1000 */ int v = 1000; /* 1000") +
                       unaccounted_row("modules/a/around.cpp", 8, "1000", "1000 still inside */ int u = 1000;") + kUnaccountedTrailer);
}

TEST_CASE("a factor of a thousand written with a standard suffix or with digit separators is read like any other (group C6; the Python's NUMBER was blind to both)", "[unitcheck][behaviour]") {
    const std::vector<std::pair<std::string, std::string>> forms = {   // the source line, the literal as written
        {"float a = 1000.0f;", "1000.0f"},
        {"unsigned b = 1000u;", "1000u"},
        {"float c = 1e3f;", "1e3f"},
        {"long d = 1'000;", "1'000"},
        {"double e = 1'000.0;", "1'000.0"},
        {"double f = 0.001f;", "0.001f"},
        {"auto g = 1000UL;", "1000UL"},
        {"long double h = 1'0e2L;", "1'0e2L"},
        {"auto i = 1000ull;", "1000ull"},
        {"double j = 1.0E-3F;", "1.0E-3F"},
        {"double k = 0.0'01;", "0.0'01"},
    };
    std::string bare, annotated;
    for (const auto& form : forms) {
        bare += form.first + "\n";
        annotated += form.first + "  // NOT-A-UNIT-CROSSING: form\n";
    }
    {
        Tree t;
        t.write("modules/a/forms.cpp", bare);
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kFailed);
        CHECK(r.out == denominator(Counts{1, 0, 0, 0, forms.size(), 0}));
        std::string expected = "\nUNACCOUNTED FACTOR OF A THOUSAND:\n";
        for (std::size_t i = 0; i < forms.size(); ++i) expected += unaccounted_row("modules/a/forms.cpp", i + 1, forms[i].second, forms[i].first);
        CHECK(r.err == expected + kUnaccountedTrailer);
    }
    {
        Tree t;
        t.write("modules/a/forms.cpp", annotated);
        const Result r = t.run();
        CHECK(r.code == kOk);
        CHECK(r.err.empty());
        for (std::size_t i = 0; i < forms.size(); ++i) CHECK(contains(r.out, annotated_row("modules/a/forms.cpp", i + 1, forms[i].second, "form")));
        CHECK(contains(r.out, denominator(Counts{1, 0, 0, forms.size(), 0, 0})));
    }
}

TEST_CASE("a thousand written in hexadecimal, binary or octal, or as a hexadecimal float, is read as C++ reads it, and the exponent of a hexadecimal float is not a number (group C7)", "[unitcheck][behaviour]") {
    // the register's two misreadings, corrected: normal_equations.cpp's  0x1p-1000  and  0x1p+1000  were counted as the number 1000 (the digits of their exponents) and carried a marker for it
    {
        Tree t;
        t.write("modules/estimation/src/normal_equations.cpp",
                "constexpr double kRangeLow = 0x1p-1000;   // NOT-A-UNIT-CROSSING: the exponent of the exact scaling's range, 2^-1000\n"
                "constexpr double kRangeHigh = 0x1p+1000;  // NOT-A-UNIT-CROSSING: the same range's upper end, 2^1000\n"
                "constexpr double kU = 0x1p-53;\n"
                "int o = 01000;\n"                                           // octal: 512
                "int h[] = {0x3E7, 0x3E9, 0b1111101001, 01751, 0x1000};\n");  // near misses, in each radix
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kOk);
        CHECK(r.out == denominator(Counts{1, 0, 0, 0, 0, 0}) + kVerdict);
    }
    // and the thousands of the other radixes are rows
    {
        Tree t;
        t.write("modules/a/radix.cpp",
                "int a = 0x3E8;\n"
                "int b = 0b1111101000;\n"
                "int c = 01750;\n"
                "double d = 0x1.F4p9;\n"
                "unsigned e = 0X3e8UL;\n"
                "double f = 0x1.0624dd2f1a9fcp-10;\n"   // the double nearest 0.001
                "int g = 0'1750;\n");
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kFailed);
        CHECK(r.out == denominator(Counts{1, 0, 0, 0, 7, 0}));
        CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/radix.cpp", 1, "0x3E8", "int a = 0x3E8;") + unaccounted_row("modules/a/radix.cpp", 2, "0b1111101000", "int b = 0b1111101000;") +
                           unaccounted_row("modules/a/radix.cpp", 3, "01750", "int c = 01750;") + unaccounted_row("modules/a/radix.cpp", 4, "0x1.F4p9", "double d = 0x1.F4p9;") +
                           unaccounted_row("modules/a/radix.cpp", 5, "0X3e8UL", "unsigned e = 0X3e8UL;") + unaccounted_row("modules/a/radix.cpp", 6, "0x1.0624dd2f1a9fcp-10", "double f = 0x1.0624dd2f1a9fcp-10;") +
                           unaccounted_row("modules/a/radix.cpp", 7, "0'1750", "int g = 0'1750;") + kUnaccountedTrailer);
    }
}

TEST_CASE("a number with a user-defined or library suffix is LISTED as a category of its own, whatever its line says, and never judged (group C7)", "[unitcheck][behaviour]") {
    Tree t;
    t.write(kHome, "constexpr double kMetresPerKm = 1000.0;\nconstexpr auto kLength = 1000_km;\n");
    t.write("modules/a/ud.cpp",
            "auto a = 1000_km;\n"                                                  // 1  listed
            "auto b = 1000ms; // NOT-A-UNIT-CROSSING: a duration, not judged\n"    // 2  listed: a marker does not change that
            "auto c = 0.001_m; // UNIT-CROSSING: m -> km\n"                        // 3  listed: and the rule against kilometres outside core/units.hpp does not fire
            "auto d = 5_km + 1000;\n"                                              // 4  the 1000 is a plain one, and unaccounted
            "auto e = 1001_km + 1_000 + 999ms;\n");                                // 5  none of these is a thousand
    const Result loud = t.run();
    CHECK(loud.code == kFailed);
    CHECK(loud.out == std::string(kHeading) + "  the crossing itself, in " + kHome + ":\n" + home_row(1, "1000.0", "constexpr double kMetresPerKm = 1000.0;") + "\n  unit conversions, 0, each naming what it converts:\n" +
                          "\n  not conversions at all, 0:\n" + "\n  user-defined or library suffixes, 4, listed and never judged:\n" + annotated_row("modules/a/ud.cpp", 1, "1000_km", "auto a = 1000_km;") +
                          annotated_row("modules/a/ud.cpp", 2, "1000ms", "auto b = 1000ms; // NOT-A-UNIT-CROSSING: a duration, not judged") +
                          annotated_row("modules/a/ud.cpp", 3, "0.001_m", "auto c = 0.001_m; // UNIT-CROSSING: m -> km") + annotated_row(kHome, 2, "1000_km", "constexpr auto kLength = 1000_km;") +
                          denominator(Counts{2, 1, 0, 0, 1, 0, 4}));
    CHECK(loud.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/ud.cpp", 4, "1000", "auto d = 5_km + 1000;") + kUnaccountedTrailer);
    // with nothing else wrong the listing does not fail the run
    Tree u;
    u.write("modules/a/only.cpp", "auto a = 1000_km;\nauto b = 0.001_m;\nauto c = 5_km;\n");
    const Result ok = u.run({"--quiet"});
    CHECK(ok.code == kOk);
    CHECK(ok.err.empty());
    CHECK(ok.out == denominator(Counts{1, 0, 0, 0, 0, 0, 2}) + kVerdict);
    // a thousand with a user-defined or library suffix is listed in whatever radix it is written, and whatever kind of literal it is
    Tree v;
    v.write("modules/a/radix_ud.cpp",
            "auto a = 0x3E8_km;\n"
            "auto b = 0b1111101000_km;\n"
            "auto c = 01750_m;\n"
            "auto d = 0x1.F4p9_m;\n"
            "auto e = 1000.0_km;\n"
            "auto f = 0x1p-10_m;\n");   // 2^-10 is not 0.001: not a thousand, so not listed
    const Result radix = v.run({"--quiet"});
    CHECK(radix.code == kOk);
    CHECK(radix.err.empty());
    CHECK(radix.out == denominator(Counts{1, 0, 0, 0, 0, 0, 5}) + kVerdict);
}

TEST_CASE("a string literal carried over several lines by backslashes counts its numbers on every one of its lines, the middle ones too (group C7, found by rule 5)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/s.cpp",
            "const char* s = \"first \\\n"
            "1000 in the middle \\\n"
            "last\";\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 0, 1, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/s.cpp", 2, "1000", "1000 in the middle \\") + kUnaccountedTrailer);
    // and a raw string that goes on to the next line: its middle lines too
    Tree u;
    u.write("modules/a/r.cpp",
            "const char* s = R\"(first\n"
            "0.001 in the middle\n"
            "last)\";\n");
    const Result raw = u.run({"--quiet"});
    CHECK(raw.code == kFailed);
    CHECK(raw.out == denominator(Counts{1, 0, 0, 0, 1, 0}));
    CHECK(raw.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/r.cpp", 2, "0.001", "0.001 in the middle") + kUnaccountedTrailer);
}

TEST_CASE("a comment that follows a literal with nothing between them leaves the literal whole (group C6)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/glued.cpp", "int a = 1000// NOT-A-UNIT-CROSSING: glued\ndouble b = 1e-3f// UNIT-CROSSING: mas -> rad\n");
    const Result r = t.run();
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(contains(r.out, "\n  unit conversions, 1, each naming what it converts:\n" + annotated_row("modules/a/glued.cpp", 2, "1e-3f", "mas -> rad") + "\n  not conversions at all, 1:\n" +
                              annotated_row("modules/a/glued.cpp", 1, "1000", "glued")));
    CHECK(contains(r.out, denominator(Counts{1, 0, 1, 1, 0, 0})));
}

TEST_CASE("a // inside a string literal, a character literal, a raw string or a block comment is no comment: what follows it is read, and what it says is no marker (group C6)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/strings.cpp",
            "const char* u = \"http://x\"; int n1 = 1000;\n"                                                   // 1  the 1000 after a // in a string: unaccounted (the Python lost it)
            "const char* v = \"//\"; double n2 = 1e3;  // NOT-A-UNIT-CROSSING: after a string\n"             // 2  declared by the real comment
            "int n3 = 1000; const char* w = \"// NOT-A-UNIT-CROSSING: fake\";\n"                             // 3  the text in the string is no marker: unaccounted (the Python took it for one)
            "const char c = '\"'; int n4 = 1000; // NOT-A-UNIT-CROSSING: after a char literal\n"             // 4  the quote in a character literal opens no string
            "const char* r = R\"(a // b)\"; int n5 = 1000;\n"                                                  // 5  unaccounted: after a raw string
            "int a = 1; /* http://x */ int n6 = 1000;\n"                                                       // 6  unaccounted: the // is inside a block comment
            "int s = 1'000; // NOT-A-UNIT-CROSSING: a separator opens no character literal\n"                  // 7  declared (a character literal would have hidden the comment)
            "const char* t = R\"(\n"                                                                           // 8  a raw string over several lines
            "// 1000 inside the raw string is text, not a comment\n"                                           // 9  unaccounted: the line begins inside a string
            ")\";\n"                                                                                           // 10
            "const char* z = \"abc\\\n"                                                                        // 11 a string carried over by a backslash
            " * 1000 is text too\";\n"                                                                        // 12 unaccounted: the line begins inside a string
            "const char* x = \"// UNIT-CROSSING: fake -> fake\"; int n7 = 1000;  // NOT-A-UNIT-CROSSING: real\n");   // 13 declared by the REAL comment: the fake marker in the string is no marker
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 4, 6, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/strings.cpp", 1, "1000", "const char* u = \"http://x\"; int n1 = 1000;") +
                       unaccounted_row("modules/a/strings.cpp", 3, "1000", "int n3 = 1000; const char* w = \"// NOT-A-UNIT-CROSSING: fake\";") +
                       unaccounted_row("modules/a/strings.cpp", 5, "1000", "const char* r = R\"(a // b)\"; int n5 = 1000;") +
                       unaccounted_row("modules/a/strings.cpp", 6, "1000", "int a = 1; /* http://x */ int n6 = 1000;") +
                       unaccounted_row("modules/a/strings.cpp", 9, "1000", "// 1000 inside the raw string is text, not a comment") +
                       unaccounted_row("modules/a/strings.cpp", 12, "1000", "* 1000 is text too\";") + kUnaccountedTrailer);
    const Result loud = t.run();
    CHECK(contains(loud.out, "\n  not conversions at all, 4:\n" + annotated_row("modules/a/strings.cpp", 2, "1e3", "after a string") + annotated_row("modules/a/strings.cpp", 4, "1000", "after a char literal") +
                                 annotated_row("modules/a/strings.cpp", 7, "1'000", "a separator opens no character literal") + annotated_row("modules/a/strings.cpp", 13, "1000", "real")));
}

TEST_CASE("the line above is a marker's place only when it is a comment line: one that begins inside a string literal is not (group C6)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/above.cpp",
            "const char* s = R\"(\n"                  // 1
            "// NOT-A-UNIT-CROSSING: fake)\";\n"       // 2  begins inside the raw string and ends it: text, no marker
            "double v = 1000.0;\n"                     // 3  unaccounted: the line above is not a comment line (the Python read it as one)
            "// NOT-A-UNIT-CROSSING: real\n"           // 4
            "double w = 1000.0;\n");                   // 5  declared by the comment line above it, as ever
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 1, 1, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/above.cpp", 3, "1000.0", "double v = 1000.0;") + kUnaccountedTrailer);

    // group C7: a line that looks like a comment but is inside a block comment is no marker either; and a marker has to be in a // comment, wherever a block comment stands on the line
    Tree b;
    b.write("modules/a/block_above.cpp",
            "/* open\n"                                                      // 1
            "// NOT-A-UNIT-CROSSING: inside a block comment, so no marker\n"  // 2  begins inside the block comment
            "*/ double v = 1000.0;\n"                                        // 3  unaccounted: the line above is not a comment line of its own
            "// NOT-A-UNIT-CROSSING: real\n"                                 // 4
            "double w = 1000.0;\n"                                           // 5  declared by the line above, as ever
            "double x = 1000.0; /* c */ // NOT-A-UNIT-CROSSING: after a block comment\n"   // 6  declared: the marker is in the line comment
            "/* NOT-A-UNIT-CROSSING: in a block comment */ double y = 1000.0;\n");         // 7  unaccounted: a marker needs a // comment
    const Result rb = b.run({"--quiet"});
    CHECK(rb.code == kFailed);
    CHECK(rb.out == denominator(Counts{1, 0, 0, 2, 2, 0}));
    CHECK(rb.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/block_above.cpp", 3, "1000.0", "*/ double v = 1000.0;") +
                        unaccounted_row("modules/a/block_above.cpp", 7, "1000.0", "/* NOT-A-UNIT-CROSSING: in a block comment */ double y = 1000.0;") + kUnaccountedTrailer);
}

TEST_CASE("the text shown is the line stripped of white space (Unicode's too), and the home rows show 80 code points, not bytes", "[unitcheck][behaviour]") {
    Tree t;
    std::string accents;
    for (int i = 0; i < 100; ++i) accents += "\xC3\xA9";
    std::string accents_cut;
    for (int i = 0; i < 61; ++i) accents_cut += "\xC3\xA9";
    t.write(kHome, "\t\xE3\x80\x80" "double a = 1000.0;\xC2\xA0 \n" "double b = 1000.0 + " + std::string(70, 'z') + ";\n" + "double c = 0.001 + " + accents + "\n");
    const Result r = t.run();
    CHECK(r.code == kOk);
    CHECK(contains(r.out, home_row(1, "1000.0", "double a = 1000.0;")));
    CHECK(contains(r.out, home_row(2, "1000.0", ("double b = 1000.0 + " + std::string(70, 'z') + ";").substr(0, 80))));
    CHECK(contains(r.out, home_row(3, "0.001", "double c = 0.001 + " + accents_cut)));   // 19 + 61 = 80 code points, 141 bytes
}

TEST_CASE("lines are the text split on \\n after universal newlines: CR LF and a lone CR end a line, a form feed and U+2028 do not", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/a.cpp", "int a = 5;\r\nint b = 1000;\n");
    t.write("modules/a/b.cpp", "int a = 5;\rint b = 1000;\n");
    t.write("modules/a/c.cpp", "int a = 5;\fint b = 1000;\n");
    t.write("modules/a/d.cpp", "int a = 5;\xE2\x80\xA8" "int b = 1000;\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/a.cpp", 2, "1000", "int b = 1000;") + unaccounted_row("modules/a/b.cpp", 2, "1000", "int b = 1000;") +
                       unaccounted_row("modules/a/c.cpp", 1, "1000", "int a = 5;\fint b = 1000;") + unaccounted_row("modules/a/d.cpp", 1, "1000", "int a = 5;\xE2\x80\xA8" "int b = 1000;") + kUnaccountedTrailer);
}

// ======================================================================================================================================== which files are production sources

TEST_CASE("the production sources: the .hpp and .cpp files below modules/ whose names end that way, none below a directory called build or tests; pathlib's order", "[unitcheck][behaviour]") {
    const std::string line = "int v = 1000;\n";
    Tree t;
    const std::vector<std::string> searched_in_order = {"modules/m/.hpp", "modules/m/a.cpp", "modules/m/a.hpp", "modules/m/f.tar.cpp", "modules/m/mytests/d.cpp", "modules/m/rebuild/e.hpp",
                                                        "modules/m/sub/b.cpp", "modules/m/testsuite/c.cpp", "modules/tests2/g.cpp"};
    for (const std::string& f : searched_in_order) t.write(f, line);
    for (const char* f : {"modules/m/a.h", "modules/m/a.cc", "modules/m/a.ipp", "modules/m/a.txt", "modules/m/a.cmake", "modules/m/a.CPP", "modules/m/a.hpp.bak", "modules/m/tests/t.cpp",
                          "modules/m/sub/tests/u.hpp", "modules/tests/v.cpp", "modules/m/build/w.cpp", "modules/build/x.hpp", "modules/m/sub/build/y.cpp", "tests/z.cpp", "tools/q.cpp", "elsewhere/r.hpp"}) {
        t.write(f, line);
    }
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{searched_in_order.size(), 0, 0, 0, searched_in_order.size(), 0}));
    std::string expected = "\nUNACCOUNTED FACTOR OF A THOUSAND:\n";
    for (const std::string& f : searched_in_order) expected += unaccounted_row(f, 1, "1000", "int v = 1000;");
    CHECK(r.err == expected + kUnaccountedTrailer);
}

TEST_CASE("an entry that is no regular file is skipped by rule, a link to a file is read, a link to a directory is not entered, a file that is not UTF-8 is read with replacement characters", "[unitcheck][behaviour]") {
    Tree t;
    fs::create_directories(t.root / "modules/m/odd.cpp");   // a directory with a source's name
    t.write("modules/m/real.cpp", "int v = 1000;\n");
    t.write("elsewhere/inside.cpp", "int v = 1000;\n");
    std::error_code ec;
    fs::create_directory_symlink(t.root / "elsewhere", t.root / "modules/m/linked", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "modules/m/real.cpp", t.root / "modules/m/w.hpp", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "nowhere", t.root / "modules/m/y.hpp", ec);
    REQUIRE_FALSE(ec);
    REQUIRE(::mkfifo((t.root / "modules/m/z.cpp").c_str(), 0600) == 0);
    t.write("modules/m/lat.cpp", std::string("caf\xE9 = 1000;\n"));
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{3, 0, 0, 0, 3, 0}));   // lat.cpp, real.cpp and the link to it
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/m/lat.cpp", 1, "1000", "caf\xEF\xBF\xBD = 1000;") + unaccounted_row("modules/m/real.cpp", 1, "1000", "int v = 1000;") +
                       unaccounted_row("modules/m/w.hpp", 1, "1000", "int v = 1000;") + kUnaccountedTrailer);
}

TEST_CASE("a tree under directories called build and tests is searched all the same (the Python tested the absolute path, and passed over every file of such a tree)", "[unitcheck][behaviour]") {
    Tree t("tests/build/deeper");
    t.write("modules/m/a.cpp", "int v = 1000;\n");
    t.write("modules/m/tests/skipped.cpp", "int v = 1000;\n");   // below a tests directory of the TREE: still skipped
    t.write("modules/m/build/skipped.cpp", "int v = 1000;\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 0, 1, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/m/a.cpp", 1, "1000", "int v = 1000;") + kUnaccountedTrailer);
}

TEST_CASE("no production source is REFUSED, exit 2, naming the modules directory: success over nothing read is the failure a checker exists against", "[unitcheck][behaviour]") {
    {
        Tree t;
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err == "unitcheck: nothing to search: no .hpp or .cpp file under " + (t.root / "modules").string() + " (outside build and tests directories)\n");
    }
    {
        Tree t;
        t.write("modules/m/tests/a.cpp", "int v = 1000;\n");
        t.write("modules/m/notes.txt", "1000\n");
        t.write("tools/x.cpp", "int v = 1000;\n");
        CHECK(t.run({"--quiet"}).code == kArgument);
    }
}

TEST_CASE("a source that cannot be read is REFUSED, exit 2, naming it -- never skipped", "[unitcheck][behaviour]") {
    if (::geteuid() == 0) SKIP("root opens every file");
    Tree t;
    t.write("modules/m/a.cpp", "int a;\n");
    t.write("modules/m/locked.cpp", "int b;\n");
    REQUIRE(::chmod((t.root / "modules/m/locked.cpp").c_str(), 0) == 0);
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kArgument);
    CHECK(r.out.empty());
    CHECK(contains(r.err, "unitcheck: cannot read " + (t.root / "modules/m/locked.cpp").string() + ": "));
    REQUIRE(::chmod((t.root / "modules/m/locked.cpp").c_str(), 0600) == 0);
}

TEST_CASE("an error the tool did not anticipate is reported as such, exit 70, and not as a pass or a finding", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/m/a.cpp", "int a;\n");
    odl::devtools_testing::ThrowingStream out;   // every write to it throws: the counts of a clean run cannot be printed
    std::ostringstream err;
    const int code = uc::run({"--quiet", "--root", t.root.string()}, Streams{out, err});
    CHECK(code == 70);
    CHECK(err.str() == "unitcheck: internal error: boom\n");
}

// ======================================================================================================================================== the command line

TEST_CASE("the command line: --quiet, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/a.cpp", "int v = 5;\n");
    const std::string quiet = denominator(Counts{1, 0, 0, 0, 0, 0}) + kVerdict;
    CHECK(run_tool({"--root=" + t.root.string(), "--quiet"}).out == quiet);
    CHECK(run_tool({"--quiet", "--root", t.root.string()}).out == quiet);
    CHECK(run_tool({"--root", t.root.string()}).out.rfind(kHeading, 0) == 0);
    const Result help = run_tool({"-h"});
    CHECK(help.code == kOk);
    CHECK(help.out.rfind("usage: unitcheck [-h] [--quiet] [--root ROOT]\n", 0) == 0);
    CHECK(contains(help.out, "options:\n  -h, --help   show this help and exit\n  --quiet      print the counts and the verdict, not the register\n"));
    CHECK(contains(help.out, "exit codes: 0 every factor of a thousand is accounted for   1 one is not, or one names kilometres outside core/units.hpp   2 an argument error or nothing to search\n"));
    CHECK(contains(help.out, "one that carries a user-defined or library suffix (1000_km, 1000ms) is listed in a category of its own and never judged.\n"));
    CHECK(run_tool({"--help"}).out == help.out);
    CHECK(help.err.empty());
    CHECK(run_tool({"-h=x"}).code == kArgument);                       // a short option takes no value
    CHECK(run_tool({"--quiet", "--help"}).out == help.out);
    // an option's value is whatever follows it unless that begins with `--` (so a directory called -x is a value)
    const Result dash = run_tool({"--root", "-x"});
    CHECK(dash.code == kArgument);
    CHECK(dash.err == "unitcheck: nothing to search: no .hpp or .cpp file under -x/modules (outside build and tests directories)\n");
    const Result missing = run_tool({"--root"});
    CHECK(missing.code == kArgument);
    CHECK(missing.err == "usage: unitcheck [-h] [--quiet] [--root ROOT]\nunitcheck: error: argument --root: expected one argument\n");
    // a word that begins with `--` is an option, never the value of the one before it: `--root --quiet` lacks its value
    const Result swallowed = run_tool({"--root", "--quiet"});
    CHECK(swallowed.code == kArgument);
    CHECK(swallowed.err == missing.err);
    CHECK(swallowed.out.empty());
    CHECK(run_tool({"--root", "--bogus"}).err == missing.err);
    const Result unknown = run_tool({"--bogus"});
    CHECK(unknown.code == kArgument);
    CHECK(unknown.err == "usage: unitcheck [-h] [--quiet] [--root ROOT]\nunitcheck: error: unrecognized arguments: --bogus\n");
    CHECK(run_tool({"--quiet=yes", "--root", t.root.string()}).code == kArgument);
    CHECK(run_tool({"--qui", "--root", t.root.string()}).code == kArgument);   // no abbreviations
    CHECK(run_tool({"--roo", t.root.string()}).code == kArgument);
    CHECK(run_tool({"stray", "--root", t.root.string()}).code == kArgument);
}

// ======================================================================================================================================== the real tree

namespace {

struct Recount {
    std::size_t sources = 0, home = 0, crossings = 0, declared = 0, unaccounted = 0, misplaced = 0, suffixed = 0;
};

std::string lstrip_blanks(const std::string& s) {
    const std::size_t i = s.find_first_not_of(" \t\f\v\r");
    return i == std::string::npos ? std::string() : s.substr(i);
}

// the register of the real production sources, made a second way: std::regex (ECMAScript, ASCII word characters) in place of the hand-written scanners, `strtod` and `strtoull` for the value, the standard
// directory iterator for the files, and comments taken out by a plain search for their two delimiters; it shares no code with the tool.  The tree is ASCII wherever a number touches a letter, and has no `/*` or
// `//` inside a string (the equality of the two counts shows both).  Hexadecimal floating literals are blanked: no thousand is written that way.
Recount recount(const fs::path& root) {
    // the numbers the tool reads, as a regular expression of the other kind: digits with digit separators, then a point and a fraction, or an exponent (floating: suffix f F l L), or neither (integer: suffix
    // u l ul lu ll ull llu), each followed by a lookahead that refuses a word character or a point (group C6 extended the Python's pattern with the separators and the suffixes); group C7 adds the
    // user-defined and library suffixes (an underscore and an identifier, or exactly h min s ms us ns d y i il if) as a second alternative, and the integers of the other radixes
    static const std::regex hex_float(R"re(0[xX][0-9a-fA-F']*\.?[0-9a-fA-F']*[pP][+-]?\d+(?:'\d+)*[fFlL]?)re");
    static const std::regex number(
        R"re((^|[^\w.])()re"
        R"re(0[xX][0-9a-fA-F]+(?:'[0-9a-fA-F]+)*(?:[uU](?:ll|LL|l|L)?|(?:ll|LL|l|L)[uU]?)?|)re"
        R"re(0[bB][01]+(?:'[01]+)*(?:[uU](?:ll|LL|l|L)?|(?:ll|LL|l|L)[uU]?)?|)re"
        R"re(\d+(?:'\d+)*\.(?:\d+(?:'\d+)*)?(?:[eE][+-]?\d+(?:'\d+)*)?[fFlL]?|)re"
        R"re(\d+(?:'\d+)*[eE][+-]?\d+(?:'\d+)*[fFlL]?|)re"
        R"re(\d+(?:'\d+)*(?:[uU](?:ll|LL|l|L)?|(?:ll|LL|l|L)[uU]?)?)re"
        R"re()(?![\w.]))re");
    static const std::regex suffixed(
        R"re((^|[^\w.])()re"
        R"re(\d+(?:'\d+)*\.(?:\d+(?:'\d+)*)?(?:[eE][+-]?\d+(?:'\d+)*)?|)re"
        R"re(\d+(?:'\d+)*[eE][+-]?\d+(?:'\d+)*|)re"
        R"re(\d+(?:'\d+)*)re"
        R"re()(_\w*|ms|min|h|s|us|ns|d|y|il|if|i)(?![\w.]))re");
    static const std::regex marker(R"re(//.*?\b(NOT-A-UNIT-CROSSING|UNIT-CROSSING)\s*:\s*(.+?)\s*$)re");
    static const std::regex kilometres(R"re(\bkm\b|kilomet)re", std::regex::icase);
    Recount out;
    for (fs::recursive_directory_iterator it(root / "modules"), end; it != end; ++it) {
        if (it->is_directory() && (it->path().filename() == "build" || it->path().filename() == "tests")) {
            it.disable_recursion_pending();
            continue;
        }
        const std::string name = it->path().filename().string();
        if (!it->is_regular_file() || !(name.ends_with(".hpp") || name.ends_with(".cpp"))) continue;
        ++out.sources;
        const std::string rel = fs::relative(it->path(), root).generic_string();
        const std::string raw = read_text(it->path());
        // the code of the file: every character of a block comment blanked (newlines kept, so that the lines stay lines), a line comment left as it is (the markers are read from it)
        std::string code = raw;
        {
            enum class Mode { Code, Line, Block } mode = Mode::Code;
            for (std::size_t k = 0; k < code.size(); ++k) {
                if (mode == Mode::Code) {
                    if (code.compare(k, 2, "//") == 0) {
                        mode = Mode::Line;
                        ++k;
                    } else if (code.compare(k, 2, "/*") == 0) {
                        mode = Mode::Block;
                        code[k] = ' ';
                        code[k + 1] = ' ';
                        ++k;
                    }
                } else if (mode == Mode::Line) {
                    if (code[k] == '\n') mode = Mode::Code;
                } else if (code.compare(k, 2, "*/") == 0) {
                    code[k] = ' ';
                    code[k + 1] = ' ';
                    ++k;
                    mode = Mode::Code;
                } else if (code[k] != '\n') {
                    code[k] = ' ';
                }
            }
        }
        std::vector<std::string> lines;
        std::vector<std::string> codes;
        const auto split = [](const std::string& text) {
            std::vector<std::string> parts;
            for (std::size_t from = 0;;) {
                const std::size_t nl = text.find('\n', from);
                if (nl == std::string::npos) {
                    parts.push_back(text.substr(from));
                    break;
                }
                parts.push_back(text.substr(from, nl - from));
                from = nl + 1;
            }
            return parts;
        };
        lines = split(raw);
        codes = split(code);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::string text = codes[i].substr(0, codes[i].find("//"));
            if (text.find_first_of("0123456789") == std::string::npos) continue;   // no digit, no number: spares the slow regular expression most of the tree
            const std::string blanked = std::regex_replace(text, hex_float, " ");
            std::size_t found = 0;
            std::size_t listed = 0;
            for (std::sregex_iterator m(blanked.begin(), blanked.end(), number), done; m != done; ++m) {
                // the value: the literal without its digit separators and its suffix letters
                std::string spelling;
                for (const char ch : (*m)[2].str()) {
                    if (ch != '\'') spelling += ch;
                }
                double v = 0.0;
                const bool hex = spelling.size() > 1 && spelling[0] == '0' && (spelling[1] == 'x' || spelling[1] == 'X');
                const bool bin = spelling.size() > 1 && spelling[0] == '0' && (spelling[1] == 'b' || spelling[1] == 'B');
                const bool floating = spelling.find_first_of(".eE") != std::string::npos && !hex;
                if (hex || bin) {
                    while (!spelling.empty() && std::string("uUlL").find(spelling.back()) != std::string::npos) spelling.pop_back();
                    v = static_cast<double>(std::strtoull(spelling.c_str() + 2, nullptr, hex ? 16 : 2));
                } else if (!floating) {
                    while (!spelling.empty() && std::string("uUlL").find(spelling.back()) != std::string::npos) spelling.pop_back();
                    v = static_cast<double>(std::strtoull(spelling.c_str(), nullptr, spelling.size() > 1 && spelling[0] == '0' ? 8 : 10));   // a leading zero makes an integer octal
                } else {
                    while (!spelling.empty() && std::string("fFlL").find(spelling.back()) != std::string::npos) spelling.pop_back();
                    v = std::strtod(spelling.c_str(), nullptr);
                }
                if (v == 1000.0 || v == 0.001) ++found;
            }
            for (std::sregex_iterator m(blanked.begin(), blanked.end(), suffixed), done; m != done; ++m) {
                std::string spelling;
                for (const char ch : (*m)[2].str()) {
                    if (ch != '\'') spelling += ch;
                }
                if (std::strtod(spelling.c_str(), nullptr) == 1000.0 || std::strtod(spelling.c_str(), nullptr) == 0.001) ++listed;
            }
            out.suffixed += listed;
            if (found == 0) continue;
            // the marker is read from the line's own comment, or from the line above when that is a comment line of its own (a `//` with nothing but blanks before it, not inside a block comment)
            const auto comment_of = [&](std::size_t k) { const std::size_t p = codes[k].find("//"); return p == std::string::npos ? std::string() : lines[k].substr(p); };
            const auto is_comment_line = [&](std::size_t k) { const std::size_t p = codes[k].find("//"); return p != std::string::npos && lstrip_blanks(codes[k].substr(0, p)).empty(); };
            std::smatch mark;
            const std::string own = comment_of(i);
            bool marked = std::regex_search(own, mark, marker);
            const std::string above = i >= 1 && is_comment_line(i - 1) ? comment_of(i - 1) : std::string();
            if (!marked && !above.empty()) marked = std::regex_search(above, mark, marker);
            for (std::size_t k = 0; k < found; ++k) {
                if (rel == kHome) ++out.home;
                else if (marked && mark[1].str() == "UNIT-CROSSING") ++(std::regex_search(mark[2].str(), kilometres) ? out.misplaced : out.crossings);
                else if (marked) ++out.declared;
                else ++out.unaccounted;
            }
        }
    }
    return out;
}

// the eight numbers a quiet run prints, in the order of its lines
std::vector<std::size_t> printed_counts(const std::string& out) {
    static const std::regex line(R"re((?:searched|1/1000|units\.hpp|CROSSING|NEITHER|\(listed\)) +(\d+)\n)re");
    std::vector<std::size_t> numbers;
    for (std::sregex_iterator m(out.begin(), out.end(), line), done; m != done; ++m) numbers.push_back(static_cast<std::size_t>(std::stoull((*m)[1].str())));
    return numbers;
}

}  // namespace

TEST_CASE("the real tree: every factor of a thousand is accounted for, and the counts are those of a second, independent register of the same sources", "[unitcheck][real_tree]") {
    const Result r = run_tool({"--quiet"});
    INFO(r.out << r.err);
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    const Recount c = recount(uc::default_root());
    CHECK(c.unaccounted == 0);
    CHECK(c.misplaced == 0);
    REQUIRE(c.sources > 0);
    CHECK(r.out == denominator(Counts{c.sources, c.home, c.crossings, c.declared, c.unaccounted, c.misplaced, c.suffixed}) + kVerdict);
    // two blind scanners would agree on zero: the crossing's own file carries the definition, and the register is not empty
    CHECK(c.home >= 1);
    CHECK(c.crossings + c.declared >= 1);
}

namespace {

// a copy of the real production sources (the .hpp and .cpp files below modules/, none below a build or tests directory), into a fresh tree
void copy_production_sources(const fs::path& from_root, Tree& into, std::vector<std::string>& copied) {
    for (fs::recursive_directory_iterator it(from_root / "modules"), end; it != end; ++it) {
        if (it->is_directory() && (it->path().filename() == "build" || it->path().filename() == "tests")) {
            it.disable_recursion_pending();
            continue;
        }
        const std::string name = it->path().filename().string();
        if (!it->is_regular_file() || !(name.ends_with(".hpp") || name.ends_with(".cpp"))) continue;
        const std::string rel = fs::relative(it->path(), from_root).generic_string();
        into.write(rel, read_text(it->path()));
        copied.push_back(rel);
    }
    std::sort(copied.begin(), copied.end());
}

std::size_t newlines_in(const std::string& s) { return static_cast<std::size_t>(std::count(s.begin(), s.end(), '\n')); }

}  // namespace

TEST_CASE("controls on a copy of the real sources: the copy passes and prints what the real tree prints; each defect injected into it is caught, by the rule it breaks", "[unitcheck][real_tree]") {
    const Result real = run_tool({"--quiet"});
    REQUIRE(real.code == kOk);
    Tree t;
    std::vector<std::string> copied;
    copy_production_sources(uc::default_root(), t, copied);
    REQUIRE(copied.size() > 2);
    const Result clean = t.run({"--quiet"});
    CHECK(clean.code == kOk);
    CHECK(clean.out == real.out);

    // a victim that is not the crossing's home, and the number of its lines
    std::string victim;
    for (const std::string& rel : copied) {
        if (rel != kHome) {
            victim = rel;
            break;
        }
    }
    REQUIRE_FALSE(victim.empty());
    const std::string original = read_text(t.root / victim);
    const std::size_t next_line = newlines_in(original) + 2;   // a "\n" is appended first, then the defect on its own line

    // (1) an unannotated factor of a thousand
    t.write(victim, original + "\ndouble injected_scale = value * 1000.0;\n");
    const Result unaccounted = t.run({"--quiet"});
    CHECK(unaccounted.code == kFailed);
    CHECK(unaccounted.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row(victim, next_line, "1000.0", "double injected_scale = value * 1000.0;") + kUnaccountedTrailer);
    const std::vector<std::size_t> base = printed_counts(clean.out);
    REQUIRE(base.size() == 8);
    CHECK(printed_counts(unaccounted.out)[5] == 1);   // ACCOUNTED FOR BY NEITHER

    // (2) the same, spelt another way, annotated honestly as a conversion that names kilometres: not a permit
    t.write(victim, original + "\ndouble injected_km = value * 1e3;  // UNIT-CROSSING: m -> km\n");
    const Result misplaced = t.run({"--quiet"});
    CHECK(misplaced.code == kFailed);
    CHECK(misplaced.err == "\nA KM<->M CROSSING OUTSIDE core/units.hpp:\n  " + victim + ":" + std::to_string(next_line) + "  annotated 'm -> km'\n" + kMisplacedTrailer);
    CHECK(printed_counts(misplaced.out)[6] == 1);     // naming km OUTSIDE

    // (3) an annotated conversion that names no kilometre, and a declared non-conversion: both pass, and the register grows by one each
    t.write(victim, original + "\ndouble injected_mas = value * 1e3;  // UNIT-CROSSING: mas -> rad\n// NOT-A-UNIT-CROSSING: a test constant\ndouble injected_row_count = 1000;\n");
    const Result honest = t.run({"--quiet"});
    CHECK(honest.code == kOk);
    const std::vector<std::size_t> grown = printed_counts(honest.out);
    REQUIRE(grown.size() == 8);
    CHECK(grown[1] == base[1] + 2);   // literals
    CHECK(grown[3] == base[3] + 1);   // UNIT-CROSSING
    CHECK(grown[4] == base[4] + 1);   // NOT-A-UNIT-CROSSING
    CHECK(grown[5] == 0);

    // (4) the marker two lines above is too far
    t.write(victim, original + "\n// NOT-A-UNIT-CROSSING: too far away\n\ndouble injected_far = 0.001;\n");
    const Result far = t.run({"--quiet"});
    CHECK(far.code == kFailed);
    CHECK(far.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row(victim, next_line + 2, "0.001", "double injected_far = 0.001;") + kUnaccountedTrailer);

    // (5) the forms the Python could not see (group C6): a standard suffix, a digit separator, and a literal that follows a // inside a string
    const std::vector<std::pair<std::string, std::string>> invisible_to_the_python = {
        {"double injected_f = value * 1000.0f;", "1000.0f"},
        {"unsigned injected_u = 1000u;", "1000u"},
        {"long injected_sep = 1'000;", "1'000"},
        {"const char* injected_url = \"http://x\"; double injected_scale = 1e3;", "1e3"},
    };
    for (const auto& [text, literal] : invisible_to_the_python) {
        INFO(text);
        t.write(victim, original + "\n" + text + "\n");
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kFailed);
        CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row(victim, next_line, literal, text) + kUnaccountedTrailer);
    }
}
