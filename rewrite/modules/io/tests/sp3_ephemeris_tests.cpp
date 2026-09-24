// sp3_ephemeris_tests.cpp — SPEC-io-formats.md §8, IOFM-A-023 through IOFM-A-031.
//
// plan/subplan_L6/L6-1.md, ruled 2026-09-25: SP3 interpolation is one
// tree-wide facility, built here, not re-implemented per tool or by measmod.
// IOFM-A-023 through IOFM-A-029 exercise the interpolator's own MECHANICS
// (exactness at its own nodes, the span/gap/manoeuvre refusals, order
// reduction, the calendar arithmetic) on small hand-built fixtures, every
// refusal query time chosen from a standalone probe of the real window-
// selection logic first (rule 5: shown firing, not assumed). IOFM-A-030/031
// measure real achieved ACCURACY on two real SP3 files read from disk.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/io/sp3.hpp>
#include <odl/io/sp3_ephemeris.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace odl;
using namespace odl::io;
using Catch::Matchers::WithinAbs;

namespace {

/// A small synthetic multi-epoch, single-satellite `Sp3File`: `n` epochs at
/// `interval_s`, starting 2026-01-01T00:00:00, positions along a smooth
/// curve -- not a real orbit. This tests the interpolator's own MECHANICS;
/// IOFM-A-030/031 (real data, below) test achieved accuracy. `skip_indices`
/// omits those epochs entirely, a genuine gap in the file itself, not a
/// point deleted from an otherwise-complete series.
Sp3File synthetic_file(int n, double interval_s, const std::string& sat_id,
                        const std::vector<int>& skip_indices = {}) {
    Sp3File f;
    f.header.pos_vel_flag = Sp3PosVelFlag::Position;
    f.header.start_epoch = time::Calendar{2026, 1, 1, 0, 0, 0.0};
    f.header.epoch_interval_s = interval_s;
    f.header.time_system = Sp3TimeSystem::GPS;
    f.header.satellite_ids = {sat_id};
    f.header.accuracy = {0};
    for (int i = 0; i < n; ++i) {
        bool skip = false;
        for (int s : skip_indices) {
            if (s == i) skip = true;
        }
        if (skip) continue;
        const double t = i * interval_s;
        const int day_add = static_cast<int>(t) / 86400;
        const double sec_of_day = t - day_add * 86400.0;
        Sp3Epoch e;
        e.epoch = time::Calendar{2026, 1, 1 + day_add, 0, 0, sec_of_day};
        Sp3SatelliteRecord sat;
        sat.position.satellite_id = sat_id;
        // a smooth cubic in t, distinct per component -- nothing orbital,
        // just something a low-order polynomial reproduces exactly at its
        // own nodes and smoothly elsewhere.
        sat.position.x_km = 7000.0 + 0.010 * t - 1.0e-7 * t * t;
        sat.position.y_km = 1000.0 - 0.020 * t + 2.0e-7 * t * t;
        sat.position.z_km = -500.0 + 0.005 * t + 3.0e-8 * t * t * t;
        e.satellites.push_back(sat);
        f.epochs.push_back(e);
    }
    f.header.num_epochs = static_cast<int>(f.epochs.size());
    return f;
}

/// IOFM-A-030/031's own real fixtures, read from disk (ODL_IO_TEST_DATA_DIR,
/// modules/io/CMakeLists.txt) rather than carried as a string literal --
/// see that CMakeLists.txt for why.
std::string read_test_data(const char* filename) {
    const char* dir = std::getenv("ODL_IO_TEST_DATA_DIR");
#ifdef ODL_IO_TEST_DATA_DIR
    static const char* const kCompiledDir = ODL_IO_TEST_DATA_DIR;
    if (!dir) dir = kCompiledDir;
#endif
    REQUIRE(dir != nullptr);
    std::ifstream in(std::string(dir) + "/" + filename);
    REQUIRE(in.is_open());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// The real-data holdout methodology (IOFM-A-030/031): every one of
/// `all`'s own points except `hold` is available; an `npoints`-point window
/// centred on `hold` (excluding it) is fit and evaluated AT `hold`'s own
/// time, and the result is compared to `hold`'s own real, known value. This
/// calls `lagrange_interpolate` directly rather than
/// `Sp3Ephemeris::position_km_at` on a file with that epoch deleted --
/// deleting an interior epoch creates a genuine ~2x-interval gap, which
/// `position_km_at` correctly refuses (IOFM-A-025); that refusal answers a
/// different question (should an arbitrary caller trust this span) than the
/// one this test asks (how accurate is the polynomial itself against a
/// point withheld only from the fit, not from the caller's own knowledge).
double holdout_error_km(const std::vector<std::pair<double, Vec3>>& all, std::size_t hold,
                         int npoints) {
    std::vector<std::pair<double, Vec3>> window;
    window.reserve(static_cast<std::size_t>(npoints));
    const long lo = static_cast<long>(hold) - npoints / 2;
    for (long k = lo; window.size() < static_cast<std::size_t>(npoints); ++k) {
        if (k == static_cast<long>(hold)) continue;
        REQUIRE(k >= 0);
        REQUIRE(static_cast<std::size_t>(k) < all.size());
        window.push_back(all[static_cast<std::size_t>(k)]);
    }
    const Vec3 interp = lagrange_interpolate(window, all[hold].first);
    const Vec3& real = all[hold].second;
    return std::sqrt(std::pow(interp.x - real.x, 2) + std::pow(interp.y - real.y, 2) +
                      std::pow(interp.z - real.z, 2));
}

std::vector<std::pair<double, Vec3>> position_series(const Sp3File& f, const std::string& sat_id) {
    std::vector<std::pair<double, Vec3>> series;
    time::Calendar first{};
    bool have_first = false;
    for (const auto& epoch : f.epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.position.satellite_id == sat_id) {
                if (!have_first) {
                    first = epoch.epoch;
                    have_first = true;
                }
                series.emplace_back(calendar_elapsed_seconds(first, epoch.epoch),
                                     Vec3{sat.position.x_km, sat.position.y_km, sat.position.z_km});
            }
        }
    }
    return series;
}

}  // namespace

// --- calendar_elapsed_seconds: pure arithmetic, no LeapTable -----------------

TEST_CASE("IOFM-A-029  calendar_elapsed_seconds: exact calendar-day arithmetic, "
          "including a leap-year February and a year boundary",
          "[io][sp3_ephemeris]") {
    using time::Calendar;
    CHECK(calendar_elapsed_seconds(Calendar{2026, 1, 1, 0, 0, 0.0}, Calendar{2026, 1, 2, 0, 0, 0.0}) ==
          86400.0);
    // 2026 is not a leap year: Feb has 28 days.
    CHECK(calendar_elapsed_seconds(Calendar{2026, 2, 28, 0, 0, 0.0}, Calendar{2026, 3, 1, 0, 0, 0.0}) ==
          86400.0);
    // 2028 IS a leap year: Feb has 29, so the SAME nominal dates are 2 days
    // apart -- proving this reads the real calendar, not a fixed 28/30/31
    // month-length table.
    CHECK(calendar_elapsed_seconds(Calendar{2028, 2, 28, 0, 0, 0.0}, Calendar{2028, 3, 1, 0, 0, 0.0}) ==
          172800.0);
    CHECK(calendar_elapsed_seconds(Calendar{2026, 12, 31, 12, 0, 0.0}, Calendar{2027, 1, 1, 12, 0, 0.0}) ==
          86400.0);
    // sub-minute, same day -- the case every SP3 epoch-to-epoch step is.
    CHECK(calendar_elapsed_seconds(Calendar{2026, 3, 29, 0, 6, 0.0}, Calendar{2026, 3, 29, 0, 7, 0.0}) ==
          60.0);
    CHECK_THAT(calendar_elapsed_seconds(Calendar{2026, 1, 1, 0, 0, 30.5}, Calendar{2026, 1, 1, 0, 1, 0.0}),
               WithinAbs(29.5, 1e-9));
    // negative when `to` precedes `from`, and difference is antisymmetric.
    CHECK(calendar_elapsed_seconds(Calendar{2026, 1, 2, 0, 0, 0.0}, Calendar{2026, 1, 1, 0, 0, 0.0}) ==
          -86400.0);
}

// --- Sp3Ephemeris::build / position_km_at: mechanics, on synthetic fixtures --

TEST_CASE("IOFM-A-023  position_km_at is exact at its own sample nodes",
          "[io][sp3_ephemeris]") {
    const auto file = synthetic_file(15, 60.0, "X01");
    const auto eph = Sp3Ephemeris::build(file, "X01");
    REQUIRE(eph.has_value());
    CHECK(eph->order() == 10);         // target_order's own default, 15-1 >= 10
    CHECK(eph->sample_count() == 15);
    for (int i = 0; i < 15; ++i) {
        const double t = i * 60.0;
        const auto pos = eph->position_km_at(t);
        REQUIRE(pos.has_value());
        const auto& want = file.epochs[static_cast<std::size_t>(i)].satellites[0].position;
        CHECK_THAT(pos->x, WithinAbs(want.x_km, 1e-8));
        CHECK_THAT(pos->y, WithinAbs(want.y_km, 1e-8));
        CHECK_THAT(pos->z, WithinAbs(want.z_km, 1e-8));
    }
}

TEST_CASE("IOFM-A-024  position_km_at refuses outside the sampled span in both "
          "directions (IOFM-F-009) and never extrapolates; the span's own two "
          "endpoints are themselves still valid queries",
          "[io][sp3_ephemeris]") {
    const auto file = synthetic_file(12, 60.0, "X01");
    const auto eph = Sp3Ephemeris::build(file, "X01");
    REQUIRE(eph.has_value());
    const double first = eph->first_sample_seconds();
    const double last = eph->last_sample_seconds();
    CHECK(first == 0.0);
    CHECK(last == 11 * 60.0);

    const auto before = eph->position_km_at(first - 1.0);
    REQUIRE_FALSE(before.has_value());
    CHECK(before.error().id == "IOFM-F-009");

    const auto after = eph->position_km_at(last + 1.0);
    REQUIRE_FALSE(after.has_value());
    CHECK(after.error().id == "IOFM-F-009");

    CHECK(eph->position_km_at(first).has_value());
    CHECK(eph->position_km_at(last).has_value());
}

TEST_CASE("IOFM-A-025  position_km_at refuses when the query's own interpolation "
          "window would span a real gap (IOFM-F-010), and still interpolates "
          "cleanly elsewhere in the SAME file",
          "[io][sp3_ephemeris]") {
    // 13 nominal epochs at 60s; epoch index 6 (t=360s) is genuinely absent --
    // a 120s gap where the file states a 60s nominal interval.
    const auto file = synthetic_file(13, 60.0, "X01", /*skip_indices=*/{6});
    const auto eph = Sp3Ephemeris::build(file, "X01", /*target_order=*/3);  // a 4-point window
    REQUIRE(eph.has_value());
    REQUIRE(eph->order() == 3);
    REQUIRE(eph->sample_count() == 12);

    // Verified directly (a standalone probe of this exact fixture, rule 5):
    // any 4-point centred window for a query in [240s, 450s] needs a point
    // from across the gap at index 6; t=300s is squarely inside that range.
    const auto at_gap = eph->position_km_at(300.0);
    REQUIRE_FALSE(at_gap.has_value());
    CHECK(at_gap.error().id == "IOFM-F-010");

    // Comfortably clear of the gap on either side, the same file succeeds.
    CHECK(eph->position_km_at(60.0).has_value());
    CHECK(eph->position_km_at(600.0).has_value());
}

TEST_CASE("IOFM-A-026  position_km_at refuses when the query's own interpolation "
          "window includes a manoeuvre-flagged sample (IOFM-F-011), and still "
          "interpolates cleanly elsewhere in the SAME file",
          "[io][sp3_ephemeris]") {
    auto file = synthetic_file(13, 60.0, "X01");
    file.epochs[6].satellites[0].position.maneuver = true;  // t=360s flagged
    const auto eph = Sp3Ephemeris::build(file, "X01", /*target_order=*/3);
    REQUIRE(eph.has_value());
    REQUIRE(eph->sample_count() == 13);  // the flagged sample is still a real sample, not dropped

    // Verified directly (the same standalone probe): a 4-point centred
    // window for a query in [240s, 450s] includes index 6's own flagged
    // sample; t=300s is squarely inside that range, same as IOFM-A-025.
    const auto at_maneuver = eph->position_km_at(300.0);
    REQUIRE_FALSE(at_maneuver.has_value());
    CHECK(at_maneuver.error().id == "IOFM-F-011");

    CHECK(eph->position_km_at(60.0).has_value());
    CHECK(eph->position_km_at(600.0).has_value());
}

TEST_CASE("IOFM-A-027  build refuses a satellite with fewer than two samples "
          "(IOFM-F-012), naming the satellite",
          "[io][sp3_ephemeris]") {
    const auto file = synthetic_file(5, 60.0, "X01");

    const auto absent = Sp3Ephemeris::build(file, "Y99");
    REQUIRE_FALSE(absent.has_value());
    CHECK(absent.error().id == "IOFM-F-012");

    const auto one_sample = synthetic_file(1, 60.0, "X01");
    const auto refused = Sp3Ephemeris::build(one_sample, "X01");
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().id == "IOFM-F-012");
}

TEST_CASE("IOFM-A-028  the order actually used is reduced to sample_count-1 when "
          "the file offers fewer than target_order+1 epochs, and interpolation "
          "still succeeds at the reduced order",
          "[io][sp3_ephemeris]") {
    const auto file = synthetic_file(5, 60.0, "X01");  // fewer than 11 points
    const auto eph = Sp3Ephemeris::build(file, "X01", /*target_order=*/10);
    REQUIRE(eph.has_value());
    CHECK(eph->order() == 4);  // 5 samples - 1, not the unreachable target of 10
    CHECK(eph->sample_count() == 5);

    const auto mid = eph->position_km_at(90.0);  // between nodes, not itself a sample
    REQUIRE(mid.has_value());
}

TEST_CASE("IOFM-A-032  central_difference_velocity_km_s matches the synthetic "
          "fixture's own known analytic derivative, and propagates "
          "position_km_at's own refusal near a span boundary",
          "[io][sp3_ephemeris]") {
    const auto file = synthetic_file(15, 60.0, "X01");
    const auto eph = Sp3Ephemeris::build(file, "X01");
    REQUIRE(eph.has_value());

    const double t = 390.0;  // well inside [0, 840], not a sample node
    const auto v = central_difference_velocity_km_s(*eph, t);
    REQUIRE(v.has_value());
    // synthetic_file's own x/y/z(t): the analytic derivative, known exactly.
    const double vx = 0.010 - 2.0 * 1.0e-7 * t;
    const double vy = -0.020 + 4.0 * 1.0e-7 * t;
    const double vz = 0.005 + 3.0 * 3.0e-8 * t * t;
    CHECK_THAT(v->x, WithinAbs(vx, 1e-9));
    CHECK_THAT(v->y, WithinAbs(vy, 1e-9));
    CHECK_THAT(v->z, WithinAbs(vz, 1e-6));  // the cubic term's own h^2 truncation, still tiny

    // Within h_s (default 1 s) of the last sample (840s), one of the two
    // position_km_at probes this needs is itself outside the span --
    // refuses (IOFM-F-009) rather than silently falling back to a one-sided
    // difference that would hide the boundary.
    const auto near_edge = central_difference_velocity_km_s(*eph, eph->last_sample_seconds() - 0.5);
    REQUIRE_FALSE(near_edge.has_value());
    CHECK(near_edge.error().id == "IOFM-F-009");
}

// --- IOFM-A-030/031: real achieved accuracy, held-out real samples -----------

TEST_CASE("IOFM-A-030  real-data holdout accuracy: an 11-point Lagrange fit against "
          "13 real, individually held-out GPS G01 samples (IGS rapid combined "
          "solution, 900s interval) stays under 1 cm, matching Schenewerk (2003) "
          "and Horemuz & Andersson (2006)",
          "[io][sp3_ephemeris][real-data]") {
    const auto text = read_test_data("gnss_g01_20260920_excerpt.sp3");
    const auto file = read_sp3(text);
    REQUIRE(file.has_value());
    const auto series = position_series(*file, "G01");
    REQUIRE(series.size() == 24);

    double max_err_km = 0.0;
    int n_tested = 0;
    for (std::size_t hold = 5; hold + 6 < series.size(); ++hold) {
        const double err_km = holdout_error_km(series, hold, /*npoints=*/11);
        max_err_km = std::max(max_err_km, err_km);
        ++n_tested;
    }
    REQUIRE(n_tested == 13);
    // Measured this round: RMS 1.07 mm, max 1.47 mm over these 13 points --
    // asserted here at 1 cm, a deliberately looser bound than the
    // measurement (rule 7's own "not chosen by whoever is judged by it":
    // the bound is the literature's own sub-cm claim, not the number this
    // specific run happened to produce).
    CHECK(max_err_km < 0.01);
}

TEST_CASE("IOFM-A-031  real-data holdout accuracy: an 11-point Lagrange fit against "
          "13 real, individually held-out Jason-3 L39 samples (GSFC SLR+DORIS "
          "dynamic orbit, 60s interval) stays under 1 cm, matching Zeitlhoefler "
          "et al. (2024)'s own degree-8-to-11 findings for altimetry orbits",
          "[io][sp3_ephemeris][real-data]") {
    const auto text = read_test_data("jason3_l39_20260329_excerpt.sp3");
    const auto file = read_sp3(text);
    REQUIRE(file.has_value());
    const auto series = position_series(*file, "L39");
    REQUIRE(series.size() == 24);

    double max_err_km = 0.0;
    int n_tested = 0;
    for (std::size_t hold = 5; hold + 6 < series.size(); ++hold) {
        const double err_km = holdout_error_km(series, hold, /*npoints=*/11);
        max_err_km = std::max(max_err_km, err_km);
        ++n_tested;
    }
    REQUIRE(n_tested == 13);
    // Measured this round: RMS 2.23 mm, max 4.43 mm over these 13 points --
    // same 1 cm bound as IOFM-A-030, same reasoning.
    CHECK(max_err_km < 0.01);
}
