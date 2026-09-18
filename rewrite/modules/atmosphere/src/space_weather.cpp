// space_weather.cpp — loading the two issuing authorities' tables.
//
// SPEC-atmosphere.md §4.3–§4.5.  CelesTrak was the source at v1.0 and is gone:
// its derived centred-81-day column is exact on 69 years of observation and
// wrong on every forecast row (worst 30.07 sfu), and the derived columns were
// the only thing convenience was buying, since ATMO-R-019 recomputes them.

#include <odl/atmosphere/space_weather.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>

namespace odl::atmosphere {
namespace {

/// The GFZ format, as its own header line states it.  Index 24 is SN, which is
/// CC BY-NC 4.0 and is NOT in the manifest's declared list.
const std::map<std::string, int, std::less<>> kGfzColumns = {
    {"YYYY",0},{"MM",1},{"DD",2},{"days",3},{"days_m",4},{"Bsr",5},{"dB",6},
    {"Kp1",7},{"Kp2",8},{"Kp3",9},{"Kp4",10},{"Kp5",11},{"Kp6",12},{"Kp7",13},{"Kp8",14},
    {"ap1",15},{"ap2",16},{"ap3",17},{"ap4",18},{"ap5",19},{"ap6",20},{"ap7",21},{"ap8",22},
    {"Ap",23},{"SN",24},{"F10.7obs",25},{"F10.7adj",26},{"D",27}};

constexpr int kWindow = 40;        ///< the centred 81-day mean needs D-40 … D+40
constexpr double kSentinel = 0.0;  ///< values below this are the file's -1 absence marker

std::vector<std::string_view> fields(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        const std::size_t s = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r') ++i;
        if (i > s) out.push_back(line.substr(s, i - s));
    }
    return out;
}

bool to_double(std::string_view s, double& out) {
    std::string tmp(s);
    try { out = std::stod(tmp); return true; } catch (...) { return false; }
}

std::string day_text(const Day& d) {
    std::ostringstream o;
    o << d.year << '-' << (d.month < 10 ? "0" : "") << d.month
      << '-' << (d.day < 10 ? "0" : "") << d.day;
    return o.str();
}

}  // namespace

long SpaceWeatherTable::index_of(const Day& d) const noexcept {
    const auto it = std::lower_bound(rows_.begin(), rows_.end(), d,
                                     [](const Row& r, const Day& k) { return r.day < k; });
    if (it == rows_.end() || !(it->day == d)) return -1;
    return it - rows_.begin();
}

bool SpaceWeatherTable::usable(const Day& d) const noexcept {
    const long i = index_of(d);
    if (i < 0) return false;
    if (i - 1 < 0) return false;                                   // F10.7 wants D-1
    if (i - kWindow < 0) return false;
    if (i + kWindow >= static_cast<long>(rows_.size())) return false;
    for (long j = i - kWindow; j <= i + kWindow; ++j)
        if (rows_[static_cast<std::size_t>(j)].f107_obs < kSentinel) return false;
    return rows_[static_cast<std::size_t>(i - 1)].f107_obs >= kSentinel;
}

odl::Result<SpaceWeatherTable, AtmoError>
SpaceWeatherTable::load(std::string_view bytes, std::string snapshot_id,
                        std::string snapshot_sha256,
                        const std::vector<std::string>& declared_columns) {
    if (declared_columns.empty()) {
        return odl::err(AtmoError{"ATMO-F-016",
            "no declared column list. The manifest entry must say which columns this tree "
            "reads, because the GFZ file's licence differs BY COLUMN and a headline-licence "
            "check cannot see that (ATMO-R-032)."});
    }
    for (const auto& c : declared_columns) {
        if (!kGfzColumns.contains(c)) {
            return odl::err(AtmoError{"ATMO-F-016",
                "the manifest declares column '" + c + "', which this file does not have."});
        }
    }

    SpaceWeatherTable t;
    t.declared_ = declared_columns;
    t.snapshot_id_ = std::move(snapshot_id);
    t.snapshot_sha256_ = std::move(snapshot_sha256);

    std::size_t start = 0;
    while (start < bytes.size()) {
        const std::size_t nl = bytes.find('\n', start);
        const std::string_view line = bytes.substr(start, (nl == std::string_view::npos)
                                                          ? std::string_view::npos : nl - start);
        start = (nl == std::string_view::npos) ? bytes.size() : nl + 1;
        if (line.empty() || line.front() == '#') continue;
        const auto f = fields(line);
        if (f.size() < 28) continue;
        Row r;
        double v = 0.0;
        if (!to_double(f[0], v)) continue;
        r.day.year = static_cast<int>(v);
        to_double(f[1], v); r.day.month = static_cast<int>(v);
        to_double(f[2], v); r.day.day = static_cast<int>(v);
        for (std::size_t k = 0; k < 8; ++k) { to_double(f[7 + k], v);  r.kp[k] = v; }
        for (std::size_t k = 0; k < 8; ++k) { to_double(f[15 + k], v); r.ap[k] = v; }
        to_double(f[23], v); r.Ap = v;
        // index 24 is SN and is NOT read: it is CC BY-NC 4.0 (ATMO-R-032).
        to_double(f[25], v); r.f107_obs = v;
        to_double(f[26], v); r.f107_adj = v;
        to_double(f[27], v);
        switch (static_cast<int>(v)) {
        case 2: r.cls = DataClass::definitive;    break;
        case 1: r.cls = DataClass::kp_definitive; break;
        case 0: r.cls = DataClass::preliminary;   break;
        default:
            return odl::err(AtmoError{"ATMO-F-005",
                "unrecognised data class " + std::to_string(static_cast<int>(v)) + " on " +
                day_text(r.day) + ". A class the loader does not know is refused, never "
                "defaulted: defaulting is how a forecast row becomes an observation."});
        }
        t.rows_.push_back(r);
    }
    if (t.rows_.empty()) {
        return odl::err(AtmoError{"ATMO-F-016", "no data rows parsed"});
    }
    std::sort(t.rows_.begin(), t.rows_.end(),
              [](const Row& a, const Row& b) { return a.day < b.day; });

    auto& s = t.summary_;
    s.rows = t.rows_.size();
    s.first = t.rows_.front().day;
    s.last = t.rows_.back().day;
    for (const auto& r : t.rows_) {
        (r.f107_obs < kSentinel ? s.f107_sentinel : s.f107_present)++;
        s.by_class[static_cast<int>(r.cls)]++;
    }
    bool run = false;
    for (std::size_t i = 0; i < t.rows_.size(); ++i) {
        const bool u = t.usable(t.rows_[i].day);
        if (u) {
            if (s.usable_days == 0) s.first_usable = t.rows_[i].day;
            s.last_usable = t.rows_[i].day;
            ++s.usable_days;
            if (!run && s.usable_days > 1) ++s.usable_breaks;
            run = true;
        } else {
            run = false;
        }
    }
    return t;
}

odl::Result<double, AtmoError>
SpaceWeatherTable::column(const Day& d, std::string_view name) const {
    if (std::find(declared_.begin(), declared_.end(), name) == declared_.end()) {
        std::string decl;
        for (const auto& c : declared_) { if (!decl.empty()) decl += ", "; decl += c; }
        return odl::err(AtmoError{"ATMO-F-016",
            "column '" + std::string(name) + "' is present in the file but NOT declared by its "
            "manifest entry, so this tree does not read it. Declared: " + decl + ". "
            "(The sunspot column is CC BY-NC 4.0 inside a CC BY 4.0 file, which is why the "
            "declaration is enforced rather than noted — ATMO-R-032.)"});
    }
    const long i = index_of(d);
    if (i < 0) return odl::err(AtmoError{"ATMO-F-004", day_text(d) + " is not in this table"});
    const Row& r = rows_[static_cast<std::size_t>(i)];
    const int idx = kGfzColumns.find(name)->second;
    if (idx >= 7 && idx <= 14) return r.kp[idx - 7];
    if (idx >= 15 && idx <= 22) return r.ap[idx - 15];
    if (idx == 23) return r.Ap;
    if (idx == 25) return r.f107_obs;
    if (idx == 26) return r.f107_adj;
    if (idx == 0) return r.day.year;
    if (idx == 1) return r.day.month;
    if (idx == 2) return r.day.day;
    if (idx == 27) return static_cast<double>(r.cls);
    return odl::err(AtmoError{"ATMO-F-016", "column '" + std::string(name) + "' is not carried"});
}

odl::Result<SpaceWeather, AtmoError>
SpaceWeatherTable::sample(const Day& d, bool require_verified) const {
    const long i = index_of(d);
    if (i < 0) {
        return odl::err(AtmoError{"ATMO-F-004",
            day_text(d) + " is outside this snapshot, which covers " + day_text(summary_.first) +
            " … " + day_text(summary_.last) + " (" + snapshot_id_ + ")."});
    }
    const auto u = static_cast<std::size_t>(i);

    if (i - kWindow < 0 || i + kWindow >= static_cast<long>(rows_.size())) {
        return odl::err(AtmoError{"ATMO-F-004",
            day_text(d) + " has no complete centred 81-day window: F10.7A needs days " +
            std::to_string(kWindow) + " before and after, and this snapshot ends " +
            day_text(summary_.last) + ". The usable end is therefore " +
            day_text(summary_.last_usable) + ", not the file's last row — the two differ by "
            "the window, which is why coverage is not the file's span."});
    }
    for (long j = i - kWindow; j <= i + kWindow; ++j) {
        const Row& g = rows_[static_cast<std::size_t>(j)];
        if (g.f107_obs < kSentinel) {
            return odl::err(AtmoError{"ATMO-F-014",
                day_text(d) + " needs the 81 days " + day_text(rows_[u - kWindow].day) + " … " +
                day_text(rows_[u + kWindow].day) + " for its centred mean, and " +
                day_text(g.day) + " carries the file's absence marker. A sentinel is NOT a "
                "value: averaging the rest would move the mean by about 1.9 sfu and call it "
                "an average of 81 days (ATMO-R-034)."});
        }
    }
    if (rows_[u - 1].f107_obs < kSentinel) {
        return odl::err(AtmoError{"ATMO-F-014",
            day_text(d) + " needs the PREVIOUS day's observed flux (" +
            day_text(rows_[u - 1].day) + "), which carries the absence marker."});
    }

    const bool verified = verified_window_ && !(d < verified_first_) && !(verified_last_ < d);
    if (require_verified && !verified) {
        return odl::err(AtmoError{"ATMO-F-015",
            day_text(d) + " is outside the window over which this tree verified the "
            "redistributed F10.7 against its issuing authority (" +
            (verified_window_ ? day_text(verified_first_) + " … " + day_text(verified_last_)
                              : std::string("no verification has been run")) +
            "). The value exists and is usable; it is simply not one this tree has checked "
            "against DRAO, and you asked for one that is."});
    }

    double sum = 0.0;
    for (long j = i - kWindow; j <= i + kWindow; ++j) sum += rows_[static_cast<std::size_t>(j)].f107_obs;

    SpaceWeather sw;
    sw.f107_previous_day = rows_[u - 1].f107_obs;        // ATMO-R-017
    sw.f107a_centred81 = sum / (2.0 * kWindow + 1.0);    // ATMO-R-018/019: computed, not read
    sw.daily.ap = rows_[u].Ap;
    sw.three_hourly.ap[0] = rows_[u].Ap;
    for (int k = 0; k < 4; ++k) sw.three_hourly.ap[static_cast<std::size_t>(k) + 1] = rows_[u].ap[static_cast<std::size_t>(7 - k)];
    double a12 = 0.0, a36 = 0.0;
    for (int k = 0; k < 8; ++k) {
        a12 += (i - 1 >= 0) ? rows_[u - 1].ap[static_cast<std::size_t>(k)] : rows_[u].ap[static_cast<std::size_t>(k)];
        a36 += (i - 2 >= 0) ? rows_[u - 2].ap[static_cast<std::size_t>(k)] : rows_[u].ap[static_cast<std::size_t>(k)];
    }
    sw.three_hourly.ap[5] = a12 / 8.0;
    sw.three_hourly.ap[6] = a36 / 8.0;
    sw.has_three_hourly = true;
    sw.snapshot_id = snapshot_id_;
    sw.snapshot_sha256 = snapshot_sha256_;
    sw.data_class = rows_[u].cls;
    sw.verification = verified ? Verification::verified_against_issuer
                               : Verification::unverified_redistribution;
    return sw;
}

odl::Result<VerificationReport, AtmoError>
SpaceWeatherTable::verify_against_issuer(std::string_view drao_bytes) {
    // DRAO publishes THREE readings a day and its own header states no selection
    // rule.  GFZ's does: "local noon-time", citing Tapping 2013.  Local noon at
    // Penticton is 20:00 UT.  Sixteen dates carry TWO readings both stamped
    // 20:00, and first-wins reproduces GFZ on all sixteen where last-wins does
    // not (ATMO-R-033) — so the rule is stated, not assumed.
    std::map<Day, double> noon;
    std::map<Day, int> seen;
    std::size_t duplicates = 0;
    std::size_t start = 0;
    while (start < drao_bytes.size()) {
        const std::size_t nl = drao_bytes.find('\n', start);
        const std::string_view line = drao_bytes.substr(start, (nl == std::string_view::npos)
                                                        ? std::string_view::npos : nl - start);
        start = (nl == std::string_view::npos) ? drao_bytes.size() : nl + 1;
        const auto f = fields(line);
        if (f.size() < 7 || f[0].size() != 8 || !std::isdigit(static_cast<unsigned char>(f[0][0])))
            continue;
        if (f[1] != "200000") continue;
        Day d;
        d.year = std::stoi(std::string(f[0].substr(0, 4)));
        d.month = std::stoi(std::string(f[0].substr(4, 2)));
        d.day = std::stoi(std::string(f[0].substr(6, 2)));
        double v = 0.0;
        if (!to_double(f[4], v)) continue;
        if (++seen[d] > 1) { ++duplicates; continue; }   // FIRST wins
        noon[d] = v;
    }
    if (noon.empty()) {
        return odl::err(AtmoError{"ATMO-F-016",
            "no local-noon readings parsed from the issuing authority's table"});
    }

    VerificationReport rep;
    rep.duplicate_timestamps = duplicates;
    bool first = true;
    for (const auto& r : rows_) {
        if (r.f107_obs < kSentinel) continue;
        const auto it = noon.find(r.day);
        if (it == noon.end()) continue;
        ++rep.overlap_days;
        const double diff = std::abs(r.f107_obs - it->second);
        if (diff == 0.0) ++rep.exact; else ++rep.differing;
        rep.worst_difference_sfu = std::max(rep.worst_difference_sfu, diff);
        if (first) { rep.first = r.day; first = false; }
        rep.last = r.day;
    }
    // dates the authority covers but where no local-noon reading exists at all
    for (const auto& [d, n] : seen) if (n == 0) ++rep.missing_noon;

    verified_window_ = rep.overlap_days > 0;
    verified_first_ = rep.first;
    verified_last_ = rep.last;
    return rep;
}

}  // namespace odl::atmosphere
