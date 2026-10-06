// SPEC-measmod.md §4.1, §7 — the station and site registry.
//
// The SINEX documents arrive as `io::SinexFile`s whose blocks hold their data lines verbatim with the
// leading space (SINEX's own data-line marker) already stripped, so every column below is the SINEX2
// column minus one. The ILRS products add one thing to the standard rows — the SOD as the last field of
// `SITE/ID` and `SITE/ECCENTRICITY` — which is read as the last whitespace-delimited token.

#include <odl/measmod/registry.hpp>

#include <erfa.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

namespace odl::measmod {

using odl::io::SinexBlock;
using odl::time::Calendar;
using odl::time::Epoch;
using odl::time::LeapTable;
using odl::time::TimeScale;

// --------------------------------------------------------------------------------------------------------
// SodKey, the distinct identifiers

odl::Result<SodKey, MeasError> SodKey::make(int pad, int system, int occupancy) {
    if (pad < 1 || pad > 9999 || system < 0 || system > 99 || occupancy < 0 || occupancy > 99) {
        std::ostringstream m;
        m << "pad " << pad << ", system " << system << ", occupancy " << occupancy
          << " cannot be a SOD: a pad is 1…9999 and the system number and the occupancy are 0…99 "
             "(SOD = pad × 10⁴ + system × 10² + occupancy); there is no nearest-station fallback";
        return odl::err("MEAS-F-001", m.str());
    }
    return SodKey{pad, system, occupancy};
}

odl::Result<SodKey, MeasError> SodKey::from_sod(long sod) {
    if (sod < 10000L || sod > 99999999L) {
        return odl::err("MEAS-F-001", "SOD " + std::to_string(sod) +
                                          " is not an eight-digit station number (pad × 10⁴ + system × 10² + occupancy)");
    }
    return make(static_cast<int>(sod / 10000L), static_cast<int>((sod / 100L) % 100L), static_cast<int>(sod % 100L));
}

// --------------------------------------------------------------------------------------------------------
// text helpers

namespace {

std::string trim(std::string_view s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) --e;
    return std::string(s.substr(b, e - b));
}

/// Columns [pos, pos + len) of a line, clamped to the line's own length.
std::string_view col(const std::string& line, std::size_t pos, std::size_t len) {
    if (pos >= line.size()) return {};
    return std::string_view(line).substr(pos, len);
}

bool parse_int(std::string_view s, int& out) {
    const std::string t = trim(s);
    if (t.empty()) return false;
    std::size_t i = 0;
    bool neg = false;
    if (t[0] == '-' || t[0] == '+') { neg = (t[0] == '-'); i = 1; }
    if (i >= t.size()) return false;
    long v = 0;
    for (; i < t.size(); ++i) {
        if (t[i] < '0' || t[i] > '9') return false;
        v = v * 10 + (t[i] - '0');
        if (v > 1000000000L) return false;
    }
    out = static_cast<int>(neg ? -v : v);
    return true;
}

bool parse_double(std::string_view s, double& out) {
    const std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end != t.c_str() + t.size() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

std::vector<std::string> tokens(std::string_view line) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        std::size_t j = i;
        while (j < line.size() && line[j] != ' ' && line[j] != '\t' && line[j] != '\r') ++j;
        if (j > i) out.emplace_back(line.substr(i, j - i));
        i = j;
    }
    return out;
}

MeasError f020(const char* where, std::size_t line_no, const std::string& text, const std::string& why) {
    std::ostringstream m;
    m << where << ", data line " << line_no << ": " << why << " — \"" << text << "\"";
    return MeasError{"MEAS-F-020", m.str()};
}

// --------------------------------------------------------------------------------------------------------
// SINEX epochs, YY:DDD:SSSSS (SPEC-measmod.md §3.6)

bool leap_year(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
int days_in_year(int y) { return leap_year(y) ? 366 : 365; }

void doy_to_month_day(int y, int doy, int& month, int& day) {
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = doy;
    for (int m = 0; m < 12; ++m) {
        const int n = dim[m] + ((m == 1 && leap_year(y)) ? 1 : 0);
        if (d <= n) { month = m + 1; day = d; return; }
        d -= n;
    }
    month = 12;
    day = 31;
}

}  // namespace

odl::Result<SinexTime, MeasError> decode_sinex_epoch(std::string_view text) {
    const std::string t = trim(text);
    if (t.size() != 12 || t[2] != ':' || t[6] != ':')
        return odl::err("MEAS-F-020", "a SINEX epoch must be YY:DDD:SSSSS, found \"" + t + "\"");
    int yy = 0, doy = 0, sec = 0;
    if (!parse_int(t.substr(0, 2), yy) || !parse_int(t.substr(3, 3), doy) || !parse_int(t.substr(7, 5), sec))
        return odl::err("MEAS-F-020", "a SINEX epoch must be YY:DDD:SSSSS, found \"" + t + "\"");
    SinexTime out;
    if (yy == 0 && doy == 0 && sec == 0) {
        out.unset = true;
        return out;
    }
    out.year = yy + (yy < 50 ? 2000 : 1900);
    out.day_of_year = doy;
    out.seconds_of_day = sec;
    if (doy < 1 || doy > days_in_year(out.year) || sec < 0 || sec > 86399)
        return odl::err("MEAS-F-020", "epoch \"" + t + "\" (year " + std::to_string(out.year) + ") is not a day of that year and a second of that day");
    return out;
}

namespace {

/// One SINEX epoch put in UTC. An end whose seconds of day are 86 399 is the end of that day (the file starts the next
/// span at 00:000:00000 of the next), so it becomes the following midnight and `end_of_day` is set: such a span is
/// closed there on the right, exclusive.
struct SnxEpoch {
    std::optional<Epoch> epoch;
    bool end_of_day = false;
    bool before_utc_era = false;   ///< a year before 1972: UTC then ran at a different rate (TIME-F-…); no Epoch is made of it
};

odl::Result<SnxEpoch, MeasError> parse_snx_epoch(std::string_view s, bool is_end, const LeapTable& leaps,
                                                 const char* where, std::size_t line_no, const std::string& line) {
    auto d = decode_sinex_epoch(s);
    if (!d) return odl::err(f020(where, line_no, line, d.error().message));
    if (d->unset) return SnxEpoch{std::nullopt, false, false};
    // The ILRS eccentricity file carries spans from 1971 (SOD 79024201 begins 71:001:00000), before the first date this tree's UTC
    // supports. Such an epoch is not made into an Epoch: a START before the era leaves the span unbounded below (every supported
    // epoch is after it), an END before the era makes the span unable to contain any supported epoch (SPEC-measmod MEAS-R-001).
    if (d->year < 1972) return SnxEpoch{std::nullopt, false, true};
    int y = d->year, doy = d->day_of_year;
    int hh = d->seconds_of_day / 3600, mm = (d->seconds_of_day % 3600) / 60, ss = d->seconds_of_day % 60;
    const bool end_of_day = is_end && d->seconds_of_day == 86399;
    if (end_of_day) {
        hh = mm = ss = 0;
        if (++doy > days_in_year(y)) { ++y; doy = 1; }
    }
    Calendar c;
    c.year = y;
    doy_to_month_day(y, doy, c.month, c.day);
    c.hour = hh;
    c.minute = mm;
    c.second = static_cast<double>(ss);
    auto e = Epoch::from_calendar(TimeScale::UTC, c, leaps);
    if (!e) {
        return odl::err(f020(where, line_no, line, "epoch \"" + trim(s) + "\" cannot be converted from UTC: " + e.error().message));
    }
    return SnxEpoch{*e, end_of_day, false};
}

// --------------------------------------------------------------------------------------------------------
// the tables

struct Span {
    std::optional<Epoch> start;     ///< unset: unknown, or before the UTC era (no lower bound)
    std::optional<Epoch> end;       ///< unset: open
    bool end_exclusive = false;     ///< the SINEX day-end convention
    bool never = false;             ///< the span ended before the UTC era: it contains no supported epoch
    [[nodiscard]] bool contains(const Epoch& t) const noexcept {
        if (never) return false;
        if (start && t < *start) return false;
        if (end) {
            if (end_exclusive ? !(t < *end) : (t > *end)) return false;
        }
        return true;
    }
};

struct Solution {
    int soln = 0;
    Span span;
    std::optional<Epoch> ref;
    bool have_pos[3] = {false, false, false};
    bool have_vel[3] = {false, false, false};
    odl::Vec3 x_ref;
    odl::Vec3 v;
};

struct Marker {
    int pad = 0;
    char point = ' ';
    std::string domes;
    std::string name;
    std::vector<Solution> solutions;
};

struct EccSpan {
    Span span;
    odl::Vec3 une;
};

struct SodData {
    SodKey key;
    int pad = 0;
    char point = ' ';
    std::string domes;
    std::string name;
    std::vector<EccSpan> ecc;
};

struct SiteIdRow {
    int pad = 0;
    char point = ' ';
    std::string domes;
    std::string name;
    long sod = 0;
};

using MarkerKey = std::pair<int, char>;

const SinexBlock* find_block(const odl::io::SinexFile& f, std::string_view name) {
    for (const auto& b : f.blocks)
        if (b.name == name) return &b;
    return nullptr;
}

odl::Result<SiteIdRow, MeasError> parse_site_id(const std::string& l, const char* where, std::size_t n) {
    if (l.size() < 21) return odl::err(f020(where, n, l, "a SITE/ID row is at least 21 columns"));
    SiteIdRow r;
    int pad = 0;
    if (!parse_int(col(l, 0, 4), pad)) return odl::err(f020(where, n, l, "the site code (columns 1–4) is not a number"));
    r.pad = pad;
    r.point = l[6];
    r.domes = trim(col(l, 8, 9));
    r.name = trim(col(l, 20, 22));
    const auto tk = tokens(l);
    int dummy = 0;
    if (tk.empty() || tk.back().size() != 8 || !parse_int(tk.back(), dummy)) {
        return odl::err(f020(where, n, l, "the ILRS SOD (eight digits) must be the last field"));
    }
    r.sod = std::strtol(tk.back().c_str(), nullptr, 10);
    if (r.sod / 10000L != pad) {
        return odl::err(f020(where, n, l, "the SOD " + tk.back() + " does not begin with the site code"));
    }
    return r;
}

std::string reference_text(const odl::io::SinexFile& f) {
    std::string value;
    if (const SinexBlock* b = find_block(f, "FILE/REFERENCE")) {
        std::string description;
        for (const auto& ln : b->lines) {
            if (ln.compare(0, 7, "VERSION") == 0) value = "VERSION " + trim(std::string_view(ln).substr(7));
            if (ln.compare(0, 11, "DESCRIPTION") == 0) description = "DESCRIPTION " + trim(std::string_view(ln).substr(11));
        }
        if (value.empty()) value = description;
    }
    if (value.empty()) value = "(no VERSION or DESCRIPTION line)";
    return value + "; SINEX header created " + f.header.creation_time;
}

std::string describe(const Epoch& t, const LeapTable& leaps) {
    auto c = t.calendar(TimeScale::UTC, leaps, 3);
    if (!c) return "TAI second " + std::to_string(t.tai_seconds());
    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d:%06.3f UTC", c->year, c->month, c->day, c->hour, c->minute, c->second);
    return buf;
}

std::string describe_span(const Span& s, const LeapTable& leaps) {
    return "[" + (s.start ? describe(*s.start, leaps) : std::string("open")) + ", " +
           (s.end ? describe(*s.end, leaps) + (s.end_exclusive ? " exclusive" : "") : std::string("open")) + "]";
}

}  // namespace

// --------------------------------------------------------------------------------------------------------
// the registry

struct SlrRegistry::Impl {
    std::map<MarkerKey, Marker> markers;                  // SLRF2020 SITE/ID joined with its solutions
    std::map<long, MarkerKey> slrf_sods;                  // SODs SLRF2020 lists → their marker
    std::set<int> slrf_pads;
    std::map<long, SodData> sods;                         // the eccentricity file's SODs
    std::set<int> ecc_pads;
    std::map<std::pair<int, std::string>, std::vector<Epoch>> psd_events;   // (pad, DOMES) → event epochs
    std::set<long> placed;
    std::size_t psd_count = 0;
    std::string slrf_release, ecc_release;
    RegistrySources sources;
    LeapTable leaps;

    explicit Impl(LeapTable l) : leaps(std::move(l)) {}
};

odl::Result<SlrRegistry, MeasError> SlrRegistry::build(const odl::io::SinexFile& slrf, const odl::io::SinexFile& ecc,
                                                       std::string_view psd_text, const LeapTable& leaps,
                                                       RegistrySources sources) {
    auto impl = std::make_shared<Impl>(leaps);
    impl->sources = std::move(sources);
    impl->slrf_release = reference_text(slrf);
    impl->ecc_release = reference_text(ecc);

    // ---- SLRF2020 SITE/ID: the markers and the SODs they carry --------------------------------------------------
    const SinexBlock* slrf_id = find_block(slrf, "SITE/ID");
    const SinexBlock* slrf_ep = find_block(slrf, "SOLUTION/EPOCHS");
    const SinexBlock* slrf_est = find_block(slrf, "SOLUTION/ESTIMATE");
    if (!slrf_id || !slrf_ep || !slrf_est)
        return odl::err("MEAS-F-020", "the coordinate file lacks SITE/ID, SOLUTION/EPOCHS or SOLUTION/ESTIMATE");
    for (std::size_t i = 0; i < slrf_id->lines.size(); ++i) {
        const std::string& l = slrf_id->lines[i];
        auto row = parse_site_id(l, "SLRF2020 SITE/ID", i + 1);
        if (!row) return odl::err(row.error());
        const MarkerKey mk{row->pad, row->point};
        auto it = impl->markers.find(mk);
        if (it == impl->markers.end()) {
            Marker m;
            m.pad = row->pad;
            m.point = row->point;
            m.domes = row->domes;
            m.name = row->name;
            impl->markers.emplace(mk, std::move(m));
        } else if (it->second.domes != row->domes) {
            return odl::err(f020("SLRF2020 SITE/ID", i + 1, l, "the marker (pad " + std::to_string(row->pad) + ", point '" +
                                                                   std::string(1, row->point) + "') is listed with two DOMES numbers"));
        }
        auto prev = impl->slrf_sods.find(row->sod);
        if (prev != impl->slrf_sods.end() && prev->second != mk)
            return odl::err(f020("SLRF2020 SITE/ID", i + 1, l, "SOD " + std::to_string(row->sod) + " is listed on two different markers"));
        impl->slrf_sods[row->sod] = mk;
        impl->slrf_pads.insert(row->pad);
    }

    // ---- SOLUTION/EPOCHS: the data spans of each marker's solutions ---------------------------------------------
    for (std::size_t i = 0; i < slrf_ep->lines.size(); ++i) {
        const std::string& l = slrf_ep->lines[i];
        if (l.size() < 40) return odl::err(f020("SLRF2020 SOLUTION/EPOCHS", i + 1, l, "a row is at least 40 columns"));
        int pad = 0, soln = 0;
        if (!parse_int(col(l, 0, 4), pad) || !parse_int(col(l, 8, 4), soln))
            return odl::err(f020("SLRF2020 SOLUTION/EPOCHS", i + 1, l, "the site code or the solution number is not a number"));
        const MarkerKey mk{pad, l[6]};
        auto m = impl->markers.find(mk);
        if (m == impl->markers.end())
            return odl::err(f020("SLRF2020 SOLUTION/EPOCHS", i + 1, l, "a solution for a marker SITE/ID does not list"));
        auto a = parse_snx_epoch(col(l, 15, 12), false, leaps, "SLRF2020 SOLUTION/EPOCHS", i + 1, l);
        if (!a) return odl::err(a.error());
        auto b = parse_snx_epoch(col(l, 28, 12), true, leaps, "SLRF2020 SOLUTION/EPOCHS", i + 1, l);
        if (!b) return odl::err(b.error());
        for (const auto& s : m->second.solutions)
            if (s.soln == soln)
                return odl::err(f020("SLRF2020 SOLUTION/EPOCHS", i + 1, l, "solution " + std::to_string(soln) + " is listed twice for the marker"));
        Solution s;
        s.soln = soln;
        s.span.start = a->epoch;
        s.span.end = b->epoch;
        s.span.end_exclusive = b->end_of_day;
        s.span.never = b->before_utc_era;
        m->second.solutions.push_back(std::move(s));
    }

    // ---- SOLUTION/ESTIMATE: the six numbers of each solution ----------------------------------------------------
    for (std::size_t i = 0; i < slrf_est->lines.size(); ++i) {
        const std::string& l = slrf_est->lines[i];
        if (l.size() < 67) return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "a row is at least 67 columns"));
        int pad = 0, soln = 0;
        if (!parse_int(col(l, 13, 4), pad) || !parse_int(col(l, 21, 4), soln))
            return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "the site code or the solution number is not a number"));
        const std::string type = trim(col(l, 6, 4));
        const std::string unit = trim(col(l, 39, 4));
        double value = 0.0;
        if (!parse_double(col(l, 46, 21), value))
            return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "the estimated value is not a number"));
        int axis = -1;
        bool is_pos = false;
        if (type == "STAX") { axis = 0; is_pos = true; }
        else if (type == "STAY") { axis = 1; is_pos = true; }
        else if (type == "STAZ") { axis = 2; is_pos = true; }
        else if (type == "VELX") { axis = 0; }
        else if (type == "VELY") { axis = 1; }
        else if (type == "VELZ") { axis = 2; }
        else continue;   // not a coordinate
        if (unit != (is_pos ? "m" : "m/y"))
            return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "unit \"" + unit + "\" where \"" + (is_pos ? "m" : "m/y") + "\" is required"));
        auto m = impl->markers.find(MarkerKey{pad, l[19]});
        if (m == impl->markers.end())
            return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "an estimate for a marker SITE/ID does not list"));
        Solution* sol = nullptr;
        for (auto& s : m->second.solutions)
            if (s.soln == soln) sol = &s;
        if (!sol) return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "an estimate with no SOLUTION/EPOCHS row"));
        auto ref = parse_snx_epoch(col(l, 26, 12), false, leaps, "SLRF2020 SOLUTION/ESTIMATE", i + 1, l);
        if (!ref) return odl::err(ref.error());
        if (!ref->epoch) return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, ref->before_utc_era ? "a reference epoch before the UTC era (1972)" : "a reference epoch of 00:000:00000"));
        if (sol->ref && !(*sol->ref == *ref->epoch))
            return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "the six estimates of one solution have different reference epochs"));
        sol->ref = ref->epoch;
        double* dst = is_pos ? &sol->x_ref.x : &sol->v.x;
        bool* have = is_pos ? sol->have_pos : sol->have_vel;
        if (have[axis]) return odl::err(f020("SLRF2020 SOLUTION/ESTIMATE", i + 1, l, "the same estimate is listed twice"));
        have[axis] = true;
        dst[axis] = value;
    }
    for (const auto& [mk, m] : impl->markers) {
        for (const auto& s : m.solutions) {
            const bool all = s.have_pos[0] && s.have_pos[1] && s.have_pos[2] && s.have_vel[0] && s.have_vel[1] && s.have_vel[2];
            if (!all)
                return odl::err("MEAS-F-020", "SLRF2020: solution " + std::to_string(s.soln) + " of marker pad " + std::to_string(mk.first) +
                                                  " point '" + std::string(1, mk.second) +
                                                  "' lacks one of its six estimates: a position with no velocity (or the reverse) is not a position");
        }
    }

    // ---- the eccentricity file ----------------------------------------------------------------------------------
    const SinexBlock* ecc_id = find_block(ecc, "SITE/ID");
    const SinexBlock* ecc_ec = find_block(ecc, "SITE/ECCENTRICITY");
    if (!ecc_id || !ecc_ec) return odl::err("MEAS-F-020", "the eccentricity file lacks SITE/ID or SITE/ECCENTRICITY");
    for (std::size_t i = 0; i < ecc_id->lines.size(); ++i) {
        const std::string& l = ecc_id->lines[i];
        auto row = parse_site_id(l, "ILRS eccentricities SITE/ID", i + 1);
        if (!row) return odl::err(row.error());
        auto prev = impl->sods.find(row->sod);
        if (prev != impl->sods.end()) {
            // an identical repeat is accepted (the pinned file lists 71100301 twice); a conflicting one is not
            if (prev->second.pad != row->pad || prev->second.point != row->point || prev->second.domes != row->domes)
                return odl::err(f020("ILRS eccentricities SITE/ID", i + 1, l, "SOD " + std::to_string(row->sod) + " is listed twice with a different pad, point or DOMES number"));
            continue;
        }
        auto key = SodKey::from_sod(row->sod);
        if (!key) return odl::err(f020("ILRS eccentricities SITE/ID", i + 1, l, key.error().message));
        SodData d;
        d.key = *key;
        d.pad = row->pad;
        d.point = row->point;
        d.domes = row->domes;
        d.name = row->name;
        impl->sods.emplace(row->sod, std::move(d));
        impl->ecc_pads.insert(row->pad);
    }
    for (std::size_t i = 0; i < ecc_ec->lines.size(); ++i) {
        const std::string& l = ecc_ec->lines[i];
        if (l.size() < 71) return odl::err(f020("ILRS eccentricities SITE/ECCENTRICITY", i + 1, l, "a row is at least 71 columns"));
        const auto tk = tokens(l);
        int dummy = 0;
        if (tk.empty() || tk.back().size() != 8 || !parse_int(tk.back(), dummy))
            return odl::err(f020("ILRS eccentricities SITE/ECCENTRICITY", i + 1, l, "the ILRS SOD (eight digits) must be the last field"));
        const long sod = std::strtol(tk.back().c_str(), nullptr, 10);
        auto d = impl->sods.find(sod);
        if (d == impl->sods.end())
            return odl::err(f020("ILRS eccentricities SITE/ECCENTRICITY", i + 1, l, "an eccentricity for SOD " + tk.back() + ", which the file's own SITE/ID does not list"));
        if (trim(col(l, 41, 3)) != "UNE")
            return odl::err(f020("ILRS eccentricities SITE/ECCENTRICITY", i + 1, l, "the eccentricity type is not UNE (the Cartesian file is not used)"));
        EccSpan e;
        auto a = parse_snx_epoch(col(l, 15, 12), false, leaps, "ILRS eccentricities SITE/ECCENTRICITY", i + 1, l);
        if (!a) return odl::err(a.error());
        auto b = parse_snx_epoch(col(l, 28, 12), true, leaps, "ILRS eccentricities SITE/ECCENTRICITY", i + 1, l);
        if (!b) return odl::err(b.error());
        e.span.start = a->epoch;
        e.span.end = b->epoch;
        e.span.end_exclusive = b->end_of_day;
        e.span.never = b->before_utc_era;
        double u = 0, n = 0, ea = 0;
        if (!parse_double(col(l, 44, 9), u) || !parse_double(col(l, 53, 9), n) || !parse_double(col(l, 62, 9), ea))
            return odl::err(f020("ILRS eccentricities SITE/ECCENTRICITY", i + 1, l, "the up, north and east eccentricities (three 9-column fields) are not numbers"));
        e.une = odl::Vec3{u, n, ea};
        d->second.ecc.push_back(e);
    }

    // ---- the two files must agree on what a SOD is --------------------------------------------------------------
    for (const auto& [sod, mk] : impl->slrf_sods) {
        auto d = impl->sods.find(sod);
        if (d == impl->sods.end()) continue;   // listed by the coordinate file only: no eccentricity, refused at lookup
        const Marker& m = impl->markers.at(mk);
        if (d->second.pad != m.pad || d->second.point != m.point || d->second.domes != m.domes)
            return odl::err("MEAS-F-020", "SOD " + std::to_string(sod) + ": the coordinate file says pad " + std::to_string(m.pad) + " point '" +
                                              std::string(1, m.point) + "' DOMES " + m.domes + ", the eccentricity file says pad " +
                                              std::to_string(d->second.pad) + " point '" + std::string(1, d->second.point) + "' DOMES " + d->second.domes);
    }
    for (const auto& [sod, d] : impl->sods) {
        auto s = impl->slrf_sods.find(sod);
        if (s != impl->slrf_sods.end() && !impl->markers.at(s->second).solutions.empty()) impl->placed.insert(sod);
    }

    // ---- the ITRF2020 post-seismic event list: (pad, DOMES) → events -------------------------------------------
    {
        std::istringstream in{std::string(psd_text)};
        std::string l;
        std::size_t n = 0;
        while (std::getline(in, l)) {
            ++n;
            const auto tk = tokens(l);
            if (tk.size() < 4 || tk[0].empty() || tk[0].find_first_not_of("0123456789") != std::string::npos) continue;
            if (std::count(tk[3].begin(), tk[3].end(), ':') != 2) continue;
            int pad = 0;
            if (!parse_int(tk[0], pad)) continue;
            auto ev = parse_snx_epoch(tk[3], false, leaps, "ITRF2020 post-seismic event list", n, l);
            if (!ev) return odl::err(ev.error());
            if (!ev->epoch) return odl::err(f020("ITRF2020 post-seismic event list", n, l, ev->before_utc_era ? "an event before the UTC era (1972)" : "an event epoch of 00:000:00000"));
            impl->psd_events[{pad, tk[2]}].push_back(*ev->epoch);
            ++impl->psd_count;
        }
        if (impl->psd_count == 0)
            return odl::err("MEAS-F-020", "the post-seismic event list holds no event: an empty list would silently remove the protection of MEAS-R-005, "
                                          "so it is refused as the wrong file");
    }

    return SlrRegistry(std::shared_ptr<const Impl>(std::move(impl)));
}

std::size_t SlrRegistry::pad_count() const noexcept { return impl_->slrf_pads.size(); }
std::size_t SlrRegistry::placed_sod_count() const noexcept { return impl_->placed.size(); }
std::size_t SlrRegistry::unplaced_sod_count() const noexcept { return impl_->sods.size() - impl_->placed.size(); }
std::size_t SlrRegistry::post_seismic_event_count() const noexcept { return impl_->psd_count; }
const std::string& SlrRegistry::slrf_release() const noexcept { return impl_->slrf_release; }
const std::string& SlrRegistry::ecc_release() const noexcept { return impl_->ecc_release; }
const RegistrySources& SlrRegistry::sources() const noexcept { return impl_->sources; }

bool SlrRegistry::knows_pad(PadId pad) const noexcept { return impl_->slrf_pads.count(pad.value) != 0; }

odl::Result<std::vector<SodKey>, MeasError> SlrRegistry::placed_sods_of(PadId pad) const {
    if (!knows_pad(pad)) {
        std::ostringstream m;
        m << "pad " << pad.value << " is not in the registry: SLRF2020 (" << impl_->slrf_release << ", sha256 " << impl_->sources.slrf_sha256 << ") lists "
          << impl_->slrf_pads.size() << " pads"
          << (impl_->ecc_pads.count(pad.value) ? "; the eccentricity file knows this pad but SLRF2020 has no coordinates for it" : "")
          << "; there is no nearest-station fallback";
        return odl::err("MEAS-F-001", m.str());
    }
    std::vector<SodKey> out;
    for (long sod : impl_->placed)
        if (sod / 10000L == pad.value) out.push_back(impl_->sods.at(sod).key);
    return out;
}

odl::Result<SlrSite, MeasError> SlrRegistry::site(SodKey key, const Epoch& when, SiteOptions options) const {
    const Impl& m = *impl_;
    const long sod = key.sod();
    const std::string sod_s = std::to_string(sod);

    // ---- 1. identity (MEAS-F-001) ----------------------------------------------------------------------------------
    auto it = m.sods.find(sod);
    if (it == m.sods.end()) {
        std::ostringstream o;
        o << "SOD " << sod_s << " (pad " << key.pad << ", system " << key.system << ", occupancy " << key.occupancy << ") is not in the registry: "
          << "SLRF2020 (" << m.slrf_release << ", sha256 " << m.sources.slrf_sha256 << ") lists " << m.slrf_pads.size() << " pads and "
          << m.slrf_sods.size() << " SODs, the eccentricity file " << m.sods.size() << " SODs; pad " << key.pad
          << (m.slrf_pads.count(key.pad) ? " is known" : (m.ecc_pads.count(key.pad) ? " is known to the eccentricity file only" : " is not known"))
          << "; there is no nearest-station or default";
        return odl::err("MEAS-F-001", o.str());
    }
    const SodData& sd = it->second;
    if (m.placed.count(sod) == 0) {
        std::ostringstream o;
        o << "SOD " << sod_s << " (" << sd.name << ") has no coordinates: ";
        auto s = m.slrf_sods.find(sod);
        if (s == m.slrf_sods.end())
            o << "the eccentricity file lists it but SLRF2020 (" << m.slrf_release << ") does not"
              << (m.slrf_pads.count(sd.pad) ? " (its pad is listed, but not this SOD)" : " (its pad is not listed either)");
        else
            o << "SLRF2020 lists it on marker pad " << s->second.first << " point '" << s->second.second << "', which has no solution";
        o << "; the SOD is not placed and there is no nearest-station fallback";
        return odl::err("MEAS-F-001", o.str());
    }
    const Marker& marker = m.markers.at(MarkerKey{sd.pad, sd.point});

    // ---- 2. the eccentricity span (MEAS-F-002) -----------------------------------------------------------------------
    const EccSpan* ecc = nullptr;
    for (const auto& e : sd.ecc)
        if (e.span.contains(when)) { ecc = &e; break; }
    if (!ecc) {
        std::ostringstream o;
        o << "SOD " << sod_s << " has no eccentricity at " << describe(when, m.leaps) << ": the file holds " << sd.ecc.size() << " span(s)";
        if (!sd.ecc.empty()) o << ", from " << describe_span(sd.ecc.front().span, m.leaps) << " to " << describe_span(sd.ecc.back().span, m.leaps);
        o << "; the nearest span and a zero eccentricity are both refused";
        return odl::err("MEAS-F-002", o.str());
    }

    // ---- 3. the solution span (MEAS-F-003) --------------------------------------------------------------------------
    const Solution* sol = nullptr;
    for (const auto& s : marker.solutions)
        if (s.span.contains(when)) { sol = &s; break; }
    if (!sol) {
        std::ostringstream o;
        o << "marker pad " << marker.pad << " point '" << marker.point << "' (SOD " << sod_s << ") has no solution at " << describe(when, m.leaps)
          << ": its " << marker.solutions.size() << " solution span(s) are";
        for (const auto& s : marker.solutions) o << " #" << s.soln << " " << describe_span(s.span, m.leaps);
        o << "; a position extrapolated beyond its data is not a position";
        return odl::err("MEAS-F-003", o.str());
    }

    // ---- 4. post-seismic (MEAS-F-004) -------------------------------------------------------------------------------
    bool override_used = false;
    auto ev = m.psd_events.find({marker.pad, marker.domes});
    if (ev != m.psd_events.end()) {
        std::optional<Epoch> latest;
        for (const auto& e : ev->second)
            if (e <= when && (!latest || *latest < e)) latest = e;
        if (latest) {
            if (!options.accept_linear_position_after_post_seismic_event) {
                std::ostringstream o;
                o << "marker pad " << marker.pad << " DOMES " << marker.domes << " (SOD " << sod_s << ") had a post-seismic event at "
                  << describe(*latest, m.leaps) << ", at or before " << describe(when, m.leaps) << ": the linear SLRF2020 position omits the post-seismic motion "
                  << "(ITRF2020 PSD list sha256 " << m.sources.psd_sha256 << "); the only way past is accept_linear_position_after_post_seismic_event, set per call";
                return odl::err("MEAS-F-004", o.str());
            }
            override_used = true;
        }
    }

    // ---- the marker, the system reference point ---------------------------------------------------------------------
    constexpr double kYear = 365.25 * 86400.0;
    const double years = when.difference(*sol->ref).to_seconds() / kYear;
    SlrSite out;
    out.sod = sd.key;
    out.pad = sd.pad;
    out.point = sd.point;
    out.domes = sd.domes;
    out.name = sd.name;
    out.marker_itrs_m = sol->x_ref + sol->v * years;
    out.eccentricity_une_m = ecc->une;
    out.slrf_release = m.slrf_release;
    out.ecc_release = m.ecc_release;
    out.post_seismic_override_used = override_used;

    double xyz[3] = {out.marker_itrs_m.x, out.marker_itrs_m.y, out.marker_itrs_m.z};
    double lon = 0, lat = 0, h = 0;
    if (eraGc2gd(1, xyz, &lon, &lat, &h) != 0)
        return odl::err("MEAS-F-020", "marker of SOD " + sod_s + ": the geodetic conversion of its position failed (the geocentre?)");
    out.marker_geodetic = Geodetic{lat, lon, h};

    // up, north, east of the marker's GEODETIC vertical (MEAS-R-003); the geocentric latitude would tilt the
    // up offset by 0.19° at mid-latitudes, 10.7 mm of Yarragadee's 3.185 m
    const double sl = std::sin(lon), cl = std::cos(lon), sp = std::sin(lat), cp = std::cos(lat);
    const odl::Vec3 up{cp * cl, cp * sl, sp};
    const odl::Vec3 north{-sp * cl, -sp * sl, cp};
    const odl::Vec3 east{-sl, cl, 0.0};
    out.srp_itrs_m = out.marker_itrs_m + up * ecc->une.x + north * ecc->une.y + east * ecc->une.z;
    double sxyz[3] = {out.srp_itrs_m.x, out.srp_itrs_m.y, out.srp_itrs_m.z};
    if (eraGc2gd(1, sxyz, &lon, &lat, &h) != 0)
        return odl::err("MEAS-F-020", "system reference point of SOD " + sod_s + ": the geodetic conversion of its position failed");
    out.srp_geodetic = Geodetic{lat, lon, h};
    return out;
}

// --------------------------------------------------------------------------------------------------------
// optical sites

odl::Result<void, MeasError> OpticalRegistry::add(OpticalStationNumber number, double latitude_rad, double longitude_rad,
                                                  double height_m, std::string citation) {
    constexpr double kPi = 3.14159265358979323846;
    // NOT-A-UNIT-CROSSING: the lowest height a site can have, in metres (the Dead Sea shore is −430 m)
    constexpr double kMinHeightM = -1000.0;
    // NOT-A-UNIT-CROSSING: the highest height a site can have, in metres (Everest is 8849 m)
    constexpr double kMaxHeightM = 10000.0;
    if (trim(citation).empty())
        return odl::err("MEAS-F-006", "optical station " + std::to_string(number.value) +
                                          ": a site without a citation is a load error — say where its coordinates come from");
    if (!std::isfinite(latitude_rad) || !std::isfinite(longitude_rad) || !std::isfinite(height_m) || latitude_rad < -kPi / 2 ||
        latitude_rad > kPi / 2 || longitude_rad < -kPi || longitude_rad > 2 * kPi || height_m < kMinHeightM || height_m > kMaxHeightM) {
        std::ostringstream o;
        o << "optical station " << number.value << ": latitude " << latitude_rad << " rad, longitude " << longitude_rad << " rad, height " << height_m
          << " m cannot be a site (latitude within ±π/2, longitude within −π…2π, height within " << kMinHeightM << "…" << kMaxHeightM
          << " m) — degrees given as radians?";
        return odl::err("MEAS-F-024", o.str());
    }
    for (const auto& s : sites_) {
        if (s.number == number) {
            if (s.location.latitude_rad == latitude_rad && s.location.longitude_rad == longitude_rad && s.location.height_m == height_m)
                return {};   // an identical repeat
            return odl::err("MEAS-F-025", "optical station " + std::to_string(number.value) +
                                              " is already held with different coordinates (cited: " + s.citation + "); replacing a station's position silently is refused");
        }
    }
    sites_.push_back(OpticalSite{number, Geodetic{latitude_rad, longitude_rad, height_m}, std::move(citation)});
    return {};
}

odl::Result<OpticalSite, MeasError> OpticalRegistry::site(OpticalStationNumber number) const {
    for (const auto& s : sites_)
        if (s.number == number) return s;
    return odl::err("MEAS-F-005", "optical station " + std::to_string(number.value) + " is not in the registry, which holds " +
                                      std::to_string(sites_.size()) + " site(s) supplied by the caller; no list is bundled");
}

}  // namespace odl::measmod
