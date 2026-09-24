#include <odl/io/horizons.hpp>

#include <array>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace odl::io {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            std::string_view line = text.substr(start, i - start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            lines.push_back(line);
            start = i + 1;
        }
    }
    return lines;
}

/// A "Label : value" or "Label: value" header line, matched by a prefix on
/// the trimmed line and split at the FIRST colon -- `HZAPI`'s own header
/// pads labels to a fixed column with spaces before the colon, not a fixed
/// label width this reader needs to reproduce.
std::optional<std::string_view> header_value(std::string_view line, std::string_view label) {
    std::string_view t = trim(line);
    if (t.size() < label.size() || t.substr(0, label.size()) != label) return std::nullopt;
    auto colon = t.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    return trim(t.substr(colon + 1));
}

/// Every maximal run of `[0-9+\-.E]` that contains at least one digit --
/// robust to `HZAPI`'s own FORTRAN-style "LABEL =VALUE" spacing (a space
/// before the sign of a positive value, none before a negative one) without
/// needing to reproduce its exact column widths.
std::vector<double> extract_numbers(std::string_view line) {
    std::vector<double> out;
    std::size_t i = 0;
    while (i < line.size()) {
        if (std::isdigit(static_cast<unsigned char>(line[i]))) {
            std::size_t start = i;
            // back up over a leading sign, if this run's own value carries one
            if (start > 0 && (line[start - 1] == '+' || line[start - 1] == '-')) --start;
            std::size_t end = i;
            while (end < line.size() &&
                   (std::isdigit(static_cast<unsigned char>(line[end])) || line[end] == '.' ||
                    line[end] == 'E' || line[end] == 'e' ||
                    ((line[end] == '+' || line[end] == '-') && end > 0 &&
                     (line[end - 1] == 'E' || line[end - 1] == 'e')))) {
                ++end;
            }
            double v = 0.0;
            auto [ptr, ec] = std::from_chars(line.data() + start, line.data() + end, v);
            (void)ptr;
            if (ec == std::errc{}) out.push_back(v);
            i = end;
        } else {
            ++i;
        }
    }
    return out;
}

int month_number(std::string_view abbrev) {
    static constexpr std::array<std::string_view, 12> kMonths{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (std::size_t i = 0; i < kMonths.size(); ++i) {
        if (kMonths[i] == abbrev) return static_cast<int>(i) + 1;
    }
    return 0;
}

/// Parses `2460615.500000000 = A.D. 2024-Nov-01 00:00:00.0000 TDB` -- the
/// leading Julian date is read past (the calendar portion that follows is
/// authoritative for this reader, SPEC-io-horizons.md §3.3); everything
/// from `A.D. ` onward is required verbatim, a deliberately narrow scope
/// (this reader's own real use, ACS3 since 2024, never needs a `B.C.` date).
odl::Result<std::pair<time::Calendar, std::string_view>, HorizonsError> parse_record_time_line(
        std::string_view line, int line_no) {
    std::string_view t = trim(line);
    auto ad = t.find("A.D. ");
    if (ad == std::string_view::npos) {
        return odl::err(HorizonsError{"IOHZ-F-001",
            "line " + std::to_string(line_no) + ": expected 'A.D. <calendar> <time system>', found: " +
            std::string(t)});
    }
    std::string_view rest = trim(t.substr(ad + 5));  // past "A.D. "
    // rest: "2024-Nov-01 00:00:00.0000 TDB"
    auto dash1 = rest.find('-');
    auto dash2 = rest.find('-', dash1 == std::string_view::npos ? 0 : dash1 + 1);
    auto space1 = rest.find(' ');
    if (dash1 == std::string_view::npos || dash2 == std::string_view::npos ||
        space1 == std::string_view::npos || space1 < dash2) {
        return odl::err(HorizonsError{"IOHZ-F-001",
            "line " + std::to_string(line_no) + ": malformed calendar date: " + std::string(rest)});
    }
    std::string_view year_s = rest.substr(0, dash1);
    std::string_view month_s = rest.substr(dash1 + 1, dash2 - dash1 - 1);
    std::string_view day_s = rest.substr(dash2 + 1, space1 - dash2 - 1);
    int month = month_number(month_s);
    if (month == 0) {
        return odl::err(HorizonsError{"IOHZ-F-001",
            "line " + std::to_string(line_no) + ": unrecognised month abbreviation: " + std::string(month_s)});
    }
    std::string_view after_date = trim(rest.substr(space1 + 1));  // "00:00:00.0000 TDB"
    auto last_space = after_date.rfind(' ');
    if (last_space == std::string_view::npos) {
        return odl::err(HorizonsError{"IOHZ-F-001",
            "line " + std::to_string(line_no) + ": no time-system token after the time: " +
            std::string(after_date)});
    }
    std::string_view time_s = trim(after_date.substr(0, last_space));
    std::string_view system_token = trim(after_date.substr(last_space + 1));

    auto nums = extract_numbers(time_s);
    if (nums.size() != 3) {
        return odl::err(HorizonsError{"IOHZ-F-001",
            "line " + std::to_string(line_no) + ": expected HH:MM:SS.ffff, found: " + std::string(time_s)});
    }
    int year = 0;
    auto [yp, yec] = std::from_chars(year_s.data(), year_s.data() + year_s.size(), year);
    int day = 0;
    auto [dp, dec] = std::from_chars(day_s.data(), day_s.data() + day_s.size(), day);
    (void)yp; (void)dp;
    if (yec != std::errc{} || dec != std::errc{}) {
        return odl::err(HorizonsError{"IOHZ-F-001",
            "line " + std::to_string(line_no) + ": malformed year/day: " + std::string(rest)});
    }

    time::Calendar cal;
    cal.year = year; cal.month = month; cal.day = day;
    cal.hour = static_cast<int>(nums[0]); cal.minute = static_cast<int>(nums[1]); cal.second = nums[2];
    return std::make_pair(cal, system_token);
}

}  // namespace

odl::Result<time::TimeScale, HorizonsError> to_time_scale(HorizonsTimeSystem s) {
    if (s == HorizonsTimeSystem::Tdb) return time::TimeScale::TDB;
    return odl::err(HorizonsError{"IOHZ-F-002",
        "Horizons time system 'UT' has no accepted mapping here -- this reader only ever "
        "requests TIME_TYPE=TDB, so a 'UT' response means the request was not honoured, "
        "not a second legitimate case"});
}

odl::Result<HorizonsEphemeris, HorizonsError> read_horizons(std::string_view text) {
    auto lines = split_lines(text);

    HorizonsEphemeris eph;
    bool have_target = false, have_center = false;
    bool have_units = false, have_frame = false, have_type = false;

    std::size_t soe_line = 0;
    bool found_soe = false, found_eoe = false;
    std::size_t eoe_line = 0;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (auto v = header_value(lines[i], "Target body name")) { eph.target_body = std::string(*v); have_target = true; }
        if (auto v = header_value(lines[i], "Center body name")) { eph.center_body = std::string(*v); have_center = true; }
        if (auto v = header_value(lines[i], "Output units")) {
            have_units = true;
            if (*v != "KM-S") {
                return odl::err(HorizonsError{"IOHZ-F-003", "Output units is '" + std::string(*v) + "', not KM-S"});
            }
        }
        if (auto v = header_value(lines[i], "Reference frame")) {
            have_frame = true;
            if (*v != "ICRF") {
                return odl::err(HorizonsError{"IOHZ-F-004", "Reference frame is '" + std::string(*v) + "', not ICRF"});
            }
        }
        if (auto v = header_value(lines[i], "Output type")) {
            have_type = true;
            if (*v != "GEOMETRIC cartesian states") {
                return odl::err(HorizonsError{"IOHZ-F-005",
                    "Output type is '" + std::string(*v) + "', not GEOMETRIC cartesian states -- a "
                    "light-time or stellar-aberration correction changes the physical quantity"});
            }
        }
        if (trim(lines[i]) == "$$SOE") { soe_line = i; found_soe = true; }
        if (trim(lines[i]) == "$$EOE") { eoe_line = i; found_eoe = true; break; }
    }

    if (!have_target || !have_center) {
        return odl::err(HorizonsError{"IOHZ-F-001", "missing Target body name / Center body name header line"});
    }
    if (!have_units) return odl::err(HorizonsError{"IOHZ-F-003", "no 'Output units' header line found"});
    if (!have_frame) return odl::err(HorizonsError{"IOHZ-F-004", "no 'Reference frame' header line found"});
    if (!have_type) return odl::err(HorizonsError{"IOHZ-F-005", "no 'Output type' header line found"});
    if (!found_soe || !found_eoe) {
        return odl::err(HorizonsError{"IOHZ-F-001", "missing $$SOE or $$EOE sentinel"});
    }

    std::size_t i = soe_line + 1;
    while (i < eoe_line) {
        // Every one of lines[i], lines[i+1], lines[i+2] must be a real data
        // line strictly before $$EOE -- checked against the vector's own
        // size first so a truncated file (ending exactly at, or just past,
        // $$SOE) refuses cleanly rather than reading out of bounds.
        if (i + 2 >= lines.size() || i + 2 >= eoe_line) {
            return odl::err(HorizonsError{"IOHZ-F-001",
                "record starting at line " + std::to_string(i + 1) + " is incomplete (fewer than 3 lines "
                "before $$EOE)"});
        }
        auto time_result = parse_record_time_line(lines[i], static_cast<int>(i + 1));
        if (!time_result.has_value()) return odl::err(time_result.error());
        auto [cal, token] = *time_result;

        HorizonsTimeSystem ts;
        if (token == "TDB") {
            ts = HorizonsTimeSystem::Tdb;
        } else if (token == "UT") {
            ts = HorizonsTimeSystem::Ut;
        } else {
            return odl::err(HorizonsError{"IOHZ-F-002",
                "line " + std::to_string(i + 1) + ": unrecognised time-system token '" + std::string(token) + "'"});
        }
        if (ts != HorizonsTimeSystem::Tdb) {
            return odl::err(HorizonsError{"IOHZ-F-002",
                "line " + std::to_string(i + 1) + ": record's own time system is '" + std::string(token) +
                "', not TDB"});
        }

        auto pos_nums = extract_numbers(lines[i + 1]);
        if (pos_nums.size() != 3) {
            return odl::err(HorizonsError{"IOHZ-F-001",
                "line " + std::to_string(i + 2) + ": expected X/Y/Z, found " + std::to_string(pos_nums.size()) +
                " numeric field(s): " + std::string(lines[i + 1])});
        }
        auto vel_nums = extract_numbers(lines[i + 2]);
        if (vel_nums.size() != 3) {
            return odl::err(HorizonsError{"IOHZ-F-001",
                "line " + std::to_string(i + 3) + ": expected VX/VY/VZ, found " + std::to_string(vel_nums.size()) +
                " numeric field(s): " + std::string(lines[i + 2])});
        }

        HorizonsStateRecord rec;
        rec.epoch = cal;
        rec.time_system = ts;
        rec.position_km = odl::Vec3{pos_nums[0], pos_nums[1], pos_nums[2]};
        rec.velocity_km_s = odl::Vec3{vel_nums[0], vel_nums[1], vel_nums[2]};
        eph.states.push_back(rec);

        i += 3;
    }

    return eph;
}

bool operator==(const HorizonsStateRecord& a, const HorizonsStateRecord& b) noexcept {
    return a.epoch.year == b.epoch.year && a.epoch.month == b.epoch.month && a.epoch.day == b.epoch.day &&
           a.epoch.hour == b.epoch.hour && a.epoch.minute == b.epoch.minute &&
           a.epoch.second == b.epoch.second && a.time_system == b.time_system &&
           a.position_km.x == b.position_km.x && a.position_km.y == b.position_km.y &&
           a.position_km.z == b.position_km.z && a.velocity_km_s.x == b.velocity_km_s.x &&
           a.velocity_km_s.y == b.velocity_km_s.y && a.velocity_km_s.z == b.velocity_km_s.z;
}

bool operator==(const HorizonsEphemeris& a, const HorizonsEphemeris& b) noexcept {
    return a.target_body == b.target_body && a.center_body == b.center_body && a.states == b.states;
}

}  // namespace odl::io
