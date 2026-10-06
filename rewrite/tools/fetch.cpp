// fetch.cpp — the manifest fetcher.
//
// Plan L0 step 3: every external input is declared with URL, SHA-256 and a licence note, and is fetched from origin into a cache.  Nothing
// enters the tree undeclared.  This is rule R11's discipline — the identity of an input is its hash, not its URL and not its version
// number — applied to code as well as data, because upstream is mutable in both.
//
// PORTED FROM tools/fetch.py (plan L0 step 8, the user's directive of 2026-10-06): the commands, their output, their messages and their exit
// codes are the Python tool's, and the behaviour the Python tool's tests pinned is pinned again by tests/devtools/fetch_tests.cpp.  Where the
// port differs it says so at the place, and the differences are small and deliberate: `list` and `path --member`, which had no caller and
// crashed on a literature entry and on an undefined name, work; a download that fails the sniff leaves no `.part` file behind; and only
// https URLs are fetched.
//
// The tool is built TWICE: at configure time by cmake/OdlBuildHostTool.cmake (the build cannot build C++ before it has verified the manifest,
// so the one tool that does that is compiled first, by the configured compiler), and again as an object library for the tests.
//
// Host program: `curl` (the manifest's `curl` tool entry, developed against 8.18.0), spawned by `fetch` ONLY, to download.  `verify`,
// `check-licences`, `verify-populated`, the build and the tests never need it.
//
// Commands
// --------
//   verify              offline.  Every entry present in the cache and hash-correct?
//   fetch               download what is missing, verify, then stop.
//   fetch --refresh     re-download everything and report upstream drift.
//   list [--json]       the entries, for humans.
//   path <id>           the cache path of one entry.
//   check-licences      plan §5 constraint 3: only licences on the permissive allowlist.
//   verify-populated    is a populated FetchContent tree the archive we pinned?
//
// Exit codes are distinct because CI reads them:
//   0 ok   1 missing from cache   2 HASH MISMATCH   3 manifest malformed   4 network failure   5 usage error   (argument errors: 2, as argparse)

#include <odl/devkit/archive.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/inflate.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/process.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "fetch.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace odl::tools::fetch {

namespace {

namespace dk = odl::devkit;
namespace fs = std::filesystem;
using dk::Json;

constexpr const char* kTool = "fetch";
constexpr int kOk = 0, kMissing = 1, kMismatch = 2, kMalformed = 3, kNetwork = 4, kUsage = 5;
constexpr int kArgumentError = 2;   // argparse's own code
constexpr int kTimeoutSeconds = 120;
constexpr const char* kUserAgent = "odl-self_built-fetch/1";

// ------------------------------------------------------------------------------------------------------------------------ small helpers

using Io = dk::Streams;

[[noreturn]] void die(Io& io, int code, const std::string& msg) { dk::die(io, kTool, code, msg); }

/// `e.member_of(key)` as a Python caller saw it: the member, or nullptr.
const Json* member_of(const Json& e, std::string_view key) { return e.find(key); }

/// Python's truth value of `e.member_of(key)`.
bool truthy(const Json& e, std::string_view key) {
    const Json* v = e.find(key);
    return v != nullptr && v->truthy();
}

/// `str(e.member_of(key, dflt))` for a value that is a string or an integer; anything else is shown as JSON.
std::string text_of(const Json& e, std::string_view key, std::string_view dflt = "") {
    const Json* v = e.find(key);
    if (v == nullptr) return std::string(dflt);
    if (v->is_string()) return v->as_string();
    if (v->is_int()) return std::to_string(v->as_int());
    if (v->is_null()) return "None";
    return v->dumps(2);
}

/// repr() of a JSON value as the Python tool printed it in a message.
std::string py_repr_json(const Json* v) {
    if (v == nullptr || v->is_null()) return "None";
    if (v->is_bool()) return v->as_bool() ? "True" : "False";
    if (v->is_int()) return std::to_string(v->as_int());
    if (v->is_string()) return dk::py_repr(v->as_string());
    return v->dumps(2);
}

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string upper_stripped(const std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b])) != 0) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) --e;
    std::string out = s.substr(b, e - b);
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

std::string first16(const std::string& hash) { return hash.substr(0, 16); }

const std::string& str_field(const Json& e, std::string_view key) { return e.find(key)->as_string(); }

// ------------------------------------------------------------------------------------------------------------------------ manifest

struct Manifest {
    Json doc;
    fs::path root;
    const Json::Array& entries() const { return doc.find("entries")->as_array(); }
    std::string setting(std::string_view key, const char* dflt) const {
        const Json* v = doc.find(key);
        return (v != nullptr && v->is_string()) ? v->as_string() : std::string(dflt);
    }
};

Json load_manifest(Io& io, const fs::path& path) {
    std::string text;
    try {
        text = dk::read_text(path);
    } catch (const std::runtime_error& exc) {
        die(io, kMalformed, "cannot read manifest " + path.string() + ": " + exc.what());
    }
    Json doc;
    try {
        doc = Json::parse(text);
    } catch (const dk::JsonError& exc) {
        die(io, kMalformed, path.string() + ": not valid JSON at line " + std::to_string(exc.line) + ": " + exc.reason);
    }
    if (!doc.is_object()) die(io, kMalformed, path.string() + ": the manifest must be a JSON object");

    const Json* schema = member_of(doc, "schema");
    if (schema == nullptr || !schema->is_int() || schema->as_int() != 1) {
        die(io, kMalformed, path.string() + ": unsupported schema " + py_repr_json(schema) + "; this tool speaks schema 1");
    }
    const Json* entries = member_of(doc, "entries");
    if (entries == nullptr || !entries->is_array()) die(io, kMalformed, path.string() + ": 'entries' must be a list");

    std::map<std::string, std::size_t> seen;
    for (std::size_t i = 0; i < entries->as_array().size(); ++i) {
        const Json& e = entries->as_array()[i];
        const std::string where = path.string() + ": entry " + std::to_string(i);
        if (!e.is_object()) die(io, kMalformed, where + ": an entry must be a JSON object");
        // `licence` is required of everything this tree REDISTRIBUTES. A `literature` entry is a provenance record and carries `terms` instead —
        // the record of where its terms were SOUGHT, which is the honest field when they could not be established (plan §5 constraint 3, rule 4).
        const Json* kind_v = member_of(e, "kind");
        const bool is_lit = kind_v != nullptr && kind_v->is_string() && kind_v->as_string() == "literature";
        const char* required[3] = {"id", "kind", is_lit ? "terms" : "licence"};
        for (const char* field : required) {
            if (!truthy(e, field)) die(io, kMalformed, where + ": missing required field " + dk::py_repr(field));
        }
        if (!member_of(e, "id")->is_string() || !member_of(e, "kind")->is_string()) die(io, kMalformed, where + ": 'id' and 'kind' must be strings");
        const std::string& id = str_field(e, "id");
        const auto dup = seen.find(id);
        if (dup != seen.end()) {
            die(io, kMalformed, where + ": duplicate id " + dk::py_repr(id) + " (first seen at entry " + std::to_string(dup->second) + ")");
        }
        seen.emplace(id, i);
        const std::string& kind = str_field(e, "kind");
        if (kind != "code" && kind != "data" && kind != "tool" && kind != "literature") {
            die(io, kMalformed, where + ": kind must be code, data, tool or literature, not " + dk::py_repr(kind));
        }
        if (is_lit && truthy(e, "licence")) {
            die(io, kMalformed,
                where +
                    ": a literature entry must NOT carry a 'licence'. It is exempt from the permissive gate because it is a provenance record "
                    "rather than a dependency, and a licence field on it would invite the exemption to be read as a grant. Put what was found, or "
                    "not found, in 'terms'.");
        }
        if (truthy(e, "provided_by_host")) continue;
        for (const char* field : {"url", "filename", "sha256"}) {
            if (!truthy(e, field)) {
                die(io, kMalformed,
                    where + " (" + id + "): missing required field " + dk::py_repr(field) +
                        "; an entry that is not provided_by_host must be pinned by URL and hash");
            }
        }
        if (!member_of(e, "sha256")->is_string()) die(io, kMalformed, where + " (" + id + "): sha256 must be a string");
        const std::string& h = str_field(e, "sha256");
        const std::string hl = lower(h);
        const bool hex = std::all_of(hl.begin(), hl.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
        if (h.size() != 64 || !hex) die(io, kMalformed, where + " (" + id + "): sha256 " + dk::py_repr(h) + " is not 64 lowercase hex characters");
    }
    return doc;
}

std::vector<const Json*> fetchable(const Manifest& m) {
    std::vector<const Json*> out;
    for (const Json& e : m.entries()) {
        if (!truthy(e, "provided_by_host")) out.push_back(&e);
    }
    return out;
}

fs::path cache_dir(const Manifest& m) { return m.root / m.setting("cache", "data/cache"); }

// Where `literature` entries land, and it is NOT the build cache.  Plan §5 constraint 3's first mechanical condition: a literature entry is
// fetched to a path no build target and no test references.
fs::path literature_dir(const Manifest& m) { return m.root / m.setting("literature", "data/literature"); }

// Where a `vendored` entry's own bytes live — TRACKED in the repository, unlike data/cache/ (gitignored, re-fetched) and data/literature/ (also
// gitignored, fetched but kept off the build path).  L6 step 2's own finding (PROVENANCE.md §37.8): an upstream whose own responses are not
// byte-stable — Horizons embeds its own request-processing timestamp in every reply, so no two live fetches of the identical query ever
// hash-match — cannot be re-fetched into the (gitignored) cache on a fresh clone at all; the pinned bytes exist nowhere unless the repository
// itself holds them.  Vendoring is the general answer: the manifest still names the URL the bytes originally came from and the licence basis
// that permits redistributing them, but `fetch`/`fetch --refresh` never try to re-acquire them, and `verify` checks the TRACKED copy directly.
fs::path vendored_dir(const Manifest& m) { return m.root / m.setting("vendored", "data/vendored"); }

bool is_literature(const Json& e) { return text_of(e, "kind") == "literature"; }
bool is_vendored(const Json& e) { return truthy(e, "vendored"); }

fs::path entry_path(const Manifest& m, const Json& e) {
    const fs::path base = is_vendored(e) ? vendored_dir(m) : is_literature(e) ? literature_dir(m) : cache_dir(m);
    return base / str_field(e, "id") / str_field(e, "filename");
}

// ------------------------------------------------------------------------------------------------------------------------ archives

// The archive members this tree reads individually, each with its own hash.
//
// AN ARCHIVE HASH SAYS NOTHING ABOUT WHAT IS TAKEN OUT OF AN ARCHIVE.  EGM2008 ships as seven members of which this tree consumes two, and
// one of the five it does not is a FORTRAN harmonic-synthesis program that plan §3.3 step 2 forbids as a source of recursions.  Declaring the
// members by hash is what turns "we did not open it" from an assertion into something a reviewer can check.
//
// That case prompted the rule and the rule is not about that case.  Every multi-member archive here has members read out of it individually —
// a licence text quoted verbatim into NOTICE, a file compared against a populated FetchContent tree — and each of those is now pinned as well
// as its container.  A rule that applies only to the case that revealed it is the shape this project keeps catching.
//
// Members with `extract` land in the cache because something outside the tools needs a path to them; the rest are verified in place, read
// straight out of the archive, because duplicating bytes to check them is not an improvement.
std::vector<const Json*> entry_members(const Json& e) {
    std::vector<const Json*> out;
    const Json* m = member_of(e, "members");
    if (m != nullptr && m->is_array()) {
        for (const Json& x : m->as_array()) out.push_back(&x);
    }
    return out;
}

fs::path extract_path(const Manifest& m, const Json& e, const Json& member) {
    return cache_dir(m) / str_field(e, "id") / "extracted" / fs::path(str_field(member, "member")).filename();
}

// The archive being read, and its decompressed bytes if it is a .tar.gz: the members of one entry are read one after another, and a tarball
// is decompressed once for all of them.
struct ArchiveCache {
    fs::path path;
    dk::Bytes tar;
    bool loaded = false;
};

dk::Bytes open_member(const fs::path& archive, const Json& e, const std::string& member, ArchiveCache& cache) {
    const std::string kind = text_of(e, "unpack", "None");
    if (kind == "gzip") {
        // A bare .gz holds ONE member and does not name it, so the declared member name is this tree's name for the decompressed bytes rather
        // than something read out of the container.  It still carries its own hash, which is the point: the hash of a gzip stream depends on
        // the compressor, and the hash of what comes out of it does not.
        const dk::Bytes raw = dk::read_bytes(archive);
        return dk::gunzip(dk::ByteView{raw.data(), raw.size()});
    }
    if (kind == "zip") {
        const dk::Bytes raw = dk::read_bytes(archive);
        auto got = dk::zip_member(dk::ByteView{raw.data(), raw.size()}, member);
        if (!got) throw std::runtime_error("There is no item named " + dk::py_repr(member) + " in the archive");
        return std::move(*got);
    }
    if (kind == "tar.gz") {
        const std::string root = truthy(e, "unpacked_root") ? text_of(e, "unpacked_root") : std::string();
        const std::string inner = root.empty() ? member : root + "/" + member;
        if (!cache.loaded || cache.path != archive) {
            const dk::Bytes raw = dk::read_bytes(archive);
            cache.tar = dk::gunzip(dk::ByteView{raw.data(), raw.size()});
            cache.path = archive;
            cache.loaded = true;
        }
        auto got = dk::tar_member(dk::ByteView{cache.tar.data(), cache.tar.size()}, inner);
        if (!got) throw std::runtime_error("filename " + dk::py_repr(inner) + " not found");
        return std::move(*got);
    }
    throw std::runtime_error("entry " + str_field(e, "id") + " declares members but unpack is " + py_repr_json(member_of(e, "unpack")));
}

// ------------------------------------------------------------------------------------------------------------------------ hashing

std::string sha256_file(Io& io, const fs::path& p) {
    try {
        return dk::sha256_file_hex(p);
    } catch (const std::runtime_error& exc) {
        die(io, kMalformed, exc.what());
    }
}

[[noreturn]] void mismatch(Io& io, const Json& e, const std::string& got, const std::string& where) {
    const std::string what = member_of(e, "role") != nullptr ? text_of(e, "role") : text_of(e, "kind");   // e.get('role', e['kind'])
    die(io, kMismatch,
        "HASH MISMATCH — refusing.\n"
        "  entry     " + str_field(e, "id") + " (" + what + ")\n"
        "  url       " + text_of(e, "url") + "\n"
        "  expected  " + str_field(e, "sha256") + "\n"
        "  obtained  " + got + "\n"
        "  at        " + where + "\n"
        "\n"
        "  The identity of an input is its hash.  Upstream changing under an unchanged\n"
        "  URL is the documented behaviour of at least one of this tree's data sources\n"
        "  (IERS EOP, revised retroactively three times: 2025-06-05, 2026-02-05,\n"
        "  2026-03-09), so this is a fault to investigate, not a hash to update.\n"
        "  If the change is intended, updating the manifest is a deliberate, reviewed,\n"
        "  logged act — see plan §3.11 point 3.");
}

// Verifies every declared member against its own hash.  Returns their names.
std::vector<std::string> verify_members(Io& io, const Manifest& m, const Json& e, ArchiveCache& cache) {
    std::vector<std::string> seen;
    for (const Json* member : entry_members(e)) {
        const std::string name = text_of(*member, "member");
        const std::string want = lower(text_of(*member, "sha256"));
        std::string got;
        if (truthy(*member, "extract")) {
            const fs::path q = extract_path(m, e, *member);
            if (!fs::exists(q)) throw std::runtime_error(q.string());   // FileNotFoundError in the Python tool: the caller reports it as MISSING
            got = sha256_file(io, q);
        } else {
            try {
                const dk::Bytes b = open_member(entry_path(m, e), e, name, cache);
                got = dk::sha256_hex(dk::ByteView{b.data(), b.size()});
            } catch (const std::exception& exc) {
                die(io, kMalformed, str_field(e, "id") + ": cannot read member " + dk::py_repr(name) + ": " + exc.what());
            }
        }
        if (got != want) {
            die(io, kMismatch,
                "ARCHIVE MEMBER HASH MISMATCH — refusing.\n"
                "  entry    " + str_field(e, "id") + "\n"
                "  member   " + name + "\n"
                "  declared " + text_of(*member, "sha256") + "\n"
                "  got      " + got + "\n"
                "\n"
                "  The archive's own hash may still match: an archive hash says nothing about\n"
                "  what is taken out of an archive, which is why the members are declared.");
        }
        seen.push_back(name);
    }
    return seen;
}

// Unpacks the declared members, AFTER the archive's own hash has verified.
//
// Each member is checked against its own declared SHA-256 as it lands, so a change in upstream's zip tooling cannot quietly change what this
// tree reads while the archive hash still matches — the archive hash would change too, but the member hash is what the module actually consumes
// and it is the one worth stating.
std::vector<std::pair<std::string, std::string>> extract_members(Io& io, const Manifest& m, const Json& e) {
    std::vector<std::pair<std::string, std::string>> out;
    std::vector<const Json*> wanted;
    for (const Json* member : entry_members(e)) {
        if (truthy(*member, "extract")) wanted.push_back(member);
    }
    if (wanted.empty()) return out;
    const fs::path archive = entry_path(m, e);
    ArchiveCache cache;
    for (const Json* member : wanted) {
        const fs::path dest = extract_path(m, e, *member);
        const std::string name = text_of(*member, "member");
        const std::string want = lower(text_of(*member, "sha256"));
        if (fs::exists(dest) && sha256_file(io, dest) == want) {
            out.emplace_back(name, "cached");
            continue;
        }
        fs::create_directories(dest.parent_path());
        const fs::path part = dest.string() + ".part";
        try {
            const dk::Bytes b = open_member(archive, e, name, cache);
            dk::write_bytes(part, dk::ByteView{b.data(), b.size()});
        } catch (const std::exception& exc) {
            die(io, kMalformed, str_field(e, "id") + ": cannot extract " + dk::py_repr(name) + " from " + archive.string() + ": " + exc.what());
        }
        const std::string got = sha256_file(io, part);
        if (got != want) {
            std::error_code ignored;
            fs::remove(part, ignored);
            die(io, kMismatch,
                "EXTRACTED MEMBER HASH MISMATCH — refusing.\n"
                "  entry    " + str_field(e, "id") + "\n"
                "  member   " + name + "\n"
                "  declared " + text_of(*member, "sha256") + "\n"
                "  got      " + got + "\n"
                "  archive  " + archive.string());
        }
        fs::rename(part, dest);
        out.emplace_back(name, got);
    }
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------ the sniff

// WHAT A FILE HAS TO LOOK LIKE BEFORE IT IS WORTH HASHING.
//
// `content/chapter10/icc10.pdf` is a 404 — every other chapter of the IERS Conventions is `iccN.pdf` and chapter 10 is `tn36_c10.pdf` — and curl
// saves the error page with exit status 0 (unless it is asked to fail).  The SHA-256 then catches it, but only on the SECOND fetch: the first
// fetch is the one that computes the hash you write into the manifest, and that is the fetch that matters.  An HTML error page hashes
// perfectly well.
//
// So the bytes are sniffed before they are hashed.  Two rules, and the first is the one that earns its place: anything that begins as an HTML
// document is refused whatever it was supposed to be, because no input this tree declares is HTML.  The second checks the magic against the
// declared extension where there is one to check.
struct Magic {
    const char* suffix;
    std::string_view bytes;
    const char* what;
};
const Magic kMagic[] = {
    {".pdf", "%PDF", "a PDF"},
    {".gz", std::string_view("\x1f\x8b", 2), "a gzip stream"},
    {".tgz", std::string_view("\x1f\x8b", 2), "a gzip stream"},
    {".zip", std::string_view("PK\x03\x04", 4), "a zip archive"},
    {".bsp", "DAF/SPK", "a DAF/SPK kernel"},
};
const std::string_view kHtmlStarts[] = {"<!doctype", "<html", "<?xml"};

// pathlib's PurePath.suffix: the final component's extension, "" for a leading dot alone or a trailing dot.
std::string path_suffix(const std::string& filename) {
    const std::string name = fs::path(filename).filename().string();
    const std::size_t i = name.rfind('.');
    if (i == std::string::npos || i == 0 || i == name.size() - 1) return "";
    return name.substr(i);
}

}  // namespace

void sniff(Io& io, const Json& e, dk::ByteView head) {
    // head.lstrip()[:16].lower()
    std::size_t b = 0;
    while (b < head.size() && (head[b] == ' ' || (head[b] >= '\t' && head[b] <= '\r'))) ++b;
    std::string low(dk::as_text(head.subspan(b, std::min<std::size_t>(16, head.size() - b))));
    low = lower(low);
    const auto shown = [&] { return dk::py_repr_bytes(head.subspan(0, std::min<std::size_t>(48, head.size()))); };
    for (const std::string_view h : kHtmlStarts) {
        if (low.compare(0, h.size(), h) == 0) {
            die(io, kMalformed,
                text_of(e, "id") + ": the server returned an HTML document, not a data file.\n"
                "  url    " + text_of(e, "url") + "\n"
                "  begins " + shown() + "\n"
                "\n"
                "  This is almost always a 404 page saved with exit status 0. It would hash\n"
                "  perfectly well, and the manifest would then pin the error page.");
        }
    }
    const std::string suffix = lower(path_suffix(text_of(e, "filename")));
    for (const Magic& m : kMagic) {
        if (suffix != m.suffix) continue;
        if (head.size() < m.bytes.size() || dk::as_text(head.subspan(0, m.bytes.size())) != m.bytes) {
            die(io, kMalformed,
                text_of(e, "id") + ": " + text_of(e, "filename") + " does not begin like " + m.what + ".\n"
                "  url    " + text_of(e, "url") + "\n"
                "  begins " + shown() + "\n"
                "  expected the first bytes to be " + dk::py_repr_bytes(dk::as_bytes(m.bytes)));
        }
    }
}

namespace {

// ------------------------------------------------------------------------------------------------------------------------ download

// Downloads to a .part file and returns its hash.  The caller renames only after the hash has been checked, so a failed or truncated download
// can never leave something behind that a later `verify` would accept.
//
// The FIRST block is sniffed before anything is hashed (see `sniff`): an HTML error page hashes perfectly well, and on a first fetch — the one
// whose hash goes into the manifest — the hash has nothing to disagree with.
std::string download(Io& io, const std::string& url, const fs::path& dest, const Json& e) {
    fs::create_directories(dest.parent_path());
    const fs::path part = dest.string() + ".part";
    std::error_code ignored;
    if (url.rfind("https://", 0) != 0) {
        die(io, kMalformed, str_field(e, "id") + ": the URL " + dk::py_repr(url) + " is not https; this tool downloads over TLS only");
    }
    const std::vector<std::string> argv = {"curl", "--silent", "--show-error", "--fail", "--location", "--max-redirs", "10", "--proto", "=https",
                                           "--proto-redir", "=https", "--user-agent", kUserAgent, "--connect-timeout", std::to_string(kTimeoutSeconds),
                                           "--speed-limit", "1", "--speed-time", std::to_string(kTimeoutSeconds), "--output", part.string(), url};
    dk::ProcessResult r;
    try {
        r = dk::run_process(argv);
    } catch (const std::runtime_error& exc) {
        fs::remove(part, ignored);
        die(io, kNetwork, "download failed for " + url + ": " + exc.what() + " (curl is the one host program `fetch` needs; the manifest declares it)");
    }
    if (r.exit_code != 0) {
        fs::remove(part, ignored);
        std::string why = r.err;
        while (!why.empty() && (why.back() == '\n' || why.back() == '\r')) why.pop_back();
        die(io, kNetwork, "download failed for " + url + ": curl exited " + std::to_string(r.exit_code) + (why.empty() ? "" : ": " + why));
    }
    // The first block (1 MiB, as the Python tool read it) is sniffed; the whole file is then hashed.  A refusal leaves no `.part` behind.
    try {
        std::ifstream in(part, std::ios::binary);
        std::vector<char> first(std::size_t{1} << 20);
        in.read(first.data(), static_cast<std::streamsize>(first.size()));
        sniff(io, e, dk::ByteView{reinterpret_cast<const std::uint8_t*>(first.data()), static_cast<std::size_t>(in.gcount())});
        return dk::sha256_file_hex(part);
    } catch (const dk::Exit&) {
        fs::remove(part, ignored);
        throw;
    } catch (const std::runtime_error& exc) {
        fs::remove(part, ignored);
        die(io, kNetwork, "download failed for " + url + ": " + exc.what());
    }
}

// ------------------------------------------------------------------------------------------------------------------------ receipts

void write_receipt(Io& io, const Manifest& m, const std::vector<std::array<std::string, 3>>& results) {
    const fs::path rec = cache_dir(m) / "receipts.json";
    fs::create_directories(rec.parent_path());
    std::time_t t = std::time(nullptr);
    std::tm utc{};
    ::gmtime_r(&t, &utc);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &utc);
    std::map<std::string, Json> existing;
    if (fs::exists(rec)) {
        try {
            const Json old = Json::parse(dk::read_text(rec));
            if (const Json* list = member_of(old, "receipts"); list != nullptr && list->is_array()) {
                for (const Json& r : list->as_array()) existing[str_field(r, "id")] = r;
            }
        } catch (const std::exception&) {
            existing.clear();
        }
    }
    for (const auto& r : results) {
        Json::Object o;
        o.emplace_back("id", Json(r[0]));
        o.emplace_back("url", Json(r[1]));
        o.emplace_back("sha256", Json(r[2]));
        o.emplace_back("retrieved", Json(std::string(stamp)));
        existing[r[0]] = Json(std::move(o));
    }
    Json::Array list;
    for (auto& kv : existing) list.push_back(kv.second);   // sorted by id, as the Python tool sorted them
    Json::Object top;
    top.emplace_back("receipts", Json(std::move(list)));
    try {
        dk::write_text(rec, Json(std::move(top)).dumps(2) + "\n");
    } catch (const std::runtime_error& exc) {
        die(io, kMalformed, exc.what());
    }
}

// ------------------------------------------------------------------------------------------------------------------------ commands

struct Args {
    bool refresh = false;
    bool json = false;
    std::string id;
    std::string member;
    bool have_member = false;
    std::string populated;
};

int cmd_verify(Io& io, const Manifest& m) {
    std::vector<const Json*> missing;
    std::vector<std::tuple<const Json*, std::string, fs::path>> bad;
    std::vector<const Json*> ok;
    const std::vector<const Json*> fetch_list = fetchable(m);
    for (const Json* e : fetch_list) {
        const fs::path p = entry_path(m, *e);
        if (!fs::exists(p)) {
            missing.push_back(e);
            continue;
        }
        const std::string got = sha256_file(io, p);
        if (got != lower(str_field(*e, "sha256"))) {
            bad.emplace_back(e, got, p);
        } else {
            ok.push_back(e);
        }
    }

    for (const auto& [e, got, p] : bad) mismatch(io, *e, got, p.string());

    std::vector<std::string> undeclared;
    for (const Json* e : fetch_list) {
        if (truthy(*e, "unpack") && !truthy(*e, "consumes")) undeclared.push_back(str_field(*e, "id"));
    }
    if (!undeclared.empty()) {
        std::string ids;
        for (std::size_t i = 0; i < undeclared.size(); ++i) ids += (i ? ", " : "") + undeclared[i];
        die(io, kMalformed,
            "an archive entry does not say what this tree consumes from it: " + ids + "\n"
            "  Every entry with 'unpack' must declare 'consumes' — 'whole-tree' or\n"
            "  'declared-members' — with a note, and list the members it reads individually\n"
            "  with their own SHA-256. An archive hash says nothing about what is taken out\n"
            "  of an archive.");
    }

    // A COLUMN LIST IS TO A TABLE WHAT A MEMBER LIST IS TO AN ARCHIVE.
    //
    // The archive guard above exists because an archive's hash says nothing about WHICH MEMBER this tree reads out of it.  The same hole opens in a
    // plain table whose columns do not all carry the same licence: `gfz-kp-ap-f107` is CC BY 4.0 except its sunspot column, which is CC BY-NC 4.0.
    //
    // The first version of this guard asked the entry to declare the COMPOUND licence, and that was wrong: it would have put a non-commercial term
    // on the allowlist, which is exactly what the allowlist exists to prevent.  An entry instead declares the licence OF WHAT THIS TREE CONSUMES,
    // and names any other licence in the file under `licence_excluded` with the columns that carry it.  Then the allowlist keeps its meaning and
    // the exclusion is checkable rather than asserted (SPEC-atmosphere ATMO-R-032, ATMO-F-016).
    for (const Json* e : fetch_list) {
        const Json* excluded = member_of(*e, "licence_excluded");
        if (excluded == nullptr || !excluded->truthy()) continue;
        const Json* columns = member_of(*e, "columns");
        const Json* declared = columns != nullptr ? member_of(*columns, "declared") : nullptr;
        if (declared == nullptr || !declared->truthy()) {
            die(io, kMalformed,
                str_field(*e, "id") + " excludes licensed content but declares no columns.\n"
                "  A file whose licence differs BY COLUMN is the case a headline-licence\n"
                "  check cannot see. Declare 'columns': {'declared': [...], 'excluded': {...}}\n"
                "  so the claim is checkable rather than asserted.");
        }
        std::set<std::string> declared_set;
        for (const Json& c : declared->as_array()) declared_set.insert(c.as_string());
        for (const auto& [lic, cols] : excluded->as_object()) {
            std::set<std::string> overlap;
            for (const Json& c : cols.as_array()) {
                if (declared_set.count(c.as_string()) != 0) overlap.insert(c.as_string());
            }
            if (!overlap.empty()) {
                std::string names;
                for (const std::string& n : overlap) names += (names.empty() ? "" : ", ") + n;
                die(io, kMalformed,
                    str_field(*e, "id") + " declares column(s) " + names + " as consumed, but they\n"
                    "  carry " + lic + ", which this entry excludes. Either the licence is wrong or\n"
                    "  this tree is reading something it says it does not.");
            }
        }
    }

    std::vector<std::string> badcols;
    for (const Json* e : fetch_list) {
        if (text_of(*e, "consumes") != "declared-columns") continue;
        const Json* columns = member_of(*e, "columns");
        const Json* declared = columns != nullptr ? member_of(*columns, "declared") : nullptr;
        if (declared == nullptr || !declared->truthy()) badcols.push_back(str_field(*e, "id"));
    }
    if (!badcols.empty()) {
        std::string ids;
        for (std::size_t i = 0; i < badcols.size(); ++i) ids += (i ? ", " : "") + badcols[i];
        die(io, kMalformed, "an entry says it consumes declared columns but lists none: " + ids);
    }

    ArchiveCache cache;
    for (const Json* e : ok) {
        io.out << "ok       " << dk::pad_right(str_field(*e, "id"), 16) << " " << first16(str_field(*e, "sha256")) << "…  " << entry_path(m, *e).string() << '\n';
        std::vector<std::string> names;
        try {
            names = verify_members(io, m, *e, cache);
        } catch (const std::runtime_error& exc) {
            missing.push_back(e);
            io.err << "MISSING  " << dk::pad_right(str_field(*e, "id"), 16) << " declared member not extracted: " << exc.what() << '\n';
            continue;
        }
        for (const Json* member : entry_members(*e)) {
            const char* where = truthy(*member, "extract") ? "in the cache" : "in place";
            io.out << "  member " << dk::pad_right(text_of(*member, "member"), 30) << " " << first16(text_of(*member, "sha256")) << "…  verified " << where << '\n';
        }
        if (!names.empty()) io.out << "  consumes " << text_of(*e, "consumes") << ", " << names.size() << " member(s) read individually\n";
        const Json* columns = member_of(*e, "columns");
        const Json* declared = columns != nullptr ? member_of(*columns, "declared") : nullptr;
        if (declared != nullptr && declared->truthy()) {
            const Json* excl = member_of(*columns, "excluded");
            const std::size_t n_excl = (excl != nullptr && excl->is_object()) ? excl->as_object().size() : 0;
            io.out << "  consumes " << text_of(*e, "consumes") << ", " << declared->as_array().size() << " column(s) declared, " << n_excl << " excluded by name\n";
        }
    }
    const auto in_missing = [&](const Json* e) { return std::find(missing.begin(), missing.end(), e) != missing.end(); };
    std::size_t vendored_missing = 0;
    for (const Json* e : missing) {
        if (is_vendored(*e)) ++vendored_missing;
    }
    for (const Json* e : fetch_list) {
        if (!in_missing(e)) continue;
        if (is_vendored(*e)) {
            io.err << "MISSING  " << dk::pad_right(str_field(*e, "id"), 16) << " vendored -- restore from git, do not fetch: "
                   << fs::relative(entry_path(m, *e), m.root).string() << '\n';
        } else {
            io.err << "MISSING  " << dk::pad_right(str_field(*e, "id"), 16) << " " << text_of(*e, "url") << '\n';
        }
    }
    for (const Json& e : m.entries()) {
        if (truthy(e, "provided_by_host")) {
            io.out << "host     " << dk::pad_right(str_field(e, "id"), 16) << " " << text_of(e, "version") << "  (not fetched: provided by the build host)\n";
        }
    }

    if (vendored_missing > 0 && vendored_missing == missing.size()) {
        io.err << "\n" << missing.size() << (missing.size() == 1 ? " vendored entry is" : " vendored entries are")
               << " missing their own tracked file(s). `fetch fetch` will not help -- restore from git.\n";
        return kMissing;
    }
    if (!missing.empty()) {
        io.err << "\n" << missing.size() << (missing.size() == 1 ? " entry is" : " entries are") << " not in the cache. Run: tools/bootstrap.sh\n";
        return kMissing;
    }
    return kOk;
}

int cmd_fetch(Io& io, const Manifest& m, const Args& args) {
    std::vector<std::array<std::string, 3>> results;
    for (const Json* e : fetchable(m)) {
        const fs::path p = entry_path(m, *e);
        const std::string& id = str_field(*e, "id");
        const std::string want = lower(str_field(*e, "sha256"));
        if (is_vendored(*e)) {
            // NEVER re-fetched, NOT EVEN under --refresh: the whole reason an entry is vendored is that a live re-fetch cannot reproduce its own
            // pinned bytes (vendored_dir's own account).  The tracked file IS the source of truth from here on.
            if (!fs::exists(p)) {
                die(io, kMissing,
                    id + " is vendored but its tracked file is missing: " + p.string() + "\n"
                    "  A vendored entry is never fetched. Restore the file from git (git checkout -- " + fs::relative(p, m.root).string() +
                        ") rather than re-fetching it.");
            }
            const std::string got = sha256_file(io, p);
            if (got != want) mismatch(io, *e, got, p.string() + " (vendored -- not re-fetched, the tracked copy itself has changed)");
            io.out << "vendored " << dk::pad_right(id, 16) << " " << first16(got) << "…  (tracked in the repository, never fetched)\n";
            results.push_back({id, truthy(*e, "url") ? text_of(*e, "url") : std::string("(vendored)"), got});
            continue;
        }
        if (fs::exists(p) && !args.refresh) {
            const std::string got = sha256_file(io, p);
            if (got != want) mismatch(io, *e, got, p.string());
            io.out << "cached   " << dk::pad_right(id, 16) << " " << first16(str_field(*e, "sha256")) << "…\n";
            for (const auto& [member, how] : extract_members(io, m, *e)) {
                io.out << "  member " << dk::pad_right(member, 30) << " " << (how == "cached" ? how : first16(how) + "…") << '\n';
            }
            results.push_back({id, text_of(*e, "url"), got});
            continue;
        }

        io.out << "fetching " << dk::pad_right(id, 16) << " " << text_of(*e, "url") << '\n';
        const fs::path part = p.string() + ".part";
        const std::string got = download(io, text_of(*e, "url"), p, *e);
        std::error_code ignored;
        if (got != want) {
            if (args.refresh && fs::exists(p)) {
                fs::remove(part, ignored);
                io.err << "\nUPSTREAM DRIFT: " << id << " now hashes " << got << ", manifest says " << str_field(*e, "sha256") << ".\n"
                       << "The cached copy is untouched.  This is the condition R11 exists to detect.\n";
                return kMismatch;
            }
            fs::remove(part, ignored);
            mismatch(io, *e, got, text_of(*e, "url") + " (download discarded)");
        }
        fs::rename(part, p);
        io.out << "ok       " << dk::pad_right(id, 16) << " " << first16(got) << "…\n";
        for (const auto& [member, how] : extract_members(io, m, *e)) {
            io.out << "  member " << dk::pad_right(member, 30) << " " << (how == "cached" ? how : first16(how) + "…") << '\n';
        }
        results.push_back({id, text_of(*e, "url"), got});
    }
    write_receipt(io, m, results);
    return kOk;
}

int cmd_list(Io& io, const Manifest& m, const Args& args) {
    if (args.json) {
        io.out << m.doc.dumps(2) << '\n';
        return kOk;
    }
    for (const Json& e : m.entries()) {
        const std::string src = truthy(e, "provided_by_host") ? "host" : text_of(e, "url");
        // A literature entry carries `terms`, never `licence` (the Python tool died on it with a KeyError): its column says so.
        const std::string licence = member_of(e, "licence") != nullptr ? text_of(e, "licence") : "(see terms)";
        io.out << dk::pad_right(str_field(e, "id"), 16) << " " << dk::pad_right(str_field(e, "kind"), 6) << " " << dk::pad_right(licence, 16) << " "
               << dk::pad_right(text_of(e, "version"), 10) << " " << src << '\n';
    }
    return kOk;
}

int cmd_path(Io& io, const Manifest& m, const Args& args) {
    for (const Json& e : m.entries()) {
        if (str_field(e, "id") != args.id) continue;
        if (truthy(e, "provided_by_host")) die(io, kUsage, args.id + " is provided by the build host and has no cache path");
        if (args.have_member) {
            for (const Json* member : entry_members(e)) {
                if (!truthy(*member, "extract")) continue;
                const std::string name = text_of(*member, "member");
                if (fs::path(name).filename().string() == args.member || name == args.member) {
                    io.out << extract_path(m, e, *member).string() << '\n';
                    return kOk;
                }
            }
            die(io, kUsage, args.id + " declares no extracted member " + dk::py_repr(args.member));
        }
        io.out << entry_path(m, e).string() << '\n';
        return kOk;
    }
    die(io, kUsage, "no manifest entry with id " + dk::py_repr(args.id));
}

// Is the tree FetchContent populated actually the archive we pinned?
//
// FetchContent honours the FIRST declaration of a name and silently ignores later ones.  That is how a top-level project pins a transitive
// dependency, and equally how a transitive dependency captures one of ours: tl::expected's own CMakeLists declares Catch2 v2.13.10 by URL with
// no hash, and before the declaration order was fixed the build fetched that over the network while reporting the pinned 3.16.0 from the cache.
//
// So this compares a file inside the populated tree against the same file inside the archive whose SHA-256 the manifest pins.  Nothing is
// inferred from a version string: the comparison is against the bytes.
int cmd_verify_populated(Io& io, const Manifest& m, const Args& args) {
    for (const Json& e : m.entries()) {
        if (str_field(e, "id") != args.id) continue;
        if (!truthy(e, "verify_path")) {
            io.out << "ok       " << dk::pad_right(args.id, 16) << " populated (no verify_path declared)\n";
            return kOk;
        }
        const std::string rel = text_of(e, "verify_path");
        const std::string inner = truthy(e, "unpacked_root") ? text_of(e, "unpacked_root") + "/" + rel : rel;
        const fs::path archive = entry_path(m, e);
        dk::Bytes pinned;
        try {
            const dk::Bytes raw = dk::read_bytes(archive);
            const dk::Bytes tar = dk::gunzip(dk::ByteView{raw.data(), raw.size()});
            auto got = dk::tar_member(dk::ByteView{tar.data(), tar.size()}, inner);
            if (!got) die(io, kMalformed, args.id + ": " + dk::py_repr(inner) + " is not in " + archive.string());
            pinned = std::move(*got);
        } catch (const std::runtime_error& exc) {
            die(io, kMalformed, args.id + ": cannot read " + dk::py_repr(inner) + " from " + archive.string() + ": " + exc.what());
        }
        const fs::path landed_path = fs::path(args.populated) / rel;
        if (!fs::exists(landed_path)) die(io, kMismatch, args.id + ": populated tree has no " + rel + " at " + landed_path.string());
        const dk::Bytes landed = dk::read_bytes(landed_path);
        if (landed != pinned) {
            die(io, kMismatch,
                "PINNED DEPENDENCY SUBSTITUTED — refusing.\n"
                "  entry      " + args.id + "\n"
                "  pinned     " + archive.string() + " (sha256 " + str_field(e, "sha256") + ")\n"
                "  populated  " + landed_path.string() + "\n"
                "  compared   " + rel + ": " + std::to_string(pinned.size()) + " bytes pinned, " + std::to_string(landed.size()) + " bytes populated\n"
                "\n"
                "  Something other than this tree's declaration populated the content.\n"
                "  FetchContent honours the first declaration of a name, so the usual cause\n"
                "  is a transitive dependency's own FetchContent_Declare getting in first —\n"
                "  typically by URL, typically with no hash. Declare this tree's pins before\n"
                "  anything is made available (odl_declare_all_code) and find the competitor.");
        }
        io.out << "ok       " << dk::pad_right(args.id, 16) << " populated tree matches the pinned archive (" << rel << ")\n";
        return kOk;
    }
    die(io, kUsage, "no manifest entry with id " + dk::py_repr(args.id));
}

// Plan §5 constraint 3, as an ALLOWLIST.
//
// The first version of this check was a denylist: refuse anything whose name contained GPL, LGPL or AGPL.  That reports success without checking
// the thing.  CALCEPH is triple-licensed CeCILL-C / CeCILL-B / CeCILL v2.1, and of those CeCILL-C is close to the LGPL and CeCILL v2.1 is
// GPL-compatible — yet neither string contains "GPL", so both would have passed.  So would MPL, EPL, CDDL, SSPL, OSL and EUPL.
//
// A denylist of forbidden licences can never be complete; an allowlist of permitted ones can.  Adding an entry here is a deliberate act with a
// reason attached, which is the point.
struct Permitted {
    const char* id;
    const char* reason;
};
const Permitted kPermissive[] = {
    {"0BSD", "public-domain-equivalent"},
    {"APACHE-2.0", "permissive, with an express patent grant"},
    {"BSD-2-CLAUSE", "permissive"},
    {"BSD-3-CLAUSE", "permissive"},
    {"BSL-1.0", "Boost; permissive, and exempts object code from the notice requirement"},
    {"CC0-1.0", "public-domain dedication"},
    {"CECILL-B", "French BSD-like. NOT CeCILL-C (close to LGPL) and NOT CeCILL v2.1 (GPL-compatible). Carries a citation obligation — see the entry's note."},
    {"CURL", "the curl licence (SPDX: curl): an MIT/X derivative, permissive; 'Permission to use, copy, modify, and distribute this software for any "
             "purpose with or without fee is hereby granted'. A build-host TOOL, never fetched, vendored or linked (the manifest's `curl` entry)."},
    {"ISC", "permissive"},
    {"MIT", "permissive"},
    {"MIT-0", "permissive, no attribution"},
    {"NCSA", "permissive"},
    {"PSF-2.0", "Python Software Foundation; permissive"},
    {"UNLICENSE", "public-domain dedication"},
    {"ZLIB", "permissive"},
    {"IERS-PUBLIC", "not an SPDX identifier: IERS public data products, published for unrestricted use. Data, never linked."},
    {"NASA-PUBLIC", "not an SPDX identifier: NASA/JPL published data products (NAIF generic kernels, JPL SSD test sets). Data, never linked."},
    {"CC-BY-4.0", "Creative Commons Attribution 4.0; permissive with attribution. NOT CC-BY-NC-4.0 or CC-BY-SA-4.0, which are different licences "
                  "that differ from it by one token in the identifier."},
    {"OGL-CANADA-2.0", "not an SPDX identifier: Open Government Licence - Canada 2.0, the default for Government of Canada data. Attribution "
                       "only. Data, never linked."},
    {"IAU-PUBLIC", "not an SPDX identifier: International Astronomical Union resolutions, published standards documents adopted by General "
                   "Assembly vote. Text, never linked."},
    {"NRL-PUBLIC", "not an SPDX identifier: US Naval Research Laboratory work released to the public domain and distributed without restriction. "
                   "NRLMSISE-00 is compiled into the TEST SUITE only (ATMO-R-027), never the library."},
    {"NGA-PUBLIC", "not an SPDX identifier: NGA published geospatial standards and models (EGM2008, WGS 84). Unrestricted use; the EGM2008 README "
                   "asks for a citation, which PROVENANCE.md §4 carries. Data, never linked."},
    {"SPACETRACK-PUBLIC", "not an SPDX identifier: USSPACECOM's own express blanket approval for transfer/redistribution of 'basic SSA data' "
                          "(Two-Line Elements, Orbital Mean-element Messages, SATCAT, decay/reentry data), conditioned on citation "
                          "(space-track.org/documentation, 'Redistribution of Basic SSA Information', fetched and quoted directly 2026-09-25). A "
                          "REAL, explicit, stated approval -- unlike FACTUAL-DATA-CITED below, this needs no search_recorded pointer."},
    {"VALLADO-UNRESTRICTED", "not an SPDX identifier: the primary distribution's own stated grant for AIAA 2006-6753's SGP4 code and test data "
                             "(celestrak.org, .../faq.php, quoted from raw bytes in the entry's own licence_note) -- 'no license associated with "
                             "the code and you may use it for any purpose...as you wish', attribution requested only. A REAL, explicit, stated "
                             "approval, the same shape as SPACETRACK-PUBLIC above -- needs no search_recorded pointer either."},
    {"ILRS-PUBLIC", "not an SPDX identifier: International Laser Ranging Service data and products, on the service's own statement "
                    "(ilrs.gsfc.nasa.gov/about/cite.html, quoted verbatim in each entry's licence_note): 'The data and products are not "
                    "copyrighted; however, in the event that you publish data or results using these data, we request that you include the "
                    "following citation'. A statement of no copyright with a citation REQUEST -- neither a grant nor a restriction, and a REAL, "
                    "explicit, stated position, so (like SPACETRACK-PUBLIC) it needs no search_recorded pointer. The tree redistributes none of "
                    "it: pin-only. The citations are carried in NOTICE through the entries' licence_note. Data, never linked."},
    {"IGS-PUBLIC", "not an SPDX identifier: International GNSS Service data and products, on the service's own Data and Product Disclaimer and "
                   "Terms of Use (5 August 2020, quoted in each entry's licence_note): 'IGS data and products have been made openly available for "
                   "use without restriction, and continue to be offered free of cost or obligation', with an ATTRIBUTION term (users agree to "
                   "appropriately cite and attribute these resources to providers and their sponsors) and a no-warranty disclaimer. A REAL, "
                   "explicit, stated position -- a grant of unrestricted use with attribution -- so, like ILRS-PUBLIC, it needs no "
                   "search_recorded pointer. The tree redistributes none of it: pin-only. Data, never linked."},
    {"NIST-PUBLIC", "not an SPDX identifier: NIST web publications and datasets, on the site's own statement (nist.gov/copyrights-disclaimers, "
                    "quoted in each entry's licence_note): 'With the exception of material marked as copyrighted, information presented on NIST "
                    "sites are considered public information and may be distributed or copied'. The StRD datasets (Standard Reference Database "
                    "140) are unmarked; the Standard Reference Data Act (15 U.S.C. 290e) lets NIST secure copyright in SRD and that power is on "
                    "record and unexercised on these pages -- stated in the entries, not hidden. The tree redistributes none of it (no NIST file "
                    "is copied into the tree): pin-only. Data, never linked."},
    {"FACTUAL-DATA-CITED", "not an SPDX identifier: computed or measured factual data (e.g. a position, a table of positions) from a source that "
                           "states no redistribution terms of its own, where a search for terms was performed and recorded (RS14's own reasoning, "
                           "SPEC-spacecraft.md §2.2 — a fact is not an expression, and stating where a number came from is not the same claim as "
                           "redistributing a copyrighted work). Every entry using this basis MUST ALSO carry `search_recorded`, below — this check "
                           "enforces that, so the basis itself cannot become a way round the gate."},
};

bool permitted(const std::string& licence) {
    const std::string key = upper_stripped(licence);
    return std::any_of(std::begin(kPermissive), std::end(kPermissive), [&](const Permitted& p) { return key == p.id; });
}

// An entry claiming FACTUAL-DATA-CITED must name WHERE its own search for terms is written up (a PROVENANCE.md section, typically) — the same role
// `terms` plays for a `literature` entry, but as a POINTER rather than the full prose, since this basis's own entries are real build inputs
// (cached, read by a test) and the full search account belongs in PROVENANCE.md, not doubled into the manifest.  Checked here, not merely
// documented, for the identical reason the `literature`/`terms` check exists: a basis nobody has to earn is not a basis, it is a bypass with a
// label on it.
constexpr const char* kSearchRecordedField = "search_recorded";

// Plan §5 constraint 3: only known-permissive licences, by allowlist.
//
// THE GATE'S SCOPE IS WHAT THIS TREE REDISTRIBUTES, which is code and data, not what it READS.  A `literature` entry is a provenance record
// rather than a dependency: it is pinned by hash so that "this was derived from that" is checkable by a future reader who fetches the same hash,
// and nothing derived from it is a copy of it.  `dop853.f` is not the same case and stays dropped — it was code to be incorporated, and
// incorporation is what this gate exists for.
//
// THE EXEMPTION IS EARNED BY CHECKED PROPERTIES AND NEVER BY THE LABEL.  A literature entry must say where its terms were sought (`terms`),
// must live outside the build cache (entry_path, above), and must be unreachable from any build input — which tools/literaturecheck proves by
// injection.
int cmd_check_licences(Io& io, const Manifest& m) {
    std::vector<const Json*> bad;
    std::vector<const Json*> lit;
    for (const Json& e : m.entries()) {
        if (is_literature(e)) {
            lit.push_back(&e);
            if (!truthy(e, "terms")) {
                io.err << "LITERATURE ENTRY WITHOUT A TERMS RECORD  " << str_field(e, "id") << "\n"
                       << "  The exemption rests on this tree not redistributing, NOT on a grant\n"
                       << "  nobody found. Record the SEARCH rather than the conclusion (rule 4):\n"
                       << "  which routes were tried for the terms and what each returned.\n";
                return kMalformed;
            }
            continue;
        }
        const std::string licence = text_of(e, "licence");
        if (!permitted(licence)) {
            bad.push_back(&e);
            continue;
        }
        if (upper_stripped(licence) == "FACTUAL-DATA-CITED" && !truthy(e, kSearchRecordedField)) bad.push_back(&e);
    }
    std::vector<std::string> ids;
    for (const Permitted& p : kPermissive) ids.emplace_back(p.id);
    std::sort(ids.begin(), ids.end());
    std::string all;
    for (std::size_t i = 0; i < ids.size(); ++i) all += (i ? ", " : "") + ids[i];
    for (const Json* e : bad) {
        const std::string licence = text_of(*e, "licence");
        if (upper_stripped(licence) == "FACTUAL-DATA-CITED" && !truthy(*e, kSearchRecordedField)) {
            io.err << "FACTUAL-DATA-CITED ENTRY WITHOUT A search_recorded POINTER  " << str_field(*e, "id") << "\n"
                   << "  This basis is earned by a recorded search, not by the label (the same rule\n"
                   << "  the `literature`/`terms` check above already enforces for a different\n"
                   << "  exemption). Add `\"" << kSearchRecordedField << "\": \"PROVENANCE.md §<section>\"` naming\n"
                   << "  exactly where the search for this entry's own terms is written up.\n";
            continue;
        }
        io.err << "LICENCE NOT ON THE PERMISSIVE LIST  " << str_field(*e, "id") << ": " << licence << "\n"
               << "  Plan §5 constraint 3 permits only licences on tools/fetch.cpp's allowlist.\n"
               << "  This is an allowlist and not a denylist on purpose: a denylist of forbidden\n"
               << "  licences can never be complete, and CeCILL v2.1 — GPL-compatible copyleft —\n"
               << "  contains no forbidden substring and would pass one.\n"
               << "  If this licence is genuinely permissive, add it to the allowlist with a\n"
               << "  one-line reason. That is meant to be a deliberate act.\n"
               << "  Permitted today: " << all << "\n";
    }
    if (!bad.empty()) return kMalformed;
    io.out << "ok       " << (m.entries().size() - lit.size()) << " entries, every licence on the permissive allowlist\n";
    if (!lit.empty()) {
        io.out << "         " << lit.size() << " literature entr" << (lit.size() == 1 ? "y" : "ies")
               << ", exempt by plan §5 constraint 3 and each carrying a terms record:\n";
        for (const Json* e : lit) {
            // Literature entries carry `terms`, never `licence`.  The two states this project's own entries distinguish are "the search found
            // nothing" (recorded terms text starting "NOT ESTABLISHED", the convention set by li-ziebart-2019-shadow and
            // rodriguez-solano-2014-dissertation) and "the search found something" — summarised here, not paraphrased, since prose long enough
            // to need paraphrasing belongs in the terms field itself, not compressed into a status line.
            const std::string terms = upper_stripped(text_of(*e, "terms"));
            const char* summary = terms.rfind("NOT ESTABLISHED", 0) == 0 ? "(none established)" : "(found — see its terms field)";
            io.out << "           " << dk::pad_right(str_field(*e, "id"), 24) << " " << summary << '\n';
        }
    }
    return kOk;
}

// ------------------------------------------------------------------------------------------------------------------------ argument parsing

const char kUsageText[] =
    "usage: fetch [-h] [--manifest MANIFEST] [--root ROOT]\n"
    "             {verify,fetch,list,path,check-licences,verify-populated} ...\n";

const char kHelpText[] =
    "\n"
    "the manifest fetcher: every external input is declared with URL, SHA-256 and a licence, fetched\n"
    "from origin into a cache, and checked there.\n"
    "\n"
    "commands:\n"
    "  verify              offline: is every entry cached and hash-correct?\n"
    "  fetch [--refresh]   download what is missing (--refresh: re-download everything and report upstream drift)\n"
    "  list [--json]       show the entries\n"
    "  path <id> [--member NAME]   print the cache path of one entry (or of an extracted archive member)\n"
    "  check-licences      plan §5 constraint 3: only licences on the permissive allowlist\n"
    "  verify-populated <id> <dir>   is a populated FetchContent tree the archive we pinned?\n"
    "\n"
    "options:\n"
    "  --manifest MANIFEST   manifest path (default: <tree>/manifest/manifest.json)\n"
    "  --root ROOT           tree root the cache is relative to\n"
    "\n"
    "exit codes: 0 ok   1 missing from cache   2 HASH MISMATCH   3 manifest malformed   4 network failure   5 usage error\n";

}  // namespace

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

int run(const std::vector<std::string>& argv, Io io) {
    try {
        fs::path root = default_root();
        fs::path manifest_path;
        bool have_manifest = false;
        std::size_t i = 0;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << "fetch: error: " << what << '\n';
            return kArgumentError;
        };
        // the global options come first, as argparse's parent parser has them
        while (i < argv.size() && argv[i].rfind("--", 0) == 0 && argv[i] != "--help") {
            std::string opt = argv[i];
            std::string value;
            const std::size_t eq = opt.find('=');
            if (eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
            } else if (opt == "--manifest" || opt == "--root") {
                if (i + 1 >= argv.size()) return usage_error("argument " + opt + ": expected one argument");
                value = argv[++i];
            }
            if (opt == "--manifest") {
                manifest_path = value;
                have_manifest = true;
            } else if (opt == "--root") {
                root = value;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
            ++i;
        }
        if (i < argv.size() && (argv[i] == "-h" || argv[i] == "--help")) {
            io.out << kUsageText << kHelpText;
            return kOk;
        }
        if (i >= argv.size()) return usage_error("the following arguments are required: cmd");
        const std::string cmd = argv[i++];
        Args args;
        std::vector<std::string> positional;
        for (; i < argv.size(); ++i) {
            const std::string& a = argv[i];
            if (a == "--refresh" && cmd == "fetch") {
                args.refresh = true;
            } else if (a == "--json" && cmd == "list") {
                args.json = true;
            } else if (a == "--member" && cmd == "path") {
                if (i + 1 >= argv.size()) return usage_error("argument --member: expected one argument");
                args.member = argv[++i];
                args.have_member = true;
            } else if (a.rfind("--", 0) == 0) {
                return usage_error("unrecognized arguments: " + a);
            } else {
                positional.push_back(a);
            }
        }
        const bool takes_id = cmd == "path" || cmd == "verify-populated";
        if (cmd != "verify" && cmd != "fetch" && cmd != "list" && cmd != "path" && cmd != "check-licences" && cmd != "verify-populated") {
            return usage_error("argument cmd: invalid choice: " + dk::py_repr(cmd) +
                               " (choose from 'verify', 'fetch', 'list', 'path', 'check-licences', 'verify-populated')");
        }
        if (takes_id) {
            if (positional.empty()) return usage_error("the following arguments are required: id");
            args.id = positional[0];
            if (cmd == "verify-populated") {
                if (positional.size() < 2) return usage_error("the following arguments are required: populated");
                args.populated = positional[1];
            }
        }
        const std::size_t wanted = cmd == "verify-populated" ? 2 : takes_id ? 1 : 0;
        if (positional.size() > wanted) return usage_error("unrecognized arguments: " + positional[wanted]);

        std::error_code ec;
        root = fs::weakly_canonical(root, ec);
        if (!have_manifest) manifest_path = root / "manifest" / "manifest.json";
        Manifest m;
        m.root = root;
        m.doc = load_manifest(io, manifest_path);
        if (cmd == "verify") return cmd_verify(io, m);
        if (cmd == "fetch") return cmd_fetch(io, m, args);
        if (cmd == "list") return cmd_list(io, m, args);
        if (cmd == "path") return cmd_path(io, m, args);
        if (cmd == "check-licences") return cmd_check_licences(io, m);
        return cmd_verify_populated(io, m, args);
    } catch (const dk::Exit& x) {
        return x.code;
    }
}

}  // namespace odl::tools::fetch

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::fetch::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
