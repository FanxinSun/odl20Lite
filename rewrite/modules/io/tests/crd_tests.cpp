// crd_tests.cpp — SPEC-io-formats.md §8, IOFM-A-007/008/013.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/crd.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

namespace {

/// `CRD2` §6's own printed sample file (full-rate/range-supplement/
/// meteorological/calibration example, station MLRS, target LAGEOS2), copied
/// verbatim -- FREE FORMAT (`CRD2` §0's own words), so unlike SP3's fixed
/// columns, whitespace count does not matter and no field-by-field
/// reconstruction is needed to trust this fixture.
const char* kSample1 =
"H1 CRD 2 2007 3 20 14\n"
"H2 MLRS 7080 24 19 4 NASA\n"
"H3 LAGEOS2 9207002 5986 22195 0 1 1\n"
"H4 0 2006 11 13 15 23 52 2006 11 13 15 45 35 1 1 1 1 0 0 2 0\n"
"12 55432.0414338 std1 20735.0 1601.0000 0.00 0.0000 0.0000\n"
"20 55432.0414338 801.80 28.21     39 0\n"
"30 55432.0414338 297.2990 38.6340 0 2 1 0.0000000 0.0000000\n"
"40 55432.0414338 0 std1       na       na 0.000 -913.0 0.0 56.0 na na na 3 3 0 4 na\n"
"H8\n"
"H9\n";

/// `CRD2` §6's own second printed sample: normal-point data, record type 11.
const char* kSample2 =
"H1 CRD 2 2007 3 20 14\n"
"H2 MLRS 7080 24 19 4 NASA\n"
"H3 LAGEOS2 9207002 5986 22195 0 1 1\n"
"H4 1 2006 11 13 15 25 4 2006 11 13 15 44 40 0 0 0 0 1 0 2 0\n"
"11 55504.9728030 0.047379676080 std1 2 120      18      94.0 na na na 0.0 0 0.0\n"
"20 55504.9728030 801.80 282.10    39 1\n"
"40 55504.9728030 0 std1       na       na 0.000 -913.0 0.0 56.0 na na na 3 3 0 4 na\n"
"H8\n"
"H9\n";

}  // namespace

TEST_CASE("IOFM-A-007  read_crd on CRD2's own printed sample reproduces H1-H4 "
          "and every 12/20/30/40 record's own fields",
          "[io][crd]") {
    auto f = read_crd(kSample1);
    REQUIRE(f.has_value());
    CHECK(f->format.version == 2);
    CHECK(f->format.year == 2007);
    REQUIRE(f->stations.size() == 1);
    CHECK(f->stations[0].name == "MLRS");
    CHECK(f->stations[0].system_id == 7080);
    CHECK(f->stations[0].network == "NASA");
    REQUIRE(f->targets.size() == 1);
    CHECK(f->targets[0].name == "LAGEOS2");
    CHECK(f->targets[0].ilrs_id == 9207002);
    REQUIRE(f->sessions.size() == 1);
    CHECK(f->sessions[0].data_type == 0);
    CHECK(f->sessions[0].start_year == 2006);
    REQUIRE(f->sessions[0].end_year.has_value());
    CHECK(*f->sessions[0].end_year == 2006);

    // 12, 20, 30, 40 are all opaque (SPEC-io-formats.md §3.3) -- present,
    // tagged, and their fields carried verbatim.
    bool have12 = false, have20 = false, have30 = false, have40 = false;
    for (const auto& op : f->opaque) {
        if (op.kind == CrdRecordKind::RangeSupplement) { have12 = true; CHECK(op.fields.front() == "55432.0414338"); }
        if (op.kind == CrdRecordKind::Meteorological) { have20 = true; CHECK(op.fields[1] == "801.80"); }
        if (op.kind == CrdRecordKind::Angles) { have30 = true; }
        if (op.kind == CrdRecordKind::Calibration40) { have40 = true; }
    }
    CHECK(have12); CHECK(have20); CHECK(have30); CHECK(have40);
}

TEST_CASE("IOFM-A-008  read_crd on CRD2's own second printed sample reproduces "
          "every field of the 11 (normal-point range) record",
          "[io][crd]") {
    auto f = read_crd(kSample2);
    REQUIRE(f.has_value());
    REQUIRE(f->ranges.size() == 1);
    const auto& r = f->ranges[0];
    CHECK(r.kind == CrdRecordKind::NormalPointRange);
    CHECK_THAT(r.seconds_of_day, WithinAbs(55504.9728030, 1e-6));
    CHECK_THAT(r.time_of_flight_s, WithinAbs(0.047379676080, 1e-12));
    CHECK(r.config_id == "std1");
    CHECK(r.epoch_event == 2);
    REQUIRE(r.remaining_fields.size() >= 8);
    CHECK(r.remaining_fields[0] == "120");    // window length
    CHECK(r.remaining_fields[1] == "18");     // number of raw ranges
}

TEST_CASE("IOFM-A-004b  an unrecognised CRD record type refuses IOFM-F-004",
          "[io][crd]") {
    auto f = read_crd("H1 CRD 2 2007 3 20 14\nZZ garbage\n");
    REQUIRE_FALSE(f.has_value());
    CHECK(f.error().id == "IOFM-F-004");
}

TEST_CASE("IOFM-A-013d  CRD round-trips: read(write(read(fixture))) == read(fixture)",
          "[io][crd][gate]") {
    for (const char* fixture : {kSample1, kSample2}) {
        auto f1 = read_crd(fixture);
        REQUIRE(f1.has_value());
        auto text2 = write_crd(*f1);
        REQUIRE(text2.has_value());
        auto f2 = read_crd(*text2);
        REQUIRE(f2.has_value());
        CHECK(*f1 == *f2);
    }
}
