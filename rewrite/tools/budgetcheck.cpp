// budgetcheck.cpp — evaluate the arithmetic in every specification budget row.
//
// Plan L0 step 8 (group C4, the user's directive of 2026-10-06) ported it from tools/budgetcheck.py.
//
// WHY THIS EXISTS.  On 2026-09-18 four of the twenty-three budget rows in this tree were wrong -- by factors of 1000, 1000, 1000 and 10^6 -- and every one of them had been read and adopted.
// One was in a specification the manager had reviewed.  The rows are the only class of number in this project with no test behind them: `tests/toolchain_smoke.cpp` had the same arithmetic
// right the whole time, because code gets tested and prose does not.  Writing the multiplication out makes the error VISIBLE to a careful reader; "visible to a careful reader" is exactly the
// standard that failed four times in one document.  This makes it CHECKABLE instead, which is the same move as printing components rather than a verdict.
//
// HOW IT AVOIDS BEING THE NEXT DISGUISE.  A checker that silently skipped rows it could not parse would be worse than none: it would report success over the rows it understood and say nothing
// about the rest.  So every budget row is classified, and the third class is a failure:
//
//   checked        contains `... = **result**`; the arithmetic was evaluated
//   no-arithmetic  contains no such expression anywhere in the row
//   UNPARSEABLE    contains one and it did not parse  ->  FAILURE
//
// Row shape is not assumed.  The tables differ between specifications -- SPEC-frames has four columns where SPEC-time has five -- so this scans the whole row for expressions rather than
// trusting a column index.
//
//   budgetcheck [--spec-dir DIR] [--quiet]
//   exit 0 all rows check   1 a row is wrong   2 a row would not parse (also an argument error, a specification that cannot be read)   70 an error the tool did not anticipate
//
// THE PROOF.  The tool's product is what it PRINTS; with --quiet (ci.sh's gate 8, the ctest `budget.arithmetic`) that is three lines, and the committed record of what budgetcheck.py printed on
// this tree is gate 8's section of the safety-net run at acac318 (128 bytes, kept with the L0-8 report as C4_budgetcheck_baseline_stdout.txt).  The port prints those bytes, every one, with exit
// 0 and an empty standard error, against a substitution list registered before the comparison and EMPTY.  What the committed record does NOT hold is the per-row lines of a run without
// --quiet and the text of any failure: the Python was not run beside the port, so those parts stand on the readings below, on tests derived by hand from the Python's own statements, on controls
// over every real row (ctest `budgetcheck.real_tree`), and on rule 5.
//
// THE ARITHMETIC IS THE PYTHON'S, operation for operation, so that the same libm gives the same bits: float() is strtod, `**` is pow, math.log10 and math.floor are log10 and floor, the tolerance is
// ((0.5 * 10^k) * 1.001), the product of quantities is built left to right, and nothing is fused (-ffp-contract=off).
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * Python's OverflowError (a number or a power of a unit outside double range: `10^400`, `km^200`, a stated `1e999`) was a traceback; here it is an UNPARSEABLE finding, exit 2.  So is a
//     unit scale that underflows to zero (the Python divided by it: ZeroDivisionError).
//   * Python's own ValueError texts are interpreter-version text; where the Python's would be one (an empty `max()`), this tool's words stand in.  An exponent of a unit or of a power of ten whose
//     magnitude exceeds a million is refused (the Python's integers have no limit; this tool's are 64 bits).
//   * A specification that is not UTF-8 is REFUSED, exit 2 (the Python raised UnicodeDecodeError); the messages name `budgetcheck`, not `budgetcheck.py`; argparse's abbreviations (`--qu`) are
//     not accepted and `-h` prints this tool's own text.
//   * REFUSED, by the maintainer's ruling of 2026-10-07 (each was a silent misreading in the Python found by reading it; plan constraint 4: refuse rather than approximate; none changes a result
//     on today's specifications, which the controls of the ctest `budgetcheck.real_tree` and the unchanged three lines of gate 8 show): (1) a `--spec-dir` that is absent or holds no SPEC-*.md
//     (the Python passed it with "0 budget rows", success over nothing read; a directory whose specifications hold no budget row still passes, it was read); (2) `10^+3`, which read as 10
//     because the plus sign made `^+3` a dimensionless "unit" raised to 3, and so any power with no unit before it; (3) a unit token `m^` with no exponent, which read as `m`; (4) int()'s digit
//     separators in an exponent (`m^1_0`, which read as `m^10`); (5) a left side that is NaN (infinity times zero), which passed because no comparison with NaN is true.  Each is UNPARSEABLE, exit 2,
//     naming the row, except (1), which is refused as speccheck refuses it ("no SPEC-*.md in DIR", exit 2).
//   * KEPT, by the same ruling: the tolerance of a stated zero is 0.5 in the stated units.  It is the stated-precision rule -- half a unit of the last stated digit -- read for a stated zero, and
//     the Python gave every zero the same 0.5 however many digits it is written with (`0`, `0.0` and `0.00` alike), which this tool keeps.
//   * `\s` and `\d` are Unicode-aware as Python's are, within the limit odl/devkit/pytext.hpp states; repr of a prose prefix is the devkit's py_repr.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "budgetcheck.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <numbers>
#include <ostream>
#include <system_error>

namespace odl::tools::budgetcheck {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "budgetcheck";
constexpr int kOk = 0, kWrong = 1, kUnparseable = 2, kInternal = 70;
constexpr std::size_t kNpos = std::string_view::npos;

using Io = dk::Streams;
using Cp = std::uint32_t;
constexpr Cp kNone = 0xFFFFFFFFU;   // no code point: what precedes the start of a text
constexpr Dim kNoDim{0, 0, 0, 0};

// ------------------------------------------------------------------------------------------------------------------------------------------------------- code points

Cp decode(std::string_view s, std::size_t i, std::size_t& after) noexcept { return dk::code_point_at(s, i, after); }
bool is_digit(Cp c) noexcept { return dk::is_py_decimal(c); }
bool is_space(Cp c) noexcept { return dk::is_py_space(c); }
bool is_ascii_letter(Cp c) noexcept { return (c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z'); }
bool is_ascii_alnum(Cp c) noexcept { return is_ascii_letter(c) || (c >= U'0' && c <= U'9'); }

// `\d*` from i: the end of the run of decimal digits (of any script) that starts there, i itself when there is none.
std::size_t digits_end(std::string_view s, std::size_t i) {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!is_digit(decode(s, i, after))) break;
        i = after;
    }
    return i;
}

// `\s*` from i.
std::size_t skip_space(std::string_view s, std::size_t i) {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!is_space(decode(s, i, after))) break;
        i = after;
    }
    return i;
}

// The code point that ends at byte `end` (end > lower), where it starts and what it is; false when the bytes there are not one well-formed code point.
bool code_point_before(std::string_view s, std::size_t end, std::size_t lower, std::size_t& start, Cp& cp) noexcept {
    start = end - 1;
    while (start > lower && (static_cast<unsigned char>(s[start]) & 0xC0U) == 0x80U) --start;
    std::size_t after = 0;
    cp = decode(s, start, after);
    return after == end;
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------------------------------------------- scanners

namespace scan {

namespace {

// SUPERSCRIPT = str.maketrans("⁻⁰¹²³⁴⁵⁶⁷⁸⁹", "-0123456789"): the character a superscript sign or digit stands for, 0 for any other code point.
char superscript_char(Cp cp) noexcept {
    switch (cp) {
        case 0x207B: return '-';
        case 0x2070: return '0';
        case 0x00B9: return '1';
        case 0x00B2: return '2';
        case 0x00B3: return '3';
        case 0x2074: return '4';
        case 0x2075: return '5';
        case 0x2076: return '6';
        case 0x2077: return '7';
        case 0x2078: return '8';
        case 0x2079: return '9';
        default: return 0;
    }
}

// t.replace("−", "-").replace("×", "*").replace("µ", "u").replace("μ", "u").replace("–", "~").replace("—", " ") and the three white spaces to " ", in one pass (no target is a source).
std::string replace_characters(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        std::size_t after = 0;
        const Cp cp = decode(text, i, after);
        switch (cp) {
            case 0x2212: out.push_back('-'); break;                // MINUS SIGN
            case 0x00D7: out.push_back('*'); break;                // MULTIPLICATION SIGN
            case 0x00B5: case 0x03BC: out.push_back('u'); break;   // both micro signs
            case 0x2013: out.push_back('~'); break;                // EN DASH = a range
            case 0x2014: case 0x2009: case 0x00A0: case 0x202F: out.push_back(' '); break;   // EM DASH = prose; THIN SPACE, NO-BREAK SPACE, NARROW NO-BREAK SPACE
            default: out.append(text.substr(i, after - i));
        }
        i = after;
    }
    return out;
}

// re.sub(r"([A-Za-z0-9])([⁻⁰¹²³⁴⁵⁶⁷⁸⁹]+)", lambda m: m.group(1) + "^" + m.group(2).translate(SUPERSCRIPT), t): 10⁻¹³ -> 10^-13, s⁻¹ -> s^-1.
std::string fold_superscripts(std::string_view t) {
    std::string out;
    out.reserve(t.size());
    for (std::size_t i = 0; i < t.size();) {
        std::size_t after = 0;
        const Cp cp = decode(t, i, after);
        if (is_ascii_alnum(cp)) {
            std::string run;
            std::size_t j = after;
            while (j < t.size()) {
                std::size_t next = 0;
                const char c = superscript_char(decode(t, j, next));
                if (c == 0) break;
                run.push_back(c);
                j = next;
            }
            if (!run.empty()) {
                out.push_back(static_cast<char>(cp));
                out.push_back('^');
                out += run;
                i = j;
                continue;
            }
        }
        out.append(t.substr(i, after - i));
        i = after;
    }
    return out;
}

// re.sub(r"(?<=\d) (?=\d)", "", t): digit grouping, "1.495 978 707" -> "1.495978707".  The look-around is on the text as it was, not as it is being made.
std::string drop_digit_grouping(std::string_view t) {
    std::string out;
    out.reserve(t.size());
    Cp prev = kNone;
    for (std::size_t i = 0; i < t.size();) {
        std::size_t after = 0;
        const Cp cp = decode(t, i, after);
        if (cp == U' ' && is_digit(prev) && after < t.size()) {
            std::size_t next = 0;
            if (is_digit(decode(t, after, next))) {
                prev = cp;
                i = after;
                continue;
            }
        }
        out.append(t.substr(i, after - i));
        prev = cp;
        i = after;
    }
    return out;
}

// re.sub(r"(\d+(?:\.\d+)?)\s*\*\s*10\^(-?\d+)", r"\1e\2", t).  Scientific notation is written with the SAME multiplication sign as a genuine factor -- "1.495978707 * 10^11 m/AU" is one
// quantity, not two -- so it is folded into exponent form BEFORE the product is split on '*'.  Missing this made the Python read "10^-6 km" as the number 10 with a unit of "^-6 km".
std::string fold_scientific(std::string_view t) {
    std::string out;
    out.reserve(t.size());
    for (std::size_t i = 0; i < t.size();) {
        const std::size_t d1 = digits_end(t, i);
        if (d1 > i) {
            // The pattern's group 1 is \d+(\.\d+)?, and its optional fraction changes nothing here: the text before a match is copied as it stands, so a match that starts after the point
            // (at the fraction's digits) gives the same string as one that starts before the whole number ("2.5 * 10^-3" and "1.2.3 * 10^4" come out the same either way).
            const std::size_t m = d1;
            std::size_t k = skip_space(t, m);
            if (k < t.size() && t[k] == '*') {
                k = skip_space(t, k + 1);
                if (t.compare(k, 3, "10^") == 0) {
                    const std::size_t e = k + 3;
                    std::size_t g = e;
                    if (g < t.size() && t[g] == '-') ++g;
                    const std::size_t end = digits_end(t, g);
                    if (end > g) {
                        out.append(t.substr(i, m - i));
                        out.push_back('e');
                        out.append(t.substr(e, end - e));
                        i = end;
                        continue;
                    }
                }
            }
        }
        std::size_t after = 0;
        (void)decode(t, i, after);
        out.append(t.substr(i, after - i));
        i = after;
    }
    return out;
}

// re.sub(r"(?<![\d.eE])10\^(-?\d+)", r"1e\1", t): `10^-13` -> `1e-13`, not after a digit, a point or an e.
std::string fold_powers_of_ten(std::string_view t) {
    std::string out;
    out.reserve(t.size());
    Cp prev = kNone;
    for (std::size_t i = 0; i < t.size();) {
        std::size_t after = 0;
        const Cp cp = decode(t, i, after);
        if (cp == U'1' && t.compare(i, 3, "10^") == 0 && !(is_digit(prev) || prev == U'.' || prev == U'e' || prev == U'E')) {
            std::size_t g = i + 3;
            if (g < t.size() && t[g] == '-') ++g;
            const std::size_t end = digits_end(t, g);
            if (end > g) {
                out += "1e";
                out.append(t.substr(i + 3, end - (i + 3)));
                i = end;   // (what follows the match is no digit, the digits being taken greedily, so it cannot begin another `10^`: the look-behind there is the loop's own, as it comes)
                continue;
            }
        }
        out.append(t.substr(i, after - i));
        prev = cp;
        i = after;
    }
    return out;
}

}  // namespace

std::string normalise(std::string_view text) { return fold_powers_of_ten(fold_scientific(drop_digit_grouping(fold_superscripts(replace_characters(text))))); }

// EXPR = re.compile(r"([^|=]+?)\s*=\s*\*\*([^*]+)\*\*"), findall.  At a start position the lazy group cannot pass the first `|` or `=` at or after it, so the `=` that follows the group is that
// one (a `|` there means no match from any start up to it); the group is the shortest run of at least one character that leaves only white space before the `=`; after the `=` the rest is
// deterministic: white space, `**`, one or more characters that are not `*`, `**`.
std::vector<std::pair<std::string, std::string>> expressions(std::string_view text) {
    std::vector<std::pair<std::string, std::string>> found;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t p = text.find_first_of("|=", i);
        if (p == kNpos) break;
        if (text[p] == '|' || p == i) {   // no start in [i, p] can match: the group needs a first character that is neither, and may not cross a `|`
            i = p + 1;
            continue;
        }
        const std::size_t q = skip_space(text, p + 1);
        const std::size_t body = q + 2;
        const std::size_t star = text.compare(q, 2, "**") == 0 ? text.find('*', body) : kNpos;
        if (star == kNpos || star == body || text.compare(star, 2, "**") != 0) {
            i = p + 1;   // the same failure for every start before the `=`
            continue;
        }
        // the group: text[i, j), j the smallest end of at least one character that leaves only white space before the `=`
        std::size_t w = p;
        while (w > i) {
            std::size_t start = 0;
            Cp cp = 0;
            if (!code_point_before(text, w, i, start, cp) || !is_space(cp)) break;
            w = start;
        }
        std::size_t after_first = 0;
        (void)decode(text, i, after_first);
        const std::size_t j = std::max(w, after_first);
        found.emplace_back(std::string(text.substr(i, j - i)), std::string(text.substr(body, star - body)));
        i = star + 2;
    }
    return found;
}

// PROSE = re.compile(r"^(?P<prose>[^\d]*?[A-Za-z][^\d]*?)(?P<rest>[-+]?\s*\d.*)$", re.S), match.  Prose holds no digit, so `rest` starts at or before the FIRST digit; it is the first digit's own
// sign and the white space before it, so the prose ends where those begin -- and has to hold an ASCII letter, which (a letter being neither sign nor white space) is any before that digit.
std::pair<std::string, std::string> strip_prose_prefix(std::string_view lhs) {
    std::size_t first_digit = kNpos;
    bool letter = false;
    for (std::size_t i = 0; i < lhs.size();) {
        std::size_t after = 0;
        const Cp cp = decode(lhs, i, after);
        if (is_digit(cp)) {
            first_digit = i;
            break;
        }
        letter = letter || is_ascii_letter(cp);
        i = after;
    }
    if (first_digit == kNpos || !letter) return {std::string(), std::string(lhs)};
    std::size_t r = first_digit;   // where the white space before the digit begins
    while (r > 0) {
        std::size_t start = 0;
        Cp cp = 0;
        if (!code_point_before(lhs, r, 0, start, cp) || !is_space(cp)) break;
        r = start;
    }
    const std::size_t s = r > 0 && (lhs[r - 1] == '+' || lhs[r - 1] == '-') ? r - 1 : r;
    return {dk::strip_py(lhs.substr(0, s)), dk::strip_py(lhs.substr(s))};
}

// row_re = re.compile(r"^\|\s*`([A-Z][A-Z0-9]*-P-[0-9a-z]+)`\s*\|"), match.
std::optional<std::string> row_id(std::string_view line) {
    if (line.empty() || line[0] != '|') return std::nullopt;
    std::size_t i = skip_space(line, 1);
    if (i >= line.size() || line[i] != '`') return std::nullopt;
    const std::size_t id_start = ++i;
    if (i >= line.size() || !(line[i] >= 'A' && line[i] <= 'Z')) return std::nullopt;
    ++i;
    while (i < line.size() && ((line[i] >= 'A' && line[i] <= 'Z') || (line[i] >= '0' && line[i] <= '9'))) ++i;
    if (line.compare(i, 3, "-P-") != 0) return std::nullopt;
    i += 3;
    const std::size_t tail = i;
    while (i < line.size() && ((line[i] >= '0' && line[i] <= '9') || (line[i] >= 'a' && line[i] <= 'z'))) ++i;
    if (i == tail || i >= line.size() || line[i] != '`') return std::nullopt;
    std::string id(line.substr(id_start, i - id_start));
    i = skip_space(line, i + 1);
    if (i >= line.size() || line[i] != '|') return std::nullopt;
    return id;
}

}  // namespace scan

// ------------------------------------------------------------------------------------------------------------------------------------------------------- arithmetic

namespace {

// int(text) as parse_unit and parse_number use it: an optional sign, then decimal digits of any script; anything else is the ValueError the Python raised, in its words -- EXCEPT a digit
// separator, which Python's int() reads (`1_0` is 10) and this tool REFUSES (the maintainer's ruling of 2026-10-07: a silent misreading of an exponent; plan constraint 4, refuse rather than
// approximate).  A magnitude above a million is refused (deliberate difference: the Python's integers have no limit, this tool's are 64 bits).
long long py_int(std::string_view text) {
    const auto invalid = [&] { return ParseError("invalid literal for int() with base 10: " + dk::py_repr(text)); };
    constexpr long long kLimit = 1'000'000;
    std::size_t i = 0;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        negative = text[i] == '-';
        ++i;
    }
    long long value = 0;
    bool any_digit = false;
    bool too_big = false;
    while (i < text.size()) {
        std::size_t after = 0;
        const Cp cp = decode(text, i, after);
        const int digit = dk::py_decimal_value(cp);
        if (digit >= 0) {
            if (!too_big) {
                value = value * 10 + digit;
                too_big = value > kLimit;
            }
            any_digit = true;
        } else if (cp == U'_') {
            throw ParseError("a digit separator in " + dk::py_repr(text) + " is not read: an exponent is digits only");
        } else {
            throw invalid();
        }
        i = after;
    }
    if (!any_digit) throw invalid();
    if (too_big) throw ParseError("exponent out of range: " + dk::py_repr(text));
    return negative ? -value : value;
}

// float(text) for a decimal number: [sign] digits [. digits] [e [sign] digits], the digits of any script.  Out of double range it is inf or 0, as in the Python.
double py_float(std::string_view text) {
    std::string ascii;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t after = 0;
        const Cp cp = decode(text, i, after);
        const int digit = dk::py_decimal_value(cp);
        if (digit >= 0) ascii.push_back(static_cast<char>('0' + digit));
        else ascii.append(text.substr(i, after - i));
        i = after;
    }
    const auto is_ascii_digit = [](char c) { return c >= '0' && c <= '9'; };
    std::size_t i = 0;
    if (i < ascii.size() && (ascii[i] == '+' || ascii[i] == '-')) ++i;
    const std::size_t whole = i;
    while (i < ascii.size() && is_ascii_digit(ascii[i])) ++i;
    bool ok = i > whole;
    if (i < ascii.size() && ascii[i] == '.') {
        ++i;
        const std::size_t fraction = i;
        while (i < ascii.size() && is_ascii_digit(ascii[i])) ++i;
        ok = ok || i > fraction;
    }
    if (ok && i < ascii.size() && (ascii[i] == 'e' || ascii[i] == 'E')) {
        ++i;
        if (i < ascii.size() && (ascii[i] == '+' || ascii[i] == '-')) ++i;
        const std::size_t exponent = i;
        while (i < ascii.size() && is_ascii_digit(ascii[i])) ++i;
        ok = i > exponent;
    }
    if (!ok || i != ascii.size()) throw ParseError("could not convert string to float: " + dk::py_repr(text));
    return std::strtod(ascii.c_str(), nullptr);
}

// 10.0 ** n.  In range it is pow's value; out of range the Python raised OverflowError (a traceback), and this is a finding.
double power_of_ten(long long n) {
    const double r = std::pow(10.0, static_cast<double>(n));
    if (std::isinf(r)) throw ParseError("10^" + std::to_string(n) + " is out of range");
    return r;
}

// UNITS: symbol -> (SI scale, dimension as (length, time, angle, MASS)).  MASS was added at L3 step 1 (SPEC-dynamics DYN-P-3 and DYN-P-4 compare solar radiation pressure's velocity derivative against
// drag's, written in N m^-2 and kg m^-3): the checker REFUSED them rather than guessing, which is the behaviour this tool exists for, and the repair was to teach it the units this tree's physics uses.
const std::map<std::string, std::pair<double, Dim>>& units() {
    static const double arcsec = std::numbers::pi / (180.0 * 3600.0);
    static const Dim length{1, 0, 0, 0};
    static const Dim time{0, 1, 0, 0};
    static const Dim angle{0, 0, 1, 0};
    static const Dim mass{0, 0, 0, 1};
    static const std::map<std::string, std::pair<double, Dim>> table = {
        {"m", {1.0, length}},      {"km", {1e3, length}},     {"mm", {1e-3, length}},  {"um", {1e-6, length}},   {"nm", {1e-9, length}},
        {"pm", {1e-12, length}},   {"fm", {1e-15, length}},   {"AU", {1.495978707e11, length}},
        {"s", {1.0, time}},        {"ms", {1e-3, time}},      {"us", {1e-6, time}},    {"ns", {1e-9, time}},     {"ps", {1e-12, time}},
        {"fs", {1e-15, time}},     {"rad", {1.0, angle}},     {"as", {arcsec, angle}}, {"mas", {arcsec * 1e-3, angle}}, {"uas", {arcsec * 1e-6, angle}},
        {"1", {1.0, kNoDim}},   // (the Python also had "", reachable only through a bare power such as `^+3`, which is refused now; the empty UNIT is read before the table)
        {"kg", {1.0, mass}},       {"g", {1e-3, mass}},       {"N", {1.0, Dim{1, -2, 0, 1}}},
        {"J", {1.0, Dim{2, -2, 0, 1}}},                       {"W", {1.0, Dim{2, -3, 0, 1}}},
    };
    return table;
}

std::string dim_text(const Dim& d) {   // a Python tuple of four ints
    return "(" + std::to_string(d[0]) + ", " + std::to_string(d[1]) + ", " + std::to_string(d[2]) + ", " + std::to_string(d[3]) + ")";
}

// NUM = r"[-+]?\d+(?:\.\d+)?(?:\s*\*\s*10\^-?\d+|[eE][-+]?\d+)?" matched at t[i]: the end of the match, kNpos when there is none.  Every choice is deterministic: a sign, the digits, a fraction
// when a point is followed by a digit, then the star form of the exponent if it is all there, else the e form if it is all there.
std::size_t match_number(std::string_view t, std::size_t i) {
    std::size_t j = i;
    if (j < t.size() && (t[j] == '+' || t[j] == '-')) ++j;
    const std::size_t digits = digits_end(t, j);
    if (digits == j) return kNpos;
    j = digits;
    if (j < t.size() && t[j] == '.') {
        const std::size_t fraction = digits_end(t, j + 1);
        if (fraction > j + 1) j = fraction;
    }
    std::size_t k = skip_space(t, j);
    if (k < t.size() && t[k] == '*') {
        k = skip_space(t, k + 1);
        if (t.compare(k, 3, "10^") == 0) {
            std::size_t m = k + 3;
            if (m < t.size() && t[m] == '-') ++m;
            const std::size_t end = digits_end(t, m);
            if (end > m) return end;
        }
    }
    if (j < t.size() && (t[j] == 'e' || t[j] == 'E')) {
        std::size_t m = j + 1;
        if (m < t.size() && (t[m] == '+' || t[m] == '-')) ++m;
        const std::size_t end = digits_end(t, m);
        if (end > m) return end;
    }
    return j;
}

}  // namespace

Quantity operator*(const Quantity& a, const Quantity& b) {
    return Quantity{a.value * b.value, Dim{a.dim[0] + b.dim[0], a.dim[1] + b.dim[1], a.dim[2] + b.dim[2], a.dim[3] + b.dim[3]}};
}

Quantity parse_unit(std::string_view input) {
    const std::string u = dk::strip_py(input);
    Quantity q{1.0, kNoDim};
    if (u.empty()) return q;
    const std::size_t slash = u.find('/');
    const std::string numerator = u.substr(0, slash);
    const std::string denominator = slash == std::string::npos ? std::string() : u.substr(slash + 1);
    const auto apply = [&](const std::string& part, long long sign) {
        for (const std::string& token : dk::split_py(part)) {
            const std::size_t caret = token.find('^');
            const std::string symbol = token.substr(0, caret);
            const std::string exponent = caret == std::string::npos ? std::string() : token.substr(caret + 1);
            // REFUSED since the maintainer's ruling of 2026-10-07, each a silent misreading in the Python: a power with no unit before it (`^+3` was read as the dimensionless "" cubed, so
            // `10^+3` was 10), and a unit symbol with `^` and no exponent (`m^` was read as `m`).
            if (caret != std::string::npos && symbol.empty()) throw ParseError("a power with no unit before it: " + dk::py_repr(token) + " (a power of ten is written 10^3 or 10^-3)");
            if (caret != std::string::npos && exponent.empty()) throw ParseError("no exponent after '^' in " + dk::py_repr(token));
            const long long e = caret == std::string::npos ? 1 : py_int(exponent);   // BEFORE the symbol is looked up, as in the Python
            const auto found = units().find(symbol);
            if (found == units().end()) throw ParseError("unknown unit " + dk::py_repr(symbol) + " in " + dk::py_repr(u));
            const long long power = e * sign;
            const double factor = std::pow(found->second.first, static_cast<double>(power));
            if (std::isinf(factor)) throw ParseError("a unit raised to " + std::to_string(power) + " is out of range: " + dk::py_repr(token));
            const Dim& dim = found->second.second;
            q = q * Quantity{factor, Dim{dim[0] * power, dim[1] * power, dim[2] * power, dim[3] * power}};
        }
    };
    apply(numerator, 1);
    apply(denominator, -1);
    return q;
}

double parse_number(std::string_view input) {
    std::string s;
    for (const char c : dk::strip_py(input)) {
        if (c != ' ') s.push_back(c);   // s.strip().replace(" ", "")
    }
    {   // re.fullmatch(r"([-+]?\d+(?:\.\d+)?)\*10\^(-?\d+)", s)
        std::size_t i = 0;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        const std::size_t digits = digits_end(s, i);
        if (digits > i) {
            std::size_t j = digits;
            if (j < s.size() && s[j] == '.') {
                const std::size_t fraction = digits_end(s, j + 1);
                if (fraction > j + 1) j = fraction;
            }
            if (s.compare(j, 4, "*10^") == 0) {
                const std::size_t k = j + 4;
                std::size_t m = k;
                if (m < s.size() && s[m] == '-') ++m;
                const std::size_t end = digits_end(s, m);
                if (end > m && end == s.size()) {
                    const double mantissa = py_float(std::string_view(s).substr(0, j));
                    return mantissa * power_of_ten(py_int(std::string_view(s).substr(k)));
                }
            }
        }
    }
    if (s.compare(0, 3, "10^") == 0) {   // re.fullmatch(r"10\^(-?\d+)", s)
        std::size_t m = 3;
        if (m < s.size() && s[m] == '-') ++m;
        const std::size_t end = digits_end(s, m);
        if (end > m && end == s.size()) return power_of_ten(py_int(std::string_view(s).substr(3)));
    }
    return py_float(s);
}

Term parse_term(std::string_view input) {
    // t.strip().lstrip("<≤≈~ ").strip()
    const std::string t = dk::strip_py(dk::lstrip_chars_py(dk::strip_py(input), "<\xE2\x89\xA4\xE2\x89\x88~ "));
    // re.match(rf"^({NUM})(?:~({NUM}))?\s*(.*)$", t) -- `(.*)$` takes what is left, which may hold no newline (`.` does not take one and `$` is the end of the text: t was stripped, so
    // there is no final newline to be before)
    const std::size_t first_end = match_number(t, 0);
    if (first_end == kNpos) throw ParseError("cannot read a quantity from " + dk::py_repr(t));
    std::size_t pos = first_end;
    std::string_view second;
    bool has_second = false;
    if (pos < t.size() && t[pos] == '~') {
        const std::size_t second_end = match_number(t, pos + 1);
        if (second_end != kNpos) {
            second = std::string_view(t).substr(pos + 1, second_end - (pos + 1));
            has_second = true;
            pos = second_end;
        }
    }
    pos = skip_space(t, pos);
    const std::string_view rest = std::string_view(t).substr(pos);
    if (rest.find('\n') != kNpos) throw ParseError("cannot read a quantity from " + dk::py_repr(t));
    Term term;
    term.values.push_back(parse_number(std::string_view(t).substr(0, first_end)));
    if (has_second) term.values.push_back(parse_number(second));
    term.unit = parse_unit(rest);
    return term;
}

double significant_tolerance(std::string_view text, double value) {
    std::string t;
    for (const char c : text) {
        if (c != ' ') t.push_back(c);   // text.replace(" ", "")
    }
    // re.search(r"(\d+(?:\.\d+)?)", t): the first run of digits and, if a point and a digit follow, the fraction; "1" when there is none
    std::string mantissa = "1";
    for (std::size_t i = 0; i < t.size();) {
        std::size_t after = 0;
        if (is_digit(decode(t, i, after))) {
            std::size_t end = digits_end(t, i);
            if (end < t.size() && t[end] == '.') {
                const std::size_t fraction = digits_end(t, end + 1);
                if (fraction > end + 1) end = fraction;
            }
            mantissa = t.substr(i, end - i);
            break;
        }
        i = after;
    }
    // digits = mantissa.replace(".", "").lstrip("0") or "0"; sig = max(len(digits), 1)
    std::string digits;
    for (const char c : mantissa) {
        if (c != '.') digits.push_back(c);
    }
    std::size_t zeros = 0;
    while (zeros < digits.size() && digits[zeros] == '0') ++zeros;
    digits.erase(0, zeros);
    if (digits.empty()) digits = "0";
    const std::size_t sig = std::max<std::size_t>(dk::code_points(digits), 1);
    if (value == 0.0) return 0.5;
    if (!std::isfinite(value)) throw ParseError("the stated value is out of range");   // the Python's math.floor of an infinity: OverflowError, a traceback
    const double exponent = std::floor(std::log10(std::fabs(value)));
    return 0.5 * std::pow(10.0, exponent - static_cast<double>(sig - 1)) * 1.001;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------- one row

RowOutcome check_row(const std::string& rid, std::string_view row, bool quiet, std::ostream& out) {
    RowOutcome outcome;
    const std::string text = scan::normalise(row);
    const std::vector<std::pair<std::string, std::string>> found = scan::expressions(text);
    if (found.empty()) {
        outcome.kind = "no-arithmetic";
        return outcome;
    }

    for (const auto& [raw_lhs, rhs] : found) {
        // lhs.strip().lstrip(";, ").strip()
        std::string lhs = dk::strip_py(dk::lstrip_chars_py(dk::strip_py(raw_lhs), ";, "));
        auto [prose, rest] = scan::strip_prose_prefix(lhs);
        lhs = rest;
        if (!prose.empty() && !quiet) out << "note     " << dk::pad_right(rid, 14) << " prose prefix ignored: " << dk::py_repr(prose) << '\n';

        std::vector<Quantity> lefts;
        std::vector<double> rvals;
        Quantity runit;
        try {
            // factors = [parse_term(p) for p in lhs.split("*") if p.strip()]
            std::vector<Term> factors;
            for (std::size_t from = 0;;) {
                const std::size_t star = lhs.find('*', from);
                const std::string piece = lhs.substr(from, star == std::string::npos ? std::string::npos : star - from);
                if (!dk::strip_py(piece).empty()) factors.push_back(parse_term(piece));
                if (star == std::string::npos) break;
                from = star + 1;
            }
            if (factors.empty()) throw ParseError("no quantity on the left");   // the Python's max() of an empty list: its own words are interpreter-version text
            // A range on the left pairs with the range on the right, elementwise.
            std::size_t n = 0;
            for (const Term& f : factors) n = std::max(n, f.values.size());
            for (std::size_t i = 0; i < n; ++i) {
                Quantity q{1.0, kNoDim};
                for (const Term& f : factors) q = q * Quantity{f.values[f.values.size() > 1 ? i : 0], kNoDim} * f.unit;
                lefts.push_back(q);
            }
            // REFUSED since the maintainer's ruling of 2026-10-07: a left side that is NaN (an infinity times zero) passed in the Python, because no comparison with NaN is true; a check never passes on it
            for (const Quantity& q : lefts) {
                if (std::isnan(q.value)) throw ParseError("the left side evaluates to NaN (a check never passes on it)");
            }
            const Term right = parse_term(rhs);
            rvals = right.values;
            runit = right.unit;
            if (runit.value == 0.0) throw ParseError("the stated unit's scale is zero");   // the Python divided by it: ZeroDivisionError, a traceback
            for (const double v : rvals) {
                if (!std::isfinite(v)) throw ParseError("the stated value is out of range");   // the Python's tolerance: OverflowError on an infinity, a traceback
            }
        } catch (const ParseError& exc) {
            outcome.problems.push_back(rid + ": UNPARSEABLE " + dk::py_repr(lhs) + " = " + dk::py_repr(rhs) + ": " + exc.what());
            continue;
        }

        if (rvals.size() != lefts.size()) {
            outcome.problems.push_back(rid + ": " + std::to_string(lefts.size()) + " value(s) on the left, " + std::to_string(rvals.size()) + " on the right");
            continue;
        }
        for (std::size_t i = 0; i < lefts.size(); ++i) {
            const Quantity& left = lefts[i];
            const double rv = rvals[i];
            const double tol = significant_tolerance(rhs, rv);
            const Quantity right = Quantity{rv, kNoDim} * runit;
            if (left.dim != right.dim) {
                outcome.problems.push_back(rid + ": dimensions differ \xE2\x80\x94 left " + dim_text(left.dim) + ", right " + dim_text(right.dim) + "  (" + lhs + " = " + rhs + ")");
                continue;
            }
            // Compare in the units the row states, so the tolerance means what the printed digits mean.
            const double got_in_units = left.value / runit.value;
            if (std::fabs(got_in_units - rv) > tol) {
                if (rv != 0.0) {
                    outcome.problems.push_back(rid + ": ARITHMETIC WRONG\n      stated   " + dk::strip_py(rhs) + "\n      computed " + dk::py_format_g(got_in_units, 6) + " (same units)\n      from     " +
                                               lhs + "\n      out by a factor of " + dk::py_format_g(got_in_units / rv, 4));
                } else {
                    outcome.problems.push_back(rid + ": stated zero");
                }
            } else if (!quiet) {
                out << "ok       " << dk::pad_right(rid, 14) << " " << lhs << " = " << dk::py_format_g(rv) << " (" << dk::py_format_g(got_in_units, 6) << " computed)\n";
            }
        }
    }
    outcome.kind = outcome.problems.empty() ? "checked" : "problem";
    return outcome;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------- the command line

namespace {

const char kUsageText[] = "usage: budgetcheck [-h] [--spec-dir SPEC_DIR] [--quiet]\n";

const char kHelpText[] =
    "\n"
    "evaluate the arithmetic in every specification budget row: each `... = **result**` is parsed and its left side computed and compared with the stated result, to half a unit of\n"
    "the last significant digit stated.  A row that looks like arithmetic and does not parse is a failure, never a skip.\n"
    "\n"
    "options:\n"
    "  -h, --help            show this help and exit\n"
    "  --spec-dir SPEC_DIR   the specifications, SPEC-*.md (default: <tree>/spec)\n"
    "  --quiet               print the totals and the verdict, not each row\n"
    "\n"
    "exit codes: 0 all rows check   1 a row is wrong   2 a row would not parse\n";

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
        std::string spec_dir = (default_root() / "spec").string();
        bool quiet = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kUnparseable;   // argparse exits 2
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
            if (opt == "--spec-dir") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument --spec-dir: expected one argument");
                    value = argv[++i];
                }
                spec_dir = value;
            } else if (opt == "--quiet" && !has_value) {
                quiet = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        // sorted(a.spec_dir.glob("SPEC-*.md")): a directory that is not there, or holds none, is no error in the Python either (kept, and reported: see the header)
        std::vector<std::string> names;
        std::error_code ec;
        if (fs::is_directory(fs::path(spec_dir), ec)) {
            for (const fs::directory_entry& entry : fs::directory_iterator(fs::path(spec_dir), fs::directory_options::skip_permission_denied, ec)) {
                const std::string name = entry.path().filename().string();
                if (name.size() >= 8 && name.rfind("SPEC-", 0) == 0 && name.compare(name.size() - 3, 3, ".md") == 0) names.push_back(name);
            }
        }
        std::sort(names.begin(), names.end());
        // REFUSED since the maintainer's ruling of 2026-10-07, as speccheck refuses it: a directory that is absent, or holds no SPEC-*.md, is success over nothing read (the Python passed it with "0
        // budget rows"), which is the failure this tool exists against.  A directory whose specifications hold no budget row passes: it was read.
        if (names.empty()) {
            io.err << kTool << ": no SPEC-*.md in " << spec_dir << '\n';
            return kUnparseable;
        }

        std::size_t rows = 0, checked = 0, none = 0;
        std::vector<std::string> all_problems;
        for (const std::string& name : names) {
            std::string text;
            try {
                text = dk::universal_newlines(dk::read_text(fs::path(spec_dir) / name));
            } catch (const std::runtime_error& exc) {
                io.err << "REFUSED  " << name << ": " << exc.what() << '\n';
                return kUnparseable;
            }
            for (const std::string& line : dk::splitlines_py(text)) {
                const std::optional<std::string> id = scan::row_id(line);
                if (!id) continue;
                ++rows;
                RowOutcome outcome = check_row(*id, line, quiet, io.out);
                for (std::string& problem : outcome.problems) all_problems.push_back(std::move(problem));
                if (outcome.kind == "checked") ++checked;
                else if (outcome.kind == "no-arithmetic") ++none;
            }
        }

        io.out << "\n" << rows << " budget rows: " << checked << " with arithmetic, checked; " << none << " with none.\n";
        if (rows != checked + none) io.out << "  " << (rows - checked - none) << " row(s) neither \xE2\x80\x94 see the failures below.\n";

        if (!all_problems.empty()) {
            io.err << '\n';
            bool unparseable = false;
            for (const std::string& problem : all_problems) {
                io.err << "FAILED   " << problem << '\n';
                unparseable = unparseable || problem.find("UNPARSEABLE") != std::string::npos;
            }
            io.err << "\n" << all_problems.size() << " problem(s). A row that looks like arithmetic and does not parse is a FAILURE, never a skip:\n"
                   << "  a checker that silently skipped them would report success over the rows it\n"
                   << "  understood and say nothing about the rest, which is the defect this tool\n"
                   << "  exists to end rather than to join.\n";
            return unparseable ? kUnparseable : kWrong;
        }

        io.out << "ok       every budget row's arithmetic evaluates to what it states\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::budgetcheck

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::budgetcheck::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
