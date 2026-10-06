// speccheck.cpp — the specification coverage checker.
//
// Plan L0 step 6 wrote it; plan L0 step 8 (group C3, the user's directive of 2026-10-06) ported it from tools/speccheck.py.  Every requirement and refusal in an adopted
// specification is discharged by an acceptance test or individually excused, and this checks it.
//
// THE DENOMINATOR IS THE WHOLE POINT.  A hand audit of the L1 specifications counted 121 requirements and refusals; a naive count of the same files gave 128, because the
// specifications cross-cite each other's identifiers by design and the naive count swept in the references.  So the denominator is OWN-PREFIX identifiers only (the prefix is
// read from each file's own `Spec ID` front-matter field, never guessed from the filename), a file with no declared Spec ID is REFUSED, and BOTH counts are printed on every
// run, labelled, with the difference named.
//
// SCOPE.  This reads the SPECIFICATIONS' internal traceability: is every requirement and refusal discharged by an acceptance ROW, or individually excused?  It does not look at the
// test suite and cannot tell whether those rows are implemented.  An adopted-but-unbuilt specification passes.  It also scans every `.cpp` of the tree for the same `TEST_CASE`
// identifier claimed twice, and refuses the same identifier defined in two specification files.
//
//   speccheck [--spec-dir DIR] [--cpp-root DIR] [--skip-test-case-check] [--quiet]
//   exit 0 complete   1 gaps found   2 an argument error   3 a specification could not be parsed   70 an error the tool did not anticipate
//
// THE PROOF.  The tool's product is what it PRINTS, and the committed record of what speccheck.py printed on this tree is gate 7's section of the safety-net run (the 272 lines
// of ci_acac318_safety_net.log, kept with the L0-8 report): the port prints those bytes, every one, on an export of the tree made at the path that run recorded (the report has the
// list of substitutions registered before the comparison: there are none).  The Python is not run beside it.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * SCOPE FIX (plan L0-8, ruling D12 a): the tree-wide scan for TEST_CASE claims does not read the root's own `build*` directories or its `data/`.  The Python read every `.cpp`
//     under the root, so a build tree inside it (a CMake build's `_deps` holds other projects' sources) was part of what "the tree" meant.
//   * A specification that cannot be read as UTF-8 is REFUSED, exit 3 (the Python raised UnicodeDecodeError, a traceback, exit 1).  A `.cpp` that cannot is skipped, as before.
//   * A "(partial" Coverage row that names a requirement no acceptance row discharges is REPORTED: one named finding per requirement (the spec, the id, the row's own text), exit 1
//     (the manager's ruling of 2026-10-07, plan L0 step 8 group C4: a DELIBERATE change of behaviour, not a port).  The Python's partition `assert` crashed on it -- a traceback, exit 1 --
//     and the C3 port kept that as one line.  The partition check stays as an internal guard (`partitions`, below); by construction it cannot fire on any input now.
//   * argparse's abbreviations (`--qu`) are not accepted, and `-h` prints this tool's own text and not the Python module's docstring.
//   * "no SPEC-*.md" names `speccheck`, not `speccheck.py`.
//   * `\w`, `\s`, `\d` and `\b` are Unicode-aware as Python's are, within the limit odl/devkit/pytext.hpp states (a code point it does not know is not a word character).
//   * A range of identifiers whose numbers have more than 18 digits (`R-001…R-9999999999999999999`) is refused; the Python would have looped over it.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "speccheck.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>

namespace odl::tools::speccheck {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "speccheck";
constexpr int kOk = 0, kGaps = 1, kArgument = 2, kUnparseable = 3, kInternal = 70;
constexpr std::size_t kNpos = std::string_view::npos;

using Io = dk::Streams;

}  // namespace

// ------------------------------------------------------------------------------------------------------------------------------------------------------- scanners

namespace scan {
namespace {

using Cp = std::uint32_t;

// The code point at byte i of well-formed UTF-8 and the index after it.  Bytes that are not UTF-8 are one U+FFFD each: no word, no digit, no blank.
Cp decode(std::string_view s, std::size_t i, std::size_t& after) noexcept {
    Cp cp = 0;
    std::size_t j = i;
    if (dk::next_code_point(s, j, cp)) {
        after = j;
        return cp;
    }
    after = i + 1;
    return 0xFFFD;
}

std::size_t prev_start(std::string_view s, std::size_t i) noexcept {   // i > 0
    std::size_t j = i - 1;
    while (j > 0 && (static_cast<unsigned char>(s[j]) & 0xC0U) == 0x80U) --j;
    return j;
}

bool word_before(std::string_view s, std::size_t i) noexcept {
    if (i == 0) return false;
    std::size_t after = 0;
    return dk::is_py_word(decode(s, prev_start(s, i), after));
}

bool word_at(std::string_view s, std::size_t i) noexcept {
    if (i >= s.size()) return false;
    std::size_t after = 0;
    return dk::is_py_word(decode(s, i, after));
}

// Python's `\b`: the position between a word character and a non-word one (or the end of the text).
bool boundary(std::string_view s, std::size_t i) noexcept { return word_before(s, i) != word_at(s, i); }

// `\s*`: every blank, "\n" included, as Python's `\s` of a str pattern.
std::size_t skip_space(std::string_view s, std::size_t i) noexcept {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!dk::is_py_space(decode(s, i, after))) break;
        i = after;
    }
    return i;
}

// `\d*`: the decimal digits of any script.
std::size_t skip_digits(std::string_view s, std::size_t i) noexcept {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!dk::is_py_decimal(decode(s, i, after))) break;
        i = after;
    }
    return i;
}

bool is_upper(char c) noexcept { return c >= 'A' && c <= 'Z'; }
bool is_upper_digit(char c) noexcept { return is_upper(c) || (c >= '0' && c <= '9'); }
bool is_lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
bool is_kind(char c) noexcept { return c == 'R' || c == 'S' || c == 'F' || c == 'A' || c == 'Q' || c == 'P'; }

bool has_at(std::string_view s, std::size_t i, std::string_view literal) noexcept { return i <= s.size() && s.size() - i >= literal.size() && s.compare(i, literal.size(), literal) == 0; }

bool at(std::string_view s, std::size_t i, char c) noexcept { return i < s.size() && s[i] == c; }

// `\d+[a-z]?` at i, then whatever `follow(position)` demands.  The letter is taken greedily and given back if what follows then fails; the digits are not given back, because no
// follower this tool uses can begin with a digit, and a backtracking regex would reach the same single answer.  The end of the match, or npos.
template <class Follow>
std::size_t digits_letter(std::string_view s, std::size_t i, Follow&& follow) {
    const std::size_t digits_end = skip_digits(s, i);
    if (digits_end == i) return kNpos;
    if (digits_end < s.size() && is_lower(s[digits_end]) && follow(digits_end + 1)) return digits_end + 1;
    if (follow(digits_end)) return digits_end;
    return kNpos;
}

// `^` of an re.M pattern: the start of the text and the position after each "\n".  `attempt(p)` is tried at each such position in order; it returns the end of its match or kNpos.
// After a match the next try is at the first line start at or after that end, which is where finditer resumes.
template <class Attempt>
void each_line_start(std::string_view s, Attempt&& attempt) {
    const std::size_t n = s.size();
    std::size_t p = 0;
    while (true) {
        const std::size_t end = attempt(p);
        std::size_t from = end == kNpos ? p + 1 : end;
        if (from > n) return;
        if (from != 0 && s[from - 1] != '\n') {
            const std::size_t newline = s.find('\n', from);
            if (newline == kNpos) return;
            from = newline + 1;
        }
        p = from;
    }
}

// The "PREFIX-KIND-" that starts every identifier of a file's own prefix, at i: the index after it, or kNpos; `kinds` are the letters allowed.
std::size_t own_head(std::string_view s, std::size_t i, std::string_view prefix, std::string_view kinds, char& kind) noexcept {
    if (!has_at(s, i, prefix)) return kNpos;
    const std::size_t k = i + prefix.size();
    if (!at(s, k, '-') || k + 2 >= s.size() || kinds.find(s[k + 1]) == std::string_view::npos || s[k + 2] != '-') return kNpos;
    kind = s[k + 1];
    return k + 3;
}

}  // namespace

std::vector<Id> find_ids(std::string_view s) {
    std::vector<Id> out;
    const std::size_t n = s.size();
    std::size_t i = 0;
    while (i < n) {
        if (is_upper(s[i]) && !word_before(s, i)) {
            std::size_t j = i + 1;
            while (j < n && is_upper_digit(s[j])) ++j;
            const std::size_t length = j - i;   // [A-Z][A-Z0-9]{1,15}: 2 to 16, and then a hyphen (nothing inside the run is one, so no shorter prefix can serve)
            if (length >= 2 && length <= 16 && j + 2 < n && s[j] == '-' && is_kind(s[j + 1]) && s[j + 2] == '-') {
                const std::size_t k = j + 3;
                const std::size_t end = digits_letter(s, k, [&](std::size_t p) { return boundary(s, p); });
                if (end != kNpos) {
                    out.push_back(Id{std::string(s.substr(i, length)), s[j + 1], std::string(s.substr(k, end - k))});
                    i = end;
                    continue;
                }
            }
        }
        std::size_t after = 0;
        (void)decode(s, i, after);
        i = after;
    }
    return out;
}

std::optional<std::string> spec_id(std::string_view s) {
    std::optional<std::string> found;
    each_line_start(s, [&](std::size_t p) -> std::size_t {
        if (found || !at(s, p, '|')) return kNpos;
        std::size_t q = skip_space(s, p + 1);
        if (!has_at(s, q, "**Spec ID**")) return kNpos;
        q = skip_space(s, q + 11);
        if (!at(s, q, '|')) return kNpos;
        q = skip_space(s, q + 1);
        if (!at(s, q, '`')) return kNpos;
        ++q;
        std::size_t j = q;
        if (j >= s.size() || !is_upper(s[j])) return kNpos;
        ++j;
        while (j < s.size() && is_upper_digit(s[j])) ++j;
        const std::size_t length = j - q;
        if (length < 2 || length > 16 || !at(s, j, '`')) return kNpos;
        q = skip_space(s, j + 1);
        if (!at(s, q, '|')) return kNpos;
        found = std::string(s.substr(j - length, length));
        return q + 1;
    });
    return found;
}

std::pair<std::string, std::string> split_coverage(std::string_view s) {
    const std::size_t start = s.find("**Coverage.**");
    if (start == kNpos) return {std::string(s), std::string()};
    // re.search(r"^## ", rest[1:], re.M): `rest[1:]` begins at start + 1, and a slice's own first position is a line start for `^`.
    std::size_t end = s.size();
    for (std::size_t q = start + 1; q < s.size();) {
        if ((q == start + 1 || s[q - 1] == '\n') && has_at(s, q, "## ")) {
            end = q;
            break;
        }
        const std::size_t newline = s.find('\n', q);
        if (newline == kNpos) break;
        q = newline + 1;
    }
    std::string body(s.substr(0, start));
    body.append(s.substr(end));
    return {std::move(body), std::string(s.substr(start, end - start))};
}

std::vector<std::string> table_defs(std::string_view s, std::string_view prefix) {
    std::vector<std::string> out;
    each_line_start(s, [&](std::size_t p) -> std::size_t {
        if (!at(s, p, '|')) return kNpos;
        std::size_t q = skip_space(s, p + 1);
        if (!at(s, q, '`')) return kNpos;
        ++q;
        char kind = 0;
        const std::size_t k = own_head(s, q, prefix, "RSFAQP", kind);
        if (k == kNpos) return kNpos;
        const std::size_t end = digits_letter(s, k, [&](std::size_t x) { return at(s, x, '`'); });
        if (end == kNpos) return kNpos;
        const std::size_t t = skip_space(s, end + 1);
        if (!at(s, t, '|')) return kNpos;
        out.emplace_back(s.substr(q, end - q));
        return t + 1;
    });
    return out;
}

std::vector<std::string> bullet_defs(std::string_view s, std::string_view prefix) {
    std::vector<std::string> out;
    each_line_start(s, [&](std::size_t p) -> std::size_t {
        std::size_t q = skip_space(s, p);
        if (!at(s, q, '-')) return kNpos;
        const std::size_t blank = skip_space(s, q + 1);
        if (blank == q + 1 || !has_at(s, blank, "**")) return kNpos;
        const std::size_t id_start = blank + 2;
        char kind = 0;
        const std::size_t k = own_head(s, id_start, prefix, "RSFAQP", kind);
        if (k == kNpos) return kNpos;
        const std::size_t end = digits_letter(s, k, [&](std::size_t x) { return has_at(s, x, ".**"); });
        if (end == kNpos) return kNpos;
        out.emplace_back(s.substr(id_start, end - id_start));
        return end + 3;
    });
    return out;
}

std::vector<std::string> acceptance_rows(std::string_view s, std::string_view prefix) {
    std::vector<std::string> out;
    each_line_start(s, [&](std::size_t p) -> std::size_t {
        if (!at(s, p, '|')) return kNpos;
        std::size_t q = skip_space(s, p + 1);
        if (!at(s, q, '`')) return kNpos;
        ++q;
        char kind = 0;
        const std::size_t k = own_head(s, q, prefix, "A", kind);
        if (k == kNpos) return kNpos;
        const std::size_t end = digits_letter(s, k, [&](std::size_t x) { return at(s, x, '`'); });
        if (end == kNpos) return kNpos;
        const std::size_t newline = s.find('\n', end + 1);   // `.*$`: to the end of the line (a "\r" is part of it, as `.` matches it)
        const std::size_t stop = newline == kNpos ? s.size() : newline;
        out.emplace_back(s.substr(p, stop - p));
        return stop;
    });
    return out;
}

Discharge discharges_of(std::string_view s) {
    Discharge out;
    const std::size_t n = s.size();
    // re.findall(r"\b([RF])-(\d+[a-z]?)\b", column)
    for (std::size_t i = 0; i < n;) {
        if ((s[i] == 'R' || s[i] == 'F') && !word_before(s, i) && at(s, i + 1, '-')) {
            const std::size_t end = digits_letter(s, i + 2, [&](std::size_t p) { return boundary(s, p); });
            if (end != kNpos) {
                out.named.emplace_back(s[i], std::string(s.substr(i + 2, end - (i + 2))));
                i = end;
                continue;
            }
        }
        std::size_t after = 0;
        (void)decode(s, i, after);
        i = after;
    }
    // re.findall(r"\b([RF])-(\d+)…([RF])-(\d+)\b", column)
    for (std::size_t i = 0; i < n;) {
        if ((s[i] == 'R' || s[i] == 'F') && !word_before(s, i) && at(s, i + 1, '-')) {
            const std::size_t a_end = skip_digits(s, i + 2);
            if (a_end > i + 2 && has_at(s, a_end, "\xE2\x80\xA6")) {
                const std::size_t second = a_end + 3;
                if (second < n && (s[second] == 'R' || s[second] == 'F') && at(s, second + 1, '-')) {
                    const std::size_t b_end = skip_digits(s, second + 2);
                    if (b_end > second + 2 && boundary(s, b_end)) {
                        out.ranges.push_back({s[i], std::string(s.substr(i + 2, a_end - (i + 2))), std::string(s.substr(second + 2, b_end - (second + 2)))});
                        i = b_end;
                        continue;
                    }
                }
            }
        }
        std::size_t after = 0;
        (void)decode(s, i, after);
        i = after;
    }
    return out;
}

std::vector<std::string> coverage_cells(std::string_view s) {
    std::vector<std::string> out;
    each_line_start(s, [&](std::size_t p) -> std::size_t {
        if (!at(s, p, '|')) return kNpos;
        for (std::size_t r = p + 1; r < s.size() && s[r] != '\n'; ++r) {
            if (s[r] == '|') {
                out.emplace_back(s.substr(p + 1, r - (p + 1)));
                return r + 1;
            }
        }
        return kNpos;
    });
    return out;
}

std::vector<std::pair<char, std::string>> cell_ids(std::string_view s, std::string_view prefix) {
    std::vector<std::pair<char, std::string>> out;
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '`') {
            char kind = 0;
            const std::size_t k = own_head(s, i + 1, prefix, "RFS", kind);
            if (k != kNpos) {
                const std::size_t end = digits_letter(s, k, [&](std::size_t x) { return at(s, x, '`'); });
                if (end != kNpos) {
                    out.emplace_back(kind, std::string(s.substr(k, end - k)));
                    i = end + 1;
                    continue;
                }
            }
        }
        std::size_t after = 0;
        (void)decode(s, i, after);
        i = after;
    }
    return out;
}

std::vector<Claim> test_case_claims(std::string_view s) {
    std::vector<Claim> out;
    std::size_t line = 1;      // the line of `counted_to`: one more than the "\n"s before it
    std::size_t counted_to = 0;
    std::size_t from = 0;
    while (true) {
        const std::size_t at_test = s.find("TEST_CASE", from);
        if (at_test == kNpos) break;
        from = at_test + 1;   // a failed attempt goes on from the next byte; a success from its own end (below)
        std::size_t q = skip_space(s, at_test + 9);
        if (!at(s, q, '(')) continue;
        q = skip_space(s, q + 1);
        if (!at(s, q, '"')) continue;
        ++q;
        if (q >= s.size() || !is_upper(s[q])) continue;
        std::size_t j = q + 1;
        while (j < s.size() && is_upper_digit(s[j])) ++j;
        const std::size_t length = j - q;
        if (length < 2 || length > 16 || j + 2 >= s.size() || s[j] != '-' || !is_kind(s[j + 1]) || s[j + 2] != '-') continue;
        const std::size_t end = digits_letter(s, j + 3, [](std::size_t) { return true; });
        if (end == kNpos) continue;
        for (std::size_t x = counted_to; x < at_test; ++x) {
            if (s[x] == '\n') ++line;
        }
        counted_to = at_test;
        out.push_back(Claim{std::string(s.substr(q, end - q)), line});
        from = end;
    }
    return out;
}

}  // namespace scan

// ------------------------------------------------------------------------------------------------------------------------------------------------------- the checks

namespace {

// pathlib's str(Path(p)): no empty and no "." components, no trailing slash; "." for what is left of nothing.
std::string path_text(const std::string& given) {
    std::string out;
    const bool absolute = !given.empty() && given.front() == '/';
    std::size_t i = 0;
    while (i <= given.size()) {
        const std::size_t slash = std::min(given.find('/', i), given.size());
        const std::string part = given.substr(i, slash - i);
        if (!part.empty() && part != ".") {
            if (!out.empty() || absolute) out += '/';
            out += part;
        }
        i = slash + 1;
    }
    if (out.empty()) return absolute ? "/" : ".";
    return out;
}

std::string join_path(const std::string& root, const std::string& name) {
    if (root == ".") return name;
    if (root == "/") return "/" + name;
    return root + "/" + name;
}

std::string right(std::size_t value, std::size_t width) { return dk::pad_left(std::to_string(value), width); }

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) out += (i != 0 ? ", " : "") + items[i];
    return out;
}

// int(digits) for the decimal digits of any script; the number of digits is held to 18 so that it fits.
std::uint64_t decimal_number(std::string_view digits, bool& too_long) {
    std::uint64_t value = 0;
    std::size_t count = 0;
    for (std::size_t i = 0; i < digits.size();) {
        std::uint32_t cp = 0;
        if (!dk::next_code_point(digits, i, cp)) break;
        ++count;
        if (count > 18) {
            too_long = true;
            return 0;
        }
        value = value * 10 + static_cast<std::uint64_t>(dk::py_decimal_value(cp));
    }
    return value;
}

// A requirement that a "(partial" Coverage row names and no acceptance row discharges: whether an ordinary row excuses it whole as well, and the first cells of the "(partial" rows.
struct PartialUntested {
    std::string id;
    bool excused_whole = false;
    std::vector<std::string> rows;
};

struct SpecStat {
    fs::path path;
    std::string shown_path;   // as the cross-file report prints it
    std::string prefix;
    std::size_t own = 0, all = 0, defined = 0, reqs = 0, tested = 0, excused = 0;
    std::vector<std::string> foreign, uncovered, dangling, duplicates, contradictory, partial;
    std::set<std::string> defined_ids;
};

// The Python read a spec with read_text(encoding="utf-8"): strict UTF-8, and universal newlines.
std::string read_spec(const fs::path& path) { return dk::universal_newlines(dk::read_text(path)); }

bool check_spec(Io& io, const fs::path& path, const std::string& shown, bool quiet, SpecStat& stat) {
    const std::string name = path.filename().string();
    std::string text;
    try {
        text = read_spec(path);
    } catch (const std::runtime_error& exc) {
        io.err << "REFUSED  " << name << ": " << exc.what() << '\n';
        throw dk::Exit{kUnparseable};
    }

    const std::optional<std::string> declared = scan::spec_id(text);
    if (!declared) {
        io.err << "REFUSED  " << name << ": no `Spec ID` field in the front matter.\n"
               << "  The denominator is own-prefix identifiers, so the prefix must be declared,\n"
               << "  not inferred from the filename. Add a front-matter row:\n"
               << "      | **Spec ID** | `TIME` |\n";
        throw dk::Exit{kUnparseable};
    }
    const std::string prefix = *declared;

    const auto [body, coverage] = scan::split_coverage(text);

    const std::vector<std::string> table = scan::table_defs(body, prefix);
    const std::vector<std::string> bullets = scan::bullet_defs(body, prefix);
    std::set<std::string> defined(table.begin(), table.end());
    defined.insert(bullets.begin(), bullets.end());
    // Counter(table + bullets): the identifiers claimed more than once, in the order they first appear
    std::vector<std::string> duplicates;
    {
        std::map<std::string, int> seen;
        std::vector<std::string> order;
        for (const std::vector<std::string>* list : {&table, &bullets}) {
            for (const std::string& id : *list) {
                if (++seen[id] == 1) order.push_back(id);
            }
        }
        for (const std::string& id : order) {
            if (seen[id] > 1) duplicates.push_back(id);
        }
    }

    std::set<std::string> reqs;   // `i.split("-")[1] in ("R", "F")`: the kind letter follows the prefix and its hyphen
    for (const std::string& id : defined) {
        const char kind = id[prefix.size() + 1];
        if (kind == 'R' || kind == 'F') reqs.insert(id);
    }

    // Acceptance rows discharge requirements through their final column.  The suffix letter matters, and so does a lettered acceptance row (IOFM-A-004b).
    std::set<std::string> discharged;
    for (const std::string& row : scan::acceptance_rows(body, prefix)) {
        std::string cut = dk::rstrip_py(row);
        while (!cut.empty() && cut.back() == '|') cut.pop_back();
        const std::size_t bar = cut.rfind('|');
        const std::string column = bar == std::string::npos ? cut : cut.substr(bar + 1);
        const scan::Discharge found = scan::discharges_of(column);
        for (const auto& [letter, number] : found.named) discharged.insert(prefix + "-" + letter + "-" + number);
        for (const scan::Discharge::Range& range : found.ranges) {
            bool too_long = false;
            const std::uint64_t first = decimal_number(range.first, too_long);
            const std::uint64_t last = decimal_number(range.last, too_long);
            if (too_long) {
                io.err << "REFUSED  " << name << ": a range of identifiers whose numbers have more than 18 digits (" << range.letter << '-' << range.first << "…" << range.letter
                       << '-' << range.last << ") cannot be expanded.\n";
                throw dk::Exit{kUnparseable};
            }
            for (std::uint64_t n = first; n <= last; ++n) {
                std::string digits = std::to_string(n);
                if (digits.size() < 3) digits.insert(0, 3 - digits.size(), '0');   // f"{n:03d}"
                const std::string candidate = prefix + "-" + range.letter + "-" + digits;
                if (defined.count(candidate) != 0) discharged.insert(candidate);
            }
        }
    }

    // EXCUSED IS THE FIRST CELL OF A COVERAGE ROW, NOT ANY MENTION IN ONE; a row marked "(partial" excuses PART of a requirement a test also covers.  For each requirement a
    // "(partial" row names, the first cells of those rows are kept (stripped, in the order the file has them): a finding below quotes them.
    std::set<std::string> excused;
    std::map<std::string, std::vector<std::string>> partial;
    for (const std::string& cell : scan::coverage_cells(coverage)) {
        const bool is_partial = cell.find("(partial") != std::string::npos;
        const std::string cell_text = dk::strip_py(cell);
        for (const auto& [letter, number] : scan::cell_ids(cell, prefix)) {
            const std::string id = prefix + "-" + letter + "-" + number;
            if (!is_partial) {
                excused.insert(id);
            } else {
                std::vector<std::string>& rows = partial[id];
                if (rows.empty() || rows.back() != cell_text) rows.push_back(cell_text);
            }
        }
    }

    std::vector<std::string> uncovered;
    std::vector<std::string> contradictory;   // BOTH tested and excused: the Coverage row is stale
    std::vector<std::string> partial_ok;
    std::vector<PartialUntested> partial_untested;
    std::size_t tested = 0;
    std::size_t excused_count = 0;
    for (const std::string& id : reqs) {
        const bool d = discharged.count(id) != 0;
        const bool e = excused.count(id) != 0;
        const auto p = partial.find(id);
        if (d) ++tested;
        if (e) ++excused_count;
        if (!d && !e) uncovered.push_back(id);
        if (d && e) contradictory.push_back(id);   // BOTH: the Coverage table's column says "why no test", so the row is stale; reported, never netted off
        if (d && p != partial.end()) partial_ok.push_back(id);
        // A "(partial" marker on something no test discharges is not a partial excuse: it is a label that misleads.  (The Python put the requirement in `contradictory` as well,
        // which counted it twice and failed its partition `assert`.)  It is REPORTED, once, whether or not an ordinary row also excuses the requirement (plan L0 step 8, group C4).
        if (!d && p != partial.end()) partial_untested.push_back(PartialUntested{id, e, p->second});
    }

    // Every identifier mentioned anywhere, so dangling references are caught.
    std::set<std::string> all_ids;
    for (const scan::Id& id : scan::find_ids(text)) all_ids.insert(id.text());
    std::set<std::string> own_ids;
    std::vector<std::string> foreign;
    for (const std::string& id : all_ids) {
        if (id.rfind(prefix + "-", 0) == 0) own_ids.insert(id);
        else foreign.push_back(id);
    }
    std::vector<std::string> dangling;
    for (const std::string& id : own_ids) {
        if (defined.count(id) == 0) dangling.push_back(id);
    }

    stat.path = path;
    stat.shown_path = shown;
    stat.prefix = prefix;
    stat.own = own_ids.size();
    stat.all = all_ids.size();
    stat.defined = defined.size();
    stat.reqs = reqs.size();
    stat.tested = tested;
    stat.excused = excused_count;
    stat.foreign = foreign;
    stat.uncovered = uncovered;
    stat.dangling = dangling;
    stat.duplicates = duplicates;
    stat.contradictory = contradictory;
    stat.partial = partial_ok;
    stat.defined_ids = defined;

    // The three printed components must PARTITION the denominator.  They are laid out as though they do, so they are made to, and the check is here and not in a reader's head.
    // (The Python was an `assert`, and it was where a "(partial" row for a requirement no test discharges landed: the requirement was counted in `uncovered` AND in `contradictory`,
    // the sum double-counted it, and the Python died with an AssertionError instead of reporting it.  That is a finding of its own now, below, and every requirement is counted
    // once in the classes above, so this is a guard against a mistake in this function, and it cannot fail on any input.)
    if (!partitions(tested, excused_count, contradictory.size(), uncovered.size(), reqs.size())) {
        dk::die(io, kTool, kGaps,
                "assertion failed: " + shown + ": " + std::to_string(tested) + " tested + " + std::to_string(excused_count) + " excused - " + std::to_string(contradictory.size()) +
                    " both + " + std::to_string(uncovered.size()) + " uncovered != " + std::to_string(reqs.size()));
    }

    const bool ok = uncovered.empty() && dangling.empty() && duplicates.empty() && contradictory.empty() && partial_untested.empty();

    if (!quiet) {
        io.out << "\n" << name << "  [Spec ID: " << prefix << "]\n";
        io.out << "  identifiers, OWN-PREFIX  " << right(stat.own, 4) << "   <- the denominator\n";
        io.out << "  identifiers, ALL         " << right(stat.all, 4)
               << (foreign.empty() ? std::string("   (no foreign identifiers referenced)") : "   (" + std::to_string(foreign.size()) + " foreign, NOT counted: " + join(foreign) + ")") << '\n';
        io.out << "  requirements + refusals  " << right(stat.reqs, 4) << '\n';
        io.out << "    discharged by a test   " << right(stat.tested, 4) << '\n';
        io.out << "    excused in §8 Coverage " << right(stat.excused, 4) << '\n';
        if (!partial_ok.empty()) io.out << "    partially excused      " << right(partial_ok.size(), 4) << "   " << join(partial_ok) << "   <- counted as tested; one clause is structural\n";
        if (!contradictory.empty()) {
            io.out << "    BOTH tested and excused" << right(contradictory.size(), 4) << "   " << join(contradictory)
                   << "   <- the Coverage row is stale; its column says \"why no test\"\n";
        }
        if (!partial_untested.empty()) {
            std::vector<std::string> ids;
            for (const PartialUntested& p : partial_untested) ids.push_back(p.id);
            io.out << "    \"(partial\" NOT TESTED  " << right(ids.size(), 4) << "   " << join(ids) << "   <- a \"(partial\" row names it and no test discharges it; see its PARTIAL finding\n";
        }
        io.out << "    UNCOVERED              " << right(uncovered.size(), 4) << (uncovered.empty() ? std::string() : "   " + join(uncovered)) << '\n';
        if (!dangling.empty()) io.out << "  DANGLING references      " << right(dangling.size(), 4) << "   " << join(dangling) << '\n';
        if (!duplicates.empty()) io.out << "  DUPLICATE definitions    " << right(duplicates.size(), 4) << "   " << join(duplicates) << '\n';
    }
    // One named finding per requirement, on standard error whatever --quiet says: the spec, the identifier, the row's own text, and what to do.
    for (const PartialUntested& p : partial_untested) {
        io.err << "  PARTIAL    " << p.id << "  (" << name << "): a \"(partial\" Coverage row names it and no acceptance row discharges it\n";
        for (const std::string& row : p.rows) io.err << "      row: " << row << '\n';
        io.err << (p.excused_whole ? "      Another Coverage row already excuses it whole: drop the \"(partial\" row, or add the acceptance row that would make it partial.\n"
                                   : "      Either the acceptance row the \"(partial\" promises is missing, or nothing tests it and a Coverage row without \"(partial\" should say why.\n");
    }
    return ok;
}

// The SAME identifier defined (as a table row or a bulleted requirement) in two DIFFERENT specification files: only possible if both declare the same `Spec ID` prefix, and
// each file looks completely clean in isolation, so nothing but this looks at the seam between them.
bool check_cross_file_duplicate_definitions(Io& io, const std::vector<SpecStat>& stats, bool quiet) {
    std::map<std::string, std::vector<std::string>> locations;
    for (const SpecStat& s : stats) {
        for (const std::string& id : s.defined_ids) locations[id].push_back(s.shown_path);
    }
    std::map<std::string, std::vector<std::string>> dupes;
    for (const auto& [id, paths] : locations) {
        if (paths.size() > 1) dupes.emplace(id, paths);
    }
    if (!quiet) {
        io.out << "\nCross-file identifier definitions (" << stats.size() << " specs read as one namespace)\n";
        io.out << "  distinct ids defined      " << right(locations.size(), 4) << '\n';
        io.out << "  DUPLICATE definitions     " << right(dupes.size(), 4) << '\n';
    }
    for (const auto& [id, paths] : dupes) {
        io.err << "  DUPLICATE  " << id << "  defined in " << paths.size() << " files:\n";
        for (const std::string& p : paths) io.err << "      " << p << '\n';
    }
    return dupes.empty();
}

// Every `.cpp` under `root`, as the list of its path components below it, sorted as pathlib sorts paths (component by component).  Directories are not followed through links,
// and a name that matches is listed whatever it is (a directory called x.cpp reads as nothing, below).  THE SCOPE FIX (ruling D12 a): the root's own `build*` directories and
// its `data/` are not entered.
std::vector<std::vector<std::string>> cpp_files_below(const fs::path& root) {
    std::vector<std::vector<std::string>> found;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return found;
    std::vector<std::string> trail;
    const auto walk = [&](const auto& self, const fs::path& dir) -> void {
        std::error_code open_error;
        fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, open_error);
        if (open_error) return;
        for (const fs::directory_iterator end; it != end; it.increment(open_error)) {
            if (open_error) return;
            const std::string name = it->path().filename().string();
            std::error_code status_error;
            const bool is_link = it->is_symlink(status_error);
            const bool is_dir = it->is_directory(status_error);
            if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".cpp") == 0) {
                std::vector<std::string> one = trail;
                one.push_back(name);
                found.push_back(std::move(one));
            }
            if (is_dir && !is_link) {
                if (trail.empty() && (name.rfind("build", 0) == 0 || name == "data")) continue;
                trail.push_back(name);
                self(self, it->path());
                trail.pop_back();
            }
        }
    };
    walk(walk, root);
    std::sort(found.begin(), found.end());
    return found;
}

// Every id claimed as a `TEST_CASE`'s own leading identifier, tree-wide, and the refusal if one id is claimed twice, whichever spec file, if any, defines it.  A `.cpp` that cannot
// be read as UTF-8 is skipped, not fatal: this check's own false negative there is far cheaper than making an unrelated encoding issue block gate 7.
bool check_duplicate_test_case_claims(Io& io, const std::string& root_given, bool quiet) {
    const std::string root = path_text(root_given);
    std::map<std::string, std::vector<std::string>> claims;
    for (const std::vector<std::string>& components : cpp_files_below(fs::path(root_given))) {
        std::string relative;
        fs::path file(root_given);
        for (const std::string& part : components) {
            relative += (relative.empty() ? "" : "/") + part;
            file /= part;
        }
        // A name that matches is not yet a source file: a directory called x.cpp, a link to nothing, a FIFO hold none, and are skipped BY RULE here, not by what reading one happens to
        // do on this filesystem (C3 relied on the latter and exited 70 on GitHub's runner, whose ext4 answers differently from tmpfs: PROVENANCE section 41.10).
        std::error_code kind_error;
        if (!fs::is_regular_file(file, kind_error)) continue;
        std::string text;
        try {
            text = dk::universal_newlines(dk::read_text(file));
        } catch (const std::runtime_error&) {
            continue;
        }
        for (const scan::Claim& claim : scan::test_case_claims(text)) claims[claim.id].push_back(join_path(root, relative) + ":" + std::to_string(claim.line));
    }
    std::map<std::string, std::vector<std::string>> dupes;
    for (const auto& [id, places] : claims) {
        if (places.size() > 1) dupes.emplace(id, places);
    }
    if (!quiet) {
        io.out << "\nTEST_CASE claims, tree-wide (`--cpp-root " << root << "`)\n";
        io.out << "  distinct ids claimed     " << right(claims.size(), 4) << '\n';
        io.out << "  DUPLICATE claims         " << right(dupes.size(), 4) << '\n';
    }
    for (const auto& [id, places] : dupes) {
        io.err << "  DUPLICATE  " << id << "  claimed by " << places.size() << " TEST_CASEs:\n";
        for (const std::string& place : places) io.err << "      " << place << '\n';
    }
    return dupes.empty();
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------- the command line

const char kUsageText[] =
    "usage: speccheck [-h] [--spec-dir SPEC_DIR] [--cpp-root CPP_ROOT]\n"
    "                 [--skip-test-case-check] [--quiet]\n";

const char kHelpText[] =
    "\n"
    "the specification coverage checker: every requirement and refusal in an adopted specification is discharged by an acceptance row or individually\n"
    "excused; no identifier is defined in two specification files; no identifier is claimed by two TEST_CASEs.  The denominator is own-prefix identifiers,\n"
    "both counts are printed, and what this reads is the specifications' own traceability, not the test suite.\n"
    "\n"
    "options:\n"
    "  -h, --help               show this help and exit\n"
    "  --spec-dir SPEC_DIR      the specifications, SPEC-*.md (default: <tree>/spec)\n"
    "  --cpp-root CPP_ROOT      where the TEST_CASE claims are scanned for (default: the tree); its own build*/ and data/ are not read\n"
    "  --skip-test-case-check   spec-only run (a synthetic spec directory is not also scanned tree-wide)\n"
    "  --quiet                  print the totals and the verdict, not each specification's own counts\n"
    "\n"
    "exit codes: 0 complete   1 gaps found   2 argument error   3 a specification could not be parsed\n";

}  // namespace

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

bool partitions(std::size_t tested, std::size_t excused, std::size_t both, std::size_t uncovered, std::size_t requirements) noexcept {
    return tested + excused + uncovered == requirements + both;   // tested + excused - both + uncovered == requirements, without the subtraction
}

int run(const std::vector<std::string>& argv, Io io) {
    try {
        std::string spec_dir_given = (default_root() / "spec").string();
        std::string cpp_root_given = default_root().string();
        bool skip_test_case_check = false;
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
            if (opt == "--spec-dir" || opt == "--cpp-root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                (opt == "--spec-dir" ? spec_dir_given : cpp_root_given) = value;
            } else if ((opt == "--skip-test-case-check" || opt == "--quiet") && !has_value) {
                (opt == "--quiet" ? quiet : skip_test_case_check) = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        // sorted(p for p in spec_dir.glob("SPEC-*.md") if p.name != "SPEC-template.md")
        const std::string spec_dir = path_text(spec_dir_given);
        std::vector<std::string> names;
        std::error_code ec;
        if (fs::is_directory(fs::path(spec_dir_given), ec)) {
            for (const fs::directory_entry& entry : fs::directory_iterator(fs::path(spec_dir_given), fs::directory_options::skip_permission_denied, ec)) {
                const std::string name = entry.path().filename().string();
                if (name.size() >= 8 && name.rfind("SPEC-", 0) == 0 && name.compare(name.size() - 3, 3, ".md") == 0 && name != "SPEC-template.md") names.push_back(name);
            }
        }
        std::sort(names.begin(), names.end());
        if (names.empty()) {
            io.err << kTool << ": no SPEC-*.md in " << spec_dir << '\n';
            return kUnparseable;
        }

        std::vector<SpecStat> stats;
        bool all_ok = true;
        for (const std::string& name : names) {
            SpecStat stat;
            all_ok = check_spec(io, fs::path(spec_dir_given) / name, join_path(spec_dir, name), quiet, stat) && all_ok;
            stats.push_back(std::move(stat));
        }
        const bool cross_file_ok = check_cross_file_duplicate_definitions(io, stats, quiet);
        const bool test_case_ok = skip_test_case_check ? true : check_duplicate_test_case_claims(io, cpp_root_given, quiet);

        std::size_t total_reqs = 0, total_own = 0, total_all = 0, total_tested = 0, total_excused = 0, total_uncovered = 0;
        for (const SpecStat& s : stats) {
            total_reqs += s.reqs;
            total_own += s.own;
            total_all += s.all;
            total_tested += s.tested;
            total_excused += s.excused;
            total_uncovered += s.uncovered.size();
        }
        io.out << "\n" << std::string(70, '=') << '\n';
        io.out << names.size() << " specifications\n";
        io.out << "  requirements and refusals, OWN-PREFIX denominator : " << total_reqs << '\n';
        io.out << "    discharged by an acceptance ROW                 : " << total_tested << '\n';
        io.out << "    excused, with a reason, in §8 Coverage          : " << total_excused << '\n';
        io.out << "    uncovered                                       : " << total_uncovered << '\n';
        io.out << "  identifiers referenced, own-prefix / all          : " << total_own << " / " << total_all << '\n';
        if (total_own != total_all) {
            io.out << "  the difference of " << (total_all - total_own) << " is cross-references between specs, which\n";
            io.out << "  are references and not definitions.  Counting them would inflate the\n";
            io.out << "  denominator; that is the 121-against-128 error, printed rather than made.\n";
        }

        if (all_ok && cross_file_ok && test_case_ok) {
            io.out << "\nok       every requirement and refusal is discharged by an acceptance ROW\n";
            io.out << "         or individually excused, in every specification.\n";
            io.out << "ok       no id is defined in more than one specification file.\n";
            if (!skip_test_case_check) io.out << "ok       no id is claimed by more than one TEST_CASE, tree-wide.\n";
            io.out << '\n';
            io.out << "         WHAT THIS DOES NOT CHECK: that those rows are implemented. This reads\n";
            io.out << "         the specifications' own traceability, not the test suite, so a spec\n";
            io.out << "         that is written and not yet built passes here — SPEC-ephemerides.md\n";
            io.out << "         does today. A green result is 'the spec is internally complete', never\n";
            io.out << "         'the module is tested'.\n";
            return kOk;
        }
        io.err << "\nFAILED   see above\n";
        return kGaps;
    } catch (const dk::Exit& stop) {
        return stop.code;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::speccheck

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::speccheck::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
