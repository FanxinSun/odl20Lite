// antex_tests.cpp — SPEC-io-formats.md §8, IOFM-A-012/013.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/antex.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

namespace {

/// One antenna, one frequency, DAZI=0 (no azimuth-dependent grid), built at
/// ANTEX14's own documented column positions (data columns 1-60, the record
/// label at column 61) -- the same "place at the documented columns, don't
/// hand-copy a printed example" discipline `sp3_tests.cpp`'s own fixtures use.
const char* kFixture =
"       1.4           G                                      ANTEX VERSION / SYST\n"
"A                                                           PCV TYPE / REFANT\n"
"                                                            END OF HEADER\n"
"                                                            START OF ANTENNA\n"
"BLOCK IIA           G01                                     TYPE / SERIAL NO\n"
"   0.0                                                      DAZI\n"
"   0.0  10.0   5.0                                          ZEN1 / ZEN2 / DZEN\n"
"     1                                                      # OF FREQUENCIES\n"
"   G01                                                      START OF FREQUENCY\n"
"      1.00      2.00    880.00                              NORTH / EAST / UP\n"
"   NOAZI    1.20    0.90    0.10\n"
"   G01                                                      END OF FREQUENCY\n"
"                                                            END OF ANTENNA\n";

}  // namespace

TEST_CASE("IOFM-A-012  read_antex on a column-verified fixture recovers the "
          "NOAZI row and the header/antenna/frequency fields",
          "[io][antex]") {
    auto f = read_antex(kFixture);
    REQUIRE(f.has_value());
    CHECK_THAT(f->header.version, WithinAbs(1.4, 1e-9));
    CHECK(f->header.satellite_system == 'G');
    CHECK(f->header.pcv_type == AntexPcvType::Absolute);

    REQUIRE(f->antennas.size() == 1);
    const auto& ant = f->antennas[0];
    CHECK(ant.antenna_type == "BLOCK IIA");
    CHECK(ant.serial_or_sat_code == "G01");
    CHECK_THAT(ant.dazi_deg, WithinAbs(0.0, 1e-9));
    CHECK_THAT(ant.zen1_deg, WithinAbs(0.0, 1e-9));
    CHECK_THAT(ant.zen2_deg, WithinAbs(10.0, 1e-9));
    CHECK_THAT(ant.dzen_deg, WithinAbs(5.0, 1e-9));
    CHECK(ant.num_frequencies == 1);

    REQUIRE(ant.frequencies.size() == 1);
    const auto& f0 = ant.frequencies[0];
    CHECK(f0.code == "G01");
    CHECK_THAT(f0.north_mm, WithinAbs(1.0, 1e-6));
    CHECK_THAT(f0.east_mm, WithinAbs(2.0, 1e-6));
    CHECK_THAT(f0.up_mm, WithinAbs(880.0, 1e-6));
    REQUIRE(f0.noazi_mm.size() == 3);
    CHECK_THAT(f0.noazi_mm[0], WithinAbs(1.20, 1e-6));
    CHECK_THAT(f0.noazi_mm[1], WithinAbs(0.90, 1e-6));
    CHECK_THAT(f0.noazi_mm[2], WithinAbs(0.10, 1e-6));
    CHECK(f0.azimuth_grid_mm.empty());
}

TEST_CASE("IOFM-A-008b  an unrecognised ANTEX PCV TYPE refuses IOFM-F-008",
          "[io][antex]") {
    std::string bad = kFixture;
    auto pos = bad.find("A                                                           PCV TYPE / REFANT");
    REQUIRE(pos != std::string::npos);
    bad[pos] = 'Z';
    auto f = read_antex(bad);
    REQUIRE_FALSE(f.has_value());
    CHECK(f.error().id == "IOFM-F-008");
}

TEST_CASE("IOFM-A-013h  ANTEX round-trips: read(write(read(fixture))) == read(fixture)",
          "[io][antex][gate]") {
    auto f1 = read_antex(kFixture);
    REQUIRE(f1.has_value());
    auto text2 = write_antex(*f1);
    REQUIRE(text2.has_value());
    auto f2 = read_antex(*text2);
    REQUIRE(f2.has_value());
    CHECK(*f1 == *f2);
}
