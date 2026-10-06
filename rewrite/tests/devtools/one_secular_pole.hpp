#pragma once
// tests/devtools/one_secular_pole.hpp — SPEC-perturbations PERT-A-009, the half of it that is a property of the SOURCE: its entry point and the scanners it is made of (plan L0 step 8, group C5).
//
// `run` is the whole check: `one_secular_pole [--root DIR]`; exit 0 the secular pole has exactly one definition, 1 it has not (or the definition is gone), 2 an argument error, a source that is not
// UTF-8, or nothing to search, 70 an error the tool did not anticipate.  The ctest `tides.one_secular_pole` runs the built program; the Catch2 cases of tests/devtools/one_secular_pole_tests.cpp call
// `run` in-process on synthetic trees.

#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::one_secular_pole {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this program was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
std::filesystem::path default_root();

/// The file that must define the secular pole, relative to the root.
inline constexpr const char* kHome = "modules/gravity/include/odl/gravity/secular_pole.hpp";

/// A window of a file that carries two or more of the four constants: the 1-based line it is centred on, and the constants (in the order of CONSTANTS) found within WINDOW lines of it.
struct Adjacent {
    std::size_t line = 0;
    std::vector<std::string> constants;
    friend bool operator==(const Adjacent&, const Adjacent&) = default;
};

/// The names of the four constants -- "55.0", "1.677", "320.5", "3.460", in that order -- of TN36-7 (21) that `text` holds, each as the pattern r"\b55\.0\b" etc. (a digit string that is not part
/// of a longer word or number) finds it.
[[nodiscard]] std::vector<std::string> constants_in(std::string_view text);

/// adjacent_hits(text): windows of the file (lines split on "\n") carrying two or more of the four constants within two lines either side of the line, a window not listed again when it carries the
/// same constants in the same order as the one before it.
[[nodiscard]] std::vector<Adjacent> adjacent_hits(std::string_view text);

}  // namespace odl::tools::one_secular_pole
