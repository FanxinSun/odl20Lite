// The devkit's tar and zip readers: archives built here byte by byte from the formats' definitions (so the expected answer is not the reader's
// own), what each refuses, and — the check that matters — every member the manifest pins, read out of the real cached archive and hashed.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/archive.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/inflate.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/sha256.hpp>

#include "archive_builders.hpp"
#include "inflate_vectors.hpp"

#include <cstring>
#include <string>

using namespace odl::devkit;
using namespace odl::devkit::testing;


TEST_CASE("tar: members by exact name, directories skipped, the last of a name wins", "[devkit][archive]") {
    const Bytes tar = concat({tar_entry("root/", "", '5'), tar_entry("root/a.txt", "alpha"), tar_entry("root/b.txt", "beta bytes"),
                              tar_entry("root/a.txt", "alpha, again")});
    CHECK(text(tar_member(view(tar), "root/b.txt")) == "beta bytes");
    CHECK(text(tar_member(view(tar), "root/a.txt")) == "alpha, again");
    CHECK(text(tar_member(view(tar), "root/c.txt")) == "<absent>");
    CHECK_THROWS_AS(tar_member(view(tar), "root"), FormatError);   // the directory exists, and is not a file
    const auto entries = tar_entries(view(tar));
    REQUIRE(entries.size() == 4);
    CHECK(entries[0].name == "root");   // the trailing slash of a directory goes, as Python's tarfile drops it
    CHECK_FALSE(entries[0].regular);
    CHECK(entries[1].name == "root/a.txt");
    CHECK(entries[1].size == 5);
    CHECK(entries[1].regular);
}

TEST_CASE("tar: the ustar prefix field, a GNU long name, a pax path and a pax size", "[devkit][archive]") {
    const std::string long_name = "very/long/directory/name/" + std::string(120, 'x') + "/file.txt";
    const std::string pax_name = "pax/" + std::string(150, 'y') + "/data.bin";
    const std::string pax = pax_record("path", pax_name) + pax_record("mtime", "1.5");
    const Bytes tar = concat({tar_entry("file", "from prefix", '0', "some/dir"),                                  // some/dir/file
                              tar_entry("././@LongLink", long_name + '\0', 'L'), tar_entry("truncated", "long one"),
                              tar_entry("pax_global_header", pax_record("comment", "0123456789abcdef"), 'g'),     // ignored
                              tar_entry("PaxHeaders/x", pax, 'x'), tar_entry("ignored-name", "pax one")});
    CHECK(text(tar_member(view(tar), "some/dir/file")) == "from prefix");
    CHECK(text(tar_member(view(tar), long_name)) == "long one");
    CHECK(text(tar_member(view(tar), pax_name)) == "pax one");
    CHECK(text(tar_member(view(tar), "truncated")) == "<absent>");
    CHECK(text(tar_member(view(tar), "ignored-name")) == "<absent>");
}

TEST_CASE("tar refuses a bad header checksum, a truncated member and a link in place of a file", "[devkit][archive]") {
    CHECK_THROWS_AS(tar_entries(view(concat({tar_entry("a", "x", '0', "", true)}))), FormatError);
    Bytes cut = concat({tar_entry("a", std::string(2000, 'z'))}, false);
    cut.resize(600);
    CHECK_THROWS_AS(tar_entries(view(cut)), FormatError);
    const Bytes link = concat({tar_entry("l", "", '2')});
    CHECK_THROWS_AS(tar_member(view(link), "l"), FormatError);
    CHECK(tar_entries(ByteView{}).empty());
    CHECK(tar_entries(view(Bytes(1024, 0))).empty());
}

TEST_CASE("zip: stored and deflated members by name, found past a comment, the last of a name wins", "[devkit][archive]") {
    ZipSpec deflated;
    deflated.name = "dir/hello.txt";
    const Bytes gz = [] {
        Bytes b;
        const std::string hex = vectors::kHello;
        for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
            b.push_back(static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        }
        return b;
    }();
    deflated.stored.assign(gz.begin() + 10, gz.end() - 8);   // the raw DEFLATE stream between gzip's header and trailer
    deflated.method = 8;
    deflated.crc = 0x66310268U;                                // the CRC-32 gzip recorded for it (little-endian 68 02 31 66)
    deflated.size = 30;
    const Bytes zip = make_zip({stored_file("a.txt", "first"), deflated, stored_file("a.txt", "second")}, "a comment after the directory");
    CHECK(text(zip_member(view(zip), "a.txt")) == "second");
    CHECK(text(zip_member(view(zip), "dir/hello.txt")) == "hello hello hello hello hello\n");
    CHECK(text(zip_member(view(zip), "absent")) == "<absent>");
    const auto entries = zip_entries(view(zip));
    REQUIRE(entries.size() == 3);
    CHECK(entries[1].name == "dir/hello.txt");
    CHECK(entries[1].size == 30);
}

TEST_CASE("zip refuses a bad CRC, a wrong size, encryption, an unknown method and zip64", "[devkit][archive]") {
    ZipSpec bad_crc = stored_file("x", "payload");
    bad_crc.crc ^= 1U;
    CHECK_THROWS_AS(zip_member(view(make_zip({bad_crc})), "x"), FormatError);
    ZipSpec bad_size = stored_file("x", "payload");
    bad_size.size += 1;
    CHECK_THROWS_AS(zip_member(view(make_zip({bad_size})), "x"), FormatError);
    ZipSpec encrypted = stored_file("x", "payload");
    encrypted.flags = 1;
    CHECK_THROWS_AS(zip_member(view(make_zip({encrypted})), "x"), FormatError);
    ZipSpec method = stored_file("x", "payload");
    method.method = 12;   // bzip2
    CHECK_THROWS_AS(zip_member(view(make_zip({method})), "x"), FormatError);
    Bytes zip64 = make_zip({stored_file("x", "payload")});
    zip64[zip64.size() - 6] = 0xFF;   // the central directory's offset: 0xFFFFFFFF marks zip64
    zip64[zip64.size() - 5] = 0xFF;
    zip64[zip64.size() - 4] = 0xFF;
    zip64[zip64.size() - 3] = 0xFF;
    CHECK_THROWS_AS(zip_entries(view(zip64)), FormatError);
    CHECK_THROWS_AS(zip_entries(as_bytes("not a zip file, though long enough to be looked at")), FormatError);
    CHECK_THROWS_AS(zip_entries(ByteView{}), FormatError);
}

#if defined(ODL_MANIFEST_PATH) && defined(ODL_MANIFEST_CACHE_ROOT) && defined(ODL_MANIFEST_VENDORED_ROOT)
TEST_CASE("every member the manifest pins reads out of its real cached archive and hashes as declared", "[devkit][archive][manifest]") {
    const Json doc = Json::parse(read_text(ODL_MANIFEST_PATH));
    std::size_t members = 0;
    std::size_t archives = 0;
    for (const Json& e : doc.find("entries")->as_array()) {
        const Json* unpack = e.find("unpack");
        if (unpack == nullptr || !unpack->is_string()) continue;
        const std::string& id = e.find("id")->as_string();
        INFO("entry " << id);
        if (e.find("vendored_members") != nullptr && e.find("vendored_members")->truthy()) {
            // An archive entry that vendors its members (tools/fetch.cpp, PROVENANCE.md section 41.7): the archive is NOT held, here or on GitHub's runner, so what
            // reads out and hashes as declared is each TRACKED member.  (The first version of this case asked for the archive of this entry too, and only a rehearsal
            // of the runner -- a cache that does not have it -- found that out.)
            const Json* tracked = e.find("members");
            REQUIRE(tracked != nullptr);
            for (const Json& m : tracked->as_array()) {
                const std::string& name = m.find("member")->as_string();
                INFO("tracked member " << name);
                const std::filesystem::path member_file = std::filesystem::path(ODL_MANIFEST_VENDORED_ROOT) / id / std::filesystem::path(name).filename();
                REQUIRE(std::filesystem::exists(member_file));
                CHECK(sha256_file_hex(member_file) == m.find("sha256")->as_string());
                ++members;
            }
            continue;
        }
        const std::filesystem::path file = std::filesystem::path(ODL_MANIFEST_CACHE_ROOT) / id / e.find("filename")->as_string();
        REQUIRE(std::filesystem::exists(file));
        const Bytes raw = read_bytes(file);
        ++archives;
        const std::string root = e.find("unpacked_root") != nullptr && e.find("unpacked_root")->truthy() ? e.find("unpacked_root")->as_string() : "";
        Bytes tar;
        if (unpack->as_string() == "tar.gz") tar = gunzip(view(raw));
        const Json* list = e.find("members");
        if (list == nullptr) continue;
        for (const Json& m : list->as_array()) {
            const std::string& name = m.find("member")->as_string();
            INFO("member " << name);
            std::optional<Bytes> got;
            if (unpack->as_string() == "gzip") got = gunzip(view(raw));
            if (unpack->as_string() == "zip") got = zip_member(view(raw), name);
            if (unpack->as_string() == "tar.gz") got = tar_member(view(tar), root.empty() ? name : root + "/" + name);
            REQUIRE(got.has_value());
            CHECK(sha256_hex(view(*got)) == m.find("sha256")->as_string());
            ++members;
        }
    }
    INFO(archives << " archives, " << members << " members");
    CHECK(archives >= 15);
    CHECK(members >= 10);
}
#endif
