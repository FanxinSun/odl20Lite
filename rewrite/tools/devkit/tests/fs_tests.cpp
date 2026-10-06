// tools/devkit/tests/fs_tests.cpp — what read_bytes and read_text refuse, and by what rule (ctest "devkit: ...").
//
// A RUNNER FINDING (plan L0 step 8, group C4; PROVENANCE section 41.10).  C3's read_bytes learned that a path was not a file from the size it read by seeking to the end: tmpfs answers -1 for
// a directory and ext4 -- GitHub's runner, whose /tmp is a disk -- answers 2^63-1, a vector of which threw std::bad_alloc, no std::runtime_error, and speccheck exited 70 there and 0 here.
// What a path is decides now, before it is opened.  So these cases are about the KIND of thing: they hold on every filesystem, and the old code passed them only on tmpfs; to see one fail
// on the old code, run the devtools tests with TMPDIR set to a directory on a disk (the runner's /tmp is one).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/devkit/fs.hpp>

#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace odl::devkit;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("read_bytes refuses a directory by its kind, in the words of the refusal, whatever the filesystem says about its size", "[devkit][fs]") {
    TempDir td;
    const fs::path dir = td.path() / "odd.cpp";
    fs::create_directories(dir);
    CHECK_THROWS_AS(read_bytes(dir), std::runtime_error);   // a runtime_error: the tools catch exactly that and name the file
    CHECK_THROWS_WITH(read_bytes(dir), ContainsSubstring("it is not a regular file"));
    CHECK_THROWS_WITH(read_bytes(dir), ContainsSubstring(dir.string()));
    CHECK_THROWS_AS(read_text(dir), std::runtime_error);
    // an empty directory and one with something in it, and the temporary directory itself
    fs::create_directories(dir / "inside");
    CHECK_THROWS_WITH(read_bytes(dir), ContainsSubstring("it is not a regular file"));
    CHECK_THROWS_WITH(read_bytes(td.path()), ContainsSubstring("it is not a regular file"));
}

TEST_CASE("read_bytes refuses a FIFO without opening it (opening one waits for a writer for ever), a link to a directory, and what is not there", "[devkit][fs]") {
    TempDir td;
    const fs::path fifo = td.path() / "fifo.cpp";
    REQUIRE(::mkfifo(fifo.c_str(), 0600) == 0);
    CHECK_THROWS_WITH(read_bytes(fifo), ContainsSubstring("it is not a regular file"));   // returns at once: reaching this line is the test

    fs::create_directories(td.path() / "real_dir");
    std::error_code ec;
    fs::create_directory_symlink(td.path() / "real_dir", td.path() / "dir_link.cpp", ec);
    REQUIRE_FALSE(ec);
    CHECK_THROWS_WITH(read_bytes(td.path() / "dir_link.cpp"), ContainsSubstring("it is not a regular file"));   // a link is followed, as opening it follows it

    CHECK_THROWS_WITH(read_bytes(td.path() / "absent.cpp"), ContainsSubstring("No such file or directory"));   // the reason is the system's, as it was when open() failed
    CHECK_THROWS_WITH(read_bytes(td.path() / "absent.cpp"), ContainsSubstring((td.path() / "absent.cpp").string()));
    fs::create_symlink(td.path() / "nowhere", td.path() / "dangling.cpp", ec);
    REQUIRE_FALSE(ec);
    CHECK_THROWS_WITH(read_bytes(td.path() / "dangling.cpp"), ContainsSubstring("No such file or directory"));
    CHECK_THROWS_AS(read_bytes(td.path() / "dangling.cpp"), std::runtime_error);
}

TEST_CASE("read_bytes reads a regular file, an empty one, and a link to one; read_text refuses what is not UTF-8", "[devkit][fs]") {
    TempDir td;
    write_text(td.path() / "a.txt", "caf\xC3\xA9\n");
    CHECK(read_text(td.path() / "a.txt") == "caf\xC3\xA9\n");
    CHECK(read_bytes(td.path() / "a.txt").size() == 6);
    write_text(td.path() / "empty.txt", "");
    CHECK(read_bytes(td.path() / "empty.txt").empty());
    CHECK(read_text(td.path() / "empty.txt").empty());
    std::error_code ec;
    fs::create_symlink(td.path() / "a.txt", td.path() / "alias.txt", ec);
    REQUIRE_FALSE(ec);
    CHECK(read_text(td.path() / "alias.txt") == "caf\xC3\xA9\n");   // a link to a regular file is that file, as in Python
    write_text(td.path() / "latin1.txt", "caf\xE9\n");
    CHECK_THROWS_WITH(read_text(td.path() / "latin1.txt"), ContainsSubstring("is not well-formed UTF-8"));
    CHECK(read_bytes(td.path() / "latin1.txt").size() == 5);   // as bytes it reads
}

TEST_CASE("read_bytes names a file it may not open, and a file too large for memory is a refusal and not an internal error", "[devkit][fs]") {
    TempDir td;
    const fs::path locked = td.path() / "locked.txt";
    write_text(locked, "x");
    if (::geteuid() != 0) {   // root opens anything
        REQUIRE(::chmod(locked.c_str(), 0) == 0);
        CHECK_THROWS_WITH(read_bytes(locked), ContainsSubstring("Permission denied"));
        CHECK_THROWS_AS(read_bytes(locked), std::runtime_error);
        REQUIRE(::chmod(locked.c_str(), 0600) == 0);
    }
    // A sparse file larger than any memory: its size is 2^62.  tmpfs makes one at once; a disk filesystem usually refuses the size, and then there is nothing to read here.
    const fs::path huge = td.path() / "huge.bin";
    {
        std::error_code ec;
        write_text(huge, "");
        fs::resize_file(huge, std::uintmax_t{1} << 62, ec);
        if (ec) {
            SKIP("this filesystem will not make a file of 2^62 bytes (" << ec.message() << "); the branch is exercised where one can be made, tmpfs among them");
        }
    }
    CHECK_THROWS_AS(read_bytes(huge), std::runtime_error);
    CHECK_THROWS_WITH(read_bytes(huge), ContainsSubstring("too large to read into memory"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------- group C5: pathlib's suffix and rglob

TEST_CASE("path_suffix is pathlib's PurePath.suffix (3.13): from the LAST dot, unless that dot is the first or the last character of the name", "[devkit][fs]") {
    // name.rfind('.') = i; the suffix is name[i:] when 0 < i < len(name) - 1, else ''
    CHECK(path_suffix("x.cpp") == ".cpp");
    CHECK(path_suffix("a.tar.gz") == ".gz");
    CHECK(path_suffix("a.b.c.d") == ".d");
    CHECK(path_suffix("CMakeLists.txt") == ".txt");
    CHECK(path_suffix("x.hpp.bak") == ".bak");
    CHECK(path_suffix("a.c") == ".c");
    CHECK(path_suffix("x.") == "");                 // the dot is the last character
    CHECK(path_suffix("x..") == "");
    CHECK(path_suffix("a.b.") == "");
    CHECK(path_suffix(".hpp") == "");               // the dot is the first character, and the only one
    CHECK(path_suffix(".") == "");
    CHECK(path_suffix("..") == "");
    CHECK(path_suffix("...") == "");
    CHECK(path_suffix("x") == "");
    CHECK(path_suffix("") == "");
    CHECK(path_suffix(".a.b") == ".b");             // a leading dot does not hide a later one
    CHECK(path_suffix("..x") == ".x");
    CHECK(path_suffix("a..b") == ".b");
    CHECK(path_suffix("a.\xC3\xA9") == ".\xC3\xA9");   // the suffix is bytes of UTF-8 like the rest
    CHECK(path_suffix("\xC3\xA9.txt") == ".txt");
}

namespace {

// the components of every file found, joined by '/' and by newlines, in the order returned
std::string listing(const std::vector<std::vector<std::string>>& found) {
    std::string out;
    for (const std::vector<std::string>& parts : found) {
        for (std::size_t i = 0; i < parts.size(); ++i) out += (i == 0 ? "" : "/") + parts[i];
        out += '\n';
    }
    return out;
}

void touch(const fs::path& p) {
    fs::create_directories(p.parent_path());
    write_text(p, "x\n");
}

}  // namespace

TEST_CASE("files_below lists every regular file below a directory, as components, in pathlib's order: component by component", "[devkit][fs]") {
    TempDir td;
    for (const char* p : {"b.txt", "a/b.cpp", "a-b/c.cpp", "a/z/y.hpp", "a/z/x.hpp", "A/upper.txt", "a.b/c", "_u/v", "a/b.cpp.bak", "deep/er/and/deeper/file.h"}) touch(td.path() / p);
    fs::create_directories(td.path() / "empty_dir/empty_too");
    // component-wise, bytes compared: "A" < "_u" < "a" < "a-b" < "a.b" < "b.txt" < "deep" < ..., and inside "a": "b.cpp" < "b.cpp.bak" < "z"; "a/b" would sort after "a-b/c" as a STRING ('/' > '-'), not as components
    CHECK(listing(files_below(td.path())) == "A/upper.txt\n_u/v\na/b.cpp\na/b.cpp.bak\na/z/x.hpp\na/z/y.hpp\na-b/c.cpp\na.b/c\nb.txt\ndeep/er/and/deeper/file.h\n");
    // a base that is a subdirectory: the components are below IT
    CHECK(listing(files_below(td.path() / "a")) == "b.cpp\nb.cpp.bak\nz/x.hpp\nz/y.hpp\n");
    CHECK(files_below(td.path() / "empty_dir").empty());
}

TEST_CASE("files_below skips a directory by NAME at any depth, never a file by its name, and a base that is no directory yields nothing", "[devkit][fs]") {
    TempDir td;
    for (const char* p : {"keep.txt", "build/skipped.txt", "a/build/skipped.txt", "a/b/build/c/skipped.txt", "a/kept.txt", "xbuild/kept.txt", "build2/kept.txt", "Build/kept.txt", "a/b/c.txt"}) touch(td.path() / p);
    touch(td.path() / "build.txt");                                                     // a FILE whose name merely begins the same
    const auto skip_build = [](const std::string& name) { return name == "build"; };
    CHECK(listing(files_below(td.path(), skip_build)) == "Build/kept.txt\na/b/c.txt\na/kept.txt\nbuild.txt\nbuild2/kept.txt\nkeep.txt\nxbuild/kept.txt\n");
    // every file when nothing is skipped
    CHECK(files_below(td.path()).size() == 10);
    // two names
    const auto skip_two = [](const std::string& name) { return name == "build" || name == "a"; };
    CHECK(listing(files_below(td.path(), skip_two)) == "Build/kept.txt\nbuild.txt\nbuild2/kept.txt\nkeep.txt\nxbuild/kept.txt\n");
    // a predicate that skips everything leaves the files of the top level only
    CHECK(listing(files_below(td.path(), [](const std::string&) { return true; })) == "build.txt\nkeep.txt\n");
    // not a directory: a file, an absent path, an empty path
    CHECK(files_below(td.path() / "keep.txt").empty());
    CHECK(files_below(td.path() / "absent").empty());
    CHECK(files_below(fs::path()).empty());
}

TEST_CASE("files_below: a link to a regular file is that file, a link to a directory is not entered, and what is no regular file is not listed", "[devkit][fs]") {
    TempDir td;
    touch(td.path() / "tree/real.txt");
    touch(td.path() / "elsewhere/inside.txt");
    std::error_code ec;
    fs::create_symlink(td.path() / "tree/real.txt", td.path() / "tree/alias.txt", ec);          // to a file: listed
    REQUIRE_FALSE(ec);
    fs::create_symlink(td.path() / "nowhere", td.path() / "tree/dangling.txt", ec);             // to nothing: not listed
    REQUIRE_FALSE(ec);
    fs::create_directory_symlink(td.path() / "elsewhere", td.path() / "tree/linked_dir", ec);   // to a directory: not entered
    REQUIRE_FALSE(ec);
    fs::create_directories(td.path() / "tree/odd.txt");                                         // a directory with a file's name: entered, holds nothing
    REQUIRE(::mkfifo((td.path() / "tree/fifo.txt").c_str(), 0600) == 0);                        // a FIFO: not listed (and not opened)
    CHECK(listing(files_below(td.path() / "tree")) == "alias.txt\nreal.txt\n");
    // a base that is itself a link to a directory is walked (the Python's rglob started from it the same way)
    fs::create_directory_symlink(td.path() / "tree", td.path() / "base_link", ec);
    REQUIRE_FALSE(ec);
    CHECK(listing(files_below(td.path() / "base_link")) == "alias.txt\nreal.txt\n");
}

TEST_CASE("files_below passes over a directory it cannot open", "[devkit][fs]") {
    if (::geteuid() == 0) SKIP("root opens every directory");
    TempDir td;
    touch(td.path() / "open/file.txt");
    touch(td.path() / "locked/hidden.txt");
    REQUIRE(::chmod((td.path() / "locked").c_str(), 0) == 0);
    CHECK(listing(files_below(td.path())) == "open/file.txt\n");
    REQUIRE(::chmod((td.path() / "locked").c_str(), 0700) == 0);   // so that the temporary directory can be removed
}
