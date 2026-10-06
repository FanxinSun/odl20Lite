// The devkit's text helpers: UTF-8, Python's padding and repr.  Every expected string is what Python 3 printed for the same call.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/text.hpp>

#include <string>

using namespace odl::devkit;

TEST_CASE("UTF-8 validation accepts the scalar values and refuses the rest", "[devkit][text]") {
    CHECK(valid_utf8(""));
    CHECK(valid_utf8("plain ASCII"));
    CHECK(valid_utf8("\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80"));
    CHECK(valid_utf8("\xF4\x8F\xBF\xBF"));            // U+10FFFF
    CHECK_FALSE(valid_utf8("\xC3"));                   // truncated
    CHECK_FALSE(valid_utf8("\xC0\x80"));               // overlong NUL
    CHECK_FALSE(valid_utf8("\xE0\x80\x80"));           // overlong
    CHECK_FALSE(valid_utf8("\xED\xA0\x80"));           // a surrogate
    CHECK_FALSE(valid_utf8("\xF4\x90\x80\x80"));       // above U+10FFFF
    CHECK_FALSE(valid_utf8("\xFF"));
    CHECK_FALSE(valid_utf8("a\x80z"));                 // a stray continuation byte
}

TEST_CASE("code points are counted, not bytes, and padding follows them", "[devkit][text]") {
    CHECK(code_points("") == 0);
    CHECK(code_points("abc") == 3);
    CHECK(code_points("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80") == 3);
    CHECK(pad_right("ab", 5) == "ab   ");
    CHECK(pad_right("abcdef", 3) == "abcdef");
    CHECK(pad_right("\xC3\xA9", 3) == "\xC3\xA9  ");
    CHECK(pad_left("ab", 5) == "   ab");
    CHECK(pad_left("abcdef", 3) == "abcdef");
}

TEST_CASE("append_utf8 and next_code_point are inverses", "[devkit][text]") {
    for (const std::uint32_t cp : {0x24U, 0x7FU, 0x80U, 0x7FFU, 0x800U, 0xFFFDU, 0xFFFFU, 0x10000U, 0x1F600U, 0x10FFFFU}) {
        std::string s;
        append_utf8(s, cp);
        std::size_t i = 0;
        std::uint32_t back = 0;
        REQUIRE(next_code_point(s, i, back));
        CHECK(back == cp);
        CHECK(i == s.size());
    }
}

TEST_CASE("repr(str) is Python's", "[devkit][text]") {
    CHECK(py_repr("thing") == "'thing'");
    CHECK(py_repr("it's") == "\"it's\"");
    CHECK(py_repr("say \"hi\"") == "'say \"hi\"'");
    CHECK(py_repr("both ' and \"") == "'both \\' and \"'");
    CHECK(py_repr("tab\there\nnewline\r\\") == "'tab\\there\\nnewline\\r\\\\'");
    CHECK(py_repr(std::string("nul\0x", 5)) == "'nul\\x00x'");
    CHECK(py_repr("\x7f") == "'\\x7f'");
    CHECK(py_repr("\xC3\xA9") == "'\xC3\xA9'");                 // é is printable: shown as is
    CHECK(py_repr("\xC2\xA0") == "'\\xa0'");                    // a no-break space is not
    CHECK(py_repr("\xE2\x80\xA8") == "'\\u2028'");              // the line separator
    CHECK(py_repr("\xEF\xBB\xBF") == "'\\ufeff'");              // the byte-order mark
    CHECK(py_repr("") == "''");
}

TEST_CASE("repr(bytes) is Python's", "[devkit][text]") {
    CHECK(py_repr_bytes(as_bytes("PK\x03\x04")) == "b'PK\\x03\\x04'");
    CHECK(py_repr_bytes(as_bytes("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n<html>")) == "b'<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\\n<html>'");
    CHECK(py_repr_bytes(as_bytes("it's")) == "b\"it's\"");
    CHECK(py_repr_bytes(as_bytes("\x1f\x8b")) == "b'\\x1f\\x8b'");
    CHECK(py_repr_bytes(as_bytes("\x7f\xff")) == "b'\\x7f\\xff'");
    CHECK(py_repr_bytes(ByteView{}) == "b''");
}
