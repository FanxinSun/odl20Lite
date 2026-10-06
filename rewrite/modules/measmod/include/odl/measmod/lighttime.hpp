#pragma once
// odl/measmod/lighttime.hpp — the two-way light time, solved to the double-precision fixed point.
//
// SPEC-measmod.md §4.5 (MEAS-R-030, -R-031, -R-033), §7 (MEAS-F-011), §8.4 (G3).
//
// A laser pulse leaves the station's reference point at t_t, is reflected by the target at t_b and returns at t_r:
//     c (t_b − t_t) = ρ_u + Δ_u        c (t_r − t_b) = ρ_d + Δ_d
// with ρ the geometric distance between the station at the leg's STATION epoch and the target at the bounce, and Δ the leg's
// extra path (atmosphere plus Shapiro, supplied by the caller as a function so that a closed-form test passes none). All times
// are TT-compatible seconds and all lengths TT-compatible metres, so c is the defining 299 792 458 m s⁻¹ and no scale factor is
// applied (§3.1). The Earth's rotation between transmit and receive is in the model because the station is asked for its
// position at each leg's own epoch.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/measmod/tracks.hpp>
#include <odl/time/epoch.hpp>

#include <functional>

namespace odl::measmod {

/// Which instant a CRD normal point's epoch tag names (`CRD2` record 11 field 5: "currently, only 1 and 2 are used for laser
/// ranging"): the spacecraft bounce time or the ground transmit time. The real files use 2.
enum class EpochEvent { Bounce = 1, GroundTransmit = 2 };

/// What a leg's extra-path function sees.
struct LegGeometry {
    odl::Vec3 station_m;
    odl::Vec3 target_m;
    odl::Vec3 station_velocity_m_s;
    odl::Vec3 up;
    odl::Vec3 up_rate_per_s;
    double range_m = 0.0;              ///< |target − station|
    odl::Vec3 direction;               ///< ĝ: the unit vector from the station to the target
    double sin_elevation = 0.0;        ///< û · ĝ
};

/// A leg's extra path length beyond the geometric range, and what the partials need of it (MEAS-R-060): its gradient with
/// respect to the TARGET's position at a fixed station epoch, and its derivative with respect to the STATION's epoch at a
/// fixed target position.
struct LegExtra {
    double delay_m = 0.0;
    odl::Vec3 gradient_target;
    double d_dt_station = 0.0;
};

using LegExtraFn = std::function<odl::Result<LegExtra, MeasError>(const LegGeometry&)>;

/// A leg that returns nothing extra: the closed-form tests' leg.
[[nodiscard]] LegExtraFn no_extra_path();

struct SolvedLeg {
    explicit SolvedLeg(const odl::time::Epoch& station_epoch_) noexcept : station_epoch(station_epoch_) {}   // an Epoch has no default
    odl::time::Epoch station_epoch;    ///< t_t (the up leg) or t_r (the down leg)
    double light_time_s = 0.0;         ///< τ = (ρ + Δ)/c
    LegGeometry geometry;
    LegExtra extra;
    int passes = 0;                    ///< fixed-point passes taken
    /// ∂(ρ + Δ)/∂(target position) at a fixed station epoch: ĝ + extra.gradient_target.
    odl::Vec3 a;
    /// ∂(ρ + Δ)/∂(station epoch) at a fixed target position: −(ĝ·v_s) + extra.d_dt_station.
    double q = 0.0;
};

struct TwoWay {
    TwoWay(const odl::time::Epoch& t_t, const odl::time::Epoch& t_b, const odl::time::Epoch& t_r, SolvedLeg up_, SolvedLeg down_, const odl::Vec3& r_b,
           const odl::Vec3& v_b, double tof)
        : transmit(t_t), bounce(t_b), receive(t_r), up(std::move(up_)), down(std::move(down_)), target_position_m(r_b), target_velocity_m_s(v_b),
          time_of_flight_s(tof) {}
    odl::time::Epoch transmit, bounce, receive;
    SolvedLeg up, down;
    odl::Vec3 target_position_m;       ///< r_b
    odl::Vec3 target_velocity_m_s;     ///< v_b
    double time_of_flight_s = 0.0;     ///< τ_u + τ_d
};

/// Each leg's duration is iterated to the DOUBLE-PRECISION FIXED POINT — the change at most 4 ulp of the duration — starting from
/// the geometric range at the tag. A hard limit of `pass_limit` passes per leg; a leg that has not converged refuses (MEAS-F-011)
/// naming the leg, the passes and the last change: a looser stopping rule would add c ε_τ/h to a finite-difference partial and
/// ruin the gate (SPEC-measmod §6.2). A refusal of the station track, the trajectory or the extra-path function is returned with
/// its own id and this module's context in the message.
[[nodiscard]] odl::Result<TwoWay, MeasError> solve_two_way(EpochEvent event, const odl::time::Epoch& tag, const StationTrack& station,
                                                           const Trajectory& target, const LegExtraFn& extra, int pass_limit = 20);

/// MEAS-R-060: the POSITION part of the complete analytic row of the one-way-equivalent range with respect to the target's state at the nominal
/// bounce, under a time-fixed variation, the bounce epoch's dependence on the trajectory folded in analytically (it is not exported: L7 maps the row
/// with Φ(t_b, t₀) and adds nothing). For the transmit-time tag `δt_b = a_u·δr/(c − a_u·v_b)`, `δt_r = (a_d·dr_b + c δt_b)/(c − q_d)`, `dr_b = δr + v_b δt_b`,
/// `δR = (c/2) δt_r`; for the bounce-time tag `δt_t = −a_u·δr/(c + q_u)`, `δt_r = a_d·δr/(c − q_d)`, `δR = (c/2)(δt_r − δt_t)`. The velocity part of the
/// row is zero to first order and is not returned: it enters only at second order in the light-time shift. Metres per metre.
[[nodiscard]] odl::Vec3 range_partial_position(const TwoWay& solved, EpochEvent event);

/// A refusal of a dependency with this module's context prepended: the id is the dependency's own (MEAS-R-063).
[[nodiscard]] MeasError with_context(const MeasError& error, const std::string& context);

}  // namespace odl::measmod
