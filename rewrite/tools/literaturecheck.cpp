// literaturecheck.cpp — nothing that builds may reach the literature.
//
// Plan L0 step 6's checker, ported to C++ in plan L0 step 8 (group C5, the user's directive of 2026-10-06) from tools/literaturecheck.py.
//
// Plan section 5 constraint 3.  A `literature` manifest entry is exempt from the permissive-licence gate because it is a PROVENANCE RECORD, NOT A DEPENDENCY: it is pinned by hash so that "this was
// derived from that" is checkable by a future reader who fetches the same hash, and nothing derived from it is a copy of it.
//
// THE EXEMPTION IS EARNED BY A CHECKED PROPERTY AND NEVER BY THE LABEL, which is what this tool is.  A carve-out that cannot be shown to fire is a carve-out that will be widened, so the three
// conditions are mechanical:
//
//   1. a literature entry is fetched OUTSIDE the build cache -- `data/literature`, not `data/cache` (tools/fetch.cpp's entry_path);
//   2. NO BUILD INPUT REFERENCES THAT PATH, which is this gate, and it is demonstrated by injecting one (plan section 4 rule 5);
//   3. the repository holds the URL and the hash and never the bytes, so this project redistributes nothing -- `.gitignore` carries `data/literature/`.
//
// Condition 2 is the one that could rot silently.  A source file that opened one of these PDFs, or a CMake target that copied one, would turn a citation into a dependency without anybody
// deciding to -- and the licence question this tree just ruled on would be back, answered wrongly by default.
//
// WHAT A BUILD INPUT IS: the root CMakeLists.txt and every CMakeLists.txt, *.cmake, *.cpp, *.hpp, *.h and *.c below modules/, tests/ and cmake/ (no path with a `build` component).  What it must not
// mention: the literature root, or any literature entry's id or filename.  Specifications and PROVENANCE are NOT build inputs on purpose: a citation is exactly where these belong.
//
//   literaturecheck [--quiet] [--root DIR]
//   exit 0 no build input can reach a literature entry   1 one does   2 an argument error, a manifest that cannot be read, or nothing to search   70 an error the tool did not anticipate
//
// THE PROOF.  The tool's product is what it PRINTS; with --quiet (ci.sh's gate 11) that is the three counts and the verdict, and the committed record of what literaturecheck.py printed is gate 11's
// section of every green ci run: ten of them, on ten different trees (297 to 304 build inputs), kept with the L0-8 report (C5_baselines/).  The port, run on an export of each of those ten commits,
// printed each section byte for byte, against a substitution list registered before the comparison and EMPTY.  The non-quiet listing and the text of a failure have no committed record: the Python
// was not run beside the port, and those parts stand on tests derived by hand, on the controls (a build input that names an entry is reached, one that does not is not) and on rule 5.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place, which a test of a synthetic tree cannot use); the default is the tree this tool was built from.
//   * The Python tested `"build" not in p.parts` on the ABSOLUTE path: a tree under a directory called `build` would have excluded every input and passed with "0".  The path RELATIVE to the root
//     is tested here.  No recorded result moves (no tree that was checked had such an ancestor).
//   * A run that has NOTHING to search or no readable manifest is REFUSED, exit 2 (the Python passed "0 inputs", and crashed on a missing manifest): success over nothing read is the failure a checker
//     exists against (the maintainer's ruling of 2026-10-07 on budgetcheck, applied here by analogy and flagged for ratification).
//   * A glob match that is not a regular file (a directory called x.cpp, a link to nothing, a FIFO) is skipped by rule; the Python's read_text raised on it.  A link to a directory is not entered (pathlib's
//     rglob does not).  Files are read as the Python read them, errors="replace" (a byte sequence that is not UTF-8 becomes U+FFFD).
//   * The needles are visited in sorted order (the Python's came out of a set, in an order that changes from run to run); only the order of the lines of a FAILING listing is affected.
//   * A manifest entry that is no object, or a literature entry without a string `id` or `filename`, is refused with a message (the Python raised KeyError); `terms` that is no string reads as empty.
//   * `terms.upper().startswith("NOT ESTABLISHED")` is read through Python's FULL case mapping for the characters that can matter: the ASCII letters, and the ten non-ASCII characters whose upper
//     case is pure ASCII (the dotless i, the long s, the sharp s, the six f- and s-ligatures), of which only the four that can occur in the phrase are mapped; any other non-ASCII character
//     fails the comparison, as it does there.
//     (The first draft of this port named only the dotless i and the long s; the full mapping, run over every code point, shows ten.)
//   * The messages name `literaturecheck`; argparse's abbreviations are not accepted and `-h` prints this tool's own text.  An option's value is whatever follows it unless that begins with `--`
//     (argparse refuses one that begins with a single dash too, so a directory called -x is a value here), and `-h=x` is refused.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "literaturecheck.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

namespace odl::tools::literaturecheck {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "literaturecheck";
constexpr int kOk = 0, kReached = 1, kArgument = 2, kInternal = 70;
constexpr std::size_t kNpos = std::string_view::npos;

using Io = dk::Streams;

std::string right(std::size_t value, std::size_t width) { return dk::pad_left(std::to_string(value), width); }

// the first `n` code points of s (Python's s[:n])
std::string first_code_points(std::string_view s, std::size_t n) {
    std::size_t i = 0;
    for (std::size_t taken = 0; taken < n && i < s.size(); ++taken) {
        std::size_t after = 0;
        (void)dk::code_point_at(s, i, after);
        i = after;
    }
    return std::string(s.substr(0, i));
}

// terms.upper().startswith("NOT ESTABLISHED"): the first characters of terms.upper(), made one code point at a time.  Python's upper() is the FULL case mapping, so ten non-ASCII characters become
// ASCII letters: the dotless i and the long s (one letter each), the sharp s (SS) and the ff, fi, fl, ffi, ffl, long-s-t and st ligatures (FF, FI, FL, FFI, FFL, ST, ST).  (The ten were found by
// mapping every code point, with Perl's full case mapping as the witness, and keeping the ones whose result is pure ASCII.)  Only four of them can be part of the phrase -- SS, FF, FI, FL, FFI
// and FFL occur nowhere in "NOT ESTABLISHED" -- so only those four are mapped; every other non-ASCII character, those six included, becomes a byte no phrase holds, which can only make the
// comparison fail, as the real mapping would.
bool starts_not_established(std::string_view terms) {
    constexpr std::string_view kWanted = "NOT ESTABLISHED";
    std::string upper;
    for (std::size_t i = 0; i < terms.size();) {
        std::size_t after = 0;
        const std::uint32_t cp = dk::code_point_at(terms, i, after);
        i = after;
        if (cp < 0x80) {
            upper += static_cast<char>(cp >= U'a' && cp <= U'z' ? cp - U'a' + U'A' : cp);
            continue;
        }
        switch (cp) {
            case 0x0131: upper += "I"; break;     // LATIN SMALL LETTER DOTLESS I
            case 0x017F: upper += "S"; break;     // LATIN SMALL LETTER LONG S
            case 0xFB05: upper += "ST"; break;    // LATIN SMALL LIGATURE LONG S T
            case 0xFB06: upper += "ST"; break;    // LATIN SMALL LIGATURE ST
            default: upper += '\x01'; break;
        }
    }
    return upper.compare(0, kWanted.size(), kWanted) == 0;
}

bool ends_with(std::string_view s, std::string_view suffix) { return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0; }

// BUILD_GLOBS = ("CMakeLists.txt", "*.cmake", "*.cpp", "*.hpp", "*.h", "*.c"), each matched against the NAME as fnmatch does: `*` is anything, the empty string included
bool matches_build_glob(std::string_view name) {
    return name == "CMakeLists.txt" || ends_with(name, ".cmake") || ends_with(name, ".cpp") || ends_with(name, ".hpp") || ends_with(name, ".h") || ends_with(name, ".c");
}

// text.split("\n")
std::vector<std::string_view> split_on_newline(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t from = 0;
    while (true) {
        const std::size_t nl = text.find('\n', from);
        if (nl == kNpos) {
            lines.push_back(text.substr(from));
            return lines;
        }
        lines.push_back(text.substr(from, nl - from));
        from = nl + 1;
    }
}

struct Entry {
    std::string id;
    std::string filename;
    std::string terms;
};

struct Input {
    std::vector<std::string> rel;   // path components below the root
    fs::path path;
};

struct Hit {
    std::string file;
    std::size_t line = 0;
    std::string needle;
    std::string text;
};

std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += (i != 0 ? "/" : "") + parts[i];
    return out;
}

const char kUsageText[] = "usage: literaturecheck [-h] [--quiet] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "nothing that builds may reach the literature (plan section 5 constraint 3): no build input (the root CMakeLists.txt and every CMakeLists.txt, *.cmake, *.cpp, *.hpp, *.h and *.c below\n"
    "modules/, tests/ and cmake/) names the literature root or any literature entry's id or filename.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --quiet      print the three counts and the verdict, not each entry\n"
    "  --root ROOT  the tree (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 no build input reaches a literature entry   1 one does   2 an argument error, a manifest that cannot be read, or nothing to search\n";

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
        std::string root_given = default_root().string();
        bool quiet = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument --root: expected one argument");
                    value = argv[++i];
                }
                root_given = value;
            } else if (opt == "--quiet" && !has_value) {
                quiet = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }
        const fs::path root(root_given);

        // doc = json.loads((ROOT / "manifest" / "manifest.json").read_text())
        const fs::path manifest_path = root / "manifest" / "manifest.json";
        dk::Json doc;
        try {
            doc = dk::Json::parse(dk::read_text(manifest_path));
        } catch (const dk::JsonError& exc) {
            io.err << kTool << ": " << manifest_path.string() << ": " << exc.reason << ": line " << exc.line << " column " << exc.column << '\n';
            return kArgument;
        } catch (const std::runtime_error& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
        const dk::Json* entries = doc.find("entries");
        if (entries == nullptr || !entries->is_array()) {
            io.err << kTool << ": " << manifest_path.string() << ": no `entries` array\n";
            return kArgument;
        }
        std::vector<Entry> lit;   // [e for e in doc["entries"] if e.get("kind") == "literature"]
        std::size_t index = 0;
        for (const dk::Json& e : entries->as_array()) {
            ++index;
            if (!e.is_object()) {
                io.err << kTool << ": " << manifest_path.string() << ": entry " << index << " is no object\n";
                return kArgument;
            }
            const dk::Json* kind = e.find("kind");
            if (kind == nullptr || !kind->is_string() || kind->as_string() != "literature") continue;
            const dk::Json* id = e.find("id");
            const dk::Json* filename = e.find("filename");
            if (id == nullptr || !id->is_string() || filename == nullptr || !filename->is_string()) {
                io.err << kTool << ": " << manifest_path.string() << ": literature entry " << index << " has no string `id` or `filename`\n";
                return kArgument;
            }
            const dk::Json* terms = e.find("terms");
            lit.push_back(Entry{id->as_string(), filename->as_string(), terms != nullptr && terms->is_string() ? terms->as_string() : std::string()});
        }
        std::string lit_root = "data/literature";   // doc.get("literature", "data/literature")
        if (const dk::Json* given = doc.find("literature")) {
            if (!given->is_string()) {
                io.err << kTool << ": " << manifest_path.string() << ": `literature` is no string\n";
                return kArgument;
            }
            lit_root = given->as_string();
        }

        // what a build input must not mention: the root, or any entry's id or filename (an empty one is no needle)
        std::set<std::string> needle_set = {lit_root};
        for (const Entry& e : lit) {
            needle_set.insert(e.id);
            needle_set.insert(e.filename);
        }
        needle_set.erase(std::string());
        const std::vector<std::string> needles(needle_set.begin(), needle_set.end());   // sorted

        // build_inputs(): ROOT/CMakeLists.txt, then BUILD_GLOBS below BUILD_DIRS without a `build` component, all sorted as paths
        std::vector<Input> inputs;
        if (fs::is_regular_file(root / "CMakeLists.txt")) inputs.push_back(Input{{"CMakeLists.txt"}, root / "CMakeLists.txt"});
        for (const char* base : {"modules", "tests", "cmake"}) {
            std::error_code ec;
            if (!fs::exists(root / base, ec)) continue;
            for (const std::vector<std::string>& parts : dk::files_below(root / base, [](const std::string& name) { return name == "build"; })) {
                if (!matches_build_glob(parts.back())) continue;
                std::vector<std::string> rel = {base};
                rel.insert(rel.end(), parts.begin(), parts.end());
                inputs.push_back(Input{rel, root / joined(rel)});
            }
        }
        std::sort(inputs.begin(), inputs.end(), [](const Input& a, const Input& b) { return a.rel < b.rel; });
        if (inputs.empty()) {
            io.err << kTool << ": nothing to search: no CMakeLists.txt, *.cmake, *.cpp, *.hpp, *.h or *.c file under " << root.string() << '\n';
            return kArgument;
        }

        std::vector<Hit> hits;
        for (const Input& input : inputs) {
            std::string text;
            try {
                text = dk::read_text_lossy(input.path);
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            const std::vector<std::string_view> lines = split_on_newline(text);
            for (const std::string& needle : needles) {
                for (std::size_t i = 0; i < lines.size(); ++i) {
                    if (lines[i].find(needle) != kNpos) hits.push_back(Hit{joined(input.rel), i + 1, needle, first_code_points(dk::strip_py(lines[i]), 88)});
                }
            }
        }

        if (!quiet) {
            io.out << "LITERATURE ENTRIES \xE2\x80\x94 pinned for provenance, exempt from the licence gate,\n";
            io.out << "and unreachable from anything that builds.\n\n";
            for (const Entry& e : lit) {
                io.out << "  " << e.id << '\n';
                io.out << "    fetched to  " << lit_root << '/' << e.id << '/' << e.filename << '\n';
                // A literature entry carries `terms`, never `licence`: read the recorded search, distinguishing found nothing (terms text starting "NOT ESTABLISHED") from found something.
                io.out << "    terms       " << (starts_not_established(e.terms) ? "NOT established \xE2\x80\x94 see the entry" : "found \xE2\x80\x94 see the entry") << '\n';
            }
        }
        io.out << "\n  literature entries              " << right(lit.size(), 5) << '\n';
        io.out << "  build inputs searched           " << right(inputs.size(), 5) << '\n';
        io.out << "  build inputs REACHING one       " << right(hits.size(), 5) << '\n';

        if (!hits.empty()) {
            io.err << "\nA BUILD INPUT REACHES A LITERATURE ENTRY:\n";
            for (const Hit& h : hits) io.err << "  " << h.file << ':' << h.line << "  mentions " << dk::py_repr(h.needle) << "\n      " << h.text << '\n';
            io.err << "\n  That turns a citation into a dependency, and the licence exemption plan \xC2\xA7" "5\n"
                   << "  constraint 3 grants does not cover it: the exemption holds because this tree\n"
                   << "  READS these and does not redistribute or build against them. If the build needs\n"
                   << "  it, it is not literature -- pin it as data, with a licence that passes the gate.\n";
            return kReached;
        }

        io.out << "\nok       no build input can reach a literature entry\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::literaturecheck

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::literaturecheck::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
