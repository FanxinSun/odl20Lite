// erp.cpp — SPEC-photon-pressure PHPR-R-008..R-011.
//
// THE CAP INTEGRAL, RS09 Eq. (2.24)/(2.31) (reflected/emitted), generalised
// from a constant albedo to albedo(lat,lon,t) and emissivity(lat,lon,t)
// (PHPR-R-009), and integrated in NADIR-CENTRED coordinates rather than
// RS09's own literal grid -- the manager read this file, found the previous
// global-(theta,phi) grid's own convergence ratio unusable as `PHPR-P-1`'s
// discriminator (PROVENANCE.md's own step-5 entry has the measured numbers),
// and diagnosed why: a "staircase" boundary. This header comment re-derives
// the fix from the actual visibility geometry, checked against RS09 itself
// (its own Eq. 2.41-2.44, SPEC-photon-pressure SS2's own citation) rather
// than assumed -- and RS09's OWN grid turns out not to be this one:
// their (theta,phi) is a colatitude/longitude pair about an axis
// PERPENDICULAR to the satellite-Earth-Sun plane (chosen so both r_hat and
// s_hat fall in one coordinate plane, which keeps their own gamma formula
// short), integrated over that same kind of if-gated global domain this
// file's own previous version already matched. So the fix below is not
// "align with RS09's literal scheme" -- RS09 states no convergence-order
// analysis to align with -- it is an independent numerical-analysis
// improvement, re-derived here, that happens to still compute the exact
// same integral RS09 defines (same domain of nonzero integrand, same
// dA, same dE_refl/dE_emit), only reparameterised.
//
// THE VARIABLE NAMES ARE DELIBERATELY NOT theta/phi/psi: `SPEC-photon-
// pressure` already uses theta for the kernel's own incidence angle (SS4.1)
// and psi for the Sun-Earth-satellite angle (SS3, RS09's own symbol) -- reusing
// either for a grid coordinate would collide with an established meaning.
// The per-cell colatitude from the SUB-SATELLITE point is called CHI below
// (and in the spec); the azimuth about the same nadir axis is AZ.
//
// THE GEOMETRY. Let r_hat be the unit vector from Earth's centre to the
// satellite. A surface element at colatitude chi FROM r_hat (chi in
// [0, pi], not from any inertial pole) and azimuth az about r_hat has
//
//   n_hat(chi, az) = cos(chi) r_hat + sin(chi) (cos(az) x_hat + sin(az) y_hat)
//
// for any right-handed orthonormal (x_hat, y_hat, r_hat). By the problem's
// own rotational symmetry about r_hat (the satellite sits ON that axis, the
// Earth is a sphere centred at the origin), the angle between n_hat and the
// direction to the satellite -- RS09's own theta, `cos_theta_cell` below --
// is a function of chi ALONE, independent of az: writing out the triangle
// Earth-centre / surface-point / satellite (sides R_E, r_sat, included angle
// chi) gives, exactly,
//
//   cos_theta_cell(chi) = (r_sat cos(chi) - R_E) / d(chi),
//   d(chi) = sqrt(R_E^2 + r_sat^2 - 2 R_E r_sat cos(chi))            (law of cosines)
//
// which is EXACTLY ZERO at chi = beta = arccos(R_E/r_sat) -- `PHPR-A-010`'s
// own cap half-angle -- because r_sat cos(beta) = R_E by beta's own
// definition, and is POSITIVE for chi < beta, negative for chi > beta (the
// numerator is monotonically decreasing in chi over [0, pi], d(chi) > 0
// throughout since r_sat > R_E strictly). So integrating chi over
// [0, beta] instead of [0, pi] makes the visibility condition EXACT AT THE
// DOMAIN'S OWN EDGE, not a gate tested inside it: no indicator function
// multiplies the integrand anywhere in the domain, so the integrand
// (cos_theta_cell(chi), and dA below) is SMOOTH -- analytic, in fact --
// over the whole closed interval, including at chi = beta itself, where it
// has a SIMPLE (transverse) zero: d(cos_theta_cell)/d(chi) at beta is
// -r_sat sin(beta)/d(beta), nonzero for any real satellite altitude. A
// midpoint-rule quadrature of a smooth integrand with a simple zero at one
// endpoint of a FIXED domain converges at the rule's own full order, since
// this fixed domain does not extend past the zero the way the old global
// grid's gate-inside-a-larger-domain construction effectively did --
// `PHPR-A-007`'s own convergence ratio predicts a clean 4x per halving,
// checked, not merely hoped for, against the actual measured numbers.
//
// This also fixes the previous grid's own waste (a LEO cap is ~3% of the
// full sphere; integrating [0, pi] spent ~97% of every cell on a region
// that always failed the old gate) -- chi's own domain is now exactly the
// cap, nothing outside it is ever visited.
//
// cos_gamma_cell (the Sun's own visibility, gating dE_refl only) is NOT a
// clean function of chi alone: n_hat's own azimuth az turns relative to
// x_hat/y_hat, two directions with no necessary relationship to the Sun (see
// THE FRAME below), so the terminator (cos_gamma = 0) is some curve in
// (chi, az) generally crossing grid cells at an angle -- it still
// staircases THIS grid, within the cap. This is real and UNRESOLVED here
// (`PHPR-Q-004` in this spec's own open questions) -- `PHPR-A-007` isolates
// the emitted term alone (constant_albedo forced to zero) specifically
// because that term has no cos_gamma gate at all and so is NOT subject to
// this remaining problem; a convergence claim for the REFLECTED term's own
// accuracy is not made by this entry.
//
// THE FRAME. r_hat is normalize(r_sat_gcrs_m), computed directly from this
// function's OWN r_sat_gcrs_m argument -- NOT read from m_gcrs_to_body's own
// rows, even though in production (Erp::accel_only) that frame's own
// z_body row already is -r_hat (attitude.cpp's own nadir-pointing
// construction): a direct caller (`PHPR-A-007`'s own convergence sweep
// among them) is free to pass m_gcrs_to_body = identity, unrelated to
// r_sat_gcrs_m, to make the returned BODY-frame force directly comparable
// to a GCRS direction without an extra rotation in the test -- coupling the
// cap's own pole to that argument's rows would silently break exactly that,
// and did, the first time this was written (caught by `PHPR-A-007` itself
// failing outright, not a subtle drift). x_hat/y_hat need only be SOME
// right-handed orthonormal pair perpendicular to r_hat -- the integral's own
// value cannot depend on which one, only the discretisation's own staircase
// PHASE relative to the terminator does, which this entry does not claim to
// control (`PHPR-Q-004` again) -- so they are built the ordinary
// always-defined way, crossing r_hat with whichever GCRS axis is LEAST
// parallel to it (never degenerate, unlike tying x_hat to the Sun's own
// direction, which -- as this file's own first version of this comment
// wrongly proposed -- is exactly degenerate in `PHPR-A-007`'s own test
// geometry, Sun and satellite along the same direction).
//
//   d_vec = r_sat - r_cell,  d = |d_vec|,  e_hat = d_vec / d         (RS09's e-hat)
//   cos_gamma_cell = s_hat . n_hat        (Sun above the cell's horizon)
//   dA = R_E^2 sin(chi) d(chi) d(az)
//
//   dE_refl = albedo/(pi d^2)   * cos_theta_cell * cos_gamma_cell * S * dA   [cos_gamma_cell>0]
//   dE_emit = emissivity/(4pi d^2) * cos_theta_cell               * S * dA
//
// dE_emit has NO cos_gamma gate -- RS09's own emitted-radiation model is
// uniform over the whole Earth, night side included (its own text: "there is
// no dependency on the angle psi"), which is what a thermal-emission model
// with no local solar dependence should give. Nor does it gate on
// cos_theta_cell any more: chi's own domain [0, beta] already guarantees it
// positive (the defensive check below is belt-and-suspenders, not the
// mechanism that fixes the staircase -- the DOMAIN is).
//
// source_direction_body for this cell's own contribution is the direction
// FROM THE SPACECRAFT TO THE CELL (this codebase's own e_D convention,
// srp_analytic.cpp's own header comment) -- the cell is the source of ITS
// OWN contribution, exactly PHPR-R-002's two-direction design put to use a
// second time.
//
// LAT/LON FED TO THE `albedo`/`emissivity` CALLBACKS: unchanged in kind from
// the previous version, still GCRS-frame spherical angles (latitude measured
// from the GCRS equator, longitude from the GCRS x-axis), NOT true Earth-
// fixed (ITRS/geodetic) latitude/longitude -- inert under this module's own
// `constant_albedo_0_3`/`constant_emissivity_0_7` (which ignore both
// arguments), but a real gap this entry does not close: a future
// lat/lon-varying model (Knocke's own zonal/seasonal one, named as the next
// refinement when this step's scope was set) would need Earth-fixed
// coordinates, which means an ITRS rotation this function does not yet take.
// Flagged, not fixed -- no test exercises it today, and fixing it is a
// second, separable change (`PHPR-Q-005`).

#include <odl/erp/erp.hpp>

#include <odl/core/units.hpp>
#include <odl/frames/vector.hpp>

#include <cmath>
#include <numbers>

namespace odl::erp {
namespace {

inline constexpr double kSolarIrradianceAt1AuWPerM2 = 1367.0;
/// RS09's own spherical-Earth radius (Eq. 2.25-2.26's own "R_E = 6371 km"),
/// the mean radius, not WGS84's equatorial 6378137.0 m -- this cap integral
/// is a spherical approximation throughout (PHPR-R-008), and 6371 km is the
/// value that reproduces PHPR-P-4's own cited 76.02 deg/37.92% at GNSS
/// altitude; the equatorial value would not.
inline constexpr double kEarthRadiusM = 6371000.0;

/// PHPR-P-6: NOT YET MEASURED, unlike Srp's own position step
/// (`PHPR-A-017`) -- this entry's own scope was the cap integral's
/// coordinate system and Srp's own velocity Jacobian, not every remaining
/// finite difference in this force family (the spec's own PHPR-P-6 says so
/// plainly rather than leaving the gap implicit).
inline constexpr double kPositionStepM = 100.0;
inline constexpr double kVelocityStepMPerS = 0.01;

dyn::DynError forward(std::string_view id, const std::string& message) {
    return dyn::DynError{id, message};
}

/// SOME right-handed orthonormal pair perpendicular to a unit `axis`, always
/// defined (`erp.cpp`'s own header comment, THE FRAME): crossing `axis` with
/// whichever GCRS basis vector it is LEAST aligned with never degenerates,
/// since a unit vector cannot be nearly parallel to all three at once (the
/// largest of its own three squared components is always >= 1/3).
struct PerpendicularPair { Vec3 x, y; };
PerpendicularPair perpendicular_pair(const Vec3& axis) {
    const Vec3 candidates[3] = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    std::size_t least_aligned = 0;
    double best_abs_dot = std::abs(axis.dot(candidates[0]));
    for (std::size_t k = 1; k < 3; ++k) {
        const double abs_dot = std::abs(axis.dot(candidates[k]));
        if (abs_dot < best_abs_dot) { best_abs_dot = abs_dot; least_aligned = k; }
    }
    const Vec3 cross = axis.cross(candidates[least_aligned]);
    const Vec3 x = (1.0 / cross.norm()) * cross;
    const Vec3 y = axis.cross(x);
    return PerpendicularPair{x, y};
}

}  // namespace

double constant_albedo_0_3(double, double, const odl::time::Epoch&) { return 0.3; }
double constant_emissivity_0_7(double, double, const odl::time::Epoch&) { return 0.7; }

odl::Result<Vec3, ErpError>
cap_integral(const macromodel::Macromodel& model, const Vec3& r_sat_gcrs_m,
            const macromodel::BodyDirection& sun_direction_body,
            const Vec3& v_rel_source_body_m_per_s, const Mat3& m_gcrs_to_body,
            const odl::time::Epoch& t, const SurfaceProperty& albedo,
            const SurfaceProperty& emissivity, int n_chi, int n_az) {
    if (n_chi < 1 || n_az < 1)
        return odl::err(ErpError{"PHPR-F-004", "cap_integral called with a non-positive grid "
                         "dimension"});

    const double r_sat_m = r_sat_gcrs_m.norm();
    if (!(r_sat_m > kEarthRadiusM))
        return odl::err(ErpError{"PHPR-F-005",
                         "cap_integral called with the satellite at or below the spherical Earth's "
                         "own radius: |r_sat| = " + std::to_string(r_sat_m) + " m against a radius "
                         "of " + std::to_string(kEarthRadiusM) + " m -- beta = arccos(R_E/r_sat) is "
                         "not defined there"});
    const double beta = std::acos(kEarthRadiusM / r_sat_m);   // PHPR-A-010's own cap half-angle

    // The frame this function integrates in: r_hat (the cap's own pole,
    // straight from THIS function's own r_sat_gcrs_m, not from
    // m_gcrs_to_body -- this file's own header comment, THE FRAME, says
    // why) and x_hat/y_hat, some always-defined perpendicular pair (their
    // own particular orientation does not affect the integral's value, only
    // the terminator's own staircase phase, `PHPR-Q-004`).
    const Vec3 r_hat = (1.0 / r_sat_m) * r_sat_gcrs_m;
    const auto [x_hat, y_hat] = perpendicular_pair(r_hat);

    // The Sun's direction, GCRS -- one shared direction for every cell
    // (negligible solar parallax across Earth's own disk, ~4e-5 rad).
    const Vec3 s_hat_gcrs = m_gcrs_to_body.transpose().apply(sun_direction_body.vec());

    const double d_chi = beta / static_cast<double>(n_chi);
    const double d_az = 2.0 * std::numbers::pi / static_cast<double>(n_az);

    Vec3 total{0.0, 0.0, 0.0};
    for (int i = 0; i < n_chi; ++i) {
        const double chi = (static_cast<double>(i) + 0.5) * d_chi;
        const double sin_chi = std::sin(chi);
        const double cos_chi = std::cos(chi);
        for (int j = 0; j < n_az; ++j) {
            const double az = (static_cast<double>(j) + 0.5) * d_az;
            const Vec3 n_hat = cos_chi * r_hat +
                (sin_chi * std::cos(az)) * x_hat + (sin_chi * std::sin(az)) * y_hat;
            const Vec3 r_cell = kEarthRadiusM * n_hat;

            const Vec3 d_vec = r_sat_gcrs_m - r_cell;
            const double d = d_vec.norm();
            if (!(d > 0.0)) continue;   // the satellite is AT this cell; not a real geometry
            const Vec3 e_hat = (1.0 / d) * d_vec;

            const double cos_theta_cell = e_hat.dot(n_hat);
            // chi in (0, beta) strictly (midpoint sampling) already makes
            // this positive -- this file's own header comment proves it --
            // so this is a defensive invariant check, not the active gate
            // the pre-nadir-centred version needed.
            if (!(cos_theta_cell > 0.0)) continue;   // this cell cannot see the satellite

            const double cos_gamma_cell = s_hat_gcrs.dot(n_hat);
            const double d_area = kEarthRadiusM * kEarthRadiusM * sin_chi * d_chi * d_az;

            // GCRS-frame spherical angles of n_hat itself, NOT chi/az (which
            // are nadir-relative, not GCRS-pole-relative) -- this file's own
            // header comment on what these feed and their own limitation.
            const double lat_rad = std::asin(n_hat.z);
            const double lon_rad = std::atan2(n_hat.y, n_hat.x);

            const Vec3 source_direction_gcrs = (-1.0 / d) * d_vec;   // spacecraft -> cell
            auto source_direction_body =
                macromodel::body_direction(m_gcrs_to_body.apply(source_direction_gcrs));
            if (!source_direction_body.has_value())
                return odl::err(ErpError{source_direction_body.error().id,
                                 source_direction_body.error().message});

            if (cos_gamma_cell > 0.0) {
                const double alpha = albedo(lat_rad, lon_rad, t);
                const double dE_refl = (alpha / (std::numbers::pi * d * d)) * cos_theta_cell *
                                       cos_gamma_cell * kSolarIrradianceAt1AuWPerM2 * d_area;
                if (dE_refl > 0.0) {
                    auto irr = macromodel::irradiance_w_per_m2(dE_refl);
                    if (!irr.has_value())
                        return odl::err(ErpError{irr.error().id, irr.error().message});
                    auto f = srp_analytic::photon_force(model, *irr, macromodel::Band::visible,
                                                        *source_direction_body, sun_direction_body,
                                                        v_rel_source_body_m_per_s);
                    if (!f.has_value()) return odl::err(ErpError{f.error().id, f.error().message});
                    total = total + *f;
                }
            }

            const double epsilon = emissivity(lat_rad, lon_rad, t);
            const double dE_emit = (epsilon / (4.0 * std::numbers::pi * d * d)) * cos_theta_cell *
                                   kSolarIrradianceAt1AuWPerM2 * d_area;
            if (dE_emit > 0.0) {
                auto irr = macromodel::irradiance_w_per_m2(dE_emit);
                if (!irr.has_value())
                    return odl::err(ErpError{irr.error().id, irr.error().message});
                auto f = srp_analytic::photon_force(model, *irr, macromodel::Band::infrared,
                                                    *source_direction_body, sun_direction_body,
                                                    v_rel_source_body_m_per_s);
                if (!f.has_value()) return odl::err(ErpError{f.error().id, f.error().message});
                total = total + *f;
            }
        }
    }
    return total;
}

Erp::Erp(macromodel::Macromodel model, const eph::Ephemeris& ephemeris, odl::time::LeapTable leaps,
        SurfaceProperty albedo, SurfaceProperty emissivity, int n_chi, int n_az)
    : model_(std::move(model)), ephemeris_(ephemeris), leaps_(std::move(leaps)),
      albedo_(std::move(albedo)), emissivity_(std::move(emissivity)), n_chi_(n_chi),
      n_az_(n_az), consumes_{} {}

dyn::ForceId Erp::id() const { return dyn::ForceId{"erp"}; }

const std::vector<dyn::ParameterId>& Erp::consumes() const { return consumes_; }

odl::Result<Vec3, dyn::DynError>
Erp::accel_only(const odl::time::Epoch& t, const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s) const {
    auto sun_state = ephemeris_.geocentric_state(eph::Body::Sun, t, leaps_);
    if (!sun_state.has_value())
        return odl::err(forward("PHPR-F-003",
            "the Sun's ephemeris state was unavailable at the requested epoch: " +
            sun_state.error().message));
    const Vec3 r_sun_gcrs_m = odl::metres_from_km(sun_state->position());
    const Vec3 to_sun_m = r_sun_gcrs_m - r_gcrs_m;
    const double r_sat_sun_m = to_sun_m.norm();

    // PHPR-R-011: the source is the Earth, so velocity relative to it is the
    // spacecraft's own GCRS velocity, unchanged -- no subtraction the way
    // Srp's own step 4 needs, because GCRS already is Earth-relative.
    const Vec3& v_rel_gcrs_m_s = v_gcrs_m_per_s;

    auto frame = attitude::nominal_yaw_steering(r_gcrs_m, to_sun_m);
    if (!frame.has_value())
        return odl::err(forward(frame.error().id, frame.error().message));
    const Vec3 to_sun_unit = (1.0 / r_sat_sun_m) * to_sun_m;
    auto sun_direction_body = macromodel::body_direction(frame->apply(to_sun_unit));
    if (!sun_direction_body.has_value())
        return odl::err(forward(sun_direction_body.error().id, sun_direction_body.error().message));
    const Vec3 v_rel_body_m_s = frame->apply(v_rel_gcrs_m_s);

    // PHPR-R-006: no shadow factor -- the Earth's own radiation is not
    // self-shadowed by the geometry the cap integral already restricts to
    // the visible cap.
    auto force_n = cap_integral(model_, r_gcrs_m, *sun_direction_body, v_rel_body_m_s, *frame, t,
                                albedo_, emissivity_, n_chi_, n_az_);
    if (!force_n.has_value())
        return odl::err(forward(force_n.error().id, force_n.error().message));

    const double mass_kg = model_.mass_kg().value();
    const Vec3 a_body_m_s2 = (1.0 / mass_kg) * (*force_n);
    return frame->transpose().apply(a_body_m_s2);
}

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Erp::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
          const Vec3& v_m_per_s, const dyn::ParameterSet&, const dyn::ParameterRegistry&) const {
    auto centre = accel_only(t, r_m.metres(), v_m_per_s);
    if (!centre.has_value()) return odl::err(centre.error());

    // PHPR-R-011: velocity dependence sized (PHPR-P-2), not yet asserted
    // with_velocity -- Earth-relative aberration is real but smaller than
    // SRP's own, by the ratio of Earth's own irradiance to the Sun's.
    // no_velocity_dependence's own bound is that measured ratio, not zero.
    Mat3 da_dr{};
    const Vec3 axis_unit[3] = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 dr = kPositionStepM * axis_unit[static_cast<std::size_t>(axis)];
        auto r_plus = accel_only(t, r_m.metres() + dr, v_m_per_s);
        auto r_minus = accel_only(t, r_m.metres() - dr, v_m_per_s);
        if (!r_plus.has_value()) return odl::err(r_plus.error());
        if (!r_minus.has_value()) return odl::err(r_minus.error());
        const Vec3 dcol_r = (1.0 / (2.0 * kPositionStepM)) * (*r_plus - *r_minus);
        da_dr.r[0][static_cast<std::size_t>(axis)] = dcol_r.x;
        da_dr.r[1][static_cast<std::size_t>(axis)] = dcol_r.y;
        da_dr.r[2][static_cast<std::size_t>(axis)] = dcol_r.z;
    }

    // The neglected velocity term's own bound, PHPR-P-2: measured directly
    // rather than assumed, the same way the position Jacobian above is
    // computed rather than guessed -- a central difference of accel_only
    // across kVelocityStepMPerS, its norm the bound this force declares.
    auto v_plus = accel_only(t, r_m.metres(), v_m_per_s + kVelocityStepMPerS * axis_unit[0]);
    auto v_minus = accel_only(t, r_m.metres(), v_m_per_s - kVelocityStepMPerS * axis_unit[0]);
    double neglected_bound_per_s = 0.0;
    if (v_plus.has_value() && v_minus.has_value()) {
        const Vec3 dcol_v = (1.0 / (2.0 * kVelocityStepMPerS)) * (*v_plus - *v_minus);
        neglected_bound_per_s = dcol_v.norm();
    }

    dyn::ParameterJacobian pj(dyn::ParameterRegistry{});
    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{*centre},
        dyn::StateJacobian::no_velocity_dependence(da_dr, neglected_bound_per_s),
        pj,
        std::nullopt,
    };
}

}  // namespace odl::erp
