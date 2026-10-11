// reprocheck.cpp — prove the build is reproducible, rather than assert it.
//
// Plan L0 step 7, ported to C++ in group C11 of step 8 (the user's directive of 2026-10-06) from tools/reprocheck.py (deleted by the same commit: `git show c03df37:rewrite/tools/reprocheck.py`).  It is ci.sh's
// gate 13, which has been OFF since the user's decision of 2026-10-08 ("Stop CI's Python now", PROVENANCE.md section 41.17) and is back with this tool.
//
// The flags in cmake/OdlReproducible.cmake are a claim; this is the evidence.  The tool builds the tree twice into directories of DIFFERENT NAME LENGTHS and compares the hash of every artefact.  The differing
// lengths matter.  The classic leak is an absolute path landing in debug info or an assert string, and if the two build directories have the same length a leaked path changes the bytes but not their count,
// which several comparison methods will still catch but which a padded or truncated field will not.  Different lengths make a leak change the size as well as the content.
//
// THE PROOF OF THE PORT (registered before any line of this file was written: C11_proof_registration.txt, item 2, in the group's report files).  The Python tool's last record is the ci of C7b (bc7cd6d):
// "880 artefacts compared / ok all 880 artefacts byte-identical across two build paths".  That tree runs an interpreter at every configure (find_package(Python3)), so the port is run on the first commit
// without it (3a7c225), which differs from bc7cd6d in three build files and in no C++ target; C11_proof_result.txt holds what came.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * What the Python ended with a traceback (cmake not installed, an artefact that cannot be read) is a refusal naming the fault, exit 2; an error the tool did not anticipate exits 70.  The Python's exit codes
//     0 (reproducible), 1 (artefacts differ) and 3 (a build failed) are the same.
//   * The options are taken by their exact names (argparse took any unambiguous abbreviation); `-h` prints this tool's own text; `--root DIR` is new (the Python's tree was the one the script stood in; the default
//     here is the tree this tool was built from).  `--build-dir DIR`, which the Python accepted and ignored ("present so ci.sh can pass one uniformly"), is REFUSED by name since group C12 (the maintainer's ruling
//     of 2026-10-11: an option accepted and silently ignored is a silent fallback, plan §5 constraint 4); ci.sh no longer passes it.
//   * The build's own output, held for the failure message, is not decoded: it is passed through as bytes with its line ends made "\n" (the Python's text=True decoded it as UTF-8 and would have raised on a byte
//     that is not).  The root is made absolute and symbolic links in it are resolved (the Python's Path.resolve()), and a trailing slash dropped.
//   * std::filesystem::remove_all removes a symbolic link named like a build directory itself, where shutil.rmtree refused it (ignore_errors) and cmake then built into the link's target.
//   * SOURCE_DATE_EPOCH is the Python's 1789603200, which is 2026-09-17T00:00:00Z: the Python's comment dated it 2026-09-18, a day late.
//   * Two behaviours of the Python were kept in group C11 and changed in group C12, by the maintainer's ruling of 2026-10-11, each with its test: (a1) a pair of builds with NO artefact in either said
//     "0 artefacts compared / ok all 0 artefacts byte-identical" and exited 0; it is REFUSED now, exit 2 ("no artefacts found in either build: nothing compared") -- success over nothing read is the failure the check
//     exists against; (a2) "NOT REPRODUCIBLE: n artefact(s) differ" counted the DIFFERS entries only and said 0 when the only trouble was an artefact one build has and the other lacks; it names both counts now,
//     "n artefact(s) differ, m present in one build only", the exit status (1) unchanged.

#include "reprocheck.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/process.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace dk = odl::devkit;
namespace fs = std::filesystem;
using odl::devkit::Streams;

namespace odl::tools::reprocheck {

namespace {

constexpr int kOk = 0;
constexpr int kDiffers = 1;
constexpr int kArgument = 2;
constexpr int kBuildFailed = 3;
constexpr int kInternal = 70;

static_assert(sizeof(kDirA) != sizeof(kDirB), "the two build directories must have names of different lengths");

/// Thrown by build() after it has said why.
struct BuildFailed {};

std::string join(const std::vector<std::string>& words, char separator) {
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i != 0) out += separator;
        out += words.at(i);
    }
    return out;
}

/// st_mode & 0o111 of what the path leads to (stat follows a link, as Path.stat() does).
bool has_an_execute_bit(const fs::path& file) {
    std::error_code error;
    const fs::perms permissions = fs::status(file, error).permissions();
    if (error) throw std::runtime_error("cannot examine " + file.string() + ": " + error.message());
    return (permissions & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none;
}

/// The two commands of one build, in the environment of the parent with a pinned clock, locale and time zone.
void build(const fs::path& root, const fs::path& out, Streams& io) {
    dk::ProcessOptions options;
    options.env = {{"SOURCE_DATE_EPOCH", kSourceDateEpoch}, {"LC_ALL", "C"}, {"TZ", "UTC"}};
    const std::vector<std::vector<std::string>> commands = {{"cmake", "-S", root.string(), "-B", out.string(), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo"},
                                                            {"cmake", "--build", out.string()}};
    for (const std::vector<std::string>& command : commands) {
        const dk::ProcessResult result = dk::run_process(command, options);
        if (result.exit_code != 0) {
            io.err << kTool << ": build failed: " << join(command, ' ') << '\n' << dk::universal_newlines(result.out) << '\n' << dk::universal_newlines(result.err) << '\n';
            throw BuildFailed{};
        }
    }
}

struct Difference {
    std::string rel;
    std::string hash_a;
    std::string hash_b;
    std::uintmax_t size_a;
    std::uintmax_t size_b;
};

void remove_tree(const fs::path& directory) {
    std::error_code ignored;   // shutil.rmtree(d, ignore_errors=True)
    fs::remove_all(directory, ignored);
}

int reproduce(const fs::path& root, bool keep, Streams& io) {
    const fs::path a = root / kDirA;
    const fs::path b = root / kDirB;
    remove_tree(a);
    remove_tree(b);

    io.out << "building twice (SOURCE_DATE_EPOCH=" << kSourceDateEpoch << ")\n";
    io.out << "  A  " << a.string() << '\n';
    io.out << "  B  " << b.string() << "   <- " << (b.string().size() - a.string().size()) << " characters longer\n" << std::flush;
    build(root, a, io);
    build(root, b, io);

    const std::map<std::string, fs::path> found_a = artefacts(a);
    const std::map<std::string, fs::path> found_b = artefacts(b);
    if (found_a.empty() && found_b.empty()) {
        // Nothing was read, so nothing is proved: the Python said "0 artefacts compared / ok" here, which is the failure a check like this exists against (the maintainer's ruling of 2026-10-11).  A pair where only
        // one build has artefacts is not this: that is the ordinary failure below (the ONLY IN lines, exit 1).
        if (!keep) {
            remove_tree(a);
            remove_tree(b);
        }
        io.err << kTool << ": no artefacts found in either build: nothing compared\n";
        return kArgument;
    }
    std::vector<std::string> only_a;
    std::vector<std::string> only_b;
    std::vector<std::string> common;
    for (const auto& entry : found_a) {
        if (found_b.count(entry.first) != 0) {
            common.push_back(entry.first);
        } else {
            only_a.push_back(entry.first);
        }
    }
    for (const auto& entry : found_b) {
        if (found_a.count(entry.first) == 0) only_b.push_back(entry.first);
    }

    std::vector<Difference> differing;
    for (const std::string& rel : common) {
        const fs::path& file_a = found_a.at(rel);
        const fs::path& file_b = found_b.at(rel);
        std::string hash_a = dk::sha256_file_hex(file_a);
        std::string hash_b = dk::sha256_file_hex(file_b);
        if (hash_a != hash_b) differing.push_back(Difference{rel, std::move(hash_a), std::move(hash_b), fs::file_size(file_a), fs::file_size(file_b)});
    }

    io.out << '\n' << common.size() << " artefacts compared\n";
    for (const Difference& d : differing) {
        io.err << "  DIFFERS  " << d.rel << "\n    A " << d.hash_a << "  " << d.size_a << " bytes\n    B " << d.hash_b << "  " << d.size_b << " bytes";
        if (d.size_a != d.size_b) io.err << "\n    (sizes differ too \xE2\x80\x94 a path length is leaking into the artefact)";
        io.err << '\n';
    }
    for (const std::string& rel : only_a) io.err << "  ONLY IN A  " << rel << '\n';
    for (const std::string& rel : only_b) io.err << "  ONLY IN B  " << rel << '\n';

    if (!keep) {
        remove_tree(a);
        remove_tree(b);
    }

    if (!differing.empty() || !only_a.empty() || !only_b.empty()) {
        io.err << "\nNOT REPRODUCIBLE: " << differing.size() << " artefact(s) differ, " << only_a.size() + only_b.size() << " present in one build only, between two builds of identical source.\n";
        return kDiffers;
    }
    io.out << "ok       all " << common.size() << " artefacts byte-identical across two build paths\n";
    return kOk;
}

/// What Path(root).resolve() and str() give: absolute, symbolic links resolved where the path exists, no trailing slash.
fs::path tidy_root(const fs::path& given) {
    std::error_code error;
    fs::path root = fs::weakly_canonical(fs::absolute(given, error), error);
    if (error) root = fs::absolute(given).lexically_normal();
    if (!root.has_filename() && root != root.root_path()) root = root.parent_path();
    return root;
}

const char kUsageText[] = "usage: reprocheck [-h] [--root ROOT] [--keep]\n";

const char kHelpText[] =
    "\n"
    "Prove the build is reproducible, rather than assert it (ci.sh's gate 13; plan L0 step 7).  The flags in cmake/OdlReproducible.cmake are a claim; this is the evidence.  It builds the tree twice, with cmake\n"
    "and Ninja, into two directories of DIFFERENT NAME LENGTHS (build-repro-a and build-repro-bbbbbbbb, below the tree) and compares the SHA-256 of every artefact: a file named *.a, *.so or *.o, or an executable\n"
    "with no suffix whose path has no component named CMakeFiles.  The lengths differ so that a leaked path changes an artefact's size as well as its bytes.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree to build twice (default: the tree this tool was built from)\n"
    "  --keep       do not delete the two build trees\n"
    "\n"
    "exit codes: 0 reproducible   1 artefacts differ   3 a build failed   2 an argument error, or cmake cannot be run, or an artefact cannot be read, or no artefact was found in either build   70 an error the tool\n"
    "            did not anticipate\n";

}  // namespace

std::map<std::string, fs::path> artefacts(const fs::path& out) {
    std::map<std::string, fs::path> found;
    for (const std::vector<std::string>& parts : dk::files_below(out)) {
        const std::string rel = join(parts, '/');
        const fs::path file = out / rel;
        const std::string suffix = dk::path_suffix(parts.back());
        bool artefact = suffix == ".a" || suffix == ".so" || suffix == ".o";
        if (!artefact && suffix.empty() && has_an_execute_bit(file)) {
            artefact = true;
            for (const std::string& part : parts) {
                if (part == "CMakeFiles") artefact = false;
            }
        }
        if (artefact) found.emplace(rel, file);
    }
    return found;
}

fs::path default_root() {
#ifdef ODL_TREE_ROOT
    return fs::path(ODL_TREE_ROOT);
#else
    return fs::current_path();
#endif
}

Settings default_settings() {
    Settings settings;
    settings.root = default_root();
    return settings;
}

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        fs::path root = settings.root;
        bool keep = false;
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
            if (opt == "--build-dir") {
                // The Python accepted this and ignored it, "present so ci.sh can pass one uniformly".  An option that is accepted and does nothing is a silent fallback (plan §5 constraint 4): it is refused, in every form, by name.
                return usage_error(std::string("argument --build-dir: this option is not used (the builds go to ") + kDirA + " and " + kDirB + " below the tree)");
            }
            if (opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (value.empty()) return usage_error("argument --root: the path is empty");   // it would be the current directory, built twice without anyone having said so
                root = value;
            } else if (opt == "--keep" && !has_value) {
                keep = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        try {
            return reproduce(tidy_root(root), keep, io);
        } catch (const BuildFailed&) {
            return kBuildFailed;
        } catch (const std::runtime_error& exc) {   // dk::run_process (cmake cannot be run), sha256_file_hex, file_size: the path and the reason
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::reprocheck

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::reprocheck::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
