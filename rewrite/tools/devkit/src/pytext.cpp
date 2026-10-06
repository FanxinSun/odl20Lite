#include <odl/devkit/pytext.hpp>

#include <odl/devkit/fs.hpp>
#include <odl/devkit/text.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace odl::devkit {

namespace {

struct Range {
    std::uint32_t lo;
    std::uint32_t hi;
};

template <std::size_t N>
[[nodiscard]] bool in_ranges(const std::array<Range, N>& table, std::uint32_t cp) noexcept {
    for (const Range& r : table) {
        if (cp < r.lo) return false;   // the tables are sorted
        if (cp <= r.hi) return true;
    }
    return false;
}

// Python's str.isalpha() (the letter categories Lm, Lt, Lu, Ll, Lo) for the scripts the header names.
constexpr std::array<Range, 80> kLetters = {{
    {0x41, 0x5A},     {0x61, 0x7A},     {0xAA, 0xAA},     {0xB5, 0xB5},     {0xBA, 0xBA},     {0xC0, 0xD6},     {0xD8, 0xF6},     {0xF8, 0x2C1},
    {0x2C6, 0x2D1},   {0x2E0, 0x2E4},   {0x2EC, 0x2EC},   {0x2EE, 0x2EE},   {0x370, 0x374},   {0x376, 0x377},   {0x37A, 0x37D},   {0x37F, 0x37F},
    {0x386, 0x386},   {0x388, 0x38A},   {0x38C, 0x38C},   {0x38E, 0x3A1},   {0x3A3, 0x3F5},   {0x3F7, 0x481},   {0x48A, 0x52F},   {0x531, 0x556},
    {0x559, 0x559},   {0x560, 0x588},   {0x5D0, 0x5EA},   {0x5EF, 0x5F2},   {0x620, 0x64A},   {0x66E, 0x66F},   {0x671, 0x6D3},   {0x6D5, 0x6D5},
    {0x6E5, 0x6E6},   {0x6EE, 0x6EF},   {0x6FA, 0x6FC},   {0x6FF, 0x6FF},   {0x1E00, 0x1F15}, {0x1F18, 0x1F1D}, {0x1F20, 0x1F45}, {0x1F48, 0x1F4D},
    {0x1F50, 0x1F57}, {0x1F59, 0x1F59}, {0x1F5B, 0x1F5B}, {0x1F5D, 0x1F5D}, {0x1F5F, 0x1F7D}, {0x1F80, 0x1FB4}, {0x1FB6, 0x1FBC}, {0x1FBE, 0x1FBE},
    {0x1FC2, 0x1FC4}, {0x1FC6, 0x1FCC}, {0x1FD0, 0x1FD3}, {0x1FD6, 0x1FDB}, {0x1FE0, 0x1FEC}, {0x1FF2, 0x1FF4}, {0x1FF6, 0x1FFC}, {0x2071, 0x2071},
    {0x207F, 0x207F}, {0x2090, 0x209C}, {0x2102, 0x2102}, {0x2107, 0x2107}, {0x210A, 0x2113}, {0x2115, 0x2115}, {0x2119, 0x211D}, {0x2124, 0x2124},
    {0x2126, 0x2126}, {0x2128, 0x2128}, {0x212A, 0x212D}, {0x212F, 0x2139}, {0x213C, 0x213F}, {0x2145, 0x2149}, {0x214E, 0x214E}, {0x2183, 0x2184},
    {0x3005, 0x3006}, {0x3031, 0x3035}, {0x303B, 0x303C}, {0x3041, 0x3096}, {0x309D, 0x309F}, {0x30A1, 0x30FA}, {0x30FC, 0x30FF}, {0x3105, 0x312F},
}};

// ... the rest of the letters of the same scripts, from U+3131 on.
constexpr std::array<Range, 17> kLettersHigh = {{
    {0x3131, 0x318E}, {0x31A0, 0x31BF}, {0x31F0, 0x31FF}, {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xA000, 0xA48C}, {0xAC00, 0xD7A3}, {0xF900, 0xFA6D},
    {0xFA70, 0xFAD9}, {0xFB00, 0xFB06}, {0xFF21, 0xFF3A}, {0xFF41, 0xFF5A}, {0xFF66, 0xFFBE}, {0xFFC2, 0xFFC7}, {0xFFCA, 0xFFCF}, {0xFFD2, 0xFFD7},
    {0xFFDA, 0xFFDC},
}};

// The numerics that are not decimal digits (No, Nl): isnumeric() true, \d false, \w true.
constexpr std::array<Range, 14> kOtherNumerics = {{
    {0xB2, 0xB3},     {0xB9, 0xB9},     {0xBC, 0xBE},     {0x2070, 0x2070}, {0x2074, 0x2079}, {0x2080, 0x2089}, {0x2150, 0x2182},
    {0x2185, 0x2189}, {0x2460, 0x249B}, {0x24EA, 0x24FF}, {0x2776, 0x2793}, {0x3007, 0x3007}, {0x3021, 0x3029}, {0x3038, 0x303A},
}};

// The first digit of each block of ten decimal digits (category Nd), and the mathematical digits' fifty.
constexpr std::array<std::uint32_t, 37> kDecimalBlocks = {
    0x30,   0x660,  0x6F0,  0x7C0,  0x966,  0x9E6,  0xA66,   0xAE6,   0xB66,   0xBE6,   0xC66,  0xCE6,  0xD66,
    0xDE6,  0xE50,  0xED0,  0xF20,  0x1040, 0x1090, 0x17E0,  0x1810,  0x1946,  0x19D0,  0x1A80, 0x1A90, 0x1B50,
    0x1BB0, 0x1C40, 0x1C50, 0xA620, 0xA8D0, 0xA900, 0xA9D0,  0xA9F0,  0xAA50,  0xABF0,  0xFF10,
};

[[nodiscard]] bool is_ascii_whitespace(char32_t c) noexcept { return c == U'\t' || c == U'\n' || c == 0x0B || c == 0x0C || c == U'\r' || c == U' '; }

[[nodiscard]] bool is_letter(char32_t c) noexcept { return is_py_word(c) && !is_py_decimal(c); }   // the regex class [^\d\W]

[[nodiscard]] bool is_word_punct(char32_t c) noexcept {   // the regex class [\w!"'&.,?]
    return is_py_word(c) || c == U'!' || c == U'"' || c == U'\'' || c == U'&' || c == U'.' || c == U',' || c == U'?';
}

[[nodiscard]] std::u32string to_u32(std::string_view s) {
    std::u32string out;
    out.reserve(s.size());
    std::size_t i = 0;
    std::uint32_t cp = 0;
    while (i < s.size()) {
        if (!next_code_point(s, i, cp)) throw std::runtime_error("text is not well-formed UTF-8");
        out.push_back(static_cast<char32_t>(cp));
    }
    return out;
}

[[nodiscard]] std::string to_utf8(std::u32string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char32_t c : s) append_utf8(out, static_cast<std::uint32_t>(c));
    return out;
}

[[nodiscard]] bool all_space(const std::u32string& s) noexcept {
    return std::all_of(s.begin(), s.end(), [](char32_t c) { return is_py_space(static_cast<std::uint32_t>(c)); });
}

// ------------------------------------------------------------------------------------------------------------------------ textwrap

// textwrap.TextWrapper.wordsep_re.split(text), empty chunks removed.  The regular expression, from CPython's textwrap.py:
//
//   ( whitespace+                                              (the ASCII whitespace \t \n \v \f \r and blank)
//   | (?<=[\w!"'&.,?]) -{2,} (?=\w)                            an em-dash between words
//   | [^whitespace]+? ( -(?: (?<=LL-) | (?<=L-L-)) (?= L-?L )  a hyphenated word: split AFTER the hyphen if two letters precede it
//                     | (?= whitespace | \Z )                  the end of a word
//                     | (?<=[\w!"'&.,?]) (?= -{2,} \w ) ) )     an em-dash after a word          (L = [^\d\W], a letter)
//
// Tried at each position in the order written; the lazy `+?` takes the smallest k for which one of its three endings holds, in the order written.
[[nodiscard]] std::vector<std::u32string> split_chunks(const std::u32string& s, bool break_on_hyphens) {
    std::vector<std::u32string> out;
    const std::size_t n = s.size();
    std::size_t p = 0;
    if (!break_on_hyphens) {   // wordsep_simple_re: (whitespace+) -- the runs of whitespace and what lies between them
        while (p < n) {
            std::size_t q = p;
            const bool ws = is_ascii_whitespace(s[p]);
            while (q < n && is_ascii_whitespace(s[q]) == ws) ++q;
            out.push_back(s.substr(p, q - p));
            p = q;
        }
        return out;
    }
    while (p < n) {
        if (is_ascii_whitespace(s[p])) {
            std::size_t q = p;
            while (q < n && is_ascii_whitespace(s[q])) ++q;
            out.push_back(s.substr(p, q - p));
            p = q;
            continue;
        }
        if (p > 0 && is_word_punct(s[p - 1])) {   // an em-dash between words: the whole run of hyphens, when a word character follows it
            std::size_t q = p;
            while (q < n && s[q] == U'-') ++q;
            if (q - p >= 2 && q < n && is_py_word(static_cast<std::uint32_t>(s[q]))) {
                out.push_back(s.substr(p, q - p));
                p = q;
                continue;
            }
        }
        std::size_t run_end = p;
        while (run_end < n && !is_ascii_whitespace(s[run_end])) ++run_end;
        bool matched = false;
        for (std::size_t k = 1; p + k <= run_end && !matched; ++k) {
            const std::size_t q = p + k;
            if (q < n && s[q] == U'-') {   // a hyphenated word: the hyphen s[q] is part of the chunk
                const std::size_t r = q + 1;
                const bool behind = (r >= 3 && is_letter(s[r - 3]) && is_letter(s[r - 2])) ||
                                    (r >= 4 && is_letter(s[r - 4]) && s[r - 3] == U'-' && is_letter(s[r - 2]));
                bool ahead = false;
                if (r < n && is_letter(s[r])) {
                    // `-?` takes a hyphen when there is one, and then a letter must follow it; without one, a letter must follow directly
                    ahead = (r + 1 < n && s[r + 1] == U'-') ? (r + 2 < n && is_letter(s[r + 2])) : (r + 1 < n && is_letter(s[r + 1]));
                }
                if (behind && ahead) {
                    out.push_back(s.substr(p, r - p));
                    p = r;
                    matched = true;
                    continue;
                }
            }
            if (q == n || is_ascii_whitespace(s[q])) {   // the end of a word
                out.push_back(s.substr(p, q - p));
                p = q;
                matched = true;
                continue;
            }
            if (is_word_punct(s[q - 1])) {   // an em-dash after a word
                std::size_t t = q;
                while (t < n && s[t] == U'-') ++t;
                if (t - q >= 2 && t < n && is_py_word(static_cast<std::uint32_t>(s[t]))) {
                    out.push_back(s.substr(p, q - p));
                    p = q;
                    matched = true;
                }
            }
        }
        if (!matched) {   // cannot happen: at k = the run's length the end of a word holds; here so that a defect cannot loop for ever
            out.push_back(s.substr(p, run_end - p));
            p = run_end;
        }
    }
    return out;
}

// TextWrapper._handle_long_word: a chunk too long for any line.
void handle_long_word(std::vector<std::u32string>& reversed_chunks, std::vector<std::u32string>& cur_line, std::size_t cur_len, long long width,
                      const WrapOptions& options) {
    const long long space_left = width < 1 ? 1 : width - static_cast<long long>(cur_len);
    if (options.break_long_words) {
        std::u32string& chunk = reversed_chunks.back();
        long long end = space_left;
        if (options.break_on_hyphens && static_cast<long long>(chunk.size()) > space_left) {
            // break after the last hyphen of the first space_left characters, but only if there is a non-hyphen before it
            long long hyphen = -1;
            for (long long i = std::min<long long>(space_left, static_cast<long long>(chunk.size())) - 1; i >= 0; --i) {
                if (chunk[static_cast<std::size_t>(i)] == U'-') {
                    hyphen = i;
                    break;
                }
            }
            if (hyphen > 0) {
                bool non_hyphen = false;
                for (long long i = 0; i < hyphen; ++i) non_hyphen = non_hyphen || chunk[static_cast<std::size_t>(i)] != U'-';
                if (non_hyphen) end = hyphen + 1;
            }
        }
        const std::size_t cut = static_cast<std::size_t>(std::min<long long>(end, static_cast<long long>(chunk.size())));
        cur_line.push_back(chunk.substr(0, cut));
        chunk.erase(0, cut);
    } else if (cur_line.empty()) {
        cur_line.push_back(std::move(reversed_chunks.back()));
        reversed_chunks.pop_back();
    }
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------------ Unicode classes

bool is_py_space(std::uint32_t cp) noexcept {
    return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x20) || cp == 0x85 || cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
           cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

bool is_py_decimal(std::uint32_t cp) noexcept {
    if (cp >= 0x1D7CE && cp <= 0x1D7FF) return true;
    for (const std::uint32_t first : kDecimalBlocks) {
        if (cp >= first && cp < first + 10) return true;
    }
    return false;
}

int py_decimal_value(std::uint32_t cp) noexcept {
    if (cp >= 0x1D7CE && cp <= 0x1D7FF) return static_cast<int>((cp - 0x1D7CE) % 10);   // the fifty mathematical digits: five runs of ten
    for (const std::uint32_t first : kDecimalBlocks) {
        if (cp >= first && cp < first + 10) return static_cast<int>(cp - first);
    }
    return -1;
}

bool is_py_word(std::uint32_t cp) noexcept {
    return cp == U'_' || in_ranges(kLetters, cp) || in_ranges(kLettersHigh, cp) || in_ranges(kOtherNumerics, cp) || is_py_decimal(cp);
}

std::string universal_newlines(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') {
            out.push_back('\n');
            if (i + 1 < s.size() && s[i + 1] == '\n') ++i;   // "\r\n" is one line end, not two
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string read_text_lossy(const std::filesystem::path& file) {
    const Bytes b = read_bytes(file);
    return universal_newlines(decode_utf8_replace(ByteView{b.data(), b.size()}));
}

// ------------------------------------------------------------------------------------------------------------------------ str methods

std::string decode_utf8_replace(ByteView bytes) {
    std::string out;
    out.reserve(bytes.size());
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    const auto in_range = [&](std::size_t k, unsigned lo, unsigned hi) { return k < n && bytes[k] >= lo && bytes[k] <= hi; };
    while (i < n) {
        const unsigned c = bytes[i];
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        std::size_t need = 0;
        unsigned lo2 = 0x80;
        unsigned hi2 = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) {
            need = 1;
        } else if (c >= 0xE0 && c <= 0xEF) {
            need = 2;
            if (c == 0xE0) lo2 = 0xA0;
            if (c == 0xED) hi2 = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 3;
            if (c == 0xF0) lo2 = 0x90;
            if (c == 0xF4) hi2 = 0x8F;
        } else {   // a continuation byte or 0xC0 0xC1 0xF5..0xFF with nothing to continue: one replacement for the byte
            append_utf8(out, 0xFFFD);
            ++i;
            continue;
        }
        std::size_t k = 1;
        bool ok = true;
        for (; k <= need; ++k) {
            if (!in_range(i + k, k == 1 ? lo2 : 0x80, k == 1 ? hi2 : 0xBF)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            std::uint32_t cp = need == 1 ? (c & 0x1FU) : need == 2 ? (c & 0x0FU) : (c & 0x07U);
            for (std::size_t j = 1; j <= need; ++j) cp = (cp << 6) | (bytes[i + j] & 0x3FU);
            append_utf8(out, cp);
            i += need + 1;
        } else {   // the lead byte and the continuation bytes that were valid so far: ONE replacement
            append_utf8(out, 0xFFFD);
            i += k;
        }
    }
    return out;
}

std::string rstrip_py(std::string_view s) {
    // Walk back over whole code points; a byte sequence that is not well-formed stops the strip there (a str cannot hold it).
    std::size_t end = s.size();
    while (end > 0) {
        std::size_t start = end - 1;
        while (start > 0 && (static_cast<unsigned char>(s[start]) & 0xC0U) == 0x80U) --start;
        std::size_t at = start;
        std::uint32_t cp = 0;
        if (!next_code_point(s.substr(0, end), at, cp) || at != end || !is_py_space(cp)) break;
        end = start;
    }
    return std::string(s.substr(0, end));
}

std::uint32_t code_point_at(std::string_view s, std::size_t i, std::size_t& after) noexcept {
    std::size_t j = i;
    std::uint32_t cp = 0;
    if (next_code_point(s, j, cp)) {
        after = j;
        return cp;
    }
    after = i + 1;
    return 0xFFFD;
}

bool code_point_before(std::string_view s, std::size_t end, std::size_t& start, std::uint32_t& cp) noexcept {
    if (end == 0 || end > s.size()) return false;
    start = end - 1;
    while (start > 0 && (static_cast<unsigned char>(s[start]) & 0xC0U) == 0x80U) --start;   // back over the continuation bytes to where a code point would begin
    std::size_t after = start;
    return next_code_point(s, after, cp) && after == end;   // a code point from there that ends exactly at `end`; next_code_point moves `after` only when it succeeds
}

bool word_boundary_at(std::string_view s, std::size_t i) noexcept {
    std::size_t start = 0;
    std::uint32_t cp = 0;
    const bool before = code_point_before(s, i, start, cp) && is_py_word(cp);   // false at the start of the text, past its end, and where the bytes are not a code point
    std::size_t next = 0;
    const bool after = i < s.size() && is_py_word(code_point_at(s, i, next));   // code_point_at reads the byte at i: it must be there
    return before != after;
}

std::string lstrip_py(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t at = i;
        std::uint32_t cp = 0;
        if (!next_code_point(s, at, cp) || !is_py_space(cp)) break;   // text that is not well-formed ends the strip, as in rstrip_py
        i = at;
    }
    return std::string(s.substr(i));
}

std::string strip_py(std::string_view s) { return rstrip_py(lstrip_py(s)); }

std::string lstrip_chars_py(std::string_view s, std::string_view chars) {
    std::vector<std::uint32_t> set;
    for (std::size_t i = 0; i < chars.size();) {
        std::size_t after = 0;
        set.push_back(code_point_at(chars, i, after));
        i = after;
    }
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t after = 0;
        const std::uint32_t cp = code_point_at(s, i, after);
        if (std::find(set.begin(), set.end(), cp) == set.end()) break;
        i = after;
    }
    return std::string(s.substr(i));
}

std::vector<std::string> split_py(std::string_view s) {
    std::vector<std::string> words;
    std::string cur;
    for (std::size_t i = 0; i < s.size();) {
        std::size_t after = 0;
        const std::uint32_t cp = code_point_at(s, i, after);
        if (is_py_space(cp)) {
            if (!cur.empty()) words.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.append(s.substr(i, after - i));
        }
        i = after;
    }
    if (!cur.empty()) words.push_back(std::move(cur));
    return words;
}

std::vector<std::string> splitlines_py(std::string_view s) {
    std::vector<std::string> lines;
    std::string cur;
    std::size_t i = 0;
    bool open = false;   // characters of an unfinished line have been seen
    while (i < s.size()) {
        std::size_t at = i;
        std::uint32_t cp = 0;
        if (!next_code_point(s, at, cp)) throw std::runtime_error("text is not well-formed UTF-8");
        const bool boundary = cp == 0x0A || cp == 0x0D || cp == 0x0B || cp == 0x0C || cp == 0x1C || cp == 0x1D || cp == 0x1E || cp == 0x85 || cp == 0x2028 ||
                              cp == 0x2029;
        if (boundary) {
            lines.push_back(cur);
            cur.clear();
            open = false;
            if (cp == 0x0D && at < s.size() && s[at] == '\n') ++at;   // \r\n is one boundary
        } else {
            cur.append(s.substr(i, at - i));
            open = true;
        }
        i = at;
    }
    if (open) lines.push_back(cur);
    return lines;
}

std::string expandtabs_py(std::string_view s, std::size_t tabsize) {
    std::string out;
    std::size_t column = 0;
    std::size_t i = 0;
    std::uint32_t cp = 0;
    while (i < s.size()) {
        const std::size_t before = i;
        if (!next_code_point(s, i, cp)) throw std::runtime_error("text is not well-formed UTF-8");
        if (cp == U'\t') {
            if (tabsize > 0) {
                const std::size_t spaces = tabsize - (column % tabsize);
                out.append(spaces, ' ');
                column += spaces;
            }
        } else {
            out.append(s.substr(before, i - before));
            if (cp == U'\n' || cp == U'\r') column = 0;
            else ++column;
        }
    }
    return out;
}

std::vector<std::string> wrap_py(std::string_view text, const WrapOptions& options) {
    if (options.width == 0) throw std::invalid_argument("invalid width 0 (must be > 0)");
    // _munge_whitespace
    std::u32string s = to_u32(options.expand_tabs ? expandtabs_py(text, options.tabsize) : std::string(text));
    if (options.replace_whitespace) {
        for (char32_t& c : s) {
            if (is_ascii_whitespace(c)) c = U' ';
        }
    }
    // _split, then _wrap_chunks
    std::vector<std::u32string> chunks = split_chunks(s, options.break_on_hyphens);
    chunks.erase(std::remove_if(chunks.begin(), chunks.end(), [](const std::u32string& c) { return c.empty(); }), chunks.end());
    std::reverse(chunks.begin(), chunks.end());   // a stack: the next chunk is at the back
    const std::u32string initial = to_u32(options.initial_indent);
    const std::u32string subsequent = to_u32(options.subsequent_indent);
    std::vector<std::string> lines;
    while (!chunks.empty()) {
        std::vector<std::u32string> cur_line;
        std::size_t cur_len = 0;
        const std::u32string& indent = lines.empty() ? initial : subsequent;
        const long long width = static_cast<long long>(options.width) - static_cast<long long>(indent.size());
        if (options.drop_whitespace && all_space(chunks.back()) && !lines.empty()) chunks.pop_back();   // whitespace at the start of a line goes
        while (!chunks.empty()) {
            const std::size_t length = chunks.back().size();
            if (static_cast<long long>(cur_len + length) <= width) {
                cur_line.push_back(std::move(chunks.back()));
                chunks.pop_back();
                cur_len += length;
            } else {
                break;
            }
        }
        if (!chunks.empty() && static_cast<long long>(chunks.back().size()) > width) {
            handle_long_word(chunks, cur_line, cur_len, width, options);
            cur_len = 0;
            for (const std::u32string& c : cur_line) cur_len += c.size();
        }
        if (options.drop_whitespace && !cur_line.empty() && all_space(cur_line.back())) {   // and whitespace at the end
            cur_len -= cur_line.back().size();
            cur_line.pop_back();
        }
        if (!cur_line.empty()) {
            std::u32string line = indent;
            for (const std::u32string& c : cur_line) line += c;
            lines.push_back(to_utf8(line));
        }
    }
    return lines;
}

// ------------------------------------------------------------------------------------------------------------------------ difflib

namespace {

struct Opcode {
    char tag;   // 'e' equal, 'r' replace, 'd' delete, 'i' insert
    std::size_t i1, i2, j1, j2;
};

// The matched index pairs of a longest common subsequence of a and b, in order (a common prefix and suffix first, then a table over the middle).
[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> matched_pairs(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    std::size_t prefix = 0;
    while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix]) ++prefix;
    std::size_t suffix = 0;
    while (suffix < a.size() - prefix && suffix < b.size() - prefix && a[a.size() - 1 - suffix] == b[b.size() - 1 - suffix]) ++suffix;
    for (std::size_t k = 0; k < prefix; ++k) pairs.emplace_back(k, k);
    const std::size_t la = a.size() - prefix - suffix;
    const std::size_t lb = b.size() - prefix - suffix;
    constexpr std::size_t kMaxCells = std::size_t{16} * 1000 * 1000;   // beyond this the middle is reported as one replacement
    if (la > 0 && lb > 0 && (la + 1) * (lb + 1) <= kMaxCells) {
        std::vector<std::uint16_t> table((la + 1) * (lb + 1), 0);   // table[x][y]: the length of the LCS of a[prefix+x..], b[prefix+y..]
        const auto cell = [&](std::size_t x, std::size_t y) -> std::uint16_t& { return table[x * (lb + 1) + y]; };
        for (std::size_t x = la; x-- > 0;) {
            for (std::size_t y = lb; y-- > 0;) {
                cell(x, y) = a[prefix + x] == b[prefix + y] ? static_cast<std::uint16_t>(cell(x + 1, y + 1) + 1) : std::max(cell(x + 1, y), cell(x, y + 1));
            }
        }
        std::size_t x = 0;
        std::size_t y = 0;
        while (x < la && y < lb) {
            if (a[prefix + x] == b[prefix + y]) {
                pairs.emplace_back(prefix + x, prefix + y);
                ++x;
                ++y;
            } else if (cell(x + 1, y) >= cell(x, y + 1)) {
                ++x;
            } else {
                ++y;
            }
        }
    }
    for (std::size_t k = 0; k < suffix; ++k) pairs.emplace_back(a.size() - suffix + k, b.size() - suffix + k);
    return pairs;
}

[[nodiscard]] std::vector<Opcode> opcodes_of(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    std::vector<Opcode> codes;
    const auto pairs = matched_pairs(a, b);
    std::size_t i = 0;
    std::size_t j = 0;
    std::size_t k = 0;
    while (k <= pairs.size()) {
        std::size_t ai = a.size();
        std::size_t bj = b.size();
        std::size_t size = 0;
        if (k < pairs.size()) {   // a maximal block of consecutive pairs
            ai = pairs[k].first;
            bj = pairs[k].second;
            size = 1;
            while (k + size < pairs.size() && pairs[k + size].first == ai + size && pairs[k + size].second == bj + size) ++size;
        }
        if (i < ai && j < bj) codes.push_back({'r', i, ai, j, bj});
        else if (i < ai) codes.push_back({'d', i, ai, j, bj});
        else if (j < bj) codes.push_back({'i', i, ai, j, bj});
        if (size > 0) codes.push_back({'e', ai, ai + size, bj, bj + size});
        i = ai + size;
        j = bj + size;
        k += size > 0 ? size : 1;
    }
    return codes;
}

[[nodiscard]] std::string format_range(std::size_t start, std::size_t stop) {   // difflib._format_range_unified
    std::size_t beginning = start + 1;
    const std::size_t length = stop - start;
    if (length == 1) return std::to_string(beginning);
    if (length == 0) --beginning;
    return std::to_string(beginning) + "," + std::to_string(length);
}

}  // namespace

std::vector<std::string> unified_diff(const std::vector<std::string>& a, const std::vector<std::string>& b, std::string_view fromfile, std::string_view tofile,
                                      std::size_t context) {
    std::vector<std::string> out;
    std::vector<Opcode> codes = opcodes_of(a, b);
    if (codes.empty()) codes.push_back({'e', 0, 1, 0, 1});
    // get_grouped_opcodes: trim the context at both ends, and split wherever an unchanged run exceeds twice the context
    if (codes.front().tag == 'e') {
        Opcode& c = codes.front();
        c = {'e', std::max(c.i1, c.i2 > context ? c.i2 - context : 0), c.i2, std::max(c.j1, c.j2 > context ? c.j2 - context : 0), c.j2};
    }
    if (codes.back().tag == 'e') {
        Opcode& c = codes.back();
        c = {'e', c.i1, std::min(c.i2, c.i1 + context), c.j1, std::min(c.j2, c.j1 + context)};
    }
    const std::size_t nn = context + context;
    std::vector<std::vector<Opcode>> groups;
    std::vector<Opcode> group;
    for (Opcode c : codes) {
        if (c.tag == 'e' && c.i2 - c.i1 > nn) {
            group.push_back({'e', c.i1, std::min(c.i2, c.i1 + context), c.j1, std::min(c.j2, c.j1 + context)});
            groups.push_back(group);
            group.clear();
            c.i1 = std::max(c.i1, c.i2 > context ? c.i2 - context : 0);
            c.j1 = std::max(c.j1, c.j2 > context ? c.j2 - context : 0);
        }
        group.push_back(c);
    }
    if (!group.empty() && !(group.size() == 1 && group.front().tag == 'e')) groups.push_back(group);
    bool started = false;
    for (const std::vector<Opcode>& g : groups) {
        if (!started) {
            started = true;
            out.push_back("--- " + std::string(fromfile));
            out.push_back("+++ " + std::string(tofile));
        }
        out.push_back("@@ -" + format_range(g.front().i1, g.back().i2) + " +" + format_range(g.front().j1, g.back().j2) + " @@");
        for (const Opcode& c : g) {
            if (c.tag == 'e') {
                for (std::size_t x = c.i1; x < c.i2; ++x) out.push_back(" " + a[x]);
                continue;
            }
            if (c.tag == 'r' || c.tag == 'd') {
                for (std::size_t x = c.i1; x < c.i2; ++x) out.push_back("-" + a[x]);
            }
            if (c.tag == 'r' || c.tag == 'i') {
                for (std::size_t y = c.j1; y < c.j2; ++y) out.push_back("+" + b[y]);
            }
        }
    }
    return out;
}

}  // namespace odl::devkit
