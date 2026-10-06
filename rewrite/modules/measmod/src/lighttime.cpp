// SPEC-measmod.md §4.5 — the two-way light time.

#include <odl/measmod/lighttime.hpp>

#include <odl/core/units.hpp>
#include <odl/measmod/shapiro.hpp>   // kSpeedOfLight_m_s

#include <cmath>
#include <limits>
#include <sstream>

namespace odl::measmod {

using odl::time::Duration;
using odl::time::Epoch;

MeasError with_context(const MeasError& error, const std::string& context) {
    return MeasError{error.id, context + ": " + error.message};
}

LegExtraFn no_extra_path() {
    return [](const LegGeometry&) -> odl::Result<LegExtra, MeasError> { return LegExtra{}; };
}

namespace {

double ulp_of(double x) noexcept {
    const double a = std::abs(x);
    return std::nextafter(a, std::numeric_limits<double>::infinity()) - a;
}

LegGeometry make_geometry(const StationKinematics& s, const odl::Vec3& target_m) {
    LegGeometry g;
    g.station_m = s.position_m;
    g.target_m = target_m;
    g.station_velocity_m_s = s.velocity_m_s;
    g.up = s.up;
    g.up_rate_per_s = s.up_rate_per_s;
    const odl::Vec3 d = target_m - s.position_m;
    g.range_m = d.norm();
    g.direction = (1.0 / g.range_m) * d;
    g.sin_elevation = s.up.dot(g.direction);
    return g;
}

/// One leg. `station_epoch_of(tau)` is the epoch at which the station is asked for its position when the leg's duration is `tau`.
template <class StationEpoch>
odl::Result<SolvedLeg, MeasError> solve_leg(const char* name, const StationTrack& station, const odl::Vec3& target_m, const LegExtraFn& extra,
                                            double tau0, StationEpoch station_epoch_of, int pass_limit) {
    const double c = kSpeedOfLight_m_s;
    double tau = tau0;
    double last_change = std::numeric_limits<double>::infinity();
    for (int pass = 1; pass <= pass_limit; ++pass) {
        const Epoch t = station_epoch_of(tau);
        auto s = station.at(t);
        if (!s) return odl::err(with_context(s.error(), std::string("range model, ") + name + " leg"));
        SolvedLeg leg(t);
        leg.geometry = make_geometry(*s, target_m);
        auto x = extra(leg.geometry);
        if (!x) return odl::err(with_context(x.error(), std::string("range model, ") + name + " leg"));
        leg.extra = *x;
        const double tau_new = (leg.geometry.range_m + leg.extra.delay_m) / c;
        last_change = std::abs(tau_new - tau);
        tau = tau_new;
        if (last_change <= 4.0 * ulp_of(tau_new)) {
            leg.light_time_s = tau_new;
            leg.passes = pass;
            leg.a = leg.geometry.direction + leg.extra.gradient_target;
            leg.q = -leg.geometry.direction.dot(leg.geometry.station_velocity_m_s) + leg.extra.d_dt_station;
            return leg;
        }
    }
    std::ostringstream m;
    m << "the " << name << " leg of the light time has not reached its fixed point in " << pass_limit << " passes (last change " << last_change
      << " s against a stopping rule of 4 ulp of the duration, " << tau << " s): the iteration is not contracting — a target or station moving at "
         "or beyond the speed of light? — and the last iterate is not returned";
    return odl::err("MEAS-F-011", m.str());
}

}  // namespace

odl::Vec3 range_partial_position(const TwoWay& s, EpochEvent event) {
    const double c = kSpeedOfLight_m_s;
    const odl::Vec3& a_u = s.up.a;
    const odl::Vec3& a_d = s.down.a;
    if (event == EpochEvent::GroundTransmit) {
        const double den = c - a_u.dot(s.target_velocity_m_s);
        const odl::Vec3 g_t = (1.0 / den) * a_u;                                                              // ∂t_b/∂r
        const odl::Vec3 dtr = (1.0 / (c - s.down.q)) * (a_d + (a_d.dot(s.target_velocity_m_s) + c) * g_t);   // ∂t_r/∂r
        return (0.5 * c) * dtr;
    }
    return (0.5 * c) * ((1.0 / (c - s.down.q)) * a_d + (1.0 / (c + s.up.q)) * a_u);
}

odl::Result<TwoWay, MeasError> solve_two_way(EpochEvent event, const Epoch& tag, const StationTrack& station, const Trajectory& target,
                                             const LegExtraFn& extra, int pass_limit) {
    const double c = kSpeedOfLight_m_s;
    auto state_at = [&](const Epoch& t, const char* what) -> odl::Result<odl::frames::GcrsState, MeasError> {
        auto s = target.state_at(t);
        if (!s) return odl::err(with_context(s.error(), std::string("range model, the target at the ") + what));
        return *s;
    };

    // ---- the up leg and the bounce ----------------------------------------------------------------------------------
    std::optional<SolvedLeg> up;
    std::optional<Epoch> t_t, t_b;
    odl::Vec3 r_b, v_b;
    if (event == EpochEvent::GroundTransmit) {
        // the tag is the transmit time t_t: the station is where it is at the tag; the target at the bounce t_t + τ_u is what the iteration finds
        auto s0 = station.at(tag);
        if (!s0) return odl::err(with_context(s0.error(), "range model, the station at the transmit epoch"));
        auto r0 = state_at(tag, "tag epoch");
        if (!r0) return odl::err(r0.error());
        double tau = (odl::metres_from_km(r0->position()) - s0->position_m).norm() / c;
        double last_change = std::numeric_limits<double>::infinity();
        for (int pass = 1; pass <= pass_limit && !up; ++pass) {
            const Epoch bounce = tag.add(Duration::from_seconds(tau));
            auto rb = state_at(bounce, "bounce epoch");
            if (!rb) return odl::err(rb.error());
            SolvedLeg leg(tag);
            leg.geometry = make_geometry(*s0, odl::metres_from_km(rb->position()));
            auto x = extra(leg.geometry);
            if (!x) return odl::err(with_context(x.error(), "range model, up leg"));
            leg.extra = *x;
            const double tau_new = (leg.geometry.range_m + leg.extra.delay_m) / c;
            last_change = std::abs(tau_new - tau);
            tau = tau_new;
            if (last_change <= 4.0 * ulp_of(tau_new)) {
                leg.light_time_s = tau_new;
                leg.passes = pass;
                leg.a = leg.geometry.direction + leg.extra.gradient_target;
                leg.q = -leg.geometry.direction.dot(leg.geometry.station_velocity_m_s) + leg.extra.d_dt_station;
                t_t = tag;
                t_b = bounce;
                r_b = leg.geometry.target_m;
                v_b = odl::metres_from_km(rb->velocity());
                up = std::move(leg);
            }
        }
        if (!up) {
            std::ostringstream m;
            m << "the up leg of the light time has not reached its fixed point in " << pass_limit << " passes (last change " << last_change
              << " s against a stopping rule of 4 ulp of the duration, " << tau << " s): the iteration is not contracting — a target or station moving at "
                 "or beyond the speed of light? — and the last iterate is not returned";
            return odl::err("MEAS-F-011", m.str());
        }
    } else {
        // the tag is the bounce time t_b: the target is where it is at the tag; the transmit epoch t_b − τ_u is what the iteration finds
        auto rb = state_at(tag, "tag epoch");
        if (!rb) return odl::err(rb.error());
        r_b = odl::metres_from_km(rb->position());
        v_b = odl::metres_from_km(rb->velocity());
        auto sb = station.at(tag);
        if (!sb) return odl::err(with_context(sb.error(), "range model, the station at the bounce epoch"));
        const double tau0 = (r_b - sb->position_m).norm() / c;
        auto leg = solve_leg("up", station, r_b, extra, tau0, [&](double tau) { return tag.add(Duration::from_seconds(-tau)); }, pass_limit);
        if (!leg) return odl::err(leg.error());
        t_t = leg->station_epoch;
        t_b = tag;
        up = std::move(*leg);
    }

    // ---- the down leg ------------------------------------------------------------------------------------------------
    auto sb = station.at(*t_b);
    if (!sb) return odl::err(with_context(sb.error(), "range model, the station at the bounce epoch"));
    const double tau0_down = (r_b - sb->position_m).norm() / c;
    const Epoch bounce = *t_b;
    auto down = solve_leg("down", station, r_b, extra, tau0_down, [&](double tau) { return bounce.add(Duration::from_seconds(tau)); }, pass_limit);
    if (!down) return odl::err(down.error());
    const double tof = up->light_time_s + down->light_time_s;
    const Epoch receive = down->station_epoch;
    return TwoWay(*t_t, *t_b, receive, std::move(*up), std::move(*down), r_b, v_b, tof);
}

}  // namespace odl::measmod
