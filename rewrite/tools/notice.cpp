// notice.cpp — NOTICE, generated from the manifest.
//
// Plan L0 step 5.  A hand-maintained NOTICE is wrong within two dependencies, so this one is not maintained at all: it is a pure function of
// manifest.json and of the pinned archives in the cache, and `--check` makes CI fail if the committed file has drifted.  The manifest already carries a
// licence and a licence note per entry because plan L0 step 3 requires it, so NOTICE generation needs no tool beyond the manifest and no dependency at
// all, which is a better answer than any third-party licence scanner: a scanner infers licences, and this reads the declaration that the fetcher
// already enforces.  Where an entry names a `licence_file` inside its archive, the licence text is quoted VERBATIM, extracted from the exact bytes the
// SHA-256 pins: a NOTICE that paraphrases a licence can be wrong, and one that quotes the pinned archive cannot be.
//
// PORTED FROM tools/notice.py (plan L0 step 8, the user's directive of 2026-10-06).  The output is the Python generator's, byte for byte, on the same
// manifest and cache, EXCEPT where D1's rule and the manager's ruling for this group say otherwise, and every such place is stated:
//   - the generator's own name and invocation (the header lines that said tools/notice.py and `python3 tools/notice.py`), and the Python fetcher's name
//     in the generator's text (tools/fetch.py -> tools/fetch.cpp): D1's generator-name tokens;
//   - TWO stated substitutions, because the old words became false: the EXCLUDED paragraph of an entry that is a derivative (the upstream file contains the
//     column, the copy vendored in this tree has it replaced), and the sentence "Every licence above is permissive." when the manifest holds a release
//     blocker (the one stage exception, named in its entry).
//   The list, line by line, is PROVENANCE.md section 41.6 and the commit message of this group.
//
// Where the Python crashed (a KeyError on an entry without a licence, a licence note that is not a string) this refuses, with a message that names the entry.
//
// Usage:  notice            write NOTICE
//         notice --check    exit 1 if NOTICE differs from what would be written
//         notice --stdout   print it instead
//         notice --root DIR the tree (default: the one this was built from)
// Exit codes: 0 ok, 1 drifted, 2 argument error (as argparse's), 3 the manifest is unreadable or malformed.

#include "notice.hpp"

#include <odl/devkit/archive.hpp>
#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/inflate.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include <iostream>
#include <optional>
#include <stdexcept>

namespace odl::tools::notice {

namespace {

namespace dk = odl::devkit;
namespace fs = std::filesystem;
using dk::Json;

constexpr const char* kTool = "notice";
constexpr int kOk = 0, kDrift = 1, kArgumentError = 2, kMalformed = 3;
constexpr std::size_t kWidth = 78;

const char kUsageText[] = "usage: notice [-h] [--check] [--stdout] [--root ROOT]\n";
const char kRegenerate[] = "cmake --build <build-dir> --target regenerate_notice";

std::string rule(char ch = '-') { return std::string(kWidth, ch); }

// ------------------------------------------------------------------------------------------------------------------------ the manifest's fields

std::string entry_name(const Json& e) {
    const Json* id = e.find("id");
    return id != nullptr && id->is_string() ? id->as_string() : "(an entry without an id)";
}

[[noreturn]] void refuse(const Json& e, const std::string& what) { throw std::runtime_error("entry " + entry_name(e) + ": " + what); }

bool truthy(const Json& e, std::string_view key) {
    const Json* v = e.find(key);
    return v != nullptr && v->truthy();
}

/// `str(value)`: a string as it is, the other scalars as Python prints them; a list or an object (no field of this tree's manifest is one) as JSON.
std::string py_str(const Json& v) {
    if (v.is_string()) return v.as_string();
    if (v.is_int()) return std::to_string(v.as_int());
    if (v.is_bool()) return v.as_bool() ? "True" : "False";
    if (v.is_null()) return "None";
    if (v.kind() == Json::Kind::Float) return dk::py_float_repr(v.as_double());
    return v.dumps(2);
}

/// `str(e.get(key, dflt))`.
std::string field(const Json& e, std::string_view key, std::string_view dflt = "") {
    const Json* v = e.find(key);
    return v == nullptr ? std::string(dflt) : py_str(*v);
}

/// `e[key]` for a key the text cannot do without, which must be a string.
const std::string& required(const Json& e, std::string_view key) {
    const Json* v = e.find(key);
    if (v == nullptr) refuse(e, "has no '" + std::string(key) + "'");
    if (!v->is_string()) refuse(e, "'" + std::string(key) + "' is not a string");
    return v->as_string();
}

bool has_release_blocker(const Json& e) {
    const Json* v = e.find("release_blocker");
    return v != nullptr && v->is_bool() && v->as_bool();
}

bool ends_with(const std::string& s, std::string_view suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }

// ------------------------------------------------------------------------------------------------------------------------ the licence texts

/// The licence file out of the cached, hash-verified archive, or nullopt when there is none to read (no licence_file, a host tool, no archive in the
/// cache, a member that is absent or not a regular file, an archive that is damaged).
std::optional<std::string> licence_text(const fs::path& root, const Json& doc, const Json& e) {
    if (!truthy(e, "licence_file") || truthy(e, "provided_by_host")) return std::nullopt;
    const Json* cache = doc.find("cache");
    const std::string cache_dir = cache != nullptr && cache->truthy() ? py_str(*cache) : "data/cache";
    const fs::path archive = root / cache_dir / required(e, "id") / required(e, "filename");
    std::error_code ec;
    if (!fs::exists(archive, ec)) return std::nullopt;
    const std::string inner = field(e, "licence_file");
    const std::string wanted = truthy(e, "unpacked_root") ? field(e, "unpacked_root") + "/" + inner : inner;
    const std::string name = archive.filename().string();
    try {
        if (field(e, "unpack") == "tar.gz" || ends_with(name, ".tar.gz") || ends_with(name, ".tgz")) {
            const dk::Bytes tar = dk::gunzip(dk::read_bytes(archive));
            const std::optional<dk::Bytes> member = dk::tar_member(tar, wanted);
            if (!member) return std::nullopt;
            return dk::decode_utf8_replace(dk::ByteView{*member});
        }
        if (ends_with(name, ".zip")) {
            const std::optional<dk::Bytes> member = dk::zip_member(dk::read_bytes(archive), wanted);
            if (!member) return std::nullopt;
            return dk::decode_utf8_replace(dk::ByteView{*member});
        }
    } catch (const std::runtime_error&) {   // the devkit's FormatError and its file errors: Python caught KeyError, TarError, BadZipFile and OSError
        return std::nullopt;
    }
    return std::nullopt;
}

// ------------------------------------------------------------------------------------------------------------------------ the paragraphs that vary

void wrapped(std::vector<std::string>& out, const std::string& text, std::string_view initial, std::string_view subsequent) {
    dk::WrapOptions options;
    options.width = kWidth;
    options.initial_indent = std::string(initial);
    options.subsequent_indent = std::string(subsequent);
    for (std::string& line : dk::wrap_py(text, options)) out.push_back(std::move(line));
}

/// The EXCLUDED paragraph of one licence.  A DERIVATIVE (an entry that names `derived_from_sha256`) says what is true of it: upstream's file contains the
/// column, the copy vendored in this tree has it replaced.  Any other entry keeps the generator's own sentence, which is true of it.
std::string excluded_paragraph(const Json& e, const std::string& lic) {
    if (truthy(e, "derived_from_sha256")) {
        return "the file at the URL above contains the above under " + lic + ", which is NOT the licence named above and NOT on this tree's permissive allowlist; "
               "the copy vendored in this tree has that column replaced by the file's own missing-value marker. This tree does not read it, and "
               "tools/fetch.cpp refuses a manifest entry that declares any of those columns as consumed. A reader who fetches the file from its URL for "
               "their own use is subject to " + lic + " for that part of it.";
    }
    return "the file contains the above under " + lic + ", which is NOT the licence named above and NOT on this tree's permissive allowlist. This tree does "
           "not read it, and tools/fetch.cpp refuses a manifest entry that declares any of those columns as consumed. A reader who fetches the file for "
           "their own use is subject to " + lic + " for that part of it.";
}

/// "Every licence above is permissive." is false of an entry under the stage exception (a release blocker); the paragraph then says so, and names it.
void permissive_paragraph(std::vector<std::string>& out, std::size_t blockers) {
    if (blockers == 0) {
        out.push_back("  Every licence above is permissive.  Plan §5 constraint 3 forbids GPL,");
        out.push_back("  LGPL and AGPL anywhere in what could ship; tools/fetch.cpp check-licences");
        out.push_back("  enforces it and CI runs it.");
        return;
    }
    if (blockers == 1) {
        out.push_back("  Every licence above is permissive, with one exception: the stage exception");
        out.push_back("  (a release blocker), named in its entry below.  Plan §5 constraint 3");
    } else {
        out.push_back("  Every licence above is permissive, with " + std::to_string(blockers) + " exceptions: the stage exceptions");
        out.push_back("  (release blockers), named in their entries below.  Plan §5 constraint 3");
    }
    out.push_back("  forbids GPL, LGPL and AGPL anywhere in what could ship; tools/fetch.cpp");
    out.push_back("  check-licences enforces it and CI runs it.");
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------------ NOTICE

std::string render(const fs::path& root, const Json& doc) {
    const Json* entries_member = doc.find("entries");
    if (entries_member == nullptr || !entries_member->is_array()) throw std::runtime_error("the manifest has no 'entries' list");
    const Json::Array& entries = entries_member->as_array();
    for (const Json& e : entries) {
        if (!e.is_object()) throw std::runtime_error("an entry of the manifest is not an object");
        required(e, "id");
        required(e, "kind");
    }
    std::vector<std::string> out;
    const auto a = [&](std::string line) { out.push_back(std::move(line)); };

    a("NOTICE — third-party components");
    a(rule('='));
    a("");
    a("GENERATED FILE.  Do not edit.  Produced by tools/notice.cpp from");
    a("manifest/manifest.json, which is the single declaration of every external");
    a("input to this tree.  To change anything here, change the manifest and run");
    a(std::string("    ") + kRegenerate);
    a("CI runs `notice --check` and fails if this file has drifted.");
    a("");
    a("This file does not describe the licence of THIS tree, which grants nothing");
    a("for now and is stated in LICENSE.  It describes what this tree depends on.");
    a("");

    std::vector<const Json*> code;
    std::vector<const Json*> data;
    std::vector<const Json*> tools;
    for (const Json& e : entries) {
        const std::string& kind = required(e, "kind");
        if (kind == "code") code.push_back(&e);
        else if (kind == "data") data.push_back(&e);
        else if (kind == "tool") tools.push_back(&e);
    }

    a(rule());
    a("SUMMARY");
    a(rule());
    a("");
    a("  " + dk::pad_right("component", 18) + " " + dk::pad_right("version", 12) + " " + dk::pad_right("licence", 16) + " kind");
    std::size_t blockers = 0;
    for (const Json& e : entries) {
        if (field(e, "kind") == "literature") continue;
        a("  " + dk::pad_right(required(e, "id"), 18) + " " + dk::pad_right(field(e, "version"), 12) + " " + dk::pad_right(required(e, "licence"), 16) + " " +
          required(e, "kind"));
        if (has_release_blocker(e)) ++blockers;
    }
    a("");
    {
        std::vector<std::string> paragraph;
        permissive_paragraph(paragraph, blockers);
        for (std::string& line : paragraph) a(std::move(line));
    }
    a("");

    struct Group {
        const char* heading;
        const std::vector<const Json*>& items;
        const char* blurb;
    };
    const Group groups[] = {
        {"COMPONENTS COMPILED INTO OR LINKED WITH THIS TREE", code,
         "Fetched from origin, pinned by SHA-256, and verified twice — once by\n"
         "  tools/fetch.cpp and once by CMake's URL_HASH."},
        {"DATA INPUTS", data,
         "External data, pinned by hash.  The identity of an input is its hash,\n"
         "  not its URL: at least one upstream here revises its published series\n"
         "  retroactively at an unchanged address."},
        {"BUILD-TIME TOOLS", tools,
         "Present on the build host.  Not vendored, not linked, shipped in nothing.\n"
         "  Listed because an external input is an external input."},
    };
    for (const Group& g : groups) {
        a(rule());
        a(g.heading);
        a(rule());
        a("");
        if (g.items.empty()) {
            a("  None at this revision.");
            a("");
            continue;
        }
        a(std::string("  ") + g.blurb);
        a("");
        for (const Json* ep : g.items) {
            const Json& e = *ep;
            a("  " + required(e, "id") + "  " + field(e, "version"));
            a("    role      " + field(e, "role", "—"));
            a("    licence   " + required(e, "licence"));
            if (truthy(e, "provided_by_host")) {
                a("    source    provided by the build host; not fetched");
            } else {
                const Json* url = e.find("url");
                const Json* sha = e.find("sha256");
                if (url == nullptr) refuse(e, "has no 'url'");
                if (sha == nullptr) refuse(e, "has no 'sha256'");
                a("    url       " + py_str(*url));
                a("    sha256    " + py_str(*sha));
                if (truthy(e, "archived_url") && field(e, "archived_url") != field(e, "url")) a("    archived  " + field(e, "archived_url"));
                if (truthy(e, "retrieved")) a("    retrieved " + field(e, "retrieved"));
            }
            if (truthy(e, "licence_note")) wrapped(out, required(e, "licence_note"), "    note      ", "              ");
            // A FILE'S LICENCE CAN DIFFER BY COLUMN, and the entry's own licence field states the licence of WHAT THIS TREE CONSUMES.  Anyone who fetches
            // the file gets the rest of it, whether or not this tree reads it, so NOTICE has to say so: this is the licence document a downstream reader
            // consults, and "CC-BY-4.0" alone would be true of our use and misleading about the file.
            const Json* excluded = e.find("licence_excluded");
            if (excluded != nullptr && excluded->truthy()) {
                if (!excluded->is_object()) refuse(e, "'licence_excluded' is not an object");
                for (const auto& [lic, cols] : excluded->as_object()) {
                    if (!cols.is_array()) refuse(e, "the columns of '" + lic + "' are not a list");
                    std::string joined;
                    for (std::size_t i = 0; i < cols.as_array().size(); ++i) joined += (i ? ", " : "") + py_str(cols.as_array()[i]);
                    a("    EXCLUDED  " + lic + ": " + joined);
                    wrapped(out, excluded_paragraph(e, lic), "              ", "              ");
                }
            }
            const Json* columns = e.find("columns");
            const Json* declared = columns != nullptr && columns->truthy() && columns->is_object() ? columns->find("declared") : nullptr;
            if (declared != nullptr && declared->truthy()) {
                if (!declared->is_array()) refuse(e, "the declared columns are not a list");
                std::string joined;
                for (std::size_t i = 0; i < declared->as_array().size(); ++i) joined += (i ? ", " : "") + py_str(declared->as_array()[i]);
                wrapped(out, "consumed: " + joined, "    columns   ", "              ");
            }
            a("");
        }
    }

    std::vector<std::pair<const Json*, std::string>> quoted;
    for (const Json& e : entries) {
        if (field(e, "kind") == "literature") continue;
        if (std::optional<std::string> text = licence_text(root, doc, e)) {
            if (!text->empty()) quoted.emplace_back(&e, std::move(*text));
        }
    }
    if (!quoted.empty()) {
        a(rule());
        a("LICENCE TEXTS, QUOTED VERBATIM FROM THE PINNED ARCHIVES");
        a(rule());
        a("");
        for (const auto& [ep, text] : quoted) {
            const Json& e = *ep;
            const std::string title = "--- " + required(e, "id") + " " + field(e, "version") + " — " + required(e, "licence") + " (" + field(e, "licence_file") + ") ";
            std::string padded = title;
            for (std::size_t n = dk::code_points(title); n < kWidth; ++n) padded.push_back('-');   // str.ljust(78, "-")
            a(padded);
            a("");
            for (const std::string& line : dk::splitlines_py(dk::rstrip_py(text))) a("  " + dk::rstrip_py(line));
            a("");
        }
    }

    std::string joined;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i != 0) joined += '\n';
        joined += out[i];
    }
    return dk::rstrip_py(joined) + "\n";
}

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

int run(const std::vector<std::string>& argv, dk::Streams io) {
    try {
        bool check = false;
        bool to_stdout = false;
        fs::path root = default_root();
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgumentError;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool have_value = false;
            const std::size_t eq = opt.rfind("--", 0) == 0 ? opt.find('=') : std::string::npos;
            if (eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                have_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << "\ngenerate NOTICE from the manifest.\n\n"
                       << "options:\n"
                       << "  --check      fail (exit 1) if NOTICE has drifted from what would be written\n"
                       << "  --stdout     print it instead of writing it\n"
                       << "  --root ROOT  the tree (default: the one this was built from)\n";
                return kOk;
            }
            if (opt == "--check" && !have_value) {
                check = true;
            } else if (opt == "--stdout" && !have_value) {
                to_stdout = true;
            } else if (opt == "--root") {
                if (!have_value) {
                    if (i + 1 >= argv.size()) return usage_error("argument --root: expected one argument");
                    value = argv[++i];
                }
                root = value;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }
        std::error_code ec;
        root = fs::weakly_canonical(root, ec);
        const fs::path manifest = root / "manifest" / "manifest.json";
        Json doc;
        try {
            doc = Json::parse(dk::read_text(manifest));
        } catch (const std::runtime_error& exc) {   // an unreadable file, bytes that are not UTF-8, and dk::JsonError, which is one
            io.err << kTool << ": cannot read " << manifest.string() << ": " << exc.what() << '\n';
            return kMalformed;
        }
        std::string text;
        try {
            text = render(root, doc);
        } catch (const std::runtime_error& exc) {
            io.err << kTool << ": " << manifest.string() << ": " << exc.what() << '\n';
            return kMalformed;
        }
        const Json* entries = doc.find("entries");
        const std::size_t count = entries != nullptr && entries->is_array() ? entries->as_array().size() : 0;
        const fs::path target = root / "NOTICE";

        if (to_stdout) {
            io.out << text;
            return kOk;
        }
        if (check) {
            std::string current;
            if (fs::exists(target, ec)) {
                const dk::Bytes b = dk::read_bytes(target);
                current = std::string(dk::as_text(dk::ByteView{b}));
            }
            if (current == text) {
                io.out << "ok       NOTICE matches the manifest (" << count << " entries)\n";
                return kOk;
            }
            io.err << "NOTICE HAS DRIFTED from the manifest.\n\n";
            for (const std::string& line : dk::unified_diff(dk::splitlines_py(current), dk::splitlines_py(text), "NOTICE (committed)", "NOTICE (from manifest)")) {
                io.err << line << '\n';
            }
            io.err << "\n  NOTICE is generated, not maintained.  Run: " << kRegenerate << '\n';
            return kDrift;
        }
        dk::write_text(target, text);
        io.out << "wrote    " << target.string() << " (" << count << " entries)\n";
        return kOk;
    } catch (const dk::Exit& x) {
        return x.code;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return 70;
    }
}

}  // namespace odl::tools::notice

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::notice::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
