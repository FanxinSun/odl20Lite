// time_tests.cpp — SPEC-time.md §8, the acceptance suite.
//
// The expected values come from published sources: the IERS leap-second table,
// IERS Conventions TN36 ch. 10, and closed-form identities.  Where a test's
// source is "this spec" it is checking a refusal, and a refusal is a design
// decision whose evidence is that it fires with the content it promised.

#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace odl::time;

namespace {

std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/// The real IERS table, from the manifest cache.  The path is passed in by CMake
/// so that the test uses the pinned, hash-verified bytes and nothing else.
const LeapTable& iers_table() {
    static const LeapTable t = [] {
        auto r = LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE),
                                  LeapProvenance{"iers-leap-seconds", "", "IERS Bulletin C 72"});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

Calendar cal(int y, int m, int d, int h = 0, int mi = 0, double s = 0.0) {
    return Calendar{y, m, d, h, mi, s};
}

Epoch must(odl::Result<Epoch, TimeError> r) {
    if (!r.has_value()) FAIL("unexpected refusal: " << r.error().message);
    return *r;
}

}  // namespace

// --------------------------------------------------------------------------- //

TEST_CASE("TIME-A-001: TT − TAI is 32.184 s exactly", "[time][spec]") {
    for (int year : {1958, 1972, 2000, 2016, 2026, 2050}) {
        const Epoch e = must(Epoch::from_calendar(TimeScale::TAI, cal(year, 6, 1), iers_table()));
        const auto tt = e.calendar(TimeScale::TT, iers_table(), 9);
        REQUIRE(tt.has_value());
        REQUIRE(tt->hour == 0);
        REQUIRE(tt->minute == 0);
        REQUIRE_THAT(tt->second, WithinAbs(32.184, 1e-9));
    }
}

TEST_CASE("TIME-A-002: dAT matches the IERS table at every row", "[time][spec]") {
    const auto& t = iers_table();
    REQUIRE(t.entries().size() == 28);
    REQUIRE(t.entries().front().mjd_utc == 41317);
    REQUIRE(t.entries().front().delta_at == 10);
    REQUIRE(t.entries().back().mjd_utc == 57754);
    REQUIRE(t.entries().back().delta_at == 37);

    for (const auto& row : t.entries()) {
        const auto on  = t.delta_at_on(row.mjd_utc);
        const auto day_before = t.delta_at_on(row.mjd_utc - 1);
        REQUIRE(on.has_value());
        REQUIRE(*on == row.delta_at);
        if (row.mjd_utc > LeapTable::kFirstSupportedMjd) {
            REQUIRE(day_before.has_value());
            REQUIRE(*day_before == row.delta_at - 1);
        }
    }
}

TEST_CASE("TIME-A-003: TAI <-> UTC round trips across every leap second",
          "[time][spec]") {
    const auto& t = iers_table();
    int checked = 0;
    for (const auto& row : t.entries()) {
        if (row.mjd_utc == LeapTable::kFirstSupportedMjd) continue;
        // 00:00:00 UTC on the day the step takes effect, then walk in SI seconds
        // either side of it so the inserted second is crossed in both directions.
        double djm0 = 0.0;
        const auto step = Epoch::from_two_part_jd(
            TimeScale::TAI, static_cast<double>(row.mjd_utc) + 2400000.5, 0.0, t);
        REQUIRE(step.has_value());
        (void)djm0;
        const Epoch boundary = step->add(Duration::from_parts(row.delta_at, 0.0));
        for (std::int64_t off = -4; off <= 2; ++off) {
            const Epoch x = boundary.add(Duration::from_parts(off, 0.0));
            const auto c = x.calendar(TimeScale::UTC, t, 6);
            REQUIRE(c.has_value());
            const auto back = Epoch::from_calendar(TimeScale::UTC, *c, t);
            REQUIRE(back.has_value());
            INFO("step MJD " << row.mjd_utc << " offset " << off << " -> "
                             << c->year << "-" << c->month << "-" << c->day << "T"
                             << c->hour << ":" << c->minute << ":" << c->second);
            REQUIRE(back->tai_seconds() == x.tai_seconds());
            ++checked;
        }
        // The inserted second renders as 23:59:60 of the preceding day.
        const auto leap = boundary.add(Duration::from_parts(-1, 0.0))
                              .calendar(TimeScale::UTC, t, 3);
        REQUIRE(leap.has_value());
        REQUIRE(leap->hour == 23);
        REQUIRE(leap->minute == 59);
        REQUIRE_THAT(leap->second, WithinAbs(60.0, 1e-6));
    }
    REQUIRE(checked == 27 * 7);
}

TEST_CASE("TIME-A-004: the 2016-12-31 leap second, the worked case of SPEC-time §4.5",
          "[time][spec]") {
    const auto& t = iers_table();
    struct Row { Calendar utc; Calendar tai; };
    const Row rows[] = {
        {cal(2016, 12, 31, 23, 59, 59), cal(2017, 1, 1, 0, 0, 35)},
        {cal(2016, 12, 31, 23, 59, 60), cal(2017, 1, 1, 0, 0, 36)},
        {cal(2017,  1,  1,  0,  0,  0), cal(2017, 1, 1, 0, 0, 37)},
    };
    for (const auto& r : rows) {
        const Epoch e = must(Epoch::from_calendar(TimeScale::UTC, r.utc, t));
        const auto got = e.calendar(TimeScale::TAI, t, 6);
        REQUIRE(got.has_value());
        REQUIRE(got->year == r.tai.year);
        REQUIRE(got->month == r.tai.month);
        REQUIRE(got->day == r.tai.day);
        REQUIRE(got->hour == r.tai.hour);
        REQUIRE(got->minute == r.tai.minute);
        REQUIRE_THAT(got->second, WithinAbs(r.tai.second, 1e-9));

        const auto back = e.calendar(TimeScale::UTC, t, 6);
        REQUIRE(back.has_value());
        REQUIRE(back->day == r.utc.day);
        REQUIRE(back->hour == r.utc.hour);
        REQUIRE(back->minute == r.utc.minute);
        REQUIRE_THAT(back->second, WithinAbs(r.utc.second, 1e-9));
    }
}

TEST_CASE("TIME-A-005: two SI seconds elapse across the 61-second minute",
          "[time][spec]") {
    const auto& t = iers_table();
    const Epoch a = must(Epoch::from_calendar(TimeScale::UTC, cal(2016, 12, 31, 23, 59, 59), t));
    const Epoch b = must(Epoch::from_calendar(TimeScale::UTC, cal(2017, 1, 1, 0, 0, 0), t));
    const Duration d = b.difference(a);
    REQUIRE(d.whole_seconds() == 2);
    REQUIRE_THAT(d.fraction(), WithinAbs(0.0, 1e-12));
}

TEST_CASE("TIME-A-006/007: TAI − GPS is 19 s exactly, and the GPS epoch aligns with UTC",
          "[time][spec]") {
    const auto& t = iers_table();
    // Check the offset itself rather than a calendar day: an earlier version of
    // this test asserted "28 February" and failed in leap years, which was the
    // test being wrong and not the code.
    for (int year : {1980, 1999, 2016, 2026, 2050}) {
        const Epoch e = must(Epoch::from_calendar(TimeScale::TAI, cal(year, 3, 1), t));
        const auto gps_cal = e.calendar(TimeScale::GPS, t, 9);
        REQUIRE(gps_cal.has_value());
        const Epoch same = must(Epoch::from_calendar(TimeScale::GPS, *gps_cal, t));
        REQUIRE(same.tai_seconds() == e.tai_seconds());
        // TAI − GPS = 19 s exactly.  The GPS reading of an instant is 19 s less
        // than its TAI reading, so reading those same fields back as TAI lands
        // 19 s EARLIER than the instant itself.  (Written the other way round
        // first; the test caught the sign, which is what a sign test is for.)
        const Epoch as_tai = must(Epoch::from_calendar(TimeScale::TAI, *gps_cal, t));
        REQUIRE(e.difference(as_tai).whole_seconds() == 19);
        REQUIRE_THAT(e.difference(as_tai).fraction(), WithinAbs(0.0, 1e-12));
    }
    const Epoch gps_epoch = must(Epoch::from_gps_week(1024, 0.0));
    const Epoch by_utc = must(Epoch::from_calendar(
        TimeScale::UTC, cal(1980, 1, 6), t));
    REQUIRE(gps_epoch.difference(by_utc).whole_seconds() == 1024 * 604800);
}

TEST_CASE("TIME-A-008: epoch round trips through every scale, to under a nanosecond",
          "[time][spec]") {
    const auto& t = iers_table();
    std::mt19937_64 rng(20260918);
    std::uniform_int_distribution<std::int64_t> secs(
        (41317 - Epoch::kOriginMjd) * 86400, (73459 - Epoch::kOriginMjd) * 86400);
    std::uniform_real_distribution<double> frac(0.0, 1.0);

    const TimeScale scales[] = {TimeScale::TAI, TimeScale::TT, TimeScale::TCG,
                                TimeScale::TDB, TimeScale::TCB, TimeScale::UTC,
                                TimeScale::GPS};
    double worst = 0.0;
    for (int i = 0; i < 2000; ++i) {
        const Epoch e = must(Epoch::from_two_part_jd(
            TimeScale::TAI,
            std::floor(static_cast<double>(Epoch::kOriginMjd + secs(rng) / 86400) + 2400000.5),
            0.25 + 0.5 * frac(rng), t));
        for (TimeScale s : scales) {
            const auto c = e.calendar(s, t, 12);
            if (!c.has_value()) continue;          // UTC may refuse past expiry
            const auto back = Epoch::from_calendar(s, *c, t);
            REQUIRE(back.has_value());
            const double err = std::abs(back->difference(e).to_seconds());
            worst = std::max(worst, err);
        }
    }
    INFO("worst round-trip error across all scales: " << worst << " s");
    REQUIRE(worst < 1e-9);
}

TEST_CASE("TIME-A-010/011: TDB − TT has the published amplitude, and a site adds a diurnal term",
          "[time][spec]") {
    const auto& t = iers_table();
    double lo = 1e9, hi = -1e9;
    for (int day = 0; day < 366; ++day) {
        const Epoch e = must(Epoch::from_calendar(TimeScale::TT, cal(2026, 1, 1), t))
                            .add(Duration::from_parts(day * 86400, 0.0));
        const double v = e.tdb_minus_tt().to_seconds();
        lo = std::min(lo, v); hi = std::max(hi, v);
    }
    // TN36 §10.1: the non-linear terms P(TT) have a maximum amplitude of about
    // 1.6 ms, so peak-to-peak is about 3.4 ms.
    INFO("TDB−TT over 2026: " << lo << " .. " << hi << " s");
    REQUIRE(hi - lo > 2.6e-3);
    REQUIRE(hi - lo < 3.8e-3);

    // A station at 45° N contributes a diurnal term of order a microsecond.
    const Site station{0.0, 4517.6, 4487.3};   // ~45° N on a spherical Earth, km
    double dlo = 1e9, dhi = -1e9;
    for (int h = 0; h < 24; ++h) {
        const Epoch e = must(Epoch::from_calendar(TimeScale::TT, cal(2026, 6, 1, h), t));
        const double d = (e.tdb_minus_tt(station) - e.tdb_minus_tt()).to_seconds();
        dlo = std::min(dlo, d); dhi = std::max(dhi, d);
    }
    INFO("diurnal term peak-to-peak: " << (dhi - dlo) * 1e6 << " us");
    REQUIRE((dhi - dlo) > 1.0e-6);
    REQUIRE((dhi - dlo) < 5.0e-6);
}

TEST_CASE("TIME-A-012: a two-part JD has an integral day and a fraction below one",
          "[time][spec]") {
    const auto& t = iers_table();
    const Epoch e = must(Epoch::from_calendar(TimeScale::UTC, cal(2016, 12, 31, 23, 59, 60), t));
    for (TimeScale s : {TimeScale::TAI, TimeScale::TT, TimeScale::TDB, TimeScale::UTC}) {
        const auto j = e.two_part_jd(s, t);
        REQUIRE(j.has_value());
        INFO("scale " << name_of(s));
        REQUIRE(j->day == std::floor(j->day));
        REQUIRE(j->fraction >= 0.0);
        REQUIRE(j->fraction < 1.0);
    }
}

TEST_CASE("TIME-A-013/014/026: the pre-1972 policy refuses UTC and UT1, not time itself",
          "[time][refusal]") {
    const auto& t = iers_table();
    const auto utc = Epoch::from_calendar(TimeScale::UTC, cal(1971, 12, 31, 23, 59, 59), t);
    REQUIRE_FALSE(utc.has_value());
    REQUIRE(utc.error().id == "TIME-F-002");
    REQUIRE(utc.error().message.find("1972") != std::string::npos);

    // TIME-R-041: the uniform scales are unaffected.
    REQUIRE(Epoch::from_calendar(TimeScale::TT,  cal(1965, 6, 1), t).has_value());
    REQUIRE(Epoch::from_calendar(TimeScale::TAI, cal(1960, 1, 1), t).has_value());
    REQUIRE(Epoch::from_calendar(TimeScale::TDB, cal(1965, 6, 1), t).has_value());

    // UT1 is derived from UTC, so it is refused there too.
    const Epoch early = must(Epoch::from_calendar(TimeScale::TAI, cal(1965, 6, 1), t));
    const auto ut1 = early.ut1_two_part_jd(Duration::from_seconds(0.1), t);
    REQUIRE_FALSE(ut1.has_value());
    REQUIRE(ut1.error().id == "TIME-F-002");
}

TEST_CASE("TIME-A-015/016: the table's expiry is enforced, with one named logged override",
          "[time][refusal]") {
    const auto& t = iers_table();
    REQUIRE(t.expiry_mjd() != 0);
    INFO("expiry: " << t.expiry_text());

    const auto past = t.delta_at_on(t.expiry_mjd() + 1);
    REQUIRE_FALSE(past.has_value());
    REQUIRE(past.error().id == "TIME-F-004");
    REQUIRE(past.error().message.find("assume_no_further_leap_seconds") != std::string::npos);
    REQUIRE(past.error().message.find(std::to_string(t.expiry_mjd())) != std::string::npos);

    LeapTable relaxed = t;
    relaxed.assume_no_further_leap_seconds(true);
    const auto allowed = relaxed.delta_at_on(t.expiry_mjd() + 1);
    REQUIRE(allowed.has_value());
    REQUIRE(*allowed == 37);
    REQUIRE(relaxed.assuming_no_further_leap_seconds());
    // The original is untouched: the override is per-table, per-run, not global.
    REQUIRE_FALSE(t.assuming_no_further_leap_seconds());
    REQUIRE_FALSE(t.delta_at_on(t.expiry_mjd() + 1).has_value());
}

TEST_CASE("TIME-A-017: a 61st second is refused on a day that does not have one",
          "[time][refusal]") {
    const auto& t = iers_table();
    const auto r = Epoch::from_calendar(TimeScale::UTC, cal(2016, 6, 30, 23, 59, 60), t);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().id == "TIME-F-003");
    // The day that does have one is accepted.
    REQUIRE(Epoch::from_calendar(TimeScale::UTC, cal(2016, 12, 31, 23, 59, 60), t).has_value());
    // ... but not at the wrong time of day.
    REQUIRE_FALSE(Epoch::from_calendar(TimeScale::UTC, cal(2016, 12, 31, 12, 0, 60), t)
                      .has_value());
}

TEST_CASE("TIME-A-018: a malformed leap-second file is refused, naming the line",
          "[time][refusal]") {
    const auto good = std::string("#  File expires on 28 June 2027\n"
                                  "    41317.0    1  1 1972       10\n"
                                  "    41499.0    1  7 1972       11\n");
    REQUIRE(LeapTable::parse(good, {}).has_value());

    struct Case { const char* text; const char* why; };
    const Case bad[] = {
        {"#  x\n    41317.0    1  1 1972       10\n    41317.0    1  1 1972       11\n",
         "non-increasing MJD"},
        {"#  x\n    41499.0    1  7 1972       11\n    41317.0    1  1 1972       10\n",
         "decreasing MJD"},
        {"#  x\n    41317.0    1  1 1972       10.5\n", "non-integral dAT after 1972"},
        {"#  x\n    41317.0    9  9 1999       10\n", "date inconsistent with MJD"},
        {"#  x\n    not-a-number\n", "unparseable row"},
        {"#  only comments\n", "no data rows"},
    };
    for (const auto& c : bad) {
        INFO(c.why);
        const auto r = LeapTable::parse(c.text, LeapProvenance{"synthetic", "", ""});
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "TIME-F-005");
    }
}

TEST_CASE("TIME-A-019: two tables coexist; there is no global leap state", "[time][spec]") {
    const auto& real_table = iers_table();
    auto shorter = LeapTable::parse("#  File expires on 28 June 2027\n"
                                    "    41317.0    1  1 1972       10\n"
                                    "    41499.0    1  7 1972       11\n",
                                    LeapProvenance{"synthetic", "", ""});
    REQUIRE(shorter.has_value());
    for (int i = 0; i < 3; ++i) {
        REQUIRE(*real_table.delta_at_on(57754) == 37);
        REQUIRE(*shorter->delta_at_on(57754) == 11);
    }
}

TEST_CASE("TIME-A-020: ordering agrees with the sign of the difference", "[time][spec]") {
    const auto& t = iers_table();
    std::mt19937_64 rng(7);
    std::uniform_int_distribution<std::int64_t> d(0, 2'000'000'000);
    for (int i = 0; i < 20000; ++i) {
        const Epoch a = must(Epoch::from_calendar(TimeScale::TAI, cal(1972, 1, 1), t))
                            .add(Duration::from_parts(d(rng), 0.0));
        const Epoch b = must(Epoch::from_calendar(TimeScale::TAI, cal(1972, 1, 1), t))
                            .add(Duration::from_parts(d(rng), 0.0));
        REQUIRE((a < b) == (b.difference(a) > Duration::zero()));
        REQUIRE((a == b) == (b.difference(a) == Duration::zero()));
    }
}

TEST_CASE("TIME-A-024: calendar fields are refused, never normalised by carrying",
          "[time][refusal]") {
    const auto& t = iers_table();
    const Calendar bad[] = {
        cal(2017, 13, 1), cal(2017, 2, 30, 0, 0, 0), cal(2017, 1, 1, 24, 0, 0),
        cal(2017, 1, 1, 0, 60, 0), cal(2017, 1, 1, 0, 0, -1.0), cal(2017, 1, 1, 0, 0, 61.0),
    };
    for (const auto& c : bad) {
        const auto r = Epoch::from_calendar(TimeScale::TAI, c, t);
        INFO(c.year << "-" << c.month << "-" << c.day << "T" << c.hour << ":" << c.minute
                    << ":" << c.second);
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "TIME-F-007");
    }
}

TEST_CASE("TIME-A-025: display rounding cannot invent a 61st second or a 24th hour",
          "[time][spec]") {
    const auto& t = iers_table();
    // A hair before midnight on a day with no leap second: rounding to 3 places
    // must not produce 60.000.
    const Epoch a = must(Epoch::from_calendar(TimeScale::UTC, cal(2016, 6, 30, 23, 59, 59), t))
                        .add(Duration::from_seconds(0.9999999));
    const auto ca = a.calendar(TimeScale::UTC, t, 3);
    REQUIRE(ca.has_value());
    REQUIRE(ca->second < 60.0);
    REQUIRE(ca->hour == 23);
    REQUIRE(ca->minute == 59);

    // The same instant on the day that DOES end in a leap second may render 60.
    const Epoch b = must(Epoch::from_calendar(TimeScale::UTC, cal(2016, 12, 31, 23, 59, 60), t));
    const auto cb = b.calendar(TimeScale::UTC, t, 3);
    REQUIRE(cb.has_value());
    REQUIRE_THAT(cb->second, WithinAbs(60.0, 1e-9));
}

TEST_CASE("structural: an Epoch cannot be built without naming a scale", "[time][structural]") {
    // TIME-R-003, TIME-R-011.  There is no default constructor and no
    // constructor from a bare number; every route in names a TimeScale.
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<Epoch>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Epoch, std::int64_t, double>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Epoch, double>);
    // TIME-R-001: no UT1 in the scale enumeration at all.
    STATIC_REQUIRE(static_cast<int>(TimeScale::GPS) >= 0);
}

TEST_CASE("L1 step 2 gate: every timescale pair converts and returns", "[time][spec][gate]") {
    const auto& t = iers_table();
    const TimeScale all[] = {TimeScale::TAI, TimeScale::TT,  TimeScale::TCG, TimeScale::TDB,
                             TimeScale::TCB, TimeScale::UTC, TimeScale::GPS};
    const Epoch anchor = must(Epoch::from_calendar(TimeScale::TAI, cal(2026, 9, 18, 6, 30, 12.5), t));

    double worst = 0.0;
    for (TimeScale from : all) {
        const auto cf = anchor.calendar(from, t, 12);
        REQUIRE(cf.has_value());
        const Epoch rebuilt = must(Epoch::from_calendar(from, *cf, t));
        for (TimeScale to : all) {
            const auto ct = rebuilt.calendar(to, t, 12);
            REQUIRE(ct.has_value());
            const Epoch back = must(Epoch::from_calendar(to, *ct, t));
            worst = std::max(worst, std::abs(back.difference(anchor).to_seconds()));
        }
    }
    INFO("worst error over all 49 ordered scale pairs: " << worst << " s");
    REQUIRE(worst < 1e-9);
}

TEST_CASE("the published constants of IERS TN36 are the ones in force", "[time][spec]") {
    const auto& t = iers_table();
    // TCG − TT = L_G/(1−L_G) (JD_TT − T0) 86400, TN36 eq. (10.1), L_G defining.
    const Epoch e = must(Epoch::from_calendar(TimeScale::TT, cal(2026, 1, 1, 12), t));
    const auto tt  = e.two_part_jd(TimeScale::TT, t);
    const auto tcg = e.calendar(TimeScale::TCG, t, 12);
    REQUIRE(tt.has_value());
    REQUIRE(tcg.has_value());
    constexpr double kLg = 6.969290134e-10, kT0 = 2443144.5003725;
    const double expected = (kLg / (1.0 - kLg)) * ((tt->day - kT0) + tt->fraction) * 86400.0;
    const double got = (static_cast<double>(tcg->hour - 12) * 3600.0 +
                        static_cast<double>(tcg->minute) * 60.0 + tcg->second);
    INFO("TCG − TT at 2026-01-01: expected " << expected << " s, got " << got << " s");
    REQUIRE_THAT(got, WithinAbs(expected, 1e-9));

    // TDB0 is a defining constant of TN36 eq. (10.3); TCB − TDB carries it.
    const auto tdb = e.two_part_jd(TimeScale::TDB, t);
    const auto tcb = e.two_part_jd(TimeScale::TCB, t);
    REQUIRE(tdb.has_value());
    REQUIRE(tcb.has_value());
    constexpr double kLb = 1.550519768e-8, kTdb0 = -6.55e-5;
    const double tcb_minus_tdb = ((tcb->day - tdb->day) + (tcb->fraction - tdb->fraction)) * 86400.0;
    const double predicted = kLb * ((tcb->day - kT0) + tcb->fraction) * 86400.0 - kTdb0;
    INFO("TCB − TDB: " << tcb_minus_tdb << " s, predicted " << predicted << " s");
    // Tight on purpose.  At 1e-6 this passed while the Julian-date splitting was
    // quantising at 40 µs; the whole value of the check is that it is tighter
    // than the defect it is meant to catch.
    REQUIRE_THAT(tcb_minus_tdb, WithinAbs(predicted, 1e-9));
}
