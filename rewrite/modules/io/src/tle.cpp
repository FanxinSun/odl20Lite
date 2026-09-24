// tle.cpp — SPEC-io-formats.md §3.2 (`TLEFMT`).

#include <odl/io/tle.hpp>

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

odl::Result<std::string, TleError> column(std::string_view line, int first, int last,
                                          int which_line, std::string_view field) {
    if (static_cast<int>(line.size()) < last) {
        return odl::err(TleError{"IOFM-F-001",
            "TLE line " + std::to_string(which_line) + ": field '" + std::string(field) +
            "' expected at columns " + std::to_string(first) + "-" + std::to_string(last) +
            " but the line is only " + std::to_string(line.size()) + " characters wide"});
    }
    return std::string(line.substr(static_cast<std::size_t>(first - 1),
                                   static_cast<std::size_t>(last - first + 1)));
}

odl::Result<int, TleError> to_int(const std::string& raw, int which_line, std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) {
        return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
            ": field '" + std::string(field) + "' is blank"});
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), value);
    if (ec != std::errc{} || ptr != t.data() + t.size()) {
        return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
            ": field '" + std::string(field) + "' is not an integer: '" + t + "'"});
    }
    return value;
}

/// Blank (all spaces, or empty) -> nullopt: a synthetic/test satellite's own
/// international designator sub-fields, unlike every other integer field
/// this reader parses, are legitimately absent rather than zero.
odl::Result<std::optional<int>, TleError> to_int_opt(const std::string& raw, int which_line,
                                                      std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) return std::optional<int>{};
    auto v = to_int(t, which_line, field);
    if (!v.has_value()) return odl::err(v.error());
    return std::optional<int>(*v);
}

odl::Result<double, TleError> to_double(const std::string& raw, int which_line, std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) {
        return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
            ": field '" + std::string(field) + "' is blank"});
    }
    try {
        std::size_t consumed = 0;
        double value = std::stod(t, &consumed);
        if (consumed != t.size()) {
            return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
                ": field '" + std::string(field) + "' is not a number: '" + t + "'"});
        }
        return value;
    } catch (const std::exception&) {
        return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
            ": field '" + std::string(field) + "' is not a number: '" + t + "'"});
    }
}

/// The "assumed decimal point" + trailing signed exponent encoding TLEFMT's
/// own second-derivative and BSTAR fields use, 8 characters: [sign](5
/// mantissa digits)[exponent sign](1 exponent digit), decimal point assumed
/// before the mantissa. `" 12345-4"` = +0.12345e-4; `"-11606-4"` = -0.11606e-4.
odl::Result<double, TleError> decimal_assumed(const std::string& raw, int which_line,
                                              std::string_view field) {
    if (raw.size() != 8) {
        return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
            ": field '" + std::string(field) + "' is not 8 characters: '" + raw + "'"});
    }
    char sign = raw[0];
    std::string mantissa = raw.substr(1, 5);
    char exp_sign = raw[6];
    char exp_digit = raw[7];
    if ((sign != ' ' && sign != '-' && sign != '+') ||
        (exp_sign != ' ' && exp_sign != '-' && exp_sign != '+') ||
        !std::isdigit(static_cast<unsigned char>(exp_digit))) {
        return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
            ": field '" + std::string(field) + "' does not match the assumed-decimal "
            "encoding: '" + raw + "'"});
    }
    for (char c : mantissa) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return odl::err(TleError{"IOFM-F-001", "TLE line " + std::to_string(which_line) +
                ": field '" + std::string(field) + "' mantissa is not all digits: '" + raw + "'"});
        }
    }
    double m = std::stod("0." + mantissa);
    double e = (exp_digit - '0') * (exp_sign == '-' ? -1.0 : 1.0);
    return (sign == '-' ? -1.0 : 1.0) * m * std::pow(10.0, e);
}

std::string encode_decimal_assumed(double value) {
    char sign = value < 0.0 ? '-' : ' ';
    double av = std::fabs(value);
    int exponent = 0;
    if (av > 0.0) {
        exponent = static_cast<int>(std::floor(std::log10(av))) + 1;
        av = av / std::pow(10.0, exponent);
        // Guard rounding pushing the mantissa to 1.00000.
        if (av >= 1.0) { av /= 10.0; ++exponent; }
    }
    long mantissa = std::lround(av * 100000.0);
    if (mantissa >= 100000) { mantissa /= 10; ++exponent; }
    std::ostringstream os;
    os << sign;
    os.width(5); os.fill('0'); os << mantissa;
    os << (exponent < 0 ? '-' : '+') << std::abs(exponent) % 10;
    return os.str();
}

}  // namespace

int tle_checksum(std::string_view line) noexcept {
    int sum = 0;
    for (char c : line) {
        if (c >= '0' && c <= '9') sum += (c - '0');
        else if (c == '-') sum += 1;
    }
    return sum % 10;
}

odl::Result<Tle, TleError> read_tle(std::string_view line1, std::string_view line2) {
    if (line1.size() < 69 || line1[0] != '1') {
        return odl::err(TleError{"IOFM-F-001", "TLE line 1 does not begin with '1' or is too short"});
    }
    if (line2.size() < 69 || line2[0] != '2') {
        return odl::err(TleError{"IOFM-F-001", "TLE line 2 does not begin with '2' or is too short"});
    }

    auto check1 = tle_checksum(line1.substr(0, 68));
    auto given1 = line1[68] - '0';
    if (given1 < 0 || given1 > 9 || check1 != given1) {
        return odl::err(TleError{"IOFM-F-007", "TLE line 1 checksum mismatch: computed " +
            std::to_string(check1) + ", line states '" + std::string(1, line1[68]) + "'"});
    }
    auto check2 = tle_checksum(line2.substr(0, 68));
    auto given2 = line2[68] - '0';
    if (given2 < 0 || given2 > 9 || check2 != given2) {
        return odl::err(TleError{"IOFM-F-007", "TLE line 2 checksum mismatch: computed " +
            std::to_string(check2) + ", line states '" + std::string(1, line2[68]) + "'"});
    }

    Tle t;

    auto sn1 = column(line1, 3, 7, 1, "satellite number"); if (!sn1.has_value()) return odl::err(sn1.error());
    auto sn1v = to_int(*sn1, 1, "satellite number"); if (!sn1v.has_value()) return odl::err(sn1v.error());
    t.satellite_number = *sn1v;

    auto cls = column(line1, 8, 8, 1, "classification"); if (!cls.has_value()) return odl::err(cls.error());
    t.classification = (*cls)[0];

    auto idy = column(line1, 10, 11, 1, "intl designator year"); if (!idy.has_value()) return odl::err(idy.error());
    auto idyv = to_int_opt(*idy, 1, "intl designator year"); if (!idyv.has_value()) return odl::err(idyv.error());
    t.intl_designator_year = *idyv;
    auto idn = column(line1, 12, 14, 1, "intl designator number"); if (!idn.has_value()) return odl::err(idn.error());
    auto idnv = to_int_opt(*idn, 1, "intl designator number"); if (!idnv.has_value()) return odl::err(idnv.error());
    t.intl_designator_number = *idnv;
    auto idp = column(line1, 15, 17, 1, "intl designator piece"); if (!idp.has_value()) return odl::err(idp.error());
    t.intl_designator_piece = trim(*idp);

    auto ey = column(line1, 19, 20, 1, "epoch year"); if (!ey.has_value()) return odl::err(ey.error());
    auto eyv = to_int(*ey, 1, "epoch year"); if (!eyv.has_value()) return odl::err(eyv.error());
    t.epoch_year = *eyv;
    auto ed = column(line1, 21, 32, 1, "epoch day"); if (!ed.has_value()) return odl::err(ed.error());
    auto edv = to_double(*ed, 1, "epoch day"); if (!edv.has_value()) return odl::err(edv.error());
    t.epoch_day = *edv;

    auto mmd = column(line1, 34, 43, 1, "mean motion dot"); if (!mmd.has_value()) return odl::err(mmd.error());
    auto mmdv = to_double(*mmd, 1, "mean motion dot"); if (!mmdv.has_value()) return odl::err(mmdv.error());
    t.mean_motion_dot = *mmdv;

    auto mmdd = column(line1, 45, 52, 1, "mean motion ddot"); if (!mmdd.has_value()) return odl::err(mmdd.error());
    auto mmddv = decimal_assumed(*mmdd, 1, "mean motion ddot"); if (!mmddv.has_value()) return odl::err(mmddv.error());
    t.mean_motion_ddot = *mmddv;

    auto bst = column(line1, 54, 61, 1, "bstar"); if (!bst.has_value()) return odl::err(bst.error());
    auto bstv = decimal_assumed(*bst, 1, "bstar"); if (!bstv.has_value()) return odl::err(bstv.error());
    t.bstar = *bstv;

    auto eph = column(line1, 63, 63, 1, "ephemeris type"); if (!eph.has_value()) return odl::err(eph.error());
    auto ephv = to_int(*eph, 1, "ephemeris type"); if (!ephv.has_value()) return odl::err(ephv.error());
    t.ephemeris_type = *ephv;

    auto eln = column(line1, 65, 68, 1, "element number"); if (!eln.has_value()) return odl::err(eln.error());
    auto elnv = to_int(*eln, 1, "element number"); if (!elnv.has_value()) return odl::err(elnv.error());
    t.element_number = *elnv;

    auto sn2 = column(line2, 3, 7, 2, "satellite number"); if (!sn2.has_value()) return odl::err(sn2.error());
    auto sn2v = to_int(*sn2, 2, "satellite number"); if (!sn2v.has_value()) return odl::err(sn2v.error());
    if (*sn2v != t.satellite_number) {
        return odl::err(TleError{"IOFM-F-001", "TLE line 1 satellite number " +
            std::to_string(t.satellite_number) + " disagrees with line 2's " + std::to_string(*sn2v)});
    }

    auto inc = column(line2, 9, 16, 2, "inclination"); if (!inc.has_value()) return odl::err(inc.error());
    auto incv = to_double(*inc, 2, "inclination"); if (!incv.has_value()) return odl::err(incv.error());
    t.inclination_deg = *incv;
    auto raan = column(line2, 18, 25, 2, "raan"); if (!raan.has_value()) return odl::err(raan.error());
    auto raanv = to_double(*raan, 2, "raan"); if (!raanv.has_value()) return odl::err(raanv.error());
    t.raan_deg = *raanv;
    auto ecc = column(line2, 27, 33, 2, "eccentricity"); if (!ecc.has_value()) return odl::err(ecc.error());
    auto eccv = to_double("0." + *ecc, 2, "eccentricity"); if (!eccv.has_value()) return odl::err(eccv.error());
    t.eccentricity = *eccv;
    auto aop = column(line2, 35, 42, 2, "arg of perigee"); if (!aop.has_value()) return odl::err(aop.error());
    auto aopv = to_double(*aop, 2, "arg of perigee"); if (!aopv.has_value()) return odl::err(aopv.error());
    t.arg_perigee_deg = *aopv;
    auto ma = column(line2, 44, 51, 2, "mean anomaly"); if (!ma.has_value()) return odl::err(ma.error());
    auto mav = to_double(*ma, 2, "mean anomaly"); if (!mav.has_value()) return odl::err(mav.error());
    t.mean_anomaly_deg = *mav;
    auto mm = column(line2, 53, 63, 2, "mean motion"); if (!mm.has_value()) return odl::err(mm.error());
    auto mmv = to_double(*mm, 2, "mean motion"); if (!mmv.has_value()) return odl::err(mmv.error());
    t.mean_motion_rev_per_day = *mmv;
    auto rn = column(line2, 64, 68, 2, "revolution number"); if (!rn.has_value()) return odl::err(rn.error());
    auto rnv = to_int(*rn, 2, "revolution number"); if (!rnv.has_value()) return odl::err(rnv.error());
    t.revolution_number = *rnv;

    return t;
}

namespace {
std::string rj(long v, int w) {
    std::string s = std::to_string(v);
    if (static_cast<int>(s.size()) >= w) return s;
    return std::string(static_cast<std::size_t>(w) - s.size(), '0') + s;
}
std::string rj_blank(long v, int w) {
    std::string s = std::to_string(v);
    if (static_cast<int>(s.size()) >= w) return s;
    return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}
/// Line 2's own four angle fields (inclination, RAAN, argument of perigee,
/// mean anomaly) are never negative (each is in [0, 360) or [0, 180]) and are
/// right-justified with LEADING SPACES to `width`, not zero-padded and not
/// sign-prefixed -- `TLEFMT`'s own printed example, " 72.8435", is a blank
/// pad character, not a sign.
std::string angle_field(double v, int width, int decimals) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << v;
    std::string s = os.str();
    if (static_cast<int>(s.size()) < width) s = std::string(static_cast<std::size_t>(width) - s.size(), ' ') + s;
    return s;
}
}  // namespace

odl::Result<std::pair<std::string, std::string>, TleError> write_tle(const Tle& tle) {
    std::ostringstream body1;
    body1 << "1 " << rj_blank(tle.satellite_number, 5) << tle.classification << " "
          << (tle.intl_designator_year ? rj(*tle.intl_designator_year, 2) : "  ")
          << (tle.intl_designator_number ? rj(*tle.intl_designator_number, 3) : "   ")
          << (tle.intl_designator_piece + std::string(3, ' ')).substr(0, 3) << " "
          << rj(tle.epoch_year, 2);
    {
        std::ostringstream d;
        d.setf(std::ios::fixed); d.precision(8);
        d << tle.epoch_day;
        std::string s = d.str();
        auto dot = s.find('.');
        while (static_cast<int>(dot) < 3) { s.insert(0, "0"); dot = s.find('.'); }
        body1 << s;
    }
    {
        std::string sign = tle.mean_motion_dot < 0 ? "-" : " ";
        std::ostringstream d;
        d.setf(std::ios::fixed); d.precision(8);
        d << std::abs(tle.mean_motion_dot);
        std::string s = d.str();  // "0.xxxxxxxx"
        body1 << " " << sign << s.substr(1);  // drop leading '0', keep '.xxxxxxxx'
    }
    body1 << " " << encode_decimal_assumed(tle.mean_motion_ddot)
          << " " << encode_decimal_assumed(tle.bstar)
          << " " << tle.ephemeris_type << " " << rj_blank(tle.element_number, 4);
    std::string b1 = body1.str();
    int chk1 = tle_checksum(b1);
    std::ostringstream full1; full1 << b1 << chk1;

    std::ostringstream body2;
    body2 << "2 " << rj_blank(tle.satellite_number, 5) << " "
          << angle_field(tle.inclination_deg, 8, 4) << " " << angle_field(tle.raan_deg, 8, 4) << " ";
    {
        std::ostringstream e;
        e.setf(std::ios::fixed); e.precision(7);
        e << tle.eccentricity;
        std::string s = e.str();  // "0.xxxxxxx"
        body2 << s.substr(2);     // 7 digits, no leading "0."
    }
    body2 << " " << angle_field(tle.arg_perigee_deg, 8, 4) << " " << angle_field(tle.mean_anomaly_deg, 8, 4)
          << " ";
    {
        std::ostringstream mm;
        mm.setf(std::ios::fixed); mm.precision(8);
        mm << tle.mean_motion_rev_per_day;
        body2 << mm.str();
    }
    body2 << rj_blank(tle.revolution_number, 5);
    std::string b2 = body2.str();
    int chk2 = tle_checksum(b2);
    std::ostringstream full2; full2 << b2 << chk2;

    return std::pair<std::string, std::string>{full1.str(), full2.str()};
}

bool operator==(const Tle& a, const Tle& b) noexcept {
    auto near = [](double x, double y, double tol) { return std::abs(x - y) < tol; };
    return a.satellite_number == b.satellite_number && a.classification == b.classification &&
           a.intl_designator_year == b.intl_designator_year &&
           a.intl_designator_number == b.intl_designator_number &&
           a.intl_designator_piece == b.intl_designator_piece && a.epoch_year == b.epoch_year &&
           near(a.epoch_day, b.epoch_day, 1e-8) && near(a.mean_motion_dot, b.mean_motion_dot, 1e-9) &&
           near(a.mean_motion_ddot, b.mean_motion_ddot, 1e-9) && near(a.bstar, b.bstar, 1e-9) &&
           a.ephemeris_type == b.ephemeris_type && a.element_number == b.element_number &&
           near(a.inclination_deg, b.inclination_deg, 1e-4) && near(a.raan_deg, b.raan_deg, 1e-4) &&
           near(a.eccentricity, b.eccentricity, 1e-7) &&
           near(a.arg_perigee_deg, b.arg_perigee_deg, 1e-4) &&
           near(a.mean_anomaly_deg, b.mean_anomaly_deg, 1e-4) &&
           near(a.mean_motion_rev_per_day, b.mean_motion_rev_per_day, 1e-8) &&
           a.revolution_number == b.revolution_number;
}

}  // namespace odl::io
