#pragma once
// odl/devkit/pytext.hpp — Python's str methods and textwrap.wrap, which the NOTICE generator's printed output depends on.
//
// Everything here is the DOCUMENTED behaviour of the Python function it replaces (the CPython 3 sources of textwrap.py, str.splitlines, str.rstrip,
// str.expandtabs and the UTF-8 decoder's "replace" handler), and is proved against the committed bytes that behaviour produced: tools/notice.cpp's
// output is the committed NOTICE, byte for byte.  Strings are UTF-8; where Python counted characters these count code points.
//
// ONE LIMIT, STATED.  Python's `\w` (and so textwrap's idea of a word) is Unicode-aware: str.isalnum() or '_'.  The devkit knows the letters and the
// numbers of the scripts a licence note can plausibly carry (Latin, IPA, Greek, Cyrillic, Armenian, Hebrew, Arabic, the superscripts and number forms,
// CJK, kana, Hangul, the full-width forms); a code point it does not know is NOT a word character.  The manifest's notes carry §, é, – and —, which it
// knows, and the NOTICE proof covers them.

#include <odl/devkit/bytes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace odl::devkit {

/// bytes.decode("utf-8", "replace"): each maximal invalid subsequence (the Unicode standard's "maximal subpart", which is what CPython 3.3+ does) becomes ONE
/// U+FFFD.  A lone 0xE9 (a Latin-1 é) is one; E1 80 followed by a byte that is not a continuation is one for the two bytes.
[[nodiscard]] std::string decode_utf8_replace(ByteView bytes);

/// str.isspace() of one code point: \t \n \v \f \r, 0x1c-0x1f, space, U+0085, U+00A0, U+1680, U+2000-U+200A, U+2028, U+2029, U+202F, U+205F, U+3000.
[[nodiscard]] bool is_py_space(std::uint32_t cp) noexcept;

/// Python's `\w` for one code point of a str pattern: str.isalnum() or '_' (letters, decimal digits and the other numerics).  See the limit above.
[[nodiscard]] bool is_py_word(std::uint32_t cp) noexcept;

/// Python's `\d` for one code point: the decimal digits (category Nd).
[[nodiscard]] bool is_py_decimal(std::uint32_t cp) noexcept;

/// The value, 0 to 9, of a decimal digit as int() reads it (the Arabic-Indic five is 5), or -1 for a code point that is not one (is_py_decimal).
[[nodiscard]] int py_decimal_value(std::uint32_t cp) noexcept;

/// What a text-mode read in Python hands back: universal newlines.  "\r\n" and a lone "\r" both become "\n"; everything else is as it was.
[[nodiscard]] std::string universal_newlines(std::string_view s);

/// str.rstrip(): without trailing whitespace (is_py_space).
[[nodiscard]] std::string rstrip_py(std::string_view s);

/// str.lstrip(): without leading whitespace.
[[nodiscard]] std::string lstrip_py(std::string_view s);

/// str.strip(): without leading and trailing whitespace.
[[nodiscard]] std::string strip_py(std::string_view s);

/// str.lstrip(chars): without the leading code points that are among those of `chars` (UTF-8: "<≤≈~ " is five code points, not a prefix).
[[nodiscard]] std::string lstrip_chars_py(std::string_view s, std::string_view chars);

/// str.split() with no argument: the runs of non-whitespace, in order; none is empty, and a string of nothing but whitespace gives none.
[[nodiscard]] std::vector<std::string> split_py(std::string_view s);

/// The code point that starts at byte i of s (i < s.size()) and the index after it.  A byte that does not begin well-formed UTF-8 is read as ONE U+FFFD of one byte, so that a
/// scanner steps over text that is not UTF-8 and never takes it for a word, a digit or a blank.
[[nodiscard]] std::uint32_t code_point_at(std::string_view s, std::size_t i, std::size_t& after) noexcept;

/// str.splitlines() (keepends False): \n, \r, \r\n, \v, \f, 0x1c, 0x1d, 0x1e, U+0085, U+2028, U+2029 end a line; a final terminator does not start another.
[[nodiscard]] std::vector<std::string> splitlines_py(std::string_view s);

/// str.expandtabs(tabsize): a tab becomes the blanks to the next multiple of `tabsize` columns; \n and \r reset the column.
[[nodiscard]] std::string expandtabs_py(std::string_view s, std::size_t tabsize = 8);

/// textwrap.TextWrapper's options (the ones that exist in this tree's use; defaults as Python's).
struct WrapOptions {
    std::size_t width = 70;
    std::string initial_indent;
    std::string subsequent_indent;
    bool expand_tabs = true;
    std::size_t tabsize = 8;
    bool replace_whitespace = true;
    bool drop_whitespace = true;
    bool break_long_words = true;
    bool break_on_hyphens = true;
};

/// textwrap.wrap(text, width, initial_indent, subsequent_indent, ...): the lines, each with its indent, none with a line terminator.  Throws
/// std::invalid_argument for a width of 0 and std::runtime_error for text that is not well-formed UTF-8 (Python's str cannot hold it).
[[nodiscard]] std::vector<std::string> wrap_py(std::string_view text, const WrapOptions& options);

/// difflib.unified_diff(a, b, fromfile, tofile, n=context, lineterm=""): the header lines, then the hunks.  The ALIGNMENT is a longest common subsequence,
/// where difflib's SequenceMatcher uses its own heuristics, so on an ambiguous change the hunk boundaries can differ from Python's; the format (the two
/// header lines, "@@ -a,b +c,d @@", ' ', '-' and '+') and the context grouping are difflib's.  Empty when a and b are equal.
[[nodiscard]] std::vector<std::string> unified_diff(const std::vector<std::string>& a, const std::vector<std::string>& b, std::string_view fromfile,
                                                    std::string_view tofile, std::size_t context = 3);

}  // namespace odl::devkit
