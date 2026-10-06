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
    std::size_t sources = 0, home = 0, crossings = 0, declared = 0, unaccounted = 0, misplaced = 0;
};

// the seven lines of the denominator, from the print statements: the labels are 34, 35 and (four times) 51 characters wide before the number, the number is 5 wide
std::string denominator(const Counts& c) {
    const std::size_t n = c.home + c.crossings + c.declared + c.unaccounted + c.misplaced;
    return "\n" + left("  production sources searched", 34) + right(c.sources, 5) + "\n" + "  literals of value 1000 or 1/1000 " + right(n, 5) + "\n" + "    in " + left(kHome, 44) + " " + right(c.home, 5) + "\n" +
           left("    annotated UNIT-CROSSING", 51) + right(c.crossings, 5) + "\n" + left("    annotated NOT-A-UNIT-CROSSING", 51) + right(c.declared, 5) + "\n" +
           left("    ACCOUNTED FOR BY NEITHER", 51) + right(c.unaccounted, 5) + "\n" + left("    naming km OUTSIDE core/units.hpp", 51) + right(c.misplaced, 5) + "\n";
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
    CHECK(tokens("1000f") == "");
    CHECK(tokens("1000.0f") == "");                      // a float literal with a suffix is invisible to this register
    CHECK(tokens("1000u 1000UL 1e3f 1e3L") == "");
    CHECK(tokens("x1000") == "");
    CHECK(tokens("_1000") == "");
    CHECK(tokens("1000_") == "");
    CHECK(tokens("1_000") == "");
    CHECK(tokens("a.1000") == "");
    CHECK(tokens("1000.f") == "");
    CHECK(tokens("1000..") == "");
    CHECK(tokens("1000.0.0") == "");
    CHECK(tokens("1.5.3") == "");
    CHECK(tokens("1.0e3.5") == "");
    CHECK(tokens("1e") == "");
    CHECK(tokens("1e+") == "");
    CHECK(tokens("1e3e") == "");
    CHECK(tokens("0x3e8") == "");
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

TEST_CASE("is_thousand: float(token) is exactly 1000.0 or exactly 0.001, however it is spelt", "[unitcheck][behaviour][scan]") {
    for (const char* spelling : {"1000", "1000.0", "1000.", "1e3", "1E3", "1.0e3", "1.0E+3", "1e+3", "10e2", "100e1", "1000e0", "0.1e4", "01000", "0001000.000", "1.0e+03", "0.001", "1e-3", "1E-3", "1.0e-03",
                                 "0.0010", "000.001", "10e-4", "100e-5", "0.00100000000000000000000001", "1000.0000000000000000000000000001"}) {
        INFO(spelling);
        CHECK(uc::scan::is_thousand(spelling));
    }
    for (const char* spelling : {"1001", "999.9999999", "1000.000000001", "100", "10000", "1e4", "1e2", "1e-2", "1e-4", "0.01", "0.0001", "0.0010000001", "1e999", "1e-999", "0", "0.0", "1", "1e0",
                                 "999", "1000.5"}) {
        INFO(spelling);
        CHECK_FALSE(uc::scan::is_thousand(spelling));
    }
    CHECK(uc::scan::is_thousand("\xD9\xA1\xD9\xA0\xD9\xA0\xD9\xA0"));                               // ARABIC-INDIC DIGITS: float() reads them
    CHECK_FALSE(uc::scan::is_thousand("1000x"));                                                    // float() refuses what is not wholly a number
    CHECK(uc::scan::is_thousand("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"));                // FULLWIDTH DIGITS
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
    t.write("modules/b/include/b.hpp", "/* 1000 in a block comment line is skipped */\n * 1000 continuation\n// 1000 in a line comment\ndouble t = 0.001; // NOT-A-UNIT-CROSSING: a tolerance\n");
    const Counts c{3, 2, 1, 2, 0, 0};
    const Result loud = t.run();
    CHECK(loud.code == kOk);
    CHECK(loud.err.empty());
    CHECK(loud.out == std::string(kHeading) + "  the crossing itself, in " + kHome + ":\n" + home_row(2, "1000.0", "constexpr double kMetresPerKm = 1000.0;") +
                          home_row(3, "1e-3", "constexpr double kKmPerMetre = 1e-3; // reverse") + "\n  unit conversions, 1, each naming what it converts:\n" +
                          annotated_row("modules/a/a.cpp", 1, "1e3", "mas -> rad") + "\n  not conversions at all, 2:\n" + annotated_row("modules/a/a.cpp", 3, "1000", "a date radix") +
                          annotated_row("modules/b/include/b.hpp", 4, "0.001", "a tolerance") + denominator(c) + kVerdict);
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
                              annotated_row("modules/a/lb.cpp", 2, "1000", "above") + annotated_row("modules/a/lb.cpp", 13, "1000", "indented above") + denominator(Counts{1, 0, 1, 2, 4, 0})));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/lb.cpp", 3, "1000", "int b = 1000;") + unaccounted_row("modules/a/lb.cpp", 6, "1000", "int c = 1000;") +
                       unaccounted_row("modules/a/lb.cpp", 8, "1000", "int e = 1000;") + unaccounted_row("modules/a/lb.cpp", 11, "1000", "int g = 1000; /* NOT-A-UNIT-CROSSING: block */") +
                       kUnaccountedTrailer);
}

TEST_CASE("literals are matched by VALUE: every spelling of 1000 and 1/1000 is one, no other number is; two on a line are two rows", "[unitcheck][behaviour]") {
    const std::vector<std::string> spellings = {"1e3", "1E3", "1.0e+3", "1000.", "1000.0", "0.001", "1.0e-03", "1E-3", "1e+3", "10e2", "100e1", "01000", "1000e0", "0.1e4"};
    std::string annotated;
    for (const std::string& s : spellings) annotated += "double v = " + s + "; // NOT-A-UNIT-CROSSING: spelling\n";
    annotated += "double w = 1e3 + 1000; // NOT-A-UNIT-CROSSING: two on one line\n";
    Tree t;
    t.write("modules/a/spelt.cpp", annotated);
    // none of these is a factor of a thousand (and the last five are not tokens at all: a suffix, a letter or a point is glued to them)
    t.write("modules/a/other.cpp",
            "double v[] = {1000.5, 999.99, 1e4, 1e-4, 100, 0.01, 1001, 0.0010000001, 1e2, 1000.000000001, 1e999};\n"
            "float f = 1000.0f; unsigned u = 1000u; double x1000 = 0; double y = a.1000; double z = 1_000;\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kOk);
    CHECK(r.out == denominator(Counts{2, 0, 0, spellings.size() + 2, 0, 0}) + kVerdict);
    const Result loud = t.run();
    for (std::size_t i = 0; i < spellings.size(); ++i) CHECK(contains(loud.out, annotated_row("modules/a/spelt.cpp", i + 1, spellings[i], "spelling")));
    CHECK(contains(loud.out, annotated_row("modules/a/spelt.cpp", spellings.size() + 1, "1e3", "two on one line") + annotated_row("modules/a/spelt.cpp", spellings.size() + 1, "1000", "two on one line")));
}

TEST_CASE("what is not looked at: a line that begins with //, * or /*, and everything after the first // of any other line (a // inside a string hides the rest of the line too)", "[unitcheck][behaviour]") {
    Tree t;
    t.write("modules/a/skips.cpp",
            "// 1000\n"
            "   // 1000\n"
            "/* 1000 */\n"
            " * 1000\n"
            "*1000\n"
            "int a = 5; // 1000\n"
            "const char* u = \"http://x\"; int n = 1000;\n"     // the // inside the string hides the 1000: the Python's behaviour, kept
            "int b = 5; /* 1000 */\n"                            // a block comment that begins after code hides nothing: the register is not a parser, so this one counts
            "\t\xE3\x80\x80" "int c = 5;\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kFailed);
    CHECK(r.out == denominator(Counts{1, 0, 0, 0, 1, 0}));
    CHECK(r.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row("modules/a/skips.cpp", 8, "1000", "int b = 5; /* 1000 */") + kUnaccountedTrailer);
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
    std::size_t sources = 0, home = 0, crossings = 0, declared = 0, unaccounted = 0, misplaced = 0;
};

std::string lstrip_blanks(const std::string& s) {
    const std::size_t i = s.find_first_not_of(" \t\f\v\r");
    return i == std::string::npos ? std::string() : s.substr(i);
}

// the register of the real production sources, made a second way: std::regex (ECMAScript, ASCII word characters) in place of the hand-written scanners, `strtod` for the value, the standard
// directory iterator for the files; it shares no code with the tool.  The tree is ASCII wherever a number touches a letter, which the equality of the two counts also shows.
Recount recount(const fs::path& root) {
    static const std::regex number(R"re((^|[^\w.])(\d+\.?\d*(?:[eE][+-]?\d+)?)(?![\w.]))re");
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
        std::string text = read_text(it->path());
        std::vector<std::string> lines;
        for (std::size_t from = 0;;) {
            const std::size_t nl = text.find('\n', from);
            if (nl == std::string::npos) {
                lines.push_back(text.substr(from));
                break;
            }
            lines.push_back(text.substr(from, nl - from));
            from = nl + 1;
        }
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];
            const std::string head = lstrip_blanks(line);
            if (head.rfind("//", 0) == 0 || head.rfind("*", 0) == 0 || head.rfind("/*", 0) == 0) continue;
            const std::string code = line.substr(0, line.find("//"));
            if (code.find_first_of("0123456789") == std::string::npos) continue;   // no digit, no number: spares the slow regular expression most of the tree
            std::size_t found = 0;
            for (std::sregex_iterator m(code.begin(), code.end(), number), done; m != done; ++m) {
                const double v = std::strtod((*m)[2].str().c_str(), nullptr);
                if (v == 1000.0 || v == 0.001) ++found;
            }
            if (found == 0) continue;
            std::smatch mark;
            bool marked = std::regex_search(line, mark, marker);
            if (!marked && i >= 1 && lstrip_blanks(lines[i - 1]).rfind("//", 0) == 0) marked = std::regex_search(lines[i - 1], mark, marker);
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

// the seven numbers a quiet run prints, in the order of its lines
std::vector<std::size_t> printed_counts(const std::string& out) {
    static const std::regex line(R"re((?:searched|1/1000|units\.hpp|CROSSING|NEITHER|units\.hpp) +(\d+)\n)re");
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
    CHECK(r.out == denominator(Counts{c.sources, c.home, c.crossings, c.declared, c.unaccounted, c.misplaced}) + kVerdict);
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
    REQUIRE(base.size() == 7);
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
    REQUIRE(grown.size() == 7);
    CHECK(grown[1] == base[1] + 2);   // literals
    CHECK(grown[3] == base[3] + 1);   // UNIT-CROSSING
    CHECK(grown[4] == base[4] + 1);   // NOT-A-UNIT-CROSSING
    CHECK(grown[5] == 0);

    // (4) the marker two lines above is too far
    t.write(victim, original + "\n// NOT-A-UNIT-CROSSING: too far away\n\ndouble injected_far = 0.001;\n");
    const Result far = t.run({"--quiet"});
    CHECK(far.code == kFailed);
    CHECK(far.err == "\nUNACCOUNTED FACTOR OF A THOUSAND:\n" + unaccounted_row(victim, next_line + 2, "0.001", "double injected_far = 0.001;") + kUnaccountedTrailer);
}
