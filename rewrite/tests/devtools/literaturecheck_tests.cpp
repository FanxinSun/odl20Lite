// tests/devtools/literaturecheck_tests.cpp — plan section 5 constraint 3's checker, "nothing that builds may reach the literature": what it prints, what it refuses, which files it reads and what it
// looks for, on synthetic trees; and the real tree.  (ctests `literaturecheck.behaviour` and `literaturecheck.real_tree`; the Python tool had no test of its own, so these are the cases that show
// it CAN fail.)
//
// Every expectation is DERIVED BY HAND from the print statements and the loops of literaturecheck.py, not taken from running the port.  Where the port deliberately differs (the needles are visited
// in sorted order; a root under a directory called `build` is searched; nothing to search, a manifest that cannot be read and a malformed entry are refused with exit 2) the case says so.
//
// A NOTE ON THIS FILE'S OWN TEXT: tools/literaturecheck scans every .cpp of tests/ for the literature root and for every literature entry's id and file name -- comments included.  So this file
// holds none of them: the default root is assembled at run time (`default_root_text`), the entries of the synthetic manifests are invented, and the real-tree cases read the real ones from the
// manifest.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/tool.hpp>

#include "literaturecheck.hpp"
#include "throwing_stream.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
namespace lc = odl::tools::literaturecheck;

namespace {

constexpr int kOk = 0, kReached = 1, kArgument = 2;

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = lc::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// The root the manifest names when it names none.
std::string default_root_text() { return std::string("data/") + "lit" + "erature"; }

// `{:5d}`
std::string right5(std::size_t n) {
    const std::string s = std::to_string(n);
    return std::string(5 - std::min<std::size_t>(5, s.size()), ' ') + s;
}

// the three counts, as the Python's three print statements print them (the first begins with the blank line)
std::string counts(std::size_t entries, std::size_t inputs, std::size_t hits) {
    return "\n  literature entries              " + right5(entries) + "\n  build inputs searched           " + right5(inputs) + "\n  build inputs REACHING one       " + right5(hits) + "\n";
}

const char kVerdict[] = "\nok       no build input can reach a literature entry\n";
const char kHeading[] = "LITERATURE ENTRIES \xE2\x80\x94 pinned for provenance, exempt from the licence gate,\nand unreachable from anything that builds.\n\n";
const char kTrailer[] =
    "\n  That turns a citation into a dependency, and the licence exemption plan \xC2\xA7" "5\n"
    "  constraint 3 grants does not cover it: the exemption holds because this tree\n"
    "  READS these and does not redistribute or build against them. If the build needs\n"
    "  it, it is not literature -- pin it as data, with a licence that passes the gate.\n";

// one line of the failing listing, and the whole listing
std::string hit(const std::string& file, std::size_t line, const std::string& needle, const std::string& text) {
    return "  " + file + ":" + std::to_string(line) + "  mentions '" + needle + "'\n      " + text + "\n";
}

std::string failure(const std::vector<std::string>& hits) {
    std::string out = "\nA BUILD INPUT REACHES A LITERATURE ENTRY:\n";
    for (const std::string& h : hits) out += h;
    return out + kTrailer;
}

// a manifest: `entries` as given (each the text of one JSON object), and a `literature` member when `literature_json` is not empty
std::string lit(const std::string& id, const std::string& filename, const std::string& terms_json = "") {
    return R"({"kind": "literature", "id": ")" + id + R"(", "filename": ")" + filename + "\"" + (terms_json.empty() ? std::string() : ", \"terms\": " + terms_json) + "}";
}

std::string manifest(const std::vector<std::string>& entries, const std::string& literature_json = "") {
    std::string out = "{\n";
    if (!literature_json.empty()) out += "  \"literature\": " + literature_json + ",\n";
    out += "  \"entries\": [";
    for (std::size_t i = 0; i < entries.size(); ++i) out += (i == 0 ? "\n    " : ",\n    ") + entries[i];
    return out + "\n  ]\n}\n";
}

struct Tree {
    TempDir td;
    fs::path root;
    // `manifest_text` empty: no manifest file at all.  `below`: the tree's root is this directory below the temporary one (to put it under a directory with a telling name).
    explicit Tree(const std::string& manifest_text, const std::string& below = "") : root(below.empty() ? td.path() : td.path() / below) {
        if (!manifest_text.empty()) write("manifest/manifest.json", manifest_text);
    }
    void write(const std::string& relative, const std::string& content) {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, content);
    }
    [[nodiscard]] fs::path manifest_path() const { return root / "manifest" / "manifest.json"; }
    [[nodiscard]] Result run(std::vector<std::string> extra = {}) const {
        std::vector<std::string> args = {"--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        return run_tool(args);
    }
};

// the two invented entries most cases use
std::string two_entries() { return manifest({lit("alpha-2001-fake", "alpha.pdf", R"("NOT ESTABLISHED: nothing found")"), lit("beta-2002-fake", "beta.pdf", R"("Licence: CC-BY 4.0")")}); }

}  // namespace

// ======================================================================================================================================== what is printed

TEST_CASE("a clean tree: the quiet run prints the three counts and the verdict, nothing else", "[literaturecheck][behaviour]") {
    Tree t(two_entries());
    t.write("CMakeLists.txt", "project(x)\n");
    t.write("modules/m/a.cpp", "int a;\n");
    t.write("tests/t.cpp", "int t;\n");
    t.write("cmake/x.cmake", "set(X 1)\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kOk);
    CHECK(r.out == counts(2, 4, 0) + kVerdict);
    CHECK(r.err.empty());
}

TEST_CASE("the listing: each entry, the path it is fetched to, and what its recorded terms say (the prefix NOT ESTABLISHED, in any case, through Python's full upper-casing)", "[literaturecheck][behaviour]") {
    struct Row {
        const char* id;
        const char* terms_json;   // "" omits the member
        bool not_established;
    };
    // expected values derived by hand from `terms.upper().startswith("NOT ESTABLISHED")`; the ten non-ASCII characters whose upper case is pure ASCII are the dotless i, the long s, the sharp
    // s (two letters) and the ff, fi, fl, ffi, ffl, long-s-t and st ligatures
    // (the JSON escapes of two of them, built at run time from a backslash and the digits)
    const std::string backslash_u = std::string("\\") + "u";
    const std::string escaped_dotless_i = "\"not establ" + backslash_u + "0131shed\"";
    const std::string escaped_st_ligature = "\"NOT E" + backslash_u + "FB06ABLISHED\"";
    const std::string nbsp_for_space = std::string("\"NOT") + "\xC2\xA0" + "ESTABLISHED\"";   // a NO-BREAK SPACE where the phrase has a blank
    const std::vector<Row> rows = {
        {"t-upper", R"("NOT ESTABLISHED: no licence found")", true},
        {"t-lower", R"("not established")", true},
        {"t-mixed", R"("Not Established, see PROVENANCE")", true},
        {"t-exact", R"("NOT ESTABLISHED")", true},
        {"t-short", R"("NOT ESTABLISHE")", false},
        {"t-found", R"("Licence: CC-BY 4.0, quoted from the page")", false},
        {"t-leading-space", R"("  NOT ESTABLISHED")", false},
        {"t-two-spaces", R"("NOT  ESTABLISHED")", false},
        {"t-empty", R"("")", false},
        {"t-absent", "", false},
        {"t-dotless-i", R"("not establıshed")", true},
        {"t-long-s", R"("not eſtablished")", true},
        {"t-st-ligature", R"("not eﬆablished")", true},
        {"t-long-s-t-ligature", R"("NOT EﬅABLISHED")", true},
        {"t-sharp-s", R"("not establißhed")", false},
        {"t-ff-ligature", R"("not eﬀablished")", false},      // the ligatures of f that upper-case to ASCII too, but to letters the phrase does not hold
        {"t-fi-ligature", R"("not eﬁablished")", false},
        {"t-fl-ligature", R"("not eﬂablished")", false},
        {"t-ffi-ligature", R"("not eﬃablished")", false},
        {"t-ffl-ligature", R"("not eﬄablished")", false},
        {"t-dotted-capital-i", R"("not establİshed")", false},
        {"t-escaped-dotless-i", escaped_dotless_i.c_str(), true},          // the same characters as JSON escapes, which the manifest's parser reads
        {"t-escaped-st-ligature", escaped_st_ligature.c_str(), true},
        {"t-nbsp-for-space", nbsp_for_space.c_str(), false},                // no non-ASCII character stands for a blank, a letter or nothing at all
        {"t-accent-inside", "\"NOT ESTA\xC3\xA9" "BLISHED\"", false},
        {"t-accent-before", "\"\xC3\xA9 NOT ESTABLISHED\"", false},
        {"t-accent-after", "\"NOT ESTABLISHED \xC3\xA9\"", true},               // after the phrase nothing counts
        {"t-number", "42", false},
        {"t-null", "null", false},
        {"t-list", R"(["NOT ESTABLISHED"])", false},
    };
    std::vector<std::string> entries;
    for (const Row& row : rows) entries.push_back(lit(row.id, std::string(row.id) + ".pdf", row.terms_json));
    Tree t(manifest(entries));
    t.write("CMakeLists.txt", "project(x)\n");
    std::string expected = kHeading;
    for (const Row& row : rows) {
        expected += std::string("  ") + row.id + "\n    fetched to  " + default_root_text() + "/" + row.id + "/" + row.id + ".pdf\n    terms       " +
                    (row.not_established ? "NOT established \xE2\x80\x94 see the entry" : "found \xE2\x80\x94 see the entry") + "\n";
    }
    expected += counts(rows.size(), 1, 0) + kVerdict;
    const Result r = t.run();
    CHECK(r.code == kOk);
    CHECK(r.out == expected);
    CHECK(r.err.empty());
}

TEST_CASE("the manifest's `literature` member is the root the entries are fetched to, and a root of the build inputs' text is reached", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("alpha-2001-fake", "alpha.pdf")}, R"("store/lit")"));
    t.write("modules/m/a.cpp", "// reads " + default_root_text() + "/alpha.pdf\n");   // the DEFAULT root is not this tree's root, and the entry's file name is not in it either
    t.write("modules/m/b.cpp", "const char* p = \"store/lit\";\n");
    const Result listing = t.run();
    CHECK(listing.code == kReached);
    CHECK(contains(listing.out, "  alpha-2001-fake\n    fetched to  store/lit/alpha-2001-fake/alpha.pdf\n    terms       found \xE2\x80\x94 see the entry\n"));
    // the file name alone is a needle: a.cpp's line holds "alpha.pdf", so it IS reached, by that and not by the default root
    CHECK(listing.err == failure({hit("modules/m/a.cpp", 1, "alpha.pdf", "// reads " + default_root_text() + "/alpha.pdf"), hit("modules/m/b.cpp", 1, "store/lit", "const char* p = \"store/lit\";")}));
    CHECK_FALSE(contains(listing.err, "'" + default_root_text() + "'"));
}

TEST_CASE("a build input that names an entry's id, its file name or the root is REACHED: exit 1, the counts on stdout, the listing and the explanation on stderr, the needles in sorted order", "[literaturecheck][behaviour]") {
    Tree t(two_entries());
    const std::string root_text = default_root_text();
    t.write("CMakeLists.txt", "add_executable(x main.cpp)\n");
    t.write("modules/m/x.cpp", "// see alpha-2001-fake and beta.pdf\nint clean;\n  open(\"" + root_text + "/alpha.pdf\");   \nALPHA-2001-FAKE\n");
    t.write("tests/t.cpp", "beta-2002-fake\n");
    const std::vector<std::string> expected_hits = {
        // x.cpp: the needles in sorted order: "alpha-2001-fake" ('-' before '.'), "alpha.pdf", "beta.pdf", the root; each needle's lines in order
        hit("modules/m/x.cpp", 1, "alpha-2001-fake", "// see alpha-2001-fake and beta.pdf"),
        hit("modules/m/x.cpp", 3, "alpha.pdf", "open(\"" + root_text + "/alpha.pdf\");"),
        hit("modules/m/x.cpp", 1, "beta.pdf", "// see alpha-2001-fake and beta.pdf"),
        hit("modules/m/x.cpp", 3, root_text, "open(\"" + root_text + "/alpha.pdf\");"),
        hit("tests/t.cpp", 1, "beta-2002-fake", "beta-2002-fake"),
    };
    const Result quiet = t.run({"--quiet"});
    CHECK(quiet.code == kReached);
    CHECK(quiet.out == counts(2, 3, 5));   // no verdict
    CHECK(quiet.err == failure(expected_hits));
    // without --quiet the listing comes first on stdout, the counts after it, and stderr is the same
    const Result loud = t.run();
    CHECK(loud.code == kReached);
    CHECK(loud.err == quiet.err);
    CHECK(loud.out.rfind(kHeading, 0) == 0);
    REQUIRE(loud.out.size() > quiet.out.size());
    CHECK(loud.out.compare(loud.out.size() - quiet.out.size(), quiet.out.size(), quiet.out) == 0);
}

TEST_CASE("a needle is plain text: what a regular expression would read in it means itself, case counts, and it is found inside a longer word", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("a+b(c)[d]", "f.pdf")}));
    t.write("modules/m/a.cpp", "xabcdx\n");                 // matched by the REGULAR EXPRESSION a+b(c)[d], not by the text
    t.write("modules/m/b.cpp", "fXpdf\n");                   // matched by f.pdf as an expression, not as text
    t.write("modules/m/c.cpp", "F.PDF a+B(c)[d]\n");         // the wrong case
    t.write("modules/m/d.cpp", "ya+b(c)[d]z\n");             // inside a longer word: reached
    t.write("modules/m/e.cpp", "xf.pdfx\n");                 // likewise
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.out == counts(1, 5, 2));
    CHECK(r.err == failure({hit("modules/m/d.cpp", 1, "a+b(c)[d]", "ya+b(c)[d]z"), hit("modules/m/e.cpp", 1, "f.pdf", "xf.pdfx")}));
}

TEST_CASE("lines are searched one at a time: a needle with a newline in it (a JSON escape in the manifest) is found nowhere", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("needle-u\\n", "needle-u.dat")}));    // the id is `needle-u` and a newline
    t.write("modules/m/a.cpp", "needle-u\nmore\n");               // `needle-u` ends a line and a line follows: the pair is not on one line
    t.write("modules/m/b.cpp", "needle-u\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kOk);
    CHECK(r.out == counts(1, 2, 0) + kVerdict);
}

TEST_CASE("an empty id, file name or root is no needle (it would match every line); the same needle twice is one", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("", ""), lit("dup-id", "dup-id"), lit("dup-id", "other-file")}, R"("")"));
    t.write("modules/m/a.cpp", "anything at all\nthe dup-id once\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.out == counts(3, 1, 1));
    CHECK(r.err == failure({hit("modules/m/a.cpp", 2, "dup-id", "the dup-id once")}));
}

TEST_CASE("lines are the text split on \\n after universal newlines: CR LF and a lone CR end a line, a form feed, a vertical tab, NEL and U+2028 do not", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("needle-x", "needle-x.dat")}));
    t.write("modules/m/a.cpp", "a\r\nneedle-x\n");
    t.write("modules/m/b.cpp", "a\rneedle-x\n");
    t.write("modules/m/c.cpp", "a\fneedle-x\n");
    t.write("modules/m/d.cpp", "a\xE2\x80\xA8needle-x\n");
    t.write("modules/m/e.cpp", "a\xC2\x85needle-x\n");
    t.write("modules/m/f.cpp", "a\vneedle-x\n");
    t.write("modules/m/g.cpp", "a\n\n\nneedle-x");   // no newline at the end
    t.write("modules/m/h.cpp", "needle-x\nneedle-x\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.out == counts(1, 8, 9));
    CHECK(r.err == failure({hit("modules/m/a.cpp", 2, "needle-x", "needle-x"), hit("modules/m/b.cpp", 2, "needle-x", "needle-x"), hit("modules/m/c.cpp", 1, "needle-x", "a\fneedle-x"),
                            hit("modules/m/d.cpp", 1, "needle-x", "a\xE2\x80\xA8needle-x"), hit("modules/m/e.cpp", 1, "needle-x", "a\xC2\x85needle-x"),
                            hit("modules/m/f.cpp", 1, "needle-x", "a\vneedle-x"), hit("modules/m/g.cpp", 4, "needle-x", "needle-x"), hit("modules/m/h.cpp", 1, "needle-x", "needle-x"),
                            hit("modules/m/h.cpp", 2, "needle-x", "needle-x")}));
}

TEST_CASE("the text shown for a hit is the line stripped of white space (Unicode's too) and cut at 88 CODE POINTS, not bytes", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("lit-id", "lit-id.dat")}));
    const std::string spaces = "\t \xC2\xA0\xE3\x80\x80";   // a tab, a space, NO-BREAK SPACE, IDEOGRAPHIC SPACE
    t.write("modules/m/a.cpp", spaces + "lit-id" + std::string(100, 'z') + "  \v\n");
    std::string eacutes;
    for (int i = 0; i < 100; ++i) eacutes += "\xC3\xA9";
    t.write("modules/m/b.cpp", "lit-id" + eacutes + "\n");
    t.write("modules/m/c.cpp", "lit-id" + std::string(82, 'z') + "\n");   // exactly 88 code points: kept whole
    t.write("modules/m/d.cpp", "lit-id" + std::string(83, 'z') + "\n");   // 89: one cut off
    t.write("modules/m/e.cpp", "x lit-id\n");                              // the needle itself may lie beyond the cut
    t.write("modules/m/f.cpp", std::string(90, 'w') + " lit-id\n");
    std::string eacutes_cut;
    for (int i = 0; i < 82; ++i) eacutes_cut += "\xC3\xA9";
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.err == failure({hit("modules/m/a.cpp", 1, "lit-id", "lit-id" + std::string(82, 'z')), hit("modules/m/b.cpp", 1, "lit-id", "lit-id" + eacutes_cut),
                            hit("modules/m/c.cpp", 1, "lit-id", "lit-id" + std::string(82, 'z')), hit("modules/m/d.cpp", 1, "lit-id", "lit-id" + std::string(82, 'z')),
                            hit("modules/m/e.cpp", 1, "lit-id", "x lit-id"), hit("modules/m/f.cpp", 1, "lit-id", std::string(88, 'w'))}));
}

// ======================================================================================================================================== which files are build inputs

TEST_CASE("the build inputs: the root CMakeLists.txt, and below modules/ tests/ and cmake/ every CMakeLists.txt, *.cmake, *.cpp, *.hpp, *.h and *.c, not below a directory called build; pathlib's order", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("needle-y", "needle-y.dat")}));
    const std::string line = "needle-y\n";
    const std::vector<std::string> searched_in_order = {
        "CMakeLists.txt", "cmake/Mod.cmake", "modules/CMakeLists.txt", "modules/m/.h", "modules/m/CMakeLists.txt", "modules/m/_deps/w.cpp", "modules/m/a.c", "modules/m/a.cmake", "modules/m/a.cpp",
        "modules/m/a.h", "modules/m/a.hpp", "modules/m/a.tar.cpp", "modules/m/build2/u.cpp", "modules/m/xbuild/v.cpp", "tests/t.cpp",
    };
    for (const std::string& f : searched_in_order) t.write(f, line);
    for (const char* f : {"modules/m/a.cc", "modules/m/a.ipp", "modules/m/a.txt", "modules/m/a.md", "modules/m/a.json", "modules/m/CMakeLists.txt.bak", "modules/m/cmakelists.txt", "modules/m/a.CPP",
                          "modules/m/a.cpp~", "modules/m/build/b.cpp", "tests/build/b.cpp", "cmake/build/b.cmake", "modules/build/c.cpp", "modules/m/build/deeper/d.hpp", "tools/x.cpp", "spec/x.hpp",
                          "data/x.cpp", "main.cpp", "sub/CMakeLists.txt", "manifest/x.cpp"}) {
        t.write(f, line);
    }
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.out == counts(1, searched_in_order.size(), searched_in_order.size()));
    std::vector<std::string> expected;
    for (const std::string& f : searched_in_order) expected.push_back(hit(f, 1, "needle-y", "needle-y"));
    CHECK(r.err == failure(expected));
}

TEST_CASE("an entry that is no regular file is skipped by rule, a link to a file is read, a link to a directory is not entered, a file that is not UTF-8 is read with replacement characters", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("needle-z", "needle-z.dat")}));
    fs::create_directories(t.root / "modules/m/odd.cpp");   // a directory with a source's name
    t.write("modules/m/real.cpp", "needle-z\n");
    t.write("elsewhere/inside.cpp", "needle-z in a directory reached through a link\n");
    std::error_code ec;
    fs::create_directory_symlink(t.root / "elsewhere", t.root / "modules/m/linked", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "modules/m/real.cpp", t.root / "modules/m/w.h", ec);
    REQUIRE_FALSE(ec);
    fs::create_symlink(t.root / "nowhere", t.root / "modules/m/y.hpp", ec);
    REQUIRE_FALSE(ec);
    REQUIRE(::mkfifo((t.root / "modules/m/z.c").c_str(), 0600) == 0);
    t.write("modules/m/lat.cpp", std::string("caf\xE9 needle-z\n"));
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    // lat.cpp, real.cpp and the link to it (w.h), in that order
    CHECK(r.out == counts(1, 3, 3));
    CHECK(r.err == failure({hit("modules/m/lat.cpp", 1, "needle-z", "caf\xEF\xBF\xBD needle-z"), hit("modules/m/real.cpp", 1, "needle-z", "needle-z"), hit("modules/m/w.h", 1, "needle-z", "needle-z")}));
    // a root CMakeLists.txt that is a directory is no input either
    Tree d(manifest({lit("needle-z", "needle-z.dat")}));
    fs::create_directories(d.root / "CMakeLists.txt");
    d.write("modules/m/a.cpp", "int a;\n");
    const Result rd = d.run({"--quiet"});
    CHECK(rd.code == kOk);
    CHECK(rd.out == counts(1, 1, 0) + kVerdict);
}

TEST_CASE("a tree under a directory called build is searched all the same (the Python tested the absolute path, and passed over every file of such a tree)", "[literaturecheck][behaviour]") {
    Tree t(manifest({lit("needle-w", "needle-w.dat")}), "build/deeper");
    t.write("modules/m/a.cpp", "needle-w\n");
    t.write("CMakeLists.txt", "project(x)\n");
    t.write("modules/m/build/skipped.cpp", "needle-w\n");   // below a build directory of the TREE: still skipped
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.out == counts(1, 2, 1));
    CHECK(r.err == failure({hit("modules/m/a.cpp", 1, "needle-w", "needle-w")}));
}

TEST_CASE("a directory that is missing is passed over: the others are searched all the same", "[literaturecheck][behaviour]") {
    for (const char* present : {"modules", "tests", "cmake"}) {
        Tree t(manifest({lit("needle-v", "needle-v.dat")}));
        t.write(std::string(present) + "/a.cpp", "needle-v\n");
        const Result r = t.run({"--quiet"});
        INFO(present);
        CHECK(r.code == kReached);
        CHECK(r.out == counts(1, 1, 1));
        CHECK(r.err == failure({hit(std::string(present) + "/a.cpp", 1, "needle-v", "needle-v")}));
    }
    // two of the three missing, and no root CMakeLists.txt
    Tree t(manifest({lit("needle-v", "needle-v.dat")}));
    t.write("modules/a.cpp", "int a;\n");
    t.write("cmake/b.cmake", "needle-v\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(r.out == counts(1, 2, 1));
}

TEST_CASE("nothing to search is REFUSED, exit 2, naming the root: success over nothing read is the failure a checker exists against", "[literaturecheck][behaviour]") {
    {
        Tree t(two_entries());
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err == "literaturecheck: nothing to search: no CMakeLists.txt, *.cmake, *.cpp, *.hpp, *.h or *.c file under " + t.root.string() + "\n");
    }
    {
        Tree t(two_entries());
        t.write("modules/m/notes.txt", "alpha-2001-fake\n");
        t.write("tools/x.cpp", "alpha-2001-fake\n");   // not a place that is searched
        CHECK(t.run({"--quiet"}).code == kArgument);
    }
}

TEST_CASE("a build input that cannot be read is REFUSED, exit 2, naming it -- never skipped", "[literaturecheck][behaviour]") {
    if (::geteuid() == 0) SKIP("root opens every file");
    Tree t(two_entries());
    t.write("modules/m/a.cpp", "int a;\n");
    t.write("modules/m/locked.cpp", "int b;\n");
    REQUIRE(::chmod((t.root / "modules/m/locked.cpp").c_str(), 0) == 0);
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kArgument);
    CHECK(r.out.empty());
    CHECK(contains(r.err, "literaturecheck: cannot read " + (t.root / "modules/m/locked.cpp").string() + ": "));
    REQUIRE(::chmod((t.root / "modules/m/locked.cpp").c_str(), 0600) == 0);
}

TEST_CASE("an error the tool did not anticipate is reported as such, exit 70, and not as a pass or a finding", "[literaturecheck][behaviour]") {
    Tree t(two_entries());
    t.write("CMakeLists.txt", "project(x)\n");
    odl::devtools_testing::ThrowingStream out;   // every write to it throws: the counts of a clean run cannot be printed
    std::ostringstream err;
    const int code = lc::run({"--quiet", "--root", t.root.string()}, Streams{out, err});
    CHECK(code == 70);
    CHECK(err.str() == "literaturecheck: internal error: boom\n");
}

// ======================================================================================================================================== the manifest

TEST_CASE("a manifest that cannot be read is refused, exit 2, with a message that names the file", "[literaturecheck][behaviour]") {
    {
        Tree t("");   // no manifest at all
        t.write("CMakeLists.txt", "project(x)\n");
        const Result r = t.run();
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err.rfind("literaturecheck: cannot read " + t.manifest_path().string() + ": ", 0) == 0);
    }
    {
        Tree t("{\"entries\": [");
        t.write("CMakeLists.txt", "project(x)\n");
        const Result r = t.run();
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err == "literaturecheck: " + t.manifest_path().string() + ": Expecting value: line 1 column 14\n");
    }
    {
        Tree t("{\"entries\": []}\n");
        t.write("manifest/manifest.json", "");   // an empty file
        t.write("CMakeLists.txt", "project(x)\n");
        const Result r = t.run();
        CHECK(r.code == kArgument);
        CHECK(r.err == "literaturecheck: " + t.manifest_path().string() + ": Expecting value: line 1 column 1\n");
    }
    {
        Tree t("{\"entries\": []}\n");
        t.write("manifest/manifest.json", "{\"entries\": [], \"note\": \"caf\xE9\"}\n");   // not UTF-8
        t.write("CMakeLists.txt", "project(x)\n");
        const Result r = t.run();
        CHECK(r.code == kArgument);
        CHECK(r.err == "literaturecheck: " + t.manifest_path().string() + " is not well-formed UTF-8\n");
    }
}

TEST_CASE("a manifest of the wrong shape is refused with a message; entries that are not literature are never looked into", "[literaturecheck][behaviour]") {
    const auto refused = [](const std::string& text, const std::string& message) {
        Tree t(text);
        t.write("CMakeLists.txt", "project(x)\n");
        const Result r = t.run();
        INFO(text);
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err == "literaturecheck: " + t.manifest_path().string() + ": " + message + "\n");
    };
    refused("[]\n", "no `entries` array");
    refused("{}\n", "no `entries` array");
    refused(R"({"entries": {"a": 1}})", "no `entries` array");
    refused(R"({"entries": null})", "no `entries` array");
    refused(R"({"entries": [{"kind": "data"}, 7]})", "entry 2 is no object");
    refused(R"({"entries": ["text"]})", "entry 1 is no object");
    refused(R"({"entries": [{"kind": "data"}, {"kind": "data"}, {"kind": "literature", "filename": "f"}]})", "literature entry 3 has no string `id` or `filename`");
    refused(R"({"entries": [{"kind": "literature", "id": "i"}]})", "literature entry 1 has no string `id` or `filename`");
    refused(R"({"entries": [{"kind": "literature", "id": 5, "filename": "f"}]})", "literature entry 1 has no string `id` or `filename`");
    refused(R"({"entries": [{"kind": "literature", "id": "i", "filename": null}]})", "literature entry 1 has no string `id` or `filename`");
    refused(R"({"literature": 5, "entries": []})", "`literature` is no string");
    // entries that are not literature may lack anything: no kind, a kind that is no string, another kind
    Tree t(R"({"entries": [{}, {"kind": 3}, {"kind": "data"}, {"kind": null, "id": 1}, {"kind": "Literature", "id": 1}]})");
    t.write("CMakeLists.txt", "project(x)\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kOk);
    CHECK(r.out == counts(0, 1, 0) + kVerdict);   // `Literature` is not `literature`
}

// ======================================================================================================================================== the command line

TEST_CASE("the command line: --quiet, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[literaturecheck][behaviour]") {
    Tree t(two_entries());
    t.write("CMakeLists.txt", "project(x)\n");
    CHECK(run_tool({"--root=" + t.root.string(), "--quiet"}).out == counts(2, 1, 0) + kVerdict);
    CHECK(run_tool({"--quiet", "--root", t.root.string()}).out == counts(2, 1, 0) + kVerdict);
    CHECK(run_tool({"--root", t.root.string()}).out.rfind(kHeading, 0) == 0);
    const Result help = run_tool({"-h"});
    CHECK(help.code == kOk);
    CHECK(help.out.rfind("usage: literaturecheck [-h] [--quiet] [--root ROOT]\n", 0) == 0);
    CHECK(contains(help.out, "options:\n  -h, --help   show this help and exit\n  --quiet      print the three counts and the verdict, not each entry\n"));
    CHECK(contains(help.out, "exit codes: 0 no build input reaches a literature entry   1 one does   2 an argument error, a manifest that cannot be read, or nothing to search\n"));
    CHECK(run_tool({"--help"}).out == help.out);
    CHECK(help.err.empty());
    CHECK(run_tool({"-h=x"}).code == kArgument);                       // a short option takes no value
    CHECK(run_tool({"--quiet", "--help"}).out == help.out);
    // an option's value is whatever follows it unless that begins with `--` (so a directory called -x is a value)
    const Result dash = run_tool({"--root", "-x"});
    CHECK(dash.code == kArgument);
    CHECK(contains(dash.err, "literaturecheck: cannot read -x/manifest/manifest.json: "));
    const Result missing = run_tool({"--root"});
    CHECK(missing.code == kArgument);
    CHECK(missing.err == "usage: literaturecheck [-h] [--quiet] [--root ROOT]\nliteraturecheck: error: argument --root: expected one argument\n");
    const Result unknown = run_tool({"--bogus"});
    CHECK(unknown.code == kArgument);
    CHECK(unknown.err == "usage: literaturecheck [-h] [--quiet] [--root ROOT]\nliteraturecheck: error: unrecognized arguments: --bogus\n");
    CHECK(run_tool({"--quiet=yes", "--root", t.root.string()}).code == kArgument);
    CHECK(run_tool({"--qui", "--root", t.root.string()}).code == kArgument);   // no abbreviations
    CHECK(run_tool({"--roo", t.root.string()}).code == kArgument);
    CHECK(run_tool({"stray", "--root", t.root.string()}).code == kArgument);
}

// ======================================================================================================================================== the real tree

namespace {

// the build inputs of the real tree, counted by a second walk that shares no code with the tool
std::size_t recount_inputs(const fs::path& root) {
    const auto matches = [](const std::string& name) {
        return name == "CMakeLists.txt" || name.ends_with(".cmake") || name.ends_with(".cpp") || name.ends_with(".hpp") || name.ends_with(".h") || name.ends_with(".c");
    };
    std::size_t n = fs::is_regular_file(root / "CMakeLists.txt") ? 1 : 0;
    for (const char* base : {"modules", "tests", "cmake"}) {
        if (!fs::is_directory(root / base)) continue;
        for (fs::recursive_directory_iterator it(root / base), end; it != end; ++it) {
            if (it->is_directory() && it->path().filename() == "build") {
                it.disable_recursion_pending();
                continue;
            }
            if (it->is_regular_file() && matches(it->path().filename().string())) ++n;
        }
    }
    return n;
}

std::size_t count_matches(const std::string& text, const std::regex& pattern) { return static_cast<std::size_t>(std::distance(std::sregex_iterator(text.begin(), text.end(), pattern), std::sregex_iterator())); }

}  // namespace

TEST_CASE("the real tree: no build input reaches a literature entry, and the counts are the manifest's and the tree's, recounted another way", "[literaturecheck][real_tree]") {
    const fs::path root = lc::default_root();
    const std::string manifest_text = read_text(root / "manifest" / "manifest.json");
    const std::size_t entries = count_matches(manifest_text, std::regex(R"re("kind"\s*:\s*"literature")re"));
    const std::size_t not_established = count_matches(manifest_text, std::regex(R"re("terms"\s*:\s*"NOT ESTABLISHED)re", std::regex::icase));
    REQUIRE(entries > 0);

    const Result quiet = run_tool({"--quiet"});
    INFO(quiet.out << quiet.err);
    CHECK(quiet.code == kOk);
    CHECK(quiet.err.empty());
    CHECK(quiet.out == counts(entries, recount_inputs(root), 0) + kVerdict);

    const Result loud = run_tool({});
    CHECK(loud.code == kOk);
    CHECK(loud.out.rfind(kHeading, 0) == 0);
    CHECK(count_matches(loud.out, std::regex("\n    fetched to  ")) == entries);
    CHECK(count_matches(loud.out, std::regex("\n    terms       NOT established")) == not_established);
    CHECK(count_matches(loud.out, std::regex("\n    terms       found")) == entries - not_established);
    CHECK(loud.out.compare(loud.out.size() - quiet.out.size(), quiet.out.size(), quiet.out) == 0);
}

TEST_CASE("the real manifest's needles fire: a source that names a real entry's id, its file name or the root is reached, one that names none is not", "[literaturecheck][real_tree]") {
    const fs::path root = lc::default_root();
    const std::string manifest_text = read_text(root / "manifest" / "manifest.json");
    const Json doc = Json::parse(manifest_text);
    const Json* all = doc.find("entries");
    REQUIRE(all != nullptr);
    REQUIRE(all->is_array());
    std::string id;
    std::string filename;
    for (const Json& e : all->as_array()) {
        const Json* kind = e.find("kind");
        const Json* e_id = e.find("id");
        const Json* e_filename = e.find("filename");
        if (kind != nullptr && kind->is_string() && kind->as_string() == "literature" && e_id != nullptr && e_id->is_string() && e_filename != nullptr && e_filename->is_string()) {
            id = e_id->as_string();
            filename = e_filename->as_string();
            break;
        }
    }
    REQUIRE_FALSE(id.empty());
    REQUIRE_FALSE(filename.empty());
    const Json* given = doc.find("literature");
    const std::string literature_root = given != nullptr && given->is_string() ? given->as_string() : default_root_text();

    Tree t(manifest_text);
    t.write("modules/m/clean.cpp", "int clean;\n");
    t.write("modules/m/by_id.cpp", "// " + id + "\n");
    t.write("modules/m/by_name.cpp", "// " + filename + "\n");
    t.write("modules/m/by_root.cpp", "// " + literature_root + "/\n");
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kReached);
    CHECK(contains(r.err, "  modules/m/by_id.cpp:1  mentions '" + id + "'\n"));
    CHECK(contains(r.err, "  modules/m/by_name.cpp:1  mentions '" + filename + "'\n"));
    CHECK(contains(r.err, "  modules/m/by_root.cpp:1  mentions '" + literature_root + "'\n"));
    CHECK_FALSE(contains(r.err, "clean.cpp"));
    CHECK(contains(r.out, "  build inputs searched           " + right5(4) + "\n"));

    // the same manifest and a tree that names none of them: clean
    Tree clean(manifest_text);
    clean.write("modules/m/clean.cpp", "int clean;\n");
    const Result rc = clean.run({"--quiet"});
    CHECK(rc.code == kOk);
    CHECK(rc.err.empty());
}
