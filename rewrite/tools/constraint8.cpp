// constraint8.cpp — keep D7's migration one line.
//
// Plan L0 step 6's checker, ported to C++ in plan L0 step 8 (group C5, the user's directive of 2026-10-06) from tools/constraint8.py.
//
// Plan section 5 constraint 8.  The owner chose C++20 with a vendored `tl::expected` over moving to C++23, on the ground that C++20 -> C++23 later is two CMake lines and deleting a shim, where the
// reverse would mean hunting every C++23 feature that had crept in over months.  That migration is near-free ONLY WHILE THE TREE STAYS INSIDE THE PART OF THE API WHERE `tl::expected` AND
// `std::expected` ARE INTERCHANGEABLE: construction, checking, unwrapping.  The monadic operations are where the two diverge.  So the constraint is:
//
//   1. Every signature names `odl::Result`, never the underlying type.
//   2. No `and_then`, `or_else`, `transform` or `transform_error`, ever.
//
// A constraint that depends on everyone remembering it is a constraint that expires quietly, and this one expires in a way nobody notices until the day the migration is attempted and turns out
// not to be one line after all.  So it is checked, and CI runs the check.
//
//   constraint8 [--root DIR]
//   exit 0 clean   1 a violation   2 an argument error, or nothing to scan   70 an error the tool did not anticipate
//
// THE PROOF.  The tool's product is what it PRINTS, and the committed record of what constraint8.py printed is gate 9's section of every green ci run: ten of them, on ten different trees (262 to 268
// files), kept with the L0-8 report (C5_baselines/).  The port, run on an export of each of those ten commits, printed each section byte for byte, against a substitution list registered before the
// comparison and EMPTY.  The Python was not run beside it.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * A run that scans NOTHING (no source file under DIR/modules or DIR/tests) is REFUSED, exit 2, naming the root.  The Python passed it with "0 files": success over nothing read, the failure a
//     checker exists against (the maintainer's ruling of 2026-10-07 on budgetcheck, applied here by analogy and flagged for ratification).
//   * A suffix match that is not a regular file (a directory called x.cpp, a link to nothing, a FIFO) is skipped by rule; the Python's read_text raised on it.  A link to a directory is not entered
//     (pathlib's rglob does not), a link to a regular file is read.  The Python read with errors="replace" and so does this (a byte sequence that is not UTF-8 becomes U+FFFD).
//   * The root is used as given (the Python resolved it: relative paths are printed below it either way); the messages name `constraint8`; argparse's abbreviations (`--ro`) are not accepted and
//     `-h` prints this tool's own text; an option's value is whatever follows it unless that begins with `--` (argparse refuses one that begins with a single dash too, so a directory called
//     -x is a value here), and `-h=x` is refused.
//   * `\b` and `\s` are Unicode-aware as Python's are, within the limit odl/devkit/pytext.hpp states.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "constraint8.hpp"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

namespace odl::tools::constraint8 {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "constraint8";
constexpr int kOk = 0, kViolation = 1, kArgument = 2, kInternal = 70;
constexpr std::size_t kNpos = std::string_view::npos;

// The one file allowed to name the underlying type: the alias itself.
constexpr const char* kAliasHeader = "modules/core/include/odl/core/result.hpp";

using Io = dk::Streams;

// `\s*` from i.
std::size_t skip_space(std::string_view s, std::size_t i) {
    while (i < s.size()) {
        std::size_t after = 0;
        if (!dk::is_py_space(dk::code_point_at(s, i, after))) break;
        i = after;
    }
    return i;
}

}  // namespace

namespace scan {

// UNDERLYING = re.compile(r"\btl::(expected|unexpected)\b").search(line): every `tl::` that does not follow a word character, then `expected` or `unexpected`, then a character that is no word
// character (or the end of the line).
bool names_underlying_type(std::string_view line) {
    for (std::size_t i = line.find("tl::"); i != kNpos; i = line.find("tl::", i + 1)) {
        if (!dk::word_boundary_at(line, i)) continue;   // `\b` before the t: the character before it is no word character
        const std::size_t j = i + 4;
        std::size_t end = kNpos;
        if (line.compare(j, 8, "expected") == 0) end = j + 8;
        else if (line.compare(j, 10, "unexpected") == 0) end = j + 10;
        if (end == kNpos) continue;
        if (dk::word_boundary_at(line, end)) return true;   // `\b` after the d: the character after it is no word character
    }
    return false;
}

// MONADIC = re.compile(r"\.\s*(and_then|or_else|transform_error|transform)\s*\(").search(line).group(1): METHOD-CALL syntax, so free functions such as std::ranges::transform and std::transform do
// not match -- those are unrelated and perfectly fine.  At each dot the alternatives are tried in the pattern's order (`transform_error` before `transform`).
std::optional<std::string> monadic_operation(std::string_view line) {
    static constexpr std::string_view kNames[] = {"and_then", "or_else", "transform_error", "transform"};
    for (std::size_t i = line.find('.'); i != kNpos; i = line.find('.', i + 1)) {
        const std::size_t j = skip_space(line, i + 1);
        for (const std::string_view name : kNames) {
            if (line.compare(j, name.size(), name) != 0) continue;
            const std::size_t k = skip_space(line, j + name.size());
            if (k < line.size() && line[k] == '(') return std::string(name);
        }
    }
    return std::nullopt;
}

}  // namespace scan

namespace {

const char kUsageText[] = "usage: constraint8 [-h] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "keep D7's migration one line (plan section 5 constraint 8): every signature names odl::Result and never the underlying type, and no monadic operation (and_then, or_else, transform,\n"
    "transform_error) is used in method-call syntax, in any .hpp .cpp .h .cc .ipp file under <root>/modules and <root>/tests.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 clean   1 a violation   2 an argument error, or nothing to scan\n";

std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += (i != 0 ? "/" : "") + parts[i];
    return out;
}

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
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        // sources(): for base in ("modules", "tests"): sorted(d.rglob("*")) with the suffix in {.hpp, .cpp, .h, .cc, .ipp}, base by base
        static const std::set<std::string> kSuffixes = {".hpp", ".cpp", ".h", ".cc", ".ipp"};
        const fs::path root(root_given);
        struct Source {
            std::string rel;
            fs::path path;
        };
        std::vector<Source> sources;
        for (const char* base : {"modules", "tests"}) {
            for (const std::vector<std::string>& parts : dk::files_below(root / base)) {
                if (kSuffixes.count(dk::path_suffix(parts.back())) == 0) continue;
                const std::string below = joined(parts);
                sources.push_back(Source{std::string(base) + "/" + below, root / base / below});
            }
        }
        if (sources.empty()) {
            io.err << kTool << ": nothing to scan: no .hpp, .cpp, .h, .cc or .ipp file under " << (root / "modules").string() << " or " << (root / "tests").string() << '\n';
            return kArgument;
        }

        std::vector<std::string> violations;
        for (const Source& source : sources) {
            std::string text;
            try {
                text = dk::read_text_lossy(source.path);
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            std::size_t n = 0;
            for (const std::string& line : dk::splitlines_py(text)) {
                ++n;
                const std::string stripped = dk::lstrip_py(line);
                if (stripped.rfind("//", 0) == 0 || stripped.rfind("*", 0) == 0) continue;   // prose about the rule is not a breach of it
                if (source.rel != kAliasHeader && scan::names_underlying_type(line)) {
                    violations.push_back(source.rel + ":" + std::to_string(n) + ": names the underlying type\n    " + dk::strip_py(line) +
                                         "\n    Constraint 8.1: every signature names odl::Result. Only\n    " + kAliasHeader + " may name tl::expected.");
                }
                if (const std::optional<std::string> operation = scan::monadic_operation(line)) {
                    violations.push_back(source.rel + ":" + std::to_string(n) + ": monadic operation '" + *operation + "'\n    " + dk::strip_py(line) +
                                         "\n    Constraint 8.2: construction, checking and unwrapping are where\n    tl::expected and std::expected are interchangeable; the monadic\n"
                                         "    operations are where they diverge. Using one forfeits the one-line\n    migration D7 was chosen for. Write it with an if and an early return.");
                }
            }
        }

        for (const std::string& violation : violations) io.err << "VIOLATION  " << violation << "\n\n";
        if (!violations.empty()) {
            io.err << violations.size() << " violation(s) of plan \xC2\xA7" "5 constraint 8 in " << sources.size() << " files\n";
            return kViolation;
        }
        io.out << "ok       " << sources.size() << " files, no constraint-8 violation (no monadic use, underlying type named only in the alias header)\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::constraint8

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::constraint8::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
