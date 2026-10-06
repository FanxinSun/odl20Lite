#pragma once
// scenario.hpp — Yarragadee through the real chain (EOP, the real registry's system reference point) observing a target that the test places
// by geometry: an elevation, a slant range and a speed. Used by the range-model tests and by the finite-difference gate (SPEC-measmod §6.2, §8.4).
//
// THE AZIMUTH IS CHOSEN BY GEOMETRY ALONE, BEFORE ANY MODEL IS RUN: of the azimuths 0, 15, … 345 deg, the first whose line of sight has a
// direction cosine μ_k with 3 |μ_k| (1 − μ_k²) at least 1.0 (the analytic maximum of the range's third-derivative factor is 1.1547, at μ = 1/√3)
// for some GCRS axis k — so that the predicted band of the gate is visible along at least one axis. It is a property of the geometry the criterion
// (a) of §6.2 talks about, not a tuning to a result.

#include "measmod_reference.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <odl/measmod/range.hpp>
#include <odl/measmod/shapiro.hpp>
#include <odl/measmod/troposphere.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace odl::measmod::testing {

class Scenario {
public:
    /// `radial_fraction` is the part of the speed along the line of sight (0: across it and across the vertical, a high pass; 1: receding).
    Scenario(const RangeObservation& base, EpochEvent event, double elevation_deg, double slant_range_m, double speed_m_s, double radial_fraction = 0.0)
        : obs_(base), orientation_(c04(), odl::eop::EopPolicy{}, leaps()), station_(base.site.srp_itrs_m, base.site.srp_geodetic, orientation_),
          t_tag_(base.epoch), rigid_orientation_(orientation_, t_tag_), rigid_station_(base.site.srp_itrs_m, base.site.srp_geodetic, rigid_orientation_),
          r0_{}, v0_{}, a0_{}, ghat_{}, east_{} {
        obs_.event = event;
        auto s = station_.at(t_tag_);
        REQUIRE(s.has_value());
        const odl::Vec3 up = s->up;
        const odl::Vec3 east = (1.0 / s->velocity_m_s.norm()) * s->velocity_m_s;
        east_ = east;
        const odl::Vec3 north = up.cross(east);
        const double kPi = 3.14159265358979323846, el = elevation_deg * kPi / 180.0;
        double best_score = -1.0;
        for (int az_deg = 0; az_deg < 360; az_deg += 15) {
            const double az = az_deg * kPi / 180.0;
            const odl::Vec3 g = std::cos(el) * (std::cos(az) * east + std::sin(az) * north) + std::sin(el) * up;
            double score = 0.0;
            for (double m : {g.x, g.y, g.z}) score = std::max(score, 3.0 * std::abs(m) * (1.0 - m * m));
            if (score > best_score) {
                best_score = score;
                azimuth_deg_ = az_deg;
                ghat_ = g;
            }
            if (score >= 1.0) break;
        }
        best_score_ = best_score;
        r0_ = s->position_m + slant_range_m * ghat_;
        const odl::Vec3 across = (1.0 / ghat_.cross(up).norm()) * ghat_.cross(up);
        v0_ = speed_m_s * (std::sqrt(1.0 - radial_fraction * radial_fraction) * across + radial_fraction * ghat_);
        a0_ = (-kGmEarthTt_m3_s2 / std::pow(r0_.norm(), 3)) * r0_;
    }
    Scenario(const Scenario&) = delete;
    Scenario& operator=(const Scenario&) = delete;

    [[nodiscard]] const RangeObservation& obs() const { return obs_; }
    /// The station through the REAL orientation chain (the model L7 will use), and through an EXACT RIGID rotation of that chain's own matrix at the tag, at its
    /// own rate about its own axis (round-off only): the two configurations of the gate (SPEC-measmod §6.2, Amendment A1).
    [[nodiscard]] const StationTrack& station() const { return station_; }
    [[nodiscard]] const StationTrack& rigid_station() const { return rigid_station_; }
    /// The unit vector along the station's velocity at the tag: the direction in which the real chain's Earth-rotation-angle floor moves the station.
    [[nodiscard]] const odl::Vec3& east() const { return east_; }
    [[nodiscard]] const odl::time::Epoch& tag() const { return t_tag_; }
    [[nodiscard]] const odl::Vec3& line_of_sight() const { return ghat_; }
    [[nodiscard]] int azimuth_deg() const { return azimuth_deg_; }
    [[nodiscard]] double geometry_score() const { return best_score_; }
    [[nodiscard]] odl::Vec3 target_position() const { return r0_; }
    [[nodiscard]] odl::Vec3 target_velocity() const { return v0_; }
    [[nodiscard]] odl::Vec3 target_acceleration() const { return a0_; }
    [[nodiscard]] odl::Vec3 station_position() const { return station_.at(t_tag_)->position_m; }

    /// The nominal trajectory (reference epoch = the tag) varied, time-fixed, at `t_p` by (dr, dv).
    [[nodiscard]] DriftTrajectory trajectory(const odl::time::Epoch& t_p, odl::Vec3 dr = {}, odl::Vec3 dv = {}) const {
        return DriftTrajectory(t_tag_, r0_, v0_, a0_, t_p, dr, dv);
    }

private:
    RangeObservation obs_;
    EopEarthOrientation orientation_;
    EarthFixedStation station_;
    odl::time::Epoch t_tag_;
    RigidRotationOrientation rigid_orientation_;
    EarthFixedStation rigid_station_;
    odl::Vec3 r0_, v0_, a0_, ghat_, east_;
    int azimuth_deg_ = 0;
    double best_score_ = 0.0;
};

/// The extra-path function the range model itself builds for `obs` (the atmosphere at the system reference point, the Shapiro term): for the tests
/// that solve the light time directly and look at what the row is made of (MEAS-A-044, -045).
inline LegExtraFn model_extra_for(const RangeObservation& obs) {
    const Atmosphere atm{obs.meteorology.pressure_mbar, obs.meteorology.temperature_k - 273.15, obs.meteorology.relative_humidity_percent / 100.0};
    // NOT-A-UNIT-CROSSING: nanometres to micrometres, the wavelength of the CRD record C0 to the zenith-delay model's unit
    const double wavelength_um = obs.wavelength_nm * 1e-3;
    auto tropo = TroposphereModel::make(obs.site.srp_geodetic.latitude_rad, obs.site.srp_geodetic.height_m, atm, wavelength_um);
    if (!tropo) FAIL("the troposphere of the first real observation did not build: " << tropo.error().id);
    return full_leg_extra(*tropo);
}

/// The first normal point of the real January 2026 LAGEOS-1 file as an observation (Yarragadee, 2026-01-01 02:07:56.8005871 UTC).
inline const RangeObservation& first_real_observation() {
    static const RangeObservation obs = [] {
        auto o = range_observations(real_passes().front(), real_registry(), leaps(), lageos1_com());
        if (!o) FAIL("the first real pass did not build observations: " << o.error().id << " " << o.error().message);
        return o->front();
    }();
    return obs;
}

}  // namespace odl::measmod::testing
