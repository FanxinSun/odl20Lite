// sp3.cpp — SPEC-io-formats.md §3.2, `SP3D`.

#include <odl/io/sp3.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <charconv>
#include <sstream>

namespace odl::io {
namespace {

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t nl = text.find('\n', start);
        std::string_view line = (nl == std::string_view::npos)
            ? text.substr(start) : text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    // A trailing newline produces one spurious empty final line; drop it.
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

std::string trim(std::string_view s) {
    std::size_t b = s.find_first_not_of(' ');
    if (b == std::string_view::npos) return "";
    std::size_t e = s.find_last_not_of(' ');
    return std::string(s.substr(b, e - b + 1));
}

/// Mandatory field: 1-indexed, inclusive columns, `SP3D`'s own convention.
/// Refuses (`IOFM-F-001`) if the line is too short to hold this column range.
odl::Result<std::string, Sp3Error> column(std::string_view line, int first, int last,
                                          int line_no, std::string_view field) {
    if (static_cast<int>(line.size()) < last) {
        return odl::err(Sp3Error{"IOFM-F-001",
            "SP3 line " + std::to_string(line_no) + ": field '" + std::string(field) +
            "' expected at columns " + std::to_string(first) + "-" + std::to_string(last) +
            " but the line is only " + std::to_string(line.size()) + " characters wide: '" +
            std::string(line) + "'"});
    }
    return std::string(line.substr(static_cast<std::size_t>(first - 1),
                                   static_cast<std::size_t>(last - first + 1)));
}

/// Optional trailing field: absent (nullopt) if the line does not reach that
/// far at all -- `SP3D`'s own P-record accuracy/flag columns (62-80) are
/// legitimately absent in files that print only through column 60.
std::optional<std::string> column_opt(std::string_view line, int first, int last) {
    if (static_cast<int>(line.size()) < first) return std::nullopt;
    int end = std::min(last, static_cast<int>(line.size()));
    return std::string(line.substr(static_cast<std::size_t>(first - 1),
                                   static_cast<std::size_t>(end - first + 1)));
}

odl::Result<int, Sp3Error> to_int(const std::string& raw, int line_no, std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is blank where an integer is required"});
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), value);
    if (ec != std::errc{} || ptr != t.data() + t.size()) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not an integer: '" + t + "'"});
    }
    return value;
}

odl::Result<double, Sp3Error> to_double(const std::string& raw, int line_no, std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is blank where a number is required"});
    }
    try {
        std::size_t consumed = 0;
        double value = std::stod(t, &consumed);
        if (consumed != t.size()) {
            return odl::err(Sp3Error{"IOFM-F-001", "SP3 line " + std::to_string(line_no) +
                ": field '" + std::string(field) + "' is not a number: '" + t + "'"});
        }
        return value;
    } catch (const std::exception&) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not a number: '" + t + "'"});
    }
}

/// Optional signed integer exponent field (P/V records' own sdev columns):
/// absent -> nullopt; present-but-blank (all spaces within a present column
/// range) -> nullopt; present and non-blank -> parsed, refusing garbage.
odl::Result<std::optional<int>, Sp3Error> to_int_opt(const std::optional<std::string>& raw,
                                                      int line_no, std::string_view field) {
    if (!raw.has_value()) return std::optional<int>{};
    std::string t = trim(*raw);
    if (t.empty()) return std::optional<int>{};
    auto v = to_int(t, line_no, field);
    if (!v.has_value()) return odl::err(v.error());
    return std::optional<int>(*v);
}

odl::Result<Sp3TimeSystem, Sp3Error> parse_time_system(const std::string& raw, int line_no) {
    std::string t = trim(raw);
    if (t == "GPS") return Sp3TimeSystem::GPS;
    if (t == "GLO") return Sp3TimeSystem::GLO;
    if (t == "GAL") return Sp3TimeSystem::GAL;
    if (t == "BDT") return Sp3TimeSystem::BDT;
    if (t == "TAI") return Sp3TimeSystem::TAI;
    if (t == "QZS") return Sp3TimeSystem::QZS;
    if (t == "UTC") return Sp3TimeSystem::UTC;
    return odl::err(Sp3Error{"IOFM-F-002", "SP3 line " + std::to_string(line_no) +
        ": Time System '" + t + "' is none of GPS, GLO, GAL, BDT, TAI, QZS, UTC"});
}

std::string time_system_code(Sp3TimeSystem s) {
    switch (s) {
        case Sp3TimeSystem::GPS: return "GPS";
        case Sp3TimeSystem::GLO: return "GLO";
        case Sp3TimeSystem::GAL: return "GAL";
        case Sp3TimeSystem::BDT: return "BDT";
        case Sp3TimeSystem::TAI: return "TAI";
        case Sp3TimeSystem::QZS: return "QZS";
        case Sp3TimeSystem::UTC: return "UTC";
    }
    return "GPS";
}

/// Splits 17 sat-id (or accuracy) slots of 3 columns each, starting at column
/// 10, from a "+ "/"++" line -- `SP3D`'s own fixed 17-per-line layout.
std::vector<std::string> read_17_slots(std::string_view line) {
    std::vector<std::string> slots;
    for (int i = 0; i < 17; ++i) {
        int first = 10 + i * 3;
        int last = first + 2;
        if (static_cast<int>(line.size()) < last) break;
        slots.push_back(std::string(line.substr(static_cast<std::size_t>(first - 1), 3)));
    }
    return slots;
}

bool starts_with(std::string_view line, std::string_view prefix) {
    return line.size() >= prefix.size() && line.substr(0, prefix.size()) == prefix;
}

}  // namespace

odl::Result<time::TimeScale, Sp3Error> to_time_scale(Sp3TimeSystem s) {
    switch (s) {
        case Sp3TimeSystem::GPS: return time::TimeScale::GPS;
        case Sp3TimeSystem::TAI: return time::TimeScale::TAI;
        case Sp3TimeSystem::UTC: return time::TimeScale::UTC;
        default: break;
    }
    return odl::err(Sp3Error{"IOFM-F-003",
        "SP3 Time System '" + time_system_code(s) + "' has no odl::time::TimeScale "
        "variant (GLONASS/Galileo/BeiDou/QZSS system time each differ from GPS time "
        "by a nonzero, and for GLONASS non-constant-in-name, offset this tree does "
        "not yet carry a scale for)"});
}

odl::Result<Sp3File, Sp3Error> read_sp3(std::string_view text) {
    auto lines = split_lines(text);
    if (lines.size() < 2) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 file has fewer than the mandatory two header lines"});
    }

    Sp3Header h;
    int ln = 1;
    {
        std::string_view l = lines[0];
        auto ver = column(l, 1, 2, ln, "version symbol");
        if (!ver.has_value()) return odl::err(ver.error());
        if (ver->size() != 2 || (*ver)[0] != '#') {
            return odl::err(Sp3Error{"IOFM-F-001", "SP3 line 1 does not begin with '#'"});
        }
        auto pv = column(l, 3, 3, ln, "pos/vel flag");
        if (!pv.has_value()) return odl::err(pv.error());
        if (*pv == "P") h.pos_vel_flag = Sp3PosVelFlag::Position;
        else if (*pv == "V") h.pos_vel_flag = Sp3PosVelFlag::Velocity;
        else return odl::err(Sp3Error{"IOFM-F-001", "SP3 line 1: pos/vel flag is '" + *pv + "', not P or V"});

        auto year = column(l, 4, 7, ln, "year"); if (!year.has_value()) return odl::err(year.error());
        auto month = column(l, 9, 10, ln, "month"); if (!month.has_value()) return odl::err(month.error());
        auto day = column(l, 12, 13, ln, "day"); if (!day.has_value()) return odl::err(day.error());
        auto hour = column(l, 15, 16, ln, "hour"); if (!hour.has_value()) return odl::err(hour.error());
        auto minute = column(l, 18, 19, ln, "minute"); if (!minute.has_value()) return odl::err(minute.error());
        auto second = column(l, 21, 31, ln, "second"); if (!second.has_value()) return odl::err(second.error());
        auto yi = to_int(*year, ln, "year"); if (!yi.has_value()) return odl::err(yi.error());
        auto moi = to_int(*month, ln, "month"); if (!moi.has_value()) return odl::err(moi.error());
        auto dai = to_int(*day, ln, "day"); if (!dai.has_value()) return odl::err(dai.error());
        auto hoi = to_int(*hour, ln, "hour"); if (!hoi.has_value()) return odl::err(hoi.error());
        auto mii = to_int(*minute, ln, "minute"); if (!mii.has_value()) return odl::err(mii.error());
        auto sei = to_double(*second, ln, "second"); if (!sei.has_value()) return odl::err(sei.error());
        h.start_epoch = time::Calendar{*yi, *moi, *dai, *hoi, *mii, *sei};

        auto nep = column(l, 33, 39, ln, "number of epochs"); if (!nep.has_value()) return odl::err(nep.error());
        auto nepi = to_int(*nep, ln, "number of epochs"); if (!nepi.has_value()) return odl::err(nepi.error());
        h.num_epochs = *nepi;

        auto du = column(l, 41, 45, ln, "data used"); if (!du.has_value()) return odl::err(du.error());
        h.data_used = trim(*du);
        auto cs = column(l, 47, 51, ln, "coordinate system"); if (!cs.has_value()) return odl::err(cs.error());
        h.coordinate_sys = trim(*cs);
        auto ot = column(l, 53, 55, ln, "orbit type"); if (!ot.has_value()) return odl::err(ot.error());
        h.orbit_type = trim(*ot);
        auto ag = column(l, 57, 60, ln, "agency"); if (!ag.has_value()) return odl::err(ag.error());
        h.agency = trim(*ag);
    }

    ln = 2;
    {
        std::string_view l = lines[1];
        auto sym = column(l, 1, 2, ln, "symbol"); if (!sym.has_value()) return odl::err(sym.error());
        if (*sym != "##") return odl::err(Sp3Error{"IOFM-F-001", "SP3 line 2 does not begin with '##'"});
        auto wk = column(l, 4, 7, ln, "GPS week"); if (!wk.has_value()) return odl::err(wk.error());
        auto wki = to_int(*wk, ln, "GPS week"); if (!wki.has_value()) return odl::err(wki.error());
        h.gps_week = *wki;
        auto sow = column(l, 9, 23, ln, "seconds of week"); if (!sow.has_value()) return odl::err(sow.error());
        auto sowd = to_double(*sow, ln, "seconds of week"); if (!sowd.has_value()) return odl::err(sowd.error());
        h.seconds_of_week = *sowd;
        // THE FIELD (`IOFM-R-002`): columns 25-38, NOT derivable from the week
        // /seconds-of-week pair above -- the predecessor's own first defect.
        auto ei = column(l, 25, 38, ln, "epoch interval"); if (!ei.has_value()) return odl::err(ei.error());
        auto eid = to_double(*ei, ln, "epoch interval"); if (!eid.has_value()) return odl::err(eid.error());
        h.epoch_interval_s = *eid;
        auto mjd = column(l, 40, 44, ln, "MJD start"); if (!mjd.has_value()) return odl::err(mjd.error());
        auto mjdi = to_int(*mjd, ln, "MJD start"); if (!mjdi.has_value()) return odl::err(mjdi.error());
        h.mod_jul_day_start = *mjdi;
        auto fd = column(l, 46, 60, ln, "fractional day"); if (!fd.has_value()) return odl::err(fd.error());
        auto fdd = to_double(*fd, ln, "fractional day"); if (!fdd.has_value()) return odl::err(fdd.error());
        h.fractional_day = *fdd;
    }

    std::size_t idx = 2;
    // "+ " satellite-id lines, until the first "++".
    int num_sats = 0;
    bool first_plus = true;
    while (idx < lines.size() && starts_with(lines[idx], "+") && !starts_with(lines[idx], "++")) {
        ln = static_cast<int>(idx) + 1;
        if (first_plus) {
            auto ns = column(lines[idx], 4, 6, ln, "number of satellites");
            if (!ns.has_value()) return odl::err(ns.error());
            auto nsi = to_int(*ns, ln, "number of satellites");
            if (!nsi.has_value()) return odl::err(nsi.error());
            num_sats = *nsi;
            first_plus = false;
        }
        for (auto& slot : read_17_slots(lines[idx])) h.satellite_ids.push_back(slot);
        ++idx;
    }
    if (num_sats < 0 || static_cast<int>(h.satellite_ids.size()) < num_sats) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 header names " + std::to_string(num_sats) +
            " satellites but only " + std::to_string(h.satellite_ids.size()) + " id slots were read"});
    }
    const auto num_sats_u = static_cast<std::size_t>(num_sats);
    h.satellite_ids.resize(num_sats_u);

    // "++" accuracy lines, until the first "%c". A slot left entirely BLANK
    // (found on a real GSFC-produced Jason-3 SP3 file, not only the spec's
    // own "0"-filled examples) states the same "no accuracy given" this
    // format's own optional numeric fields already use blank for -- treated
    // as 0, the same value the spec's own zero-padding already means, not a
    // parse failure.
    while (idx < lines.size() && starts_with(lines[idx], "++")) {
        for (auto& slot : read_17_slots(lines[idx])) {
            if (trim(slot).empty()) { h.accuracy.push_back(0); continue; }
            auto v = to_int(slot, static_cast<int>(idx) + 1, "accuracy");
            if (!v.has_value()) return odl::err(v.error());
            h.accuracy.push_back(*v);
        }
        ++idx;
    }
    h.accuracy.resize(num_sats_u);

    // Exactly two "%c" lines.
    if (idx + 1 >= lines.size() || !starts_with(lines[idx], "%c") || !starts_with(lines[idx + 1], "%c")) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 file is missing its two mandatory '%c' lines"});
    }
    {
        ln = static_cast<int>(idx) + 1;
        std::string_view l = lines[idx];
        auto ft = column(l, 4, 5, ln, "file type"); if (!ft.has_value()) return odl::err(ft.error());
        h.file_type = trim(*ft);
        auto r2 = column(l, 7, 8, ln, "%c reserved field"); if (!r2.has_value()) return odl::err(r2.error());
        h.c1_reserved_2char = *r2;
        auto ts = column(l, 10, 12, ln, "time system"); if (!ts.has_value()) return odl::err(ts.error());
        auto tsv = parse_time_system(*ts, ln); if (!tsv.has_value()) return odl::err(tsv.error());
        h.time_system = *tsv;
        h.c1_trailer = l.size() > 12 ? std::string(l.substr(12)) : std::string{};
    }
    h.c2_line = lines[idx + 1].size() > 2 ? std::string(lines[idx + 1].substr(2)) : std::string{};
    idx += 2;

    if (idx + 1 >= lines.size() || !starts_with(lines[idx], "%f") || !starts_with(lines[idx + 1], "%f")) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 file is missing its two mandatory '%f' lines"});
    }
    h.f1_line = lines[idx].size() > 2 ? std::string(lines[idx].substr(2)) : std::string{};
    h.f2_line = lines[idx + 1].size() > 2 ? std::string(lines[idx + 1].substr(2)) : std::string{};
    idx += 2;

    if (idx + 1 >= lines.size() || !starts_with(lines[idx], "%i") || !starts_with(lines[idx + 1], "%i")) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 file is missing its two mandatory '%i' lines"});
    }
    h.i1_line = lines[idx].size() > 2 ? std::string(lines[idx].substr(2)) : std::string{};
    h.i2_line = lines[idx + 1].size() > 2 ? std::string(lines[idx + 1].substr(2)) : std::string{};
    idx += 2;

    // Comment lines, until the first epoch header ("* ").
    while (idx < lines.size() && starts_with(lines[idx], "/*")) {
        h.comments.push_back(lines[idx].size() > 3 ? std::string(lines[idx].substr(3)) : std::string{});
        ++idx;
    }

    Sp3File file;
    file.header = std::move(h);

    // Epochs.
    while (idx < lines.size() && starts_with(lines[idx], "*")) {
        ln = static_cast<int>(idx) + 1;
        std::string_view l = lines[idx];
        auto year = column(l, 4, 7, ln, "epoch year"); if (!year.has_value()) return odl::err(year.error());
        auto month = column(l, 9, 10, ln, "epoch month"); if (!month.has_value()) return odl::err(month.error());
        auto day = column(l, 12, 13, ln, "epoch day"); if (!day.has_value()) return odl::err(day.error());
        auto hour = column(l, 15, 16, ln, "epoch hour"); if (!hour.has_value()) return odl::err(hour.error());
        auto minute = column(l, 18, 19, ln, "epoch minute"); if (!minute.has_value()) return odl::err(minute.error());
        auto second = column(l, 21, 31, ln, "epoch second"); if (!second.has_value()) return odl::err(second.error());
        auto yi = to_int(*year, ln, "epoch year"); if (!yi.has_value()) return odl::err(yi.error());
        auto moi = to_int(*month, ln, "epoch month"); if (!moi.has_value()) return odl::err(moi.error());
        auto dai = to_int(*day, ln, "epoch day"); if (!dai.has_value()) return odl::err(dai.error());
        auto hoi = to_int(*hour, ln, "epoch hour"); if (!hoi.has_value()) return odl::err(hoi.error());
        auto mii = to_int(*minute, ln, "epoch minute"); if (!mii.has_value()) return odl::err(mii.error());
        auto sei = to_double(*second, ln, "epoch second"); if (!sei.has_value()) return odl::err(sei.error());

        Sp3Epoch epoch;
        epoch.epoch = time::Calendar{*yi, *moi, *dai, *hoi, *mii, *sei};
        ++idx;

        while (idx < lines.size() && starts_with(lines[idx], "P")) {
            ln = static_cast<int>(idx) + 1;
            std::string_view pl = lines[idx];
            Sp3PositionRecord pr;
            auto sid = column(pl, 2, 4, ln, "satellite id"); if (!sid.has_value()) return odl::err(sid.error());
            pr.satellite_id = *sid;
            auto x = column(pl, 5, 18, ln, "x"); if (!x.has_value()) return odl::err(x.error());
            auto y = column(pl, 19, 32, ln, "y"); if (!y.has_value()) return odl::err(y.error());
            auto z = column(pl, 33, 46, ln, "z"); if (!z.has_value()) return odl::err(z.error());
            auto ck = column(pl, 47, 60, ln, "clock"); if (!ck.has_value()) return odl::err(ck.error());
            auto xd = to_double(*x, ln, "x"); if (!xd.has_value()) return odl::err(xd.error());
            auto yd = to_double(*y, ln, "y"); if (!yd.has_value()) return odl::err(yd.error());
            auto zd = to_double(*z, ln, "z"); if (!zd.has_value()) return odl::err(zd.error());
            auto ckd = to_double(*ck, ln, "clock"); if (!ckd.has_value()) return odl::err(ckd.error());
            pr.x_km = *xd; pr.y_km = *yd; pr.z_km = *zd; pr.clock_us = *ckd;

            auto xs = to_int_opt(column_opt(pl, 62, 63), ln, "x sdev"); if (!xs.has_value()) return odl::err(xs.error());
            auto ys = to_int_opt(column_opt(pl, 65, 66), ln, "y sdev"); if (!ys.has_value()) return odl::err(ys.error());
            auto zs = to_int_opt(column_opt(pl, 68, 69), ln, "z sdev"); if (!zs.has_value()) return odl::err(zs.error());
            auto cs = to_int_opt(column_opt(pl, 71, 73), ln, "clock sdev"); if (!cs.has_value()) return odl::err(cs.error());
            pr.x_sdev = *xs; pr.y_sdev = *ys; pr.z_sdev = *zs; pr.clock_sdev = *cs;
            if (auto ev = column_opt(pl, 75, 75)) pr.clock_event = (*ev == "E");
            if (auto pp = column_opt(pl, 76, 76)) pr.clock_pred = (*pp == "P");
            if (auto mm = column_opt(pl, 79, 79)) pr.maneuver = (*mm == "M");
            if (auto op = column_opt(pl, 80, 80)) pr.orbit_pred = (*op == "P");

            Sp3SatelliteRecord rec;
            rec.position = pr;
            ++idx;

            if (idx < lines.size() && starts_with(lines[idx], "EP")) {
                ln = static_cast<int>(idx) + 1;
                std::string_view epl = lines[idx];
                Sp3PositionCorrelation corr;
                auto f = [&](int a, int b, std::string_view name) -> odl::Result<int, Sp3Error> {
                    auto c = column(epl, a, b, ln, name); if (!c.has_value()) return odl::err(c.error());
                    return to_int(*c, ln, name);
                };
                auto xsd = f(5, 8, "EP x sdev"); if (!xsd.has_value()) return odl::err(xsd.error());
                auto ysd = f(10, 13, "EP y sdev"); if (!ysd.has_value()) return odl::err(ysd.error());
                auto zsd = f(15, 18, "EP z sdev"); if (!zsd.has_value()) return odl::err(zsd.error());
                auto ckd2 = f(20, 26, "EP clk sdev"); if (!ckd2.has_value()) return odl::err(ckd2.error());
                auto xy = f(28, 35, "EP xy corr"); if (!xy.has_value()) return odl::err(xy.error());
                auto xz = f(37, 44, "EP xz corr"); if (!xz.has_value()) return odl::err(xz.error());
                auto xc = f(46, 53, "EP xc corr"); if (!xc.has_value()) return odl::err(xc.error());
                auto yz = f(55, 62, "EP yz corr"); if (!yz.has_value()) return odl::err(yz.error());
                auto yc = f(64, 71, "EP yc corr"); if (!yc.has_value()) return odl::err(yc.error());
                auto zc = f(73, 80, "EP zc corr"); if (!zc.has_value()) return odl::err(zc.error());
                corr.x_sdev_mm = *xsd; corr.y_sdev_mm = *ysd; corr.z_sdev_mm = *zsd;
                corr.clock_sdev_psec = *ckd2;
                corr.xy_correlation = *xy; corr.xz_correlation = *xz; corr.xc_correlation = *xc;
                corr.yz_correlation = *yz; corr.yc_correlation = *yc; corr.zc_correlation = *zc;
                rec.position_correlation = corr;
                ++idx;
            }

            if (idx < lines.size() && starts_with(lines[idx], "V")) {
                ln = static_cast<int>(idx) + 1;
                std::string_view vl = lines[idx];
                Sp3VelocityRecord vr;
                auto vsid = column(vl, 2, 4, ln, "V satellite id"); if (!vsid.has_value()) return odl::err(vsid.error());
                vr.satellite_id = *vsid;
                auto vx = column(vl, 5, 18, ln, "vx"); if (!vx.has_value()) return odl::err(vx.error());
                auto vy = column(vl, 19, 32, ln, "vy"); if (!vy.has_value()) return odl::err(vy.error());
                auto vz = column(vl, 33, 46, ln, "vz"); if (!vz.has_value()) return odl::err(vz.error());
                auto vckt = column(vl, 47, 60, ln, "clock rate"); if (!vckt.has_value()) return odl::err(vckt.error());
                auto vxd = to_double(*vx, ln, "vx"); if (!vxd.has_value()) return odl::err(vxd.error());
                auto vyd = to_double(*vy, ln, "vy"); if (!vyd.has_value()) return odl::err(vyd.error());
                auto vzd = to_double(*vz, ln, "vz"); if (!vzd.has_value()) return odl::err(vzd.error());
                auto vckd = to_double(*vckt, ln, "clock rate"); if (!vckd.has_value()) return odl::err(vckd.error());
                vr.x_dm_s = *vxd; vr.y_dm_s = *vyd; vr.z_dm_s = *vzd; vr.clock_rate = *vckd;
                auto vxs = to_int_opt(column_opt(vl, 62, 63), ln, "vx sdev"); if (!vxs.has_value()) return odl::err(vxs.error());
                auto vys = to_int_opt(column_opt(vl, 65, 66), ln, "vy sdev"); if (!vys.has_value()) return odl::err(vys.error());
                auto vzs = to_int_opt(column_opt(vl, 68, 69), ln, "vz sdev"); if (!vzs.has_value()) return odl::err(vzs.error());
                auto vcs = to_int_opt(column_opt(vl, 71, 73), ln, "clockrate sdev"); if (!vcs.has_value()) return odl::err(vcs.error());
                vr.x_sdev = *vxs; vr.y_sdev = *vys; vr.z_sdev = *vzs; vr.clock_rate_sdev = *vcs;
                rec.velocity = vr;
                ++idx;

                if (idx < lines.size() && starts_with(lines[idx], "EV")) {
                    ln = static_cast<int>(idx) + 1;
                    std::string_view evl = lines[idx];
                    Sp3VelocityCorrelation vcorr;
                    auto f = [&](int a, int b, std::string_view name) -> odl::Result<int, Sp3Error> {
                        auto c = column(evl, a, b, ln, name); if (!c.has_value()) return odl::err(c.error());
                        return to_int(*c, ln, name);
                    };
                    auto xsd = f(5, 8, "EV x sdev"); if (!xsd.has_value()) return odl::err(xsd.error());
                    auto ysd = f(10, 13, "EV y sdev"); if (!ysd.has_value()) return odl::err(ysd.error());
                    auto zsd = f(15, 18, "EV z sdev"); if (!zsd.has_value()) return odl::err(zsd.error());
                    auto ckd2 = f(20, 26, "EV clkrate sdev"); if (!ckd2.has_value()) return odl::err(ckd2.error());
                    auto xy = f(28, 35, "EV xy corr"); if (!xy.has_value()) return odl::err(xy.error());
                    auto xz = f(37, 44, "EV xz corr"); if (!xz.has_value()) return odl::err(xz.error());
                    auto xc = f(46, 53, "EV xc corr"); if (!xc.has_value()) return odl::err(xc.error());
                    auto yz = f(55, 62, "EV yz corr"); if (!yz.has_value()) return odl::err(yz.error());
                    auto yc = f(64, 71, "EV yc corr"); if (!yc.has_value()) return odl::err(yc.error());
                    auto zc = f(73, 80, "EV zc corr"); if (!zc.has_value()) return odl::err(zc.error());
                    vcorr.x_sdev = *xsd; vcorr.y_sdev = *ysd; vcorr.z_sdev = *zsd;
                    vcorr.clock_rate_sdev = *ckd2;
                    vcorr.xy_correlation = *xy; vcorr.xz_correlation = *xz; vcorr.xc_correlation = *xc;
                    vcorr.yz_correlation = *yz; vcorr.yc_correlation = *yc; vcorr.zc_correlation = *zc;
                    rec.velocity_correlation = vcorr;
                    ++idx;
                }
            }

            epoch.satellites.push_back(std::move(rec));
        }

        file.epochs.push_back(std::move(epoch));
    }

    if (idx >= lines.size() || !starts_with(lines[idx], "EOF")) {
        return odl::err(Sp3Error{"IOFM-F-001", "SP3 file does not end with 'EOF'"});
    }

    return file;
}

namespace {

std::string pad_left(std::string s, std::size_t width) {
    if (s.size() >= width) return s;
    return std::string(width - s.size(), ' ') + s;
}

std::string fixed(double v, int width, int decimals) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << v;
    return pad_left(os.str(), static_cast<std::size_t>(width));
}

std::string int_field(long v, int width) {
    return pad_left(std::to_string(v), static_cast<std::size_t>(width));
}

std::string opt_int_field(const std::optional<int>& v, int width) {
    if (!v.has_value()) return std::string(static_cast<std::size_t>(width), ' ');
    return int_field(*v, width);
}

}  // namespace

odl::Result<std::string, Sp3Error> write_sp3(const Sp3File& file) {
    const Sp3Header& h = file.header;
    std::ostringstream out;

    out << "#" << (h.pos_vel_flag == Sp3PosVelFlag::Position ? "dP" : "dV")
        << int_field(h.start_epoch.year, 4) << " " << int_field(h.start_epoch.month, 2)
        << " " << int_field(h.start_epoch.day, 2) << " " << int_field(h.start_epoch.hour, 2)
        << " " << int_field(h.start_epoch.minute, 2) << " "
        << fixed(h.start_epoch.second, 11, 8) << " " << int_field(h.num_epochs, 7)
        << " " << pad_left(h.data_used, 5) << " " << pad_left(h.coordinate_sys, 5)
        << " " << pad_left(h.orbit_type, 3) << " " << pad_left(h.agency, 4) << "\n";

    out << "## " << int_field(h.gps_week, 4) << " " << fixed(h.seconds_of_week, 15, 8)
        << " " << fixed(h.epoch_interval_s, 14, 8) << " " << int_field(h.mod_jul_day_start, 5)
        << " " << fixed(h.fractional_day, 15, 13) << "\n";

    int num_sats = static_cast<int>(h.satellite_ids.size());
    int id_lines = std::max(5, (num_sats + 16) / 17);
    for (int row = 0; row < id_lines; ++row) {
        out << "+ ";                        // cols 1-2
        // cols 3-9 (7 columns): col 3 always blank; row 0 carries "Number of
        // Sats" at cols 4-6 then 3 more blanks (cols 7-9); every other row is
        // 7 blanks throughout.
        if (row == 0) out << " " << int_field(num_sats, 3) << std::string(3, ' ');
        else out << std::string(7, ' ');
        for (int col = 0; col < 17; ++col) {
            int i = row * 17 + col;
            out << (i < num_sats ? pad_left(h.satellite_ids[static_cast<std::size_t>(i)], 3)
                                 : std::string("  0"));
        }
        out << "\n";
    }
    for (int row = 0; row < id_lines; ++row) {
        out << "++" << std::string(7, ' ');  // cols 1-2 "++", cols 3-9 unused
        for (int col = 0; col < 17; ++col) {
            int i = row * 17 + col;
            out << (i < num_sats ? int_field(h.accuracy[static_cast<std::size_t>(i)], 3)
                                 : std::string("  0"));
        }
        out << "\n";
    }

    out << "%c " << pad_left(h.file_type, 2) << " " << pad_left(h.c1_reserved_2char, 2) << " "
        << time_system_code(h.time_system) << h.c1_trailer << "\n";
    out << "%c" << h.c2_line << "\n";
    out << "%f" << h.f1_line << "\n";
    out << "%f" << h.f2_line << "\n";
    out << "%i" << h.i1_line << "\n";
    out << "%i" << h.i2_line << "\n";
    for (const auto& c : h.comments) out << "/* " << c << "\n";

    for (const auto& epoch : file.epochs) {
        out << "*  " << int_field(epoch.epoch.year, 4) << " " << int_field(epoch.epoch.month, 2)
            << " " << int_field(epoch.epoch.day, 2) << " " << int_field(epoch.epoch.hour, 2)
            << " " << int_field(epoch.epoch.minute, 2) << " " << fixed(epoch.epoch.second, 11, 8) << "\n";
        for (const auto& rec : epoch.satellites) {
            const auto& p = rec.position;
            out << "P" << pad_left(p.satellite_id, 3) << fixed(p.x_km, 14, 6) << fixed(p.y_km, 14, 6)
                << fixed(p.z_km, 14, 6) << fixed(p.clock_us, 14, 6) << " "
                << opt_int_field(p.x_sdev, 2) << " " << opt_int_field(p.y_sdev, 2) << " "
                << opt_int_field(p.z_sdev, 2) << " " << opt_int_field(p.clock_sdev, 3) << " "
                << (p.clock_event ? "E" : " ") << (p.clock_pred ? "P" : " ") << "  "
                << (p.maneuver ? "M" : " ") << (p.orbit_pred ? "P" : " ") << "\n";
            if (rec.position_correlation) {
                const auto& c = *rec.position_correlation;
                out << "EP  " << int_field(c.x_sdev_mm, 4) << " " << int_field(c.y_sdev_mm, 4) << " "
                    << int_field(c.z_sdev_mm, 4) << " " << int_field(c.clock_sdev_psec, 7) << " "
                    << int_field(c.xy_correlation, 8) << " " << int_field(c.xz_correlation, 8) << " "
                    << int_field(c.xc_correlation, 8) << " " << int_field(c.yz_correlation, 8) << " "
                    << int_field(c.yc_correlation, 8) << " " << int_field(c.zc_correlation, 8) << "\n";
            }
            if (rec.velocity) {
                const auto& v = *rec.velocity;
                out << "V" << pad_left(v.satellite_id, 3) << fixed(v.x_dm_s, 14, 6) << fixed(v.y_dm_s, 14, 6)
                    << fixed(v.z_dm_s, 14, 6) << fixed(v.clock_rate, 14, 6) << " "
                    << opt_int_field(v.x_sdev, 2) << " " << opt_int_field(v.y_sdev, 2) << " "
                    << opt_int_field(v.z_sdev, 2) << " " << opt_int_field(v.clock_rate_sdev, 3) << "\n";
                if (rec.velocity_correlation) {
                    const auto& c = *rec.velocity_correlation;
                    out << "EV  " << int_field(c.x_sdev, 4) << " " << int_field(c.y_sdev, 4) << " "
                        << int_field(c.z_sdev, 4) << " " << int_field(c.clock_rate_sdev, 7) << " "
                        << int_field(c.xy_correlation, 8) << " " << int_field(c.xz_correlation, 8) << " "
                        << int_field(c.xc_correlation, 8) << " " << int_field(c.yz_correlation, 8) << " "
                        << int_field(c.yc_correlation, 8) << " " << int_field(c.zc_correlation, 8) << "\n";
                }
            }
        }
    }
    out << "EOF\n";
    return out.str();
}

namespace {
bool calendar_eq(const time::Calendar& a, const time::Calendar& b) noexcept {
    return a.year == b.year && a.month == b.month && a.day == b.day && a.hour == b.hour &&
           a.minute == b.minute && std::abs(a.second - b.second) < 1e-9;
}
}  // namespace

bool operator==(const Sp3Header& a, const Sp3Header& b) noexcept {
    return a.pos_vel_flag == b.pos_vel_flag && calendar_eq(a.start_epoch, b.start_epoch) &&
           a.num_epochs == b.num_epochs && a.data_used == b.data_used &&
           a.coordinate_sys == b.coordinate_sys && a.orbit_type == b.orbit_type &&
           a.agency == b.agency && a.gps_week == b.gps_week &&
           std::abs(a.seconds_of_week - b.seconds_of_week) < 1e-6 &&
           std::abs(a.epoch_interval_s - b.epoch_interval_s) < 1e-6 &&
           a.mod_jul_day_start == b.mod_jul_day_start &&
           std::abs(a.fractional_day - b.fractional_day) < 1e-9 &&
           a.satellite_ids == b.satellite_ids && a.accuracy == b.accuracy &&
           a.file_type == b.file_type && a.c1_reserved_2char == b.c1_reserved_2char &&
           a.time_system == b.time_system && a.c1_trailer == b.c1_trailer &&
           a.c2_line == b.c2_line && a.f1_line == b.f1_line && a.f2_line == b.f2_line &&
           a.i1_line == b.i1_line && a.i2_line == b.i2_line;
}

bool operator==(const Sp3PositionRecord& a, const Sp3PositionRecord& b) noexcept {
    return a.satellite_id == b.satellite_id && std::abs(a.x_km - b.x_km) < 1e-9 &&
           std::abs(a.y_km - b.y_km) < 1e-9 && std::abs(a.z_km - b.z_km) < 1e-9 &&
           std::abs(a.clock_us - b.clock_us) < 1e-9 && a.x_sdev == b.x_sdev &&
           a.y_sdev == b.y_sdev && a.z_sdev == b.z_sdev && a.clock_sdev == b.clock_sdev &&
           a.clock_event == b.clock_event && a.clock_pred == b.clock_pred &&
           a.maneuver == b.maneuver && a.orbit_pred == b.orbit_pred;
}

bool operator==(const Sp3PositionCorrelation& a, const Sp3PositionCorrelation& b) noexcept {
    return a.x_sdev_mm == b.x_sdev_mm && a.y_sdev_mm == b.y_sdev_mm && a.z_sdev_mm == b.z_sdev_mm &&
           a.clock_sdev_psec == b.clock_sdev_psec && a.xy_correlation == b.xy_correlation &&
           a.xz_correlation == b.xz_correlation && a.xc_correlation == b.xc_correlation &&
           a.yz_correlation == b.yz_correlation && a.yc_correlation == b.yc_correlation &&
           a.zc_correlation == b.zc_correlation;
}

bool operator==(const Sp3VelocityRecord& a, const Sp3VelocityRecord& b) noexcept {
    return a.satellite_id == b.satellite_id && std::abs(a.x_dm_s - b.x_dm_s) < 1e-9 &&
           std::abs(a.y_dm_s - b.y_dm_s) < 1e-9 && std::abs(a.z_dm_s - b.z_dm_s) < 1e-9 &&
           std::abs(a.clock_rate - b.clock_rate) < 1e-9 && a.x_sdev == b.x_sdev &&
           a.y_sdev == b.y_sdev && a.z_sdev == b.z_sdev && a.clock_rate_sdev == b.clock_rate_sdev;
}

bool operator==(const Sp3VelocityCorrelation& a, const Sp3VelocityCorrelation& b) noexcept {
    return a.x_sdev == b.x_sdev && a.y_sdev == b.y_sdev && a.z_sdev == b.z_sdev &&
           a.clock_rate_sdev == b.clock_rate_sdev && a.xy_correlation == b.xy_correlation &&
           a.xz_correlation == b.xz_correlation && a.xc_correlation == b.xc_correlation &&
           a.yz_correlation == b.yz_correlation && a.yc_correlation == b.yc_correlation &&
           a.zc_correlation == b.zc_correlation;
}

bool operator==(const Sp3SatelliteRecord& a, const Sp3SatelliteRecord& b) noexcept {
    return a.position == b.position && a.position_correlation == b.position_correlation &&
           a.velocity == b.velocity && a.velocity_correlation == b.velocity_correlation;
}

bool operator==(const Sp3Epoch& a, const Sp3Epoch& b) noexcept {
    return calendar_eq(a.epoch, b.epoch) && a.satellites == b.satellites;
}

bool operator==(const Sp3File& a, const Sp3File& b) noexcept {
    return a.header == b.header && a.epochs == b.epochs;
}

}  // namespace odl::io
