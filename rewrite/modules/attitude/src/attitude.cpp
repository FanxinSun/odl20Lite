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
///
/// NEGATED relative to the raw ATAN2 (the manager's own finding, corrected
/// the same day the IIF-noon-turn "lead" was first reported, PROVENANCE.md
/// Sec.30.12/Sec.30.14): ATAN2(t_hat.u_midnight, r_hat.u_midnight) is the
/// angle from the SATELLITE forward to midnight, not from midnight forward
/// to the satellite -- it runs opposite to the direction of motion, the
/// reverse of this function's own stated contract and of KOUBA09's mu.
/// Checked directly, not merely argued: d(r_hat)/d(true anomaly) is +t_hat
/// (confirmed against a real propagated trajectory, t_hat.v_hat = 0.999989),
/// and expanding u_midnight's own components in the (r_hat,t_hat) frame as
/// that frame itself rotates forward by a small true angle dphi gives
/// mu(t+dt) = mu(t) - dphi for the un-negated formula -- mu DECREASING as
/// the satellite moves forward, for any satellite, a property of the
/// formula, not of one trajectory. This fix stands on its own: `psi_nominal`
/// below also carries a sign flip relative to KOUBA09's Eq.4/5 as printed,
/// but NOT to compensate for this negation -- mu_rad is now KOUBA09's own
/// mu, with nothing left to compensate from it specifically. That flip has
/// a separate, pre-existing cause, this tree's own yaw convention, explained
/// at `psi_nominal` below (an earlier version of this comment, and of
/// `psi_nominal`/`psidot_nominal`/`turn_ramp_sign`'s own, attributed it to
/// this negation instead -- numerically harmless, since every formula below
/// was proved against real data regardless, but the wrong causal claim, a
/// comment-contradicts-code bug of the same class as this function's own
/// defect, not a difference of degree -- the manager's own finding,
/// PROVENANCE.md Sec.30.16).
[[nodiscard]] double mu_rad(const OrbitTriad& tri, const Vec3& s_hat) noexcept {
    const Vec3 s_orb_raw = s_hat - s_hat.dot(tri.n_hat) * tri.n_hat;
    const Vec3 u_midnight = -1.0 * normalized(s_orb_raw);
    return -std::atan2(tri.t_hat.dot(u_midnight), tri.r_hat.dot(u_midnight));
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

/// KOUBA09 Eq. 4, REWRITTEN for this tree's own yaw convention, not
/// transcribed as printed (PROVENANCE.md Sec.30.16): `frame_from_yaw` below
/// builds x_body = -cos(psi)*t_hat - sin(psi)*n_hat, a right-handed
/// rotation about nadir starting from -t_hat; KOUBA09's own psi is a
/// right-handed rotation about nadir starting from +t_hat, x = cos(psi)*
/// t_hat - sin(psi)*n_hat. The two name the same physical x_body only at
/// psi_tree = pi - psi_KOUBA09 (mod 2pi) -- checked against an INDEPENDENT
/// x_body construction (`nominal_yaw_steering`'s own z=-r_hat/y=(z x
/// s)/x=y x z build, which every block's own off-turn attitude already
/// goes through), both KOUBA09's own rotation and this tree's, agreeing
/// with it to under 4e-15 over 2000 random geometries. Substituting Eq. 4
/// as printed into the standard identity pi - ATAN2(y,x) = ATAN2(y,-x)
/// gives exactly this function's own form: the sin(mu) term's own sign
/// flip relative to Eq. 4 as printed IS that substitution. `TYAW-A-013`
/// transcribes Eq. 4 independently and checks it against this relation
/// directly, through the off-turn interface. PROVED against real ORBEX
/// data too, not assumed: this exact form, fed mu_rad's own (KOUBA09-true)
/// output, matches CODE's own real attitude to 0.000-0.001 deg; checked at
/// 21 points spread across a full real day, far from any turn.
///
/// NO `x_sign` PARAMETER (removed here, 2026-09-24, the manager's own
/// ruling, PROVENANCE.md Sec.30.19/TYAW-Q-007): this function used to take
/// one, computing KOUBA09's Eq. 5 (his own `x_sign` = -1 form) for IIR --
/// a real, deliberate transcription of his own printed equation, not a
/// slip. The bug was applying it here at all: Eq. 5 is IIR's yaw angle in
/// KOUBA09's own frame for that block, and this tree's own frame is NOT
/// that frame, for any block -- `frame_from_yaw`/`nominal_yaw_steering`
/// above already implement the single, block-agnostic convention
/// `SPEC-photon-pressure`'s `PHPR-R-004` took from `MSGA15` (+x toward the
/// Sun, universally), and `MSGA15`'s own text on IIR (his own -x_BF, not
/// +x_BF, is the face he says stays sunlit; the IGS frame's x/y are then
/// STATED inverted relative to his) works out to +x TOWARD the Sun for IIR
/// too once carried through -- confirmed independently two ways before
/// anything changed: against `MSGA15`'s own words (not skimmed once), and
/// against CODE's own real G05 (IIR-M) quaternion away from any turn
/// (x_body.dot(sun_hat) = 0.985 to 1.000 at all 318 off-turn epochs
/// checked, matching `nominal_yaw_steering` to 0.00013 deg). Eq. 5's own
/// `x_sign` = -1 form does not belong in this function at all: applying it
/// made `gps_yaw_attitude` emit +x AWAY from the Sun throughout every IIR
/// turn while the ideal law right outside emits +x TOWARD it -- an
/// approximately 180 deg jump at every IIR hand-over, `TYAW-A-014` (below)
/// showing it FAIL on the code as it stood (`plan` rule 5) before this fix,
/// masked until now because `TYAW-A-002`'s own "exact at onset" check
/// samples precisely where `evaluate_turn`'s own strict `gap > 0.0` boundary
/// makes BOTH the turn path and the off-turn path fall through to the same
/// `nominal_yaw_steering` call -- a guard that had never actually fired.
/// `turn_ramp_sign` below keeps its own `x_sign` parameter: PROVED, not
/// merely argued, that removing `x_sign` from THIS function alone, leaving
/// `turn_ramp_sign` untouched, leaves `evaluate_turn`'s own `gap` sign (so
/// the turn's own active region, timing, and hand-over points) EXACTLY
/// unchanged for every block -- the `psi_s` shift and the nominal-law delta
/// used to build `gap` cancel algebraically, confirmed both symbolically
/// and by direct numerical re-evaluation across a dense IIR noon-turn
/// sweep (4000 points, zero activity mismatches, returned psi differing
/// from the pre-fix value by exactly pi at every active one, to 8.88e-16) --
/// while `frame_from_yaw`'s own x_body, fed the corrected psi, flips sign
/// throughout the ENTIRE turn, not only at the boundary, landing on the
/// Sun-facing convention the ideal law already uses outside it. `TYAW-A-013`
/// is extended to check IIR (`plan`'s own x_sign=-1 case) the same way as
/// every other block now that this is safe to test.
[[nodiscard]] double psi_nominal(double beta, double mu) noexcept {
    return std::atan2(-std::tan(beta), -std::sin(mu));
}

/// KOUBA09 Eq. 6, evaluated at a specific mu (not necessarily the current
/// query mu -- shadow crossing needs it at the entry angle specifically).
/// NEGATED relative to Eq. 6 as printed, for the SAME reason `psi_nominal`
/// above is (PROVENANCE.md Sec.30.16, correcting an earlier version of this
/// comment that named `psi_nominal`'s own fix, rather than this tree's yaw
/// convention, as the cause): psi_tree = pi - psi_KOUBA09 gives
/// d(psi_tree)/d(mu) = -d(psi_KOUBA09)/d(mu) directly, independent of
/// mu_rad's own sign -- this function returns d(psi_tree)/d(mu), the
/// negative of Eq. 6 as printed (d(psi_KOUBA09)/d(mu)), because `psi_tree`
/// and `psi_KOUBA09` are themselves related by that same negation, not
/// because anything needs compensating. Checked by finite difference
/// against the fixed `psi_nominal` directly (PROVENANCE.md Sec.30.14/
/// Sec.30.16), not assumed from the chain rule alone.
[[nodiscard]] double psidot_nominal(double beta, double mu) noexcept {
    const double t = std::tan(beta);
    const double s = std::sin(mu);
    return -(kMuDotRadPerS * t * std::cos(mu) / (s * s + t * t));
}

/// The ramp's own constant-rate direction for a noon/midnight-shaped turn
/// (KOUBA09's own SIGN[R, psi_dot_n(t_s)]) -- Eq. 15 (II/IIA) and Eq. 16
/// (IIR) carry this EXACT SAME term, verbatim, not a block-dependent one:
/// his own text states the two turns are "modeled in the same fashion...
/// except for the 180 deg reversal of X-bar", and that reversal is the
/// ATAN2 term alone (Eq. 4 vs Eq. 5) -- SIGN[R, psi_dot_n(t_s)] is untouched
/// by it. His own psi_dot_n, Eq. 6, is ALSO printed as a single formula, no
/// IIR variant given, matching `psidot_nominal` above (which, independent
/// of any of this, already does not take an x_sign, PROVENANCE.md
/// Sec.30.16). No `x_sign` PARAMETER here either (removed 2026-09-24, the
/// manager's own finding and correction of an earlier version of this
/// function, PROVENANCE.md Sec.30.20): algebraically, sign(psi_dot_n) =
/// sign(beta) * sign(cos(mu_s)), and cos(mu_s) has a FIXED sign for each
/// turn family (negative approaching noon, positive approaching midnight,
/// since the turn's own half-width is always < 90 deg) -- so this needs no
/// direct, beta-near-zero-fragile evaluation of psi_dot_n itself. sign(beta)
/// uses TYAW-R-007's own RULED convention: sign(0) := +1, the stateless
/// tie-break for the beta-near-zero edge case (TYAW-Q-002).
///
/// An earlier version of this function multiplied by `x_sign`, reasoning
/// (wrongly) that IIR's own "180 deg reversal" reaches this term too --
/// it does not, per KOUBA09's own Eq. 15/16 quoted above, confirmed
/// independently three ways before this was trusted: (1) the source's own
/// rendered page, not a prior transcription (the same discipline that
/// caught a dropped minus sign in `MSGA15`'s own text this same day); (2) a
/// direct symbolic/numerical check that d(psi_K)/d(mu), Eq. 4 or Eq. 5
/// alike, is x_sign-INDEPENDENT (x^2 = 1 cancels inside the ATAN2
/// derivative), confirmed to 2.2e-9 over 2000 random points; (3) `TYAW-A-015`
/// (the permanent guard for this), checking the ramp's own sense against an
/// INDEPENDENT finite difference of `nominal_yaw_steering`'s own psi at
/// onset, shown to FAIL for IIR on the `x_sign`-multiplied code (`plan`
/// rule 5) before being trusted to pass after. The earlier version's own
/// mistake ran IIR's ramp AGAINST the nominal law's own rate at onset,
/// forcing the turn to complete "the long way round" -- roughly a lap and
/// change of ATAN2's own branch -- rather than catching up promptly; this
/// is what CODE's own real G05 data was showing all along (PROVENANCE.md
/// Sec.30.20's own pointwise account), which an earlier pass on this same
/// day misread as evidence of KOUBA09's own printed hardware wind-up effect
/// (PROVENANCE.md Sec.30.14, commit d7a1452) -- a real, printed effect, but
/// not the cause of the residual measured here; that explanation is
/// withdrawn in place there, not deleted.
[[nodiscard]] double turn_ramp_sign(double beta, bool is_noon) noexcept {
    const double sign_beta = (beta < 0.0) ? -1.0 : 1.0;
    return -(sign_beta * (is_noon ? -1.0 : 1.0));
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
/// for a midnight/night turn. No `x_sign` parameter (removed here,
/// PROVENANCE.md Sec.30.19/TYAW-Q-007, `psi_nominal`'s own comment has the
/// full account): this function only ever needed it to pass through to
/// `psi_nominal`, which no longer takes one.
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
[[nodiscard]] TurnResult evaluate_turn(double beta, double mu_current,
                                        double mu_center, double onset_rad, double rate_rad_per_s,
                                        double ramp_sign) noexcept {
    const double width_sq = onset_rad * std::abs(beta) - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_s = mu_center - half_width;
    const double mu_q = wrap_near(mu_current, mu_center);
    if (mu_q < mu_s) return {false, 0.0};

    const double psi_s = psi_nominal(beta, mu_s);
    const double psi_ramp = psi_s + ramp_sign * rate_rad_per_s * (mu_q - mu_s) / kMuDotRadPerS;
    const double delta_raw = psi_nominal(beta, mu_q) - psi_s;
    const double delta_directional = wrap_directional(delta_raw, ramp_sign > 0.0);
    const double psi_nom_directional = psi_s + delta_directional;
    const double gap = (psi_nom_directional - psi_ramp) * ramp_sign;
    if (gap > 0.0) return {true, psi_ramp};
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
/// beta may be either sign here) nominal rate's own sign at entry. `mu_s`
/// IS the true-time entry (PROVENANCE.md Sec.30.14): mu now increases with
/// true time (`mu_rad`'s own fix above), so the smaller of the two boundary
/// values is the one reached first -- this was already the code's own
/// structure, not something this fix needed to change, only to confirm.
/// No `x_sign` parameter (removed here, PROVENANCE.md Sec.30.19/TYAW-Q-007,
/// `psi_nominal`'s own comment has the full account): this function is
/// only ever reached for II/IIA (`gps_yaw_attitude`'s own dispatch), whose
/// x_sign was always +1, so dropping the now-unused pass-through changes
/// nothing here -- it is removed for the same reason it was removed from
/// `psi_nominal` and `evaluate_turn`, not carried as a vestige.
[[nodiscard]] TurnResult evaluate_shadow_crossing(double beta, double mu_current,
                                                   double rate_rad_per_s, double rr_rad_per_s2,
                                                   double bias_rad) noexcept {
    const double width_sq = kShadowHalfAngleRad * kShadowHalfAngleRad - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_s = -half_width;
    const double mu_e = half_width;
    const double mu_q = wrap_near(mu_current, 0.0);
    if (mu_q < mu_s || mu_q > mu_e) return {false, 0.0};

    const double psi_s = psi_nominal(beta, mu_s);
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
/// The swing has a closed form, not a numerical walk: d(psi_n)/d(mu) (of
/// the FIXED `psi_nominal` above) is -tan(beta)*cos(mu) / (sin(mu)^2 +
/// tan(beta)^2) (`psidot_nominal`, already carrying this same negation,
/// divided by mu_dot) and integrates, by the standard 1/(u^2+a^2) form
/// under u = sin(mu), to -ATAN(sin(mu)/tan(beta)) -- unlike `psi_nominal`
/// itself (an ATAN2, with a branch cut), this antiderivative is smooth and
/// single-valued for every mu in the window at any beta != 0, so no
/// unwrapping or step count is needed: the swing is just its value at
/// mu_exit minus its value at mu_entry (PROVENANCE.md Sec.30.14 records
/// the sign flip this carries relative to the pre-fix version, verified
/// against real ORBEX data, not assumed from the antiderivative alone).
/// Evaluated AT the query mu_q, this constant rate is exactly linear
/// interpolation in mu between (mu_entry, psi_entry) and (mu_exit,
/// psi_entry + swing) -- mu_dot itself cancels (a constant rate over a
/// mu-interval, sampled at a fraction of that interval, does not need to
/// know how fast mu itself moves), so it does not appear in the return
/// expression at all. No `x_sign` parameter (removed here, PROVENANCE.md
/// Sec.30.19/TYAW-Q-007, `psi_nominal`'s own comment has the full account):
/// this function is only ever reached for IIF/IIIA, whose x_sign was
/// always +1, so dropping the now-unused pass-through changes nothing here.
[[nodiscard]] TurnResult evaluate_shadow_constant_rate(double beta, double mu_current) noexcept {
    const double width_sq = kShadowHalfAngleRad * kShadowHalfAngleRad - beta * beta;
    if (width_sq <= 0.0) return {false, 0.0};
    const double half_width = std::sqrt(width_sq);
    const double mu_entry = -half_width;
    const double mu_exit = half_width;
    const double mu_q = wrap_near(mu_current, 0.0);
    if (mu_q < mu_entry || mu_q > mu_exit) return {false, 0.0};

    const double psi_entry = psi_nominal(beta, mu_entry);
    const double tan_beta = std::tan(beta);
    const double swing = std::atan(std::sin(mu_entry) / tan_beta) - std::atan(std::sin(mu_exit) / tan_beta);
    return {true, psi_entry + swing * (mu_q - mu_entry) / (mu_exit - mu_entry)};
}

/// Builds M_gcrs_to_body from a turn's own yaw angle psi and the orbit
/// triad. z_body is always nadir (yaw rotates only about it). Verified
/// numerically (PROVENANCE) against `nominal_yaw_steering`'s own independent
/// z=-r_hat/y=(z x s)/x=y x z construction, across many random geometries, to
/// machine precision (max component error 4e-16): x_body =
/// -cos(psi)*t_hat - sin(psi)*n_hat is the relation that reproduces it -- for
/// WHATEVER psi this is fed, correctly, always; this is a statement about
/// the FRAME formula alone, not about `TYAW-P-2` continuity at hand-over
/// (an earlier version of this comment claimed continuity followed from
/// this identity directly -- it does not, PROVENANCE.md Sec.30.19 found:
/// IIR's own psi WAS discontinuous at hand-over, `x_sign` bug, fixed at
/// `psi_nominal`, not here; II/IIA's own shadow-crossing law is STILL
/// discontinuous at exit, on KOUBA09's own terms, a documented gap,
/// `SPEC-thrust-yaw` Sec.4.1's own "largely uncertain" post-shadow period).
/// Continuity, where it holds, is a property of whatever function computes
/// psi -- `evaluate_turn`'s own onset construction, `evaluate_shadow_
/// constant_rate`'s own swing -- checked per caller, not inherited from
/// this function alone.
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

// --- SPEC-galileo-attitude: GALY-R-001..R-005 ------------------------------

/// GSC's own orbital RF (Sec.2, Sec.3.1): +Z toward Earth centre (nadir),
/// +Y perpendicular to the orbital plane ("across-track"), +X completing
/// the right-handed system, "pointing mainly in the flight direction"
/// (along-track). Expressed in terms of THIS file's own OrbitTriad:
/// Z_orb = -r_hat (nadir), X_orb = t_hat (prograde, matches "along-track"
/// directly), Y_orb = Z_orb x X_orb = -n_hat -- the unique right-handed
/// completion, GSC's own text naming no separate sign for Y beyond
/// "completes the system".
struct GalileoOrbitalSun { double x, y, z; };

[[nodiscard]] GalileoOrbitalSun galileo_orbital_sun(const OrbitTriad& tri, const Vec3& s_hat) noexcept {
    return GalileoOrbitalSun{tri.t_hat.dot(s_hat), -tri.n_hat.dot(s_hat), -tri.r_hat.dot(s_hat)};
}

/// GSC Sec.3.1.1's own IOV equation and Sec.3.1.2's own FOC equation,
/// REDUCED to one shared form (GALY-A-004 proves the reduction, not merely
/// asserts it): IOV's own psi_r = atan2(-S_Y/sqrt(1-S_Z^2), -S_X/sqrt(1-S_Z^2))
/// -- sqrt(1-S_Z^2) is a positive scalar for any physical S (|S_Z|<1) and
/// cancels inside atan2 -- and FOC's own psi(t) = atan2(s.n, s.(r x n)),
/// with n = n_hat (orbit normal): s.n_hat = -S_Y (this file's own
/// galileo_orbital_sun), and s.(r_hat x n_hat) = s.(-t_hat) = -S_X (since
/// t_hat = n_hat x r_hat here, r_hat x n_hat = -(n_hat x r_hat) = -t_hat) --
/// giving atan2(-S_Y, -S_X), ALGEBRAICALLY IDENTICAL to IOV's own primary
/// form. Both blocks' own nominal law is this one function.
[[nodiscard]] double galileo_psi_nominal(const GalileoOrbitalSun& s) noexcept {
    return std::atan2(-s.y, -s.x);
}

/// The inverse of `galileo_orbital_sun`: reconstructs a unit GCRS vector
/// from its own components in the Galileo orbital RF -- used to turn IOV's
/// own substituted auxiliary Sun vector back into a real direction
/// `nominal_yaw_steering` can consume, reusing that function's own frame
/// construction rather than re-deriving it.
[[nodiscard]] Vec3 from_galileo_orbital(const OrbitTriad& tri, const GalileoOrbitalSun& s) noexcept {
    return s.x * tri.t_hat + s.y * (-1.0 * tri.n_hat) + s.z * (-1.0 * tri.r_hat);
}

/// GSC Sec.3.1.1's own auxiliary-region constants.
constexpr double kGalileoBetaXRad = 15.0 * kDegToRad;
constexpr double kGalileoBetaYRad = 2.0 * kDegToRad;

[[nodiscard]] bool galileo_iov_in_auxiliary_region(const GalileoOrbitalSun& s) noexcept {
    return std::abs(s.x) < std::sin(kGalileoBetaXRad) && std::abs(s.y) < std::sin(kGalileoBetaYRad);
}

/// GSC Sec.3.1.1's own auxiliary Sun reference vector S_H, substituted for
/// the real S_o inside the named region so the yaw rate stays bounded as
/// beta -> 0 near noon/midnight. Gamma ("the sign of S_oy at the beginning
/// of the auxiliary region") is read at the CURRENT query instant instead
/// of remembered from region entry: beta moves on the orbital-precession
/// timescale, far slower than the eta-driven transit through this region,
/// so its sign is constant across one transit except exactly at beta = 0 --
/// the SAME stateless tie-break shape `gps_yaw_attitude`'s own sign(beta)
/// already uses (TYAW-Q-002's own ruled convention), not a new one invented
/// here. `GALY-A-007` checks continuity at the region's own boundary,
/// which this approximation does not disturb (Gamma does not change sign
/// within one transit by construction).
[[nodiscard]] GalileoOrbitalSun galileo_iov_auxiliary(const GalileoOrbitalSun& s) noexcept {
    const double gamma = (s.y < 0.0) ? -1.0 : 1.0;
    const double sin_bx = std::sin(kGalileoBetaXRad);
    const double sin_by = std::sin(kGalileoBetaYRad);
    const double hy = 0.5 * (sin_by * gamma + s.y) +
                      0.5 * (sin_by * gamma - s.y) * std::cos(M_PI * std::abs(s.x) / sin_bx);
    const double hz_sq = 1.0 - s.x * s.x - hy * hy;
    const double hz = std::sqrt(hz_sq < 0.0 ? 0.0 : hz_sq) * ((s.z < 0.0) ? -1.0 : 1.0);
    return GalileoOrbitalSun{s.x, hy, hz};
}

/// GSC Sec.3.1.1's own IOV formula and Sec.3.1.2's own FOC formula, as a
/// 2-argument closed form in (beta, mu) alone -- GPS's own `psi_nominal`
/// shape, for Galileo's own convention: S_X = cos(beta)*sin(mu), S_Y =
/// -sin(beta), S_Z = cos(beta)*cos(mu) (derived from this file's own
/// `galileo_orbital_sun` and the standard (r_hat, n_hat, t_hat) triad,
/// checked against a real fixture, `GALY-A-004`), giving psi = atan2(-S_Y,
/// -S_X) = atan2(sin(beta), -cos(beta)*sin(mu)).
[[nodiscard]] double galileo_psi_nominal_beta_mu(double beta, double mu) noexcept {
    return std::atan2(std::sin(beta), -std::cos(beta) * std::sin(mu));
}

/// Builds M_gcrs_to_body from a Galileo yaw angle psi (this file's own
/// atan2(-S_Y,-S_X) convention) and the orbit triad -- Galileo's own
/// `frame_from_yaw`. DERIVED, not guessed: substituting S_X =
/// -sqrt(1-S_Z^2)*cos(psi), S_Y = -sqrt(1-S_Z^2)*sin(psi) (psi's own
/// definition) into `nominal_yaw_steering`'s own x_body construction
/// (z_body=-r_hat, y_body=normalize(z_body x s_hat), x_body=y_body x
/// z_body), expanded in terms of t_hat/n_hat/r_hat, gives x_body =
/// -cos(psi)*t_hat + sin(psi)*n_hat EXACTLY -- the sqrt(1-S_Z^2) factor
/// cancels. NOTE the sign on the sin(psi)*n_hat term is OPPOSITE GPS's own
/// `frame_from_yaw` (-sin, not +sin): a DIFFERENT psi convention (this
/// file's own atan2(-S_Y,-S_X), not KOUBA09's Eq. 4/5), not a transcription
/// of GPS's own formula with a sign slipped -- `GALY-A-010` checks this
/// reproduces `nominal_yaw_steering`'s own output exactly when fed the
/// UNMODIFIED nominal psi, proving the derivation rather than trusting the
/// algebra alone.
[[nodiscard]] Mat3 galileo_frame_from_psi(const OrbitTriad& tri, double psi) noexcept {
    const Vec3 z_body = -1.0 * tri.r_hat;
    const Vec3 x_body = (-std::cos(psi)) * tri.t_hat + std::sin(psi) * tri.n_hat;
    const Vec3 y_body = z_body.cross(x_body);
    Mat3 m;
    m.r[0] = {x_body.x, x_body.y, x_body.z};
    m.r[1] = {y_body.x, y_body.y, y_body.z};
    m.r[2] = {z_body.x, z_body.y, z_body.z};
    return m;
}

/// GSC Sec.3.1.2's own colinearity angle epsilon, PROVED (not merely
/// observed, `GALY-A-009`) to depend on mu ALONE, independent of beta:
/// epsilon's own defining construction (x = n x s, y = n x x, epsilon =
/// fold(arccos(r.y_hat))) reduces, substituting S_X = cos(beta)*sin(mu),
/// S_Z = cos(beta)*cos(mu), to cos(raw_epsilon) = S_Z/cos(beta) = cos(mu)
/// EXACTLY -- the cos(beta) factor cancels. So `raw_epsilon = |mu|`, and
/// `epsilon = fold(|mu|)`, `fold(x) = x` for `x <= 90 deg` else `180 deg -
/// x`. THIS IS WHAT MAKES THE MODIFIED LAW'S OWN "MOMENT ITS WINDOW OPENED"
/// COMPUTABLE FROM GEOMETRY ALONE (the manager's own instruction): the
/// window's own entry mu is a FIXED closed-form constant (+/-10 deg near
/// midnight, 170/190 deg near noon), never requiring a remembered crossing
/// -- the same closed-form-entry-point shape `evaluate_shadow_crossing`
/// already uses for GPS's own IIF shadow (mu_s = -half_width, a constant
/// given beta, not searched for or remembered).
[[nodiscard]] double galileo_fold_epsilon_rad(double mu) noexcept {
    const double m = std::abs(wrap_near(mu, 0.0));
    return (m <= M_PI / 2.0) ? m : (M_PI - m);
}

constexpr double kFocBetaGateRad = 4.1 * kDegToRad;
constexpr double kFocEpsilonGateRad = 10.0 * kDegToRad;
/// GSC Sec.3.1.2's own printed period, the modified law's own cosine ramp.
constexpr double kFocSwingPeriodS = 5656.0;

struct FocWindow {
    bool active;
    double mu_s;  ///< the window's own entry mu (a fixed constant, this beta-independent)
};

/// GSC Sec.3.1.2's own switch-over condition (|beta| < 4.1 deg AND epsilon <
/// 10 deg), reformulated through `galileo_fold_epsilon_rad`'s own proof: the
/// window is `mu` within 10 deg of 0 (midnight) or within 10 deg of +/-180
/// deg (noon), `mu_s` the LOWER edge of whichever window `mu` currently
/// sits in (the edge a normal, mu-increasing prograde pass enters through
/// first) -- stateless, no remembered "previous epoch" needed (GSC's own
/// third condition, "the colinearity angle for the previous epoch was
/// bigger than 10 deg", is exactly "mu was, an instant ago, outside this
/// same geometric window", automatic for a monotonically increasing mu and
/// so not separately tracked).
[[nodiscard]] FocWindow galileo_foc_window(double beta, double mu) noexcept {
    if (std::abs(beta) >= kFocBetaGateRad) return {false, 0.0};
    if (galileo_fold_epsilon_rad(mu) >= kFocEpsilonGateRad) return {false, 0.0};
    // Inside the window: which side (midnight or noon) decides mu_s, the
    // entry a normal, mu-increasing pass crosses first.
    const double mu_midnight = wrap_near(mu, 0.0);
    if (std::abs(mu_midnight) < M_PI / 2.0) return {true, -kFocEpsilonGateRad};
    return {true, M_PI - kFocEpsilonGateRad};
}

/// GSC Sec.3.1.2's own "modified yaw steering law", transcribed: psi_mod
/// (t_mod) = 90deg*sign + (psi_init - 90deg*sign)*cos(2*pi/5656s * t_mod),
/// sign = sign(psi_init), psi_init = psi(t) AT the switch-over,
/// t_mod = elapsed time since it. `mu_s` (`galileo_foc_window`'s own
/// closed-form window entry) makes psi_init computable directly:
/// `galileo_psi_nominal_beta_mu(beta, mu_s)`, the SAME current beta
/// (assumed constant across the brief transit, the SAME approximation
/// IOV's own Gamma already relies on, Sec.3, `SPEC-galileo-attitude.md`).
/// `t_mod` needs an angular RATE to convert an elapsed mu into elapsed
/// time: `mu_dot_rad_per_s` is the CURRENT (r,v)'s own instantaneous
/// osculating rate, |r x v|/|r|^2 -- computed fresh from the caller's own
/// state each call, not a fixed constant the way GPS's own
/// `kMuDotRadPerS` is (a different orbit, a different period) -- so this
/// function, and its own caller, stay stateless.
[[nodiscard]] double galileo_foc_modified_psi(double beta, double mu_dot_rad_per_s, double mu_s,
                                              double mu_current) noexcept {
    const double psi_init = galileo_psi_nominal_beta_mu(beta, mu_s);
    const double sign_init = (psi_init < 0.0) ? -1.0 : 1.0;
    const double mu_q = wrap_near(mu_current, mu_s);
    const double t_mod = (mu_q - mu_s) / mu_dot_rad_per_s;
    const double half_pi_signed = M_PI / 2.0 * sign_init;
    return half_pi_signed +
           (psi_init - half_pi_signed) * std::cos(2.0 * M_PI / kFocSwingPeriodS * t_mod);
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
    // No `x_sign` here (removed 2026-09-24, the last of it -- PROVENANCE.md
    // Sec.30.19/30.20): every block now shares one frame and one ramp-sign
    // rule, differing only in the HARDWARE RATES `rates` itself carries.
    const GpsBlock effective_block = (block == GpsBlock::IIIA) ? GpsBlock::IIF : block;

    const OrbitTriad tri = orbit_triad(r_gcrs_m, v_gcrs_m_per_s);
    const Vec3 s_hat = normalized(sun_direction_gcrs);
    const double beta = signed_beta_rad(s_hat, tri.n_hat);
    const double mu = mu_rad(tri, s_hat);

    const double noon_rate = rates.noon_deg_per_s * kDegToRad;
    const double night_rate = rates.night_deg_per_s * kDegToRad;
    const double noon_onset = std::atan(kMuDotRadPerS / noon_rate);  // KOUBA09 Eq. 7

    // TYAW-R-001/R-002/R-003: the noon turn LAGS, for every block --
    // KOUBA09's own words, "the actual yaw angle to temporarily lag behind
    // the nominal yaw attitude" (stated for II/IIA and IIR explicitly), and
    // CODE's own real IIF data, read in KOUBA09's own mu once `mu_rad`'s
    // own sign was corrected: centred within 0.1 deg of the lag law's own
    // prediction at both real crossings checked, not the mirror-imaged
    // "lead" an earlier pass reported before the mu fix (PROVENANCE.md
    // Sec.30.12/Sec.30.14 -- `evaluate_turn_lead` was a real, working
    // implementation of a shape IIF does not actually fly, reverted once
    // the mu bug was found, not because the implementation was wrong for
    // what it modelled).
    TurnResult result = evaluate_turn(beta, mu, M_PI, noon_onset, noon_rate,
                                       turn_ramp_sign(beta, /*is_noon=*/true));

    if (!result.active) {
        switch (effective_block) {
            case GpsBlock::II_IIA:
                // TYAW-R-001: no separate beta0-based midnight turn -- the
                // night side is the shadow-crossing regime, whenever
                // actually eclipsed, regardless of beta.
                result = evaluate_shadow_crossing(beta, mu, night_rate,
                                                   rates.spin_up_deg_per_s2 * kDegToRad,
                                                   rates.yaw_bias_deg * kDegToRad);
                break;
            case GpsBlock::IIR_IIRM: {
                // TYAW-R-002: midnight turn, same rate-limited ramp shape as
                // noon, IIR's own single hardware rate (identical to noon).
                const double night_onset = std::atan(kMuDotRadPerS / night_rate);
                result = evaluate_turn(beta, mu, 0.0, night_onset, night_rate,
                                        turn_ramp_sign(beta, /*is_noon=*/false));
                break;
            }
            case GpsBlock::IIF:
                // TYAW-R-003: night side is Shape E, the shadow-crossing
                // regime (like II/IIA's own TYAW-R-001), not a rate-limited
                // ramp -- `rates.night_deg_per_s` is not read here.
                result = evaluate_shadow_constant_rate(beta, mu);
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

odl::Result<Mat3, AttitudeError>
galileo_yaw_attitude(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                     const Vec3& sun_direction_gcrs, GalileoBlock block) {
    const OrbitTriad tri = orbit_triad(r_gcrs_m, v_gcrs_m_per_s);
    const Vec3 s_hat = normalized(sun_direction_gcrs);
    const GalileoOrbitalSun s = galileo_orbital_sun(tri, s_hat);

    if (block == GalileoBlock::IOV) {
        const GalileoOrbitalSun eff =
            galileo_iov_in_auxiliary_region(s) ? galileo_iov_auxiliary(s) : s;
        const Vec3 s_eff = from_galileo_orbital(tri, eff);
        // GALY-F-002: forwarded unchanged from ATTD-F-001 (nominal_yaw_steering's
        // own singularity guard), reached only if the auxiliary substitution
        // itself somehow lands exactly on the nadir axis -- not expected, since
        // the whole point of the substitution is to stay away from it, but not
        // asserted unreachable either.
        return nominal_yaw_steering(r_gcrs_m, s_eff);
    }

    // FOC: GSC's own "modified yaw steering law" inside its own named
    // near-colinearity switch-over region (GALY-R-003, built per the
    // manager's own ruling, 2026-09-24 -- withdraws the earlier version's
    // own refusal there, GALY-F-001 retired); GSC's own primary formula
    // outside it.
    const double beta = signed_beta_rad(s_hat, tri.n_hat);
    const double mu = mu_rad(tri, s_hat);
    const FocWindow window = galileo_foc_window(beta, mu);
    if (window.active) {
        const double mu_dot_rad_per_s =
            r_gcrs_m.cross(v_gcrs_m_per_s).norm() / (r_gcrs_m.norm() * r_gcrs_m.norm());
        const double psi = galileo_foc_modified_psi(beta, mu_dot_rad_per_s, window.mu_s, mu);
        return galileo_frame_from_psi(tri, psi);
    }
    // GALY-F-002: forwarded unchanged from ATTD-F-001, outside the colinearity
    // region (which is itself outside the nadir singularity nominal_yaw_steering
    // guards -- the two regions do not coincide).
    return nominal_yaw_steering(r_gcrs_m, s_hat);
}

double galileo_native_yaw_angle_pre_substitution(const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
                                                 const Vec3& sun_direction_gcrs) noexcept {
    const OrbitTriad tri = orbit_triad(r_gcrs_m, v_gcrs_m_per_s);
    const Vec3 s_hat = normalized(sun_direction_gcrs);
    return galileo_psi_nominal(galileo_orbital_sun(tri, s_hat));
}

}  // namespace odl::attitude
