#pragma once
// tools/budgetcheck.hpp — the budget-row arithmetic checker's entry point, and the pieces it is made of (plan L0 step 8, group C4).
//
// `run` is the whole tool: `budgetcheck [--spec-dir DIR] [--quiet]`; exit 0 every row checks, 1 a row is wrong, 2 a row would not parse (also an argument error and a specification that
// could not be read), 70 an error the tool did not anticipate.  The tests call it in-process on synthetic specifications; ci.sh's gate 8 and the ctest `budget.arithmetic` run the built tool.
//
// `scan` is the Python tool's regular expressions as hand-written scanners over UTF-8 text, and the rest of this header is its arithmetic: each exposed so that it can be tested against the
// semantics of the code it replaces (the Python's text is in each comment).  `\s` and `\d` are Unicode-aware as Python's are, within the limit odl/devkit/pytext.hpp states.

#include <odl/devkit/tool.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odl::tools::budgetcheck {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--spec-dir` (as `<root>/spec`) defaults to.
std::filesystem::path default_root();

namespace scan {

/// normalise(text): the Unicode the specifications are written in, made parseable.  U+2212 -> '-', U+00D7 -> '*', U+00B5 and U+03BC -> 'u', U+2013 -> '~', U+2014 -> ' ',
/// U+2009, U+00A0 and U+202F -> ' '; an ASCII letter or digit followed by superscript signs and digits (10⁻¹³, s⁻¹) becomes `10^-13`, `s^-1`; a blank between two digits of any
/// script is dropped ("1.495 978 707"); `D * 10^n` (the same sign as a genuine factor) is folded into `De n`, and a `10^n` not after a digit, a point or an `e` into `1en`.
[[nodiscard]] std::string normalise(std::string_view text);

/// EXPR.findall(text): r"([^|=]+?)\s*=\s*\*\*([^*]+)\*\*", as (the left, the bold result) pairs, left to right, non-overlapping.  The left never crosses a `|` or an `=` and
/// excludes the white space before the `=`.
[[nodiscard]] std::vector<std::pair<std::string, std::string>> expressions(std::string_view text);

/// strip_prose_prefix(lhs): PROSE = r"^(?P<prose>[^\d]*?[A-Za-z][^\d]*?)(?P<rest>[-+]?\s*\d.*)$" (re.S) -- prose with no digit in it and an ASCII letter, then the first number with its
/// sign -- as (prose.strip(), rest.strip()); ("", lhs) when it does not match.
[[nodiscard]] std::pair<std::string, std::string> strip_prose_prefix(std::string_view lhs);

/// row_re.match(line).group(1): r"^\|\s*`([A-Z][A-Z0-9]*-P-[0-9a-z]+)`\s*\|".
[[nodiscard]] std::optional<std::string> row_id(std::string_view line);

}  // namespace scan

/// What the Python raised as ParseError, and (in its words, or this tool's where the interpreter's would be version text) as the ValueErrors it caught: a row that looks like
/// arithmetic and will not parse.
struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Dimension as (length, time, angle, mass).
using Dim = std::array<long long, 4>;

struct Quantity {
    double value = 1.0;
    Dim dim{};
};

/// Quantity.__mul__: the values multiply, the dimensions add.
[[nodiscard]] Quantity operator*(const Quantity& a, const Quantity& b);

/// parse_unit: `km s^-1`, `mm/uas`, `m/AU`, `mm`, the empty string.  The first `/` splits numerator from denominator; a token is `symbol` or `symbol^integer`; an unknown symbol is a
/// ParseError.  (A bare `^3` is the dimensionless "" raised to 3, as in the Python.)
[[nodiscard]] Quantity parse_unit(std::string_view u);

/// parse_number: `7.5`, `1.11*10^-16`, `10^-13`, `1e-5`, `+3`, as float() and `10.0 ** int()` read them; blanks are ignored.
[[nodiscard]] double parse_number(std::string_view s);

struct Term {
    std::vector<double> values;   // one, or two for a range `30~100`
    Quantity unit;
};

/// parse_term: `10 uas`, `30~100 uas`, `1.11*10^-16 s`, `0.034 mm/uas`, `10^-13 AU`.  Leading `<`, `≤`, `≈`, `~` and blanks are dropped.
[[nodiscard]] Term parse_term(std::string_view t);

/// significant_tolerance: half a unit of the last SIGNIFICANT digit of the stated value, times 1.001 -- the row is rounded for reading and the check accepts that rounding and nothing looser.
[[nodiscard]] double significant_tolerance(std::string_view text, double value);

struct RowOutcome {
    std::string kind;                  // "checked", "no-arithmetic" or "problem"
    std::vector<std::string> problems;
};

/// check_row: classifies one budget row and writes its `note` and `ok` lines to `out` (not when `quiet`).
[[nodiscard]] RowOutcome check_row(const std::string& rid, std::string_view row, bool quiet, std::ostream& out);

}  // namespace odl::tools::budgetcheck
