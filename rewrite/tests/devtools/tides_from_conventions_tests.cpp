// tests/devtools/tides_from_conventions_tests.cpp — the tidal EOP coefficients out of the document (plan L0 step 8, group C10): the number pattern and its decimal digits of any script, the lines of pdftotext's
// output, the rows of the tables of chapters 5, 6 and 8 with every filter, the consistency relation of Tables 6.5a/b/c by hand, the generated text row by row, the command line, the whole tool on synthetic
// "PDFs" through the in-process seam and through a stand-in program (tests/devtools/fake_pdftotext.cpp), every refusal, and — on the real tree — the committed headers: their rows re-emitted byte for byte,
// their hashes against the manifest and the cache, their counts and the consistency relation against the control written before the port.
// (ctests `tides_from_conventions.behaviour` and `tides_from_conventions.real_tree`; the hidden case [.regeneration] runs the real pdftotext on the real PDFs when asked for by tag.)
//
// pdftotext is a regeneration-only host program (poppler, GPL: not part of the build, the tests or CI, not declared in the manifest, not redistributed — the maintainer's ruling D9): no case of
// [behaviour] or [real_tree] needs it.
//
// Every expectation is DERIVED BY HAND from the statements of tides_from_conventions.py (read from git, never run), or comes from the committed headers (produced by the Python on 2026-09-18) and from the
// control written before the port (scratchpad c10/controls/tides_expected_stderr.sh, an awk recount from the committed headers alone); none is taken from running the port to see what it says.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/tool.hpp>

#include "tides_from_conventions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;
namespace tc = odl::tools::tides_from_conventions;
using namespace odl::devkit;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const tc::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = tc::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) out += (i != 0 ? sep : std::string()) + v.at(i);
    return out;
}

// printf-style "%*.*f" and "%*d": the formats of Python's  {x:W.Pf}  and  {n:Wd}, written here with the C library rather than the devkit
std::string pf(double v, int width, int precision) {
    char buf[128];
    std::snprintf(buf, sizeof buf, "%*.*f", width, precision, v);
    return buf;
}
std::string pd(long long v, int width) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%*lld", width, v);
    return buf;
}

// Environment variables set for the length of a case and put back after it.
class EnvScope {
public:
    EnvScope() = default;
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
    ~EnvScope() {
        for (const auto& [name, saved] : saved_) {
            if (saved.has_value()) ::setenv(name.c_str(), saved->c_str(), 1);
            else ::unsetenv(name.c_str());
        }
    }
    void set(const std::string& name, const std::string& value) {
        remember(name);
        ::setenv(name.c_str(), value.c_str(), 1);
    }
    void unset(const std::string& name) {
        remember(name);
        ::unsetenv(name.c_str());
    }

private:
    void remember(const std::string& name) {
        if (saved_.count(name) != 0) return;
        const char* old = std::getenv(name.c_str());
        saved_.emplace(name, old != nullptr ? std::optional<std::string>(old) : std::nullopt);
    }
    std::map<std::string, std::optional<std::string>> saved_;
};

// where the stand-in `pdftotext` is: fakebin/ beside this executable (tools/CMakeLists.txt builds it there), found at run time and not compiled in (a build path inside an executable would make two builds differ)
fs::path fake_pdftotext() { return fs::canonical("/proc/self/exe").parent_path() / "fakebin" / "pdftotext"; }

// ---- synthetic tables ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// a row of a table of chapters 5 or 8: a lead (a name, perhaps a degree column), six multipliers, the Doodson number, the period, four coefficients
std::string tide_line(const std::string& lead, const std::array<std::string, 6>& m, const std::string& doodson, const std::string& period, const std::array<std::string, 4>& c) {
    return lead + "   " + join({m.begin(), m.end()}, "  ") + "   " + doodson + "   " + period + "   " + join({c.begin(), c.end()}, "   ") + "\n";
}

// chapter 8: Tables 8.2a (2 rows), 8.2b (1), 8.3a (3), 8.3b (1), a caption that stops 8.3b, and a row after it that must not be read.  Row values are chosen so that the emitted text is checked by hand.
std::string chapter8_text() {
    std::string t = "                       IERS Conventions (2010) chapter 8  12 3.5 6.7 (numbers that are no rows)\n";
    t += "Table 8.2a: Ocean tide diurnal variations in the pole coordinates\n";
    t += "  Doodson   Name   l  l' F D Om    period   xp sin  xp cos  yp sin  yp cos   (a header with no rows)\n";
    t += tide_line("  Q1", {"1", "-2", "0", "-2", "0", "0"}, "135.655", "1.1195149", {"-0.05", "0.01", "-0.06", "0.02"});
    t += tide_line("  O1", {"1", "-1", "0", "0", "0", "0"}, "145.555", "1.0758059", {"-0.26", "0.51", "0.10", "-0.04"});
    t += "Table 8.2b: Ocean tide semidiurnal variations in the pole coordinates\n";
    t += tide_line("  N2", {"2", "-1", "0", "-2", "0", "0"}, "245.655", "0.5274312", {"0.1", "-0.2", "0.3", "-0.4"});
    t += "Table 8.3a: Ocean tide diurnal variations in UT1 and LOD\n";
    t += tide_line("  Q1", {"1", "-2", "0", "-2", "0", "0"}, "135.655", "1.1195149", {"1.5", "-2.5", "3.5", "-4.5"});
    t += tide_line("  O1", {"1", "-1", "0", "0", "0", "0"}, "145.555", "1.0758059", {"2.25", "0.0", "-1.0", "7.75"});
    t += tide_line("  K1", {"1", "1", "0", "0", "0", "0"}, "165.555", "0.9972696", {"0.5", "0.5", "0.5", "0.5"});
    t += "Table 8.3b: Ocean tide semidiurnal variations in UT1 and LOD\n";
    t += tide_line("  M2", {"2", "0", "0", "0", "0", "0"}, "255.555", "0.5175252", {"-10.0", "20.0", "-30.0", "40.0"});
    t += "Table 8.4: something else\n";
    t += tide_line("  XX", {"9", "9", "9", "9", "9", "9"}, "999.999", "9.9999999", {"9.0", "9.0", "9.0", "9.0"});   // after the stop: not read
    return t;
}

// chapter 5: Table 5.1a (2 rows, each with a leading degree column), Table 5.1b (1), then 5.5.4
std::string chapter5_text() {
    std::string t = "IERS Conventions (2010) chapter 5\n";
    t += "Table 5.1a: Libration in pole coordinates\n";
    t += tide_line("  2   Q1", {"1", "-2", "0", "-2", "0", "0"}, "135.655", "1.1195149", {"3.0", "-4.0", "5.0", "-6.0"});
    t += tide_line("  3   O1", {"1", "-1", "0", "0", "0", "0"}, "145.555", "1.0758059", {"0.125", "0.25", "0.5", "1.0"});
    t += "Table 5.1b: Libration in UT1 and LOD\n";
    t += tide_line("  2   M2", {"2", "0", "0", "0", "0", "0"}, "255.555", "0.5175252", {"7.0", "8.0", "9.0", "10.0"});
    t += "5.5.4 Something else\n";
    t += tide_line("  XX", {"9", "9", "9", "9", "9", "9"}, "999.999", "9.9999999", {"9.0", "9.0", "9.0", "9.0"});
    return t;
}

// a row of Tables 6.5a/b/c: a name, the Doodson number with a comma (not a number), deg/hr, six multipliers, five Delaunay multipliers, and the tail
std::string ch6_line(const std::string& name, double deg, const std::array<int, 6>& dood, const std::array<int, 5>& del, const std::vector<std::string>& tail) {
    std::string s = "  " + name + "  125,755  " + pf(deg, 0, 5);
    for (const int x : dood) s += " " + std::to_string(x);
    for (const int x : del) s += " " + std::to_string(x);
    for (const std::string& t : tail) s += "  " + t;
    return s + "\n";
}

// chapter 6: 48 rows in 6.5a (dk_real 1.0, dk_imag 2.0, amp_ip 1.0, amp_op 2.0 -- consistent -- except the seventh, whose amp_op is 1.0), 21 in 6.5b (printed dk_real, amp_ip, dk_imag, amp_op = 1.0, 1.0, 2.0, 2.0),
// and 2 in 6.5c (dk_real, amp_ip); `n_a`, `n_b`, `n_c` change the counts
std::string chapter6_text(std::size_t n_a = 48, std::size_t n_b = 21, std::size_t n_c = 2) {
    std::string t = "IERS Conventions (2010) chapter 6\n";
    t += "Table 6.5a: Corrections for the frequency dependence of k21 (units 1e-5)\n";
    t += "  Name  Doodson  deg/hr  tau s h p N' ps  l l' F D Om  dkR dkI Ip Op   (header)\n";
    for (std::size_t i = 0; i < n_a; ++i) {
        const double deg = 10.0 + 0.5 * static_cast<double>(i);
        t += ch6_line("a" + std::to_string(i), deg, {1, -2, 0, 1, 0, static_cast<int>(i % 5) - 2}, {1, 0, 0, 2, -1}, {"1.0", "2.0", "1.0", i == 6 ? "1.0" : "2.0"});
    }
    t += "Table 6.5b: Corrections for the frequency dependence of k20\n";
    for (std::size_t i = 0; i < n_b; ++i) t += ch6_line("b" + std::to_string(i), 0.5 + 0.25 * static_cast<double>(i), {0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 1}, {"1.0", "1.0", "2.0", "2.0"});
    t += "Table 6.5c: Corrections for the frequency dependence of k22\n";
    for (std::size_t i = 0; i < n_c; ++i) t += ch6_line("c" + std::to_string(i), 30.0 + static_cast<double>(i), {2, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 1}, {"-0.5", "3.0"});
    t += "6.2.2 Something else\n";
    t += ch6_line("zz", 37.0, {2, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 1}, {"-0.5", "3.0"});   // after the stop: not read
    return t;
}

// The three "PDFs" of a run (their bytes ARE the text the stand-in writes), and a settings object whose seam reads them.
struct Pdfs {
    TempDir td{"odl-tides"};
    fs::path root = td.path();
    fs::path ch5 = root / "icc5.pdf";
    fs::path ch8 = root / "icc8.pdf";
    fs::path ch6 = root / "icc6.pdf";
    explicit Pdfs(const std::string& t5 = chapter5_text(), const std::string& t8 = chapter8_text(), const std::string& t6 = chapter6_text()) {
        write_text(ch5, t5);
        write_text(ch8, t8);
        write_text(ch6, t6);
    }
    // the seam: the stand-in is not run; the file's text is returned
    [[nodiscard]] tc::Settings seam() const {
        tc::Settings s;
        s.pdftotext = [](const fs::path& pdf) { return read_text(pdf); };
        return s;
    }
    [[nodiscard]] tc::Settings program() const {
        tc::Settings s;
        s.program = fake_pdftotext().string();
        return s;
    }
    [[nodiscard]] std::vector<std::string> args(bool with_ch6 = false) const {
        std::vector<std::string> a = {"--ch5", ch5.string(), "--ch8", ch8.string(), "--out", (root / "out" / "tide_tables.hpp").string()};
        if (with_ch6) {
            a.push_back("--ch6");
            a.push_back(ch6.string());
            a.push_back("--out-ch6");
            a.push_back((root / "out6" / "solid_tide_tables.hpp").string());
        }
        return a;
    }
};

const char kUsage[] = "usage: tides_from_conventions [-h] --ch5 CH5 --ch8 CH8 --out OUT [--ch6 CH6] [--out-ch6 OUT_CH6]\n";

// What `-h` prints after the usage line: the port's own text (the Python's argparse help was wrapped to the terminal's width and cannot be reproduced byte for byte), RECORDED here whole so that every word of it is
// pinned, not only the lines that the fragments in the command-line case look at.
const char kHelp[] =
    "\n"
    "The tidal EOP coefficients, out of the document: the tables of chapters 5, 6 and 8 of the IERS Conventions (2010), read from the pinned PDFs through `pdftotext -layout` (a regeneration-only host program) and\n"
    "written as generated source.  Usage: tides_from_conventions --ch5 icc5.pdf --ch8 icc8.pdf --out <header> [--ch6 icc6.pdf --out-ch6 <header>]\n"
    "\n"
    "options:\n"
    "  -h, --help          show this help and exit\n"
    "  --ch5 CH5           chapter 5 (Table 5.1: libration)\n"
    "  --ch8 CH8           chapter 8 (Tables 8.2 and 8.3: ocean tides)\n"
    "  --out OUT           the header for chapters 5 and 8 (modules/eop/src/tide_tables.hpp)\n"
    "  --ch6 CH6           IERS Conventions chapter 6, for Tables 6.5a/b/c\n"
    "  --out-ch6 OUT_CH6   where to write the solid Earth tide tables (modules/tides/src/solid_tide_tables.hpp); goes with --ch6\n"
    "\n"
    "exit codes: 0 written   1 a table is not found or its row count is not the expected one, or --ch6 and --out-ch6 do not go together   2 an argument error, or pdftotext or an input or output path failed\n"
    "            70 an error the tool did not anticipate\n";

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the number pattern

TEST_CASE("is_numeric: the Python's  ^[+-]?(\\d+\\.?\\d*|\\.\\d+)$  with decimal digits of any script, by hand", "[tides_from_conventions][behaviour]") {
    for (const char* yes : {"5", "+5", "-5", "0", "007", "5.", "5.5", "-5.25", "+0.5", ".5", "-.5", "+.5", "12345.6789", "1.", "-1.", "0.0"}) {
        INFO(yes);
        CHECK(tc::is_numeric(yes));
    }
    for (const char* no : {"", ".", "+", "-", "+-5", "--5", "5.5.5", "1e5", "1E5", "5-", "5+", "5,5", "5 5", "abc", "a5", "5a", "+.", "-.", "..5", "5..", "0x10", "1_0", "inf", "nan", "NaN", "135,655",
                           "\xE2\x88\x92" "5"}) {   // the last: U+2212 followed by 5 -- to_lines() has replaced that sign before, so it is no number here
        INFO(no);
        CHECK_FALSE(tc::is_numeric(no));
    }
    // Python's \d is every Unicode decimal digit (category Nd): Arabic-Indic three, Devanagari five, fullwidth five, mixed with ASCII; a superscript two, a fraction and a roman numeral are not digits
    CHECK(tc::is_numeric("\xD9\xA3"));               // U+0663
    CHECK(tc::is_numeric("\xD9\xA3.\xD9\xA5"));      // 3.5 in Arabic-Indic digits
    CHECK(tc::is_numeric("-\xE0\xA5\xAB"));          // -5 in Devanagari (U+096B)
    CHECK(tc::is_numeric("\xEF\xBC\x95"));           // U+FF15
    CHECK(tc::is_numeric("1\xD9\xA3"));
    CHECK(tc::is_numeric(".\xD9\xA3"));
    CHECK_FALSE(tc::is_numeric("\xC2\xB2"));         // U+00B2, superscript two
    CHECK_FALSE(tc::is_numeric("\xC2\xBD"));         // U+00BD
    CHECK_FALSE(tc::is_numeric("\xE2\x85\xA3"));     // U+2163, roman numeral four
    CHECK_FALSE(tc::is_numeric("5\xC2\xB2"));
    CHECK_FALSE(tc::is_numeric("\xFF"));             // not UTF-8 at all

    CHECK(tc::numeric_value("5") == 5.0);
    CHECK(tc::numeric_value("-5.5") == -5.5);
    CHECK(tc::numeric_value("+.5") == 0.5);
    CHECK(tc::numeric_value("5.") == 5.0);
    CHECK(tc::numeric_value(".25") == 0.25);
    CHECK(tc::numeric_value("007") == 7.0);
    CHECK(tc::numeric_value("0.1") == 0.1);
    CHECK(tc::numeric_value("-0") == 0.0);
    CHECK(std::signbit(tc::numeric_value("-0")));
    CHECK(tc::numeric_value("\xD9\xA3.\xD9\xA5") == 3.5);       // float("٣.٥") is 3.5
    CHECK(tc::numeric_value("-\xE0\xA5\xAB") == -5.0);
    CHECK(tc::numeric_value("1\xD9\xA2") == 12.0);               // 1 and Arabic-Indic two
    CHECK(tc::numeric_value("\xEF\xBC\x95\xEF\xBC\x95") == 55.0);
    CHECK(std::isinf(tc::numeric_value(std::string(400, '9'))));   // a number beyond the double range reads as infinity (table_rows refuses it)
    // the zero of another script is the digit 0, written out (not its UTF-8 bytes, where strtod would stop): it counts after a non-zero digit, before a point and at the end
    CHECK(tc::numeric_value("1\xD9\xA0") == 10.0);                 // 1 and Arabic-Indic zero (U+0660)
    CHECK(tc::numeric_value("5\xEF\xBC\x90") == 50.0);             // 5 and fullwidth zero (U+FF10)
    CHECK(tc::numeric_value("\xEF\xBC\x90.5") == 0.5);             // fullwidth zero, a point, 5
    CHECK(tc::numeric_value("\xE0\xA5\xA6\xE0\xA5\xA7") == 1.0);   // Devanagari zero and one (U+0966, U+0967): 01
    // neither function reads past the end of the view it is given (the tokens of the tool are views into longer lines): the same text as a view into a longer buffer, whatever follows it there, gives what a copy of
    // the text on its own gives
    for (const std::string text : {"", "+", "-", ".", "+.", "5", "5.", "-5.", ".5", "\xD9\xA3", "5\xD9\xA0", "\xD9\xA0.", "+\xD9\xA3"}) {
        for (const char next : {'7', '0', '.', 'x', '+'}) {
            const std::string buffer = text + next + "9";
            const std::string_view view = std::string_view(buffer).substr(0, text.size());
            INFO("the text '" << text << "' followed by '" << next << "'");
            CHECK(tc::is_numeric(view) == tc::is_numeric(text));
            CHECK(tc::numeric_value(view) == tc::numeric_value(text));
        }
    }
}

TEST_CASE("to_lines: universal newlines, the PDF's minus sign as a hyphen, then str.splitlines()", "[tides_from_conventions][behaviour]") {
    CHECK(tc::to_lines("") == std::vector<std::string>{});
    CHECK(tc::to_lines("a\nb\n") == std::vector<std::string>{"a", "b"});
    CHECK(tc::to_lines("a\r\nb\rc\n") == std::vector<std::string>{"a", "b", "c"});
    CHECK(tc::to_lines("a\n\nb") == std::vector<std::string>{"a", "", "b"});
    CHECK(tc::to_lines("\n\n") == std::vector<std::string>{"", ""});
    CHECK(tc::to_lines("x \xE2\x88\x92" "1.5 \xE2\x88\x92" "2\n") == std::vector<std::string>{"x -1.5 -2"});
    // splitlines also breaks at a form feed (pdftotext puts one at the end of every page), a vertical tab, the file/group/record separators, NEL, and the Unicode line and paragraph separators
    CHECK(tc::to_lines("page one\x0c" "page two\n") == std::vector<std::string>{"page one", "page two"});
    CHECK(tc::to_lines("a\x0b" "b") == std::vector<std::string>{"a", "b"});
    CHECK(tc::to_lines("a\x1c" "b\x1d" "c\x1e" "d") == std::vector<std::string>{"a", "b", "c", "d"});
    CHECK(tc::to_lines("a\xC2\x85" "b") == std::vector<std::string>{"a", "b"});
    CHECK(tc::to_lines("a\xE2\x80\xA8" "b\xE2\x80\xA9" "c") == std::vector<std::string>{"a", "b", "c"});
    // a trailing blank line is kept, a trailing line end is not a line
    CHECK(tc::to_lines("a\n \n") == std::vector<std::string>{"a", " "});
    // only U+2212 is replaced; an en dash and a hyphen-minus stay
    CHECK(tc::to_lines("\xE2\x80\x93" "1 -1") == std::vector<std::string>{"\xE2\x80\x93" "1 -1"});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the rows

TEST_CASE("table_rows: the rows between a caption and the next, fields from the end, every filter", "[tides_from_conventions][behaviour]") {
    const std::vector<std::string> lines = tc::to_lines(chapter8_text());
    const tc::Rows a = tc::table_rows(lines, "Table 8.2a:", {"Table 8.2b:"});
    REQUIRE(a.size() == 2);
    // the row order is the table's: Q1 then O1; the six multipliers, the PERIOD (not the Doodson number), the four coefficients
    CHECK(a.at(0) == tc::Row{1, -2, 0, -2, 0, 0, 1.1195149, -0.05, 0.01, -0.06, 0.02});
    CHECK(a.at(1) == tc::Row{1, -1, 0, 0, 0, 0, 1.0758059, -0.26, 0.51, 0.10, -0.04});
    const tc::Rows b = tc::table_rows(lines, "Table 8.2b:", {"Table 8.3a:"});
    REQUIRE(b.size() == 1);
    CHECK(b.at(0) == tc::Row{2, -1, 0, -2, 0, 0, 0.5274312, 0.1, -0.2, 0.3, -0.4});
    CHECK(tc::table_rows(lines, "Table 8.3a:", {"Table 8.3b:"}).size() == 3);
    // the last table ends at the first of several stop captions, and what follows it is not read
    const tc::Rows d = tc::table_rows(lines, "Table 8.3b:", {"Table 8.4", "8.3.2", "Table 8.5"});
    REQUIRE(d.size() == 1);
    CHECK(d.at(0) == tc::Row{2, 0, 0, 0, 0, 0, 0.5175252, -10.0, 20.0, -30.0, 40.0});
    // without the stop caption the row after it is read too
    CHECK(tc::table_rows(lines, "Table 8.3b:", {"never"}).size() == 2);
    // the stop is a caption that BEGINS the stripped line
    CHECK(tc::table_rows(lines, "Table 8.3b:", {"Table 8.4"}).size() == 1);
    CHECK(tc::table_rows(lines, "Table 8.3b:", {"able 8.4"}).size() == 2);
    // the first caption wins when there are several; text before it is not read
    const std::vector<std::string> twice = {"  Table 9.1: first", " 1 0 0 0 0 0  100.000  1.0  1 2 3 4", " 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1", "Table 9.1: second", " 2 0 0 0 0 0  200.000  2.0  5 6 7 8"};
    CHECK(tc::table_rows(twice, "Table 9.1:", {"zz"}).size() == 2);   // the first caption wins (its leading blanks are stripped); the line of eighteen ones has no point in its seventh number; the second caption has no twelve numbers
    CHECK(tc::table_rows(twice, "Table 9.1:", {"zz"}) == tc::Rows{tc::Row{1, 0, 0, 0, 0, 0, 1.0, 1, 2, 3, 4}, tc::Row{2, 0, 0, 0, 0, 0, 2.0, 5, 6, 7, 8}});
}

TEST_CASE("table_rows: a row is the last twelve numbers, the multipliers truncated, a Doodson number with a point, a positive period", "[tides_from_conventions][behaviour]") {
    const auto rows = [](const std::vector<std::string>& body) {
        std::vector<std::string> lines = {"Table T:"};
        lines.insert(lines.end(), body.begin(), body.end());
        return tc::table_rows(lines, "Table T:", {"STOP"});
    };
    const std::string c4 = "0.5 0.25 -0.125 2.0";
    // exactly twelve numeric tokens
    CHECK(rows({"1 2 3 4 5 6 111.111 0.5 " + c4}).size() == 1);
    // eleven: skipped; thirteen and more: the first are ignored (a leading degree column, the name column are not numbers or are dropped from the front)
    CHECK(rows({"1 2 3 4 5 111.111 0.5 " + c4}).empty());
    CHECK(rows({"99 1 2 3 4 5 6 111.111 0.5 " + c4})
              == tc::Rows{tc::Row{1, 2, 3, 4, 5, 6, 0.5, 0.5, 0.25, -0.125, 2.0}});
    CHECK(rows({"7 8 9 1 2 3 4 5 6 111.111 0.5 " + c4}).at(0).at(0) == 1.0);
    // a name made of letters and digits ("2Q1", "chi1") is not a number and drops out wherever it stands
    CHECK(rows({"2Q1 1 2 3 4 5 6 111.111 0.5 " + c4}).size() == 1);
    CHECK(rows({"1 2 3 4 5 6 chi1 111.111 0.5 " + c4}).size() == 1);
    // the multipliers are int(float(x)): truncated toward zero, a sign on a zero lost; signs and points are fine
    const tc::Rows t = rows({"1.9 -1.9 +2 -0.5 -0 3. 111.111 0.5 " + c4});
    REQUIRE(t.size() == 1);
    CHECK(t.at(0).at(0) == 1.0);
    CHECK(t.at(0).at(1) == -1.0);
    CHECK(t.at(0).at(2) == 2.0);
    CHECK(t.at(0).at(3) == 0.0);
    CHECK(t.at(0).at(4) == 0.0);
    CHECK(t.at(0).at(5) == 3.0);
    // the Doodson number is the seventh of the twelve and must hold a point; the period (the eighth) must be positive
    CHECK(rows({"1 2 3 4 5 6 111111 0.5 " + c4}).empty());          // no point
    CHECK(rows({"1 2 3 4 5 6 111. 0.5 " + c4}).size() == 1);         // a point is enough
    CHECK(rows({"1 2 3 4 5 6 111.111 0 " + c4}).empty());            // period zero
    CHECK(rows({"1 2 3 4 5 6 111.111 0.0 " + c4}).empty());
    CHECK(rows({"1 2 3 4 5 6 111.111 -0.5 " + c4}).empty());         // negative
    CHECK(rows({"1 2 3 4 5 6 111.111 0.0000001 " + c4}).size() == 1); // tiny but positive
    // the coefficients are read as floats, in order, of any sign; leading and trailing blanks and tabs do not matter
    const tc::Rows c = rows({"\t  1 2 3 4 5 6   111.111\t0.5  -1.5 +2.5 .5 -.5  \t"});
    REQUIRE(c.size() == 1);
    CHECK(c.at(0) == tc::Row{1, 2, 3, 4, 5, 6, 0.5, -1.5, 2.5, 0.5, -0.5});
    // blank lines, captions and prose between rows are skipped; the stop ends the table
    CHECK(rows({"", "text 1 2 3", "1 2 3 4 5 6 111.111 0.5 " + c4, "   ", "STOP and more", "1 2 3 4 5 6 111.111 0.5 " + c4}).size() == 1);
    // decimal digits of another script are numbers too (Python's float() reads them)
    CHECK(rows({"\xD9\xA1 2 3 4 5 6 111.111 0.5 " + c4}).at(0).at(0) == 1.0);
    // a number beyond the double range cannot be a multiplier: the Python's int(float(x)) raised OverflowError (a traceback); the port refuses it
    try {
        (void)rows({std::string(400, '9') + " 2 3 4 5 6 111.111 0.5 " + c4});
        FAIL("no exception");
    } catch (const tc::BadInput& e) {
        CHECK(std::string(e.what()) == "a number in a table is too large: " + std::string(400, '9'));
    }
    // ... and the refusal names the token that overflowed, wherever it stands among the six multipliers
    try {
        (void)rows({"1 2 " + std::string(400, '9') + " 4 5 6 111.111 0.5 " + c4});
        FAIL("no exception");
    } catch (const tc::BadInput& e) {
        CHECK(std::string(e.what()) == "a number in a table is too large: " + std::string(400, '9'));
    }
    // the Doodson number, the period and the coefficients are floats and may be infinite without a refusal (Python's float() gives inf)
    CHECK(rows({"1 2 3 4 5 6 111.111 " + std::string(400, '9') + " " + c4}).size() == 1);
    // ... the Doodson number too: it is not one of the six multipliers, which are the only numbers that int() is applied to.  With no point in it the line is a stray one and is skipped; with a point it is a row
    CHECK(rows({"1 2 3 4 5 6 " + std::string(400, '9') + " 0.5 " + c4}).empty());
    CHECK(rows({"1 2 3 4 5 6 " + std::string(400, '9') + ".5 0.5 " + c4}).size() == 1);
    // the caption line itself is never a row, even when twelve numbers follow its marker on it; the row on the next line is read
    const tc::Rows after_caption = tc::table_rows(std::vector<std::string>{"Table T: 1 2 3 4 5 6 111.111 0.5 " + c4, "7 8 9 1 2 3 222.222 0.75 " + c4}, "Table T:", {"STOP"});
    REQUIRE(after_caption.size() == 1);
    CHECK(after_caption.at(0).at(0) == 7.0);
    CHECK(after_caption.at(0).at(6) == 0.75);
    // a caption that is not there
    try {
        (void)tc::table_rows(std::vector<std::string>{"Table A:", "x"}, "Table 8.2a:", {});
        FAIL("no exception");
    } catch (const tc::Fatal& e) {
        CHECK(std::string(e.what()) == "table 'Table 8.2a:' not found");
    }
    CHECK_THROWS_AS(tc::table_rows({}, "X", {}), tc::Fatal);
    // the caption must BEGIN the stripped line: a caption in the middle of a line is not one
    CHECK_THROWS_AS(tc::table_rows(std::vector<std::string>{"see Table 8.2a: here"}, "Table 8.2a:", {}), tc::Fatal);
    CHECK(tc::table_rows(std::vector<std::string>{"\t Table 8.2a: here"}, "Table 8.2a:", {}).empty());
}

TEST_CASE("ch6_specs and ch6_rows: three tables, three column orders, the filters", "[tides_from_conventions][behaviour]") {
    const std::vector<tc::Ch6Spec>& specs = tc::ch6_specs();
    REQUIRE(specs.size() == 3);
    CHECK(specs.at(0).name == "kSolidTideDiurnal");
    CHECK(specs.at(0).start == "Table 6.5a:");
    CHECK(specs.at(0).stop == std::vector<std::string>{"Table 6.5b:"});
    CHECK(specs.at(0).tail == std::vector<tc::Column>{tc::Column::DkReal, tc::Column::DkImag, tc::Column::AmpIp, tc::Column::AmpOp});
    CHECK(specs.at(0).dk_scale == 1e-5);
    CHECK(specs.at(0).what == "Table 6.5a - diurnal (m = 1) corrections for the frequency dependence of k21");
    CHECK(specs.at(1).name == "kSolidTideZonal");
    CHECK(specs.at(1).start == "Table 6.5b:");
    CHECK(specs.at(1).stop == std::vector<std::string>{"Table 6.5c:"});
    CHECK(specs.at(1).tail == std::vector<tc::Column>{tc::Column::DkReal, tc::Column::AmpIp, tc::Column::DkImag, tc::Column::AmpOp});   // the printed order of 6.5b interleaves
    CHECK(specs.at(1).dk_scale == 1.0);
    CHECK(specs.at(1).what == "Table 6.5b - zonal (m = 0) corrections for the frequency dependence of k20");
    CHECK(specs.at(2).name == "kSolidTideSemidiurnal");
    CHECK(specs.at(2).start == "Table 6.5c:");
    CHECK(specs.at(2).stop == std::vector<std::string>{"6.2.2"});
    CHECK(specs.at(2).tail == std::vector<tc::Column>{tc::Column::DkReal, tc::Column::AmpIp});   // the real part only
    CHECK(specs.at(2).dk_scale == 1.0);
    CHECK(specs.at(2).what == "Table 6.5c - semidiurnal (m = 2) corrections for k22; the real part only");
    CHECK(&tc::ch6_specs() == &specs);

    // the synthetic chapter: 48 + 21 + 2 rows; the Doodson number with a comma drops out, so a row has 1 + 6 + 5 + (4, 4, 2) numeric tokens
    const std::vector<std::string> lines = tc::to_lines(chapter6_text());
    const std::vector<tc::Ch6Row> a = tc::ch6_rows(lines, specs.at(0));
    const std::vector<tc::Ch6Row> b = tc::ch6_rows(lines, specs.at(1));
    const std::vector<tc::Ch6Row> c = tc::ch6_rows(lines, specs.at(2));
    REQUIRE(a.size() == 48);
    REQUIRE(b.size() == 21);
    REQUIRE(c.size() == 2);
    // 6.5a: the tail is dk_real, dk_imag, amp_ip, amp_op in this order
    CHECK(a.at(0).deg_per_hour == 10.0);
    CHECK(a.at(0).doodson == std::array<int, 6>{1, -2, 0, 1, 0, -2});
    CHECK(a.at(0).delaunay == std::array<int, 5>{1, 0, 0, 2, -1});
    CHECK(a.at(0).dk_real == 1.0);
    CHECK(a.at(0).dk_imag == 2.0);
    CHECK(a.at(0).amp_ip == 1.0);
    CHECK(a.at(0).amp_op == 2.0);
    CHECK(a.at(6).amp_op == 1.0);          // the seventh row is the one with the other amp_op
    CHECK(a.at(47).deg_per_hour == 10.0 + 0.5 * 47);
    CHECK(a.at(3).doodson.at(5) == 1);     // (3 % 5) - 2
    // 6.5b: dk_real, amp_ip, dk_imag, amp_op -- the printed 1.0, 1.0, 2.0, 2.0 are dk_real 1, amp_ip 1, dk_imag 2, amp_op 2
    CHECK(b.at(0).deg_per_hour == 0.5);
    CHECK(b.at(0).doodson == std::array<int, 6>{0, 0, 1, 0, 0, 0});
    CHECK(b.at(0).delaunay == std::array<int, 5>{0, 0, 0, 0, 1});
    CHECK(b.at(0).dk_real == 1.0);
    CHECK(b.at(0).amp_ip == 1.0);
    CHECK(b.at(0).dk_imag == 2.0);
    CHECK(b.at(0).amp_op == 2.0);
    CHECK(b.at(20).deg_per_hour == 0.5 + 0.25 * 20);
    // 6.5c: dk_real and amp_ip only; the other two stay zero
    CHECK(c.at(0).dk_real == -0.5);
    CHECK(c.at(0).amp_ip == 3.0);
    CHECK(c.at(0).dk_imag == 0.0);
    CHECK(c.at(0).amp_op == 0.0);
    CHECK(c.at(1).deg_per_hour == 31.0);
    // the stop ends the last table: the row after "6.2.2" is not read

    // the filters, one line at a time (a table of 6.5c's shape: want = 1 + 6 + 5 + 2 = 14 numbers)
    const tc::Ch6Spec& s = specs.at(2);
    const auto rows = [&](const std::string& body) {
        std::vector<std::string> l = {"Table 6.5c: x"};
        l.push_back(body);
        return tc::ch6_rows(l, s);
    };
    const std::string good = "20.5  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0";
    CHECK(rows(good).size() == 1);
    CHECK(rows("20.5  1 0 0 0 0 0  0 0 0 0 1  -0.5").empty());                  // 13 numbers
    CHECK(rows(good + " 7.0").empty());                                         // 15
    CHECK(rows("0.0  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());                // deg/hr must be above 0
    CHECK(rows("-1.5  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());
    CHECK(rows("40.0  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());               // and below 40
    CHECK(rows("39.99999  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").size() == 1);
    CHECK(rows("0.00001  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").size() == 1);
    CHECK(rows("20.5  21 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());              // a multiplier above 20
    CHECK(rows("20.5  20 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").size() == 1);           // 20 itself is allowed
    CHECK(rows("20.5  -21 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());
    CHECK(rows("20.5  -20 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").size() == 1);
    CHECK(rows("20.5  1 0 0 0 0 0  0 0 0 0 21  -0.5 3.0").empty());               // the last Delaunay multiplier is checked too
    CHECK(rows("20.5  1.5 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());             // not integral
    CHECK(rows("20.5  1 0 0 0 0 0  0 0 0 0 1.5  -0.5 3.0").empty());
    CHECK(rows("20.5  1.0 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").size() == 1);          // integral with a point
    CHECK(rows("20.5  1 0 0 0 0 0  0 0 0 0 1  -99.5 99.0").size() == 1);          // the tail is not a multiplier: any size
    CHECK(rows("20.5 125,755 1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").size() == 1);     // the Doodson number with a comma is no number
    CHECK(rows("20.5 125.755 1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0").empty());         // ... with a point it would be one, and the count is off
    // a caption of the same table, a header, a page number: not rows
    CHECK(rows("Table 6.5c continued 2 3").empty());
    // the caption line itself is never a row, even when a complete row follows its marker on it; the row on the next line is read
    {
        const std::vector<tc::Ch6Row> after = tc::ch6_rows(std::vector<std::string>{"Table 6.5c: 21.5  2 0 0 0 0 0  0 0 0 0 1  -0.25 4.0", "20.5  1 0 0 0 0 0  0 0 0 0 1  -0.5 3.0"}, s);
        REQUIRE(after.size() == 1);
        CHECK(after.at(0).deg_per_hour == 20.5);
        CHECK(after.at(0).dk_real == -0.5);
    }
    // the stop and the missing caption
    try {
        (void)tc::ch6_rows(std::vector<std::string>{"nothing"}, s);
        FAIL("no exception");
    } catch (const tc::Fatal& e) {
        CHECK(std::string(e.what()) == "table 'Table 6.5c:' not found");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the consistency relation

TEST_CASE("ch6_consistency: Amp(ip) dkI = Amp(op) dkR, row by row, by hand", "[tides_from_conventions][behaviour]") {
    const auto row = [](double deg, double dkr, double dki, double ip, double op) {
        tc::Ch6Row r;
        r.deg_per_hour = deg;
        r.dk_real = dkr;
        r.dk_imag = dki;
        r.amp_ip = ip;
        r.amp_op = op;
        return r;
    };
    // no imaginary column at all (dk_imag == 0 and amp_op == 0 in every row)
    CHECK(tc::ch6_consistency({row(1.0, 1.0, 0.0, 5.0, 0.0)}, "T") == "T: the table has no imaginary column; the relation does not apply");
    CHECK(tc::ch6_consistency({}, "T") == "T: the table has no imaginary column; the relation does not apply");
    // a consistent row: dkR 1, dkI 2, ip 1, op 2: residual 1*2 - 2*1 = 0; bound 0.05 * 3 = 0.15; terms max(2, 2) = 2 > bound: it constrains, with residual 0 -- the worst stays 0 and no row is named
    CHECK(tc::ch6_consistency({row(13.5, 1.0, 2.0, 1.0, 2.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 0.00 of its rounding bound");
    // an inconsistent row: op 1: residual 1*2 - 1*1 = 1; 1 / 0.15 = 6.666... -> 6.67, at its deg/hr (Python's repr of the float)
    CHECK(tc::ch6_consistency({row(13.5, 1.0, 2.0, 1.0, 1.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 6.67 of its rounding bound at 13.5 deg/hr");
    // the worst of several; ties keep the FIRST (the Python's  d > worst)
    CHECK(tc::ch6_consistency({row(1.0, 1.0, 2.0, 1.0, 1.0), row(2.0, 1.0, 2.0, 1.0, 1.0), row(3.0, 1.0, 2.0, 1.0, 2.0)}, "T") ==
          "T: 3 of 3 rows constrain it (0 lost to the printed precision), worst residual 6.67 of its rounding bound at 1.0 deg/hr");
    CHECK(tc::ch6_consistency({row(1.0, 1.0, 2.0, 1.0, 2.0), row(2.5, 1.0, 2.0, 1.0, 0.0)}, "T") ==
          "T: 2 of 2 rows constrain it (0 lost to the printed precision), worst residual 13.33 of its rounding bound at 2.5 deg/hr");   // residual 2 / 0.15
    // a row that is swamped by the printed precision: dkR 1, dkI 0.01, ip 1, op 0 -> terms max(0.01, 0) = 0.01 <= bound 0.0505: counted as applicable, constrains nothing
    CHECK(tc::ch6_consistency({row(1.0, 1.0, 0.01, 1.0, 0.0)}, "T") == "T: 0 of 1 rows constrain it (1 lost to the printed precision), worst residual 0.00 of its rounding bound");
    // and a row with neither (dkI = 0, op = 0) is not applicable: it is not counted at all
    CHECK(tc::ch6_consistency({row(1.0, 1.0, 2.0, 1.0, 2.0), row(2.0, 5.0, 0.0, 7.0, 0.0), row(3.0, 1.0, 0.01, 1.0, 0.0)}, "T") ==
          "T: 1 of 2 rows constrain it (1 lost to the printed precision), worst residual 0.00 of its rounding bound");
    // a row that has only an op amplitude (dkI = 0): dkR 1, ip 1, op 3: residual 1*0 - 3*1 = -3, |.| = 3; bound 0.05 * 1 = 0.05; terms max(0, 3) = 3: constrains, d = 60
    CHECK(tc::ch6_consistency({row(7.25, 1.0, 0.0, 1.0, 3.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 60.00 of its rounding bound at 7.25 deg/hr");
    // zero bound (both Love numbers zero) with a nonzero op: bound 0, terms 0 * ... : amp_op * dkR = 0 and amp_ip * dkI = 0, terms 0 <= bound 0: constrains nothing
    CHECK(tc::ch6_consistency({row(1.0, 0.0, 0.0, 4.0, 4.0)}, "T") == "T: 0 of 1 rows constrain it (1 lost to the printed precision), worst residual 0.00 of its rounding bound");
    // the bound is no flag: a row whose bound is exactly 1 (0.05 * 20 is the double 1.0) or exactly 0.1 (0.05 * 2 is the double 0.1) is judged like any other: dkR 20, op 3 -> residual -60, d = 60; dkR 2, op 3 -> -6 / 0.1
    REQUIRE(0.05 * 20.0 == 1.0);
    REQUIRE(0.05 * 2.0 == 0.1);
    CHECK(tc::ch6_consistency({row(12.5, 20.0, 0.0, 0.0, 3.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 60.00 of its rounding bound at 12.5 deg/hr");
    CHECK(tc::ch6_consistency({row(12.5, 2.0, 0.0, 0.0, 3.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 60.00 of its rounding bound at 12.5 deg/hr");
    // a bound that underflows to zero (0.05 times the smallest denormal is 0) while the product it is compared with does not: the row constrains, and its d is 0 -- no division by the zero bound, no row named
    REQUIRE(0.05 * std::numeric_limits<double>::denorm_min() == 0.0);
    CHECK(tc::ch6_consistency({row(12.5, std::numeric_limits<double>::denorm_min(), 0.0, 1.0, 1.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 0.00 of its rounding bound");
    // the float repr of the degree rate: 0.1 + 0.2 prints with all its digits
    CHECK(tc::ch6_consistency({row(0.1 + 0.2, 1.0, 2.0, 1.0, 1.0)}, "T") == "T: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 6.67 of its rounding bound at 0.30000000000000004 deg/hr");
    CHECK(tc::ch6_consistency({row(15.0, 1.0, 2.0, 1.0, 1.0)}, "kName") == "kName: 1 of 1 rows constrain it (0 lost to the printed precision), worst residual 6.67 of its rounding bound at 15.0 deg/hr");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the generated text

TEST_CASE("emit and emit_ch6: the rows, character by character", "[tides_from_conventions][behaviour]") {
    const tc::Rows rows = {tc::Row{1, -2, 0, -2, 0, 0, 1.1195149, -0.05, 0.01, -0.06, 0.02}, tc::Row{12, 0, -3, 100, 7, -1, 0.5274312, 1234.5678, -9999.9999, 0.0, 0.00004}};
    const std::string text = tc::emit(rows, "kName", "microarcseconds", "What it is");
    const std::vector<std::string> lines = lines_of(text);
    REQUIRE(lines.size() == 5);
    CHECK(lines.at(0) == "// What it is  (2 constituents, microarcseconds)");
    CHECK(lines.at(1) == "inline constexpr TideTerm kName[] = {");
    // {int(a):3d} joined by ", "; {period:12.7f}; {c:9.4f} joined by ", "
    CHECK(lines.at(2) == "    {{  1,  -2,   0,  -2,   0,   0},    1.1195149,   -0.0500,    0.0100,   -0.0600,    0.0200},");
    CHECK(lines.at(3) == "    {{ 12,   0,  -3, 100,   7,  -1},    0.5274312, 1234.5678, -9999.9999,    0.0000,    0.0000},");   // 1234.5678 fills its nine columns, -9999.9999 overflows them
    CHECK(lines.at(4) == "};");
    CHECK(text.back() == ';');   // no trailing newline: the header adds it
    // the same rows built with the C library's formats, for a spread of values
    const tc::Row r = {-5, 3, 20, -100, 0, 9, 123456.7890123, 0.12345, -1.5, 99999.99994, -0.00006};
    const std::string one = tc::emit({r}, "k", "u", "w");
    std::string expected = "    {{";
    for (int i = 0; i < 6; ++i) expected += (i != 0 ? ", " : "") + pd(static_cast<long long>(r[static_cast<std::size_t>(i)]), 3);
    expected += "}, " + pf(r[6], 12, 7);
    for (std::size_t i = 7; i < 11; ++i) expected += ", " + pf(r[i], 9, 4);
    expected += "},";
    CHECK(lines_of(one).at(2) == expected);
    // int(a) truncates toward zero and has no negative zero; the multipliers read from a table are integral, but emit takes any float
    const std::string trunc = tc::emit({tc::Row{1.9, -1.9, -0.5, 0.5, -0.0, 2.0, 1.0, 0.0, 0.0, 0.0, 0.0}}, "k", "u", "w");
    CHECK(lines_of(trunc).at(2) == "    {{  1,  -1,   0,   0,   0,   2},    1.0000000,    0.0000,    0.0000,    0.0000,    0.0000},");
    // no rows: the caption and the empty array
    CHECK(tc::emit({}, "kEmpty", "u", "w") == "// w  (0 constituents, u)\ninline constexpr TideTerm kEmpty[] = {\n};");
    // a negative zero coefficient prints with its sign, as Python's format does
    CHECK(contains(tc::emit({tc::Row{0, 0, 0, 0, 0, 0, 1.0, -0.0, 0.0, 0.0, 0.0}}, "k", "u", "w"), "   -0.0000,    0.0000"));
    // a coefficient whose rounding is a tie in decimal but not in binary rounds as the binary value does: 0.00005 is above the tie, 0.00015 below (as doubles)
    CHECK(contains(tc::emit({tc::Row{0, 0, 0, 0, 0, 0, 1.0, 0.00005, 0.00015, 0.5, 0.00025}}, "k", "u", "w"), ", " + pf(0.00005, 9, 4) + ", " + pf(0.00015, 9, 4) + ", " + pf(0.5, 9, 4) + ", " + pf(0.00025, 9, 4) + "}"));

    // ---- chapter 6 ----
    const tc::Ch6Spec& spec = tc::ch6_specs().at(0);
    tc::Ch6Row a;
    a.deg_per_hour = 13.39866;
    a.doodson = {1, -2, 0, -2, 0, 0};
    a.delaunay = {1, 0, 0, 2, 0};
    a.dk_real = -29.0;
    a.dk_imag = 3.0;
    a.amp_ip = -3.2;
    a.amp_op = 0.1;
    tc::Ch6Row b;   // a row of the shape of 6.5c: nothing in the last two columns
    b.deg_per_hour = 0.00441;
    b.doodson = {0, 0, 0, 0, 1, 0};
    b.delaunay = {0, 0, 0, 0, 1};
    b.dk_real = 0.0;
    b.amp_ip = 1234.5;
    const std::string t6 = tc::emit_ch6({a, b}, "kSolidTideDiurnal", spec);
    const std::vector<std::string> l6 = lines_of(t6);
    REQUIRE(l6.size() == 6);
    CHECK(l6.at(0) == "// Table 6.5a - diurnal (m = 1) corrections for the frequency dependence of k21");
    CHECK(l6.at(1) == "//   2 constituents; dk as printed (scale 1e-05), amplitudes as printed (scale 1e-12)");
    CHECK(l6.at(2) == "inline constexpr SolidTideTerm kSolidTideDiurnal[] = {");
    CHECK(l6.at(3) == "    {   13.39866, {  1,  -2,   0,  -2,   0,   0}, {  1,   0,   0,   2,   0},    -29.00000,      3.00000,     -3.2,      0.1},");
    CHECK(l6.at(4) == "    {    0.00441, {  0,   0,   0,   0,   1,   0}, {  0,   0,   0,   0,   1},      0.00000,      0.00000,   1234.5,      0.0},");
    CHECK(l6.at(5) == "};");
    CHECK(t6.back() == ';');
    // the scale is printed with %g: 1 for 1.0, 1e-05 for 1e-5
    CHECK(contains(tc::emit_ch6({}, "kSolidTideZonal", tc::ch6_specs().at(1)), "//   0 constituents; dk as printed (scale 1), amplitudes as printed (scale 1e-12)\n"));
    // the row built with the C library
    const std::string expected6 = "    {" + pf(a.deg_per_hour, 11, 5) + ", {" + pd(1, 3) + ", " + pd(-2, 3) + ", " + pd(0, 3) + ", " + pd(-2, 3) + ", " + pd(0, 3) + ", " + pd(0, 3) + "}, {" + pd(1, 3) + ", " + pd(0, 3) + ", " + pd(0, 3) + ", " +
                                  pd(2, 3) + ", " + pd(0, 3) + "}, " + pf(a.dk_real, 12, 5) + ", " + pf(a.dk_imag, 12, 5) + ", " + pf(a.amp_ip, 8, 1) + ", " + pf(a.amp_op, 8, 1) + "},";
    CHECK(l6.at(3) == expected6);
}

TEST_CASE("tide_header and ch6_header: the generated files, line by line, and the refusal of a missing table", "[tides_from_conventions][behaviour]") {
    const std::vector<std::string> names = {"kPoleOceanDiurnal", "kPoleOceanSemidiurnal", "kUt1OceanDiurnal", "kUt1OceanSemidiurnal", "kPoleLibration", "kUt1Libration"};
    std::vector<tc::NamedRows> tables;
    for (std::size_t i = 0; i < names.size(); ++i) tables.push_back({names.at(i), tc::Rows(i + 1, tc::Row{1, 0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0})});
    const std::string h = tc::tide_header("aaaa", "bbbb", tables);
    const std::vector<std::string> lines = lines_of(h);
    const std::vector<std::string> head = {
        "#pragma once",
        "// tide_tables.hpp — GENERATED.  Do not edit.",
        "//",
        "// Produced by tools/tides_from_conventions.cpp from the hash-pinned IERS",
        "// Conventions (2010) chapters in the manifest:",
        "//",
        "//   chapter 5  sha256 aaaa",
        "//   chapter 8  sha256 bbbb",
        "//",
        "// SPEC-eop.md EOP-R-007: the models are implemented from the tables PRINTED IN",
        "// THE CONVENTIONS, not from the IERS Fortran, which carries no licence.",
        "// EOP-R-008: every table here is verified against that routine's own published",
        "// test case, and a transcription that fails its test case is a build failure.",
        "//",
        "// The six multipliers are of (gamma, l, l', F, D, Omega), where gamma = GMST + pi.",
        "",
        "#include <array>",
        "",
        "namespace odl::eop::tides {",
        "",
        "struct TideTerm {",
        "    std::array<int, 6> arg;",
        "    double period_days;   // carried as DATA, not a comment: Table 5.1a is filtered",
        "                          // to its near-diurnal rows and the filter needs it",
        "    double sin1, cos1, sin2, cos2;",
        "};",
        "",
    };
    REQUIRE(lines.size() > head.size());
    for (std::size_t i = 0; i < head.size(); ++i) {
        INFO("line " << (i + 1));
        CHECK(lines.at(i) == head.at(i));
    }
    // the six tables follow in this order, each as emit() writes it, with the caption of the Python and a blank line after it
    const std::vector<std::pair<std::string, std::string>> captions = {
        {"// Table 8.2a - diurnal ocean-tide variations in pole coordinates (xp sin, xp cos, yp sin, yp cos)  (1 constituents, microarcseconds)", "kPoleOceanDiurnal"},
        {"// Table 8.2b - semidiurnal ocean-tide variations in pole coordinates  (2 constituents, microarcseconds)", "kPoleOceanSemidiurnal"},
        {"// Table 8.3a - diurnal ocean-tide variations in UT1 and LOD (UT1 sin, UT1 cos, LOD sin, LOD cos)  (3 constituents, microseconds / microseconds per day)", "kUt1OceanDiurnal"},
        {"// Table 8.3b - semidiurnal ocean-tide variations in UT1 and LOD  (4 constituents, microseconds / microseconds per day)", "kUt1OceanSemidiurnal"},
        {"// Table 5.1a - libration in pole coordinates. ONLY the near-diurnal rows are applied: TN36 5.5.1.1 says the long-period terms are already in the observed series  (5 constituents, microarcseconds)",
         "kPoleLibration"},
        {"// Table 5.1b - semidiurnal libration in UT1 and LOD  (6 constituents, microseconds / microseconds per day)", "kUt1Libration"},
    };
    std::size_t at = head.size();
    for (const auto& [caption, name] : captions) {
        INFO(name);
        REQUIRE(at + 1 < lines.size());
        CHECK(lines.at(at) == caption);
        CHECK(lines.at(at + 1) == "inline constexpr TideTerm " + name + "[] = {");
        while (at < lines.size() && lines.at(at) != "};") ++at;
        REQUIRE(at + 1 < lines.size());
        CHECK(lines.at(at + 1) == "");
        at += 2;
    }
    CHECK(lines.at(at) == "}  // namespace odl::eop::tides");
    CHECK(at + 1 == lines.size());
    CHECK(h.back() == '\n');
    // the tables are looked up by name, in any order of the argument; a missing one is a logic error naming it
    std::vector<tc::NamedRows> reversed(tables.rbegin(), tables.rend());
    CHECK(tc::tide_header("aaaa", "bbbb", reversed) == h);
    tables.erase(tables.begin() + 2);
    try {
        (void)tc::tide_header("a", "b", tables);
        FAIL("no exception");
    } catch (const std::logic_error& e) {
        CHECK(std::string(e.what()) == "the table kUt1OceanDiurnal was not given");
    }

    // chapter 6
    const std::vector<tc::Ch6Row> t_a(2), t_b(1), t_c(3);
    const std::string h6 = tc::ch6_header("cafe", {t_a, t_b, t_c});
    const std::vector<std::string> l6 = lines_of(h6);
    const std::vector<std::string> head6 = {
        "#pragma once",
        "// solid_tide_tables.hpp — GENERATED.  Do not edit.",
        "//",
        "// Produced by tools/tides_from_conventions.cpp --ch6 from the hash-pinned IERS",
        "// Conventions (2010) chapter 6:  sha256 cafe",
        "//",
        "// SPEC-perturbations PERT-R-014: Step 2's frequency-dependent corrections are",
        "// implemented from the tables PRINTED IN THE CONVENTIONS.  Several hundred",
        "// numbers is its own defect source by hand, and the IERS Fortran carries no",
        "// licence at all, so neither route is open.  Extraction is NOT part of the",
        "// build: pdftotext's layout varies with the poppler version and a build that",
        "// re-ran it would not be reproducible.",
        "//",
        "// VALUES ARE AS PRINTED so that this file can be diffed against the PDF.  The",
        "// scales are named below and applied once, in the module.",
        "//",
        "// The tail columns differ between the three tables and 6.5a's Love-number",
        "// corrections are in units of 1e-5 where 6.5b's and 6.5c's are absolute; see",
        "// CH6_TABLES in the generator.",
        "",
        "#include <array>",
        "",
        "namespace odl::tides::tables {",
        "",
        "struct SolidTideTerm {",
        "    double deg_per_hour;",
        "    std::array<int, 6> doodson;    // tau, s, h, p, N', ps",
        "    std::array<int, 5> delaunay;   // l, l', F, D, Omega",
        "    double dk_real;                // AS PRINTED",
        "    double dk_imag;                // AS PRINTED; zero where the table has no such column",
        "    double amp_ip;                 // AS PRINTED, units of 1e-12",
        "    double amp_op;                 // AS PRINTED, units of 1e-12; zero where absent",
        "};",
        "",
        "/// TN36-6 Table 6.5a prints dk in units of 1e-5.  Tables 6.5b and 6.5c print it",
        "/// absolute.  One number per table, applied once, named here.",
        "inline constexpr double kDkScaleDiurnal = 1e-05;",
        "inline constexpr double kDkScaleZonal = 1;",
        "inline constexpr double kDkScaleSemidiurnal = 1;",
        "/// Every amplitude column in all three tables is in units of 1e-12.",
        "inline constexpr double kAmplitudeScale = 1e-12;",
        "",
    };
    REQUIRE(l6.size() > head6.size());
    for (std::size_t i = 0; i < head6.size(); ++i) {
        INFO("line " << (i + 1));
        CHECK(l6.at(i) == head6.at(i));
    }
    // then the three tables of emit_ch6, each followed by a blank line, and the footer
    CHECK(l6.at(head6.size()) == "// Table 6.5a - diurnal (m = 1) corrections for the frequency dependence of k21");
    CHECK(l6.at(head6.size() + 1) == "//   2 constituents; dk as printed (scale 1e-05), amplitudes as printed (scale 1e-12)");
    CHECK(contains(h6, "\n// Table 6.5b - zonal (m = 0) corrections for the frequency dependence of k20\n//   1 constituents; dk as printed (scale 1), amplitudes as printed (scale 1e-12)\n"));
    CHECK(contains(h6, "\n// Table 6.5c - semidiurnal (m = 2) corrections for k22; the real part only\n//   3 constituents; dk as printed (scale 1), amplitudes as printed (scale 1e-12)\n"));
    CHECK(l6.at(l6.size() - 1) == "}  // namespace odl::tides::tables");
    CHECK(l6.at(l6.size() - 2) == "");
    CHECK(l6.at(l6.size() - 3) == "};");
    CHECK(h6.back() == '\n');
    // exactly three tables are needed
    CHECK_THROWS_AS(tc::ch6_header("x", {t_a, t_b}), std::out_of_range);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the command line

TEST_CASE("run_on: the command line", "[tides_from_conventions][behaviour]") {
    tc::Settings settings;
    settings.pdftotext = [](const fs::path&) -> std::string { throw std::runtime_error("the seam must not be called by an argument error"); };
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_with(settings, {flag});
        CHECK(r.code == 0);
        CHECK(r.err.empty());
        CHECK(r.out.rfind(kUsage, 0) == 0);
        CHECK(r.out == std::string(kUsage) + kHelp);
        CHECK(contains(r.out, "\n  --ch5 CH5           chapter 5 (Table 5.1: libration)\n"));
        CHECK(contains(r.out, "\n  --ch8 CH8           chapter 8 (Tables 8.2 and 8.3: ocean tides)\n"));
        CHECK(contains(r.out, "\n  --out OUT           the header for chapters 5 and 8 (modules/eop/src/tide_tables.hpp)\n"));
        CHECK(contains(r.out, "\n  --ch6 CH6           IERS Conventions chapter 6, for Tables 6.5a/b/c\n"));
        CHECK(contains(r.out, "\n  --out-ch6 OUT_CH6   where to write the solid Earth tide tables (modules/tides/src/solid_tide_tables.hpp); goes with --ch6\n"));
        CHECK(contains(r.out, "\nexit codes: 0 written   1 a table is not found or its row count is not the expected one, or --ch6 and --out-ch6 do not go together   2 an argument error, or pdftotext or an input or output path failed\n"
                              "            70 an error the tool did not anticipate\n"));
        CHECK(contains(r.out, "Usage: tides_from_conventions --ch5 icc5.pdf --ch8 icc8.pdf --out <header> [--ch6 icc6.pdf --out-ch6 <header>]\n"));
    }
    CHECK(run_with(settings, {"--ch5", "x", "-h"}).code == 0);   // help wherever it stands before an error
    // required arguments: all three named in order, only the missing ones
    const std::string req = "tides_from_conventions: error: the following arguments are required: ";
    CHECK(run_with(settings, {}).err == std::string(kUsage) + req + "--ch5, --ch8, --out\n");
    CHECK(run_with(settings, {"--ch8", "b", "--out", "c"}).err == std::string(kUsage) + req + "--ch5\n");
    CHECK(run_with(settings, {"--ch5", "a", "--out", "c"}).err == std::string(kUsage) + req + "--ch8\n");
    CHECK(run_with(settings, {"--ch5", "a", "--ch8", "b"}).err == std::string(kUsage) + req + "--out\n");
    CHECK(run_with(settings, {"--ch5", "a"}).err == std::string(kUsage) + req + "--ch8, --out\n");
    CHECK(run_with(settings, {"--ch6", "x", "--out-ch6", "y"}).err == std::string(kUsage) + req + "--ch5, --ch8, --out\n");
    CHECK(run_with(settings, {}).code == 2);
    CHECK(run_with(settings, {}).out.empty());
    // unknown, abbreviated and stray arguments
    for (const std::string& bad : {std::string("--bogus"), std::string("x"), std::string("--ch"), std::string("--ch7"), std::string("--o"), std::string("-o"), std::string("--check")}) {
        const Result r = run_with(settings, {bad});
        INFO(bad);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "tides_from_conventions: error: unrecognized arguments: " + bad + "\n");
    }
    // an option without its value: at the end, or before another option
    for (const char* option : {"--ch5", "--ch8", "--out", "--ch6", "--out-ch6"}) {
        const std::string opt = option;
        const Result at_end = run_with(settings, {opt});
        CHECK(at_end.code == 2);
        CHECK(at_end.err == std::string(kUsage) + "tides_from_conventions: error: argument " + opt + ": expected one argument\n");
        const Result before = run_with(settings, {opt, "--ch5", "x"});
        CHECK(before.code == 2);
        CHECK(before.err == std::string(kUsage) + "tides_from_conventions: error: argument " + opt + ": expected one argument\n");
        // only "--" starts an option: a following word that begins with three hyphens is one too
        const Result triple = run_with(settings, {opt, "---x"});
        CHECK(triple.code == 2);
        CHECK(triple.err == std::string(kUsage) + "tides_from_conventions: error: argument " + opt + ": expected one argument\n");
    }
    CHECK(tc::default_settings().program == "pdftotext");
    CHECK_FALSE(static_cast<bool>(tc::default_settings().pdftotext));
    CHECK(std::string(tc::kTool) == "tides_from_conventions");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the whole tool on synthetic PDFs

TEST_CASE("run_on: chapters 5 and 8 through the seam -- the tables, the report on standard error, the header, the paths", "[tides_from_conventions][behaviour]") {
    const Pdfs pdfs;
    const Result r = run_with(pdfs.seam(), pdfs.args());
    CHECK(r.code == 0);
    CHECK(r.out.empty());
    const fs::path out = pdfs.root / "out" / "tide_tables.hpp";   // the directory did not exist: it is made
    CHECK(r.err ==
          "extracted: {'kPoleOceanDiurnal': 2, 'kPoleOceanSemidiurnal': 1, 'kUt1OceanDiurnal': 3, 'kUt1OceanSemidiurnal': 1, 'kPoleLibration': 2, 'kUt1Libration': 1}\n"
          "wrote " + out.string() + " (10 constituents)\n");
    REQUIRE(fs::exists(out));
    const std::string header = read_text(out);
    const std::string sha5 = sha256_hex(as_bytes(chapter5_text()));
    const std::string sha8 = sha256_hex(as_bytes(chapter8_text()));
    CHECK(contains(header, "//   chapter 5  sha256 " + sha5 + "\n//   chapter 8  sha256 " + sha8 + "\n"));
    // the tables of the text, row by row, as emit() writes them
    CHECK(contains(header,
                   "// Table 8.2a - diurnal ocean-tide variations in pole coordinates (xp sin, xp cos, yp sin, yp cos)  (2 constituents, microarcseconds)\n"
                   "inline constexpr TideTerm kPoleOceanDiurnal[] = {\n"
                   "    {{  1,  -2,   0,  -2,   0,   0},    1.1195149,   -0.0500,    0.0100,   -0.0600,    0.0200},\n"
                   "    {{  1,  -1,   0,   0,   0,   0},    1.0758059,   -0.2600,    0.5100,    0.1000,   -0.0400},\n"
                   "};\n"));
    CHECK(contains(header,
                   "inline constexpr TideTerm kPoleOceanSemidiurnal[] = {\n"
                   "    {{  2,  -1,   0,  -2,   0,   0},    0.5274312,    0.1000,   -0.2000,    0.3000,   -0.4000},\n"
                   "};\n"));
    CHECK(contains(header, "(3 constituents, microseconds / microseconds per day)\ninline constexpr TideTerm kUt1OceanDiurnal[] = {\n    {{  1,  -2,   0,  -2,   0,   0},    1.1195149,    1.5000,   -2.5000,    3.5000,   -4.5000},\n"));
    CHECK(contains(header, "    {{  2,   0,   0,   0,   0,   0},    0.5175252,  -10.0000,   20.0000,  -30.0000,   40.0000},\n"));
    // chapter 5: the leading degree column is not part of the row
    CHECK(contains(header, "inline constexpr TideTerm kPoleLibration[] = {\n    {{  1,  -2,   0,  -2,   0,   0},    1.1195149,    3.0000,   -4.0000,    5.0000,   -6.0000},\n"
                           "    {{  1,  -1,   0,   0,   0,   0},    1.0758059,    0.1250,    0.2500,    0.5000,    1.0000},\n};\n"));
    CHECK(contains(header, "inline constexpr TideTerm kUt1Libration[] = {\n    {{  2,   0,   0,   0,   0,   0},    0.5175252,    7.0000,    8.0000,    9.0000,   10.0000},\n};\n"));
    CHECK_FALSE(contains(header, "999"));   // the rows after the stop captions are not read
    // the whole header is what tide_header writes from the extracted rows
    const std::vector<std::string> c8 = tc::to_lines(chapter8_text());
    const std::vector<std::string> c5 = tc::to_lines(chapter5_text());
    const std::vector<tc::NamedRows> tables = {
        {"kPoleOceanDiurnal", tc::table_rows(c8, "Table 8.2a:", {"Table 8.2b:"})},
        {"kPoleOceanSemidiurnal", tc::table_rows(c8, "Table 8.2b:", {"Table 8.3a:"})},
        {"kUt1OceanDiurnal", tc::table_rows(c8, "Table 8.3a:", {"Table 8.3b:"})},
        {"kUt1OceanSemidiurnal", tc::table_rows(c8, "Table 8.3b:", {"Table 8.4", "8.3.2", "Table 8.5"})},
        {"kPoleLibration", tc::table_rows(c5, "Table 5.1a:", {"Table 5.1b:", "5.5.2"})},
        {"kUt1Libration", tc::table_rows(c5, "Table 5.1b:", {"5.5.4", "Table 5.2"})},
    };
    CHECK(header == tc::tide_header(sha5, sha8, tables));

    // the path is reported as pathlib's str(Path(p)) writes it: no "." component, no doubled or trailing slash
    {
        const Pdfs q;
        const std::string given = q.root.string() + "/./sub//deeper/h.hpp";
        std::vector<std::string> args = {"--ch5", q.ch5.string(), "--ch8", q.ch8.string(), "--out", given};
        const Result s = run_with(q.seam(), args);
        CHECK(s.code == 0);
        CHECK(contains(s.err, "wrote " + q.root.string() + "/sub/deeper/h.hpp (10 constituents)\n"));
        CHECK(fs::exists(q.root / "sub" / "deeper" / "h.hpp"));
    }
    // the = forms and any order of the options
    {
        const Pdfs q;
        const std::string out_path = (q.root / "e.hpp").string();
        const Result s = run_with(q.seam(), {"--out=" + out_path, "--ch8=" + q.ch8.string(), "--ch5=" + q.ch5.string()});
        CHECK(s.code == 0);
        CHECK(fs::exists(q.root / "e.hpp"));
        CHECK(contains(s.err, "wrote " + out_path + " (10 constituents)\n"));
    }
    // a relative --out is relative to the working directory, and its report says so
    {
        const Pdfs q;
        const fs::path saved = fs::current_path();
        fs::current_path(q.root);
        const Result s = run_with(q.seam(), {"--ch5", q.ch5.string(), "--ch8", q.ch8.string(), "--out", "rel/./t.hpp"});
        fs::current_path(saved);
        CHECK(s.code == 0);
        CHECK(contains(s.err, "wrote rel/t.hpp (10 constituents)\n"));
        CHECK(fs::exists(q.root / "rel" / "t.hpp"));
    }
    // the same text twice: the header is overwritten, not appended to
    {
        const Result again = run_with(pdfs.seam(), pdfs.args());
        CHECK(again.code == 0);
        CHECK(read_text(out) == header);
    }
}

TEST_CASE("run_on: chapter 6 -- the three tables, the counts asserted, the consistency lines, the order of the report", "[tides_from_conventions][behaviour]") {
    const Pdfs pdfs;
    const Result r = run_with(pdfs.seam(), pdfs.args(true));
    CHECK(r.code == 0);
    CHECK(r.out.empty());
    const fs::path out6 = pdfs.root / "out6" / "solid_tide_tables.hpp";
    const fs::path out = pdfs.root / "out" / "tide_tables.hpp";
    CHECK(r.err ==
          "extracted kSolidTideDiurnal: 48 constituents\n"
          "  kSolidTideDiurnal: 48 of 48 rows constrain it (0 lost to the printed precision), worst residual 6.67 of its rounding bound at 13.0 deg/hr\n"   // the seventh row, deg/hr 10 + 0.5 * 6
          "extracted kSolidTideZonal: 21 constituents\n"
          "  kSolidTideZonal: 21 of 21 rows constrain it (0 lost to the printed precision), worst residual 0.00 of its rounding bound\n"
          "extracted kSolidTideSemidiurnal: 2 constituents\n"
          "  kSolidTideSemidiurnal: the table has no imaginary column; the relation does not apply\n"
          "wrote " + out6.string() + " (71 constituents)\n"
          "extracted: {'kPoleOceanDiurnal': 2, 'kPoleOceanSemidiurnal': 1, 'kUt1OceanDiurnal': 3, 'kUt1OceanSemidiurnal': 1, 'kPoleLibration': 2, 'kUt1Libration': 1}\n"
          "wrote " + out.string() + " (10 constituents)\n");
    REQUIRE(fs::exists(out6));
    REQUIRE(fs::exists(out));
    const std::string header = read_text(out6);
    CHECK(contains(header, "// Conventions (2010) chapter 6:  sha256 " + sha256_hex(as_bytes(chapter6_text())) + "\n"));
    CHECK(contains(header, "inline constexpr SolidTideTerm kSolidTideDiurnal[] = {\n    {   10.00000, {  1,  -2,   0,   1,   0,  -2}, {  1,   0,   0,   2,  -1},      1.00000,      2.00000,      1.0,      2.0},\n"));
    CHECK(contains(header, "inline constexpr SolidTideTerm kSolidTideZonal[] = {\n    {    0.50000, {  0,   0,   1,   0,   0,   0}, {  0,   0,   0,   0,   1},      1.00000,      2.00000,      1.0,      2.0},\n"));
    CHECK(contains(header, "inline constexpr SolidTideTerm kSolidTideSemidiurnal[] = {\n    {   30.00000, {  2,   0,   0,   0,   0,   0}, {  0,   0,   0,   0,   1},     -0.50000,      0.00000,      3.0,      0.0},\n"));
    CHECK(contains(header, "//   48 constituents; dk as printed (scale 1e-05)"));
    CHECK(contains(header, "//   21 constituents; dk as printed (scale 1)"));
    CHECK(contains(header, "//   2 constituents; dk as printed (scale 1)"));
    CHECK_FALSE(contains(header, "37.00000"));   // the row after "6.2.2" is not read
    // the header is what ch6_header writes from the rows
    const std::vector<std::string> lines = tc::to_lines(chapter6_text());
    std::vector<std::vector<tc::Ch6Row>> tables;
    for (const tc::Ch6Spec& spec : tc::ch6_specs()) tables.push_back(tc::ch6_rows(lines, spec));
    CHECK(header == tc::ch6_header(sha256_hex(as_bytes(chapter6_text())), tables));
}

TEST_CASE("run_on: every refusal -- a missing table, a wrong count, options that contradict, an unreadable file", "[tides_from_conventions][behaviour]") {
    // a caption that is not in the text: exit 1, the message alone; nothing is written
    {
        std::string t8 = chapter8_text();
        t8.replace(t8.find("Table 8.3a:"), 11, "Table 8.9a:");
        const Pdfs pdfs(chapter5_text(), t8);
        const Result r = run_with(pdfs.seam(), pdfs.args());
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == "table 'Table 8.3a:' not found\n");
        CHECK_FALSE(fs::exists(pdfs.root / "out"));
    }
    {
        const Pdfs pdfs("no tables here\n");
        const Result r = run_with(pdfs.seam(), pdfs.args());
        CHECK(r.code == 1);
        CHECK(r.err == "table 'Table 5.1a:' not found\n");   // chapter 8's tables are found first and then chapter 5's
        CHECK_FALSE(fs::exists(pdfs.root / "out" / "tide_tables.hpp"));
    }
    {
        const Pdfs pdfs(chapter5_text(), "nothing\n");
        CHECK(run_with(pdfs.seam(), pdfs.args()).err == "table 'Table 8.2a:' not found\n");   // the first of chapter 8's six
    }
    // the options must go together: --ch6 without --out-ch6 and the other way round (this check comes before any file is read)
    {
        const Pdfs pdfs;
        std::vector<std::string> args = pdfs.args();
        args.push_back("--ch6");
        args.push_back(pdfs.ch6.string());
        const Result r = run_with(pdfs.seam(), args);
        CHECK(r.code == 1);
        CHECK(r.err == "--ch6 and --out-ch6 go together\n");
        CHECK_FALSE(fs::exists(pdfs.root / "out"));
        args = pdfs.args();
        args.push_back("--out-ch6");
        args.push_back((pdfs.root / "x6.hpp").string());
        const Result r2 = run_with(pdfs.seam(), args);
        CHECK(r2.code == 1);
        CHECK(r2.err == "--ch6 and --out-ch6 go together\n");
    }
    // the counts of chapter 6 are asserted: 48 + 21 + 2; the message is the Python's, four indented lines
    const std::string shape =
        ".\n"
        "  The table's shape in the PDF has changed, or pdftotext laid it out\n"
        "  differently. Re-read Tables 6.5a/b/c before touching this number:\n"
        "  the three have three different column orders and 6.5a's dk is in\n"
        "  units of 1e-5 where the other two are absolute.";
    {
        const Pdfs pdfs(chapter5_text(), chapter8_text(), chapter6_text(47, 21, 2));
        const Result r = run_with(pdfs.seam(), pdfs.args(true));
        CHECK(r.code == 1);
        CHECK(r.err ==
              "extracted kSolidTideDiurnal: 47 constituents\n"
              "  kSolidTideDiurnal: 47 of 47 rows constrain it (0 lost to the printed precision), worst residual 6.67 of its rounding bound at 13.0 deg/hr\n"
              "extracted kSolidTideZonal: 21 constituents\n"
              "  kSolidTideZonal: 21 of 21 rows constrain it (0 lost to the printed precision), worst residual 0.00 of its rounding bound\n"
              "extracted kSolidTideSemidiurnal: 2 constituents\n"
              "  kSolidTideSemidiurnal: the table has no imaginary column; the relation does not apply\n"
              "kSolidTideDiurnal: extracted 47 constituents, expected 48" + shape + "\n");
        CHECK_FALSE(fs::exists(pdfs.root / "out6"));   // neither header is written
        CHECK_FALSE(fs::exists(pdfs.root / "out"));
    }
    {
        const Pdfs pdfs(chapter5_text(), chapter8_text(), chapter6_text(48, 22, 2));
        const Result r = run_with(pdfs.seam(), pdfs.args(true));
        CHECK(r.code == 1);
        CHECK(contains(r.err, "kSolidTideZonal: extracted 22 constituents, expected 21" + shape + "\n"));
        CHECK_FALSE(contains(r.err, "kSolidTideDiurnal: extracted"));   // the first table is right: no message for it
    }
    {
        const Pdfs pdfs(chapter5_text(), chapter8_text(), chapter6_text(48, 21, 3));
        const Result r = run_with(pdfs.seam(), pdfs.args(true));
        CHECK(r.code == 1);
        CHECK(contains(r.err, "kSolidTideSemidiurnal: extracted 3 constituents, expected 2" + shape + "\n"));
    }
    {
        const Pdfs pdfs(chapter5_text(), chapter8_text(), "no tables\n");
        const Result r = run_with(pdfs.seam(), pdfs.args(true));
        CHECK(r.code == 1);
        CHECK(r.err == "table 'Table 6.5a:' not found\n");
    }
    // a PDF that is not there: the seam is told the path and reads nothing, then the hash fails -- a read error naming the file, exit 2
    {
        const Pdfs pdfs;
        fs::remove(pdfs.ch5);
        tc::Settings s;
        s.pdftotext = [&](const fs::path&) { return chapter5_text() + chapter8_text(); };
        const Result r = run_with(s, pdfs.args());
        CHECK(r.code == 2);
        CHECK(r.err.rfind("extracted: {", 0) == 0);   // the extraction was reported before the hash of the missing file was asked for
        CHECK(contains(r.err, "\ntides_from_conventions: "));
        CHECK(contains(r.err, "icc5.pdf"));
    }
    // the output cannot be written: a file stands where its directory should be
    {
        const Pdfs pdfs;
        write_text(pdfs.root / "out", "a file");
        const Result r = run_with(pdfs.seam(), pdfs.args());
        CHECK(r.code == 2);
        CHECK(r.err.rfind("extracted: {", 0) == 0);                       // the extraction was reported
        CHECK(contains(r.err, "\ntides_from_conventions: "));              // then the refusal
        CHECK_FALSE(contains(r.err, "wrote "));
    }
    // an error nobody anticipated: the seam throws something that is not a runtime error
    {
        const Pdfs pdfs;
        tc::Settings s;
        s.pdftotext = [](const fs::path&) -> std::string { throw std::logic_error("nonsense"); };
        const Result r = run_with(s, pdfs.args());
        CHECK(r.code == 70);
        CHECK(r.out.empty());
        CHECK(r.err == "tides_from_conventions: internal error: nonsense\n");
    }
    // a seam that throws a runtime error is an input that could not be read: exit 2 with the tool's name
    {
        const Pdfs pdfs;
        tc::Settings s;
        s.pdftotext = [](const fs::path&) -> std::string { throw std::runtime_error("no such luck"); };
        const Result r = run_with(s, pdfs.args());
        CHECK(r.code == 2);
        CHECK(r.err == "tides_from_conventions: no such luck\n");
    }
}


namespace {

// the calls the stand-in logged: each "--- call" ... "--- end" block is the argument list of one call
std::vector<std::vector<std::string>> logged_calls(const fs::path& log) {
    std::vector<std::vector<std::string>> out;
    if (!fs::exists(log)) return out;
    std::istringstream in(read_text(log));
    std::string line;
    while (std::getline(in, line)) {
        if (line == "--- call") out.emplace_back();
        else if (line != "--- end" && !out.empty()) out.back().push_back(line);
    }
    return out;
}

}  // namespace

TEST_CASE("run_on: the host program -- its arguments, its failures, text that is not UTF-8, a program that is not there", "[tides_from_conventions][behaviour]") {
    const Pdfs pdfs;
    EnvScope env;
    const fs::path log = pdfs.root / "calls.log";
    env.set("ODL_FAKE_PDFTOTEXT_LOG", log.string());
    for (const char* steering : {"ODL_FAKE_PDFTOTEXT_EXIT", "ODL_FAKE_PDFTOTEXT_STDERR", "ODL_FAKE_PDFTOTEXT_ONLY"}) env.unset(steering);
    REQUIRE(fs::exists(fake_pdftotext()));
    const std::string program = fake_pdftotext().string();

    // The stand-in writes the "PDF" unchanged: the headers are the ones the seam gives, and the calls are  program -layout PDF -  for chapter 6, then 5, then 8.
    {
        const Result r = run_with(pdfs.program(), pdfs.args(true));
        CHECK(r.code == 0);
        CHECK(r.out.empty());
        const Pdfs other;
        const Result via_seam = run_with(other.seam(), other.args(true));
        CHECK(via_seam.code == 0);
        CHECK(read_text(pdfs.root / "out" / "tide_tables.hpp") == read_text(other.root / "out" / "tide_tables.hpp"));
        CHECK(read_text(pdfs.root / "out6" / "solid_tide_tables.hpp") == read_text(other.root / "out6" / "solid_tide_tables.hpp"));
        const std::vector<std::vector<std::string>> calls = logged_calls(log);
        REQUIRE(calls.size() == 3);
        CHECK(calls.at(0) == std::vector<std::string>{"-layout", pdfs.ch6.string(), "-"});
        CHECK(calls.at(1) == std::vector<std::string>{"-layout", pdfs.ch5.string(), "-"});
        CHECK(calls.at(2) == std::vector<std::string>{"-layout", pdfs.ch8.string(), "-"});
    }
    // a program named without a slash is looked up on PATH; the default is  pdftotext
    {
        const Pdfs q;
        EnvScope path;
        path.set("PATH", fake_pdftotext().parent_path().string() + ":" + (std::getenv("PATH") != nullptr ? std::getenv("PATH") : ""));
        const Result r = run_with(tc::default_settings(), q.args());
        CHECK(r.code == 0);
        CHECK(fs::exists(q.root / "out" / "tide_tables.hpp"));
    }
    // a non-zero status: the tool's name, the command, the status and the tail of the program's standard error; only the first failing call is reported
    {
        const Pdfs q;
        env.set("ODL_FAKE_PDFTOTEXT_EXIT", "3");
        env.set("ODL_FAKE_PDFTOTEXT_STDERR", "boom");
        const Result r = run_with(q.program(), q.args());
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == "tides_from_conventions: " + program + " -layout " + q.ch5.string() + " - exited with status 3: boom\n\n");   // the stand-in ends its message with a newline of its own
        CHECK_FALSE(fs::exists(q.root / "out"));
        // no message from the program: no colon and no tail
        env.set("ODL_FAKE_PDFTOTEXT_STDERR", "");
        env.unset("ODL_FAKE_PDFTOTEXT_STDERR");
        const Result bare = run_with(q.program(), q.args());
        CHECK(bare.err == "tides_from_conventions: " + program + " -layout " + q.ch5.string() + " - exited with status 3\n");
        // a long message: its LAST 500 bytes
        env.set("ODL_FAKE_PDFTOTEXT_STDERR", "BEGIN" + std::string(600, 'x') + "END");
        const Result longr = run_with(q.program(), q.args());
        CHECK(longr.code == 2);
        CHECK(longr.err == "tides_from_conventions: " + program + " -layout " + q.ch5.string() + " - exited with status 3: " + std::string(496, 'x') + "END\n\n");
        CHECK_FALSE(contains(longr.err, "BEGIN"));
        // only chapter 8 fails: chapter 5 was read, the failure names the other file
        env.set("ODL_FAKE_PDFTOTEXT_STDERR", "no chapter 8");
        env.set("ODL_FAKE_PDFTOTEXT_ONLY", "icc8");
        const Result only8 = run_with(q.program(), q.args());
        CHECK(only8.code == 2);
        CHECK(only8.err == "tides_from_conventions: " + program + " -layout " + q.ch8.string() + " - exited with status 3: no chapter 8\n\n");
        // and with --ch6 the first call is chapter 6
        env.set("ODL_FAKE_PDFTOTEXT_ONLY", "icc6");
        const Result only6 = run_with(q.program(), q.args(true));
        CHECK(only6.err == "tides_from_conventions: " + program + " -layout " + q.ch6.string() + " - exited with status 3: no chapter 8\n\n");
        env.unset("ODL_FAKE_PDFTOTEXT_EXIT");
        env.unset("ODL_FAKE_PDFTOTEXT_STDERR");
        env.unset("ODL_FAKE_PDFTOTEXT_ONLY");
    }
    // text that is not UTF-8: refused with the command named
    {
        const Pdfs q("\xFF\xFE not text\n");
        const Result r = run_with(q.program(), q.args());
        CHECK(r.code == 2);
        CHECK(r.err == "tides_from_conventions: " + program + " -layout " + q.ch5.string() + " - wrote text that is not UTF-8\n");
    }
    // a PDF that is not there: what the program says about it
    {
        const Pdfs q;
        fs::remove(q.ch5);
        const Result r = run_with(q.program(), q.args());
        CHECK(r.code == 2);
        CHECK(r.err == "tides_from_conventions: " + program + " -layout " + q.ch5.string() + " - exited with status 1: Error: Couldn't open file '" + q.ch5.string() + "'\n\n");
    }
    // a program that is not there at all
    {
        const Pdfs q;
        tc::Settings s;
        s.program = (q.root / "no-such-pdftotext").string();
        const Result r = run_with(s, q.args());
        CHECK(r.code == 2);
        CHECK(r.err.rfind("tides_from_conventions: cannot run " + s.program + ": ", 0) == 0);
        CHECK_FALSE(fs::exists(q.root / "out"));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the real tree

namespace {

fs::path tree_path(const char* relative) { return fs::path(ODL_TREE_ROOT) / relative; }

// One generated table of a committed header: its comment lines (above the declaration), the declaration line, the row lines, and "};"; the numbers of every row.
struct Table {
    std::string name;
    std::vector<std::string> comments;
    std::vector<std::string> row_lines;
    std::vector<std::vector<double>> numbers;
    [[nodiscard]] std::string text() const {
        std::string t;
        for (const std::string& c : comments) t += c + "\n";
        return t;
    }
};

std::vector<double> numbers_of(std::string row) {
    for (char& c : row) {
        if (c == '{' || c == '}' || c == ',') c = ' ';
    }
    std::vector<double> out;
    std::istringstream in(row);
    std::string token;
    while (in >> token) out.push_back(std::strtod(token.c_str(), nullptr));
    return out;
}

// the tables of `type` ("TideTerm" or "SolidTideTerm") in the order of the file; `n_comments` comment lines above each declaration
std::vector<Table> tables_of(const std::string& header, const std::string& type, std::size_t n_comments) {
    const std::vector<std::string> lines = lines_of(header);
    std::vector<Table> out;
    const std::string prefix = "inline constexpr " + type + " ";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines.at(i).rfind(prefix, 0) != 0) continue;
        Table t;
        const std::string rest = lines.at(i).substr(prefix.size());
        t.name = rest.substr(0, rest.find('['));
        for (std::size_t k = n_comments; k > 0; --k) t.comments.push_back(lines.at(i - k));
        t.comments.push_back(lines.at(i));
        std::size_t j = i + 1;
        for (; j < lines.size() && lines.at(j) != "};"; ++j) {
            t.row_lines.push_back(lines.at(j));
            t.numbers.push_back(numbers_of(lines.at(j)));
        }
        out.push_back(std::move(t));
    }
    return out;
}

std::string table_text(const Table& t) {
    std::string s;
    for (const std::string& c : t.comments) s += c + "\n";
    for (const std::string& r : t.row_lines) s += r + "\n";
    return s + "};";
}

std::string sha_after(const std::string& header, const std::string& label) {
    const std::size_t at = header.find(label);
    REQUIRE(at != std::string::npos);
    const std::size_t start = at + label.size();
    return header.substr(start, 64);
}

}  // namespace

TEST_CASE("the committed tide_tables.hpp: its six tables re-emitted byte for byte, and the whole file rebuilt from its own rows", "[tides_from_conventions][real_tree]") {
    const std::string committed = read_text(tree_path("modules/eop/src/tide_tables.hpp"));
    const std::vector<Table> tables = tables_of(committed, "TideTerm", 1);
    REQUIRE(tables.size() == 6);
    const std::vector<std::string> names = {"kPoleOceanDiurnal", "kPoleOceanSemidiurnal", "kUt1OceanDiurnal", "kUt1OceanSemidiurnal", "kPoleLibration", "kUt1Libration"};
    const std::vector<std::size_t> counts = {41, 30, 41, 30, 25, 11};   // the control: the counts the Python printed on 2026-09-18 ('extracted: {...}')
    std::vector<tc::NamedRows> named;
    for (std::size_t k = 0; k < tables.size(); ++k) {
        const Table& t = tables.at(k);
        INFO(t.name);
        CHECK(t.name == names.at(k));
        REQUIRE(t.row_lines.size() == counts.at(k));
        // "// WHAT  (N constituents, UNIT)"
        const std::string& caption = t.comments.at(0);
        const std::size_t open = caption.rfind("  (");
        REQUIRE(open != std::string::npos);
        const std::string what = caption.substr(3, open - 3);
        const std::string inside = caption.substr(open + 3, caption.size() - open - 4);   // "N constituents, UNIT"
        REQUIRE(inside.find(" constituents, ") != std::string::npos);
        CHECK(std::stoul(inside.substr(0, inside.find(' '))) == counts.at(k));
        const std::string unit = inside.substr(inside.find(" constituents, ") + 15);
        tc::Rows rows;
        for (const std::vector<double>& n : t.numbers) {
            REQUIRE(n.size() == 11);
            tc::Row row{};
            for (std::size_t i = 0; i < 11; ++i) row.at(i) = n.at(i);
            rows.push_back(row);
        }
        // every row, every number printed by emit(): the committed text is its own fixed point
        CHECK(tc::emit(rows, t.name, unit, what) == table_text(t));
        named.push_back({t.name, rows});
    }
    // the whole file: the header, the hashes it names, the six tables, the footer
    const std::string sha5 = sha_after(committed, "//   chapter 5  sha256 ");
    const std::string sha8 = sha_after(committed, "//   chapter 8  sha256 ");
    CHECK(tc::tide_header(sha5, sha8, named) == committed);
    CHECK(committed.find("tools/tides_from_conventions.cpp") != std::string::npos);
}

TEST_CASE("the committed solid_tide_tables.hpp: its three tables re-emitted byte for byte, the consistency relation of the control, and the whole file rebuilt", "[tides_from_conventions][real_tree]") {
    const std::string committed = read_text(tree_path("modules/tides/src/solid_tide_tables.hpp"));
    const std::vector<Table> tables = tables_of(committed, "SolidTideTerm", 2);
    REQUIRE(tables.size() == 3);
    const std::vector<tc::Ch6Spec>& specs = tc::ch6_specs();
    const std::vector<std::size_t> counts = {48, 21, 2};   // 48 + 21 + 2 = 71, "counted from the PDF" by the Python
    // the control, written before the port, from the committed rows alone (an awk recount of the Python function's formulas)
    const std::vector<std::string> expected_consistency = {
        "kSolidTideDiurnal: 14 of 48 rows constrain it (34 lost to the printed precision), worst residual 0.83 of its rounding bound at 15.04328 deg/hr",
        "kSolidTideZonal: 18 of 21 rows constrain it (3 lost to the printed precision), worst residual 0.79 of its rounding bound at 0.00441 deg/hr",
        "kSolidTideSemidiurnal: the table has no imaginary column; the relation does not apply",
    };
    std::vector<std::vector<tc::Ch6Row>> parsed;
    for (std::size_t k = 0; k < tables.size(); ++k) {
        const Table& t = tables.at(k);
        INFO(t.name);
        CHECK(t.name == specs.at(k).name);
        REQUIRE(t.row_lines.size() == counts.at(k));
        std::vector<tc::Ch6Row> rows;
        for (const std::vector<double>& n : t.numbers) {
            REQUIRE(n.size() == 16);
            tc::Ch6Row r;
            r.deg_per_hour = n.at(0);
            for (std::size_t i = 0; i < 6; ++i) r.doodson.at(i) = static_cast<int>(n.at(1 + i));
            for (std::size_t i = 0; i < 5; ++i) r.delaunay.at(i) = static_cast<int>(n.at(7 + i));
            r.dk_real = n.at(12);
            r.dk_imag = n.at(13);
            r.amp_ip = n.at(14);
            r.amp_op = n.at(15);
            rows.push_back(r);
        }
        CHECK(tc::emit_ch6(rows, t.name, specs.at(k)) == table_text(t));
        CHECK(tc::ch6_consistency(rows, t.name) == expected_consistency.at(k));
        parsed.push_back(rows);
    }
    const std::string sha6 = sha_after(committed, "// Conventions (2010) chapter 6:  sha256 ");
    CHECK(tc::ch6_header(sha6, parsed) == committed);
    CHECK(committed.find("tools/tides_from_conventions.cpp --ch6") != std::string::npos);
    // the rows satisfy the filters the extraction applied: deg/hr in (0, 40), small integer multipliers; and 6.5a's rows have the four-column tail, 6.5c's the real part only
    for (const std::vector<tc::Ch6Row>& rows : parsed) {
        for (const tc::Ch6Row& r : rows) {
            CHECK(r.deg_per_hour > 0.0);
            CHECK(r.deg_per_hour < 40.0);
            for (const int x : r.doodson) CHECK(std::abs(x) <= 20);
            for (const int x : r.delaunay) CHECK(std::abs(x) <= 20);
        }
    }
    for (const tc::Ch6Row& r : parsed.at(2)) {
        CHECK(r.dk_imag == 0.0);
        CHECK(r.amp_op == 0.0);
    }
}

TEST_CASE("the hashes the committed headers name are the manifest's pinned hashes and those of the cached PDFs", "[tides_from_conventions][real_tree]") {
    const Json manifest = Json::parse(read_text(tree_path("manifest/manifest.json")));
    const auto pinned = [&](const std::string& id) {
        const Json* entries = manifest.find("entries");
        REQUIRE(entries != nullptr);
        for (const Json& e : entries->as_array()) {
            const Json* name = e.find("id");
            if (name != nullptr && name->as_string() == id) return std::pair<std::string, std::string>(e.find("sha256")->as_string(), e.find("filename")->as_string());
        }
        FAIL("no manifest entry " << id);
        return std::pair<std::string, std::string>();
    };
    const std::string tides = read_text(tree_path("modules/eop/src/tide_tables.hpp"));
    const std::string solid = read_text(tree_path("modules/tides/src/solid_tide_tables.hpp"));
    const std::string cache = manifest.find("cache")->as_string();
    const std::vector<std::tuple<std::string, std::string>> chapters = {{"tn36-chapter5", sha_after(tides, "//   chapter 5  sha256 ")},
                                                                         {"tn36-chapter8", sha_after(tides, "//   chapter 8  sha256 ")},
                                                                         {"tn36-chapter6", sha_after(solid, "// Conventions (2010) chapter 6:  sha256 ")}};
    for (const auto& [id, named] : chapters) {
        INFO(id);
        const auto [sha, filename] = pinned(id);
        CHECK(named == sha);
        const fs::path file = fs::path(ODL_TREE_ROOT) / cache / id / filename;
        REQUIRE(fs::exists(file));
        CHECK(sha256_file_hex(file) == sha);
    }
}

TEST_CASE("pdftotext on the real PDFs reproduces the committed headers (regeneration only: needs poppler, run when asked for by tag)", "[.regeneration][tides_from_conventions]") {
    // The committed headers were made on 2026-09-18 with a poppler version they do not record; the first comparison of group C10 reproduced them byte for byte with poppler 26.01.0.  A different poppler may lay
    // the tables out differently: if this differs, compare the EXTRACTED NUMBER ROWS first (the lines of the two headers that are rows) before concluding anything about the tool.
    const Json manifest = Json::parse(read_text(tree_path("manifest/manifest.json")));
    const std::string cache = manifest.find("cache")->as_string();
    const auto pdf = [&](const char* id, const char* name) { return fs::path(ODL_TREE_ROOT) / cache / id / name; };
    if (!fs::exists(pdf("tn36-chapter5", "icc5.pdf")) || !fs::exists(pdf("tn36-chapter8", "icc8.pdf")) || !fs::exists(pdf("tn36-chapter6", "icc6.pdf"))) SKIP("the IERS chapters are not in the cache");
    const TempDir td("odl-tides-regen");
    const fs::path out = td.path() / "tide_tables.hpp";
    const fs::path out6 = td.path() / "solid_tide_tables.hpp";
    const Result r = run_with(tc::default_settings(), {"--ch5", pdf("tn36-chapter5", "icc5.pdf").string(), "--ch8", pdf("tn36-chapter8", "icc8.pdf").string(), "--out", out.string(), "--ch6",
                                                         pdf("tn36-chapter6", "icc6.pdf").string(), "--out-ch6", out6.string()});
    if (r.code == 2 && contains(r.err, "cannot run pdftotext")) SKIP("pdftotext is not installed");
    REQUIRE(r.code == 0);
    const std::string a = read_text(out);
    const std::string b = read_text(tree_path("modules/eop/src/tide_tables.hpp"));
    const std::string c = read_text(out6);
    const std::string d = read_text(tree_path("modules/tides/src/solid_tide_tables.hpp"));
    CHECK(a == b);
    CHECK(c == d);
}
