// crd.cpp — SPEC-io-formats.md §3.3 (`CRD2`). FREE FORMAT: whitespace-tokenised.

#include <odl/io/crd.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <sstream>

namespace odl::io {
namespace {

std::vector<std::string> tokenize(std::string_view line) {
    std::vector<std::string> toks;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        std::size_t start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i > start) toks.push_back(std::string(line.substr(start, i - start)));
    }
    return toks;
}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t nl = text.find('\n', start);
        std::string_view line = (nl == std::string_view::npos) ? text.substr(start)
                                                                : text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

odl::Result<CrdRecordKind, CrdError> tag_to_kind(const std::string& raw_tag, int line_no) {
    std::string tag = upper(raw_tag);
    if (tag == "H1") return CrdRecordKind::Format;
    if (tag == "H2") return CrdRecordKind::Station;
    if (tag == "H3") return CrdRecordKind::Target;
    if (tag == "H4") return CrdRecordKind::Session;
    if (tag == "H5") return CrdRecordKind::Prediction;
    if (tag == "H8") return CrdRecordKind::EndOfSession;
    if (tag == "H9") return CrdRecordKind::EndOfFile;
    if (tag == "C0") return CrdRecordKind::Config0;
    if (tag == "C1") return CrdRecordKind::Config1;
    if (tag == "C2") return CrdRecordKind::Config2;
    if (tag == "C3") return CrdRecordKind::Config3;
    if (tag == "C4") return CrdRecordKind::Config4;
    if (tag == "C5") return CrdRecordKind::Config5;
    if (tag == "C6") return CrdRecordKind::Config6;
    if (tag == "C7") return CrdRecordKind::Config7;
    if (tag == "10") return CrdRecordKind::FullRateRange;
    if (tag == "11") return CrdRecordKind::NormalPointRange;
    if (tag == "12") return CrdRecordKind::RangeSupplement;
    if (tag == "20") return CrdRecordKind::Meteorological;
    if (tag == "21") return CrdRecordKind::SkyQuality;
    if (tag == "30") return CrdRecordKind::Angles;
    if (tag == "40") return CrdRecordKind::Calibration40;
    if (tag == "41") return CrdRecordKind::Calibration41;
    if (tag == "42") return CrdRecordKind::Calibration42;
    if (tag == "50") return CrdRecordKind::SessionStatistics;
    if (tag == "60") return CrdRecordKind::Compatibility;
    if (tag.size() == 2 && tag[0] == '9' && std::isdigit(static_cast<unsigned char>(tag[1])))
        return CrdRecordKind::UserDefined;
    if (tag == "00") return CrdRecordKind::Comment;
    return odl::err(CrdError{"IOFM-F-004", "CRD line " + std::to_string(line_no) +
        ": record type '" + raw_tag + "' is not one of CRD2's own twenty-eight record types"});
}

odl::Result<double, CrdError> to_double(const std::string& s, int line_no, std::string_view field) {
    try {
        std::size_t consumed = 0;
        double v = std::stod(s, &consumed);
        if (consumed != s.size()) throw std::invalid_argument("trailing");
        return v;
    } catch (const std::exception&) {
        return odl::err(CrdError{"IOFM-F-001", "CRD line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not a number: '" + s + "'"});
    }
}

odl::Result<int, CrdError> to_int(const std::string& s, int line_no, std::string_view field) {
    int v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        return odl::err(CrdError{"IOFM-F-001", "CRD line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not an integer: '" + s + "'"});
    }
    return v;
}

odl::Result<std::optional<int>, CrdError> to_int_or_na(const std::string& s, int line_no, std::string_view field) {
    if (upper(s) == "NA") return std::optional<int>{};
    auto v = to_int(s, line_no, field);
    if (!v.has_value()) return odl::err(v.error());
    return std::optional<int>(*v);
}

odl::Result<void, CrdError> need(const std::vector<std::string>& toks, std::size_t n, int line_no,
                                 std::string_view kind) {
    if (toks.size() < n) {
        return odl::err(CrdError{"IOFM-F-001", "CRD line " + std::to_string(line_no) + ": " +
            std::string(kind) + " needs " + std::to_string(n) + " fields, found " +
            std::to_string(toks.size())});
    }
    return {};
}

}  // namespace

odl::Result<CrdFile, CrdError> read_crd(std::string_view text) {
    CrdFile file;
    bool have_format = false;
    auto lines = split_lines(text);
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        auto toks = tokenize(lines[static_cast<std::size_t>(i)]);
        if (toks.empty()) continue;
        int ln = i + 1;
        auto kindr = tag_to_kind(toks[0], ln);
        if (!kindr.has_value()) return odl::err(kindr.error());
        CrdRecordKind kind = *kindr;

        switch (kind) {
            case CrdRecordKind::Format: {
                auto ok = need(toks, 7, ln, "H1"); if (!ok.has_value()) return odl::err(ok.error());
                CrdFormatHeader h;
                auto v = to_int(toks[2], ln, "version"); if (!v.has_value()) return odl::err(v.error());
                h.version = *v;
                auto y = to_int(toks[3], ln, "year"); if (!y.has_value()) return odl::err(y.error());
                h.year = *y;
                auto mo = to_int(toks[4], ln, "month"); if (!mo.has_value()) return odl::err(mo.error());
                h.month = *mo;
                auto da = to_int(toks[5], ln, "day"); if (!da.has_value()) return odl::err(da.error());
                h.day = *da;
                auto ho = to_int(toks[6], ln, "hour"); if (!ho.has_value()) return odl::err(ho.error());
                h.hour = *ho;
                file.format = h;
                have_format = true;
                break;
            }
            case CrdRecordKind::Station: {
                auto ok = need(toks, 7, ln, "H2"); if (!ok.has_value()) return odl::err(ok.error());
                CrdStationHeader h;
                h.name = toks[1];
                auto si = to_int(toks[2], ln, "system id"); if (!si.has_value()) return odl::err(si.error());
                h.system_id = *si;
                auto sn = to_int(toks[3], ln, "system number"); if (!sn.has_value()) return odl::err(sn.error());
                h.system_number = *sn;
                auto oc = to_int(toks[4], ln, "occupancy"); if (!oc.has_value()) return odl::err(oc.error());
                h.occupancy = *oc;
                auto ts = to_int(toks[5], ln, "epoch time scale"); if (!ts.has_value()) return odl::err(ts.error());
                h.epoch_time_scale = *ts;
                h.network = toks[6];
                file.stations.push_back(h);
                break;
            }
            case CrdRecordKind::Target: {
                auto ok = need(toks, 8, ln, "H3"); if (!ok.has_value()) return odl::err(ok.error());
                CrdTargetHeader h;
                h.name = toks[1];
                auto id = to_int(toks[2], ln, "ilrs id"); if (!id.has_value()) return odl::err(id.error());
                h.ilrs_id = *id;
                h.sic = toks[3];
                h.norad_id = toks[4];
                auto sts = to_int(toks[5], ln, "spacecraft time scale"); if (!sts.has_value()) return odl::err(sts.error());
                h.spacecraft_time_scale = *sts;
                auto tc = to_int(toks[6], ln, "target class"); if (!tc.has_value()) return odl::err(tc.error());
                h.target_class = *tc;
                h.target_location = toks[7];
                file.targets.push_back(h);
                break;
            }
            case CrdRecordKind::Session: {
                auto ok = need(toks, 15, ln, "H4"); if (!ok.has_value()) return odl::err(ok.error());
                CrdSessionHeader h;
                auto dt = to_int(toks[1], ln, "data type"); if (!dt.has_value()) return odl::err(dt.error());
                h.data_type = *dt;
                auto sy = to_int(toks[2], ln, "start year"); if (!sy.has_value()) return odl::err(sy.error());
                h.start_year = *sy;
                auto smo = to_int(toks[3], ln, "start month"); if (!smo.has_value()) return odl::err(smo.error());
                h.start_month = *smo;
                auto sd = to_int(toks[4], ln, "start day"); if (!sd.has_value()) return odl::err(sd.error());
                h.start_day = *sd;
                auto sh = to_int(toks[5], ln, "start hour"); if (!sh.has_value()) return odl::err(sh.error());
                h.start_hour = *sh;
                auto smi = to_int(toks[6], ln, "start minute"); if (!smi.has_value()) return odl::err(smi.error());
                h.start_minute = *smi;
                auto sse = to_int(toks[7], ln, "start second"); if (!sse.has_value()) return odl::err(sse.error());
                h.start_second = *sse;
                auto ey = to_int_or_na(toks[8], ln, "end year"); if (!ey.has_value()) return odl::err(ey.error());
                h.end_year = *ey;
                auto emo = to_int_or_na(toks[9], ln, "end month"); if (!emo.has_value()) return odl::err(emo.error());
                h.end_month = *emo;
                auto edd = to_int_or_na(toks[10], ln, "end day"); if (!edd.has_value()) return odl::err(edd.error());
                h.end_day = *edd;
                auto eh = to_int_or_na(toks[11], ln, "end hour"); if (!eh.has_value()) return odl::err(eh.error());
                h.end_hour = *eh;
                auto emi = to_int_or_na(toks[12], ln, "end minute"); if (!emi.has_value()) return odl::err(emi.error());
                h.end_minute = *emi;
                auto ese = to_int_or_na(toks[13], ln, "end second"); if (!ese.has_value()) return odl::err(ese.error());
                h.end_second = *ese;
                auto rf = to_int(toks[14], ln, "release flag"); if (!rf.has_value()) return odl::err(rf.error());
                h.release_flag = *rf;
                if (toks.size() > 15) h.tropo_applied = (toks[15] == "1");
                if (toks.size() > 16) h.com_applied = (toks[16] == "1");
                if (toks.size() > 17) h.receive_amp_applied = (toks[17] == "1");
                for (std::size_t k = 18; k < toks.size(); ++k) h.remaining_fields.push_back(toks[k]);
                file.sessions.push_back(h);
                break;
            }
            case CrdRecordKind::FullRateRange:
            case CrdRecordKind::NormalPointRange: {
                auto ok = need(toks, 5, ln, "range record"); if (!ok.has_value()) return odl::err(ok.error());
                CrdRangeRecord r;
                r.kind = kind;
                auto sod = to_double(toks[1], ln, "seconds of day"); if (!sod.has_value()) return odl::err(sod.error());
                r.seconds_of_day = *sod;
                auto tof = to_double(toks[2], ln, "time of flight"); if (!tof.has_value()) return odl::err(tof.error());
                r.time_of_flight_s = *tof;
                r.config_id = toks[3];
                auto ee = to_int(toks[4], ln, "epoch event"); if (!ee.has_value()) return odl::err(ee.error());
                r.epoch_event = *ee;
                for (std::size_t k = 5; k < toks.size(); ++k) r.remaining_fields.push_back(toks[k]);
                file.ranges.push_back(r);
                break;
            }
            default: {
                CrdOpaqueRecord r;
                r.kind = kind;
                r.fields.assign(toks.begin() + 1, toks.end());
                file.opaque.push_back(r);
                break;
            }
        }
    }
    if (!have_format) {
        return odl::err(CrdError{"IOFM-F-001", "CRD file has no H1 format header record"});
    }
    return file;
}

namespace {
std::string fmt_double(double v, int decimals) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << v;
    return os.str();
}
std::string opt_to_str(const std::optional<int>& v) {
    return v.has_value() ? std::to_string(*v) : "na";
}
}  // namespace

odl::Result<std::string, CrdError> write_crd(const CrdFile& file) {
    std::ostringstream out;
    out << "H1 CRD " << file.format.version << " " << file.format.year << " " << file.format.month
        << " " << file.format.day << " " << file.format.hour << "\n";
    for (const auto& s : file.stations) {
        out << "H2 " << s.name << " " << s.system_id << " " << s.system_number << " " << s.occupancy
            << " " << s.epoch_time_scale << " " << s.network << "\n";
    }
    for (const auto& t : file.targets) {
        out << "H3 " << t.name << " " << t.ilrs_id << " " << t.sic << " " << t.norad_id << " "
            << t.spacecraft_time_scale << " " << t.target_class << " " << t.target_location << "\n";
    }
    for (const auto& h : file.sessions) {
        out << "H4 " << h.data_type << " " << h.start_year << " " << h.start_month << " "
            << h.start_day << " " << h.start_hour << " " << h.start_minute << " " << h.start_second
            << " " << opt_to_str(h.end_year) << " " << opt_to_str(h.end_month) << " "
            << opt_to_str(h.end_day) << " " << opt_to_str(h.end_hour) << " " << opt_to_str(h.end_minute)
            << " " << opt_to_str(h.end_second) << " " << h.release_flag << " "
            << (h.tropo_applied ? 1 : 0) << " " << (h.com_applied ? 1 : 0) << " "
            << (h.receive_amp_applied ? 1 : 0);
        for (const auto& f : h.remaining_fields) out << " " << f;
        out << "\n";
    }
    for (const auto& r : file.ranges) {
        out << (r.kind == CrdRecordKind::FullRateRange ? "10" : "11") << " "
            << fmt_double(r.seconds_of_day, 12) << " " << fmt_double(r.time_of_flight_s, 12) << " "
            << r.config_id << " " << r.epoch_event;
        for (const auto& f : r.remaining_fields) out << " " << f;
        out << "\n";
    }
    for (const auto& op : file.opaque) {
        static const std::pair<CrdRecordKind, const char*> kNames[] = {
            {CrdRecordKind::Config0, "C0"}, {CrdRecordKind::Config1, "C1"}, {CrdRecordKind::Config2, "C2"},
            {CrdRecordKind::Config3, "C3"}, {CrdRecordKind::Config4, "C4"}, {CrdRecordKind::Config5, "C5"},
            {CrdRecordKind::Config6, "C6"}, {CrdRecordKind::Config7, "C7"},
            {CrdRecordKind::RangeSupplement, "12"}, {CrdRecordKind::Meteorological, "20"},
            {CrdRecordKind::SkyQuality, "21"}, {CrdRecordKind::Angles, "30"},
            {CrdRecordKind::Calibration40, "40"}, {CrdRecordKind::Calibration41, "41"},
            {CrdRecordKind::Calibration42, "42"}, {CrdRecordKind::SessionStatistics, "50"},
            {CrdRecordKind::Compatibility, "60"}, {CrdRecordKind::Comment, "00"},
            {CrdRecordKind::Prediction, "H5"}, {CrdRecordKind::EndOfSession, "H8"},
            {CrdRecordKind::EndOfFile, "H9"}, {CrdRecordKind::UserDefined, "90"},
        };
        const char* tag = "00";
        for (const auto& kv : kNames) if (kv.first == op.kind) { tag = kv.second; break; }
        out << tag;
        for (const auto& f : op.fields) out << " " << f;
        out << "\n";
    }
    return out.str();
}

bool operator==(const CrdFormatHeader& a, const CrdFormatHeader& b) noexcept {
    return a.version == b.version && a.year == b.year && a.month == b.month && a.day == b.day &&
           a.hour == b.hour;
}
bool operator==(const CrdStationHeader& a, const CrdStationHeader& b) noexcept {
    return a.name == b.name && a.system_id == b.system_id && a.system_number == b.system_number &&
           a.occupancy == b.occupancy && a.epoch_time_scale == b.epoch_time_scale && a.network == b.network;
}
bool operator==(const CrdTargetHeader& a, const CrdTargetHeader& b) noexcept {
    return a.name == b.name && a.ilrs_id == b.ilrs_id && a.sic == b.sic && a.norad_id == b.norad_id &&
           a.spacecraft_time_scale == b.spacecraft_time_scale && a.target_class == b.target_class &&
           a.target_location == b.target_location;
}
bool operator==(const CrdSessionHeader& a, const CrdSessionHeader& b) noexcept {
    return a.data_type == b.data_type && a.start_year == b.start_year && a.start_month == b.start_month &&
           a.start_day == b.start_day && a.start_hour == b.start_hour && a.start_minute == b.start_minute &&
           a.start_second == b.start_second && a.end_year == b.end_year && a.end_month == b.end_month &&
           a.end_day == b.end_day && a.end_hour == b.end_hour && a.end_minute == b.end_minute &&
           a.end_second == b.end_second && a.release_flag == b.release_flag &&
           a.tropo_applied == b.tropo_applied && a.com_applied == b.com_applied &&
           a.receive_amp_applied == b.receive_amp_applied && a.remaining_fields == b.remaining_fields;
}
bool operator==(const CrdRangeRecord& a, const CrdRangeRecord& b) noexcept {
    return a.kind == b.kind && std::abs(a.seconds_of_day - b.seconds_of_day) < 1e-9 &&
           std::abs(a.time_of_flight_s - b.time_of_flight_s) < 1e-12 && a.config_id == b.config_id &&
           a.epoch_event == b.epoch_event && a.remaining_fields == b.remaining_fields;
}
bool operator==(const CrdOpaqueRecord& a, const CrdOpaqueRecord& b) noexcept {
    return a.kind == b.kind && a.fields == b.fields;
}
bool operator==(const CrdFile& a, const CrdFile& b) noexcept {
    return a.format == b.format && a.stations == b.stations && a.targets == b.targets &&
           a.sessions == b.sessions && a.ranges == b.ranges && a.opaque == b.opaque;
}

}  // namespace odl::io
