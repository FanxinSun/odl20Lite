// tests/devtools/msis_coefficients_tests.cpp — NRLMSISE-00's fitted coefficients as generated source (plan L0 step 8, group C10): the Python's number pattern as a scanner, the DATA statements, the aliasing of
// COMMON/PARM7/ on synthetic data whose every value says where it came from, the format of a value and of a row, the generated text line by line, the command line, the write and the check on synthetic trees
// with every refusal, and — on the real tree — the committed header against the pinned Fortran, and the Fortran's own declarations against the names, the shapes and the order the tool assumes.
// (ctests `msis_coefficients.behaviour` and `msis_coefficients.real_tree`; the ctest `atmosphere.msis_coefficients_match_generator` runs the built tool with --check.)
//
// Every expectation is DERIVED BY HAND from the statements of msis_coefficients.py (read from git, never run), or comes from a SECOND METHOD written here apart from the tool (a line-oriented reader of the
// Fortran's DATA statements and of its declarations, a reader of the committed header's numbers), or from the committed header (produced by the Python); none is taken from running the port to see what it says.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/tool.hpp>

#include "msis_coefficients.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace mc = odl::tools::msis_coefficients;
using namespace odl::devkit;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const mc::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = mc::run_on(settings, args, Streams{out, err});
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

std::vector<std::string> tokens_of(std::string_view s) {
    std::vector<std::string> out;
    for (const std::string_view t : mc::find_numbers(s)) out.emplace_back(t);
    return out;
}

// the 64 arrays of COMMON/PARM7/ in the order BLOCK DATA GTD7BK declares them (read from the pinned Fortran, lines 1654-1664, and checked against it in the [real_tree] case below)
const std::vector<std::string> kParm7 = {"PT1", "PT2", "PT3", "PA1", "PA2", "PA3", "PB1", "PB2", "PB3", "PC1", "PC2", "PC3", "PD1", "PD2", "PD3", "PE1", "PE2", "PE3", "PF1", "PF2", "PF3", "PG1", "PG2", "PG3",
                                         "PH1", "PH2", "PH3", "PI1", "PI2", "PI3", "PJ1", "PJ2", "PJ3", "PK1", "PL1", "PL2", "PM1", "PM2", "PN1", "PN2", "PO1", "PO2", "PP1", "PP2", "PQ1", "PQ2", "PR1", "PR2",
                                         "PS1", "PS2", "PU1", "PU2", "PV1", "PV2", "PW1", "PW2", "PX1", "PX2", "PY1", "PY2", "PZ1", "PZ2", "PAA1", "PAA2"};

// A synthetic DATA statement in the shape of the pinned file: the name line, continuation lines with a star in the sixth column, five values to a line, the last followed by the slash.  `tokens` are the
// values as they are written (so that D and E exponents can be mixed).
std::string data_statement(const std::string& name, const std::vector<std::string>& tokens) {
    if (tokens.empty()) return "      DATA " + name + "/ /\n";   // a statement with no values
    std::string s = "      DATA " + name + "/\n";
    for (std::size_t i = 0; i < tokens.size(); i += 5) {
        s += "     * ";
        for (std::size_t j = i; j < std::min(i + 5, tokens.size()); ++j) {
            s += tokens.at(j);
            s += (j + 1 == tokens.size()) ? "/" : ",";
            if (j + 1 < std::min(i + 5, tokens.size())) s += " ";
        }
        s += "\n";
    }
    return s;
}

// The value of element i of array k of the synthetic Fortran: 100 k + i, an integer (so exact in decimal), written with an E exponent for even i and a D exponent for odd i, negated for every third element.
double synthetic_value(std::size_t k, std::size_t i) { return (i % 3 == 2 ? -1.0 : 1.0) * (100.0 * static_cast<double>(k) + static_cast<double>(i)); }
std::string synthetic_token(double v, std::size_t i) {
    const std::string digits = std::to_string(std::llround(std::fabs(v))) + ".0";
    return std::string(v < 0.0 ? "-" : "") + digits + (i % 2 == 0 ? "E+00" : "D+00");
}

std::vector<std::string> synthetic_tokens(std::size_t k, std::size_t count) {
    std::vector<std::string> t;
    for (std::size_t i = 0; i < count; ++i) t.push_back(synthetic_token(synthetic_value(k, i), i));
    return t;
}

// the whole synthetic Fortran: a comment, the 64 arrays in order (values 100 k + i), then PTM (10 values), PDM (80) and PAVGM (10) with values 1000 + 100 table + i
std::string synthetic_fortran(const std::vector<std::pair<std::string, std::vector<std::string>>>& replace = {}) {
    std::string t = "C     synthetic NRLMSISE-00 data\n      BLOCK DATA GTD7BK\n";
    const auto statement = [&](const std::string& name, std::vector<std::string> tokens) {
        for (const auto& [n, v] : replace) {
            if (n == name) tokens = v;
        }
        return data_statement(name, tokens);
    };
    for (std::size_t k = 0; k < kParm7.size(); ++k) t += statement(kParm7.at(k), synthetic_tokens(k, 50));
    t += statement("PTM", synthetic_tokens(64, 10));
    t += statement("PDM", synthetic_tokens(65, 80));
    t += statement("PAVGM", synthetic_tokens(66, 10));
    t += "      END\n";
    return t;
}

// flat element f of the synthetic COMMON/PARM7/: array f / 50, element f % 50
double flat_value(std::size_t f) { return synthetic_value(f / 50, f % 50); }

// A tree for run_on: manifest/manifest.json with the entry, cache/nrlmsise00-fortran/NRLMSISE-00.FOR, and the directory the header goes to.
struct Tree {
    TempDir td{"odl-mc"};
    fs::path root = td.path();
    std::string fortran;
    std::string sha;
    explicit Tree(const std::string& text, const std::string& manifest_override = "") : fortran(text), sha(sha256_hex(as_bytes(text))) {
        fs::create_directories(root / "manifest");
        fs::create_directories(root / "cache" / "nrlmsise00-fortran");
        fs::create_directories(root / "modules" / "atmosphere" / "src");
        write_text(root / "cache" / "nrlmsise00-fortran" / "NRLMSISE-00.FOR", text);
        write_text(root / "manifest" / "manifest.json", manifest_override.empty() ? manifest(sha) : manifest_override);
    }
    static std::string manifest(const std::string& pinned) {
        return "{\"cache\": \"cache\", \"entries\": [{\"id\": \"other\", \"filename\": \"x\", \"sha256\": \"00\"}, "
               "{\"id\": \"nrlmsise00-fortran\", \"filename\": \"NRLMSISE-00.FOR\", \"sha256\": \"" + pinned + "\"}]}";
    }
    [[nodiscard]] Result run(const std::vector<std::string>& extra = {}) const {
        std::vector<std::string> args = {"--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        return run_with(mc::default_settings(), args);
    }
    [[nodiscard]] fs::path header() const { return root / "modules" / "atmosphere" / "src" / "msis_coefficients.hpp"; }
};

const char kUsage[] = "usage: msis_coefficients [-h] [--root ROOT] [--out OUT] [--check]\n";

// What `-h` prints after the usage line: the port's own text (the Python's argparse help was wrapped to the terminal's width and cannot be reproduced byte for byte), RECORDED here whole so that every word of it is
// pinned, not only the lines that the fragments in the command-line case look at.
const char kHelp[] =
    "\n"
    "Extract NRLMSISE-00's fitted coefficients from the pinned FORTRAN (manifest entry nrlmsise00-fortran) as generated source, modules/atmosphere/src/msis_coefficients.hpp: 3,300 numbers, read\n"
    "through the aliasing of COMMON/PARM7/ (SPEC-atmosphere ATMO-R-001/002).  Not part of the build; --check is a ctest.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree to read the manifest and the cache of, and to write into (default: the tree this tool was built from)\n"
    "  --out OUT    the generated header, relative to the tree unless absolute (default: modules/atmosphere/src/msis_coefficients.hpp)\n"
    "  --check      do not write: exit 1 unless the committed header is exactly what the extraction emits\n"
    "\n"
    "exit codes: 0 written / the committed header reproduces   1 it differs, or the FORTRAN is not the shape the extraction expects   2 an argument error, or a file that is missing or cannot be read\n"
    "            70 an error the tool did not anticipate\n";

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the number pattern

TEST_CASE("find_numbers: the Python's  [-+]?\\d*\\.?\\d+(?:[EeDd][-+]?\\d+)?  with findall's leftmost, non-overlapping matches, by hand", "[msis_coefficients][behaviour]") {
    CHECK(tokens_of("") == std::vector<std::string>{});
    CHECK(tokens_of("abc / ,") == std::vector<std::string>{});
    CHECK(tokens_of("1.5 2") == std::vector<std::string>{"1.5", "2"});
    CHECK(tokens_of("9.86573E-01, 1.62228E-02,-1.04323E-01") == std::vector<std::string>{"9.86573E-01", "1.62228E-02", "-1.04323E-01"});
    CHECK(tokens_of("+3.0E+02,-4d-3") == std::vector<std::string>{"+3.0E+02", "-4d-3"});
    // a point needs a digit after it to belong to the number: "1." is the number 1, a lone "." is nothing
    CHECK(tokens_of("1.") == std::vector<std::string>{"1"});
    CHECK(tokens_of(".") == std::vector<std::string>{});
    CHECK(tokens_of(".5") == std::vector<std::string>{".5"});
    CHECK(tokens_of("-.5") == std::vector<std::string>{"-.5"});
    CHECK(tokens_of("5.e3") == std::vector<std::string>{"5", "3"});   // "5" then ".e" is nothing, then "3"
    CHECK(tokens_of("12.34.56") == std::vector<std::string>{"12.34", ".56"});
    // the exponent is part of the number only if it is whole: letter, optional sign, at least one digit
    CHECK(tokens_of("1E") == std::vector<std::string>{"1"});
    CHECK(tokens_of("1E+") == std::vector<std::string>{"1"});
    CHECK(tokens_of("1E-x") == std::vector<std::string>{"1"});
    CHECK(tokens_of("1E+5") == std::vector<std::string>{"1E+5"});
    CHECK(tokens_of("1e5") == std::vector<std::string>{"1e5"});
    CHECK(tokens_of("1D2") == std::vector<std::string>{"1D2"});
    CHECK(tokens_of("1d-2") == std::vector<std::string>{"1d-2"});
    CHECK(tokens_of("1e5e6") == std::vector<std::string>{"1e5", "6"});   // the second e has no mantissa of its own
    CHECK(tokens_of("2.5E3E4") == std::vector<std::string>{"2.5E3", "4"});
    CHECK(tokens_of(".5E1") == std::vector<std::string>{".5E1"});
    // signs belong to the number that follows them; a sign with no number after it is nothing
    CHECK(tokens_of("3-4") == std::vector<std::string>{"3", "-4"});
    CHECK(tokens_of("+-3") == std::vector<std::string>{"-3"});
    CHECK(tokens_of("--3") == std::vector<std::string>{"-3"});
    CHECK(tokens_of("+") == std::vector<std::string>{});
    CHECK(tokens_of("-+") == std::vector<std::string>{});
    CHECK(tokens_of("1,2,3") == std::vector<std::string>{"1", "2", "3"});
    CHECK(tokens_of("1 2\n3") == std::vector<std::string>{"1", "2", "3"});
    CHECK(tokens_of("0.00000E+00") == std::vector<std::string>{"0.00000E+00"});
    CHECK(tokens_of("007") == std::vector<std::string>{"007"});
    // only ASCII digits: the pinned file is ASCII (checked), and the Python's \d would also take other scripts' digits -- here an Arabic-Indic digit is nothing
    CHECK(tokens_of("\xD9\xA3") == std::vector<std::string>{});
    // the scanner never reads past the end of the view it is given: the tool hands it the body of a DATA statement, a view into the middle of the Fortran whose next character is the closing slash, and in a
    // test the next character can be anything.  The same text as a view into a longer buffer, whatever follows it there, gives the tokens that a copy of the text on its own gives.
    for (const std::string text : {"", "+", "-", "1", "1.", "1.5", ".", ".5", "5.", "+.", "-.5E", "1E", "1E+", "1E+5", "1.5D", "1.5D-", "12.34.", "1.5E3E", "7-"}) {
        for (const char next : {'7', '.', 'E', 'e', 'D', '+', '-', '/', ' '}) {
            const std::string buffer = text + next + "9";
            INFO("the text '" << text << "' followed by '" << next << "'");
            CHECK(tokens_of(std::string_view(buffer).substr(0, text.size())) == tokens_of(text));
        }
    }
    // the tokens are views into the text
    const std::string text = "x 12.5 y";
    const std::vector<std::string_view> views = mc::find_numbers(text);
    REQUIRE(views.size() == 1);
    CHECK(views.at(0).data() == text.data() + 2);
    CHECK(views.at(0).size() == 4);
}

TEST_CASE("read_data: the first DATA statement that begins a line, up to the next slash, D read as E", "[msis_coefficients][behaviour]") {
    CHECK(mc::read_data("      DATA PT1/ 1.0, 2.5D+00, -3.0E-01/\n", "PT1") == std::vector<double>{1.0, 2.5, -0.3});
    // continuation lines: the star in the sixth column is no part of a number
    CHECK(mc::read_data("      DATA PT1/\n     * 1.0, 2.0,\n     * 3.0/\n      END\n", "PT1") == std::vector<double>{1.0, 2.0, 3.0});
    CHECK(mc::read_data("      DATA PT1/\n     $ 1.0, 2.0,\n     $ 3.0/\n", "PT1") == std::vector<double>{1.0, 2.0, 3.0});
    // a lower-case d is an exponent too, and both are read as e
    CHECK(mc::read_data("      DATA X/ 2d3, 2D3, 2e3/", "X") == std::vector<double>{2000.0, 2000.0, 2000.0});
    // the name is followed by its slash: PT1 is not PT10
    CHECK(mc::read_data("      DATA PT10/ 9/\n      DATA PT1/ 1/\n", "PT1") == std::vector<double>{1.0});
    CHECK(mc::read_data("      DATA PT10/ 9/\n      DATA PT1/ 1/\n", "PT10") == std::vector<double>{9.0});
    // the pattern is anchored to the start of a line (re.M): exactly six blanks, then DATA
    CHECK(mc::read_data("C      DATA PT1/ 5/\n      DATA PT1/ 7/\n", "PT1") == std::vector<double>{7.0});   // the first match is inside a comment line
    CHECK(mc::read_data("      DATA PT1/ 7/\nxx      DATA PT1/ 5/\n", "PT1") == std::vector<double>{7.0});   // a match at the very start of the text counts
    CHECK_THROWS_AS(mc::read_data("       DATA PT1/ 7/\n", "PT1"), mc::Fatal);                              // seven blanks: not a match
    CHECK_THROWS_AS(mc::read_data("     DATA PT1/ 7/\n", "PT1"), mc::Fatal);                                // five blanks: not a match
    CHECK_THROWS_AS(mc::read_data("      data PT1/ 7/\n", "PT1"), mc::Fatal);                               // the word is DATA
    CHECK_THROWS_AS(mc::read_data("      DATA  PT1/ 7/\n", "PT1"), mc::Fatal);                              // one blank between DATA and the name
    // the first number may touch the opening slash and the last the closing one: the body begins right after the name's slash and ends right before the next
    CHECK(mc::read_data("      DATA X/1.5,-2.5/", "X") == std::vector<double>{1.5, -2.5});
    CHECK(mc::read_data("      DATA X/7/", "X") == std::vector<double>{7.0});
    CHECK(mc::read_data("      DATA X/-7/\n      DATA Y/8/", "X") == std::vector<double>{-7.0});
    // nothing before the first slash: an empty list; nothing at all: refused with the name
    CHECK(mc::read_data("      DATA PT1/ /\n", "PT1").empty());
    try {
        (void)mc::read_data("      DATA PT1/ 7/\n", "PT2");
        FAIL("no exception");
    } catch (const mc::Fatal& e) {
        CHECK(std::string(e.what()) == "no DATA statement for PT2");
    }
    // a statement with no closing slash anywhere after it: an input the tool cannot read (the Python's body.index raised ValueError)
    try {
        (void)mc::read_data("      DATA PT1/ 1.0, 2.0\n", "PT1");
        FAIL("no exception");
    } catch (const mc::BadInput& e) {
        CHECK(std::string(e.what()) == "the DATA statement for PT1 has no closing slash");
    }
    // the body ends at the FIRST slash after the name: a later statement is not read into this one
    CHECK(mc::read_data("      DATA PT1/ 1.0, 2.0/\n      DATA PT2/ 3.0/\n", "PT1") == std::vector<double>{1.0, 2.0});
    CHECK(mc::read_data("      DATA PT1/ 1.0, 2.0/\n      DATA PT2/ 3.0/\n", "PT2") == std::vector<double>{3.0});
    // Fatal and BadInput are runtime errors
    CHECK_THROWS_AS(mc::read_data("", "A"), std::runtime_error);
}

TEST_CASE("parm7_names: the sixty-four arrays in the order BLOCK DATA GTD7BK declares them", "[msis_coefficients][behaviour]") {
    CHECK(mc::parm7_names() == kParm7);
    REQUIRE(mc::parm7_names().size() == 64);
    CHECK(mc::parm7_names().front() == "PT1");
    CHECK(mc::parm7_names().back() == "PAA2");
    // T is skipped among the letters after K (PT is the temperature array, PT1..PT3 come first), and K has one array only
    CHECK(std::count(mc::parm7_names().begin(), mc::parm7_names().end(), "PT1") == 1);
    CHECK(std::count(mc::parm7_names().begin(), mc::parm7_names().end(), "PT2") == 1);
    CHECK(std::count(mc::parm7_names().begin(), mc::parm7_names().end(), "PK2") == 0);
    CHECK(std::count(mc::parm7_names().begin(), mc::parm7_names().end(), "PT4") == 0);
    CHECK(std::count(mc::parm7_names().begin(), mc::parm7_names().end(), "PS2") == 1);
    CHECK(std::count(mc::parm7_names().begin(), mc::parm7_names().end(), "PU1") == 1);
    CHECK(&mc::parm7_names() == &mc::parm7_names());   // one list
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the aliasing

TEST_CASE("extract: the aliasing of COMMON/PARM7/ -- every value of the views is the value of its place in the flat block", "[msis_coefficients][behaviour]") {
    const mc::Blocks blocks = mc::extract(synthetic_fortran());
    REQUIRE(blocks.size() == 10);
    struct Expect {
        const char* name;
        std::size_t n, m, offset;   // offset in the flat block (npos: a flat table)
    };
    // pt(150), pd(150,9), ps(150), pdl(25,2), ptl(100,4), pma(100,10), sam(100): 150 + 1350 + 150 + 50 + 400 + 1000 + 100 = 3200, in this order, the two-dimensional ones column-major
    const std::array<Expect, 7> views = {Expect{"pt", 150, 0, 0}, Expect{"pd", 150, 9, 150}, Expect{"ps", 150, 0, 1500}, Expect{"pdl", 25, 2, 1650}, Expect{"ptl", 100, 4, 1700}, Expect{"pma", 100, 10, 2100},
                                         Expect{"sam", 100, 0, 3100}};
    for (std::size_t b = 0; b < views.size(); ++b) {
        const Expect& e = views.at(b);
        INFO(e.name);
        CHECK(blocks.at(b).first == e.name);
        CHECK(blocks.at(b).second.n == e.n);
        CHECK(blocks.at(b).second.m == e.m);
        const std::size_t take = e.n * (e.m != 0 ? e.m : 1);
        REQUIRE(blocks.at(b).second.values.size() == take);
        for (std::size_t i = 0; i < take; ++i) CHECK(blocks.at(b).second.values.at(i) == flat_value(e.offset + i));
    }
    // the three tables that are not part of the common block, named in lower case
    CHECK(blocks.at(7).first == "ptm");
    CHECK(blocks.at(8).first == "pdm");
    CHECK(blocks.at(9).first == "pavgm");
    CHECK(blocks.at(7).second.n == 10);
    CHECK(blocks.at(7).second.m == 0);
    CHECK(blocks.at(8).second.n == 10);
    CHECK(blocks.at(8).second.m == 8);
    CHECK(blocks.at(9).second.n == 10);
    CHECK(blocks.at(9).second.m == 0);
    REQUIRE(blocks.at(7).second.values.size() == 10);
    REQUIRE(blocks.at(8).second.values.size() == 80);
    REQUIRE(blocks.at(9).second.values.size() == 10);
    for (std::size_t i = 0; i < 10; ++i) CHECK(blocks.at(7).second.values.at(i) == synthetic_value(64, i));
    for (std::size_t i = 0; i < 80; ++i) CHECK(blocks.at(8).second.values.at(i) == synthetic_value(65, i));
    for (std::size_t i = 0; i < 10; ++i) CHECK(blocks.at(9).second.values.at(i) == synthetic_value(66, i));
    // a few places read off by hand: pd's first column is the arrays PA1..PA3 (arrays 3, 4, 5: values 300.., 400.., 500..), ps's first element is PJ1's (array 30), pdl is PK1 (33), ptl starts at PL1, sam is
    // PAA1 and PAA2 (62, 63)
    CHECK(blocks.at(1).second.values.at(0) == 300.0);
    CHECK(blocks.at(1).second.values.at(50) == 400.0);
    CHECK(blocks.at(1).second.values.at(100) == 500.0);
    CHECK(blocks.at(1).second.values.at(150) == 600.0);   // the second column begins with PB1
    CHECK(blocks.at(2).second.values.at(0) == 3000.0);
    CHECK(blocks.at(3).second.values.at(0) == 3300.0);
    CHECK(blocks.at(4).second.values.at(0) == 3400.0);
    CHECK(blocks.at(6).second.values.at(0) == 6200.0);
    CHECK(blocks.at(6).second.values.at(99) == 6349.0);    // element 49 of PAA2 (100 * 63 + 49; 49 % 3 == 1, so not negated)
}

TEST_CASE("extract: the refusals -- a missing array, a wrong count, a wrong table", "[msis_coefficients][behaviour]") {
    const auto message_of = [](const std::string& text) {
        try {
            (void)mc::extract(text);
        } catch (const mc::Fatal& e) {
            return std::string(e.what());
        }
        return std::string("(nothing was thrown)");
    };
    // 49 or 51 values in an array of the common block
    CHECK(message_of(synthetic_fortran({{"PT2", synthetic_tokens(1, 49)}})) == "PT2: expected 50 values, parsed 49");
    CHECK(message_of(synthetic_fortran({{"PAA2", synthetic_tokens(63, 51)}})) == "PAA2: expected 50 values, parsed 51");
    CHECK(message_of(synthetic_fortran({{"PK1", std::vector<std::string>{}}})) == "PK1: expected 50 values, parsed 0");
    // the tables outside the common block: 10, 80, 10 values
    CHECK(message_of(synthetic_fortran({{"PTM", synthetic_tokens(64, 11)}})) == "PTM: expected 10, parsed 11");
    CHECK(message_of(synthetic_fortran({{"PDM", synthetic_tokens(65, 79)}})) == "PDM: expected 80, parsed 79");
    CHECK(message_of(synthetic_fortran({{"PAVGM", synthetic_tokens(66, 9)}})) == "PAVGM: expected 10, parsed 9");
    // an array whose statement is absent: the first missing one in declaration order is named
    std::string text = synthetic_fortran();
    const std::size_t at = text.find("      DATA PA2/");
    REQUIRE(at != std::string::npos);
    text.replace(at + 6, 4, "dATA");   // a statement that no longer begins DATA
    CHECK(message_of(text) == "no DATA statement for PA2");
    std::string no_table = synthetic_fortran();
    no_table.replace(no_table.find("      DATA PDM/"), 15, "      DATA QQQ/");
    CHECK(message_of(no_table) == "no DATA statement for PDM");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the text

TEST_CASE("format_value and format_values: the Python's  f\"{v: .6e}\"  and five to a row", "[msis_coefficients][behaviour]") {
    // a space where the plus sign would be
    CHECK(mc::format_value(1.0) == " 1.000000e+00");
    CHECK(mc::format_value(-1.0) == "-1.000000e+00");
    CHECK(mc::format_value(0.0) == " 0.000000e+00");
    CHECK(mc::format_value(-0.0) == "-0.000000e+00");
    CHECK(mc::format_value(123456789.0) == " 1.234568e+08");
    CHECK(mc::format_value(9.86573e-01) == " 9.865730e-01");
    CHECK(mc::format_value(-1.04323e-01) == "-1.043230e-01");
    CHECK(mc::format_value(1e100) == " 1.000000e+100");
    CHECK(mc::format_value(1.5e-7) == " 1.500000e-07");
    CHECK(mc::format_value(-2.5e-300) == "-2.500000e-300");
    CHECK(mc::format_value(std::numeric_limits<double>::infinity()) == " inf");
    CHECK(mc::format_value(-std::numeric_limits<double>::infinity()) == "-inf");
    CHECK(mc::format_value(std::numeric_limits<double>::quiet_NaN()) == " nan");
    // the rounding is that of the decimal expansion of the double: 0.1234565 is a hair below the tie at the sixth decimal in binary, and 2.5e0 has no rounding to do
    CHECK(mc::format_value(2.5) == " 2.500000e+00");
    CHECK(mc::format_value(1.0000004) == " 1.000000e+00");
    CHECK(mc::format_value(1.0000006) == " 1.000001e+00");

    // rows: four blanks, the values joined by ", " (each value carries its own sign column), a comma at the end of every row, the last row shorter, no final newline
    CHECK(mc::format_values({}) == "");
    CHECK(mc::format_values({1.5}) == "     1.500000e+00,");
    CHECK(mc::format_values({1.0, 2.0, 3.0, 4.0, 5.0}) == "     1.000000e+00,  2.000000e+00,  3.000000e+00,  4.000000e+00,  5.000000e+00,");
    CHECK(mc::format_values({1.0, -2.0, 3.0, 4.0, 5.0, 6.0}) ==
          "     1.000000e+00, -2.000000e+00,  3.000000e+00,  4.000000e+00,  5.000000e+00,\n"
          "     6.000000e+00,");
    CHECK(mc::format_values({1.0, 2.0}) == "     1.000000e+00,  2.000000e+00,");
    // ten values: two full rows, and no empty row after the second
    CHECK(mc::format_values({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}) ==
          "     1.000000e+00,  2.000000e+00,  3.000000e+00,  4.000000e+00,  5.000000e+00,\n"
          "     6.000000e+00,  7.000000e+00,  8.000000e+00,  9.000000e+00,  1.000000e+01,");
    // the indent of the two-dimensional views is eight blanks
    CHECK(mc::format_values({1.0, 2.0}, 8) == "         1.000000e+00,  2.000000e+00,");
    CHECK(mc::format_values({1.0}, 0) == " 1.000000e+00,");
    CHECK(mc::format_values({1.0}, 1) == "  1.000000e+00,");
}

TEST_CASE("emit: the generated header, line by line, from the statements of the Python's emit()", "[msis_coefficients][behaviour]") {
    mc::Blocks blocks;
    mc::Block one;
    one.values = {1.5, -2.0, 3.25, 4.0, 5.0, 6.0};
    one.n = 6;
    one.m = 0;
    blocks.emplace_back("pt", one);
    mc::Block two;
    two.values = {1, 2, 3, 4, 5, 6};
    two.n = 3;
    two.m = 2;
    blocks.emplace_back("pd", two);
    mc::Block three;
    three.values = {7.0};
    three.n = 1;
    three.m = 0;
    blocks.emplace_back("ptm", three);
    const std::string text = mc::emit("0123abcd", blocks, 64);
    const std::vector<std::string> lines = lines_of(text);
    const std::vector<std::string> expected = {
        "#pragma once",
        "// msis_coefficients.hpp — GENERATED.  Do not edit.",
        "//",
        "// Produced by tools/msis_coefficients.cpp from the hash-pinned NRL reference",
        "// implementation NRLMSISE-00.FOR, sha256 0123abcd",
        "//",
        "// NRLMSISE-00 has no published closed form: the model IS these coefficients",
        "// together with the code that combines them, which is why plan §4 rule 6 calls",
        "// the reference normative rather than an oracle.  They are EXTRACTED rather than",
        "// transcribed because there are 3 300 of them.",
        "//",
        "// BLOCK DATA GTD7BK fills COMMON/PARM7/ through 64 fifty-element DATA arrays",
        "// PT1..PAA2.  Every subroutine reads that same storage through DIFFERENT names —",
        "// pt(150), pd(150,9), ps(150), pdl(25,2), ptl(100,4), pma(100,10), sam(100),",
        "// which is 3200 again.  The views below are that aliasing made explicit; the",
        "// two-dimensional ones are COLUMN-MAJOR as FORTRAN stores them, so pd[j][i] here",
        "// is PD(i+1, j+1) there.",
        "",
        "#include <array>",
        "",
        "namespace odl::atmosphere::coeff {",
        "",
        "inline constexpr std::array<double, 6> kPT = {",
        "     1.500000e+00, -2.000000e+00,  3.250000e+00,  4.000000e+00,  5.000000e+00,",
        "     6.000000e+00,",
        "};",
        "",
        "inline constexpr std::array<std::array<double, 3>, 2> kPD = {{",
        "    {",
        "         1.000000e+00,  2.000000e+00,  3.000000e+00,",
        "    },",
        "    {",
        "         4.000000e+00,  5.000000e+00,  6.000000e+00,",
        "    },",
        "}};",
        "",
        "inline constexpr std::array<double, 1> kPTM = {",
        "     7.000000e+00,",
        "};",
        "",
        "}  // namespace odl::atmosphere::coeff",
    };
    REQUIRE(lines.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("line " << (i + 1));
        CHECK(lines.at(i) == expected.at(i));
    }
    CHECK(text.back() == '\n');
    CHECK(text.size() >= 2);
    CHECK(text.substr(text.size() - 2) != "\n\n");
    // the number of DATA arrays is the argument, not a constant
    CHECK(contains(mc::emit("x", blocks, 7), "through 7 fifty-element DATA arrays"));
    // a name is written in upper case, letter by letter: every lower-case letter (the first and the last of the alphabet included), nothing that stands just outside the two ranges ('@' and '[' beside the upper-case
    // letters, '`' and '{' beside the lower-case ones), no digit and no underscore, and an upper-case letter as it is
    {
        mc::Block b;
        b.values = {1.0};
        b.n = 1;
        b.m = 0;
        mc::Blocks named;
        named.emplace_back("abcdefghijklmnopqrstuvwxyz", b);
        named.emplace_back("ABCDEFGHIJKLMNOPQRSTUVWXYZ", b);
        named.emplace_back("a0_9z@[`{", b);
        const std::string t = mc::emit("x", named, 1);
        std::size_t both = 0;
        for (std::size_t at = t.find("> kABCDEFGHIJKLMNOPQRSTUVWXYZ = {\n"); at != std::string::npos; at = t.find("> kABCDEFGHIJKLMNOPQRSTUVWXYZ = {\n", at + 1)) ++both;
        CHECK(both == 2);
        CHECK(contains(t, "> kA0_9Z@[`{ = {\n"));
    }
    // no blocks: the header and the footer only
    const std::vector<std::string> empty_lines = lines_of(mc::emit("x", {}, 64));
    REQUIRE(empty_lines.size() == 23);
    CHECK(empty_lines.at(20) == "namespace odl::atmosphere::coeff {");
    CHECK(empty_lines.at(21) == "");
    CHECK(empty_lines.at(22) == "}  // namespace odl::atmosphere::coeff");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the command line

TEST_CASE("run_on: the command line", "[msis_coefficients][behaviour]") {
    const mc::Settings settings{fs::path("/nonexistent-root-for-the-argument-tests")};
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_with(settings, {flag});
        CHECK(r.code == 0);
        CHECK(r.err.empty());
        CHECK(r.out.rfind(kUsage, 0) == 0);
        CHECK(r.out == std::string(kUsage) + kHelp);
        CHECK(contains(r.out, "\noptions:\n  -h, --help   show this help and exit\n"));
        CHECK(contains(r.out, "\n  --root ROOT  the tree to read the manifest and the cache of, and to write into (default: the tree this tool was built from)\n"));
        CHECK(contains(r.out, "\n  --out OUT    the generated header, relative to the tree unless absolute (default: modules/atmosphere/src/msis_coefficients.hpp)\n"));
        CHECK(contains(r.out, "\n  --check      do not write: exit 1 unless the committed header is exactly what the extraction emits\n"));
        CHECK(contains(r.out, "\nexit codes: 0 written / the committed header reproduces   1 it differs, or the FORTRAN is not the shape the extraction expects   2 an argument error, or a file that is missing or cannot be read\n"
                              "            70 an error the tool did not anticipate\n"));
    }
    // help is honoured wherever it stands before the first error, also after other options
    CHECK(run_with(settings, {"--check", "-h"}).code == 0);
    CHECK(run_with(settings, {"--root", "x", "--help"}).code == 0);
    // an unknown option or a stray word
    for (const std::string& bad : {std::string("--bogus"), std::string("x"), std::string("-c"), std::string("--chec"), std::string("--check=1"), std::string("--root-dir=x")}) {
        const Result r = run_with(settings, {bad});
        INFO(bad);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "msis_coefficients: error: unrecognized arguments: " + bad + "\n");
    }
    // an option that takes a value, without one: at the end, or followed by another option
    for (const std::vector<std::string>& args : {std::vector<std::string>{"--root"}, std::vector<std::string>{"--out"}, std::vector<std::string>{"--root", "--check"}, std::vector<std::string>{"--out", "--root", "x"}, std::vector<std::string>{"--out", "---x"}}) {
        const Result r = run_with(settings, args);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "msis_coefficients: error: argument " + args.front() + ": expected one argument\n");
    }
    // the value after = may be empty, and a value may look like a short option (only "--" starts an option)
    CHECK(run_with(settings, {"--root", "-x"}).code == 2);   // the root "-x" does not exist: a missing manifest (exit 2 for another reason), not a usage error
    CHECK(contains(run_with(settings, {"--root", "-x"}).err, "manifest.json"));
    CHECK(mc::default_settings().root == fs::path(ODL_TREE_ROOT));
    CHECK(mc::default_root() == fs::path(ODL_TREE_ROOT));
    CHECK(std::string(mc::kTool) == "msis_coefficients");
    CHECK(std::string(mc::kSourceId) == "nrlmsise00-fortran");
    CHECK(std::string(mc::kDefaultOut) == "modules/atmosphere/src/msis_coefficients.hpp");
}

TEST_CASE("run_on: write, check, and the options of a tree", "[msis_coefficients][behaviour]") {
    const std::string text = synthetic_fortran();
    const Tree tree(text);
    const std::string expected = mc::emit(tree.sha, mc::extract(text), 64);

    // --check with nothing written yet: the header does not exist, so it differs
    const Result missing = tree.run({"--check"});
    CHECK(missing.code == 1);
    CHECK(missing.out.empty());
    CHECK(missing.err == "REGENERATED COEFFICIENTS DIFFER from the committed file\n");

    // write: exit 0, the report, the file
    const Result written = tree.run();
    CHECK(written.code == 0);
    CHECK(written.err.empty());
    CHECK(written.out == "wrote modules/atmosphere/src/msis_coefficients.hpp: 3300 coefficients (64 DATA arrays -> 3200 flat -> 7 views, plus 3 tables)\n");
    CHECK(read_text(tree.header()) == expected);

    // the file begins as the Python's emit() began, with this tree's hash, and holds one row per five values
    const std::vector<std::string> lines = lines_of(expected);
    CHECK(lines.at(0) == "#pragma once");
    CHECK(lines.at(4) == "// implementation NRLMSISE-00.FOR, sha256 " + tree.sha);
    CHECK(contains(expected, "inline constexpr std::array<double, 150> kPT = {\n     0.000000e+00,  1.000000e+00, -2.000000e+00,  3.000000e+00,  4.000000e+00,\n"));   // 100 k + i, every third negated
    CHECK(contains(expected, "inline constexpr std::array<std::array<double, 150>, 9> kPD = {{\n    {\n         3.000000e+02,  3.010000e+02, -3.020000e+02,"));   // array 3 (PA1), 300 + i
    CHECK(contains(expected, "inline constexpr std::array<std::array<double, 10>, 8> kPDM = {{\n"));
    CHECK(contains(expected, "inline constexpr std::array<double, 10> kPAVGM = {\n"));
    CHECK(std::count(lines.begin(), lines.end(), "    {") == 9 + 2 + 4 + 10 + 8);   // one per column of pd (9), pdl (2), ptl (4), pma (10) and pdm (8)

    // --check now: the same text, exit 0
    const Result ok = tree.run({"--check"});
    CHECK(ok.code == 0);
    CHECK(ok.err.empty());
    CHECK(ok.out == "ok  modules/atmosphere/src/msis_coefficients.hpp reproduces from nrlmsise00-fortran\n");
    // ... also when the committed file has CRLF line ends (Python's read_text() translates them)
    {
        std::string crlf;
        for (const char c : expected) {
            if (c == '\n') crlf += '\r';
            crlf += c;
        }
        write_text(tree.header(), crlf);
        const Result r = tree.run({"--check"});
        CHECK(r.code == 0);
    }
    // ... but not when one character differs, or the file is longer, or shorter
    for (const std::string& changed : {expected.substr(0, expected.size() - 2) + "\n", expected + "\n", expected.substr(0, expected.size() / 2), std::string(), "x" + expected}) {
        write_text(tree.header(), changed);
        const Result r = tree.run({"--check"});
        CHECK(r.code == 1);
        CHECK(r.err == "REGENERATED COEFFICIENTS DIFFER from the committed file\n");
        CHECK(r.out.empty());
    }
    {
        std::string off_by_one = expected;
        off_by_one.at(off_by_one.find("1.000000e+00")) = '2';
        write_text(tree.header(), off_by_one);
        CHECK(tree.run({"--check"}).code == 1);
    }
    // the check does not write
    write_text(tree.header(), "old");
    CHECK(tree.run({"--check"}).code == 1);
    CHECK(read_text(tree.header()) == "old");
    // writing replaces it
    CHECK(tree.run().code == 0);
    CHECK(read_text(tree.header()) == expected);

    // --out: relative to the tree, with a space or an equals sign, or absolute; the report names the path as given
    const Result relative = tree.run({"--out", "modules/atmosphere/src/other.hpp"});
    CHECK(relative.code == 0);
    CHECK(relative.out == "wrote modules/atmosphere/src/other.hpp: 3300 coefficients (64 DATA arrays -> 3200 flat -> 7 views, plus 3 tables)\n");
    CHECK(read_text(tree.root / "modules" / "atmosphere" / "src" / "other.hpp") == expected);
    const Result eq = tree.run({"--out=modules/atmosphere/src/third.hpp"});
    CHECK(eq.code == 0);
    CHECK(read_text(tree.root / "modules" / "atmosphere" / "src" / "third.hpp") == expected);
    const fs::path absolute = tree.root / "absolute.hpp";
    const Result abs = tree.run({"--out", absolute.string()});
    CHECK(abs.code == 0);
    CHECK(abs.out == "wrote " + absolute.string() + ": 3300 coefficients (64 DATA arrays -> 3200 flat -> 7 views, plus 3 tables)\n");
    CHECK(read_text(absolute) == expected);
    const Result abs_check = tree.run({"--check", "--out", absolute.string()});
    CHECK(abs_check.code == 0);
    CHECK(abs_check.out == "ok  " + absolute.string() + " reproduces from nrlmsise00-fortran\n");
    // --root with an equals sign
    const Result root_eq = run_with(mc::default_settings(), {"--root=" + tree.root.string(), "--check", "--out", absolute.string()});
    CHECK(root_eq.code == 0);
    // the root of the Settings is used when --root is not given, and --root overrides it
    mc::Settings s;
    s.root = tree.root;
    CHECK(run_with(s, {"--check"}).code == 0);
    CHECK(run_with(mc::Settings{fs::path("/nonexistent")}, {"--root", tree.root.string(), "--check"}).code == 0);
    // the last of two --out wins
    CHECK(tree.run({"--out", "modules/atmosphere/src/a.hpp", "--out", "modules/atmosphere/src/b.hpp"}).code == 0);
    CHECK(fs::exists(tree.root / "modules" / "atmosphere" / "src" / "b.hpp"));
    CHECK_FALSE(fs::exists(tree.root / "modules" / "atmosphere" / "src" / "a.hpp"));
}

TEST_CASE("run_on: every refusal of the manifest, the cache and the Fortran", "[msis_coefficients][behaviour]") {
    const std::string text = synthetic_fortran();

    // the cached bytes are not the pinned bytes: exit 1, the message alone (the Python's sys.exit(message))
    {
        const Tree tree(text, Tree::manifest(std::string(64, '0')));
        const Result r = tree.run();
        CHECK(r.code == 1);
        CHECK(r.out.empty());
        CHECK(r.err == "nrlmsise00-fortran: cached bytes are not the pinned bytes\n");
        CHECK_FALSE(fs::exists(tree.header()));
    }
    // a Fortran of the wrong shape: exit 1, the message alone
    {
        std::string no_pa2 = text;
        no_pa2.replace(no_pa2.find("      DATA PA2/"), 15, "      DATA XX2/");
        const Tree tree(no_pa2);
        const Result r = tree.run();
        CHECK(r.code == 1);
        CHECK(r.err == "no DATA statement for PA2\n");
        CHECK_FALSE(fs::exists(tree.header()));
    }
    {
        const Tree tree(synthetic_fortran({{"PDM", synthetic_tokens(65, 79)}}));
        const Result r = tree.run({"--check"});
        CHECK(r.code == 1);
        CHECK(r.err == "PDM: expected 80, parsed 79\n");
    }
    // a DATA statement with no closing slash: an input this does not read, exit 2 and the tool's name
    {
        const Tree tree("      DATA PT1/ 1.0, 2.0\n");
        const Result r = tree.run();
        CHECK(r.code == 2);
        CHECK(r.err == "msis_coefficients: the DATA statement for PT1 has no closing slash\n");
    }
    // the manifest: missing, not JSON, no entries or no cache, an entry without an id, no such entry, an entry without a filename or hash
    {
        const Tree tree(text);
        fs::remove(tree.root / "manifest" / "manifest.json");
        const Result r = tree.run();
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err.rfind("msis_coefficients: ", 0) == 0);
        CHECK(contains(r.err, (tree.root / "manifest" / "manifest.json").string()));
    }
    {
        const Tree tree(text, "{");
        const Result r = tree.run();
        CHECK(r.code == 2);
        CHECK(r.err == "msis_coefficients: " + (tree.root / "manifest" / "manifest.json").string() + ": Expecting property name enclosed in double quotes: line 1 column 2\n");
    }
    const auto manifest_error = [&](const std::string& manifest) {
        const Tree tree(text, manifest);
        const Result r = tree.run();
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        return r.err;
    };
    const std::string mp = "msis_coefficients: ";
    CHECK(contains(manifest_error("{\"cache\": \"cache\"}"), "no \"entries\" list or no \"cache\" string\n"));
    CHECK(contains(manifest_error("{\"entries\": []}"), "no \"entries\" list or no \"cache\" string\n"));
    CHECK(contains(manifest_error("{\"entries\": {}, \"cache\": \"cache\"}"), "no \"entries\" list or no \"cache\" string\n"));
    CHECK(contains(manifest_error("{\"entries\": [], \"cache\": 5}"), "no \"entries\" list or no \"cache\" string\n"));
    CHECK(contains(manifest_error("[1]"), "no \"entries\" list or no \"cache\" string\n"));
    CHECK(contains(manifest_error("{\"entries\": [{\"filename\": \"x\"}], \"cache\": \"cache\"}"), ": an entry without an id\n"));
    CHECK(contains(manifest_error("{\"entries\": [5], \"cache\": \"cache\"}"), ": an entry without an id\n"));
    CHECK(contains(manifest_error("{\"entries\": [{\"id\": 7}], \"cache\": \"cache\"}"), ": an entry without an id\n"));
    CHECK(contains(manifest_error("{\"entries\": [{\"id\": \"other\"}], \"cache\": \"cache\"}"), ": no entry nrlmsise00-fortran\n"));
    CHECK(contains(manifest_error("{\"entries\": [], \"cache\": \"cache\"}"), ": no entry nrlmsise00-fortran\n"));
    CHECK(manifest_error("{\"entries\": [{\"id\": \"nrlmsise00-fortran\", \"sha256\": \"00\"}], \"cache\": \"cache\"}") == mp + "nrlmsise00-fortran: no \"filename\" or no \"sha256\" in the manifest\n");
    CHECK(manifest_error("{\"entries\": [{\"id\": \"nrlmsise00-fortran\", \"filename\": \"x\"}], \"cache\": \"cache\"}") == mp + "nrlmsise00-fortran: no \"filename\" or no \"sha256\" in the manifest\n");
    CHECK(manifest_error("{\"entries\": [{\"id\": \"nrlmsise00-fortran\", \"filename\": 1, \"sha256\": \"00\"}], \"cache\": \"cache\"}") ==
          mp + "nrlmsise00-fortran: no \"filename\" or no \"sha256\" in the manifest\n");
    CHECK(manifest_error("{\"entries\": [{\"id\": \"nrlmsise00-fortran\", \"filename\": \"x\", \"sha256\": 5}], \"cache\": \"cache\"}") ==
          mp + "nrlmsise00-fortran: no \"filename\" or no \"sha256\" in the manifest\n");
    // the first entry with the id is the one used (the Python's next())
    {
        const Tree tree(text, "{\"entries\": [{\"id\": \"nrlmsise00-fortran\", \"filename\": \"NRLMSISE-00.FOR\", \"sha256\": \"" + sha256_hex(as_bytes(text)) +
                                  "\"}, {\"id\": \"nrlmsise00-fortran\", \"filename\": \"nothing\", \"sha256\": \"0\"}], \"cache\": \"cache\"}");
        CHECK(tree.run({"--check"}).code == 1);   // read fine, the header is not there
        CHECK(tree.run().code == 0);
    }
    // the cached file is missing: a read error naming the file, exit 2
    {
        const Tree tree(text);
        fs::remove(tree.root / "cache" / "nrlmsise00-fortran" / "NRLMSISE-00.FOR");
        const Result r = tree.run();
        CHECK(r.code == 2);
        CHECK(r.err.rfind("msis_coefficients: ", 0) == 0);
        CHECK(contains(r.err, "NRLMSISE-00.FOR"));
    }
    // the output cannot be written (its directory does not exist): a write error naming the file, exit 2
    {
        const Tree tree(text);
        const Result r = tree.run({"--out", "no/such/dir/h.hpp"});
        CHECK(r.code == 2);
        CHECK(r.err.rfind("msis_coefficients: ", 0) == 0);
        CHECK(contains(r.err, "h.hpp"));
    }
    // the sha of the cached bytes is checked BEFORE they are parsed: a wrong hash on a file of the wrong shape says the hash
    {
        const Tree tree("not fortran", Tree::manifest(std::string(64, 'a')));
        CHECK(tree.run().err == "nrlmsise00-fortran: cached bytes are not the pinned bytes\n");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the real tree

namespace {

// SECOND METHOD for the Fortran: the lines of the statement  DATA NAME/ ... /  read line by line (the statement begins at the name line; a continuation line has a non-blank character in the sixth column; the
// statement ends at the line that holds the closing slash), the tokens split at commas, D read as E.
std::vector<double> data_by_lines(const std::vector<std::string>& lines, const std::string& name) {
    const std::string head = "      DATA " + name + "/";
    std::vector<double> values;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines.at(i).rfind(head, 0) != 0) continue;
        std::string body = lines.at(i).substr(head.size());
        std::size_t j = i;
        for (;;) {
            const std::size_t slash = body.find('/');
            const bool last = slash != std::string::npos;
            if (last) body = body.substr(0, slash);
            std::size_t pos = 0;
            while (pos <= body.size()) {
                const std::size_t comma = body.find(',', pos);
                std::string tok = body.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                tok.erase(std::remove(tok.begin(), tok.end(), ' '), tok.end());
                for (char& c : tok) {
                    if (c == 'D') c = 'E';
                }
                if (!tok.empty()) values.push_back(std::strtod(tok.c_str(), nullptr));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            if (last) break;
            ++j;
            while (j < lines.size() && !lines.at(j).empty() && (lines.at(j).front() == 'C' || lines.at(j).front() == 'c')) ++j;   // a comment line (a C in the first column) inside the statement
            REQUIRE(j < lines.size());
            REQUIRE(lines.at(j).size() > 6);
            REQUIRE(lines.at(j).at(5) != ' ');   // a continuation line
            body = lines.at(j).substr(6);
        }
        return values;
    }
    FAIL("no DATA statement for " << name);
    return values;
}

fs::path real_fortran() { return fs::path(ODL_TREE_ROOT) / "data" / "cache" / "nrlmsise00-fortran" / "NRLMSISE-00.FOR"; }

}  // namespace

TEST_CASE("the committed header reproduces from the pinned Fortran (the ctest atmosphere.msis_coefficients_match_generator, in process)", "[msis_coefficients][real_tree]") {
    const Result r = run_with(mc::default_settings(), {"--check"});
    CHECK(r.code == 0);
    CHECK(r.err.empty());
    CHECK(r.out == "ok  modules/atmosphere/src/msis_coefficients.hpp reproduces from nrlmsise00-fortran\n");
}

TEST_CASE("the pinned Fortran declares what the tool extracts: the names and the order of the sixty-four arrays, the views of the subroutines, the tables", "[msis_coefficients][real_tree]") {
    const std::vector<std::string> lines = lines_of(universal_newlines(read_text(real_fortran())));
    // (1) BLOCK DATA GTD7BK:  COMMON/PARM7/PT1(50),PT2(50),...  with continuation lines ("     $ ...") -- the names, in order, each a 50-element array
    std::string block;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines.at(i).find("COMMON/PARM7/PT1(50)") == std::string::npos) continue;
        block = lines.at(i).substr(lines.at(i).find("COMMON/PARM7/") + 13);
        for (std::size_t j = i + 1; j < lines.size() && lines.at(j).size() > 6 && lines.at(j).at(5) == '$'; ++j) block += lines.at(j).substr(6);
        break;
    }
    REQUIRE_FALSE(block.empty());
    block.erase(std::remove(block.begin(), block.end(), ' '), block.end());
    std::vector<std::string> declared;
    std::size_t pos = 0;
    while (pos < block.size()) {
        const std::size_t open = block.find('(', pos);
        REQUIRE(open != std::string::npos);
        const std::size_t close = block.find(')', open);
        REQUIRE(close != std::string::npos);
        CHECK(block.substr(open + 1, close - open - 1) == "50");
        declared.push_back(block.substr(pos, open - pos));
        pos = close + 1;
        if (pos < block.size()) {
            REQUIRE(block.at(pos) == ',');
            ++pos;
        }
    }
    CHECK(declared == mc::parm7_names());
    CHECK(declared.size() == 64);

    // (2) the subroutines' own view:  COMMON/PARM7/PT(150),PD(150,9),PS(150),PDL(25,2),PTL(100,4),\n $ PMA(100,10),SAM(100)  -- the same in each of the two subroutines that declare it
    std::size_t views_found = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t at = lines.at(i).find("COMMON/PARM7/PT(150)");
        if (at == std::string::npos) continue;
        std::string decl = lines.at(i).substr(at + 13);
        for (std::size_t j = i + 1; j < lines.size() && lines.at(j).size() > 6 && lines.at(j).at(5) == '$'; ++j) decl += lines.at(j).substr(6);
        decl.erase(std::remove(decl.begin(), decl.end(), ' '), decl.end());
        CHECK(decl == "PT(150),PD(150,9),PS(150),PDL(25,2),PTL(100,4),PMA(100,10),SAM(100)");
        ++views_found;
    }
    CHECK(views_found == 2);
    // ... and the tool's views are these (names in lower case, n elements per column, m columns), 3,200 doubles in all
    const mc::Blocks blocks = mc::extract(read_text(real_fortran()));
    REQUIRE(blocks.size() == 10);
    const std::vector<std::tuple<std::string, std::size_t, std::size_t>> views = {{"pt", 150, 0}, {"pd", 150, 9}, {"ps", 150, 0}, {"pdl", 25, 2}, {"ptl", 100, 4}, {"pma", 100, 10}, {"sam", 100, 0}};
    std::size_t total = 0;
    for (std::size_t b = 0; b < views.size(); ++b) {
        CHECK(blocks.at(b).first == std::get<0>(views.at(b)));
        CHECK(blocks.at(b).second.n == std::get<1>(views.at(b)));
        CHECK(blocks.at(b).second.m == std::get<2>(views.at(b)));
        total += blocks.at(b).second.values.size();
    }
    CHECK(total == 3200);

    // (3) the tables outside the block:  COMMON/LOWER7/PTM(10),PDM(10,8)  and  COMMON/MAVG7/PAVGM(10)
    bool lower7 = false, mavg7 = false;
    for (const std::string& line : lines) {
        if (line.find("COMMON/LOWER7/PTM(10),PDM(10,8)") != std::string::npos) lower7 = true;
        if (line.find("COMMON/MAVG7/PAVGM(10)") != std::string::npos) mavg7 = true;
    }
    CHECK(lower7);
    CHECK(mavg7);
    CHECK(blocks.at(7).first == "ptm");
    CHECK(blocks.at(7).second.values.size() == 10);
    CHECK(blocks.at(8).first == "pdm");
    CHECK(blocks.at(8).second.n == 10);
    CHECK(blocks.at(8).second.m == 8);
    CHECK(blocks.at(8).second.values.size() == 80);
    CHECK(blocks.at(9).first == "pavgm");
    CHECK(blocks.at(9).second.values.size() == 10);
}

TEST_CASE("the 3,300 values of the pinned Fortran by a line-oriented reader, and the committed header's numbers by a reader of its own", "[msis_coefficients][real_tree]") {
    const std::vector<std::string> lines = lines_of(universal_newlines(read_text(real_fortran())));
    const mc::Blocks blocks = mc::extract(read_text(real_fortran()));

    // the flat COMMON/PARM7/ block: array by array, 64 x 50, by the second reader; then the views, which are slices of it
    std::vector<double> flat;
    for (const std::string& name : mc::parm7_names()) {
        const std::vector<double> v = data_by_lines(lines, name);
        REQUIRE(v.size() == 50);
        flat.insert(flat.end(), v.begin(), v.end());
    }
    REQUIRE(flat.size() == 3200);
    std::size_t offset = 0;
    for (std::size_t b = 0; b < 7; ++b) {
        const std::vector<double>& values = blocks.at(b).second.values;
        for (std::size_t i = 0; i < values.size(); ++i) {
            REQUIRE(values.at(i) == flat.at(offset + i));
        }
        offset += values.size();
    }
    CHECK(offset == 3200);
    const std::vector<double> ptm = data_by_lines(lines, "PTM");
    const std::vector<double> pdm = data_by_lines(lines, "PDM");
    const std::vector<double> pavgm = data_by_lines(lines, "PAVGM");
    CHECK(blocks.at(7).second.values == ptm);
    CHECK(blocks.at(8).second.values == pdm);
    CHECK(blocks.at(9).second.values == pavgm);

    // a few values read off the file by eye (lines 1673-1676 of the pinned Fortran): PT1 begins 9.86573E-01, 1.62228E-02, 1.55270E-02, -1.04323E-01, -3.75801E-03
    CHECK(blocks.at(0).second.values.at(0) == 9.86573E-01);
    CHECK(blocks.at(0).second.values.at(1) == 1.62228E-02);
    CHECK(blocks.at(0).second.values.at(2) == 1.55270E-02);
    CHECK(blocks.at(0).second.values.at(3) == -1.04323E-01);
    CHECK(blocks.at(0).second.values.at(4) == -3.75801E-03);

    // the committed header, read by a reader that knows only its shape: every data row (a line of values) after the namespace line, split at commas; the numbers in the order of the file are the blocks' values in the
    // order of the blocks, and each token is exactly the format of its value
    const std::vector<std::string> header = lines_of(read_text(fs::path(ODL_TREE_ROOT) / mc::kDefaultOut));
    std::vector<std::string> tokens;
    bool in_namespace = false;
    for (const std::string& line : header) {
        if (line == "namespace odl::atmosphere::coeff {") {
            in_namespace = true;
            continue;
        }
        if (!in_namespace) continue;
        std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) continue;
        const char c = line.at(first);
        if (!(c == '-' || (c >= '0' && c <= '9'))) continue;
        std::size_t pos = first;   // the first character of a value that is not a blank: a digit, or the minus sign
        while (pos != std::string::npos && pos < line.size()) {
            const std::size_t comma = line.find(',', pos);
            REQUIRE(comma != std::string::npos);   // every value of a row is followed by a comma
            tokens.push_back(line.substr(pos, comma - pos));
            pos = line.find_first_not_of(' ', comma + 1);
        }
    }
    std::vector<double> all;
    for (const auto& [name, block] : blocks) all.insert(all.end(), block.values.begin(), block.values.end());
    REQUIRE(all.size() == 3300);
    REQUIRE(tokens.size() == 3300);
    for (std::size_t i = 0; i < all.size(); ++i) {
        const std::string formatted = mc::format_value(all.at(i));   // a blank or a minus sign, then d.dddddde+xx
        REQUIRE(tokens.at(i) == formatted.substr(formatted.find_first_not_of(' ')));
    }
}
