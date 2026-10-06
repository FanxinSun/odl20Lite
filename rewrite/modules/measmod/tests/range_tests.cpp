// range_tests.cpp — SPEC-measmod.md §8.2 and §8.4: MEAS-A-012, -014 … -017 (the CRD normal point as an observation), -034 … -041 (the range model
// and what it applied), -050 … -053 (the interface). The finite-difference gate (-042 … -045) is gate_tests.cpp.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/measmod/range.hpp>
#include <odl/measmod/shapiro.hpp>

#include "measmod_reference.hpp"
#include "scenario.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <string>
#include <type_traits>
#include <vector>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using odl::Vec3;
using odl::time::Calendar;
using odl::time::Epoch;
using odl::time::TimeScale;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double c = kSpeedOfLight_m_s;

struct Np { double sod; double tof; int event = 2; const char* config = "new"; };

/// A one-pass CRD text for Yarragadee (SOD 70900513) observing LAGEOS-1, with one meteorology record per normal point.
std::string pass_text(int y, int mo, int d, int h, int mi, int s, const std::vector<Np>& nps, const char* h4_tail = "0 0 0 0 1 0 2 0", const char* c0 = "C0 0 532.000 new la1 mcp ti1 swm met cac",
                      const char* station_scale = "3") {
    std::string t = "H1 CRD 2 " + std::to_string(y) + " " + std::to_string(mo) + " " + std::to_string(d) + " " + std::to_string(h) + "\n";
    t += std::string("H2 YARL 7090 5 13 ") + station_scale + " ILRS\n";
    t += "H3 lageos1 7603901 1155 8820 0 1 1\n";
    t += "H4 1 " + std::to_string(y) + " " + std::to_string(mo) + " " + std::to_string(d) + " " + std::to_string(h) + " " + std::to_string(mi) + " " + std::to_string(s) +
         " na na na na na na " + h4_tail + "\n";
    t += std::string(c0) + "\n";
    char b[200];
    for (const Np& np : nps) {
        std::snprintf(b, sizeof b, "20 %.3f 976.70 310.30 12. 0\n", np.sod);
        t += b;
        std::snprintf(b, sizeof b, "11 %.12f %.12f %s %d 120.0 10 30.0 0.000 0.000 na 1.0 0 na\n", np.sod, np.tof, np.config, np.event);
        t += b;
    }
    t += "H8\nH9\n";
    return t;
}

odl::io::CrdPass one_pass(const std::string& text) {
    auto p = odl::io::read_crd_passes(text);
    if (!p) FAIL("test pass did not read: " << p.error().id << " " << p.error().message);
    REQUIRE(p->size() == 1);
    return p->front();
}

odl::Result<std::vector<RangeObservation>, MeasError> build(const odl::io::CrdPass& p, const SphericalCentreOfMass& com = lageos1_com()) {
    return range_observations(p, real_registry(), leaps(), com);
}

Calendar cal(const Epoch& e, TimeScale s = TimeScale::UTC) {
    auto r = e.calendar(s, leaps(), 9);
    REQUIRE(r.has_value());
    return *r;
}

}  // namespace

// ========================================================================================================================
TEST_CASE("MEAS-A-012  the target identity: a LAGEOS-1 pass with a LAGEOS-1 centre-of-mass declaration builds, with a LAGEOS-2 declaration it "
          "refuses MEAS-F-007 naming both identifiers; the reader's refusal of an H2 time-scale code is carried with its own id",
          "[measmod][range][builder]") {
    const odl::io::CrdPass& real = real_passes().front();
    CHECK(build(real, lageos1_com()).has_value());
    auto lageos2 = SphericalCentreOfMass::make(9207002, 0.251, "a LAGEOS-2 declaration");
    REQUIRE(lageos2.has_value());
    auto wrong = build(real, *lageos2);
    REQUIRE_FALSE(wrong.has_value());
    CHECK(wrong.error().id == "MEAS-F-007");
    CHECK_THAT(wrong.error().message, ContainsSubstring("7603901"));
    CHECK_THAT(wrong.error().message, ContainsSubstring("9207002"));
    // the reader resolves the time scale at parse time (IOFM-R-010): a pass whose H2 code is 5 never reaches the builder, and the id is the reader's own
    auto bad = odl::io::read_crd_passes(pass_text(2026, 1, 1, 2, 7, 49, {{7676.8005871, 0.051212898595}}, "0 0 0 0 1 0 2 0", "C0 0 532.000 new la1", "5"));
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "IOFM-F-015");
    for (const char* ok : {"3", "4", "7"}) {
        auto p = odl::io::read_crd_passes(pass_text(2026, 1, 1, 2, 7, 49, {{7676.8005871, 0.051212898595}}, "0 0 0 0 1 0 2 0", "C0 0 532.000 new la1", ok));
        REQUIRE(p.has_value());
        CHECK(p->front().time_scale == TimeScale::UTC);
        CHECK(build(p->front()).has_value());
    }
}

TEST_CASE("MEAS-A-014  a CRD record's epoch is the H4 start date plus the seconds of day: the real first normal point is 2026-01-01 02:07:56.8005871 UTC "
          "= 02:08:33.8005871 TAI to 1 ns; a pass with no normal point refuses MEAS-F-023",
          "[measmod][range][builder][real]") {
    const RangeObservation& obs = first_real_observation();
    const Calendar u = cal(obs.epoch, TimeScale::UTC);
    CHECK((u.year == 2026 && u.month == 1 && u.day == 1 && u.hour == 2 && u.minute == 7));
    CHECK_THAT(u.second, WithinAbs(56.8005871, 1e-9));
    const Calendar t = cal(obs.epoch, TimeScale::TAI);
    CHECK((t.year == 2026 && t.month == 1 && t.day == 1 && t.hour == 2 && t.minute == 8));
    CHECK_THAT(t.second, WithinAbs(33.8005871, 1e-9));                              // TAI - UTC = 37 s
    CHECK(obs.event == EpochEvent::GroundTransmit);                                  // the real file tags the transmit time
    CHECK(obs.time_of_flight_s == 0.051212898595);
    CHECK(obs.wavelength_nm == 532.0);
    CHECK(obs.site.sod == SodKey{7090, 5, 13});
    CHECK(obs.com.ilrs_satellite_id() == 7603901);
    // every normal point of the pass becomes an observation, in file order, with the site resolved at its own epoch
    auto all = build(real_passes().front());
    REQUIRE(all.has_value());
    CHECK(all->size() == real_passes().front().ranges.size());
    for (std::size_t i = 1; i < all->size(); ++i) CHECK((*all)[i - 1].epoch <= (*all)[i].epoch);
    // no normal point at all
    odl::io::CrdPass empty = real_passes().front();
    empty.ranges.clear();
    auto none = build(empty);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().id == "MEAS-F-023");
}

TEST_CASE("MEAS-A-015  a pass crossing 00:00 UTC: the epochs run forward, the last two are on the next day, and the wrong rollover (every record on the "
          "start's date) is shown failing the same monotonicity assertion",
          "[measmod][range][builder]") {
    const std::vector<Np> nps = {{86395.2, 0.0512}, {86399.9, 0.0513}, {0.3, 0.0514}, {4.7, 0.0515}};
    auto obs = build(one_pass(pass_text(2026, 3, 14, 23, 59, 40, nps)));
    REQUIRE(obs.has_value());
    REQUIRE(obs->size() == 4);
    auto monotone = [](const std::vector<Epoch>& e) {
        for (std::size_t i = 1; i < e.size(); ++i)
            if (e[i] < e[i - 1]) return false;
        return true;
    };
    std::vector<Epoch> got;
    for (const auto& o : *obs) got.push_back(o.epoch);
    CHECK(monotone(got));
    CHECK((cal(got[0]).day == 14 && cal(got[1]).day == 14));
    CHECK((cal(got[2]).day == 15 && cal(got[3]).day == 15));
    CHECK_THAT(cal(got[0]).second, WithinAbs(55.2, 1e-8));
    CHECK_THAT(cal(got[2]).second, WithinAbs(0.3, 1e-8));
    CHECK_THAT(got[3].difference(got[0]).to_seconds(), WithinAbs(9.5, 1e-8));      // 55.2 -> 4.7 across midnight: 9.5 s
    // THE WRONG ROLLOVER: every record on the H4 start's date — the last two land a day too early and the assertion fails for them
    std::vector<Epoch> naive;
    const Epoch midnight = utc(2026, 3, 14, 0, 0, 0.0);
    for (const Np& np : nps) naive.push_back(midnight.add(odl::time::Duration::from_seconds(np.sod)));
    CHECK_FALSE(monotone(naive));
    CHECK(got[3].difference(naive[3]).to_seconds() > 86399.0);                      // the naive epoch is a whole day early
}

TEST_CASE("MEAS-A-016  leap seconds: 86400.5 s of day on 2016-12-31 is 23:59:60.5 UTC; the same value on 2026-01-01 refuses with the time module's own id; "
          "86401 s and negative seconds refuse MEAS-F-021",
          "[measmod][range][builder]") {
    auto ok = build(one_pass(pass_text(2016, 12, 31, 23, 59, 50, {{86399.9, 0.0512}, {86400.5, 0.0513}})));
    REQUIRE(ok.has_value());
    const Calendar leap = cal((*ok)[1].epoch);
    CHECK((leap.year == 2016 && leap.month == 12 && leap.day == 31 && leap.hour == 23 && leap.minute == 59));
    CHECK_THAT(leap.second, WithinAbs(60.5, 1e-8));
    CHECK_THAT((*ok)[1].epoch.difference((*ok)[0].epoch).to_seconds(), WithinAbs(0.6, 1e-8));   // 23:59:59.9 -> 23:59:60.5: 0.6 s of SI time
    // the same value on a day with no leap second: the calendar conversion's own refusal (a TIME-F id), carried
    auto direct = Epoch::from_calendar(TimeScale::UTC, Calendar{2026, 1, 1, 23, 59, 60.5}, leaps());
    REQUIRE_FALSE(direct.has_value());
    auto no_leap = build(one_pass(pass_text(2026, 1, 1, 23, 59, 50, {{86399.9, 0.0512}, {86400.5, 0.0513}})));
    REQUIRE_FALSE(no_leap.has_value());
    CHECK(no_leap.error().id == direct.error().id);
    CHECK_THAT(no_leap.error().message, ContainsSubstring("86400.5"));
    // out of range: 86401 and negative
    for (double bad : {86401.0, 86500.0, -0.5}) {
        auto r = build(one_pass(pass_text(2026, 1, 1, 23, 59, 50, {{86399.9, 0.0512}, {bad, 0.0513}})));
        INFO("seconds of day " << bad);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-021");
    }
}

TEST_CASE("MEAS-A-017  a pass whose epochs run backwards after the rollover rule refuses MEAS-F-019 naming the record; the in-order pass does not",
          "[measmod][range][builder]") {
    auto back = build(one_pass(pass_text(2026, 1, 1, 0, 0, 0, {{100.0, 0.0512}, {50.0, 0.0513}})));       // start 00:00:00: 50 s is not 'tomorrow', it is earlier
    REQUIRE_FALSE(back.has_value());
    CHECK(back.error().id == "MEAS-F-019");
    CHECK_THAT(back.error().message, ContainsSubstring("normal point 2 of 2"));
    CHECK(build(one_pass(pass_text(2026, 1, 1, 0, 0, 0, {{50.0, 0.0512}, {100.0, 0.0513}}))).has_value());
    // equal epochs are allowed (a pass may hold two records at one instant), a day's worth later is the largest a pass can span
    CHECK(build(one_pass(pass_text(2026, 1, 1, 0, 0, 0, {{50.0, 0.0512}, {50.0, 0.0513}}))).has_value());
}

TEST_CASE("MEAS-A-034  the CRD session checks, cut from the real file's H4 (0 0 0 0 1 0 2 0): a one-way range type, applied refraction or centre of mass, "
          "an unapplied system delay and the wrong epoch events and data type each refuse naming the field and the value; the real header passes",
          "[measmod][range][builder]") {
    const odl::io::CrdPass real = real_passes().front();
    REQUIRE(build(real).has_value());
    auto expect = [](const odl::io::CrdPass& p, const char* id, const char* field, const char* what) {
        INFO(what);
        auto r = build(p);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == std::string_view(id));
        CHECK_THAT(r.error().message, ContainsSubstring(field));
    };
    { auto p = real; p.session.remaining_fields[2] = "1"; expect(p, "MEAS-F-008", "field 21", "range type 1 (one-way)"); }
    { auto p = real; p.session.remaining_fields[2] = "0"; expect(p, "MEAS-F-008", "field 21", "range type 0 (no ranges)"); }
    { auto p = real; p.session.tropo_applied = true; expect(p, "MEAS-F-009", "field 16", "refraction applied"); }
    { auto p = real; p.session.com_applied = true; expect(p, "MEAS-F-009", "field 17", "centre of mass applied"); }
    { auto p = real; p.session.remaining_fields[0] = "0"; expect(p, "MEAS-F-009", "field 19", "system delay NOT applied"); }
    { auto p = real; p.session.data_type = 0; expect(p, "MEAS-F-008", "data type", "full-rate data"); }
    { auto p = real; p.session.remaining_fields.resize(2); expect(p, "MEAS-F-008", "range-type", "range-type indicator missing"); }
    for (int ev : {0, 3, 4, 5, 6}) {
        auto p = real;
        p.ranges.front().epoch_event = ev;
        expect(p, "MEAS-F-008", "epoch event", "an epoch event other than 1 and 2");
    }
    for (int ev : {1, 2}) {
        auto p = real;
        for (auto& r : p.ranges) r.epoch_event = ev;
        auto r = build(p);
        REQUIRE(r.has_value());
        CHECK(r->front().event == (ev == 1 ? EpochEvent::Bounce : EpochEvent::GroundTransmit));
    }
}

TEST_CASE("MEAS-A-035  the meteorology: linear between the two bracketing records, the nearest record held outside the span with the distance reported, the "
          "real file's first normal point held 0.4129 ms, a pass with no record 20 refuses MEAS-F-012",
          "[measmod][range][builder]") {
    using odl::io::CrdMeteorology;
    const std::vector<CrdMeteorology> recs = {{100.0, 1000.0, 280.0, 50.0, 0}, {200.0, 990.0, 282.0, 60.0, 0}};
    auto mid = interpolate_meteorology(recs, 150.0, 0.0);
    REQUIRE(mid.has_value());
    CHECK((mid->pressure_mbar == 995.0 && mid->temperature_k == 281.0 && mid->relative_humidity_percent == 55.0));
    CHECK_FALSE(mid->held);
    auto q = interpolate_meteorology(recs, 125.0, 0.0);
    REQUIRE(q.has_value());
    CHECK_THAT(q->pressure_mbar, WithinAbs(997.5, 1e-12));
    auto at_first = interpolate_meteorology(recs, 100.0, 0.0);
    REQUIRE(at_first.has_value());
    CHECK((at_first->pressure_mbar == 1000.0 && !at_first->held));                    // exactly at a record: that record, not held
    auto before = interpolate_meteorology(recs, 50.0, 0.0);
    REQUIRE(before.has_value());
    CHECK((before->pressure_mbar == 1000.0 && before->held && before->held_distance_s == 50.0));
    auto after = interpolate_meteorology(recs, 250.0, 0.0);
    REQUIRE(after.has_value());
    CHECK((after->pressure_mbar == 990.0 && after->held && after->held_distance_s == 50.0));
    auto single = interpolate_meteorology({{100.0, 1000.0, 280.0, 50.0, 0}}, 130.0, 0.0);
    REQUIRE(single.has_value());
    CHECK((single->pressure_mbar == 1000.0 && single->held && single->held_distance_s == 30.0));
    // the 00:00 UTC rollover: a pass starting at 86000 s, records at 86100 and 50 s (next day): the query at 25 s is between them, 325 of 350 s of the way
    auto rolled = interpolate_meteorology({{86100.0, 1000.0, 280.0, 50.0, 0}, {50.0, 990.0, 282.0, 60.0, 0}}, 25.0, 86000.0);
    REQUIRE(rolled.has_value());
    CHECK_THAT(rolled->pressure_mbar, WithinAbs(1000.0 - 10.0 * 325.0 / 350.0, 1e-9));
    CHECK_FALSE(rolled->held);
    auto none = interpolate_meteorology({}, 100.0, 0.0);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().id == "MEAS-F-012");

    // the REAL file: the first normal point (7676.8005871 s) is before its first record 20 (7676.801 s): held by 0.4129 ms; the second is bracketed
    auto all = build(real_passes().front());
    REQUIRE(all.has_value());
    CHECK((*all)[0].meteorology.held);
    CHECK_THAT((*all)[0].meteorology.held_distance_s, WithinAbs(0.0004129, 1e-9));
    CHECK((*all)[0].meteorology.pressure_mbar == 976.70);
    CHECK((*all)[0].meteorology.temperature_k == 310.30);
    CHECK((*all)[0].meteorology.relative_humidity_percent == 12.0);
    CHECK_FALSE((*all)[1].meteorology.held);
    // a pass with no record 20 refuses
    odl::io::CrdPass nomet = real_passes().front();
    nomet.meteorology.clear();
    auto r = build(nomet);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().id == "MEAS-F-012");
}

TEST_CASE("MEAS-A-036  the wavelength from C0: the real file's configuration 'new' is 532.000 nm; two C0 records select by the normal point's own configuration id; "
          "an id with no C0 refuses MEAS-F-012",
          "[measmod][range][builder]") {
    CHECK(first_real_observation().wavelength_nm == 532.0);
    const std::string two = pass_text(2026, 1, 1, 2, 7, 49, {{7676.8, 0.0512, 2, "new"}, {7700.0, 0.0513, 2, "old"}}, "0 0 0 0 1 0 2 0",
                                      "C0 0 532.000 new la1\nC0 0 1064.000 old la2");
    auto obs = build(one_pass(two));
    REQUIRE(obs.has_value());
    CHECK((*obs)[0].wavelength_nm == 532.0);
    CHECK((*obs)[1].wavelength_nm == 1064.0);
    auto missing = build(one_pass(pass_text(2026, 1, 1, 2, 7, 49, {{7676.8, 0.0512, 2, "zzz"}})));
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().id == "MEAS-F-012");
    CHECK_THAT(missing.error().message, ContainsSubstring("'zzz'"));
    CHECK_THAT(missing.error().message, ContainsSubstring("'new'"));
}

// ---- the model -----------------------------------------------------------------------------------------------------------------
namespace {
// LAGEOS-like geometry: elevation 52 deg at a slant range of 6.6e6 m (the target at 1.227e7 m), 5.7 km/s across the line of sight
Scenario lageos_like(EpochEvent ev, double com_m = 0.251) {
    RangeObservation base = first_real_observation();
    base.com = lageos1_com(com_m);
    return Scenario(base, ev, 52.0, 6.6e6, 5700.0);
}
}  // namespace

TEST_CASE("MEAS-A-037  the centre of mass: R_model(delta) - R_model(0) = -delta exactly for 0.251 m; the wrong sign (adding delta) is shown failing by 2 delta",
          "[measmod][range]") {
    for (EpochEvent ev : {EpochEvent::GroundTransmit, EpochEvent::Bounce}) {
        const Scenario with = lageos_like(ev, 0.251), without = lageos_like(ev, 0.0);
        auto a = model_range(with.obs(), with.station(), with.trajectory(with.tag()));
        auto b = model_range(without.obs(), without.station(), without.trajectory(without.tag()));
        REQUIRE(a.has_value());
        REQUIRE(b.has_value());
        CHECK_THAT(a->range_m() - b->range_m(), WithinAbs(-0.251, 1e-9));
        CHECK(a->time_of_flight_s() == b->time_of_flight_s());                       // the same light time: the correction is applied to the range, not the geometry
        // the wrong sign: R(0) + delta differs from R(delta) by 2 delta
        CHECK_FALSE(std::abs((b->range_m() + 0.251) - a->range_m()) <= 1e-9);
        CHECK_THAT(std::abs((b->range_m() + 0.251) - a->range_m()), WithinAbs(0.502, 1e-9));
        // the residual is observed - modelled, the observed one-way-equivalent range being c ToF_obs / 2
        CHECK_THAT(a->observed_range_m(), WithinAbs(0.5 * c * with.obs().time_of_flight_s, 1e-9));
        CHECK_THAT(a->residual_m(), WithinAbs(a->observed_range_m() - a->range_m(), 0.0));
    }
}

TEST_CASE("MEAS-A-038  refusals around the centre of mass and the observed value: a non-spherical target, a negative or non-finite correction, a blank citation, "
          "a time of flight that is not finite and positive; a cited 0.251 m and a time of flight of 0.05 s pass",
          "[measmod][range]") {
    auto ns = non_spherical_target("an array target");
    REQUIRE_FALSE(ns.has_value());
    CHECK(ns.error().id == "MEAS-F-013");
    CHECK_THAT(ns.error().message, ContainsSubstring("an array target"));
    for (double bad : {-0.001, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto r = SphericalCentreOfMass::make(7603901, bad, "cited");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-017");
    }
    for (const char* blank : {"", " ", "\t", " \n "}) {
        auto r = SphericalCentreOfMass::make(7603901, 0.251, blank);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-006");
    }
    CHECK(SphericalCentreOfMass::make(7603901, 0.251, "cited").has_value());
    CHECK(SphericalCentreOfMass::make(7603901, 0.0, "cited").has_value());           // zero is a value (a point reflector), only a negative one is a sign error
    for (double bad : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        odl::io::CrdPass p = real_passes().front();
        p.ranges.front().time_of_flight_s = bad;
        auto r = build(p);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-018");
    }
    CHECK(build(real_passes().front()).has_value());
}

TEST_CASE("MEAS-A-039  the observed one-way-equivalent range of the real first normal point is c x 0.051212898595 / 2, an independent 60-digit product",
          "[measmod][range][real]") {
    const RangeObservation& obs = first_real_observation();
    CHECK(obs.time_of_flight_s == ref::np_first_tof_s);
    CHECK_THAT(0.5 * c * obs.time_of_flight_s, WithinAbs(ref::np_first_observed_range_m, 1e-9));
    const Scenario s = lageos_like(EpochEvent::GroundTransmit);
    auto m = model_range(s.obs(), s.station(), s.trajectory(s.tag()));
    REQUIRE(m.has_value());
    CHECK_THAT(m->observed_range_m(), WithinAbs(ref::np_first_observed_range_m, 1e-9));
}

TEST_CASE("MEAS-A-040  the Applied record of a modelled range: the two legs (their light times satisfy c tau = rho + delta, their delays DIFFER from their own "
          "elevations), the zenith delay and mapping, the centre of mass and its citation, the meteorology, the registry's release strings, the passes, and the "
          "omitted list — never empty, exactly the nine terms, each with a magnitude and a source",
          "[measmod][range]") {
    const Scenario s = lageos_like(EpochEvent::GroundTransmit);
    auto m = model_range(s.obs(), s.station(), s.trajectory(s.tag()));
    REQUIRE(m.has_value());
    const RangeApplied& a = m->applied();
    for (const AppliedLeg* leg : {&a.up, &a.down}) {
        CHECK_THAT(c * leg->light_time_s, WithinAbs(leg->geometric_range_m + leg->delay_atm_m + leg->delay_shapiro_m, 1e-9));
        CHECK(leg->passes >= 1);
        CHECK(leg->passes <= 5);
        CHECK(leg->delay_atm_m > 1.0);                                              // a metre-and-a-half to several metres at 52 degrees
        CHECK(leg->delay_atm_m < 4.0);
        CHECK(leg->delay_shapiro_m > 0.003);                                        // millimetres (MEAS-P-6)
        CHECK(leg->delay_shapiro_m < 0.012);
        CHECK_THAT(leg->direction.norm(), WithinAbs(1.0, 1e-14));
        CHECK(leg->elevation_rad > 0.8);                                            // about 52 degrees
        CHECK(leg->elevation_rad < 1.0);
    }
    // the two legs differ: different station epochs (22 ms apart: the Earth turned and the target moved), so different elevations and delays
    CHECK(a.up.delay_atm_m != a.down.delay_atm_m);
    CHECK(a.up.elevation_rad != a.down.elevation_rad);
    CHECK(a.up.light_time_s != a.down.light_time_s);
    CHECK(a.ztd_m > 1.6);
    CHECK(a.ztd_m < 2.6);
    CHECK_THAT(a.up.delay_atm_m, WithinAbs(a.ztd_m * a.mapping_up, 1e-12));
    CHECK_THAT(a.down.delay_atm_m, WithinAbs(a.ztd_m * a.mapping_down, 1e-12));
    CHECK(a.com_m == 0.251);
    CHECK_THAT(a.com_citation, ContainsSubstring("ILRS LAGEOS"));
    CHECK((a.meteorology.pressure_mbar == 976.70 && a.meteorology.held));
    CHECK_THAT(a.slrf_release, ContainsSubstring("260205"));
    CHECK_THAT(a.ecc_release, ContainsSubstring("UNE 250513"));
    CHECK_FALSE(a.post_seismic_override_used);
    REQUIRE(a.omitted.size() == 9);
    const char* names[] = {"solid_earth_tide", "ocean_tide_loading", "pole_tide", "atmospheric_loading", "tropospheric_gradients",
                           "itrs_realisation", "sun_shapiro_delay", "data_handling_biases", "com_station_dependence"};
    for (std::size_t i = 0; i < 9; ++i) {
        CHECK(a.omitted[i].name == names[i]);
        CHECK_FALSE(a.omitted[i].magnitude.empty());
        CHECK_FALSE(a.omitted[i].source.empty());
    }
    CHECK_FALSE(omitted_range_terms().empty());
    // the transmit, bounce and receive epochs are consistent with the legs
    CHECK_THAT(m->bounce_tt().difference(m->transmit_tt()).to_seconds(), WithinAbs(a.up.light_time_s, 1e-16));
    CHECK_THAT(m->receive_tt().difference(m->bounce_tt()).to_seconds(), WithinAbs(a.down.light_time_s, 1e-16));
    CHECK(m->transmit_tt() == s.tag());
    CHECK_THAT(m->time_of_flight_s(), WithinAbs(a.up.light_time_s + a.down.light_time_s, 0.0));
    CHECK_THAT(m->range_m(), WithinAbs(0.5 * c * m->time_of_flight_s() - 0.251, 1e-9));
}

TEST_CASE("MEAS-A-041  the analytic rows equal the closed-form derivatives: for the G3 configurations the position part of the row equals the 60-digit "
          "derivative of the closed-form time of flight, for both events, to 1e-12 relative — three orders tighter than a differenced row could be",
          "[measmod][range][gate]") {
    const Epoch t0 = utc(2026, 1, 1, 2, 8, 0.0);
    struct Geom { const char* name; Vec3 s0, vs, r0, vr; double e2[3], e1[3]; };
    const Geom geoms[] = {
        {"LAGEOS-like", {ref::lt_lageos_s0_x, ref::lt_lageos_s0_y, ref::lt_lageos_s0_z}, {ref::lt_lageos_vs_x, ref::lt_lageos_vs_y, ref::lt_lageos_vs_z},
         {ref::lt_lageos_r0_x, ref::lt_lageos_r0_y, ref::lt_lageos_r0_z}, {ref::lt_lageos_vr_x, ref::lt_lageos_vr_y, ref::lt_lageos_vr_z},
         {ref::lt_lageos_e2_d_range_dx, ref::lt_lageos_e2_d_range_dy, ref::lt_lageos_e2_d_range_dz},
         {ref::lt_lageos_e1_d_range_dx, ref::lt_lageos_e1_d_range_dy, ref::lt_lageos_e1_d_range_dz}},
        {"LEO-like", {ref::lt_leo_s0_x, ref::lt_leo_s0_y, ref::lt_leo_s0_z}, {ref::lt_leo_vs_x, ref::lt_leo_vs_y, ref::lt_leo_vs_z},
         {ref::lt_leo_r0_x, ref::lt_leo_r0_y, ref::lt_leo_r0_z}, {ref::lt_leo_vr_x, ref::lt_leo_vr_y, ref::lt_leo_vr_z},
         {ref::lt_leo_e2_d_range_dx, ref::lt_leo_e2_d_range_dy, ref::lt_leo_e2_d_range_dz},
         {ref::lt_leo_e1_d_range_dx, ref::lt_leo_e1_d_range_dy, ref::lt_leo_e1_d_range_dz}},
    };
    for (const Geom& g : geoms) {
        UniformStation station(t0, g.s0, g.vs);
        LinearTrajectory target(t0, g.r0, g.vr);
        for (EpochEvent ev : {EpochEvent::GroundTransmit, EpochEvent::Bounce}) {
            INFO(g.name << (ev == EpochEvent::GroundTransmit ? ", event 2" : ", event 1"));
            auto sol = solve_two_way(ev, t0, station, target, no_extra_path());
            REQUIRE(sol.has_value());
            const Vec3 row = range_partial_position(*sol, ev);
            const double* want = ev == EpochEvent::GroundTransmit ? g.e2 : g.e1;
            CHECK_THAT(row.x, WithinRel(want[0], 1e-12));
            CHECK_THAT(row.y, WithinRel(want[1], 1e-12));
            CHECK_THAT(row.z, WithinRel(want[2], 1e-12));
        }
        // the two events' rows differ at v/c, as the formulas say: the event-2 row has the target's ĝ·v/c in it (1.9e-5 relative at 5.7 km/s)
        auto s2 = solve_two_way(EpochEvent::GroundTransmit, t0, station, target, no_extra_path());
        auto s1 = solve_two_way(EpochEvent::Bounce, t0, station, target, no_extra_path());
        REQUIRE(s1.has_value());
        REQUIRE(s2.has_value());
        const Vec3 diff = range_partial_position(*s2, EpochEvent::GroundTransmit) - range_partial_position(*s1, EpochEvent::Bounce);
        CHECK(diff.norm() > 1e-6);
        CHECK(diff.norm() < 1e-4);
    }
}

// ---- the interface ---------------------------------------------------------------------------------------------------------------
// negative interface checks must be dependent (a requires-expression outside a template that names a missing member is a hard error, not false)
template <class T> concept HasUnitlessRange = requires(const T& m) { m.range(); };
template <class T> concept HasScalelessBounce = requires(const T& m) { m.bounce(); };
template <class T> concept HasAzimuth = requires(const T& m) { m.azimuth_rad(); };

TEST_CASE("MEAS-A-050  ModelledRange cannot be formed from raw numbers; its accessors carry their unit and their time scale in their names",
          "[measmod][interface]") {
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<ModelledRange>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<ModelledRange, double, double, double, Epoch, Epoch, Epoch, Partials<odl::frames::Frame::GCRS, 1>, RangeApplied>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<ModelledRange, double>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<ModelledRange, double, double>);
    STATIC_REQUIRE(requires(const ModelledRange& m) { m.range_m(); m.observed_range_m(); m.residual_m(); m.bounce_tt(); m.transmit_tt(); m.receive_tt(); m.partials(); m.applied(); });
    STATIC_REQUIRE_FALSE(HasUnitlessRange<ModelledRange>);                   // no accessor without a unit
    STATIC_REQUIRE_FALSE(HasScalelessBounce<ModelledRange>);                  // no epoch without its scale
    STATIC_REQUIRE_FALSE(HasAzimuth<ModelledRange>);             // a range result has no angles
    // the partials row is typed by the frame of the state it differentiates, and has the shape 1 x 6
    STATIC_REQUIRE(Partials<odl::frames::Frame::GCRS, 1>::frame == odl::frames::Frame::GCRS);
    STATIC_REQUIRE(Partials<odl::frames::Frame::GCRS, 1>::rows == 1);
    STATIC_REQUIRE_FALSE(std::is_same_v<Partials<odl::frames::Frame::GCRS, 1>, Partials<odl::frames::Frame::ITRS, 1>>);
    const Scenario s = lageos_like(EpochEvent::GroundTransmit);
    auto m = model_range(s.obs(), s.station(), s.trajectory(s.tag()));
    REQUIRE(m.has_value());
    CHECK(m->partials().d.size() == 1);
    CHECK(m->partials().d[0].size() == 6);
    for (int k = 3; k < 6; ++k) CHECK(m->partials().d[0][static_cast<std::size_t>(k)] == 0.0);   // the velocity columns are zero to first order
}

TEST_CASE("MEAS-A-051  a dependency's refusal comes back with its OWN id and this module's context: the EOP series, the time module's leap table, a trajectory, "
          "a station; and there is no persistent error state (a good call after a failing one gives the good result)",
          "[measmod][interface]") {
    const Scenario s = lageos_like(EpochEvent::GroundTransmit);
    auto good = model_range(s.obs(), s.station(), s.trajectory(s.tag()));
    REQUIRE(good.has_value());
    // the EOP series' refusal: an epoch past its coverage
    const Epoch late = utc(2027, 5, 1, 0, 0, 0.0);
    auto direct = c04().at(late, odl::eop::EopPolicy{});
    REQUIRE_FALSE(direct.has_value());
    RangeObservation late_obs = s.obs();
    late_obs.epoch = late;
    auto r = model_range(late_obs, s.station(), s.trajectory(late));
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().id == direct.error().id);                                                // the EOP module's id, not a MEAS one
    CHECK(r.error().id.substr(0, 3) == "EOP");
    CHECK_THAT(r.error().message, ContainsSubstring("range model"));
    // the time module's refusal from the builder: a date past the leap-second table's horizon
    auto from_calendar = Epoch::from_calendar(TimeScale::UTC, Calendar{2028, 1, 1, 0, 0, 5.0}, leaps());
    REQUIRE_FALSE(from_calendar.has_value());
    auto past = build(one_pass(pass_text(2028, 1, 1, 0, 0, 0, {{5.0, 0.0512}})));
    REQUIRE_FALSE(past.has_value());
    CHECK(past.error().id == from_calendar.error().id);
    // a trajectory and a station that refuse
    RefusingTrajectory t_refusing(s.tag(), s.target_position(), s.target_velocity(), 0.001);
    auto rt = model_range(s.obs(), s.station(), t_refusing);
    REQUIRE_FALSE(rt.has_value());
    CHECK(rt.error().id == "TRAJ-F-999");
    RefusingStation s_refusing(s.tag(), s.station_position(), Vec3{}, 0.030);
    auto rs = model_range(s.obs(), s_refusing, s.trajectory(s.tag()));
    REQUIRE_FALSE(rs.has_value());
    CHECK(rs.error().id == "STN-F-999");
    // no persistent state: the good call again, after four failing ones, gives the same numbers
    auto again = model_range(s.obs(), s.station(), s.trajectory(s.tag()));
    REQUIRE(again.has_value());
    CHECK(again->range_m() == good->range_m());
    CHECK(again->partials().d[0][0] == good->partials().d[0][0]);
}

TEST_CASE("MEAS-A-052  substitutability: the identical model_range code path runs with a closed-form station and with the production Earth-fixed station, "
          "and the two agree where they must",
          "[measmod][interface]") {
    const Scenario s = lageos_like(EpochEvent::GroundTransmit);
    auto prod = model_range(s.obs(), s.station(), s.trajectory(s.tag()));
    REQUIRE(prod.has_value());
    // a station in uniform motion that mimics the real one at the tag epoch (same position, velocity, vertical): over 40 ms the rotation's curvature is
    // 1/2 omega^2 r tau^2 ~ 6e-6 m and the vertical turns by 3e-6 rad, so the two ranges agree to 1e-4 m
    auto k = s.station().at(s.tag());
    REQUIRE(k.has_value());
    UniformStation closed(s.tag(), k->position_m, k->velocity_m_s, k->up);
    auto cl = model_range(s.obs(), closed, s.trajectory(s.tag()));
    REQUIRE(cl.has_value());
    CHECK_THAT(cl->range_m(), WithinAbs(prod->range_m(), 1e-4));
    CHECK_THAT(cl->partials().d[0][0], WithinAbs(prod->partials().d[0][0], 1e-6));
    CHECK_THAT(cl->partials().d[0][1], WithinAbs(prod->partials().d[0][1], 1e-6));
    CHECK_THAT(cl->partials().d[0][2], WithinAbs(prod->partials().d[0][2], 1e-6));
    // and they are NOT identical (the production one rotates): the closed form is a genuine stand-in, not the same object
    CHECK(cl->range_m() != prod->range_m());
}

TEST_CASE("MEAS-A-053  no module-level or thread-local mutable state: the module's sources contain no thread_local and no non-const static variable",
          "[measmod][interface]") {
    namespace fs = std::filesystem;
    const fs::path root = ODL_MEASMOD_DIR;
    std::size_t files = 0, lines = 0;
    const std::regex nonconst_static(R"(^\s*static\s+(?!const\b)(?!constexpr\b)(?!inline\b)(?!_assert)(?!_cast)[^()]*(=[^=]|;)\s*(//.*)?$)");
    for (const auto& e : fs::recursive_directory_iterator(root / "src")) {
        if (e.path().extension() != ".cpp") continue;
        ++files;
        std::ifstream in(e.path());
        std::string line;
        std::size_t n = 0;
        while (std::getline(in, line)) {
            ++n;
            ++lines;
            INFO(e.path().filename().string() << ":" << n << "  " << line);
            CHECK(line.find("thread_local") == std::string::npos);
            CHECK_FALSE(std::regex_search(line, nonconst_static));
        }
    }
    for (const auto& e : fs::recursive_directory_iterator(root / "include")) {
        if (e.path().extension() != ".hpp") continue;
        ++files;
        std::ifstream in(e.path());
        std::string line;
        while (std::getline(in, line)) {
            ++lines;
            CHECK(line.find("thread_local") == std::string::npos);
        }
    }
    CHECK(files >= 8);                                                       // the scan found the module's sources (it cannot pass by finding nothing)
    CHECK(lines > 1000);
    // the scan can fail: a synthetic offender is caught by the same patterns
    CHECK(std::regex_search(std::string("static int counter = 0;"), nonconst_static));
    CHECK(std::regex_search(std::string("    static std::vector<double> cache;"), nonconst_static));
    CHECK_FALSE(std::regex_search(std::string("    static const std::vector<OmittedTerm> terms = {"), nonconst_static));
    CHECK_FALSE(std::regex_search(std::string("    [[nodiscard]] static odl::Result<SodKey, MeasError> make(int pad);"), nonconst_static));
}
