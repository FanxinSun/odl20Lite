// tools/devkit/tests/pytext_tests.cpp — Python's str methods and textwrap, as the NOTICE generator used them (ctest "devkit: ...").
//
// Where an expectation is not obvious it is DERIVED from the Python source by hand, not typed from memory of an output: the wrapped lines below follow
// textwrap.TextWrapper._wrap_chunks and _handle_long_word, the decoder's from the Unicode standard's own example (the "maximal subpart" table), and the diff's
// from difflib.unified_diff's grouping.  Every one of them is also exercised end to end: tools/notice.cpp's output is the committed NOTICE, byte for byte.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace odl::devkit;

namespace {

ByteView bytes_of(const std::vector<std::uint8_t>& v) { return ByteView{v}; }

std::string cps(std::initializer_list<std::uint32_t> list) {
    std::string out;
    for (const std::uint32_t cp : list) append_utf8(out, cp);
    return out;
}

using Lines = std::vector<std::string>;

Lines wrap(const std::string& text, std::size_t width, const std::string& initial = "", const std::string& subsequent = "") {
    WrapOptions o;
    o.width = width;
    o.initial_indent = initial;
    o.subsequent_indent = subsequent;
    return wrap_py(text, o);
}

}  // namespace

TEST_CASE("decode_utf8_replace is Python's bytes.decode('utf-8', 'replace'): one U+FFFD per maximal invalid subsequence", "[devkit][text]") {
    const std::string fffd = cps({0xFFFD});
    CHECK(decode_utf8_replace(bytes_of({})) == "");
    CHECK(decode_utf8_replace(bytes_of({'a', 'b', 'c'})) == "abc");
    CHECK(decode_utf8_replace(bytes_of({0xC3, 0xA9})) == cps({0xE9}));                      // a well-formed é
    CHECK(decode_utf8_replace(bytes_of({0xE2, 0x80, 0x94})) == cps({0x2014}));              // an em dash
    CHECK(decode_utf8_replace(bytes_of({0xF0, 0x9F, 0x98, 0x80})) == cps({0x1F600}));       // four bytes
    CHECK(decode_utf8_replace(bytes_of({'a', 0xE9, 'b'})) == "a" + fffd + "b");             // a lone Latin-1 é: one replacement (the CALCEPH licence file)
    CHECK(decode_utf8_replace(bytes_of({0xE9})) == fffd);

    // The Unicode standard's own example of the maximal-subpart policy (Table 3-8): 61 F1 80 80 E1 80 C2 62 80 63 80 BF 64
    //   -> a, U+FFFD for (F1 80 80), U+FFFD for (E1 80), U+FFFD for (C2), b, U+FFFD for (80), c, U+FFFD for (80), U+FFFD for (BF), d
    CHECK(decode_utf8_replace(bytes_of({0x61, 0xF1, 0x80, 0x80, 0xE1, 0x80, 0xC2, 0x62, 0x80, 0x63, 0x80, 0xBF, 0x64})) ==
          "a" + fffd + fffd + fffd + "b" + fffd + "c" + fffd + fffd + "d");

    // what is never a lead or never valid: continuation bytes, C0 and C1, F5..FF; overlongs; surrogates; above U+10FFFF
    CHECK(decode_utf8_replace(bytes_of({0x80})) == fffd);
    CHECK(decode_utf8_replace(bytes_of({0xC0, 0xAF})) == fffd + fffd);
    CHECK(decode_utf8_replace(bytes_of({0xFF})) == fffd);
    CHECK(decode_utf8_replace(bytes_of({0xE0, 0x80, 0x80})) == fffd + fffd + fffd);        // an overlong three-byte form
    CHECK(decode_utf8_replace(bytes_of({0xED, 0xA0, 0x80})) == fffd + fffd + fffd);        // a surrogate
    CHECK(decode_utf8_replace(bytes_of({0xF4, 0x90, 0x80, 0x80})) == fffd + fffd + fffd + fffd);   // U+110000
    // a sequence cut off by the end of the data is ONE replacement
    CHECK(decode_utf8_replace(bytes_of({0xE2, 0x82})) == fffd);
    CHECK(decode_utf8_replace(bytes_of({'x', 0xF0, 0x9F})) == "x" + fffd);
}

TEST_CASE("the Python character classes", "[devkit][text]") {
    for (const std::uint32_t cp : {0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x1CU, 0x1DU, 0x1EU, 0x1FU, 0x20U, 0x85U, 0xA0U, 0x1680U, 0x2000U, 0x200AU, 0x2028U, 0x2029U, 0x202FU, 0x205FU, 0x3000U}) {
        INFO("U+" << std::hex << cp);
        CHECK(is_py_space(cp));
    }
    for (const std::uint32_t cp : {0x08U, 0x0EU, 0x21U, 0x41U, 0xA1U, 0x200BU, 0x2027U, 0x202EU, 0xFEFFU}) {
        INFO("U+" << std::hex << cp);
        CHECK_FALSE(is_py_space(cp));
    }
    // \w: letters, decimal digits, the other numerics and the underscore; not punctuation, symbols or marks
    for (const std::uint32_t cp : {0x30U, 0x39U, 0x41U, 0x5AU, 0x5FU, 0x61U, 0x7AU, 0xAAU, 0xB2U, 0xB5U, 0xBCU, 0xC0U, 0xE9U, 0xFFU, 0x3A9U, 0x416U, 0x5D0U, 0x627U, 0x660U, 0x2082U, 0x2160U, 0x4E2DU, 0xFF10U}) {
        INFO("U+" << std::hex << cp);
        CHECK(is_py_word(cp));
    }
    for (const std::uint32_t cp : {0x20U, 0x21U, 0x2DU, 0x2EU, 0xA7U, 0xABU, 0xD7U, 0xF7U, 0x2013U, 0x2014U, 0x2018U, 0x2022U, 0x20ACU}) {
        INFO("U+" << std::hex << cp);
        CHECK_FALSE(is_py_word(cp));
    }
    // \d is the decimal digits only: the superscripts and fractions are \w but not \d
    CHECK(is_py_decimal(U'7'));
    CHECK(is_py_decimal(0x663U));
    CHECK(is_py_decimal(0xFF15U));
    CHECK_FALSE(is_py_decimal(0xB2U));
    CHECK_FALSE(is_py_decimal(0xBCU));
    CHECK_FALSE(is_py_decimal(U'a'));
}

TEST_CASE("rstrip, splitlines and expandtabs", "[devkit][text]") {
    CHECK(rstrip_py("") == "");
    CHECK(rstrip_py("abc") == "abc");
    CHECK(rstrip_py("abc \t\r\n") == "abc");
    CHECK(rstrip_py(" a b ") == " a b");
    CHECK(rstrip_py("   ") == "");
    CHECK(rstrip_py("x" + cps({0xA0, 0x3000, 0x2003, 0x85}) + " ") == "x");   // Unicode whitespace goes too
    CHECK(rstrip_py("x" + cps({0x200B})) == "x" + cps({0x200B}));              // a zero-width space is not whitespace
    CHECK(rstrip_py(cps({0xE9}) + "  ") == cps({0xE9}));

    CHECK(splitlines_py("") == Lines{});
    CHECK(splitlines_py("a") == Lines{"a"});
    CHECK(splitlines_py("a\n") == Lines{"a"});                // a final terminator does not start another line
    CHECK(splitlines_py("a\n\n") == Lines{"a", ""});
    CHECK(splitlines_py("\n") == Lines{""});
    CHECK(splitlines_py("a\nb") == Lines{"a", "b"});
    CHECK(splitlines_py("a\r\nb\rc") == Lines{"a", "b", "c"});   // \r\n is ONE boundary
    CHECK(splitlines_py("a\r\n\r\nb") == Lines{"a", "", "b"});
    // every boundary Python has: \v \f 0x1c 0x1d 0x1e U+0085 U+2028 U+2029
    CHECK(splitlines_py(std::string("a\x0b" "b\x0c" "c\x1c" "d\x1d" "e\x1e" "f") + cps({0x85}) + "g" + cps({0x2028}) + "h" + cps({0x2029}) + "i") ==
          Lines{"a", "b", "c", "d", "e", "f", "g", "h", "i"});
    CHECK(splitlines_py("a\tb c") == Lines{"a\tb c"});      // a tab is not a boundary

    CHECK(expandtabs_py("") == "");
    CHECK(expandtabs_py("abc") == "abc");
    CHECK(expandtabs_py("\t") == "        ");
    CHECK(expandtabs_py("a\tb") == "a       b");            // to the next multiple of 8
    CHECK(expandtabs_py("abcdefgh\tx") == "abcdefgh        x");
    CHECK(expandtabs_py("ab\tc\td", 4) == "ab  c   d");
    CHECK(expandtabs_py("a\nb\tc") == "a\nb       c");       // a newline starts the column count again
    CHECK(expandtabs_py("a\tb", 0) == "ab");                // a tab size of zero removes the tabs
    CHECK(expandtabs_py(cps({0xE9}) + "\tx") == cps({0xE9}) + "       x");   // columns are code points
}

TEST_CASE("wrap_py is textwrap.wrap: the chunks, the lines, the indents", "[devkit][text]") {
    CHECK(wrap("", 10) == Lines{});
    CHECK(wrap("   \t\n  ", 10) == Lines{});
    CHECK(wrap("hello", 10) == Lines{"hello"});
    CHECK(wrap("hello world", 5) == Lines{"hello", "world"});
    CHECK(wrap("aaa bbb ccc", 7) == Lines{"aaa bbb", "ccc"});
    CHECK(wrap("aaa bbb ccc", 11) == Lines{"aaa bbb ccc"});
    CHECK(wrap("aaa   bbb", 20) == Lines{"aaa   bbb"});                  // a run of blanks inside a line stays
    CHECK(wrap("  leading and trailing  ", 40) == Lines{"  leading and trailing"});   // leading blanks of the FIRST line stay, trailing go
    CHECK(wrap("a\nb\tc", 20) == Lines{"a b       c"});                  // tabs expand, then every whitespace character is one blank

    // the indents count against the width, and the first line has its own
    CHECK(wrap("one two three four", 10, "> ", "  ") == Lines{"> one two", "  three", "  four"});
    CHECK(wrap("alpha beta gamma", 20, "    note      ", "              ") == Lines{"    note      alpha", "              beta", "              gamma"});

    // hyphens: a break may follow a hyphen between letters; digits and a single letter do not qualify
    CHECK(wrap("well-known thing", 9) == Lines{"well-", "known", "thing"});
    CHECK(wrap("CC-BY-NC-4.0 licence", 6) == Lines{"CC-BY-", "NC-4.0", "licenc", "e"});   // "NC-" is followed by a digit: no break after it
    CHECK(wrap("pre-post", 8) == Lines{"pre-post"});
    CHECK(wrap("xxx a-b-cd", 8) == Lines{"xxx a-b-", "cd"});                      // "a-b-" is a chunk: a hyphen after a single letter qualifies when the letter before it does

    // an em dash between words is a chunk of its own, so a line may end before or after it
    CHECK(wrap("word--another word", 8) == Lines{"word--", "another", "word"});
    CHECK(wrap("one -- two", 40) == Lines{"one -- two"});
    CHECK(wrap("xx aaa--bbb", 8) == Lines{"xx aaa--", "bbb"});                   // the line may end after the dashes: "aaa", "--", "bbb" are three chunks

    // a word longer than the line is cut at the width; with break_long_words false it stays whole
    CHECK(wrap("abcdefghij", 4) == Lines{"abcd", "efgh", "ij"});
    CHECK(wrap("aaaa-bbbbbbbbbb", 8) == Lines{"aaaa-bbb", "bbbbbbb"});
    CHECK(wrap("abc def", 1) == Lines{"a", "b", "c", "d", "e", "f"});
    WrapOptions whole;
    whole.width = 4;
    whole.break_long_words = false;
    CHECK(wrap_py("abcdefghij kl", whole) == Lines{"abcdefghij", "kl"});

    // break_on_hyphens off: only whitespace is a place to break
    WrapOptions plain;
    plain.width = 9;
    plain.break_on_hyphens = false;
    CHECK(wrap_py("well-known thing", plain) == Lines{"well-know", "n thing"});    // only whitespace is a place to break, so the long word is cut

    // non-ASCII text counts code points, and § é – — are what the manifest's notes carry
    CHECK(wrap(cps({0xE9}) + cps({0xE9}) + " " + cps({0xE9}) + cps({0xE9}), 2) == Lines{cps({0xE9, 0xE9}), cps({0xE9, 0xE9})});
    CHECK(wrap("plan " + cps({0xA7}) + "5 constraint 3", 12) == Lines{"plan " + cps({0xA7}) + "5", "constraint 3"});
    CHECK(wrap("a " + cps({0x2014}) + " b", 3) == Lines{"a " + cps({0x2014}), "b"});

    // refusals
    WrapOptions none;
    none.width = 0;
    CHECK_THROWS_AS(wrap_py("x", none), std::invalid_argument);
    CHECK_THROWS_AS(wrap_py(std::string("a\xE9" "b"), WrapOptions{}), std::runtime_error);   // not UTF-8
}

TEST_CASE("unified_diff is difflib's format and context grouping", "[devkit][text]") {
    const auto numbered = [](int n) {
        Lines v;
        for (int i = 1; i <= n; ++i) v.push_back("l" + std::to_string(i));
        return v;
    };
    CHECK(unified_diff({}, {}, "A", "B").empty());
    CHECK(unified_diff(numbered(5), numbered(5), "A", "B").empty());

    // one line changed in the middle of ten: three lines of context each side
    Lines b = numbered(10);
    b[4] = "X";
    CHECK(unified_diff(numbered(10), b, "A", "B") ==
          Lines{"--- A", "+++ B", "@@ -2,7 +2,7 @@", " l2", " l3", " l4", "-l5", "+X", " l6", " l7", " l8"});

    // an insertion, a deletion
    Lines inserted = numbered(4);
    inserted.insert(inserted.begin() + 2, "new");
    CHECK(unified_diff(numbered(4), inserted, "A", "B") == Lines{"--- A", "+++ B", "@@ -1,4 +1,5 @@", " l1", " l2", "+new", " l3", " l4"});
    CHECK(unified_diff(inserted, numbered(4), "A", "B") == Lines{"--- A", "+++ B", "@@ -1,5 +1,4 @@", " l1", " l2", "-new", " l3", " l4"});

    // nothing before, nothing after: the empty range is "0,0" and "1,n"
    CHECK(unified_diff({}, {"x", "y"}, "A", "B") == Lines{"--- A", "+++ B", "@@ -0,0 +1,2 @@", "+x", "+y"});
    CHECK(unified_diff({"x", "y"}, {}, "A", "B") == Lines{"--- A", "+++ B", "@@ -1,2 +0,0 @@", "-x", "-y"});
    CHECK(unified_diff({"only"}, {"other"}, "A", "B") == Lines{"--- A", "+++ B", "@@ -1 +1 @@", "-only", "+other"});   // a one-line range is the bare number

    // two changes far apart are two hunks; two close together are one
    Lines far = numbered(30);
    far[2] = "P";
    far[26] = "Q";
    const Lines two = unified_diff(numbered(30), far, "A", "B");
    CHECK(two[2] == "@@ -1,6 +1,6 @@");
    CHECK(two.back() == " l30");
    CHECK(std::count_if(two.begin(), two.end(), [](const std::string& l) { return l.rfind("@@", 0) == 0; }) == 2);
    Lines near = numbered(30);
    near[2] = "P";
    near[8] = "Q";
    const Lines one = unified_diff(numbered(30), near, "A", "B");
    CHECK(std::count_if(one.begin(), one.end(), [](const std::string& l) { return l.rfind("@@", 0) == 0; }) == 1);
    // the context is a parameter
    CHECK(unified_diff(numbered(10), b, "A", "B", 1) == Lines{"--- A", "+++ B", "@@ -4,3 +4,3 @@", " l4", "-l5", "+X", " l6"});
}

TEST_CASE("the decimal digits of other scripts have their values, and a text-mode read has universal newlines", "[devkit][text]") {
    // what tools/speccheck.cpp's scanners rest on: Python's `\d` matches the digits of every script and int() reads them (the value is the offset in the block of ten)
    CHECK(py_decimal_value(U'0') == 0);
    CHECK(py_decimal_value(U'9') == 9);
    CHECK(py_decimal_value(0x665) == 5);    // ARABIC-INDIC DIGIT FIVE
    CHECK(py_decimal_value(0x967) == 1);    // DEVANAGARI DIGIT ONE
    CHECK(py_decimal_value(0xFF17) == 7);   // FULLWIDTH DIGIT SEVEN
    CHECK(py_decimal_value(0x1D7D9) == 1);  // MATHEMATICAL DOUBLE-STRUCK DIGIT ONE
    CHECK(py_decimal_value(0x1D7FF) == 9);  // MATHEMATICAL MONOSPACE DIGIT NINE
    CHECK(py_decimal_value(U'a') == -1);
    CHECK(py_decimal_value(0xB2) == -1);    // SUPERSCRIPT TWO is a number, not a decimal digit
    CHECK((py_decimal_value(0x665) >= 0) == is_py_decimal(0x665));
    // universal newlines: "\r\n" and a lone "\r" become "\n"; a "\r\r\n" is a CR line end and then a CRLF one
    CHECK(universal_newlines("a\r\nb\rc\nd\r\r\ne") == "a\nb\nc\nd\n\ne");
    CHECK(universal_newlines("").empty());
    CHECK(universal_newlines("no line ends") == "no line ends");
    CHECK(universal_newlines("\r") == "\n");
    CHECK(universal_newlines("\n\r") == "\n\n");
}

TEST_CASE("strip, lstrip(chars), split() and the code point at a byte, and Python's 'g' format", "[devkit][text]") {
    // what tools/budgetcheck.cpp's scanners rest on
    const std::string nbsp = cps({0xA0});
    const std::string em_space = cps({0x2003});
    CHECK(strip_py("  a b \t\n") == "a b");
    CHECK(strip_py(nbsp + "x" + em_space) == "x");   // U+00A0 and U+2003 are white space to str.strip()
    CHECK(strip_py("   ").empty());
    CHECK(strip_py("").empty());
    CHECK(lstrip_py(cps({0x3000}) + " x ") == "x ");   // U+3000, the ideographic space, then a blank
    CHECK(lstrip_py("x ") == "x ");
    CHECK(lstrip_py("\xff ") == "\xff ");   // text that is not UTF-8 stops the strip; a str cannot hold it
    CHECK(rstrip_py(lstrip_py(" \t x \n")) == strip_py(" \t x \n"));

    // str.lstrip(chars) takes a SET of characters: "<≤≈~ " is the five, in any order and number
    const std::string five = std::string("<") + cps({0x2264, 0x2248}) + "~ ";
    CHECK(lstrip_chars_py(cps({0x2264}) + "~ <5 mm", five) == "5 mm");
    CHECK(lstrip_chars_py("~~ ;x", ";, ") == "~~ ;x");   // the first character is not in the set
    CHECK(lstrip_chars_py(";, ;, a", ";, ") == "a");
    CHECK(lstrip_chars_py("abc", "").empty() == false);
    CHECK(lstrip_chars_py("abc", "") == "abc");
    CHECK(lstrip_chars_py("", "x").empty());
    CHECK(lstrip_chars_py(";;;", ";").empty());
    CHECK(lstrip_chars_py(cps({0x2264, 0x2264, 0x2248, 'q'}), five) == "q");

    // str.split(): runs of white space (U+00A0 included) separate, nothing is empty, and bytes that are not UTF-8 are part of a word
    CHECK(split_py(" a  b\tc\n") == std::vector<std::string>{"a", "b", "c"});
    CHECK(split_py("").empty());
    CHECK(split_py(" \t ").empty());
    CHECK(split_py("a" + nbsp + "b") == std::vector<std::string>{"a", "b"});
    CHECK(split_py("a\xff" "b c") == std::vector<std::string>{"a\xff" "b", "c"});
    CHECK(split_py("one") == std::vector<std::string>{"one"});

    // the code point at a byte, and the index after it
    std::size_t after = 99;
    CHECK(code_point_at("a", 0, after) == U'a');
    CHECK(after == 1);
    CHECK(code_point_at("\xc3\xa9x", 0, after) == 0xE9);
    CHECK(after == 2);
    CHECK(code_point_at("\xff", 0, after) == 0xFFFD);   // not UTF-8: one replacement of one byte
    CHECK(after == 1);
    CHECK(code_point_at("\xe2\x82", 0, after) == 0xFFFD);   // truncated: one of one byte, and the next call reads the stray continuation byte alone
    CHECK(after == 1);
    CHECK(code_point_at("\xe2\x82", 1, after) == 0xFFFD);
    CHECK(after == 2);
    CHECK(code_point_at("a\xe2\x82\xac", 1, after) == 0x20AC);   // the euro sign
    CHECK(after == 4);

    // format(v, ".6g"): printf's, except for the words of the non-finite values
    CHECK(py_format_g(7.5) == "7.5");
    CHECK(py_format_g(15.0) == "15");
    CHECK(py_format_g(100000.0) == "100000");        // the decimal exponent 5 is below the precision 6: fixed
    CHECK(py_format_g(1234567.0) == "1.23457e+06");  // 6 is not: scientific, two exponent digits at least
    CHECK(py_format_g(0.0001) == "0.0001");          // the exponent -4 is still fixed
    CHECK(py_format_g(1e-5) == "1e-05");
    CHECK(py_format_g(2.5e-16) == "2.5e-16");
    CHECK(py_format_g(1e22) == "1e+22");
    CHECK(py_format_g(0.0) == "0");
    CHECK(py_format_g(-0.0) == "-0");
    CHECK(py_format_g(-7.5) == "-7.5");
    CHECK(py_format_g(1.0 / 3.0, 4) == "0.3333");
    CHECK(py_format_g(0.5, 0) == "0.5");   // a precision of 0 is read as 1
    CHECK(py_format_g(std::numeric_limits<double>::infinity()) == "inf");
    CHECK(py_format_g(-std::numeric_limits<double>::infinity()) == "-inf");
    CHECK(py_format_g(std::numeric_limits<double>::quiet_NaN()) == "nan");
    CHECK(py_format_g(std::copysign(std::numeric_limits<double>::quiet_NaN(), -1.0)) == "nan");   // printf would write -nan
}
