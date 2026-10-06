// tests/devtools/notice_tests.cpp — NOTICE, generated from the manifest (ctest `notice.behaviour`; plan L0 step 8, group C2).
//
// tools/notice.py had no test of its own: ci.sh's gate 6 (`notice.py --check`) was its only check, and it exercised one manifest, the real one.  Here the
// generator's text is held on synthetic manifests whose expected NOTICE is DERIVED by hand from the generator's source, line by line (not typed from its
// output); the licence texts quoted out of tar.gz and zip archives, with the Latin-1 byte, the form feed and the trailing blanks that make them hard; the
// two stated substitutions (the EXCLUDED paragraph of a derivative, the sentence about permissive licences when a release blocker exists); the tool's
// four outcomes and every refusal; and, last, the real tree: the committed NOTICE is what the tool renders from the real manifest and the real cache.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/inflate.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "archive_builders.hpp"
#include "notice.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
using namespace odl::devkit::testing;
namespace nt = odl::tools::notice;

namespace {

Json object(std::initializer_list<std::pair<const char*, Json>> members) {
    Json::Object o;
    for (const auto& m : members) o.emplace_back(m.first, m.second);
    return Json(std::move(o));
}

Json with(Json e, const std::string& key, Json value) {
    e.set(key, std::move(value));
    return e;
}

Json list(std::initializer_list<const char*> items) {
    Json::Array a;
    for (const char* s : items) a.emplace_back(s);
    return Json(std::move(a));
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

std::string spaces(std::size_t n) { return std::string(n, ' '); }

const std::string kSha = std::string(64, 'a');

Json code_entry() {
    return object({{"id", Json("lib")}, {"kind", Json("code")}, {"version", Json("1.0")}, {"role", Json("a library")}, {"licence", Json("MIT")},
                   {"url", Json("https://example.invalid/lib.tar.gz")}, {"filename", Json("lib.tar.gz")}, {"sha256", Json(kSha)}});
}

Json data_entry() {
    return object({{"id", Json("table")}, {"kind", Json("data")}, {"version", Json("2026")}, {"role", Json("a table")}, {"licence", Json("CC-BY-4.0")},
                   {"url", Json("https://example.invalid/table.txt")}, {"filename", Json("table.txt")}, {"sha256", Json(kSha)},
                   {"retrieved", Json("2026-01-01")}, {"licence_note", Json("Attribution: Somebody.")}});
}

Json tool_entry() {
    return object({{"id", Json("tool")}, {"kind", Json("tool")}, {"version", Json(">=1")}, {"role", Json("a host tool")}, {"licence", Json("BSD-3-Clause")},
                   {"provided_by_host", Json(true)}});
}

Json paper_entry() {
    return object({{"id", Json("paper")}, {"kind", Json("literature")}, {"terms", Json("NOT ESTABLISHED: searched, found nothing")}, {"url", Json("https://example.invalid/p.pdf")},
                   {"filename", Json("p.pdf")}, {"sha256", Json(kSha)}});
}

Json manifest(const std::vector<Json>& entries) {
    return object({{"schema", Json(std::int64_t{1})}, {"cache", Json("data/cache")}, {"entries", Json(Json::Array(entries.begin(), entries.end()))}});
}

struct Tree {
    TempDir dir;
    const fs::path& root() const { return dir.path(); }
    void write_manifest(const Json& doc) const {
        fs::create_directories(root() / "manifest");
        write_text(root() / "manifest" / "manifest.json", doc.dumps(2, false));
    }
    void put_archive(const std::string& id, const std::string& filename, const Bytes& bytes) const {
        fs::create_directories(root() / "data" / "cache" / id);
        write_bytes(root() / "data" / "cache" / id / filename, view(bytes));
    }
};

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run(const fs::path& root, std::vector<std::string> args) {
    std::ostringstream out;
    std::ostringstream err;
    std::vector<std::string> argv = {"--root", root.string()};
    argv.insert(argv.end(), args.begin(), args.end());
    const int code = nt::run(argv, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_plain(std::vector<std::string> args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = nt::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

std::string lines_of(std::initializer_list<std::string> lines) {
    std::string out;
    for (const std::string& l : lines) out += l + "\n";
    return out;
}

// the generator's own header, as the C++ tool states it (the three D1 token substitutions are in it)
std::string header_text() {
    return lines_of({"NOTICE — third-party components", std::string(78, '='), "",
                     "GENERATED FILE.  Do not edit.  Produced by tools/notice.cpp from",
                     "manifest/manifest.json, which is the single declaration of every external",
                     "input to this tree.  To change anything here, change the manifest and run",
                     "    cmake --build <build-dir> --target regenerate_notice",
                     "CI runs `notice --check` and fails if this file has drifted.", "",
                     "This file does not describe the licence of THIS tree, which grants nothing",
                     "for now and is stated in LICENSE.  It describes what this tree depends on.", ""});
}

const std::string kRuleLine = std::string(78, '-');

}  // namespace

TEST_CASE("the text of a small manifest, line by line from the generator's source", "[notice]") {
    TempDir td;
    const Json doc = manifest({code_entry(), data_entry(), tool_entry(), paper_entry()});
    const std::string got = nt::render(td.path(), doc);

    const std::string expected =
        header_text() + lines_of({
            kRuleLine, "SUMMARY", kRuleLine, "",
            // "  " + id:<18 + " " + version:<12 + " " + licence:<16 + " " + kind
            "  component" + spaces(10) + "version" + spaces(6) + "licence" + spaces(10) + "kind",
            "  lib" + spaces(16) + "1.0" + spaces(10) + "MIT" + spaces(14) + "code",
            "  table" + spaces(14) + "2026" + spaces(9) + "CC-BY-4.0" + spaces(8) + "data",
            "  tool" + spaces(15) + ">=1" + spaces(10) + "BSD-3-Clause" + spaces(5) + "tool",   // the literature entry is not in the table
            "",
            "  Every licence above is permissive.  Plan §5 constraint 3 forbids GPL,",
            "  LGPL and AGPL anywhere in what could ship; tools/fetch.cpp check-licences",
            "  enforces it and CI runs it.", "",
            kRuleLine, "COMPONENTS COMPILED INTO OR LINKED WITH THIS TREE", kRuleLine, "",
            "  Fetched from origin, pinned by SHA-256, and verified twice — once by",
            "  tools/fetch.cpp and once by CMake's URL_HASH.", "",
            "  lib  1.0", "    role      a library", "    licence   MIT", "    url       https://example.invalid/lib.tar.gz", "    sha256    " + kSha, "",
            kRuleLine, "DATA INPUTS", kRuleLine, "",
            "  External data, pinned by hash.  The identity of an input is its hash,",
            "  not its URL: at least one upstream here revises its published series",
            "  retroactively at an unchanged address.", "",
            "  table  2026", "    role      a table", "    licence   CC-BY-4.0", "    url       https://example.invalid/table.txt", "    sha256    " + kSha,
            "    retrieved 2026-01-01", "    note      Attribution: Somebody.", "",
            kRuleLine, "BUILD-TIME TOOLS", kRuleLine, "",
            "  Present on the build host.  Not vendored, not linked, shipped in nothing.",
            "  Listed because an external input is an external input.", "",
            "  tool  >=1", "    role      a host tool", "    licence   BSD-3-Clause", "    source    provided by the build host; not fetched"});
    CHECK(got == expected);

    // a group with no entry says so; the role defaults to an em dash; an archived_url that differs from the url is shown, one that does not is not
    const Json bare = with(with(code_entry(), "archived_url", Json("https://example.invalid/archive/lib.tar.gz")), "role", Json(""));
    const std::string only_code = nt::render(td.path(), manifest({bare}));
    CHECK(contains(only_code, "DATA INPUTS\n" + kRuleLine + "\n\n  None at this revision.\n\n"));
    CHECK(contains(only_code, "BUILD-TIME TOOLS\n" + kRuleLine + "\n\n  None at this revision.\n"));
    CHECK(contains(only_code, "    archived  https://example.invalid/archive/lib.tar.gz\n"));
    CHECK(contains(only_code, "    role      \n"));    // an empty role stays empty
    Json no_role = code_entry();
    Json::Object members;
    for (const auto& m : no_role.as_object()) {
        if (m.first != "role") members.push_back(m);
    }
    CHECK(contains(nt::render(td.path(), manifest({Json(members)})), "    role      —\n"));
    CHECK_FALSE(contains(nt::render(td.path(), manifest({with(code_entry(), "archived_url", Json("https://example.invalid/lib.tar.gz"))})), "archived"));
    CHECK(got.back() == '\n');
    CHECK(got.substr(got.size() - 2) != "\n\n");

    // an entry of a kind that has no group is in the SUMMARY and nowhere else (as the generator had it), and it still needs a licence
    const std::string odd = nt::render(td.path(), manifest({with(code_entry(), "kind", Json("other"))}));
    CHECK(contains(odd, "  lib" + spaces(16) + "1.0" + spaces(10) + "MIT" + spaces(14) + "other\n"));
    CHECK_FALSE(contains(odd, "  lib  1.0\n"));
    CHECK(contains(odd, "COMPONENTS COMPILED INTO OR LINKED WITH THIS TREE\n" + kRuleLine + "\n\n  None at this revision.\n"));
    Json no_licence = with(code_entry(), "kind", Json("other"));
    Json::Object kept;
    for (const auto& m : no_licence.as_object()) {
        if (m.first != "licence") kept.push_back(m);
    }
    CHECK_THROWS_AS(nt::render(td.path(), manifest({Json(kept)})), std::runtime_error);
}

TEST_CASE("a licence note wraps at 78 columns, with the python wrapper's hyphen and long-word rules", "[notice]") {
    TempDir td;
    const std::string note = "The CC BY 4.0 licence applies to everything in the file except the sunspot-number column, which is CC BY-NC 4.0, and that column is "
                             "described at https://example.invalid/an/extremely-long/address/that/cannot-possibly-fit/on-one-line/of-the-notice.txt . Done.";
    const std::string got = nt::render(td.path(), manifest({with(data_entry(), "licence_note", Json(note))}));
    const std::string indent = spaces(14);
    // Derived by hand from _wrap_chunks, 64 columns a line after the 14-column indent: the chunks split after a hyphen between letters ("sunspot-",
    // "BY-", "extremely-", "cannot-", ...), and a line takes a chunk only if the whole of it fits.
    CHECK(contains(got, "    note      The CC BY 4.0 licence applies to everything in the file except\n" +
                            indent + "the sunspot-number column, which is CC BY-NC 4.0, and that\n" +
                            indent + "column is described at https://example.invalid/an/extremely-\n" +
                            indent + "long/address/that/cannot-possibly-fit/on-one-line/of-the-\n" +
                            indent + "notice.txt . Done.\n"));
    for (const std::string& line : splitlines_py(got)) {
        INFO("line: " << line);
        CHECK(code_points(line) <= 78);
    }
    // empty and blank-only notes print nothing, and a note that is not a string is refused
    CHECK_FALSE(contains(nt::render(td.path(), manifest({with(data_entry(), "licence_note", Json(""))})), "note      "));
    CHECK_FALSE(contains(nt::render(td.path(), manifest({with(data_entry(), "licence_note", Json("   \t "))})), "note      "));
    CHECK_THROWS_AS(nt::render(td.path(), manifest({with(data_entry(), "licence_note", Json(std::int64_t{7}))})), std::runtime_error);
}

TEST_CASE("the licence texts are quoted out of the pinned archives, verbatim and tidied as Python tidied them", "[notice]") {
    Tree t;
    // a tar.gz whose licence file has CRLF line ends, a blank line, trailing blanks, a tab, a form feed, a Latin-1 byte, and blank lines at the end
    const std::string text = std::string("Line one  \r\n\r\n\tIndented\x0c" "Formfeed split  \ncaf\xE9 licence\n\n  \n");
    t.put_archive("lib", "lib.tar.gz", gzip_stored(view(concat({tar_entry("lib-1.0/LICENSE", text), tar_entry("lib-1.0/OTHER", "not this one")}))));
    // a zip with the member in a directory
    t.put_archive("zipped", "z.zip", make_zip({stored_file("docs/COPYING", "Zip licence.\n")}));
    // a plain .tgz name, found without `unpack`
    t.put_archive("tgz", "x.tgz", gzip_stored(view(concat({tar_entry("LICENSE.txt", "Tgz licence.\n")}))));
    const Json lib = with(with(with(code_entry(), "licence_file", Json("LICENSE")), "unpacked_root", Json("lib-1.0")), "unpack", Json("tar.gz"));
    const Json zipped = with(with(with(with(code_entry(), "id", Json("zipped")), "filename", Json("z.zip")), "licence_file", Json("docs/COPYING")), "licence", Json("ISC"));
    const Json tgz = with(with(with(with(code_entry(), "id", Json("tgz")), "filename", Json("x.tgz")), "licence_file", Json("LICENSE.txt")), "licence", Json("BSD-2-Clause"));
    t.write_manifest(manifest({lib, zipped, tgz}));
    const std::string got = nt::render(t.root(), manifest({lib, zipped, tgz}));

    const auto title = [](const std::string& id, const std::string& licence, const std::string& file) {
        std::string s = "--- " + id + " 1.0 — " + licence + " (" + file + ") ";
        while (code_points(s) < 78) s += '-';
        return s;
    };
    const std::string quoted_header = kRuleLine + "\nLICENCE TEXTS, QUOTED VERBATIM FROM THE PINNED ARCHIVES\n" + kRuleLine + "\n\n";
    // Derived from the generator: text.rstrip() first -- the text's end "licence\n\n  \n" is removed, so no blank line from the end of the text survives --
    // then splitlines(): "Line one  " | "" | "\tIndented" | "Formfeed split  " | "caf<U+FFFD> licence" (CRLF is one boundary, the form feed is one, the
    // Latin-1 byte is a replacement character), each as "  " + line.rstrip(): so a blank line of the text is the two blanks of the indent alone, a leading
    // tab stays, trailing blanks go; then one blank line.
    const std::string lib_block = title("lib", "MIT", "LICENSE") + "\n\n  Line one\n  \n  \tIndented\n  Formfeed split\n  caf\xEF\xBF\xBD licence\n\n";
    CHECK(contains(got, quoted_header + lib_block));
    CHECK(contains(got, title("zipped", "ISC", "docs/COPYING") + "\n\n  Zip licence.\n\n"));
    CHECK(contains(got, title("tgz", "BSD-2-Clause", "LICENSE.txt") + "\n\n  Tgz licence.\n"));
    const std::string last_line = "  Tgz licence.\n";
    CHECK(got.substr(got.size() - last_line.size()) == last_line);   // and the whole text ends after the last licence line, with one line feed

    // no block when there is nothing to read: no licence_file, a host tool, no archive in the cache, an absent member, a damaged archive, an empty file
    t.put_archive("damaged", "d.tar.gz", Bytes{'n', 'o', 't', ' ', 'g', 'z', 'i', 'p'});
    t.put_archive("empty", "e.tar.gz", gzip_stored(view(concat({tar_entry("LICENSE", "")}))));
    t.put_archive("nomember", "n.zip", make_zip({stored_file("other", "x")}));
    const auto of = [&](const std::string& id, const std::string& filename, const std::string& file) {
        return with(with(with(code_entry(), "id", Json(id)), "filename", Json(filename)), "licence_file", Json(file));
    };
    const std::string none = nt::render(t.root(), manifest({of("damaged", "d.tar.gz", "LICENSE"), of("empty", "e.tar.gz", "LICENSE"), of("nomember", "n.zip", "LICENSE"),
                                                            of("missing", "m.tar.gz", "LICENSE"), with(tool_entry(), "licence_file", Json("LICENSE")), code_entry()}));
    CHECK_FALSE(contains(none, "LICENCE TEXTS"));
}

TEST_CASE("the EXCLUDED paragraph says what is true of the entry, and the summary names a release blocker", "[notice]") {
    TempDir td;
    const Json excluded = object({{"CC-BY-NC-4.0", list({"SN", "days"})}});
    const Json columns = object({{"declared", list({"YYYY", "MM", "DD", "Kp1"})}, {"excluded", object({{"SN", Json("not needed")}})}});
    const Json plain = with(with(with(data_entry(), "licence_excluded", excluded), "columns", columns), "consumes", Json("declared-columns"));
    const std::string indent = spaces(14);

    // an entry that is NOT a derivative keeps the generator's sentence, with only the Python fetcher's name replaced
    const std::string old_form = nt::render(td.path(), manifest({plain}));
    // (the same six lines as the committed NOTICE had, wrapped by the Python generator, except that "tools/fetch.py" is one character longer)
    CHECK(contains(old_form, "    EXCLUDED  CC-BY-NC-4.0: SN, days\n" + indent + "the file contains the above under CC-BY-NC-4.0, which is NOT the\n" +
                                 indent + "licence named above and NOT on this tree's permissive allowlist.\n" +
                                 indent + "This tree does not read it, and tools/fetch.cpp refuses a\n" +
                                 indent + "manifest entry that declares any of those columns as consumed. A\n" +
                                 indent + "reader who fetches the file for their own use is subject to CC-\n" +
                                 indent + "BY-NC-4.0 for that part of it.\n"));
    CHECK(contains(old_form, "    columns   consumed: YYYY, MM, DD, Kp1\n"));

    // a DERIVATIVE says that upstream's file contains the column and the vendored copy has it replaced (the stated substitution S6)
    const std::string derivative = nt::render(td.path(), manifest({with(plain, "derived_from_sha256", Json(kSha))}));
    CHECK(contains(derivative, "    EXCLUDED  CC-BY-NC-4.0: SN, days\n" +
                                   indent + "the file at the URL above contains the above under CC-BY-NC-4.0,\n" +
                                   indent + "which is NOT the licence named above and NOT on this tree's\n" +
                                   indent + "permissive allowlist; the copy vendored in this tree has that\n" +
                                   indent + "column replaced by the file's own missing-value marker. This\n" +
                                   indent + "tree does not read it, and tools/fetch.cpp refuses a manifest\n" +
                                   indent + "entry that declares any of those columns as consumed. A reader\n" +
                                   indent + "who fetches the file from its URL for their own use is subject\n" +
                                   indent + "to CC-BY-NC-4.0 for that part of it.\n"));
    // more than one licence is excluded: one paragraph for each, in the manifest's order
    const Json two = object({{"CC-BY-NC-4.0", list({"SN"})}, {"GPL-3.0-only", list({"extra"})}});
    const std::string both = nt::render(td.path(), manifest({with(data_entry(), "licence_excluded", two)}));
    CHECK(both.find("EXCLUDED  CC-BY-NC-4.0: SN") < both.find("EXCLUDED  GPL-3.0-only: extra"));

    // the sentence about permissive licences: the old three lines with no release blocker, four lines naming it with one, and the count with more
    const std::string none = nt::render(td.path(), manifest({data_entry()}));
    CHECK(contains(none, "  Every licence above is permissive.  Plan §5 constraint 3 forbids GPL,\n  LGPL and AGPL anywhere in what could ship; tools/fetch.cpp check-licences\n  enforces it and CI runs it.\n"));
    const Json blocker = with(with(data_entry(), "licence", Json("NONCOMMERCIAL-STAGE-EXCEPTION")), "release_blocker", Json(true));
    const std::string one = nt::render(td.path(), manifest({data_entry(), blocker}));
    CHECK(contains(one, "  Every licence above is permissive, with one exception: the stage exception\n  (a release blocker), named in its entry below.  Plan §5 constraint 3\n"
                        "  forbids GPL, LGPL and AGPL anywhere in what could ship; tools/fetch.cpp\n  check-licences enforces it and CI runs it.\n"));
    const Json second = with(with(with(data_entry(), "id", Json("other")), "licence", Json("NONCOMMERCIAL-STAGE-EXCEPTION")), "release_blocker", Json(true));
    const std::string several = nt::render(td.path(), manifest({data_entry(), blocker, second}));
    CHECK(contains(several, "  Every licence above is permissive, with 2 exceptions: the stage exceptions\n  (release blockers), named in their entries below.  Plan §5 constraint 3\n"));
    // a flag that is false, or not a boolean, is not a blocker
    CHECK(contains(nt::render(td.path(), manifest({with(data_entry(), "release_blocker", Json(false))})), "  Every licence above is permissive.  Plan"));
    CHECK(contains(nt::render(td.path(), manifest({with(data_entry(), "release_blocker", Json("true"))})), "  Every licence above is permissive.  Plan"));
}

TEST_CASE("the tool writes, prints and checks, and says how it drifted", "[notice]") {
    Tree t;
    const Json doc = manifest({code_entry(), data_entry(), paper_entry()});
    t.write_manifest(doc);
    const std::string text = nt::render(t.root(), doc);

    // default: writes NOTICE and says how many entries the manifest holds (the literature entry counts)
    Result r = run(t.root(), {});
    CHECK(r.code == 0);
    CHECK(r.out == "wrote    " + fs::weakly_canonical(t.root() / "NOTICE").string() + " (3 entries)\n");
    CHECK(read_text(t.root() / "NOTICE") == text);

    // --check on a current file
    r = run(t.root(), {"--check"});
    CHECK(r.code == 0);
    CHECK(r.out == "ok       NOTICE matches the manifest (3 entries)\n");
    CHECK(r.err.empty());

    // --stdout prints it, writes nothing, and wins over --check
    fs::remove(t.root() / "NOTICE");
    r = run(t.root(), {"--stdout"});
    CHECK(r.code == 0);
    CHECK(r.out == text);
    CHECK_FALSE(fs::exists(t.root() / "NOTICE"));
    CHECK(run(t.root(), {"--check", "--stdout"}).out == text);

    // a missing NOTICE has drifted from everything: the whole text is the diff
    r = run(t.root(), {"--check"});
    CHECK(r.code == 1);
    CHECK(r.out.empty());
    CHECK(contains(r.err, "NOTICE HAS DRIFTED from the manifest.\n\n--- NOTICE (committed)\n+++ NOTICE (from manifest)\n@@ -0,0 +1,"));
    CHECK(contains(r.err, "+NOTICE — third-party components\n"));
    CHECK(contains(r.err, "\n  NOTICE is generated, not maintained.  Run: cmake --build <build-dir> --target regenerate_notice\n"));

    // one changed line: the diff names it, with its context, and the file is not touched
    std::string edited = text;
    edited.replace(edited.find("a library"), 9, "a LIBRARY");
    write_text(t.root() / "NOTICE", edited);
    r = run(t.root(), {"--check"});
    CHECK(r.code == 1);
    CHECK(contains(r.err, "-    role      a LIBRARY\n+    role      a library\n"));
    CHECK(contains(r.err, "--- NOTICE (committed)\n+++ NOTICE (from manifest)\n@@ "));
    CHECK(read_text(t.root() / "NOTICE") == edited);
    // and the default form puts it right
    CHECK(run(t.root(), {}).code == 0);
    CHECK(run(t.root(), {"--check"}).code == 0);

    // --root=DIR is the same as --root DIR
    std::ostringstream out;
    std::ostringstream err;
    CHECK(nt::run({"--root=" + t.root().string(), "--check"}, Streams{out, err}) == 0);
}

TEST_CASE("what it refuses, with its own exit codes", "[notice]") {
    Tree t;
    // no manifest, a manifest that is not JSON, bytes that are not UTF-8, a manifest that is not an object with entries
    Result r = run(t.root(), {"--check"});
    CHECK(r.code == 3);
    CHECK(contains(r.err, "notice: cannot read "));
    fs::create_directories(t.root() / "manifest");
    write_text(t.root() / "manifest" / "manifest.json", "{ not json");
    r = run(t.root(), {});
    CHECK(r.code == 3);
    CHECK(contains(r.err, "cannot read"));
    write_bytes(t.root() / "manifest" / "manifest.json", Bytes{'{', 0xE9, '}'});
    CHECK(run(t.root(), {}).code == 3);
    write_text(t.root() / "manifest" / "manifest.json", "[1, 2]");
    CHECK(run(t.root(), {}).code == 3);
    write_text(t.root() / "manifest" / "manifest.json", "{\"schema\": 1}");
    r = run(t.root(), {});
    CHECK(r.code == 3);
    CHECK(contains(r.err, "no 'entries' list"));
    CHECK_FALSE(fs::exists(t.root() / "NOTICE"));

    // an entry that lacks what the text needs is refused, naming the entry (the Python died on a KeyError)
    const Json base_entry = code_entry();   // not `code_entry().as_object()` in a range-for: the temporary would be gone before the loop (C++20)
    Json::Object no_licence;
    for (const auto& m : base_entry.as_object()) {
        if (m.first != "licence") no_licence.push_back(m);
    }
    t.write_manifest(manifest({Json(no_licence)}));
    r = run(t.root(), {});
    CHECK(r.code == 3);
    CHECK(contains(r.err, "entry lib: has no 'licence'"));
    t.write_manifest(manifest({with(code_entry(), "licence", Json(std::int64_t{3}))}));
    CHECK(contains(run(t.root(), {}).err, "entry lib: 'licence' is not a string"));
    Json::Object no_url;
    for (const auto& m : base_entry.as_object()) {
        if (m.first != "url") no_url.push_back(m);
    }
    t.write_manifest(manifest({Json(no_url)}));
    CHECK(contains(run(t.root(), {}).err, "entry lib: has no 'url'"));
    t.write_manifest(manifest({with(code_entry(), "licence_excluded", Json("CC-BY-NC-4.0"))}));
    CHECK(contains(run(t.root(), {}).err, "'licence_excluded' is not an object"));
    t.write_manifest(manifest({with(code_entry(), "licence_excluded", object({{"CC-BY-NC-4.0", Json("SN")}}))}));
    CHECK(contains(run(t.root(), {}).err, "the columns of 'CC-BY-NC-4.0' are not a list"));
    t.write_manifest(object({{"entries", Json(Json::Array{Json(std::int64_t{1})})}}));
    CHECK(contains(run(t.root(), {}).err, "an entry of the manifest is not an object"));
    CHECK_FALSE(fs::exists(t.root() / "NOTICE"));

    // the command line: argparse's code 2, and -h is not an error
    CHECK(run_plain({"--nonsense"}).code == 2);
    r = run_plain({"--nonsense"});
    CHECK(contains(r.err, "usage: notice [-h] [--check] [--stdout] [--root ROOT]\nnotice: error: unrecognized arguments: --nonsense"));
    CHECK(run_plain({"positional"}).code == 2);
    CHECK(run_plain({"--root"}).code == 2);
    CHECK(contains(run_plain({"--root"}).err, "argument --root: expected one argument"));
    CHECK(run_plain({"--check=yes"}).code == 2);   // an option that takes no value
    r = run_plain({"-h"});
    CHECK(r.code == 0);
    CHECK(contains(r.out, "--check"));
    CHECK(contains(r.out, "--stdout"));
    CHECK(run_plain({"--help"}).code == 0);
}

TEST_CASE("the committed NOTICE is what the tool renders from the real manifest and the real cache", "[notice]") {
    const fs::path root = nt::default_root();
    const Json doc = Json::parse(read_text(root / "manifest" / "manifest.json"));
    const std::string rendered = nt::render(root, doc);
    const std::string committed = read_text(root / "NOTICE");
    // not a bare equality first: a failure should say WHERE
    const std::vector<std::string> a = splitlines_py(committed);
    const std::vector<std::string> b = splitlines_py(rendered);
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (a[i] != b[i]) {
            FAIL("NOTICE line " << i + 1 << " differs\n  committed: " << a[i] << "\n  rendered:  " << b[i]);
        }
    }
    CHECK(a.size() == b.size());
    CHECK(rendered == committed);
    // the two substitutions are in it, as the manifest's own entries call for them
    CHECK(contains(rendered, "  Every licence above is permissive, with one exception: the stage exception\n"));
    CHECK(contains(rendered, "the file at the URL above contains the above under CC-BY-NC-4.0,\n"));
    CHECK(contains(rendered, "Produced by tools/notice.cpp from\n"));
    CHECK_FALSE(contains(rendered, "tools/notice.py from"));
    CHECK_FALSE(contains(rendered, "python3 tools/notice.py"));
}
