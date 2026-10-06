#pragma once
// tests/devtools/configure_is_current.hpp — "a stale configure tests a subset and reports 100%": its entry point (plan L0 step 8, group C5).
//
// `run` is the whole check: `configure_is_current BUILD_DIR [--root DIR]`; exit 0 the configure is current, 1 it is stale or the build directory is no build directory, 2 an argument error, 70 an error
// the program did not anticipate.  The ctest `build.configure_is_current` runs the built program on CMAKE_BINARY_DIR; the Catch2 cases of tests/devtools/configure_is_current_tests.cpp call `run`
// in-process on synthetic trees with files of chosen ages.

#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace odl::tools::configure_is_current {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this program was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
std::filesystem::path default_root();

}  // namespace odl::tools::configure_is_current
