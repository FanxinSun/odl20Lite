// attitude.cpp — SPEC-photon-pressure PHPR-R-004, SPEC-thrust-yaw TYAW-R-001..R-007.

#include <odl/attitude/attitude.hpp>

#include <cmath>
#include <string>

namespace odl::attitude {
namespace {

/// sin of ~2e-4 arcsec -- see attitude.hpp's own reasoning for why this is
/// tight rather than generous.
inline constexpr double kMinAxisNorm = 1.0e-9;

[[nodiscard]] Vec3 normalized(const Vec3& v) noexcept {
    const double n = v.norm();
    return Vec3{v.x / n, v.y / n, v.z / n};
}

// --- SPEC-thrust-yaw: shared machinery for the four GPS block laws --------

constexpr double kDegToRad = M_PI / 180.0;
/// KOUBA09 Eq. 6's own stated average GPS orbital angular velocity.
constexpr double kMuDotRadPerS = 0.00836 * kDegToRad;
/// KOUBA09 Eq. 17 (E_sh = R_E/r_s, point-source) widened to this stated
/// figure to include the penumbra (TYAW-R-001, Bar-Sever 1996 via KOUBA09) --
/// a fixed constant, not recomputed from the caller's own r_s each call,
/// matching KOUBA09's own "13.5 deg is likely a good choice."
constexpr double kShadowHalfAngleRad = 13.5 * kDegToRad;

/// The (r_hat, n_hat, t_hat) triad KOUBA09's own beta/mu are defined against:
/// r_hat radial, n_hat the orbit normal (r x v, matching the same right-hand
/// sense `PHPR-A-011`'s own RTN decomposition already uses), t_hat = n_hat x
/// r_hat the prograde tangential direction.
struct OrbitTriad {
    Vec3 r_hat, n_hat, t_hat;
};

[[nodiscard]] OrbitTriad orbit_triad(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s) noexcept {
    const Vec3 r_hat = normalized(r_gcrs_m);
    const Vec3 n_hat = normalized(r_gcrs_m.cross(v_gcrs_m_per_s));
    const Vec3 t_hat = n_hat.cross(r_hat);
    return {r_hat, n_hat, t_hat};
}

/// Signed Sun elevation above the orbital plane (KOUBA09's own beta, Fig. 3's
/// own sign convention: positive when the Sun forms an acute angle with the
/// orbit normal).
[[nodiscard]] double signed_beta_rad(const Vec3& s_hat, const Vec3& n_hat) noexcept {
    const double s_dot_n = s_hat.dot(n_hat);
    return std::asin(s_dot_n < -1.0 ? -1.0 : (s_dot_n > 1.0 ? 1.0 : s_dot_n));
}

/// KOUBA09's own mu: orbit angle from midnight (mu = 0 there), positive in
/// the direction of motion. Degenerates only where the Sun's own projection
/// onto the orbital plane is undefined (|beta| = 90 deg exactly) -- every
/// caller below only reaches this after confirming |beta| is well inside a
/// turn or shadow threshold, all far below 90 deg, so that degeneracy never
/// actually occurs where the result is used.
[[nodiscard]] double mu_rad(const OrbitTriad& tri, const Vec3& s_hat) noexcept {
    const Vec3 s_orb_raw = s_hat - s_hat.dot(tri.n_hat) * tri.n_hat;
    const Vec3 u_midnight = -1.0 * normalized(s_orb_raw);
    return std::atan2(tri.t_hat.dot(u_midnight), tri.r_hat.dot(u_midnight));
}

/// Shifts `angle` by a multiple of 2*pi to the representative nearest
/// `reference` -- the wrap-safe way to compare an ATAN2-branch angle (mu, or
/// an ATAN2-valued psi) against a value that need not sit in (-pi, pi].
[[nodiscard]] double wrap_near(double angle, double reference) noexcept {
    while (angle - reference > M_PI) angle -= 2.0 * M_PI;
    while (angle - reference < -M_PI) angle += 2.0 * M_PI;
    return angle;
}

/// Reduces `delta` to [0, 2*pi) if `positive_direction`, else to (-2*pi, 0].
/// `evaluate_turn`'s own fix (manager's review): the nominal law's change
/// SINCE ONSET, put on the branch in the turn's own direction of travel --
/// never the branch nearest the ramp's own (unboundedly growing) value, the
/// earlier, wrong approach. For any beta the true swing is under 180 deg
/// (KOUBA09's own turn covers less than a full half-turn), so this branch is
/// unambiguous regardless of how far the ramp has itself run on.
[[nodiscard]] double wrap_directional(double delta, bool positive_direction) noexcept {
    constexpr double kTwoPi = 2.0 * M_PI;
    double d = std::fmod(delta, kTwoPi);
    if (positive_direction) {
        if (d < 0.0) d += kTwoPi;
    } else {
        if (d > 0.0) d -= kTwoPi;
    }
    return d;
}

/// KOUBA09 Eq. 4 (`x_sign` = +1) / Eq. 5 (`x_sign` = -1, IIR's own 180 deg
/// X-axis reversal): psi_n = ATAN2(-x_sign*tan(beta), x_sign*sin(mu)).
[[nodiscard]] double psi_nominal(double beta, double mu, double x_sign) noexcept {
    return std::atan2(-x_sign * std::tan(beta), x_sign * std::sin(mu));
}

/// KOUBA09 Eq. 6, evaluated at a specific mu (not necessarily the current
/// query mu -- shadow crossing needs it at the entry angle specifically).
[[nodiscard]] double psidot_nominal(double beta, double mu) noexcept {
    const double t = std::tan(beta);
    const double s = std::sin(mu);
    return kMuDotRadPerS * t * std::cos(mu) / (s * s + t * t);
}

/// The ramp's own constant-rate direction for a noon/midnight-shaped turn
/// (KOUBA09's own SIGN[R, psi_dot_n(t_s)], Eq. 15/16): algebraically,
/// sign(psi_dot_n) = x_sign * sign(beta) * sign(cos(mu_s)), and cos(mu_s) has
/// a FIXED sign for each turn family (negative approaching noon, positive
/// approaching midnight, since the turn's own half-width is always < 90 deg)
/// -- so this needs no direct, beta-near-zero-fragile evaluation of
/// psi_dot_n itself. sign(beta) uses TYAW-R-007's own RULED convention:
/// sign(0) := +1, the stateless tie-break for the beta-near-zero edge case
/// (TYAW-Q-002).
[[nodiscard]] double turn_ramp_sign(double beta, double x_sign, bool is_noon) noexcept {
    const double sign_beta = (beta < 0.0) ? -1.0 : 1.0;
    return x_sign * sign_beta * (is_noon ? -1.0 : 1.0);
}

struct TurnResult {
    bool active;
    double psi_rad;
};

/// KOUBA09 Eq. 7/8/9/15/16, reformulated in mu rather than t (mu evolves
/// linearly with t at the fixed rate `kMuDotRadPerS`, so an angle-since-onset
/// carries the same information a time-since-onset would, without a
/// remembered reference epoch -- TYAW-R-007's own statelessness). `onset_rad`
/// is beta_0 = atan(mu_dot/R) (Eq. 7); `mu_center` is pi for a noon turn, 0
/// for a midnight/night turn.
///
/// KOUBA09's own text terminates the turn operationally ("until ... the
/// lagging angle catches up with the nominal yaw attitude"), not by a closed
/// form -- verified (by direct simulation of that operational rule, recorded
/// in PROVENANCE) to be EXACTLY the region where the nominal law is still
/// "ahead" of the constant-rate ramp, in the ramp's own direction of travel:
/// a single-instant, stateless comparison, not an integration from onset.
///
/// The nominal law's own change since onset is read on the DIRECTIONAL
/// branch (`wrap_directional`), not the branch nearest the ramp's own
/// accumulated value -- an earlier version used "nearest the ramp" and was
/// wrong: as beta -> 0 the true swing tends to 180 deg exactly (the turn's
/// own nominal law swings through very nearly its own full half-turn), so
/// "nearest the ramp" picks the wrong copy of 2*pi once the ramp's own
/// (unboundedly growing) value has drifted far enough -- found by the
/// manager's own review, not by the grid that first tested it (a bound
/// fitted to a grid is not a bound, rule 7). The directional branch needs no
/// such bound: the true swing is under 180 deg for every beta > 0, so it is
/// unambiguous regardless of how far mu_current sits past onset, and once
/// caught up, the ramp's own value keeps growing while the directional
/// value stays bounded, so a finished turn stays reported finished.
[[nodiscard]] TurnResult evaluate_turn(double beta, double mu_current, double x_sign,
                                        double mu_center, double onset_rad, double rate_rad_per_s,
                                        double ramp_sign) noexcept {
    const double width_sq = onset_rad * std::abs(beta) - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_s = mu_center - half_width;
    const double mu_q = wrap_near(mu_current, mu_center);
    if (mu_q < mu_s) return {false, 0.0};

    const double psi_s = psi_nominal(beta, mu_s, x_sign);
    const double psi_ramp = psi_s + ramp_sign * rate_rad_per_s * (mu_q - mu_s) / kMuDotRadPerS;
    const double delta_raw = psi_nominal(beta, mu_q, x_sign) - psi_s;
    const double delta_directional = wrap_directional(delta_raw, ramp_sign > 0.0);
    const double psi_nom_directional = psi_s + delta_directional;
    const double gap = (psi_nom_directional - psi_ramp) * ramp_sign;
    if (gap > 0.0) return {true, psi_ramp};
    return {false, 0.0};
}

/// TYAW-R-003 (IIF's own noon turn -- changed from `evaluate_turn`'s own LAG
/// shape after CODE's own real ORBEX data showed IIF's noon turn is the SAME
/// rate-limited ramp run backwards in time: it leaves the ideal law EARLY and
/// merges with it where the ideal law's own rate falls back to the hardware
/// limit, rather than leaving the ideal law AT the limit and catching up
/// late (PROVENANCE.md Sec.30.12 -- the manager's own ruling, `TYAW-Q-006`'s
/// own ANALYTIC/searched roles exchanged relative to `evaluate_turn`, not a
/// new formula). A planned manoeuvre, not a hardware-limited one -- the same
/// character as IIF's own night side already flying Shape E, which likewise
/// needs its own end state known in advance.
///
/// `mu_e` (the merge point) is `evaluate_turn`'s own analytic onset, `mu_s`,
/// reflected to the FAR side of `mu_center` -- the SAME onset_rad relation
/// (`TYAW-P-1`), since |psi_dot_n| is exactly symmetric about mu_center
/// (psi_dot_n(2*mu_center - mu) = psi_dot_n(mu), checked directly from Eq. 6:
/// cos is even and sin^2 is even under this reflection, both at mu_center =
/// pi). The turn is active for mu_q <= mu_e; the OTHER boundary (leaving the
/// ideal law early) has no closed form and is not searched for at
/// evaluation time either -- like `evaluate_turn`'s own catch-up condition,
/// it falls out of a SINGLE gap check at the query mu_q itself, verified
/// (PROVENANCE.md Sec.30.12) to reproduce a small-step ground-truth walk
/// exactly across every beta and mu_q tested: the directional branch and the
/// active-region sign are each `evaluate_turn`'s own, with both senses
/// reversed (measuring the swing SINCE mu_e as mu_q moves away from it in
/// the DEcreasing direction, the mirror image of `evaluate_turn`'s own
/// increasing walk from mu_s).
[[nodiscard]] TurnResult evaluate_turn_lead(double beta, double mu_current, double x_sign,
                                             double mu_center, double onset_rad, double rate_rad_per_s,
                                             double ramp_sign) noexcept {
    const double width_sq = onset_rad * std::abs(beta) - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_e = mu_center + half_width;
    const double mu_q = wrap_near(mu_current, mu_center);
    if (mu_q > mu_e) return {false, 0.0};

    const double psi_e = psi_nominal(beta, mu_e, x_sign);
    const double psi_ramp = psi_e + ramp_sign * rate_rad_per_s * (mu_q - mu_e) / kMuDotRadPerS;
    const double delta_raw = psi_nominal(beta, mu_q, x_sign) - psi_e;
    const double delta_directional = wrap_directional(delta_raw, ramp_sign < 0.0);
    const double psi_nom_directional = psi_e + delta_directional;
    const double gap = (psi_nom_directional - psi_ramp) * ramp_sign;
    if (gap < 0.0) return {true, psi_ramp};
    return {false, 0.0};
}

/// KOUBA09 Eq. 17-22 (II/IIA only): entry/exit from the FIXED shadow
/// half-angle `kShadowHalfAngleRad` (not a rate-derived onset threshold --
/// eclipse is a geometric fact, not a hardware limit), so unlike
/// `evaluate_turn`, both edges are closed-form (Eq. 18/19) and symmetric
/// about mu = 0. A spin-up phase at `rr_rad_per_s2` up to `rate_rad_per_s`
/// (Eq. 20/21), then constant rate (Eq. 22), both signed by the yaw bias --
/// the solar sensor has lost the Sun during eclipse, so the turn direction
/// comes from the KNOWN bias sign, not from the (otherwise ambiguous, since
/// beta may be either sign here) nominal rate's own sign at entry.
[[nodiscard]] TurnResult evaluate_shadow_crossing(double beta, double mu_current, double x_sign,
                                                   double rate_rad_per_s, double rr_rad_per_s2,
                                                   double bias_rad) noexcept {
    const double width_sq = kShadowHalfAngleRad * kShadowHalfAngleRad - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_s = -half_width;
    const double mu_e = half_width;
    const double mu_q = wrap_near(mu_current, 0.0);
    if (mu_q < mu_s || mu_q > mu_e) return {false, 0.0};

    const double psi_s = psi_nominal(beta, mu_s, x_sign);
    const double psidot_s = psidot_nominal(beta, mu_s);
    const double sign_bias = (bias_rad < 0.0) ? -1.0 : 1.0;
    const double target_rate = sign_bias * rate_rad_per_s;  // KOUBA09 SIGN(R, b)
    const double spin_accel = sign_bias * rr_rad_per_s2;    // KOUBA09 SIGN(RR, b)

    const double dt = (mu_q - mu_s) / kMuDotRadPerS;
    const double t1 = (target_rate - psidot_s) / spin_accel;  // Eq. 20

    if (dt <= t1) {
        return {true, psi_s + (psidot_s + spin_accel * dt / 2.0) * dt};  // Eq. 21
    }
    const double psi_at_t1 = psidot_s * t1 + spin_accel * t1 * t1 / 2.0;
    return {true, psi_s + psi_at_t1 + target_rate * (dt - t1)};  // Eq. 22
}

/// TYAW-R-003 (IIF's own night/midnight law -- changed from a rate-limited
/// ramp to this after real ORBEX data showed the ramp's own onset nine-plus
/// degrees from where the real rate actually starts and its own 0.06 deg/s
/// off by two orders of magnitude more than this shape's own fit,
/// PROVENANCE.md Sec.30.8/30.10 record the verdict). `DIL10`'s own text
/// frames the midnight turn by the SHADOW itself, not a hardware rate
/// limit: the yaw "catches up with the nominal yaw angle towards the end of
/// the Earth's shadow" -- eclipsed from mu = -half_width to +half_width,
/// `kShadowHalfAngleRad` the SAME fixed shadow half-angle `TYAW-R-001`'s own
/// II/IIA shadow-crossing uses (a stated CHOICE borrowed for IIF, not
/// re-derived independently -- no IIF-specific eclipse geometry source was
/// found, PROVENANCE.md Sec.30.2's own search). A SINGLE constant rate spans
/// the whole passage: the nominal law's own unwrapped swing from entry to
/// exit, divided by the window's own duration -- not a hardware rate at
/// all, since during actual eclipse there is no Sun sensor to chase one
/// against.
///
/// The swing has a closed form, not a numerical walk: d(psi_n)/d(mu) =
/// tan(beta)*cos(mu) / (sin(mu)^2 + tan(beta)^2) (dividing `psidot_nominal`
/// by mu_dot) integrates, by the standard 1/(u^2+a^2) form under u =
/// sin(mu), to ATAN(sin(mu)/tan(beta)) -- unlike `psi_nominal` itself
/// (an ATAN2, with a branch cut), this antiderivative is smooth and
/// single-valued for every mu in the window at any beta != 0, so no
/// unwrapping or step count is needed: the swing is just its value at
/// mu_exit minus its value at mu_entry, verified (PROVENANCE.md Sec.30.10)
/// against an independent small-step nearest-branch accumulation to better
/// than 1e-9 deg from beta = 0.001 deg to 13.499 deg. Evaluated AT the
/// query mu_q, this constant rate is exactly linear interpolation in mu
/// between (mu_entry, psi_entry) and (mu_exit, psi_entry + swing) --
/// mu_dot itself cancels (a constant rate over a mu-interval, sampled at a
/// fraction of that interval, does not need to know how fast mu itself
/// moves), so it does not appear in the return expression at all.
[[nodiscard]] TurnResult evaluate_shadow_constant_rate(double beta, double mu_current,
                                                        double x_sign) noexcept {
    const double width_sq = kShadowHalfAngleRad * kShadowHalfAngleRad - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_entry = -half_width;
    const double mu_exit = half_width;
    const double mu_q = wrap_near(mu_current, 0.0);
    if (mu_q < mu_entry || mu_q > mu_exit) return {false, 0.0};

    const double psi_entry = psi_nominal(beta, mu_entry, x_sign);
    const double tan_beta = std::tan(beta);
    const double swing = std::atan(std::sin(mu_exit) / tan_beta) - std::atan(std::sin(mu_entry) / tan_beta);
    return {true, psi_entry + swing * (mu_q - mu_entry) / (mu_exit - mu_entry)};
}

/// Builds M_gcrs_to_body from a turn's own yaw angle psi and the orbit
/// triad. z_body is always nadir (yaw rotates only about it). Verified
/// numerically (PROVENANCE) against `nominal_yaw_steering`'s own independent
/// z=-r_hat/y=(z x s)/x=y x z construction, across many random geometries, to
/// machine precision (max component error 4e-16): x_body =
/// -cos(psi)*t_hat - sin(psi)*n_hat is the relation that reproduces it,
/// bit-for-bit continuity at every turn hand-over (`TYAW-P-2`) following from
/// that same identity rather than from a separate argument.
[[nodiscard]] Mat3 frame_from_yaw(const OrbitTriad& tri, double psi) noexcept {
    const Vec3 z_body = -1.0 * tri.r_hat;
    const Vec3 x_body = (-std::cos(psi)) * tri.t_hat + (-std::sin(psi)) * tri.n_hat;
    const Vec3 y_body = z_body.cross(x_body);
    Mat3 m;
    m.r[0] = {x_body.x, x_body.y, x_body.z};
    m.r[1] = {y_body.x, y_body.y, y_body.z};
    m.r[2] = {z_body.x, z_body.y, z_body.z};
    return m;
}

}  // namespace

odl::Result<Mat3, AttitudeError>
nominal_yaw_steering(const Vec3& r_gcrs_m, const Vec3& sun_direction_gcrs) {
    const Vec3 r_hat = normalized(r_gcrs_m);
    const Vec3 s_hat = normalized(sun_direction_gcrs);
    const Vec3 z_body = -1.0 * r_hat;

    const Vec3 y_axis_raw = z_body.cross(s_hat);
    const double y_norm = y_axis_raw.norm();
    if (y_norm < kMinAxisNorm) {
        return odl::err(AttitudeError{"ATTD-F-001",
            "the Sun is within the nadir singularity's own tolerance of the nadir axis: "
            "the panel rotation axis (z_body x s_hat) has norm " + std::to_string(y_norm) +
            ", below " + std::to_string(kMinAxisNorm) + " -- undefined, not merely small"});
    }
    const Vec3 y_body = (1.0 / y_norm) * y_axis_raw;
    const Vec3 x_body = y_body.cross(z_body);

    Mat3 m;
    m.r[0] = {x_body.x, x_body.y, x_body.z};
    m.r[1] = {y_body.x, y_body.y, y_body.z};
    m.r[2] = {z_body.x, z_body.y, z_body.z};
    return m;
}

odl::Result<Mat3, AttitudeError>
gps_yaw_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s, const Vec3& sun_direction_gcrs,
                  GpsBlock block, const HardwareYawRates& rates) {
    // TYAW-R-004: IIIA is TYAW-R-003's own IIF law, unchanged -- not a fifth
    // implementation (TYAW-A-005 checks this is bit-identical to IIF).
    const GpsBlock effective_block = (block == GpsBlock::IIIA) ? GpsBlock::IIF : block;
    const double x_sign = (effective_block == GpsBlock::IIR_IIRM) ? -1.0 : 1.0;

    const OrbitTriad tri = orbit_triad(r_gcrs_m, v_gcrs_m_per_s);
    const Vec3 s_hat = normalized(sun_direction_gcrs);
    const double beta = signed_beta_rad(s_hat, tri.n_hat);
    const double mu = mu_rad(tri, s_hat);

    const double noon_rate = rates.noon_deg_per_s * kDegToRad;
    const double night_rate = rates.night_deg_per_s * kDegToRad;
    const double noon_onset = std::atan(kMuDotRadPerS / noon_rate);  // KOUBA09 Eq. 7
    const double noon_ramp_sign = turn_ramp_sign(beta, x_sign, /*is_noon=*/true);

    // TYAW-R-002 (II/IIA, IIR): the noon turn LAGS -- KOUBA09's own words,
    // "the actual yaw angle to temporarily lag behind the nominal yaw
    // attitude" (PROVENANCE.md Sec.30.12), confirmed, not merely un-checked.
    // TYAW-R-003 (IIF): the noon turn LEADS -- CODE's own real data,
    // PROVENANCE.md Sec.30.12.
    TurnResult result = (effective_block == GpsBlock::IIF)
        ? evaluate_turn_lead(beta, mu, x_sign, M_PI, noon_onset, noon_rate, noon_ramp_sign)
        : evaluate_turn(beta, mu, x_sign, M_PI, noon_onset, noon_rate, noon_ramp_sign);

    if (!result.active) {
        switch (effective_block) {
            case GpsBlock::II_IIA:
                // TYAW-R-001: no separate beta0-based midnight turn -- the
                // night side is the shadow-crossing regime, whenever
                // actually eclipsed, regardless of beta.
                result = evaluate_shadow_crossing(beta, mu, x_sign, night_rate,
                                                   rates.spin_up_deg_per_s2 * kDegToRad,
                                                   rates.yaw_bias_deg * kDegToRad);
                break;
            case GpsBlock::IIR_IIRM: {
                // TYAW-R-002: midnight turn, same rate-limited ramp shape as
                // noon, IIR's own single hardware rate (identical to noon).
                const double night_onset = std::atan(kMuDotRadPerS / night_rate);
                result = evaluate_turn(beta, mu, x_sign, 0.0, night_onset, night_rate,
                                        turn_ramp_sign(beta, x_sign, /*is_noon=*/false));
                break;
            }
            case GpsBlock::IIF:
                // TYAW-R-003: night side is Shape E, the shadow-crossing
                // regime (like II/IIA's own TYAW-R-001), not a rate-limited
                // ramp -- `rates.night_deg_per_s` is not read here.
                result = evaluate_shadow_constant_rate(beta, mu, x_sign);
                break;
            case GpsBlock::IIIA:
                break;  // unreachable: effective_block never IIIA
        }
    }

    if (result.active) {
        return frame_from_yaw(tri, result.psi_rad);
    }
    // TYAW-F-001: forwarded unchanged from ATTD-F-001, outside any turn.
    return nominal_yaw_steering(r_gcrs_m, sun_direction_gcrs);
}

}  // namespace odl::attitude
