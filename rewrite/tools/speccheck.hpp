#pragma once
// tools/speccheck.hpp — the specification coverage checker's entry point, and the scanners it is made of (plan L0 step 8, group C3).
//
// `run` is the whole tool: `speccheck [--spec-dir DIR] [--cpp-root DIR] [--skip-test-case-check] [--quiet]`, exit 0 complete, 1 gaps found, 2 an argument
// error, 3 a specification that could not be parsed.  The tests call it in-process on synthetic trees; ci.sh's gate 7 runs the built tool.
//
// The `scan` namespace is the Python tool's regular expressions as hand-written scanners over UTF-8 text, exposed so that each can be tested against the
// semantics of the pattern it replaces (the Python's text is in each comment).  They take text whose line ends are "\n" already (devkit's
// universal_newlines: what a text-mode read in Python returns); `\s`, `\w`, `\d` and `\b` are Unicode-aware as Python's are, within the limit pytext.hpp states.

#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odl::tools::speccheck {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--spec-dir` (as `<root>/spec`) and `--cpp-root` default to.
std::filesystem::path default_root();

/// The invariant a specification's printed counts are held to: tested + excused - both + uncovered == requirements, where `both` is how many are tested AND excused (so
/// counted in each of the first two) and `uncovered` how many are neither.  The Python asserted it; it cannot fail by construction, and is kept as an internal guard, a
/// function of its own so that a test can show it fires on the numbers the Python's `assert` crashed on (a "(partial" row for an undischarged requirement, counted twice).
[[nodiscard]] bool partitions(std::size_t tested, std::size_t excused, std::size_t both, std::size_t uncovered, std::size_t requirements) noexcept;

namespace scan {

/// One identifier as ID_RE found it: PREFIX-KIND-NUMBER, the number being digits and at most one lower-case letter.
struct Id {
    std::string prefix;
    char kind = 'R';
    std::string number;
    [[nodiscard]] std::string text() const { return prefix + "-" + kind + "-" + number; }
};

/// ID_RE.findall(text): r"\b([A-Z][A-Z0-9]{1,15})-([RSFAQP])-(\d+[a-z]?)\b".
[[nodiscard]] std::vector<Id> find_ids(std::string_view text);

/// SPEC_ID_RE.search(text).group(1): r"^\|\s*\*\*Spec ID\*\*\s*\|\s*`([A-Z][A-Z0-9]{1,15})`\s*\|" (re.M).  The first match in the text.
[[nodiscard]] std::optional<std::string> spec_id(std::string_view text);

/// split_coverage(text): the text without its Coverage region (the first "**Coverage.**" to the next line that starts "## ", or the end), and that region.
[[nodiscard]] std::pair<std::string, std::string> split_coverage(std::string_view text);

/// table_defs: r"^\|\s*`(PREFIX-[RSFAQP]-\d+[a-z]?)`\s*\|" (re.M), the identifiers in order.
[[nodiscard]] std::vector<std::string> table_defs(std::string_view text, std::string_view prefix);

/// bullet_defs: r"^\s*-\s+\*\*(PREFIX-[RSFAQP]-\d+[a-z]?)\.\*\*" (re.M), the identifiers in order.
[[nodiscard]] std::vector<std::string> bullet_defs(std::string_view text, std::string_view prefix);

/// The acceptance rows: re.findall(r"^\|\s*`PREFIX-A-\d+[a-z]?`.*$", body, re.M), each match as text (from its `|` to the end of its line).
[[nodiscard]] std::vector<std::string> acceptance_rows(std::string_view body, std::string_view prefix);

/// What a row discharges through its last column: the identifiers `\b([RF])-(\d+[a-z]?)\b` names and the ranges `\b([RF])-(\d+)…([RF])-(\d+)\b` (the
/// ellipsis is U+2026) spell, as (letter, number) pairs for the first and ranges as (letter, first, last) with the digits as text.
struct Discharge {
    std::vector<std::pair<char, std::string>> named;
    struct Range {
        char letter = 'R';
        std::string first;
        std::string last;
    };
    std::vector<Range> ranges;
};
[[nodiscard]] Discharge discharges_of(std::string_view column);

/// The first cell of each line of the Coverage region that has two pipes: re.findall(r"^\|(.*?)\|", coverage, re.M).
[[nodiscard]] std::vector<std::string> coverage_cells(std::string_view coverage);

/// The identifiers a coverage cell names between backticks: re.findall(r"`PREFIX-([RFS])-(\d+[a-z]?)`", cell), as (letter, number).
[[nodiscard]] std::vector<std::pair<char, std::string>> cell_ids(std::string_view cell, std::string_view prefix);

/// A `TEST_CASE("<ID> ...` of a C++ source: r'TEST_CASE\s*\(\s*"([A-Z][A-Z0-9]{1,15}-[RSFAQP]-\d+[a-z]?)', with the line its `TEST_CASE` starts on.
struct Claim {
    std::string id;
    std::size_t line = 0;
};
[[nodiscard]] std::vector<Claim> test_case_claims(std::string_view text);

}  // namespace scan

}  // namespace odl::tools::speccheck
