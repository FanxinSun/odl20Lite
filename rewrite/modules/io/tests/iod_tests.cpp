// iod_tests.cpp — SPEC-io-formats.md §8, IOFM-A-010/013.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/iod.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

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
