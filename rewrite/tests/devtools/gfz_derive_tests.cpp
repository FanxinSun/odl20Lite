// tests/devtools/gfz_derive_tests.cpp — the derivative of GFZ's file that the tree vendors (ctest `gfz.derivation`; plan L0 step 8, group C1b).
//
// Three things are held here.  (1) The transformation itself, on synthetic files shaped as the real one is: the sunspot-number field of every data
// row becomes the marker and no other byte changes; every way a file can fail to be shaped so is refused.  (2) `check_derived`, which needs no
// original: every field of a row is read in its own columns and every field has its own refusal.  (3) The vendored file itself and the manifest
// entry that pins it: the pinned derivative, every invariant, and the injection of rule 5 -- one sunspot number left unchanged -- shown refused.
// The original upstream file exists only where somebody fetched it on 2026-09-18, so nothing here may need it.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/text.hpp>

#include "gfz_derive.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
using namespace odl::tools::gfz_derive;

namespace {

constexpr std::size_t kRowBytes = kRowChars + 1;   // with the line feed

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// Forty header lines, each starting with '#'.
std::string header() {
    std::string h;
    for (std::size_t i = 1; i <= kHeaderLines; ++i) h += "# synthetic header line " + std::to_string(i) + "\n";
    return h;
}

// One data row in the file's own layout (its header line 38): iiii ii ii iiiii fffff.f iiii ii, eight ff.fff, eight iiii, iiii, iii, ffffff.f ffffff.f i.
// `index` counts days from 1932-01-01 (January only); `sn` is the four characters of the sunspot-number field, blank included.
std::string make_row(int index, const std::string& sn, int d = 2) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d %02d %02d %5d %7.1f %4d %2d", 1932, 1, 1 + index, index, index + 0.5, 1352, 10 + index);
    std::string row = buf;
    for (int k = 0; k < 8; ++k) {
        std::snprintf(buf, sizeof buf, " %6.3f", ((k + index) % 9) + (k % 3) / 3.0);
        row += buf;
    }
    for (int k = 0; k < 8; ++k) {
        std::snprintf(buf, sizeof buf, " %4d", 3 + k + index);
        row += buf;
    }
    std::snprintf(buf, sizeof buf, "  %4d", 12 + index);
    row += buf;
    row += sn;
    std::snprintf(buf, sizeof buf, " %8.1f %8.1f %1d", 100.5 + index, 101.5 + index, d);
    row += buf;
    return row;
}

std::string file_of(const std::vector<std::string>& sn_fields) {
    std::string f = header();
    for (std::size_t i = 0; i < sn_fields.size(); ++i) f += make_row(static_cast<int>(i), sn_fields[i]) + "\n";
    return f;
}

// the byte offset of data row `n` (1-based) in a file made by file_of
std::size_t row_at(std::size_t n) { return header().size() + (n - 1) * kRowBytes; }

std::string refusal_of(const std::string& bytes) {
    try {
        (void)derive(bytes);
    } catch (const std::runtime_error& refused) {
        return refused.what();
    }
    return "<not refused>";
}

std::string violation_of(const std::string& bytes) { return check_derived(bytes).value_or("<no violation>"); }

// `good` with the characters at [at, at+text.size()) of data row n replaced
std::string with_at(std::string bytes, std::size_t n, std::size_t at, const std::string& text) {
    bytes.replace(row_at(n) + at, text.size(), text);
    return bytes;
}

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_tool(const std::vector<std::string>& argv, const Pins& pins) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = run_with(argv, Streams{out, err}, pins);
    return Result{code, out.str(), err.str()};
}

}  // namespace

TEST_CASE("the row builder agrees with the real file's own layout, and the constants agree with its header", "[gfz]") {
    // the first and the last data rows of the vendored file (the real values, the sunspot number as the vendored file has it)
    const std::string first = "1932 01 01     0     0.5 1352 10  3.333  2.667  2.333  2.667  3.333  2.667  3.333  3.333   18   12    9   12   18   12   18   18    15  -1     -1.0     -1.0 2";
    REQUIRE(first.size() == kRowChars);
    std::string built;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d %02d %02d %5d %7.1f %4d %2d", 1932, 1, 1, 0, 0.5, 1352, 10);
    built = buf;
    for (const double kp : {3.333, 2.667, 2.333, 2.667, 3.333, 2.667, 3.333, 3.333}) {
        std::snprintf(buf, sizeof buf, " %6.3f", kp);
        built += buf;
    }
    for (const int ap : {18, 12, 9, 12, 18, 12, 18, 18}) {
        std::snprintf(buf, sizeof buf, " %4d", ap);
        built += buf;
    }
    std::snprintf(buf, sizeof buf, "  %4d", 15);
    built += buf;
    built += std::string(kSnMarker);
    std::snprintf(buf, sizeof buf, " %8.1f %8.1f %1d", -1.0, -1.0, 2);
    built += buf;
    CHECK(built == first);
    CHECK(first.substr(kSnOffset, kSnWidth) == kSnMarker);
    CHECK(make_row(3, "  16").size() == kRowChars);
}

TEST_CASE("derive replaces the sunspot-number field of every data row, and no other byte", "[gfz]") {
    const std::vector<std::string> sn = {"  22", "   0", " 503", "  -1", "  16", "   7"};   // a two-digit, a zero, three digits, the marker already, ...
    const std::string original = file_of(sn);
    const Derived d = derive(original);
    CHECK(d.rows == 6);
    CHECK(d.changed == 5);                       // the row that already carried the marker is not counted
    REQUIRE(d.bytes.size() == original.size());

    // the transformation, stated independently of the implementation: the four bytes at 134..137 of each row, and nothing else
    std::string expected = original;
    for (std::size_t n = 1; n <= 6; ++n) expected.replace(row_at(n) + 134, 4, "  -1");
    CHECK(d.bytes == expected);
    for (std::size_t i = 0; i < original.size(); ++i) {
        bool in_field = false;
        for (std::size_t n = 1; n <= 6; ++n) in_field = in_field || (i >= row_at(n) + 134 && i < row_at(n) + 138);
        if (!in_field) {
            INFO("byte " << i);
            REQUIRE(d.bytes[i] == original[i]);
        }
    }
    CHECK(d.bytes.substr(0, header().size()) == header());   // the 40 header lines, the licence line included, untouched

    // idempotent: the derivative of the derivative is itself, with nothing changed
    const Derived again = derive(d.bytes);
    CHECK(again.bytes == d.bytes);
    CHECK(again.changed == 0);

    // what the original fails and the derivative satisfies
    CHECK(violation_of(original).find("data row 1: the sunspot-number field (bytes 135-138) is '  22'") == 0);
    CHECK_FALSE(check_derived(d.bytes).has_value());
}

TEST_CASE("derive refuses a file that is not shaped as the pinned one, naming the line", "[gfz]") {
    const std::string good = file_of({"  22", "   0", " 503"});
    CHECK(refusal_of(good) == "<not refused>");
    CHECK(contains(refusal_of(""), "the file is empty"));
    CHECK(contains(refusal_of(good.substr(0, good.size() - 1)), "does not end with a line feed"));
    {
        std::string crlf = good;
        crlf.replace(crlf.find('\n'), 1, "\r\n");
        CHECK(contains(refusal_of(crlf), "a carriage return at byte"));
    }
    {
        std::string bad_header = good;
        bad_header[bad_header.find("# synthetic header line 7") ] = 'x';
        CHECK(contains(refusal_of(bad_header), "header line 7 does not start with '#'"));
    }
    {   // one header line short: the first data row is then line 40
        CHECK(contains(refusal_of(good.substr(good.find('\n') + 1)), "header line 40 does not start with '#'"));
    }
    CHECK(contains(refusal_of(good + "# a stray comment\n"), "starts with '#', but the 40 header lines are over"));
    CHECK(contains(refusal_of(header()), "no data rows"));
    {   // a row one character short, and one long
        std::string short_row = good;
        short_row.erase(row_at(2) + 20, 1);
        CHECK(contains(refusal_of(short_row), "data row 2 (line 42) is 157 characters, the file's format says 158"));
        std::string long_row = good;
        long_row.insert(row_at(3) + 20, " ");
        CHECK(contains(refusal_of(long_row), "data row 3 (line 43) is 159 characters"));
    }
    // the field itself: only a blank and a non-negative integer, or the marker
    for (const char* field : {"  1x", "  -2", "1234", "    ", " -12", "  1 ", " 1.5"}) {
        INFO("SN field '" << field << "'");
        const std::string refused = refusal_of(with_at(good, 2, kSnOffset, field));
        CHECK(contains(refused, "data row 2: the sunspot-number field (bytes 135-138) is"));
        CHECK(contains(refused, "neither a non-negative integer nor the marker '  -1'"));
    }
}

TEST_CASE("check_derived holds the derivative to every field of every row, and names what it finds", "[gfz]") {
    const std::string good = derive(file_of({"  22", "   0", " 503", "  16", "   7", "  41"})).bytes;
    REQUIRE_FALSE(check_derived(good).has_value());

    // RULE 5: an unchanged sunspot number -- the defect this whole derivation exists to prevent -- is refused, and so is every other way a row can fail
    const auto refused_with = [&](const std::string& bytes, const std::string& phrase) {
        INFO("expected: " << phrase);
        const std::string got = violation_of(bytes);
        CHECK(contains(got, phrase));
    };
    refused_with(with_at(good, 4, kSnOffset, "  16"), "data row 4: the sunspot-number field (bytes 135-138) is '  16', not the marker '  -1'");
    refused_with(with_at(good, 4, kSnOffset, "  16"), "it still carries SILSO's CC BY-NC 4.0 sunspot number");
    refused_with(with_at(good, 2, 4, "x"), "data row 2: byte 5 lies between the fields and is not a blank");
    refused_with(with_at(good, 2, 129, "7"), "data row 2: byte 130 lies between the fields and is not a blank");
    refused_with(with_at(good, 1, 5, "13"), "data row 1: MM '13' is not a month");
    refused_with(with_at(good, 1, 8, "32"), "data row 1: DD '32' is not a day of a month");
    refused_with(with_at(good, 3, 8, "04"), "data row 3: days '    2' does not agree with the date");
    refused_with(with_at(good, 1, 15, "5"), "data row 1: days '    5' does not agree with the date");
    refused_with(with_at(good, 1, 17, "    0.6"), "data row 1: days_m '    0.6' is not the day number plus a half");
    refused_with(with_at(good, 2, 17, "    1x5"), "data row 2: days_m '    1x5' is not a fixed-point number");
    refused_with(with_at(good, 2, 25, "   0"), "data row 2: Bsr '   0' is not a rotation number");
    refused_with(with_at(good, 2, 30, "28"), "data row 2: dB '28' is not a day within a Bartels rotation");
    refused_with(with_at(good, 2, 33, "10.000"), "data row 2: Kp1 '10.000' is not a Kp");
    refused_with(with_at(good, 2, 40, "-0.333"), "data row 2: Kp2 '-0.333' is not a Kp");
    refused_with(with_at(good, 2, 47, " 2 333"), "data row 2: Kp3 ' 2 333' is not a fixed-point number");
    refused_with(with_at(good, 2, 89, " 401"), "data row 2: ap1 ' 401' is not an ap or Ap");
    refused_with(with_at(good, 2, 94, "  -2"), "data row 2: ap2 '  -2' is not an ap or Ap");
    refused_with(with_at(good, 2, 130, "  x1"), "data row 2: Ap '  x1' is not a whole number");
    refused_with(with_at(good, 2, 130, " 401"), "data row 2: Ap ' 401' is not an ap or Ap");
    refused_with(with_at(good, 2, 139, "     0.0"), "data row 2: F10.7obs '     0.0' is not a flux");
    refused_with(with_at(good, 2, 148, "    -5.0"), "data row 2: F10.7adj '    -5.0' is not a flux");
    refused_with(with_at(good, 2, 157, "3"), "data row 2: D '3' is not 0, 1 or 2");

    // the day numbers: consecutive from 0.  A row missing, and a file that starts late.
    std::string gap = good;
    gap.erase(row_at(3), kRowBytes);
    refused_with(gap, "data row 3: days '    3' does not follow the previous row's by one");
    refused_with(good.substr(0, header().size()) + good.substr(row_at(2)), "data row 1: days '    1' is not 0 on the first row");

    // the shape, as for `derive`
    refused_with(good.substr(0, good.size() - 1), "does not end with a line feed");
    refused_with("", "the file is empty");

    // missing values are values of the file: -1.000, -1 and -1.0 are what the real file writes for a missing Kp, ap and F10.7
    std::string missing = good;
    missing = with_at(missing, 2, 33, "-1.000");
    missing = with_at(missing, 2, 89, "  -1");
    missing = with_at(missing, 2, 139, "    -1.0");
    CHECK_FALSE(check_derived(missing).has_value());
}

TEST_CASE("the vendored file is the pinned derivative, satisfies every invariant, and is held to them by injection", "[gfz]") {
    const Bytes bytes = read_bytes(ODL_VENDORED_GFZ);
    const std::string text(as_text(ByteView{bytes}));

    // the three places the derivative's hash is written down agree: this tool, the manifest's pin, the file
    CHECK(sha256_hex(ByteView{bytes}) == kDerivedSha256);
    const Json manifest = Json::parse(read_text(ODL_MANIFEST_PATH));
    const Json* entry = nullptr;
    for (const Json& e : manifest.find("entries")->as_array()) {
        if (e.find("id")->as_string() == "gfz-kp-ap-f107") entry = &e;
    }
    REQUIRE(entry != nullptr);
    CHECK(entry->find("sha256")->as_string() == kDerivedSha256);
    CHECK(entry->find("derived_from_sha256")->as_string() == kOriginalSha256);
    CHECK(entry->find("vendored")->as_bool());
    CHECK(entry->find("derivation_tool")->as_string() == "tools/gfz_derive.cpp");
    CHECK(contains(entry->find("licence_note")->as_string(), "CHANGED: the sunspot-number column SN (CC BY-NC 4.0) of every data row is replaced by the file's own missing-value marker -1"));
    CHECK(contains(entry->find("licence_note")->as_string(), "https://creativecommons.org/licenses/by/4.0/"));

    // where the data rows start in THIS file: after its 40 header lines
    std::size_t data_start = 0;
    for (std::size_t i = 0; i < kHeaderLines; ++i) data_start = text.find('\n', data_start) + 1;
    constexpr std::size_t kRows = 34594;
    REQUIRE(text.size() == data_start + kRows * kRowBytes);

    // the invariants, without the original
    CHECK(text.size() == 5504038);   // the upstream file's size: the transformation does not change it
    CHECK_FALSE(check_derived(text).has_value());
    const Derived again = derive(text);
    CHECK(again.rows == kRows);
    CHECK(again.changed == 0);       // every SN field already is the marker
    CHECK(again.bytes == text);
    std::size_t not_marker = 0;
    for (std::size_t n = 0; n < kRows; ++n) {
        if (text.substr(data_start + n * kRowBytes + kSnOffset, kSnWidth) != kSnMarker) ++not_marker;
    }
    CHECK(not_marker == 0);

    // the file's own header places the SN field where this tool does: on its names line (line 40) the token "SN" ends at 0-based column 137
    const std::size_t names_begin = text.rfind('\n', data_start - 2) + 1;
    const std::string names = text.substr(names_begin, data_start - 1 - names_begin);
    CHECK(names.size() == kRowChars);
    CHECK(names.substr(kSnOffset + kSnWidth - 2, 2) == "SN");
    CHECK(contains(text.substr(0, data_start), "which have the CC BY-NC 4.0 license"));   // the header's licence line is kept, as the licence requires

    // the first and the last rows, by date; the first, as the file's own text has it
    CHECK(text.substr(data_start, kRowChars) == "1932 01 01     0     0.5 1352 10  3.333  2.667  2.333  2.667  3.333  2.667  3.333  3.333   18   12    9   12   18   12   18   18    15  -1     -1.0     -1.0 2");
    CHECK(text.substr(text.size() - kRowBytes, kRowChars) == "2026 09 17 34593 34593.5 2633 16  2.667  1.667  1.667  1.667  2.333  2.333  1.667  1.667   12    6    6    6    9    9    6    6     8  -1     96.8     97.7 0");

    // RULE 5: the defect, injected -- one sunspot number left as the original had it -- is refused, by name, wherever it sits
    for (const std::size_t n : {std::size_t{1}, std::size_t{20000}, kRows}) {
        INFO("data row " << n);
        std::string injected = text;
        injected.replace(data_start + (n - 1) * kRowBytes + kSnOffset, kSnWidth, "  16");
        const std::string got = violation_of(injected);
        CHECK(contains(got, "data row " + std::to_string(n) + ": the sunspot-number field (bytes 135-138) is '  16', not the marker '  -1'"));
    }
    // and the derivation closes the loop: a copy with real-looking sunspot numbers in every row is mapped back to exactly the vendored bytes
    std::string with_numbers = text;
    for (std::size_t n = 0; n < kRows; ++n) with_numbers.replace(data_start + n * kRowBytes + kSnOffset, kSnWidth, n % 2 == 0 ? "  16" : " 503");
    const Derived restored = derive(with_numbers);
    CHECK(restored.changed == kRows);
    CHECK(restored.bytes == text);
}

TEST_CASE("the executable: its three forms, the pins it holds a run to, and its refusals", "[gfz]") {
    TempDir td;
    const fs::path dir = td.path();
    const std::string original = file_of({"  22", "   0", " 503", "  16"});
    const std::string derived = derive(original).bytes;
    const std::string original_hash = sha256_hex(as_bytes(original));
    const std::string derived_hash = sha256_hex(as_bytes(derived));
    const Pins pins{original_hash, derived_hash};
    write_text(dir / "original.txt", original);

    // derive: writes the derivative and says what it did
    Result r = run_tool({(dir / "original.txt").string(), (dir / "derived.txt").string()}, pins);
    CHECK(r.code == 0);
    CHECK(read_text(dir / "derived.txt") == derived);
    CHECK(contains(r.out, "4 rows; the sunspot-number field of 4 of them replaced by '  -1'"));
    CHECK(contains(r.out, "sha256 " + derived_hash));
    CHECK_FALSE(fs::exists(dir / "derived.txt.part"));

    // --check: the vendored copy is the derivative of the original, or the first differing byte is named
    r = run_tool({"--check", (dir / "original.txt").string(), (dir / "derived.txt").string()}, pins);
    CHECK(r.code == 0);
    CHECK(contains(r.out, "is exactly the derivative of"));
    std::string tampered = derived;
    tampered[row_at(2) + 3] = '7';
    write_text(dir / "tampered.txt", tampered);
    r = run_tool({"--check", (dir / "original.txt").string(), (dir / "tampered.txt").string()}, pins);
    CHECK(r.code == 2);
    CHECK(contains(r.err, "the first difference is at byte " + std::to_string(row_at(2) + 4)));

    // --verify: needs no original
    r = run_tool({"--verify", (dir / "derived.txt").string()}, pins);
    CHECK(r.code == 0);
    CHECK(contains(r.out, "is the pinned derivative and satisfies every check"));
    r = run_tool({"--verify", (dir / "original.txt").string()}, pins);                // an original is not the derivative
    CHECK(r.code == 2);
    CHECK(contains(r.err, "not to the pinned derivative"));
    // the hash is the pinned one but the contents are not (an unchanged sunspot number): the checks themselves refuse
    const std::string unchanged = with_at(derived, 3, kSnOffset, "  16");
    write_text(dir / "unchanged-sn.txt", unchanged);
    r = run_tool({"--verify", (dir / "unchanged-sn.txt").string()}, Pins{original_hash, sha256_hex(as_bytes(unchanged))});
    CHECK(r.code == 2);
    CHECK(contains(r.err, "data row 3: the sunspot-number field"));

    // the pins: an input that is not the pinned original is refused, and an output that is not the pinned derivative is not written
    write_text(dir / "other.txt", file_of({"  22", "   0"}));
    r = run_tool({(dir / "other.txt").string(), (dir / "never.txt").string()}, pins);
    CHECK(r.code == 3);
    CHECK(contains(r.err, "not to the pinned original"));
    CHECK_FALSE(fs::exists(dir / "never.txt"));
    r = run_tool({(dir / "original.txt").string(), (dir / "never.txt").string()}, Pins{original_hash, std::string(64, '0')});
    CHECK(r.code == 2);
    CHECK(contains(r.err, "the tool and the pin disagree; nothing was written"));
    CHECK_FALSE(fs::exists(dir / "never.txt"));
    CHECK_FALSE(fs::exists(dir / "never.txt.part"));
    r = run_tool({(dir / "original.txt").string(), (dir / "original.txt").string()}, pins);   // never over the original
    CHECK(r.code == 5);
    CHECK(read_text(dir / "original.txt") == original);

    // an original whose OTHER fields are wrong is not turned into a vendored file either, though its hash and its derivative's are "pinned"
    const std::string odd = with_at(original, 2, 33, "10.000");
    write_text(dir / "odd.txt", odd);
    r = run_tool({(dir / "odd.txt").string(), (dir / "never.txt").string()}, Pins{sha256_hex(as_bytes(odd)), sha256_hex(as_bytes(derive(odd).bytes))});
    CHECK(r.code == 2);
    CHECK(contains(r.err, "the derivative fails its own checks: data row 2: Kp1 '10.000' is not a Kp"));
    CHECK_FALSE(fs::exists(dir / "never.txt"));

    // a file the shape of which is wrong is refused even when its hash is the pinned one
    const std::string crlf = file_of({"  22"}) + "\r\n";
    write_text(dir / "crlf.txt", crlf);
    r = run_tool({(dir / "crlf.txt").string(), (dir / "never.txt").string()}, Pins{sha256_hex(as_bytes(crlf)), derived_hash});
    CHECK(r.code == 3);
    CHECK(contains(r.err, "a carriage return"));

    // usage, and what is not there
    CHECK(run_tool({}, pins).code == 5);
    CHECK(run_tool({"--bogus", "a"}, pins).code == 5);
    CHECK(run_tool({"--check", "a"}, pins).code == 5);
    CHECK(run_tool({"a", "b", "c"}, pins).code == 5);
    r = run_tool({(dir / "missing.txt").string(), (dir / "never.txt").string()}, pins);
    CHECK(r.code == 3);

    // the real pins: a synthetic file is not the pinned original, whoever writes it
    std::ostringstream out;
    std::ostringstream err;
    CHECK(run({(dir / "original.txt").string(), (dir / "never.txt").string()}, Streams{out, err}) == 3);
    CHECK(contains(err.str(), "hashes to " + original_hash + ", not to the pinned original " + std::string(kOriginalSha256)));
}
