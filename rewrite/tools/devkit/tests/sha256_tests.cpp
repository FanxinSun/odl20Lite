// The devkit's SHA-256 against the FIPS 180-4 examples and against an independent implementation (GNU coreutils' sha256sum, run once while this
// test was written, on inputs around every padding boundary).  The strongest check is elsewhere: every hash the manifest pins is reproduced
// from the real cached bytes by `manifest.verify_offline`.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/sha256.hpp>

#include <string>

using namespace odl::devkit;

namespace {

std::string hash_of(const std::string& s) { return sha256_hex(as_bytes(s)); }

}  // namespace

TEST_CASE("SHA-256 reproduces the FIPS 180-4 examples", "[devkit][sha256]") {
    CHECK(hash_of("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hash_of("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // the 448-bit message: its padding does not fit in the same block
    CHECK(hash_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // one million 'a'
    CHECK(hash_of(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("SHA-256 agrees with sha256sum at every padding boundary", "[devkit][sha256]") {
    // lengths of 'a' around the 55/56 and 63/64 boundaries and their second blocks; digests from GNU coreutils sha256sum
    struct Known {
        std::size_t n;
        const char* digest;
    };
    const Known known[] = {
        {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
        {119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"},
        {120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"},
        {127, "c57e9278af78fa3cab38667bef4ce29d783787a2f731d4e12200270f0c32320a"},
        {128, "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e"},
    };
    for (const Known& k : known) {
        INFO("length " << k.n);
        CHECK(hash_of(std::string(k.n, 'a')) == k.digest);
    }
}

TEST_CASE("SHA-256 is the same however the input is split", "[devkit][sha256]") {
    std::string data;
    for (int i = 0; i < 1000; ++i) data.push_back(static_cast<char>((i * 31 + 7) & 0xFF));
    const std::string whole = hash_of(data);
    for (std::size_t chunk = 1; chunk <= 150; ++chunk) {
        Sha256 h;
        for (std::size_t at = 0; at < data.size(); at += chunk) {
            h.update(as_bytes(std::string_view(data).substr(at, chunk)));
        }
        const auto d = h.finish();
        INFO("chunk " << chunk);
        REQUIRE(to_hex(ByteView{d.data(), d.size()}) == whole);
    }
}

TEST_CASE("SHA-256 of a file is SHA-256 of its bytes, and an absent file is refused", "[devkit][sha256]") {
    TempDir dir;
    const auto f = dir.path() / "abc.txt";
    write_text(f, "abc");
    CHECK(sha256_file_hex(f) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK_THROWS_AS(sha256_file_hex(dir.path() / "absent"), std::runtime_error);
}
