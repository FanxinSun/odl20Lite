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

}  // namespace odl::io
