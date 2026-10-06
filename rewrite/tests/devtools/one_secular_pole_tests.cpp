// tests/devtools/one_secular_pole_tests.cpp — SPEC-perturbations PERT-A-009, the half of it that is a property of the SOURCE: the secular pole is defined once.  The two scanners against the semantics of
// the patterns they replace, what the check prints and refuses on synthetic trees (the defect injected: a second copy within the window, a definition that lost a constant, a definition whose
// constants have drifted apart), and the real tree, recounted another way, with defects injected into a copy of the real sources.  (ctests `secular_pole.behaviour` and `secular_pole.real_tree`;
// the Python test had no test of its own, so these are the cases that show it CAN fail.)
//
// Every expectation is DERIVED BY HAND from the patterns, the window rule and the print statements of test_one_secular_pole.py, not taken from running the port.  Where the port deliberately differs
// (the path RELATIVE to the root is tested for `build` and `tests`; nothing to search and a source that is not UTF-8 are refused with exit 2) the case says so.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/tool.hpp>

#include "one_secular_pole.hpp"
#include "throwing_stream.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
namespace sp = odl::tools::one_secular_pole;

namespace {

constexpr int kOk = 0, kFailed = 1, kArgument = 2;
const std::string kHomePath = sp::kHome;

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = sp::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

struct Tree {
    TempDir td;
    fs::path root;
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

// the definition as it should look: the four constants, adjacent
const char kDefinition[] = "// the definition\nconstexpr double kXs0 = 55.0;\nconstexpr double kXsRate = 1.677;\nconstexpr double kYs0 = 320.5;\nconstexpr double kYsRate = 3.460;\n";

const char kTrailer[] = "\nThe secular pole is defined once, in gravity, and consumed by the pole tide (GRAV-R-029, PERT-R-006). A second copy is how the two drift apart.\n";

// `{name:<8}`
std::string left8(const std::string& name) { return name + std::string(8 - std::min<std::size_t>(8, name.size()), ' '); }

// the four component lines; `notes[i]` is the " (also, scattered, in ...)" text of the i-th constant, empty for none, and `missing[i]` replaces the line with the MISSING form
std::string components(const std::vector<std::string>& notes = {"", "", "", ""}, const std::vector<bool>& missing = {false, false, false, false}) {
    static const char* names[] = {"55.0", "1.677", "320.5", "3.460"};
    std::string out;
    for (std::size_t i = 0; i < 4; ++i) {
        if (missing[i]) out += std::string("MISSING  ") + names[i] + " is not in " + kHomePath + "\n";
        else out += "ok       " + left8(names[i]) + " in " + kHomePath + (notes[i].empty() ? "" : "   (also, scattered, in " + notes[i] + ")") + "\n";
    }
    return out;
}

std::string summary(std::size_t searched, std::size_t scattered, std::size_t copies) {
    return "\n" + std::to_string(searched) + " module sources searched. " + std::to_string(scattered) +
           " carry one or more of the four constants incidentally and scattered; " + std::to_string(copies) +
           " put 2 or more within 2 lines, which is what a restatement of TN36-7 (21) looks like.\n";
}

std::string names_of(const std::vector<sp::Adjacent>& hits) {
    std::string out;
    for (const sp::Adjacent& h : hits) {
        out += std::to_string(h.line) + ":";
        for (std::size_t i = 0; i < h.constants.size(); ++i) out += (i == 0 ? "" : ",") + h.constants[i];
        out += ";";
    }
    return out;
}

std::string names_of(const std::vector<std::string>& constants) {
    std::string out;
    for (std::size_t i = 0; i < constants.size(); ++i) out += (i == 0 ? "" : ",") + constants[i];
    return out;
}

// n blank lines
std::string blanks(std::size_t n) { return std::string(n, '\n'); }

}  // namespace

// ======================================================================================================================================== the scanners

TEST_CASE("constants_in: each of the four as its pattern \\b55\\.0\\b etc. finds it -- not glued to a word character or a digit on either side, the dot a dot -- in the order of the four", "[secular_pole][behaviour][scan]") {
    const auto in = [](const std::string& text) { return names_of(sp::constants_in(text)); };
    CHECK(in("") == "");
    CHECK(in("xs = 55.0 + 1.677 * (t - 2000)") == "55.0,1.677");
    CHECK(in("3.460 320.5 1.677 55.0") == "55.0,1.677,320.5,3.460");     // the order of the four, not of the text
    CHECK(in("55.0\n1.677\n320.5\n3.460\n") == "55.0,1.677,320.5,3.460");  // across lines
    CHECK(in("(55.0) -55.0 55.0, 55.0. .55.0 1.55.0 55.0.1") == "55.0");
    CHECK(in("a=1.677;") == "1.677");
    CHECK(in("155.0 55.0") == "55.0");                                   // the first occurrence is glued to a digit, a later one is not
    CHECK(in("x55.0 y55.0 55.0") == "55.0");
    // not a match
    for (const char* text : {"155.0", "55.00", "55.0f", "x55.0", "_55.0", "55.0_", "55.0e3", "55x0", "5.0", "255.0", "55.05", "11.677", "1.6770", "1.67", "21.677", "320.50", "1320.5", "3.4600", "3.46", "23.460",
                             "1.677e0", "320.5x", "320,5"}) {
        INFO(text);
        CHECK(in(text) == "");
    }
    CHECK(in("\xC3\xA9" "55.0") == "");                                  // an accented letter is a word character
    CHECK(in("55.0" "\xC3\xA9") == "");
    CHECK(in("\xC2\xB7" "55.0") == "55.0");                              // a middle dot is not
    CHECK(in("\xC2\xB2" "55.0") == "");                                  // a superscript two is
}

TEST_CASE("adjacent_hits: windows of two lines either side that carry two of the four; one is not listed again when it carries the same constants in the same order as the one before it", "[secular_pole][behaviour][scan]") {
    const auto hits = [](const std::string& text) { return names_of(sp::adjacent_hits(text)); };
    CHECK(hits("") == "");
    CHECK(hits("55.0\n") == "");
    CHECK(hits("x = 55.0 + 1.677;\n") == "1:55.0,1.677;");                                  // a window on the first line; the others carry the same
    CHECK(hits("a\nb\nx = 55.0 + 1.677;\nc\nd\n") == "1:55.0,1.677;");                      // the window of the FIRST line reaches the third
    // the worked example of the derivation: windows [i-2, i+2], the first of a run of equal windows is the one listed
    CHECK(hits("x = 55.0 + 1.677;\ny\ny\ny\ny\nz = 320.5;\nw = 3.460;") == "1:55.0,1.677;5:320.5,3.460;");
    // two lines FOUR apart share a window (centred on the line between them: the third line, 2 + 2), five apart share none
    CHECK(hits("55.0" + blanks(4) + "1.677" + blanks(3)) == "3:55.0,1.677;");
    CHECK(hits("55.0" + blanks(5) + "1.677" + blanks(3)) == "");
    // the same constants in a different order are a different window, listed again (the lists are compared, not the sets)
    CHECK(hits("a 3.460\nb 55.0\n" + blanks(3) + "c 3.460\n") == "1:3.460,55.0;4:55.0,3.460;");
    // a window with more than two is listed with all of them, in the order they were met scanning the window from its top; the window of line 4 (lines 2..4) has lost the 55.0, so it is a different list
    CHECK(hits("// 55.0\nx = 1.677 + 320.5;\ny = 3.460;\n") == "1:55.0,1.677,320.5,3.460;4:1.677,320.5,3.460;");
    // lines are split on \n only: a carriage return is not a line end here (the callers translate newlines first)
    CHECK(hits("55.0\r1.677\r\n") == "1:55.0,1.677;");
    // the same constant twice is one constant, not two: two of the FOUR are needed
    CHECK(hits("a 55.0\nb 55.0\nc 55.0\n") == "");
}

// ======================================================================================================================================== what is printed and refused

TEST_CASE("a tree that defines the pole once passes: the four component lines, with a note where a constant also occurs scattered, the summary, the verdict", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    t.write("modules/atmo/msis.hpp", "const double zn2 = 55.0;\n");                       // a spline node, scattered
    t.write("modules/atmo/other.cpp", "double u = 55.0;\n" + blanks(5) + "double v = 3.460;\n");   // two of them, six lines apart: no window holds both
    const Result r = t.run();
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(r.out == components({"modules/atmo/msis.hpp, modules/atmo/other.cpp", "", "", "modules/atmo/other.cpp"}) + summary(3, 2, 0) + "the secular pole has exactly one definition\n");
}

TEST_CASE("a second definition is caught: two constants within the window, in any production source: exit 1, the FAILED line, the explanation; the summary is printed first", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    t.write("modules/tides/pole_tide.cpp", "// pole tide\ndouble xs = 55.0 + 1.677 * (t - 2000.0);\ndouble ys = 320.5 + 3.460 * (t - 2000.0);\n");
    const Result r = t.run();
    CHECK(r.code == kFailed);
    const std::string note = "modules/tides/pole_tide.cpp";
    CHECK(r.out == components({note, note, note, note}) + summary(2, 1, 1));
    CHECK(r.err == "FAILED   modules/tides/pole_tide.cpp:1 has 55.0, 1.677, 320.5, 3.460 within 2 lines\n" + std::string(kTrailer));
    // each of the files that restates it is listed, files in sorted order, each of its windows
    Tree u;
    u.write(kHomePath, kDefinition);
    u.write("modules/b/two.cpp", "a = 55.0;\nb = 1.677;\n" + blanks(10) + "c = 320.5;\nd = 3.460;\n");
    u.write("modules/a/one.hpp", "z = 320.5 * 3.460;\n");
    const Result ru = u.run();
    CHECK(ru.code == kFailed);
    CHECK(ru.err == "FAILED   modules/a/one.hpp:1 has 320.5, 3.460 within 2 lines\nFAILED   modules/b/two.cpp:1 has 55.0, 1.677 within 2 lines\nFAILED   modules/b/two.cpp:12 has 320.5, 3.460 within 2 lines\n" +
                        std::string(kTrailer));
    CHECK(contains(ru.out, summary(3, 2, 2)));
}

TEST_CASE("the definition must carry all four constants: a missing one is a MISSING line, and the check fails without a summary", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, "constexpr double kXs0 = 55.0;\nconstexpr double kXsRate = 1.677;\nconstexpr double kYs0 = 320.5;\n");
    t.write("modules/a/a.cpp", "double u = 3.460;\n");   // the fourth, elsewhere: not the definition
    const Result r = t.run();
    CHECK(r.code == kFailed);
    CHECK(r.out == components({"", "", "", ""}, {false, false, false, true}));
    CHECK(r.err == "\n" + kHomePath + " does not carry all four constants\n");
    // no definition at all
    Tree none;
    none.write("modules/a/a.cpp", "double u = 3.460;\n");
    const Result rn = none.run();
    CHECK(rn.code == kFailed);
    CHECK(rn.out == components({"", "", "", ""}, {true, true, true, true}));
    CHECK(rn.err == "\n" + kHomePath + " does not carry all four constants\n");
}

TEST_CASE("the definition must keep two of the four within the window, or the check is no longer looking at what it was written for", "[secular_pole][behaviour]") {
    const auto spaced = [](std::size_t gap) {
        return "a = 55.0;\n" + blanks(gap) + "b = 1.677;\n" + blanks(gap) + "c = 320.5;\n" + blanks(gap) + "d = 3.460;\n";
    };
    {
        Tree t;
        t.write(kHomePath, spaced(3));   // four lines apart (three blank lines between): adjacent
        const Result r = t.run();
        CHECK(r.code == kOk);
        CHECK(r.out == components() + summary(1, 0, 0) + "the secular pole has exactly one definition\n");
    }
    {
        Tree t;
        t.write(kHomePath, spaced(4));   // five apart: no window holds two
        const Result r = t.run();
        CHECK(r.code == kFailed);
        CHECK(r.out == components());
        CHECK(r.err == "\n" + kHomePath + " does not put any pair of them within 2 lines, so this test is no longer looking at the definition it was written for\n");
    }
}

TEST_CASE("a restatement in a test, or below a build directory, is not a production source: it is not searched", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    t.write("modules/x/tests/copy.cpp", "double xs = 55.0 + 1.677;\n");
    t.write("modules/x/sub/tests/copy.hpp", "double xs = 55.0 + 1.677;\n");
    t.write("modules/tests/copy.cpp", "double xs = 55.0 + 1.677;\n");
    t.write("modules/x/build/copy.cpp", "double xs = 55.0 + 1.677;\n");
    t.write("modules/build/copy.hpp", "double xs = 55.0 + 1.677;\n");
    t.write("tests/copy.cpp", "double xs = 55.0 + 1.677;\n");          // not under modules at all
    t.write("tools/copy.cpp", "double xs = 55.0 + 1.677;\n");
    t.write("modules/x/mytests/ok.cpp", "int n = 1;\n");                 // `tests` is a whole directory name
    t.write("modules/x/ok.h", "double xs = 55.0 + 1.677;\n");            // not .hpp or .cpp
    t.write("modules/x/ok.cc", "double xs = 55.0 + 1.677;\n");
    t.write("modules/x/ok.hpp.bak", "double xs = 55.0 + 1.677;\n");
    const Result r = t.run();
    CHECK(r.code == kOk);
    CHECK(r.out == components() + summary(2, 0, 0) + "the secular pole has exactly one definition\n");   // the definition and mytests/ok.cpp
    // a file called .hpp is a source (the glob's `*` may be empty)
    t.write("modules/x/.hpp", "double xs = 55.0 + 1.677;\n");
    const Result rd = t.run();
    CHECK(rd.code == kFailed);
    CHECK(contains(rd.err, "FAILED   modules/x/.hpp:1 has 55.0, 1.677 within 2 lines\n"));
}

TEST_CASE("an entry that is no regular file is skipped by rule, a link to a file is read, a link to a directory is not entered, a source that is not UTF-8 is refused", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    fs::create_directories(t.root / "modules/x/odd.cpp");
    t.write("elsewhere/inside.cpp", "double xs = 55.0 + 1.677;\n");
    t.write("keep/real.cpp", "double xs = 55.0 + 1.677;\n");
    std::error_code ec;
    fs::create_directory_symlink(t.root / "elsewhere", t.root / "modules/x/linked", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "nowhere", t.root / "modules/x/dangling.hpp", ec);
    REQUIRE_FALSE(ec);
    REQUIRE(::mkfifo((t.root / "modules/x/fifo.cpp").c_str(), 0600) == 0);
    const Result clean = t.run();
    CHECK(clean.code == kOk);
    CHECK(clean.out == components() + summary(1, 0, 0) + "the secular pole has exactly one definition\n");
    fs::create_symlink(t.root / "keep/real.cpp", t.root / "modules/x/alias.hpp", ec);   // a link to a file IS read
    REQUIRE_FALSE(ec);
    const Result linked = t.run();
    CHECK(linked.code == kFailed);
    CHECK(contains(linked.err, "FAILED   modules/x/alias.hpp:1 has 55.0, 1.677 within 2 lines\n"));
    // not UTF-8: refused (the Python read strictly and died with a traceback)
    Tree bad;
    bad.write(kHomePath, kDefinition);
    bad.write("modules/x/bad.cpp", std::string("caf\xE9 = 1;\n"));
    const Result rb = bad.run();
    CHECK(rb.code == kArgument);
    CHECK(rb.out.empty());
    CHECK(rb.err == "one_secular_pole: " + (bad.root / "modules/x/bad.cpp").string() + " is not well-formed UTF-8\n");
}

TEST_CASE("lines are the text split on \\n after universal newlines: a CR LF and a lone CR end a line, a form feed does not", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    t.write("modules/a/crlf.cpp", "a\r\nb\r\nx = 55.0;\r\ny = 1.677;\r\n");   // windows: i=0 sees lines 1..3, i=1 sees 1..4: the pair is first seen from the second line
    t.write("modules/b/cr.cpp", "a\rb\rx = 55.0;\ry = 1.677;\r");
    t.write("modules/c/ff.cpp", "a\fb\fx = 55.0;\fy = 1.677;\f");               // no newline at all: one line, the pair on it
    const Result r = t.run();
    CHECK(r.code == kFailed);
    CHECK(r.err == "FAILED   modules/a/crlf.cpp:2 has 55.0, 1.677 within 2 lines\nFAILED   modules/b/cr.cpp:2 has 55.0, 1.677 within 2 lines\nFAILED   modules/c/ff.cpp:1 has 55.0, 1.677 within 2 lines\n" +
                       std::string(kTrailer));
}

TEST_CASE("a tree under directories called build and tests is searched all the same (the Python tested the absolute path, and passed over every file of such a tree)", "[secular_pole][behaviour]") {
    Tree t("tests/build/deeper");
    t.write(kHomePath, kDefinition);
    t.write("modules/x/copy.cpp", "double xs = 55.0 + 1.677;\n");
    const Result r = t.run();
    CHECK(r.code == kFailed);
    CHECK(contains(r.err, "FAILED   modules/x/copy.cpp:1 has 55.0, 1.677 within 2 lines\n"));
}

TEST_CASE("no module source is REFUSED, exit 2, naming the modules directory: success over nothing read is the failure a checker exists against", "[secular_pole][behaviour]") {
    {
        Tree t;
        const Result r = t.run();
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err == "one_secular_pole: nothing to search: no .hpp or .cpp file under " + (t.root / "modules").string() + " (outside build and tests directories)\n");
    }
    {
        Tree t;
        t.write("modules/x/tests/a.cpp", "double xs = 55.0 + 1.677;\n");
        t.write("modules/x/notes.txt", "55.0 1.677\n");
        CHECK(t.run().code == kArgument);
    }
}

TEST_CASE("a source that cannot be read is REFUSED, exit 2, naming it -- never skipped", "[secular_pole][behaviour]") {
    if (::geteuid() == 0) SKIP("root opens every file");
    Tree t;
    t.write(kHomePath, kDefinition);
    t.write("modules/m/locked.cpp", "int b;\n");
    REQUIRE(::chmod((t.root / "modules/m/locked.cpp").c_str(), 0) == 0);
    const Result r = t.run();
    CHECK(r.code == kArgument);
    CHECK(r.out.empty());
    CHECK(contains(r.err, "one_secular_pole: cannot read " + (t.root / "modules/m/locked.cpp").string() + ": "));
    REQUIRE(::chmod((t.root / "modules/m/locked.cpp").c_str(), 0600) == 0);
}

TEST_CASE("an error the tool did not anticipate is reported as such, exit 70, and not as a pass or a failure", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    odl::devtools_testing::ThrowingStream out;   // every write to it throws: the component lines cannot be printed
    std::ostringstream err;
    const int code = sp::run({"--root", t.root.string()}, Streams{out, err});
    CHECK(code == 70);
    CHECK(err.str() == "one_secular_pole: internal error: boom\n");
}

TEST_CASE("the command line: --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[secular_pole][behaviour]") {
    Tree t;
    t.write(kHomePath, kDefinition);
    const std::string expected = components() + summary(1, 0, 0) + "the secular pole has exactly one definition\n";
    CHECK(run_tool({"--root=" + t.root.string()}).out == expected);
    CHECK(run_tool({"--root", t.root.string()}).out == expected);
    const Result help = run_tool({"-h"});
    CHECK(help.code == kOk);
    CHECK(help.out.rfind("usage: one_secular_pole [-h] [--root ROOT]\n", 0) == 0);
    CHECK(contains(help.out, "options:\n  -h, --help   show this help and exit\n  --root ROOT  the tree (default: the tree this program was built from)\n"));
    CHECK(contains(help.out, "exit codes: 0 one definition   1 not   2 an argument error, a source that is not UTF-8, or nothing to search\n"));
    CHECK(run_tool({"--help"}).out == help.out);
    CHECK(help.err.empty());
    CHECK(run_tool({"-h=x"}).code == kArgument);                       // a short option takes no value
    CHECK(run_tool({"--root=" + t.root.string(), "--help"}).out == help.out);
    // an option's value is whatever follows it unless that begins with `--` (so a directory called -x is a value)
    const Result dash = run_tool({"--root", "-x"});
    CHECK(dash.code == kArgument);
    CHECK(dash.err == "one_secular_pole: nothing to search: no .hpp or .cpp file under -x/modules (outside build and tests directories)\n");
    const Result missing = run_tool({"--root"});
    CHECK(missing.code == kArgument);
    CHECK(missing.err == "usage: one_secular_pole [-h] [--root ROOT]\none_secular_pole: error: argument --root: expected one argument\n");
    const Result unknown = run_tool({"--bogus"});
    CHECK(unknown.code == kArgument);
    CHECK(unknown.err == "usage: one_secular_pole [-h] [--root ROOT]\none_secular_pole: error: unrecognized arguments: --bogus\n");
    CHECK(run_tool({"--roo", t.root.string()}).code == kArgument);   // no abbreviations
    CHECK(run_tool({"stray", "--root", t.root.string()}).code == kArgument);
}

// ======================================================================================================================================== the real tree

namespace {

struct Source {
    std::string rel;
    std::string text;
};

// the production sources of a tree: the .hpp and .cpp files below modules/, none below a build or tests directory, by the standard directory iterator; sorted
std::vector<Source> production_sources(const fs::path& root) {
    std::vector<Source> out;
    for (fs::recursive_directory_iterator it(root / "modules"), end; it != end; ++it) {
        if (it->is_directory() && (it->path().filename() == "build" || it->path().filename() == "tests")) {
            it.disable_recursion_pending();
            continue;
        }
        const std::string name = it->path().filename().string();
        if (!it->is_regular_file() || !(name.ends_with(".hpp") || name.ends_with(".cpp"))) continue;
        out.push_back(Source{fs::relative(it->path(), root).generic_string(), read_text(it->path())});
    }
    std::sort(out.begin(), out.end(), [](const Source& a, const Source& b) { return a.rel < b.rel; });
    return out;
}

const char* const kNames[] = {"55.0", "1.677", "320.5", "3.460"};

// which of the four a piece of text holds: std::regex (ECMAScript, ASCII word characters) in place of the hand-written scanner
std::vector<std::string> found_in(const std::string& text) {
    static const std::regex patterns[] = {std::regex(R"re(\b55\.0\b)re"), std::regex(R"re(\b1\.677\b)re"), std::regex(R"re(\b320\.5\b)re"), std::regex(R"re(\b3\.460\b)re")};
    std::vector<std::string> out;
    for (std::size_t i = 0; i < 4; ++i) {
        if (std::regex_search(text, patterns[i])) out.emplace_back(kNames[i]);
    }
    return out;
}

// the windows of a file, by the Python's loop written out again
bool has_adjacent(const std::string& text) {
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
    std::vector<std::vector<std::string>> per_line;
    for (const std::string& line : lines) per_line.push_back(found_in(line));
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::vector<std::string> near;
        for (std::size_t j = (i >= 2 ? i - 2 : 0); j < std::min(lines.size(), i + 3); ++j) {
            for (const std::string& n : per_line[j]) {
                if (std::find(near.begin(), near.end(), n) == near.end()) near.push_back(n);
            }
        }
        if (near.size() >= 2) return true;
    }
    return false;
}

// what the check must print for a tree whose definition is sound: from the sources alone
std::string expected_output(const std::vector<Source>& sources) {
    std::map<std::string, std::vector<std::string>> carries;
    std::size_t copies = 0;
    for (const Source& s : sources) {
        const std::vector<std::string> found = found_in(s.text);
        if (found.empty()) continue;
        carries[s.rel] = found;
        if (s.rel != kHomePath && has_adjacent(s.text)) ++copies;
    }
    std::string out;
    for (const char* name : kNames) {
        std::string others;
        bool in_home = false;
        for (const auto& [file, got] : carries) {
            if (std::find(got.begin(), got.end(), name) == got.end()) continue;
            if (file == kHomePath) in_home = true;
            else others += (others.empty() ? "" : ", ") + file;
        }
        out += in_home ? "ok       " + left8(name) + " in " + kHomePath + (others.empty() ? "" : "   (also, scattered, in " + others + ")") + "\n" : std::string("MISSING  ") + name + " is not in " + kHomePath + "\n";
    }
    return out + summary(sources.size(), carries.size() - 1, copies) + "the secular pole has exactly one definition\n";
}

std::size_t newlines_in(const std::string& s) { return static_cast<std::size_t>(std::count(s.begin(), s.end(), '\n')); }

}  // namespace

TEST_CASE("the real tree: the secular pole has exactly one definition, and what is printed is what an independent search of the same sources gives", "[secular_pole][real_tree]") {
    const std::vector<Source> sources = production_sources(sp::default_root());
    REQUIRE_FALSE(sources.empty());
    const Result r = run_tool({});
    INFO(r.out << r.err);
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(r.out == expected_output(sources));
    // the definition itself, found a second way: all four in the home file, and two of them within the window
    const auto home = std::find_if(sources.begin(), sources.end(), [](const Source& s) { return s.rel == kHomePath; });
    REQUIRE(home != sources.end());
    CHECK(found_in(home->text).size() == 4);
    CHECK(has_adjacent(home->text));
}

TEST_CASE("controls on a copy of the real sources: the copy passes and prints what the real tree prints; a second definition injected into it is caught; a constant removed from the definition is caught", "[secular_pole][real_tree]") {
    const fs::path real_root = sp::default_root();
    const Result real = run_tool({});
    REQUIRE(real.code == kOk);
    Tree t;
    const std::vector<Source> sources = production_sources(real_root);
    for (const Source& s : sources) t.write(s.rel, s.text);
    const Result clean = t.run();
    CHECK(clean.code == kOk);
    CHECK(clean.out == real.out);

    // (1) a second definition, in a file that holds none of the four, after enough blank lines that only the injected line is in the window that first reaches it
    std::string victim;
    std::string original;
    for (const Source& s : sources) {
        if (found_in(s.text).empty()) {
            victim = s.rel;
            original = s.text;
            break;
        }
    }
    REQUIRE_FALSE(victim.empty());
    t.write(victim, original + blanks(5) + "double xs = 55.0 + 1.677 * (t - 2000.0);\n");
    const Result copied = t.run();
    CHECK(copied.code == kFailed);
    // the injected line is the (N + 6)-th (N newlines in the original, five more, then the line); the first window to reach it is centred two lines above it: line N + 4
    CHECK(copied.err == "FAILED   " + victim + ":" + std::to_string(newlines_in(original) + 4) + " has 55.0, 1.677 within 2 lines\n" + std::string(kTrailer));
    CHECK(contains(copied.out, "; 1 put 2 or more within 2 lines"));
    t.write(victim, original);

    // (2) the definition loses a constant
    std::string home_text;
    for (const Source& s : sources) {
        if (s.rel == kHomePath) home_text = s.text;
    }
    REQUIRE_FALSE(home_text.empty());
    std::string without = home_text;
    for (std::size_t at = without.find("3.460"); at != std::string::npos; at = without.find("3.460", at)) without.replace(at, 5, "3.461");
    t.write(kHomePath, without);
    const Result lost = t.run();
    CHECK(lost.code == kFailed);
    CHECK(contains(lost.out, "MISSING  3.460 is not in " + kHomePath + "\n"));
    CHECK(lost.err == "\n" + kHomePath + " does not carry all four constants\n");
}
