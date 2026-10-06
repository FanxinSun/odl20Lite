// iod.cpp — SPEC-io-formats.md §3.5 (`IODFMT`). Fixed-column, 80 characters.

#include <odl/io/iod.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace odl::io {
namespace {

std::string trim(std::string_view s) {
    std::size_t b = s.find_first_not_of(' ');
    if (b == std::string_view::npos) return "";
    std::size_t e = s.find_last_not_of(' ');
    return std::string(s.substr(b, e - b + 1));
}

odl::Result<std::string, IodError> column(std::string_view line, int first, int last, std::string_view field) {
    if (static_cast<int>(line.size()) < last) {
        return odl::err(IodError{"IOFM-F-001", "IOD line: field '" + std::string(field) +
            "' expected at columns " + std::to_string(first) + "-" + std::to_string(last) +
            " but the line is only " + std::to_string(line.size()) + " characters wide"});
    }
    return std::string(line.substr(static_cast<std::size_t>(first - 1),
                                   static_cast<std::size_t>(last - first + 1)));
}

std::optional<std::string> column_opt(std::string_view line, int first, int last) {
    if (static_cast<int>(line.size()) < first) return std::nullopt;
    int end = std::min(last, static_cast<int>(line.size()));
    return std::string(line.substr(static_cast<std::size_t>(first - 1),
                                   static_cast<std::size_t>(end - first + 1)));
}

odl::Result<int, IodError> to_int(const std::string& raw, std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) {
        return odl::err(IodError{"IOFM-F-001", "IOD line: field '" + std::string(field) + "' is blank"});
    }
    int v = 0;
    auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if (ec != std::errc{} || ptr != t.data() + t.size()) {
        return odl::err(IodError{"IOFM-F-001", "IOD line: field '" + std::string(field) +
            "' is not an integer: '" + t + "'"});
    }
    return v;
}

odl::Result<IodAngleFormat, IodError> to_angle_format(char c) {
    switch (c) {
        case '1': return IodAngleFormat::RaDecArcsec;
        case '2': return IodAngleFormat::RaDecArcmin;
        case '3': return IodAngleFormat::RaDecDeg;
        case '4': return IodAngleFormat::AzElArcsec;
        case '5': return IodAngleFormat::AzElArcmin;
        case '6': return IodAngleFormat::AzElDeg;
        case '7': return IodAngleFormat::RaDecMixed;
        default:
            return odl::err(IodError{"IOFM-F-005", std::string("IOD angle format code '") + c +
                "' is not one of IODFMT's own seven digits (1-7)"});
    }
}

char angle_format_char(IodAngleFormat f) { return static_cast<char>('0' + static_cast<int>(f)); }

}  // namespace

odl::Result<IodObservation, IodError> read_iod(std::string_view line) {
    IodObservation o;

    auto des = column(line, 1, 15, "designation"); if (!des.has_value()) return odl::err(des.error());
    o.designation = trim(*des);

    auto stn = column(line, 17, 20, "station number"); if (!stn.has_value()) return odl::err(stn.error());
    auto stnv = to_int(*stn, "station number"); if (!stnv.has_value()) return odl::err(stnv.error());
    o.station_number = *stnv;

    if (auto ss = column_opt(line, 22, 22)) {
        if (!trim(*ss).empty()) o.station_status = (*ss)[0];
    }

    auto date = column(line, 24, 31, "date"); if (!date.has_value()) return odl::err(date.error());
    auto yr = to_int(date->substr(0, 4), "year"); if (!yr.has_value()) return odl::err(yr.error());
    auto mo = to_int(date->substr(4, 2), "month"); if (!mo.has_value()) return odl::err(mo.error());
    auto da = to_int(date->substr(6, 2), "day"); if (!da.has_value()) return odl::err(da.error());
    o.year = *yr; o.month = *mo; o.day = *da;

    auto hh = column(line, 32, 33, "hour"); if (!hh.has_value()) return odl::err(hh.error());
    auto mm = column(line, 34, 35, "minute"); if (!mm.has_value()) return odl::err(mm.error());
    auto ss = column(line, 36, 40, "second"); if (!ss.has_value()) return odl::err(ss.error());
    auto hhv = to_int(*hh, "hour"); if (!hhv.has_value()) return odl::err(hhv.error());
    auto mmv = to_int(*mm, "minute"); if (!mmv.has_value()) return odl::err(mmv.error());
    o.hour = *hhv; o.minute = *mmv;
    // cols 36-40: SS.sss -- two whole-second digits, three thousandths digits,
    // decimal point assumed between them (IODFMT's own "millisecond precision").
    auto secv = to_int(*ss, "second"); if (!secv.has_value()) return odl::err(secv.error());
    // UNIT-CROSSING: milliseconds -> seconds (IODFMT's SS.sss field, cols 36-40).
    o.second = static_cast<double>(*secv) / 1000.0;

    if (auto tu = column_opt(line, 42, 43)) o.time_uncertainty = trim(*tu);

    auto af = column(line, 45, 45, "angle format"); if (!af.has_value()) return odl::err(af.error());
    auto afv = to_angle_format((*af)[0]); if (!afv.has_value()) return odl::err(afv.error());
    o.angle_format = *afv;

    if (auto ec = column_opt(line, 46, 46)) {
        std::string t = trim(*ec);
        if (!t.empty()) { auto v = to_int(t, "epoch code"); if (!v.has_value()) return odl::err(v.error()); o.epoch_code = *v; }
    }

    auto ang = column(line, 48, 61, "angle"); if (!ang.has_value()) return odl::err(ang.error());
    o.angle_raw = *ang;

    if (auto pu = column_opt(line, 63, 64)) o.positional_uncertainty = trim(*pu);
    if (auto ob = column_opt(line, 66, 66)) { if (!trim(*ob).empty()) o.optical_behavior = (*ob)[0]; }

    if (auto mag = column_opt(line, 67, 70)) {
        std::string t = *mag;
        if (!trim(t).empty()) {
            char sign = t.empty() ? ' ' : t[0];
            std::string digits = t.size() > 1 ? t.substr(1) : "";
            if (!trim(digits).empty()) {
                auto v = to_int(digits, "magnitude"); if (!v.has_value()) return odl::err(v.error());
                double mval = static_cast<double>(*v) / 10.0;
                o.visual_magnitude = (sign == '-') ? -mval : mval;
            }
        }
    }
    if (auto mu = column_opt(line, 72, 73)) o.magnitude_uncertainty = trim(*mu);
    if (auto fp = column_opt(line, 75, 80)) o.flash_period = trim(*fp);

    return o;
}

namespace {
std::string rj(const std::string& s, int w) {
    if (static_cast<int>(s.size()) >= w) return s.substr(0, static_cast<std::size_t>(w));
    return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}
std::string lj(const std::string& s, int w) {
    if (static_cast<int>(s.size()) >= w) return s.substr(0, static_cast<std::size_t>(w));
    return s + std::string(static_cast<std::size_t>(w) - s.size(), ' ');
}
std::string zpad(long v, int w) {
    std::string s = std::to_string(v < 0 ? -v : v);
    if (static_cast<int>(s.size()) < w) s = std::string(static_cast<std::size_t>(w) - s.size(), '0') + s;
    return s;
}
}  // namespace

odl::Result<std::string, IodError> write_iod(const IodObservation& o) {
    std::ostringstream out;
    out << lj(o.designation, 15) << " " << rj(std::to_string(o.station_number), 4) << " "
        << (o.station_status ? std::string(1, *o.station_status) : " ") << " "
        << zpad(o.year, 4) << zpad(o.month, 2) << zpad(o.day, 2)
        // UNIT-CROSSING: seconds -> milliseconds, the write-side inverse of the read above.
        << zpad(o.hour, 2) << zpad(o.minute, 2) << zpad(static_cast<long>(o.second * 1000.0 + 0.5), 5)
        << " " << lj(o.time_uncertainty, 2) << " " << angle_format_char(o.angle_format)
        << (o.epoch_code ? std::to_string(*o.epoch_code) : " ") << " " << lj(o.angle_raw, 14)
        << " " << lj(o.positional_uncertainty, 2) << " "
        << (o.optical_behavior ? std::string(1, *o.optical_behavior) : " ");
    if (o.visual_magnitude) {
        long mv = static_cast<long>(std::abs(*o.visual_magnitude) * 10.0 + 0.5);
        out << (*o.visual_magnitude < 0 ? "-" : "+") << zpad(mv, 3);
    } else {
        out << "    ";
    }
    out << " " << lj(o.magnitude_uncertainty, 2) << " " << lj(o.flash_period, 6);
    return out.str();
}

bool operator==(const IodObservation& a, const IodObservation& b) noexcept {
    return a.designation == b.designation && a.station_number == b.station_number &&
           a.station_status == b.station_status && a.year == b.year && a.month == b.month &&
           a.day == b.day && a.hour == b.hour && a.minute == b.minute &&
           std::abs(a.second - b.second) < 1e-6 && a.time_uncertainty == b.time_uncertainty &&
           a.angle_format == b.angle_format && a.epoch_code == b.epoch_code &&
           a.angle_raw == b.angle_raw && a.positional_uncertainty == b.positional_uncertainty &&
           a.optical_behavior == b.optical_behavior &&
           ((!a.visual_magnitude && !b.visual_magnitude) ||
            (a.visual_magnitude && b.visual_magnitude &&
             std::abs(*a.visual_magnitude - *b.visual_magnitude) < 1e-6)) &&
           a.magnitude_uncertainty == b.magnitude_uncertainty && a.flash_period == b.flash_period;
}

// ---- v1.1: the angle and uncertainty decoders (SPEC-io-formats.md §3.5.1) ----------------------------------------------------------------------------------------------

namespace {

constexpr double kPi = 3.14159265358979323846;

IodError f016(const std::string& what) { return IodError{"IOFM-F-016", "IOD angle field: " + what}; }

/// 10^n for a small n, by multiplication: the implied decimal point of a digit field, and no literal factor of a thousand anywhere in this file's decoders.
double pow10_of(int n) {
    double v = 1.0;
    for (int i = 0; i < n; ++i) v *= 10.0;
    return v;
}

/// The digits of raw[pos, pos + width) as an integer, blanks as zeros in place ("3  " is 300); IOFM-F-016 for anything else.
odl::Result<long, IodError> digits_at(std::string_view raw, std::size_t pos, std::size_t width, const char* what) {
    long v = 0;
    for (std::size_t i = 0; i < width; ++i) {
        const char c = raw[pos + i];
        if (c == ' ') {
            v *= 10;
        } else if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
        } else {
            return odl::err(f016(std::string(what) + " has '" + c + "' in a digit position (columns " + std::to_string(48 + pos) + "-" + std::to_string(48 + pos + width - 1) + ")"));
        }
    }
    return v;
}

}  // namespace

odl::Result<IodAngles, IodError> decode_iod_angles(char format_code, std::string_view raw) {
    if (raw.size() != 14) return odl::err(f016("the angle field must be the 14 characters of columns 48-61, not " + std::to_string(raw.size())));
    if (format_code < '1' || format_code > '7') {
        return odl::err(f016(format_code == ' ' ? std::string("no position reported (the format code of column 45 is blank)")
                                                : std::string("the format code '") + format_code + "' is not one of the seven (1-7)"));
    }
    const int fmt = format_code - '0';
    const bool ra_dec = fmt == 1 || fmt == 2 || fmt == 3 || fmt == 7;
    IodAngles out;
    out.kind = ra_dec ? IodAngleKind::RaDec : IodAngleKind::AzEl;

    // the sign of the second coordinate, column 55
    if (raw[7] != '+' && raw[7] != '-') return odl::err(f016(std::string("the sign in column 55 is '") + raw[7] + "', not '+' or '-'"));
    const double sign = raw[7] == '-' ? -1.0 : 1.0;

    // ---- the first coordinate, columns 48-54 -------------------------------------------------------------------------------------------------------------------------
    double first_deg = 0.0;
    if (ra_dec) {
        auto hh = digits_at(raw, 0, 2, "right ascension hours");
        if (!hh) return odl::err(hh.error());
        auto mm = digits_at(raw, 2, 2, "right ascension minutes");
        if (!mm) return odl::err(mm.error());
        if (*hh >= 24) return odl::err(f016("right ascension hours " + std::to_string(*hh) + " is 24 or more"));
        if (*mm >= 60) return odl::err(f016("right ascension minutes " + std::to_string(*mm) + " is 60 or more"));
        double hours = static_cast<double>(*hh) + static_cast<double>(*mm) / 60.0;
        if (fmt == 1 || fmt == 7) {
            auto ss = digits_at(raw, 4, 2, "right ascension seconds");
            if (!ss) return odl::err(ss.error());
            auto tenths = digits_at(raw, 6, 1, "right ascension tenths of a second");
            if (!tenths) return odl::err(tenths.error());
            if (*ss >= 60) return odl::err(f016("right ascension seconds " + std::to_string(*ss) + " is 60 or more"));
            hours += (static_cast<double>(*ss) + static_cast<double>(*tenths) / 10.0) / 3600.0;
        } else {                                               // formats 2, 3: the minutes carry a three-digit decimal fraction
            auto frac = digits_at(raw, 4, 3, "right ascension thousandths of a minute");
            if (!frac) return odl::err(frac.error());
            hours += static_cast<double>(*frac) / pow10_of(3) / 60.0;
        }
        first_deg = hours * 15.0;
    } else {
        auto dd = digits_at(raw, 0, 3, "azimuth degrees");
        if (!dd) return odl::err(dd.error());
        double deg = static_cast<double>(*dd);
        if (fmt == 6) {                                         // DDD dddd
            auto frac = digits_at(raw, 3, 4, "azimuth ten-thousandths of a degree");
            if (!frac) return odl::err(frac.error());
            deg += static_cast<double>(*frac) / pow10_of(4);
        } else {
            auto mm = digits_at(raw, 3, 2, "azimuth arcminutes");
            if (!mm) return odl::err(mm.error());
            if (*mm >= 60) return odl::err(f016("azimuth arcminutes " + std::to_string(*mm) + " is 60 or more"));
            deg += static_cast<double>(*mm) / 60.0;
            auto tail = digits_at(raw, 5, 2, fmt == 4 ? "azimuth arcseconds" : "azimuth hundredths of an arcminute");
            if (!tail) return odl::err(tail.error());
            if (fmt == 4) {
                if (*tail >= 60) return odl::err(f016("azimuth arcseconds " + std::to_string(*tail) + " is 60 or more"));
                deg += static_cast<double>(*tail) / 3600.0;
            } else {
                deg += static_cast<double>(*tail) / pow10_of(2) / 60.0;
            }
        }
        if (deg > 360.0) return odl::err(f016("azimuth " + std::to_string(deg) + " degrees is above 360"));
        first_deg = deg;
    }

    // ---- the second coordinate, columns 56-61 ------------------------------------------------------------------------------------------------------------------------
    const char* name = ra_dec ? "declination" : "elevation";
    auto dd = digits_at(raw, 8, 2, name);
    if (!dd) return odl::err(dd.error());
    double second_deg = static_cast<double>(*dd);
    if (fmt == 3 || fmt == 6 || fmt == 7) {                     // DD dddd
        auto frac = digits_at(raw, 10, 4, name);
        if (!frac) return odl::err(frac.error());
        second_deg += static_cast<double>(*frac) / pow10_of(4);
    } else {                                                    // DD MM SS (1, 4) or DD MM mm (2, 5)
        auto mm = digits_at(raw, 10, 2, name);
        if (!mm) return odl::err(mm.error());
        if (*mm >= 60) return odl::err(f016(std::string(name) + " arcminutes " + std::to_string(*mm) + " is 60 or more"));
        second_deg += static_cast<double>(*mm) / 60.0;
        auto tail = digits_at(raw, 12, 2, name);
        if (!tail) return odl::err(tail.error());
        if (fmt == 1 || fmt == 4) {
            if (*tail >= 60) return odl::err(f016(std::string(name) + " arcseconds " + std::to_string(*tail) + " is 60 or more"));
            second_deg += static_cast<double>(*tail) / 3600.0;
        } else {
            second_deg += static_cast<double>(*tail) / pow10_of(2) / 60.0;
        }
    }
    if (second_deg > 90.0) return odl::err(f016(std::string(name) + " " + std::to_string(second_deg) + " degrees is above 90"));

    const double to_rad = kPi / 180.0;
    double first_rad = first_deg * to_rad;
    if (first_rad >= 2.0 * kPi) first_rad -= 2.0 * kPi;          // 360 degrees exactly is 0
    out.first_rad = first_rad;
    out.second_rad = sign * second_deg * to_rad;
    return out;
}

odl::Result<IodAngles, IodError> decode_iod_angles(const IodObservation& obs) {
    return decode_iod_angles(angle_format_char(obs.angle_format), obs.angle_raw);
}

namespace {

/// `MX` -> M × 10^(X-8); an empty field is "not reported"; IOFM-F-017 otherwise.
odl::Result<std::optional<double>, IodError> mx_value(const std::string& field, const char* what) {
    if (field.empty()) return std::optional<double>{};
    if (field.size() != 2 || field[0] < '0' || field[0] > '9' || field[1] < '0' || field[1] > '9') {
        return odl::err(IodError{"IOFM-F-017", std::string("IOD ") + what + " uncertainty field '" + field + "' is neither two digits nor two blanks"});
    }
    const int m = field[0] - '0', x = field[1] - '0';
    const double mantissa = static_cast<double>(m);
    return std::optional<double>{x >= 8 ? mantissa * pow10_of(x - 8) : mantissa / pow10_of(8 - x)};      // one rounding, of the exact decimal
}

}  // namespace

odl::Result<std::optional<double>, IodError> decode_iod_time_uncertainty(const IodObservation& obs) { return mx_value(obs.time_uncertainty, "time"); }

odl::Result<std::optional<double>, IodError> decode_iod_position_uncertainty(const IodObservation& obs) {
    auto v = mx_value(obs.positional_uncertainty, "positional");
    if (!v || !v->has_value()) return v;
    // the unit by the format: seconds of arc (1, 4), minutes of arc (2, 5), degrees (3, 6, 7)
    double to_rad = kPi / 180.0;
    switch (obs.angle_format) {
        case IodAngleFormat::RaDecArcsec:
        case IodAngleFormat::AzElArcsec: to_rad /= 3600.0; break;
        case IodAngleFormat::RaDecArcmin:
        case IodAngleFormat::AzElArcmin: to_rad /= 60.0; break;
        default: break;
    }
    return std::optional<double>{**v * to_rad};
}

}  // namespace odl::io
