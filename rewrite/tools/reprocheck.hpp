#pragma once
// tools/reprocheck.hpp — prove the build is reproducible, rather than assert it (plan L0 step 7; ci.sh's gate 13; plan L0 step 8, group C11).
//
// `run` is the whole tool: `reprocheck [--root DIR] [--keep]` (`--build-dir`, which the Python accepted and ignored, is refused by name since group C12).  The flags in cmake/OdlReproducible.cmake are a claim; this is the evidence.  It builds the tree twice, with cmake and Ninja,
// into two directories of the tree whose NAMES HAVE DIFFERENT LENGTHS (build-repro-a, build-repro-bbbbbbbb), and compares the SHA-256 of every artefact.  The lengths matter: the classic leak is an absolute
// path landing in debug info or an assert string, and if the two directories had the same length a leaked path would change the bytes but not their count, which a padded or truncated field would hide;
// with different lengths a leak changes the size as well as the content.
//
// An ARTEFACT is a regular file (a link to one counts) whose name ends in .a, .so or .o, or an executable file with no suffix whose path has no component named CMakeFiles.
// Exit 0 reproducible; 1 artefacts differ (or one build has artefacts the other lacks); 3 a build failed; 2 an argument error, or cmake cannot be run, or an artefact cannot be read, or no artefact was found in either build; 70 an error the tool
// did not anticipate.

#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace odl::tools::reprocheck {

inline constexpr char kTool[] = "reprocheck";

/// A fixed instant, so that anything reading a clock is pinned rather than free: 1789603200 is 2026-09-17T00:00:00Z (the Python's comment gave 2026-09-18, a day late; the number is what was used).
inline constexpr char kSourceDateEpoch[] = "1789603200";

/// The two build directories, below the tree.  Their lengths differ by seven characters.
inline constexpr char kDirA[] = "build-repro-a";
inline constexpr char kDirB[] = "build-repro-bbbbbbbb";

/// The artefacts below `out`: path relative to `out`, components joined by '/', mapped to the file; in the order of Python's sorted() on those strings (the bytes' order: the names are ASCII).
[[nodiscard]] std::map<std::string, std::filesystem::path> artefacts(const std::filesystem::path& out);

struct Settings {
    std::filesystem::path root;   // the tree to build twice: the one with the CMakeLists.txt
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::reprocheck
