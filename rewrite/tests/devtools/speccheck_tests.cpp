// tests/devtools/speccheck_tests.cpp — the specification coverage checker's refusals, exercised.  (ctests `speccheck.*`; the port of the three Python tests of tests/ and more.)
//
// What the Python tests asserted is asserted again with the same synthetic input: the duplicate TEST_CASE claim (tag [test_case_dup]), the same identifier defined in two specification
// files ([cross_file]), and a lettered acceptance row ([lettered]).  The cases after them cover what the Python tests never reached: the printed text of a run, each kind of finding,
// every scanner against the semantics of the regular expression it replaces ([scan], expectations derived from the pattern in speccheck.py, NOT from running the port),
// universal newlines, the tree walk's scope (the root's own build*/ and data/ are not read: ruling D12 a), the refusals and their exit codes, and the real tree ([real_tree]).
//
// A note on this file's own text: tools/speccheck scans every .cpp of the tree for `TEST_CASE("<ID> ...`, so an id-shaped name here would be a claim.  The synthetic sources the cases write
// are assembled at run time (`claim(...)`), and none of this file's own TEST_CASE names begins with an identifier.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "speccheck.hpp"

#include <sys/stat.h>

#include <cstdlib>
#include <filesystem>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
namespace sc = odl::tools::speccheck;

namespace {

constexpr int kOk = 0, kGaps = 1, kArgument = 2, kUnparseable = 3;

struct Result {
    int code = -1;
    std::string out;
    std::string err;
    [[nodiscard]] std::string both() const { return out + err; }
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = sc::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// The number as Python's `{n:>4}` printed it.
std::string w4(std::size_t n) {
    const std::string digits = std::to_string(n);
    return std::string(digits.size() < 4 ? 4 - digits.size() : 0, ' ') + digits;
}

// A temporary tree: <root>/ for the C++ sources, <root>/spec/ for the specifications.
struct Tree {
    TempDir td;
    fs::path root;
    fs::path spec;
    Tree() : root(td.path()), spec(root / "spec") { fs::create_directories(spec); }
    void write(const std::string& relative, const std::string& content) {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, content);
    }
    void spec_file(const std::string& name, const std::string& content) { write_text(spec / name, content); }
    // the Python tests' own command lines: --spec-dir and --cpp-root, then what the case adds
    [[nodiscard]] Result run(std::vector<std::string> extra = {}) const {
        std::vector<std::string> args = {"--spec-dir", spec.string(), "--cpp-root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        return run_tool(args);
    }
};

// What a front matter row and nothing else makes: a valid spec with nothing to cover.
const std::string kMinimalSpec = "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n";

// TEST_CASE(...) as a C++ source would carry it, assembled here so that this file's own text holds no claim.
std::string claim(const std::string& id, const std::string& label) { return std::string("TEST_") + "CASE(\"" + id + "  " + label + "\", \"[synthetic]\") { CHECK(true); }\n"; }

// A minimal, otherwise-valid spec: every -R-/-F- id mentioned in `rows` gets a Coverage excuse, so the only thing that can fail the synthetic files is what the case is about.
std::string spec_of(const std::string& prefix, const std::string& rows) {
    std::vector<std::string> ids;
    const std::regex pattern(prefix + "-[RF]-[0-9]+[a-z]?");
    for (std::sregex_iterator it(rows.begin(), rows.end(), pattern), end; it != end; ++it) {
        if (std::find(ids.begin(), ids.end(), it->str()) == ids.end()) ids.push_back(it->str());
    }
    std::string coverage;
    for (const std::string& id : ids) coverage += "| `" + id + "` | synthetic, excused so this test is only about cross-file duplication |\n";
    return "| | |\n|---|---|\n| **Spec ID** | `" + prefix + "` |\n\n## 5. Required behaviour\n\n" + rows + "\n\n## 8. Coverage\n\n**Coverage.**\n\n| id | reason |\n|---|---|\n" + coverage;
}

// A spec of one requirement and one acceptance row whose own id is `acceptance_id`.
std::string spec_with_acceptance_id(const std::string& acceptance_id) {
    return "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n"
           "## 5. Required behaviour\n\n"
           "- **SYN-R-001.** A synthetic requirement.\n\n"
           "## 8. Acceptance tests\n\n"
           "| id | what is checked | expected value | source | tolerance | discharges |\n"
           "|---|---|---|---|---|---|\n"
           "| `" + acceptance_id + "` | checks SYN-R-001 | a value | a source | exact | R-001 |\n\n"
           "**Coverage.**\n\n| id | why no test |\n|---|---|\n";
}

// A spec of three requirements-or-refusals, two acceptance rows and what `extra` adds (a Coverage section, further rows).
std::string spec_three(const std::string& extra) {
    return "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n"
           "- **SYN-R-001.** One.\n"
           "- **SYN-R-002.** Two.\n"
           "- **SYN-F-003.** Refusal.\n\n"
           "| `SYN-A-001` | checks | R-001, F-003 |\n"
           "| `SYN-A-002` | checks | R-002 |\n" + extra;
}

}  // namespace

// ======================================================================================================================================== ported: duplicate TEST_CASE claims

TEST_CASE("duplicate TEST_CASE claims: two different files claiming one id are refused, named, and the corrected form passes", "[speccheck][test_case_dup]") {
    // THE HISTORICAL SHAPE (L5 step 4): two different files, one id, unrelated tests
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("a_tests.cpp", claim("SRPA-A-011", "pure absorber at oblique incidence"));
        t.write("b_tests.cpp", claim("SRPA-A-011", "the general formula against 20 published vectors"));
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.both(), "SRPA-A-011"));                                          // ... and names the id
        CHECK((contains(r.both(), "a_tests.cpp") && contains(r.both(), "b_tests.cpp")));   // ... and names both locations
    }
    // the SAME defect within ONE file
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("one_file_tests.cpp", claim("MCRM-A-099", "first claim") + claim("MCRM-A-099", "second claim"));
        CHECK(t.run({"--quiet"}).code == kGaps);
    }
    // the corrected form must pass
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("a_tests.cpp", claim("SRPA-A-014", "pure absorber at oblique incidence"));
        t.write("b_tests.cpp", claim("SRPA-A-015", "the general formula against 20 published vectors"));
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kOk);
        CHECK(r.err.empty());
    }
    // THE FAILURE MODE THIS TOOL MUST NOT HAVE: a tree with a genuine duplicate must not report success
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("x_tests.cpp", claim("EPH-A-001", "claim one") + claim("EPH-A-001", "claim two"));
        const Result r = t.run({"--quiet"});
        CHECK_FALSE(contains(r.out, "ok       no id is claimed by more than one TEST_CASE"));
        CHECK(contains(r.err, "FAILED   see above"));
    }
    // a legitimate split (a lettered suffix) is NOT a duplicate: a mechanically distinct id
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("y_tests.cpp", claim("EPH-A-001", "the gate") + claim("EPH-A-001b", "the full sweep"));
        CHECK(t.run({"--quiet"}).code == kOk);
    }
}

// ======================================================================================================================================== ported: cross-file duplicate definitions

TEST_CASE("cross-file duplicate definitions: one id defined in two specification files is refused, in either row form, and a distinct id or a lettered split is not", "[speccheck][cross_file]") {
    const auto run_specs = [](const std::map<std::string, std::string>& files) {
        Tree t;
        for (const auto& [name, content] : files) t.spec_file(name, content);
        // --skip-test-case-check: this is about the cross-file SPEC check, and the default --cpp-root would otherwise scan unrelated sources
        return t.run({"--skip-test-case-check", "--quiet"});
    };

    // THE HISTORICAL SHAPE: two different FILES, one PREFIX, one id
    Result r = run_specs({{"SPEC-a.md", spec_of("DUPX", "- **DUPX-R-001.** first file's own row.\n")}, {"SPEC-b.md", spec_of("DUPX", "- **DUPX-R-001.** second file, same id, unrelated text.\n")}});
    CHECK(r.code == kGaps);
    CHECK(contains(r.both(), "DUPX-R-001"));
    CHECK((contains(r.both(), "SPEC-a.md") && contains(r.both(), "SPEC-b.md")));

    // the collision survives a table-row against a bulleted-row form mismatch (defined_ids is the UNION of both kinds of definition)
    r = run_specs({{"SPEC-c.md", spec_of("DUPY", "| `DUPY-R-001` | table-row form |\n")}, {"SPEC-d.md", spec_of("DUPY", "- **DUPY-R-001.** bulleted form, same id.\n")}});
    CHECK(r.code == kGaps);

    // distinct, genuinely unrelated specs must pass: the same NUMBER under two prefixes
    r = run_specs({{"SPEC-e.md", spec_of("ONEX", "- **ONEX-R-001.** first spec's own requirement.\n")}, {"SPEC-f.md", spec_of("TWOX", "- **TWOX-R-001.** second spec's own requirement, same NUMBER.\n")}});
    CHECK(r.code == kOk);

    // a lettered-suffix split across files is NOT a duplicate: the literal id strings differ
    r = run_specs({{"SPEC-g.md", spec_of("DUPZ", "- **DUPZ-R-001.** the original.\n")}, {"SPEC-h.md", spec_of("DUPZ", "- **DUPZ-R-001b.** an amendment, lettered, genuinely distinct.\n")}});
    CHECK(r.code == kOk);

    // THE FAILURE MODE THIS CHECK MUST NOT HAVE: success on the cross-file line over an unresolved duplicate
    r = run_specs({{"SPEC-i.md", spec_of("DUPW", "- **DUPW-R-001.** first file.\n")}, {"SPEC-j.md", spec_of("DUPW", "- **DUPW-R-001.** second file.\n")}});
    CHECK_FALSE(contains(r.out, "ok       no id is defined in more than one specification file"));
}

// ======================================================================================================================================== ported: a lettered acceptance row

TEST_CASE("a lettered acceptance row discharges its requirement, and an undischarged one is still reported UNCOVERED", "[speccheck][lettered]") {
    const auto run_spec = [](const std::string& text, bool quiet) {
        Tree t;
        t.spec_file("SPEC-synthetic.md", text);
        std::vector<std::string> extra = {"--skip-test-case-check"};
        if (quiet) extra.emplace_back("--quiet");
        return t.run(extra);
    };
    // THE HISTORICAL SHAPE (L6 step 1): IOFM-A-004b's own discharge column went unread
    Result r = run_spec(spec_with_acceptance_id("SYN-A-001b"), true);
    CHECK(r.code == kOk);
    // the unlettered case must still work
    r = run_spec(spec_with_acceptance_id("SYN-A-001"), true);
    CHECK(r.code == kOk);
    // THE FAILURE MODE THE FIX MUST NOT HAVE INTRODUCED: a requirement genuinely undischarged (the row names a DIFFERENT one) is still UNCOVERED
    const std::string two =
        "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n"
        "## 5. Required behaviour\n\n"
        "- **SYN-R-001.** A synthetic requirement.\n"
        "- **SYN-R-002.** A second synthetic requirement, never discharged.\n\n"
        "## 8. Acceptance tests\n\n"
        "| id | what is checked | expected value | source | tolerance | discharges |\n"
        "|---|---|---|---|---|---|\n"
        "| `SYN-A-001b` | checks SYN-R-001 | a value | a source | exact | R-001 |\n\n"
        "**Coverage.**\n\n| id | why no test |\n|---|---|\n";
    r = run_spec(two, true);
    CHECK(r.code == kGaps);
    r = run_spec(two, false);
    CHECK(contains(r.both(), "SYN-R-002"));   // ... and is named UNCOVERED, not silently passed
    CHECK(contains(r.out, "    UNCOVERED              " + w4(1) + "   SYN-R-002\n"));
}

// ======================================================================================================================================== what a run prints

TEST_CASE("a clean spec prints the counts exactly as the Python printed them", "[speccheck][behaviour]") {
    // derived from the print statements of speccheck.py: 5 identifiers (3 requirements or refusals, 2 acceptance rows), none foreign; both rows discharge: R-001 and F-003 by the
    // first, R-002 by the second
    Tree t;
    t.spec_file("SPEC-synthetic.md", spec_three(""));
    const Result r = t.run({"--skip-test-case-check"});
    REQUIRE(r.code == kOk);
    CHECK(r.err.empty());
    const std::string expected =
        "\n"
        "SPEC-synthetic.md  [Spec ID: SYN]\n"
        "  identifiers, OWN-PREFIX  " + w4(5) + "   <- the denominator\n"
        "  identifiers, ALL         " + w4(5) + "   (no foreign identifiers referenced)\n"
        "  requirements + refusals  " + w4(3) + "\n"
        "    discharged by a test   " + w4(3) + "\n"
        "    excused in §8 Coverage " + w4(0) + "\n"
        "    UNCOVERED              " + w4(0) + "\n"
        "\n"
        "Cross-file identifier definitions (1 specs read as one namespace)\n"
        "  distinct ids defined      " + w4(5) + "\n"
        "  DUPLICATE definitions     " + w4(0) + "\n"
        "\n"
        "======================================================================\n"
        "1 specifications\n"
        "  requirements and refusals, OWN-PREFIX denominator : 3\n"
        "    discharged by an acceptance ROW                 : 3\n"
        "    excused, with a reason, in §8 Coverage          : 0\n"
        "    uncovered                                       : 0\n"
        "  identifiers referenced, own-prefix / all          : 5 / 5\n"
        "\n"
        "ok       every requirement and refusal is discharged by an acceptance ROW\n"
        "         or individually excused, in every specification.\n"
        "ok       no id is defined in more than one specification file.\n"
        "\n"
        "         WHAT THIS DOES NOT CHECK: that those rows are implemented. This reads\n"
        "         the specifications' own traceability, not the test suite, so a spec\n"
        "         that is written and not yet built passes here — SPEC-ephemerides.md\n"
        "         does today. A green result is 'the spec is internally complete', never\n"
        "         'the module is tested'.\n";
    CHECK(r.out == expected);
}

TEST_CASE("with the tree-wide scan on, the claims block follows the cross-file block and the ok line follows the other ok lines", "[speccheck][behaviour]") {
    Tree t;
    t.spec_file("SPEC-synthetic.md", kMinimalSpec);
    t.write("a_tests.cpp", claim("SRPA-A-014", "one") + claim("SRPA-A-015", "two"));
    const Result r = t.run();
    REQUIRE(r.code == kOk);
    const std::string claims_block = "\nTEST_CASE claims, tree-wide (`--cpp-root " + t.root.string() + "`)\n  distinct ids claimed     " + w4(2) + "\n  DUPLICATE claims         " + w4(0) + "\n";
    const std::size_t at = r.out.find(claims_block);
    REQUIRE(at != std::string::npos);
    CHECK(r.out.find("Cross-file identifier definitions") < at);
    CHECK(r.out.find("\n======================================================================\n") > at);
    CHECK(contains(r.out, "ok       no id is defined in more than one specification file.\nok       no id is claimed by more than one TEST_CASE, tree-wide.\n\n         WHAT THIS DOES NOT CHECK"));
    // and --skip-test-case-check takes both the block and the ok line out, as the Python's did
    const Result skipped = t.run({"--skip-test-case-check"});
    CHECK_FALSE(contains(skipped.out, "TEST_CASE claims"));
    CHECK_FALSE(contains(skipped.out, "no id is claimed by more than one TEST_CASE"));
}

TEST_CASE("--quiet takes each specification's block and the cross-file block out and keeps the totals and the verdict", "[speccheck][behaviour]") {
    Tree t;
    t.spec_file("SPEC-synthetic.md", spec_three(""));
    const Result r = t.run({"--skip-test-case-check", "--quiet"});
    REQUIRE(r.code == kOk);
    CHECK_FALSE(contains(r.out, "[Spec ID:"));
    CHECK_FALSE(contains(r.out, "Cross-file identifier definitions"));
    CHECK(r.out.rfind("\n======================================================================\n1 specifications\n", 0) == 0);
    CHECK(contains(r.out, "ok       every requirement and refusal is discharged"));
}

TEST_CASE("specifications are read in the order of their names, SPEC-template.md and anything else that does not match are not read", "[speccheck][behaviour]") {
    Tree t;
    t.spec_file("SPEC-b.md", kMinimalSpec);
    t.spec_file("SPEC-a.md", kMinimalSpec);
    t.spec_file("SPEC-template.md", "no front matter here: it would be refused if it were read");
    t.spec_file("SPEC-x.txt", "nor this");
    t.spec_file("other.md", "nor this");
    t.spec_file("SPEC_notes.md", "nor this: the name has to begin SPEC-, with the hyphen");
    t.spec_file("SPEC-.md", kMinimalSpec);   // `*` matches nothing: SPEC-*.md matches SPEC-.md, which sorts before SPEC-a.md
    const Result r = t.run({"--skip-test-case-check"});
    REQUIRE(r.code == kOk);
    const std::size_t dash = r.out.find("SPEC-.md  [Spec ID: SYN]");
    const std::size_t a = r.out.find("SPEC-a.md  [Spec ID: SYN]");
    const std::size_t b = r.out.find("SPEC-b.md  [Spec ID: SYN]");
    REQUIRE((dash != std::string::npos && a != std::string::npos && b != std::string::npos));
    CHECK(dash < a);
    CHECK(a < b);
    CHECK(contains(r.out, "\n3 specifications\n"));
}

// ======================================================================================================================================== each kind of finding

TEST_CASE("foreign identifiers are counted apart and never in the denominator", "[speccheck][behaviour]") {
    Tree t;
    // SYNC-R-009's prefix begins with this file's own, SYN, and is not it: "own" is the prefix and its hyphen, not the letters
    t.spec_file("SPEC-synthetic.md", spec_three("\nSee OTHER-R-007 and also ELSE-Q-002 and SYNC-R-009, which belong to other specifications.\n"));
    const Result r = t.run({"--skip-test-case-check"});
    REQUIRE(r.code == kOk);   // a reference is not a finding
    CHECK(contains(r.out, "  identifiers, OWN-PREFIX  " + w4(5) + "   <- the denominator\n"));
    CHECK(contains(r.out, "  identifiers, ALL         " + w4(8) + "   (3 foreign, NOT counted: ELSE-Q-002, OTHER-R-007, SYNC-R-009)\n"));   // sorted
    CHECK(contains(r.out, "  identifiers referenced, own-prefix / all          : 5 / 8\n"));
    CHECK(contains(r.out, "  the difference of 3 is cross-references between specs, which\n"));
    CHECK(contains(r.out, "  denominator; that is the 121-against-128 error, printed rather than made.\n"));
}

TEST_CASE("a gap in any one specification fails the run, whichever place it holds among the others", "[speccheck][behaviour]") {
    const std::string gap = "| | |\n|---|---|\n| **Spec ID** | `AAA` |\n\n- **AAA-R-001.** Never discharged.\n";
    const std::string clean = "| | |\n|---|---|\n| **Spec ID** | `BBB` |\n\n- **BBB-R-001.** Discharged.\n\n| `BBB-A-001` | checks | R-001 |\n";
    for (const bool gap_first : {true, false}) {
        Tree t;
        t.spec_file("SPEC-a.md", gap_first ? gap : clean);
        t.spec_file("SPEC-b.md", gap_first ? clean : gap);
        const Result r = t.run({"--skip-test-case-check"});
        INFO("the gap is in the " << (gap_first ? "first" : "last") << " specification");
        CHECK(r.code == kGaps);
        CHECK(contains(r.out, "uncovered                                       : 1\n"));
        CHECK_FALSE(contains(r.out, "ok       every requirement"));
    }
}

TEST_CASE("a dangling reference, an uncovered requirement and a duplicate definition are each named and each fail the gate", "[speccheck][behaviour]") {
    {   // dangling: an own-prefix identifier mentioned and defined nowhere
        Tree t;
        t.spec_file("SPEC-synthetic.md", spec_three("\nThe note mentions SYN-R-099, which nothing defines.\n"));
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.out, "  DANGLING references      " + w4(1) + "   SYN-R-099\n"));
        CHECK(contains(r.err, "FAILED   see above"));
        CHECK_FALSE(contains(r.out, "ok       every requirement"));
    }
    {   // uncovered: R-002's acceptance row is not there
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-F-003.** Refusal.\n\n| `SYN-A-001` | checks | R-001 |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.out, "    discharged by a test   " + w4(1) + "\n"));
        CHECK(contains(r.out, "    UNCOVERED              " + w4(2) + "   SYN-F-003, SYN-R-002\n"));   // sorted
        CHECK(contains(r.out, "uncovered                                       : 2\n"));
    }
    {   // duplicate: R-001 defined by a bullet and by a table row; the order is the order of first appearance, table rows first
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n| `SYN-R-002` | a table row |\n- **SYN-R-001.** One.\n- **SYN-R-001.** One again.\n- **SYN-R-002.** Two, defined twice.\n\n"
                    "| `SYN-A-001` | checks | R-001, R-002 |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.out, "  DUPLICATE definitions    " + w4(2) + "   SYN-R-002, SYN-R-001\n"));
    }
}

TEST_CASE("§8 Coverage excuses by the FIRST CELL of a row and not by any mention, several identifiers to a cell", "[speccheck][behaviour]") {
    // R-001 is excused by name in its own row, whose second cell mentions R-002 in passing (which must excuse nothing); F-003's row names two identifiers in one cell
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-F-003.** Refusal.\n\n"
                    "**Coverage.**\n\n| id | why no test |\n|---|---|\n| `SYN-R-001` | structural; compare `SYN-R-002` |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.out, "    excused in §8 Coverage " + w4(1) + "\n"));
        CHECK(contains(r.out, "    UNCOVERED              " + w4(2) + "   SYN-F-003, SYN-R-002\n"));
    }
    {
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-F-003.** Refusal.\n\n"
                    "**Coverage.**\n\n| id | why no test |\n|---|---|\n| `SYN-R-001`, `SYN-R-002` | both structural |\n| `SYN-F-003` | structural |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kOk);
        CHECK(contains(r.out, "    excused in §8 Coverage " + w4(3) + "\n"));
    }
    {   // an excuse for a structural item (-S-) is read (the pattern allows RFS) and is not a requirement, so it counts as nothing
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-S-004.** Structural.\n\n| `SYN-A-001` | checks | R-001 |\n\n"
                    "**Coverage.**\n\n| id | why no test |\n|---|---|\n| `SYN-S-004` | structural |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kOk);
        CHECK(contains(r.out, "  requirements + refusals  " + w4(1) + "\n"));
        CHECK(contains(r.out, "    excused in §8 Coverage " + w4(0) + "\n"));
    }
}

TEST_CASE("a partial excuse counts as tested and is printed apart; an excuse for something a test discharges is a stale row, reported and never netted off", "[speccheck][behaviour]") {
    {   // partial, and discharged: counted as tested, named on its own line, passes
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One, three clauses.\n\n| `SYN-A-001` | checks | R-001 |\n\n"
                    "**Coverage.**\n\n| id | why no test |\n|---|---|\n| `SYN-R-001` (partial: third clause only) | structural |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kOk);
        CHECK(contains(r.out, "    discharged by a test   " + w4(1) + "\n    excused in §8 Coverage " + w4(0) + "\n"));
        CHECK(contains(r.out, "    partially excused      " + w4(1) + "   SYN-R-001   <- counted as tested; one clause is structural\n"));
    }
    {   // excused AND discharged: both lists name it, the verdict fails
        Tree t;
        t.spec_file("SPEC-synthetic.md",
                    "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n\n| `SYN-A-001` | checks | R-001 |\n\n"
                    "**Coverage.**\n\n| id | why no test |\n|---|---|\n| `SYN-R-001` | stale: a test does discharge it |\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.out, "    BOTH tested and excused" + w4(1) + "   SYN-R-001   <- the Coverage row is stale; its column says \"why no test\"\n"));
        CHECK(contains(r.out, "    discharged by a test   " + w4(1) + "\n    excused in §8 Coverage " + w4(1) + "\n"));
    }
}

TEST_CASE("a partial marker on a requirement no test discharges is not a partial excuse: the partition check refuses it, with the Python's exit status", "[speccheck][behaviour]") {
    // speccheck.py's `assert`: R-001 is in `uncovered` AND in `contradictory`, so tested + excused - both + uncovered = 0 + 0 - 1 + 1 = 0, not 1, and the Python died with
    // an AssertionError (a traceback, exit 1).  The port keeps the check and the status and writes one line.
    Tree t;
    t.spec_file("SPEC-synthetic.md",
                "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n\n**Coverage.**\n\n| id | why no test |\n|---|---|\n| `SYN-R-001` (partial: nothing tests it) | structural |\n");
    const Result r = t.run({"--skip-test-case-check"});
    CHECK(r.code == kGaps);
    CHECK(contains(r.err, "speccheck: assertion failed: "));
    CHECK(contains(r.err, "0 tested + 0 excused - 1 both + 1 uncovered != 1"));
}

TEST_CASE("a range of identifiers in a discharge column discharges what lies in it and is defined; the endpoints are discharged by name as well", "[speccheck][behaviour]") {
    // R-001, R-002, R-004 are defined, R-003 is not.  Row: R-001…R-004.  The range reaches 001, 002 and 004 (003 is not defined: ignored) and the two endpoints are named too.
    Tree t;
    t.spec_file("SPEC-synthetic.md",
                "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-R-004.** Four.\n- **SYN-R-005.** Five.\n\n"
                "| `SYN-A-001` | a range | R-001…R-004 |\n");
    const Result r = t.run({"--skip-test-case-check"});
    CHECK(r.code == kGaps);   // R-005 is outside the range
    CHECK(contains(r.out, "    discharged by a test   " + w4(3) + "\n"));
    CHECK(contains(r.out, "    UNCOVERED              " + w4(1) + "   SYN-R-005\n"));
    // a descending range is empty (range(3, 2)), and its endpoints are still named
    Tree d;
    d.spec_file("SPEC-synthetic.md", "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-R-003.** Three.\n\n| `SYN-A-001` | a range | R-003…R-001 |\n");
    const Result descending = d.run({"--skip-test-case-check"});
    CHECK(contains(descending.out, "    discharged by a test   " + w4(2) + "\n"));   // R-003 and R-001 by name; R-002 is not in range(3, 2)
    // endpoints written without their zeros are padded to three digits (f"{n:03d}"): R-1…R-3 reaches SYN-R-001, 002 and 003, and the names "R-1" and "R-3" are no identifiers of the file
    Tree padded;
    padded.spec_file("SPEC-synthetic.md", "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-R-003.** Three.\n\n| `SYN-A-001` | a range | R-1…R-3 |\n");
    CHECK(padded.run({"--skip-test-case-check"}).code == kOk);
    // the range's own word boundaries: a word character after the last number (R-003x) or before the first letter (xR-001) and it is not a range; the names inside are read on their own
    {
        const std::string head = "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n- **SYN-R-002.** Two.\n- **SYN-R-003.** Three.\n\n";
        Tree after;
        after.spec_file("SPEC-synthetic.md", head + "| `SYN-A-001` | glued | R-001…R-003x |\n");
        const Result a = after.run({"--skip-test-case-check"});
        CHECK(a.code == kGaps);
        CHECK(contains(a.out, "    UNCOVERED              " + w4(2) + "   SYN-R-002, SYN-R-003\n"));
        Tree before;
        before.spec_file("SPEC-synthetic.md", head + "| `SYN-A-001` | glued | xR-001…R-003 |\n");
        const Result b = before.run({"--skip-test-case-check"});
        CHECK(b.code == kGaps);
        CHECK(contains(b.out, "    UNCOVERED              " + w4(2) + "   SYN-R-001, SYN-R-002\n"));
    }
    // the range keeps its own letter: F-001…F-003 reaches the refusals, and only them
    Tree refusals;
    refusals.spec_file("SPEC-synthetic.md", "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-F-001.** One.\n- **SYN-F-002.** Two.\n- **SYN-F-003.** Three.\n\n| `SYN-A-001` | a range | F-001…F-003 |\n");
    CHECK(refusals.run({"--skip-test-case-check"}).code == kOk);
    // a number of more than 18 digits cannot be expanded: refused, not looped over
    Tree big;
    big.spec_file("SPEC-synthetic.md", "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n- **SYN-R-001.** One.\n\n| `SYN-A-001` | a range | R-001…R-9999999999999999999 |\n");
    const Result refused = big.run({"--skip-test-case-check"});
    CHECK(refused.code == kUnparseable);
    CHECK(contains(refused.err, "REFUSED  SPEC-synthetic.md: a range of identifiers whose numbers have more than 18 digits"));
}

// ======================================================================================================================================== refusals and exit codes

TEST_CASE("refusals: no specification, no Spec ID, a spec that is not UTF-8, and what was printed before the refusal stays printed", "[speccheck][behaviour]") {
    {   // no SPEC-*.md at all (and a directory that is not there)
        Tree t;
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kUnparseable);
        CHECK(r.err == "speccheck: no SPEC-*.md in " + t.spec.string() + "\n");
        CHECK(r.out.empty());
        const Result missing = run_tool({"--spec-dir", (t.root / "absent").string(), "--skip-test-case-check"});
        CHECK(missing.code == kUnparseable);
        CHECK(contains(missing.err, "no SPEC-*.md in "));
    }
    {   // a spec with no declared Spec ID is REFUSED, not guessed at; the one read before it has printed its block
        Tree t;
        t.spec_file("SPEC-a.md", kMinimalSpec);
        t.spec_file("SPEC-b.md", "# a specification without a front matter row\n");
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kUnparseable);
        CHECK(r.err ==
              "REFUSED  SPEC-b.md: no `Spec ID` field in the front matter.\n"
              "  The denominator is own-prefix identifiers, so the prefix must be declared,\n"
              "  not inferred from the filename. Add a front-matter row:\n"
              "      | **Spec ID** | `TIME` |\n");
        CHECK(contains(r.out, "SPEC-a.md  [Spec ID: SYN]"));
        CHECK_FALSE(contains(r.out, "specifications\n"));   // the totals were never reached
    }
    {   // a spec that is not UTF-8 (the Python raised UnicodeDecodeError, a traceback): refused, naming the file
        Tree t;
        t.spec_file("SPEC-bad.md", std::string("| **Spec ID** | `SYN` |\n\xE9\n"));
        const Result r = t.run({"--skip-test-case-check"});
        CHECK(r.code == kUnparseable);
        CHECK(contains(r.err, "REFUSED  SPEC-bad.md: "));
    }
}

TEST_CASE("the command line: usage errors are code 2, -h is not an error, and both spellings of an option's value are read", "[speccheck][behaviour]") {
    CHECK(run_tool({"--bogus"}).code == kArgument);
    Result r = run_tool({"--bogus"});
    CHECK(contains(r.err, "usage: speccheck [-h]"));
    CHECK(contains(r.err, "speccheck: error: unrecognized arguments: --bogus"));
    CHECK(run_tool({"extra"}).code == kArgument);
    CHECK(run_tool({"--spec-dir"}).code == kArgument);
    CHECK(run_tool({"--spec-dir", "--quiet"}).code == kArgument);   // a flag is not a directory
    CHECK(run_tool({"--quiet=yes"}).code == kArgument);             // a flag takes no value
    r = run_tool({"-h"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "usage: speccheck [-h] [--spec-dir SPEC_DIR]"));
    CHECK(contains(r.out, "--skip-test-case-check"));
    CHECK(run_tool({"--help"}).code == kOk);

    Tree t;
    t.spec_file("SPEC-synthetic.md", spec_three(""));
    const Result equals = run_tool({"--spec-dir=" + t.spec.string(), "--cpp-root=" + t.root.string(), "--quiet"});
    CHECK(equals.code == kOk);
    CHECK(equals.out == t.run({"--quiet"}).out);
}

// ======================================================================================================================================== what is scanned, and what is not

TEST_CASE("the tree-wide scan: the root's own build*/ and data/ are not read, nested ones are, links are not followed, and a file that is not UTF-8 is skipped", "[speccheck][behaviour]") {
    const auto duplicated = [](Tree& t, const std::string& where) {
        t.write("a_tests.cpp", claim("SRPA-A-011", "the one"));
        t.write(where, claim("SRPA-A-011", "the other"));
        return t.run({"--quiet"});
    };
    // THE SCOPE FIX (ruling D12 a): a build tree and the data directory of the ROOT are not part of what the tree means
    for (const char* excluded : {"build/x_tests.cpp", "build-ci/_deps/other-src/x_tests.cpp", "build-repro-a/x_tests.cpp", "data/vendored/x_tests.cpp"}) {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        const Result r = duplicated(t, excluded);
        INFO("under " << excluded);
        CHECK(r.code == kOk);
    }
    // ... and only the root's own: the same names below a module are source, and are read
    for (const char* read : {"modules/data/x_tests.cpp", "modules/build/x_tests.cpp", "tests/build-info/x_tests.cpp", "x/data/build/x_tests.cpp"}) {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        const Result r = duplicated(t, read);
        INFO("under " << read);
        CHECK(r.code == kGaps);
        CHECK(contains(r.err, read));
    }
    // a name that merely starts like one of them at the root is not the directory: `builder/` and `database/` are read
    for (const char* read : {"builder/x_tests.cpp", "database/x_tests.cpp"}) {
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        const Result r = duplicated(t, read);
        INFO("under " << read);
        CHECK((r.code == kGaps) == (std::string(read).rfind("build", 0) != 0));   // `builder/` begins with "build": the rule is the prefix, as `build*/` says
    }
    {   // a directory reached through a link is not entered (pathlib's rglob does not follow them)
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("a_tests.cpp", claim("SRPA-A-011", "the one"));
        t.write("elsewhere/x_tests.cpp", claim("SRPA-A-011", "the other"));
        // the target is outside the scanned root: copy the case to a root of its own
        Tree scanned;
        scanned.spec_file("SPEC-synthetic.md", kMinimalSpec);
        scanned.write("a_tests.cpp", claim("SRPA-A-011", "the one"));
        std::error_code ec;
        fs::create_directory_symlink(t.root / "elsewhere", scanned.root / "linked", ec);
        REQUIRE_FALSE(ec);
        CHECK(scanned.run({"--quiet"}).code == kOk);
    }
    {   // a directory named x.cpp is a match that reads as nothing, and so are a link to it, a link to nothing and a FIFO (SKIPPED BY RULE: they are not regular files -- C3 left it to what
        // reading one did, which was an exception on GitHub's ext4 and an error on tmpfs, PROVENANCE section 41.10); a file that is not UTF-8 is skipped; all leave a real duplicate to be found
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        fs::create_directories(t.root / "odd.cpp");
        std::error_code ec;
        fs::create_directory_symlink(t.root / "odd.cpp", t.root / "dir_link.cpp", ec);
        REQUIRE_FALSE(ec);
        fs::create_symlink(t.root / "nowhere", t.root / "dangling.cpp", ec);
        REQUIRE_FALSE(ec);
        REQUIRE(::mkfifo((t.root / "fifo.cpp").c_str(), 0600) == 0);   // opening one would wait for ever
        t.write("latin1.cpp", std::string("// caf\xE9\n") + claim("SRPA-A-011", "hidden by an encoding error: skipped, as the Python skipped it"));
        t.write("a_tests.cpp", claim("SRPA-A-011", "the one"));
        const Result single = t.run({"--quiet"});
        CHECK(single.code == kOk);   // the only readable claim is single
        CHECK_FALSE(contains(single.both(), "internal error"));
        t.write("b_tests.cpp", claim("SRPA-A-011", "the other"));
        CHECK(t.run({"--quiet"}).code == kGaps);
    }
    {   // a link to a regular file is that file, as pathlib read it: the same source under two names claims its id twice
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        t.write("a_tests.cpp", claim("SRPA-A-011", "the one"));
        std::error_code ec;
        fs::create_symlink(t.root / "a_tests.cpp", t.root / "alias_tests.cpp", ec);
        REQUIRE_FALSE(ec);
        const Result r = t.run({"--quiet"});
        CHECK(r.code == kGaps);
        CHECK(contains(r.err, "alias_tests.cpp"));
    }
    {   // a --cpp-root that is not there has nothing to claim
        Tree t;
        t.spec_file("SPEC-synthetic.md", kMinimalSpec);
        const Result r = run_tool({"--spec-dir", t.spec.string(), "--cpp-root", (t.root / "absent").string()});
        CHECK(r.code == kOk);
        CHECK(contains(r.out, "  distinct ids claimed     " + w4(0) + "\n"));
    }
}

TEST_CASE("the duplicate claims are listed in pathlib's order, component by component, with the line each was found on", "[speccheck][behaviour]") {
    Tree t;
    t.spec_file("SPEC-synthetic.md", kMinimalSpec);
    // "a-b/c.cpp" sorts before "a/b.cpp" as a string ('-' < '/'), and after it as pathlib sorts ("a" < "a-b")
    t.write("a-b/c.cpp", "\n\n" + claim("SRPA-A-011", "later in the tree by name"));
    t.write("a/b.cpp", claim("SRPA-A-011", "first"));
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kGaps);
    const std::string first = t.root.string() + "/a/b.cpp:1\n";
    const std::string second = t.root.string() + "/a-b/c.cpp:3\n";
    REQUIRE(contains(r.err, first));
    REQUIRE(contains(r.err, second));
    CHECK(r.err.find(first) < r.err.find(second));
    CHECK(contains(r.err, "  DUPLICATE  SRPA-A-011  claimed by 2 TEST_CASEs:\n"));
    // the root as it was typed is printed the way pathlib prints a Path: no "//", no "/./", no trailing slash
    const Result messy = run_tool({"--spec-dir", t.spec.string(), "--cpp-root", t.root.string() + "//./", "--quiet"});
    CHECK(messy.code == kGaps);
    CHECK(contains(messy.err, first));
    CHECK(contains(messy.err, second));
}

TEST_CASE("line ends: a specification and a source with CRLF or lone CR line ends read as the LF ones do, and a CRLF counts one line", "[speccheck][behaviour]") {
    const std::string lf = spec_three("");
    const auto with_endings = [](std::string text, const std::string& ending) {
        std::string out;
        for (const char c : text) {
            if (c == '\n') out += ending;
            else out += c;
        }
        return out;
    };
    Tree reference;
    reference.spec_file("SPEC-synthetic.md", lf);
    const Result expected = reference.run({"--skip-test-case-check"});
    REQUIRE(expected.code == kOk);
    for (const char* ending : {"\r\n", "\r"}) {
        Tree t;
        t.spec_file("SPEC-synthetic.md", with_endings(lf, ending));
        const Result r = t.run({"--skip-test-case-check"});
        INFO("line end of " << (std::string(ending).size() == 2 ? "CRLF" : "CR"));
        CHECK(r.code == kOk);
        CHECK(r.out == expected.out);
    }
    // the line of a claim, in a source with CRLF line ends: the third line, however it is ended
    Tree t;
    t.spec_file("SPEC-synthetic.md", kMinimalSpec);
    t.write("c_tests.cpp", with_endings("// one\n// two\n" + claim("SRPA-A-011", "x"), "\r\n"));
    t.write("d_tests.cpp", with_endings("\n\n\n\n" + claim("SRPA-A-011", "y"), "\r"));
    const Result r = t.run({"--quiet"});
    CHECK(r.code == kGaps);
    CHECK(contains(r.err, t.root.string() + "/c_tests.cpp:3\n"));
    CHECK(contains(r.err, t.root.string() + "/d_tests.cpp:5\n"));
}

// ======================================================================================================================================== the scanners, against the patterns they replace

TEST_CASE("find_ids: ID_RE's word boundaries, prefix length and lettered suffix", "[speccheck][scan]") {
    const auto ids = [](const std::string& text) {
        std::vector<std::string> out;
        for (const sc::scan::Id& id : sc::scan::find_ids(text)) out.push_back(id.text());
        return out;
    };
    // r"\b([A-Z][A-Z0-9]{1,15})-([RSFAQP])-(\d+[a-z]?)\b":
    //   xTIME-R-002 (no boundary before T), TIME-R-004ab (a second letter spoils the boundary), A-R-005 (a one-character prefix) and a 17-character prefix are not identifiers;
    //   a 16-character one is
    CHECK(ids("TIME-R-001 xTIME-R-002 TIME-R-003a TIME-R-004ab A-R-005 ABCDEFGHIJKLMNOPQ-R-006 ABCDEFGHIJKLMNOP-R-007 (TIME-R-008) TIME-R-009.") ==
          std::vector<std::string>{"TIME-R-001", "TIME-R-003a", "ABCDEFGHIJKLMNOP-R-007", "TIME-R-008", "TIME-R-009"});
    CHECK(ids("_TIME-R-001 TIME_R-001 TIME-X-001 TIME-r-001 time-R-001 TIME-R-") == std::vector<std::string>{});
    // the kind letters are R S F A Q P; a digit may sit in the prefix after its first letter
    CHECK(ids("T1-S-1 T1-F-2 T1-A-3 T1-Q-4 T1-P-5 1T-R-6") == std::vector<std::string>{"T1-S-1", "T1-F-2", "T1-A-3", "T1-Q-4", "T1-P-5"});
    // Unicode-aware boundaries: a non-ASCII letter next to it is a word character (no boundary), a symbol or a dash is not
    CHECK(ids("éTIME-R-001 TIME-R-001é") == std::vector<std::string>{});
    CHECK(ids("§TIME-R-001 —TIME-R-002… TIME-R-003§") == std::vector<std::string>{"TIME-R-001", "TIME-R-002", "TIME-R-003"});
    // `\d` is Unicode's: the Arabic-Indic one-two-three is a number
    CHECK(ids("TIME-R-\xD9\xA1\xD9\xA2\xD9\xA3 ") == std::vector<std::string>{"TIME-R-\xD9\xA1\xD9\xA2\xD9\xA3"});
    // findall does not overlap: after TIME-R-001 the scan resumes at its end
    CHECK(ids("TIME-R-001TIME-R-002") == std::vector<std::string>{});   // no boundary between 001 and T, so the first fails; the second has none before it
    CHECK(ids("TIME-R-001,TIME-R-002") == std::vector<std::string>{"TIME-R-001", "TIME-R-002"});
}

TEST_CASE("spec_id: the front-matter row, anywhere in the text, at the start of a line, its whitespace as Python's \\s", "[speccheck][scan]") {
    using sc::scan::spec_id;
    CHECK(spec_id("| **Spec ID** | `TIME` |\n") == std::optional<std::string>("TIME"));
    CHECK(spec_id("intro\n\n|   **Spec ID**   |   `ATMO`   |   \n") == std::optional<std::string>("ATMO"));
    CHECK(spec_id("text | **Spec ID** | `TIME` |\n") == std::nullopt);          // not at the start of a line
    CHECK(spec_id("| Spec ID | `TIME` |\n") == std::nullopt);                    // not bold
    CHECK(spec_id("| **Spec ID** | `A` |\n") == std::nullopt);                   // one character
    CHECK(spec_id("| **Spec ID** | `ABCDEFGHIJKLMNOPQ` |\n") == std::nullopt);    // seventeen
    CHECK(spec_id("| **Spec ID** | `ABCDEFGHIJKLMNOP` |\n") == std::optional<std::string>("ABCDEFGHIJKLMNOP"));
    CHECK(spec_id("| **Spec ID** | `time` |\n") == std::nullopt);                 // lower case
    CHECK(spec_id("| **Spec ID** | `FIRST` |\n| **Spec ID** | `SECOND` |\n") == std::optional<std::string>("FIRST"));   // search: the first
    // `\s*` takes a newline too: the row may be broken over lines
    CHECK(spec_id("|\n**Spec ID**\n|\n`BROKEN`\n|\n") == std::optional<std::string>("BROKEN"));
    // ... and the blanks of every script Python's `\s` knows: a no-break space, an em space, an ideographic space
    CHECK(spec_id("|\xC2\xA0**Spec ID**\xE2\x80\x83|\xE3\x80\x80`NBSP`\xC2\xA0|\n") == std::optional<std::string>("NBSP"));
    CHECK(spec_id("|\xE2\x80\x8B**Spec ID** | `ZWSP` |\n") == std::nullopt);   // a zero-width space (U+200B) is not whitespace to Python
    CHECK(spec_id("") == std::nullopt);
}

TEST_CASE("split_coverage: the region runs from the marker to the next line that starts with '## ', or to the end", "[speccheck][scan]") {
    using sc::scan::split_coverage;
    auto [body, coverage] = split_coverage("A\n**Coverage.**\nrow\n## Next\nB\n");
    CHECK(body == "A\n## Next\nB\n");
    CHECK(coverage == "**Coverage.**\nrow\n");
    std::tie(body, coverage) = split_coverage("A\n**Coverage.**\nrow\nlast row, no heading after");
    CHECK(body == "A\n");
    CHECK(coverage == "**Coverage.**\nrow\nlast row, no heading after");
    std::tie(body, coverage) = split_coverage("no marker here\n## 8. x\n");
    CHECK(body == "no marker here\n## 8. x\n");
    CHECK(coverage.empty());
    // a '## ' that does not start a line is not a heading; '##' without the blank is not either
    std::tie(body, coverage) = split_coverage("**Coverage.**\nsee ## here\n##notaheading\n## real\n");
    CHECK(coverage == "**Coverage.**\nsee ## here\n##notaheading\n");
    CHECK(body == "## real\n");
    // only the FIRST marker counts
    std::tie(body, coverage) = split_coverage("x\n**Coverage.**\na\n**Coverage.**\nb\n## h\n");
    CHECK(coverage == "**Coverage.**\na\n**Coverage.**\nb\n");
}

TEST_CASE("table_defs and bullet_defs: the definition patterns, lettered ids, own prefix only, and the whitespace that may span lines", "[speccheck][scan]") {
    using sc::scan::bullet_defs;
    using sc::scan::table_defs;
    // r"^\|\s*`(SYN-[RSFAQP]-\d+[a-z]?)`\s*\|"
    CHECK(table_defs("| `SYN-R-001` | text |\n|`SYN-F-002b`|x\n| `SYN-R-003bc` | no |\n| `OTH-R-004` | foreign |\n| `SYN-X-005` | no kind |\n  | `SYN-R-006` | indented |\n| SYN-R-007 | no backticks |\n",
                     "SYN") == std::vector<std::string>{"SYN-R-001", "SYN-F-002b"});
    CHECK(table_defs("| `SYN-R-001`\n", "SYN").empty());                  // no closing pipe
    CHECK(table_defs("| `SYN-R-001 | no closing backtick |\n", "SYN").empty());   // no closing backtick: the pattern needs both
    CHECK(table_defs("| SYN-R-001` | no opening backtick |\n", "SYN").empty());
    // `\s*` takes the newline: the row may be broken
    CHECK(table_defs("|\n  `SYN-R-001`\n|\n", "SYN") == std::vector<std::string>{"SYN-R-001"});
    // r"^\s*-\s+\*\*(SYN-[RSFAQP]-\d+[a-z]?)\.\*\*"
    CHECK(bullet_defs("- **SYN-R-001.** a\n  - **SYN-F-002.** nested\n* **SYN-R-003.** star\n-**SYN-R-004.** no blank\n- **SYN-R-005** no dot\n- **SYN-R-006a.** lettered\n- **OTH-R-007.** foreign\n\t-\t**SYN-S-008.** tabs\n",
                      "SYN") == std::vector<std::string>{"SYN-R-001", "SYN-F-002", "SYN-R-006a", "SYN-S-008"});
    CHECK(bullet_defs("-\n**SYN-R-001.**\n", "SYN") == std::vector<std::string>{"SYN-R-001"});   // `\s+` takes the newline
    CHECK(bullet_defs("text - **SYN-R-001.**\n", "SYN").empty());                                  // the dash begins the line
}

TEST_CASE("acceptance_rows, discharges_of: the row as the regular expression reads it, the identifiers and the ranges of the last column", "[speccheck][scan]") {
    using sc::scan::acceptance_rows;
    using sc::scan::discharges_of;
    const auto rows = acceptance_rows("| `SYN-A-001` | x | R-001 |\n| `SYN-A-002b` | y | R-002 |  \n| `SYN-R-003` | not acceptance |\n| `OTH-A-004` | foreign |\ntext | `SYN-A-005` | not at the start\n", "SYN");
    CHECK(rows == std::vector<std::string>{"| `SYN-A-001` | x | R-001 |", "| `SYN-A-002b` | y | R-002 |  "});
    // r"\b([RF])-(\d+[a-z]?)\b": named; the prefix of an identifier does not matter (the boundary after its hyphen suffices)
    auto d = discharges_of(" R-001, F-002 and TIME-R-003, R-021a ");
    CHECK(d.named == std::vector<std::pair<char, std::string>>{{'R', "001"}, {'F', "002"}, {'R', "003"}, {'R', "021a"}});
    CHECK(d.ranges.empty());
    // not named: no boundary before the letter, a second letter after the number, a lower-case kind, a missing hyphen
    CHECK(discharges_of("xR-001 R-001ab r-001 R001 R-").named.empty());
    // r"\b([RF])-(\d+)…([RF])-(\d+)\b": the range, found alongside the identifiers it is made of
    d = discharges_of("R-001…R-003");
    CHECK(d.named == std::vector<std::pair<char, std::string>>{{'R', "001"}, {'R', "003"}});
    REQUIRE(d.ranges.size() == 1);
    CHECK((d.ranges[0].letter == 'R' && d.ranges[0].first == "001" && d.ranges[0].last == "003"));
    d = discharges_of("F-010…F-012; R-5…R-7");
    REQUIRE(d.ranges.size() == 2);
    CHECK((d.ranges[0].letter == 'F' && d.ranges[0].first == "010" && d.ranges[0].last == "012"));
    CHECK((d.ranges[1].letter == 'R' && d.ranges[1].first == "5" && d.ranges[1].last == "7"));
    CHECK(discharges_of("R-001..R-003 R-001-R-003").ranges.empty());   // only the one-character ellipsis, U+2026
}

TEST_CASE("coverage_cells and cell_ids: the first cell of a row with two pipes, and the backticked own-prefix identifiers in it", "[speccheck][scan]") {
    using sc::scan::cell_ids;
    using sc::scan::coverage_cells;
    CHECK(coverage_cells("| `SYN-R-001`, `SYN-R-002` | a reason |\n|no second pipe\ntext | not at the start |\n| `SYN-R-003` (partial) | b |\n|---|---|\n") ==
          std::vector<std::string>{" `SYN-R-001`, `SYN-R-002` ", " `SYN-R-003` (partial) ", "---"});
    CHECK(coverage_cells("||\n").size() == 1);   // an empty first cell is a cell
    // r"`SYN-([RFS])-(\d+[a-z]?)`": the kinds R, F and S only; backticks required; lettered
    CHECK(cell_ids(" `SYN-R-001`, `SYN-F-002b`, `SYN-S-003`, `SYN-A-004`, `SYN-R-005bc`, SYN-R-006, `OTH-R-007` ", "SYN") ==
          std::vector<std::pair<char, std::string>>{{'R', "001"}, {'F', "002b"}, {'S', "003"}});
}

TEST_CASE("test_case_claims: the macro's own leading identifier, as the regular expression finds it, with its line", "[speccheck][scan]") {
    using sc::scan::test_case_claims;
    const std::string macro = std::string("TEST_") + "CASE";
    const auto found = [&](const std::string& text) {
        std::vector<std::pair<std::string, std::size_t>> out;
        for (const sc::scan::Claim& c : test_case_claims(text)) out.emplace_back(c.id, c.line);
        return out;
    };
    using Found = std::vector<std::pair<std::string, std::size_t>>;
    // r'TEST_CASE\s*\(\s*"([A-Z][A-Z0-9]{1,15}-[RSFAQP]-\d+[a-z]?)'
    CHECK(found(macro + "(\"SRPA-A-011  a label\", \"[x]\")\n" + macro + " (\n  \"EPH-A-001b the sweep\"\n)") == Found{{"SRPA-A-011", 1}, {"EPH-A-001b", 2}});
    CHECK(found("\n\n  " + macro + "(\"MCRM-A-099: third line\")") == Found{{"MCRM-A-099", 3}});
    // not claims: a one-character prefix, a lower-case one, a quote missing, a name that is not an identifier, the id after a space in the quotes
    CHECK(found(macro + "(\"A-R-001\")" + macro + "(\"srpa-A-011\")" + macro + "(SRPA-A-011)" + macro + "(\"plain name\")" + macro + "(\" SRPA-A-011\")").empty());
    // a letter glued to the number is taken (no boundary after the optional letter), and a macro inside a longer name still matches
    CHECK(found(macro + "(\"ABC-A-001x: glued\")") == Found{{"ABC-A-001x", 1}});
    CHECK(found(macro + "(\"ABC-A-003xy: a letter, then another\")") == Found{{"ABC-A-003x", 1}});   // the optional letter needs no boundary after it
    CHECK(found("MY_" + macro + "(\"ABC-A-002 inside a longer macro name\")") == Found{{"ABC-A-002", 1}});
    // a backslash before the quote (the macro written inside a string literal) is not a claim
    CHECK(found("\"" + macro + "(\\\"SRPA-A-011  in a literal\\\")\"").empty());
    CHECK(found("").empty());
}

// ======================================================================================================================================== the real tree

TEST_CASE("the real tree: every specification is traceable, no id is defined twice and no TEST_CASE id is claimed twice", "[speccheck][real_tree]") {
    // what ci.sh's gate 7 runs, as a test: the default arguments, on the tree this test was built from
    const Result r = run_tool({});
    INFO(r.out << r.err);
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(contains(r.out, "ok       every requirement and refusal is discharged by an acceptance ROW\n"));
    CHECK(contains(r.out, "ok       no id is defined in more than one specification file.\n"));
    CHECK(contains(r.out, "ok       no id is claimed by more than one TEST_CASE, tree-wide.\n"));
    CHECK(contains(r.out, "(`--cpp-root " + sc::default_root().string() + "`)"));
}
