// one_secular_pole.cpp — SPEC-perturbations PERT-A-009, the half of it that is a property of the SOURCE rather than of a running program.
//
// Plan L0 step 6's test, ported to C++ in plan L0 step 8 (group C5, the user's directive of 2026-10-06) from tests/test_one_secular_pole.py.
//
// GRAV-R-029 and the manager's binding condition on GRAV-Q-005: the secular pole is defined ONCE, in `gravity`, and L2 step 3's pole tide CONSUMES that definition rather than restating it.  *Two
// definitions is how the static field ends up secular and the pole tide ends up mean.*  The runtime half -- that perturbing gravity's definition moves the pole tide -- is in the C++ tests.  This is the
// other half: the four constants of TN36-7 (21), xs = 55.0 + 1.677 (t - 2000), ys = 320.5 + 3.460 (t - 2000), mas, appear as ONE definition in exactly one file.
//
// PRODUCTION SOURCES ONLY.  A test that asserts the four constants is the opposite of a second definition (GRAV-A-028 does exactly that, on purpose), and a test happens to use 55.0 as an orbital
// inclination.  The rule is about code that COMPUTES the pole, not about code that checks it.
//
// THIS GUARD HAS BEEN NARROWED TWICE AND WAS REPLACED INSTEAD OF NARROWED A THIRD TIME.  It first matched a bare `55.0` anywhere (a test used it as an inclination, so the test directory was
// excluded); then a bare `55.0` in production (`msis_thermosphere.hpp` carries it as NRLMSISE-00's ZN2 mesospheric spline node, 55 km, so it required two of the four in one file).  Each narrowing
// was locally right and the trend is not: A GUARD THAT ANSWERS EVERY FALSE POSITIVE BY LOWERING ITS OWN SENSITIVITY CONVERGES ON A GUARD THAT FIRES AT NOTHING.  So the DISCRIMINATOR changed, not the
// threshold.  TN36-7 (21) is two expressions, and a copy of either puts its offset and its rate NEXT TO EACH OTHER; a coincidence scatters them.  ADJACENCY is the property that separates the two:
// two of the four within WINDOW lines of each other is an equation, not an accident.
//
//   one_secular_pole [--root DIR]
//   exit 0 the secular pole has exactly one definition   1 it has not, or the definition is gone   2 an argument error, a source that is not UTF-8, or nothing to search   70 an error the program
//   did not anticipate
//
// THE PROOF.  This test prints only when it fails or into ctest's log of a pass, so no committed record holds its output; the committed record is that ctest PASSED it at each of the ten commits whose
// ci runs are kept with the L0-8 report.  The port passes on an export of each of those ten commits, and its output on the real tree equals the text derived BY HAND from the Python's statements and an
// independent grep of the constants, registered before the port existed (C5_proof_registration.txt, section 3).  The cases of one_secular_pole_tests.cpp inject the defect (a second copy within the
// window, the definition missing a constant) and show it fail.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this program was built from.
//   * The Python tested `"build" not in p.parts` and `"tests" not in p.parts` on the ABSOLUTE path: a tree under a directory called `build` or `tests` would have excluded every source.  The path
//     RELATIVE to the root is tested here.
//   * A run with NO module source is REFUSED, exit 2 (by analogy with the maintainer's ruling of 2026-10-07 on budgetcheck, flagged for ratification); a source that is not UTF-8 is REFUSED, exit 2
//     (the Python read strictly and died with a traceback); an entry of the glob that is not a regular file is skipped by rule.
//   * `\b` is Unicode-aware as Python's is, within the limit odl/devkit/pytext.hpp states.  The messages name `one_secular_pole`; argparse's abbreviations are not accepted and `-h` prints this
//     program's own text.  An option's value is whatever follows it unless that begins with `--` (argparse refuses one that begins with a single dash too, so a directory called -x is a value
//     here), and `-h=x` is refused.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "one_secular_pole.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>

namespace odl::tools::one_secular_pole {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "one_secular_pole";
constexpr int kOk = 0, kFailed = 1, kArgument = 2, kInternal = 70;
constexpr std::size_t kNpos = std::string_view::npos;

using Io = dk::Streams;

// TN36-7 (21): xs = 55.0 + 1.677 (t - 2000), ys = 320.5 + 3.460 (t - 2000), mas.  The order is the dictionary's insertion order.
constexpr std::string_view kConstants[] = {"55.0", "1.677", "320.5", "3.460"};

constexpr std::size_t kWindow = 2;        // lines either side; a wrapped expression stays inside this
constexpr std::size_t kMinTogether = 2;   // two of the four adjacent is an equation, not an accident

// re.search(r"\b<literal>\b", text) where the literal's first and last characters are digits: a match that does not follow a word character and is not followed by one
bool holds(std::string_view text, std::string_view literal) {
    for (std::size_t at = text.find(literal); at != kNpos; at = text.find(literal, at + 1)) {
        if (dk::word_boundary_at(text, at) && dk::word_boundary_at(text, at + literal.size())) return true;
    }
    return false;
}

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

std::string joined(const std::vector<std::string>& parts, const char* separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) out += (i != 0 ? separator : "") + parts[i];
    return out;
}

const char kUsageText[] = "usage: one_secular_pole [-h] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "the secular pole is defined once (SPEC-perturbations PERT-A-009, the source half): the four constants of TN36-7 (21) are in gravity's secular_pole.hpp, and no other production source of\n"
    "<root>/modules puts two of them within two lines of each other.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree (default: the tree this program was built from)\n"
    "\n"
    "exit codes: 0 one definition   1 not   2 an argument error, a source that is not UTF-8, or nothing to search\n";

}  // namespace

std::vector<std::string> constants_in(std::string_view text) {
    std::vector<std::string> found;
    for (const std::string_view constant : kConstants) {
        if (holds(text, constant)) found.emplace_back(constant);
    }
    return found;
}

std::vector<Adjacent> adjacent_hits(std::string_view text) {
    const std::vector<std::string_view> lines = split_on_newline(text);
    std::vector<std::vector<std::string>> per_line;
    per_line.reserve(lines.size());
    for (const std::string_view line : lines) per_line.push_back(constants_in(line));
    std::vector<Adjacent> out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::size_t lo = i >= kWindow ? i - kWindow : 0;
        const std::size_t hi = std::min(lines.size(), i + kWindow + 1);
        std::vector<std::string> near;
        for (std::size_t j = lo; j < hi; ++j) {
            for (const std::string& name : per_line[j]) {
                if (std::find(near.begin(), near.end(), name) == near.end()) near.push_back(name);
            }
        }
        if (near.size() >= kMinTogether && (out.empty() || out.back().constants != near)) out.push_back(Adjacent{i + 1, near});
    }
    return out;
}

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
        const fs::path root(root_given);

        // SEARCHED: the .hpp and .cpp files below modules/, none under a `build` or a `tests` directory
        std::vector<std::string> searched;   // paths relative to the root
        for (const std::vector<std::string>& parts : dk::files_below(root / "modules", [](const std::string& name) { return name == "build" || name == "tests"; })) {
            // rglob("*.hpp") and rglob("*.cpp") are fnmatch patterns on the NAME: `*` may be empty, so a file called `.hpp` matches too (it has no pathlib `suffix`, which is why this is not path_suffix)
            const std::string& name = parts.back();
            if (name.ends_with(".hpp") || name.ends_with(".cpp")) searched.push_back("modules/" + joined(parts, "/"));
        }
        if (searched.empty()) {
            io.err << kTool << ": nothing to search: no .hpp or .cpp file under " << (root / "modules").string() << " (outside build and tests directories)\n";
            return kArgument;
        }

        std::map<std::string, std::vector<std::string>> carries;
        std::map<std::string, std::vector<Adjacent>> adjacent;
        for (const std::string& rel : searched) {
            std::string text;
            try {
                text = dk::universal_newlines(dk::read_text(root / rel));   // read strictly, as the Python did
            } catch (const std::runtime_error& exc) {
                io.err << kTool << ": " << exc.what() << '\n';
                return kArgument;
            }
            const std::vector<std::string> found = constants_in(text);
            if (found.empty()) continue;
            carries[rel] = found;
            std::vector<Adjacent> hits = adjacent_hits(text);
            if (!hits.empty()) adjacent[rel] = std::move(hits);
        }

        // the components, printed rather than a verdict
        for (const std::string_view name : kConstants) {
            std::vector<std::string> where;   // sorted: the map is
            for (const auto& [file, got] : carries) {
                if (std::find(got.begin(), got.end(), name) != got.end()) where.push_back(file);
            }
            std::vector<std::string> others;
            for (const std::string& f : where) {
                if (f != kHome) others.push_back(f);
            }
            if (std::find(where.begin(), where.end(), kHome) != where.end()) {
                io.out << "ok       " << dk::pad_right(name, 8) << " in " << kHome << (others.empty() ? std::string() : "   (also, scattered, in " + joined(others, ", ") + ")") << '\n';
            } else {
                io.out << "MISSING  " << name << " is not in " << kHome << '\n';
            }
        }

        const auto home = carries.find(kHome);
        std::vector<std::string> all_four;
        for (const std::string_view name : kConstants) all_four.emplace_back(name);
        if (home == carries.end() || home->second != all_four) {
            io.err << '\n' << kHome << " does not carry all four constants\n";
            return kFailed;
        }
        if (adjacent.find(kHome) == adjacent.end()) {
            io.err << '\n' << kHome << " does not put any pair of them within " << kWindow << " lines, so this test is no longer looking at the definition it was written for\n";
            return kFailed;
        }

        std::map<std::string, std::vector<Adjacent>> copies;
        for (const auto& [file, hits] : adjacent) {
            if (file != kHome) copies[file] = hits;
        }
        const std::size_t scattered = carries.size() - 1;
        io.out << '\n' << searched.size() << " module sources searched. " << scattered << " carry one or more of the four constants incidentally and scattered; " << copies.size() << " put "
               << kMinTogether << " or more within " << kWindow << " lines, which is what a restatement of TN36-7 (21) looks like.\n";
        if (!copies.empty()) {
            for (const auto& [file, hits] : copies) {
                for (const Adjacent& hit : hits) io.err << "FAILED   " << file << ':' << hit.line << " has " << joined(hit.constants, ", ") << " within " << kWindow << " lines\n";
            }
            io.err << "\nThe secular pole is defined once, in gravity, and consumed by the pole tide (GRAV-R-029, PERT-R-006). A second copy is how the two drift apart.\n";
            return kFailed;
        }
        io.out << "the secular pole has exactly one definition\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::one_secular_pole

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::one_secular_pole::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
