#include <odl/devkit/pyfmt.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace odl::devkit {

std::string py_format_g(double v, int precision) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    std::array<char, 400> buf{};   // far more than any precision a tool asks for needs
    const int n = std::snprintf(buf.data(), buf.size(), "%.*g", precision, v);
    return std::string(buf.data(), n > 0 ? static_cast<std::size_t>(std::min<int>(n, static_cast<int>(buf.size()) - 1)) : 0);
}

std::string py_float_repr(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    std::array<char, 40> buf{};
    const auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v, std::chars_format::scientific);
    std::string sci(buf.data(), res.ptr);          // "-1.2345e+02": the shortest digits that read back as v
    std::string out;
    std::size_t i = 0;
    if (sci[i] == '-') {
        out.push_back('-');
        ++i;
    }
    std::string digits;
    for (; i < sci.size() && sci[i] != 'e'; ++i) {
        if (sci[i] != '.') digits.push_back(sci[i]);
    }
    const int e10 = std::atoi(sci.c_str() + i + 1);
    const int decpt = e10 + 1;
    const int n = static_cast<int>(digits.size());
    if (decpt > -4 && decpt <= 16) {
        if (decpt <= 0) {
            out += "0.";
            out.append(static_cast<std::size_t>(-decpt), '0');
            out += digits;
        } else if (decpt >= n) {
            out += digits;
            out.append(static_cast<std::size_t>(decpt - n), '0');
            out += ".0";
        } else {
            out.append(digits, 0, static_cast<std::size_t>(decpt));
            out.push_back('.');
            out.append(digits, static_cast<std::size_t>(decpt), std::string::npos);
        }
    } else {
        out.push_back(digits[0]);
        if (n > 1) {
            out.push_back('.');
            out.append(digits, 1, std::string::npos);
        }
        const int x = decpt - 1;
        out.push_back('e');
        out.push_back(x < 0 ? '-' : '+');
        const int ax = x < 0 ? -x : x;
        if (ax < 10) out.push_back('0');
        out += std::to_string(ax);
    }
    return out;
}

std::string py_float_hex(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(v);
    std::string out = (bits >> 63) != 0 ? "-" : "";
    const std::uint64_t expo = (bits >> 52) & 0x7FFU;
    const std::uint64_t frac = bits & ((std::uint64_t{1} << 52) - 1);
    if (expo == 0 && frac == 0) return out + "0x0.0p+0";
    int e = 0;
    char lead = '1';
    if (expo == 0) {
        lead = '0';
        e = -1022;
    } else {
        e = static_cast<int>(expo) - 1023;
    }
    static constexpr char kDigits[] = "0123456789abcdef";
    out += "0x";
    out.push_back(lead);
    out.push_back('.');
    for (int k = 12; k >= 0; --k) out.push_back(kDigits[(frac >> (4 * k)) & 0xFU]);
    out.push_back('p');
    out.push_back(e < 0 ? '-' : '+');
    out += std::to_string(e < 0 ? -e : e);
    return out;
}

}  // namespace odl::devkit
