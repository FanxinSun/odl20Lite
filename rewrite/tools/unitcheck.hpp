#pragma once
// tools/unitcheck.hpp — the register of every factor of a thousand in production code: its entry point and the scanners it is made of (plan L0 step 8, group C5).
//
// `run` is the whole tool: `unitcheck [--quiet] [--root DIR]`; exit 0 every factor of a thousand is accounted for, 1 one is not (or names kilometres outside core/units.hpp), 2 an argument error or
// nothing to search, 70 an error the tool did not anticipate.  The tests call it in-process on synthetic trees; ci.sh's gate 12 runs the built tool (--quiet).  The scanners are the Python tool's
// regular expressions, hand-written, exposed so that each can be tested against the semantics of the pattern it replaces.

#include <odl/devkit/tool.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::unitcheck {

int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
std::filesystem::path default_root();

namespace scan {

struct Marker {
    std::string kind;   // "UNIT-CROSSING" or "NOT-A-UNIT-CROSSING"
    std::string what;   // the text after the colon, without its trailing white space
};

/// MARKER.search(line): r"//.*?\b(NOT-A-UNIT-CROSSING|UNIT-CROSSING)\s*:\s*(.+?)\s*$".  The first `//` is where it starts; the marker name must not follow a word character; its first (leftmost)
/// occurrence that is followed by `:` is the one read.
[[nodiscard]] std::optional<Marker> marker(std::string_view line);

/// [m.group(1) for m in NUMBER.finditer(code)]: NUMBER = r"(?<![\w.])(\d+\.?\d*(?:[eE][+-]?\d+)?)(?![\w.])" -- the numeric tokens of a line of code, as the regular expression's backtracking leaves
/// them (a number glued to a letter, a digit or a point on either side is no token: `1000f`, `x1000`, `1.5.3`).
[[nodiscard]] std::vector<std::string> number_tokens(std::string_view code);

/// is_thousand(tok): float(tok) is exactly 1000.0 or exactly 0.001, however the token is spelt.
[[nodiscard]] bool is_thousand(std::string_view token);

/// re.search(r"\bkm\b|kilomet", what, re.I): kilometres named.
[[nodiscard]] bool names_km(std::string_view what);

}  // namespace scan

}  // namespace odl::tools::unitcheck
