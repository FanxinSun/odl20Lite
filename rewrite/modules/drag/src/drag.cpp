// drag.cpp — SPEC-drag §4.
//
// a = -0.5 * rho * C_D * (A/m) * v_rel * |v_rel|, standard across every
// orbital-mechanics text this tree could name (Montenbruck & Gill, Vallado,
// Vallado & McClain) -- momentum flux (rho |v_rel|) times reference area times
// a coefficient, along the incoming relative-wind direction, the same shape as
// aerodynamic drag anywhere else. Re-derived in the spec header, not only
// cited (plan rule 8's converse): it is dynamic pressure times an area times a
// dimensionless coefficient, and dimensional analysis alone fixes the form up
// to that one coefficient, which is what C_D exists to carry.

#include <odl/drag/drag.hpp>
#include <odl/drag/geodetic.hpp>
#include <odl/core/units.hpp>
#include <odl/frames/transform.hpp>

#include <cmath>
#include <numbers>
#include <string>

namespace odl::drag {

namespace detail {

/// Gregorian day-of-year, 1-366. No existing utility in this tree computes
/// one (checked: atmosphere's own tests state it as a literal); this is the
/// standard cumulative-month-day calculation, verified by SPEC-drag DRAG-A-009
/// against the Gregorian leap-year rule's own three named exceptions (1900,
/// 2000, 2100) rather than trusted from memory.
///
/// In `detail`, not anonymous: 1900 and 2100 are outside the leap table's
/// representable UTC range (before 1972, or past any realistic table
/// expiry -- SPEC-time TIME-F-002/TIME-F-004), so DRAG-A-009 calls this
/// function directly rather than routing the century exceptions through an
/// `Epoch` that cannot represent them. Not part of this module's public
/// contract: `acceleration` and `Drag` are the surface every other caller
/// uses, and both compute this internally.
int day_of_year(int year, int month, int day) noexcept {
    constexpr int kCumulative[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    const bool leap = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
    int doy = kCumulative[month - 1] + day;
    if (leap && month > 2) doy += 1;
    return doy;
}

}  // namespace detail

odl::Result<DragResult, DragError>
acceleration(const odl::time::Epoch& t, const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
            double c_d, double area_m2, double mass_kg, const atmosphere::SpaceWeather& sw,
            const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps,
            bool require_verified) {
    if (require_verified && sw.verification != atmosphere::Verification::verified_against_issuer)
        return odl::err(DragError{"DRAG-F-001",
                         "drag acceleration requested with require_verified=true, but the "
                         "space-weather sample carries 'unverified_redistribution' -- the same "
                         "reason SPEC-atmosphere ATMO-F-015 refuses a direct sample() call"});

    if (!(mass_kg > 0.0))
        return odl::err(DragError{"DRAG-F-002", "mass must be positive, got " +
                         std::to_string(mass_kg) + " kg"});
    if (!(area_m2 > 0.0))
        return odl::err(DragError{"DRAG-F-002", "area must be positive, got " +
                         std::to_string(area_m2) + " m^2"});

    // --- DRAG-R-002: the co-rotating atmosphere, via the SAME GCRS<->ITRS
    // chain SPEC-frames already built and verified, not a hand-rolled
    // omega x r. ITRS velocity IS the velocity relative to a rigidly
    // co-rotating atmosphere, by construction -- a point fixed to the solid
    // Earth has zero ITRS velocity, exactly what "co-rotating" means.
    //
    // THE CROSSING (DYN-R-011's discipline, core/units.hpp named at each
    // site): this module's own interface is metres throughout, matching
    // dyn::Force's convention, but frames::State<F> is km throughout,
    // matching SPEC-frames's. Two crossings per entry point: km in, km out.
    const frames::GcrsState gcrs_state{t, odl::km_from_metres(r_gcrs_m),
                                       odl::km_from_metres(v_gcrs_m_per_s)};
    auto itrs_r = frames::to_itrs(gcrs_state, eop, leaps);
    if (!itrs_r.has_value())
        return odl::err(DragError{"DRAG-F-003", "GCRS to ITRS transform failed: " +
                         itrs_r.error().message});
    const Vec3 r_itrs = odl::metres_from_km(itrs_r->position());
    const Vec3 v_rel = odl::metres_from_km(itrs_r->velocity());   // co-rotating-frame velocity

    const double speed = v_rel.norm();
    if (!(speed > 0.0))
        return odl::err(DragError{"DRAG-F-004",
                         "zero velocity relative to the co-rotating atmosphere: drag has no "
                         "well-defined direction"});

    // --- geodetic point under the satellite, for the atmosphere model
    const auto geo = itrs_to_geodetic(r_itrs);

    auto cal = t.calendar(odl::time::TimeScale::UTC, leaps);
    if (!cal.has_value())
        return odl::err(DragError{"DRAG-F-003", "epoch to UTC calendar failed: " + cal.error().message});
    const double seconds_of_day = cal->hour * 3600.0 + cal->minute * 60.0 + cal->second;

    atmosphere::Place place;
    place.day_of_year = detail::day_of_year(cal->year, cal->month, cal->day);
    place.seconds_of_day = seconds_of_day;
    place.geodetic_latitude_deg = geo.latitude_rad * 180.0 / std::numbers::pi;
    place.longitude_deg = geo.longitude_rad * 180.0 / std::numbers::pi;
    place.altitude_km = odl::km_from_metres(geo.altitude_m);   // THE CROSSING (DYN-R-011's discipline)

    auto rho = atmosphere::for_drag(place, sw);
    if (!rho.has_value())
        return odl::err(DragError{"DRAG-F-005", "atmosphere::for_drag refused: " + rho.error().message});

    // --- DRAG-R-001: a = -0.5 rho C_D (A/m) v_rel |v_rel|, in ITRS (the
    // co-rotating frame is where v_rel lives), then rotated back to GCRS. A
    // rotation commutes with the force law here: the law is built entirely
    // from v_rel's own magnitude and direction, both frame-independent under a
    // pure rotation, so computing in ITRS and rotating the RESULT back gives
    // the identical vector to computing "in GCRS" by any route that respects
    // the same physical v_rel -- there is only one such route.
    const double scale = -0.5 * rho->total_mass_kg_m3 * c_d * (area_m2 / mass_kg) * speed;
    const Vec3 a_itrs = scale * v_rel;

    auto rot = frames::gcrs_to_itrs(t, eop, leaps);
    if (!rot.has_value())
        return odl::err(DragError{"DRAG-F-003", "GCRS to ITRS rotation failed: " + rot.error().message});
    const Mat3& M = rot->m;                                     // GCRS -> ITRS
    const Vec3 a_gcrs = M.transpose().apply(a_itrs);             // ITRS -> GCRS: M^T

    return DragResult{a_gcrs, rho->record};
}

// --------------------------------------------------------------------------

Drag::Drag(dyn::ParameterId c_d_id, double area_m2, double mass_kg,
          atmosphere::SpaceWeather sw, odl::eop::EopRecord eop, odl::time::LeapTable leaps)
    : c_d_id_(c_d_id), area_m2_(area_m2), mass_kg_(mass_kg), sw_(std::move(sw)),
      eop_(std::move(eop)), leaps_(std::move(leaps)), consumes_{c_d_id} {}

dyn::ForceId Drag::id() const { return dyn::ForceId{"drag"}; }

const std::vector<dyn::ParameterId>& Drag::consumes() const { return consumes_; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Drag::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
           const Vec3& v_m_per_s, const dyn::ParameterSet& params,
           const dyn::ParameterRegistry& registry) const {
    auto c_d = params.value(c_d_id_);
    if (!c_d.has_value())
        return odl::err(dyn::DynError{"DRAG-F-006", "drag_coefficient parameter not set "
                         "in this ParameterSet: " + c_d.error().message});

    const Vec3 r = r_m.metres();
    auto result = acceleration(t, r, v_m_per_s, *c_d, area_m2_, mass_kg_, sw_, eop_, leaps_);
    if (!result.has_value())
        return odl::err(dyn::DynError{result.error().id, result.error().message});

    // --- DRAG-R-003: the velocity Jacobian, exact and analytic. a is
    // LINEAR in the tensor sense in v_rel through v_rel|v_rel|, whose exact
    // derivative is |v| I + (v (x) v)/|v| -- verified in PROVENANCE.md §28
    // by direct differentiation, not assumed. Density is NOT a function of
    // velocity, so this is the WHOLE velocity dependence, no approximation.
    // THE CROSSING, again (DYN-R-011): km in to frames::State, m out.
    auto itrs_r = frames::to_itrs(
        frames::GcrsState{t, odl::km_from_metres(r), odl::km_from_metres(v_m_per_s)}, eop_, leaps_);
    if (!itrs_r.has_value())
        return odl::err(dyn::DynError{"DRAG-F-003", itrs_r.error().message});
    const Vec3 v_rel = odl::metres_from_km(itrs_r->velocity());
    const double speed = v_rel.norm();

    auto rot = frames::gcrs_to_itrs(t, eop_, leaps_);
    if (!rot.has_value()) return odl::err(dyn::DynError{"DRAG-F-003", rot.error().message});
    const Mat3& M = rot->m;

    atmosphere::Place place_for_rho;
    {
        const auto itrs_pos = odl::metres_from_km(itrs_r->position());
        const auto geo = itrs_to_geodetic(itrs_pos);
        auto cal = t.calendar(odl::time::TimeScale::UTC, leaps_);
        if (!cal.has_value()) return odl::err(dyn::DynError{"DRAG-F-003", cal.error().message});
        place_for_rho.day_of_year = detail::day_of_year(cal->year, cal->month, cal->day);
        place_for_rho.seconds_of_day = cal->hour * 3600.0 + cal->minute * 60.0 + cal->second;
        place_for_rho.geodetic_latitude_deg = geo.latitude_rad * 180.0 / std::numbers::pi;
        place_for_rho.longitude_deg = geo.longitude_rad * 180.0 / std::numbers::pi;
        place_for_rho.altitude_km = odl::km_from_metres(geo.altitude_m);   // THE CROSSING
    }
    auto rho_result = atmosphere::for_drag(place_for_rho, sw_);
    if (!rho_result.has_value())
        return odl::err(dyn::DynError{"DRAG-F-005", rho_result.error().message});
    const double rho = rho_result->total_mass_kg_m3;

    const double coeff = -0.5 * rho * (*c_d) * (area_m2_ / mass_kg_);
    Mat3 dadv_itrs{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) {
            const double vi = (i == 0 ? v_rel.x : i == 1 ? v_rel.y : v_rel.z);
            const double vj = (j == 0 ? v_rel.x : j == 1 ? v_rel.y : v_rel.z);
            dadv_itrs.r[i][j] = coeff * ((i == j ? speed : 0.0) + vi * vj / speed);
        }
    // chain through the rotation: da_gcrs/dv_gcrs = M^T . dadv_itrs . M
    // (dv_itrs/dv_gcrs = M at fixed position -- SPEC-drag DRAG-R-003's own note)
    const Mat3 dadv_gcrs = M.transpose().times(dadv_itrs).times(M);

    // --- DRAG-R-004: the position Jacobian's RADIAL part, from a locally
    // exponential atmosphere (scale height H = -rho / (d rho / d altitude),
    // itself estimated by ONE extra atmosphere call -- an approximation
    // NAMED as one, standard in orbit determination (GEODYN, Bernese and
    // similar tools use the same local-exponential partial), with the
    // lateral (latitude/longitude/local-time) gradient NEGLECTED and its
    // rough size recorded in PROVENANCE.md §28 rather than silently assumed
    // zero.
    Mat3 dadr{};
    {
        constexpr double kAltStepKm = 1.0;   // DRAG-P-1: pre-registered, not fitted
        atmosphere::Place bumped = place_for_rho;
        bumped.altitude_km += kAltStepKm;
        auto rho_up = atmosphere::for_drag(bumped, sw_);
        if (rho_up.has_value()) {
            const double d_rho_d_alt_m =
                (rho_up->total_mass_kg_m3 - rho) / odl::metres_from_km(kAltStepKm);   // THE CROSSING
            const Vec3 itrs_pos_m = odl::metres_from_km(itrs_r->position());
            const double r_norm = itrs_pos_m.norm();
            const Vec3 r_hat = (1.0 / r_norm) * itrs_pos_m;
            // d(a)/d(r) ~ (d(rho)/d(alt)) * r_hat (x) d(a)/d(rho).
            // a = coeff * speed * v_rel (coeff itself proportional to rho,
            // everything else in it independent of rho), so
            // d(a)/d(rho) = (coeff/rho) * speed * v_rel -- DRAG-A-010 found
            // this missing its speed factor: the first draft wrote
            // (coeff/rho)*v_rel, off by exactly |v_rel| (~7.7 km/s at LEO,
            // matching the ~7300x discrepancy the finite-difference check
            // measured before the fix).
            const Vec3 a_direction = (coeff / rho) * speed * v_rel;
            Mat3 dadr_itrs{};
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) {
                    const double ai = (i == 0 ? a_direction.x : i == 1 ? a_direction.y : a_direction.z);
                    const double rj = (j == 0 ? r_hat.x : j == 1 ? r_hat.y : r_hat.z);
                    dadr_itrs.r[i][j] = ai * d_rho_d_alt_m * rj;
                }
            dadr = M.transpose().times(dadr_itrs).times(M);
        }
        // if the bumped-altitude call refused (e.g. at a boundary), dadr stays
        // zero -- a declared, visible degradation, not a silent one: DRAG-A-009
        // asserts the un-bumped call always succeeds where this matters.
    }

    // --- DRAG-R-005: d(a)/d(C_D) is exact and trivial -- a is LINEAR in
    // C_D, so d(a)/d(C_D) = a / C_D. Computed directly from the already-known
    // acceleration rather than by dividing (avoiding a spurious C_D~0 blowup
    // in the formula, even though physically C_D is never allowed near zero).
    dyn::ParameterJacobian dparam(registry);
    const Vec3 dadc = (1.0 / (*c_d)) * result->acceleration_m_s2;
    auto set_r = dparam.set_column(c_d_id_, dadc);
    if (!set_r.has_value()) return odl::err(set_r.error());

    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{result->acceleration_m_s2},
        dyn::StateJacobian::with_velocity(dadr, dadv_gcrs),
        std::move(dparam)};
}

}  // namespace odl::drag
