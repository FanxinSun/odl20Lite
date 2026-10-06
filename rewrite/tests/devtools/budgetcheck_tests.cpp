// tests/devtools/budgetcheck_tests.cpp — the budget-row checker's arithmetic, its refusals and its printed text, exercised.  (ctests `budget.checker_catches_the_historical_errors`, `budgetcheck.behaviour`,
// `budgetcheck.real_tree`; the port of tests/test_budgetcheck.py and more.)
//
// What the Python test asserted is asserted again with the same synthetic rows: the four wrong rows of 2026-09-18 are caught and say ARITHMETIC WRONG, the corrected forms pass, a row that looks like
// arithmetic and will not parse is REFUSED and never skipped, prose beside the arithmetic is dropped when it holds no digit and refused when it does, and a row with no arithmetic is counted as such
// (tag [historical]).  The cases after them cover what the Python test never reached: the printed text of a run (every expectation DERIVED BY HAND from the print statements and the arithmetic of
// budgetcheck.py, not taken from running the port), every kind of finding and the exit codes, each scanner against the semantics of the regular expression it replaces ([scan]), the unit table,
// the quirks the port keeps, and the real tree ([real_tree]): every real row alone, and every real row with its stated result made wrong.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "budgetcheck.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace bc = odl::tools::budgetcheck;
using namespace odl::devkit;
using Catch::Approx;

namespace {

constexpr int kOk = 0, kWrong = 1, kUnparseable = 2;

struct Result {
    int code = -1;
    std::string out;
    std::string err;
    [[nodiscard]] std::string both() const { return out + err; }
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = bc::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// The header the Python test put over its rows (it matches no row), and a row of five columns whose consequence column carries the expression.
const std::string kHeader = "| id | quantity | budget | consequence | basis |\n|---|---|---|---|---|\n";

std::string row(const std::string& rid, const std::string& consequence) { return "| `" + rid + "` | a quantity | a budget | " + consequence + " | a basis |\n"; }

// A synthetic specification of `rows` in a directory of its own, run with --spec-dir (and --quiet, as the Python test ran it, unless asked not to).
Result run_rows(const std::string& rows, bool quiet = true) {
    TempDir td;
    write_text(td.path() / "SPEC-synthetic.md", kHeader + rows);
    std::vector<std::string> args = {"--spec-dir", td.path().string()};
    if (quiet) args.push_back("--quiet");
    return run_tool(args);
}

const std::string kSuccess = "ok       every budget row's arithmetic evaluates to what it states\n";

std::string summary(std::size_t rows, std::size_t checked, std::size_t none) {
    return "\n" + std::to_string(rows) + " budget rows: " + std::to_string(checked) + " with arithmetic, checked; " + std::to_string(none) + " with none.\n";
}

// "ok       <id padded to 14> <rest>\n" as the Python's f"ok       {rid:<14} ..." printed it
std::string ok_line(const std::string& rid, const std::string& rest) { return "ok       " + pad_right(rid, 14) + " " + rest + "\n"; }

std::string note_line(const std::string& rid, const std::string& repr_of_prose) { return "note     " + pad_right(rid, 14) + " prose prefix ignored: " + repr_of_prose + "\n"; }

const std::string kBlurb =
    "\n%N problem(s). A row that looks like arithmetic and does not parse is a FAILURE, never a skip:\n"
    "  a checker that silently skipped them would report success over the rows it\n"
    "  understood and say nothing about the rest, which is the defect this tool\n"
    "  exists to end rather than to join.\n";

std::string blurb(std::size_t n) {
    std::string text = kBlurb;
    text.replace(text.find("%N"), 2, std::to_string(n));
    return text;
}

}  // namespace

// ======================================================================================================================================== ported: the historical errors

TEST_CASE("the four wrong rows of 2026-09-18, replayed as synthetic specifications, are caught, say so, and the corrected forms pass", "[budgetcheck][historical]") {
    // --- the four real errors of 2026-09-18
    const std::vector<std::pair<std::string, std::string>> historical = {
        {"TIME-P-1 as written: 1 ns given as 7.5 pm", row("TIME-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 pm** at LEO")},
        {"TIME-P-1's achieved figure: 8 × 10⁻¹⁶ m", row("TIME-P-1", "1.11 × 10⁻¹⁶ s × 7.5 km s⁻¹ = **8 × 10⁻¹⁶ m**")},
        {"TIME-P-2 as written: 1 ns given as 7.5 nm", row("TIME-P-2", "1 ns × 7.5 km s⁻¹ = **7.5 nm** at LEO")},
        {"EPH-P-1 as written: 10⁻¹³ AU given as 15 µm", row("EPH-P-1", "10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 µm**")},
    };
    for (const auto& [name, rows] : historical) {
        const Result r = run_rows(rows);
        INFO("CATCHES " << name << "\n" << r.both());
        CHECK(r.code == kWrong);
        CHECK(contains(r.err, "ARITHMETIC WRONG"));   // ... and says so
    }
    // --- the corrected forms must pass
    const std::vector<std::pair<std::string, std::string>> corrected = {
        {"7.5 µm", row("TIME-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 µm** at LEO")},
        {"15 mm", row("EPH-P-1", "10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 mm**")},
        {"a range", row("EOP-P-5", "5–20 µs × 0.51 mm/µs = **2.6–10.2 mm**")},
    };
    for (const auto& [name, rows] : corrected) {
        const Result r = run_rows(rows);
        INFO("accepts the corrected " << name << "\n" << r.both());
        CHECK(r.code == kOk);
    }
}

TEST_CASE("a row that looks like arithmetic and will not parse is REFUSED, not skipped, and success is never reported over it", "[budgetcheck][historical]") {
    const std::vector<std::pair<std::string, std::string>> refused = {
        {"an unknown unit", row("X-P-1", "1 furlong × 7.5 km s⁻¹ = **1 mm**")},
        {"mismatched dimensions", row("X-P-2", "1 ns × 7.5 km s⁻¹ = **3 rad**")},
        {"a factor with no units", row("X-P-3", "30 µas × 0.034 = **1.0 mm**")},
        {"nothing numeric on the left", row("X-P-4", "the model's own realisation = **1 mm**")},
    };
    for (const auto& [name, rows] : refused) {
        const Result r = run_rows(rows);
        INFO("REFUSES " << name << " rather than skipping it\n" << r.both());
        CHECK((r.code == kWrong || r.code == kUnparseable));
        CHECK_FALSE(contains(r.out, "ok       every budget row"));   // ... and does not report success
    }
    // prose beside the arithmetic, which SPEC-template.md section 7 now requires: digit-free prose is dropped and reported; prose containing a digit is NOT dropped, because at that point prose
    // cannot be told from a factor
    const Result prose = run_rows(row("X-P-7", "achieved, over one revolution, 1 ns × 7.5 km s⁻¹ = **7.5 µm**"));
    CHECK(prose.code == kOk);   // accepts digit-free prose before the arithmetic
    const Result digit = run_rows(row("X-P-8", "oscillatory at 90 cycles per revolution: 1 ns × 7.5 km s⁻¹ = **7.5 µm**"));
    CHECK((digit.code == kWrong || digit.code == kUnparseable));   // REFUSES prose containing a digit rather than guessing
    CHECK_FALSE(contains(digit.out, "ok       every budget row"));
}

TEST_CASE("a row with no arithmetic is legitimately not checked, and is counted as such", "[budgetcheck][historical]") {
    const Result r = run_rows(row("X-P-5", "—") + row("X-P-6", "exact"));
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "2 with none"));
    CHECK(r.out == summary(2, 0, 2) + kSuccess);
}

// ======================================================================================================================================== the printed text of a run

TEST_CASE("a run without --quiet prints one line per expression, in the Python's format, then the totals and the verdict", "[budgetcheck][behaviour]") {
    // 1 ns × 7.5 km s⁻¹: 1e-9 s * 7.5 * 1000 m/s = 7.5e-6 m = 7.5 um; the computed figure is printed to six significant digits, the stated one as {rv:g}
    const Result r = run_rows(row("X-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 µm** at LEO"), false);
    CHECK(r.code == kOk);
    CHECK(r.out == ok_line("X-P-1", "1 ns * 7.5 km s^-1 = 7.5 (7.5 computed)") + summary(1, 1, 0) + kSuccess);
    CHECK(r.err.empty());
    // --quiet prints the totals and the verdict only
    const Result q = run_rows(row("X-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 µm** at LEO"));
    CHECK(q.out == summary(1, 1, 0) + kSuccess);
    // the astronomical unit: 1e-13 AU * 1.495978707e11 m/AU = 0.01495978707 m = 14.9598 mm, stated 15 mm (half a unit of the last significant digit is 0.5005)
    const Result au = run_rows(row("EPH-P-1", "10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 mm**"), false);
    CHECK(au.code == kOk);
    CHECK(au.out == ok_line("EPH-P-1", "1e-13 AU * 1.495978707e11 m/AU = 15 (14.9598 computed)") + summary(1, 1, 0) + kSuccess);
    // a range on the left pairs with the range on the right, elementwise, one line each: 5 us * 0.51 mm/us = 2.55 mm (stated 2.6, tolerance 0.05005) and 20 us * 0.51 mm/us = 10.2 mm
    const Result range = run_rows(row("EOP-P-5", "5–20 µs × 0.51 mm/µs = **2.6–10.2 mm**"), false);
    CHECK(range.code == kOk);
    CHECK(range.out == ok_line("EOP-P-5", "5~20 us * 0.51 mm/us = 2.6 (2.55 computed)") + ok_line("EOP-P-5", "5~20 us * 0.51 mm/us = 10.2 (10.2 computed)") + summary(1, 1, 0) + kSuccess);
}

TEST_CASE("the prose before an expression is dropped, reported with its repr, and a row of two expressions prints each in turn", "[budgetcheck][behaviour]") {
    const Result r = run_rows(row("X-P-7", "achieved, over one revolution, 1 ns × 7.5 km s⁻¹ = **7.5 µm**"), false);
    CHECK(r.code == kOk);
    CHECK(r.out == note_line("X-P-7", "'achieved, over one revolution,'") + ok_line("X-P-7", "1 ns * 7.5 km s^-1 = 7.5 (7.5 computed)") + summary(1, 1, 0) + kSuccess);
    // --quiet drops the note as it drops every other line
    CHECK(run_rows(row("X-P-7", "achieved, over one revolution, 1 ns × 7.5 km s⁻¹ = **7.5 µm**")).out == summary(1, 1, 0) + kSuccess);
    // two expressions in one cell: the second one's left side opens with prose ("and"), which is dropped, with the note BEFORE that expression's ok line; one row, counted once
    const Result two = run_rows(row("X-P-3", "1 km = **1000 m** and 2 km = **2000 m**"), false);
    CHECK(two.code == kOk);
    CHECK(two.out == ok_line("X-P-3", "1 km = 1000 (1000 computed)") + note_line("X-P-3", "'and'") + ok_line("X-P-3", "2 km = 2000 (2000 computed)") + summary(1, 1, 0) + kSuccess);
    // what separates two expressions in a cell is dropped from the next one's left side: `;` `,` and blanks (lstrip(";, ")), and not anything else
    const Result separated = run_rows(row("X-P-3", "1 km = **1000 m**; 2 km = **2000 m**, 3 km = **3000 m**"), false);
    CHECK(separated.code == kOk);
    CHECK(separated.out == ok_line("X-P-3", "1 km = 1000 (1000 computed)") + ok_line("X-P-3", "2 km = 2000 (2000 computed)") + ok_line("X-P-3", "3 km = 3000 (3000 computed)") + summary(1, 1, 0) + kSuccess);
    const Result colon = run_rows(row("X-P-3", "1 km = **1000 m**: 2 km = **2000 m**"));   // a colon is not dropped: the left side is then ": 2 km", which is no quantity
    CHECK(colon.code == kUnparseable);
    // the prose's repr is Python's: a quote in it changes the quotes, a non-ASCII character is kept
    const Result quote = run_rows(row("X-P-9", "the model's term, 1 m = **1 m**"), false);
    CHECK(quote.code == kOk);
    CHECK(contains(quote.out, note_line("X-P-9", "\"the model's term,\"")));
}

TEST_CASE("a wrong row is reported on standard error with its components, the exit code is 1, and the totals say that the row is neither", "[budgetcheck][behaviour]") {
    // 1 ns x 7.5 km/s = 7.5e-6 m; stated "7.5 pm" = 7.5 in units of 1e-12 m, computed 7.5e6 of them: out by a factor of 1e6
    const Result r = run_rows(row("TIME-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 pm** at LEO"));
    CHECK(r.code == kWrong);
    CHECK(r.out == "\n1 budget rows: 0 with arithmetic, checked; 0 with none.\n  1 row(s) neither \xE2\x80\x94 see the failures below.\n");
    CHECK(r.err == "\nFAILED   TIME-P-1: ARITHMETIC WRONG\n      stated   7.5 pm\n      computed 7.5e+06 (same units)\n      from     1 ns * 7.5 km s^-1\n      out by a factor of 1e+06\n" + blurb(1));
    // the second historical row: 1.11e-16 s x 7.5 km/s = 8.325e-13 m, stated 8e-16 m: out by 8.325e-13 / 8e-16 = 1040.6 (four digits: 1041).  The texts printed are the NORMALISED ones.
    const Result second = run_rows(row("TIME-P-1", "1.11 × 10⁻¹⁶ s × 7.5 km s⁻¹ = **8 × 10⁻¹⁶ m**"));
    CHECK(second.code == kWrong);
    CHECK(second.err == "\nFAILED   TIME-P-1: ARITHMETIC WRONG\n      stated   8e-16 m\n      computed 8.325e-13 (same units)\n      from     1.11e-16 s * 7.5 km s^-1\n      out by a factor of 1041\n" + blurb(1));
    // six significant digits of the computed figure and four of the factor, whatever the stated one has: 0.333333333 m is out by a factor of 0.333333333
    const Result digits = run_rows(row("X-P-1", "1 m × 0.333333333 = **1 m**"));
    CHECK(digits.code == kWrong);
    CHECK(digits.err == "\nFAILED   X-P-1: ARITHMETIC WRONG\n      stated   1 m\n      computed 0.333333 (same units)\n      from     1 m * 0.333333333\n      out by a factor of 0.3333\n" + blurb(1));
    // the stated result is quoted stripped: blanks inside the stars are the author's, not part of the figure
    const Result padded = run_rows(row("TIME-P-1", "1 ns × 7.5 km s⁻¹ = ** 7.5 pm **"));
    CHECK(padded.code == kWrong);
    CHECK(contains(padded.err, "      stated   7.5 pm\n      computed 7.5e+06 (same units)\n"));
    // a stated zero: its tolerance is 0.5 in the stated units -- the stated-precision rule (half a unit of the last stated digit) read for a stated 0, KEPT by the maintainer's ruling of
    // 2026-10-07 -- so 1 mm is refused and 0.4 mm is accepted, and the Python's message for the refusal is short
    const Result zero = run_rows(row("X-P-1", "1 mm = **0 mm**"));
    CHECK(zero.code == kWrong);
    CHECK(zero.err == "\nFAILED   X-P-1: stated zero\n" + blurb(1));
    const Result zero_ok = run_rows(row("X-P-1", "0.4 mm = **0 mm**"), false);
    CHECK(zero_ok.code == kOk);
    CHECK(zero_ok.out == ok_line("X-P-1", "0.4 mm = 0 (0.4 computed)") + summary(1, 1, 0) + kSuccess);
    // ... and the same 0.5 whatever the digits of the zero: `0.00` is no tighter than `0`, as in the Python
    CHECK(run_rows(row("X-P-1", "0.4 mm = **0.00 mm**")).code == kOk);
    CHECK(run_rows(row("X-P-1", "1 mm = **0.00 mm**")).code == kWrong);
}

TEST_CASE("dimensions that differ, a count of values that differs, and a row that cannot be read are each reported in the Python's words; unparseable outranks wrong", "[budgetcheck][behaviour]") {
    // length against angle
    const Result dims = run_rows(row("X-P-2", "1 ns × 7.5 km s⁻¹ = **3 rad**"));
    CHECK(dims.code == kWrong);
    CHECK(contains(dims.err, "FAILED   X-P-2: dimensions differ \xE2\x80\x94 left (1, 0, 0, 0), right (0, 0, 1, 0)  (1 ns * 7.5 km s^-1 = 3 rad)\n"));
    // a range on one side only
    const Result left_range = run_rows(row("X-P-3", "5–20 µs × 1 m/µs = **5 m**"));
    CHECK(left_range.code == kWrong);
    CHECK(contains(left_range.err, "FAILED   X-P-3: 2 value(s) on the left, 1 on the right\n"));
    const Result right_range = run_rows(row("X-P-3", "5 µs × 1 m/µs = **5–6 m**"));
    CHECK(right_range.code == kWrong);
    CHECK(contains(right_range.err, "FAILED   X-P-3: 1 value(s) on the left, 2 on the right\n"));
    // an unknown unit, and the left side that is no quantity at all (its repr has the quote Python chooses for a string holding an apostrophe)
    const Result unit = run_rows(row("X-P-1", "1 furlong × 7.5 km s⁻¹ = **1 mm**"));
    CHECK(unit.code == kUnparseable);
    CHECK(contains(unit.err, "FAILED   X-P-1: UNPARSEABLE '1 furlong * 7.5 km s^-1' = '1 mm': unknown unit 'furlong' in 'furlong'\n"));
    const Result words = run_rows(row("X-P-4", "the model's own realisation = **1 mm**"));
    CHECK(words.code == kUnparseable);
    CHECK(contains(words.err, "FAILED   X-P-4: UNPARSEABLE \"the model's own realisation\" = '1 mm': cannot read a quantity from \"the model's own realisation\"\n"));
    // a wrong row and an unparseable one in one run: the code is 2 (UNPARSEABLE is in a message), the FAILED lines come in the order the rows do, and neither row is counted
    const Result both = run_rows(row("X-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 pm**") + row("X-P-2", "1 furlong = **1 mm**") + row("X-P-3", "1 m = **1 m**"));
    CHECK(both.code == kUnparseable);
    CHECK(both.out == "\n3 budget rows: 1 with arithmetic, checked; 0 with none.\n  2 row(s) neither \xE2\x80\x94 see the failures below.\n");
    CHECK(both.err.find("FAILED   X-P-1: ARITHMETIC WRONG") < both.err.find("FAILED   X-P-2: UNPARSEABLE"));
    CHECK(contains(both.err, blurb(2)));
    // the left side that is nothing at all (an `=` with only white space before it): the Python's max() of an empty list, here the tool's own words
    const Result empty = run_rows("| `X-P-1` | = **1 mm** |\n");
    CHECK(empty.code == kUnparseable);
    CHECK(contains(empty.err, "FAILED   X-P-1: UNPARSEABLE '' = '1 mm': no quantity on the left\n"));
    // out of double range: `10^400` is folded to `1e400` first, which float() reads as an infinity with no error, so the row is merely wrong (computed inf); a unit power the double cannot hold is
    // where the Python raised OverflowError (a traceback) and this is a finding, as is a stated value that is infinite (the Python's tolerance raised it)
    const Result big = run_rows(row("X-P-1", "10^400 m = **1 m**"));
    CHECK(big.code == kWrong);
    CHECK(contains(big.err, "FAILED   X-P-1: ARITHMETIC WRONG\n      stated   1 m\n      computed inf (same units)\n      from     1e400 m\n      out by a factor of inf\n"));
    const Result huge_unit = run_rows(row("X-P-1", "1 km^200 = **1 m**"));
    CHECK(huge_unit.code == kUnparseable);
    CHECK(contains(huge_unit.err, "a unit raised to 200 is out of range: 'km^200'"));
    const Result zero_unit = run_rows(row("X-P-1", "1 m = **1 fm^30**"));   // fm^30 is 1e-450: it underflows to 0, which the Python divided by (ZeroDivisionError)
    CHECK(zero_unit.code == kUnparseable);
    CHECK(contains(zero_unit.err, "FAILED   X-P-1: UNPARSEABLE '1 m' = '1 fm^30': the stated unit's scale is zero\n"));
    const Result infinite = run_rows(row("X-P-1", "1 m = **1e999 m**"));
    CHECK(infinite.code == kUnparseable);
    CHECK(contains(infinite.err, "the stated value is out of range"));
}

TEST_CASE("the silent misreadings the Python had are REFUSED as UNPARSEABLE, naming the row; a left side that is NaN never passes", "[budgetcheck][behaviour]") {
    // the maintainer's ruling of 2026-10-07 (group C5's first item): each of these was read as something else, or passed
    const auto unparseable = [](const std::string& consequence, const std::string& expected) {
        const Result r = run_rows(row("X-P-1", consequence));
        INFO(consequence << "\n" << r.both());
        CHECK(r.code == kUnparseable);
        CHECK(contains(r.err, "FAILED   X-P-1: UNPARSEABLE " + expected + "\n"));
        CHECK_FALSE(contains(r.out, "ok       every budget row"));
        CHECK(r.out == "\n1 budget rows: 0 with arithmetic, checked; 0 with none.\n  1 row(s) neither \xE2\x80\x94 see the failures below.\n");
    };
    unparseable("10^+3 m = **1000 m**", "'10^+3 m' = '1000 m': a power with no unit before it: '^+3' (a power of ten is written 10^3 or 10^-3)");   // was 10 m
    unparseable("5 m^ = **5 m**", "'5 m^' = '5 m': no exponent after '^' in 'm^'");                                                                // was 5 m
    unparseable("1 m^1_0 = **1 m**", "'1 m^1_0' = '1 m': a digit separator in '1_0' is not read: an exponent is digits only");                    // was 1 m^10
    unparseable("1e999 m * 0 = **1 m**", "'1e999 m * 0' = '1 m': the left side evaluates to NaN (a check never passes on it)");                    // passed, "computed nan"
    unparseable("1 m = **5 m^**", "'1 m' = '5 m^': no exponent after '^' in 'm^'");                                                                // the stated side is read the same way
    // an infinity on the left that is not NaN is plain wrong arithmetic, as before
    const Result inf = run_rows(row("X-P-1", "1e999 m = **1 m**"));
    CHECK(inf.code == kWrong);
    CHECK(contains(inf.err, "computed inf (same units)"));
    // what each of them looks like when it is written as it should be, and is accepted
    const Result fine = run_rows(row("X-P-1", "10^3 m = **1000 m**") + row("X-P-2", "1e+3 m = **1000 m**") + row("X-P-3", "5 m^1 = **5 m**") + row("X-P-4", "1 m^10 = **1 m^10**"), false);
    CHECK(fine.code == kOk);
    CHECK(fine.out == ok_line("X-P-1", "1e3 m = 1000 (1000 computed)") + ok_line("X-P-2", "1e+3 m = 1000 (1000 computed)") + ok_line("X-P-3", "5 m^1 = 5 (5 computed)") +
                          ok_line("X-P-4", "1 m^10 = 1 (1 computed)") + summary(4, 4, 0) + kSuccess);
}

TEST_CASE("every row of a specification counts, specifications are read in name order, SPEC-template.md is not excluded, and what is not a specification is not read", "[budgetcheck][behaviour]") {
    TempDir td;
    write_text(td.path() / "SPEC-b.md", kHeader + row("B-P-1", "1 m = **1 m**"));
    write_text(td.path() / "SPEC-a.md", kHeader + row("A-P-1", "1 km = **1000 m**") + row("A-P-2", "—"));
    write_text(td.path() / "SPEC-template.md", kHeader + row("T-P-1", "1 mm = **1 mm**"));   // the Python does not exclude it
    write_text(td.path() / "SPEC-.md", row("E-P-1", "1 s = **1 s**"));                         // `SPEC-*.md` matches an empty `*`
    write_text(td.path() / "SPEC.md", row("N-P-1", "1 s = **99 s**"));                         // no hyphen
    write_text(td.path() / "SPEC-x.txt", row("N-P-2", "1 s = **99 s**"));
    write_text(td.path() / "spec-x.md", row("N-P-3", "1 s = **99 s**"));
    write_text(td.path() / "XSPEC-x.md", row("N-P-4", "1 s = **99 s**"));
    write_text(td.path() / "SPECIAL.md", row("N-P-5", "1 s = **99 s**"));      // begins with SPEC, but not SPEC-
    const Result r = run_tool({"--spec-dir", td.path().string()});
    CHECK(r.code == kOk);
    // sorted by name: "SPEC-.md" < "SPEC-a.md" < "SPEC-b.md" < "SPEC-template.md"
    CHECK(r.out == ok_line("E-P-1", "1 s = 1 (1 computed)") + ok_line("A-P-1", "1 km = 1000 (1000 computed)") + ok_line("B-P-1", "1 m = 1 (1 computed)") + ok_line("T-P-1", "1 mm = 1 (1 computed)") +
                       summary(5, 4, 1) + kSuccess);
    // only a line that begins with the pipe, the backticked identifier and a pipe is a row
    const Result shapes = run_rows(" | `X-P-1` | 1 m = **99 m** |\n| X-P-2 | 1 m = **99 m** |\n| `x-P-3` | 1 m = **99 m** |\n| `X-P-4` 1 m = **99 m** |\n| `X-P-5`| 5 m = **5 m** |\n");
    CHECK(shapes.code == kOk);
    CHECK(shapes.out == summary(1, 1, 0) + kSuccess);   // only the last: the others are not rows
    // CRLF and a lone CR are line ends (the Python's text-mode read), and so are the others str.splitlines splits on
    const Result crlf = run_rows("| `X-P-1` | 1 m = **1 m** |\r\n| `X-P-2` | 1 m = **1 m** |\r| `X-P-3` | 2 m = **2 m** |\n");
    CHECK(crlf.code == kOk);
    CHECK(crlf.out == summary(3, 3, 0) + kSuccess);
    const Result separator = run_rows("| `X-P-1` | 1 m = **1 m**\xE2\x80\xA8| `X-P-2` | 1 m = **1 m** |\n");   // U+2028 ends a line for splitlines
    CHECK(separator.out == summary(2, 2, 0) + kSuccess);
}

TEST_CASE("a directory that is absent, or holds no specification, is REFUSED (exit 2, named): success over nothing read is the failure this tool exists against; one that was read passes", "[budgetcheck][behaviour]") {
    // the maintainer's ruling of 2026-10-07 (group C5's first item): the Python passed each of these with "0 budget rows ... ok"
    TempDir td;
    const auto refused = [&](const fs::path& dir, bool quiet) {
        std::vector<std::string> args = {"--spec-dir", dir.string()};
        if (quiet) args.push_back("--quiet");
        const Result r = run_tool(args);
        INFO(dir.string() << (quiet ? "  --quiet" : ""));
        CHECK(r.code == kUnparseable);
        CHECK(r.out.empty());
        CHECK(r.err == "budgetcheck: no SPEC-*.md in " + dir.string() + "\n");
    };
    refused(td.path(), false);                // an empty directory
    refused(td.path(), true);
    refused(td.path() / "absent", false);     // a directory that is not there
    refused(td.path() / "absent", true);
    write_text(td.path() / "a file", "x");
    refused(td.path() / "a file", false);     // a file where a directory was meant
    write_text(td.path() / "SPEC.md", row("N-P-1", "1 s = **99 s**"));   // names that are no specification: nothing read is still nothing read
    write_text(td.path() / "SPEC-x.txt", row("N-P-2", "1 s = **99 s**"));
    write_text(td.path() / "spec-x.md", row("N-P-3", "1 s = **99 s**"));
    refused(td.path(), false);
    // ... and a directory whose specifications hold no budget row WAS read, so it passes with its "0 budget rows"
    write_text(td.path() / "SPEC-prose.md", "No budget rows here.\n\n| id | what |\n|---|---|\n| `X-R-1` | a requirement |\n");
    const Result read = run_tool({"--spec-dir", td.path().string()});
    CHECK(read.code == kOk);
    CHECK(read.out == summary(0, 0, 0) + kSuccess);
    CHECK(read.err.empty());
    CHECK(run_tool({"--quiet"}).code == kOk);   // the default directory is the tree's own, which holds specifications: the run is not refused
}

TEST_CASE("a specification that cannot be read is refused by name, exit 2, whatever it is that cannot be read", "[budgetcheck][behaviour]") {
    {
        TempDir td;
        write_text(td.path() / "SPEC-latin1.md", "| `X-P-1` | caf\xE9 |\n");   // not UTF-8: the Python raised UnicodeDecodeError, a traceback
        const Result r = run_tool({"--spec-dir", td.path().string(), "--quiet"});
        CHECK(r.code == kUnparseable);
        CHECK(contains(r.err, "REFUSED  SPEC-latin1.md: "));
        CHECK(contains(r.err, "is not well-formed UTF-8"));
        CHECK(r.out.empty());
    }
    {   // a directory called SPEC-x.md matches the glob and is no file (the Python: IsADirectoryError)
        TempDir td;
        fs::create_directories(td.path() / "SPEC-dir.md");
        const Result r = run_tool({"--spec-dir", td.path().string(), "--quiet"});
        CHECK(r.code == kUnparseable);
        CHECK(contains(r.err, "REFUSED  SPEC-dir.md: "));
        CHECK(contains(r.err, "it is not a regular file"));
    }
}

TEST_CASE("the command line: --spec-dir in both forms, --quiet, -h, and the arguments argparse refused", "[budgetcheck][behaviour]") {
    TempDir td;
    write_text(td.path() / "SPEC-a.md", row("A-P-1", "1 m = **1 m**"));
    const std::string both = ok_line("A-P-1", "1 m = 1 (1 computed)") + summary(1, 1, 0) + kSuccess;
    CHECK(run_tool({"--spec-dir", td.path().string()}).out == both);
    CHECK(run_tool({"--spec-dir=" + td.path().string()}).out == both);
    CHECK(run_tool({"--quiet", "--spec-dir", td.path().string()}).out == summary(1, 1, 0) + kSuccess);
    const Result help = run_tool({"-h"});
    CHECK(help.code == kOk);
    CHECK(help.out.rfind("usage: budgetcheck [-h] [--spec-dir SPEC_DIR] [--quiet]\n", 0) == 0);
    CHECK(run_tool({"--help"}).out == help.out);
    const Result missing = run_tool({"--spec-dir"});
    CHECK(missing.code == kUnparseable);   // argparse exits 2
    CHECK(contains(missing.err, "budgetcheck: error: argument --spec-dir: expected one argument\n"));
    const Result flag_after = run_tool({"--spec-dir", "--quiet"});   // a value that looks like an option is none
    CHECK(flag_after.code == kUnparseable);
    CHECK(contains(flag_after.err, "expected one argument"));
    const Result unknown = run_tool({"--bogus"});
    CHECK(unknown.code == kUnparseable);
    CHECK(contains(unknown.err, "usage: budgetcheck [-h] [--spec-dir SPEC_DIR] [--quiet]\nbudgetcheck: error: unrecognized arguments: --bogus\n"));
    CHECK(run_tool({"--quiet=1"}).code == kUnparseable);   // a flag takes no value
    CHECK(run_tool({"--qu"}).code == kUnparseable);        // no abbreviations
    CHECK(run_tool({"stray"}).code == kUnparseable);
}

// ======================================================================================================================================== the arithmetic

TEST_CASE("the unit table is the Python's: every symbol, its scale and its dimension", "[budgetcheck][behaviour]") {
    const double arcsec = 3.141592653589793 / (180.0 * 3600.0);   // math.pi / (180.0 * 3600.0)
    using D = bc::Dim;
    const D L{1, 0, 0, 0}, T{0, 1, 0, 0}, A{0, 0, 1, 0}, M{0, 0, 0, 1}, N0{0, 0, 0, 0};
    struct Entry {
        const char* symbol;
        double scale;
        D dim;
    };
    const std::vector<Entry> table = {
        {"m", 1.0, L},      {"km", 1e3, L},     {"mm", 1e-3, L},        {"um", 1e-6, L},        {"nm", 1e-9, L},         {"pm", 1e-12, L},   {"fm", 1e-15, L},
        {"AU", 1.495978707e11, L},
        {"s", 1.0, T},      {"ms", 1e-3, T},    {"us", 1e-6, T},        {"ns", 1e-9, T},        {"ps", 1e-12, T},        {"fs", 1e-15, T},
        {"rad", 1.0, A},    {"as", arcsec, A},  {"mas", arcsec * 1e-3, A}, {"uas", arcsec * 1e-6, A},
        {"", 1.0, N0},      {"1", 1.0, N0},
        {"kg", 1.0, M},     {"g", 1e-3, M},     {"N", 1.0, D{1, -2, 0, 1}}, {"J", 1.0, D{2, -2, 0, 1}}, {"W", 1.0, D{2, -3, 0, 1}},
    };
    for (const Entry& e : table) {
        const bc::Quantity q = bc::parse_unit(e.symbol);
        INFO("unit '" << e.symbol << "'");
        CHECK(q.value == e.scale);   // pow(x, 1.0) is x
        CHECK(q.dim == e.dim);
    }
    // the first '/' splits; a power divides; the symbols multiply
    const bc::Quantity speed = bc::parse_unit("km s^-1");
    CHECK(speed.value == 1000.0);
    CHECK(speed.dim == D{1, -1, 0, 0});
    const bc::Quantity density = bc::parse_unit("kg m^-3");
    CHECK(density.dim == D{-3, 0, 0, 1});
    const bc::Quantity per_second = bc::parse_unit("m/s");
    CHECK(per_second.dim == D{1, -1, 0, 0});
    CHECK(bc::parse_unit("mm/uas").dim == D{1, 0, -1, 0});
    CHECK(bc::parse_unit("mm/uas").value == Approx(1e-3 / (arcsec * 1e-6)).epsilon(1e-12));
    CHECK(bc::parse_unit("km^2").value == 1e6);
    CHECK(bc::parse_unit("  m  ").dim == L);   // the string is stripped
    CHECK(bc::parse_unit("").dim == N0);
    CHECK(bc::parse_unit("   ").value == 1.0);
}

TEST_CASE("parse_unit refuses what it cannot read in the Python's words, in the Python's order, and keeps its quirks", "[budgetcheck][behaviour]") {
    const auto message = [](const std::string& unit) -> std::string {
        try {
            (void)bc::parse_unit(unit);
        } catch (const bc::ParseError& exc) {
            return exc.what();
        }
        return "no error";
    };
    CHECK(message("furlong") == "unknown unit 'furlong' in 'furlong'");
    CHECK(message("m furlong") == "unknown unit 'furlong' in 'm furlong'");
    CHECK(message("m/s/s") == "unknown unit 's/s' in 'm/s/s'");   // only the FIRST slash splits
    CHECK(message("M") == "unknown unit 'M' in 'M'");             // case matters
    CHECK(message("m^x") == "invalid literal for int() with base 10: 'x'");
    CHECK(message("foo^x") == "invalid literal for int() with base 10: 'x'");   // int() before the lookup
    // a digit separator in an exponent: int() read it (`m^1_0` was m^10); REFUSED by the maintainer's ruling of 2026-10-07, wherever the underscore stands
    CHECK(message("m^1_0") == "a digit separator in '1_0' is not read: an exponent is digits only");
    CHECK(message("m^1__0") == "a digit separator in '1__0' is not read: an exponent is digits only");
    CHECK(message("m^_1") == "a digit separator in '_1' is not read: an exponent is digits only");
    CHECK(message("m^1_") == "a digit separator in '1_' is not read: an exponent is digits only");
    CHECK(message("m^-1_0") == "a digit separator in '-1_0' is not read: an exponent is digits only");
    CHECK(message("m^-") == "invalid literal for int() with base 10: '-'");
    CHECK(message("m^2000000") == "exponent out of range: '2000000'");
    CHECK(message("km^200") == "a unit raised to 200 is out of range: 'km^200'");
    CHECK(message("m^1000000") == "no error");                                       // the limit is a million: it is allowed, one more is not
    CHECK(message("m^-1000000") == "no error");
    CHECK(message("  furlong ") == "unknown unit 'furlong' in 'furlong'");              // the unit text is stripped before it is read, and quoted stripped
    CHECK(message("m^1000001") == "exponent out of range: '1000001'");
    CHECK(message("m^-1000001") == "exponent out of range: '-1000001'");
    CHECK(bc::parse_unit("m^1000000").dim == bc::Dim{1000000, 0, 0, 0});
    CHECK(message("m") == "no error");
    // REFUSED by the maintainer's ruling of 2026-10-07, each a silent misreading in the Python: `m^` was read as `m`, and a power with no unit before it as the dimensionless "" raised to it
    // (so `10^+3` was 10)
    CHECK(message("m^") == "no exponent after '^' in 'm^'");
    CHECK(message("km s^") == "no exponent after '^' in 's^'");
    CHECK(message("m/s^") == "no exponent after '^' in 's^'");
    CHECK(message("^3") == "a power with no unit before it: '^3' (a power of ten is written 10^3 or 10^-3)");
    CHECK(message("^+3") == "a power with no unit before it: '^+3' (a power of ten is written 10^3 or 10^-3)");
    CHECK(message("^-13 AU") == "a power with no unit before it: '^-13' (a power of ten is written 10^3 or 10^-3)");
    CHECK(message("m ^2") == "a power with no unit before it: '^2' (a power of ten is written 10^3 or 10^-3)");   // a blank before the caret makes it a token of its own
    CHECK(message("^") == "a power with no unit before it: '^' (a power of ten is written 10^3 or 10^-3)");
    // what stays: the empty unit, an explicit plus sign, and the digits of any script, are read as before
    CHECK(bc::parse_unit("").dim == bc::Dim{0, 0, 0, 0});
    CHECK(bc::parse_unit("m^+2").dim == bc::Dim{2, 0, 0, 0});
    CHECK(bc::parse_unit("m^\xD9\xA2").dim == bc::Dim{2, 0, 0, 0});   // ARABIC-INDIC DIGIT TWO: int() reads every script's digits
    CHECK(bc::parse_unit("m^0").dim == bc::Dim{0, 0, 0, 0});
    CHECK(bc::parse_unit("m^-2").dim == bc::Dim{-2, 0, 0, 0});
    CHECK(bc::parse_unit("s^-1/m^-1").dim == bc::Dim{1, -1, 0, 0});   // the denominator's powers are negated: s^-1 over m^-1 is m s^-1
}

TEST_CASE("parse_number reads what float() and 10.0 ** int() read, and says what it cannot", "[budgetcheck][behaviour]") {
    CHECK(bc::parse_number("7.5") == 7.5);
    CHECK(bc::parse_number("+3") == 3.0);
    CHECK(bc::parse_number("-3.25") == -3.25);
    CHECK(bc::parse_number("1e-5") == 1e-5);
    CHECK(bc::parse_number("1E+3") == 1000.0);
    CHECK(bc::parse_number(" 1 000 ") == 1000.0);   // strip() and replace(" ", "")
    CHECK(bc::parse_number("1.11*10^-16") == Approx(1.11e-16).epsilon(1e-14));
    CHECK(bc::parse_number("1.11 * 10^-16") == Approx(1.11e-16).epsilon(1e-14));
    CHECK(bc::parse_number("-2*10^3") == Approx(-2000.0).epsilon(1e-14));
    CHECK(bc::parse_number("10^-13") == Approx(1e-13).epsilon(1e-14));
    CHECK(bc::parse_number("10^3") == 1000.0);
    CHECK(bc::parse_number("10^0") == 1.0);
    CHECK(bc::parse_number("\xD9\xA3.\xD9\xA5") == 3.5);   // ARABIC-INDIC 3.5: float() reads the digits of every script
    CHECK(bc::parse_number("007") == 7.0);
    CHECK(bc::parse_number(".5") == 0.5);       // float() reads a number with no digit before the point, and one with none after it
    CHECK(bc::parse_number("5.") == 5.0);
    CHECK(bc::parse_number("+.5e1") == 5.0);
    CHECK(bc::parse_number("-1.5E-2") == -0.015);
    CHECK(std::isinf(bc::parse_number("1e999")));   // float() is inf there, and no error: the finding is made where the value is USED
    CHECK(bc::parse_number("1e-999") == 0.0);
    const auto message = [](const std::string& text) -> std::string {
        try {
            (void)bc::parse_number(text);
        } catch (const bc::ParseError& exc) {
            return exc.what();
        }
        return "no error";
    };
    CHECK(message("abc") == "could not convert string to float: 'abc'");
    CHECK(message("") == "could not convert string to float: ''");
    CHECK(message("1.2.3") == "could not convert string to float: '1.2.3'");
    CHECK(message("1e") == "could not convert string to float: '1e'");
    CHECK(message("10^400") == "10^400 is out of range");   // the Python: OverflowError
    CHECK(message("10^3x") == "could not convert string to float: '10^3x'");
    CHECK(message("2*1034") == "could not convert string to float: '2*1034'");     // `*10` is not `*10^`
    CHECK(message("2*10^3x") == "could not convert string to float: '2*10^3x'");   // the whole text has to be the number
    CHECK(bc::parse_number("\t5\n") == 5.0);                                       // strip(): any white space around it
    CHECK(bc::parse_number("\xC2\xA0" "5") == 5.0);
    CHECK(message("1*10^2000000") == "exponent out of range: '2000000'");
}

TEST_CASE("parse_term reads a quantity: a number, an optional range, and the unit that follows", "[budgetcheck][behaviour]") {
    using D = bc::Dim;
    {
        const bc::Term t = bc::parse_term("10 uas");
        REQUIRE(t.values.size() == 1);
        CHECK(t.values[0] == 10.0);
        CHECK(t.unit.dim == D{0, 0, 1, 0});
    }
    {
        const bc::Term t = bc::parse_term("30~100 uas");
        REQUIRE(t.values.size() == 2);
        CHECK(t.values[0] == 30.0);
        CHECK(t.values[1] == 100.0);
    }
    {
        const bc::Term t = bc::parse_term("1.11*10^-16 s");
        REQUIRE(t.values.size() == 1);
        CHECK(t.values[0] == Approx(1.11e-16).epsilon(1e-14));
        CHECK(t.unit.dim == D{0, 1, 0, 0});
    }
    {
        const bc::Term t = bc::parse_term("0.034 mm/uas");
        CHECK(t.values == std::vector<double>{0.034});
        CHECK(t.unit.dim == D{1, 0, -1, 0});
    }
    {
        const bc::Term t = bc::parse_term("1e-13 AU");
        CHECK(t.values == std::vector<double>{1e-13});
        CHECK(t.unit.dim == D{1, 0, 0, 0});
    }
    // the leading signs of an inequality are dropped, in any mix, with blanks
    CHECK(bc::parse_term("< 5 mm").values == std::vector<double>{5.0});
    CHECK(bc::parse_term("\xE2\x89\xA4 5 mm").values == std::vector<double>{5.0});   // ≤
    CHECK(bc::parse_term("\xE2\x89\x88\xE2\x89\xA4~ < 5 mm").values == std::vector<double>{5.0});   // ≈≤~ <
    CHECK(bc::parse_term("  \t 5mm  ").unit.dim == D{1, 0, 0, 0});   // no blank is needed between the number and the unit
    CHECK(bc::parse_term("5").unit.dim == D{0, 0, 0, 0});
    CHECK(bc::parse_term("-5 mm").values == std::vector<double>{-5.0});   // the minus is the number's, not stripped
    CHECK(bc::parse_term("+5 mm").values == std::vector<double>{5.0});
    CHECK(bc::parse_term("1e3 m").values == std::vector<double>{1000.0});   // the e form of the exponent, with either e, with or without a sign
    CHECK(bc::parse_term("1E3 m").values == std::vector<double>{1000.0});
    CHECK(bc::parse_term("1e+3 m").values == std::vector<double>{1000.0});
    CHECK(bc::parse_term("1E-13 AU").values == std::vector<double>{1e-13});
    CHECK(bc::parse_term("1.5e2 m").values == std::vector<double>{150.0});
    CHECK(bc::parse_term("5 \xC2\xA0mm").unit.dim == D{1, 0, 0, 0});     // NBSP is white space to \s
    const auto message = [](const std::string& text) -> std::string {
        try {
            (void)bc::parse_term(text);
        } catch (const bc::ParseError& exc) {
            return exc.what();
        }
        return "no error";
    };
    CHECK(message("the model's own realisation") == "cannot read a quantity from \"the model's own realisation\"");
    CHECK(message("") == "cannot read a quantity from ''");
    CHECK(message("mm 5") == "cannot read a quantity from 'mm 5'");
    CHECK(message(".5 mm") == "cannot read a quantity from '.5 mm'");   // NUM wants a digit first
    CHECK(message("2 * 10^ m") == "unknown unit '*' in '* 10^ m'");    // a star form with no digits in its exponent is no exponent: the number is 2 and the unit begins at the star
    // a power of ten that normalise has not folded (it folds `10^3` and `10^-3` into `1e3` and `1e-3`, not `10^+3`) is no longer read as the number 10 with a "unit" `^+3`: it is refused
    CHECK(message("10^-13 AU") == "a power with no unit before it: '^-13' (a power of ten is written 10^3 or 10^-3)");
    CHECK(message("10^+3 m") == "a power with no unit before it: '^+3' (a power of ten is written 10^3 or 10^-3)");
    CHECK(message("1e m") == "unknown unit 'e' in 'e m'");              // an e with no digit after it is no exponent: it begins the unit
    CHECK(message("1e+ m") == "unknown unit 'e+' in 'e+ m'");
    CHECK(message("5 ~ 6 mm") == "unknown unit '~' in '~ 6 mm'");      // the range's tilde has to touch both numbers; here the unit text starts with it
    CHECK(message("5~ mm") == "unknown unit '~' in '~ mm'");
    CHECK(message("5 mm\nx") == "cannot read a quantity from '5 mm\\nx'");   // `(.*)$`: `.` does not take a newline, and `$` is the end: a newline inside the unit text ends the match
    CHECK(message("5 mm\n") == "no error");                                    // the trailing newline is stripped before the pattern sees the text
    CHECK(message("5 \n\n mm") == "no error");                                // white space, newlines included, is skipped (\s*) before the unit
}

TEST_CASE("the tolerance is half a unit of the last SIGNIFICANT digit of the stated value, times 1.001", "[budgetcheck][behaviour]") {
    const auto tol = [](const char* text, double value) { return bc::significant_tolerance(text, value); };
    CHECK(tol("7.5 um", 7.5) == Approx(0.05005).epsilon(1e-12));        // digits "75": 2 significant, exponent 0: 0.5 * 10^(0-1) * 1.001
    CHECK(tol("15 mm", 15.0) == Approx(0.5005).epsilon(1e-12));         // 2 significant, exponent 1
    CHECK(tol("8e-16 m", 8e-16) == Approx(5.005e-17).epsilon(1e-12));   // 1 significant ("8"), exponent -16
    CHECK(tol("100 m", 100.0) == Approx(0.5005).epsilon(1e-12));        // trailing zeros count: 3 significant, exponent 2
    CHECK(tol("0.034 mm/uas", 0.034) == Approx(5.005e-4).epsilon(1e-12));   // leading zeros do not: "34", 2 significant, exponent -2
    CHECK(tol("0.50 m", 0.5) == Approx(0.5 * 1e-2 * 1.001).epsilon(1e-12));   // "050" -> "50": 2 significant, exponent -1 (log10(0.5) = -0.30 floors to -1): 0.5 * 10^(-1-1) * 1.001
    CHECK(tol("2.6~10.2 mm", 10.2) == Approx(0.5005).epsilon(1e-12));   // the FIRST number of the text gives the digits, the value gives the exponent: "26" is 2 significant, 10.2 has exponent 1
    CHECK(tol("2.6~10.2 mm", 2.6) == Approx(0.05005).epsilon(1e-12));
    CHECK(tol("1 m", 0.0) == 0.5);                // a stated zero: 0.5, in the stated units
    CHECK(tol("0 m", 0.0) == 0.5);
    CHECK(tol("0.00 m", 0.0) == 0.5);             // the same 0.5 however many digits the zero is written with
    CHECK(tol("zero", 5.0) == Approx(0.5005).epsilon(1e-12));   // no digit in the text: the mantissa is "1", one significant digit, exponent 0
    CHECK(tol("0", 5.0) == Approx(0.5005).epsilon(1e-12));      // "0" -> lstrip -> "" -> "0": length 1
    CHECK(tol("1 2", 10.0) == Approx(0.5005).epsilon(1e-12));   // spaces are removed first: "12"
    CHECK(tol("\xD9\xA7.\xD9\xA5 m", 7.5) == Approx(0.05005).epsilon(1e-12));   // 7.5 in Arabic-Indic digits: two code points of digits
    CHECK_THROWS_AS(bc::significant_tolerance("1e999", std::numeric_limits<double>::infinity()), bc::ParseError);
    CHECK_THROWS_AS(bc::significant_tolerance("1", std::numeric_limits<double>::infinity()), bc::ParseError);
    CHECK(bc::significant_tolerance("5", -5.0) == Approx(0.5005).epsilon(1e-12));   // the sign of the value does not matter
}

// ======================================================================================================================================== the scanners

TEST_CASE("normalise: the Unicode the specifications are written in, made parseable", "[budgetcheck][behaviour][scan]") {
    const auto n = [](const std::string& text) { return bc::scan::normalise(text); };
    CHECK(n("1 ns × 7.5 km s⁻¹ = **7.5 µm** at LEO") == "1 ns * 7.5 km s^-1 = **7.5 um** at LEO");
    CHECK(n("10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 µm**") == "1e-13 AU * 1.495978707e11 m/AU = **15 um**");
    // the characters: U+2212 -> '-', U+00D7 -> '*', both micro signs -> 'u', en dash -> '~' (a range), em dash -> ' ' (prose), the three narrow spaces -> ' '
    CHECK(n("\xE2\x88\x92" "5") == "-5");
    CHECK(n("2\xC3\x97" "3") == "2*3");
    CHECK(n("5 \xC2\xB5s, 5 \xCE\xBCs") == "5 us, 5 us");
    CHECK(n("5\xE2\x80\x93" "20") == "5~20");
    CHECK(n("a \xE2\x80\x94 b") == "a   b");
    CHECK(n("a\xC2\xA0" "b\xE2\x80\x89" "c\xE2\x80\xAF" "d") == "a b c d");
    // ... and the blank between digits that those make: digit grouping
    CHECK(n("1\xC2\xA0" "000") == "1000");
    CHECK(n("1\xE2\x80\x89" "000") == "1000");
    CHECK(n("1\xE2\x80\xAF" "000") == "1000");
    CHECK(n("1 2 3") == "123");
    CHECK(n("1  2") == "1  2");   // two blanks are not "a blank"
    CHECK(n("1\t2") == "1\t2");   // nor is a tab, nor any other white space: only the ASCII blank is grouped away
    CHECK(n("1\xE2\x80\x83" "2") == "1\xE2\x80\x83" "2");   // EM SPACE
    CHECK(n("1 a") == "1 a");
    CHECK(n("a 1") == "a 1");
    CHECK(n("\xD9\xA1 \xD9\xA2") == "\xD9\xA1\xD9\xA2");   // `\d` is every script's digits
    // superscripts: an ASCII letter or digit, then a run of superscript signs and digits, becomes ^ and the run in ASCII
    CHECK(n("m\xC2\xB2 s\xE2\x81\xBB\xC2\xB9 x\xE2\x81\xBB\xC2\xB9\xC2\xB2") == "m^2 s^-1 x^-12");
    CHECK(n("\xC2\xB2") == "\xC2\xB2");                       // nothing before it: left as it is
    CHECK(n(" \xE2\x81\xBB\xC2\xB3") == " \xE2\x81\xBB\xC2\xB3");
    CHECK(n("10\xE2\x81\xB0") == "1e0");                      // SUPERSCRIPT ZERO: 10^0, then 1e0
    CHECK(n("\xC3\xA9\xC2\xB2") == "\xC3\xA9\xC2\xB2");       // a non-ASCII letter does not carry a superscript
    // the star form of scientific notation is folded BEFORE the product is split on '*'
    CHECK(n("2 * 10^3") == "2e3");
    CHECK(n("2.5*10^-3") == "2.5e-3");
    CHECK(n("2 *\t10^3") == "2e3");
    CHECK(n("2 * 10^") == "2 * 10^");
    CHECK(n("2 * 10^-") == "2 * 10^-");
    CHECK(n("1.2.3 * 10^4") == "1.2.3e4");           // the match starts at "2.3"
    CHECK(n("a 3 * 2 * 10^3") == "a 3 * 2e3");       // only the factor next to the 10^
    CHECK(n("2 \xE2\x80\x8B* 10^3") == "2 \xE2\x80\x8B* 1e3");   // a zero-width space is no white space for \s, so the star form is not folded; the 10^3 after the blank is, by the other rule
    // a bare power of ten, not after a digit, a point or an e
    CHECK(n("10^3") == "1e3");
    CHECK(n("x10^3") == "x1e3");
    CHECK(n("a 10^-2") == "a 1e-2");
    CHECK(n("10^3, 10^4") == "1e3, 1e4");
    CHECK(n("10^3 10^4") == "1e310^4");               // the blank between the 3 and the 1 is DIGIT GROUPING, which runs first; then the digits of the first power are greedy
    CHECK(n("210^3") == "210^3");
    CHECK(n("e10^3") == "e10^3");
    CHECK(n("E10^3") == "E10^3");
    CHECK(n(".10^3") == ".10^3");
    CHECK(n("10^310^4") == "1e310^4");                // the digits are greedy
    CHECK(n("10^") == "10^");
    CHECK(n("") == "");
    CHECK(n("no change") == "no change");
}

TEST_CASE("expressions: EXPR.findall, the left side, the bold result, and where it cannot match", "[budgetcheck][behaviour][scan]") {
    using Pairs = std::vector<std::pair<std::string, std::string>>;
    const auto e = [](const std::string& text) { return bc::scan::expressions(text); };
    CHECK(e("| a | 1 m = **1 m** | b |") == Pairs{{" 1 m", "1 m"}});
    CHECK(e("x = **y**") == Pairs{{"x", "y"}});
    CHECK(e(" x  = **y**") == Pairs{{" x", "y"}});               // white space before the `=` is not in the left side, white space before the left side is
    CHECK(e("x=**y**") == Pairs{{"x", "y"}});
    CHECK(e("x =    **y z**") == Pairs{{"x", "y z"}});
    CHECK(e("=**y**").empty());                                  // the left side needs a character that is neither | nor =
    CHECK(e("| = **y** |") == Pairs{{" ", "y"}});                // ... and a lone blank is one (the shortest left side that leaves only white space before the `=`)
    CHECK(e("a = b = **c**") == Pairs{{" b", "c"}});             // the first `=` is not followed by `**`; the second is, and the left side starts after the first
    CHECK(e("a = **b** c = **d**") == Pairs{{"a", "b"}, {" c", "d"}});
    CHECK(e("a | b = **c**") == Pairs{{" b", "c"}});             // a left side never crosses a `|`
    CHECK(e("a = **b*c**").empty());                             // `[^*]+` stops at the first star, and `**` has to follow it
    CHECK(e("a = ****").empty());                                // at least one character between the stars
    CHECK(e("a = **b**").size() == 1);
    CHECK(e("a = **b***") == Pairs{{"a", "b"}});
    CHECK(e("a = **b* *").empty());
    CHECK(e("a = *b**").empty());
    CHECK(e("a = *bc**").empty());                               // two stars open the result, not one
    CHECK(e("a = **b").empty());
    CHECK(e("a = ** b **") == Pairs{{"a", " b "}});              // blanks inside the stars are part of the result
    CHECK(e("a = **b | c**") == Pairs{{"a", "b | c"}});          // ... and so are a `|` and an `=`
    CHECK(e("a = **b = c**") == Pairs{{"a", "b = c"}});
    CHECK(e("no expression here").empty());
    CHECK(e("").empty());
    CHECK(e("a \xC2\xB5 = **b**") == Pairs{{"a \xC2\xB5", "b"}});   // the left side may hold any character that is neither | nor =
    CHECK(e("a\xE2\x80\x83= **b**") == Pairs{{"a", "b"}});          // EM SPACE before the `=` is white space to `\s`
    CHECK(e("a =\xE2\x80\x83**b**") == Pairs{{"a", "b"}});
    CHECK(e("a\n= **b**") == Pairs{{"a", "b"}});                    // `[^|=]` takes a newline, `\s` is one
    CHECK(e("a\xC2\xA0\x80 = **b**") == Pairs{{"a\xC2\xA0\x80", "b"}});   // NBSP then a stray continuation byte: not well-formed, so not white space, and the group keeps it
}

TEST_CASE("strip_prose_prefix: prose with no digit and a letter, then the first number with its sign", "[budgetcheck][behaviour][scan]") {
    using P = std::pair<std::string, std::string>;
    const auto s = [](const std::string& text) { return bc::scan::strip_prose_prefix(text); };
    CHECK(s("foo 5") == P{"foo", "5"});
    CHECK(s("achieved, over one revolution, 1 ns * 7.5 km") == P{"achieved, over one revolution,", "1 ns * 7.5 km"});
    CHECK(s("a - 5") == P{"a", "- 5"});                        // the sign belongs to the number
    CHECK(s("x-5") == P{"x", "-5"});
    CHECK(s("x+-5") == P{"x+", "-5"});                         // only ONE sign: the other stays in the prose
    CHECK(s("x +5") == P{"x", "+5"});
    CHECK(s("x  \t 5") == P{"x", "5"});
    CHECK(s("5 km") == P(std::string(), "5 km"));              // no prose: the digit is first
    CHECK(s("no digits at all") == P(std::string(), "no digits at all"));
    CHECK(s("") == P(std::string(), ""));
    CHECK(s("   ") == P(std::string(), "   "));
    CHECK(s("\xC2\xB5 5") == P(std::string(), "\xC2\xB5 5"));   // a micro sign is no ASCII letter: no prose
    CHECK(s("- 5 mm") == P(std::string(), "- 5 mm"));
    CHECK(s("( 5 )") == P(std::string(), "( 5 )"));            // punctuation is not a letter either
    CHECK(s("a(5") == P{"a(", "5"});
    CHECK(s("ab\xD9\xA1\xD9\xA2") == P{"ab", "\xD9\xA1\xD9\xA2"});   // the first digit of ANY script
    CHECK(s("a\n5") == P{"a", "5"});
    CHECK(s("the 90 cycles") == P{"the", "90 cycles"});
    CHECK(s("one 5 two 6") == P{"one", "5 two 6"});             // only the first digit splits
}

TEST_CASE("row_id: the pipe, the backticked identifier of three parts, and the pipe after it", "[budgetcheck][behaviour][scan]") {
    const auto id = [](const std::string& line) { return bc::scan::row_id(line); };
    CHECK(id("| `X-P-1` | a |") == std::optional<std::string>("X-P-1"));
    CHECK(id("|`X-P-1`|") == std::optional<std::string>("X-P-1"));
    CHECK(id("|  `AB12-P-3a`  | b") == std::optional<std::string>("AB12-P-3a"));
    CHECK(id("|\xE2\x80\x83`X-P-1` |") == std::optional<std::string>("X-P-1"));   // EM SPACE is `\s`
    CHECK(id("| `TYAW-P-12` | a |") == std::optional<std::string>("TYAW-P-12"));
    CHECK(id("| `X-P-1`|") == std::optional<std::string>("X-P-1"));
    CHECK_FALSE(id(" | `X-P-1` |"));       // the pipe is the first character
    CHECK_FALSE(id("| X-P-1 |"));          // no backticks
    CHECK_FALSE(id("| `x-P-1` |"));        // the first character is an upper-case letter
    CHECK_FALSE(id("| `1X-P-1` |"));
    CHECK_FALSE(id("| `X-p-1` |"));        // the letter P is upper-case
    CHECK_FALSE(id("| `X-R-1` |"));        // a requirement is not a budget row
    CHECK_FALSE(id("| `X-P-` |"));         // a tail of one or more
    CHECK_FALSE(id("| `X-P-1A` |"));       // lower-case letters and digits only
    CHECK_FALSE(id("| `X-P-1` x"));        // a pipe after the closing backtick
    CHECK_FALSE(id("| `X-P-1"));
    CHECK_FALSE(id("| `X-P-1 | a |"));    // no closing backtick, though a pipe follows
    CHECK_FALSE(id("| `A-P-P-1` |"));
    CHECK_FALSE(id("| ``-P-1 |"));
    CHECK_FALSE(id("|"));
    CHECK_FALSE(id(""));
}

// ======================================================================================================================================== the real tree

namespace {

// Every budget row of the tree's specifications, in file and line order.
std::vector<std::string> real_rows() {
    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::directory_iterator(bc::default_root() / "spec")) {
        const std::string name = entry.path().filename().string();
        if (name.size() >= 8 && name.rfind("SPEC-", 0) == 0 && name.compare(name.size() - 3, 3, ".md") == 0) files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    std::vector<std::string> rows;
    for (const fs::path& file : files) {
        for (const std::string& line : splitlines_py(universal_newlines(read_text(file)))) {
            if (bc::scan::row_id(line)) rows.push_back(line);
        }
    }
    return rows;
}

// The row with the digit 9 put in front of the first digit of its first bold result (`= **7.5 µm**` -> `= **97.5 µm**`); empty if there is no such place.
std::string with_wrong_result(const std::string& line) {
    for (std::size_t at = line.find('='); at != std::string::npos; at = line.find('=', at + 1)) {
        std::size_t i = at + 1;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (line.compare(i, 2, "**") != 0) continue;
        for (std::size_t j = i + 2; j < line.size() && line[j] != '*'; ++j) {
            if (line[j] >= '0' && line[j] <= '9') return line.substr(0, j) + "9" + line.substr(j);
        }
        return std::string();
    }
    return std::string();
}

}  // namespace

TEST_CASE("the real tree: the checker passes the specifications, quietly, and every number it counts is the sum of the two classes", "[budgetcheck][real_tree]") {
    const Result r = run_tool({"--quiet"});
    INFO(r.both());
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(r.out.size() > kSuccess.size());
    CHECK(r.out.compare(r.out.size() - kSuccess.size(), kSuccess.size(), kSuccess) == 0);
    // "<n> budget rows: <a> with arithmetic, checked; <b> with none." and n = a + b: no row is neither
    const std::string first = r.out.substr(1, r.out.find('\n', 1) - 1);
    std::size_t n = 0, a = 0, b = 0;
    char tail[8] = {};
    REQUIRE(std::sscanf(first.c_str(), "%zu budget rows: %zu with arithmetic, checked; %zu with none.%1s", &n, &a, &b, tail) == 3);
    CHECK(n == a + b);
    CHECK(a > 0);
    CHECK(n == real_rows().size());
}

TEST_CASE("the real tree, row by row: each row alone passes, and each row with its stated result made wrong FAILS (so nothing is skipped or loosened)", "[budgetcheck][real_tree]") {
    const std::vector<std::string> rows = real_rows();
    REQUIRE(!rows.empty());
    std::size_t with_arithmetic = 0, with_none = 0;
    for (const std::string& line : rows) {
        const std::string rid = *bc::scan::row_id(line);
        // C2, the harness's own control: the row alone is accepted, and the tool says whether it has arithmetic
        const Result alone = run_rows(line + "\n");
        INFO("row " << rid << ": " << line << "\n" << alone.both());
        REQUIRE(alone.code == kOk);
        const bool has_arithmetic = contains(alone.out, "1 budget rows: 1 with arithmetic, checked; 0 with none.");
        const bool has_none = contains(alone.out, "1 budget rows: 0 with arithmetic, checked; 1 with none.");
        REQUIRE((has_arithmetic != has_none));
        if (has_none) {
            ++with_none;
            continue;
        }
        ++with_arithmetic;
        // C1, the negative control: the same row with its first bold result made wrong is NOT accepted, and is reported as wrong arithmetic, not as unparseable
        const std::string wrong = with_wrong_result(line);
        REQUIRE(!wrong.empty());
        const Result broken = run_rows(wrong + "\n");
        INFO("made wrong: " << wrong << "\n" << broken.both());
        CHECK(broken.code == kWrong);
        CHECK(contains(broken.err, rid + ": ARITHMETIC WRONG"));
        CHECK_FALSE(contains(broken.out, "ok       every budget row"));
    }
    CHECK(with_arithmetic + with_none == rows.size());
    CHECK(with_arithmetic > 0);
}
