#pragma once
// tools/constraint8.hpp — plan section 5 constraint 8's checker: its entry point and the two scanners it is made of (plan L0 step 8, group C5).
//
// `run` is the whole tool: `constraint8 [--root DIR]`; exit 0 clean, 1 a violation, 2 an argument error or nothing to scan, 70 an error the tool did not anticipate.  The tests call it in-process on
// synthetic trees; ci.sh's gate 9 and the ctest `constraint8.result_only` run the built tool.  The scanners are the Python tool's two regular expressions, hand-written, exposed so that each can be
// tested against the semantics of the pattern it replaces.

#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::constraint8 {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
std::filesystem::path default_root();

namespace scan {

/// UNDERLYING.search(line): r"\btl::(expected|unexpected)\b" -- the underlying type of odl::Result named.
[[nodiscard]] bool names_underlying_type(std::string_view line);

/// MONADIC.search(line).group(1): r"\.\s*(and_then|or_else|transform_error|transform)\s*\(" -- a monadic operation in METHOD-CALL syntax (so std::transform and `->transform(` do not match);
/// the first (leftmost) match's operation, none if there is none.
[[nodiscard]] std::optional<std::string> monadic_operation(std::string_view line);

}  // namespace scan

}  // namespace odl::tools::constraint8
