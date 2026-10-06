#include <odl/devkit/json.hpp>

#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/text.hpp>

#include <charconv>
#include <cmath>
#include <limits>

namespace odl::devkit {

namespace {

constexpr std::size_t kMaxDepth = 512;

[[nodiscard]] constexpr bool is_ws(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
[[nodiscard]] constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

[[nodiscard]] int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}

    Json document() {
        std::size_t i = skip_ws(0);
        Json v = value(i, 0);
        i = skip_ws(i);
        if (i != s_.size()) fail("Extra data", i);
        return v;
    }

private:
    [[noreturn]] void fail(const char* reason, std::size_t pos) const {
        std::size_t line = 1;
        std::size_t line_start = 0;
        for (std::size_t k = 0; k < pos && k < s_.size(); ++k) {
            if (s_[k] == '\n') {
                ++line;
                line_start = k + 1;
            }
        }
        const std::size_t end = pos < s_.size() ? pos : s_.size();
        const std::size_t column = code_points(s_.substr(line_start, end - line_start)) + 1;
        throw JsonError(std::string(reason), line, column, pos);
    }

    [[nodiscard]] std::size_t skip_ws(std::size_t i) const noexcept {
        while (i < s_.size() && is_ws(s_[i])) ++i;
        return i;
    }

    // `i` is where the value must start; on return it is just past it.
    Json value(std::size_t& i, std::size_t depth) {
        if (i >= s_.size()) fail("Expecting value", i);
        const char c = s_[i];
        if (c == '"') return Json(string(i));
        if (c == '{') return object(i, depth);
        if (c == '[') return array(i, depth);
        if (c == 't' && s_.substr(i, 4) == "true") {
            i += 4;
            return Json(true);
        }
        if (c == 'f' && s_.substr(i, 5) == "false") {
            i += 5;
            return Json(false);
        }
        if (c == 'n' && s_.substr(i, 4) == "null") {
            i += 4;
            return Json();
        }
        if (c == '-' || is_digit(c)) return number(i);
        fail("Expecting value", i);
    }

    Json object(std::size_t& i, std::size_t depth) {
        if (depth >= kMaxDepth) fail("Nesting too deep", i);
        ++i;   // {
        Json::Object members;
        i = skip_ws(i);
        if (i < s_.size() && s_[i] == '}') {
            ++i;
            return Json(std::move(members));
        }
        for (;;) {
            if (i >= s_.size() || s_[i] != '"') fail("Expecting property name enclosed in double quotes", i);
            std::string key = string(i);
            i = skip_ws(i);
            if (i >= s_.size() || s_[i] != ':') fail("Expecting ':' delimiter", i);
            i = skip_ws(i + 1);
            Json v = value(i, depth + 1);
            // Python's dict: a repeated key keeps its FIRST position and takes the LAST value.
            bool replaced = false;
            for (auto& m : members) {
                if (m.first == key) {
                    m.second = std::move(v);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) members.emplace_back(std::move(key), std::move(v));
            i = skip_ws(i);
            if (i < s_.size() && s_[i] == '}') {
                ++i;
                return Json(std::move(members));
            }
            if (i >= s_.size() || s_[i] != ',') fail("Expecting ',' delimiter", i);
            i = skip_ws(i + 1);
        }
    }

    Json array(std::size_t& i, std::size_t depth) {
        if (depth >= kMaxDepth) fail("Nesting too deep", i);
        ++i;   // [
        Json::Array items;
        i = skip_ws(i);
        if (i < s_.size() && s_[i] == ']') {
            ++i;
            return Json(std::move(items));
        }
        for (;;) {
            items.push_back(value(i, depth + 1));
            i = skip_ws(i);
            if (i < s_.size() && s_[i] == ']') {
                ++i;
                return Json(std::move(items));
            }
            if (i >= s_.size() || s_[i] != ',') fail("Expecting ',' delimiter", i);
            i = skip_ws(i + 1);
        }
    }

    // `i` is at the opening quote; on return just past the closing one.
    std::string string(std::size_t& i) {
        const std::size_t begin = i;
        ++i;
        std::string out;
        for (;;) {
            if (i >= s_.size()) fail("Unterminated string starting at", begin);
            const char c = s_[i];
            if (c == '"') {
                ++i;
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20U) fail("Invalid control character at", i);
            if (c != '\\') {
                out.push_back(c);
                ++i;
                continue;
            }
            if (i + 1 >= s_.size()) fail("Unterminated string starting at", begin);
            const char e = s_[i + 1];
            switch (e) {
                case '"': out.push_back('"'); i += 2; break;
                case '\\': out.push_back('\\'); i += 2; break;
                case '/': out.push_back('/'); i += 2; break;
                case 'b': out.push_back('\b'); i += 2; break;
                case 'f': out.push_back('\f'); i += 2; break;
                case 'n': out.push_back('\n'); i += 2; break;
                case 'r': out.push_back('\r'); i += 2; break;
                case 't': out.push_back('\t'); i += 2; break;
                case 'u': {
                    std::uint32_t cp = hex4(i);
                    i += 6;
                    if (cp >= 0xD800U && cp <= 0xDBFFU) {
                        if (i + 1 < s_.size() && s_[i] == '\\' && s_[i + 1] == 'u') {
                            const std::uint32_t lo = hex4(i);
                            if (lo >= 0xDC00U && lo <= 0xDFFFU) {
                                cp = 0x10000U + ((cp - 0xD800U) << 10) + (lo - 0xDC00U);
                                i += 6;
                            } else {
                                fail("Invalid \\uXXXX escape: lone surrogate", i);
                            }
                        } else {
                            fail("Invalid \\uXXXX escape: lone surrogate", i);
                        }
                    } else if (cp >= 0xDC00U && cp <= 0xDFFFU) {
                        fail("Invalid \\uXXXX escape: lone surrogate", i - 6);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail("Invalid \\escape", i);
            }
        }
    }

    // `i` is at a backslash followed by u: the four hexadecimal digits after them.
    std::uint32_t hex4(std::size_t i) const {
        if (i + 6 > s_.size()) fail("Invalid \\uXXXX escape", i + 1);
        std::uint32_t v = 0;
        for (std::size_t k = 2; k < 6; ++k) {
            const int h = hex_value(s_[i + k]);
            if (h < 0) fail("Invalid \\uXXXX escape", i + 1);
            v = (v << 4) | static_cast<std::uint32_t>(h);
        }
        return v;
    }

    Json number(std::size_t& i) {
        const std::size_t start = i;
        std::size_t k = i;
        if (s_[k] == '-') {
            ++k;
            if (k >= s_.size()) fail("Expecting value", start);
        }
        if (s_[k] >= '1' && s_[k] <= '9') {
            while (k < s_.size() && is_digit(s_[k])) ++k;
        } else if (s_[k] == '0') {
            ++k;
        } else {
            fail("Expecting value", start);
        }
        bool is_float = false;
        if (k + 1 < s_.size() && s_[k] == '.' && is_digit(s_[k + 1])) {
            is_float = true;
            k += 2;
            while (k < s_.size() && is_digit(s_[k])) ++k;
        }
        if (k + 1 < s_.size() && (s_[k] == 'e' || s_[k] == 'E')) {
            std::size_t e = k + 1;
            if (e + 1 < s_.size() && (s_[e] == '-' || s_[e] == '+')) ++e;
            std::size_t digits_from = e;
            while (e < s_.size() && is_digit(s_[e])) ++e;
            if (e > digits_from) {
                is_float = true;
                k = e;
            }
        }
        const char* first = s_.data() + start;
        const char* last = s_.data() + k;
        i = k;
        if (is_float) {
            double d = 0.0;
            const auto r = std::from_chars(first, last, d);
            if (r.ec != std::errc() || r.ptr != last || !std::isfinite(d)) fail("Number out of range", start);
            return Json(d);
        }
        std::int64_t n = 0;
        const auto r = std::from_chars(first, last, n);
        if (r.ec != std::errc() || r.ptr != last) fail("Number out of range", start);
        return Json(n);
    }

    std::string_view s_;
};

void write_string(std::string& out, const std::string& s, bool ascii) {
    static constexpr char kDigits[] = "0123456789abcdef";
    const auto hex4 = [&](std::uint32_t v) {
        out += "\\u";
        for (int k = 3; k >= 0; --k) out.push_back(kDigits[(v >> (4 * k)) & 0xFU]);
    };
    out.push_back('"');
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint32_t cp = 0;
        if (!next_code_point(s, i, cp)) throw std::invalid_argument("json: a string is not well-formed UTF-8");
        switch (cp) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (cp >= 0x20U && cp < 0x7FU) {
                    out.push_back(static_cast<char>(cp));
                } else if (cp < 0x20U) {
                    hex4(cp);
                } else if (!ascii) {
                    append_utf8(out, cp);   // ensure_ascii=False: DEL and everything above it stays as it is
                } else if (cp < 0x10000U) {
                    hex4(cp);
                } else {
                    const std::uint32_t v = cp - 0x10000U;
                    hex4(0xD800U | (v >> 10));
                    hex4(0xDC00U | (v & 0x3FFU));
                }
        }
    }
    out.push_back('"');
}

void write(std::string& out, const Json& v, int indent, int level, bool ascii) {
    const auto newline = [&](int lvl) {
        out.push_back('\n');
        out.append(static_cast<std::size_t>(indent * lvl), ' ');
    };
    switch (v.kind()) {
        case Json::Kind::Null: out += "null"; return;
        case Json::Kind::Bool: out += v.as_bool() ? "true" : "false"; return;
        case Json::Kind::Int: out += std::to_string(v.as_int()); return;
        case Json::Kind::Float: {
            const double d = v.as_double();
            if (!std::isfinite(d)) throw std::invalid_argument("json: a non-finite number cannot be written");
            out += py_float_repr(d);
            return;
        }
        case Json::Kind::String: write_string(out, v.as_string(), ascii); return;
        case Json::Kind::Array: {
            const auto& a = v.as_array();
            if (a.empty()) {
                out += "[]";
                return;
            }
            out.push_back('[');
            for (std::size_t k = 0; k < a.size(); ++k) {
                if (k > 0) out.push_back(',');
                newline(level + 1);
                write(out, a[k], indent, level + 1, ascii);
            }
            newline(level);
            out.push_back(']');
            return;
        }
        case Json::Kind::Object: {
            const auto& o = v.as_object();
            if (o.empty()) {
                out += "{}";
                return;
            }
            out.push_back('{');
            for (std::size_t k = 0; k < o.size(); ++k) {
                if (k > 0) out.push_back(',');
                newline(level + 1);
                write_string(out, o[k].first, ascii);
                out += ": ";
                write(out, o[k].second, indent, level + 1, ascii);
            }
            newline(level);
            out.push_back('}');
            return;
        }
    }
}

[[noreturn]] void wrong_kind(const char* wanted) {
    throw std::logic_error(std::string("json: the value is not ") + wanted);
}

}  // namespace

JsonError::JsonError(std::string reason_, std::size_t line_, std::size_t column_, std::size_t position_)
    : std::runtime_error(reason_ + ": line " + std::to_string(line_) + " column " + std::to_string(column_) + " (char " + std::to_string(position_) + ")"),
      reason(std::move(reason_)),
      line(line_),
      column(column_),
      position(position_) {}

bool Json::as_bool() const {
    if (const bool* p = std::get_if<bool>(&v_)) return *p;
    wrong_kind("a boolean");
}

std::int64_t Json::as_int() const {
    if (const auto* p = std::get_if<std::int64_t>(&v_)) return *p;
    wrong_kind("an integer");
}

double Json::as_double() const {
    if (const auto* p = std::get_if<double>(&v_)) return *p;
    if (const auto* q = std::get_if<std::int64_t>(&v_)) return static_cast<double>(*q);
    wrong_kind("a number");
}

const std::string& Json::as_string() const {
    if (const auto* p = std::get_if<std::string>(&v_)) return *p;
    wrong_kind("a string");
}

const Json::Array& Json::as_array() const {
    if (const auto* p = std::get_if<Array>(&v_)) return *p;
    wrong_kind("an array");
}

Json::Array& Json::as_array() {
    if (auto* p = std::get_if<Array>(&v_)) return *p;
    wrong_kind("an array");
}

const Json::Object& Json::as_object() const {
    if (const auto* p = std::get_if<Object>(&v_)) return *p;
    wrong_kind("an object");
}

Json::Object& Json::as_object() {
    if (auto* p = std::get_if<Object>(&v_)) return *p;
    wrong_kind("an object");
}

const Json* Json::find(std::string_view key) const noexcept {
    const auto* o = std::get_if<Object>(&v_);
    if (o == nullptr) return nullptr;
    for (const auto& m : *o) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

Json* Json::find(std::string_view key) noexcept {
    auto* o = std::get_if<Object>(&v_);
    if (o == nullptr) return nullptr;
    for (auto& m : *o) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

void Json::set(std::string key, Json value) {
    auto& o = as_object();
    for (auto& m : o) {
        if (m.first == key) {
            m.second = std::move(value);
            return;
        }
    }
    o.emplace_back(std::move(key), std::move(value));
}

bool Json::truthy() const noexcept {
    switch (kind()) {
        case Kind::Null: return false;
        case Kind::Bool: return std::get<bool>(v_);
        case Kind::Int: return std::get<std::int64_t>(v_) != 0;
        case Kind::Float: return std::get<double>(v_) != 0.0;
        case Kind::String: return !std::get<std::string>(v_).empty();
        case Kind::Array: return !std::get<Array>(v_).empty();
        case Kind::Object: return !std::get<Object>(v_).empty();
    }
    return false;
}

Json Json::parse(std::string_view text) { return Parser(text).document(); }

std::string Json::dumps(int indent, bool ensure_ascii) const {
    if (indent < 1) throw std::invalid_argument("json: indent must be at least 1");
    std::string out;
    write(out, *this, indent, 0, ensure_ascii);
    return out;
}

}  // namespace odl::devkit
