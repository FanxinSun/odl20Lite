// SPEC-measmod.md §4.2, §4.5, §4.8 — the two-way range: observation builder, model and partials.

#include <odl/measmod/range.hpp>

#include <odl/measmod/shapiro.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace odl::measmod {

using odl::time::Calendar;
using odl::time::Duration;
using odl::time::Epoch;
using odl::time::TimeScale;

// --------------------------------------------------------------------------------------------------------
// the centre of mass

odl::Result<SphericalCentreOfMass, MeasError> SphericalCentreOfMass::make(long ilrs_satellite_id, double metres, std::string citation) {
    if (citation.find_first_not_of(" \t\r\n") == std::string::npos)
        return odl::err("MEAS-F-006", "the centre-of-mass correction of satellite " + std::to_string(ilrs_satellite_id) +
                                          " has no citation: a value without a citation is a load error — say where the number comes from");
    if (!std::isfinite(metres) || metres < 0.0) {
        std::ostringstream m;
        m << "the centre-of-mass correction of satellite " << ilrs_satellite_id << " is " << metres << " m: it is the distance, one way, by which the "
          << "effective reflection point lies nearer the station than the centre of mass, so it is finite and not negative (the sign convention of "
             "SPEC-measmod §3.3); a negative value is a sign error and is neither used as it is nor made positive";
        return odl::err("MEAS-F-017", m.str());
    }
    return SphericalCentreOfMass(ilrs_satellite_id, metres, std::move(citation));
}

odl::Result<SphericalCentreOfMass, MeasError> non_spherical_target(std::string_view target_name) {
    return odl::err("MEAS-F-013", "target '" + std::string(target_name) +
                                      "' is not a sphere with a cited centre-of-mass correction: an array offset in the body frame plus attitude is not "
                                      "implemented until a campaign needs it, and a zero correction is not substituted");
}

// --------------------------------------------------------------------------------------------------------
// the omitted terms

const std::vector<OmittedTerm>& omitted_range_terms() {
    static const std::vector<OmittedTerm> terms = {
        {"solid_earth_tide", "up to 0.32 m radial (0.38 m at lunar perigee), 0.066 m horizontal at the mean distances",
         "computed from h2 = 0.6078 and l2 = 0.0847 (TN36 7.1.1) with the equilibrium tide: SPEC-measmod 6.4, MEAS-P-18 to -21"},
        {"ocean_tide_loading", "centimetres, \"can reach 100 mm\" at the worst coastal station",
         "TN36 7.1.2; no coefficient is obtainable under this tree's rules (a request form): SPEC-measmod 6.4"},
        {"pole_tide", "25 mm radial and 7 mm horizontal at most", "TN36 7.1.4: \"the maximum radial displacement is approximately 25 mm, and the maximum horizontal displacement is about 7 mm\""},
        {"atmospheric_loading", "not modelled by the ILRS either", "the ILRS contribution to ITRF2020: \"the non-tidal atmospheric loading effects on station positions were not modeled\""},
        {"tropospheric_gradients", "up to 5 cm of delay at low elevation", "TN36 9.1.3"},
        {"itrs_realisation", "millimetres to centimetres between IGS05 ... IGb20 and SLR20", "quoted from memory, not quantified: SPEC-measmod 6.4"},
        {"sun_shapiro_delay", "not modelled: for a near-Earth satellite only the Earth is considered", "TN36 11.2"},
        {"data_handling_biases", "a station's range bias is typically millimetres to centimetres", "the ILRS Data Handling File, deferred by ruling R3: SPEC-measmod 6.4"},
        {"com_station_dependence", "the centre of mass of a geodetic sphere varies with station, epoch and wavelength: about 4.5 mm for LAGEOS",
         "the ILRS centre-of-mass page and Rodriguez, Appleby & Otsubo (2019), deferred by ruling R3: SPEC-measmod 6.4"},
    };
    return terms;
}

// --------------------------------------------------------------------------------------------------------
// the meteorology of a normal point (MEAS-R-035)

namespace {

double unwrap(double seconds_of_day, double start) { return seconds_of_day < start ? seconds_of_day + 86400.0 : seconds_of_day; }

}  // namespace

odl::Result<Meteorology, MeasError> interpolate_meteorology(const std::vector<odl::io::CrdMeteorology>& records, double seconds_of_day,
                                                            double session_start_seconds_of_day) {
    if (records.empty())
        return odl::err("MEAS-F-012", "the pass has no record 20 (meteorology): a standard atmosphere is not substituted");
    const double t = unwrap(seconds_of_day, session_start_seconds_of_day);
    struct P { double t; const odl::io::CrdMeteorology* m; };
    std::vector<P> p;
    p.reserve(records.size());
    for (const auto& r : records) p.push_back({unwrap(r.seconds_of_day, session_start_seconds_of_day), &r});
    std::stable_sort(p.begin(), p.end(), [](const P& a, const P& b) { return a.t < b.t; });
    Meteorology out;
    auto take = [&](const odl::io::CrdMeteorology& r) {
        out.pressure_mbar = r.pressure_mbar;
        out.temperature_k = r.temperature_k;
        out.relative_humidity_percent = r.relative_humidity_percent;
    };
    if (t < p.front().t) {
        take(*p.front().m);
        out.held = true;
        out.held_distance_s = p.front().t - t;
        return out;
    }
    if (t > p.back().t) {
        take(*p.back().m);
        out.held = true;
        out.held_distance_s = t - p.back().t;
        return out;
    }
    // bracketed: the last record at or before t and the first at or after it
    std::size_t hi = 0;
    while (hi + 1 < p.size() && p[hi].t < t) ++hi;
    const std::size_t lo = (hi > 0 && p[hi].t > t) ? hi - 1 : hi;
    if (lo == hi || p[hi].t == p[lo].t) {
        take(*p[hi].m);
        return out;
    }
    const double w = (t - p[lo].t) / (p[hi].t - p[lo].t);
    const auto& a = *p[lo].m;
    const auto& b = *p[hi].m;
    out.pressure_mbar = a.pressure_mbar + w * (b.pressure_mbar - a.pressure_mbar);
    out.temperature_k = a.temperature_k + w * (b.temperature_k - a.temperature_k);
    out.relative_humidity_percent = a.relative_humidity_percent + w * (b.relative_humidity_percent - a.relative_humidity_percent);
    return out;
}

// --------------------------------------------------------------------------------------------------------
// the builder

namespace {

bool leap_year(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

void next_day(int& y, int& m, int& d) {
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int n = dim[m - 1] + ((m == 2 && leap_year(y)) ? 1 : 0);
    if (++d > n) {
        d = 1;
        if (++m > 12) {
            m = 1;
            ++y;
        }
    }
}

MeasError refuse(const char* id, const std::string& what) { return MeasError{id, what}; }

}  // namespace

odl::Result<std::vector<RangeObservation>, MeasError> range_observations(const odl::io::CrdPass& pass, const SlrRegistry& registry,
                                                                         const odl::time::LeapTable& leaps, const SphericalCentreOfMass& com,
                                                                         const RangeBuildOptions& options) {
    const auto& h4 = pass.session;
    // ---- what the pass is, before any normal point (MEAS-R-034) --------------------------------------------------------
    if (pass.target.ilrs_id != com.ilrs_satellite_id()) {
        return odl::err(refuse("MEAS-F-007", "the pass is of satellite " + std::to_string(pass.target.ilrs_id) + " (" + pass.target.name +
                                                 ", H3 field 3) but the centre-of-mass declaration (cited: " + com.citation() + ") was made for satellite " +
                                                 std::to_string(com.ilrs_satellite_id()) + ": one satellite's centre of mass is not applied to another's ranges"));
    }
    if (h4.data_type != 1)
        return odl::err(refuse("MEAS-F-008", "the H4 data type is " + std::to_string(h4.data_type) + ", not 1 (normal point): full-rate and sampled-engineering records are not modelled as normal points"));
    if (h4.tropo_applied)
        return odl::err(refuse("MEAS-F-009", "H4 field 16: tropospheric refraction is already applied to this pass; the model applies it itself and would apply it twice"));
    if (h4.com_applied)
        return odl::err(refuse("MEAS-F-009", "H4 field 17: the centre of mass is already applied to this pass; the model applies it itself and would apply it twice"));
    if (h4.remaining_fields.size() < 3)
        return odl::err(refuse("MEAS-F-008", "the H4 record lacks its station-system-delay (field 19) or range-type (field 21) indicators: " +
                                                 std::to_string(h4.remaining_fields.size()) + " field(s) after the receive-amplitude flag, 4 are required"));
    if (h4.remaining_fields[0] != "1")
        return odl::err(refuse("MEAS-F-009", "H4 field 19: the station system delay is applied = '" + h4.remaining_fields[0] +
                                                 "', not 1: an uncalibrated time of flight would carry the system delay (tens of metres) into the residual"));
    if (h4.remaining_fields[2] != "2")
        return odl::err(refuse("MEAS-F-008", "H4 field 21: the range type indicator is '" + h4.remaining_fields[2] + "', not 2 (two-way): one-way and transponder ranging are not modelled"));
    auto sod = SodKey::make(pass.station.system_id, pass.station.system_number, pass.station.occupancy);
    if (!sod) return odl::err(sod.error());

    std::vector<const odl::io::CrdRangeRecord*> nps;
    for (const auto& r : pass.ranges)
        if (r.kind == odl::io::CrdRecordKind::NormalPointRange) nps.push_back(&r);
    if (nps.empty()) return odl::err(refuse("MEAS-F-023", "the pass of " + pass.target.name + " at station " + std::to_string(sod->sod()) + " has no normal point"));
    if (pass.meteorology.empty()) return odl::err(refuse("MEAS-F-012", "the pass has no record 20 (meteorology): a standard atmosphere is not substituted"));

    // ---- the normal points ---------------------------------------------------------------------------------------------
    const double start_sod = h4.start_hour * 3600.0 + h4.start_minute * 60.0 + h4.start_second;
    std::vector<RangeObservation> out;
    out.reserve(nps.size());
    std::optional<Epoch> first, previous;
    for (std::size_t i = 0; i < nps.size(); ++i) {
        const odl::io::CrdRangeRecord& np = *nps[i];
        const std::string where = "normal point " + std::to_string(i + 1) + " of " + std::to_string(nps.size());
        if (!std::isfinite(np.seconds_of_day) || np.seconds_of_day < 0.0 || np.seconds_of_day >= 86401.0)
            return odl::err(refuse("MEAS-F-021", where + ": the seconds of day " + std::to_string(np.seconds_of_day) +
                                                     " is negative or at least 86 401 (a leap second is written 86 400.x)"));
        if (np.epoch_event != 1 && np.epoch_event != 2)
            return odl::err(refuse("MEAS-F-008", where + ": the epoch event is " + std::to_string(np.epoch_event) + "; only 1 (spacecraft bounce time) and 2 (ground transmit time) are modelled"));
        if (!std::isfinite(np.time_of_flight_s) || !(np.time_of_flight_s > 0.0))
            return odl::err(refuse("MEAS-F-018", where + ": the time of flight " + std::to_string(np.time_of_flight_s) + " s is not finite and positive"));

        // the epoch: the H4 start date plus the seconds of day, plus one day when the seconds of day is smaller than the start's (the pass crossed 00:00 UTC)
        int y = h4.start_year, mo = h4.start_month, d = h4.start_day;
        const bool rolled = np.seconds_of_day < start_sod;
        if (rolled) next_day(y, mo, d);
        auto epoch_r = [&]() -> odl::Result<Epoch, odl::Diagnostic> {
            if (np.seconds_of_day < 86400.0) {
                auto midnight = Epoch::from_calendar(TimeScale::UTC, Calendar{y, mo, d, 0, 0, 0.0}, leaps);
                if (!midnight) return odl::err(midnight.error());
                return midnight->add(Duration::from_seconds(np.seconds_of_day));
            }
            // 86 400.x is a leap second (CRD2): a UTC day that ends in one has a 23:59:60, and the calendar conversion refuses a day that does not
            return Epoch::from_calendar(TimeScale::UTC, Calendar{y, mo, d, 23, 59, 60.0 + (np.seconds_of_day - 86400.0)}, leaps);
        }();
        if (!epoch_r)   // the time module's own refusal is carried (MEAS-R-011): a 23:59:60 on a UTC day that has no leap second
            return odl::err(with_context(epoch_r.error(), where + ": the seconds of day " + std::to_string(np.seconds_of_day) + " on " + std::to_string(y) + "-" +
                                                              std::to_string(mo) + "-" + std::to_string(d) + " is not an instant of that UTC day"));
        const Epoch epoch = *epoch_r;
        if (!first) first = epoch;
        if (previous && epoch < *previous)
            return odl::err(refuse("MEAS-F-019", where + ": its epoch is before the previous normal point's once the 00:00 UTC rollover rule is applied (a file out of order, or a rule that does not fit it)"));
        if (epoch.difference(*first).to_seconds() > 86400.5)
            return odl::err(refuse("MEAS-F-019", where + ": the pass spans more than one day (CRD2 section 0: a pass must not exceed one day)"));
        previous = epoch;

        auto w = pass.wavelength_nm(np.config_id);
        if (!w) return odl::err(refuse("MEAS-F-012", where + ": " + w.error().message));
        auto met = interpolate_meteorology(pass.meteorology, np.seconds_of_day, start_sod);
        if (!met) return odl::err(met.error());
        auto site = registry.site(*sod, epoch, options.site);
        if (!site) return odl::err(with_context(site.error(), where));

        out.push_back(RangeObservation{epoch, np.epoch_event == 1 ? EpochEvent::Bounce : EpochEvent::GroundTransmit, np.time_of_flight_s, *w, *met, *site, com});
    }
    return out;
}

// --------------------------------------------------------------------------------------------------------
// the model

LegExtraFn full_leg_extra(const TroposphereModel& troposphere) {
    return [tropo = troposphere](const LegGeometry& g) -> odl::Result<LegExtra, MeasError> {
        const double r1 = g.station_m.norm(), r2 = g.target_m.norm();
        auto sh = shapiro_leg(r1, r2, g.range_m);
        if (!sh) return odl::err(sh.error());
        auto tl = tropo.leg(g.sin_elevation);
        if (!tl) return odl::err(tl.error());
        const odl::Vec3 perp = g.up - g.sin_elevation * g.direction;                       // û − (û·ĝ) ĝ
        const odl::Vec3 rhat_b = (1.0 / r2) * g.target_m;
        const double ghat_v = g.direction.dot(g.station_velocity_m_s);
        LegExtra e;
        e.delay_m = sh->delay_m + tl->delay_m;
        e.gradient_target = sh->d_rho * g.direction + sh->d_r2 * rhat_b + (tl->d_delay_d_sin_e / g.range_m) * perp;
        e.d_dt_station = -sh->d_rho * ghat_v + sh->d_r1 * (((1.0 / r1) * g.station_m).dot(g.station_velocity_m_s)) +
                         tl->d_delay_d_sin_e * (-perp.dot(g.station_velocity_m_s) / g.range_m + g.direction.dot(g.up_rate_per_s));
        return e;
    };
}

odl::Result<ModelledRange, MeasError> model_range(const RangeObservation& obs, const StationTrack& station, const Trajectory& target) {
    const double c = kSpeedOfLight_m_s;
    // ---- the atmosphere at the system reference point (MEAS-R-037), CRD units to the model's ----------------------------
    const Atmosphere atm{obs.meteorology.pressure_mbar, obs.meteorology.temperature_k - 273.15, obs.meteorology.relative_humidity_percent / 100.0};
    // NOT-A-UNIT-CROSSING: nanometres to micrometres, the transmit wavelength of the CRD record C0 to the zenith-delay model's unit
    const double wavelength_um = obs.wavelength_nm * 1e-3;
    auto tropo = TroposphereModel::make(obs.site.srp_geodetic.latitude_rad, obs.site.srp_geodetic.height_m, atm, wavelength_um);
    if (!tropo) return odl::err(with_context(tropo.error(), "range model, the troposphere"));

    const LegExtraFn extra = full_leg_extra(*tropo);

    auto solved = solve_two_way(obs.event, obs.epoch, station, target, extra);
    if (!solved) return odl::err(solved.error());
    const TwoWay& s = *solved;

    // ---- the value, the residual -----------------------------------------------------------------------------------------
    const double range = 0.5 * c * s.time_of_flight_s - obs.com.metres();
    const double observed = 0.5 * c * obs.time_of_flight_s;

    // ---- the complete analytic row w.r.t. the target's state at the nominal bounce, time-fixed (MEAS-R-060) -----------------
    const odl::Vec3 row = range_partial_position(s, obs.event);
    Partials<odl::frames::Frame::GCRS, 1> partials;
    partials.d[0] = {row.x, row.y, row.z, 0.0, 0.0, 0.0};

    // ---- what was applied ---------------------------------------------------------------------------------------------------
    RangeApplied applied;
    auto fill = [&](AppliedLeg& dst, const SolvedLeg& src, double& mapping) -> odl::Result<void, MeasError> {
        dst.geometric_range_m = src.geometry.range_m;
        dst.light_time_s = src.light_time_s;
        dst.elevation_rad = std::asin(std::clamp(src.geometry.sin_elevation, -1.0, 1.0));
        dst.direction = src.geometry.direction;
        dst.passes = src.passes;
        auto sh = shapiro_leg(src.geometry.station_m.norm(), src.geometry.target_m.norm(), src.geometry.range_m);
        if (!sh) return odl::err(sh.error());
        auto tl = tropo->leg(src.geometry.sin_elevation);
        if (!tl) return odl::err(tl.error());
        dst.delay_shapiro_m = sh->delay_m;
        dst.delay_atm_m = tl->delay_m;
        mapping = tl->mapping;
        return {};
    };
    if (auto r = fill(applied.up, s.up, applied.mapping_up); !r) return odl::err(r.error());
    if (auto r = fill(applied.down, s.down, applied.mapping_down); !r) return odl::err(r.error());
    applied.ztd_m = tropo->zenith_total_m();
    applied.com_m = obs.com.metres();
    applied.com_citation = obs.com.citation();
    applied.meteorology = obs.meteorology;
    applied.slrf_release = obs.site.slrf_release;
    applied.ecc_release = obs.site.ecc_release;
    applied.post_seismic_override_used = obs.site.post_seismic_override_used;
    applied.omitted = omitted_range_terms();

    return ModelledRange(range, observed, s.time_of_flight_s, s.transmit, s.bounce, s.receive, partials, std::move(applied));
}

}  // namespace odl::measmod
