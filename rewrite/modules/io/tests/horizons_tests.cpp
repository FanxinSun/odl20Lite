// horizons_tests.cpp — SPEC-io-horizons.md §8, IOHZ-A-001 through IOHZ-A-008.
//
// IOHZ-A-001 through IOHZ-A-007 use fixtures HAND-BUILT at HZAPI's own
// documented positions/tokens, verified against a real query's own output
// (SPEC-io-horizons.md §2) but not themselves a copy of that real response
// -- their own numeric values are deliberately distinct placeholders.
// IOHZ-A-008 reads the real thing: the manifest's own pinned ACS3 capture
// (FACTUAL-DATA-CITED, ruled 2026-09-25, PROVENANCE.md §37.4).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/io/horizons.hpp>

#include <fstream>
#include <limits>
#include <sstream>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::ContainsSubstring;
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

TEST_CASE("IOHZ-A-008  read_horizons on the manifest's own real, pinned ACS3 capture "
          "(FACTUAL-DATA-CITED, PROVENANCE.md §37.4) parses cleanly: five real records, "
          "a real target/center body name, and every position within a plausible LEO range",
          "[io][horizons][real-data]") {
    std::ifstream in(ODL_HORIZONS_ACS3_TXT);
    REQUIRE(in.is_open());
    std::stringstream ss;
    ss << in.rdbuf();

    auto eph = read_horizons(ss.str());
    REQUIRE(eph.has_value());
    CHECK(eph->target_body.substr(0, 4) == "ACS3");
    CHECK(eph->center_body.substr(0, 5) == "Earth");
    REQUIRE(eph->states.size() == 5);

    for (const auto& r : eph->states) {
        CHECK(r.time_system == HorizonsTimeSystem::Tdb);
        CHECK(r.epoch.year == 2026);
        CHECK(r.epoch.month == 9);
        CHECK(r.epoch.day == 25);
        // ACS3's own real orbit: ~994x1023 km altitude, a LEO radius comfortably
        // inside [6871, 8000] km and a speed comfortably inside [6, 8] km/s --
        // not a re-derivation of the real value (this file's own numbers are
        // whatever Horizons actually returned, not asserted here field by
        // field), a plausibility bound on real data, the same spirit
        // IOFM-A-030/031's own held-out checks use for a different format.
        const double r_km = r.position_km.norm();
        CHECK(r_km > 6871.0);
        CHECK(r_km < 8000.0);
        const double v_km_s = r.velocity_km_s.norm();
        CHECK(v_km_s > 6.0);
        CHECK(v_km_s < 8.0);
    }
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

// --- IOHZ-A-009, IOHZ-A-010 (v1.1): the writer, which makes the layer's exit gate "every format round-trips" literal for this format too ------------------------------------------

TEST_CASE("IOHZ-A-009  write_horizons round-trips (v1.1): read(write(read(x))) == read(x) on the real pinned ACS3 capture, and on a hand-built table of awkward values (negative and "
          "tiny components, a seconds field with nine decimals, an empty table); the text is stable under a second pass",
          "[io][horizons]") {
    std::ifstream in(ODL_HORIZONS_ACS3_TXT);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    auto real = read_horizons(ss.str());
    REQUIRE(real.has_value());
    REQUIRE(real->states.size() == 5);
    auto text = write_horizons(*real);
    REQUIRE(text.has_value());
    auto back = read_horizons(*text);
    REQUIRE(back.has_value());
    CHECK(*back == *real);
    auto again = write_horizons(*back);
    REQUIRE(again.has_value());
    CHECK(*again == *text);                                       // the writer is a function of the structure alone

    HorizonsEphemeris h;
    h.target_body = "Test body (-999)";
    h.center_body = "Earth (399)";
    HorizonsStateRecord a;
    a.epoch = time::Calendar{2026, 9, 25, 0, 0, 0.0};
    a.position_km = Vec3{7049.479204680989, -1823.447981968663, 760.5020081570782};
    a.velocity_km_s = Vec3{0.4906605773465501, -1.093395175266581, -7.262466176454558};
    HorizonsStateRecord b = a;
    b.epoch = time::Calendar{1999, 12, 31, 23, 59, 59.123456789};
    b.position_km = Vec3{-1.0e-300, 2.5e+300, -0.0};
    b.velocity_km_s = Vec3{1.0 / 3.0, -2.0 / 3.0, 123456789.12345679};
    HorizonsStateRecord c = a;
    c.epoch = time::Calendar{2026, 1, 1, 3, 4, 5.5};
    h.states = {a, b, c};
    auto ht = write_horizons(h);
    REQUIRE(ht.has_value());
    auto hb = read_horizons(*ht);
    REQUIRE(hb.has_value());
    CHECK(*hb == h);                                              // every position and velocity to the last bit, the seconds to the last bit
    CHECK_THAT(*ht, ContainsSubstring("$$SOE"));
    CHECK_THAT(*ht, ContainsSubstring("1999-Dec-31 23:59:59.123456789 TDB"));

    HorizonsEphemeris empty;
    empty.target_body = "Nothing";
    empty.center_body = "Nowhere";
    auto et = write_horizons(empty);
    REQUIRE(et.has_value());
    auto eb = read_horizons(*et);
    REQUIRE(eb.has_value());
    CHECK(*eb == empty);
}

TEST_CASE("IOHZ-A-010  write_horizons refuses what the reader could not read back (v1.1): a record in a time system other than TDB refuses IOHZ-F-002; a component that is not finite, a month "
          "outside 1 .. 12 and a body name that holds a line break refuse IOHZ-F-006",
          "[io][horizons]") {
    HorizonsEphemeris h;
    h.target_body = "T";
    h.center_body = "C";
    HorizonsStateRecord r;
    r.epoch = time::Calendar{2026, 9, 25, 0, 0, 0.0};
    r.position_km = Vec3{1.0, 2.0, 3.0};
    r.velocity_km_s = Vec3{0.1, 0.2, 0.3};
    h.states = {r};
    REQUIRE(write_horizons(h).has_value());

    HorizonsEphemeris ut = h;
    ut.states.front().time_system = HorizonsTimeSystem::Ut;
    auto a = write_horizons(ut);
    REQUIRE_FALSE(a.has_value());
    CHECK(a.error().id == "IOHZ-F-002");

    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        HorizonsEphemeris nf = h;
        nf.states.front().velocity_km_s.z = bad;
        auto b = write_horizons(nf);
        REQUIRE_FALSE(b.has_value());
        CHECK(b.error().id == "IOHZ-F-006");
    }
    HorizonsEphemeris month = h;
    month.states.front().epoch.month = 13;
    auto m = write_horizons(month);
    REQUIRE_FALSE(m.has_value());
    CHECK(m.error().id == "IOHZ-F-006");

    HorizonsEphemeris line = h;
    line.center_body = "Earth\n$$SOE";
    auto l = write_horizons(line);
    REQUIRE_FALSE(l.has_value());
    CHECK(l.error().id == "IOHZ-F-006");
}

