// tests/devtools/unicode_tables_tests.cpp — the Unicode facts the C++ development tools rest on, DERIVED from the Unicode Character Database files the manifest pins (plan L0 step 8, group C6).
// (ctests `unicode_tables.parser` and `unicode_tables.derivation`.)
//
// Two tools stand in for a Python behaviour that depends on Unicode's case mappings:
//
//   * tools/literaturecheck.cpp reads `terms.upper().startswith("NOT ESTABLISHED")`.  Python's upper() is the FULL case mapping, so some non-ASCII characters become ASCII letters.
//   * tools/unitcheck.cpp reads `re.search(r"\bkm\b|kilomet", what, re.I)`.  Python's IGNORECASE equates some non-ASCII characters with ASCII letters.
//
// The first version of both was written against a witness that is not in the tree (a one-off run of another language's case mapping; the documentation's own sentence).  This file is what replaces it: it
// reads UnicodeData.txt and SpecialCasing.txt of UCD 15.1.0 from the cache (the bytes the manifest pins, hash-checked here), derives the two facts by the rules in ucd_case_tables.hpp, and holds each
// tool to the derivation over EVERY scalar value, black box: the tool is called on texts that put the character where each letter of the phrase (or of the pattern) stands, and its answer must be what the
// derivation says Python's would be.
//
// The derivation is checked against the one place where it meets Python's own statements: the number and the names of the non-ASCII letters that IGNORECASE equates with an ASCII letter (four, named in
// docs.python.org/3.13/library/re.html), and the standard library's whole table of extra cases (re/_casefix.py of the 3.13.15 that made the baselines: the same 50 entries, compared once as a file --
// the L0-8 report's C6 evidence -- since the table is Python's and is not copied here).
//
// Every expectation that is not a derivation is derived by hand and written where it is used.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/sha256.hpp>

#include "literaturecheck.hpp"
#include "ucd_case_tables.hpp"
#include "unitcheck.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dk = odl::devkit;
namespace uc = odl::devtools_testing::ucd;
namespace lc = odl::tools::literaturecheck;
namespace un = odl::tools::unitcheck;
namespace fs = std::filesystem;

namespace {

constexpr const char* kUnicodeDataId = "ucd-unicodedata";
constexpr const char* kSpecialCasingId = "ucd-specialcasing";

struct Pinned {
    std::string unicode_data;
    std::string special_casing;
    uc::CaseTables tables;
    uc::ExtraCases extra;
};

const dk::Json* entry_of(const dk::Json& manifest, const std::string& id) {
    const dk::Json* entries = manifest.find("entries");
    if (entries == nullptr) return nullptr;
    for (const dk::Json& e : entries->as_array()) {
        const dk::Json* name = e.find("id");
        if (name != nullptr && name->is_string() && name->as_string() == id) return &e;
    }
    return nullptr;
}

// The pinned bytes of one entry, from the cache, after their SHA-256 has been compared with the manifest's: a derivation made on other bytes would be a statement about something else.
std::string pinned_text(const dk::Json& manifest, const std::string& id) {
    const dk::Json* e = entry_of(manifest, id);
    if (e == nullptr) throw std::runtime_error("the manifest has no entry " + id);
    const fs::path file = fs::path(ODL_MANIFEST_CACHE_ROOT) / id / e->find("filename")->as_string();
    if (!fs::exists(file)) throw std::runtime_error(file.string() + " is not in the cache: run tools/bootstrap.sh (or the host `fetch fetch`)");
    const std::string have = dk::sha256_file_hex(file);
    if (have != e->find("sha256")->as_string()) throw std::runtime_error(file.string() + " has sha256 " + have + ", the manifest pins " + e->find("sha256")->as_string());
    return dk::read_text(file);
}

const Pinned& pinned() {
    static const Pinned p = [] {
        const dk::Json manifest = dk::Json::parse(dk::read_text(ODL_MANIFEST_PATH));
        Pinned out;
        out.unicode_data = pinned_text(manifest, kUnicodeDataId);
        out.special_casing = pinned_text(manifest, kSpecialCasingId);
        out.tables = uc::read_tables(out.unicode_data, out.special_casing);
        out.extra = uc::derive_extra_cases(out.tables);
        return out;
    }();
    return p;
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

template <typename F>
std::string thrown(F&& f) {
    try {
        f();
    } catch (const std::exception& exc) {
        return exc.what();
    }
    return "";
}

std::string lowered(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// every scalar value in [first, last]: a surrogate is no character a UTF-8 text can hold
template <typename F>
void for_each_scalar(char32_t first, char32_t last, F&& f) {
    for (char32_t c = first; c <= last; ++c) {
        if (uc::is_surrogate(c)) continue;
        f(c);
    }
}

// the one line of a file that starts with `prefix`, removed (the line must be there exactly once)
std::string without_line_starting(const std::string& text, const std::string& prefix) {
    std::string out;
    std::size_t found = 0;
    std::size_t from = 0;
    while (from < text.size()) {
        std::size_t end = text.find('\n', from);
        end = end == std::string::npos ? text.size() : end + 1;
        if (text.compare(from, prefix.size(), prefix) == 0) {
            ++found;
        } else {
            out.append(text, from, end - from);
        }
        from = end;
    }
    if (found != 1) throw std::logic_error("the file has " + std::to_string(found) + " lines starting with '" + prefix + "', not one");
    return out;
}

struct Expected {
    char32_t code_point;
    const char* upper;
};

// P1, derived by hand from the names in the two files: the sharp s makes SS; the dotless i and the long s make I and S; the ff, fi, fl, ffi and ffl ligatures make FF, FI, FL, FFI and FFL; the
// long-s-t and st ligatures both make ST.
const Expected kTenUppers[] = {
    {0x00DF, "SS"}, {0x0131, "I"}, {0x017F, "S"},  {0xFB00, "FF"}, {0xFB01, "FI"},
    {0xFB02, "FL"}, {0xFB03, "FFI"}, {0xFB04, "FFL"}, {0xFB05, "ST"}, {0xFB06, "ST"},
};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// the parser and the derivation on files small enough to read, and shown able to refuse and to be wrong
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

// Excerpts in the real files' shape: fifteen fields per line (the case mappings are fields 12 and 13), `code; lower; title; upper; # name` and, with a condition list, `...; condition; # name`.
const char kSmallUnicodeData[] =
    "0049;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;0069;\n"
    "004B;LATIN CAPITAL LETTER K;Lu;0;L;;;;;N;;;;006B;\n"
    "0053;LATIN CAPITAL LETTER S;Lu;0;L;;;;;N;;;;0073;\n"
    "0069;LATIN SMALL LETTER I;Ll;0;L;;;;;N;;;0049;;0049\n"
    "006B;LATIN SMALL LETTER K;Ll;0;L;;;;;N;;;004B;;004B\n"
    "0073;LATIN SMALL LETTER S;Ll;0;L;;;;;N;;;0053;;0053\n"
    "00DF;LATIN SMALL LETTER SHARP S;Ll;0;L;;;;;N;;;;;\n"
    "0130;LATIN CAPITAL LETTER I WITH DOT ABOVE;Lu;0;L;0049 0307;;;;N;LATIN CAPITAL LETTER I DOT;;;0069;\n"
    "0131;LATIN SMALL LETTER DOTLESS I;Ll;0;L;;;;;N;;;0049;;0049\n"
    "017F;LATIN SMALL LETTER LONG S;Ll;0;L;<compat> 0073;;;;N;;;0053;;0053\n"
    "212A;KELVIN SIGN;Lu;0;L;004B;;;;N;DEGREES KELVIN;;;006B;\n"
    "FB05;LATIN SMALL LIGATURE LONG S T;Ll;0;L;<compat> 017F 0074;;;;N;;;;;\n";

const char kSmallSpecialCasing[] =
    "# a comment line\n"
    "\n"
    "00DF; 00DF; 0053 0073; 0053 0053; # LATIN SMALL LETTER SHARP S\n"
    "0130; 0069 0307; 0130; 0130; # LATIN CAPITAL LETTER I WITH DOT ABOVE\n"
    "03A3; 03C2; 03A3; 03A3; Final_Sigma; # GREEK CAPITAL LETTER SIGMA\n"
    "FB05; FB05; 0053 0074; 0053 0054; # LATIN SMALL LIGATURE LONG S T\n";

}  // namespace

TEST_CASE("the tables are read as the files' lines say: simple mappings from fields 12 and 13, unconditional full mappings from SpecialCasing, conditional ones counted and not used",
          "[unicode][parser]") {
    const uc::CaseTables t = uc::read_tables(kSmallUnicodeData, kSmallSpecialCasing);
    CHECK(t.unicode_data_lines == 12);
    CHECK(t.special_casing_unconditional == 3);
    CHECK(t.special_casing_conditional == 1);

    // field 12 is the UPPER case and 13 the LOWER case: a capital has a lower case only, a small letter an upper case only
    CHECK(t.simple_upper.at(0x0069) == 0x0049);
    CHECK(t.simple_lower.at(0x0049) == 0x0069);
    CHECK(t.simple_lower.at(0x212A) == 0x006B);
    CHECK(t.simple_upper.count(0x0049) == 0);
    CHECK(t.simple_lower.count(0x0069) == 0);
    CHECK(t.simple_upper.count(0x00DF) == 0);          // the sharp s has no simple upper case: only SpecialCasing has one

    // the full mappings: SpecialCasing first, then the simple one, then the character itself
    CHECK(uc::full_upper(t, 0x00DF) == U"SS");
    CHECK(uc::full_upper(t, 0xFB05) == U"ST");
    CHECK(uc::full_upper(t, 0x0131) == U"I");           // from UnicodeData's field 12: its SpecialCasing line is absent
    CHECK(uc::full_upper(t, 0x0130) == std::u32string(1, 0x0130));   // its unconditional entry maps it to itself
    CHECK(uc::full_upper(t, 0x00E9) == std::u32string(1, 0x00E9));   // no entry anywhere: itself
    CHECK(uc::full_lower(t, 0x0130) == (std::u32string{0x0069, 0x0307}));
    CHECK(uc::full_lower(t, 0x03A3) == std::u32string(1, 0x03A3)); // only the CONDITIONAL entry (final sigma) mentions it: not used, as str.upper() and lower() of the table do not
    CHECK(uc::lower1(t, 0x0130) == 0x0069);
    CHECK(uc::lower1(t, 0x212A) == 0x006B);
    CHECK(uc::lower1(t, 0x0131) == 0x0131);
}

TEST_CASE("the ten, the extra cases and the equivalences are derived from the excerpt by hand-checkable steps", "[unicode][parser]") {
    const uc::CaseTables t = uc::read_tables(kSmallUnicodeData, kSmallSpecialCasing);

    // the non-ASCII code points with an entry and a pure-ASCII upper case: U+00DF (SS), U+0131 (I), U+017F (S), U+FB05 (ST); not U+0130 (its upper case is itself) and not U+212A (itself)
    const std::vector<uc::AsciiUpper> ascii = uc::non_ascii_with_ascii_upper(t);
    REQUIRE(ascii.size() == 4);
    CHECK(ascii[0].code_point == 0x00DF);
    CHECK(ascii[0].upper == "SS");
    CHECK(ascii[1].code_point == 0x0131);
    CHECK(ascii[1].upper == "I");
    CHECK(ascii[2].code_point == 0x017F);
    CHECK(ascii[2].upper == "S");
    CHECK(ascii[3].code_point == 0xFB05);
    CHECK(ascii[3].upper == "ST");

    // groups by upper case: "I" = {I, i, dotless i} and "S" = {S, s, long s}, whose lower-case codes differ (0069, 0131) and (0073, 017F); "K" = {K, k} has ONE lower-case code; the Kelvin sign is its own upper case
    const uc::ExtraCases extra = uc::derive_extra_cases(t);
    REQUIRE(extra.size() == 4);
    CHECK(extra.at(0x0069) == std::set<char32_t>{0x0131});
    CHECK(extra.at(0x0131) == std::set<char32_t>{0x0069});
    CHECK(extra.at(0x0073) == std::set<char32_t>{0x017F});
    CHECK(extra.at(0x017F) == std::set<char32_t>{0x0073});
    CHECK(extra.count(0x006B) == 0);

    // re.IGNORECASE: the Kelvin sign matches k by its LOWER case; the dotless i matches i by an EXTRA case; the dotted capital I by its lower case i; the long s matches s by an extra case
    CHECK(uc::ignorecase_matches(t, extra, U'k', 0x212A));
    CHECK(uc::ignorecase_matches(t, extra, U'K', 0x212A));
    CHECK(uc::ignorecase_matches(t, extra, U'i', 0x0130));
    CHECK(uc::ignorecase_matches(t, extra, U'i', 0x0131));
    CHECK(uc::ignorecase_matches(t, extra, U's', 0x017F));
    CHECK_FALSE(uc::ignorecase_matches(t, extra, U's', 0x0131));
    CHECK_FALSE(uc::ignorecase_matches(t, extra, U'k', 0x0131));
    CHECK_FALSE(uc::ignorecase_matches(t, extra, U'k', 0x00DF));

    const std::vector<uc::Equivalent> eq = uc::non_ascii_equivalents_of_ascii_letters(t, extra);
    REQUIRE(eq.size() == 4);
    CHECK(eq[0].letter == 'i');
    CHECK(eq[0].code_point == 0x0130);
    CHECK(eq[1].letter == 'i');
    CHECK(eq[1].code_point == 0x0131);
    CHECK(eq[2].letter == 'k');
    CHECK(eq[2].code_point == 0x212A);
    CHECK(eq[3].letter == 's');
    CHECK(eq[3].code_point == 0x017F);
}

TEST_CASE("the readers take the shapes the files may have: no final newline, padded and compact fields, the last code point", "[unicode][parser]") {
    // no newline after the last line (the files end with one; a reader that drops the last piece must not drop a line)
    uc::CaseTables a;
    uc::read_unicode_data(a, "0049;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;0069;");
    CHECK(a.unicode_data_lines == 1);
    CHECK(a.simple_lower.at(0x0049) == 0x0069);
    uc::CaseTables b;
    uc::read_special_casing(b, "00DF; 00DF; 0053 0073; 0053 0053;");
    CHECK(b.special_casing_unconditional == 1);
    CHECK(b.special_upper.at(0x00DF) == U"SS");
    // the last code point of Unicode
    uc::CaseTables c;
    uc::read_unicode_data(c, "10FFFF;LAST;Ll;0;L;;;;;N;;;10FFFF;;\n");
    CHECK(c.simple_upper.at(0x10FFFF) == 0x10FFFF);
    // fields padded with spaces and tabs on both sides, and fields with no padding at all
    uc::CaseTables d;
    uc::read_special_casing(d, "\t  00DF \t;  00DF ; 0053 0073 ;\t0053 0053 ;\t# a comment\n");
    CHECK(d.special_upper.at(0x00DF) == U"SS");
    CHECK(d.special_lower.at(0x00DF) == std::u32string(1, 0x00DF));
    uc::CaseTables e;
    uc::read_special_casing(e, "00DF;00DF;0053 0073;0053 0053;\n");
    CHECK(e.special_upper.at(0x00DF) == U"SS");
    uc::CaseTables f;
    uc::read_special_casing(f, "00DF; 00DF; 0053 0073; 0053 0053 ; # a comment with a padded last field\n03A3; 03C2; 03A3; 03A3; Final_Sigma ; # a condition list that is padded\n");
    CHECK(f.special_casing_unconditional == 1);
    CHECK(f.special_casing_conditional == 1);
}

TEST_CASE("the boundaries of the derivation: U+0080 is the first non-ASCII code point, 'z' the last letter, and a chain of upper cases makes no group out of a character that has an upper case of its own", "[unicode][parser]") {
    // U+0080 has an ASCII upper case (A) and a lower case (a); U+00E0's upper case is U+0080, which is not ASCII; U+0150's lower case is z
    const uc::CaseTables t = uc::read_tables(
        "0080;AA;Lu;0;L;;;;;N;;;0041;0061;\n"
        "00E0;BB;Ll;0;L;;;;;N;;;0080;;\n"
        "0150;CC;Lu;0;L;;;;;N;;;;007A;\n",
        "");
    const std::vector<uc::AsciiUpper> ascii = uc::non_ascii_with_ascii_upper(t);
    REQUIRE(ascii.size() == 1);
    CHECK(ascii[0].code_point == 0x0080);
    CHECK(ascii[0].upper == "A");
    const uc::ExtraCases none = uc::derive_extra_cases(uc::read_tables("0150;CC;Lu;0;L;;;;;N;;;;007A;\n0080;AA;Lu;0;L;;;;;N;;;;0061;\n", ""));
    const std::vector<uc::Equivalent> eq = uc::non_ascii_equivalents_of_ascii_letters(uc::read_tables("0150;CC;Lu;0;L;;;;;N;;;;007A;\n0080;AA;Lu;0;L;;;;;N;;;;0061;\n", ""), none);
    REQUIRE(eq.size() == 2);
    CHECK(eq[0].letter == 'a');
    CHECK(eq[0].code_point == 0x0080);
    CHECK(eq[1].letter == 'z');
    CHECK(eq[1].code_point == 0x0150);
    // a -> b and b -> c as upper cases: the group of c is {b, c} (c is its own upper case), the group of b is {a} alone (b has an upper case of its own, c, so it is not in it): one pair of extra cases, not two
    const uc::CaseTables chain = uc::read_tables("0061;A;Ll;0;L;;;;;N;;;0062;;\n0062;B;Ll;0;L;;;;;N;;;0063;;\n", "");
    const uc::ExtraCases extra = uc::derive_extra_cases(chain);
    REQUIRE(extra.size() == 2);
    CHECK(extra.at(0x0062) == std::set<char32_t>{0x0063});
    CHECK(extra.at(0x0063) == std::set<char32_t>{0x0062});
}

TEST_CASE("append_utf8 writes each code point in its shortest form: the last of each length and the first of the next", "[unicode][parser]") {
    const auto enc = [](char32_t c) {
        std::string s;
        uc::append_utf8(s, c);
        return s;
    };
    CHECK(enc(0x00) == std::string(1, '\0'));
    CHECK(enc(0x7F) == "\x7F");
    CHECK(enc(0x80) == "\xC2\x80");
    CHECK(enc(0x7FF) == "\xDF\xBF");
    CHECK(enc(0x800) == "\xE0\xA0\x80");
    CHECK(enc(0xFFFF) == "\xEF\xBF\xBF");
    CHECK(enc(0x10000) == "\xF0\x90\x80\x80");
    CHECK(enc(0x10FFFF) == "\xF4\x8F\xBF\xBF");
    CHECK(enc(0x212A) == "\xE2\x84\xAA");   // KELVIN SIGN
    CHECK(enc(0x00E9) == "\xC3\xA9");
}

TEST_CASE("a file of another shape is refused, with the line named", "[unicode][parser]") {
    const auto read = [](const std::string& unicode_data, const std::string& special_casing) { (void)uc::read_tables(unicode_data, special_casing); };
    const std::string good_line = "0049;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;0069;\n";

    // UnicodeData.txt
    CHECK(contains(thrown([&] { read(good_line + "0069;LATIN SMALL LETTER I;Ll;0;L;;;;;N;;;0049;0049\n", ""); }), "UnicodeData.txt line 2: 14 fields, not 15"));
    CHECK(contains(thrown([&] { read(good_line + good_line, ""); }), "UnicodeData.txt line 2: the code point occurs twice"));
    CHECK(contains(thrown([&] { read("0049;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;0069a;\n", ""); }), "UnicodeData.txt line 1: '0069a' is not a code point (upper-case hex digits only)"));
    CHECK(contains(thrown([&] { read("0049;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;0069;\n0059;X;Lu;0;L;;;;;N;;;;0079;\n0070;LATIN SMALL LETTER P;Ll;0;L;;;;;N;;;00dz;;\n", ""); }),
                   "UnicodeData.txt line 3"));
    CHECK(contains(thrown([&] { read("0049;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;110000;\n", ""); }), "'110000' is beyond U+10FFFF"));
    CHECK(contains(thrown([&] { read("49;LATIN CAPITAL LETTER I;Lu;0;L;;;;;N;;;;0069;\n", ""); }), "'49' is not a code point (4 to 6 hex digits)"));
    CHECK(contains(thrown([&] { read(good_line + "\n", ""); }), "UnicodeData.txt line 2: 1 fields, not 15"));          // a blank line is no line of the file
    CHECK(thrown([&] { read(good_line, ""); }).empty());

    // SpecialCasing.txt
    CHECK(contains(thrown([&] { read("", "00DF; 00DF; 0053 0073; 0053 0053\n"); }), "SpecialCasing.txt line 1: neither"));                       // no closing semicolon
    CHECK(contains(thrown([&] { read("", "00DF; 00DF; 0053 0073;\n"); }), "SpecialCasing.txt line 1: neither"));
    CHECK(contains(thrown([&] { read("", "00DF; 00DF; 0053 0073; 0053 0053; tr; extra;\n"); }), "SpecialCasing.txt line 1: neither"));
    CHECK(contains(thrown([&] { read("", "00DF; 00DF; 0053 0073; 0053 0053; ;\n"); }), "SpecialCasing.txt line 1: neither"));         // six fields, the fifth blank: no condition list, and no unconditional entry has six
    CHECK(contains(thrown([&] { read("", "00DF; 00DF; 0053 0073; 0053 0053; tr; extra\n"); }), "SpecialCasing.txt line 1: neither"));   // six fields, the sixth not empty: a condition list is followed by nothing
    CHECK(contains(thrown([&] { read("", "00DF; 00DF; 0053 0073; 0053 0053; tr; ; x\n"); }), "SpecialCasing.txt line 1: neither"));    // seven: a condition, an empty field, and one more
    CHECK(contains(thrown([&] { read("", "# c\n00DF; 00DF; 0053 0073; 0053 0053;\n00DF; 00DF; 0053 0073; 0053 0053;\n"); }), "SpecialCasing.txt line 3: a second unconditional entry"));
    CHECK(contains(thrown([&] { read("", "00DF; ; 0053 0073; 0053 0053;\n"); }), "SpecialCasing.txt line 1: an empty mapping"));
    CHECK(thrown([&] { read("", "# only a comment\n\n"); }).empty());
    CHECK(thrown([&] { read("", "03A3; 03C2; 03A3; 03A3; Final_Sigma; # sigma\n"); }).empty());   // a conditional entry is well-formed and not kept

    // a group of code points with one upper case, one of which lowercases to two code points, is what the table's own generator would die on: no table exists to compare with
    const uc::CaseTables bad = uc::read_tables(
        "0041;A;Lu;0;L;;;;;N;;;;0061;\n"
        "0061;a;Ll;0;L;;;;;N;;;0041;;0041\n",
        "0061; 0061 0301; 0041; 0041;\n");
    CHECK(contains(thrown([&] { (void)uc::derive_extra_cases(bad); }), "whose lower case is not one code point"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// the pinned files
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("the pinned files are the bytes the manifest pins, at the version the baselines' Python carries, under the licence the manifest records", "[unicode][derivation]") {
    const dk::Json manifest = dk::Json::parse(dk::read_text(ODL_MANIFEST_PATH));
    const struct {
        const char* id;
        const char* filename;
        std::size_t bytes;
    } wanted[] = {{kUnicodeDataId, "UnicodeData.txt", 1914200}, {kSpecialCasingId, "SpecialCasing.txt", 16832}};

    for (const auto& w : wanted) {
        INFO(w.id);
        const dk::Json* e = entry_of(manifest, w.id);
        REQUIRE(e != nullptr);
        CHECK(e->find("kind")->as_string() == "data");
        CHECK(e->find("filename")->as_string() == w.filename);
        CHECK(e->find("url")->as_string() == std::string("https://www.unicode.org/Public/15.1.0/ucd/") + w.filename);
        CHECK(e->find("licence")->as_string() == "Unicode-3.0");
        CHECK_FALSE(e->find("upstream_mutable")->as_bool());
        const std::string note = e->find("licence_note")->as_string();
        CHECK(contains(note, "Permission is hereby granted, free of charge, to any person obtaining a copy of data files"));
        CHECK(contains(note, "Copyright \xC2\xA9 1991-2026 Unicode, Inc."));
        CHECK(contains(note, "free and open-source Unicode License v3"));
        CHECK(contains(e->find("version")->as_string(), "15.1.0"));

        const fs::path file = fs::path(ODL_MANIFEST_CACHE_ROOT) / w.id / w.filename;
        REQUIRE(fs::exists(file));
        CHECK(fs::file_size(file) == w.bytes);
        CHECK(dk::sha256_file_hex(file) == e->find("sha256")->as_string());
    }

    const Pinned& p = pinned();   // which compares the hashes again and refuses on a difference
    CHECK(p.special_casing.rfind("# SpecialCasing-15.1.0.txt\n", 0) == 0);   // the file's own first line names its version
    // recounted another way (wc -l, and awk over SpecialCasing's non-comment lines) before this was written: 34931 lines, 103 unconditional and 16 conditional entries
    CHECK(p.tables.unicode_data_lines == 34931);
    CHECK(p.tables.special_casing_unconditional == 103);
    CHECK(p.tables.special_casing_conditional == 16);
}

TEST_CASE("the non-ASCII characters whose full upper case is pure ASCII are exactly ten, derived from the pinned files", "[unicode][derivation]") {
    const Pinned& p = pinned();
    const std::vector<uc::AsciiUpper> derived = uc::non_ascii_with_ascii_upper(p.tables);
    REQUIRE(derived.size() == std::size(kTenUppers));
    for (std::size_t i = 0; i < derived.size(); ++i) {
        INFO("derived #" << i << " = " << uc::detail::show(derived[i].code_point) << " -> " << derived[i].upper);
        CHECK(derived[i].code_point == kTenUppers[i].code_point);
        CHECK(derived[i].upper == kTenUppers[i].upper);
    }

    // the four that can stand in "NOT ESTABLISHED" are the ones literaturecheck maps: an upper case that is a substring of the phrase.  SS, FF, FI, FL, FFI and FFL are not (the phrase has no
    // doubled letter and no F).
    const std::string phrase = "NOT ESTABLISHED";
    std::vector<char32_t> in_phrase;
    for (const uc::AsciiUpper& a : derived) {
        if (phrase.find(a.upper) != std::string::npos) in_phrase.push_back(a.code_point);
    }
    CHECK(in_phrase == (std::vector<char32_t>{0x0131, 0x017F, 0xFB05, 0xFB06}));
}

TEST_CASE("the non-ASCII characters re.IGNORECASE equates with an ASCII letter are exactly the four the re documentation names, derived from the pinned files", "[unicode][derivation]") {
    const Pinned& p = pinned();
    const std::vector<uc::Equivalent> eq = uc::non_ascii_equivalents_of_ascii_letters(p.tables, p.extra);
    REQUIRE(eq.size() == 4);
    CHECK(eq[0].letter == 'i');
    CHECK(eq[0].code_point == 0x0130);   // LATIN CAPITAL LETTER I WITH DOT ABOVE: its lower case is i
    CHECK(eq[1].letter == 'i');
    CHECK(eq[1].code_point == 0x0131);   // LATIN SMALL LETTER DOTLESS I: an extra case of i (the same upper case, I)
    CHECK(eq[2].letter == 'k');
    CHECK(eq[2].code_point == 0x212A);   // KELVIN SIGN: its lower case is k
    CHECK(eq[3].letter == 's');
    CHECK(eq[3].code_point == 0x017F);   // LATIN SMALL LETTER LONG S: an extra case of s

    // "they will match the 52 ASCII letters and 4 additional non-ASCII letters: U+0130, U+0131, U+017F and U+212A" (docs.python.org/3.13/library/re.html, IGNORECASE, read 2026-10-07)
    std::set<char32_t> distinct;
    for (const uc::Equivalent& e : eq) distinct.insert(e.code_point);
    CHECK(distinct == (std::set<char32_t>{0x0130, 0x0131, 0x017F, 0x212A}));
}

TEST_CASE("the derived table of extra cases is the standard library's, and the choices the derivation makes cannot matter on these files", "[unicode][derivation]") {
    const Pinned& p = pinned();

    // The standard library of the 3.13.15 that made the baselines holds 50 entries (re/_casefix.py, compared once, entry for entry, as a file: the L0-8 report's C6 evidence); a few of them by name.
    CHECK(p.extra.size() == 50);
    CHECK(p.extra.at(0x0069) == std::set<char32_t>{0x0131});
    CHECK(p.extra.at(0x0073) == std::set<char32_t>{0x017F});
    CHECK(p.extra.at(0x0131) == std::set<char32_t>{0x0069});
    CHECK(p.extra.at(0x017F) == std::set<char32_t>{0x0073});
    CHECK(p.extra.at(0x0345) == (std::set<char32_t>{0x03B9, 0x1FBE}));
    CHECK(p.extra.at(0xFB05) == std::set<char32_t>{0xFB06});
    CHECK(p.extra.at(0xFB06) == std::set<char32_t>{0xFB05});
    CHECK(p.extra.count(0x006B) == 0);   // the Kelvin sign is k's by a lower-case mapping, not by the table

    // re's lowering is the first code point of the FULL lower-case mapping; the simple mapping would do as well wherever the two differ in length: for every code point that SpecialCasing lowers
    // unconditionally, the first code point of the full mapping IS the simple mapping (or the character itself when UnicodeData has none).
    std::size_t checked = 0;
    for (const auto& kv : p.tables.special_lower) {
        const auto simple = p.tables.simple_lower.find(kv.first);
        const char32_t simple_lower = simple == p.tables.simple_lower.end() ? kv.first : simple->second;
        INFO(uc::detail::show(kv.first));
        CHECK(kv.second.front() == simple_lower);
        ++checked;
    }
    CHECK(checked == 103);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// each tool, held to the derivation over every scalar value
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

constexpr std::string_view kPhrase = "NOT ESTABLISHED";

// The places a character can stand in the phrase: every distinct slice of one to three letters (the longest ASCII upper case a character has is three), with the text before and after it.  Hand-derived
// count: 12 distinct letters (N O T space E S A B L I H D: T, E and S each occur twice), 14 distinct pairs, 13 distinct triples.
struct Slice {
    std::string before_lower, after_lower, before_upper, after_upper;
};

std::vector<Slice> phrase_slices() {
    const std::string phrase(kPhrase);
    std::set<std::string> seen;
    std::vector<Slice> out;
    for (std::size_t len = 1; len <= 3; ++len) {
        for (std::size_t at = 0; at + len <= phrase.size(); ++at) {
            if (!seen.insert(phrase.substr(at, len)).second) continue;
            Slice s;
            s.before_upper = phrase.substr(0, at);
            s.after_upper = phrase.substr(at + len);
            s.before_lower = lowered(s.before_upper);
            s.after_lower = lowered(s.after_upper);
            out.push_back(s);
        }
    }
    return out;
}

struct Sweep {
    std::vector<std::string> mismatches;   // the first few
    std::size_t texts = 0;
    std::size_t expected_true = 0;
    std::size_t got_true = 0;
    std::size_t differing = 0;
};

void note(Sweep& s, const std::string& what) {
    ++s.differing;
    if (s.mismatches.size() < 12) s.mismatches.push_back(what);
}

// `tool(text)` against terms.upper().startswith("NOT ESTABLISHED") as the derivation gives it, for the characters first..last put in every place of the phrase
Sweep sweep_not_established(const Pinned& p, char32_t first, char32_t last, const std::function<bool(std::string_view)>& tool) {
    static const std::vector<Slice> slices = phrase_slices();
    Sweep out;
    std::string text;
    for_each_scalar(first, last, [&](char32_t c) {
        // c's upper case as it would sit in the phrase: ASCII as it is; anything else as a byte no letter of the phrase is
        std::string derived;
        bool pure_ascii = true;
        for (const char32_t x : uc::full_upper(p.tables, c)) {
            if (x < 0x80) {
                derived += static_cast<char>(x);
            } else {
                derived += '\x01';
                pure_ascii = false;
            }
        }
        for (const Slice& s : slices) {
            text = s.before_lower;
            uc::append_utf8(text, c);
            text += s.after_lower;
            bool expected = false;
            if (pure_ascii) expected = (s.before_upper + derived + s.after_upper).compare(0, kPhrase.size(), kPhrase) == 0;
            const bool got = tool(text);
            ++out.texts;
            if (expected) ++out.expected_true;
            if (got) ++out.got_true;
            if (got != expected) note(out, uc::detail::show(c) + " in '" + text + "': derived " + (expected ? "true" : "false") + ", tool " + (got ? "true" : "false"));
        }
    });
    return out;
}

// the places a character can stand in `km` and in `kilomet`: (text before, the letter, text after)
struct Place {
    std::string before;
    char letter;
    std::string after;
};

std::vector<Place> km_places() {
    std::vector<Place> out;
    out.push_back({"", 'k', "m"});   // \bkm\b
    out.push_back({"k", 'm', ""});
    const std::string word = "kilomet";
    for (std::size_t i = 0; i < word.size(); ++i) out.push_back({word.substr(0, i), word[i], word.substr(i + 1)});
    return out;
}

// `tool(text)` against re.search(r"\bkm\b|kilomet", text, re.I) as the derivation gives it, for the characters first..last put in every place of `km` and of `kilomet`.  A character matches the letter it
// stands for iff IGNORECASE equates them; the \b of `km` are satisfied (the text is the two letters and nothing else), the three characters that can match being letters.
Sweep sweep_km(const Pinned& p, char32_t first, char32_t last, const std::function<bool(std::string_view)>& tool) {
    static const std::vector<Place> places = km_places();
    Sweep out;
    std::string text;
    for_each_scalar(first, last, [&](char32_t c) {
        const char32_t lower_c = uc::lower1(p.tables, c);
        for (const Place& pl : places) {
            const char32_t lower_letter = uc::lower1(p.tables, static_cast<char32_t>(pl.letter));
            bool expected = lower_c == lower_letter;
            if (!expected) {
                const auto it = p.extra.find(lower_letter);
                expected = it != p.extra.end() && it->second.count(lower_c) != 0;
            }
            text = pl.before;
            uc::append_utf8(text, c);
            text += pl.after;
            const bool got = tool(text);
            ++out.texts;
            if (expected) ++out.expected_true;
            if (got) ++out.got_true;
            if (got != expected) note(out, uc::detail::show(c) + " for '" + std::string(1, pl.letter) + "' in '" + text + "': derived " + (expected ? "true" : "false") + ", tool " + (got ? "true" : "false"));
        }
    });
    return out;
}

std::string listed(const Sweep& s) {
    std::string out;
    for (const std::string& m : s.mismatches) out += m + "\n";
    return out;
}

}  // namespace

TEST_CASE("literaturecheck's reading of terms.upper().startswith(\"NOT ESTABLISHED\") agrees with the derivation for every scalar value in every place of the phrase", "[unicode][derivation][sweep]") {
    const Pinned& p = pinned();
    CHECK(phrase_slices().size() == 39);

    const Sweep s = sweep_not_established(p, 0x80, uc::kLastCodePoint, [](std::string_view text) { return lc::scan::starts_not_established(text); });
    INFO(listed(s));
    CHECK(s.differing == 0);
    CHECK(s.texts == 39 * (0x10FFFF - 0x7F - 0x800));   // every scalar value from U+0080 (the surrogates, 0x800 of them, are no characters), 39 places each
    // hand-derived: the four characters that can stand in the phrase, each in the one place whose slice is its upper case: the dotless i for the I, the long s for the S (its first), the two ligatures
    // for the ST; and ASCII, which this sweep does not visit
    CHECK(s.expected_true == 4);
    CHECK(s.got_true == 4);
}

TEST_CASE("the sweep over the phrase is able to fail: a reading that forgets one of the four, or maps one too many, is shown", "[unicode][derivation][sweep]") {
    const Pinned& p = pinned();
    const auto real = [](std::string_view text) { return lc::scan::starts_not_established(text); };

    // forgets the st ligature (U+FB06): the text with it is NOT recognised
    const Sweep forgets = sweep_not_established(p, 0x100, 0xFFFF, [&](std::string_view text) { return text.find("\xEF\xAC\x86") == std::string_view::npos && real(text); });
    CHECK(forgets.differing == 1);
    CHECK(contains(listed(forgets), "U+FB06"));

    // maps the sharp s (U+00DF) to the S it is NOT (its upper case is SS): the text "not eßtablished" is wrongly recognised
    const Sweep too_many = sweep_not_established(p, 0x80, 0xFFFF, [&](std::string_view text) {
        std::string t(text);
        for (std::size_t at = t.find("\xC3\x9F"); at != std::string::npos; at = t.find("\xC3\x9F", at + 1)) t.replace(at, 2, "s");
        return real(t);
    });
    CHECK(too_many.differing == 1);
    CHECK(contains(listed(too_many), "U+00DF in 'not eßtablished'"));
}

TEST_CASE("unitcheck's reading of re.search(r\"\\bkm\\b|kilomet\", what, re.I) agrees with the derivation for every scalar value in every place of the two words", "[unicode][derivation][sweep]") {
    const Pinned& p = pinned();
    CHECK(km_places().size() == 9);

    const Sweep s = sweep_km(p, 0x80, uc::kLastCodePoint, [](std::string_view text) { return un::scan::names_km(text); });
    INFO(listed(s));
    CHECK(s.differing == 0);
    CHECK(s.texts == 9 * (0x10FFFF - 0x7F - 0x800));
    // hand-derived: the Kelvin sign for the k of `km` and of `kilomet` (two places); the dotted capital I and the dotless i for the i of `kilomet` (two characters, one place each)
    CHECK(s.expected_true == 4);
    CHECK(s.got_true == 4);
}

TEST_CASE("the sweep over km is able to fail: a reading that forgets the Kelvin sign, or the dotless i, is shown", "[unicode][derivation][sweep]") {
    const Pinned& p = pinned();
    const auto real = [](std::string_view text) { return un::scan::names_km(text); };

    const Sweep no_kelvin = sweep_km(p, 0x100, 0xFFFF, [&](std::string_view text) { return text.find("\xE2\x84\xAA") == std::string_view::npos && real(text); });
    CHECK(no_kelvin.differing == 2);
    CHECK(contains(listed(no_kelvin), "U+212A for 'k' in '\xE2\x84\xAAm'"));
    CHECK(contains(listed(no_kelvin), "U+212A for 'k' in '\xE2\x84\xAAilomet'"));

    const Sweep no_dotless = sweep_km(p, 0x100, 0xFFFF, [&](std::string_view text) { return text.find("\xC4\xB1") == std::string_view::npos && real(text); });
    CHECK(no_dotless.differing == 1);
    CHECK(contains(listed(no_dotless), "U+0131 for 'i' in 'k\xC4\xB1lomet'"));

    // the long s equates with s, which neither word has: a reading that matched it with an i would be wrong
    const Sweep long_s_as_i = sweep_km(p, 0x100, 0xFFFF, [&](std::string_view text) {
        std::string t(text);
        for (std::size_t at = t.find("\xC5\xBF"); at != std::string::npos; at = t.find("\xC5\xBF", at + 1)) t.replace(at, 2, "i");
        return real(t);
    });
    CHECK(long_s_as_i.differing == 1);
    CHECK(contains(listed(long_s_as_i), "U+017F for 'i' in 'k\xC5\xBFlomet'"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// the derivation shown able to fail on the real data
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("a pinned file with one line removed gives a different derivation: the checks above are not satisfied by any input", "[unicode][derivation]") {
    const Pinned& p = pinned();

    // without the long-s-t ligature's entry in SpecialCasing.txt the ligature keeps its own (non-ASCII) upper case: nine, not ten
    const uc::CaseTables no_ligature = uc::read_tables(p.unicode_data, without_line_starting(p.special_casing, "FB05;"));
    CHECK(no_ligature.special_casing_unconditional == 102);
    const std::vector<uc::AsciiUpper> nine = uc::non_ascii_with_ascii_upper(no_ligature);
    CHECK(nine.size() == 9);
    for (const uc::AsciiUpper& a : nine) CHECK(a.code_point != 0xFB05);

    // without the Kelvin sign's line in UnicodeData.txt it has no lower case: three equivalences, not four, and k has none
    const uc::CaseTables no_kelvin = uc::read_tables(without_line_starting(p.unicode_data, "212A;"), p.special_casing);
    const uc::ExtraCases extra = uc::derive_extra_cases(no_kelvin);
    const std::vector<uc::Equivalent> three = uc::non_ascii_equivalents_of_ascii_letters(no_kelvin, extra);
    CHECK(three.size() == 3);
    for (const uc::Equivalent& e : three) CHECK(e.letter != 'k');

    // without the dotless i's line in UnicodeData.txt (its upper case I) the group of I loses it: no extra case for i, and the dotless i matches nothing.  Three equivalences remain: the dotted capital I
    // (i), the Kelvin sign (k) and the long s (s).
    const uc::CaseTables no_dotless = uc::read_tables(without_line_starting(p.unicode_data, "0131;"), p.special_casing);
    const uc::ExtraCases extra_no_dotless = uc::derive_extra_cases(no_dotless);
    CHECK(extra_no_dotless.count(0x0069) == 0);
    const std::vector<uc::Equivalent> still = uc::non_ascii_equivalents_of_ascii_letters(no_dotless, extra_no_dotless);
    CHECK(still.size() == 3);
    for (const uc::Equivalent& e : still) CHECK(e.code_point != 0x0131);
}
