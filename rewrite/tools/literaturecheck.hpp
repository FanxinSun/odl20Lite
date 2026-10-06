#pragma once
// tools/literaturecheck.hpp — plan section 5 constraint 3's checker "nothing that builds may reach the literature": its entry point (plan L0 step 8, group C5).
//
// `run` is the whole tool: `literaturecheck [--quiet] [--root DIR]`; exit 0 no build input reaches a literature entry, 1 one does, 2 an argument error, a manifest that cannot be read, or nothing to
// search, 70 an error the tool did not anticipate.  The tests call it in-process on synthetic trees; ci.sh's gate 11 runs the built tool (--quiet).

#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace odl::tools::literaturecheck {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
std::filesystem::path default_root();

}  // namespace odl::tools::literaturecheck
