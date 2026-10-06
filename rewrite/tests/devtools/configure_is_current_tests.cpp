// tests/devtools/configure_is_current_tests.cpp — "a stale configure tests a subset and reports 100%": the check that no CMakeLists.txt of the tree is newer than the build system the build directory
// was generated with.  Files of CHOSEN AGES (set with utimensat, in fixed seconds, so that nothing depends on the clock or on the file system's rounding), what is printed and refused, which files
// are looked at, and the real tree.  (ctests `configure_is_current.behaviour` and `configure_is_current.real_tree`; the ctest `build.configure_is_current` runs the program on the real build
// directory; the Python test had no test of its own, so these are the cases that show it CAN fail.)
//
// Every expectation is DERIVED BY HAND from test_configure_is_current.py's statements, not taken from running the port.  Where the port deliberately differs (the path RELATIVE to the root is tested
// for `build` and `_deps`; a tree with no CMakeLists.txt is refused with exit 2; an entry that is no regular file is skipped) the case says so.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/tool.hpp>

#include "configure_is_current.hpp"
#include "throwing_stream.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <ctime>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
namespace cic = odl::tools::configure_is_current;

namespace {

constexpr int kOk = 0, kStale = 1, kArgument = 2;
constexpr time_t kT = 1'700'000'000;          // the build system's time in most cases (2023-11-14)
constexpr time_t kFuture = 2'100'000'000;     // 2036: later than any real file here, and below the year-2038 limit of the oldest file systems

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_tool(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = cic::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

// the modification time of a file (the one a link points to, unless `follow` is false), to the nanosecond
void set_mtime(const fs::path& path, time_t seconds, long nanoseconds = 0, bool follow = true) {
    struct timespec times[2] = {};
    times[0].tv_nsec = UTIME_OMIT;   // the access time is left alone
    times[1].tv_sec = seconds;
    times[1].tv_nsec = nanoseconds;
    REQUIRE(::utimensat(AT_FDCWD, path.c_str(), times, follow ? 0 : AT_SYMLINK_NOFOLLOW) == 0);
}

struct Fixture {
    TempDir td;
    fs::path root;    // the tree
    fs::path build;   // the build directory, beside it
    explicit Fixture(const std::string& below = "tree") : root(td.path() / below), build(td.path() / "b") {
        fs::create_directories(root);
        fs::create_directories(build);
    }
    void cmake(const std::string& relative, time_t seconds, long nanoseconds = 0) {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, "# a build file\n");
        set_mtime(root / relative, seconds, nanoseconds);
    }
    void generated(const std::string& name, time_t seconds, long nanoseconds = 0) {
        write_text(build / name, "# generated\n");
        set_mtime(build / name, seconds, nanoseconds);
    }
    [[nodiscard]] Result run_with(const std::string& build_argument, std::vector<std::string> extra = {}) const {
        std::vector<std::string> args = {build_argument, "--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        return run_tool(args);
    }
    [[nodiscard]] Result run(std::vector<std::string> extra = {}) const { return run_with(build.string(), std::move(extra)); }
};

std::string checked(std::size_t count, const std::string& against = "build.ninja") { return std::to_string(count) + " CMakeLists.txt files, checked against " + against + "\n"; }
const char kOkLine[] = "ok       the configure is current; the suite is the whole tree\n";

std::string stale(const std::vector<std::string>& newer) {
    std::string out = "STALE CONFIGURE \xE2\x80\x94 this suite may be a subset of the tree.\n";
    for (const std::string& p : newer) out += "  newer than the build system: " + p + "\n";
    return out + "  A build directory configured before a module existed tests every OTHER\n  module and reports 100%. Re-run cmake.\n";
}

}  // namespace

// ======================================================================================================================================== what is printed

TEST_CASE("a configure that is current passes: no CMakeLists.txt is newer than the generated build system, and the same age is not newer", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    f.cmake("CMakeLists.txt", kT - 100);
    f.cmake("modules/a/CMakeLists.txt", kT - 1);
    f.cmake("tests/CMakeLists.txt", kT);                   // exactly as old: not newer
    f.cmake("cmake/deeper/still/CMakeLists.txt", kT - 5);
    const Result r = f.run();
    CHECK(r.code == kOk);
    CHECK(r.out == checked(4) + kOkLine);
    CHECK(r.err.empty());
}

TEST_CASE("a CMakeLists.txt newer than the build system is a STALE CONFIGURE: exit 1, the count on stdout, the files on stderr in pathlib's order, the explanation", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    f.cmake("CMakeLists.txt", kT + 1);                      // the root's, newer by a second
    f.cmake("modules/b/CMakeLists.txt", kT + 3600);
    f.cmake("modules/a/CMakeLists.txt", kT - 1);            // older: not listed
    f.cmake("modules/a/deeper/CMakeLists.txt", kT + 5);
    f.cmake("tests/CMakeLists.txt", kT);                    // equal: not listed
    const Result r = f.run();
    CHECK(r.code == kStale);
    CHECK(r.out == checked(5));
    // sorted as paths are: the root's first (an upper-case C sorts before a lower-case m), then modules/a/deeper before modules/b
    CHECK(r.err == stale({"CMakeLists.txt", "modules/a/deeper/CMakeLists.txt", "modules/b/CMakeLists.txt"}));
}

TEST_CASE("times are compared as the Python compared them: seconds as a double, strictly later wins, half a second counts", "[configure_is_current][behaviour]") {
    {
        Fixture f;
        f.generated("build.ninja", kT);
        f.cmake("CMakeLists.txt", kT, 500'000'000);     // half a second later
        const Result r = f.run();
        CHECK(r.code == kStale);
        CHECK(r.err == stale({"CMakeLists.txt"}));
    }
    {
        Fixture f;
        f.generated("build.ninja", kT, 500'000'000);
        f.cmake("CMakeLists.txt", kT, 499'000'000);     // a millisecond earlier
        CHECK(f.run().code == kOk);
    }
    {
        Fixture f;
        f.generated("build.ninja", kT, 250'000'000);
        f.cmake("CMakeLists.txt", kT, 250'000'000);     // the very same instant
        CHECK(f.run().code == kOk);
    }
    {
        Fixture f;   // a whole second later than a fraction of one: nanoseconds are billionths of a second
        f.generated("build.ninja", kT, 900'000'000);
        f.cmake("CMakeLists.txt", kT + 1, 0);
        CHECK(f.run().code == kStale);
    }
    {
        Fixture f;
        f.generated("build.ninja", kT + 1, 0);
        f.cmake("CMakeLists.txt", kT, 900'000'000);   // a tenth of a second older
        CHECK(f.run().code == kOk);
    }
    {
        // one nanosecond later: at 1.7e9 seconds a double resolves 2.4e-7 seconds, so both times are the same double and the check passes, as the Python's did -- a blind spot of the original's
        // float comparison (a file touched within about 120 nanoseconds of the generation cannot be told from it), kept because the port compares what the Python compared
        Fixture f;
        f.generated("build.ninja", kT, 999'999'999);
        f.cmake("CMakeLists.txt", kT + 1, 0);
        CHECK(f.run().code == kOk);
    }
}

TEST_CASE("the build system is build.ninja, or Makefile when there is none; build.ninja is the one used when both exist", "[configure_is_current][behaviour]") {
    {
        Fixture f;
        f.generated("Makefile", kT);
        f.cmake("CMakeLists.txt", kT - 10);
        const Result r = f.run();
        CHECK(r.code == kOk);
        CHECK(r.out == checked(1, "Makefile") + kOkLine);
    }
    {
        Fixture f;
        f.generated("build.ninja", kT - 100);   // the one that counts: older than the CMakeLists.txt
        f.generated("Makefile", kT + 100);      // newer than it, and ignored
        f.cmake("CMakeLists.txt", kT);
        const Result r = f.run();
        CHECK(r.code == kStale);
        CHECK(r.out == checked(1));
        CHECK(r.err == stale({"CMakeLists.txt"}));
    }
}

// ======================================================================================================================================== which files are looked at

TEST_CASE("the CMakeLists.txt files looked at: every one below the root except below a directory called build or _deps; the whole name must be CMakeLists.txt", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    const time_t newer = kT + 1000;
    // looked at (all newer, so the listing shows exactly which)
    for (const char* p : {"CMakeLists.txt", "a/CMakeLists.txt", "xbuild/CMakeLists.txt", "build2/CMakeLists.txt", "_deps2/CMakeLists.txt", "a/_dep/CMakeLists.txt", "Build/CMakeLists.txt"}) f.cmake(p, newer);
    // not looked at
    for (const char* p : {"build/CMakeLists.txt", "a/build/CMakeLists.txt", "a/b/build/c/CMakeLists.txt", "_deps/CMakeLists.txt", "a/_deps/x/CMakeLists.txt", "build/_deps/CMakeLists.txt", "CMakeLists.txt.in",
                          "CMakeLists.txt~", "cmakelists.txt", "a/CMakeLists.TXT", "a/CMakeLists", "a/MyCMakeLists.txt", "a/CMakeLists.txt.bak"}) {
        f.cmake(p, newer);
    }
    const Result r = f.run();
    CHECK(r.code == kStale);
    CHECK(r.out == checked(7));
    CHECK(r.err == stale({"Build/CMakeLists.txt", "CMakeLists.txt", "_deps2/CMakeLists.txt", "a/CMakeLists.txt", "a/_dep/CMakeLists.txt", "build2/CMakeLists.txt", "xbuild/CMakeLists.txt"}));
}

TEST_CASE("an entry that is no regular file is skipped by rule; a link to a file is followed (the time of the file it points to counts, not the link's own); a link to a directory is not entered", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    f.cmake("CMakeLists.txt", kT - 100);
    std::error_code ec;
    // a directory with the name, newer than the build system: skipped
    fs::create_directories(f.root / "dir/CMakeLists.txt");
    set_mtime(f.root / "dir/CMakeLists.txt", kT + 5000);
    // a link to nothing: skipped (the Python's stat() on it raised)
    fs::create_directories(f.root / "dangling");
    fs::create_symlink(f.root / "nowhere", f.root / "dangling/CMakeLists.txt", ec);
    REQUIRE_FALSE(ec);
    // a link to an OLD file: read through, and the file's time counts -- the link itself is as new as the moment it was made, long after kT, which `lstat` would have reported
    f.cmake("real/CMakeLists.txt", kT - 50);
    fs::create_directories(f.root / "linked");
    fs::create_symlink(f.root / "real/CMakeLists.txt", f.root / "linked/CMakeLists.txt", ec);
    REQUIRE_FALSE(ec);
    struct stat own {};
    REQUIRE(::lstat((f.root / "linked/CMakeLists.txt").c_str(), &own) == 0);
    REQUIRE(own.st_mtime > kT);
    // a link to a directory that holds a NEWER CMakeLists.txt, outside the tree: not entered
    fs::create_directories(f.td.path() / "outside");
    write_text(f.td.path() / "outside/CMakeLists.txt", "# outside the tree\n");
    set_mtime(f.td.path() / "outside/CMakeLists.txt", kT + 9000);
    fs::create_directory_symlink(f.td.path() / "outside", f.root / "via_link", ec);
    REQUIRE_FALSE(ec);
    const Result r = f.run();
    CHECK(r.code == kOk);
    CHECK(r.out == checked(3) + kOkLine);   // CMakeLists.txt, real/CMakeLists.txt and linked/CMakeLists.txt (the link, read through)
    CHECK(r.err.empty());
    // and a link whose file IS newer is stale, by the file's time: the file, and the link to it
    set_mtime(f.root / "real/CMakeLists.txt", kT + 1);
    const Result rs = f.run();
    CHECK(rs.code == kStale);
    CHECK(rs.out == checked(3));
    CHECK(rs.err == stale({"linked/CMakeLists.txt", "real/CMakeLists.txt"}));
}

TEST_CASE("a tree under directories called build and _deps is looked at all the same (the Python tested the absolute path, and found nothing in such a tree)", "[configure_is_current][behaviour]") {
    Fixture f("build/_deps/tree");
    f.generated("build.ninja", kT);
    f.cmake("CMakeLists.txt", kT + 1);
    f.cmake("modules/a/CMakeLists.txt", kT - 1);
    f.cmake("build/skipped/CMakeLists.txt", kT + 1);   // below a build directory of the TREE: still skipped
    const Result r = f.run();
    CHECK(r.code == kStale);
    CHECK(r.out == checked(2));
    CHECK(r.err == stale({"CMakeLists.txt"}));
}

TEST_CASE("a tree with no CMakeLists.txt is REFUSED, exit 2, naming the root: a configure cannot be current against nothing", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    f.cmake("build/CMakeLists.txt", kT);   // below a build directory: not looked at
    const Result r = f.run();
    CHECK(r.code == kArgument);
    CHECK(r.out.empty());
    CHECK(r.err == "configure_is_current: nothing to check: no CMakeLists.txt under " + f.root.string() + "\n");
}

// ======================================================================================================================================== the build directory

TEST_CASE("no build directory, one that does not exist, one with no generated build system: each is refused, exit 1, with the Python's message", "[configure_is_current][behaviour]") {
    Fixture f;
    f.cmake("CMakeLists.txt", kT);
    // no argument at all: `build = None`
    const Result none = run_tool({"--root", f.root.string()});
    CHECK(none.code == kStale);
    CHECK(none.out.empty());
    CHECK(none.err == "no build directory given or it does not exist: None\n");
    // a directory that is not there
    const Result gone = f.run_with((f.td.path() / "missing").string());
    CHECK(gone.code == kStale);
    CHECK(gone.err == "no build directory given or it does not exist: " + (f.td.path() / "missing").string() + "\n");
    // there, and empty
    const Result empty = f.run();
    CHECK(empty.code == kStale);
    CHECK(empty.out.empty());
    CHECK(empty.err == "neither build.ninja nor Makefile in " + f.build.string() + "\n");
    // a file where a directory belongs: it exists, and holds no build system
    write_text(f.td.path() / "afile", "x\n");
    const Result file = f.run_with((f.td.path() / "afile").string());
    CHECK(file.code == kStale);
    CHECK(file.err == "neither build.ninja nor Makefile in " + (f.td.path() / "afile").string() + "\n");
}

TEST_CASE("the build directory is named in a message as pathlib spells it: no trailing slash, no repeated slash, no . component; the empty argument is the current directory", "[configure_is_current][behaviour]") {
    Fixture f;
    f.cmake("CMakeLists.txt", kT);
    const std::string base = f.td.path().string();
    CHECK(f.run_with(base + "/missing/").err == "no build directory given or it does not exist: " + base + "/missing\n");
    CHECK(f.run_with(base + "//missing///x").err == "no build directory given or it does not exist: " + base + "/missing/x\n");
    CHECK(f.run_with(base + "/./missing/./x/.").err == "no build directory given or it does not exist: " + base + "/missing/x\n");
    CHECK(f.run_with(base + "/missing/../x").err == "no build directory given or it does not exist: " + base + "/missing/../x\n");   // `..` is kept
    CHECK(f.run_with("//no-such-dir-here").err == "no build directory given or it does not exist: //no-such-dir-here\n");           // exactly two leading slashes are kept
    CHECK(f.run_with("///no-such-dir-here").err == "no build directory given or it does not exist: /no-such-dir-here\n");           // three are one
    CHECK(f.run_with("no-such-dir-here/./").err == "no build directory given or it does not exist: no-such-dir-here\n");
    // a real directory spelt oddly is found all the same
    f.generated("build.ninja", kT);
    CHECK(f.run_with(f.build.string() + "//").code == kOk);
    CHECK(f.run_with(f.build.string() + "/./").code == kOk);
    // the empty argument is `Path("")`, which is "." -- the current directory: run from a directory that holds no build system, so that the message says where it looked
    struct WorkingDirectory {
        fs::path saved = fs::current_path();
        ~WorkingDirectory() {
            std::error_code ec;
            fs::current_path(saved, ec);
        }
    } restore;
    Fixture g;
    fs::current_path(g.build);
    CHECK(g.run_with("").err == "neither build.ninja nor Makefile in .\n");
    CHECK(g.run_with(".").err == "neither build.ninja nor Makefile in .\n");
    CHECK(g.run_with("./").err == "neither build.ninja nor Makefile in .\n");
    CHECK(g.run_with("././").err == "neither build.ninja nor Makefile in .\n");
}

TEST_CASE("an error the tool did not anticipate is reported as such, exit 70, and not as a stale or a current configure", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    f.cmake("CMakeLists.txt", kT - 1);
    odl::devtools_testing::ThrowingStream out;   // every write to it throws: the count of the CMakeLists.txt files cannot be printed
    std::ostringstream err;
    const int code = cic::run({f.build.string(), "--root", f.root.string()}, Streams{out, err});
    CHECK(code == 70);
    CHECK(err.str() == "configure_is_current: internal error: boom\n");
}

// ======================================================================================================================================== the command line

TEST_CASE("the command line: BUILD_DIR first or last, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[configure_is_current][behaviour]") {
    Fixture f;
    f.generated("build.ninja", kT);
    f.cmake("CMakeLists.txt", kT - 1);
    const std::string expected = checked(1) + kOkLine;
    CHECK(run_tool({f.build.string(), "--root", f.root.string()}).out == expected);
    CHECK(run_tool({"--root", f.root.string(), f.build.string()}).out == expected);
    CHECK(run_tool({"--root=" + f.root.string(), f.build.string()}).out == expected);
    const Result help = run_tool({"-h"});
    CHECK(help.code == kOk);
    CHECK(help.out.rfind("usage: configure_is_current [-h] [--root ROOT] BUILD_DIR\n", 0) == 0);
    CHECK(contains(help.out, "options:\n  -h, --help   show this help and exit\n  --root ROOT  the tree (default: the tree this program was built from)\n"));
    CHECK(contains(help.out, "exit codes: 0 the configure is current   1 it is stale, or BUILD_DIR is no build directory   2 an argument error\n"));
    CHECK(run_tool({"--help"}).out == help.out);
    CHECK(help.err.empty());
    CHECK(run_tool({"-h=x"}).code == kArgument);                       // a short option takes no value
    CHECK(run_tool({f.build.string(), "--help"}).out == help.out);
    // an option's value is whatever follows it unless that begins with `--` (so a directory called -x is a value)
    const Result dash = run_tool({f.build.string(), "--root", "-x"});
    CHECK(dash.code == kArgument);
    CHECK(dash.err == "configure_is_current: nothing to check: no CMakeLists.txt under -x\n");
    CHECK(run_tool({"-"}).err == "no build directory given or it does not exist: -\n");   // a lone dash is a name, not an option
    const Result missing = run_tool({"--root"});
    CHECK(missing.code == kArgument);
    CHECK(missing.err == "usage: configure_is_current [-h] [--root ROOT] BUILD_DIR\nconfigure_is_current: error: argument --root: expected one argument\n");
    const Result unknown = run_tool({f.build.string(), "--bogus"});
    CHECK(unknown.code == kArgument);
    CHECK(unknown.err == "usage: configure_is_current [-h] [--root ROOT] BUILD_DIR\nconfigure_is_current: error: unrecognized arguments: --bogus\n");
    const Result two = run_tool({f.build.string(), "extra", "--root", f.root.string()});
    CHECK(two.code == kArgument);
    CHECK(two.err == "usage: configure_is_current [-h] [--root ROOT] BUILD_DIR\nconfigure_is_current: error: unrecognized arguments: extra\n");
    CHECK(run_tool({f.build.string(), "--roo", f.root.string()}).code == kArgument);   // no abbreviations
    CHECK(run_tool({"-x", f.build.string()}).code == kArgument);
}

// ======================================================================================================================================== the real tree

namespace {

// the CMakeLists.txt files of the real tree, counted by the standard directory iterator (a second way): every regular file with that name below the root, not below a directory called build or _deps
std::vector<std::string> real_lists(const fs::path& root) {
    std::vector<std::string> out;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied), end; it != end; ++it) {
        if (it->is_directory() && (it->path().filename() == "build" || it->path().filename() == "_deps")) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file() && it->path().filename() == "CMakeLists.txt") out.push_back(fs::relative(it->path(), root).generic_string());
    }
    return out;
}

}  // namespace

TEST_CASE("the real tree against a build system generated in the future: every CMakeLists.txt is older, the check passes, and the count is the tree's", "[configure_is_current][real_tree]") {
    const fs::path root = cic::default_root();
    const std::vector<std::string> lists = real_lists(root);
    REQUIRE_FALSE(lists.empty());
    TempDir td;
    fs::create_directories(td.path() / "b");
    write_text(td.path() / "b/build.ninja", "# generated\n");
    set_mtime(td.path() / "b/build.ninja", kFuture);
    const Result r = run_tool({(td.path() / "b").string(), "--root", root.string()});
    INFO(r.out << r.err);
    CHECK(r.code == kOk);
    CHECK(r.out == checked(lists.size()) + kOkLine);
}

TEST_CASE("the real tree against a build system generated at the dawn of time: every CMakeLists.txt is newer, and every one is named", "[configure_is_current][real_tree]") {
    const fs::path root = cic::default_root();
    std::vector<std::string> lists = real_lists(root);
    REQUIRE_FALSE(lists.empty());
    TempDir td;
    fs::create_directories(td.path() / "b");
    write_text(td.path() / "b/Makefile", "# generated\n");
    set_mtime(td.path() / "b/Makefile", 1);
    const Result r = run_tool({(td.path() / "b").string(), "--root", root.string()});
    CHECK(r.code == kStale);
    CHECK(r.out == checked(lists.size(), "Makefile"));
    // every file once, and the explanation
    const std::string line = "  newer than the build system: ";
    std::size_t named = 0;
    for (std::size_t at = r.err.find(line); at != std::string::npos; at = r.err.find(line, at + 1)) ++named;
    CHECK(named == lists.size());
    for (const std::string& rel : lists) CHECK(contains(r.err, "  newer than the build system: " + rel + "\n"));
    CHECK(r.err.rfind("STALE CONFIGURE \xE2\x80\x94 this suite may be a subset of the tree.\n", 0) == 0);
    CHECK(contains(r.err, "  A build directory configured before a module existed tests every OTHER\n  module and reports 100%. Re-run cmake.\n"));
}
