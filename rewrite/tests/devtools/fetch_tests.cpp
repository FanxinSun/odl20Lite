// tests/devtools/fetch_tests.cpp — the fetcher's refusals, exercised.  (ctest `fetcher.behaviour`; the port of tests/test_fetch.py.)
//
// The manifest's value is entirely in what it REFUSES.  A fetcher that downloads correctly but accepts a corrupted cache has bought nothing, so
// the refusal paths are the ones worth testing, and they are tested against a synthetic manifest in a temporary directory rather than against
// the real cache.  Every check of the Python test is here with the same synthetic input; the cases after "NEW" cover what the Python test never
// reached: the archive members, the columns rule, `list`, `path --member`, the receipts, the download refusals and `verify-populated`.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/inflate.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "archive_builders.hpp"
#include "fetch.hpp"
#include "inflate_vectors.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
using namespace odl::devkit::testing;

namespace {

constexpr int kOk = 0, kMissing = 1, kMismatch = 2, kMalformed = 3, kNetwork = 4, kUsage = 5, kArgument = 2;

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run(const fs::path& root, const fs::path& manifest, std::vector<std::string> args) {
    std::ostringstream out;
    std::ostringstream err;
    std::vector<std::string> argv = {"--root", root.string(), "--manifest", manifest.string()};
    argv.insert(argv.end(), args.begin(), args.end());
    const int code = odl::tools::fetch::run(argv, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_plain(std::vector<std::string> args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = odl::tools::fetch::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

Json object(std::initializer_list<std::pair<const char*, Json>> members) {
    Json::Object o;
    for (const auto& m : members) o.emplace_back(m.first, m.second);
    return Json(std::move(o));
}

Json with(Json e, const std::string& key, Json value) {
    e.set(key, std::move(value));
    return e;
}

Json without(const Json& e, const std::string& key) {
    Json::Object o;
    for (const auto& m : e.as_object()) {
        if (m.first != key) o.emplace_back(m);
    }
    return Json(std::move(o));
}

// Each manifest gets its own file.  They shared one path in the first draft of the Python test, so a later case silently clobbered an earlier
// one's manifest and three checks failed against a file they had not written.
fs::path write_manifest(const fs::path& dir, const std::vector<Json>& entries) {
    static int seq = 0;
    ++seq;
    const fs::path m = dir / ("manifest-" + std::to_string(seq) + ".json");
    Json::Array list(entries.begin(), entries.end());
    write_text(m, object({{"schema", Json(std::int64_t{1})}, {"cache", Json("cache")}, {"vendored", Json("vendored")}, {"literature", Json("papers")},
                          {"entries", Json(std::move(list))}}).dumps(2));
    return m;
}

std::string make_blob(const fs::path& dir, const std::string& id, const std::string& name, const std::string& content, const char* base = "cache") {
    const fs::path p = dir / base / id / name;
    fs::create_directories(p.parent_path());
    write_text(p, content);
    return sha256_hex(as_bytes(content));
}

// gzip -9n of "hello hello hello hello hello\n" (inflate_vectors.hpp), as bytes
Bytes hello_gz() {
    const std::string hex = vectors::kHello;
    Bytes out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) out.push_back(static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    return out;
}

Json data_entry(const std::string& id, const std::string& name, const std::string& digest, const char* licence = "CC0-1.0") {
    return object({{"id", Json(id)}, {"kind", Json("data")}, {"licence", Json(licence)}, {"url", Json("https://example.invalid/" + name)},
                   {"filename", Json(name)}, {"sha256", Json(digest)}});
}

// A literature entry: a provenance record, `terms` and no `licence`.  Its file lands in the manifest's `literature` directory, which these synthetic manifests set to `papers`
// (write_manifest): a test that named the tree's own would be a build input that mentions it, which literaturecheck (ci.sh gate 11) refuses.
Json literature_entry(const std::string& id, const std::string& name, const std::string& digest) {
    return object({{"id", Json(id)}, {"kind", Json("literature")}, {"terms", Json("NOT ESTABLISHED: the search found nothing")},
                   {"url", Json("https://example.invalid/" + name)}, {"filename", Json(name)}, {"sha256", Json(digest)}});
}

Json code_entry(const std::string& id, const std::string& name, const std::string& digest) { return with(data_entry(id, name, digest), "kind", Json("code")); }

// Environment variables set for the length of a test and put back after it, whatever the test does.
class EnvScope {
public:
    EnvScope() = default;
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
    ~EnvScope() {
        for (const auto& [name, saved] : saved_) {
            if (saved.has_value()) ::setenv(name.c_str(), saved->c_str(), 1);
            else ::unsetenv(name.c_str());
        }
    }
    void set(const std::string& name, const std::string& value) {
        remember(name);
        ::setenv(name.c_str(), value.c_str(), 1);
    }
    void unset(const std::string& name) {
        remember(name);
        ::unsetenv(name.c_str());
    }

private:
    void remember(const std::string& name) {
        if (saved_.count(name) != 0) return;
        const char* old = std::getenv(name.c_str());
        saved_.emplace(name, old != nullptr ? std::optional<std::string>(old) : std::nullopt);
    }
    std::map<std::string, std::optional<std::string>> saved_;
};

// Where the stand-in `curl` is: `fakebin/` beside this executable (tools/CMakeLists.txt builds it there).  Found at RUN time and not compiled in: a
// build directory's path inside the executable would make the build differ between two build paths (tools/reprocheck.py compares them).
std::string fake_curl_dir() { return (fs::canonical("/proc/self/exe").parent_path() / "fakebin").string(); }

// The stand-in `curl` (tests/devtools/fake_curl.cpp, built as `curl`) first on PATH; `calls()` is every argument list it was given.
struct FakeCurl {
    EnvScope env;
    fs::path log;

    explicit FakeCurl(const fs::path& scratch) : log(scratch / "fake-curl.log") {
        const char* path = std::getenv("PATH");
        env.set("PATH", fake_curl_dir() + ":" + (path != nullptr ? path : ""));
        env.set("ODL_FAKE_CURL_LOG", log.string());
        for (const char* steering : {"ODL_FAKE_CURL_BODY", "ODL_FAKE_CURL_EXIT", "ODL_FAKE_CURL_STDERR", "ODL_FAKE_CURL_ONLY_URL"}) env.unset(steering);
    }
    void deliver(const fs::path& body) { env.set("ODL_FAKE_CURL_BODY", body.string()); }
    void fail(int status, const std::string& message) {
        env.set("ODL_FAKE_CURL_EXIT", std::to_string(status));
        env.set("ODL_FAKE_CURL_STDERR", message);
    }
    void fail_only(const std::string& url_part, int status, const std::string& message) {   // only a URL containing `url_part` fails
        fail(status, message);
        env.set("ODL_FAKE_CURL_ONLY_URL", url_part);
    }
    [[nodiscard]] std::vector<std::vector<std::string>> calls() const {
        std::vector<std::vector<std::string>> out;
        if (!fs::exists(log)) return out;
        std::istringstream in(read_text(log));
        std::string line;
        while (std::getline(in, line)) {
            if (line == "--- call") out.emplace_back();
            else if (line != "--- end" && !out.empty()) out.back().push_back(line);
        }
        return out;
    }
};

bool has(const std::vector<std::string>& args, const std::string& flag) { return std::find(args.begin(), args.end(), flag) != args.end(); }

// The argument after `flag`, or "<absent>" when there is none (bounds-checked: an argument list the tool changed must fail a CHECK, not read past its end).
std::string value_after(const std::vector<std::string>& args, const std::string& flag) {
    const auto at = std::find(args.begin(), args.end(), flag);
    if (at == args.end() || at + 1 == args.end()) return "<absent>";
    return *(at + 1);
}

}  // namespace

TEST_CASE("verify accepts a cache that matches, refuses a corrupted one, and reports a missing one", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string good = "the bytes that were declared";
    const std::string digest = make_blob(root, "thing", "thing.bin", good);
    const Json entry = data_entry("thing", "thing.bin", digest);
    const fs::path m = write_manifest(root, {entry});

    Result r = run(root, m, {"verify"});
    CHECK(r.code == kOk);

    // --- the refusal that matters most
    write_text(root / "cache" / "thing" / "thing.bin", good + "!");
    r = run(root, m, {"verify"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, digest.substr(0, 16)));                                       // the refusal names the expected hash
    CHECK(contains(r.err, sha256_hex(as_bytes(good + "!")).substr(0, 16)));            // ... and the obtained hash
    CHECK(contains(r.err, "not a hash to update"));                                     // ... and says it is not a hash to update
    // NEW: `fetch` refuses a cached file that is not the declared one, rather than trusting it or silently downloading over it
    r = run(root, m, {"fetch"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "HASH MISMATCH"));
    CHECK(read_text(root / "cache" / "thing" / "thing.bin") == good + "!");   // and leaves it as it found it

    // --- missing from the cache
    fs::remove(root / "cache" / "thing" / "thing.bin");
    r = run(root, m, {"verify"});
    CHECK(r.code == kMissing);   // and NOT a network failure: verify never touches the network (the URL is .invalid, which cannot resolve)
}

TEST_CASE("vendored entries are never fetched, and are checked against the TRACKED path", "[fetcher]") {
    // An upstream whose own responses are not byte-stable (Horizons embeds a request-processing timestamp in every reply) cannot be re-fetched
    // into the gitignored cache on a fresh clone at all, so it is committed as a tracked file instead, and the fetcher must never try to
    // re-acquire it, not even under --refresh.
    TempDir td;
    const fs::path root = td.path();
    const std::string content = "vendored bytes, tracked in git";
    const std::string vdigest = make_blob(root, "vthing", "vthing.bin", content, "vendored");
    const Json ventry = with(with(data_entry("vthing", "vthing.bin", vdigest), "vendored", Json(true)), "url", Json("https://example.invalid/never-reached.bin"));
    const fs::path vm = write_manifest(root, {ventry});

    Result r = run(root, vm, {"verify"});
    CHECK(r.code == kOk);                       // verify accepts a vendored entry whose tracked file matches
    r = run(root, vm, {"fetch"});
    CHECK(r.code == kOk);                       // fetch does not try to re-acquire a vendored entry
    CHECK(contains(r.out, "vendored"));         // ... and reports it as vendored, not downloaded
    r = run(root, vm, {"fetch", "--refresh"});
    CHECK(r.code == kOk);                       // --refresh ALSO never re-acquires one

    fs::remove(root / "vendored" / "vthing" / "vthing.bin");
    r = run(root, vm, {"verify"});
    CHECK(r.code == kMissing);                  // a MISSING vendored entry is reported distinctly
    CHECK(contains(r.err, "restore from git"));
    r = run(root, vm, {"fetch"});
    CHECK(r.code == kMissing);                  // fetch on a missing vendored entry refuses rather than downloading it

    make_blob(root, "vthing", "vthing.bin", content, "vendored");
    write_text(root / "vendored" / "vthing" / "vthing.bin", "tampered");
    r = run(root, vm, {"verify"});
    CHECK(r.code == kMismatch);                 // a tampered vendored file is refused
    r = run(root, vm, {"fetch"});
    CHECK(r.code == kMismatch);                 // NEW: by `fetch` too, which never repairs a tracked file by downloading
    CHECK(contains(r.err, "the tracked copy itself has changed"));
}

TEST_CASE("a malformed manifest is refused with its own exit code", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest = make_blob(root, "thing", "thing.bin", "the bytes that were declared");
    const Json entry = data_entry("thing", "thing.bin", digest);

    CHECK(run(root, write_manifest(root, {with(entry, "sha256", Json("not-a-hash"))}), {"verify"}).code == kMalformed);   // a malformed sha256
    CHECK(run(root, write_manifest(root, {entry, entry}), {"verify"}).code == kMalformed);                                // a duplicate id
    CHECK(run(root, write_manifest(root, {without(entry, "sha256")}), {"verify"}).code == kMalformed);                    // an entry with no hash
    CHECK(run(root, write_manifest(root, {with(entry, "kind", Json("wishful"))}), {"verify"}).code == kMalformed);        // an unknown kind

    const fs::path bad = root / "bad-json.json";
    write_text(bad, "{\"schema\": 1, \"entries\": [");
    const Result r = run(root, bad, {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "line 1"));            // invalid JSON is refused with a line number

    const Result wrong_schema = run(root, [&] {
        const fs::path p = root / "schema2.json";
        write_text(p, "{\"schema\": 2, \"entries\": []}");
        return p;
    }(), {"verify"});
    CHECK(wrong_schema.code == kMalformed);
    CHECK(contains(wrong_schema.err, "unsupported schema 2"));
}

TEST_CASE("check-licences is an ALLOWLIST (plan §5 constraint 3)", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest = make_blob(root, "thing", "thing.bin", "the bytes that were declared");
    const Json entry = data_entry("thing", "thing.bin", digest);

    CHECK(run(root, write_manifest(root, {entry}), {"check-licences"}).code == kOk);   // a permissive manifest passes

    // CeCILL-2.1 is GPL-compatible copyleft and CeCILL-C is close to the LGPL, yet neither string contains "GPL" — a denylist passed both
    // silently, and CALCEPH is triple-licensed across exactly those two and CeCILL-B.
    for (const char* lic : {"GPL-3.0-only", "LGPL-2.1", "AGPL-3.0", "CeCILL-2.1", "CeCILL-C", "MPL-2.0", "EPL-2.0", "CDDL-1.0", "SSPL-1.0", "OSL-3.0",
                            "Proprietary", "unstated"}) {
        INFO("licence " << lic);
        const Result r = run(root, write_manifest(root, {with(entry, "licence", Json(lic))}), {"check-licences"});
        CHECK(r.code == kMalformed);
        CHECK(contains(r.err, "LICENCE NOT ON THE PERMISSIVE LIST"));
    }
    CHECK(run(root, write_manifest(root, {with(entry, "licence", Json("CeCILL-B"))}), {"check-licences"}).code == kOk);   // the one of the three that is permissive
    CHECK(run(root, write_manifest(root, {with(entry, "licence", Json("curl"))}), {"check-licences"}).code == kOk);       // the host tool's licence, case-insensitively
    CHECK(run(root, write_manifest(root, {with(entry, "licence", Json("  mit  "))}), {"check-licences"}).code == kOk);    // upper() and strip(), as the Python did

    // FACTUAL-DATA-CITED is earned by a recorded search, not by the label: an entry claiming it WITHOUT search_recorded is refused.
    const Json cited = with(entry, "licence", Json("FACTUAL-DATA-CITED"));
    Result r = run(root, write_manifest(root, {cited}), {"check-licences"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "search_recorded"));
    CHECK(run(root, write_manifest(root, {with(cited, "search_recorded", Json("PROVENANCE.md §37.4"))}), {"check-licences"}).code == kOk);
}

TEST_CASE("a host-provided tool needs no hash, but still needs a licence; path refuses what has no path", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest = make_blob(root, "thing", "thing.bin", "the bytes that were declared");
    const Json entry = data_entry("thing", "thing.bin", digest);
    const Json tool = object({{"id", Json("curl")}, {"kind", Json("tool")}, {"licence", Json("curl")}, {"provided_by_host", Json(true)}});
    const fs::path m3 = write_manifest(root, {entry, tool});

    Result r = run(root, m3, {"verify"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "host     curl"));
    CHECK(run(root, write_manifest(root, {without(tool, "licence")}), {"verify"}).code == kMalformed);

    r = run(root, m3, {"path", "thing"});
    CHECK(contains(r.out, "cache/thing/thing.bin\n"));   // path prints the cache location
    CHECK(run(root, m3, {"path", "curl"}).code == kUsage);          // a host-provided entry has no path
    CHECK(run(root, m3, {"path", "nonexistent"}).code == kUsage);   // nor does an unknown id
}

TEST_CASE("sniff: an HTML error page hashes perfectly well, so the bytes are looked at BEFORE they are hashed", "[fetcher]") {
    // `content/chapter10/icc10.pdf` is a 404 — every other IERS chapter is `iccN.pdf` and chapter 10 is `tn36_c10.pdf` — and curl without --fail
    // saved the error page with exit status 0, after which pdftotext reported sixty syntax errors rather than a wrong file.
    const auto verdict = [](const std::string& payload, const std::string& filename) {
        std::ostringstream out;
        std::ostringstream err;
        Streams io{out, err};
        const Json e = object({{"id", Json("probe")}, {"url", Json("https://example.invalid/x")}, {"filename", Json(filename)}});
        try {
            odl::tools::fetch::sniff(io, e, as_bytes(payload));
        } catch (const Exit& x) {
            return x.code;
        }
        return 0;
    };
    const std::string page = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n<html><head>\n<title>404";
    CHECK(verdict(page, "icc10.pdf") == kMalformed);                // an HTML error page named .pdf
    CHECK(verdict(page, "de440.bsp") == kMalformed);                // named .bsp
    CHECK(verdict(page, "eopc04.1962-now") == kMalformed);          // with no known extension
    CHECK(verdict("  \n<html>", "x.bin") == kMalformed);            // leading whitespace does not hide it
    CHECK(verdict("<?xml version=\"1.0\"?>", "x.bin") == kMalformed);
    CHECK(verdict("not a pdf at all, just text", "icc6.pdf") == kMalformed);
    CHECK(verdict("plain text", "desai.txt.gz") == kMalformed);
    CHECK(verdict("plain text", "egm.zip") == kMalformed);
    CHECK(verdict("%PDF-1.6\n%\xe2\xe3\xcf\xd3", "icc6.pdf") == 0);                  // a real PDF
    CHECK(verdict(std::string("\x1f\x8b\x08\x00", 4), "desai.txt.gz") == 0);          // a real gzip stream
    CHECK(verdict("PK\x03\x04\x14", "egm.zip") == 0);                                  // a real zip
    CHECK(verdict("DAF/SPK ", "de440.bsp") == 0);                                      // a real SPK kernel
    CHECK(verdict("anything at all", "eopc04.1962-now") == 0);                         // an extension with no declared magic
    CHECK(verdict("", "x.PDF") == kMalformed);                                         // the extension is compared in lower case
}

// ------------------------------------------------------------------------------------------------------------------------------------- NEW

TEST_CASE("archive entries: members verified in place, tampering refused, an undeclared 'consumes' refused", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string plain = "hello hello hello hello hello\n";
    const Bytes gz = hello_gz();
    const fs::path file = root / "cache" / "arch" / "hello.gz";
    fs::create_directories(file.parent_path());
    write_bytes(file, view(gz));
    const Json member = object({{"member", Json("hello.txt")}, {"sha256", Json(sha256_hex(as_bytes(plain)))}});
    const Json arch = with(with(with(with(data_entry("arch", "hello.gz", sha256_hex(view(gz))), "unpack", Json("gzip")), "consumes", Json("declared-members")),
                                "members", Json(Json::Array{member})), "consumes_note", Json("the one member"));

    Result r = run(root, write_manifest(root, {arch}), {"verify"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "member hello.txt"));
    CHECK(contains(r.out, "verified in place"));
    CHECK(contains(r.out, "1 member(s) read individually"));

    // an archive hash says nothing about what is taken out of it: a member whose declared hash is wrong is refused although the archive's is right
    const Json wrong_member = object({{"member", Json("hello.txt")}, {"sha256", Json(std::string(64, 'a'))}});
    r = run(root, write_manifest(root, {with(arch, "members", Json(Json::Array{wrong_member}))}), {"verify"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "ARCHIVE MEMBER HASH MISMATCH"));

    // every entry with `unpack` must say what the tree consumes from it
    r = run(root, write_manifest(root, {without(arch, "consumes")}), {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "does not say what this tree consumes"));

    // a declared member that the archive does not hold
    const Json absent = object({{"member", Json("absent.txt")}, {"sha256", Json(std::string(64, 'b'))}});
    const Json tar_arch = with(arch, "unpack", Json("zip"));   // not a zip
    r = run(root, write_manifest(root, {with(tar_arch, "members", Json(Json::Array{absent}))}), {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "cannot read member"));
}

TEST_CASE("fetch from a populated cache: extracts the declared members, once, and writes the receipts", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string plain = "hello hello hello hello hello\n";
    const Bytes gz = hello_gz();
    const fs::path file = root / "cache" / "arch" / "hello.gz";
    fs::create_directories(file.parent_path());
    write_bytes(file, view(gz));
    const std::string plain_hash = sha256_hex(as_bytes(plain));
    const auto make = [&](const std::string& declared) {
        const Json member = object({{"member", Json("hello.txt")}, {"sha256", Json(declared)}, {"extract", Json(true)}});
        return with(with(with(data_entry("arch", "hello.gz", sha256_hex(view(gz))), "unpack", Json("gzip")), "consumes", Json("declared-members")),
                    "members", Json(Json::Array{member}));
    };
    const fs::path m = write_manifest(root, {make(plain_hash)});

    Result r = run(root, m, {"verify"});
    CHECK(r.code == kMissing);   // the declared member is not extracted yet
    CHECK(contains(r.err, "declared member not extracted"));

    r = run(root, m, {"fetch"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "cached   arch"));
    CHECK(contains(r.out, "member hello.txt"));
    CHECK(read_text(root / "cache" / "arch" / "extracted" / "hello.txt") == plain);
    CHECK_FALSE(fs::exists(root / "cache" / "arch" / "extracted" / "hello.txt.part"));

    r = run(root, m, {"verify"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "verified in the cache"));
    r = run(root, m, {"fetch"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "member " + pad_right("hello.txt", 30) + " cached"));   // the second time it is already there

    r = run(root, m, {"path", "arch", "--member", "hello.txt"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "cache/arch/extracted/hello.txt\n"));
    CHECK(run(root, m, {"path", "arch", "--member", "nothing"}).code == kUsage);

    // the receipts: sorted by id, a UTC timestamp, the pinned hash
    const Json receipts = Json::parse(read_text(root / "cache" / "receipts.json"));
    const auto& list = receipts.find("receipts")->as_array();
    REQUIRE(list.size() == 1);
    CHECK(list[0].find("id")->as_string() == "arch");
    CHECK(list[0].find("sha256")->as_string() == sha256_hex(view(gz)));
    CHECK(std::regex_match(list[0].find("retrieved")->as_string(), std::regex(R"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ)")));

    // a declared member hash that is wrong: refused when it lands, and nothing is left behind
    fs::remove_all(root / "cache" / "arch" / "extracted");
    r = run(root, write_manifest(root, {make(std::string(64, 'c'))}), {"fetch"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "EXTRACTED MEMBER HASH MISMATCH"));
    CHECK_FALSE(fs::exists(root / "cache" / "arch" / "extracted" / "hello.txt"));
    CHECK_FALSE(fs::exists(root / "cache" / "arch" / "extracted" / "hello.txt.part"));
}

TEST_CASE("the columns rule: a file whose licence differs by column must say which columns are consumed", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest = make_blob(root, "table", "table.txt", "a table");
    const Json base = data_entry("table", "table.txt", digest, "CC-BY-4.0");
    const auto excluded = [](const char* lic, std::initializer_list<const char*> cols) {
        Json::Array a;
        for (const char* c : cols) a.emplace_back(c);
        return object({{lic, Json(std::move(a))}});
    };
    const auto columns = [](std::initializer_list<const char*> declared) {
        Json::Array a;
        for (const char* c : declared) a.emplace_back(c);
        return object({{"declared", Json(std::move(a))}});
    };
    // excludes licensed content but declares no columns
    Result r = run(root, write_manifest(root, {with(base, "licence_excluded", excluded("CC-BY-NC-4.0", {"sunspot"}))}), {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "declares no columns"));
    // declares a column as consumed that carries the excluded licence
    r = run(root, write_manifest(root, {with(with(base, "licence_excluded", excluded("CC-BY-NC-4.0", {"sunspot"})), "columns", columns({"kp", "sunspot"}))}), {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "column(s) sunspot"));
    // says it consumes declared columns and lists none
    r = run(root, write_manifest(root, {with(base, "consumes", Json("declared-columns"))}), {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "lists none"));
    // the honest entry passes and says what it consumes
    r = run(root, write_manifest(root, {with(with(with(base, "licence_excluded", excluded("CC-BY-NC-4.0", {"sunspot"})), "columns", columns({"kp", "ap"})), "consumes", Json("declared-columns"))}), {"verify"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "2 column(s) declared"));
}

TEST_CASE("list and list --json, including a literature entry (which the Python tool died on)", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest = make_blob(root, "thing", "thing.bin", "x");
    const Json paper = object({{"id", Json("a-paper")}, {"kind", Json("literature")}, {"terms", Json("NOT ESTABLISHED: the search found nothing")},
                               {"url", Json("https://example.invalid/p.pdf")}, {"filename", Json("p.pdf")}, {"sha256", Json(std::string(64, 'd'))}});
    const fs::path m = write_manifest(root, {data_entry("thing", "thing.bin", digest), paper});
    Result r = run(root, m, {"list"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "thing "));
    CHECK(contains(r.out, "a-paper "));
    CHECK(contains(r.out, "(see terms)"));
    r = run(root, m, {"list", "--json"});
    CHECK(r.code == kOk);
    CHECK(Json::parse(r.out) == Json::parse(read_text(m)));
    // check-licences reports a literature entry's terms record, by its two states
    r = run(root, m, {"check-licences"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "1 literature entry, exempt"));
    CHECK(contains(r.out, "a-paper"));
    CHECK(contains(r.out, "(none established)"));
    const Json found = with(paper, "terms", Json("The publisher's page says: all rights reserved."));
    r = run(root, write_manifest(root, {data_entry("thing", "thing.bin", digest), found}), {"check-licences"});
    CHECK(contains(r.out, "(found — see its terms field)"));
    // a literature entry that carries a licence is refused outright: the exemption must not read as a grant
    r = run(root, write_manifest(root, {with(paper, "licence", Json("MIT"))}), {"verify"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "must NOT carry a 'licence'"));
}

TEST_CASE("fetch refuses what it will not download, and a failed download leaves nothing behind", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest(64, 'e');
    // not https: refused before curl is asked
    Result r = run(root, write_manifest(root, {with(data_entry("thing", "thing.bin", digest), "url", Json("http://example.invalid/thing.bin"))}), {"fetch"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "not https"));
    // https to a host that cannot resolve (the .invalid TLD never does): a network failure, with curl's own reason, and no .part.  curl is the
    // stand-in: the real one, told to retry, would spend twenty seconds of this test on the delays.
    FakeCurl curl(root);
    curl.fail(6, "curl: (6) Could not resolve host: example.invalid");
    r = run(root, write_manifest(root, {data_entry("thing", "thing.bin", digest)}), {"fetch"});
    CHECK(r.code == kNetwork);
    CHECK(contains(r.err, "download failed for https://example.invalid/thing.bin: curl exited 6: curl: (6) Could not resolve host: example.invalid"));
    CHECK_FALSE(fs::exists(root / "cache" / "thing" / "thing.bin.part"));
    CHECK_FALSE(fs::exists(root / "cache" / "thing" / "thing.bin"));
}

TEST_CASE("verify-populated: the populated tree must be the archive that was pinned", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    // a tarball of pkg-1.0/ in STORED deflate blocks (valid gzip; the tests have no compressor)
    const std::string header = "// the pinned header\n#pragma once\n";
    const Bytes tar = concat({tar_entry("pkg-1.0/", "", '5'), tar_entry("pkg-1.0/LICENSE", "terms"), tar_entry("pkg-1.0/src/a.hpp", header)});
    const Bytes gz = gzip_stored(view(tar));
    const fs::path archive = root / "cache" / "pkg" / "pkg.tar.gz";
    fs::create_directories(archive.parent_path());
    write_bytes(archive, view(gz));
    const Json pkg = with(with(with(with(with(data_entry("pkg", "pkg.tar.gz", sha256_hex(view(gz)), "MIT"), "kind", Json("code")), "unpack", Json("tar.gz")),
                                   "unpacked_root", Json("pkg-1.0")), "verify_path", Json("src/a.hpp")), "consumes", Json("whole-tree"));
    const fs::path m2 = write_manifest(root, {pkg});

    const fs::path populated = root / "populated";
    fs::create_directories(populated / "src");
    write_text(populated / "src" / "a.hpp", header);
    Result r = run(root, m2, {"verify-populated", "pkg", populated.string()});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "populated tree matches the pinned archive (src/a.hpp)"));

    write_text(populated / "src" / "a.hpp", header + "// a competitor's edit\n");
    r = run(root, m2, {"verify-populated", "pkg", populated.string()});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "PINNED DEPENDENCY SUBSTITUTED"));
    CHECK(contains(r.err, "bytes pinned"));

    fs::remove(populated / "src" / "a.hpp");
    r = run(root, m2, {"verify-populated", "pkg", populated.string()});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "populated tree has no src/a.hpp"));

    CHECK(run(root, m2, {"verify-populated", "unknown", populated.string()}).code == kUsage);
    r = run(root, write_manifest(root, {without(pkg, "verify_path")}), {"verify-populated", "pkg", populated.string()});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "no verify_path declared"));
    // a verify_path the archive does not hold
    r = run(root, write_manifest(root, {with(pkg, "verify_path", Json("src/absent.hpp"))}), {"verify-populated", "pkg", populated.string()});
    CHECK(r.code == kMalformed);
}

TEST_CASE("the command line: usage errors are argparse's code 2, and -h is not an error", "[fetcher]") {
    CHECK(run_plain({}).code == kArgument);
    Result r = run_plain({"frobnicate"});
    CHECK(r.code == kArgument);
    CHECK(contains(r.err, "invalid choice"));
    CHECK(run_plain({"--root"}).code == kArgument);
    CHECK(run_plain({"--nonsense", "verify"}).code == kArgument);
    CHECK(run_plain({"verify", "extra"}).code == kArgument);
    CHECK(run_plain({"path"}).code == kArgument);
    CHECK(run_plain({"verify-populated", "only-one-argument"}).code == kArgument);
    // --skip-literature belongs to `verify` and `fetch`, and to no other command: it cannot be mistaken for something every command takes
    for (const char* other : {"list", "check-licences", "path", "verify-populated"}) {
        INFO("command " << other);
        CHECK(run_plain({other, "--skip-literature"}).code == kArgument);
    }
    CHECK(run_plain({"verify", "--skip-literature=yes"}).code == kArgument);   // and it is a flag: it takes no value
    r = run_plain({"-h"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "verify-populated"));
    CHECK(contains(r.out, "--skip-literature"));
    CHECK(run_plain({"--manifest", "/nonexistent/manifest.json", "verify"}).code == kMalformed);   // an unreadable manifest is malformed, not a crash
}

TEST_CASE("what the tool did not anticipate is reported with its own code, not a crash", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest(64, '1');
    // the cache's own directory is a FILE: nothing can be created under it
    write_text(root / "cache", "not a directory");
    Result r = run(root, write_manifest(root, {data_entry("thing", "thing.bin", digest)}), {"fetch"});
    CHECK(r.code == 70);
    CHECK(contains(r.err, "internal error"));
    // an entry whose url, filename or hash is not a string is a malformed manifest, not a crash
    for (const char* field : {"url", "filename"}) {
        INFO("field " << field);
        r = run(root, write_manifest(root, {with(data_entry("thing", "thing.bin", digest), field, Json(std::int64_t{7}))}), {"verify"});
        CHECK(r.code == kMalformed);
        CHECK(contains(r.err, std::string(field) + " must be a string"));
    }
}

TEST_CASE("a damaged receipts file is replaced, not trusted and not fatal", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string content = "x";
    const std::string digest = make_blob(root, "thing", "thing.bin", content);
    const fs::path m = write_manifest(root, {data_entry("thing", "thing.bin", digest)});
    REQUIRE(run(root, m, {"fetch"}).code == kOk);
    for (const char* damage : {"not json at all", "{\"receipts\": [{\"url\": \"u\"}]}", "{\"receipts\": [7]}", "[]"}) {
        INFO("receipts.json was: " << damage);
        write_text(root / "cache" / "receipts.json", damage);
        REQUIRE(run(root, m, {"fetch"}).code == kOk);
        const Json fresh = Json::parse(read_text(root / "cache" / "receipts.json"));
        REQUIRE(fresh.find("receipts")->as_array().size() == 1);
        CHECK(fresh.find("receipts")->as_array()[0].find("id")->as_string() == "thing");
    }
}

// The cases below put a stand-in `curl` first on PATH (tests/devtools/fake_curl.cpp), so the download path is exercised without a network.  Until
// they existed nothing tested it: the Python tool's test never reached a download either, and the first real download of the whole manifest
// (PROVENANCE.md section 41, group C1b) found what curl's defaults had changed.

TEST_CASE("a download: the flags curl is given, and the file lands only after its hash has been checked", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string content = "the pinned bytes\n";
    write_text(root / "body.bin", content);
    FakeCurl curl(root);
    curl.deliver(root / "body.bin");
    const fs::path m = write_manifest(root, {data_entry("thing", "thing.bin", sha256_hex(as_bytes(content)))});

    Result r = run(root, m, {"fetch"});
    REQUIRE(r.code == kOk);
    CHECK(contains(r.out, "fetching thing"));
    CHECK(contains(r.out, "ok       thing"));
    CHECK(read_text(root / "cache" / "thing" / "thing.bin") == content);
    CHECK_FALSE(fs::exists(root / "cache" / "thing" / "thing.bin.part"));

    const auto calls = curl.calls();
    REQUIRE(calls.size() == 1);
    const auto& args = calls[0];
    CHECK(args.back() == "https://example.invalid/thing.bin");   // the URL, last
    for (const char* flag : {"--silent", "--show-error", "--fail", "--location", "--http1.1", "--retry-all-errors"}) {
        INFO("flag " << flag);
        CHECK(has(args, flag));
    }
    CHECK(value_after(args, "--proto") == "=https");             // TLS only, and not downgraded by a redirect either
    CHECK(value_after(args, "--proto-redir") == "=https");
    CHECK(value_after(args, "--max-redirs") == "10");
    CHECK(value_after(args, "--retry") == "4");
    CHECK(value_after(args, "--retry-delay") == "5");
    CHECK(value_after(args, "--connect-timeout") == "30");       // a host that does not answer at all is given 30 s, not the 120 that cost two hours (section 41.7) ...
    CHECK(value_after(args, "--speed-limit") == "1");            // ... and a transfer that has started is given 120 s of silence, as before
    CHECK(value_after(args, "--speed-time") == "120");
    CHECK(value_after(args, "--output") == (root / "cache" / "thing" / "thing.bin.part").string());   // into a .part, renamed after the hash

    // the second time it is in the cache, and curl is not called at all
    r = run(root, m, {"fetch"});
    CHECK(contains(r.out, "cached   thing"));
    CHECK(curl.calls().size() == 1);
    CHECK(run(root, m, {"verify"}).code == kOk);
}

TEST_CASE("a download that arrives with the wrong hash is discarded, an HTML page is refused, and neither leaves a file", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string pinned = "what the manifest pinned\n";
    const std::string served = "what upstream serves today\n";
    write_text(root / "wrong.bin", served);
    write_text(root / "page.bin", "<!DOCTYPE html><html><body>404 Not Found</body></html>\n");
    FakeCurl curl(root);
    const fs::path m = write_manifest(root, {data_entry("thing", "thing.bin", sha256_hex(as_bytes(pinned)))});
    const fs::path landed = root / "cache" / "thing" / "thing.bin";

    curl.deliver(root / "wrong.bin");
    Result r = run(root, m, {"fetch"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "HASH MISMATCH"));
    CHECK(contains(r.err, "(download discarded)"));
    CHECK(contains(r.err, sha256_hex(as_bytes(served))));        // what it obtained, in full
    CHECK_FALSE(fs::exists(landed));
    CHECK_FALSE(fs::exists(landed.string() + ".part"));

    curl.deliver(root / "page.bin");
    r = run(root, m, {"fetch"});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "returned an HTML document"));
    CHECK_FALSE(fs::exists(landed));
    CHECK_FALSE(fs::exists(landed.string() + ".part"));
    CHECK(curl.calls().size() == 2);
}

TEST_CASE("a curl that fails, one that wrote nothing, and one that is not there are each a network failure, in their own words", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const fs::path m = write_manifest(root, {data_entry("thing", "thing.bin", std::string(64, 'a'))});
    const fs::path landed = root / "cache" / "thing" / "thing.bin";
    {
        FakeCurl curl(root);
        curl.fail(22, "curl: (22) The requested URL returned error: 404");
        const Result r = run(root, m, {"fetch"});
        CHECK(r.code == kNetwork);
        CHECK(contains(r.err, "download failed for https://example.invalid/thing.bin: curl exited 22: curl: (22) The requested URL returned error: 404"));
        CHECK_FALSE(fs::exists(landed));
        CHECK_FALSE(fs::exists(landed.string() + ".part"));
    }
    {
        FakeCurl curl(root);   // exit status 0 and no output file
        const Result r = run(root, m, {"fetch"});
        CHECK(r.code == kNetwork);
        CHECK(contains(r.err, "download failed for https://example.invalid/thing.bin"));
        CHECK_FALSE(fs::exists(landed));
        CHECK_FALSE(fs::exists(landed.string() + ".part"));
    }
    {
        EnvScope env;          // no curl on PATH at all
        fs::create_directories(root / "empty");
        env.set("PATH", (root / "empty").string());
        const Result r = run(root, m, {"fetch"});
        CHECK(r.code == kNetwork);
        CHECK(contains(r.err, "curl is the one host program"));
        CHECK_FALSE(fs::exists(landed.string() + ".part"));
    }
}

TEST_CASE("fetch --refresh downloads what is already cached, and reports upstream drift with the cached copy untouched", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string pinned = "pinned\n";
    const std::string digest = make_blob(root, "thing", "thing.bin", pinned);
    const fs::path m = write_manifest(root, {data_entry("thing", "thing.bin", digest)});
    const fs::path cached = root / "cache" / "thing" / "thing.bin";
    write_text(root / "drifted.bin", "upstream moved\n");
    write_text(root / "same.bin", pinned);
    FakeCurl curl(root);

    curl.deliver(root / "drifted.bin");
    Result r = run(root, m, {"fetch", "--refresh"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "UPSTREAM DRIFT: thing now hashes "));
    CHECK(contains(r.err, "The cached copy is untouched."));
    CHECK(read_text(cached) == pinned);
    CHECK_FALSE(fs::exists(cached.string() + ".part"));

    curl.deliver(root / "same.bin");
    r = run(root, m, {"fetch", "--refresh"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "fetching thing"));                   // it downloaded although the file was there
    CHECK(read_text(cached) == pinned);
    CHECK(curl.calls().size() == 2);
}

TEST_CASE("fetch stops at the first entry that fails; --keep-going tries them all, lists the failures, and exits with the first one's code", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string content = "the bytes every URL serves\n";
    write_text(root / "body.bin", content);
    const std::string good = sha256_hex(as_bytes(content));
    const auto entries = [&] {
        return std::vector<Json>{data_entry("first", "first.bin", good), data_entry("drops", "drops.bin", good),
                                 data_entry("moved", "moved.bin", std::string(64, 'b')), data_entry("last", "last.bin", good)};
    };
    const fs::path m = write_manifest(root, entries());
    FakeCurl curl(root);
    curl.deliver(root / "body.bin");
    curl.fail_only("drops.bin", 35, "curl: (35) TLS connect error");

    // the default: the second entry's connection drops and the run stops there, the last entry untried
    Result r = run(root, m, {"fetch"});
    CHECK(r.code == kNetwork);
    CHECK(curl.calls().size() == 2);
    CHECK(fs::exists(root / "cache" / "first" / "first.bin"));
    CHECK_FALSE(fs::exists(root / "cache" / "last" / "last.bin"));
    CHECK_FALSE(fs::exists(root / "cache" / "receipts.json"));   // as the Python tool: a run that dies writes no receipts

    // --keep-going: every entry is tried, the third (a hash that moved) is refused and discarded, the fourth lands
    fs::remove_all(root / "cache");
    r = run(root, m, {"fetch", "--keep-going"});
    CHECK(r.code == kNetwork);                                  // the FIRST failure's code, not the last's (a mismatch, 2)
    CHECK(curl.calls().size() == 2 + 4);
    CHECK(fs::exists(root / "cache" / "first" / "first.bin"));
    CHECK(fs::exists(root / "cache" / "last" / "last.bin"));
    CHECK_FALSE(fs::exists(root / "cache" / "drops" / "drops.bin"));
    CHECK_FALSE(fs::exists(root / "cache" / "moved" / "moved.bin"));
    CHECK_FALSE(fs::exists(root / "cache" / "moved" / "moved.bin.part"));
    CHECK(contains(r.err, "download failed for https://example.invalid/drops.bin"));    // each failure's own message is still there ...
    CHECK(contains(r.err, "HASH MISMATCH"));
    CHECK(contains(r.err, "fetch: 2 of 4 entries did not make it"));                    // ... and the summary names them, in manifest order
    CHECK(contains(r.err, pad_right("drops", 34) + " exit 4\n"));
    CHECK(contains(r.err, pad_right("moved", 34) + " exit 2\n"));
    CHECK(r.err.find("drops   ") < r.err.find("moved   "));
    const Json receipts = Json::parse(read_text(root / "cache" / "receipts.json"));   // the receipts are the two that made it
    const auto& list = receipts.find("receipts")->as_array();
    REQUIRE(list.size() == 2);
    CHECK(list[0].find("id")->as_string() == "first");
    CHECK(list[1].find("id")->as_string() == "last");

    // when nothing fails the flag changes nothing; and it belongs to `fetch` only
    const fs::path ok = write_manifest(root, {data_entry("first", "first.bin", good), data_entry("last", "last.bin", good)});
    CHECK(run(root, ok, {"fetch", "--keep-going"}).code == kOk);
    CHECK(run(root, ok, {"verify", "--keep-going"}).code == kArgument);
}

TEST_CASE("the stage exception (plan §5 constraint 3's one): accepted on drao-fluxtable with its flag, refused in every other form, and listed", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string digest = std::string(64, 'd');
    const char* const id = "NONCOMMERCIAL-STAGE-EXCEPTION";
    const Json plain = data_entry("thing", "thing.bin", digest);
    const Json drao = with(with(data_entry("drao-fluxtable", "fluxtable.txt", digest, id), "release_blocker", Json(true)), "vendored", Json(true));
    const auto check = [&](const std::vector<Json>& entries) { return run(root, write_manifest(root, entries), {"check-licences"}); };
    const auto refused = [&](const Result& r) {
        CHECK(r.code == kMalformed);
        CHECK(contains(r.err, "THE STAGE EXCEPTION IS NOT AVAILABLE HERE"));
    };

    // accepted: on that entry, with that flag; and listed as a release blocker, with the count of the others
    Result r = check({plain, drao});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "ok       2 entries: 1 on the permissive allowlist, 1 under the stage exception (a release blocker, below)"));
    CHECK(contains(r.out, "RELEASE BLOCKER: plan §5 constraint 3's one exception"));
    CHECK(contains(r.out, "no commercial release may include it:"));
    CHECK(contains(r.out, pad_right("drao-fluxtable", 24) + " " + id + "\n"));
    CHECK(check({with(drao, "licence", Json("noncommercial-stage-exception"))}).code == kOk);   // the id is compared as every licence is: upper() and strip()
    r = check({plain});
    CHECK(contains(r.out, "ok       1 entries, every licence on the permissive allowlist"));        // and nothing is said of blockers when there are none
    CHECK_FALSE(contains(r.out, "RELEASE BLOCKER"));

    // RULE 5: every way of getting it wrong is refused, by name
    refused(check({without(drao, "release_blocker")}));                                              // the id without the flag
    refused(check({with(drao, "release_blocker", Json(false))}));                                    // the flag false
    refused(check({with(drao, "release_blocker", Json("true"))}));                                   // the flag a string
    refused(check({with(drao, "release_blocker", Json(std::int64_t{1}))}));                          // the flag a number
    refused(check({with(drao, "licence", Json("MIT"))}));                                            // the flag without the id, on the entry
    refused(check({with(plain, "release_blocker", Json(true))}));                                    // the flag on another entry
    refused(check({with(plain, "licence", Json(id))}));                                              // the id on another entry, without the flag
    refused(check({with(with(plain, "licence", Json(id)), "release_blocker", Json(true))}));         // the id AND the flag on another entry: it must not spread
    refused(check({drao, with(with(plain, "licence", Json(id)), "release_blocker", Json(true))}));   // a second one beside the first
    refused(check({with(plain, "release_blocker", Json("yes"))}));                                   // a flag that is not a boolean, on an ordinary entry
    refused(check({with(plain, "release_blocker", Json(std::int64_t{1}))}));
    r = check({with(plain, "release_blocker", Json(false))});                                        // a flag that is false is nothing, anywhere
    CHECK(r.code == kOk);
    CHECK(contains(check({without(drao, "release_blocker")}).err, "release_blocker absent"));
    CHECK(contains(check({with(drao, "release_blocker", Json("true"))}).err, "release_blocker \"true\""));
    // the id is NOT on the allowlist proper: nothing else reaches it, and the ordinary refusal does not mention it as permitted
    const Result unlisted = check({with(plain, "licence", Json("CC-BY-NC-4.0"))});
    CHECK(unlisted.code == kMalformed);
    CHECK(contains(unlisted.err, "LICENCE NOT ON THE PERMISSIVE LIST"));
    CHECK_FALSE(contains(unlisted.err, "NONCOMMERCIAL-STAGE-EXCEPTION,"));
}

TEST_CASE("an upstream_mutable entry must be vendored or name an immutable snapshot, or check-licences refuses it", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const Json plain = data_entry("thing", "thing.bin", std::string(64, 'e'));
    const Json mutable_entry = with(plain, "upstream_mutable", Json(true));
    const auto check = [&](const std::vector<Json>& entries) { return run(root, write_manifest(root, entries), {"check-licences"}); };

    // the defect this exists for: pinned, rewritten upstream in place, neither vendored nor backed by a snapshot -- what GitHub's workflow ran into
    Result r = check({mutable_entry});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "UPSTREAM-MUTABLE ENTRY THAT IS NEITHER VENDORED NOR BACKED BY AN IMMUTABLE SNAPSHOT  thing"));
    CHECK(contains(r.err, "MUST"));
    CHECK(contains(r.err, "PROVENANCE.md section 41.3"));
    r = check({data_entry("plain", "plain.bin", std::string(64, 'a')), mutable_entry,
               with(data_entry("other", "other.bin", std::string(64, 'f')), "upstream_mutable", Json(true))});
    CHECK(r.code == kMalformed);                                                  // every offender is named, not the first only
    CHECK(contains(r.err, "SNAPSHOT  thing"));
    CHECK(contains(r.err, "SNAPSHOT  other"));
    CHECK(r.err.find("SNAPSHOT  plain") == std::string::npos);

    // the two ways out
    CHECK(check({with(mutable_entry, "vendored", Json(true))}).code == kOk);
    CHECK(check({with(mutable_entry, "archived_url", Json("https://example.invalid/archive/thing.bin"))}).code == kOk);
    // ... which are not satisfied by the label alone
    CHECK(check({with(mutable_entry, "vendored", Json(false))}).code == kMalformed);
    CHECK(check({with(mutable_entry, "archived_url", Json(""))}).code == kMalformed);
    CHECK(check({with(mutable_entry, "archived_url", Json(std::int64_t{7}))}).code == kMalformed);
    CHECK(check({with(mutable_entry, "archived_url", Json::parse("null"))}).code == kMalformed);

    // an entry that is not upstream_mutable is not asked, and neither is a literature entry (a provenance record, never a build input)
    CHECK(check({with(plain, "upstream_mutable", Json(false))}).code == kOk);
    const Json paper = object({{"id", Json("a-paper")}, {"kind", Json("literature")}, {"terms", Json("NOT ESTABLISHED: searched, found nothing")},
                               {"url", Json("https://example.invalid/p.pdf")}, {"filename", Json("p.pdf")}, {"sha256", Json(std::string(64, 'a'))},
                               {"upstream_mutable", Json(true)}});
    CHECK(check({plain, paper}).code == kOk);

    // it joins the other refusals: a manifest with a licence problem AND an unbacked entry reports both
    r = check({with(plain, "licence", Json("Proprietary")), with(data_entry("other", "other.bin", std::string(64, 'f')), "upstream_mutable", Json(true))});
    CHECK(r.code == kMalformed);
    CHECK(contains(r.err, "LICENCE NOT ON THE PERMISSIVE LIST"));
    CHECK(contains(r.err, "SNAPSHOT  other"));
}

TEST_CASE("--skip-literature: verify neither requires nor checks a literature entry, says how many it left, and can hide neither a data nor a code entry", "[fetcher]") {
    // GitHub's workflow cannot reach several literature hosts (PROVENANCE.md section 41.7), so it leaves literature alone; the flag is a filter on an entry's KIND and on
    // nothing else, and everything below that is not about the papers is about the entries the flag must never be able to hide.
    TempDir td;
    const fs::path root = td.path();
    const std::string paper = "a paper that no build input reads";
    const std::string paper_digest = sha256_hex(as_bytes(paper));
    const std::string data_digest = make_blob(root, "dat", "dat.bin", "data the build reads");
    const std::string code_digest = make_blob(root, "cod", "cod.bin", "code the build reads");
    const fs::path m = write_manifest(root, {data_entry("dat", "dat.bin", data_digest), code_entry("cod", "cod.bin", code_digest),
                                             literature_entry("paper-one", "one.pdf", paper_digest), literature_entry("paper-two", "two.pdf", paper_digest)});

    // without the flag the papers are required like everything else, and nothing is said about skipping
    Result r = run(root, m, {"verify"});
    CHECK(r.code == kMissing);
    CHECK(contains(r.err, "MISSING  paper-one"));
    CHECK(contains(r.err, "MISSING  paper-two"));
    CHECK_FALSE(contains(r.out, "skipped"));

    // with it: neither required nor verified, the count and the reason are said, and the others are verified as ever
    r = run(root, m, {"verify", "--skip-literature"});
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(contains(r.out, "skipped  2 literature entries: neither required nor verified (--skip-literature).\n"));
    CHECK(contains(r.out, "no build input and no test reads"));
    CHECK(contains(r.out, "refuse GitHub's runners"));
    CHECK(contains(r.out, "Every run WITHOUT the"));
    CHECK(contains(r.out, "requires and verifies them"));
    CHECK(contains(r.out, "ok       dat"));
    CHECK(contains(r.out, "ok       cod"));
    CHECK_FALSE(contains(r.out, "ok       paper"));   // nothing is said ok about what was not looked at

    // a paper that IS there is not looked at either, a corrupt one included: the flag is about not reaching for them, and the run says so
    make_blob(root, "paper-one", "one.pdf", "NOT the paper", "papers");
    CHECK(run(root, m, {"verify", "--skip-literature"}).code == kOk);
    CHECK(run(root, m, {"verify"}).code == kMismatch);   // and without the flag the corruption is found
    CHECK(contains(run(root, m, {"verify", "--skip-literature"}).out, "skipped  2 literature entries"));   // the count is what was left, not what was missing

    // THE FLAG CANNOT HIDE AN ENTRY THE BUILD READS.  A data entry missing, a code entry missing and a data entry corrupt each still fail under the flag, by name,
    // and the papers are not blamed; the run still says what it left.
    const fs::path dat = root / "cache" / "dat" / "dat.bin";
    const fs::path cod = root / "cache" / "cod" / "cod.bin";
    fs::remove(dat);
    r = run(root, m, {"verify", "--skip-literature"});
    CHECK(r.code == kMissing);
    CHECK(contains(r.err, "MISSING  dat"));
    CHECK_FALSE(contains(r.err, "paper-"));
    CHECK(contains(r.out, "skipped  2 literature entries"));
    make_blob(root, "dat", "dat.bin", "data the build reads");
    fs::remove(cod);
    r = run(root, m, {"verify", "--skip-literature"});
    CHECK(r.code == kMissing);
    CHECK(contains(r.err, "MISSING  cod"));
    CHECK_FALSE(contains(r.err, "paper-"));
    make_blob(root, "cod", "cod.bin", "code the build reads");
    write_text(dat, "corrupted");
    r = run(root, m, {"verify", "--skip-literature"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "HASH MISMATCH"));
    CHECK(contains(r.err, "entry     dat"));
    make_blob(root, "dat", "dat.bin", "data the build reads");
    CHECK(run(root, m, {"verify", "--skip-literature"}).code == kOk);

    // an entry is literature by its KIND, not by its name or its file: a data entry that looks like a paper is still required
    const fs::path lookalike = write_manifest(root, {data_entry("paper-three", "three.pdf", paper_digest), literature_entry("paper-four", "four.pdf", paper_digest)});
    r = run(root, lookalike, {"verify", "--skip-literature"});
    CHECK(r.code == kMissing);
    CHECK(contains(r.err, "MISSING  paper-three"));
    CHECK_FALSE(contains(r.err, "paper-four"));
    CHECK(contains(r.out, "skipped  1 literature entry: "));   // and the singular is the singular

    // the notice is printed whenever the flag is given, whatever the count: a log that shows a green run shows what that green did not cover
    const fs::path none = write_manifest(root, {data_entry("dat", "dat.bin", data_digest)});
    r = run(root, none, {"verify", "--skip-literature"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "skipped  0 literature entries: "));
}

TEST_CASE("--skip-literature: fetch does not reach for a literature entry, fetches everything else as ever, and a refusing host cannot make it wait", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const std::string content = "the bytes every URL serves\n";
    write_text(root / "body.bin", content);
    const std::string good = sha256_hex(as_bytes(content));
    FakeCurl curl(root);
    curl.deliver(root / "body.bin");
    // paper.txt, not paper.pdf: the sniff would refuse a "PDF" that is not one, and this case is about which URLs are asked for
    const fs::path m = write_manifest(root, {data_entry("dat", "dat.bin", good), literature_entry("paper", "paper.txt", good), code_entry("cod", "cod.bin", good)});

    Result r = run(root, m, {"fetch", "--skip-literature"});
    REQUIRE(r.code == kOk);
    CHECK(contains(r.out, "skipped  1 literature entry: not fetched (--skip-literature).\n"));
    REQUIRE(curl.calls().size() == 2);
    for (const auto& call : curl.calls()) CHECK(call.back() != "https://example.invalid/paper.txt");
    CHECK(fs::exists(root / "cache" / "dat" / "dat.bin"));
    CHECK(fs::exists(root / "cache" / "cod" / "cod.bin"));
    CHECK_FALSE(fs::exists(root / "papers" / "paper" / "paper.txt"));
    const Json receipts = Json::parse(read_text(root / "cache" / "receipts.json"));
    REQUIRE(receipts.find("receipts")->as_array().size() == 2);   // the two that were fetched; the paper has none

    // the same manifest without the flag reaches for all three, and the paper lands where literature lands
    fs::remove_all(root / "cache");
    r = run(root, m, {"fetch"});
    CHECK(r.code == kOk);
    CHECK(curl.calls().size() == 2 + 3);
    CHECK(fs::exists(root / "papers" / "paper" / "paper.txt"));
    CHECK_FALSE(contains(r.out, "skipped"));

    // a host that refuses the runner, for a paper: under the flag nobody asks it, so nobody waits for it ...
    fs::remove_all(root / "cache");
    fs::remove_all(root / "papers");
    curl.fail_only("paper.txt", 28, "curl: (28) Connection timed out after 30001 milliseconds");
    const std::size_t before = curl.calls().size();
    r = run(root, m, {"fetch", "--keep-going", "--skip-literature"});
    CHECK(r.code == kOk);
    CHECK(curl.calls().size() == before + 2);

    // ... while the same refusal of a DATA entry is still a failure under the flag, listed by name: it cannot hide an input the build reads
    fs::remove_all(root / "cache");
    curl.fail_only("dat.bin", 28, "curl: (28) Connection timed out after 30001 milliseconds");
    r = run(root, m, {"fetch", "--keep-going", "--skip-literature"});
    CHECK(r.code == kNetwork);
    CHECK(contains(r.err, "fetch: 1 of 2 entries did not make it"));
    CHECK(contains(r.err, pad_right("dat", 34) + " exit 4\n"));
    CHECK(fs::exists(root / "cache" / "cod" / "cod.bin"));   // the one that could be fetched was
}

TEST_CASE("an archive entry that vendors its members: the tracked members ARE the entry, each against its own pin, and the directory holds exactly them", "[fetcher]") {
    // The shape of vallado-sgp4-verification-vectors (PROVENANCE.md section 41.7): a zip of which the tree reads 33 test files and which also holds source
    // code it must not redistribute.  The archive is not held; its pin stays as upstream's identity.
    TempDir td;
    const fs::path root = td.path();
    const std::string one = "member one\r\nwith a DOS line ending\r\n";   // bytes, kept exactly
    const std::string two = "member two\n";
    const std::string archive_pin(64, 'a');                              // no file here has this hash
    const auto member = [](const std::string& path, const std::string& content) {
        return object({{"member", Json(path)}, {"sha256", Json(sha256_hex(as_bytes(content)))}, {"role", Json("a test file")}, {"extract", Json(true)}});
    };
    const Json entry = object({{"id", Json("bundle")}, {"kind", Json("data")}, {"vendored", Json(true)}, {"vendored_members", Json(true)}, {"licence", Json("CC0-1.0")},
                               {"url", Json("https://example.invalid/bundle.zip")}, {"filename", Json("bundle.zip")}, {"sha256", Json(archive_pin)},
                               {"unpack", Json("zip")}, {"consumes", Json("declared-members")},
                               {"members", Json(Json::Array{member("pkg/dir/one.dat", one), member("pkg/two.dat", two)})}});
    const fs::path dir = root / "vendored" / "bundle";
    make_blob(root, "bundle", "one.dat", one, "vendored");
    make_blob(root, "bundle", "two.dat", two, "vendored");
    const fs::path m = write_manifest(root, {entry});
    FakeCurl curl(root);

    // verify: there is nothing to hash but the members, and it says so
    Result r = run(root, m, {"verify"});
    CHECK(r.code == kOk);
    CHECK(r.err.empty());
    CHECK(contains(r.out, "archive not held (its hash is upstream's identity); 2 members tracked in vendored/bundle/"));
    CHECK(contains(r.out, "member pkg/dir/one.dat"));
    CHECK(contains(r.out, "verified in the repository (tracked)"));
    CHECK(contains(r.out, "consumes declared-members, 2 member(s) read individually"));
    CHECK(run(root, m, {"check-licences"}).code == kOk);

    // fetch: nothing is downloaded, not even under --refresh (there is nothing to compare a download with, and nothing it could repair), and the receipt is the pin
    r = run(root, m, {"fetch"});
    CHECK(r.code == kOk);
    CHECK(contains(r.out, "vendored bundle"));
    CHECK(contains(r.out, "2 members tracked in the repository, never fetched; the archive is not held"));
    CHECK(run(root, m, {"fetch", "--refresh"}).code == kOk);
    CHECK(curl.calls().empty());
    const Json receipts = Json::parse(read_text(root / "cache" / "receipts.json"));
    REQUIRE(receipts.find("receipts")->as_array().size() == 1);
    CHECK(receipts.find("receipts")->as_array()[0].find("sha256")->as_string() == archive_pin);
    CHECK(receipts.find("receipts")->as_array()[0].find("url")->as_string() == "https://example.invalid/bundle.zip");

    // `path`: the directory, or one member by its file name or by its name in the archive
    CHECK(run(root, m, {"path", "bundle"}).out == dir.string() + "\n");
    CHECK(run(root, m, {"path", "bundle", "--member", "one.dat"}).out == (dir / "one.dat").string() + "\n");
    CHECK(run(root, m, {"path", "bundle", "--member", "pkg/two.dat"}).out == (dir / "two.dat").string() + "\n");
    CHECK(run(root, m, {"path", "bundle", "--member", "nope.dat"}).code == kUsage);

    // a member that is not there: named, with where to restore it from; `fetch` refuses in its own words and downloads nothing
    fs::remove(dir / "two.dat");
    r = run(root, m, {"verify"});
    CHECK(r.code == kMissing);
    CHECK(contains(r.err, "MISSING  bundle"));
    CHECK(contains(r.err, "vendored member pkg/two.dat -- restore from git, do not fetch: vendored/bundle/two.dat"));
    CHECK(contains(r.err, "1 vendored entry is missing their own tracked file(s). `fetch fetch` will not help"));
    r = run(root, m, {"fetch"});
    CHECK(r.code == kMissing);
    CHECK(contains(r.err, "vendors its members but 1 of its tracked files is missing"));
    CHECK(contains(r.err, "git checkout -- vendored/bundle"));
    CHECK(curl.calls().empty());
    make_blob(root, "bundle", "two.dat", two, "vendored");

    // a member that is not the declared one: refused by its own pin, naming both hashes, and left as it was found
    write_text(dir / "one.dat", "tampered");
    r = run(root, m, {"verify"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "VENDORED MEMBER HASH MISMATCH"));
    CHECK(contains(r.err, "member   pkg/dir/one.dat"));
    CHECK(contains(r.err, "declared " + sha256_hex(as_bytes(one))));
    CHECK(contains(r.err, "got      " + sha256_hex(as_bytes(std::string("tampered")))));
    CHECK(run(root, m, {"fetch"}).code == kMismatch);
    CHECK(read_text(dir / "one.dat") == "tampered");
    make_blob(root, "bundle", "one.dat", one, "vendored");

    // THE DIRECTORY HOLDS EXACTLY THE DECLARED MEMBERS: a file nobody declared (a source file, a copy of the archive, one in a subdirectory) is refused, by name,
    // by both commands -- "none of the code" is checked, not promised
    for (const std::string stray : {"extra.txt", "bundle.zip", "sub/code.cpp"}) {
        INFO("stray file " << stray);
        fs::create_directories((dir / stray).parent_path());
        write_text(dir / stray, "what the entry does not declare");
        r = run(root, m, {"verify"});
        CHECK(r.code == kMismatch);
        CHECK(contains(r.err, "VENDORED DIRECTORY HOLDS WHAT THE ENTRY DOES NOT DECLARE"));
        CHECK(contains(r.err, "undeclared " + stray));
        CHECK(run(root, m, {"fetch"}).code == kMismatch);
        fs::remove_all(dir / *fs::path(stray).begin());
    }
    // every stray file is named, in one sorted list, whatever order the directory lists them in (created in alphabetical order, which a file system that lists
    // the newest first, as tmpfs does, hands back reversed: five files are not sorted by luck)
    for (const char* name : {"a.txt", "b.txt", "c.txt", "d.txt", "e.txt"}) write_text(dir / name, "stray");
    r = run(root, m, {"verify"});
    CHECK(r.code == kMismatch);
    CHECK(contains(r.err, "undeclared a.txt, b.txt, c.txt, d.txt, e.txt\n"));
    for (const char* name : {"a.txt", "b.txt", "c.txt", "d.txt", "e.txt"}) fs::remove(dir / name);
    // a symbolic link is a file too, and the cheapest way to put something in a directory without holding its bytes here: pointing at a file that is there, and at one that is not
    write_text(root / "elsewhere.txt", "kept somewhere else");
    for (const fs::path& target : {root / "elsewhere.txt", root / "no-such-file.txt"}) {
        INFO("symlink to " << target);
        fs::create_symlink(target, dir / "link.txt");
        r = run(root, m, {"verify"});
        CHECK(r.code == kMismatch);
        CHECK(contains(r.err, "undeclared link.txt"));
        fs::remove(dir / "link.txt");
    }
    CHECK(run(root, m, {"verify"}).code == kOk);   // and clean again
}

TEST_CASE("an archive entry that vendors its members must say what it vendors, or the manifest is malformed", "[fetcher]") {
    TempDir td;
    const fs::path root = td.path();
    const auto member = [](const std::string& path, const std::string& digest) {
        return object({{"member", Json(path)}, {"sha256", Json(digest)}, {"extract", Json(true)}});
    };
    const std::string h1(64, '1');
    const std::string h2(64, '2');
    const Json entry = object({{"id", Json("bundle")}, {"kind", Json("data")}, {"vendored", Json(true)}, {"vendored_members", Json(true)}, {"licence", Json("CC0-1.0")},
                               {"url", Json("https://example.invalid/bundle.zip")}, {"filename", Json("bundle.zip")}, {"sha256", Json(std::string(64, 'a'))},
                               {"unpack", Json("zip")}, {"consumes", Json("declared-members")}, {"members", Json(Json::Array{member("pkg/one.dat", h1), member("pkg/two.dat", h2)})}});
    const auto refused = [&](const Json& e, const std::string& words) {
        const Result r = run(root, write_manifest(root, {e}), {"verify"});
        INFO("expected: " << words << "; got: " << r.err);
        CHECK(r.code == kMalformed);
        CHECK(contains(r.err, words));
    };
    refused(without(entry, "vendored"), "vendored_members is set, but the entry is not vendored");
    refused(with(entry, "vendored_members", Json("yes")), "vendored_members must be true or false");
    refused(with(entry, "members", Json(Json::Array{})), "vendored_members names no members");
    refused(without(entry, "members"), "vendored_members names no members");
    refused(with(entry, "members", Json(Json::Array{member("a/same.dat", h1), member("b/same.dat", h2)})), "two members are both called 'same.dat'");
    refused(with(entry, "members", Json(Json::Array{member("pkg/one.dat", "not-a-hash")})), "has no sha256 of 64 hex characters");
    refused(with(entry, "members", Json(Json::Array{object({{"member", Json("pkg/one.dat")}})})), "has no sha256 of 64 hex characters");
    refused(with(entry, "members", Json(Json::Array{object({{"member", Json(std::int64_t{3})}, {"sha256", Json(h1)}})})), "every member of a vendored_members entry must be an object");
    refused(with(entry, "members", Json(Json::Array{member("dir/", h1)})), "every member of a vendored_members entry must be an object");

    // false is the ordinary vendored entry: the archive itself is the tracked file, and it is not there
    const Result plain = run(root, write_manifest(root, {with(entry, "vendored_members", Json(false))}), {"verify"});
    CHECK(plain.code == kMissing);
    CHECK(contains(plain.err, "vendored/bundle/bundle.zip"));
}
