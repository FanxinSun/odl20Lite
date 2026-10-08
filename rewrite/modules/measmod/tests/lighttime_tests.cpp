// lighttime_tests.cpp — SPEC-measmod.md §8.4, MEAS-A-030 … MEAS-A-033 (G3 and the light-time iteration).
//
// G3: a closed-form two-way light time, target AND station in uniform motion, for both epoch events, to 1e-8 m. The expected values are
// tools/measmod_reference.cpp's: the two quadratics (c^2 - v^2) tau^2 -/+ 2 (D.v) tau - D^2 = 0 solved in 60 digits, derived independently of the
// fixed-point iteration they test.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/eop/series.hpp>
#include <odl/measmod/lighttime.hpp>
#include <odl/measmod/shapiro.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include "measmod_reference.hpp"
#include "test_tracks.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using odl::Vec3;
using odl::time::Calendar;
using odl::time::Epoch;
using odl::time::TimeScale;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

std::string slurp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const odl::time::LeapTable& leaps() {
    static const odl::time::LeapTable t = [] {
        auto r = odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), {"iers-leap-seconds", "", ""});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

Epoch tag_epoch() {
    auto e = Epoch::from_calendar(TimeScale::UTC, Calendar{2026, 1, 1, 2, 8, 0.0}, leaps());
    REQUIRE(e.has_value());
    return *e;
}

struct Geometry { Vec3 s0, vs, r0, vr; double tau_u2, tau_d2, tof2, range2, tau_u1, tau_d1, tof1, range1; const char* name; };

std::vector<Geometry> geometries() {
    return {
        {{ref::lt_lageos_s0_x, ref::lt_lageos_s0_y, ref::lt_lageos_s0_z}, {ref::lt_lageos_vs_x, ref::lt_lageos_vs_y, ref::lt_lageos_vs_z},
         {ref::lt_lageos_r0_x, ref::lt_lageos_r0_y, ref::lt_lageos_r0_z}, {ref::lt_lageos_vr_x, ref::lt_lageos_vr_y, ref::lt_lageos_vr_z},
         ref::lt_lageos_e2_tau_u_s, ref::lt_lageos_e2_tau_d_s, ref::lt_lageos_e2_tof_s, ref::lt_lageos_e2_range_m,
         ref::lt_lageos_e1_tau_u_s, ref::lt_lageos_e1_tau_d_s, ref::lt_lageos_e1_tof_s, ref::lt_lageos_e1_range_m, "LAGEOS-like"},
        {{ref::lt_leo_s0_x, ref::lt_leo_s0_y, ref::lt_leo_s0_z}, {ref::lt_leo_vs_x, ref::lt_leo_vs_y, ref::lt_leo_vs_z},
         {ref::lt_leo_r0_x, ref::lt_leo_r0_y, ref::lt_leo_r0_z}, {ref::lt_leo_vr_x, ref::lt_leo_vr_y, ref::lt_leo_vr_z},
         ref::lt_leo_e2_tau_u_s, ref::lt_leo_e2_tau_d_s, ref::lt_leo_e2_tof_s, ref::lt_leo_e2_range_m,
         ref::lt_leo_e1_tau_u_s, ref::lt_leo_e1_tau_d_s, ref::lt_leo_e1_tof_s, ref::lt_leo_e1_range_m, "LEO-like"},
    };
}

constexpr double c = kSpeedOfLight_m_s;
constexpr double kPi = 3.14159265358979323846;

}  // namespace

TEST_CASE("MEAS-A-030  G3, event 2: the light time with target AND station in uniform motion equals the closed form to 1e-8 m of range; a "
          "station taken at rest is shown failing",
          "[measmod][lighttime][gate]") {
    const Epoch t0 = tag_epoch();
    for (const Geometry& g : geometries()) {
        INFO(g.name);
        UniformStation station(t0, g.s0, g.vs);
        LinearTrajectory target(t0, g.r0, g.vr);
        auto sol = solve_two_way(EpochEvent::GroundTransmit, t0, station, target, no_extra_path());
        REQUIRE(sol.has_value());
        const double range = 0.5 * c * sol->time_of_flight_s;
        CHECK_THAT(range, WithinAbs(g.range2, 1e-8));                                  // the gate: a few ulp of the range
        CHECK_THAT(sol->up.light_time_s, WithinAbs(g.tau_u2, 3e-17));
        CHECK_THAT(sol->down.light_time_s, WithinAbs(g.tau_d2, 3e-17));
        CHECK(sol->transmit == t0);                                                    // event 2: the tag is the transmit time
        CHECK_THAT(sol->bounce.difference(t0).to_seconds(), WithinAbs(g.tau_u2, 3e-17));
        CHECK_THAT(sol->receive.difference(sol->bounce).to_seconds(), WithinAbs(g.tau_d2, 3e-17));
        CHECK(sol->up.passes <= 5);                                                    // the contraction factor v/c ~ 2e-5: a handful of passes
        CHECK(sol->down.passes <= 5);
        // RULE 5: a model that ignored the station's motion (the station at rest at the transmit point) misses by metres, and the same assertion fails
        UniformStation at_rest(t0, g.s0, Vec3{});
        auto frozen = solve_two_way(EpochEvent::GroundTransmit, t0, at_rest, target, no_extra_path());
        REQUIRE(frozen.has_value());
        const double frozen_range = 0.5 * c * frozen->time_of_flight_s;
        CHECK(std::abs(frozen_range - g.range2) > 0.1);
        CHECK_FALSE(std::abs(frozen_range - g.range2) <= 1e-8);
    }
}

TEST_CASE("MEAS-A-031  G3, event 1 (the tag is the bounce): the up leg solved backwards for a moving station equals its closed form to 1e-8 m; "
          "the two events are different quantities and each matches its own closed form",
          "[measmod][lighttime][gate]") {
    const Epoch t0 = tag_epoch();
    for (const Geometry& g : geometries()) {
        INFO(g.name);
        UniformStation station(t0, g.s0, g.vs);
        LinearTrajectory target(t0, g.r0, g.vr);
        auto e1 = solve_two_way(EpochEvent::Bounce, t0, station, target, no_extra_path());
        REQUIRE(e1.has_value());
        CHECK_THAT(0.5 * c * e1->time_of_flight_s, WithinAbs(g.range1, 1e-8));
        CHECK_THAT(e1->up.light_time_s, WithinAbs(g.tau_u1, 3e-17));
        CHECK_THAT(e1->down.light_time_s, WithinAbs(g.tau_d1, 3e-17));
        CHECK(e1->bounce == t0);                                                       // event 1: the tag is the bounce time
        CHECK_THAT(t0.difference(e1->transmit).to_seconds(), WithinAbs(g.tau_u1, 3e-17));
        CHECK_THAT(e1->receive.difference(t0).to_seconds(), WithinAbs(g.tau_d1, 3e-17));
        // the same tag read as the other event is a different range (by decimetres here: the target and the station move during the up leg)
        auto e2 = solve_two_way(EpochEvent::GroundTransmit, t0, station, target, no_extra_path());
        REQUIRE(e2.has_value());
        CHECK(std::abs(0.5 * c * (e1->time_of_flight_s - e2->time_of_flight_s)) > 0.01);
        // and with the wrong event the closed form of the OTHER event is missed
        CHECK_FALSE(std::abs(0.5 * c * e2->time_of_flight_s - g.range1) <= 1e-8);
    }
}

TEST_CASE("MEAS-A-032  the Earth-fixed station through the real chain: the light time agrees with an independent bisection solver that uses the "
          "same station track; the Earth's rotation between transmit and receive is in the model — freezing the station is shown failing",
          "[measmod][lighttime]") {
    const Epoch t0 = tag_epoch();
    auto series = odl::eop::EopSeries::load_c04(slurp(ODL_C04_FILE), odl::eop::EopProvenance{"eop-c04-20", "", "", ""}, leaps());
    REQUIRE(series.has_value());
    const EopEarthOrientation orientation(*series, odl::eop::EopPolicy{}, leaps());
    const Vec3 srp_itrs{ref::yarl_srp_x_m, ref::yarl_srp_y_m, ref::yarl_srp_z_m};
    const EarthFixedStation station(srp_itrs, Geodetic{ref::yarl_srp_lat_rad, ref::yarl_srp_lon_rad, ref::yarl_srp_h_m}, orientation);
    auto s0 = station.at(t0);
    REQUIRE(s0.has_value());
    // the station's GCRS state is the real chain's: |s| is the geocentric radius, |v| the Earth-rotation speed at this latitude, û a unit vector
    CHECK_THAT(s0->position_m.norm(), WithinAbs(std::hypot(std::hypot(ref::yarl_srp_x_m, ref::yarl_srp_y_m), ref::yarl_srp_z_m), 1e-6));
    CHECK_THAT(s0->up.norm(), WithinAbs(1.0, 1e-14));
    CHECK_THAT(s0->velocity_m_s.norm(), WithinAbs(7.292115e-5 * std::hypot(ref::yarl_srp_x_m, ref::yarl_srp_y_m), 0.01));   // omega x s, |v| = omega * equatorial distance
    CHECK_THAT(s0->velocity_m_s.dot(s0->up), WithinAbs(0.0, 0.05));                    // the surface velocity is nearly horizontal
    // a target 40 degrees above the horizon toward the east (the station's velocity direction), 6.6e6 m away, moving across the line of sight
    const Vec3 east = (1.0 / s0->velocity_m_s.norm()) * s0->velocity_m_s;
    const Vec3 ghat = std::cos(40.0 * kPi / 180.0) * east + std::sin(40.0 * kPi / 180.0) * s0->up;
    const Vec3 r0 = s0->position_m + 6.6e6 * ghat;
    const Vec3 across = (1.0 / s0->up.cross(east).norm()) * s0->up.cross(east);
    const Vec3 vr = 5700.0 * across;
    LinearTrajectory target(t0, r0, vr);
    auto sol = solve_two_way(EpochEvent::GroundTransmit, t0, station, target, no_extra_path());
    REQUIRE(sol.has_value());

    // the independent solver: bisection on c tau - |r(t_t + tau) - s(t_t)| (up) and c tau - |r_b - s(t_b + tau)| (down), same station and target objects
    auto bisect = [](auto f) {
        double lo = 0.0, hi = 1.0;
        for (int i = 0; i < 200; ++i) {
            const double mid = 0.5 * (lo + hi);
            (f(mid) > 0.0 ? hi : lo) = mid;
        }
        return 0.5 * (lo + hi);
    };
    auto pos = [&](const Epoch& t) { return odl::metres_from_km(target.state_at(t)->position()); };
    const double tau_u = bisect([&](double tau) { return c * tau - (pos(t0.add(odl::time::Duration::from_seconds(tau))) - s0->position_m).norm(); });
    const Epoch t_b = t0.add(odl::time::Duration::from_seconds(tau_u));
    const Vec3 r_b = pos(t_b);
    const double tau_d = bisect([&](double tau) { return c * tau - (r_b - station.at(t_b.add(odl::time::Duration::from_seconds(tau)))->position_m).norm(); });
    CHECK_THAT(0.5 * c * sol->time_of_flight_s, WithinAbs(0.5 * c * (tau_u + tau_d), 1e-9));
    CHECK_THAT(sol->up.light_time_s, WithinAbs(tau_u, 1e-17));
    CHECK_THAT(sol->down.light_time_s, WithinAbs(tau_d, 1e-17));

    // the Earth's rotation between transmit and receive is in the model: with the station FROZEN at the transmit epoch (the caller-side mistake)
    // the range changes by metres (predicted 0.35 km/s x 22 ms = 7.7 m for the eastward component), and the assertion of the first check fails for it
    UniformStation frozen_station(t0, s0->position_m, Vec3{});
    auto frozen = solve_two_way(EpochEvent::GroundTransmit, t0, frozen_station, target, no_extra_path());
    REQUIRE(frozen.has_value());
    const double effect = std::abs(0.5 * c * (frozen->time_of_flight_s - sol->time_of_flight_s));
    CHECK(effect > 1.0);
    CHECK(effect < 20.0);                                                              // 0.46 km/s x 44 ms is the most it can be: 20.5 m (MEAS-P-3)
    CHECK_FALSE(effect <= 1e-9);
    // the receive epoch is later than the bounce by the down leg; the station moved by v tau_d between the two ends
    const double moved = (station.at(sol->receive)->position_m - station.at(sol->bounce)->position_m).norm();
    CHECK_THAT(moved, WithinAbs(s0->velocity_m_s.norm() * sol->down.light_time_s, 1e-3));
}

TEST_CASE("MEAS-A-033  the light-time iteration: an ordinary leg converges in at most 5 passes; a target moving away at 1.2 c refuses MEAS-F-011 "
          "after 20 passes, naming the leg, the passes and the last change; a trajectory or a station that refuses mid-iteration returns its own id",
          "[measmod][lighttime]") {
    const Epoch t0 = tag_epoch();
    const Geometry g = geometries().front();
    UniformStation station(t0, g.s0, g.vs);
    {
        LinearTrajectory target(t0, g.r0, g.vr);
        auto ok = solve_two_way(EpochEvent::GroundTransmit, t0, station, target, no_extra_path());
        REQUIRE(ok.has_value());
        CHECK(ok->up.passes <= 5);
        CHECK(ok->down.passes <= 5);
        CHECK(ok->up.passes >= 2);                                                     // the geometric range is not already the fixed point
    }
    // a target receding along the line of sight at 1.2 c: |D + v tau| = c tau has no root (the pulse never catches it) and the map's slope is 1.2
    const Vec3 d = g.r0 - g.s0;
    const Vec3 away = (1.2 * c / d.norm()) * d;
    LinearTrajectory superluminal(t0, g.r0, away);
    for (EpochEvent ev : {EpochEvent::GroundTransmit, EpochEvent::Bounce}) {
        auto r = solve_two_way(ev, t0, station, superluminal, no_extra_path());
        INFO((ev == EpochEvent::GroundTransmit ? "event 2" : "event 1"));
        if (ev == EpochEvent::GroundTransmit) {
            REQUIRE_FALSE(r.has_value());
            CHECK(r.error().id == "MEAS-F-011");
            CHECK_THAT(r.error().message, ContainsSubstring("up leg"));
            CHECK_THAT(r.error().message, ContainsSubstring("20 passes"));
            CHECK_THAT(r.error().message, ContainsSubstring("last change"));
        } else {
            // event 1: the target is evaluated once at the bounce, so the (uniform-motion) up leg is fine; the station is at rest relative to light
            CHECK(r.has_value());
        }
    }
    // the pass limit is a parameter: with a limit of 1 an ordinary leg that needs 2 passes refuses, with the same id
    {
        LinearTrajectory target(t0, g.r0, g.vr);
        auto r = solve_two_way(EpochEvent::GroundTransmit, t0, station, target, no_extra_path(), 1);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-011");
        CHECK_THAT(r.error().message, ContainsSubstring("1 passes"));
    }
    // a station that moves at 1.2 c (its own motion makes the down leg non-contracting)
    UniformStation fast_station(t0, g.s0, (1.2 * c / d.norm()) * (g.r0 - g.s0));
    LinearTrajectory still(t0, g.r0, Vec3{});
    auto sr = solve_two_way(EpochEvent::Bounce, t0, fast_station, still, no_extra_path());
    REQUIRE_FALSE(sr.has_value());
    CHECK(sr.error().id == "MEAS-F-011");
    // refusals of the target and of the station come back with their OWN ids and this module's context
    RefusingTrajectory refusing_target(t0, g.r0, g.vr, 0.001);                        // holds nothing 1 ms past the tag: the bounce is 19 ms later
    auto rt = solve_two_way(EpochEvent::GroundTransmit, t0, station, refusing_target, no_extra_path());
    REQUIRE_FALSE(rt.has_value());
    CHECK(rt.error().id == "TRAJ-F-999");
    CHECK_THAT(rt.error().message, ContainsSubstring("range model"));
    RefusingStation refusing_station(t0, g.s0, g.vs, 0.030);                          // fine for the up leg and the bounce, refuses at the receive epoch (38 ms)
    LinearTrajectory target(t0, g.r0, g.vr);
    auto rs = solve_two_way(EpochEvent::GroundTransmit, t0, refusing_station, target, no_extra_path());
    REQUIRE_FALSE(rs.has_value());
    CHECK(rs.error().id == "STN-F-999");
    CHECK_THAT(rs.error().message, ContainsSubstring("down leg"));
}
