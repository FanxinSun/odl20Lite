// cpf.cpp — SPEC-io-formats.md §3.4 (`CPF2`). FREE FORMAT: whitespace-tokenised.

#include <odl/io/cpf.hpp>

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

odl::Result<int, CpfError> to_int(const std::string& s, int line_no, std::string_view field) {
    int v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        return odl::err(CpfError{"IOFM-F-001", "CPF line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not an integer: '" + s + "'"});
    }
    return v;
}

odl::Result<long, CpfError> to_long(const std::string& s, int line_no, std::string_view field) {
    long v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        return odl::err(CpfError{"IOFM-F-001", "CPF line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not an integer: '" + s + "'"});
    }
    return v;
}

odl::Result<double, CpfError> to_double(const std::string& s, int line_no, std::string_view field) {
    try {
        std::size_t consumed = 0;
        double v = std::stod(s, &consumed);
        if (consumed != s.size()) throw std::invalid_argument("trailing");
        return v;
    } catch (const std::exception&) {
        return odl::err(CpfError{"IOFM-F-001", "CPF line " + std::to_string(line_no) +
            ": field '" + std::string(field) + "' is not a number: '" + s + "'"});
    }
}

odl::Result<void, CpfError> need(const std::vector<std::string>& toks, std::size_t n, int line_no,
                                 std::string_view kind) {
    if (toks.size() < n) {
        return odl::err(CpfError{"IOFM-F-001", "CPF line " + std::to_string(line_no) + ": " +
            std::string(kind) + " needs at least " + std::to_string(n) + " fields, found " +
            std::to_string(toks.size())});
    }
    return {};
}

odl::Result<CpfDirectionFlag, CpfError> to_direction(const std::string& s, int line_no) {
    if (s == "0") return CpfDirectionFlag::Common;
    if (s == "1") return CpfDirectionFlag::Transmit;
    if (s == "2") return CpfDirectionFlag::Receive;
    return odl::err(CpfError{"IOFM-F-001", "CPF line " + std::to_string(line_no) +
        ": direction flag is '" + s + "', not 0, 1 or 2"});
}

}  // namespace

odl::Result<CpfFile, CpfError> read_cpf(std::string_view text) {
    CpfFile file;
    bool have_h1 = false, have_h2 = false;
    auto lines = split_lines(text);
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        auto toks = tokenize(lines[static_cast<std::size_t>(i)]);
        if (toks.empty()) continue;
        int ln = i + 1;
        const std::string& tag = toks[0];

        if (tag == "H1") {
            auto ok = need(toks, 11, ln, "H1"); if (!ok.has_value()) return odl::err(ok.error());
            CpfHeader1 h;
            auto v = to_int(toks[2], ln, "version"); if (!v.has_value()) return odl::err(v.error());
            h.version = *v;
            h.source = toks[3];
            auto py = to_int(toks[4], ln, "prod year"); if (!py.has_value()) return odl::err(py.error());
            h.prod_year = *py;
            auto pmo = to_int(toks[5], ln, "prod month"); if (!pmo.has_value()) return odl::err(pmo.error());
            h.prod_month = *pmo;
            auto pd = to_int(toks[6], ln, "prod day"); if (!pd.has_value()) return odl::err(pd.error());
            h.prod_day = *pd;
            auto ph = to_int(toks[7], ln, "prod hour"); if (!ph.has_value()) return odl::err(ph.error());
            h.prod_hour = *ph;
            auto sq = to_int(toks[8], ln, "sequence number"); if (!sq.has_value()) return odl::err(sq.error());
            h.sequence_number = *sq;
            auto sdq = to_int(toks[9], ln, "sub-daily sequence number"); if (!sdq.has_value()) return odl::err(sdq.error());
            h.sub_daily_sequence_number = *sdq;
            h.target_name = toks[10];
            if (toks.size() > 11) h.notes = toks[11];
            file.header1 = h;
            have_h1 = true;
        } else if (tag == "H2") {
            auto ok = need(toks, 23, ln, "H2"); if (!ok.has_value()) return odl::err(ok.error());
            CpfHeader2 h;
            auto id = to_long(toks[1], ln, "ilrs id"); if (!id.has_value()) return odl::err(id.error());
            h.ilrs_id = *id;
            auto sic = to_long(toks[2], ln, "sic"); if (!sic.has_value()) return odl::err(sic.error());
            h.sic = *sic;
            auto nid = to_long(toks[3], ln, "norad id"); if (!nid.has_value()) return odl::err(nid.error());
            h.norad_id = *nid;
            int* ints[] = {&h.start_year, &h.start_month, &h.start_day, &h.start_hour, &h.start_minute,
                           &h.start_second, &h.end_year, &h.end_month, &h.end_day, &h.end_hour,
                           &h.end_minute, &h.end_second, &h.table_step_s, &h.compatible_with_tiv,
                           &h.target_class, &h.reference_frame, &h.rotational_angle_type,
                           &h.com_correction, &h.target_location};
            for (std::size_t k = 0; k < 19; ++k) {
                auto v = to_int(toks[4 + k], ln, "H2 field"); if (!v.has_value()) return odl::err(v.error());
                *ints[k] = *v;
            }
            file.header2 = h;
            have_h2 = true;
        } else if (tag == "10") {
            auto ok = need(toks, 8, ln, "position record"); if (!ok.has_value()) return odl::err(ok.error());
            CpfPositionRecord r;
            auto dir = to_direction(toks[1], ln); if (!dir.has_value()) return odl::err(dir.error());
            r.direction = *dir;
            auto mjd = to_int(toks[2], ln, "mjd"); if (!mjd.has_value()) return odl::err(mjd.error());
            r.mjd = *mjd;
            auto sod = to_double(toks[3], ln, "seconds of day"); if (!sod.has_value()) return odl::err(sod.error());
            r.seconds_of_day = *sod;
            auto lf = to_int(toks[4], ln, "leap second flag"); if (!lf.has_value()) return odl::err(lf.error());
            r.leap_second_flag = *lf;
            auto x = to_double(toks[5], ln, "x"); if (!x.has_value()) return odl::err(x.error());
            r.x_m = *x;
            auto y = to_double(toks[6], ln, "y"); if (!y.has_value()) return odl::err(y.error());
            r.y_m = *y;
            auto z = to_double(toks[7], ln, "z"); if (!z.has_value()) return odl::err(z.error());
            r.z_m = *z;
            file.positions.push_back(r);
        } else {
            static const char* kKnown[] = {"H3","H4","H5","H9","20","30","40","50","60","70","99","00"};
            bool ok = false;
            for (const char* k : kKnown) if (tag == k) { ok = true; break; }
            if (!ok) {
                return odl::err(CpfError{"IOFM-F-004", "CPF line " + std::to_string(ln) +
                    ": record type '" + tag + "' is not one of CPF2's own record types"});
            }
            CpfOpaqueRecord r;
            r.tag = tag;
            r.fields.assign(toks.begin() + 1, toks.end());
            file.opaque.push_back(r);
        }
    }
    if (!have_h1) return odl::err(CpfError{"IOFM-F-001", "CPF file has no H1 header record"});
    if (!have_h2) return odl::err(CpfError{"IOFM-F-001", "CPF file has no H2 header record"});
    return file;
}

namespace {
std::string fmt(double v, int decimals) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << v;
    return os.str();
}
}  // namespace

odl::Result<std::string, CpfError> write_cpf(const CpfFile& file) {
    std::ostringstream out;
    const auto& h1 = file.header1;
    out << "H1 CPF " << h1.version << " " << h1.source << " " << h1.prod_year << " " << h1.prod_month
        << " " << h1.prod_day << " " << h1.prod_hour << " " << h1.sequence_number << " "
        << h1.sub_daily_sequence_number << " " << h1.target_name;
    if (!h1.notes.empty()) out << " " << h1.notes;
    out << "\n";

    const auto& h2 = file.header2;
    out << "H2 " << h2.ilrs_id << " " << h2.sic << " " << h2.norad_id << " "
        << h2.start_year << " " << h2.start_month << " " << h2.start_day << " " << h2.start_hour << " "
        << h2.start_minute << " " << h2.start_second << " "
        << h2.end_year << " " << h2.end_month << " " << h2.end_day << " " << h2.end_hour << " "
        << h2.end_minute << " " << h2.end_second << " "
        << h2.table_step_s << " " << h2.compatible_with_tiv << " " << h2.target_class << " "
        << h2.reference_frame << " " << h2.rotational_angle_type << " " << h2.com_correction << " "
        << h2.target_location << "\n";

    // Every other header/trailer record type this reader carries opaque
    // (H3-H5, H9, 20/30/.../99, comments), in the file's own original order.
    // Position records (`10`) are written afterward, as a block -- true
    // interleaving of `10` with another per-epoch record type (e.g. a `20`
    // velocity row after each position) is not reproduced byte-for-byte;
    // no fixture this reader is tested against interleaves them, and this
    // is the same class of scope limit `IOFM-Q-001` already states for CRD.
    for (const auto& op : file.opaque) {
        out << op.tag;
        for (const auto& f : op.fields) out << " " << f;
        out << "\n";
    }

    for (const auto& r : file.positions) {
        out << "10 " << static_cast<int>(r.direction) << " " << r.mjd << " " << fmt(r.seconds_of_day, 6)
            << " " << r.leap_second_flag << " " << fmt(r.x_m, 3) << " " << fmt(r.y_m, 3) << " "
            << fmt(r.z_m, 3) << "\n";
    }
    return out.str();
}

bool operator==(const CpfHeader1& a, const CpfHeader1& b) noexcept {
    return a.version == b.version && a.source == b.source && a.prod_year == b.prod_year &&
           a.prod_month == b.prod_month && a.prod_day == b.prod_day && a.prod_hour == b.prod_hour &&
           a.sequence_number == b.sequence_number &&
           a.sub_daily_sequence_number == b.sub_daily_sequence_number &&
           a.target_name == b.target_name && a.notes == b.notes;
}
bool operator==(const CpfHeader2& a, const CpfHeader2& b) noexcept {
    return a.ilrs_id == b.ilrs_id && a.sic == b.sic && a.norad_id == b.norad_id &&
           a.start_year == b.start_year && a.start_month == b.start_month && a.start_day == b.start_day &&
           a.start_hour == b.start_hour && a.start_minute == b.start_minute &&
           a.start_second == b.start_second && a.end_year == b.end_year && a.end_month == b.end_month &&
           a.end_day == b.end_day && a.end_hour == b.end_hour && a.end_minute == b.end_minute &&
           a.end_second == b.end_second && a.table_step_s == b.table_step_s &&
           a.compatible_with_tiv == b.compatible_with_tiv && a.target_class == b.target_class &&
           a.reference_frame == b.reference_frame && a.rotational_angle_type == b.rotational_angle_type &&
           a.com_correction == b.com_correction && a.target_location == b.target_location;
}
bool operator==(const CpfPositionRecord& a, const CpfPositionRecord& b) noexcept {
    return a.direction == b.direction && a.mjd == b.mjd &&
           std::abs(a.seconds_of_day - b.seconds_of_day) < 1e-6 &&
           // NOT-A-UNIT-CROSSING: equality tolerance in metres, not a conversion.
           a.leap_second_flag == b.leap_second_flag && std::abs(a.x_m - b.x_m) < 1e-3 &&
           // NOT-A-UNIT-CROSSING: same tolerance as above, y/z components.
           std::abs(a.y_m - b.y_m) < 1e-3 && std::abs(a.z_m - b.z_m) < 1e-3;
}
bool operator==(const CpfOpaqueRecord& a, const CpfOpaqueRecord& b) noexcept {
    return a.tag == b.tag && a.fields == b.fields;
}
bool operator==(const CpfFile& a, const CpfFile& b) noexcept {
    return a.header1 == b.header1 && a.header2 == b.header2 && a.positions == b.positions &&
           a.opaque == b.opaque;
}

}  // namespace odl::io
