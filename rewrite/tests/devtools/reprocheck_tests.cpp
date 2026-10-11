// tests/devtools/reprocheck_tests.cpp — the reproducibility check (plan L0 step 8, group C11; ci.sh's gate 13): what an artefact is, every line the tool says and every exit it takes, through a stand-in `cmake`
// (tests/devtools/fake_cmake.cpp) that "builds" whatever artefacts a recipe lists -- agreeing, differing in content, differing in size, missing from one side, failing -- and two real builds with cmake, Ninja and the
// compiler of a tiny project, once clean and once with the build directory's path compiled into the program.
// (ctests `reprocheck.behaviour`, `reprocheck.real_cmake` and `reprocheck.real_tree`.)
//
// THE [real_cmake] CASES NEED cmake, Ninja AND A C++ COMPILER AT TEST TIME: they build the tiny project with the real tools, the compiler being the one cmake finds (the CXX of the environment, then `c++`).  This is no new
// requirement -- this tree's own build needs the same three, and the workflow installs them -- but it is the one place where a ctest runs a build, so it is said here.  The [behaviour] cases need none of them: they run
// the tool against a stand-in `cmake`.  The one [real_tree] case reads tools/ci.sh of the tree.
//
// Every expectation is DERIVED BY HAND from the statements of reprocheck.py (read from git, never run: `git show c03df37:rewrite/tools/reprocheck.py`), or comes from a SECOND METHOD written here apart from the
// tool (SHA-256 values from coreutils' sha256sum, typed in; a POSIX readdir/lstat/stat walk of a real build tree; the calendar arithmetic of the clock's date; the sort order of the Python's strings written out
// by hand); none is taken from running the port to see what it says.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/tool.hpp>

#include "reprocheck.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;
namespace rc = odl::tools::reprocheck;
using namespace odl::devkit;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_with(const rc::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = rc::run_on(settings, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

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

// where the stand-in is: fakebin/ beside this executable (tools/CMakeLists.txt builds it there), found at run time and not compiled in
fs::path fakebin() { return fs::canonical("/proc/self/exe").parent_path() / "fakebin"; }

std::string sha(const std::string& text) { return sha256_hex(as_bytes(text)); }

// an output buffer that notes in a file, each time it is flushed, that it was: the file the stand-in cmake notes its calls in, so that the order of the two can be read
class NotingBuffer : public std::stringbuf {
public:
    explicit NotingBuffer(fs::path note) : note_(std::move(note)) {}

protected:
    int sync() override {
        std::ofstream(note_, std::ios::app) << "--- flush\n";
        return std::stringbuf::sync();
    }

private:
    fs::path note_;
};

// the three lines the tool says before it builds, for a tree at `root`
std::string header(const fs::path& root) {
    return "building twice (SOURCE_DATE_EPOCH=1789603200)\n  A  " + (root / "build-repro-a").string() + "\n  B  " + (root / "build-repro-bbbbbbbb").string() + "   <- 7 characters longer\n";
}

// the em dash of the note, in UTF-8
const char kSizesNote[] = "    (sizes differ too \xE2\x80\x94 a path length is leaking into the artefact)\n";

// the last line of a run that fails, with a blank line before it: how many artefacts differ and how many one build has and the other lacks -- both said, zero included
std::string verdict(std::size_t differ, std::size_t one_build_only) {
    return "\nNOT REPRODUCIBLE: " + std::to_string(differ) + " artefact(s) differ, " + std::to_string(one_build_only) + " present in one build only, between two builds of identical source.\n";
}

// the refusal of --build-dir, which the tool does not use
const char kBuildDirNotUsed[] = "argument --build-dir: this option is not used (the builds go to build-repro-a and build-repro-bbbbbbbb below the tree)";
const char kUsage[] = "usage: reprocheck [-h] [--root ROOT] [--keep]\n";

// ---- the stand-in cmake, its recipe and its log ---------------------------------------------------------------------------------------------------------------------------------------------------------

struct Call {
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::string mode;
    std::string side;
    std::string existed;
    std::string held;
};

std::vector<Call> read_calls(const fs::path& log) {
    std::vector<Call> calls;
    if (!fs::exists(log)) return calls;
    std::istringstream in(read_text(log));
    std::string line;
    while (std::getline(in, line)) {
        if (line == "--- call") {
            calls.emplace_back();
        } else if (line == "--- end" || calls.empty()) {
            continue;
        } else if (line.rfind("arg ", 0) == 0) {
            calls.back().args.push_back(line.substr(4));
        } else if (line.rfind("env ", 0) == 0) {
            const std::string kv = line.substr(4);
            const std::size_t eq = kv.find('=');
            calls.back().env[kv.substr(0, eq)] = kv.substr(eq + 1);
        } else if (line.rfind("mode ", 0) == 0) {
            calls.back().mode = line.substr(5);
        } else if (line.rfind("side ", 0) == 0) {
            calls.back().side = line.substr(5);
        } else if (line.rfind("directory_existed_before ", 0) == 0) {
            calls.back().existed = line.substr(25);
        } else if (line.rfind("directory_held ", 0) == 0) {
            calls.back().held = line.substr(15);
        }
    }
    return calls;
}

struct Fixture {
    TempDir td{"odl-rc"};
    fs::path base = fs::canonical(td.path());
    fs::path root = base / "tree";
    fs::path bin = base / "bin";
    fs::path log = base / "calls.log";
    fs::path recipe_file = base / "recipe.txt";
    EnvScope env;

    explicit Fixture(const std::string& recipe) {
        fs::create_directories(root);
        fs::create_directories(bin);
        REQUIRE(fs::exists(fakebin() / "cmake"));
        fs::copy_file(fakebin() / "cmake", bin / "cmake");
        write_text(recipe_file, recipe);
        const char* path = std::getenv("PATH");
        env.set("PATH", bin.string() + ":" + (path != nullptr ? path : ""));
        env.set("ODL_FAKE_CMAKE_LOG", log.string());
        env.set("ODL_FAKE_CMAKE_RECIPE", recipe_file.string());
        env.unset("ODL_FAKE_CMAKE_FAIL");
        env.unset("ODL_FAKE_CMAKE_EXIT");
        // values the tool must REPLACE in the build's environment, not merely add to it
        env.set("SOURCE_DATE_EPOCH", "1");
        env.set("LC_ALL", "en_US.UTF-8");
        env.set("TZ", "America/New_York");
    }
    [[nodiscard]] fs::path dir_a() const { return root / "build-repro-a"; }
    [[nodiscard]] fs::path dir_b() const { return root / "build-repro-bbbbbbbb"; }
    [[nodiscard]] rc::Settings settings() const {
        rc::Settings s;
        s.root = root;
        return s;
    }
    [[nodiscard]] Result run(const std::vector<std::string>& extra = {}) const { return run_with(settings(), extra); }
    [[nodiscard]] std::vector<Call> calls() const { return read_calls(log); }
    [[nodiscard]] std::string head() const { return header(root); }
};

// the recipe of the artefacts both builds agree on: three artefacts and four files that are none
const char kAgreeing[] =
    "* file tools/notice 755 hello\n"
    "* file libx.a 644 archive\n"
    "* file CMakeFiles/x.dir/y.cpp.o 644 object\n"
    "* file CMakeFiles/3.28/CompilerIdCXX/a.out 755 compiler-id\n"
    "* file readme.txt 644 text\n"
    "* file tools/data 644 data\n"
    "* file script.sh 755 #!/bin/sh\n";

// ---- a real build tree, read by a second method -----------------------------------------------------------------------------------------------------------------------------------------------------

// POSIX readdir, lstat and stat, and the rule as the test states it: a regular file (a link to one counts; a link to a directory is not entered); the text after the LAST dot of the name when that dot is neither
// first nor last is its suffix; .a, .so and .o, or an executable with no suffix and no directory called CMakeFiles on its way.
void walk(const std::string& base, const std::string& rel, std::set<std::string>& found) {
    DIR* d = ::opendir((rel.empty() ? base : base + "/" + rel).c_str());
    if (d == nullptr) return;
    std::vector<std::string> names;
    while (const dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != "..") names.push_back(n);
    }
    ::closedir(d);
    for (const std::string& n : names) {
        const std::string r = rel.empty() ? n : rel + "/" + n;
        struct stat l{};
        if (::lstat((base + "/" + r).c_str(), &l) != 0) continue;
        if (S_ISDIR(l.st_mode)) {
            walk(base, r, found);
            continue;
        }
        struct stat s{};
        if (::stat((base + "/" + r).c_str(), &s) != 0 || !S_ISREG(s.st_mode)) continue;
        const std::size_t dot = n.rfind('.');
        const std::string suffix = (dot != std::string::npos && dot != 0 && dot + 1 != n.size()) ? n.substr(dot) : std::string();
        bool is_artefact = suffix == ".a" || suffix == ".so" || suffix == ".o";
        if (!is_artefact && suffix.empty() && (s.st_mode & 0111) != 0 && ("/" + r + "/").find("/CMakeFiles/") == std::string::npos) is_artefact = true;
        if (is_artefact) found.insert(r);
    }
}

std::set<std::string> walked(const fs::path& directory) {
    std::set<std::string> found;
    walk(directory.string(), "", found);
    return found;
}

// a tiny project: a library, a program that links it, the flags of cmake/OdlReproducible.cmake (the build tree is INSIDE the source tree, so the build map is written last: GCC tries the last first), and
// optionally the build directory's path compiled into the program
void write_tiny_project(const fs::path& tree, bool leak) {
    fs::create_directories(tree);
    write_text(tree / "CMakeLists.txt",
               "cmake_minimum_required(VERSION 3.20)\n"
               "project(tiny CXX)\n"
               "add_compile_options(\"-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.\" \"-ffile-prefix-map=${CMAKE_BINARY_DIR}=.\" -fno-ident)\n"
               "set(CMAKE_CXX_ARCHIVE_CREATE \"<CMAKE_AR> qcD <TARGET> <LINK_FLAGS> <OBJECTS>\")\n"
               "set(CMAKE_CXX_ARCHIVE_APPEND \"<CMAKE_AR> qD  <TARGET> <LINK_FLAGS> <OBJECTS>\")\n"
               "set(CMAKE_CXX_ARCHIVE_FINISH \"<CMAKE_RANLIB> -D <TARGET>\")\n"
               "add_library(tinylib STATIC lib.cpp)\n"
               "add_executable(tiny main.cpp)\n"
               "target_link_libraries(tiny PRIVATE tinylib)\n" +
               std::string(leak ? "target_compile_definitions(tiny PRIVATE BUILD_DIR=\"${CMAKE_BINARY_DIR}\")\n" : ""));
    write_text(tree / "lib.cpp", "int answer() { return 42; }\n");
    write_text(tree / "main.cpp",
               "#include <cstdio>\n"
               "int answer();\n"
               "#ifdef BUILD_DIR\n"
               "const char* where = BUILD_DIR;\n"
               "#else\n"
               "const char* where = \"no leak\";\n"
               "#endif\n"
               "int main() { std::puts(where); return answer() == 42 ? 0 : 1; }\n");
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the constants

TEST_CASE("the two build directories differ in name length by seven, and the pinned clock is 2026-09-17", "[reprocheck][behaviour]") {
    CHECK(std::string(rc::kDirA) == "build-repro-a");
    CHECK(std::string(rc::kDirB) == "build-repro-bbbbbbbb");
    CHECK(std::strlen(rc::kDirA) == 13);
    CHECK(std::strlen(rc::kDirB) == 20);
    CHECK(std::string(rc::kSourceDateEpoch) == "1789603200");
    // SECOND METHOD for the date: the days from 1970-01-01 to a civil date by the calendar (the proleptic Gregorian days-from-civil algorithm), times 86,400 s
    const auto days_from_civil = [](long long y, long long m, long long d) {
        y -= m <= 2 ? 1 : 0;
        const long long era = (y >= 0 ? y : y - 399) / 400;
        const long long yoe = y - era * 400;
        const long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    };
    CHECK(days_from_civil(1970, 1, 1) == 0);
    CHECK(days_from_civil(2000, 3, 1) == 11017);
    CHECK(std::to_string(days_from_civil(2026, 9, 17) * 86400) == rc::kSourceDateEpoch);
    CHECK(std::to_string(days_from_civil(2026, 9, 18) * 86400) != rc::kSourceDateEpoch);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- what an artefact is

TEST_CASE("artefacts: the suffixes, the execute bits, CMakeFiles, links, and what is no file", "[reprocheck][behaviour]") {
    TempDir td{"odl-rc-art"};
    const fs::path out = fs::canonical(td.path());
    const auto make = [&](const std::string& rel, mode_t mode) {
        const fs::path p = out / rel;
        fs::create_directories(p.parent_path());
        write_text(p, "x");
        REQUIRE(::chmod(p.c_str(), mode) == 0);
    };
    // artefacts
    make("libx.a", 0644);
    make("y.so", 0644);
    make("z.o", 0644);
    make("deep/er/obj.o", 0644);
    make("CMakeFiles/t.dir/m.cpp.o", 0644);   // a suffix makes an artefact even inside CMakeFiles
    make("tools/prog", 0755);                  // an executable with no suffix
    make("ex_u", 0744);                        // any one of the three execute bits is enough
    make("ex_g", 0654);
    make("ex_o", 0645);
    make("MyCMakeFiles/exe", 0755);            // CMakeFiles is a whole component, not a substring
    make(".hidden", 0755);                     // a dot file has no suffix
    make("noext.", 0755);                      // nor has a name ending in a dot
    make("dir.o/inner", 0755);                 // a directory named like an artefact is no artefact, what is in it is looked at
    // not artefacts
    make("tools/data", 0644);                  // no suffix, not executable
    make("tools/run.sh", 0755);                // a suffix that is none of the three
    make("x.so.1", 0755);                      // the suffix is .1
    make("a.tar.gz", 0755);
    make("lib.A", 0644);                       // the suffixes are matched exactly
    make("u.O", 0644);
    make("CMakeFiles/some/exe", 0755);         // no suffix, but under CMakeFiles
    make("a/CMakeFiles/b/exe", 0755);          // at any depth
    make("CMakeFiles/3.28/CompilerIdCXX/a.out", 0755);
    make("sub/CMakeFiles", 0755);              // a FILE of that name: its path has the component
    // links, a pipe, a broken link
    fs::create_symlink("tools/prog", out / "link_to_prog");   // a link to an executable with no suffix: an artefact by its own name
    fs::create_symlink("z.o", out / "link.o");                // a link whose name has the suffix: an artefact
    fs::create_symlink("tools/data", out / "link_to_data");   // to a file that is not executable: none
    fs::create_symlink("nowhere", out / "broken.o");          // to nothing: no file
    fs::create_symlink("deep", out / "dirlink.o");            // to a directory: not entered, and not a file
    REQUIRE(::mkfifo((out / "fifo.o").c_str(), 0644) == 0);   // a pipe is no regular file

    const std::map<std::string, fs::path> found = rc::artefacts(out);
    const std::vector<std::string> expected = {".hidden", "CMakeFiles/t.dir/m.cpp.o", "MyCMakeFiles/exe", "deep/er/obj.o", "dir.o/inner", "ex_g", "ex_o", "ex_u", "libx.a", "link.o", "link_to_prog", "noext.", "tools/prog", "y.so", "z.o"};
    std::vector<std::string> keys;
    for (const auto& entry : found) keys.push_back(entry.first);
    CHECK(keys == expected);   // and in the order of the Python's sorted() on those strings: bytes, so '.' < 'C' < 'M' < 'd'
    CHECK(found.at("tools/prog") == out / "tools/prog");
    CHECK(found.at("link_to_prog") == out / "link_to_prog");
    CHECK(found.at("CMakeFiles/t.dir/m.cpp.o") == out / "CMakeFiles/t.dir/m.cpp.o");
    CHECK(found.at("dir.o/inner") == out / "dir.o/inner");
    // an absent directory, an empty one, a file given as the directory: nothing
    CHECK(rc::artefacts(out / "absent").empty());
    fs::create_directories(out / "empty");
    CHECK(rc::artefacts(out / "empty").empty());
    CHECK(rc::artefacts(out / "z.o").empty());
    // the second method agrees on this tree too
    std::set<std::string> by_the_walk = walked(out);
    CHECK(std::vector<std::string>(by_the_walk.begin(), by_the_walk.end()) == expected);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the tool, through the stand-in

TEST_CASE("run_on: two builds that agree -- the lines, the exit, the four calls of cmake, the environment, the clean-up", "[reprocheck][behaviour]") {
    const Fixture f(kAgreeing);
    const Result r = f.run();
    CHECK(r.code == 0);
    CHECK(r.out == f.head() + "\n3 artefacts compared\nok       all 3 artefacts byte-identical across two build paths\n");
    CHECK(r.err.empty());
    CHECK_FALSE(fs::exists(f.dir_a()));
    CHECK_FALSE(fs::exists(f.dir_b()));

    const std::vector<Call> calls = f.calls();
    REQUIRE(calls.size() == 4);
    const std::string root = f.root.string();
    const std::string a = f.dir_a().string();
    const std::string b = f.dir_b().string();
    const std::vector<std::string> configure_a = {"-S", root, "-B", a, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo"};
    const std::vector<std::string> configure_b = {"-S", root, "-B", b, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo"};
    CHECK(calls.at(0).args == configure_a);
    CHECK(calls.at(0).mode == "configure");
    CHECK(calls.at(0).side == "a");
    CHECK(calls.at(1).args == std::vector<std::string>({"--build", a}));
    CHECK(calls.at(1).mode == "build");
    CHECK(calls.at(1).side == "a");
    CHECK(calls.at(2).args == configure_b);
    CHECK(calls.at(2).mode == "configure");
    CHECK(calls.at(2).side == "b");
    CHECK(calls.at(3).args == std::vector<std::string>({"--build", b}));
    CHECK(calls.at(3).mode == "build");
    CHECK(calls.at(3).side == "b");
    for (const Call& call : calls) {
        // the pinned clock, locale and time zone REPLACE the parent's (the fixture set others)
        CHECK(call.env.at("SOURCE_DATE_EPOCH") == "1789603200");
        CHECK(call.env.at("LC_ALL") == "C");
        CHECK(call.env.at("TZ") == "UTC");
    }
}

TEST_CASE("run_on: the three lines that open the run are flushed before the first build starts -- someone watching builds of minutes sees them at once", "[reprocheck][behaviour]") {
    const Fixture f(kAgreeing);
    NotingBuffer buffer(f.log);
    std::ostream out(&buffer);
    std::ostringstream err;
    CHECK(rc::run_on(f.settings(), {}, Streams{out, err}) == 0);
    const std::string text = read_text(f.log);
    const std::size_t flushed = text.find("--- flush");
    const std::size_t first_call = text.find("--- call");
    REQUIRE(flushed != std::string::npos);
    REQUIRE(first_call != std::string::npos);
    CHECK(flushed < first_call);
}

TEST_CASE("run_on: what an earlier run left in the build directories is removed first, and --keep keeps the two trees", "[reprocheck][behaviour]") {
    const Fixture f(kAgreeing);
    // a stale artefact in each directory: if it survived it would be one more artefact, and ONLY IN A or B would say so
    fs::create_directories(f.dir_a() / "old");
    fs::create_directories(f.dir_b() / "old");
    write_text(f.dir_a() / "old" / "stale.o", "old");
    write_text(f.dir_b() / "old" / "stale.o", "old");
    const Result r = f.run();
    CHECK(r.code == 0);
    CHECK(r.out == f.head() + "\n3 artefacts compared\nok       all 3 artefacts byte-identical across two build paths\n");
    CHECK(r.err.empty());
    const std::vector<Call> calls = f.calls();
    REQUIRE(calls.size() == 4);
    CHECK(calls.at(0).existed == "no");
    CHECK(calls.at(0).held.empty());
    CHECK(calls.at(2).existed == "no");
    CHECK(calls.at(2).held.empty());
    CHECK_FALSE(fs::exists(f.dir_a()));
    CHECK_FALSE(fs::exists(f.dir_b()));

    // --keep: the same lines, and both trees stay, each holding its artefacts and the stand-in's cache
    fs::remove(f.log);
    const Result kept = f.run({"--keep"});
    CHECK(kept.code == 0);
    CHECK(kept.out == r.out);
    CHECK(kept.err.empty());
    for (const fs::path& dir : {f.dir_a(), f.dir_b()}) {
        CHECK(fs::exists(dir / "CMakeCache.txt"));
        CHECK(fs::exists(dir / "libx.a"));
        CHECK(fs::exists(dir / "tools" / "notice"));
        CHECK(rc::artefacts(dir).size() == 3);
    }
    // ... and a third run, without --keep, removes what the second kept before it builds
    fs::remove(f.log);
    const Result third = f.run();
    CHECK(third.code == 0);
    const std::vector<Call> again = f.calls();
    REQUIRE(again.size() == 4);
    CHECK(again.at(0).existed == "no");
    CHECK(again.at(2).existed == "no");
    CHECK_FALSE(fs::exists(f.dir_a()));
}

TEST_CASE("run_on: an artefact whose bytes hold the build directory's path -- the sizes differ, and the note says so", "[reprocheck][behaviour]") {
    const Fixture f(
        "* file tools/notice 755 hello\n"
        "* file bin/app 755 built-in-@OUT@\n");
    const Result r = f.run();
    const std::string content_a = "built-in-" + f.dir_a().string();
    const std::string content_b = "built-in-" + f.dir_b().string();
    REQUIRE(content_b.size() == content_a.size() + 7);
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n2 artefacts compared\n");   // the verdict line "ok ..." is not said
    CHECK(r.err ==
          "  DIFFERS  bin/app\n    A " + sha(content_a) + "  " + std::to_string(content_a.size()) + " bytes\n    B " + sha(content_b) + "  " + std::to_string(content_b.size()) + " bytes\n" + kSizesNote + verdict(1, 0));
    // the trees are removed whatever the verdict
    CHECK_FALSE(fs::exists(f.dir_a()));
    CHECK_FALSE(fs::exists(f.dir_b()));
}

TEST_CASE("run_on: artefacts of the same size with other bytes -- DIFFERS, the hashes of coreutils, and no note", "[reprocheck][behaviour]") {
    const Fixture f("* file lib.a 644 side-@SIDE@\n");
    const Result r = f.run();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n1 artefacts compared\n");
    CHECK(r.err ==
          "  DIFFERS  lib.a\n    A 8f8b53291aad6cf9050edb0c0f45a19b2ab8a3174422b5f4e4df35ead1ba2dbb  6 bytes\n    B 7e0bd07ef8d43d2da3c7390a5843f507ccb310ce18a070dbc9f0b12fe1224494  6 bytes\n" + verdict(1, 0));
    // and the hashes are of the whole contents: "abc" and "bbc" are the FIPS 180 vector and its neighbour
    const Fixture g("* file one.o 644 @SIDE@bc\n");
    const Result s = g.run();
    CHECK(s.code == 1);
    CHECK(s.err ==
          "  DIFFERS  one.o\n    A ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  3 bytes\n    B 77e6f78af45f649c5f3b8ebe484a91a144eb203a34a89c8dc5b1c4ca87bc6f71  3 bytes\n" + verdict(1, 0));
}

TEST_CASE("run_on: an artefact one build has and the other lacks -- ONLY IN A, ONLY IN B, and a verdict that counts them", "[reprocheck][behaviour]") {
    const Fixture f(
        "a file onlya.o 644 x\n"
        "b file onlyb.o 644 y\n"
        "* file both.o 644 z\n");
    const Result r = f.run();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n1 artefacts compared\n");
    CHECK(r.err == "  ONLY IN A  onlya.o\n  ONLY IN B  onlyb.o\n" + verdict(0, 2));   // the verdict used to say 0, and no more: it names every reason it failed

    // only on one side
    const Fixture g(
        "b file onlyb.o 644 y\n"
        "* file both.o 644 z\n");
    const Result s = g.run();
    CHECK(s.code == 1);
    CHECK(s.err == "  ONLY IN B  onlyb.o\n" + verdict(0, 1));
    const Fixture h(
        "a file onlya.o 644 y\n"
        "* file both.o 644 z\n");
    const Result t = h.run();
    CHECK(t.code == 1);
    CHECK(t.err == "  ONLY IN A  onlya.o\n" + verdict(0, 1));
    // differences and artefacts on one side only together: both counts, each as it is
    const Fixture k(
        "a file onlya.o 644 x\n"
        "a file onlya2.o 644 x\n"
        "b file onlyb.o 644 y\n"
        "* file lib.a 644 side-@SIDE@\n"
        "* file same.o 644 z\n");
    const Result u = k.run();
    CHECK(u.code == 1);
    CHECK(u.err ==
          "  DIFFERS  lib.a\n    A 8f8b53291aad6cf9050edb0c0f45a19b2ab8a3174422b5f4e4df35ead1ba2dbb  6 bytes\n    B 7e0bd07ef8d43d2da3c7390a5843f507ccb310ce18a070dbc9f0b12fe1224494  6 bytes\n"
          "  ONLY IN A  onlya.o\n  ONLY IN A  onlya2.o\n  ONLY IN B  onlyb.o\n" + verdict(1, 3));
}

TEST_CASE("run_on: when only one build has artefacts it is the ordinary failure, not the refusal of a pair with none", "[reprocheck][behaviour]") {
    const Fixture f(
        "a file onlya.o 644 x\n"
        "* file readme.txt 644 text\n");
    const Result r = f.run();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n0 artefacts compared\n");
    CHECK(r.err == "  ONLY IN A  onlya.o\n" + verdict(0, 1));
}

TEST_CASE("run_on: differences, then ONLY IN A, then ONLY IN B, each in the order of the Python's sorted strings -- bytes, not path components", "[reprocheck][behaviour]") {
    // by string, 'B' (0x42) < 'a' and '-' (0x2D) < '.' (0x2E) < '/' (0x2F), so a-b/x.o < a.o < a/b.o; by path components a/b.o would come first
    const Fixture f(
        "* file a/b.o 644 same\n"
        "* file a-b/x.o 644 @SIDE@\n"
        "* file a.o 644 @SIDE@@SIDE@\n"
        "* file B.o 644 q@SIDE@\n"
        "* file c.o 644 @OUT@\n"
        "a file z1.o 644 x\n"
        "a file Z0.o 644 x\n"
        "b file y1.o 644 x\n"
        "b file Y0.o 644 x\n");
    const Result r = f.run();
    const std::string path_a = f.dir_a().string();
    const std::string path_b = f.dir_b().string();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n5 artefacts compared\n");
    const auto same_size = [](const std::string& rel, const std::string& x, const std::string& y) {
        return "  DIFFERS  " + rel + "\n    A " + sha(x) + "  " + std::to_string(x.size()) + " bytes\n    B " + sha(y) + "  " + std::to_string(y.size()) + " bytes\n";
    };
    CHECK(r.err ==
          same_size("B.o", "qa", "qb") + same_size("a-b/x.o", "a", "b") + same_size("a.o", "aa", "bb") + same_size("c.o", path_a, path_b) + kSizesNote +
              "  ONLY IN A  Z0.o\n  ONLY IN A  z1.o\n  ONLY IN B  Y0.o\n  ONLY IN B  y1.o\n" + verdict(4, 4));
}

TEST_CASE("run_on: a difference in the last byte of a file of three megabytes is found -- the hash is of the whole file", "[reprocheck][behaviour]") {
    const std::string body(3 * 1024 * 1024, 'x');
    const Fixture f("* file big.o 644 " + body + "@SIDE@\n");
    const Result r = f.run();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n1 artefacts compared\n");
    const std::string size = std::to_string(body.size() + 1);
    CHECK(r.err ==
          "  DIFFERS  big.o\n    A " + sha(body + "a") + "  " + size + " bytes\n    B " + sha(body + "b") + "  " + size + " bytes\n" + verdict(1, 0));
}

TEST_CASE("run_on: a link is an artefact by its own name and is hashed by what it leads to", "[reprocheck][behaviour]") {
    const Fixture f(
        "* file real.bin 644 target-@SIDE@\n"
        "* link alias.o real.bin\n"
        "* file stable.o 644 fixed\n");
    const Result r = f.run();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n2 artefacts compared\n");
    CHECK(r.err ==
          "  DIFFERS  alias.o\n    A " + sha("target-a") + "  8 bytes\n    B " + sha("target-b") + "  8 bytes\n" + verdict(1, 0));
}

TEST_CASE("run_on: a pair of builds with no artefact in either is refused -- success over nothing read is the failure the check exists against", "[reprocheck][behaviour]") {
    const Fixture f(
        "* file readme.txt 644 text\n"
        "* file CMakeFiles/exe 755 hidden\n");
    const Result r = f.run();
    CHECK(r.code == 2);
    CHECK(r.out == f.head());   // the three opening lines: no "compared" line, no "ok"
    CHECK(r.err == "reprocheck: no artefacts found in either build: nothing compared\n");
    // the trees are removed as they are after any run that reached the comparison
    CHECK_FALSE(fs::exists(f.dir_a()));
    CHECK_FALSE(fs::exists(f.dir_b()));
    // and --keep keeps them
    const Result kept = f.run({"--keep"});
    CHECK(kept.code == 2);
    CHECK(kept.err == "reprocheck: no artefacts found in either build: nothing compared\n");
    CHECK(fs::exists(f.dir_a() / "readme.txt"));
    CHECK(fs::exists(f.dir_b() / "readme.txt"));
    // a recipe of nothing at all is the same
    const Fixture g("");
    const Result s = g.run();
    CHECK(s.code == 2);
    CHECK(s.err == "reprocheck: no artefacts found in either build: nothing compared\n");
}

TEST_CASE("run_on: a build that fails -- exit 3, the command, what cmake said; nothing is cleaned up and the second tree is not built after the first fails", "[reprocheck][behaviour]") {
    struct Case {
        const char* fail;        // configure:a ...
        std::size_t calls;       // how many calls were made, the failing one included
        bool command_is_configure;
        bool side_a;
    };
    const std::vector<Case> cases = {{"configure:a", 1, true, true}, {"build:a", 2, false, true}, {"configure:b", 3, true, false}, {"build:b", 4, false, false}};
    for (const Case& c : cases) {
        INFO("failing " << c.fail);
        Fixture f(kAgreeing);
        f.env.set("ODL_FAKE_CMAKE_FAIL", c.fail);
        f.env.set("ODL_FAKE_CMAKE_EXIT", "9");   // whatever the status, the tool's own is 3
        const Result r = f.run();
        const fs::path dir = c.side_a ? f.dir_a() : f.dir_b();
        const std::string command = c.command_is_configure ? "cmake -S " + f.root.string() + " -B " + dir.string() + " -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo" : "cmake --build " + dir.string();
        CHECK(r.code == 3);
        CHECK(r.out == f.head());   // the three lines, no "compared", no "ok"
        CHECK(r.err == "reprocheck: build failed: " + command + "\nfake cmake: standard output of " + c.fail + "\n\nfake cmake: standard error of " + c.fail + "\n\n");
        CHECK(f.calls().size() == c.calls);
        // nothing is cleaned up: a tree that was configured is still there (the failing configure made none)
        CHECK(fs::exists(f.dir_a()) == (std::string(c.fail) != "configure:a"));
        CHECK(fs::exists(f.dir_b()) == (c.calls >= 4 || std::string(c.fail) == "build:b"));
    }
}

TEST_CASE("run_on: what a failed build said is passed through as bytes, with \"\\r\\n\" and a lone \"\\r\" made \"\\n\" as text mode made them", "[reprocheck][behaviour]") {
    Fixture f(kAgreeing);
    f.env.set("ODL_FAKE_CMAKE_FAIL", "build:a");
    f.env.set("ODL_FAKE_CMAKE_FAIL_OUT", "one\\r\\ntwo\\rthree\\n\\xFF\\x80 end\\r");   // decoded by the stand-in: one CR LF two CR three LF, two bytes that are no UTF-8, " end", CR
    f.env.set("ODL_FAKE_CMAKE_FAIL_ERR", "caf\\xC3\\xA9 \\\\ \\r\\n");                    // "caf", e-acute in UTF-8, a space, one backslash, a space, CR LF
    const Result r = f.run();
    CHECK(r.code == 3);
    CHECK(r.out == f.head());
    CHECK(r.err == "reprocheck: build failed: cmake --build " + f.dir_a().string() + "\n" + "one\ntwo\nthree\n\xFF\x80 end\n" + "\n" + "caf\xC3\xA9 \\ \n" + "\n");
}

TEST_CASE("run_on: with --keep the two trees stay whatever the verdict", "[reprocheck][behaviour]") {
    const Fixture f(
        "* file tools/notice 755 hello\n"
        "* file bin/app 755 built-in-@OUT@\n");
    const Result r = f.run({"--keep"});
    CHECK(r.code == 1);
    CHECK(fs::exists(f.dir_a() / "bin" / "app"));
    CHECK(fs::exists(f.dir_b() / "bin" / "app"));
    CHECK(fs::exists(f.dir_a() / "CMakeCache.txt"));
    CHECK(fs::exists(f.dir_b() / "CMakeCache.txt"));
}

TEST_CASE("run_on: a build directory that cannot be removed is no refusal -- the Python's rmtree ignored its errors -- and what stays in it is seen", "[reprocheck][behaviour]") {
    if (::geteuid() == 0) return;   // root can remove it
    Fixture f(kAgreeing);
    fs::create_directories(f.dir_a() / "ro");
    write_text(f.dir_a() / "ro" / "stale.o", "old");
    REQUIRE(::chmod((f.dir_a() / "ro").c_str(), 0555) == 0);
    struct Restore {   // declared after the fixture, so it runs before the temporary directory is removed
        fs::path directory;
        ~Restore() { ::chmod(directory.c_str(), 0755); }
    } restore{f.dir_a() / "ro"};
    const Result r = f.run();
    CHECK(r.code == 1);
    CHECK(r.out == f.head() + "\n3 artefacts compared\n");
    CHECK(r.err == "  ONLY IN A  ro/stale.o\n" + verdict(0, 1));
    const std::vector<Call> calls = f.calls();
    REQUIRE(calls.size() == 4);
    CHECK(calls.at(0).existed == "yes");   // the part that could not be removed is still there when cmake is called
    CHECK(calls.at(0).held == "ro");
    CHECK(calls.at(2).existed == "no");
}

TEST_CASE("run_on: an empty --root is refused -- it would be the current directory, built twice without anyone having said so", "[reprocheck][behaviour]") {
    const Fixture f(kAgreeing);
    for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{{"--root="}, {"--root", ""}}) {
        INFO("arguments: " << args.front());
        const Result r = f.run(args);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "reprocheck: error: argument --root: the path is empty\n");
    }
    CHECK(f.calls().empty());
}

TEST_CASE("run_on: --build-dir is refused by name, in every form, before anything is built -- an option accepted and silently ignored is a silent fallback", "[reprocheck][behaviour]") {
    const Fixture f(kAgreeing);
    for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{{"--build-dir"},
                                                                                       {"--build-dir", "somewhere"},
                                                                                       {"--build-dir=somewhere"},
                                                                                       {"--build-dir="},
                                                                                       {"--build-dir", ""},
                                                                                       {"--keep", "--build-dir", "--keep"},
                                                                                       {"--root", f.root.string(), "--build-dir", f.root.string()},
                                                                                       {"--build-dir", "x", "--bogus"}}) {
        INFO("arguments: " << args.front() << (args.size() > 1 ? " ..." : ""));
        const Result r = f.run(args);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kUsage) + "reprocheck: error: " + kBuildDirNotUsed + "\n");   // the first fault met is the one said
    }
    CHECK(f.calls().empty());   // not one of them built anything
}

TEST_CASE("run_on: the command line -- -h whole, the refusals, --root in both forms, --keep", "[reprocheck][behaviour]") {
    const char* const usage = kUsage;
    // What `-h` prints after the usage line: the port's own text, RECORDED here whole so that every word of it is pinned.
    const char help[] =
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
    const Fixture f(kAgreeing);
    for (const char* flag : {"-h", "--help"}) {
        const Result r = f.run({flag});
        CHECK(r.code == 0);
        CHECK(r.out == std::string(usage) + help);
        CHECK(r.err.empty());
    }
    // -h wins over what follows it, and nothing is built
    CHECK(f.run({"-h", "--bogus"}).code == 0);
    CHECK(f.calls().empty());
    // a later option is not looked at once -h is met, an earlier bad one is
    CHECK(f.run({"--bogus", "-h"}).code == 2);

    const auto refused = [&](const std::vector<std::string>& args, const std::string& what) {
        INFO("arguments: " << (args.empty() ? std::string("(none)") : args.front()));
        const Result r = f.run(args);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(usage) + "reprocheck: error: " + what + "\n");
    };
    refused({"--bogus"}, "unrecognized arguments: --bogus");
    refused({"extra"}, "unrecognized arguments: extra");
    refused({"--keep=1"}, "unrecognized arguments: --keep=1");
    refused({"--kee"}, "unrecognized arguments: --kee");   // argparse took an abbreviation; this tool takes exact names
    refused({"--root"}, "argument --root: expected one argument");
    refused({"--root", "--keep"}, "argument --root: expected one argument");
    refused({"--root", "---x"}, "argument --root: expected one argument");   // anything that begins with two hyphens is taken for the next option, three included
    refused({"--build-dir"}, kBuildDirNotUsed);   // (the whole of its cases is the one above)
    CHECK(f.calls().empty());   // not one of them built anything

    // --root in both forms; --keep; in any order
    const Fixture g(kAgreeing);
    rc::Settings elsewhere;
    elsewhere.root = g.base / "nowhere";   // the settings' root is replaced by --root
    for (const std::vector<std::string>& args : std::vector<std::vector<std::string>>{{"--root", g.root.string()},
                                                                                       {"--root=" + g.root.string()},
                                                                                       {"--keep", "--root", g.root.string(), "--keep"}}) {
        INFO("arguments: " << args.front());
        const Result r = run_with(elsewhere, args);
        CHECK(r.code == 0);
        CHECK(r.out == header(g.root) + "\n3 artefacts compared\nok       all 3 artefacts byte-identical across two build paths\n");
        CHECK(r.err.empty());
    }
}

TEST_CASE("run_on: the root is made absolute, its links resolved and its trailing slash dropped", "[reprocheck][behaviour]") {
    const Fixture f(kAgreeing);
    const std::string expected = header(f.root) + "\n3 artefacts compared\nok       all 3 artefacts byte-identical across two build paths\n";
    // a trailing slash
    CHECK(f.run({"--root", f.root.string() + "/"}).out == expected);
    // a path with a dot and a dot-dot in it
    CHECK(f.run({"--root", (f.root / "." / ".." / "tree").string()}).out == expected);
    // a symbolic link to the tree
    const fs::path link = f.base / "link-to-tree";
    fs::create_symlink(f.root, link);
    CHECK(f.run({"--root", link.string()}).out == expected);
    // a relative path
    const fs::path relative = fs::relative(f.root, fs::current_path());
    CHECK(f.run({"--root", relative.string()}).out == expected);
    // and what cmake was given is the same resolved tree each time
    for (const Call& call : f.calls()) {
        if (call.mode == "configure") CHECK(call.args.at(1) == f.root.string());
    }
    // a root that does not exist yet cannot be resolved, so its trailing slash must be dropped by hand: cmake is given the root without it (the stand-in makes the directory)
    fs::remove(f.log);
    const fs::path future = f.base / "not-yet";
    const Result r = f.run({"--root", future.string() + "/"});
    CHECK(r.code == 0);
    CHECK(r.out == header(future) + "\n3 artefacts compared\nok       all 3 artefacts byte-identical across two build paths\n");
    const std::vector<Call> calls = f.calls();
    REQUIRE(calls.size() == 4);
    CHECK(calls.at(0).args.at(1) == future.string());
    CHECK(calls.at(0).args.at(3) == (future / "build-repro-a").string());
    CHECK(calls.at(2).args.at(1) == future.string());
}

TEST_CASE("run_on: refusals -- cmake cannot be run, an artefact cannot be read", "[reprocheck][behaviour]") {
    {
        // no cmake on PATH: the Python raised FileNotFoundError and ended in a traceback; this names the fault and exits 2
        Fixture f(kAgreeing);
        fs::create_directories(f.base / "emptybin");
        f.env.set("PATH", (f.base / "emptybin").string());
        const Result r = f.run();
        CHECK(r.code == 2);
        CHECK(r.out == f.head());
        CHECK(r.err == "reprocheck: cannot run 'cmake': No such file or directory\n");
    }
    if (::geteuid() != 0) {   // a file one cannot open is only unreadable to someone who is not root
        const Fixture f("* file bad.o 000 x\n");
        const Result r = f.run();
        CHECK(r.code == 2);
        CHECK(r.out == f.head());
        CHECK(r.err == "reprocheck: cannot open " + (f.dir_a() / "bad.o").string() + "\n");
        // nothing was cleaned up: a refusal is not a verdict
        CHECK(fs::exists(f.dir_a() / "bad.o"));
    }
}

TEST_CASE("run: the default settings are the tree the tool was built from", "[reprocheck][behaviour]") {
    CHECK_FALSE(rc::default_root().empty());
    CHECK(rc::default_settings().root == rc::default_root());
#ifdef ODL_TREE_ROOT
    CHECK(rc::default_root() == fs::path(ODL_TREE_ROOT));
#endif
    // run() is run_on() with them: the usage error comes before anything is built
    std::ostringstream out;
    std::ostringstream err;
    CHECK(rc::run({"--bogus"}, Streams{out, err}) == 2);
    CHECK(out.str().empty());
    CHECK(err.str() == std::string(kUsage) + "reprocheck: error: unrecognized arguments: --bogus\n");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the real thing

TEST_CASE("real cmake, Ninja and the compiler: a tiny project built twice is reproducible, and the count is a second method's", "[reprocheck][real_cmake]") {
    TempDir td{"odl-rc-real"};
    const fs::path tree = fs::canonical(td.path()) / "tiny";
    write_tiny_project(tree, false);
    rc::Settings settings;
    settings.root = tree;
    const Result r = run_with(settings, {"--keep"});
    INFO("standard error: " << r.err);
    CHECK(r.code == 0);
    CHECK(r.err.empty());
    // the two trees agree on what is in them, and the count printed is the number the walk finds
    const std::set<std::string> in_a = walked(tree / "build-repro-a");
    const std::set<std::string> in_b = walked(tree / "build-repro-bbbbbbbb");
    CHECK(in_a == in_b);
    CHECK(in_a.count("tiny") == 1);
    CHECK(in_a.count("libtinylib.a") == 1);
    CHECK(in_a.count("CMakeFiles/tiny.dir/main.cpp.o") == 1);
    CHECK(in_a.count("CMakeFiles/tinylib.dir/lib.cpp.o") == 1);
    CHECK(in_a.size() >= 4);
    CHECK(r.out == header(tree) + "\n" + std::to_string(in_a.size()) + " artefacts compared\nok       all " + std::to_string(in_a.size()) + " artefacts byte-identical across two build paths\n");
    // and the tool's own list is the walk's
    std::set<std::string> by_the_tool;
    for (const auto& entry : rc::artefacts(tree / "build-repro-a")) by_the_tool.insert(entry.first);
    CHECK(by_the_tool == in_a);
}

TEST_CASE("real cmake, Ninja and the compiler: the build directory's path compiled into the program is found, in that program and its object file and nowhere else", "[reprocheck][real_cmake]") {
    TempDir td{"odl-rc-leak"};
    const fs::path tree = fs::canonical(td.path()) / "tiny";
    write_tiny_project(tree, true);
    rc::Settings settings;
    settings.root = tree;
    const Result r = run_with(settings, {});
    INFO("standard error: " << r.err);
    CHECK(r.code == 1);
    CHECK(r.err.find("  DIFFERS  tiny\n") != std::string::npos);
    CHECK(r.err.find("  DIFFERS  CMakeFiles/tiny.dir/main.cpp.o\n") != std::string::npos);
    CHECK(r.err.find("DIFFERS  libtinylib.a") == std::string::npos);
    CHECK(r.err.find("DIFFERS  CMakeFiles/tinylib.dir/lib.cpp.o") == std::string::npos);
    CHECK(r.err.find("ONLY IN") == std::string::npos);
    CHECK(r.err.find(verdict(2, 0)) != std::string::npos);
    CHECK(r.out.find("ok       all") == std::string::npos);
    // the note appears for exactly those blocks whose two sizes differ: cut the report into its DIFFERS blocks and read each block's sizes by hand
    const std::size_t verdict_at = r.err.find("\nNOT REPRODUCIBLE");
    REQUIRE(verdict_at != std::string::npos);
    std::vector<std::string> blocks;
    for (std::size_t at = r.err.find("  DIFFERS  "); at != std::string::npos;) {
        const std::size_t next = r.err.find("  DIFFERS  ", at + 1);
        blocks.push_back(r.err.substr(at, std::min(next, verdict_at) - at));
        at = next;
    }
    CHECK(blocks.size() == 2);
    for (const std::string& block : blocks) {
        const auto bytes_on = [&](const std::string& marker) {   // the number before " bytes" on the line that begins with the marker
            const std::size_t from = block.find(marker);
            REQUIRE(from != std::string::npos);
            const std::string line = block.substr(from + 1, block.find('\n', from + 1) - (from + 1));
            const std::size_t last_gap = line.rfind("  ");
            return line.substr(last_gap + 2, line.size() - (last_gap + 2) - std::string(" bytes").size());
        };
        const bool sizes_differ = bytes_on("\n    A ") != bytes_on("\n    B ");
        CHECK((block.find(kSizesNote) != std::string::npos) == sizes_differ);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the tree's own gate

TEST_CASE("the real tree: ci.sh runs gate 13 as the tool with no option -- --build-dir is refused, so a call that still passed it would fail the gate", "[reprocheck][real_tree]") {
    const std::string ci = read_text(fs::path(ODL_TREE_ROOT) / "tools" / "ci.sh");
    const std::string gate = "gate \"build is reproducible\"\n";
    const std::size_t at = ci.find(gate);
    REQUIRE(at != std::string::npos);
    CHECK(ci.find(gate, at + 1) == std::string::npos);   // one gate of that name
    const std::size_t call_ends = ci.find('\n', at + gate.size());
    REQUIRE(call_ends != std::string::npos);
    CHECK(ci.substr(at + gate.size(), call_ends - (at + gate.size())) == "\"$BUILD/tools/reprocheck\"");   // the line after the gate's title is the call, and it names no option
    // and it is the only line of ci.sh that is not a comment and names the tool
    std::istringstream lines(ci);
    std::string line;
    std::size_t calls = 0;
    while (std::getline(lines, line)) {
        const std::size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line.at(first) == '#') continue;
        if (line.find("reprocheck") != std::string::npos) ++calls;
    }
    CHECK(calls == 1);
}
