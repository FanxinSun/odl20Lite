// iod_tests.cpp — SPEC-io-formats.md §8, IOFM-A-010/013, and (v1.1) IOFM-A-039 … -041, the angle and uncertainty decoders.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/iod.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// Every fixture below is built directly at IODFMT's own documented column
// positions (not a transcribed printed example -- this format's own worked
// examples were reached only through an AI-summarised re-fetch, not this
// session's own direct pdftotext extraction, and are not trusted to the same
// byte-exact standard the SP3-d PDF was). This is still a real check of this
// reader against its normative source (the column table itself), just not a
// rank-1 "published worked example" claim -- SPEC-io-formats.md `IOFM-A-010`.

TEST_CASE("IOFM-A-010  read_iod on a column-verified fixture reproduces every "
          "field, including the mantissa-exponent-adjacent uncertainty fields "
          "carried raw (SPEC-io-formats.md §3.5's own stated scope)",
          "[io][iod]") {
    std::string line(80, ' ');
    auto put = [&](int first, std::string_view s) {
        for (std::size_t i = 0; i < s.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = s[i];
    };
    put(1, "12345 98067A");
    put(17, "2420");
    put(22, "G");
    put(24, "20260115");
    put(32, "2213");
    put(36, "45678");
    put(42, "56");
    put(45, "2");
    put(46, "5");
    put(48, "0917234+123456");
    put(63, "25");
    put(66, "S");
    put(67, "+056");
    put(72, "05");
    put(75, "008500");

    auto o = read_iod(line);
    REQUIRE(o.has_value());
    CHECK(o->designation == "12345 98067A");
    CHECK(o->station_number == 2420);
    REQUIRE(o->station_status.has_value());
    CHECK(*o->station_status == 'G');
    CHECK(o->year == 2026);
    CHECK(o->month == 1);
    CHECK(o->day == 15);
    CHECK(o->hour == 22);
    CHECK(o->minute == 13);
    CHECK_THAT(o->second, WithinAbs(45.678, 1e-6));
    CHECK(o->time_uncertainty == "56");
    CHECK(o->angle_format == IodAngleFormat::RaDecArcmin);
    REQUIRE(o->epoch_code.has_value());
    CHECK(*o->epoch_code == 5);
    CHECK(o->angle_raw == "0917234+123456");
    CHECK(o->positional_uncertainty == "25");
    REQUIRE(o->optical_behavior.has_value());
    CHECK(*o->optical_behavior == 'S');
    REQUIRE(o->visual_magnitude.has_value());
    CHECK_THAT(*o->visual_magnitude, WithinAbs(5.6, 1e-6));
    CHECK(o->magnitude_uncertainty == "05");
    CHECK(o->flash_period == "008500");
}

TEST_CASE("IOFM-A-005b  an unrecognised IOD angle-format code refuses IOFM-F-005",
          "[io][iod]") {
    std::string line(80, ' ');
    auto put = [&](int first, std::string_view s) {
        for (std::size_t i = 0; i < s.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = s[i];
    };
    put(1, "12345 98067A"); put(17, "2420"); put(24, "20260115"); put(32, "22134567");
    put(45, "9");  // not one of 1-7
    auto o = read_iod(line);
    REQUIRE_FALSE(o.has_value());
    CHECK(o.error().id == "IOFM-F-005");
}

TEST_CASE("IOFM-A-013f  IOD round-trips: read(write(read(fixture))) == read(fixture)",
          "[io][iod][gate]") {
    std::string line(80, ' ');
    auto put = [&](int first, std::string_view s) {
        for (std::size_t i = 0; i < s.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = s[i];
    };
    put(1, "12345 98067A");
    put(17, "2420");
    put(22, "G");
    put(24, "20260115");
    put(32, "2213");
    put(36, "45678");
    put(42, "56");
    put(45, "2");
    put(46, "5");
    put(48, "0917234+123456");
    put(63, "25");
    put(66, "S");
    put(67, "+056");
    put(72, "05");
    put(75, "008500");

    auto o1 = read_iod(line);
    REQUIRE(o1.has_value());
    auto text2 = write_iod(*o1);
    REQUIRE(text2.has_value());
    auto o2 = read_iod(*text2);
    REQUIRE(o2.has_value());
    CHECK(*o1 == *o2);
}

// ---- v1.1: the angle and uncertainty decoders (IOFM-R-011, -R-012, IOFM-F-016, -F-017) ------------------------------------------------------------------------------------

namespace {
constexpr long double kPiL = 3.14159265358979323846264338327950288L;
double rad_of_deg(long double d) { return static_cast<double>(d * kPiL / 180.0L); }
double rad_of_hours(long double h) { return rad_of_deg(h * 15.0L); }
}  // namespace

TEST_CASE("IOFM-A-039  the IOD angle decoder against the document's own four examples (formats 1, 2, 3 and 7) and hand-built lines for formats 4, 5 and 6 to the printed layout: "
          "blanks are zeros, the sign, the wrap of azimuth into [0, 2 pi)",
          "[io][iod]") {
    struct Case { char fmt; const char* raw; IodAngleKind kind; double first; double second; };
    const Case cases[] = {
        // IODFMT's own examples
        {'1', "1122334+112233", IodAngleKind::RaDec, rad_of_hours(11.0L + 22.0L / 60.0L + 33.4L / 3600.0L), rad_of_deg(11.0L + 22.0L / 60.0L + 33.0L / 3600.0L)},
        {'2', "1122   +1122  ", IodAngleKind::RaDec, rad_of_hours(11.0L + 22.0L / 60.0L), rad_of_deg(11.0L + 22.0L / 60.0L)},
        {'3', "11223  +112   ", IodAngleKind::RaDec, rad_of_hours(11.0L + 22.3L / 60.0L), rad_of_deg(11.2L)},
        {'7', "1122334+112222", IodAngleKind::RaDec, rad_of_hours(11.0L + 22.0L / 60.0L + 33.4L / 3600.0L), rad_of_deg(11.2222L)},
        // formats 4, 5, 6, to the printed layout (the document prints no azimuth/elevation example)
        {'4', "1234556-012345", IodAngleKind::AzEl, rad_of_deg(123.0L + 45.0L / 60.0L + 56.0L / 3600.0L), -rad_of_deg(1.0L + 23.0L / 60.0L + 45.0L / 3600.0L)},
        {'5', "2301250+451230", IodAngleKind::AzEl, rad_of_deg(230.0L + 12.50L / 60.0L), rad_of_deg(45.0L + 12.30L / 60.0L)},
        {'6', "1234567+891234", IodAngleKind::AzEl, rad_of_deg(123.4567L), rad_of_deg(89.1234L)},
        // the extremes and the wrap: the first coordinate's range ends are 0 and (just below) 24 h / exactly 360 degrees, which is 0
        {'1', "0000000+000000", IodAngleKind::RaDec, 0.0, 0.0},
        {'1', "2359599-900000", IodAngleKind::RaDec, rad_of_hours(23.0L + 59.0L / 60.0L + 59.9L / 3600.0L), -rad_of_deg(90.0L)},
        {'6', "3600000+000000", IodAngleKind::AzEl, 0.0, 0.0},
        {'6', "3599999-900000", IodAngleKind::AzEl, rad_of_deg(359.9999L), -rad_of_deg(90.0L)},
    };
    for (const Case& c : cases) {
        INFO("format " << c.fmt << ", '" << c.raw << "'");
        auto a = decode_iod_angles(c.fmt, c.raw);
        REQUIRE(a.has_value());
        CHECK(a->kind == c.kind);
        CHECK_THAT(a->first_rad, WithinAbs(c.first, 1e-15));
        CHECK_THAT(a->second_rad, WithinAbs(c.second, 1e-15));
        CHECK(a->first_rad >= 0.0);
        CHECK(a->first_rad < 2.0 * 3.14159265358979323846);
    }
    // through an observation: the same decoding by the format the line carries
    IodObservation o;
    o.angle_format = IodAngleFormat::RaDecMixed;
    o.angle_raw = "1122334+112222";
    auto via = decode_iod_angles(o);
    REQUIRE(via.has_value());
    CHECK_THAT(via->first_rad, WithinAbs(rad_of_hours(11.0L + 22.0L / 60.0L + 33.4L / 3600.0L), 1e-15));
}

TEST_CASE("IOFM-A-040  the decoder's refusals, each alone: minutes 60, hours 24, azimuth 361 degrees, declination 91 degrees, a letter in a digit position, a blank sign, a blank "
          "format code refuse IOFM-F-016; the adjacent in-range values pass",
          "[io][iod]") {
    const struct { char fmt; const char* raw; const char* why; } refused[] = {
        {'1', "1160334+112233", "minutes 60"},       {'1', "2422334+112233", "hours 24"},        {'6', "3610000+000000", "azimuth 361 degrees"},
        {'1', "1122334+912233", "declination 91"},    {'1', "11A2334+112233", "a letter in a digit position"},
        {'1', "1122334 112233", "a blank sign"},      {' ', "1122334+112233", "a blank format code"},
        {'1', "1122600+112233", "seconds 60"},        {'1', "1122334+116033", "declination arcminutes 60"},
        {'4', "1236056+012345", "azimuth arcminutes 60"}, {'6', "3600001+000000", "azimuth above 360 by a ten-thousandth"},
        {'6', "1234567+900001", "elevation above 90 by a ten-thousandth"}, {'8', "1122334+112233", "a format code that is not one of the seven"},
        {'1', "1122334*112233", "a sign that is neither + nor -"},
    };
    for (const auto& r : refused) {
        INFO(r.why);
        auto a = decode_iod_angles(r.fmt, r.raw);
        REQUIRE_FALSE(a.has_value());
        CHECK(a.error().id == "IOFM-F-016");
    }
    const struct { char fmt; const char* raw; const char* why; } accepted[] = {
        {'1', "1159334+112233", "minutes 59"},        {'1', "2322334+112233", "hours 23"},        {'6', "3600000+000000", "azimuth 360 degrees"},
        {'1', "1122334+900000", "declination 90"},    {'1', "1122359+115959", "declination arcseconds 59"},      {'4', "3595959+895959", "all sixty-minus-one"},
    };
    for (const auto& r : accepted) {
        INFO(r.why);
        CHECK(decode_iod_angles(r.fmt, r.raw).has_value());
    }
    // a field that is not the 14 characters of columns 48-61
    CHECK_FALSE(decode_iod_angles('1', "1122334+11223").has_value());
}

TEST_CASE("IOFM-A-041  the uncertainties: the document's own MX examples — 15 is 0.001, 56 is 0.05, 17 is 0.1, 97 is 0.9, 18 is 1, 28 is 2, 58 is 5, 19 is 10, 99 is 90 — in the time "
          "unit and in each format's angle unit converted to radians; two blanks give no value; '1 ' and 'ab' refuse IOFM-F-017",
          "[io][iod]") {
    const struct { const char* mx; double value; } examples[] = {{"15", 0.001}, {"56", 0.05}, {"17", 0.1}, {"97", 0.9}, {"18", 1.0}, {"28", 2.0}, {"58", 5.0}, {"19", 10.0}, {"99", 90.0}};
    for (const auto& e : examples) {
        INFO("MX " << e.mx);
        IodObservation o;
        o.time_uncertainty = e.mx;
        auto t = decode_iod_time_uncertainty(o);
        REQUIRE(t.has_value());
        REQUIRE(t->has_value());
        CHECK_THAT(**t, WithinRel(e.value, 1e-15));
        // the positional uncertainty by the format's unit
        const struct { IodAngleFormat fmt; double unit_rad; } units[] = {
            {IodAngleFormat::RaDecArcsec, static_cast<double>(kPiL / 180.0L / 3600.0L)}, {IodAngleFormat::AzElArcsec, static_cast<double>(kPiL / 180.0L / 3600.0L)},
            {IodAngleFormat::RaDecArcmin, static_cast<double>(kPiL / 180.0L / 60.0L)},   {IodAngleFormat::AzElArcmin, static_cast<double>(kPiL / 180.0L / 60.0L)},
            {IodAngleFormat::RaDecDeg, static_cast<double>(kPiL / 180.0L)},              {IodAngleFormat::AzElDeg, static_cast<double>(kPiL / 180.0L)},
            {IodAngleFormat::RaDecMixed, static_cast<double>(kPiL / 180.0L)}};
        for (const auto& u : units) {
            IodObservation p;
            p.angle_format = u.fmt;
            p.positional_uncertainty = e.mx;
            auto v = decode_iod_position_uncertainty(p);
            REQUIRE(v.has_value());
            REQUIRE(v->has_value());
            CHECK_THAT(**v, WithinRel(e.value * u.unit_rad, 1e-15));
        }
    }
    // two blanks (the reader's trimmed empty field): not reported
    IodObservation none;
    auto t0 = decode_iod_time_uncertainty(none);
    REQUIRE(t0.has_value());
    CHECK_FALSE(t0->has_value());
    auto p0 = decode_iod_position_uncertainty(none);
    REQUIRE(p0.has_value());
    CHECK_FALSE(p0->has_value());
    // a digit and a blank, a blank and a digit (both are "1" once the reader has trimmed them), a non-digit, three characters
    for (const char* bad : {"1", "ab", "1a", "a1", "123", "-1"}) {
        INFO("field '" << bad << "'");
        IodObservation o;
        o.time_uncertainty = bad;
        o.positional_uncertainty = bad;
        auto t = decode_iod_time_uncertainty(o);
        REQUIRE_FALSE(t.has_value());
        CHECK(t.error().id == "IOFM-F-017");
        auto p = decode_iod_position_uncertainty(o);
        REQUIRE_FALSE(p.has_value());
        CHECK(p.error().id == "IOFM-F-017");
    }
    // the reader keeps what a real line carries: 'put(42, "56")' of IOFM-A-010 is 0.05 s, and its '25' positional uncertainty in format 2 (arcminutes) is 2 x 10^-3 arcminute
    std::string line(80, ' ');
    auto put = [&](int first, std::string_view text) {
        for (std::size_t i = 0; i < text.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = text[i];
    };
    put(1, "12345 98067A");
    put(17, "2420");
    put(24, "20260115");
    put(32, "2213");
    put(36, "45678");
    put(42, "56");
    put(45, "2");
    put(46, "5");
    put(48, "0917234+123456");
    put(63, "25");
    auto obs = read_iod(line);
    REQUIRE(obs.has_value());
    auto tu = decode_iod_time_uncertainty(*obs);
    REQUIRE(tu.has_value());
    REQUIRE(tu->has_value());
    CHECK_THAT(**tu, WithinRel(0.05, 1e-15));
    auto pu = decode_iod_position_uncertainty(*obs);
    REQUIRE(pu.has_value());
    REQUIRE(pu->has_value());
    CHECK_THAT(**pu, WithinRel(0.002 * 3.14159265358979323846 / 180.0 / 60.0, 1e-13));     // M = 2, X = 5: 2 x 10^(5-8) arcminute
}
