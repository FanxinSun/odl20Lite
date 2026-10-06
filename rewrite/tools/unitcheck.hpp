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
/// them (a number glued to a letter, a digit or a point on either side is no token: `x1000`, `1.5.3`) -- EXTENDED in group C6 to what C++ writes: the digits may carry digit separators (`1'000`, an
/// apostrophe between two digits, in the integer part, the fraction and the exponent), and the number may end in its standard suffix (`1000u`, `1000UL`, `1000.0f`, `1e3L`: [uU](ll|LL|l|L)? or
/// (ll|LL|l|L)[uU]? after an integer literal, [fFlL] after a floating one).  The token is the literal as written, suffix and separators included.  A suffix of the other kind (`1000f`, `1000.0u`) and a
/// user-defined one (`1000_km`, `1000ms`) are not C++'s standard ones: the number stays glued to it and is no token.
[[nodiscard]] std::vector<std::string> number_tokens(std::string_view code);

/// is_thousand(tok): float(tok) is exactly 1000.0 or exactly 0.001, however the token is spelt -- a token that is one literal wholly (see number_tokens), its separators and suffix taken off.
[[nodiscard]] bool is_thousand(std::string_view token);

/// re.search(r"\bkm\b|kilomet", what, re.I): kilometres named.
[[nodiscard]] bool names_km(std::string_view what);

/// Where C++ lexing stands at the start of a line: in code, inside a block comment, inside a string literal that a backslash at the end of the previous line carried over, or inside a raw string literal
/// R"d( ... )d" (which may span lines) with its delimiter.  A character literal, an ordinary string without a backslash at its line's end, and a line comment never carry over.
struct LexState {
    enum class Mode { Code, BlockComment, String, RawString };
    Mode mode = Mode::Code;
    std::string raw_delimiter;   // the d of R"d( ... )d" while mode is RawString

    [[nodiscard]] bool in_string_literal() const noexcept { return mode == Mode::String || mode == Mode::RawString; }
};

/// Where the comment of this line starts: the offset of the first `//` that is NOT inside a string literal ("..."), a character literal ('...'), a raw string literal (prefix R, LR, uR, UR or u8R) or a block
/// comment, or std::string_view::npos when the line has no line comment.  `state` is where the line starts and is left where it ends, so that the lines of a file are given in order.  A digit separator
/// (`1'000`) does not open a character literal.  Group C6: the Python took the first `//` of the line, wherever it stood, and so lost whatever followed a `//` inside a string.
[[nodiscard]] std::size_t comment_start(std::string_view line, LexState& state);

}  // namespace scan

}  // namespace odl::tools::unitcheck
