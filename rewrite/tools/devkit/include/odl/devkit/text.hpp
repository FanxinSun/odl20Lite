#pragma once
// odl/devkit/text.hpp — the string behaviours the Python tools had for free and their printed output depends on.
//
// Everything here is the DOCUMENTED behaviour of the Python function it replaces, and is proved against the committed bytes that behaviour
// produced (the NOTICE, the generated headers, the baseline log of ci.sh).  Strings are UTF-8 throughout; where Python counted characters
// these count code points.

#include <odl/devkit/bytes.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace odl::devkit {

/// Decodes the code point that starts at s[i] and advances i past it; false (and i unchanged) if the bytes there are not well-formed UTF-8.
[[nodiscard]] bool next_code_point(std::string_view s, std::size_t& i, std::uint32_t& cp) noexcept;

/// Appends the UTF-8 encoding of `cp` (a scalar value: not a surrogate, not above U+10FFFF).
void append_utf8(std::string& out, std::uint32_t cp);

/// Is `s` well-formed UTF-8 (no overlongs, no surrogates, nothing above U+10FFFF)?  Python's read_text(encoding="utf-8") raises on the rest.
[[nodiscard]] bool valid_utf8(std::string_view s) noexcept;

/// The number of code points of well-formed UTF-8 (what len() of the Python str gave).
[[nodiscard]] std::size_t code_points(std::string_view s) noexcept;

/// str.ljust(width) / format spec "<width": pads on the right with spaces to `width` CODE POINTS; a longer string is returned whole.
[[nodiscard]] std::string pad_right(std::string_view s, std::size_t width);

/// str.rjust(width) / format spec ">width".
[[nodiscard]] std::string pad_left(std::string_view s, std::size_t width);

/// repr(str): the quote Python chooses (a single quote unless the string holds one and no double quote), \\, the quote, \t \n \r, other C0 controls
/// and DEL as \xNN; non-ASCII printed as-is except the code points that are certainly not printable (C1 controls, NBSP, soft hyphen, the Unicode
/// line and paragraph separators, zero-width and bidirectional controls, the byte-order mark), which Python writes as \xNN, \uNNNN or \UNNNNNNNN.
/// The exotic cases appear only in error messages about malformed input.
[[nodiscard]] std::string py_repr(std::string_view s);

/// repr(bytes): b'...' with the same quote rule, \\, \t \n \r, and every byte outside 0x20..0x7e as \xNN.
[[nodiscard]] std::string py_repr_bytes(ByteView b);

}  // namespace odl::devkit
