// cpf_tests.cpp — SPEC-io-formats.md §8, IOFM-A-009/013.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/cpf.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

namespace {

/// `CPF2`'s own printed sample (target gps35, source AIU), copied verbatim --
/// free format, whitespace-tokenised, the same "no reconstruction needed"
/// simplicity `crd_tests.cpp`'s own fixtures rely on.
const char* kSample =
"H1 CPF 2 AIU 2005 11 16 4 320 1 gps35\n"
"H2 9305401 3535 22779 2005 11 15 23 59 47 2005 11 20 23 29 47 900 1 1 0 0 0 1\n"
"H9\n"
"10 0 53689 86387.000000 0 -13785362.868 -12150743.695 19043830.747\n"
"10 0 53690    887.000000 0 -13656536.158 -14288496.731 17628980.237\n";

}  // namespace

TEST_CASE("IOFM-A-009  read_cpf on CPF2's own printed sample header and "
          "position records reproduces every field",
          "[io][cpf]") {
    auto f = read_cpf(kSample);
    REQUIRE(f.has_value());
    CHECK(f->header1.version == 2);
    CHECK(f->header1.source == "AIU");
    CHECK(f->header1.prod_year == 2005);
    CHECK(f->header1.sequence_number == 320);
    CHECK(f->header1.target_name == "gps35");
    CHECK(f->header1.notes.empty());

    CHECK(f->header2.ilrs_id == 9305401);
    CHECK(f->header2.sic == 3535);
    CHECK(f->header2.norad_id == 22779);
    CHECK(f->header2.start_year == 2005);
    CHECK(f->header2.table_step_s == 900);
    CHECK(f->header2.target_location == 1);

    REQUIRE(f->positions.size() == 2);
    CHECK(f->positions[0].direction == CpfDirectionFlag::Common);
    CHECK(f->positions[0].mjd == 53689);
    CHECK_THAT(f->positions[0].seconds_of_day, WithinAbs(86387.0, 1e-6));
    CHECK_THAT(f->positions[0].x_m, WithinAbs(-13785362.868, 1e-3));
    CHECK_THAT(f->positions[1].z_m, WithinAbs(17628980.237, 1e-3));
}

TEST_CASE("IOFM-A-004c  an unrecognised CPF record type refuses IOFM-F-004",
          "[io][cpf]") {
    auto f = read_cpf("H1 CPF 2 AIU 2005 11 16 4 320 1 gps35\n"
                       "H2 9305401 3535 22779 2005 11 15 23 59 47 2005 11 20 23 29 47 900 1 1 0 0 0 1\n"
                       "H9\nZZ garbage\n");
    REQUIRE_FALSE(f.has_value());
    CHECK(f.error().id == "IOFM-F-004");
}

TEST_CASE("IOFM-A-013e  CPF round-trips: read(write(read(fixture))) == read(fixture)",
          "[io][cpf][gate]") {
    auto f1 = read_cpf(kSample);
    REQUIRE(f1.has_value());
    auto text2 = write_cpf(*f1);
    REQUIRE(text2.has_value());
    auto f2 = read_cpf(*text2);
    REQUIRE(f2.has_value());
    CHECK(*f1 == *f2);
}
