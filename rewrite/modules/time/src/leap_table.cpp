#include <odl/time/leap_table.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace odl::time {
namespace {

constexpr std::array<std::string_view, 12> kMonths = {
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december"};

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// Gregorian calendar date to MJD.  Written out rather than delegated so that
/// the leap-table parser has no dependency beyond the standard library: it runs
/// before anything else in the tree is trustworthy.
std::int64_t mjd_of(int y, int m, int d) noexcept {
    const long long a = (14 - m) / 12;
    const long long yy = y + 4800 - a;
    const long long mm = m + 12 * a - 3;
    const long long jdn = d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
    return static_cast<std::int64_t>(jdn) - 2400001;   // JDN -> MJD at 0h
}

}  // namespace

odl::Result<LeapTable, TimeError> LeapTable::parse(std::string_view text,
                                                   LeapProvenance provenance) {
    LeapTable table;
    table.provenance_ = std::move(provenance);

    std::istringstream in{std::string(text)};
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        const std::string lowered = lower(line);

        // "#  File expires on 28 June 2027"
        if (const auto at = lowered.find("expires on"); at != std::string::npos) {
            std::istringstream ex{line.substr(at + 10)};
            int day = 0, year = 0;
            std::string month;
            if (ex >> day >> month >> year) {
                const std::string ml = lower(month);
                for (std::size_t i = 0; i < kMonths.size(); ++i) {
                    if (kMonths[i].substr(0, 3) == ml.substr(0, 3)) {
                        table.expiry_mjd_ = mjd_of(year, static_cast<int>(i) + 1, day);
                        table.expiry_text_ = line.substr(line.find_first_not_of("# "));
                        break;
                    }
                }
            }
            continue;
        }

        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;

        std::istringstream row{line};
        double mjd = 0.0;
        int d = 0, m = 0, y = 0;
        double dat = 0.0;
        if (!(row >> mjd >> d >> m >> y >> dat)) {
            return odl::err("TIME-F-005",
                            "leap-second file " + table.provenance_.source_id + ": line " +
                            std::to_string(line_no) + " is not 'MJD day month year dAT': \"" +
                            line + "\"");
        }
        const auto mjd_i = static_cast<std::int64_t>(mjd);
        const auto dat_i = static_cast<std::int64_t>(dat);

        if (static_cast<double>(dat_i) != dat && mjd_i >= kFirstSupportedMjd) {
            return odl::err("TIME-F-005",
                            "leap-second file " + table.provenance_.source_id + ": line " +
                            std::to_string(line_no) + " has non-integral dAT " +
                            std::to_string(dat) + " at MJD " + std::to_string(mjd_i) +
                            "; dAT is integral from 1972-01-01 (MJD 41317)");
        }
        if (mjd_of(y, m, d) != mjd_i) {
            return odl::err("TIME-F-005",
                            "leap-second file " + table.provenance_.source_id + ": line " +
                            std::to_string(line_no) + " states MJD " + std::to_string(mjd_i) +
                            " but the date " + std::to_string(y) + "-" + std::to_string(m) +
                            "-" + std::to_string(d) + " is MJD " +
                            std::to_string(mjd_of(y, m, d)));
        }
        if (!table.entries_.empty() && mjd_i <= table.entries_.back().mjd_utc) {
            return odl::err("TIME-F-005",
                            "leap-second file " + table.provenance_.source_id + ": line " +
                            std::to_string(line_no) + " has MJD " + std::to_string(mjd_i) +
                            " which does not increase on the previous row's " +
                            std::to_string(table.entries_.back().mjd_utc));
        }
        table.entries_.push_back(LeapEntry{mjd_i, dat_i});
    }

    if (table.entries_.empty()) {
        return odl::err("TIME-F-005",
                        "leap-second file " + table.provenance_.source_id +
                        ": no data rows found");
    }
    return table;
}

odl::Result<std::int64_t, TimeError> LeapTable::delta_at_on(std::int64_t mjd_utc) const {
    if (mjd_utc < kFirstSupportedMjd) {
        return odl::err("TIME-F-002",
                        "UTC is refused before 1972-01-01 (MJD " +
                        std::to_string(kFirstSupportedMjd) + "); requested MJD " +
                        std::to_string(mjd_utc) + ". Before 1972 UTC ran at a different RATE "
                        "from TAI, so a 'second' there is not an SI second and this module's "
                        "microsecond claim could not be honoured. TAI, TT, TCG, TDB and TCB "
                        "remain available at that epoch.");
    }
    if (expiry_mjd_ != 0 && mjd_utc > expiry_mjd_ && !assume_no_further_) {
        return odl::err("TIME-F-004",
                        "UTC is refused after the leap-second table's expiry; requested MJD " +
                        std::to_string(mjd_utc) + ", table expires at MJD " +
                        std::to_string(expiry_mjd_) + " (\"" + expiry_text_ + "\") from " +
                        provenance_.source_id +
                        (provenance_.bulletin.empty() ? "" : ", " + provenance_.bulletin) +
                        ". A leap second announced after this table was fetched would shift UTC "
                        "by 1 s, which is 7.5 km along track at LEO, silently. To proceed anyway "
                        "set assume_no_further_leap_seconds explicitly for this run; it is "
                        "recorded in run provenance with this table's hash.");
    }
    auto it = std::upper_bound(entries_.begin(), entries_.end(), mjd_utc,
                               [](std::int64_t m, const LeapEntry& e) { return m < e.mjd_utc; });
    return (it == entries_.begin()) ? entries_.front().delta_at : std::prev(it)->delta_at;
}

std::int64_t LeapTable::leap_at_end_of(std::int64_t mjd_utc) const noexcept {
    const auto next = std::find_if(entries_.begin(), entries_.end(),
                                   [&](const LeapEntry& e) { return e.mjd_utc == mjd_utc + 1; });
    if (next == entries_.end() || next == entries_.begin()) return 0;
    return next->delta_at - std::prev(next)->delta_at;
}

}  // namespace odl::time
