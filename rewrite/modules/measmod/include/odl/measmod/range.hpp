#pragma once
// odl/measmod/range.hpp — the two-way SLR range observation and its model.
//
// SPEC-measmod.md §4.2, §4.5, §4.8, §5.3, §5.4, §7, §8.2, §8.4.
//
// A CRD normal point is the UNCORRECTED two-way time of flight with the station's system delay applied (CRD2 §3.4): the model applies
// the troposphere and the centre of mass itself, solves the light time with the Earth's rotation between transmit and receive in the
// model, and returns the modelled range, the residual, a complete analytic partials row with respect to the target's state at the
// bounce epoch, and a record of what was applied — including, always, what was NOT (§6.4).

#include <odl/core/result.hpp>
#include <odl/io/crd.hpp>
#include <odl/measmod/lighttime.hpp>
#include <odl/measmod/partials.hpp>
#include <odl/measmod/registry.hpp>
#include <odl/measmod/tracks.hpp>
#include <odl/measmod/troposphere.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace odl::measmod {

/// The surface weather of a normal point, in the units of the CRD record `20` (millibar, kelvin, per cent), after the interpolation of
/// MEAS-R-035: linear between the two records that bracket the tag; a tag outside the records' span HOLDS the nearest record's values and
/// says so, with the distance in seconds.
struct Meteorology {
    double pressure_mbar = 0.0;
    double temperature_k = 0.0;
    double relative_humidity_percent = 0.0;
    bool held = false;
    double held_distance_s = 0.0;
};

/// The centre-of-mass correction of a SPHERICAL target (MEAS-R-032): the distance, one way, by which the effective reflection point lies
/// nearer the station than the centre of mass, with the citation of where the number comes from and the ILRS satellite identifier
/// (`H3` field 3) it was declared for, because a centre of mass is a property of one target (MEAS-F-007).
class SphericalCentreOfMass {
public:
    /// MEAS-F-017 for a negative or non-finite correction; MEAS-F-006 for a blank citation.
    [[nodiscard]] static odl::Result<SphericalCentreOfMass, MeasError> make(long ilrs_satellite_id, double metres, std::string citation);
    [[nodiscard]] long ilrs_satellite_id() const noexcept { return ilrs_satellite_id_; }
    [[nodiscard]] double metres() const noexcept { return metres_; }
    [[nodiscard]] const std::string& citation() const noexcept { return citation_; }

private:
    SphericalCentreOfMass(long id, double m, std::string c) : ilrs_satellite_id_(id), metres_(m), citation_(std::move(c)) {}
    long ilrs_satellite_id_;
    double metres_;
    std::string citation_;
};

/// MEAS-F-013: a target that is not a sphere (an array offset in the body frame plus attitude) is refused until a campaign needs it. There is
/// no non-spherical model, and no zero correction in its place.
[[nodiscard]] odl::Result<SphericalCentreOfMass, MeasError> non_spherical_target(std::string_view target_name);

struct RangeObservation {
    odl::time::Epoch epoch;                 ///< the tag: the transmit or the bounce instant, by `event`
    EpochEvent event = EpochEvent::GroundTransmit;
    double time_of_flight_s = 0.0;          ///< observed, two-way, the system delay applied, NOT corrected for troposphere or centre of mass
    double wavelength_nm = 0.0;
    Meteorology meteorology;
    SlrSite site;
    SphericalCentreOfMass com;
};

struct RangeBuildOptions {
    SiteOptions site;                       ///< the registry's one named override (MEAS-R-005)
};

/// MEAS-R-010 … -R-011, -R-032, -R-034 … -R-036: the normal points of a CRD pass as observations. The pass's time scale is already resolved
/// (the reader refused an unknown one). Refusals, each naming the field and the value: the target's identity (MEAS-F-007), the range type,
/// the data type and the epoch event (MEAS-F-008), an applied-flag the model would apply again or one that is not applied (MEAS-F-009), a
/// pass with no meteorology or a configuration with no wavelength (MEAS-F-012), a time of flight that is not finite and positive
/// (MEAS-F-018), an order that runs backwards or a pass over a day long (MEAS-F-019), a seconds of day out of range (MEAS-F-021), a pass
/// with no normal point (MEAS-F-023), and the registry's own (MEAS-F-001 … -004).
[[nodiscard]] odl::Result<std::vector<RangeObservation>, MeasError> range_observations(const odl::io::CrdPass& pass, const SlrRegistry& registry,
                                                                                       const odl::time::LeapTable& leaps,
                                                                                       const SphericalCentreOfMass& com,
                                                                                       const RangeBuildOptions& options = {});

/// MEAS-R-035 alone, for testing and for callers with their own records: the interpolation of the pass's meteorology to `seconds_of_day`,
/// both on the pass's own day count (the rollover of MEAS-R-011 applied by the caller). MEAS-F-012 for a pass with no record `20`.
[[nodiscard]] odl::Result<Meteorology, MeasError> interpolate_meteorology(const std::vector<odl::io::CrdMeteorology>& records, double seconds_of_day,
                                                                          double session_start_seconds_of_day);

/// One leg of the model's record of what it applied.
struct AppliedLeg {
    double geometric_range_m = 0.0;
    double light_time_s = 0.0;
    double delay_atm_m = 0.0;
    double delay_shapiro_m = 0.0;
    double elevation_rad = 0.0;
    odl::Vec3 direction;                    ///< ĝ, GCRS, station to target
    int passes = 0;
};

/// A term the model does not include, with its size and where the size comes from (MEAS-R-038, §6.4).
struct OmittedTerm {
    std::string name;
    std::string magnitude;
    std::string source;
};

struct RangeApplied {
    AppliedLeg up, down;
    double ztd_m = 0.0;
    double mapping_up = 0.0, mapping_down = 0.0;
    double com_m = 0.0;
    std::string com_citation;
    Meteorology meteorology;
    std::string slrf_release, ecc_release;
    bool post_seismic_override_used = false;
    std::vector<OmittedTerm> omitted;       ///< never empty (MEAS-R-038)
};

/// The omitted terms of §6.4, by name, magnitude and source: the same list on every modelled range.
[[nodiscard]] const std::vector<OmittedTerm>& omitted_range_terms();

/// The model's answer. Only `model_range` builds one: no `ModelledRange` can be formed from raw numbers (MEAS-R-062).
class ModelledRange {
public:
    [[nodiscard]] double range_m() const noexcept { return range_m_; }                    ///< c ToF/2 − δ_com, one-way-equivalent
    [[nodiscard]] double observed_range_m() const noexcept { return observed_m_; }        ///< c ToF_obs/2
    [[nodiscard]] double residual_m() const noexcept { return observed_m_ - range_m_; }   ///< observed − modelled
    [[nodiscard]] double time_of_flight_s() const noexcept { return tof_s_; }             ///< modelled, two-way
    [[nodiscard]] const odl::time::Epoch& transmit_tt() const noexcept { return transmit_; }
    /// The epoch at which the partials apply: the nominal bounce (MEAS-R-060). L7 maps the row with Φ(t_b, t₀) and adds nothing.
    [[nodiscard]] const odl::time::Epoch& bounce_tt() const noexcept { return bounce_; }
    [[nodiscard]] const odl::time::Epoch& receive_tt() const noexcept { return receive_; }
    [[nodiscard]] const Partials<odl::frames::Frame::GCRS, 1>& partials() const noexcept { return partials_; }
    [[nodiscard]] const RangeApplied& applied() const noexcept { return applied_; }

private:
    friend odl::Result<ModelledRange, MeasError> model_range(const RangeObservation&, const StationTrack&, const Trajectory&);
    ModelledRange(double range, double observed, double tof, odl::time::Epoch tt, odl::time::Epoch tb, odl::time::Epoch tr,
                  Partials<odl::frames::Frame::GCRS, 1> p, RangeApplied a)
        : range_m_(range), observed_m_(observed), tof_s_(tof), transmit_(tt), bounce_(tb), receive_(tr), partials_(p), applied_(std::move(a)) {}
    double range_m_, observed_m_, tof_s_;
    odl::time::Epoch transmit_, bounce_, receive_;
    Partials<odl::frames::Frame::GCRS, 1> partials_;
    RangeApplied applied_;
};

/// The extra path of one leg in the full model: the Shapiro term plus the atmosphere (ZTD · FCULa), with the derivatives the partials need
/// (MEAS-R-060). It is what `model_range` hands the light-time solver; exposed so that a test can build the very same solution and look at what the
/// row is made of (MEAS-A-044, -045). The model is copied into the function.
[[nodiscard]] LegExtraFn full_leg_extra(const TroposphereModel& troposphere);

/// MEAS-R-030 … -R-033, -R-037, -R-038, -R-060: the two-way range of `obs` through the station and the target. A refusal of the station track
/// or the trajectory is returned with its own id and the context of the leg; the elevation limit and the light-time limit are this module's
/// own (MEAS-F-010, MEAS-F-011).
[[nodiscard]] odl::Result<ModelledRange, MeasError> model_range(const RangeObservation& obs, const StationTrack& station, const Trajectory& target);

}  // namespace odl::measmod
