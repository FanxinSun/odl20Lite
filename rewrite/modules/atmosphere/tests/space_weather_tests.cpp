// space_weather_tests.cpp — the ingestion layer's own gates.
//
// SPEC-atmosphere.md §8.  THESE ARE INDEPENDENT OF ATMO-A-001.  The 17 published
// cases supply F10.7 and Ap as literal constants in the reference driver's DATA
// statements; they never touch a file, a column, a date or a coverage window.
// So the model gate exercises NONE of this, and reading it as evidence that the
// space-weather layer works would be exactly the PERT-A-001 mistake.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/atmosphere/space_weather.hpp>

#include <fstream>
#include <sstream>
#include <vector>

using namespace odl::atmosphere;

namespace {

std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream o; o << f.rdbuf(); return o.str();
}

/// Exactly the manifest entry's declared list.  SN is absent, and that absence
/// is the point: it is CC BY-NC 4.0 inside a CC BY 4.0 file.
std::vector<std::string> declared() {
    return {"YYYY","MM","DD","Kp1","Kp2","Kp3","Kp4","Kp5","Kp6","Kp7","Kp8",
            "ap1","ap2","ap3","ap4","ap5","ap6","ap7","ap8","Ap","F10.7obs","F10.7adj","D"};
}

/// Civil-date arithmetic, so the centred mean can be checked by a route that is
/// not the loader's own index walk.  Howard Hinnant's days_from_civil.
long days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned long>(y - era * 400);
    const unsigned long doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    const unsigned long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long>(doe) - 719468;
}
Day civil_from_days(long z) {
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<unsigned long>(z - era * 146097);
    const unsigned long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long y = static_cast<long>(yoe) + era * 400;
    const unsigned long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned long mp = (5 * doy + 2) / 153;
    const unsigned long d = doy - (153 * mp + 2) / 5 + 1;
    const unsigned long m = (mp < 10) ? mp + 3 : mp - 9;
    return Day{static_cast<int>(y + (m <= 2)), static_cast<int>(m), static_cast<int>(d)};
}
Day civil_add(const Day& d, int days) {
    return civil_from_days(days_from_civil(d.year, static_cast<unsigned>(d.month),
                                           static_cast<unsigned>(d.day)) + days);
}

SpaceWeatherTable load() {
    auto t = SpaceWeatherTable::load(slurp(ODL_GFZ_KP_AP_F107), "gfz-kp-ap-f107",
                                     "0121821b20799731", declared());
    REQUIRE(t.has_value());
    return *t;
}

}  // namespace

TEST_CASE("ATMO-A-010  the data classes partition the file", "[spaceweather]") {
    const auto t = load();
    const auto& s = t.summary();
    INFO("rows " << s.rows << "  definitive " << s.by_class[0] << "  kp-definitive "
         << s.by_class[1] << "  preliminary " << s.by_class[2]);
    CHECK(s.rows == 34594);
    CHECK(s.by_class[0] == 34424);   // D = 2
    CHECK(s.by_class[1] == 153);     // D = 1
    CHECK(s.by_class[2] == 17);      // D = 0
    CHECK(s.by_class[3] == 0);       // no forecast rows in this source at all
    CHECK(s.by_class[0] + s.by_class[1] + s.by_class[2] + s.by_class[3] == s.rows);
}

TEST_CASE("ATMO-A-023  usable coverage is a set, not an interval", "[spaceweather]") {
    const auto t = load();
    const auto& s = t.summary();
    INFO("sentinels " << s.f107_sentinel << " of " << s.rows
         << "; usable " << s.usable_days << " with " << s.usable_breaks << " breaks");
    CHECK(s.f107_sentinel == 6178);
    CHECK(s.f107_present + s.f107_sentinel == s.rows);
    CHECK(s.usable_days == 22991);
    // THE POINT: the usable days are NOT contiguous.  An interval [first, last]
    // would accept an epoch inside one of these holes.
    CHECK(s.usable_breaks == 25);
    CHECK(s.first_usable == Day{1956, 10, 14});
    CHECK(s.last_usable == Day{2026, 8, 8});

    // and the interval's endpoints really do contain unusable days
    CHECK_FALSE(t.usable(Day{1958, 1, 1}));
    CHECK(t.usable(Day{2020, 1, 1}));
}

TEST_CASE("ATMO-A-014  a sentinel is absence, not a value", "[spaceweather]") {
    const auto t = load();
    // 1947-03-01 … 03-04 is a four-day gap inside the early record.
    const auto bad = t.sample(Day{1947, 3, 2});
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "ATMO-F-014");
    // proven both ways: a day whose whole window is present works
    CHECK(t.sample(Day{2020, 1, 1}).has_value());
}

TEST_CASE("ATMO-A-022  an undeclared column cannot be read", "[spaceweather]") {
    const auto t = load();
    const auto sn = t.column(Day{2020, 1, 1}, "SN");
    REQUIRE_FALSE(sn.has_value());
    CHECK(sn.error().id == "ATMO-F-016");
    // proven both ways: every declared column reads
    for (const auto& c : declared()) {
        INFO("column " << c);
        CHECK(t.column(Day{2020, 1, 1}, c).has_value());
    }
}

TEST_CASE("ATMO-A-018  F10.7 is the previous day's, and F10.7A is computed", "[spaceweather]") {
    const auto t = load();
    const Day d{2020, 6, 15};
    const auto s = t.sample(d);
    REQUIRE(s.has_value());
    const auto prev = t.column(Day{2020, 6, 14}, "F10.7obs");
    REQUIRE(prev.has_value());
    CHECK(s->f107_previous_day == *prev);
    // and NOT the day's own value, which is the off-by-one this guards
    const auto own = t.column(d, "F10.7obs");
    REQUIRE(own.has_value());
    if (*own != *prev) CHECK(s->f107_previous_day != *own);

    // THE CENTRED MEAN, RECOMPUTED BY AN INDEPENDENT ROUTE: civil-date
    // arithmetic here against the loader's index walk.  The point of ATMO-R-019
    // is that this value is COMPUTED and not read from a derived column, so the
    // test computes it too rather than comparing the loader with itself.
    double sum = 0.0;
    int n = 0;
    for (int k = -40; k <= 40; ++k) {
        const auto v = t.column(civil_add(d, k), "F10.7obs");
        REQUIRE(v.has_value());
        sum += *v; ++n;
    }
    REQUIRE(n == 81);
    CHECK_THAT(s->f107a_centred81, Catch::Matchers::WithinRel(sum / 81.0, 1e-15));
    CHECK(s->snapshot_id == "gfz-kp-ap-f107");
}

TEST_CASE("ATMO-A-020  the redistribution verified against the issuing authority",
          "[spaceweather][gate]") {
    auto t = load();
    const auto rep = t.verify_against_issuer(slurp(ODL_DRAO_FLUXTABLE));
    REQUIRE(rep.has_value());
    INFO("overlap " << rep->overlap_days << "  exact " << rep->exact
         << "  differing " << rep->differing << "  worst " << rep->worst_difference_sfu
         << " sfu  duplicates " << rep->duplicate_timestamps);
    CHECK(rep->overlap_days == 7969);
    CHECK(rep->exact == 7969);          // GFZ's F10.7obs IS DRAO's first 20:00 UT reading
    CHECK(rep->differing == 0);
    CHECK(rep->worst_difference_sfu == 0.0);
    CHECK(rep->duplicate_timestamps == 16);   // ATMO-R-033: first-wins, on the 16 that differ
    CHECK(rep->first == Day{2004, 10, 28});
    CHECK(rep->last == Day{2026, 9, 17});
}

TEST_CASE("ATMO-A-021  the verification flag is load-bearing", "[spaceweather]") {
    auto t = load();
    REQUIRE(t.verify_against_issuer(slurp(ODL_DRAO_FLUXTABLE)).has_value());

    // inside the verified window
    const auto after = t.sample(Day{2020, 1, 1});
    REQUIRE(after.has_value());
    CHECK(after->verification == Verification::verified_against_issuer);

    // before it — the flag is SET, which is the half that is easy to get right
    const auto before = t.sample(Day{1990, 1, 1});
    REQUIRE(before.has_value());
    CHECK(before->verification == Verification::unverified_redistribution);

    // AND A CONSUMER CAN REQUIRE VERIFIED INPUTS.  Without this the mark degrades
    // to silence for everyone who does not look.
    const auto demanded = t.sample(Day{1990, 1, 1}, /*require_verified=*/true);
    REQUIRE_FALSE(demanded.has_value());
    CHECK(demanded.error().id == "ATMO-F-015");
    CHECK(t.sample(Day{2020, 1, 1}, true).has_value());
}
