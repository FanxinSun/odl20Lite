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
//     capital I with a dot above (U+0130, with i) and the dotless i (U+0131, with i).  (The first draft of this port named only the Kelvin sign and the dotless i.)
//   * `\w`, `\d`, `\s` and `\b` are Unicode-aware as Python's are, within the limit odl/devkit/pytext.hpp states.  The messages name `unitcheck`; argparse's abbreviations are not accepted and `-h`
//     prints this tool's own text.  An option's value is whatever follows it unless that begins with `--` (argparse refuses one that begins with a single dash too, so a directory called -x is a
//     value here), and `-h=x` is refused.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "unitcheck.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
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

// [eE][+-]?\d+ at p: the end of the exponent, kNpos when there is none
std::size_t exponent_end(std::string_view s, std::size_t p) {
    if (p >= s.size() || (s[p] != 'e' && s[p] != 'E')) return kNpos;
    std::size_t q = p + 1;
    if (q < s.size() && (s[q] == '+' || s[q] == '-')) ++q;
    const std::size_t end = digits_end(s, q);
    return end > q ? end : kNpos;
}

// NUMBER matched at i, the end of the match or kNpos.  `\d+\.?\d*(?:[eE][+-]?\d+)?` followed by a lookahead that only the full-length candidates can satisfy: a shorter run of digits is followed by a digit
// (a word character), and a point that is not taken is followed by the point.  So the candidates, in the engine's order, are: after `D.F`, the exponent first and then none; after `D` with no point, the
// exponent first when an e follows (the bare `D` is followed by the e, a word character), else `D` alone.
std::size_t number_end_at(std::string_view s, std::size_t i) {
    if (!lookbehind_ok(s, i)) return kNpos;
    const std::size_t d_end = digits_end(s, i);
    if (d_end == i) return kNpos;
    if (d_end < s.size() && s[d_end] == '.') {
        const std::size_t f_end = digits_end(s, d_end + 1);
        const std::size_t with_exponent = exponent_end(s, f_end);
        if (with_exponent != kNpos && lookahead_ok(s, with_exponent)) return with_exponent;
        if (lookahead_ok(s, f_end)) return f_end;
        return kNpos;
    }
    const std::size_t with_exponent = exponent_end(s, d_end);
    if (with_exponent != kNpos) return lookahead_ok(s, with_exponent) ? with_exponent : kNpos;
    return lookahead_ok(s, d_end) ? d_end : kNpos;
}

// the digits of any script as ASCII, for strtod
std::string ascii_digits(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size();) {
        std::size_t after = 0;
        const Cp cp = dk::code_point_at(s, i, after);
        const int digit = dk::py_decimal_value(cp);
        if (digit >= 0) out.push_back(static_cast<char>('0' + digit));
        else out.append(s.substr(i, after - i));
        i = after;
    }
    return out;
}

// the case-insensitive match of one code point with the ASCII letter `c`, as re.I reads it for the letters of "kilomet" and "km": the text's character is lower-cased (Python's sre takes the first
// code point of the full lower-case mapping) and compared with the pattern's letter, and the letters i and s have one extra equivalent each (the dotless i and the long s).  The non-ASCII characters
// that come out equal to one of these ASCII letters are exactly three (every code point was mapped, with Perl's lc as the witness): the Kelvin sign (to k), the capital I with a dot above (to i, the
// first of the two code points of its full lower case) and the dotless i (the extra equivalent of i).
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

}  // namespace

namespace scan {

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
        const std::size_t end = number_end_at(code, i);
        if (end != kNpos) {
            tokens.emplace_back(code.substr(i, end - i));
            i = end;
        } else {
            std::size_t after = 0;
            (void)dk::code_point_at(code, i, after);
            i = after;
        }
    }
    return tokens;
}

bool is_thousand(std::string_view token) {
    const std::string ascii = ascii_digits(token);
    char* end = nullptr;
    const double v = std::strtod(ascii.c_str(), &end);
    if (end != ascii.c_str() + ascii.size()) return false;   // float(tok) raised ValueError
    return v == 1000.0 || v == 0.001;
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
    "UNIT-CROSSING that names kilometres anywhere but core/units.hpp is refused.\n"
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

        std::vector<Row> home, crossings, declared, unaccounted, misplaced;
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
            for (std::size_t index = 0; index < lines.size(); ++index) {
                const std::string_view line = lines[index];
                const std::string_view prev = index >= 1 ? lines[index - 1] : std::string_view();
                const std::string stripped = dk::lstrip_py(line);
                if (stripped.rfind("//", 0) == 0 || stripped.rfind("*", 0) == 0 || stripped.rfind("/*", 0) == 0) continue;
                const std::size_t slashes = line.find("//");
                const std::string_view code = slashes == kNpos ? line : line.substr(0, slashes);
                std::vector<std::string> found;
                for (std::string& token : scan::number_tokens(code)) {
                    if (scan::is_thousand(token)) found.push_back(std::move(token));
                }
                if (found.empty()) continue;
                std::optional<scan::Marker> mark = scan::marker(line);
                if (!mark && dk::lstrip_py(prev).rfind("//", 0) == 0) mark = scan::marker(prev);
                for (const std::string& literal : found) {
                    Row row{rel, index + 1, literal, dk::strip_py(line), std::string()};
                    if (rel == kCrossingHome) {
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
        }

        // THE DENOMINATOR.  The last number is the gate; the others make it legible.
        io.out << "\n  production sources searched     " << right(srcs.size(), 5) << '\n';
        const std::size_t n = home.size() + crossings.size() + declared.size() + unaccounted.size() + misplaced.size();
        io.out << "  literals of value 1000 or 1/1000 " << right(n, 5) << '\n';
        io.out << "    in " << dk::pad_right(kCrossingHome, 44) << ' ' << right(home.size(), 5) << '\n';
        io.out << "    annotated UNIT-CROSSING                        " << right(crossings.size(), 5) << '\n';
        io.out << "    annotated NOT-A-UNIT-CROSSING                  " << right(declared.size(), 5) << '\n';
        io.out << "    ACCOUNTED FOR BY NEITHER                       " << right(unaccounted.size(), 5) << '\n';
        io.out << "    naming km OUTSIDE core/units.hpp               " << right(misplaced.size(), 5) << '\n';

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
