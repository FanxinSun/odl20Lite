// The devkit's JSON: what a manifest holds, what a manifest must never hold, and the layout the tools write back.  The strongest check is the
// committed manifest itself: it parses, and the writer returns it byte for byte.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/text.hpp>

#include <cstdint>
#include <memory>
#include <string>

using namespace odl::devkit;

namespace {

const JsonError* error_of(const std::string& text) {
    static thread_local std::unique_ptr<JsonError> keep;
    try {
        (void)Json::parse(text);
    } catch (const JsonError& e) {
        keep = std::make_unique<JsonError>(e);
        return keep.get();
    }
    return nullptr;
}

}  // namespace

TEST_CASE("JSON parses the six kinds and keeps an object's members in file order", "[devkit][json]") {
    const Json v = Json::parse(R"({"z": 1, "a": [true, false, null, 2.5, "s"], "m": {"y": {}, "x": []}})");
    REQUIRE(v.is_object());
    const auto& members = v.as_object();
    REQUIRE(members.size() == 3);
    CHECK(members[0].first == "z");
    CHECK(members[1].first == "a");
    CHECK(members[2].first == "m");
    CHECK(v.find("z")->as_int() == 1);
    const auto& a = v.find("a")->as_array();
    REQUIRE(a.size() == 5);
    CHECK(a[0].as_bool() == true);
    CHECK(a[1].as_bool() == false);
    CHECK(a[2].is_null());
    CHECK(a[3].as_double() == 2.5);
    CHECK(a[4].as_string() == "s");
    CHECK(v.find("m")->as_object()[0].first == "y");
    CHECK(v.find("absent") == nullptr);
    CHECK(Json(std::int64_t{3}).find("x") == nullptr);   // not an object: nothing to find
}

TEST_CASE("JSON strings: the escapes, UTF-8 passed through, surrogate pairs combined", "[devkit][json]") {
    const Json v = Json::parse(R"(["a\"b\\c\/d\b\f\n\r\t", "\u00e9", "\u20ac", "\ud83d\ude00", "é€😀"])");
    const auto& a = v.as_array();
    CHECK(a[0].as_string() == std::string("a\"b\\c/d\b\f\n\r\t"));
    CHECK(a[1].as_string() == "\xC3\xA9");
    CHECK(a[2].as_string() == "\xE2\x82\xAC");
    CHECK(a[3].as_string() == "\xF0\x9F\x98\x80");
    CHECK(a[4].as_string() == "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
}

TEST_CASE("JSON numbers: integers stay integers, the grammar is the standard one", "[devkit][json]") {
    const Json v = Json::parse("[0, -0, 7, -12, 3.0, 1e3, 1E-2, -2.5e+1, 9223372036854775807]");
    const auto& a = v.as_array();
    CHECK(a[0].is_int());
    CHECK(a[1].is_int());
    CHECK(a[1].as_int() == 0);
    CHECK(a[2].as_int() == 7);
    CHECK(a[3].as_int() == -12);
    CHECK_FALSE(a[4].is_int());
    CHECK(a[4].as_double() == 3.0);
    CHECK(a[5].as_double() == 1000.0);
    CHECK(a[6].as_double() == 0.01);
    CHECK(a[7].as_double() == -25.0);
    CHECK(a[8].as_int() == std::int64_t{9223372036854775807LL});
}

TEST_CASE("JSON refuses what Python 3.12's decoder refused, with its message and the line of the offence", "[devkit][json]") {
    struct Case {
        const char* text;
        const char* reason;
        std::size_t line;
    };
    const Case cases[] = {
        {"{\"schema\": 1, \"entries\": [", "Expecting value", 1},
        {"", "Expecting value", 1},
        {"[1, 2", "Expecting ',' delimiter", 1},
        {"[1 2]", "Expecting ',' delimiter", 1},
        {"{\"a\" 1}", "Expecting ':' delimiter", 1},
        {"{a: 1}", "Expecting property name enclosed in double quotes", 1},
        {"{\"a\": 1,}", "Expecting property name enclosed in double quotes", 1},
        {"[1,]", "Expecting value", 1},
        {"\"abc", "Unterminated string starting at", 1},
        {"[\"a\nb\"]", "Invalid control character at", 1},
        {"[\"\\q\"]", "Invalid \\escape", 1},
        {"[\"\\u12\"]", "Invalid \\uXXXX escape", 1},
        {"1 2", "Extra data", 1},
        {"{}\n\n  x", "Extra data", 3},
        {"[\n1,\n2,\n}", "Expecting value", 4},
        {"01", "Extra data", 1},
        {"-", "Expecting value", 1},
        {"NaN", "Expecting value", 1},
        {"Infinity", "Expecting value", 1},
        {"[\"\\ud800\"]", "Invalid \\uXXXX escape: lone surrogate", 1},
        {"99999999999999999999", "Number out of range", 1},
    };
    for (const Case& c : cases) {
        INFO("input: " << c.text);
        const JsonError* e = error_of(c.text);
        REQUIRE(e != nullptr);
        CHECK(e->reason == c.reason);
        CHECK(e->line == c.line);
    }
}

TEST_CASE("JSON refuses nesting deeper than it will follow", "[devkit][json]") {
    const std::string deep = std::string(600, '[') + std::string(600, ']');
    const JsonError* e = error_of(deep);
    REQUIRE(e != nullptr);
    CHECK(e->reason == "Nesting too deep");
    CHECK(error_of(std::string(100, '[') + std::string(100, ']')) == nullptr);
}

TEST_CASE("JSON: a repeated key keeps its first position and takes its last value, as a Python dict does", "[devkit][json]") {
    const Json v = Json::parse(R"({"a": 1, "b": 2, "a": 3})");
    REQUIRE(v.as_object().size() == 2);
    CHECK(v.as_object()[0].first == "a");
    CHECK(v.find("a")->as_int() == 3);
}

TEST_CASE("JSON truthiness is Python's: an empty string counts as missing", "[devkit][json]") {
    CHECK_FALSE(Json().truthy());
    CHECK_FALSE(Json(false).truthy());
    CHECK_FALSE(Json(std::int64_t{0}).truthy());
    CHECK_FALSE(Json(0.0).truthy());
    CHECK_FALSE(Json("").truthy());
    CHECK_FALSE(Json(Json::Array{}).truthy());
    CHECK_FALSE(Json(Json::Object{}).truthy());
    CHECK(Json(true).truthy());
    CHECK(Json(std::int64_t{-1}).truthy());
    CHECK(Json(" ").truthy());
    CHECK(Json::parse("[0]").truthy());
}

TEST_CASE("JSON writer: json.dumps(indent=2) exactly", "[devkit][json]") {
    const Json v = Json::parse(R"({"a": [1, 2, {"b": null}], "c": "é\n\u007f", "d": [], "e": {}, "f": "😀", "g": 1.5, "h": true})");
    const std::string want =
        "{\n"
        "  \"a\": [\n"
        "    1,\n"
        "    2,\n"
        "    {\n"
        "      \"b\": null\n"
        "    }\n"
        "  ],\n"
        "  \"c\": \"\\u00e9\\n\\u007f\",\n"
        "  \"d\": [],\n"
        "  \"e\": {},\n"
        "  \"f\": \"\\ud83d\\ude00\",\n"
        "  \"g\": 1.5,\n"
        "  \"h\": true\n"
        "}";
    CHECK(v.dumps(2) == want);
    CHECK(Json::parse(want).dumps(2) == want);
    CHECK(Json::parse("[]").dumps(2) == "[]");
    CHECK(Json(std::int64_t{5}).dumps(2) == "5");
    CHECK(Json("q\"\\\b\f\r\t\x01").dumps(2) == "\"q\\\"\\\\\\b\\f\\r\\t\\u0001\"");
}

TEST_CASE("JSON writer: floats are repr(float)", "[devkit][json]") {
    CHECK(Json(1.0).dumps(2) == "1.0");
    CHECK(Json(0.1).dumps(2) == "0.1");
    CHECK(Json(1e-5).dumps(2) == "1e-05");
    CHECK(Json(1e16).dumps(2) == "1e+16");
    CHECK(Json(1e15).dumps(2) == "1000000000000000.0");
    CHECK(Json(-0.0).dumps(2) == "-0.0");
    CHECK(Json(123456789012345678.0).dumps(2) == "1.2345678901234568e+17");
    CHECK(Json(0.0001).dumps(2) == "0.0001");
}

TEST_CASE("Python's float formats: repr and hex", "[devkit][pyfmt]") {
    CHECK(py_float_repr(2.5) == "2.5");
    CHECK(py_float_repr(100.0) == "100.0");
    CHECK(py_float_repr(1.0 / 3.0) == "0.3333333333333333");
    CHECK(py_float_repr(5e-324) == "5e-324");
    CHECK(py_float_repr(1.7976931348623157e308) == "1.7976931348623157e+308");
    CHECK(py_float_repr(12345.678) == "12345.678");
    CHECK(py_float_hex(1.0) == "0x1.0000000000000p+0");
    CHECK(py_float_hex(1.5) == "0x1.8000000000000p+0");
    CHECK(py_float_hex(0.5) == "0x1.0000000000000p-1");
    CHECK(py_float_hex(-2.0) == "-0x1.0000000000000p+1");
    CHECK(py_float_hex(0.0) == "0x0.0p+0");
    CHECK(py_float_hex(-0.0) == "-0x0.0p+0");
    CHECK(py_float_hex(5e-324) == "0x0.0000000000001p-1022");
    CHECK(py_float_hex(0.1) == "0x1.999999999999ap-4");
}

#ifdef ODL_MANIFEST_PATH
namespace {

// The text of one entry as it lies in the manifest: the writer's output, every line indented by four spaces.
std::string as_lying_in_the_manifest(const Json& entry, bool ensure_ascii) {
    std::string out = "    ";
    for (const char c : entry.dumps(2, ensure_ascii)) {
        out.push_back(c);
        if (c == '\n') out += "    ";
    }
    return out;
}

}  // namespace

TEST_CASE("the committed manifest parses, and the writer reproduces every entry of it byte for byte", "[devkit][json][manifest]") {
    const std::string text = read_text(ODL_MANIFEST_PATH);
    REQUIRE(valid_utf8(text));
    const Json doc = Json::parse(text);
    REQUIRE(doc.is_object());
    REQUIRE(doc.find("entries") != nullptr);
    const auto& entries = doc.find("entries")->as_array();
    CHECK(entries.size() >= 100);
    // The manifest was written by hand and by scripts calling json.dumps(entry, indent=2), the scripts with ensure_ascii on (a section sign is
    // §) and the hand with it off (a section sign is the character).  Each entry lies in the file exactly as the writer prints it in ONE
    // of the two modes: those that need the escapes are Python's own output, so this is the writer checked against Python's bytes.
    std::size_t ascii_only = 0;
    std::size_t raw_only = 0;
    for (const Json& e : entries) {
        const bool a = text.find(as_lying_in_the_manifest(e, true)) != std::string::npos;
        const bool r = text.find(as_lying_in_the_manifest(e, false)) != std::string::npos;
        INFO("entry " << e.find("id")->as_string());
        REQUIRE((a || r));
        if (a && !r) ++ascii_only;
        if (r && !a) ++raw_only;
    }
    INFO(ascii_only << " entries need the escapes, " << raw_only << " the raw characters");
    CHECK(ascii_only >= 10);
    CHECK(raw_only >= 10);
    // and reading what was written gives back the same document, in both modes
    CHECK(Json::parse(doc.dumps(2, true)) == doc);
    CHECK(Json::parse(doc.dumps(2, false)) == doc);
}
#endif
