// srp.cpp — SPEC-photon-pressure PHPR-R-005..R-007.

#include <odl/srp/srp.hpp>

#include <odl/core/units.hpp>
#include <odl/macromodel/irradiance.hpp>
#include <odl/shadow/conical.hpp>
#include <odl/srp_analytic/srp_analytic.hpp>

#include <cmath>

namespace odl::srp {
namespace {

/// RHS12 Eq. (6)'s own value, 1367 W/m^2 at 1 au -- the same constant
/// srp_analytic.cpp's own file-local one names, duplicated here rather than
/// exposed from that file, because PHPR-R-001's whole point is that this
/// plugin supplies its OWN, distance-scaled irradiance instead of using it.
inline constexpr double kSolarIrradianceAt1AuWPerM2 = 1367.0;

/// PHPR-P-6.  The step for the one REMAINING finite-difference Jacobian,
/// d(a)/d(r) (d(a)/d(v) is analytic since PHPR-R-010, no step to choose).
/// SIZED, not guessed (plan rule 7): `srp_tests.cpp`'s own `PHPR-A-017`
/// sweeps this central-difference step from 1e4 m down to 1e-2 m against
/// the true pipeline and finds the shape rule 7 predicts -- truncation
/// error (~h^2) falling as h shrinks, until round-off (~machine-epsilon *
/// |a| / h) takes over, with a broad plateau of near-minimum total error in
/// between. 100 m sits inside that plateau, which is what the sweep checks,
/// not merely a cube-root estimate (h_opt ~ eps_mach^(1/3) * (orbital
/// position scale ~1e7 m) ~ 60 m puts it in the same place, but an estimate
/// is not a measurement).
///
/// A STEP LANDING ACROSS THE SHADOW FUNCTION'S OWN PENUMBRA/UMBRA BOUNDARY:
/// `shadow/conical.cpp` was read (not assumed) to confirm its `fraction` is
/// continuous at every one of its own branch boundaries -- each pair of
/// adjacent closed-form pieces (sunlit/penumbra/umbra/annular) is built so
/// the lens- or disc-area ratio each piece computes agrees exactly where
/// the next piece's domain begins. So a bumped evaluation straddling such a
/// boundary sees a KINK -- a possible slope discontinuity in an otherwise
/// continuous function, the same shape drag's own species cutoffs have
/// (`DRAG-A-010`'s own precedent) -- never a JUMP (an actual discontinuity
/// in the value, which would make a straddling difference meaningless
/// rather than merely locally less accurate). Not yet isolated the way
/// `DRAG-P-1`'s own jump was pinned to a single altitude; carried as a
/// further characterisation, not a defect this entry closes.
inline constexpr double kPositionStepM = 100.0;

dyn::DynError forward(std::string_view id, const std::string& message) {
    return dyn::DynError{id, message};
}

}  // namespace

Srp::Srp(macromodel::Macromodel model, const eph::Ephemeris& ephemeris, odl::time::LeapTable leaps)
    : model_(std::move(model)), ephemeris_(ephemeris), leaps_(std::move(leaps)), consumes_{} {}

dyn::ForceId Srp::id() const { return dyn::ForceId{"srp"}; }

const std::vector<dyn::ParameterId>& Srp::consumes() const { return consumes_; }

odl::Result<Srp::AccelAndVelocityJacobian, dyn::DynError>
Srp::accel_only(const odl::time::Epoch& t, const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s) const {
    // 1. The Sun's GCRS state (SPEC-ephemerides gives km/km-s; the crossing
    // to metres is named, core/units.hpp's own discipline, DYN-R-011).
    auto sun_state = ephemeris_.geocentric_state(eph::Body::Sun, t, leaps_);
    if (!sun_state.has_value())
        return odl::err(forward("PHPR-F-003",
            "the Sun's ephemeris state was unavailable at the requested epoch: " +
            sun_state.error().message));
    const Vec3 r_sun_gcrs_m = odl::metres_from_km(sun_state->position());
    const Vec3 v_sun_gcrs_m_s = odl::metres_from_km(sun_state->velocity());

    // 2. The Sun AS SEEN FROM THE SPACECRAFT, not from Earth's centre --
    // both the distance (for the 1/r^2 irradiance scaling, PHPR-R-001) and
    // the direction (for attitude and the kernel's own source direction)
    // are spacecraft-relative. The two are geocentrically indistinguishable
    // to within a spacecraft-Earth distance over an AU (<0.03%), but the
    // exact vector is already being formed for the direction, so the exact
    // distance costs nothing extra to take from the same vector rather than
    // approximate from a different one.
    const Vec3 to_sun_m = r_sun_gcrs_m - r_gcrs_m;
    const double r_sat_sun_m = to_sun_m.norm();

    // 3. The distance-scaled irradiance (PHPR-R-001): 1367 * (au/r)^2, not
    // srp_force's own fixed constant.
    const double au_m = odl::metres_from_km(eph::Ephemeris::kAstronomicalUnitKm);
    auto irradiance = macromodel::irradiance_w_per_m2(
        kSolarIrradianceAt1AuWPerM2 * (au_m / r_sat_sun_m) * (au_m / r_sat_sun_m));
    if (!irradiance.has_value())
        return odl::err(forward(irradiance.error().id, irradiance.error().message));

    // 4. Velocity relative to the irradiance SOURCE (PHPR-R-010's own named
    // parameter), not the spacecraft's bare GCRS velocity: aberration
    // depends on velocity relative to the Sun, which Earth's own ~29.8 km/s
    // heliocentric motion dominates over the spacecraft's ~7.4 km/s
    // geocentric speed alone (PHPR-P-5).
    const Vec3 v_rel_gcrs_m_s = v_gcrs_m_per_s - v_sun_gcrs_m_s;

    // 5. Nominal attitude resolves both the Sun's direction and the relative
    // velocity into body frame. `body_direction()` requires a unit vector
    // (MCRM-F-002) -- `to_sun_m` is a position difference, ~1 au in metres,
    // not one, so it is normalised before rotating, not after.
    auto frame = attitude::nominal_yaw_steering(r_gcrs_m, to_sun_m);
    if (!frame.has_value())
        return odl::err(forward(frame.error().id, frame.error().message));
    const Vec3 to_sun_unit = (1.0 / r_sat_sun_m) * to_sun_m;
    auto sun_direction_body = macromodel::body_direction(frame->apply(to_sun_unit));
    if (!sun_direction_body.has_value())
        return odl::err(forward(sun_direction_body.error().id, sun_direction_body.error().message));
    const Vec3 v_rel_body_m_s = frame->apply(v_rel_gcrs_m_s);

    // 6. The shadow factor: SECM (conical.hpp), sharing GCRS with every
    // other input here (PHPR-R-006 -- and never passed into the kernel: the
    // Earth does not eclipse its own surface, a spacecraft in the Earth's
    // shadow still receives the Earth's infrared, so a shared "attenuate by
    // shadow" step inside the kernel would be wrong for ERP by
    // construction).
    auto shadow_result = shadow::conical(frames::Position<frames::Frame::GCRS>{r_sun_gcrs_m},
                                         frames::Position<frames::Frame::GCRS>{r_gcrs_m});
    if (!shadow_result.has_value())
        return odl::err(forward(shadow_result.error().id, shadow_result.error().message));
    const double shadow_factor = shadow_result->fraction;

    // 7. The kernel, force AND its analytic velocity Jacobian together
    // (PHPR-R-010) -- this caller needs the Jacobian the same per-surface
    // computation already produces internally, so it calls
    // `photon_force_and_velocity_jacobian` directly rather than the
    // `photon_force` wrapper that only extracts `.force` from it.
    auto force_n = srp_analytic::photon_force_and_velocity_jacobian(
        model_, *irradiance, macromodel::Band::visible, *sun_direction_body, *sun_direction_body,
        v_rel_body_m_s);
    if (!force_n.has_value())
        return odl::err(forward(force_n.error().id, force_n.error().message));

    const double mass_kg = model_.mass_kg().value();
    const Vec3 a_body_m_s2 = (shadow_factor / mass_kg) * force_n->force;
    const Vec3 a_gcrs_m_s2 = frame->transpose().apply(a_body_m_s2);   // M_gcrs_to_body^T = M_body_to_gcrs

    // d(a_body)/d(v_rel_body): the same (shadow_factor/mass_kg) scaling
    // that turns force into acceleration, since both factors are constants
    // w.r.t. v_rel and so pass straight through the derivative. Then
    // rotated body->GCRS on BOTH sides -- a Jacobian's row space is its
    // output's frame, its column space its input's, and both are body
    // frame here: d(a_gcrs)/d(v_gcrs) = R^T * d(a_body)/d(v_body) * R,
    // R = frame (this file's own GCRS->body convention, step 5 above).
    // v_rel_gcrs = v_gcrs - v_sun_gcrs makes d(v_rel_gcrs)/d(v_gcrs) the
    // identity (the Sun's own velocity does not depend on the spacecraft's
    // state), so this already IS d(a)/d(v) for the state velocity
    // `Srp::accel`'s caller means -- no further chain-rule term needed.
    Mat3 da_dv_body{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            da_dv_body.r[i][j] = (shadow_factor / mass_kg) * force_n->d_force_d_velocity.r[i][j];
    const Mat3 da_dv_gcrs = frame->transpose().times(da_dv_body).times(*frame);

    return AccelAndVelocityJacobian{a_gcrs_m_s2, da_dv_gcrs};
}

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Srp::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
          const Vec3& v_m_per_s, const dyn::ParameterSet&, const dyn::ParameterRegistry&) const {
    auto centre = accel_only(t, r_m.metres(), v_m_per_s);
    if (!centre.has_value()) return odl::err(centre.error());

    // PHPR-R-007: the velocity Jacobian is not a declaration without a term
    // behind it -- PHPR-R-010 put a real aberration term in the force, so
    // this is with_velocity. d(a)/d(v) is `accel_only`'s own analytic
    // result (PHPR-R-010), not a finite difference; `PHPR-A-006` checks it
    // against an independent finite difference of the whole plugin call, so
    // that test now checks THIS PRODUCTION VALUE rather than one finite
    // difference against another. d(a)/d(r) has no such closed form here
    // (kPositionStepM's own comment above) and stays a central difference:
    // six bumped `accel_only` calls, half what this used to cost now that
    // d(a)/d(v) needs none of its own -- the per-`accel()`,
    // per-STM-propagation-step cost of this Jacobian.
    Mat3 da_dr{};
    const Vec3 axis_unit[3] = {Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}};
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 dr = kPositionStepM * axis_unit[static_cast<std::size_t>(axis)];

        auto r_plus = accel_only(t, r_m.metres() + dr, v_m_per_s);
        auto r_minus = accel_only(t, r_m.metres() - dr, v_m_per_s);
        if (!r_plus.has_value()) return odl::err(r_plus.error());
        if (!r_minus.has_value()) return odl::err(r_minus.error());
        const Vec3 dcol_r =
            (1.0 / (2.0 * kPositionStepM)) * (r_plus->acceleration - r_minus->acceleration);

        da_dr.r[0][static_cast<std::size_t>(axis)] = dcol_r.x;
        da_dr.r[1][static_cast<std::size_t>(axis)] = dcol_r.y;
        da_dr.r[2][static_cast<std::size_t>(axis)] = dcol_r.z;
    }

    dyn::ParameterJacobian pj(dyn::ParameterRegistry{});
    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{centre->acceleration},
        dyn::StateJacobian::with_velocity(da_dr, centre->d_acceleration_d_velocity),
        pj,
        std::nullopt,
    };
}

}  // namespace odl::srp
