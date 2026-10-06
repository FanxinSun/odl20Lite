#pragma once
// angle_scenario.hpp — Yarragadee through the real chain at the epoch of the first real normal point, observing a target that the test places by geometry for the angle gate:
// an elevation, a slant range, a speed and a direction of that speed. Used by the angle gate (SPEC-measmod §6.2 (v), §8.7: MEAS-A-096 … -099).
//
// THE LINE OF SIGHT IS CHOSEN BY GEOMETRY ALONE, BEFORE ANY MODEL IS RUN. Of the local azimuths 0, 15, … 345 degrees at the stated elevation, the first whose line of sight
//   (1) lies in the CLASS of the sizing: for right ascension and declination, |declination| <= 39.5 degrees of the geometric direction in the frame of the observation (the
//       ICRF, or the true equator and equinox of date); for azimuth and elevation, the elevation of 30 degrees is within the class (20 to 39.5 degrees) and every azimuth is;
//   (2) has a VISIBILITY SCORE of at least 0.3: the largest over the three GCRS axes of the displacement of |2 Im[(w1/w0)^3]| / (2/cos^3 40 deg), where w0 = g_x + i g_y and
//       w1 = e_x + i e_y are the line of sight and the displacement axis in the frame of the output (the frame of the observation; north-east-up for azimuth and elevation) — the
//       third derivative of the first angle along that axis as a fraction of the sizing's coefficient, so that the band is at least 0.3 of its prediction (three times the
//       lower limit of criterion (a)). It is a property of the geometry, which criterion (a) talks about, not a tuning to a result.

#include "scenario.hpp"

#include <odl/measmod/angles.hpp>

#include <erfa.h>

#include <algorithm>
#include <cmath>
#include <complex>

namespace odl::measmod::testing {

enum class AngleGate { Geometric, Astrometric, ApparentRefracted, AstrometricOfDate };
enum class AngleConfig { RealChain, ExactRigid };

inline const char* angle_gate_name(AngleGate g) {
    switch (g) {
        case AngleGate::Geometric: return "Geometric (right ascension, declination)";
        case AngleGate::Astrometric: return "Astrometric (right ascension, declination)";
        case AngleGate::ApparentRefracted: return "ApparentRefracted (azimuth, elevation)";
        case AngleGate::AstrometricOfDate: return "Astrometric, true equator and equinox of date";
    }
    return "?";
}
inline const char* angle_config_name(AngleConfig c) { return c == AngleConfig::RealChain ? "real chain" : "exact rigid rotation"; }

/// The visibility score of one displacement axis (see the header comment).
inline double visibility_score(const odl::Vec3& g_frame, const odl::Vec3& e_frame) {
    constexpr double kPi = 3.14159265358979323846;
    using C = std::complex<double>;
    const C ratio = C(e_frame.x, e_frame.y) / C(g_frame.x, g_frame.y);
    const C cube = ratio * ratio * ratio;
    return std::abs(2.0 * cube.imag()) / (2.0 / std::pow(std::cos(40.0 * kPi / 180.0), 3.0));
}

class AngleScenario {
public:
    static constexpr double kMaxDeclination_deg = 39.5;
    static constexpr double kMinScore = 0.3;
    static constexpr double kRigidOffset_s = 0.01;     ///< the rigid configuration's reference epoch lies this long BEFORE the observation epoch (SPEC §6.2 (v))

    /// `radial_fraction` is the part of the speed along the line of sight (0: across it and across the vertical; 1: receding).
    AngleScenario(AngleGate gate, double elevation_deg, double slant_range_m, double speed_m_s, double radial_fraction)
        : gate_(gate), base_(first_real_observation()), t_o_(base_.epoch), orientation_(c04(), odl::eop::EopPolicy{}, leaps()),
          station_(base_.site.srp_itrs_m, base_.site.srp_geodetic, orientation_),
          rigid_orientation_(orientation_, t_o_.add(odl::time::Duration::from_seconds(-kRigidOffset_s))),
          rigid_station_(base_.site.srp_itrs_m, base_.site.srp_geodetic, rigid_orientation_), motion_(ephemeris_de440s(), leaps()),
          site_{OpticalStationNumber{9999}, base_.site.srp_geodetic, "Yarragadee's system reference point from the real registry, the gate's observer"},
          frame_matrix_(odl::Mat3::identity()), r0_{}, v0_{}, a0_{}, g_{}, t_p_(t_o_) {
        constexpr double kPi = 3.14159265358979323846;
        auto s = station_.at(t_o_);
        REQUIRE(s.has_value());
        auto sample = orientation_.at(t_o_);
        REQUIRE(sample.has_value());
        if (gate == AngleGate::AstrometricOfDate) {
            auto jd = t_o_.two_part_jd(odl::time::TimeScale::TT, leaps());
            REQUIRE(jd.has_value());
            double r[3][3];
            eraPnm06a(jd->day, jd->fraction, r);
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) frame_matrix_.r[i][j] = r[i][j];
        }
        const double sl = std::sin(site_.location.longitude_rad), cl = std::cos(site_.location.longitude_rad);
        const double sp = std::sin(site_.location.latitude_rad), cp = std::cos(site_.location.latitude_rad);
        odl::Mat3 enu{};
        enu.r[0] = {-sl, cl, 0.0};
        enu.r[1] = {-sp * cl, -sp * sl, cp};
        enu.r[2] = {cp * cl, cp * sl, sp};
        const odl::Mat3 to_local = enu.times(sample->gcrs_to_itrs);     // GCRS -> (east, north, up)
        const odl::Mat3 from_local = to_local.transpose();
        const double el = elevation_deg * kPi / 180.0;
        const odl::Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        bool found = false;
        for (int az_deg = 0; az_deg < 360 && !found; az_deg += 15) {
            const double az = az_deg * kPi / 180.0;
            const odl::Vec3 g_local{std::cos(el) * std::sin(az), std::cos(el) * std::cos(az), std::sin(el)};      // east, north, up
            const odl::Vec3 g = from_local.apply(g_local);
            double score = 0.0;
            double declination_deg = 0.0;
            if (gate == AngleGate::ApparentRefracted) {
                const odl::Vec3 g_frame{g_local.y, g_local.x, g_local.z};                                           // north, east, up
                for (const odl::Vec3& e : axes) {
                    const odl::Vec3 e_local = to_local.apply(e);
                    score = std::max(score, visibility_score(g_frame, odl::Vec3{e_local.y, e_local.x, e_local.z}));
                }
                declination_deg = elevation_deg;
            } else {
                const odl::Vec3 m = frame_matrix_.apply(g);
                declination_deg = std::asin(m.z) * 180.0 / kPi;
                for (const odl::Vec3& e : axes) score = std::max(score, visibility_score(m, frame_matrix_.apply(e)));
            }
            const bool in_class = std::abs(declination_deg) <= kMaxDeclination_deg;
            if (in_class && score >= kMinScore) {
                found = true;
                azimuth_deg_ = az_deg;
                score_ = score;
                declination_deg_ = declination_deg;
                g_ = g;
            }
        }
        REQUIRE(found);
        r0_ = s->position_m + slant_range_m * g_;
        const odl::Vec3 across = (1.0 / g_.cross(s->up).norm()) * g_.cross(s->up);
        v0_ = speed_m_s * (std::sqrt(1.0 - radial_fraction * radial_fraction) * across + radial_fraction * g_);
        a0_ = (-kGmEarthTt_m3_s2 / std::pow(r0_.norm(), 3)) * r0_;
        // the epoch of the variation: the nominal EMISSION epoch, solved from the nominal trajectory
        const DriftTrajectory nominal(t_o_, r0_, v0_, a0_, t_o_);
        auto em = solve_emission(t_o_, s->position_m, nominal);
        REQUIRE(em.has_value());
        t_p_ = em->emission;
        slant_at_emission_ = em->range_m;
    }
    AngleScenario(const AngleScenario&) = delete;
    AngleScenario& operator=(const AngleScenario&) = delete;

    [[nodiscard]] AngleGate gate() const { return gate_; }
    [[nodiscard]] const odl::time::Epoch& observation_epoch() const { return t_o_; }
    [[nodiscard]] const odl::time::Epoch& variation_epoch() const { return t_p_; }
    [[nodiscard]] const StationTrack& station(AngleConfig c) const { return c == AngleConfig::RealChain ? static_cast<const StationTrack&>(station_) : rigid_station_; }
    [[nodiscard]] const EarthOrientation& orientation(AngleConfig c) const { return c == AngleConfig::RealChain ? static_cast<const EarthOrientation&>(orientation_) : rigid_orientation_; }
    [[nodiscard]] const EarthMotion& motion() const { return motion_; }
    [[nodiscard]] const OpticalSite& site() const { return site_; }
    [[nodiscard]] const odl::Mat3& frame_matrix() const { return frame_matrix_; }
    [[nodiscard]] int azimuth_deg() const { return azimuth_deg_; }
    [[nodiscard]] double score() const { return score_; }
    [[nodiscard]] double declination_deg() const { return declination_deg_; }
    [[nodiscard]] const odl::Vec3& line_of_sight() const { return g_; }
    [[nodiscard]] odl::Vec3 target_position() const { return r0_; }
    [[nodiscard]] odl::Vec3 target_velocity() const { return v0_; }
    [[nodiscard]] double slant_range_at_emission() const { return slant_at_emission_; }
    [[nodiscard]] odl::Vec3 station_position(AngleConfig c) const { return station(c).at(t_o_)->position_m; }

    /// The nominal trajectory (reference epoch = the observation epoch) varied, time-fixed, at the nominal emission epoch by (dr, dv).
    [[nodiscard]] DriftTrajectory trajectory(odl::Vec3 dr = {}, odl::Vec3 dv = {}) const { return DriftTrajectory(t_o_, r0_, v0_, a0_, t_p_, dr, dv); }

    /// The observation: the declared reduction and frame, the real first normal point's weather and wavelength for the refraction.
    [[nodiscard]] AngleObservation observation() const {
        switch (gate_) {
            case AngleGate::Geometric:
                return AngleObservation{t_o_, AngleKind::RaDec, 0.0, 0.0, std::nullopt, AngleReduction::Geometric, AngleFrame::Icrf, odl::Mat3::identity(), site_, std::nullopt};
            case AngleGate::Astrometric:
                return AngleObservation{t_o_, AngleKind::RaDec, 0.0, 0.0, std::nullopt, AngleReduction::Astrometric, AngleFrame::Icrf, odl::Mat3::identity(), site_, std::nullopt};
            case AngleGate::AstrometricOfDate:
                return AngleObservation{t_o_, AngleKind::RaDec, 0.0, 0.0, std::nullopt, AngleReduction::Astrometric, AngleFrame::TrueOfDate, frame_matrix_, site_, std::nullopt};
            case AngleGate::ApparentRefracted:
                return AngleObservation{t_o_, AngleKind::AzEl, 0.0, 0.0, std::nullopt, AngleReduction::ApparentRefracted, AngleFrame::Local, odl::Mat3::identity(), site_, atmosphere()};
        }
        return AngleObservation{t_o_, AngleKind::RaDec, 0.0, 0.0, std::nullopt, AngleReduction::Geometric, AngleFrame::Icrf, odl::Mat3::identity(), site_, std::nullopt};
    }

    [[nodiscard]] AngleAtmosphere atmosphere() const {
        // NOT-A-UNIT-CROSSING: nanometres to micrometres, the wavelength of the CRD record C0 to the refraction model's unit (as in scenario.hpp)
        return AngleAtmosphere{Atmosphere{base_.meteorology.pressure_mbar, base_.meteorology.temperature_k - 273.15, base_.meteorology.relative_humidity_percent / 100.0},
                               base_.wavelength_nm * 1e-3};
    }

private:
    AngleGate gate_;
    RangeObservation base_;
    odl::time::Epoch t_o_;
    EopEarthOrientation orientation_;
    EarthFixedStation station_;
    RigidRotationOrientation rigid_orientation_;
    EarthFixedStation rigid_station_;
    EphemerisEarthMotion motion_;
    OpticalSite site_;
    odl::Mat3 frame_matrix_;
    odl::Vec3 r0_, v0_, a0_, g_;
    odl::time::Epoch t_p_;
    int azimuth_deg_ = 0;
    double score_ = 0.0;
    double declination_deg_ = 0.0;
    double slant_at_emission_ = 0.0;
};

}  // namespace odl::measmod::testing
