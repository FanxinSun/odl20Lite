#include <odl/devkit/text.hpp>

#include <cstdint>

namespace odl::devkit {

namespace {

[[nodiscard]] constexpr bool is_continuation(unsigned char c) noexcept { return (c & 0xC0U) == 0x80U; }

void append_hex(std::string& out, std::uint32_t v, int digits) {
    static constexpr char kDigits[] = "0123456789abcdef";
    for (int k = digits - 1; k >= 0; --k) out.push_back(kDigits[(v >> (4 * k)) & 0xFU]);
}

[[nodiscard]] bool certainly_unprintable(std::uint32_t cp) noexcept {
    return (cp >= 0x80U && cp <= 0xA0U) || cp == 0xADU || (cp >= 0x200BU && cp <= 0x200FU) || cp == 0x2028U || cp == 0x2029U ||
           (cp >= 0x202AU && cp <= 0x202EU) || (cp >= 0x2060U && cp <= 0x206FU) || cp == 0xFEFFU || (cp >= 0xFFF9U && cp <= 0xFFFBU);
}

}  // namespace

bool next_code_point(std::string_view s, std::size_t& i, std::uint32_t& cp) noexcept {
    const auto at = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
    const unsigned char c = at(i);
    if (c < 0x80U) {
        cp = c;
        i += 1;
        return true;
    }
    std::size_t len = 0;
    std::uint32_t min = 0;
    std::uint32_t v = 0;
    if ((c & 0xE0U) == 0xC0U) {
        len = 2;
        v = c & 0x1FU;
        min = 0x80U;
    } else if ((c & 0xF0U) == 0xE0U) {
        len = 3;
        v = c & 0x0FU;
        min = 0x800U;
    } else if ((c & 0xF8U) == 0xF0U) {
        len = 4;
        v = c & 0x07U;
        min = 0x10000U;
    } else {
        return false;
    }
    if (i + len > s.size()) return false;
    for (std::size_t k = 1; k < len; ++k) {
        if (!is_continuation(at(i + k))) return false;
        v = (v << 6) | (at(i + k) & 0x3FU);
    }
    if (v < min || v > 0x10FFFFU || (v >= 0xD800U && v <= 0xDFFFU)) return false;
    cp = v;
    i += len;
    return true;
}

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80U) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800U) {
        out.push_back(static_cast<char>(0xC0U | (cp >> 6)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp < 0x10000U) {
        out.push_back(static_cast<char>(0xE0U | (cp >> 12)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (cp >> 18)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    }
}

bool valid_utf8(std::string_view s) noexcept {
    std::size_t i = 0;
    std::uint32_t cp = 0;
    while (i < s.size()) {
        if (!next_code_point(s, i, cp)) return false;
    }
    return true;
}

std::size_t code_points(std::string_view s) noexcept {
    std::size_t n = 0;
    for (const char c : s) {
        if (!is_continuation(static_cast<unsigned char>(c))) ++n;
    }
    return n;
}

std::string pad_right(std::string_view s, std::size_t width) {
    std::string out(s);
    const std::size_t n = code_points(s);
    if (n < width) out.append(width - n, ' ');
    return out;
}

std::string pad_left(std::string_view s, std::size_t width) {
    const std::size_t n = code_points(s);
    std::string out;
    if (n < width) out.assign(width - n, ' ');
    out.append(s);
    return out;
}

std::string py_repr(std::string_view s) {
    bool single = false;
    bool dbl = false;
    for (const char c : s) {
        if (c == '\'') single = true;
        if (c == '"') dbl = true;
    }
    const char quote = (single && !dbl) ? '"' : '\'';
    std::string out;
    out.push_back(quote);
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint32_t cp = 0;
        const std::size_t start = i;
        if (!next_code_point(s, i, cp)) {   // not UTF-8: Python could not have held it as a str; show the byte
            out += "\\x";
            append_hex(out, static_cast<unsigned char>(s[start]), 2);
            i = start + 1;
            continue;
        }
        if (cp == static_cast<std::uint32_t>(static_cast<unsigned char>(quote)) || cp == '\\') {
            out.push_back('\\');
            out.push_back(static_cast<char>(cp));
        } else if (cp == '\t') {
            out += "\\t";
        } else if (cp == '\n') {
            out += "\\n";
        } else if (cp == '\r') {
            out += "\\r";
        } else if (cp < 0x20U || cp == 0x7FU) {
            out += "\\x";
            append_hex(out, cp, 2);
        } else if (cp < 0x7FU || !certainly_unprintable(cp)) {
            out.append(s.substr(start, i - start));
        } else if (cp < 0x100U) {
            out += "\\x";
            append_hex(out, cp, 2);
        } else if (cp < 0x10000U) {
            out += "\\u";
            append_hex(out, cp, 4);
        } else {
            out += "\\U";
            append_hex(out, cp, 8);
        }
    }
    out.push_back(quote);
    return out;
}

std::string py_repr_bytes(ByteView b) {
    bool single = false;
    bool dbl = false;
    for (const std::uint8_t c : b) {
        if (c == '\'') single = true;
        if (c == '"') dbl = true;
    }
    const char quote = (single && !dbl) ? '"' : '\'';
    std::string out = "b";
    out.push_back(quote);
    for (const std::uint8_t c : b) {
        if (c == static_cast<std::uint8_t>(quote) || c == '\\') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c == '\t') {
            out += "\\t";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c < 0x20U || c >= 0x7FU) {
            out += "\\x";
            append_hex(out, c, 2);
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    out.push_back(quote);
    return out;
}

}  // namespace odl::devkit
