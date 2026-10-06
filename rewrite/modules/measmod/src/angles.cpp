// SPEC-measmod.md §4.7 — the angle models.

#include <odl/measmod/angles.hpp>

#include <odl/core/units.hpp>
#include <odl/measmod/shapiro.hpp>   // kSpeedOfLight_m_s

#include <erfa.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace odl::measmod {

using odl::Mat3;
using odl::Vec3;
using odl::time::Duration;
using odl::time::Epoch;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxVacuumZenith_rad = 75.0 * kPi / 180.0;      // MEAS-R-054: the refraction model's documented range
constexpr double kPoleLimit_rad = 1.0e-9;                            // MEAS-R-057 / MEAS-F-026: nearer than this to the pole of its coordinate system, the first angle is undefined

double ulp_of(double x) noexcept {
    const double a = std::abs(x);
    return std::nextafter(a, std::numeric_limits<double>::infinity()) - a;
}

Mat3 outer(const Vec3& a, const Vec3& b) noexcept {
    Mat3 m{};
    const double av[3] = {a.x, a.y, a.z}, bv[3] = {b.x, b.y, b.z};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = av[i] * bv[j];
    return m;
}

Mat3 add(const Mat3& a, const Mat3& b) noexcept {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = a.r[i][j] + b.r[i][j];
    return m;
}

Mat3 sub(const Mat3& a, const Mat3& b) noexcept {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = a.r[i][j] - b.r[i][j];
    return m;
}

Mat3 scaled(const Mat3& a, double s) noexcept {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = s * a.r[i][j];
    return m;
}

/// The row vector `r` times the matrix `m`: the gradient of (r . m x) with respect to x.
Vec3 row_times(const Vec3& r, const Mat3& m) noexcept {
    return Vec3{r.x * m.r[0][0] + r.y * m.r[1][0] + r.z * m.r[2][0], r.x * m.r[0][1] + r.y * m.r[1][1] + r.z * m.r[2][1],
                r.x * m.r[0][2] + r.y * m.r[1][2] + r.z * m.r[2][2]};
}

/// The row of the matrix `m` (the gradient of its i-th component).
Vec3 row_of(const Mat3& m, std::size_t i) noexcept { return Vec3{m.r[i][0], m.r[i][1], m.r[i][2]}; }

double wrap_two_pi(double a) noexcept {
    double r = std::fmod(a, 2.0 * kPi);
    if (r < 0.0) r += 2.0 * kPi;
    if (r >= 2.0 * kPi) r = 0.0;
    return r;
}

MeasError f015(const std::string& what) { return MeasError{"MEAS-F-015", what}; }

/// MEAS-F-026: the direction (a vector of the norm `len`) is within the pole limit of the pole of its coordinate system; `h` is the length of its component off the pole axis.
MeasError f026(double h, double len, const char* what) {
    std::ostringstream m;
    m << "the direction is " << std::atan2(h, std::sqrt(len * len - h * h)) << " rad from " << what << ", nearer than the limit of " << kPoleLimit_rad
      << " rad: the first angle is undefined there and its partial unbounded — no azimuth of 0 and no partial of infinity is returned";
    return MeasError{"MEAS-F-026", m.str()};
}

/// The Jacobian of the geometric direction with respect to the target's state at the nominal emission epoch, time-fixed (MEAS-R-060):
/// `dt_e = −n·dr/(c + n·v)`, `dr_e = dr + v dt_e`, `dn = (I − n nᵀ) dr_e / ρ`.
Mat3 geometric_jacobian(const Vec3& n, const Vec3& v, double rho) noexcept {
    const double c = kSpeedOfLight_m_s;
    Mat3 k = sub(Mat3::identity(), scaled(outer(v, n), 1.0 / (c + n.dot(v))));
    Mat3 p = sub(Mat3::identity(), outer(n, n));
    return scaled(p.times(k), 1.0 / rho);
}

}  // namespace

// ---- the aberration ---------------------------------------------------------------------------------------------------------------------------------------------------

Vec3 aberration_apply(const Vec3& n, const Vec3& beta) noexcept {
    const double bm1 = std::sqrt(1.0 - beta.dot(beta));            // 1/γ
    const double nb = n.dot(beta);
    const double w1 = 1.0 + nb / (1.0 + bm1);
    return (1.0 / (1.0 + nb)) * (bm1 * n + w1 * beta);
}

Vec3 aberration_remove(const Vec3& n, const Vec3& beta) noexcept { return aberration_apply(n, -1.0 * beta); }

Mat3 aberration_apply_jacobian(const Vec3& n, const Vec3& beta) noexcept {
    const double bm1 = std::sqrt(1.0 - beta.dot(beta));
    const double q = 1.0 + n.dot(beta);
    const Vec3 a = aberration_apply(n, beta);
    Mat3 j = add(scaled(Mat3::identity(), bm1), scaled(outer(beta, beta), 1.0 / (1.0 + bm1)));
    j = sub(j, outer(a, beta));
    return scaled(j, 1.0 / q);
}

Mat3 aberration_remove_jacobian(const Vec3& n, const Vec3& beta) noexcept { return aberration_apply_jacobian(n, -1.0 * beta); }

// ---- the refraction ---------------------------------------------------------------------------------------------------------------------------------------------------

odl::Result<RefractionConstants, MeasError> refraction_constants(const AngleAtmosphere& atmosphere) {
    auto ok = validate_atmosphere(atmosphere.atmosphere, atmosphere.wavelength_um);
    if (!ok) return odl::err(ok.error());
    RefractionConstants c;
    eraRefco(atmosphere.atmosphere.pressure_hpa, atmosphere.atmosphere.temperature_c, atmosphere.atmosphere.relative_humidity, atmosphere.wavelength_um, &c.a, &c.b);
    return c;
}

double refracted_zenith_distance(double z_vacuum_rad, const RefractionConstants& c) noexcept {
    double z = z_vacuum_rad;
    for (int i = 0; i < 50; ++i) {
        const double t = std::tan(z), s2 = 1.0 + t * t;
        const double g = z + c.a * t + c.b * t * t * t - z_vacuum_rad;
        const double dg = 1.0 + c.a * s2 + 3.0 * c.b * t * t * s2;
        const double step = g / dg;
        z -= step;
        if (std::abs(step) < 1e-15) break;
    }
    return z;
}

double refracted_zenith_derivative(double z_observed_rad, const RefractionConstants& c) noexcept {
    const double t = std::tan(z_observed_rad), s2 = 1.0 + t * t;
    return 1.0 / (1.0 + c.a * s2 + 3.0 * c.b * t * t * s2);
}

// ---- the emission epoch -----------------------------------------------------------------------------------------------------------------------------------------------

odl::Result<Emission, MeasError> solve_emission(const Epoch& observation, const Vec3& observer_m, const Trajectory& target, int pass_limit) {
    const double c = kSpeedOfLight_m_s;
    auto at_observation = target.state_at(observation);
    if (!at_observation) return odl::err(with_context(at_observation.error(), "angle model, the target at the observation epoch"));
    double tau = (odl::metres_from_km(at_observation->position()) - observer_m).norm() / c;
    double last_change = std::numeric_limits<double>::infinity();
    for (int pass = 1; pass <= pass_limit; ++pass) {
        const Epoch t_e = observation.add(Duration::from_seconds(-tau));
        auto st = target.state_at(t_e);
        if (!st) return odl::err(with_context(st.error(), "angle model, the target at the emission epoch"));
        const Vec3 r = odl::metres_from_km(st->position()), v = odl::metres_from_km(st->velocity());
        const Vec3 d = r - observer_m;
        const double rho = d.norm();
        const double tau_new = rho / c;
        last_change = std::abs(tau_new - tau);
        tau = tau_new;
        if (last_change <= 4.0 * ulp_of(tau_new)) return Emission(t_e, tau_new, rho, r, v, (1.0 / rho) * d, pass);
    }
    std::ostringstream m;
    m << "the emission epoch has not reached its fixed point in " << pass_limit << " passes (last change " << last_change << " s against a stopping rule of 4 ulp of the light time, " << tau
      << " s): the iteration is not contracting — a target at or beyond the speed of light? — and the last iterate is not returned";
    return odl::err("MEAS-F-011", m.str());
}

double wrap_to_pi(double angle_rad) noexcept {
    double r = std::remainder(angle_rad, 2.0 * kPi);
    if (r <= -kPi) r += 2.0 * kPi;
    return r;
}

// ---- the observation --------------------------------------------------------------------------------------------------------------------------------------------------

odl::Result<AngleObservation, MeasError> angle_observation(const odl::io::IodObservation& iod, const OpticalRegistry& sites, AngleReduction reduction, std::optional<EquinoxOfDate> of_date,
                                                           std::optional<AngleAtmosphere> atmosphere, const odl::time::LeapTable& leaps) {
    auto site = sites.site(OpticalStationNumber{iod.station_number});
    if (!site) return odl::err(site.error());                                                                // MEAS-F-005, with the number and how many sites the registry holds
    auto angles = odl::io::decode_iod_angles(iod);
    if (!angles) return odl::err(with_context(angles.error(), "IOD observation, the angles"));              // the decoder's own id
    const AngleKind kind = angles->kind == odl::io::IodAngleKind::RaDec ? AngleKind::RaDec : AngleKind::AzEl;

    // MEAS-F-015: a reduction that does not fit the coordinate kind
    const bool fits = kind == AngleKind::RaDec ? (reduction == AngleReduction::Astrometric || reduction == AngleReduction::Geometric) : reduction == AngleReduction::ApparentRefracted;
    if (!fits) {
        std::ostringstream m;
        m << "the reduction " << (reduction == AngleReduction::Astrometric ? "Astrometric" : reduction == AngleReduction::Geometric ? "Geometric" : "ApparentRefracted")
          << " does not fit an observation of " << (kind == AngleKind::RaDec ? "right ascension and declination (Astrometric or Geometric)" : "azimuth and elevation (ApparentRefracted)");
        return odl::err(f015(m.str()));
    }

    // the epoch
    auto epoch = Epoch::from_calendar(odl::time::TimeScale::UTC, odl::time::Calendar{iod.year, iod.month, iod.day, iod.hour, iod.minute, iod.second}, leaps);
    if (!epoch) return odl::err(with_context(epoch.error(), "IOD observation, the epoch"));

    // MEAS-R-055: the epoch-code policy, for right ascension and declination
    AngleFrame frame = kind == AngleKind::AzEl ? AngleFrame::Local : AngleFrame::Icrf;
    Mat3 to_frame = Mat3::identity();
    if (kind == AngleKind::RaDec) {
        const std::optional<int>& code = iod.epoch_code;
        if (code && *code == 5) {
            frame = AngleFrame::Icrf;                                                                         // 2000: the mean equator and equinox of J2000, read as the ICRF (23.2 mas, below the format's precision)
        } else if (!code || *code == 0) {
            if (!of_date) {
                return odl::err("MEAS-F-016", "IOD epoch code is 0 or blank (\"of date\") and the caller declared no equinox: pass MeanEquinoxOfDate or TrueEquinoxOfDate — the code does not say whether the date's equinox is mean or true, and J2000 is not assumed");
            }
            auto jd = epoch->two_part_jd(odl::time::TimeScale::TT, leaps);
            if (!jd) return odl::err(with_context(jd.error(), "IOD observation, the TT date of the equinox"));
            double r[3][3];
            if (*of_date == EquinoxOfDate::Mean) {
                eraPmat06(jd->day, jd->fraction, r);                                                           // GCRS → the mean equator and equinox of date
                frame = AngleFrame::MeanOfDate;
            } else {
                eraPnm06a(jd->day, jd->fraction, r);                                                           // GCRS → the true equator and equinox of date
                frame = AngleFrame::TrueOfDate;
            }
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) to_frame.r[i][j] = r[i][j];
        } else {
            std::ostringstream m;
            m << "IOD epoch code " << *code << " (" << (*code == 1 ? "1855" : *code == 2 ? "1875" : *code == 3 ? "1900" : *code == 4 ? "1950" : *code == 6 ? "2050" : "unknown")
              << ") is refused: only 5 (2000, read as the ICRF) and 0 (of date, with a declared equinox) are accepted — reading it as J2000 would be a precession of up to 4.5 degrees";
            return odl::err("MEAS-F-016", m.str());
        }
    }

    // the uncertainty
    auto sigma = odl::io::decode_iod_position_uncertainty(iod);
    if (!sigma) return odl::err(with_context(sigma.error(), "IOD observation, the positional uncertainty"));

    // MEAS-R-054: an azimuth/elevation observation is refracted by an atmosphere the caller supplies
    if (kind == AngleKind::AzEl && !atmosphere) {
        return odl::err("MEAS-F-022", "an azimuth and elevation observation needs the atmosphere the refraction is computed with (an IOD line carries none): pressure, temperature, humidity and wavelength");
    }
    if (atmosphere) {
        auto ok = validate_atmosphere(atmosphere->atmosphere, atmosphere->wavelength_um);
        if (!ok) return odl::err(ok.error());
    }
    return AngleObservation{*epoch, kind, angles->first_rad, angles->second_rad, *sigma, reduction, frame, to_frame, *site, atmosphere};
}

// ---- the models -------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

std::vector<OmittedTerm> omitted_angle_terms(AngleReduction reduction) {
    std::vector<OmittedTerm> out = {
        {"station_displacement", "up to 0.38 m of the observer's position (solid Earth tide), that is 0.38 m / ρ rad: 80 mas at ρ = 1e6 m", "computed: SPEC-measmod §6.4, MEAS-P-20; named follow-on"},
        {"gravitational_light_deflection", "up to 0.57 mas for a ray that grazes the Earth (4 G M / (c² R)), less for the campaigns' ranges", "computed: MEAS-P-24"},
        {"solar_term_in_aberration", "at most 0.4 µas", "read: eraAb's own note"},
    };
    if (reduction == AngleReduction::Astrometric) out.push_back({"aberration_cross_term", "about 0.03 mas (the product of the annual and diurnal aberrations, 1.5e-10 rad)", "computed: SPEC-measmod §4.7"});
    if (reduction == AngleReduction::ApparentRefracted)
        out.push_back({"refraction_model_error", "eraRefco's own \"worst 62 mas, RMS 8 mas\" for optical and infrared wavelengths up to 75° of zenith distance", "read: ERFA's documentation of eraRefco"});
    return out;
}

AngleApplied begin_applied(const AngleObservation& obs, const Emission& em) {
    AngleApplied a;
    a.reduction = obs.reduction;
    a.frame = obs.frame;
    a.light_time_s = em.light_time_s;
    a.range_m = em.range_m;
    a.passes = em.passes;
    a.gcrs_to_frame = obs.gcrs_to_frame;
    a.atmosphere = obs.atmosphere;
    a.omitted = omitted_angle_terms(obs.reduction);
    return a;
}

double angle_between(const Vec3& a, const Vec3& b) noexcept { return std::atan2(a.cross(b).norm(), a.dot(b)); }

}  // namespace

odl::Result<ModelledRaDec, MeasError> model_radec(const AngleObservation& obs, const StationTrack& station, const Trajectory& target, const EarthMotion& motion) {
    if (obs.kind != AngleKind::RaDec) return odl::err(f015("model_radec was given an azimuth and elevation observation"));
    if (obs.reduction == AngleReduction::ApparentRefracted) return odl::err(f015("model_radec was given the reduction ApparentRefracted, which is an azimuth and elevation reduction"));
    if (!std::isfinite(obs.a_rad) || !std::isfinite(obs.b_rad)) return odl::err("MEAS-F-018", "angle model: an observed angle is not finite — no residual of NaN is returned");
    const double c = kSpeedOfLight_m_s;

    auto observer = station.at(obs.epoch);
    if (!observer) return odl::err(with_context(observer.error(), "angle model, the station at the observation epoch"));
    auto em = solve_emission(obs.epoch, observer->position_m, target);
    if (!em) return odl::err(em.error());
    const Vec3& n_geo = em->direction;

    AngleApplied applied = begin_applied(obs, *em);
    Vec3 n = n_geo;
    Mat3 j_reduction = Mat3::identity();
    if (obs.reduction == AngleReduction::Astrometric) {
        auto earth = motion.at(obs.epoch);
        if (!earth) return odl::err(with_context(earth.error(), "angle model, the Earth's motion at the observation epoch"));
        const Vec3 beta = (1.0 / c) * earth->velocity_m_s;
        n = aberration_remove(n_geo, beta);
        j_reduction = aberration_remove_jacobian(n_geo, beta);
        applied.aberration_beta = beta;
        applied.aberration_shift_rad = angle_between(n_geo, n);
    }
    const Vec3 m = obs.gcrs_to_frame.apply(n);
    const double h = std::hypot(m.x, m.y);
    if (h < kPoleLimit_rad * std::sqrt(m.dot(m))) return odl::err(f026(h, std::sqrt(m.dot(m)), "the celestial pole of the observation's frame"));
    const double ra = wrap_two_pi(std::atan2(m.y, m.x));
    const double dec = std::atan2(m.z, h);

    // the row (MEAS-R-060): dα = (x dy − y dx)/h², dδ = (h² dz − z x dx − z y dy)/(h |m|²) for a displacement dm of m
    const Mat3 j = obs.gcrs_to_frame.times(j_reduction).times(geometric_jacobian(n_geo, em->target_velocity_m_s, em->range_m));
    const double m2 = m.dot(m);
    const Vec3 row_ra = (1.0 / (h * h)) * Vec3{-m.y, m.x, 0.0};
    const Vec3 row_dec = (1.0 / (h * m2)) * Vec3{-m.z * m.x, -m.z * m.y, h * h};
    Partials<odl::frames::Frame::GCRS, 2> partials;
    const Vec3 g_ra = row_times(row_ra, j), g_dec = row_times(row_dec, j);
    partials.d[0] = {g_ra.x, g_ra.y, g_ra.z, 0.0, 0.0, 0.0};
    partials.d[1] = {g_dec.x, g_dec.y, g_dec.z, 0.0, 0.0, 0.0};
    return ModelledRaDec(AngleResult(ra, dec, obs.a_rad, obs.b_rad, obs.epoch, em->emission, partials, std::move(applied)));
}

odl::Result<ModelledAzEl, MeasError> model_azel(const AngleObservation& obs, const StationTrack& station, const Trajectory& target, const EarthOrientation& earth) {
    if (obs.kind != AngleKind::AzEl) return odl::err(f015("model_azel was given a right ascension and declination observation"));
    if (obs.reduction != AngleReduction::ApparentRefracted) return odl::err(f015("model_azel needs the reduction ApparentRefracted"));
    if (!std::isfinite(obs.a_rad) || !std::isfinite(obs.b_rad)) return odl::err("MEAS-F-018", "angle model: an observed angle is not finite — no residual of NaN is returned");
    if (!obs.atmosphere) return odl::err("MEAS-F-022", "an azimuth and elevation observation needs the atmosphere the refraction is computed with");
    const double c = kSpeedOfLight_m_s;
    auto refco = refraction_constants(*obs.atmosphere);
    if (!refco) return odl::err(refco.error());

    auto observer = station.at(obs.epoch);
    if (!observer) return odl::err(with_context(observer.error(), "angle model, the station at the observation epoch"));
    auto orientation = earth.at(obs.epoch);
    if (!orientation) return odl::err(with_context(orientation.error(), "angle model, the Earth's orientation at the observation epoch"));
    auto em = solve_emission(obs.epoch, observer->position_m, target);
    if (!em) return odl::err(em.error());
    const Vec3& n_geo = em->direction;

    // the diurnal aberration, then the rotation GCRS → ITRS → the local horizon at the site's geodetic latitude and longitude
    const Vec3 beta_d = (1.0 / c) * observer->velocity_m_s;
    const Vec3 n_app = aberration_apply(n_geo, beta_d);
    const double sl = std::sin(obs.site.location.longitude_rad), cl = std::cos(obs.site.location.longitude_rad);
    const double sp = std::sin(obs.site.location.latitude_rad), cp = std::cos(obs.site.location.latitude_rad);
    Mat3 enu{};
    enu.r[0] = {-sl, cl, 0.0};
    enu.r[1] = {-sp * cl, -sp * sl, cp};
    enu.r[2] = {cp * cl, cp * sl, sp};
    const Mat3 to_local = enu.times(orientation->gcrs_to_itrs);
    const Vec3 loc = to_local.apply(n_app);
    const double h = std::hypot(loc.x, loc.y);
    if (loc.z > 0.0 && h < kPoleLimit_rad * std::sqrt(loc.dot(loc))) return odl::err(f026(h, std::sqrt(loc.dot(loc)), "the zenith"));   // the nadir is F-010's (vacuum zenith distance above 75 degrees)
    const double az = wrap_two_pi(std::atan2(loc.x, loc.y));                // from north (the second local axis) through east (the first)
    const double z_v = std::atan2(h, loc.z);
    if (z_v > kMaxVacuumZenith_rad) {
        std::ostringstream m;
        m << "the vacuum zenith distance " << z_v * 180.0 / kPi << "° exceeds 75°, the limit of the refraction model's documented accuracy (ERFA's eraRefco: \"worst 62 mas, RMS 8 mas\" up to 75°): the refraction is not extrapolated";
        return odl::err("MEAS-F-010", m.str());
    }
    const double z_o = refracted_zenith_distance(z_v, *refco);
    const double el = 0.5 * kPi - z_o;

    AngleApplied applied = begin_applied(obs, *em);
    applied.aberration_beta = beta_d;
    applied.aberration_shift_rad = angle_between(n_geo, n_app);
    applied.refraction_a = refco->a;
    applied.refraction_b = refco->b;
    applied.zenith_vacuum_rad = z_v;
    applied.zenith_observed_rad = z_o;
    applied.refraction_rad = z_v - z_o;

    // the row: the local components of the displacement of the apparent direction, then dα, dz_v and the refraction's dz_o/dz_v
    const Mat3 j = to_local.times(aberration_apply_jacobian(n_geo, beta_d)).times(geometric_jacobian(n_geo, em->target_velocity_m_s, em->range_m));
    const Vec3 row_e = row_of(j, 0), row_n = row_of(j, 1), row_u = row_of(j, 2);
    const Vec3 g_az = (1.0 / (h * h)) * (loc.y * row_e - loc.x * row_n);                   // dα = (e dn − n de)... with e the east and n the north component
    // dz_v = (u dh − h du)/(h² + u²), dh = (e de + n dn)/h
    const Vec3 dh = (1.0 / h) * (loc.x * row_e + loc.y * row_n);
    const Vec3 g_zv = (1.0 / (h * h + loc.z * loc.z)) * (loc.z * dh - h * row_u);
    const double dzo_dzv = refracted_zenith_derivative(z_o, *refco);
    const Vec3 g_el = -dzo_dzv * g_zv;
    Partials<odl::frames::Frame::GCRS, 2> partials;
    partials.d[0] = {g_az.x, g_az.y, g_az.z, 0.0, 0.0, 0.0};
    partials.d[1] = {g_el.x, g_el.y, g_el.z, 0.0, 0.0, 0.0};
    return ModelledAzEl(AngleResult(az, el, obs.a_rad, obs.b_rad, obs.epoch, em->emission, partials, std::move(applied)));
}

}  // namespace odl::measmod
