// tests/devtools/constraint8_tests.cpp — plan section 5 constraint 8's checker: its two scanners against the patterns they replace, its refusals and its printed text on synthetic trees, and the real
// tree.  (ctests `constraint8.behaviour` and `constraint8.real_tree`; the Python tool had no test of its own, so these are the cases that show it CAN fail.)
//
// Every expectation is DERIVED BY HAND from the two regular expressions and the print statements of constraint8.py, not taken from running the port.
//
// A NOTE ON THIS FILE'S OWN TEXT: tools/constraint8 scans every .hpp .cpp .h .cc .ipp file of tests/ for the very texts these cases are about, on every line that does not begin with a comment
// marker -- trailing comments included.  So no line of this file holds the underlying type's name or a monadic call in one piece: the inputs are assembled at run time (`underlying`, `call`), and
// the prose about them is on lines of its own.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "constraint8.hpp"
#include "throwing_stream.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
namespace c8 = odl::tools::constraint8;

namespace {

constexpr int kOk = 0, kViolation = 1, kArgument = 2;

struct Result {
    int code = -1;
    std::string out;
    std::string err;
    [[nodiscard]] std::string both() const { return out + err; }
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = c8::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// The underlying type's name, `<scope>::<member>`, assembled so that this file holds it nowhere in one piece.
std::string underlying(const std::string& member) { return std::string("t") + "l" + "::" + member; }

// A method-call syntax, `.<name><spacing>(`.
std::string call(const std::string& name, const std::string& before = "", const std::string& after = "") { return "." + before + name + after + "("; }

constexpr const char* kAlias = "modules/core/include/odl/core/result.hpp";

struct Tree {
    TempDir td;
    fs::path root;
    Tree() : root(td.path()) {}
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

std::string violation_81(const std::string& rel, std::size_t line, const std::string& text) {
    return "VIOLATION  " + rel + ":" + std::to_string(line) + ": names the underlying type\n    " + text + "\n    Constraint 8.1: every signature names odl::Result. Only\n    " + kAlias + " may name " +
           underlying("expected") + ".\n\n";
}

std::string violation_82(const std::string& rel, std::size_t line, const std::string& op, const std::string& text) {
    return "VIOLATION  " + rel + ":" + std::to_string(line) + ": monadic operation '" + op + "'\n    " + text +
           "\n    Constraint 8.2: construction, checking and unwrapping are where\n    " + underlying("expected") + " and std::expected are interchangeable; the monadic\n"
           "    operations are where they diverge. Using one forfeits the one-line\n    migration D7 was chosen for. Write it with an if and an early return.\n\n";
}

}  // namespace

// ======================================================================================================================================== the scanners

TEST_CASE("names_underlying_type: the scope and the member, not glued to a word on either side", "[constraint8][behaviour][scan]") {
    const auto names = [](const std::string& line) { return c8::scan::names_underlying_type(line); };
    CHECK(names(underlying("expected") + "<int, E>"));
    CHECK(names("using R = " + underlying("expected") + "<T, E>;"));
    CHECK(names(underlying("unexpected") + "(e)"));
    CHECK(names("return " + underlying("unexpected") + "<E>{e};"));
    CHECK(names("(" + underlying("expected")));
    CHECK(names(underlying("expected")));   // at the end of the line
    CHECK(names("a " + underlying("expected") + " b"));
    CHECK(names("\xE2\x86\x92" + underlying("expected")));   // an arrow is no word character
    CHECK(names("x" + underlying("expected") + " " + underlying("expected")));   // the second one counts
    // a word character before the scope (including '_' and a letter of any script), or after the member, is not a match
    CHECK_FALSE(names("x" + underlying("expected")));
    CHECK_FALSE(names("_" + underlying("expected")));
    CHECK_FALSE(names("s" + underlying("expected")));
    CHECK_FALSE(names("\xC3\xB1" + underlying("expected")));   // n with a tilde: `\w` is Unicode-aware
    CHECK_FALSE(names("1" + underlying("expected")));
    CHECK_FALSE(names(underlying("expected") + "_value"));
    CHECK_FALSE(names(underlying("expected") + "x"));
    CHECK_FALSE(names(underlying("unexpected") + "ly"));
    CHECK_FALSE(names(underlying("expected") + "\xC3\xA9"));
    CHECK_FALSE(names(underlying("exp")));
    CHECK_FALSE(names("t" + std::string("l") + ":expected"));   // one colon
    CHECK_FALSE(names("t" + std::string("l") + ":xexpected"));  // one colon and any character: the scope is two colons, not `tl:` and one more byte
    CHECK_FALSE(names("std::expected<int, E>"));
    CHECK_FALSE(names("expected"));
    CHECK_FALSE(names(""));
}

TEST_CASE("monadic_operation: a METHOD CALL of one of the four, with white space allowed on either side of the name", "[constraint8][behaviour][scan]") {
    const auto op = [](const std::string& line) { return c8::scan::monadic_operation(line); };
    CHECK(op("r" + call("and_then")) == std::optional<std::string>("and_then"));
    CHECK(op("r" + call("or_else", "", " ")) == std::optional<std::string>("or_else"));
    CHECK(op("r. " + call("transform_error").substr(1)) == std::optional<std::string>("transform_error"));
    CHECK(op("r" + call("transform")) == std::optional<std::string>("transform"));
    CHECK(op("r" + call("transform_error", "", " ")) == std::optional<std::string>("transform_error"));   // the longer name is tried first
    CHECK(op("r" + call("and_then", " \t")) == std::optional<std::string>("and_then"));
    CHECK(op("a.b" + call("transform")) == std::optional<std::string>("transform"));                       // the second dot
    CHECK(op("x.\xC2\xA0" + call("transform").substr(1)) == std::optional<std::string>("transform"));      // NO-BREAK SPACE is `\s`
    CHECK(op("a" + call("or_else") + "f)" + call("and_then") + "g)") == std::optional<std::string>("or_else"));   // the leftmost
    CHECK(op("a" + call("transform") + "f)" + call("and_then") + "g)") == std::optional<std::string>("transform"));
    CHECK(op("a.." + call("and_then").substr(1)) == std::optional<std::string>("and_then"));                 // the first dot is followed by a dot: the SECOND one is the method call's
    // the name must follow the dot directly (white space aside) and be the whole word before the parenthesis
    CHECK_FALSE(op("r" + call("transform_foo")));
    CHECK_FALSE(op("r." + std::string("x") + "and_then("));
    CHECK_FALSE(op("r" + call("and_thenx")));
    CHECK_FALSE(op("r" + call("transformx")));
    CHECK_FALSE(op("r.and_then"));              // no parenthesis
    CHECK_FALSE(op("r.and_then;"));
    CHECK_FALSE(op("r.and_then x ("));
    CHECK_FALSE(op("r->transform("));          // an arrow is not a dot
    CHECK_FALSE(op("std::transform(a, b, c, f)"));
    CHECK_FALSE(op("std::ranges::transform(a, f)"));
    CHECK_FALSE(op("transform(a)"));
    CHECK_FALSE(op("a.map(f)"));
    CHECK_FALSE(op(".(x)"));
    CHECK_FALSE(op(""));
}

// ======================================================================================================================================== behaviour on synthetic trees

TEST_CASE("a clean tree passes, and what it prints is derived from the Python's print statement", "[constraint8][behaviour]") {
    Tree t;
    t.write("modules/a/include/a.hpp", "// " + underlying("expected") + " in a comment, and " + call("and_then") + " too\n * " + underlying("unexpected") + "\nint f();\n");
    t.write("tests/t.cpp", "int main() { return 0; }\n");
    const Result r = t.run();
    CHECK(r.code == kOk);
    CHECK(r.out == "ok       2 files, no constraint-8 violation (no monadic use, underlying type named only in the alias header)\n");
    CHECK(r.err.empty());
}

TEST_CASE("the underlying type named outside the alias header is violation 8.1, with the Python's text, the line stripped and numbered; the alias header may name it", "[constraint8][behaviour]") {
    Tree t;
    t.write("modules/a/bad.hpp", "int a;\n\n    " + underlying("expected") + "<int, E> f();   \n");
    const Result r = t.run();
    CHECK(r.code == kViolation);
    CHECK(r.out.empty());
    CHECK(r.err == violation_81("modules/a/bad.hpp", 3, underlying("expected") + "<int, E> f();") + "1 violation(s) of plan \xC2\xA7" "5 constraint 8 in 1 files\n");
    // the alias header itself: allowed to name it ...
    Tree alias;
    alias.write(kAlias, "template <class T, class E> using Result = " + underlying("expected") + "<T, E>;\n");
    const Result ok = alias.run();
    CHECK(ok.code == kOk);
    CHECK(ok.out == "ok       1 files, no constraint-8 violation (no monadic use, underlying type named only in the alias header)\n");
    // ... but a monadic call there is still a violation (8.2 has no exception)
    Tree alias_monad;
    alias_monad.write(kAlias, "auto g = r" + call("transform") + "f);\n");
    const Result bad = alias_monad.run();
    CHECK(bad.code == kViolation);
    CHECK(contains(bad.err, violation_82(kAlias, 1, "transform", "auto g = r" + call("transform") + "f);")));
    // a file of the same name elsewhere is not the alias
    Tree elsewhere;
    elsewhere.write("modules/other/include/odl/core/result.hpp", "using R = " + underlying("expected") + "<int, int>;\n");
    CHECK(elsewhere.run().code == kViolation);
}

TEST_CASE("each monadic operation is violation 8.2, naming it; one line can carry both violations, in the order 8.1 then 8.2", "[constraint8][behaviour]") {
    Tree t;
    t.write("modules/a/m.cpp",
            "auto a = r" + call("and_then") + "f);\nauto b = r" + call("or_else") + "f);\nauto c = r" + call("transform_error") + "f);\nauto d = r" + call("transform") + "f);\nint ok = 0;\n");
    const Result r = t.run();
    CHECK(r.code == kViolation);
    CHECK(r.err == violation_82("modules/a/m.cpp", 1, "and_then", "auto a = r" + call("and_then") + "f);") + violation_82("modules/a/m.cpp", 2, "or_else", "auto b = r" + call("or_else") + "f);") +
                       violation_82("modules/a/m.cpp", 3, "transform_error", "auto c = r" + call("transform_error") + "f);") +
                       violation_82("modules/a/m.cpp", 4, "transform", "auto d = r" + call("transform") + "f);") + "4 violation(s) of plan \xC2\xA7" "5 constraint 8 in 1 files\n");
    Tree both;
    both.write("modules/a/b.hpp", underlying("expected") + "<int, E> g = h" + call("and_then") + "k);\n");
    const std::string line = underlying("expected") + "<int, E> g = h" + call("and_then") + "k);";
    const Result rb = both.run();
    CHECK(rb.code == kViolation);
    CHECK(rb.err == violation_81("modules/a/b.hpp", 1, line) + violation_82("modules/a/b.hpp", 1, "and_then", line) + "2 violation(s) of plan \xC2\xA7" "5 constraint 8 in 1 files\n");
}

TEST_CASE("a line is a comment to this tool when it BEGINS (after white space) with // or *, and not otherwise: a trailing comment and a block opener are scanned", "[constraint8][behaviour]") {
    Tree t;
    t.write("modules/a/c.cpp",
            "  // " + underlying("expected") + "\n\t* " + underlying("expected") + "\n   *" + underlying("expected") + "\nint x = 0;   // " + underlying("expected") + "\n/* " + underlying("expected") + " */\n");
    const Result r = t.run();
    CHECK(r.code == kViolation);
    // lines 4 (a trailing comment) and 5 (a line that begins with /*) are scanned; 1-3 are not
    CHECK(contains(r.err, "modules/a/c.cpp:4: names the underlying type\n"));
    CHECK(contains(r.err, "modules/a/c.cpp:5: names the underlying type\n"));
    CHECK_FALSE(contains(r.err, "modules/a/c.cpp:1:"));
    CHECK_FALSE(contains(r.err, "modules/a/c.cpp:2:"));
    CHECK_FALSE(contains(r.err, "modules/a/c.cpp:3:"));
    CHECK(contains(r.err, "2 violation(s) of plan \xC2\xA7" "5 constraint 8 in 1 files\n"));
}

TEST_CASE("what is scanned: modules/ and tests/, the five suffixes, pathlib's order, files below a build directory too; what is not", "[constraint8][behaviour]") {
    const std::string bad = underlying("expected") + " x;\n";
    Tree t;
    for (const char* name : {"s.hpp", "s.cpp", "s.h", "s.cc", "s.ipp", "s.tar.hpp"}) t.write(std::string("modules/m/") + name, bad);
    for (const char* name : {"s.txt", "s.md", "s.cmake", "s.HPP", "s.hpp.bak", ".hpp", "hpp", "s.c", "s."}) t.write(std::string("modules/m/") + name, bad);   // none of these suffixes
    t.write("tests/t.cpp", bad);
    t.write("tools/x.cpp", bad);       // not under modules/ or tests/
    t.write("spec/x.hpp", bad);
    t.write("modules/m/build/b.hpp", bad);   // constraint8 does NOT exclude a build directory
    const Result r = t.run();
    CHECK(r.code == kViolation);
    // 6 suffix matches + the build one + the test: 8 files, in the order modules (sorted as pathlib sorts) and then tests
    CHECK(contains(r.err, "constraint 8 in 8 files\n"));
    const std::vector<std::string> order = {"modules/m/build/b.hpp:1", "modules/m/s.cc:1", "modules/m/s.cpp:1", "modules/m/s.h:1", "modules/m/s.hpp:1", "modules/m/s.ipp:1", "modules/m/s.tar.hpp:1", "tests/t.cpp:1"};
    std::size_t at = 0;
    for (const std::string& place : order) {
        const std::size_t found = r.err.find("VIOLATION  " + place + ":", at);
        INFO(place);
        REQUIRE(found != std::string::npos);
        at = found;
    }
    CHECK_FALSE(contains(r.err, "tools/x.cpp"));
    CHECK_FALSE(contains(r.err, "spec/x.hpp"));
    CHECK_FALSE(contains(r.err, "s.HPP"));
    CHECK_FALSE(contains(r.err, "s.txt"));
    // pathlib's order is component by component: "a/b.cpp" comes before "a-b/c.cpp" (as a string '-' sorts before '/')
    Tree sorted;
    sorted.write("modules/a-b/c.cpp", bad);
    sorted.write("modules/a/b.cpp", bad);
    const Result rs = sorted.run();
    CHECK(rs.err.find("modules/a/b.cpp:1") < rs.err.find("modules/a-b/c.cpp:1"));
}

TEST_CASE("an entry that is no regular file is skipped by rule, a link to a file is read, a link to a directory is not entered; a file that is not UTF-8 is read with replacement characters", "[constraint8][behaviour]") {
    Tree t;
    fs::create_directories(t.root / "modules/m/odd.cpp");   // a directory with a source's name
    t.write("modules/m/real.hpp", "int a;\n");
    t.write("elsewhere/inside.hpp", underlying("expected") + " in a directory reached through a link\n");
    std::error_code ec;
    fs::create_directory_symlink(t.root / "elsewhere", t.root / "modules/m/linked_dir", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "modules/m/real.hpp", t.root / "modules/m/alias.hpp", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "nowhere", t.root / "modules/m/dangling.hpp", ec);
    REQUIRE_FALSE(ec);
    REQUIRE(::mkfifo((t.root / "modules/m/fifo.cpp").c_str(), 0600) == 0);
    t.write("modules/m/latin1.cpp", std::string("caf\xE9 ") + underlying("expected") + " x;\n");
    const Result r = t.run();
    CHECK(r.code == kViolation);
    CHECK_FALSE(contains(r.both(), "internal error"));
    CHECK_FALSE(contains(r.err, "linked_dir"));
    // real.hpp, alias.hpp (the link to it) and latin1.cpp are read: three files
    CHECK(contains(r.err, "constraint 8 in 3 files\n"));
    // the byte that is not UTF-8 becomes U+FFFD in the line quoted
    CHECK(contains(r.err, "    caf\xEF\xBF\xBD " + underlying("expected") + " x;\n"));
}

TEST_CASE("lines are counted as str.splitlines counts them: CRLF and CR are one line end, and so are a form feed and U+2028", "[constraint8][behaviour]") {
    const std::string bad = "x" + call("transform") + "f);";
    Tree t;
    t.write("modules/m/a.cpp", "a\r\n" + bad + "\n");
    t.write("modules/m/b.cpp", "a\r" + bad + "\n");
    t.write("modules/m/c.cpp", "a\f" + bad + "\n");
    t.write("modules/m/d.cpp", "a\xE2\x80\xA8" + bad + "\n");
    t.write("modules/m/e.cpp", "a\n\n\n" + bad + "\n");
    const Result r = t.run();
    CHECK(r.code == kViolation);
    for (const char* name : {"a", "b", "c", "d"}) CHECK(contains(r.err, std::string("modules/m/") + name + ".cpp:2: monadic operation 'transform'\n"));
    CHECK(contains(r.err, "modules/m/e.cpp:4: monadic operation 'transform'\n"));
}

TEST_CASE("the line quoted in either violation is stripped of white space, Unicode's too, and not cut", "[constraint8][behaviour]") {
    Tree t;
    const std::string padded_82 = "auto a = r" + call("transform") + "f);";
    const std::string padded_81 = underlying("unexpected") + "<E> e;";
    t.write("modules/m/pad.cpp", "\t\xE3\x80\x80    " + padded_82 + "   \xC2\xA0 \n" + std::string(2, ' ') + padded_81 + std::string(100, ' ') + "\n");
    const Result r = t.run();
    CHECK(r.code == kViolation);
    CHECK(r.err == violation_82("modules/m/pad.cpp", 1, "transform", padded_82) + violation_81("modules/m/pad.cpp", 2, padded_81) + "2 violation(s) of plan \xC2\xA7" "5 constraint 8 in 1 files\n");
}

TEST_CASE("a file that cannot be read is REFUSED, exit 2, naming it -- never skipped", "[constraint8][behaviour]") {
    if (::geteuid() == 0) SKIP("root opens every file");
    Tree t;
    t.write("modules/m/a.hpp", "int a;\n");
    t.write("modules/m/locked.hpp", "int b;\n");
    REQUIRE(::chmod((t.root / "modules/m/locked.hpp").c_str(), 0) == 0);
    const Result r = t.run();
    CHECK(r.code == kArgument);
    CHECK(r.out.empty());
    CHECK(contains(r.err, "constraint8: cannot read " + (t.root / "modules/m/locked.hpp").string() + ": "));
    REQUIRE(::chmod((t.root / "modules/m/locked.hpp").c_str(), 0600) == 0);
}

TEST_CASE("an error the tool did not anticipate is reported as such, exit 70, and not as a pass or a violation", "[constraint8][behaviour]") {
    Tree t;
    t.write("modules/m/a.hpp", "int a;\n");
    odl::devtools_testing::ThrowingStream out;   // every write to it throws: the verdict of a clean run cannot be printed
    std::ostringstream err;
    const int code = c8::run({"--root", t.root.string()}, Streams{out, err});
    CHECK(code == 70);
    CHECK(err.str() == "constraint8: internal error: boom\n");
}

TEST_CASE("a tree with nothing to scan is REFUSED, exit 2, naming the root; the command line", "[constraint8][behaviour]") {
    {
        Tree t;
        const Result r = t.run();
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(contains(r.err, "constraint8: nothing to scan: no .hpp, .cpp, .h, .cc or .ipp file under " + (t.root / "modules").string() + " or " + (t.root / "tests").string() + "\n"));
    }
    {
        Tree t;
        t.write("modules/m/notes.txt", "no source here\n");
        CHECK(t.run().code == kArgument);
    }
    {
        Tree t;
        t.write("modules/m/a.hpp", "int a;\n");
        CHECK(run_tool({"--root=" + t.root.string()}).code == kOk);
        const Result help = run_tool({"-h"});
        CHECK(help.code == kOk);
        CHECK(help.out.rfind("usage: constraint8 [-h] [--root ROOT]\n", 0) == 0);
        CHECK(contains(help.out, "options:\n  -h, --help   show this help and exit\n"));
        CHECK(contains(help.out, "exit codes: 0 clean   1 a violation   2 an argument error, or nothing to scan\n"));
        CHECK(help.err.empty());
        CHECK(run_tool({"--help"}).out == help.out);
        CHECK(run_tool({"-h=x"}).code == kArgument);                  // a short option takes no value
        CHECK(run_tool({"--root=" + t.root.string(), "--help"}).out == help.out);
        // an option's value is whatever follows it unless that begins with `--` (so a directory called -x is a value)
        const Result dash = run_tool({"--root", "-x"});
        CHECK(dash.code == kArgument);
        CHECK(contains(dash.err, "constraint8: nothing to scan: no .hpp, .cpp, .h, .cc or .ipp file under -x/modules or -x/tests\n"));
        const Result missing = run_tool({"--root"});
        CHECK(missing.code == kArgument);
        CHECK(contains(missing.err, "constraint8: error: argument --root: expected one argument\n"));
        const Result unknown = run_tool({"--bogus"});
        CHECK(unknown.code == kArgument);
        CHECK(contains(unknown.err, "usage: constraint8 [-h] [--root ROOT]\nconstraint8: error: unrecognized arguments: --bogus\n"));
        CHECK(run_tool({"--ro", t.root.string()}).code == kArgument);   // no abbreviations
        CHECK(run_tool({"stray"}).code == kArgument);
    }
}

// ======================================================================================================================================== the real tree

TEST_CASE("the real tree: the constraint holds, and the count is the number of files with those suffixes below modules/ and tests/ that a second, independent walk finds", "[constraint8][real_tree]") {
    const Result r = run_tool({});
    INFO(r.both());
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    const std::regex shape(R"(^ok       (\d+) files, no constraint-8 violation \(no monadic use, underlying type named only in the alias header\)\n$)");
    std::smatch m;
    REQUIRE(std::regex_match(r.out, m, shape));
    std::size_t counted = 0;
    for (const char* base : {"modules", "tests"}) {
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(c8::default_root() / base)) {
            if (!entry.is_regular_file()) continue;
            const std::string suffix = entry.path().extension().string();
            if (suffix == ".hpp" || suffix == ".cpp" || suffix == ".h" || suffix == ".cc" || suffix == ".ipp") ++counted;
        }
    }
    CHECK(std::stoull(m[1].str()) == counted);
}

TEST_CASE("controls on a copy of the real sources: the copy passes and prints what the real tree prints; each defect injected into it is caught, by the rule it breaks; the same text in a comment is not one", "[constraint8][real_tree]") {
    const fs::path real_root = c8::default_root();
    const Result real = run_tool({});
    REQUIRE(real.code == kOk);
    Tree t;
    std::vector<std::string> copied;   // relative paths of every file the tool reads, in the order of the walk below (not the tool's)
    for (const char* base : {"modules", "tests"}) {
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(real_root / base)) {
            if (!entry.is_regular_file()) continue;
            const std::string suffix = entry.path().extension().string();
            if (suffix != ".hpp" && suffix != ".cpp" && suffix != ".h" && suffix != ".cc" && suffix != ".ipp") continue;
            const std::string rel = fs::relative(entry.path(), real_root).generic_string();
            fs::create_directories((t.root / rel).parent_path());
            fs::copy_file(entry.path(), t.root / rel);
            copied.push_back(rel);
        }
    }
    REQUIRE(copied.size() > 2);
    const Result clean = t.run();
    CHECK(clean.code == kOk);
    CHECK(clean.out == real.out);
    CHECK(clean.out == "ok       " + std::to_string(copied.size()) + " files, no constraint-8 violation (no monadic use, underlying type named only in the alias header)\n");

    // a module header that is not the alias, and a test source
    std::string module_file;
    std::string test_file;
    for (const std::string& rel : copied) {
        if (module_file.empty() && rel.rfind("modules/", 0) == 0 && rel != kAlias) module_file = rel;
        if (test_file.empty() && rel.rfind("tests/", 0) == 0) test_file = rel;
    }
    REQUIRE_FALSE(module_file.empty());
    REQUIRE_FALSE(test_file.empty());
    const std::string module_original = read_text(t.root / module_file);
    const std::string test_original = read_text(t.root / test_file);
    const auto lines_of = [](const std::string& s) { return static_cast<std::size_t>(std::count(s.begin(), s.end(), '\n')); };

    // (1) the underlying type named in a module: rule 8.1, at the line it was written
    const std::string leaked = underlying("expected") + "<int, int> leaked;";
    t.write(module_file, module_original + "\n" + leaked + "\n");
    const Result r1 = t.run();
    CHECK(r1.code == kViolation);
    CHECK(r1.err == violation_81(module_file, lines_of(module_original) + 2, leaked) + "1 violation(s) of plan \xC2\xA7" "5 constraint 8 in " + std::to_string(copied.size()) + " files\n");

    // (2) a monadic call in a test: rule 8.2
    const std::string chained = "auto r = x" + call("or_else") + "f);";
    t.write(module_file, module_original);
    t.write(test_file, test_original + "\n" + chained + "\n");
    const Result r2 = t.run();
    CHECK(r2.code == kViolation);
    CHECK(r2.err == violation_82(test_file, lines_of(test_original) + 2, "or_else", chained) + "1 violation(s) of plan \xC2\xA7" "5 constraint 8 in " + std::to_string(copied.size()) + " files\n");

    // (3) the same two texts in comments, and one after code: the first two are no violation, the third is (the tool skips only lines that BEGIN as a comment)
    t.write(test_file, test_original);
    t.write(module_file, module_original + "\n// " + leaked + "\n * " + chained + "\n");
    CHECK(t.run().code == kOk);
    t.write(module_file, module_original + "\nint spare = 0;   // " + leaked + "\n");
    const Result r3 = t.run();
    CHECK(r3.code == kViolation);
    CHECK(contains(r3.err, module_file + ":" + std::to_string(lines_of(module_original) + 2) + ": names the underlying type\n"));
    t.write(module_file, module_original);
    t.write(test_file, test_original);

    // (4) the alias header: it may name the underlying type, and may not chain
    const std::string alias_original = read_text(t.root / kAlias);
    t.write(kAlias, alias_original + "\n" + leaked + "\n");
    CHECK(t.run().code == kOk);
    t.write(kAlias, alias_original + "\n" + chained + "\n");
    const Result r4 = t.run();
    CHECK(r4.code == kViolation);
    CHECK(contains(r4.err, violation_82(kAlias, lines_of(alias_original) + 2, "or_else", chained)));
    t.write(kAlias, alias_original);
    CHECK(t.run().out == real.out);
}
