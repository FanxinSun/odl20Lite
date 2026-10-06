// configure_is_current.cpp — a stale configure tests a subset and reports 100%.
//
// Plan L0 step 6's test, ported to C++ in plan L0 step 8 (group C5, the user's directive of 2026-10-06) from tests/test_configure_is_current.py.
//
// The manager's first check of L2 step 3 reported "100% tests passed, 165 of 165" from a build directory that had never been configured with `tides`, `relativity` or `thirdbody` in the tree.  A
// fresh configure gives 194.  That is the unstated denominator again -- a count of passed tests with no statement of how many there should be -- and it is the one that would have certified a step on a
// suite that did not contain it.
//
// WHY THIS IS A TEST AND NOT A LINE IN ci.sh.  The first attempt put the check in `tools/ci.sh` just before ctest.  It could never fire: ci.sh CONFIGURES at gate 3 and then tests at gate 5, so by
// the time the check ran the build system was always newer than the CMakeLists.  Dead code that reads like protection.  The failure mode is somebody running ctest -- or a single test binary --
// against an old build directory, and the only place that can be caught is inside the suite.
//
//   configure_is_current BUILD_DIR [--root DIR]
//   exit 0 the configure is current   1 it is stale, or BUILD_DIR is no build directory   2 an argument error   70 an error the program did not anticipate
//
// THE PROOF.  This test prints only into ctest's log, so no committed record holds its output; the committed record is that ctest PASSED it in every green ci run kept with the L0-8 report (the
// build directories of those runs are fresh configures).  The cases of configure_is_current_tests.cpp make a build directory and a tree with files of chosen ages and show the check fail on a
// CMakeLists.txt newer than the generated build system, in a subdirectory too, and pass when none is.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this program was built from.  The build directory is the first argument, as it was.
//   * The Python tested `"build" not in p.parts` and `"_deps" not in p.parts` on the ABSOLUTE path: a tree under a directory called `build` would have listed nothing and passed.  The path RELATIVE to
//     the root is tested here.  A tree with NO CMakeLists.txt is REFUSED, exit 2 (by analogy with the maintainer's ruling of 2026-10-07 on budgetcheck, flagged for ratification).
//   * The times are compared as Python compared them: st_mtime, the seconds as a double (the whole seconds plus the nanoseconds times 1e-9), with the strict `>`; a link is followed (stat, not lstat).
//     The build directory is named in a message as pathlib spells it (`a//b/./c/` is `a/b/c`, the empty argument is `.`), as the Python's `{build}` printed it.
//   * A match that is not a regular file (a directory called CMakeLists.txt, a link to nothing) is skipped by rule.  The messages name the program; argparse's abbreviations are not accepted and `-h`
//     prints this program's own text.  An option's value is whatever follows it unless that begins with `--` (argparse refuses one that begins with a single dash too, so a directory called -x is a
//     value here), and `-h=x` is refused; a lone `-` is a name, as there.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "configure_is_current.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace odl::tools::configure_is_current {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "configure_is_current";
constexpr int kOk = 0, kStale = 1, kArgument = 2, kInternal = 70;

using Io = dk::Streams;

// Path.stat().st_mtime: the seconds as a float, from the whole seconds and the nanoseconds; false when the path cannot be stat-ed
bool modified(const fs::path& path, double& seconds) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) return false;
    seconds = static_cast<double>(st.st_mtim.tv_sec) + static_cast<double>(st.st_mtim.tv_nsec) * 1e-9;
    return true;
}

// str(Path(text)): pathlib's spelling of a path, which is what the Python's messages print -- repeated slashes and `.` components dropped, no trailing slash, "." for the empty path (which then
// names the current directory); exactly two leading slashes are kept (POSIX lets them differ from one), three or more are one.  `..` is kept, as pathlib keeps it.
std::string pathlib_spelling(const std::string& text) {
    std::size_t lead = 0;
    while (lead < text.size() && text[lead] == '/') ++lead;
    std::string out = lead == 2 ? "//" : (lead >= 1 ? "/" : "");
    bool first = true;
    for (std::size_t i = lead; i < text.size();) {
        const std::size_t slash = text.find('/', i);
        const std::size_t end = slash == std::string::npos ? text.size() : slash;
        const std::string part = text.substr(i, end - i);
        if (!part.empty() && part != ".") {
            if (!first) out += '/';
            out += part;
            first = false;
        }
        i = end + 1;
    }
    return out.empty() ? "." : out;
}

const char kUsageText[] = "usage: configure_is_current [-h] [--root ROOT] BUILD_DIR\n";

const char kHelpText[] =
    "\n"
    "a stale configure tests a subset and reports 100%: no CMakeLists.txt of the tree (outside build and _deps directories) may be newer than the generated build system (build.ninja, or Makefile)\n"
    "of BUILD_DIR.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree (default: the tree this program was built from)\n"
    "\n"
    "exit codes: 0 the configure is current   1 it is stale, or BUILD_DIR is no build directory   2 an argument error\n";

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
        std::vector<std::string> positional;
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
            } else if (opt.rfind("-", 0) == 0 && opt != "-") {
                return usage_error("unrecognized arguments: " + argv[i]);
            } else {
                positional.push_back(argv[i]);
            }
        }
        if (positional.size() > 1) return usage_error("unrecognized arguments: " + positional[1]);

        // build = Path(sys.argv[1]) if len(sys.argv) > 1 else None
        if (positional.empty()) {
            io.err << "no build directory given or it does not exist: None\n";
            return kStale;
        }
        const std::string build_text = pathlib_spelling(positional[0]);
        const fs::path build(build_text);
        std::error_code ec;
        if (!fs::exists(build, ec)) {
            io.err << "no build directory given or it does not exist: " << build_text << '\n';
            return kStale;
        }
        // generated = build / "build.ninja", else build / "Makefile" -- the one that exists (stat follows a link and fails on what is absent) -- and its time
        double stamp = 0.0;
        fs::path generated = build / "build.ninja";
        if (!modified(generated, stamp)) {
            generated = build / "Makefile";
            if (!modified(generated, stamp)) {
                io.err << "neither build.ninja nor Makefile in " << build_text << '\n';
                return kStale;
            }
        }
        // lists = sorted(p for p in ROOT.rglob("CMakeLists.txt") if "build" not in p.parts and "_deps" not in p.parts)
        const fs::path root(root_given);
        std::vector<std::vector<std::string>> lists;
        for (const std::vector<std::string>& parts : dk::files_below(root, [](const std::string& name) { return name == "build" || name == "_deps"; })) {
            if (parts.back() == "CMakeLists.txt") lists.push_back(parts);
        }
        if (lists.empty()) {
            io.err << kTool << ": nothing to check: no CMakeLists.txt under " << root.string() << '\n';
            return kArgument;
        }
        std::vector<std::string> newer;
        for (const std::vector<std::string>& parts : lists) {
            std::string relative;
            fs::path path = root;
            for (const std::string& part : parts) {
                relative += (relative.empty() ? "" : "/") + part;
                path /= part;
            }
            double seconds = 0.0;
            if (!modified(path, seconds)) {
                io.err << kTool << ": cannot read the time of " << path.string() << '\n';
                return kArgument;
            }
            if (seconds > stamp) newer.push_back(relative);
        }

        io.out << lists.size() << " CMakeLists.txt files, checked against " << generated.filename().string() << '\n';
        if (!newer.empty()) {
            io.err << "STALE CONFIGURE \xE2\x80\x94 this suite may be a subset of the tree.\n";
            for (const std::string& p : newer) io.err << "  newer than the build system: " << p << '\n';
            io.err << "  A build directory configured before a module existed tests every OTHER\n"
                   << "  module and reports 100%. Re-run cmake.\n";
            return kStale;
        }
        io.out << "ok       the configure is current; the suite is the whole tree\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

}  // namespace odl::tools::configure_is_current

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::configure_is_current::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
