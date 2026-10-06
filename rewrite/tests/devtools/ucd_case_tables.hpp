#pragma once
// tests/devtools/ucd_case_tables.hpp — Python's case mappings, DERIVED from the Unicode Character Database files this tree pins (manifest entries `ucd-unicodedata` and `ucd-specialcasing`, UCD 15.1.0).
//
// The C++ development tools emulate two things of Python's text handling at the level of case: str.upper() (tools/literaturecheck.cpp: terms.upper().startswith("NOT ESTABLISHED")) and re.IGNORECASE
// (tools/unitcheck.cpp: \bkm\b|kilomet).  Both were first written against witnesses that are not in the tree (a one-off run of another language's case mapping for the first, the documentation's own
// sentence for the second); this is the derivation from pinned data, so that anyone can rerun it and a change of the pin shows (L0 step 8, group C6; tests/devtools/unicode_tables_tests.cpp uses it).
//
//   full_upper(c)  the unconditional entry of SpecialCasing.txt (one WITHOUT a condition list), else UnicodeData.txt's simple uppercase mapping (field 12), else c itself.  Python's str.upper() makes
//                  this mapping one character at a time; the conditional entries (Final_Sigma, Lithuanian, Turkish, Azeri) are not applied by it.
//   full_lower(c)  likewise, with the lower-case column and UnicodeData's field 13.
//   lower1(c)      the first code point of full_lower(c): what the `re` engine lowers both the pattern's literal and the text's character with (CPython's _PyUnicode_ToLowercase, which is the first
//                  code point of the full mapping where there is one).
//   extra cases    the table Lib/re/_casefix.py holds, as its comment says it is made: "Maps the code of lowercased character to codes of different lowercased characters which have the same
//                  uppercase" -- the code points sharing one full upper case (a group of two or more) whose lower1 codes are two or more different ones, each mapped to the others.
//   re.IGNORECASE  a text character t matches the pattern character p iff lower1(t) == lower1(p) or lower1(t) is one of extra_cases[lower1(p)].
//
// What this header does NOT claim: that it runs Python.  The check against Python is the one place the claims meet it: the number of non-ASCII letters that re.IGNORECASE equates with an ASCII letter and
// their names, which the re module's documentation states (four: U+0130, U+0131, U+017F, U+212A), and the whole `_casefix` table, compared once with the one in the installed standard library (the L0-8
// report's C6 evidence).

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace odl::devtools_testing::ucd {

constexpr char32_t kLastCodePoint = 0x10FFFF;

[[nodiscard]] inline bool is_surrogate(char32_t c) noexcept { return c >= 0xD800 && c <= 0xDFFF; }

struct CaseTables {
    std::map<char32_t, char32_t> simple_upper;          // UnicodeData.txt field 12
    std::map<char32_t, char32_t> simple_lower;          // field 13
    std::map<char32_t, std::u32string> special_upper;   // SpecialCasing.txt, the entries without a condition list
    std::map<char32_t, std::u32string> special_lower;
    std::size_t unicode_data_lines = 0;
    std::size_t special_casing_unconditional = 0;
    std::size_t special_casing_conditional = 0;
};

namespace detail {

[[nodiscard]] inline std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

// the fields of a line, empty ones kept
[[nodiscard]] inline std::vector<std::string_view> split(std::string_view s, char separator) {
    std::vector<std::string_view> out;
    std::size_t from = 0;
    for (;;) {
        const std::size_t at = s.find(separator, from);
        if (at == std::string_view::npos) {
            out.push_back(s.substr(from));
            return out;
        }
        out.push_back(s.substr(from, at - from));
        from = at + 1;
    }
}

// the lines of a file: split on LF; the empty piece after a final LF is no line
[[nodiscard]] inline std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> out = split(text, '\n');
    if (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

// "U+00DF": a code point, as the files and the reports write it
[[nodiscard]] inline std::string show(char32_t c) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string hex;
    for (std::uint32_t v = static_cast<std::uint32_t>(c); v != 0; v >>= 4) hex.insert(hex.begin(), kDigits[v & 0xFU]);
    while (hex.size() < 4) hex.insert(hex.begin(), '0');
    return "U+" + hex;
}

// 4 to 6 UPPER-CASE hex digits, at most 10FFFF: the form every code point of both files is written in; anything else is refused with the place named
[[nodiscard]] inline char32_t parse_code_point(std::string_view s, const std::string& where) {
    if (s.size() < 4 || s.size() > 6) throw std::runtime_error(where + ": '" + std::string(s) + "' is not a code point (4 to 6 hex digits)");
    std::uint32_t v = 0;
    for (const char c : s) {
        std::uint32_t d = 0;
        if (c >= '0' && c <= '9') {
            d = static_cast<std::uint32_t>(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            d = static_cast<std::uint32_t>(c - 'A') + 10U;
        } else {
            throw std::runtime_error(where + ": '" + std::string(s) + "' is not a code point (upper-case hex digits only)");
        }
        v = v * 16U + d;
    }
    if (v > kLastCodePoint) throw std::runtime_error(where + ": '" + std::string(s) + "' is beyond U+10FFFF");
    return static_cast<char32_t>(v);
}

// a mapping of SpecialCasing.txt: one or more code points separated by white space
[[nodiscard]] inline std::u32string parse_sequence(std::string_view s, const std::string& where) {
    std::u32string out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i >= s.size()) break;
        std::size_t j = i;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t') ++j;
        out.push_back(parse_code_point(s.substr(i, j - i), where));
        i = j;
    }
    if (out.empty()) throw std::runtime_error(where + ": an empty mapping");
    return out;
}

}  // namespace detail

/// UnicodeData.txt: fifteen `;`-separated fields on every line, the code point first, the simple uppercase mapping in field 12 and the lowercase one in field 13 (UAX #44, 5.7.1); a line of another
/// shape, a code point twice, or a blank line is refused.  (A range is a pair of lines, `<..., First>` and `<..., Last>`, which carry no case mapping.)
inline void read_unicode_data(CaseTables& tables, std::string_view text) {
    std::set<char32_t> seen;
    std::size_t number = 0;
    for (const std::string_view line : detail::lines_of(text)) {
        ++number;
        const std::string where = "UnicodeData.txt line " + std::to_string(number);
        const std::vector<std::string_view> f = detail::split(line, ';');
        if (f.size() != 15) throw std::runtime_error(where + ": " + std::to_string(f.size()) + " fields, not 15");
        const char32_t c = detail::parse_code_point(f[0], where);
        if (!seen.insert(c).second) throw std::runtime_error(where + ": the code point occurs twice");
        if (!f[12].empty()) tables.simple_upper[c] = detail::parse_code_point(f[12], where);
        if (!f[13].empty()) tables.simple_lower[c] = detail::parse_code_point(f[13], where);
        ++tables.unicode_data_lines;
    }
}

/// SpecialCasing.txt: `code; lower; title; upper; # name` or, with a condition list, `code; lower; title; upper; condition; # name`.  The unconditional entries are kept, the conditional ones counted.
inline void read_special_casing(CaseTables& tables, std::string_view text) {
    std::size_t number = 0;
    for (std::string_view line : detail::lines_of(text)) {
        ++number;
        const std::string where = "SpecialCasing.txt line " + std::to_string(number);
        const std::size_t hash = line.find('#');
        if (hash != std::string_view::npos) line = line.substr(0, hash);
        line = detail::trim(line);
        if (line.empty()) continue;
        const std::vector<std::string_view> f = detail::split(line, ';');
        const bool unconditional = f.size() == 5 && detail::trim(f[4]).empty();
        const bool conditional = f.size() == 6 && !detail::trim(f[4]).empty() && detail::trim(f[5]).empty();
        if (!unconditional && !conditional) throw std::runtime_error(where + ": neither `code; lower; title; upper;` nor the same with a condition list");
        if (conditional) {
            ++tables.special_casing_conditional;
            continue;
        }
        const char32_t c = detail::parse_code_point(detail::trim(f[0]), where);
        if (tables.special_lower.count(c) != 0) throw std::runtime_error(where + ": a second unconditional entry for the code point");
        tables.special_lower[c] = detail::parse_sequence(f[1], where);
        tables.special_upper[c] = detail::parse_sequence(f[3], where);
        ++tables.special_casing_unconditional;
    }
}

[[nodiscard]] inline CaseTables read_tables(std::string_view unicode_data, std::string_view special_casing) {
    CaseTables t;
    read_unicode_data(t, unicode_data);
    read_special_casing(t, special_casing);
    return t;
}

[[nodiscard]] inline std::u32string full_upper(const CaseTables& t, char32_t c) {
    if (const auto it = t.special_upper.find(c); it != t.special_upper.end()) return it->second;
    if (const auto it = t.simple_upper.find(c); it != t.simple_upper.end()) return std::u32string(1, it->second);
    return std::u32string(1, c);
}

[[nodiscard]] inline std::u32string full_lower(const CaseTables& t, char32_t c) {
    if (const auto it = t.special_lower.find(c); it != t.special_lower.end()) return it->second;
    if (const auto it = t.simple_lower.find(c); it != t.simple_lower.end()) return std::u32string(1, it->second);
    return std::u32string(1, c);
}

[[nodiscard]] inline char32_t lower1(const CaseTables& t, char32_t c) { return full_lower(t, c).front(); }

/// A non-ASCII character whose full upper case is made of ASCII characters only, and that upper case.
struct AsciiUpper {
    char32_t code_point;
    std::string upper;
};

/// Every non-ASCII code point whose full upper case is pure ASCII (and not empty), in code point order.  A code point without an upper-case entry maps to itself, which for a non-ASCII one is not ASCII, so
/// only the code points that have an entry need to be looked at.
[[nodiscard]] inline std::vector<AsciiUpper> non_ascii_with_ascii_upper(const CaseTables& t) {
    std::set<char32_t> candidates;
    for (const auto& kv : t.simple_upper) candidates.insert(kv.first);
    for (const auto& kv : t.special_upper) candidates.insert(kv.first);
    std::vector<AsciiUpper> out;
    for (const char32_t c : candidates) {
        if (c < 0x80) continue;
        const std::u32string u = full_upper(t, c);
        std::string ascii;
        for (const char32_t x : u) {
            if (x >= 0x80) {
                ascii.clear();
                break;
            }
            ascii += static_cast<char>(x);
        }
        if (!ascii.empty()) out.push_back({c, ascii});
    }
    return out;
}

/// `re`'s extra cases: lower-cased code -> the other lower-cased codes whose characters share its upper case (see the top of this file).  A group of two or more code points that has a member with
/// a lower case of more than one code point is refused: the table's generator would die on it, so no table exists to compare with.
using ExtraCases = std::map<char32_t, std::set<char32_t>>;

[[nodiscard]] inline ExtraCases derive_extra_cases(const CaseTables& t) {
    // the code points that have an upper-case entry are the only ones whose upper case can differ from themselves; group them by upper case...
    std::map<std::u32string, std::set<char32_t>> groups;
    std::set<char32_t> with_entry;
    for (const auto& kv : t.simple_upper) with_entry.insert(kv.first);
    for (const auto& kv : t.special_upper) with_entry.insert(kv.first);
    for (const char32_t c : with_entry) {
        if (is_surrogate(c)) continue;
        const std::u32string u = full_upper(t, c);
        groups[u].insert(c);
    }
    // ... and a character that is its own upper case belongs to the group of that upper case
    for (auto& kv : groups) {
        if (kv.first.size() == 1 && full_upper(t, kv.first.front()) == kv.first) kv.second.insert(kv.first.front());
    }
    ExtraCases extra;
    for (const auto& kv : groups) {
        if (kv.second.size() < 2) continue;
        std::set<char32_t> lowers;
        for (const char32_t c : kv.second) {
            const std::u32string lo = full_lower(t, c);
            if (lo.size() != 1) throw std::runtime_error("a group of code points with one upper case has a member (" + detail::show(c) + ") whose lower case is not one code point");
            lowers.insert(lo.front());
        }
        if (lowers.size() < 2) continue;
        for (const char32_t l : lowers) {
            for (const char32_t m : lowers) {
                if (m != l) extra[l].insert(m);
            }
        }
    }
    return extra;
}

/// re.IGNORECASE on a str pattern: does the text character match the pattern character?
[[nodiscard]] inline bool ignorecase_matches(const CaseTables& t, const ExtraCases& extra, char32_t pattern_char, char32_t text_char) {
    const char32_t lo = lower1(t, pattern_char);
    const char32_t text_lo = lower1(t, text_char);
    if (text_lo == lo) return true;
    const auto it = extra.find(lo);
    return it != extra.end() && it->second.count(text_lo) != 0;
}

/// A non-ASCII code point that re.IGNORECASE equates with an ASCII letter (the lower-case one is named).
struct Equivalent {
    char letter;
    char32_t code_point;
};

/// Every (letter, non-ASCII code point) pair such that the code point matches the letter under re.IGNORECASE, letters 'a'..'z' in order, code points ascending.  A non-ASCII code point matches a letter only
/// if its lower1 is the letter's or one of the letter's extra cases: either it has a lower-case entry (lower1 differs from it), or it is itself one of the extra cases.
[[nodiscard]] inline std::vector<Equivalent> non_ascii_equivalents_of_ascii_letters(const CaseTables& t, const ExtraCases& extra) {
    std::set<char32_t> candidates;
    for (const auto& kv : t.simple_lower) candidates.insert(kv.first);
    for (const auto& kv : t.special_lower) candidates.insert(kv.first);
    for (const auto& kv : extra) {
        candidates.insert(kv.first);
        for (const char32_t c : kv.second) candidates.insert(c);
    }
    std::vector<Equivalent> out;
    for (char letter = 'a'; letter <= 'z'; ++letter) {
        for (const char32_t c : candidates) {
            if (c < 0x80 || is_surrogate(c)) continue;
            if (ignorecase_matches(t, extra, static_cast<char32_t>(letter), c)) out.push_back({letter, c});
        }
    }
    return out;
}

/// A scalar value as UTF-8 (appended).
inline void append_utf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0U | (c >> 6));
        out += static_cast<char>(0x80U | (c & 0x3FU));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0U | (c >> 12));
        out += static_cast<char>(0x80U | ((c >> 6) & 0x3FU));
        out += static_cast<char>(0x80U | (c & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (c >> 18));
        out += static_cast<char>(0x80U | ((c >> 12) & 0x3FU));
        out += static_cast<char>(0x80U | ((c >> 6) & 0x3FU));
        out += static_cast<char>(0x80U | (c & 0x3FU));
    }
}

}  // namespace odl::devtools_testing::ucd
