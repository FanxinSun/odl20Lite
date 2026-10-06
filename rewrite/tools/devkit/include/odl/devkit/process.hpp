#pragma once
// odl/devkit/process.hpp — run a program, feed it, and collect what it says.  POSIX only, as ci.sh already is.
//
// The tools spawn exactly three kinds of program, all named in each tool's header and none of them part of the build, the tests or CI
// except `cmake` (reprocheck) and `curl` (fetch): curl for the one online step, and the regeneration-only host programs.  Nothing here goes
// through a shell, so there is no quoting to get wrong.

#include <string>
#include <utility>
#include <vector>

namespace odl::devkit {

struct ProcessOptions {
    /// Variables set (or replaced) in the child's environment, on top of the parent's.
    std::vector<std::pair<std::string, std::string>> env;
    /// Bytes written to the child's standard input, which is then closed.
    std::string input;
};

struct ProcessResult {
    int exit_code = -1;   // the exit status; 128 + n when the child was killed by signal n
    std::string out;      // everything it wrote to standard output
    std::string err;      // ... and to standard error
};

/// Runs argv[0] (looked up on PATH) with the rest as arguments and waits for it.  Throws std::runtime_error if it cannot be started at all
/// (not found, not executable); a program that runs and fails is a result, not an exception.
[[nodiscard]] ProcessResult run_process(const std::vector<std::string>& argv, const ProcessOptions& options = {});

}  // namespace odl::devkit
