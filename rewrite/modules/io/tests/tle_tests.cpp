// tle_tests.cpp — SPEC-io-formats.md §8, IOFM-A-006/007/013.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/tle.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

namespace {

/// `STR3` (Spacetrack Report No. 3) §13's own first sample test case (SGP/
/// SGP4 satellite 88888), field values read directly from the extracted
/// text, placed at `TLEFMT`'s own documented column positions (the same
/// build-and-width-verify discipline `sp3_tests.cpp`'s own fixtures use --
/// not hand-copied spacing, which a scanned 1980 report's own OCR/extraction
/// cannot be trusted for). The checksum digit is COMPUTED by this same
/// discipline's own `tle_checksum` (a mechanical, standard algorithm
/// `TLEFMT` states in full, not itself a value the source needs to supply).
const char* kLine1 = "1 88888U          80275.98708465  .00073094  13844-3  66816-4 0    87";
const char* kLine2 = "2 88888  72.8435 115.9689 0086731  52.6988 110.5714 16.05824518  1058";

}  // namespace

TEST_CASE("IOFM-A-006  read_tle on STR3's own SGP4 sample element set (satellite "
          "88888) reproduces every field; the checksum validates",
          "[io][tle]") {
    auto t = read_tle(kLine1, kLine2);
    REQUIRE(t.has_value());
    CHECK(t->satellite_number == 88888);
    CHECK(t->classification == 'U');
    // Satellite 88888 is STR3's own synthetic verification target, not a real
    // launched object -- it carries no international designator at all,
    // legitimately absent rather than zero (Tle's own header comment).
    CHECK_FALSE(t->intl_designator_year.has_value());
    CHECK_FALSE(t->intl_designator_number.has_value());
    CHECK(t->intl_designator_piece.empty());
    CHECK(t->epoch_year == 80);
    CHECK_THAT(t->epoch_day, WithinAbs(275.98708465, 1e-8));
    CHECK_THAT(t->mean_motion_dot, WithinAbs(0.00073094, 1e-8));
    CHECK_THAT(t->mean_motion_ddot, WithinAbs(0.13844e-3, 1e-9));
    CHECK_THAT(t->bstar, WithinAbs(0.66816e-4, 1e-9));
    CHECK(t->ephemeris_type == 0);
    CHECK(t->element_number == 8);
    CHECK_THAT(t->inclination_deg, WithinAbs(72.8435, 1e-4));
    CHECK_THAT(t->raan_deg, WithinAbs(115.9689, 1e-4));
    CHECK_THAT(t->eccentricity, WithinAbs(0.0086731, 1e-7));
    CHECK_THAT(t->arg_perigee_deg, WithinAbs(52.6988, 1e-4));
    CHECK_THAT(t->mean_anomaly_deg, WithinAbs(110.5714, 1e-4));
    CHECK_THAT(t->mean_motion_rev_per_day, WithinAbs(16.05824518, 1e-8));
    CHECK(t->revolution_number == 105);
}

TEST_CASE("IOFM-A-006b  tle_checksum matches TLEFMT's own modulo-10 rule: digits "
          "at face value, '-' as 1, everything else as 0",
          "[io][tle]") {
    // Both lines above were constructed with the checksum computed this same
    // way; an independent hand check on a short, simple string:
    CHECK(tle_checksum("1234567890") == 5);        // sum 45, mod 10 = 5
    CHECK(tle_checksum("1-1") == tle_checksum("111"));  // '-' counts as 1, same as a literal 1
}

TEST_CASE("IOFM-A-006c  a corrupted checksum digit refuses IOFM-F-007",
          "[io][tle][gate]") {
    std::string bad_line1 = kLine1;
    bad_line1.back() = (bad_line1.back() == '7') ? '6' : '7';  // flip the checksum digit
    auto t = read_tle(bad_line1, kLine2);
    REQUIRE_FALSE(t.has_value());
    CHECK(t.error().id == "IOFM-F-007");
}

TEST_CASE("IOFM-A-006d  disagreeing satellite numbers between the two lines refuse",
          "[io][tle]") {
    std::string bad_line2 = kLine2;
    bad_line2.replace(2, 5, "99999");
    // Recompute a valid checksum for the tampered line so the failure is
    // specifically the cross-line disagreement, not an incidental checksum
    // mismatch.
    int chk = tle_checksum(std::string_view(bad_line2).substr(0, 68));
    bad_line2.back() = static_cast<char>('0' + chk);
    auto t = read_tle(kLine1, bad_line2);
    REQUIRE_FALSE(t.has_value());
    CHECK(t.error().id == "IOFM-F-001");
}

TEST_CASE("IOFM-A-013b  TLE round-trips: read(write(read(fixture))) == read(fixture)",
          "[io][tle][gate]") {
    auto t1 = read_tle(kLine1, kLine2);
    REQUIRE(t1.has_value());
    auto lines2 = write_tle(*t1);
    REQUIRE(lines2.has_value());
    CHECK(lines2->first.size() == 69);
    CHECK(lines2->second.size() == 69);
    auto t2 = read_tle(lines2->first, lines2->second);
    REQUIRE(t2.has_value());
    CHECK(*t1 == *t2);
}

TEST_CASE("IOFM-A-013c  a hand-built Tle round-trips (self-consistency)",
          "[io][tle]") {
    Tle t;
    t.satellite_number = 25544;
    t.classification = 'U';
    t.intl_designator_year = 98;
    t.intl_designator_number = 67;
    t.intl_designator_piece = "A";
    t.epoch_year = 26;
    t.epoch_day = 45.12345678;
    t.mean_motion_dot = 0.00001234;
    t.mean_motion_ddot = 0.0;
    t.bstar = 0.12345e-4;
    t.ephemeris_type = 0;
    t.element_number = 999;
    t.inclination_deg = 51.6416;
    t.raan_deg = 247.4627;
    t.eccentricity = 0.0006703;
    t.arg_perigee_deg = 130.5360;
    t.mean_anomaly_deg = 325.0288;
    t.mean_motion_rev_per_day = 15.72125391;
    t.revolution_number = 12345;

    auto lines = write_tle(t);
    REQUIRE(lines.has_value());
    auto back = read_tle(lines->first, lines->second);
    REQUIRE(back.has_value());
    CHECK(t == *back);
}
