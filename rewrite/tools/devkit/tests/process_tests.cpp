// The devkit's process spawner and file helpers.  The programs it runs here are `sh` and `cat`, which every POSIX host has; the tools
// themselves spawn only `curl`, `cmake` and the regeneration-only host programs named in their headers.

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/process.hpp>

#include <string>

using namespace odl::devkit;

TEST_CASE("a program's streams and exit status come back separately", "[devkit][process]") {
    const ProcessResult r = run_process({"sh", "-c", "echo to-out; echo to-err >&2; exit 3"});
    CHECK(r.exit_code == 3);
    CHECK(r.out == "to-out\n");
    CHECK(r.err == "to-err\n");
}

TEST_CASE("input is fed to the program and the environment is amended, not replaced", "[devkit][process]") {
    ProcessOptions o;
    o.input = "one\ntwo\n";
    const ProcessResult echo = run_process({"cat"}, o);
    CHECK(echo.exit_code == 0);
    CHECK(echo.out == "one\ntwo\n");

    ProcessOptions e;
    e.env = {{"ODL_DEVKIT_PROBE", "set by the test"}};
    const ProcessResult env = run_process({"sh", "-c", "printf '%s|' \"$ODL_DEVKIT_PROBE\"; command -v sh >/dev/null && printf inherited"}, e);
    CHECK(env.out == "set by the test|inherited");
}

TEST_CASE("input larger than a pipe, and output larger than a pipe, do not deadlock", "[devkit][process]") {
    ProcessOptions o;
    o.input.assign(1 << 20, 'x');   // 1 MiB: a pipe holds 64 KiB
    const ProcessResult r = run_process({"cat"}, o);
    CHECK(r.exit_code == 0);
    CHECK(r.out.size() == o.input.size());
}

TEST_CASE("a signal is reported as 128 + n, and a program that cannot be started is an exception", "[devkit][process]") {
    CHECK(run_process({"sh", "-c", "kill -TERM $$"}).exit_code == 128 + 15);
    CHECK_THROWS_AS(run_process({"odl-no-such-program-exists"}), std::runtime_error);
    CHECK_THROWS_AS(run_process({}), std::runtime_error);
}

TEST_CASE("a child that exits without reading its input costs a short write, not the parent", "[devkit][process]") {
    ProcessOptions o;
    o.input.assign(1 << 20, 'y');
    CHECK(run_process({"sh", "-c", "exit 0"}, o).exit_code == 0);
}

TEST_CASE("files: bytes, UTF-8 text, a refusal of what is not UTF-8, and a temporary directory that goes away", "[devkit][fs]") {
    std::filesystem::path kept;
    {
        TempDir dir("odl-devkit-test");
        kept = dir.path();
        REQUIRE(std::filesystem::is_directory(kept));
        const auto f = kept / "t.txt";
        write_text(f, "caf\xC3\xA9\n");
        CHECK(read_text(f) == "caf\xC3\xA9\n");
        CHECK(read_bytes(f).size() == 6);
        write_bytes(kept / "bad.bin", as_bytes("\xFF\xFE"));
        CHECK_THROWS_AS(read_text(kept / "bad.bin"), std::runtime_error);
        CHECK_THROWS_AS(read_bytes(kept / "absent"), std::runtime_error);
        CHECK_THROWS_AS(write_text(kept / "no-such-dir" / "x", "y"), std::runtime_error);
        write_text(kept / "empty", "");
        CHECK(read_bytes(kept / "empty").empty());
    }
    CHECK_FALSE(std::filesystem::exists(kept));
}
