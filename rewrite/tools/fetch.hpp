#pragma once
// fetch.hpp — what tools/fetch.cpp shows the rest of the build: the tool itself, as a function, so that tests/devtools/fetch_tests.cpp runs it
// in-process on a synthetic manifest and reads both of its streams, and the one check the tests call on its own.

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace odl::tools::fetch {

/// The tool.  `argv` is its arguments WITHOUT the program name; the result is its exit code (0 ok, 1 missing from the cache, 2 hash mismatch,
/// 3 manifest malformed, 4 network failure, 5 usage error, 2 for an argument the parser refuses, 70 for an error it did not anticipate).
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// Refuses an HTML page (a 404 saved with exit status 0) and a file that does not begin like its extension says: writes the message to
/// io.err and throws devkit::Exit{3}.  Returns for anything else, and for an extension with no declared magic.
void sniff(odl::devkit::Streams& io, const odl::devkit::Json& entry, odl::devkit::ByteView head);

/// The tree root the cache is relative to when --root is not given: the source tree this tool was built from (ODL_TREE_ROOT), else the
/// current directory.
[[nodiscard]] std::filesystem::path default_root();

}  // namespace odl::tools::fetch
