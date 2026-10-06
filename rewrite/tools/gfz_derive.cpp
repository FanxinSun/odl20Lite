// gfz_derive.cpp — the derivative of GFZ's Kp/ap/F10.7 file that the tree vendors (plan L0 step 8, group C1b; the manager's ruling G1).  The reason,
// and the one change it makes, byte-exactly, are in gfz_derive.hpp.
//
// Three forms:
//   gfz_derive <original> <derived>            derive.  <original> must hash to the pinned upstream file, the result to the pinned derivative.
//   gfz_derive --check <original> <derived>    derive in memory and compare with <derived> (the vendored file) byte for byte.
//   gfz_derive --verify <derived>              no original needed: the file hashes to the pinned derivative and satisfies check_derived.
// Exit codes: 0 ok, 2 the derivative differs or fails its checks, 3 an input refused, 5 usage error, 70 an error the tool did not anticipate.
//
// Written in C++ and by no hand: the vendored file is this tool's output (its SHA-256 was first obtained from an independent derivation with sed
// and compared byte for byte, PROVENANCE.md section 41), and tests/devtools/gfz_derive_tests.cpp holds both the transformation and the vendored
// file to their invariants, with an injected unchanged sunspot number shown refused.

#include "gfz_derive.hpp"

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/text.hpp>

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace odl::tools::gfz_derive {

namespace {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

constexpr const char* kTool = "gfz_derive";
constexpr int kOk = 0, kDiffers = 2, kRefused = 3, kUsage = 5, kInternal = 70;

[[noreturn]] void refuse(const std::string& what) { throw std::runtime_error(what); }

std::string str(std::size_t n) { return std::to_string(n); }

// ------------------------------------------------------------------------------------------------------------------------ the shape of the file

struct Scan {
    std::vector<std::string_view> rows;   // the data rows, without their line feeds
    std::vector<std::size_t> offsets;     // the byte offset of each row in the text
};

Scan scan(std::string_view text) {
    if (text.empty()) refuse("the file is empty");
    if (text.back() != '\n') refuse("the file does not end with a line feed");
    if (const std::size_t cr = text.find('\r'); cr != std::string_view::npos) {
        refuse("a carriage return at byte " + str(cr) + ": the file's lines end with a line feed alone");
    }
    Scan s;
    std::size_t at = 0;
    std::size_t line_no = 0;
    while (at < text.size()) {
        const std::size_t end = text.find('\n', at);   // there is one: the text ends with it
        const std::string_view line = text.substr(at, end - at);
        ++line_no;
        if (line_no <= kHeaderLines) {
            if (line.empty() || line[0] != '#') {
                refuse("header line " + str(line_no) + " does not start with '#' (the file has " + str(kHeaderLines) + " header lines, all starting with '#')");
            }
        } else {
            const std::string row = "data row " + str(line_no - kHeaderLines) + " (line " + str(line_no) + ")";
            if (!line.empty() && line[0] == '#') refuse(row + " starts with '#', but the " + str(kHeaderLines) + " header lines are over");
            if (line.size() != kRowChars) refuse(row + " is " + str(line.size()) + " characters, the file's format says " + str(kRowChars));
            s.rows.push_back(line);
            s.offsets.push_back(at);
        }
        at = end + 1;
    }
    if (s.rows.empty()) refuse("no data rows after the " + str(kHeaderLines) + " header lines");
    return s;
}

// " " then a right-aligned non-negative integer: "  16", " 503", "   0".
bool is_sn_number(std::string_view field) {
    if (field.size() != kSnWidth || field[0] != ' ') return false;
    std::size_t i = 1;
    while (i < field.size() && field[i] == ' ') ++i;
    if (i == field.size()) return false;
    for (; i < field.size(); ++i) {
        if (field[i] < '0' || field[i] > '9') return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------------------------------------ the fields of a row

enum class Kind { integer, fixed1, fixed3, sn };

struct Field {
    const char* name;
    std::size_t at;
    std::size_t width;
    Kind kind;
};

// The file's own description of a row (its header lines 38 and 40): iiii ii ii iiiii fffff.f iiii ii, eight ff.fff, eight iiii, iiii, iii, ffffff.f,
// ffffff.f, i -- each field after a single blank, except Ap's two.  The SN field is taken WITH its separating blank, which is what `derive` replaces.
const std::vector<Field>& fields() {
    static const std::vector<Field> table = [] {
        static const std::array<const char*, 8> kp = {"Kp1", "Kp2", "Kp3", "Kp4", "Kp5", "Kp6", "Kp7", "Kp8"};
        static const std::array<const char*, 8> ap = {"ap1", "ap2", "ap3", "ap4", "ap5", "ap6", "ap7", "ap8"};
        std::vector<Field> t = {{"YYYY", 0, 4, Kind::integer},  {"MM", 5, 2, Kind::integer},   {"DD", 8, 2, Kind::integer},
                                {"days", 11, 5, Kind::integer}, {"days_m", 17, 7, Kind::fixed1}, {"Bsr", 25, 4, Kind::integer},
                                {"dB", 30, 2, Kind::integer}};
        for (std::size_t k = 0; k < 8; ++k) t.push_back({kp[k], 33 + 7 * k, 6, Kind::fixed3});
        for (std::size_t k = 0; k < 8; ++k) t.push_back({ap[k], 89 + 5 * k, 4, Kind::integer});
        t.push_back({"Ap", 130, 4, Kind::integer});
        t.push_back({"SN", kSnOffset, kSnWidth, Kind::sn});
        t.push_back({"F10.7obs", 139, 8, Kind::fixed1});
        t.push_back({"F10.7adj", 148, 8, Kind::fixed1});
        t.push_back({"D", 157, 1, Kind::integer});
        return t;
    }();
    return table;
}

std::optional<long long> parse_int(std::string_view f) {
    std::size_t i = 0;
    while (i < f.size() && f[i] == ' ') ++i;
    bool negative = false;
    if (i < f.size() && f[i] == '-') {
        negative = true;
        ++i;
    }
    if (i == f.size()) return std::nullopt;
    long long v = 0;
    for (; i < f.size(); ++i) {
        if (f[i] < '0' || f[i] > '9') return std::nullopt;
        v = v * 10 + (f[i] - '0');
    }
    return negative ? -v : v;
}

// "  3.333" -> 3333, "-1.000" -> -1000: the number times 10^decimals, as an integer, so that nothing here is a floating-point comparison.
std::optional<long long> parse_fixed(std::string_view f, std::size_t decimals) {
    const std::size_t dot = f.find('.');
    if (dot == std::string_view::npos || f.size() - dot - 1 != decimals) return std::nullopt;
    std::string digits(f.substr(0, dot));
    digits.append(f.substr(dot + 1));
    return parse_int(digits);
}

// Days since 1970-01-01 of a civil date (Howard Hinnant's days_from_civil), for the date-against-day-number check.
long long days_from_civil(long long y, long long m, long long d) {
    y -= (m <= 2) ? 1 : 0;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

std::optional<std::string> check_row(std::string_view row, std::size_t number, long long& previous_days) {
    const std::string where = "data row " + str(number) + ": ";
    // the blanks between the fields
    std::array<bool, kRowChars> covered{};
    for (const Field& f : fields()) {
        for (std::size_t i = f.at; i < f.at + f.width; ++i) covered[i] = true;
    }
    for (std::size_t i = 0; i < kRowChars; ++i) {
        if (!covered[i] && row[i] != ' ') return where + "byte " + str(i + 1) + " lies between the fields and is not a blank";
    }
    std::vector<long long> v(fields().size(), 0);
    for (std::size_t k = 0; k < fields().size(); ++k) {
        const Field& f = fields()[k];
        const std::string_view text = row.substr(f.at, f.width);
        if (f.kind == Kind::sn) {
            if (text != kSnMarker) {
                return where + "the sunspot-number field (bytes " + str(f.at + 1) + "-" + str(f.at + f.width) + ") is " + dk::py_repr(text) +
                       ", not the marker " + dk::py_repr(kSnMarker) + ": it still carries SILSO's CC BY-NC 4.0 sunspot number";
            }
            continue;
        }
        std::optional<long long> parsed;
        if (f.kind == Kind::integer) parsed = parse_int(text);
        else parsed = parse_fixed(text, f.kind == Kind::fixed1 ? 1 : 3);
        if (!parsed) return where + std::string(f.name) + " " + dk::py_repr(text) + " is not a " + (f.kind == Kind::integer ? "whole number" : "fixed-point number");
        v[k] = *parsed;
    }
    const auto at = [&](const char* name) {
        for (std::size_t k = 0; k < fields().size(); ++k) {
            if (std::string_view(fields()[k].name) == name) return v[k];
        }
        throw std::logic_error(std::string("no field ") + name);
    };
    const auto bad = [&](const char* name, const std::string& why) -> std::optional<std::string> {
        for (const Field& f : fields()) {
            if (std::string_view(f.name) == name) return where + name + " " + dk::py_repr(row.substr(f.at, f.width)) + " " + why;
        }
        throw std::logic_error(std::string("no field ") + name);
    };
    const long long year = at("YYYY"), month = at("MM"), day = at("DD"), days = at("days");
    if (month < 1 || month > 12) return bad("MM", "is not a month");
    if (day < 1 || day > 31) return bad("DD", "is not a day of a month");
    if (days_from_civil(year, month, day) - days_from_civil(1932, 1, 1) != days) return bad("days", "does not agree with the date");
    if (previous_days < 0 ? days != 0 : days != previous_days + 1) return bad("days", previous_days < 0 ? "is not 0 on the first row" : "does not follow the previous row's by one");
    previous_days = days;
    if (at("days_m") != days * 10 + 5) return bad("days_m", "is not the day number plus a half");
    if (at("Bsr") < 1) return bad("Bsr", "is not a rotation number");
    if (at("dB") < 1 || at("dB") > 27) return bad("dB", "is not a day within a Bartels rotation");
    for (const Field& f : fields()) {
        const long long value = at(f.name);
        const std::string_view name = f.name;
        if (name.substr(0, 2) == "Kp" && value != -1000 && (value < 0 || value > 9000)) return bad(f.name, "is not a Kp");
        if ((name.substr(0, 2) == "ap" || name == "Ap") && value != -1 && (value < 0 || value > 400)) return bad(f.name, "is not an ap or Ap");
        if (name.substr(0, 5) == "F10.7" && value != -10 && value <= 0) return bad(f.name, "is not a flux");
    }
    if (at("D") < 0 || at("D") > 2) return bad("D", "is not 0, 1 or 2");
    return std::nullopt;
}

// ------------------------------------------------------------------------------------------------------------------------ the command line

std::string read_all(const fs::path& file) {
    const dk::Bytes b = dk::read_bytes(file);
    return std::string(dk::as_text(dk::ByteView{b}));
}

std::string hex_of(const std::string& bytes) { return dk::sha256_hex(dk::as_bytes(bytes)); }

}  // namespace

Derived derive(std::string_view original) {
    const Scan s = scan(original);
    Derived d;
    d.bytes.assign(original);
    d.rows = s.rows.size();
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        const std::string_view field = s.rows[i].substr(kSnOffset, kSnWidth);
        if (field == kSnMarker) continue;
        if (!is_sn_number(field)) {
            refuse("data row " + str(i + 1) + ": the sunspot-number field (bytes " + str(kSnOffset + 1) + "-" + str(kSnOffset + kSnWidth) + ") is " +
                   dk::py_repr(field) + ", neither a non-negative integer nor the marker " + dk::py_repr(kSnMarker));
        }
        d.bytes.replace(s.offsets[i] + kSnOffset, kSnWidth, kSnMarker);
        ++d.changed;
    }
    return d;
}

std::optional<std::string> check_derived(std::string_view text) {
    Scan s;
    try {
        s = scan(text);
    } catch (const std::runtime_error& refused) {
        return std::string(refused.what());
    }
    long long previous_days = -1;
    for (std::size_t i = 0; i < s.rows.size(); ++i) {
        if (std::optional<std::string> why = check_row(s.rows[i], i + 1, previous_days)) return why;
    }
    return std::nullopt;
}

int run_with(const std::vector<std::string>& argv, dk::Streams io, const Pins& pins) {
    const auto say = [&](int code, const std::string& message) -> int {
        io.err << kTool << ": " << message << '\n';
        return code;
    };
    try {
        const bool check = argv.size() == 3 && argv[0] == "--check";
        const bool verify = argv.size() == 2 && argv[0] == "--verify";
        const bool make = argv.size() == 2 && argv[0].rfind("--", 0) != 0;
        if (!check && !verify && !make) {
            return say(kUsage, "usage: gfz_derive <original> <derived>  |  --check <original> <derived>  |  --verify <derived>");
        }
        if (verify) {
            const std::string derived = read_all(argv[1]);
            if (hex_of(derived) != pins.derived) return say(kDiffers, argv[1] + " hashes to " + hex_of(derived) + ", not to the pinned derivative " + std::string(pins.derived));
            if (const std::optional<std::string> why = check_derived(derived)) return say(kDiffers, argv[1] + ": " + *why);
            io.out << kTool << ": " << argv[1] << " is the pinned derivative and satisfies every check (" << derived.size() << " bytes)\n";
            return kOk;
        }
        const std::string& original_path = check ? argv[1] : argv[0];
        const std::string& derived_path = check ? argv[2] : argv[1];
        const std::string original = read_all(original_path);
        if (hex_of(original) != pins.original) {
            return say(kRefused, original_path + " hashes to " + hex_of(original) + ", not to the pinned original " + std::string(pins.original) +
                                     ": this tool derives the one file the manifest pinned on 2026-09-18");
        }
        const Derived d = derive(original);
        if (hex_of(d.bytes) != pins.derived) {
            return say(kDiffers, "the derivative hashes to " + hex_of(d.bytes) + ", not to the pinned " + std::string(pins.derived) + ": the tool and the pin disagree; nothing was written");
        }
        if (const std::optional<std::string> why = check_derived(d.bytes)) return say(kDiffers, "the derivative fails its own checks: " + *why);
        if (check) {
            const std::string vendored = read_all(derived_path);
            if (vendored == d.bytes) {
                io.out << kTool << ": " << derived_path << " is exactly the derivative of " << original_path << " (" << d.rows << " rows, " << d.bytes.size() << " bytes)\n";
                return kOk;
            }
            std::size_t first = 0;
            while (first < vendored.size() && first < d.bytes.size() && vendored[first] == d.bytes[first]) ++first;
            return say(kDiffers, derived_path + " is not the derivative of " + original_path + ": the first difference is at byte " + str(first + 1));
        }
        std::error_code same;
        if (fs::exists(derived_path) && fs::equivalent(original_path, derived_path, same)) return say(kUsage, "the derivative would overwrite the original: " + original_path);
        const fs::path part = derived_path + ".part";
        dk::write_text(part, d.bytes);
        fs::rename(part, derived_path);
        io.out << kTool << ": " << d.rows << " rows; the sunspot-number field of " << d.changed << " of them replaced by " << dk::py_repr(kSnMarker) << "; " << d.bytes.size()
               << " bytes; sha256 " << hex_of(d.bytes) << '\n';
        return kOk;
    } catch (const std::runtime_error& refused) {
        return say(kRefused, refused.what());
    } catch (const std::exception& unexpected) {
        return say(kInternal, std::string("internal error: ") + unexpected.what());
    }
}

int run(const std::vector<std::string>& argv, dk::Streams io) { return run_with(argv, io, Pins{kOriginalSha256, kDerivedSha256}); }

}  // namespace odl::tools::gfz_derive

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::gfz_derive::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
