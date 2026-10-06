// position_tests.cpp — SPEC-measmod.md §8.2 and §8.6: MEAS-A-013 (the SP3 and Horizons tags), -060 … -063 (the ephemeris-position model: the plumbing identities on the
// real vendored table, the frame-code table and the reference point, the finite-difference gate G1 for the position, the builders). The position gate is a pre-registered
// comparison: its criteria are the frozen row of SPEC-measmod §6.2 ("position (linear)"), run ONCE; the test case carries the tag [pregistered], which an unfiltered run of the
// binary would execute (MEAS-A-062 is run alone, its log kept).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/measmod/position.hpp>

#include "measmod_reference.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

using namespace odl::measmod;
using namespace odl::measmod::testing;
namespace ref = odl::measmod::ref;
using odl::Vec3;
using odl::time::Calendar;
using odl::time::Epoch;
using odl::time::TimeScale;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kEarthRotation_rad_s = 7.2921150e-5;

double ulp_of(double x) { return std::nextafter(std::abs(x), std::numeric_limits<double>::infinity()) - std::abs(x); }

/// The first epoch's first record of the real ILRS file, as the builder's inputs.
struct FirstSp3 {
    const odl::io::Sp3Header& header;
    const Calendar& epoch;
    const odl::io::Sp3PositionRecord& record;
};
FirstSp3 first_sp3() {
    const auto& f = real_sp3();
    return FirstSp3{f.header, f.epochs.front().epoch, f.epochs.front().satellites.front().position};
}

Epoch tdb_epoch(const odl::io::HorizonsStateRecord& r) {
    auto e = Epoch::from_calendar(TimeScale::TDB, r.epoch, leaps());
    REQUIRE(e.has_value());
    return *e;
}

TableTrajectory table_trajectory() {
    std::vector<TableTrajectory::Row> rows;
    for (const auto& s : acs3_table().states) rows.push_back({tdb_epoch(s), s.position_km, s.velocity_km_s});
    return TableTrajectory(std::move(rows));
}

}  // namespace

TEST_CASE("MEAS-A-013  the SP3 and Horizons tags: header systems GPS, TAI and UTC resolve through io::to_time_scale (2026-01-03 00:00:00 GPS is 2026-01-03 00:00:19 TAI and "
          "2026-01-02 23:59:42 UTC); GLO, GAL, BDT and QZS refuse MEAS-F-014; the real ILRS file's header (UTC) resolves; a Horizons token other than TDB refuses IOHZ-F-002 unchanged",
          "[measmod][position]") {
    const FirstSp3 first = first_sp3();
    const Calendar day{2026, 1, 3, 0, 0, 0.0};
    odl::io::Sp3Header h = first.header;
    // the real header's own system
    CHECK(h.time_system == odl::io::Sp3TimeSystem::UTC);
    auto real = position_observation(h, first.epoch, first.record, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(real.has_value());
    auto utc_direct = Epoch::from_calendar(TimeScale::UTC, first.epoch, leaps());
    REQUIRE(utc_direct.has_value());
    CHECK(real->epoch == *utc_direct);
    // GPS = TAI - 19 s and UTC = TAI - 37 s in 2026: 2026-01-03 00:00:00 GPS is 00:00:19 TAI and 2026-01-02 23:59:42 UTC — the same instant, three ways
    h.time_system = odl::io::Sp3TimeSystem::GPS;
    auto gps = position_observation(h, day, first.record, ReferencePoint::CentreOfMass, leaps());
    h.time_system = odl::io::Sp3TimeSystem::TAI;
    auto tai = position_observation(h, Calendar{2026, 1, 3, 0, 0, 19.0}, first.record, ReferencePoint::CentreOfMass, leaps());
    h.time_system = odl::io::Sp3TimeSystem::UTC;
    auto utc = position_observation(h, Calendar{2026, 1, 2, 23, 59, 42.0}, first.record, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(gps.has_value());
    REQUIRE(tai.has_value());
    REQUIRE(utc.has_value());
    CHECK(gps->epoch == tai->epoch);
    CHECK(utc->epoch == tai->epoch);
    // the same digits are different instants in different systems: 2026-01-03 00:00:00 UTC is 18 s after 2026-01-03 00:00:00 GPS (UTC = GPS - 18 s)
    h.time_system = odl::io::Sp3TimeSystem::UTC;
    auto utc_same_digits = position_observation(h, day, first.record, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(utc_same_digits.has_value());
    CHECK_THAT(utc_same_digits->epoch.difference(gps->epoch).to_seconds(), WithinAbs(18.0, 1e-9));
    // the other four systems refuse, each naming itself
    for (auto sys : {odl::io::Sp3TimeSystem::GLO, odl::io::Sp3TimeSystem::GAL, odl::io::Sp3TimeSystem::BDT, odl::io::Sp3TimeSystem::QZS}) {
        h.time_system = sys;
        auto r = position_observation(h, day, first.record, ReferencePoint::CentreOfMass, leaps());
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-014");
    }
    // Horizons: TDB resolves; any other token refuses with the reader's own id
    odl::io::HorizonsStateRecord rec = acs3_table().states.front();
    auto ok = position_observation(rec, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(ok.has_value());
    CHECK(ok->epoch == tdb_epoch(rec));
    rec.time_system = odl::io::HorizonsTimeSystem::Ut;
    auto ut = position_observation(rec, ReferencePoint::CentreOfMass, leaps());
    REQUIRE_FALSE(ut.has_value());
    CHECK(ut.error().id == "IOHZ-F-002");
}

TEST_CASE("MEAS-A-060  the plumbing identities on the real vendored Horizons table (5 records; ICRF, geocentre, TDB), never against an SGP4 propagation: a GCRS observation against "
          "the table's own states has residual exactly 0; the ITRS observation built by the frames chain has residual below 1e-9 m; the 69 s defect (the printed times read as UTC) "
          "gives a residual above 100 km against a two-body propagation and is detected; an error of 0.5 s in the Earth's rotation gives the residual 0.5 s x 7.292e-5 rad/s x the "
          "distance from the axis (above 200 m where that exceeds 5.5e6 m)",
          "[measmod][position]") {
    const auto& table = acs3_table();
    REQUIRE(table.states.size() == 5);
    const EopEarthOrientation earth(finals(), predictions_allowed(), leaps());
    const TableTrajectory truth = table_trajectory();

    SECTION("(i) a GCRS observation against the table's own states: the residual is exactly 0") {
        for (const auto& rec : table.states) {
            auto obs = position_observation(rec, ReferencePoint::CentreOfMass, leaps());
            REQUIRE(obs.has_value());
            auto m = model_position(*obs, truth, earth);
            REQUIRE(m.has_value());
            const Vec3 res = m->residual_m();
            CHECK(res.x == 0.0);
            CHECK(res.y == 0.0);
            CHECK(res.z == 0.0);
            CHECK(m->applied().frame == PositionFrame::GCRS);
            CHECK(m->applied().omitted.empty());
        }
    }

    SECTION("(ii) an ITRS observation built from the same state by the frames chain: the residual is below 1e-9 m") {
        for (const auto& rec : table.states) {
            const Epoch t = tdb_epoch(rec);
            auto sample = earth.at(t);
            REQUIRE(sample.has_value());
            const Vec3 r_m = odl::metres_from_km(rec.position_km);
            const PositionObservation obs{t, PositionFrame::ITRS, sample->gcrs_to_itrs.apply(r_m), ReferencePoint::CentreOfMass};
            auto m = model_position(obs, truth, earth);
            REQUIRE(m.has_value());
            CHECK(m->residual_m().norm() < 1e-9);
            CHECK(m->applied().frame == PositionFrame::ITRS);
            REQUIRE(m->applied().omitted.size() == 1);
            CHECK(m->applied().omitted.front().name == "itrs_realisation");
            // the observation is a rotation of the GCRS state: the geocentric distance is unchanged to round-off
            CHECK_THAT(obs.position_m.norm(), WithinRel(r_m.norm(), 1e-14));
        }
    }

    SECTION("(iii) the 69 s defect: the printed times read as UTC instead of TDB") {
        for (const auto& rec : table.states) {
            const Epoch right = tdb_epoch(rec);
            auto wrong_e = Epoch::from_calendar(TimeScale::UTC, rec.epoch, leaps());                 // the defect, built on purpose
            REQUIRE(wrong_e.has_value());
            // the same digits are a LATER instant when read as UTC (the TDB clock runs 69.184 s ahead of UTC in 2026): the instant is TT - UTC = 69.184 s past the right one, TDB - TT under 2 ms
            const double late = wrong_e->difference(right).to_seconds();
            CHECK_THAT(late, WithinAbs(69.184, 0.01));
            const Vec3 r_m = odl::metres_from_km(rec.position_km), v_m = odl::metres_from_km(rec.velocity_km_s);
            const PositionObservation defective{*wrong_e, PositionFrame::GCRS, r_m, ReferencePoint::CentreOfMass};
            // the identity against the table itself cannot be satisfied: the table holds no state at the wrong epoch
            auto against_table = model_position(defective, truth, earth);
            CHECK_FALSE(against_table.has_value());
            // against a two-body propagation from the nearest record (the record itself) the residual is the along-track displacement v x 69 s
            const TwoBodyTrajectory two_body(right, r_m, v_m);
            auto m = model_position(defective, two_body, earth);
            REQUIRE(m.has_value());
            const double residual = m->residual_m().norm();
            CHECK(residual > 100e3);
            CHECK_THAT(residual, WithinRel(v_m.norm() * late, 0.02));
            // and the correctly resolved observation passes the same comparison: the two-body state at its own epoch is the record
            const PositionObservation correct{right, PositionFrame::GCRS, r_m, ReferencePoint::CentreOfMass};
            auto good = model_position(correct, two_body, earth);
            REQUIRE(good.has_value());
            CHECK(good->residual_m().norm() < 1e-6);
        }
    }

    SECTION("(iv) an error of 0.5 s in the Earth's rotation") {
        const OffsetOrientation late(earth, 0.5);
        int above_200_m = 0;
        for (const auto& rec : table.states) {
            const Epoch t = tdb_epoch(rec);
            auto sample = earth.at(t);
            REQUIRE(sample.has_value());
            const Vec3 r_m = odl::metres_from_km(rec.position_km);
            const PositionObservation obs{t, PositionFrame::ITRS, sample->gcrs_to_itrs.apply(r_m), ReferencePoint::CentreOfMass};
            auto m = model_position(obs, truth, late);
            REQUIRE(m.has_value());
            const Vec3 itrs = sample->gcrs_to_itrs.apply(r_m);
            const double axis_distance = std::hypot(itrs.x, itrs.y);
            const double expected = kEarthRotation_rad_s * 0.5 * axis_distance;
            const double residual = m->residual_m().norm();
            WARN(std::setprecision(5) << "distance from the axis " << axis_distance / 1e3 << " km: residual " << residual << " m, expected " << expected << " m");
            CHECK_THAT(residual, WithinRel(expected, 0.02));
            if (axis_distance > 5.5e6) {
                CHECK(residual > 200.0);
                ++above_200_m;
            }
        }
        CHECK(above_200_m >= 1);
    }
}

TEST_CASE("MEAS-A-061  the SP3 frame-code table: IGS05 IGS08 IGb08 IGS14 IGb14 IGS20 IGb20 SLR20 map to ITRS (the real ILRS file's SLR20 included, read from its header); XYZ99, an "
          "empty code and a lower-case code refuse MEAS-F-014; only CentreOfMass is accepted as a reference point; positions are kilometres; an all-zero position (the absent "
          "marker) and a non-finite component refuse MEAS-F-018",
          "[measmod][position]") {
    const FirstSp3 first = first_sp3();
    odl::io::Sp3Header h = first.header;
    const std::vector<std::string> expected = {"IGS05", "IGS08", "IGb08", "IGS14", "IGb14", "IGS20", "IGb20", "SLR20"};
    CHECK(accepted_sp3_frame_codes() == expected);
    // the real file's own code, as its header says it
    CHECK(h.coordinate_sys == "SLR20");
    for (const std::string& code : expected) {
        h.coordinate_sys = code;
        auto o = position_observation(h, first.epoch, first.record, ReferencePoint::CentreOfMass, leaps());
        REQUIRE(o.has_value());
        CHECK(o->frame == PositionFrame::ITRS);
    }
    h.coordinate_sys = "IGS14  ";                                                  // the blanks of a fixed-width field are not part of the code
    CHECK(position_observation(h, first.epoch, first.record, ReferencePoint::CentreOfMass, leaps()).has_value());
    for (const char* code : {"XYZ99", "", "slr20", "igs14", "IGS14X", "WGS84", "ITRF2", "SLR2"}) {
        h.coordinate_sys = code;
        auto o = position_observation(h, first.epoch, first.record, ReferencePoint::CentreOfMass, leaps());
        INFO("code '" << code << "'");
        REQUIRE_FALSE(o.has_value());
        CHECK(o.error().id == "MEAS-F-014");
    }
    h.coordinate_sys = "SLR20";

    // the reference point: both builders
    for (ReferencePoint p : {ReferencePoint::AntennaPhaseCentre, ReferencePoint::Unspecified}) {
        auto a = position_observation(h, first.epoch, first.record, p, leaps());
        REQUIRE_FALSE(a.has_value());
        CHECK(a.error().id == "MEAS-F-014");
        auto b = position_observation(acs3_table().states.front(), p, leaps());
        REQUIRE_FALSE(b.has_value());
        CHECK(b.error().id == "MEAS-F-014");
    }
    // ... and the model, for an observation that was built by hand with another point
    {
        const EopEarthOrientation earth(finals(), predictions_allowed(), leaps());
        const TableTrajectory truth = table_trajectory();
        auto good = position_observation(acs3_table().states.front(), ReferencePoint::CentreOfMass, leaps());
        REQUIRE(good.has_value());
        PositionObservation by_hand = *good;
        by_hand.point = ReferencePoint::AntennaPhaseCentre;
        auto m = model_position(by_hand, truth, earth);
        REQUIRE_FALSE(m.has_value());
        CHECK(m.error().id == "MEAS-F-014");
    }

    // the unit: kilometres in the file, metres in the observation, through the tree's one crossing
    odl::io::Sp3PositionRecord rec = first.record;
    rec.x_km = 1.0;
    rec.y_km = -2.5;
    rec.z_km = 0.001;
    auto units = position_observation(h, first.epoch, rec, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(units.has_value());
    CHECK_THAT(units->position_m.x, WithinAbs(1000.0, 1e-12));
    CHECK_THAT(units->position_m.y, WithinAbs(-2500.0, 1e-12));
    CHECK_THAT(units->position_m.z, WithinAbs(1.0, 1e-12));

    // MEAS-F-018: the format's marker for an absent position, and what is not a number
    rec.x_km = rec.y_km = rec.z_km = 0.0;
    auto absent = position_observation(h, first.epoch, rec, ReferencePoint::CentreOfMass, leaps());
    REQUIRE_FALSE(absent.has_value());
    CHECK(absent.error().id == "MEAS-F-018");
    rec.x_km = 1.0;                                                                 // one zero component is a position: the marker is all three
    CHECK(position_observation(h, first.epoch, rec, ReferencePoint::CentreOfMass, leaps()).has_value());
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        odl::io::Sp3PositionRecord r2 = first.record;
        r2.y_km = bad;
        auto o = position_observation(h, first.epoch, r2, ReferencePoint::CentreOfMass, leaps());
        REQUIRE_FALSE(o.has_value());
        CHECK(o.error().id == "MEAS-F-018");
        odl::io::HorizonsStateRecord hz = acs3_table().states.front();
        hz.position_km.z = bad;
        auto o2 = position_observation(hz, ReferencePoint::CentreOfMass, leaps());
        REQUIRE_FALSE(o2.has_value());
        CHECK(o2.error().id == "MEAS-F-018");
    }
}

TEST_CASE("MEAS-A-063  the builders: a Horizons record is a GCRS observation at its TDB epoch with the position in metres; an SP3 record is an ITRS observation at its header-system "
          "epoch — the real ILRS file's first record (2025-12-28 00:00:00 UTC, PL51 -11319.687869 -4845.099064 -497.695158) is 12 323.069 km from the centre",
          "[measmod][position]") {
    // the vendored table's first record, against the independent decimal conversion of its printed text (tools/measmod_reference.py)
    const auto& hz = acs3_table().states.front();
    auto h = position_observation(hz, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(h.has_value());
    CHECK(h->frame == PositionFrame::GCRS);
    CHECK(h->epoch == tdb_epoch(hz));
    CHECK_THAT(h->position_m.x, WithinAbs(ref::position_horizons_x_m, 2.0 * ulp_of(ref::position_horizons_x_m)));
    CHECK_THAT(h->position_m.y, WithinAbs(ref::position_horizons_y_m, 2.0 * ulp_of(ref::position_horizons_y_m)));
    CHECK_THAT(h->position_m.z, WithinAbs(ref::position_horizons_z_m, 2.0 * ulp_of(ref::position_horizons_z_m)));
    CHECK_THAT(h->position_m.norm(), WithinRel(ref::position_horizons_norm_km * 1e3, 1e-13));
    CHECK_THAT(ref::position_horizons_norm_km, WithinAbs(7321.098, 5e-4));
    // the epoch is the TDB instant, not the UTC reading of the same digits (69 s earlier)
    auto as_utc = Epoch::from_calendar(TimeScale::UTC, hz.epoch, leaps());
    REQUIRE(as_utc.has_value());
    CHECK(h->epoch != *as_utc);
    CHECK_THAT(as_utc->difference(h->epoch).to_seconds(), WithinAbs(69.184, 0.01));

    // the real ILRS file's first record
    const FirstSp3 first = first_sp3();
    CHECK(first.record.satellite_id == "L51");
    CHECK(first.epoch.year == 2025);
    CHECK(first.epoch.month == 12);
    CHECK(first.epoch.day == 28);
    CHECK(first.epoch.hour == 0);
    CHECK(first.epoch.minute == 0);
    CHECK(first.epoch.second == 0.0);
    auto s = position_observation(first.header, first.epoch, first.record, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(s.has_value());
    CHECK(s->frame == PositionFrame::ITRS);
    CHECK_THAT(s->position_m.x, WithinAbs(ref::position_sp3_x_m, 2.0 * ulp_of(ref::position_sp3_x_m)));
    CHECK_THAT(s->position_m.y, WithinAbs(ref::position_sp3_y_m, 2.0 * ulp_of(ref::position_sp3_y_m)));
    CHECK_THAT(s->position_m.z, WithinAbs(ref::position_sp3_z_m, 2.0 * ulp_of(ref::position_sp3_z_m)));
    CHECK_THAT(s->position_m.norm(), WithinRel(ref::position_sp3_norm_km * 1e3, 1e-13));
    CHECK_THAT(ref::position_sp3_norm_km, WithinAbs(12323.069, 5e-4));
    // its epoch is the UTC instant, and not GPS time (UTC = GPS - 18 s in 2025)
    auto utc = Epoch::from_calendar(TimeScale::UTC, Calendar{2025, 12, 28, 0, 0, 0.0}, leaps());
    auto gps = Epoch::from_calendar(TimeScale::GPS, Calendar{2025, 12, 28, 0, 0, 0.0}, leaps());
    REQUIRE(utc.has_value());
    REQUIRE(gps.has_value());
    CHECK(s->epoch == *utc);
    CHECK(s->epoch != *gps);
    CHECK_THAT(gps->difference(*utc).to_seconds(), WithinAbs(-18.0, 1e-9));
    // and the file is what the manifest says: 5040 epochs of 120 s, one satellite, the SLR20 frame, the UTC system
    const auto& f = real_sp3();
    CHECK(f.epochs.size() == 5040);
    CHECK(f.header.num_epochs == 5040);
    CHECK_THAT(f.header.epoch_interval_s, WithinAbs(120.0, 1e-9));
    CHECK(f.header.satellite_ids.size() == 1);
    CHECK(f.header.coordinate_sys == "SLR20");
}

namespace {

// ---- MEAS-A-062: G1 for the position (pre-registered; the frozen row "position (linear)" of SPEC-measmod §6.2) -------------------------------------------------

constexpr std::array<double, 5> kHs = {{10.0, 30.0, 100.0, 300.0, 1000.0}};
constexpr std::array<double, 4> kHvs = {{0.01, 0.1, 1.0, 10.0}};

/// The frozen row's noise bound: three ulp of the largest operand, 1.227e7 m (the LAGEOS-like sizing's), 5.59e-9 m. F = 0 for a linear function, so ε(h) = ν/h.
double nu_position() { return 3.0 * ulp_of(1.227e7); }

struct PositionOutcome {
    double worst_b = 0.0;                 ///< max over (i, k), h of |f̂ − a| / ε
    double worst_c = 0.0;                 ///< max over (i, k), h_v of |f̂_v| / (ν/h_v)
    double band = 0.0;                    ///< max over (i, k) of the spread of the five estimates
    double violation_identity = 0.0;      ///< the same as worst_b, against the identity instead of M
    double violation_transpose = 0.0;     ///< ... against M transposed (the inverse rotation)
    double largest_component = 0.0;
    bool analytic_inside_spread = true;
    bool analytic_velocity_zero = true;
    odl::Mat3 m = odl::Mat3::identity();
};

PositionOutcome position_gate(PositionFrame frame) {
    PositionOutcome o;
    const auto& rec = acs3_table().states.front();
    auto gcrs = position_observation(rec, ReferencePoint::CentreOfMass, leaps());
    REQUIRE(gcrs.has_value());
    const EopEarthOrientation earth(finals(), predictions_allowed(), leaps());
    const Epoch t = gcrs->epoch;
    const Vec3 r0 = gcrs->position_m, v0 = odl::metres_from_km(rec.velocity_km_s);
    const Vec3 a0 = (-3.986004415e14 / std::pow(r0.norm(), 3.0)) * r0;
    PositionObservation obs = *gcrs;
    if (frame == PositionFrame::ITRS) {
        auto sample = earth.at(t);
        REQUIRE(sample.has_value());
        obs.frame = PositionFrame::ITRS;
        obs.position_m = sample->gcrs_to_itrs.apply(r0);
    }
    auto trajectory = [&](const Vec3& dr, const Vec3& dv) { return DriftTrajectory(t, r0, v0, a0, t, dr, dv); };
    auto nominal = model_position(obs, trajectory({}, {}), earth);
    REQUIRE(nominal.has_value());
    o.m = nominal->applied().rotation;
    o.largest_component = std::max({std::abs(r0.x), std::abs(r0.y), std::abs(r0.z), std::abs(obs.position_m.x), std::abs(obs.position_m.y), std::abs(obs.position_m.z)});

    auto y = [&](const Vec3& dr, const Vec3& dv) {
        auto m = model_position(obs, trajectory(dr, dv), earth);
        REQUIRE(m.has_value());
        return m->position_m();
    };
    auto component = [](const Vec3& v, std::size_t i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); };
    const Vec3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const double nu = nu_position();
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t k = 0; k < 3; ++k) {
            const double a = nominal->partials().d[i][k];
            for (std::size_t c = 3; c < 6; ++c)
                if (nominal->partials().d[i][c] != 0.0) o.analytic_velocity_zero = false;
            const double identity = i == k ? 1.0 : 0.0, transpose = o.m.r[k][i];
            double lo = std::numeric_limits<double>::infinity(), hi = -lo;
            for (std::size_t s = 0; s < kHs.size(); ++s) {
                const double h = kHs[s], eps = nu / h;
                const double f = (component(y(h * e[k], {}), i) - component(y(-h * e[k], {}), i)) / (2.0 * h);
                lo = std::min(lo, f);
                hi = std::max(hi, f);
                o.worst_b = std::max(o.worst_b, std::abs(f - a) / eps);
                o.violation_identity = std::max(o.violation_identity, std::abs(f - identity) / eps);
                o.violation_transpose = std::max(o.violation_transpose, std::abs(f - transpose) / eps);
            }
            o.band = std::max(o.band, hi - lo);
            if (a < lo || a > hi) o.analytic_inside_spread = false;
            for (double hv : kHvs) {
                const double f = (component(y({}, hv * e[k]), i) - component(y({}, -hv * e[k]), i)) / (2.0 * hv);
                o.worst_c = std::max(o.worst_c, std::abs(f) / (nu / hv));
            }
        }
    }
    return o;
}

void check_position_gate(PositionFrame frame) {
    const PositionOutcome o = position_gate(frame);
    const double nu = nu_position(), b_pred = nu / kHs.front();                      // B_pred = max eps = nu / 10
    WARN(std::setprecision(4) << (frame == PositionFrame::GCRS ? "GCRS" : "ITRS") << " observation: band B = " << o.band << " (B_pred " << b_pred << ", upper bound " << 10.0 * b_pred
                              << "); worst |f̂ − a|/ε = " << o.worst_b << "; worst |f̂_v|/(ν/h_v) = " << o.worst_c << "; analytic row inside the raw spread: "
                              << (o.analytic_inside_spread ? "yes" : "no") << "; violation of (b) by the identity " << o.violation_identity << " ε, by M transposed " << o.violation_transpose
                              << " ε; largest component " << o.largest_component << " m (three ulp " << 3.0 * ulp_of(o.largest_component) << ")");
    // the frozen noise bound bounds three ulp of the largest operand of this state, in either frame
    CHECK(3.0 * ulp_of(o.largest_component) <= nu * 1.005);
    CHECK_THAT(nu, WithinRel(5.59e-9, 2e-3));                                         // the frozen figure, as printed in §6.2
    // (a) the position row has only its upper half: a linear function has no truncation band to be degenerate
    CHECK(o.band <= 10.0 * b_pred);
    // (b) the analytic row [M 0] lies within the first-principles error (noise only) of every estimate
    CHECK(o.worst_b <= 1.0);
    // (c) the velocity columns are exactly zero, and every estimate is within the noise bound
    CHECK(o.analytic_velocity_zero);
    CHECK(o.worst_c <= 1.0);
    // the gate can fail (rule 5): a row without M — the identity — fails (b) by at least 1e6 epsilon in the ITRS case, where M is a rotation by hours of the Earth's turn
    if (frame == PositionFrame::ITRS) {
        CHECK(o.violation_identity >= 1e6);
        CHECK(o.violation_transpose >= 1e6);                                          // and so does the inverse rotation
    } else {
        CHECK(o.violation_identity <= 1.0);                                           // in GCRS the identity IS the row
    }
}

}  // namespace

TEST_CASE("MEAS-A-062  G1, position: the row [M 0] against finite differences of the model, a linear function whose estimates carry only noise, for the GCRS and for the ITRS "
          "observation: (a) upper half, (b) |f - a| <= nu/h at every size, (c) the velocity columns exactly zero; a row without M fails (b) by at least 1e6 epsilon",
          "[measmod][gate][position][pregistered]") {
    SECTION("the GCRS observation") { check_position_gate(PositionFrame::GCRS); }
    SECTION("the ITRS observation, through the real Earth-orientation chain") { check_position_gate(PositionFrame::ITRS); }
}
