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
