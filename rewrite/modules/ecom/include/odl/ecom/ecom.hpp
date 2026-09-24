#pragma once
// odl/ecom/ecom.hpp — ECOM2, the empirical CODE orbit model (Arnold et al. 2015, `ARN15`).
//
// SPEC-ecom.md. Independent of `odl::attitude`: this module reuses none of
// that module's own formulas (not even mu_rad -- see delta_u_rad below);
// the D/Y/B frame (Eq.1) and Delta-u are pure functions of (r, v,
// sun_direction) alone, `ARN15`'s own point, and why this force needs no
// Macromodel, no yaw-steering provider, unlike `srp_analytic`.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/force.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/frames/state.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <cstddef>
#include <vector>

namespace odl::ecom {

using EcomError = odl::Diagnostic;

/// `ECOM-R-004`. General truncation order: D carries n_D even harmonics of
/// Delta-u, B carries n_B odd harmonics. D4B1 (n_D=2, n_B=1) is CODE's own
/// adopted configuration since 2015-01-04, named via `d4b1_order()` below,
/// not a special case in `Ecom`'s own logic -- any other (n_D, n_B) is the
/// same code path.
struct EcomOrder {
    int n_D = 0;
    int n_B = 0;
};

[[nodiscard]] inline EcomOrder d4b1_order() noexcept { return EcomOrder{2, 1}; }

/// `ARN15` Eq.5. `D_even_c`/`D_even_s` hold D's own even harmonics (index i
/// -> order 2*(i+1), i = 0..n_D-1); `B_odd_c`/`B_odd_s` hold B's own odd
/// harmonics (index i -> order 2*(i+1)-1, i = 0..n_B-1). Y carries only its
/// own constant term (`ARN15`'s own printed form -- no periodic Y terms in
/// the extended ECOM). A caller-owned value type: nothing here reads a
/// library (`ECOM-R-004`).
struct EcomCoefficients {
    double D0 = 0.0;
    std::vector<double> D_even_c, D_even_s;  ///< size order.n_D each
    double Y0 = 0.0;
    double B0 = 0.0;
    std::vector<double> B_odd_c, B_odd_s;    ///< size order.n_B each
};

/// D(Delta_u)/dD, B(Delta_u)/dB -- one Vec3 (a direction in GCRS) per scalar
/// in `EcomCoefficients`, same shape, so a caller can walk both in lockstep
/// when registering parameters. This IS d(acceleration)/d(that one
/// coefficient); the acceleration is exactly linear in each (`ECOM-R-003`'s
/// own printed form, checked by `ECOM-A-006`), so no further chain rule is
/// needed here.
struct EcomSensitivities {
    Vec3 D0;
    std::vector<Vec3> D_even_c, D_even_s;
    Vec3 Y0;
    Vec3 B0;
    std::vector<Vec3> B_odd_c, B_odd_s;
};

/// `ECOM-R-006`/`ECOM-P-1`. `d_velocity` is d(acceleration)/d(v), fully
/// analytic via Delta-u's own v-dependence (e_D/e_Y/e_B do not depend on v
/// at all in this module's own Delta-u construction -- see delta_u_rad).
/// NO d(acceleration)/d(r): that half is analytically intractable through
/// this same Delta-u construction (the orbit-normal n_hat's own r-dependence
/// compounds through the D/Y/B frame too), so it is taken by central finite
/// difference in `Ecom::accel`'s own wrapper -- `Srp::accel`'s own
/// established split (`PHPR-P-6`), not a new choice.
struct EcomResult {
    Vec3 acceleration_m_s2;
    Vec3 e_D, e_Y, e_B;
    double delta_u_rad = 0.0;
    EcomSensitivities d_coefficients;
    Mat3 d_velocity;
};

/// `ECOM-R-001`/`ECOM-R-002`/`ECOM-R-003`. Computes the Eq.1 frame, Delta-u,
/// the acceleration `a = D(Delta_u) e_D + Y0 e_Y + B(Delta_u) e_B` (no a0
/// term -- a caller wanting one registers another force, `srp_analytic`,
/// say, in the same `ForceSet`), and both analytic sensitivities above.
///
/// Delta-u = u - u_s (`ARN15`, satellite's own argument of latitude minus
/// the Sun's own), computed here as the SINGLE in-plane angle from the
/// Sun's own orbital-plane projection to the satellite -- `ARN15`'s own
/// "independent of the coordinate system" claim taken directly (`ECOM-A-002`
/// checks it), with NO ascending-node reference at all, so no dependence on
/// this tree's own `attitude::mu_rad` (a differently-sourced, KOUBA09-
/// specific formula with its own sign history -- not reused, not
/// duplicated). Refuses (`ECOM-F-002`) where the Sun is exactly on the orbit
/// normal (beta = +-90 deg exactly): the Sun's own in-plane projection is
/// then undefined, not merely small -- Delta-u's own real degeneracy, not an
/// artifact of how it is computed here. `ECOM-A-004`'s own reduction test
/// cross-checks this formula independently, via a separately-built
/// ascending-node construction, precisely so the two never share a bug.
///
/// Refuses (`ECOM-F-001`) where e_D is parallel to e_r -- the spacecraft
/// exactly on the Earth-Sun line, the same singularity `FRAME-F-005`
/// already names for `srp_analytic`.
[[nodiscard]] odl::Result<EcomResult, EcomError>
ecom_acceleration(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s, const Vec3& sun_direction_gcrs,
                   const EcomOrder& order, const EcomCoefficients& coefficients);

/// D(Delta_u), B(Delta_u) alone -- `ARN15` Eq.5's own harmonic sums,
/// evaluated directly (not through `Ecom`/`ecom_acceleration`), so
/// `ECOM-A-003`/`004`'s own symmetry and reduction checks can probe the
/// functions without a frame or a state.
[[nodiscard]] double d_of(const EcomOrder& order, const EcomCoefficients& c, double delta_u_rad) noexcept;
[[nodiscard]] double b_of(const EcomOrder& order, const EcomCoefficients& c, double delta_u_rad) noexcept;

/// The registered-parameter surface: `EcomCoefficients`' own shape with each
/// scalar replaced by the `ParameterId` a caller's own registry issued for
/// it (`Drag`'s own one-`ParameterId`-per-`declare` precedent, extended to
/// as many scalars as `order` implies).
struct EcomParameterIds {
    dyn::ParameterId D0;
    std::vector<dyn::ParameterId> D_even_c, D_even_s;
    dyn::ParameterId Y0;
    dyn::ParameterId B0;
    std::vector<dyn::ParameterId> B_odd_c, B_odd_s;

    /// D0, D_even_c[0], D_even_s[0], ..., D_even_c[n_D-1], D_even_s[n_D-1],
    /// Y0, B0, B_odd_c[0], B_odd_s[0], ..., B_odd_c[n_B-1], B_odd_s[n_B-1] --
    /// `ARN15` Eq.5's own natural order, and `dyn::Force::consumes()`'s own
    /// required flat form.
    [[nodiscard]] std::vector<dyn::ParameterId> flattened() const;
};

/// `ecom`, `ARN15` Eq.5 as a `dyn::Force`. `ephemeris`/`leaps` are what this
/// force needs to find the Sun's own GCRS direction at the query epoch --
/// bound at construction, the same shape `Srp` already takes. No
/// construction-time coefficient VALUES: `order`/`param_ids` are the only
/// state this class itself owns; every `accel` call reads live values from
/// `dyn::ParameterSet`, so there is no redundant "initial value nobody
/// reads after the first call" to keep in sync.
class Ecom final : public dyn::Force {
public:
    Ecom(EcomOrder order, EcomParameterIds param_ids, const eph::Ephemeris& ephemeris,
         odl::time::LeapTable leaps);

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
          const Vec3& v_m_per_s, const dyn::ParameterSet& params,
          const dyn::ParameterRegistry& registry) const override;

private:
    EcomOrder order_;
    EcomParameterIds param_ids_;
    std::vector<dyn::ParameterId> flattened_ids_;
    const eph::Ephemeris& ephemeris_;
    odl::time::LeapTable leaps_;
};

}  // namespace odl::ecom
