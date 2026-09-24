// horizons_tests.cpp — SPEC-io-horizons.md §8, IOHZ-A-001 through IOHZ-A-007.
//
// Every fixture is HAND-BUILT at HZAPI's own documented positions/tokens,
// verified against a real query's own output (SPEC-io-horizons.md §2) but
// not itself a copy of that real response -- its own numeric values are
// deliberately distinct placeholders, not the real captured ephemeris,
// since the real response is not committed pending the manager's own
// licence ruling (§9/§10 IOHZ-Q-002).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/horizons.hpp>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

namespace {

/// A minimal, structurally faithful two-record vector-table response --
/// the same header lines, sentinel markers and three-line-per-record shape
/// SPEC-io-horizons.md §2/§3 verified against a real query, with
/// deliberately round, clearly-synthetic numeric values.
const char* kFixture =
R"hz(API VERSION: 1.2
API SOURCE: NASA/JPL Horizons API

*******************************************************************************
Ephemeris / API_USER Thu Jan  1 00:00:00 2026 Pasadena, USA      / Horizons
*******************************************************************************
Target body name: TESTSAT (spacecraft) (-999999)  {source: TESTSAT}
Center body name: Earth (399)                     {source: DE441}
Center-site name: BODY CENTER
*******************************************************************************
Start time      : A.D. 2026-Jan-01 00:00:00.0000 TDB
Stop  time      : A.D. 2026-Jan-01 01:00:00.0000 TDB
Step-size       : 60 minutes
*******************************************************************************
Output units    : KM-S
Calendar mode   : Mixed Julian/Gregorian
Output type     : GEOMETRIC cartesian states
Reference frame : ICRF
*******************************************************************************
JDTDB
   X     Y     Z
   VX    VY    VZ
*******************************************************************************
$$SOE
2461041.500000000 = A.D. 2026-Jan-01 00:00:00.0000 TDB
 X = 1000.000000000000E+00 Y =-2000.500000000000E+00 Z = 3000.250000000000E+00
 VX=-1.500000000000E+00 VY= 2.250000000000E+00 VZ=-3.125000000000E+00
2461041.541666667 = A.D. 2026-Jan-01 01:00:00.0000 TDB
 X =-4000.125000000000E+00 Y = 5000.750000000000E+00 Z =-6000.375000000000E+00
 VX= 4.500000000000E+00 VY=-5.250000000000E+00 VZ= 6.125000000000E+00
$$EOE
*******************************************************************************

TIME

  Barycentric Dynamical Time ("TDB" or T_eph) output was requested.
)hz";

std::string replace_once(std::string s, const std::string& from, const std::string& to) {
    auto pos = s.find(from);
    REQUIRE(pos != std::string::npos);
    return s.substr(0, pos) + to + s.substr(pos + from.size());
}

}  // namespace

TEST_CASE("IOHZ-A-001  read_horizons on a fixture built at HZAPI's own documented shape "
          "reproduces target_body, center_body and every field of both records exactly",
          "[io][horizons]") {
    auto eph = read_horizons(kFixture);
    REQUIRE(eph.has_value());
    // target_body/center_body are carried VERBATIM (SPEC-io-horizons.md §5), including the
    // trailing {source: ...} annotation the real format prints -- not stripped further.
    CHECK(eph->target_body == "TESTSAT (spacecraft) (-999999)  {source: TESTSAT}");
    CHECK(eph->center_body == "Earth (399)                     {source: DE441}");
    REQUIRE(eph->states.size() == 2);

    const auto& r0 = eph->states[0];
    CHECK(r0.epoch.year == 2026);
    CHECK(r0.epoch.month == 1);
    CHECK(r0.epoch.day == 1);
    CHECK(r0.epoch.hour == 0);
    CHECK(r0.epoch.minute == 0);
    CHECK_THAT(r0.epoch.second, WithinAbs(0.0, 1e-9));
    CHECK(r0.time_system == HorizonsTimeSystem::Tdb);
    CHECK_THAT(r0.position_km.x, WithinAbs(1000.0, 1e-9));
    CHECK_THAT(r0.position_km.y, WithinAbs(-2000.5, 1e-9));
    CHECK_THAT(r0.position_km.z, WithinAbs(3000.25, 1e-9));
    CHECK_THAT(r0.velocity_km_s.x, WithinAbs(-1.5, 1e-9));
    CHECK_THAT(r0.velocity_km_s.y, WithinAbs(2.25, 1e-9));
    CHECK_THAT(r0.velocity_km_s.z, WithinAbs(-3.125, 1e-9));

    const auto& r1 = eph->states[1];
    CHECK(r1.epoch.hour == 1);
    CHECK_THAT(r1.position_km.x, WithinAbs(-4000.125, 1e-9));
    CHECK_THAT(r1.position_km.y, WithinAbs(5000.75, 1e-9));
    CHECK_THAT(r1.position_km.z, WithinAbs(-6000.375, 1e-9));
    CHECK_THAT(r1.velocity_km_s.x, WithinAbs(4.5, 1e-9));
    CHECK_THAT(r1.velocity_km_s.y, WithinAbs(-5.25, 1e-9));
    CHECK_THAT(r1.velocity_km_s.z, WithinAbs(6.125, 1e-9));
}

TEST_CASE("IOHZ-A-002  a record whose own time-system token reads UT instead of TDB "
          "refuses IOHZ-F-002, naming the token found",
          "[io][horizons]") {
    std::string bad = replace_once(kFixture, "2461041.500000000 = A.D. 2026-Jan-01 00:00:00.0000 TDB",
                                              "2461041.500000000 = A.D. 2026-Jan-01 00:00:00.0000 UT ");
    auto eph = read_horizons(bad);
    REQUIRE_FALSE(eph.has_value());
    CHECK(eph.error().id == "IOHZ-F-002");
}

TEST_CASE("IOHZ-A-003  a header whose Output units line reads AU-D instead of KM-S "
          "refuses IOHZ-F-003",
          "[io][horizons]") {
    std::string bad = replace_once(kFixture, "Output units    : KM-S", "Output units    : AU-D");
    auto eph = read_horizons(bad);
    REQUIRE_FALSE(eph.has_value());
    CHECK(eph.error().id == "IOHZ-F-003");
}

TEST_CASE("IOHZ-A-004  a header whose Reference frame line reads B1950 instead of ICRF "
          "refuses IOHZ-F-004",
          "[io][horizons]") {
    std::string bad = replace_once(kFixture, "Reference frame : ICRF", "Reference frame : B1950");
    auto eph = read_horizons(bad);
    REQUIRE_FALSE(eph.has_value());
    CHECK(eph.error().id == "IOHZ-F-004");
}

TEST_CASE("IOHZ-A-005  a header whose Output type line reads ASTROMETRIC cartesian "
          "states (a VEC_CORR=LT response) instead of GEOMETRIC refuses IOHZ-F-005",
          "[io][horizons]") {
    std::string bad = replace_once(kFixture, "Output type     : GEOMETRIC cartesian states",
                                              "Output type     : ASTROMETRIC cartesian states");
    auto eph = read_horizons(bad);
    REQUIRE_FALSE(eph.has_value());
    CHECK(eph.error().id == "IOHZ-F-005");
}

TEST_CASE("IOHZ-A-006  a response missing its own $$EOE line entirely refuses IOHZ-F-001; "
          "the substantial header/footer prose every real response carries around $$SOE/"
          "$$EOE is never itself mistaken for a malformed record (IOHZ-A-001 already shows "
          "the SAME fixture's own prose parsing cleanly when $$EOE IS present)",
          "[io][horizons]") {
    std::string bad = replace_once(kFixture, "$$EOE\n", "");
    auto eph = read_horizons(bad);
    REQUIRE_FALSE(eph.has_value());
    CHECK(eph.error().id == "IOHZ-F-001");
}

TEST_CASE("IOHZ-A-007  to_time_scale(Tdb) succeeds with TimeScale::TDB; "
          "to_time_scale(Ut) refuses IOHZ-F-002",
          "[io][horizons]") {
    auto tdb = to_time_scale(HorizonsTimeSystem::Tdb);
    REQUIRE(tdb.has_value());
    CHECK(*tdb == time::TimeScale::TDB);

    auto ut = to_time_scale(HorizonsTimeSystem::Ut);
    REQUIRE_FALSE(ut.has_value());
    CHECK(ut.error().id == "IOHZ-F-002");
}
