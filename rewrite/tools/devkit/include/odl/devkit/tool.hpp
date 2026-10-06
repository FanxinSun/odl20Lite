#pragma once
// odl/devkit/tool.hpp — what every development tool of the tree has in common: two output streams, and one way to stop.
//
// A tool never writes to std::cout or std::cerr itself.  It is handed a `Streams`, so that a test runs the tool IN-PROCESS on a synthetic tree and
// reads what it said — the Python tools were tested through subprocess, and the refusal paths are the ones worth testing (tests/devtools).

#include <iosfwd>
#include <string>
#include <string_view>

namespace odl::devkit {

struct Streams {
    std::ostream& out;
    std::ostream& err;
};

/// Thrown by die() and caught at the top of a tool's `run`, which returns `code`.  The message has already been written.
struct Exit {
    int code;
};

/// Writes "<tool>: <message>\n" to err and throws Exit{code}.  The prefix is the tool's own name, as the Python tools' `die` wrote it.
[[noreturn]] void die(Streams& io, std::string_view tool, int code, std::string_view message);

}  // namespace odl::devkit
