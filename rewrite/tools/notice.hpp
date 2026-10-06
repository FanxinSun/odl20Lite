#pragma once
// notice.hpp — what tools/notice.cpp shows the rest of the build: the tool itself, as a function, so that tests/devtools/notice_tests.cpp runs it
// in-process on a synthetic tree and reads both of its streams, and the renderer on its own.

#include <odl/devkit/json.hpp>
#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace odl::tools::notice {

/// The tool.  `argv` is its arguments WITHOUT the program name: [--check] [--stdout] [--root DIR].  The result is its exit code: 0 ok, 1 NOTICE has drifted
/// from the manifest (--check), 2 an argument the parser refuses (as argparse's), 3 the manifest is unreadable or malformed.
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// NOTICE's whole text for a manifest: the summary table, one block per component, and the licence texts quoted verbatim out of the pinned archives of the
/// entries that name a licence file (read from the cache under `root`).  Throws std::runtime_error, in words that name the entry, for a manifest that
/// lacks what the text needs (the Python generator died on it with a KeyError).
[[nodiscard]] std::string render(const std::filesystem::path& root, const odl::devkit::Json& doc);

/// The tree root when --root is not given: the source tree this tool was built from (ODL_TREE_ROOT), else the current directory.
[[nodiscard]] std::filesystem::path default_root();

}  // namespace odl::tools::notice
