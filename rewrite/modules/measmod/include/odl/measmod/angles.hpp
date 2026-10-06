#pragma once
// odl/measmod/angles.hpp — the angle observation (right ascension and declination, azimuth and elevation) and its models.
//
// SPEC-measmod.md §3.5, §4.7 (MEAS-R-050 … -R-056), §4.8, §5.3 – §5.5, §7, §8.7.
//
// An angle observation DECLARES the reduction it was made under (MEAS-R-050) and there is no default: a place reduced against catalogue stars carries the stars' annual
// aberration (up to 20.5″), which the satellite does not share; an azimuth and elevation are the direction as seen through the atmosphere. The model reproduces the declared
// reduction of the target's state: the emission epoch solved from the observer at the observation epoch, the direction at emission, and then the reduction's own steps.
// The observer, the Earth's orientation and the Earth's motion are all evaluated at the OBSERVATION epoch, which no perturbation of the target moves.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/io/iod.hpp>
#include <odl/measmod/lighttime.hpp>
#include <odl/measmod/partials.hpp>
#include <odl/measmod/range.hpp>
#include <odl/measmod/registry.hpp>
#include <odl/measmod/tracks.hpp>
#include <odl/measmod/troposphere.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <optional>
#include <string>
#include <vector>

namespace odl::measmod {

/// The reduction an angle observation was made under (MEAS-R-050).
enum class AngleReduction { Astrometric, Geometric, ApparentRefracted };

/// Which equinox an "of date" line (IOD epoch code 0 or blank) is referred to, declared by the caller (MEAS-R-055).
enum class EquinoxOfDate { Mean, True };

enum class AngleKind { RaDec, AzEl };

/// The frame an observation's coordinates are in: the ICRF (IOD epoch code 5, MEAS-R-055), the mean or the true equator and equinox of the date, or the observer's local horizon.
enum class AngleFrame { Icrf, MeanOfDate, TrueOfDate, Local };

/// The atmosphere an apparent, refracted angle was observed through, supplied by the caller (an IOD line carries none): pressure, temperature and humidity at the observer
/// and the wavelength of the light (MEAS-R-054).
struct AngleAtmosphere {
    Atmosphere atmosphere;
    double wavelength_um = 0.0;
};

/// A measured angle pair at an epoch, with its declared reduction. A plain aggregate: `angle_observation` is the way to build one from an IOD line.
struct AngleObservation {
    odl::time::Epoch epoch;                             ///< the observation epoch, resolved from the line's UTC calendar
    AngleKind kind = AngleKind::RaDec;
    double a_rad = 0.0, b_rad = 0.0;                    ///< right ascension and declination, or azimuth and elevation
    std::optional<double> sigma_rad;                    ///< the line's positional uncertainty, "assumed to apply equally to both components"
    AngleReduction reduction = AngleReduction::Geometric;
    AngleFrame frame = AngleFrame::Icrf;
    odl::Mat3 gcrs_to_frame = odl::Mat3::identity();    ///< the rotation of an "of date" frame at the epoch (bias-precession or bias-precession-nutation); the identity otherwise
    OpticalSite site;
    std::optional<AngleAtmosphere> atmosphere;
};

/// MEAS-R-050, -R-054, -R-055, -R-056. There is NO overload without `reduction` (MEAS-A-082). Refusals: MEAS-F-005 (a station number the registry does not hold), MEAS-F-015 (a
/// reduction that does not fit the coordinate kind), MEAS-F-016 (an epoch code of 1, 2, 3, 4 or 6, or "of date" with no declared equinox), MEAS-F-022 (an azimuth/elevation
/// observation with no atmosphere, or a non-physical one), and the decoders' own IOFM-F-016 / -F-017 and the time module's, each with its own id.
[[nodiscard]] odl::Result<AngleObservation, MeasError> angle_observation(const odl::io::IodObservation& iod, const OpticalRegistry& sites, AngleReduction reduction,
                                                                         std::optional<EquinoxOfDate> of_date, std::optional<AngleAtmosphere> atmosphere,
                                                                         const odl::time::LeapTable& leaps);

// ---- the term functions: public, tested building blocks ---------------------------------------------------------------------------------------------------------------

/// MEAS-R-053: the aberration `A(n; β) = [ n/γ + (1 + (n·β)/(1 + 1/γ)) β ] / (1 + n·β)` of the unit direction `n` for an observer moving with `β = v/c` (`eraAb`'s formula without
/// its solar-potential term), and its exact special-relativistic inverse `D(n; β) = A(n; −β)`.
[[nodiscard]] odl::Vec3 aberration_apply(const odl::Vec3& n, const odl::Vec3& beta) noexcept;
[[nodiscard]] odl::Vec3 aberration_remove(const odl::Vec3& n, const odl::Vec3& beta) noexcept;

/// The Jacobians of `A` and `D` with respect to the direction, as 3 × 3 matrices acting on a displacement tangent to the sphere at `n` (the row of MEAS-R-060).
[[nodiscard]] odl::Mat3 aberration_apply_jacobian(const odl::Vec3& n, const odl::Vec3& beta) noexcept;
[[nodiscard]] odl::Mat3 aberration_remove_jacobian(const odl::Vec3& n, const odl::Vec3& beta) noexcept;

/// MEAS-R-054: `ERFA`'s `eraRefco` constants of the refraction `z_v = z_o + A tan z_o + B tan³ z_o`, for the supplied atmosphere. MEAS-F-022 for a non-physical one.
struct RefractionConstants {
    double a = 0.0;
    double b = 0.0;
};
[[nodiscard]] odl::Result<RefractionConstants, MeasError> refraction_constants(const AngleAtmosphere& atmosphere);

/// The observed zenith distance `z_o` that satisfies `z_v = z_o + A tan z_o + B tan³ z_o` (Newton, to 1e-14 rad) for the vacuum zenith distance `z_v`, and `dz_o/dz_v`.
[[nodiscard]] double refracted_zenith_distance(double z_vacuum_rad, const RefractionConstants& c) noexcept;
[[nodiscard]] double refracted_zenith_derivative(double z_observed_rad, const RefractionConstants& c) noexcept;

/// MEAS-R-051: the emission epoch `t_e` that solves `|r(t_e) − s| = c (t_o − t_e)`, `s` the observer's position at the observation epoch `t_o`, iterated to the double-precision fixed
/// point as the range's legs are (MEAS-R-031: the change at most 4 ulp, at most `pass_limit` passes, MEAS-F-011). No Shapiro or atmospheric term enters a direction's light time.
struct Emission {
    Emission(const odl::time::Epoch& t_e, double tau, double rho, const odl::Vec3& r, const odl::Vec3& v, const odl::Vec3& n, int p)
        : emission(t_e), light_time_s(tau), range_m(rho), target_position_m(r), target_velocity_m_s(v), direction(n), passes(p) {}
    odl::time::Epoch emission;
    double light_time_s;
    double range_m;                         ///< |r(t_e) − s|
    odl::Vec3 target_position_m;
    odl::Vec3 target_velocity_m_s;
    odl::Vec3 direction;                    ///< the geometric unit direction from the observer to the target at emission
    int passes;
};
[[nodiscard]] odl::Result<Emission, MeasError> solve_emission(const odl::time::Epoch& observation, const odl::Vec3& observer_m, const Trajectory& target, int pass_limit = 20);

// ---- the models -----------------------------------------------------------------------------------------------------------------------------------------------------

/// What an angle model applied and what it did not (MEAS-R-062).
struct AngleApplied {
    AngleReduction reduction = AngleReduction::Geometric;
    AngleFrame frame = AngleFrame::Icrf;
    double light_time_s = 0.0;
    double range_m = 0.0;
    int passes = 0;
    odl::Vec3 aberration_beta;                 ///< the velocity over c applied: the Earth's (Astrometric, removed), the observer's (ApparentRefracted); zero for Geometric
    double aberration_shift_rad = 0.0;         ///< the angle between the geometric direction and the reduced one
    double refraction_a = 0.0, refraction_b = 0.0;
    double zenith_vacuum_rad = 0.0, zenith_observed_rad = 0.0;
    double refraction_rad = 0.0;               ///< z_v − z_o
    std::optional<AngleAtmosphere> atmosphere;
    odl::Mat3 gcrs_to_frame = odl::Mat3::identity();
    std::vector<OmittedTerm> omitted;
};

/// The numbers every angle model returns, and the epochs; the two result classes below name the pair.
struct AngleResult {
    AngleResult(double a, double b, double oa, double ob, odl::time::Epoch obs, odl::time::Epoch emit, Partials<odl::frames::Frame::GCRS, 2> p, AngleApplied ap)
        : a_rad(a), b_rad(b), observed_a_rad(oa), observed_b_rad(ob), observation_epoch(obs), emission_epoch(emit), partials(p), applied(std::move(ap)) {}
    double a_rad, b_rad;
    double observed_a_rad, observed_b_rad;
    odl::time::Epoch observation_epoch, emission_epoch;
    Partials<odl::frames::Frame::GCRS, 2> partials;
    AngleApplied applied;
};

/// MEAS-R-056: the residual of right ascension or azimuth is `observed − modelled` wrapped to (−π, π]; that of declination or elevation is the plain difference. No `cos δ` scaling.
[[nodiscard]] double wrap_to_pi(double angle_rad) noexcept;

class ModelledRaDec {
public:
    [[nodiscard]] double ra_rad() const noexcept { return r_.a_rad; }
    [[nodiscard]] double dec_rad() const noexcept { return r_.b_rad; }
    [[nodiscard]] double observed_ra_rad() const noexcept { return r_.observed_a_rad; }
    [[nodiscard]] double observed_dec_rad() const noexcept { return r_.observed_b_rad; }
    [[nodiscard]] double residual_ra_rad() const noexcept { return wrap_to_pi(r_.observed_a_rad - r_.a_rad); }
    [[nodiscard]] double residual_dec_rad() const noexcept { return r_.observed_b_rad - r_.b_rad; }
    [[nodiscard]] const odl::time::Epoch& epoch() const noexcept { return r_.observation_epoch; }
    /// The epoch at which the partials apply (MEAS-R-060): the emission epoch.
    [[nodiscard]] const odl::time::Epoch& emission_tt() const noexcept { return r_.emission_epoch; }
    [[nodiscard]] const Partials<odl::frames::Frame::GCRS, 2>& partials() const noexcept { return r_.partials; }
    [[nodiscard]] const AngleApplied& applied() const noexcept { return r_.applied; }

private:
    friend odl::Result<ModelledRaDec, MeasError> model_radec(const AngleObservation&, const StationTrack&, const Trajectory&, const EarthMotion&);
    explicit ModelledRaDec(AngleResult r) : r_(std::move(r)) {}
    AngleResult r_;
};

class ModelledAzEl {
public:
    [[nodiscard]] double azimuth_rad() const noexcept { return r_.a_rad; }
    [[nodiscard]] double elevation_rad() const noexcept { return r_.b_rad; }
    [[nodiscard]] double observed_azimuth_rad() const noexcept { return r_.observed_a_rad; }
    [[nodiscard]] double observed_elevation_rad() const noexcept { return r_.observed_b_rad; }
    [[nodiscard]] double residual_azimuth_rad() const noexcept { return wrap_to_pi(r_.observed_a_rad - r_.a_rad); }
    [[nodiscard]] double residual_elevation_rad() const noexcept { return r_.observed_b_rad - r_.b_rad; }
    [[nodiscard]] const odl::time::Epoch& epoch() const noexcept { return r_.observation_epoch; }
    [[nodiscard]] const odl::time::Epoch& emission_tt() const noexcept { return r_.emission_epoch; }
    [[nodiscard]] const Partials<odl::frames::Frame::GCRS, 2>& partials() const noexcept { return r_.partials; }
    [[nodiscard]] const AngleApplied& applied() const noexcept { return r_.applied; }

private:
    friend odl::Result<ModelledAzEl, MeasError> model_azel(const AngleObservation&, const StationTrack&, const Trajectory&, const EarthOrientation&);
    explicit ModelledAzEl(AngleResult r) : r_(std::move(r)) {}
    AngleResult r_;
};

/// MEAS-R-051 … -R-053, -R-056, -R-060: a right-ascension and declination observation, `Astrometric` (the natural direction: the geometric one with the Earth's annual aberration
/// removed) or `Geometric`, in the observation's frame (the ICRF or of date). MEAS-F-015 for a reduction that does not fit, MEAS-F-011 for an emission epoch that does not converge,
/// and a dependency's refusal with its own id.
[[nodiscard]] odl::Result<ModelledRaDec, MeasError> model_radec(const AngleObservation& obs, const StationTrack& station, const Trajectory& target, const EarthMotion& motion);

/// MEAS-R-054, -R-056, -R-060: an azimuth and elevation observation, `ApparentRefracted`: the geometric direction with the observer's diurnal aberration applied, rotated to the local
/// horizon, and the refraction of the supplied atmosphere. MEAS-F-010 for a vacuum zenith distance above 75°.
[[nodiscard]] odl::Result<ModelledAzEl, MeasError> model_azel(const AngleObservation& obs, const StationTrack& station, const Trajectory& target, const EarthOrientation& earth);

}  // namespace odl::measmod
