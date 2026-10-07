// unitcheck.cpp — the register of every factor of a thousand in production code.
//
// Plan L0 step 6's checker, ported to C++ in plan L0 step 8 (group C5, the user's directive of 2026-10-06) from tools/unitcheck.py.
//
// SPEC-dynamics DYN-R-040, DYN-A-010.  Plan section 3.4 step 1 asks for a gate that fails on a km<->m scaling written outside `core/units.hpp`.
//
// THE GATE AS THE PLAN WORDED IT WOULD FIRE FOURTEEN TIMES AND CATCH NOTHING, and that was measured before this tool was written rather than after it went green.  Enumerating every 1e3-family
// literal in production sources found FIFTEEN, in seven files, of which exactly ONE is the crossing -- and it is the definition in `core/units.hpp` that the gate must permit.  The other fourteen
// are g/cm^3 -> kg/m^3, a YYDDD radix, two "no longitude" sentinels, eight milliarcsecond and millisecond conversions in `eop`, a mas -> rad constant in `gravity`, and a row-count threshold in
// `tides`.  Fourteen false positives to one true positive, before L4 adds a dozen forces with their own constants.
//
// SO THIS IS A REGISTER, NOT A SEARCH.  Every such literal is either in `core/units.hpp` or carries a marker on its own line saying what it is.  The gate prints the whole register and fails on one
// that carries neither.  WHY NOT NARROW THE SEARCH INSTEAD: this tree has watched that approach fail three times (a licence DENYLIST that passed CeCILL; the secular-pole guard, narrowed twice and
// then replaced).  A SEARCH CONVERGES ON FLAGGING NOTHING; A REGISTER CONVERGES ON ACCOUNTING FOR EVERYTHING.  Adding an entry is a deliberate act with a reason attached.
//
// TWO MARKERS, NOT ONE:
//
//     // UNIT-CROSSING: <from> -> <to>          a genuine unit conversion
//     // NOT-A-UNIT-CROSSING: <what it is>      a factor of 1000 that converts nothing
//
// The marker goes ON THE LINE, or on the line IMMEDIATELY above it -- one line of lookback and no more: a window of several lines would let an annotation drift onto a literal it was never
// written for, which is the attribution failure this register exists to avoid.  LITERALS ARE MATCHED BY VALUE, NOT BY SPELLING: `1e3`, `1.0E+3`, `1000.`, `0.001` and `1.0e-03` are the same
// number differently written, and a spelling list is a denylist wearing a different hat.  AN ANNOTATION IS NOT A PERMIT: a UNIT-CROSSING that names kilometres outside core/units.hpp is refused
// however honestly it is labelled -- if kilometres are involved it IS the crossing, and the crossing has one site.
//
//   unitcheck [--quiet] [--root DIR]
//   exit 0 every factor of a thousand is accounted for   1 one is not, or one names kilometres outside core/units.hpp   2 an argument error or nothing to search   70 an error the tool did not anticipate
//
// THE PROOF.  The tool's product is what it PRINTS; with --quiet (ci.sh's gate 12) that is the counts and the verdict, and the committed record of what unitcheck.py printed is gate 12's section of every
// green ci run: ten of them, on ten different trees, kept with the L0-8 report (C5_baselines/): 164 production sources and 30 literals (1 in core/units.hpp, 15 UNIT-CROSSING, 14 NOT-A-UNIT-CROSSING,
// none unaccounted, none misplaced) on every one.  The port, run on an export of each of those ten commits, printed each section byte for byte, against a substitution list registered before the
// comparison and EMPTY; and an independent PCRE-and-awk count of the literals, made before any C++ existed, found the same 30 in the same 15 files.  The non-quiet REGISTER (every literal with its
// file, line, spelling and annotation) and the text of a failure have no committed record: the Python was not run beside the port, and those parts stand on tests derived by hand from its statements,
// on the controls and on rule 5.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this tool was built from.
//   * The Python tested `"build" not in p.parts` and `"tests" not in p.parts` on the ABSOLUTE path: a tree under a directory called `build` or `tests` would have excluded every source and passed.  The
//     path RELATIVE to the root is tested here.  No recorded result moves.
//   * A run with NO production source is REFUSED, exit 2 (the Python passed "0 sources"): success over nothing read is the failure a checker exists against (the maintainer's ruling of 2026-10-07 on
//     budgetcheck, applied here by analogy and flagged for ratification).
//   * A glob match that is not a regular file (a directory called x.cpp, a link to nothing, a FIFO) is skipped by rule; the Python's read_text raised on it.  A link to a directory is not entered.  Files
//     are read as the Python read them, errors="replace".
//   * `re.I` in `\bkm\b|kilomet` is read as ASCII case-insensitivity plus the three non-ASCII characters Python's IGNORECASE equates with one of those letters: the Kelvin sign (U+212A, with k), the
//     capital I with a dot above (U+0130, with i) and the dotless i (U+0131, with i).  (The first draft of this port named only the Kelvin sign and the dotless i.)  Since group C6 the three are DERIVED, in
//     tests/devtools/unicode_tables_tests.cpp, from the Unicode Character Database 15.1.0 files the manifest pins, and this reading is held to the derivation over every code point.
//   * `\w`, `\d`, `\s` and `\b` are Unicode-aware as Python's are, within the limit odl/devkit/pytext.hpp states.  The messages name `unitcheck`; argparse's abbreviations are not accepted and `-h`
//     prints this tool's own text.  An option's value is whatever follows it unless that begins with `--` (argparse refuses one that begins with a single dash too, so a directory called -x is a
//     value here), and `-h=x` is refused.
//
// THREE BLIND SPOTS OF THE PYTHON, FIXED IN GROUP C6 (the maintainer's ruling of 2026-10-07; each has Catch2 cases, and the changes are neutral on every recorded tree: the ten gate-12 sections and the real
// tree's non-quiet register come out byte for byte as before, since none of these forms occurs in the production sources):
//   * A literal with a STANDARD SUFFIX is a number: `1000.0f`, `1000u`, `1000UL`, `1e3f`, `1e3L`.  The suffix is the one of its kind -- [uU](ll|LL|l|L)? or (ll|LL|l|L)[uU]? after an integer literal, [fFlL]
//     after a floating one (a point or an exponent); `1000f` and `1000.0u` are not C++ and stay invisible, and so does a user-defined or library suffix (`1000_km`, the chrono `1000ms`).  The Python's
//     NUMBER refused any number with a letter after it, so these were invisible to the register.
//   * C++ DIGIT SEPARATORS are part of the number: `1'000`, `1'000.0`, `1e1'0` (an apostrophe BETWEEN TWO DIGITS).  The Python read `1'000` as the two numbers 1 and 000.
//   * A `//` inside a STRING literal, a character literal, a raw string (R"d( ... )d", which may span lines) or a block comment is not a comment: the comment starts at the first `//` that C++ lexes as
//     one, and a line that begins inside a string is not a comment line even if it begins with `//` or `*`.  The Python took the first `//` of the line, wherever it stood, so a `//` in a string hid
//     the rest of the line (a factor of a thousand after it was lost) and could serve as a marker.
//
// GROUP C7 (the maintainer's rulings of 2026-10-07, each with Catch2 cases): the fourth blind spot, the other radixes, and the units a suffix names.  On today's tree the register changes in exactly one
// way, the correction of a misreading (two rows leave it, below); the ten recorded gate-12 sections come out as they were recorded after the same correction and one new line.
//   * A LINE IS SKIPPED ONLY IF THE LEXER SAYS IT IS COMMENT.  The lexer (lex_line, state carried across lines) gives, for each line, the spans of text that are not comment -- code, strings, character
//     literals and raw strings: numbers in them still count -- and the numbers are read in those spans.  The Python skipped every line that began with `//`, `*` or `/*`, and cut the rest at its first `//`: a
//     multiplication continued on a line that began with `*`, and the statement after a block comment that opened the line, were invisible, and the lines of a block comment that did not begin with `*` were
//     read.  Now a line that begins with `*` outside a comment is code, a line inside a block comment is comment whatever it begins with, and the code after `*/` is read.  No such line held a factor of a
//     thousand (scans of the production sources, group C6).  The line above a literal serves as its marker's place when it is a comment line of its own: a `//` comment with nothing but blanks before it,
//     that does not begin inside a string or a block comment.
//   * HEXADECIMAL, BINARY AND OCTAL literals are read as C++ reads them, with the same digit separators and standard suffixes: `0x3E8`, `0b1111101000`, `01750` and `0x1.F4p9` are 1000; `01000` is 512 (an
//     integer that begins with 0 is octal; a floating literal is decimal, `01000.0` is 1000; `089` is no literal).  A hexadecimal floating literal is ONE number: `0x1p-1000` and `0x1p+1000` of
//     modules/estimation/src/normal_equations.cpp, which the register carried as rows (the digits of their exponents read as the number 1000, each with a marker saying so), are 2^-1000 and 2^1000 and LEAVE
//     the register: 30 literals become 28 and 14 declared non-conversions 12.  That is the CORRECTION OF A MISREADING, and both rows are named here.
//   * USER-DEFINED AND LIBRARY SUFFIXES are a CATEGORY OF THEIR OWN in the register: a number that carries one (an underscore and an identifier, `1000_km`; exactly h, min, s, ms, us, ns, d, y, i, il or if,
//     `1000ms`) whose value is 1000 or 0.001 is LISTED, with its line, in its own section and in the denominator, counted in the total, and never judged: no marker is asked of it, none changes its bucket and it
//     fails nothing.  The names of units are what a register of unit crossings is for, and a reviewer should see them.  None occurs in the production sources today.  Any other letters after a number (`1000f`,
//     `1000.0u`, `1000sec`) are not C++ and stay invisible, as ratified in group C6.
//
// KNOWN LIMITS, KEPT AND DOCUMENTED (not changed without a ruling):
//   * Numbers INSIDE string literals still count (the register holds two: normal_equations.cpp's message for 2^-1000 .. 2^1000), and so do the digits of a character literal.
//   * A leading-point literal (`.001`, `.5e3`) is invisible: the point refuses the lookbehind.  (Not in the ruling; none in the production sources.)
//   * A `//` comment that ends in a backslash carries onto the next line in C++; here it does not (the tree's -Wall -Werror refuses such a comment: -Wcomment).  Trigraphs are not read.  A number that C++
//     would lex into a longer pp-number (`0xE+1000`, which is one invalid token) is read as the literals it looks like.

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/rational.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "unitcheck.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace odl::tools::unitcheck {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "unitcheck";
constexpr int kOk = 0, kFailed = 1, kArgument = 2, kInternal = 70;
constexpr std::size_t kNpos = std::string_view::npos;
constexpr const char* kCrossingHome = "modules/core/include/odl/core/units.hpp";

using Io = dk::Streams;
using Cp = std::uint32_t;

bool is_word(Cp cp) noexcept { return dk::is_py_word(cp); }

// `\d*` from i: the end of the run of decimal digits (of any script).
std::size_t digits_end(std::string_view s, std::size_t i) {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!dk::is_py_decimal(dk::code_point_at(s, i, after))) break;
        i = after;
    }
    return i;
}

// `\s*` from i.
std::size_t skip_space(std::string_view s, std::size_t i) {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!dk::is_py_space(dk::code_point_at(s, i, after))) break;
        i = after;
    }
    return i;
}

// (?<![\w.]) at i: the character before i is neither a word character nor a point (the start of the text is neither)
bool lookbehind_ok(std::string_view s, std::size_t i) {
    if (i == 0) return true;
    std::size_t start = 0;
    Cp cp = 0;
    if (!dk::code_point_before(s, i, start, cp)) return true;   // not well-formed: no word, no point
    return !(is_word(cp) || cp == U'.');
}

// (?![\w.]) at e: the character at e is neither a word character nor a point (the end of the text is neither)
bool lookahead_ok(std::string_view s, std::size_t e) {
    if (e >= s.size()) return true;
    std::size_t after = 0;
    const Cp cp = dk::code_point_at(s, e, after);
    return !(is_word(cp) || cp == U'.');
}

bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }
bool is_hex_digit(char c) { return is_ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool is_binary_digit(char c) { return c == '0' || c == '1'; }

// `\d+(?:'\d+)*` from i: decimal digits with C++14 digit separators.  An apostrophe belongs to the number only BETWEEN TWO DIGITS (it is a separator there and a character literal's quote anywhere else); the end
// of the longest such run, or i when no digit starts at i.  (Group C6: the Python's NUMBER knew no separator, so `1'000` was the two numbers 1 and 000.)
std::size_t digit_seq_end(std::string_view s, std::size_t i) {
    std::size_t end = digits_end(s, i);
    if (end == i) return i;
    while (end < s.size() && s[end] == '\'') {
        const std::size_t more = digits_end(s, end + 1);
        if (more == end + 1) break;   // no digit after the apostrophe: not a separator
        end = more;
    }
    return end;
}

// the same for the ASCII digits of one radix (hexadecimal, binary, and the decimal digits of a binary exponent): a run of them with an apostrophe allowed between two
template <typename IsDigit>
std::size_t ascii_seq_end(std::string_view s, std::size_t i, IsDigit is_digit) {
    std::size_t end = i;
    while (end < s.size() && is_digit(s[end])) ++end;
    if (end == i) return i;
    while (end + 1 < s.size() && s[end] == '\'' && is_digit(s[end + 1])) {
        ++end;
        while (end < s.size() && is_digit(s[end])) ++end;
    }
    return end;
}

// [eE][+-]?\d+ at p (digits with separators): the end of the exponent, kNpos when there is none
std::size_t exponent_end(std::string_view s, std::size_t p) {
    if (p >= s.size() || (s[p] != 'e' && s[p] != 'E')) return kNpos;
    std::size_t q = p + 1;
    if (q < s.size() && (s[q] == '+' || s[q] == '-')) ++q;
    const std::size_t end = digit_seq_end(s, q);
    return end > q ? end : kNpos;
}

// The end of the standard suffix at p, or p when there is none (group C6).  After an INTEGER literal: [uU](ll|LL|l|L)? or (ll|LL|l|L)[uU]? -- `ll` and `LL` must keep their case, `lL` is no suffix.  After a
// FLOATING one (a point or an exponent was seen): [fF] or [lL].  A suffix of the other kind is not C++ (`1000f`, `1000.0u`).
std::size_t suffix_end(std::string_view s, std::size_t p, bool floating) {
    const auto at = [&](std::size_t k) { return k < s.size() ? s[k] : '\0'; };
    if (floating) {
        const char c = at(p);
        return (c == 'f' || c == 'F' || c == 'l' || c == 'L') ? p + 1 : p;
    }
    const auto long_end = [&](std::size_t k) -> std::size_t {   // (ll|LL|l|L) at k, or k
        if ((at(k) == 'l' && at(k + 1) == 'l') || (at(k) == 'L' && at(k + 1) == 'L')) return k + 2;
        if (at(k) == 'l' || at(k) == 'L') return k + 1;
        return k;
    };
    if (at(p) == 'u' || at(p) == 'U') return long_end(p + 1);
    const std::size_t q = long_end(p);
    if (q == p) return p;
    return (at(q) == 'u' || at(q) == 'U') ? q + 1 : q;
}

// the end of the run of word characters from p (the identifier a suffix is made of)
std::size_t word_run_end(std::string_view s, std::size_t p) {
    while (p < s.size()) {
        std::size_t after = 0;
        if (!is_word(dk::code_point_at(s, p, after))) break;
        p = after;
    }
    return p;
}

// A USER-DEFINED suffix begins with an underscore (1000_km); a LIBRARY-defined one is a suffix of <chrono> or <complex> that a number can carry: h min s ms us ns d y, i il if.  (Group C7.)
bool is_user_suffix(std::string_view run) {
    static constexpr std::string_view kLibrary[] = {"h", "min", "s", "ms", "us", "ns", "d", "y", "i", "il", "if"};
    if (run.empty()) return false;
    if (run.front() == '_') return true;
    return std::find(std::begin(kLibrary), std::end(kLibrary), run) != std::end(kLibrary);
}

struct Suffix {
    std::size_t end = kNpos;   // the end of the token, kNpos when the body is no literal here
    bool user = false;         // the suffix is a user-defined or library one
};

// The token for a body that ends at `body_end`: with its standard suffix when there is one, or its user-defined or library suffix, as the lookahead (?![\w.]) lets it.  Nothing fits when a word character or
// a point follows what is taken: `1000f`, `1000.0u`, `1000_`, `1000.5.3`.  (A point after a number is part of a C++ pp-number whatever follows it, so `1000_km.count()` is not a literal and a suffix.)
Suffix with_suffix(std::string_view s, std::size_t body_end, bool floating) {
    const std::size_t end = suffix_end(s, body_end, floating);
    if (lookahead_ok(s, end)) return {end, false};
    const std::size_t run_end = word_run_end(s, body_end);
    if (run_end > body_end && is_user_suffix(s.substr(body_end, run_end - body_end)) && lookahead_ok(s, run_end)) return {run_end, true};
    return {};
}

enum class Base { Decimal, Octal, Hex, Binary };

// One numeric literal as C++ writes it, found at a position: where its body (digits, point, exponent) and the whole token end.
struct Literal {
    Base base = Base::Decimal;
    bool floating = false;     // a decimal or hexadecimal floating literal
    std::size_t body_end = 0;  // where the digits and the exponent end
    std::size_t end = 0;       // where the token ends, its suffix included
    bool user_suffix = false;  // the suffix is a user-defined or library one
};

std::optional<Literal> hex_literal_at(std::string_view s, std::size_t i) {
    const std::size_t p = i + 2;
    const std::size_t int_end = ascii_seq_end(s, p, is_hex_digit);
    std::size_t frac_end = int_end;
    const bool point = int_end < s.size() && s[int_end] == '.';
    if (point) frac_end = ascii_seq_end(s, int_end + 1, is_hex_digit);
    if (int_end == p && (!point || frac_end == int_end + 1)) return std::nullopt;   // no digit at all: `0x`, `0x.`, `0xg`
    std::size_t exp_end = kNpos;
    if (frac_end < s.size() && (s[frac_end] == 'p' || s[frac_end] == 'P')) {
        std::size_t r = frac_end + 1;
        if (r < s.size() && (s[r] == '+' || s[r] == '-')) ++r;
        const std::size_t digits_end_at = ascii_seq_end(s, r, is_ascii_digit);
        if (digits_end_at > r) exp_end = digits_end_at;
    }
    const bool floating = point || exp_end != kNpos;
    if (floating && exp_end == kNpos) return std::nullopt;   // a hexadecimal floating literal needs its binary exponent
    const std::size_t body_end = floating ? exp_end : int_end;
    const Suffix suffix = with_suffix(s, body_end, floating);
    if (suffix.end == kNpos) return std::nullopt;
    return Literal{Base::Hex, floating, body_end, suffix.end, suffix.user};
}

std::optional<Literal> binary_literal_at(std::string_view s, std::size_t i) {
    const std::size_t end = ascii_seq_end(s, i + 2, is_binary_digit);
    if (end == i + 2) return std::nullopt;
    const Suffix suffix = with_suffix(s, end, false);
    if (suffix.end == kNpos) return std::nullopt;
    return Literal{Base::Binary, false, end, suffix.end, suffix.user};
}

// The Python's `\d+\.?\d*(?:[eE][+-]?\d+)?` followed by a lookahead that only the full-length candidates can satisfy: a shorter run of digits is followed by a digit (a word character), and a point that is not taken
// is followed by the point.  So the candidates, in the engine's order, are: after `D.F`, the exponent first and then none; after `D` with no point, the exponent first when an e follows (the bare `D` is followed by
// the e, a word character), else `D` alone.  Group C6 made the digits runs with separators and added the optional standard suffix after the exponent; group C7 adds the user-defined and library suffixes, and reads an
// integer literal that begins with 0 as octal (`01750` is 1000, `01000` is 512; `089` is no literal).
std::optional<Literal> decimal_literal_at(std::string_view s, std::size_t i) {
    const std::size_t d_end = digit_seq_end(s, i);
    if (d_end == i) return std::nullopt;
    const bool point = d_end < s.size() && s[d_end] == '.';
    const std::size_t mantissa_end = point ? digit_seq_end(s, d_end + 1) : d_end;   // `1.` takes its point: the end is d_end + 1 when no digit follows it
    const std::size_t with_exponent = exponent_end(s, mantissa_end);
    if (with_exponent != kNpos) {
        const Suffix suffix = with_suffix(s, with_exponent, true);
        if (suffix.end != kNpos) return Literal{Base::Decimal, true, with_exponent, suffix.end, suffix.user};
    }
    const Suffix suffix = with_suffix(s, mantissa_end, point);
    if (suffix.end == kNpos) return std::nullopt;
    Base base = Base::Decimal;
    if (!point && s[i] == '0' && mantissa_end - i > 1) {
        // an integer with a leading zero: octal, when it is made of ASCII digits (and separators) and those are 0 to 7
        bool ascii = true;
        bool octal_digits = true;
        for (std::size_t k = i; k < mantissa_end; ++k) {
            if (s[k] == '\'') continue;
            if (!is_ascii_digit(s[k])) ascii = false;
            else if (s[k] > '7') octal_digits = false;
        }
        if (ascii) {
            if (!octal_digits) return std::nullopt;   // `089`: no literal
            base = Base::Octal;
        }
    }
    return Literal{base, point, mantissa_end, suffix.end, suffix.user};
}

// the literal that begins at i, if one does: the lookbehind (?<![\w.]) first
std::optional<Literal> literal_at(std::string_view s, std::size_t i) {
    if (!lookbehind_ok(s, i)) return std::nullopt;
    if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) return hex_literal_at(s, i);
    if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'b' || s[i + 1] == 'B')) return binary_literal_at(s, i);
    return decimal_literal_at(s, i);
}

// the digits of any script as ASCII, for strtod, and without the digit separators
std::string ascii_digits(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size();) {
        std::size_t after = 0;
        const Cp cp = dk::code_point_at(s, i, after);
        const int digit = dk::py_decimal_value(cp);
        if (digit >= 0) out.push_back(static_cast<char>('0' + digit));
        else if (cp != U'\'') out.append(s.substr(i, after - i));
        i = after;
    }
    return out;
}

// the value of the digits of an integer literal in one radix, separators skipped, as far as it matters: 1001 stands for anything above 1000
unsigned long long small_value(std::string_view digits, unsigned radix) {
    unsigned long long value = 0;
    for (const char c : digits) {
        if (c == '\'') continue;
        const unsigned digit = is_ascii_digit(c) ? static_cast<unsigned>(c - '0') : (c >= 'a' && c <= 'f') ? static_cast<unsigned>(c - 'a') + 10U : static_cast<unsigned>(c - 'A') + 10U;
        value = value * radix + digit;
        if (value > 1000) return 1001;
    }
    return value;
}

// is the value of this hexadecimal floating literal's body ("0x" digits [. digits] p [sign] digits, separators allowed) the double 1000 or the double 0.001?  It is computed exactly and rounded once, as the
// compiler does; a value beyond the range of a double is not either.
bool hex_float_is_thousand(std::string_view body) {
    dk::BigInt mantissa;
    std::int64_t fraction_digits = 0;
    bool after_point = false;
    std::size_t p = 2;
    for (; p < body.size() && body[p] != 'p' && body[p] != 'P'; ++p) {
        const char c = body[p];
        if (c == '\'') continue;
        if (c == '.') {
            after_point = true;
            continue;
        }
        const int digit = is_ascii_digit(c) ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - 'A' + 10;
        mantissa = mantissa.shifted_left(4) + dk::BigInt(digit);
        if (after_point) ++fraction_digits;
        if (mantissa.bit_length() > 5000) return false;   // no double has so many significant digits to be 1000 with
    }
    ++p;   // the p
    bool negative = false;
    if (p < body.size() && (body[p] == '+' || body[p] == '-')) negative = body[p++] == '-';
    std::int64_t exponent = 0;
    for (; p < body.size(); ++p) {
        if (body[p] == '\'') continue;
        exponent = exponent * 10 + (body[p] - '0');
        if (exponent > 100000) return false;
    }
    if (negative) exponent = -exponent;
    if (mantissa.is_zero()) return false;
    const std::int64_t shift = exponent - 4 * fraction_digits;
    if (shift > 6000 || shift < -6000) return false;
    const dk::Rational value = shift >= 0 ? dk::Rational(mantissa.shifted_left(static_cast<std::size_t>(shift))) : dk::Rational(mantissa, dk::BigInt(1).shifted_left(static_cast<std::size_t>(-shift)));
    try {
        const double d = value.to_double();
        return d == 1000.0 || d == 0.001;
    } catch (const std::range_error&) {
        return false;
    }
}

// the case-insensitive match of one code point with the ASCII letter `c`, as re.I reads it for the letters of "kilomet" and "km": the text's character is lower-cased (Python's sre takes the first
// code point of the full lower-case mapping) and compared with the pattern's letter, and the letters i and s have one extra equivalent each (the dotless i and the long s).  The non-ASCII characters
// that come out equal to one of these ASCII letters are exactly three: the Kelvin sign (to k), the capital I with a dot above (to i, the first of the two code points of its full lower case) and the dotless i
// (the extra equivalent of i).  They are DERIVED from the pinned Unicode Character Database 15.1.0 by tests/devtools/unicode_tables_tests.cpp, which holds names_km to that derivation over every code
// point; the fourth character `re` equates with an ASCII letter, the long s, goes with s, which neither word has.
bool ci_is(Cp cp, char c) {
    const Cp upper = static_cast<Cp>(c - 'a' + 'A');
    if (cp == static_cast<Cp>(static_cast<unsigned char>(c)) || cp == upper) return true;
    if (c == 'k') return cp == 0x212A;                  // KELVIN SIGN
    if (c == 'i') return cp == 0x0130 || cp == 0x0131;  // LATIN CAPITAL LETTER I WITH DOT ABOVE, LATIN SMALL LETTER DOTLESS I
    return false;
}

// re.search(r"\bkm\b|kilomet", what, re.I) matches at i by the first alternative (`km` with a word boundary on each side) or the second
bool km_at(std::string_view s, std::size_t i) {
    std::size_t at = 0;   // set before each alternative is tried
    const auto take = [&](char c) {
        if (at >= s.size()) return false;
        std::size_t after = 0;
        if (!ci_is(dk::code_point_at(s, at, after), c)) return false;
        at = after;
        return true;
    };
    if (dk::word_boundary_at(s, i)) {
        at = i;
        if (take('k') && take('m') && dk::word_boundary_at(s, at)) return true;
    }
    at = i;
    for (const char c : {'k', 'i', 'l', 'o', 'm', 'e', 't'}) {
        if (!take(c)) return false;
    }
    return true;
}

// an identifier character as the lexer below sees it: an ASCII letter or digit, the underscore, or any byte of a multi-byte UTF-8 sequence
bool ident_char(char c) {
    const auto u = static_cast<unsigned char>(c);
    return (u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || u == '_' || u >= 0x80;
}

// The end of the quoted literal that begins just before i (its opening quote is s[i - 1]): the index after the closing quote, or s.size() when the line ends first.  A backslash takes the next
// character with it (\" \\ \'); `continued` says that the line ended on a backslash inside the literal -- a line splice, which carries a string over to the next line.
std::size_t quoted_end(std::string_view s, std::size_t i, char quote, bool& continued) {
    continued = false;
    while (i < s.size()) {
        const char c = s[i];
        if (c == '\\') {
            if (i + 1 >= s.size()) {
                continued = true;
                return s.size();
            }
            i += 2;
        } else if (c == quote) {
            return i + 1;
        } else {
            ++i;
        }
    }
    return s.size();
}

// A raw string literal opens at the quote at q when the identifier [ident_begin, q) just before it is one of the prefixes R, LR, uR, UR, u8R and a delimiter of at most sixteen characters (none a space, a
// parenthesis, a backslash or a tab, vertical tab or form feed) is followed by `(`.  Gives the delimiter and the index after the `(`.
bool raw_string_open(std::string_view s, std::size_t q, std::size_t ident_begin, std::string& delimiter, std::size_t& content) {
    const std::string_view prefix = s.substr(ident_begin, q - ident_begin);
    if (prefix != "R" && prefix != "LR" && prefix != "uR" && prefix != "UR" && prefix != "u8R") return false;
    std::size_t k = q + 1;
    while (k < s.size() && s[k] != '(') {
        const char c = s[k];
        if (c == ' ' || c == ')' || c == '\\' || c == '\t' || c == '\v' || c == '\f' || k - (q + 1) >= 16) return false;
        ++k;
    }
    if (k >= s.size()) return false;
    delimiter.assign(s.substr(q + 1, k - (q + 1)));
    content = k + 1;
    return true;
}

}  // namespace

namespace scan {

LexedLine lex_line(std::string_view s, LexState& state) {
    using Mode = LexState::Mode;
    LexedLine out;
    std::size_t i = 0;
    std::size_t ident_begin = 0;   // where the identifier token that came last begins: the text from there to a quote is a raw string's prefix only if it is exactly one of the five
    // the stretch of text that is not comment, open from `open`: a line that starts inside a block comment has none until the comment closes (group C7)
    std::size_t open = state.mode == Mode::BlockComment ? kNpos : 0;
    const auto close = [&](std::size_t end) {
        if (open != kNpos && end > open) out.code.push_back(CodeSpan{open, end});
        open = kNpos;
    };
    while (i < s.size()) {
        if (state.mode == Mode::BlockComment) {
            const std::size_t end = s.find("*/", i);
            if (end == kNpos) return out;   // the rest of the line is comment, and the comment goes on
            i = end + 2;
            state.mode = Mode::Code;
            open = i;
            continue;
        }
        if (state.mode == Mode::RawString) {
            const std::string closer = ")" + state.raw_delimiter + "\"";
            const std::size_t end = s.find(closer, i);
            if (end == kNpos) {
                close(s.size());
                return out;
            }
            i = end + closer.size();
            state.mode = Mode::Code;
            continue;
        }
        if (state.mode == Mode::String) {   // a string that a backslash at the end of the line carried over
            bool continued = false;
            i = quoted_end(s, i, '"', continued);
            if (continued) {
                close(s.size());
                return out;
            }
            state.mode = Mode::Code;
            continue;
        }
        const char c = s[i];
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            out.comment_at = i;
            close(i);
            return out;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            close(i);
            state.mode = Mode::BlockComment;
            i += 2;   // the `*` of the opener is not the `*` of a closer: `/*/` is still open
            continue;
        }
        if (c == '"') {
            std::string delimiter;
            std::size_t content = 0;
            if (raw_string_open(s, i, ident_begin, delimiter, content)) {
                state.mode = Mode::RawString;
                state.raw_delimiter = delimiter;
                i = content;
                continue;
            }
            bool continued = false;
            i = quoted_end(s, i + 1, '"', continued);
            if (continued) {
                state.mode = Mode::String;
                close(s.size());
                return out;
            }
            continue;
        }
        if (c == '\'') {   // a character literal; it ends with its line if nothing closes it (a digit separator is taken with its number, below, and never reaches here)
            bool continued = false;
            i = quoted_end(s, i + 1, '\'', continued);
            continue;
        }
        if (is_ascii_digit(c)) {
            // a number: digits, letters and underscores, and an apostrophe before one of them -- a digit separator, which must not open a character literal.  (A point or the sign of an exponent ends the run and the
            // next digit begins another; that changes nothing here, where only the apostrophes matter.)
            ++i;
            while (i < s.size() && (ident_char(s[i]) || (s[i] == '\'' && i + 1 < s.size() && ident_char(s[i + 1])))) ++i;
            continue;
        }
        if (ident_char(c)) {
            ident_begin = i;
            while (i < s.size() && ident_char(s[i])) ++i;
            continue;
        }
        ++i;
    }
    close(s.size());
    return out;
}

std::size_t comment_start(std::string_view s, LexState& state) { return lex_line(s, state).comment_at; }

std::optional<Marker> marker(std::string_view line) {
    static constexpr std::string_view kNames[] = {"NOT-A-UNIT-CROSSING", "UNIT-CROSSING"};
    const std::size_t slashes = line.find("//");
    if (slashes == kNpos) return std::nullopt;
    // Only the first `//` can start a match: its lazy `.*?` reaches everything after it.  Candidates for the marker name's first character, left to right.
    for (std::size_t s = slashes + 2; s < line.size();) {
        std::size_t after = 0;
        (void)dk::code_point_at(line, s, after);
        if (dk::word_boundary_at(line, s)) {   // `\b` before the name: the name begins with a word character, so the character before it must be none
            for (const std::string_view name : kNames) {
                if (line.compare(s, name.size(), name) != 0) continue;
                const std::size_t colon = skip_space(line, s + name.size());
                if (colon >= line.size() || line[colon] != ':') continue;
                // `\s*(.+?)\s*$`: the first non-blank character begins the text, which runs to the last non-blank one; if nothing but blanks follows (and something does) the text is the last blank
                const std::size_t begin = skip_space(line, colon + 1);
                if (begin < line.size()) {
                    std::size_t end = line.size();
                    while (end > begin) {
                        std::size_t start = 0;
                        Cp cp = 0;
                        if (!dk::code_point_before(line, end, start, cp) || !dk::is_py_space(cp)) break;
                        end = start;
                    }
                    return Marker{std::string(name), std::string(line.substr(begin, end - begin))};
                }
                if (colon + 1 < line.size()) {
                    std::size_t start = 0;
                    Cp cp = 0;
                    (void)dk::code_point_before(line, line.size(), start, cp);   // all that follows the colon is blank, every code point of it well-formed: this is the last blank
                    return Marker{std::string(name), std::string(line.substr(start))};
                }
            }
        }
        s = after;
    }
    return std::nullopt;
}

std::vector<std::string> number_tokens(std::string_view code) {
    std::vector<std::string> tokens;
    for (std::size_t i = 0; i < code.size();) {
        const std::optional<Literal> literal = literal_at(code, i);
        if (literal) {
            tokens.emplace_back(code.substr(i, literal->end - i));
            i = literal->end;
        } else {
            std::size_t after = 0;
            (void)dk::code_point_at(code, i, after);
            i = after;
        }
    }
    return tokens;
}

bool is_thousand(std::string_view token) {
    // a token is a literal wholly: float(tok) raised ValueError on what was not wholly a number.  The separators and the suffix are the literal's spelling and type, not its value (group C6): `1'000`, `1000.0f`
    // and `1e3L` are 1000; and since group C7 so are `0x3E8`, `0b1111101000`, `01750` and `0x1.F4p9`, as C++ reads them, and `01000` is 512.
    const std::optional<Literal> literal = literal_at(token, 0);
    if (!literal || literal->end != token.size()) return false;
    const std::string_view body = token.substr(0, literal->body_end);
    switch (literal->base) {
        case Base::Decimal: {
            const std::string ascii = ascii_digits(body);
            char* end = nullptr;
            const double v = std::strtod(ascii.c_str(), &end);
            if (end != ascii.c_str() + ascii.size()) return false;
            return v == 1000.0 || v == 0.001;
        }
        case Base::Octal: return small_value(body, 8) == 1000;
        case Base::Binary: return small_value(body.substr(2), 2) == 1000;
        case Base::Hex: return literal->floating ? hex_float_is_thousand(body) : small_value(body.substr(2), 16) == 1000;
    }
    return false;
}

std::string_view user_suffix_of(std::string_view token) {
    const std::optional<Literal> literal = literal_at(token, 0);
    if (!literal || literal->end != token.size() || !literal->user_suffix) return {};
    return token.substr(literal->body_end);
}

bool names_km(std::string_view what) {
    for (std::size_t i = 0; i < what.size();) {
        if (km_at(what, i)) return true;
        std::size_t after = 0;
        (void)dk::code_point_at(what, i, after);
        i = after;
    }
    return false;
}

}  // namespace scan

namespace {

struct Row {
    std::string rel;
    std::size_t line = 0;
    std::string literal;
    std::string text;   // line.strip()
    std::string what;   // the annotation, when there is one
};

std::string right(std::size_t value, std::size_t width) { return dk::pad_left(std::to_string(value), width); }

std::string first_code_points(std::string_view s, std::size_t n) {
    std::size_t i = 0;
    for (std::size_t taken = 0; taken < n && i < s.size(); ++taken) {
        std::size_t after = 0;
        (void)dk::code_point_at(s, i, after);
        i = after;
    }
    return std::string(s.substr(0, i));
}

std::vector<std::string_view> split_on_newline(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t from = 0;
    while (true) {
        const std::size_t nl = text.find('\n', from);
        if (nl == kNpos) {
            lines.push_back(text.substr(from));
            return lines;
        }
        lines.push_back(text.substr(from, nl - from));
        from = nl + 1;
    }
}

std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += (i != 0 ? "/" : "") + parts[i];
    return out;
}

const char kUsageText[] = "usage: unitcheck [-h] [--quiet] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "the register of every factor of a thousand in production code (SPEC-dynamics DYN-R-040): every literal whose VALUE is exactly 1000 or 0.001, in the .hpp and .cpp files of <root>/modules\n"
    "(tests excluded), is either in core/units.hpp or carries `// UNIT-CROSSING: <from> -> <to>` or `// NOT-A-UNIT-CROSSING: <what it is>` on its line or the line directly above.  A\n"
    "UNIT-CROSSING that names kilometres anywhere but core/units.hpp is refused.  Literals are read as C++ reads them (decimal, hexadecimal, binary, octal, floating, with their separators and\n"
    "suffixes); one that carries a user-defined or library suffix (1000_km, 1000ms) is listed in a category of its own and never judged.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --quiet      print the counts and the verdict, not the register\n"
    "  --root ROOT  the tree (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 every factor of a thousand is accounted for   1 one is not, or one names kilometres outside core/units.hpp   2 an argument error or nothing to search\n";

}  // namespace

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

int run(const std::vector<std::string>& argv, Io io) {
    try {
        std::string root_given = default_root().string();
        bool quiet = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument --root: expected one argument");
                    value = argv[++i];
                }
                root_given = value;
            } else if (opt == "--quiet" && !has_value) {
                quiet = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }
        const fs::path root(root_given);

        // production_sources(): the .hpp and .cpp files below modules/, none under a `build` or a `tests` directory, sorted as paths (files_below returns them in that order already)
        struct Source {
            std::vector<std::string> rel;
            fs::path path;
        };
        std::vector<Source> srcs;
        for (const std::vector<std::string>& parts : dk::files_below(root / "modules", [](const std::string& name) { return name == "build" || name == "tests"; })) {
            // rglob("*.hpp") and rglob("*.cpp") are fnmatch patterns on the NAME: `*` may be empty, so a file called `.hpp` matches too (it has no pathlib `suffix`, which is why this is not path_suffix)
            const std::string& name = parts.back();
            if (!name.ends_with(".hpp") && !name.ends_with(".cpp")) continue;
            std::vector<std::string> rel = {"modules"};
            rel.insert(rel.end(), parts.begin(), parts.end());
            srcs.push_back(Source{rel, root / joined(rel)});
        }
        if (srcs.empty()) {
            io.err << kTool << ": nothing to search: no .hpp or .cpp file under " << (root / "modules").string() << " (outside build and tests directories)\n";
            return kArgument;
        }

        std::vector<Row> home, crossings, declared, unaccounted, misplaced, suffixed;
        for (const Source& source : srcs) {
            const std::string rel = joined(source.rel);
            std::string text;
            try {
                text = dk::read_text_lossy(source.path);
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            const std::vector<std::string_view> lines = split_on_newline(text);
            // How C++ lexes the file: where each line's comment begins -- the first `//` that is not inside a string, a character literal, a raw string or a block comment (group C6; the Python took the first `//` of the
            // line wherever it stood) -- and the spans of the line that are not comment (group C7).
            struct LineLex {
                std::size_t comment_at = kNpos;
                std::vector<scan::CodeSpan> code;
            };
            std::vector<LineLex> lex(lines.size());
            scan::LexState state;
            for (std::size_t k = 0; k < lines.size(); ++k) {
                scan::LexedLine lexed = scan::lex_line(lines[k], state);
                lex[k].comment_at = lexed.comment_at;
                lex[k].code = std::move(lexed.code);
            }
            for (std::size_t index = 0; index < lines.size(); ++index) {
                const std::string_view line = lines[index];
                const std::string_view prev = index >= 1 ? lines[index - 1] : std::string_view();
                // the numbers of the line are those of its CODE: what the lexer says is not comment (group C7; the Python skipped every line that began with //, * or /*, and read the rest of the line up to its
                // first //, so that a multiplication continued on a line that began with * was invisible and so was the code after a block comment that opened the line)
                std::vector<std::string> found;
                for (const scan::CodeSpan& span : lex[index].code) {
                    for (std::string& token : scan::number_tokens(line.substr(span.begin, span.end - span.begin))) {
                        if (scan::is_thousand(token)) found.push_back(std::move(token));
                    }
                }
                if (found.empty()) continue;
                // the marker is read from the line's own comment, which starts where the comment starts (the regular expression's `//.*?` begins at the first `//` it is given)
                const std::size_t comment_at = lex[index].comment_at;
                std::optional<scan::Marker> mark = comment_at == kNpos ? std::nullopt : scan::marker(line.substr(comment_at));
                // or from the line above, when that one is a comment line of its own: a line comment with nothing but blanks before it.  (A line that begins inside a string or a block comment is none: its comment, when
                // it has one, comes after the end of the string or of the block, which is something before it.)
                if (!mark && index >= 1 && lex[index - 1].comment_at != kNpos && dk::lstrip_py(prev.substr(0, lex[index - 1].comment_at)).empty()) {
                    mark = scan::marker(prev);
                }
                for (const std::string& literal : found) {
                    Row row{rel, index + 1, literal, dk::strip_py(line), std::string()};
                    if (!scan::user_suffix_of(literal).empty()) {
                        // a number with a user-defined or library suffix names its unit: it is LISTED, whatever markers its line carries, and never judged
                        suffixed.push_back(std::move(row));
                    } else if (rel == kCrossingHome) {
                        home.push_back(std::move(row));
                    } else if (mark && mark->kind == "UNIT-CROSSING") {
                        // AN ANNOTATION IS NOT A PERMIT: no conversion outside core/units.hpp may name kilometres -- if kilometres are involved it IS the crossing, and the crossing has one site.
                        row.what = mark->what;
                        (scan::names_km(mark->what) ? misplaced : crossings).push_back(std::move(row));
                    } else if (mark) {
                        row.what = mark->what;
                        declared.push_back(std::move(row));
                    } else {
                        unaccounted.push_back(std::move(row));
                    }
                }
            }
        }

        if (!quiet) {
            io.out << "THE REGISTER \xE2\x80\x94 every literal whose value is exactly 1000 or 0.001,\n";
            io.out << "in production sources (tests excluded).\n\n";
            io.out << "  the crossing itself, in " << kCrossingHome << ":\n";
            for (const Row& r : home) io.out << "    " << right(r.line, 5) << "  " << dk::pad_right(r.literal, 8) << ' ' << first_code_points(r.text, 80) << '\n';
            io.out << "\n  unit conversions, " << crossings.size() << ", each naming what it converts:\n";
            for (const Row& r : crossings) io.out << "    " << r.rel << ':' << r.line << "  " << dk::pad_right(r.literal, 8) << ' ' << r.what << '\n';
            io.out << "\n  not conversions at all, " << declared.size() << ":\n";
            for (const Row& r : declared) io.out << "    " << r.rel << ':' << r.line << "  " << dk::pad_right(r.literal, 8) << ' ' << r.what << '\n';
            // group C7: the numbers that name a unit by a suffix (1000_km, 1000ms), which a reviewer should see; no marker is asked of them and none can fail
            io.out << "\n  user-defined or library suffixes, " << suffixed.size() << ", listed and never judged:\n";
            for (const Row& r : suffixed) io.out << "    " << r.rel << ':' << r.line << "  " << dk::pad_right(r.literal, 8) << ' ' << first_code_points(r.text, 80) << '\n';
        }

        // THE DENOMINATOR.  The last number is the gate; the others make it legible.
        io.out << "\n  production sources searched     " << right(srcs.size(), 5) << '\n';
        const std::size_t n = home.size() + crossings.size() + declared.size() + unaccounted.size() + misplaced.size() + suffixed.size();
        io.out << "  literals of value 1000 or 1/1000 " << right(n, 5) << '\n';
        io.out << "    in " << dk::pad_right(kCrossingHome, 44) << ' ' << right(home.size(), 5) << '\n';
        io.out << "    annotated UNIT-CROSSING                        " << right(crossings.size(), 5) << '\n';
        io.out << "    annotated NOT-A-UNIT-CROSSING                  " << right(declared.size(), 5) << '\n';
        io.out << "    ACCOUNTED FOR BY NEITHER                       " << right(unaccounted.size(), 5) << '\n';
        io.out << "    naming km OUTSIDE core/units.hpp               " << right(misplaced.size(), 5) << '\n';
        io.out << "    with a user-defined or library suffix (listed) " << right(suffixed.size(), 5) << '\n';

        if (!misplaced.empty()) {
            io.err << "\nA KM<->M CROSSING OUTSIDE core/units.hpp:\n";
            for (const Row& r : misplaced) io.err << "  " << r.rel << ':' << r.line << "  annotated '" << r.what << "'\n";
            io.err << "\n  Annotating it does not make it legal. SPEC-frames FRAME-R-062 and\n"
                   << "  SPEC-dynamics DYN-R-010 give the km<->m crossing ONE site, and this is not it:\n"
                   << "  call metres_from_km / km_from_metres, or state_accel_km_s2_from_m_s2 and\n"
                   << "  field_position_m_from_state_km if what you are crossing is a state quantity.\n"
                   << "  One conversion site becomes six, and five of them are somebody's afternoon.\n";
            return kFailed;
        }
        if (!unaccounted.empty()) {
            io.err << "\nUNACCOUNTED FACTOR OF A THOUSAND:\n";
            for (const Row& r : unaccounted) io.err << "  " << r.rel << ':' << r.line << "  [" << r.literal << "]  " << first_code_points(r.text, 90) << '\n';
            io.err << "\n  Every factor of a thousand in production code is either the km<->m crossing\n"
                   << "  in core/units.hpp, or says what it is. Add ONE of:\n"
                   << "      // UNIT-CROSSING: <from> -> <to>\n"
                   << "      // NOT-A-UNIT-CROSSING: <what it is>\n"
                   << "  on that line or the one directly above it. If it IS a km<->m scaling, it does not belong here at all:\n"
                   << "  call core/units.hpp, which is what SPEC-frames FRAME-R-062 and SPEC-dynamics\n"
                   << "  DYN-R-010 require and what this gate exists to keep true.\n";
            return kFailed;
        }

        io.out << "\nok       every factor of a thousand in production code is accounted for\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::unitcheck

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::unitcheck::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
