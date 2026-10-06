// The devkit's DEFLATE, gzip and CRC-32.  The gzip vectors (inflate_vectors.hpp) were made once with the system gzip from plaintexts that this
// file regenerates, so the expected output is not the decoder's own; the hand-built streams are derived from RFC 1951 and its fixed code table.
// The multi-block streams and every pinned archive of the manifest are exercised against the real cache (archive_tests.cpp, fetch_tests.cpp).

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/inflate.hpp>

#include "inflate_vectors.hpp"

#include <string>

using namespace odl::devkit;

namespace {

Bytes from_hex(const std::string& hex) {
    Bytes out;
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(static_cast<std::uint8_t>(nibble(hex[i]) * 16 + nibble(hex[i + 1])));
    return out;
}

ByteView view(const Bytes& b) { return ByteView{b.data(), b.size()}; }

std::string text(const Bytes& b) { return std::string(as_text(view(b))); }

std::string integers() {
    std::string s;
    for (int i = 1; i <= 400; ++i) s += std::to_string(i) + "\n";
    return s;
}

std::string sentences() {
    std::string s;
    for (int i = 0; i < 400; ++i) s += "the quick brown fox " + std::to_string(i % 17) + " jumps over the lazy dog; ";
    return s;
}

}  // namespace

TEST_CASE("CRC-32 has the standard check value and chains", "[devkit][inflate]") {
    CHECK(crc32(as_bytes("123456789")) == 0xCBF43926U);
    CHECK(crc32(as_bytes("")) == 0U);
    CHECK(crc32(as_bytes("56789"), crc32(as_bytes("1234"))) == 0xCBF43926U);
}

TEST_CASE("gunzip reads gzip's own output: fixed, dynamic, empty, long runs", "[devkit][inflate]") {
    CHECK(text(gunzip(view(from_hex(vectors::kHello)))) == "hello hello hello hello hello\n");
    CHECK(gunzip(view(from_hex(vectors::kEmpty))).empty());
    CHECK(text(gunzip(view(from_hex(vectors::kIntegers)))) == integers());
    CHECK(text(gunzip(view(from_hex(vectors::kIntegersFast)))) == integers());
    CHECK(text(gunzip(view(from_hex(vectors::kSentences)))) == sentences());
    CHECK(text(gunzip(view(from_hex(vectors::kRun)))) == std::string(70000, 'a'));
}

TEST_CASE("gunzip reads every member of a concatenation, and skips zero padding after one", "[devkit][inflate]") {
    Bytes two = from_hex(vectors::kHello);
    const Bytes b = from_hex(vectors::kIntegers);
    two.insert(two.end(), b.begin(), b.end());
    CHECK(text(gunzip(view(two))) == "hello hello hello hello hello\n" + integers());
    two.insert(two.end(), 7, 0);
    CHECK(text(gunzip(view(two))) == "hello hello hello hello hello\n" + integers());
}

TEST_CASE("gunzip refuses a bad CRC, a bad length, a truncated stream and trailing garbage", "[devkit][inflate]") {
    const Bytes good = from_hex(vectors::kIntegers);
    {
        Bytes bad = good;
        bad[bad.size() - 8] ^= 0x01U;   // the CRC
        CHECK_THROWS_AS(gunzip(view(bad)), FormatError);
    }
    {
        Bytes bad = good;
        bad[bad.size() - 4] ^= 0x01U;   // ISIZE
        CHECK_THROWS_AS(gunzip(view(bad)), FormatError);
    }
    {
        Bytes bad(good.begin(), good.end() - 20);
        CHECK_THROWS_AS(gunzip(view(bad)), FormatError);
    }
    {
        Bytes bad = good;
        bad.push_back(0x41);   // a byte that is neither zero padding nor a member
        CHECK_THROWS_AS(gunzip(view(bad)), FormatError);
    }
    CHECK_THROWS_AS(gunzip(view(from_hex("00001f8b"))), FormatError);   // zero padding is for AFTER a member only
    CHECK_THROWS_AS(gunzip(ByteView{}), FormatError);
    CHECK_THROWS_AS(gunzip(as_bytes("not gzip at all, just text")), FormatError);
}

TEST_CASE("inflate: stored blocks, a hand-built fixed block, and what it refuses", "[devkit][inflate]") {
    // BFINAL=0 stored "abc", then BFINAL=1 stored "defg": LEN, then NLEN = ~LEN, little-endian
    const Bytes stored = from_hex("00" "0300fcff" "616263" "01" "0400fbff" "64656667");
    std::size_t used = 0;
    CHECK(text(inflate(view(stored), &used)) == "abcdefg");
    CHECK(used == stored.size());
    CHECK_THROWS_AS(inflate(view(from_hex("010300fdff616263"))), FormatError);   // the length and its complement disagree
    CHECK_THROWS_AS(inflate(view(from_hex("010900f6ff616263"))), FormatError);   // a stored block that runs past the end
    CHECK_THROWS_AS(inflate(view(from_hex("07"))), FormatError);                 // block type 3 is reserved
    // fixed Huffman, written out from RFC 1951's code table: BFINAL=1 BTYPE=01, literal 'a' (8 bits 10010001), length 3 (7 bits 0000001),
    // distance code 00000 = distance 1, end of block (7 bits 0000000): 'a' then a copy of 3 at distance 1 is "aaaa"
    CHECK(text(inflate(view(from_hex("4b040200")))) == "aaaa");
    // the same with distance code 00100 + extra bit 0 = distance 5, which reaches before the start of the output
    CHECK_THROWS_AS(inflate(view(from_hex("4b041200"))), FormatError);
    // a dynamic block whose header is cut short
    CHECK_THROWS_AS(inflate(view(from_hex("05"))), FormatError);
    CHECK_THROWS_AS(inflate(ByteView{}), FormatError);
}

TEST_CASE("inflate reports how many input bytes the stream used", "[devkit][inflate]") {
    Bytes raw = from_hex(vectors::kHello);
    raw.erase(raw.begin(), raw.begin() + 10);    // the gzip header
    const std::size_t stream = raw.size() - 8;   // the trailer
    raw.push_back(0xAA);                         // and one more byte after it
    std::size_t used = 0;
    CHECK(text(inflate(view(raw), &used)) == "hello hello hello hello hello\n");
    CHECK(used == stream);
}
