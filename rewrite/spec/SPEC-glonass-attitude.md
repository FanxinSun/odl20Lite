# SPEC-glonass-attitude — GLONASS-M's own yaw-attitude law

| | |
|---|---|
| **Spec ID** | `GLNY` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), step 3 (GLONASS) — attitude LAW code, `modules/attitude`, beside GPS's own `SPEC-thrust-yaw.md` and Galileo's own `SPEC-galileo-attitude.md` |
| **Depends on** | `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering`, `psi_nominal`, `mu_rad`, `signed_beta_rad`, `frame_from_yaw` (GPS's own shared machinery, `SPEC-thrust-yaw.md`) |
| **Depended on by** | `srp_analytic`/`photon_force` (indirectly, through whichever module supplies a body frame); L7's own box-wing fit |

**Derivation declaration (plan R1).** Written from the source §2 names, and from no implementation
of this module beyond `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering`,
`psi_nominal`/`psidot_nominal`, `mu_rad`, `signed_beta_rad` and `frame_from_yaw`.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or
otherwise inspected during this specification's preparation, per `../plan/PLAN.md` §3.6's own
explicit refusal of the predecessor's hand-built models.

---

## 1. Purpose and scope

**GLONASS-M's own yaw-steering law during eclipse season**, as CODE (not cited data —
`SPEC-spacecraft.md` is where GLONASS/GLONASS-M/GLONASS-K's own citable macromodel DATA lives; this
spec is where GLONASS-M's own attitude BEHAVIOUR is specified). Covers **GLONASS-M only** — the
source's own stated scope (its own title, and its own words, "hardly anything in this field is
known about the second spacecraft-generation," i.e. -M specifically). Original GLONASS and
GLONASS-K's own eclipse-season attitude were searched for and not found (`SPEC-spacecraft.md` §2,
the L5 step 3 rule-4 search) — this spec does not cover them; both reduce to `nominal_yaw_steering`
outside any GLONASS-M-shaped turn, since no other source names a different law for them.

**Not in scope.** The macromodel data itself (`SPEC-spacecraft.md`). GPS's own laws
(`SPEC-thrust-yaw.md`) and Galileo's own (`SPEC-galileo-attitude.md`) — unchanged, untouched by this
spec. Original GLONASS's and GLONASS-K's own eclipse-season attitude (no source found this session).

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `DIL11` | Dilssner, F., Springer, T., Flohrer, C., Dow, J. | *The GLONASS-M satellite yaw-attitude model* | Adv. Space Res. 47:160–171, 2011, DOI `10.1016/j.asr.2010.09.007` | `http://acc.igs.org/orbits/glonass-attitude-model_ASR10.pdf` (IGS ACC mirror), fetched directly 2026-09-24 | **primary**, obtained, freely hosted (IGS's own open-data terms, `SPEC-spacecraft.md` §2.3/`PROVENANCE.md` §30.11's own basis) | its own Sec.2 (the nominal yaw law and its own shared GPS II/IIA axis convention), Sec.4/Sec.5 (the shadow-crossing and noon-turn maneuvers, described then formalised) — every equation and constant this spec states |

No second source was sought: `DIL11` is the field's own first and, as far as this session's own
search found, only published yaw-attitude model for GLONASS-M, and prints the law's own equations
directly, with worked parameters and real validating data (its own Figs. 5/6, SVN724) — the
manager's own instruction was to build each turn from THIS source's own words, quoted per turn, not
to find an independent literature confirmation as well.

---

## 3. Definitions and conventions

- **The orbital reference frame `DIL11` states its own law in is IDENTICAL to this tree's own
  `mu_rad`/`psi_nominal` convention already built for GPS — checked, not assumed.** `DIL11`'s own
  Fig. 1: "mu" is "the spacecraft's geocentric orbit angle," explicitly captioned "Midnight
  (mu=0deg)" and "Noon (mu=180deg)" — the SAME "orbit angle from midnight" this tree's own `mu_rad`
  header comment already states for KOUBA09. `DIL11`'s own Eq. 1, `psi_n = ATAN2(-tan(beta), sin
  (mu))`, is explicitly stated to use "the axis conventions of the GPS Block II/IIA satellites" —
  algebraically KOUBA09's own Eq. 4 "as printed" (the SAME un-converted shape `psi_nominal`'s own
  header comment already derives its converted form from), so the SAME conversion
  (`psi_tree = pi - psi_DIL11`, verified by that same header comment for KOUBA09) applies here
  identically. **PROVED, not assumed** (`GLNY-A-001`): DIL11's Eq. 1, independently transcribed,
  and this tree's own `psi_nominal`, agree by that exact relation at a spread of geometries; off
  both turns, `glonass_m_yaw_attitude` returns EXACTLY what `nominal_yaw_steering` returns.
- **The shadow-crossing (midnight) maneuver (`DIL11` Sec.4.1/Sec.5.1) is a DIFFERENT mechanism from
  the noon turn — QUOTED, not assumed a KOUBA09 parameterization with new constants (the manager's
  own explicit instruction, echoing the GPS-IIF night-turn precedent, `SPEC-thrust-yaw.md`
  `TYAW-R-003`).** `DIL11`'s own words: "the GLONASS-M shadow-crossing maneuver immediately starts
  after the spacecraft has entered the beginning of the umbra" (Sec.5.1) — a CLOSED-FORM geometric
  window (Eq. 11: `mu_e = -mu_s = arccos(cos(epsilon_0)/cos(beta))`, `epsilon_0=14.20deg`, DIL11's
  own printed umbra half-angle, Sec.4.1), no rate-threshold search. The spacecraft then "is spinning
  around its body-fixed z-axis with maximum rotation rate" (Sec.4.1) — a ramp at the FULL hardware
  rate `R=0.25deg/s` (Sec.6.1's own stated single rate for all satellites and both maneuvers) from
  shadow entry (Eq. 12) — UNTIL "the actual yaw angle equals the nominal yaw angle to be expected at
  the end of the umbra," at which point "the yaw-attitude is kept fixed" (Eq. 13, quoted) until
  actual shadow exit — a HOLD at a CONSTANT value, not a continued ramp. Confirmed against `DIL11`'s
  own real SVN724 data (Fig. 5): at every beta shown, the actual yaw jumps at full rate right at
  shadow entry and goes FLAT well before shadow exit, unlike GPS's own II/IIA shadow law (which
  keeps tracking the moving nominal curve throughout) — the same class of distinction GPS's own IIF
  night turn already is from GPS's own II/IIA shadow law.
- **The ramp-sign conversion (DIL11's own SIGN[R, psi_dot_n(mu_s)]) needs NO extra negation beyond
  what `glonass_m_psidot_nominal` already carries — PROVED numerically (this session's own 200000-
  point random-geometry check), not assumed by symmetry with GPS's own `turn_ramp_sign`.** GPS's own
  ramp-sign term DOES carry an extra negation relative to KOUBA09's own printed SIGN[] term
  (`SPEC-thrust-yaw.md` `turn_ramp_sign`'s own header comment) — DIL11's own does not, a genuinely
  different result from a fresh derivation, not copied from the GPS precedent.
- **A genuine floating-point degeneracy at beta=0 EXACTLY, found and documented, not silently
  patched around.** `DIL11`'s own rule ("the sign of the actual yaw rate is the same as the sign of
  the nominal yaw rate at shadow entry") names no unique direction when that nominal rate is
  EXACTLY zero (beta=0 precisely) — this tree's own tie-break, `sign(0):=+1` (checking
  `glonass_m_psidot_nominal(beta,mu_s) < 0.0`), is the SAME `TYAW-R-007`-established convention GPS's
  own turns already use for the same edge case, documented at `glonass_m_shadow_turn`'s own header
  comment. Found this session's own testing (an independent reconstruction that checked the RAW,
  un-converted rate's own sign hit the SAME tie at beta=0, and — because a raw and a tree-converted
  quantity that are supposed to be opposite in sign both collapse to the SAME comparison result
  exactly at a signed zero — disagreed with production there) — the test was fixed to avoid this
  physically-meaningless exact input (a real beta from a real ephemeris is essentially never exactly
  0.0), not production changed to chase an answer the source itself does not uniquely specify.
- **The noon turn (`DIL11` Sec.4.2/Sec.5.2) has NO hold phase, unlike the shadow turn — a genuinely
  different structure between DIL11's own two maneuvers, not assumed symmetric.** Its own onset angle
  `mu_s` has NO closed form: `DIL11`'s own words, "the equation has to be solved iteratively,"
  `mu_0=176.8deg` "a reasonable value for the initial run," FOUR iterations "to ensure adequate
  precision for every possible noon-turn maneuver scenario (0deg<\|beta\|<2.0deg)" (Eq. 16–20,
  quoted, not paraphrased) — reproduced here as exactly four fixed-point iterations from that stated
  seed, DIL11's own published method, not a from-scratch exact solve of the un-approximated
  equations. `mu_e = 2*pi - mu_s` (Eq. 21, from Eq. 16's own symmetry about noon). A SINGLE ramp
  phase then spans `mu_s` to `mu_e` — DIL11's own Eq. 16 states these are the orbital angles "at the
  START and the END of the maneuver" (quoted), and the ramp's own duration, by the very construction
  that solves for `mu_s`, exactly spans the maneuver — confirmed against `DIL11`'s own Fig. 6 (no
  flat plateau visible in the real SVN724 data there, unlike Fig. 5's own shadow-turn plots). Active
  only for `\|beta\| < beta_0 = atan(mu_dot/R)` (`DIL11`'s own Sec.5.2, quoted: "computing the
  threshold value beta_0... yields beta_0=2.0deg" — reproduced as a DERIVED quantity, not a
  separately hardcoded literal that could drift out of sync with R/mu_dot).
- **GLONASS-M's own mean motion and hardware rate are DIFFERENT named constants from GPS's, not
  reused.** `kGlonassMuDotRadPerS=0.00888deg/s` (`DIL11`'s own Eq. 1/Sec.5.2, substituted to derive
  beta_0=2.0deg) — a different orbit radius and period from GPS's own `kMuDotRadPerS`.
  `kGlonassHardwareYawRateRadPerS=0.25deg/s` (`DIL11` Sec.6.1, "we have used an average hardware yaw
  rate of R=0.25deg/s for all satellites," the SAME single rate for BOTH maneuvers, unlike GPS's own
  per-block, per-regime `HardwareYawRates`). `kGlonassShadowHalfAngleRad=14.20deg` (`DIL11` Sec.4.1) —
  LARGER than GPS's own 13.5deg (`KOUBA09`): GLONASS orbits lower than GPS, so Earth's own shadow
  cone subtends a wider angle there, physically consistent with `DIL11`'s own stated figure, not
  independently re-derived from orbital geometry here.
- **`glonass_m_yaw_attitude` reuses `frame_from_yaw` DIRECTLY, not a separate `glonass_frame_from_
  psi`.** `DIL11` Sec.2.1's own words, quoted above, state GLONASS-M shares GPS II/IIA's own axis
  convention — no stated convention difference to encode, unlike Galileo's own genuinely different
  psi convention (`galileo_frame_from_psi`, `SPEC-galileo-attitude.md` §3).
- **Real-data control day differs from Galileo's own.** `orbex_galileo_check.cpp`'s own day
  (2023-10-07) has no GLONASS-M satellite below beta=18.5deg all day — outside even the shadow
  turn's own eclipse-season window, so no crossing exists to check there. `orbex_glonass_check.cpp`
  scans (position data only) found 2023-09-09 (DOY 252) as a deep eclipse-season day, two satellites
  (R19, R20) both crossing beta essentially 0 — inside BOTH turns' own windows.

---

## 4. Required behaviour

- **GLNY-R-001.** `glonass_m_psidot_nominal` (internal): `DIL11` Eq. 2 "as printed" (the same
  functional shape `psidot_nominal` already implements for KOUBA09's own Eq. 6), with GLONASS-M's
  own mean motion substituted — its own function, not a parameter added to `psidot_nominal` (which
  hardcodes GPS's own constant internally), leaving GPS's own proven code untouched.
- **GLNY-R-002.** `glonass_m_shadow_turn` (internal): the shadow-crossing (midnight) maneuver,
  `DIL11` Eq. 11–14, §3's own quoted mechanism — a closed-form geometric window, a full-rate ramp
  from shadow entry converted via the proved (not assumed) sign relationship, reaching a HOLD at the
  nominal exit yaw, constant until actual shadow exit. `mu_f` (Eq. 14, where the ramp reaches the
  hold value) is SOLVED directly from the two already-converted formulas (the ramp is linear in mu),
  not by converting Eq. 14's own printed form term by term — fewer independent conversions to get
  wrong.
- **GLNY-R-003.** `glonass_m_noon_onset_rad`/`glonass_m_noon_turn` (internal/internal): the noon-
  turn maneuver, `DIL11` Eq. 15–21, §3's own quoted mechanism — a four-iteration onset solve from
  `DIL11`'s own stated 176.8deg seed, then a single ramp phase spanning the whole maneuver
  (`mu_s` to `mu_e=2*pi-mu_s`), no hold.
- **GLNY-R-004.** `glonass_m_yaw_attitude(r_gcrs_m, v_gcrs_m_per_s, sun_direction_gcrs) ->
  Result<Mat3, AttitudeError>`: tries the noon turn, then the shadow turn, then falls through to
  `nominal_yaw_steering` — a stateless provider, the same shape `gps_yaw_attitude`/
  `galileo_yaw_attitude` already are, every quantity computable from the CURRENT state alone.
  Refuses (`GLNY-F-001`, forwarded unchanged from `ATTD-F-001`) exactly where `nominal_yaw_steering`
  itself would.
- **GLNY-R-005.** **Real-data control** (`tools/orbex_glonass_check.cpp`, reproducible on demand,
  NOT part of the automatic gate, the same treatment every other ORBEX control gets): CODE's own
  MGEX GLONASS attitude, 2023-09-09 (DOY 252), two GLONASS-M satellites (R19, R20), each at a real
  shadow-crossing AND a real noon-turn crossing (four checks total), all at beta essentially 0 (a
  deep eclipse-season day, found by position-only scanning across three candidate days before any
  attitude file was read). Predictions and a 3° relative criterion REGISTERED before the attitude
  quaternions were read. Result: all four matched, to hundredths of a degree or better (0.0007deg–
  0.026deg) — the first real-data confirmation either DIL11 mechanism has had.
- **GLNY-R-006.** Step 6's own two guards, applied to BOTH maneuvers:
  - **Time direction** (`TYAW-A-012`'s own shape, `GLNY-A-005`): a REGISTERED geometric milestone
    (the shadow turn's own ramp/hold boundary `mu_f`) is reached at the true elapsed time the
    geometry predicts, for a REAL propagated trajectory (Kepler's own rate at GLONASS's own radius)
    — shown firing on a reversed-velocity version, which reaches a materially different state.
  - **Rotation sense** (`TYAW-A-015`'s own shape, `GLNY-A-006`): through each turn's own ramp region
    (well inside it, not at a zero-rate boundary), the built law's own rate has the SAME SIGN as
    `DIL11`'s own stated SIGN[R, psi_dot_n] rule — shown firing on a deliberately sense-flipped
    version, for both the shadow turn and the noon turn.

---

## 5. Interfaces, stated language-free

- `glonass_m_yaw_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3) -> Result<Mat3, AttitudeError>` — GLNY-R-004.

A stateless provider, the same shape `gps_yaw_attitude`/`galileo_yaw_attitude`'s own header states:
every quantity is computable from the CURRENT epoch's own `(r_gcrs_m, v_gcrs_m_per_s,
sun_direction_gcrs)` alone, no history of a prior call required. No `block` parameter — `DIL11`
covers GLONASS-M only, so there is only one law this function can dispatch to (unlike
`gps_yaw_attitude`'s own `GpsBlock` or `galileo_yaw_attitude`'s own `GalileoBlock`).

---

## 6. Precision and accuracy

- **GLNY-P-1.** No hardware rates supplied by the caller (unlike GPS's own `HardwareYawRates`):
  `DIL11`'s own hardware rate (`R=0.25deg/s`, stated as a single average for every satellite and both
  maneuvers, Sec.6.1) is a fixed constant this module carries, not a per-satellite or per-caller
  value — `DIL11`'s own stated precision limit: "a yaw rate error of +/-0.01deg/s can cause a
  yaw-attitude error at the end of a 12-minute duration half-turn of about +/-7deg" (Sec.6.1) — a
  property of the source's own averaging, not this implementation's own approximation.
- **GLNY-P-2.** `glonass_m_noon_onset_rad`'s own four-iteration solve matches `DIL11`'s own stated
  beta=0 result (mu_s=176.8deg) to four decimal places; a convergence sweep (4 vs 20 iterations,
  this session's own record) found sub-arcsecond agreement through most of `(0,2)deg`, degrading to
  ~207 arcsec (~0.06deg) right at beta=1.99deg, next to the beta_0=2.03deg edge — `DIL11`'s own
  stated "adequate precision," not exact, a property of the published method reproduced here.
- **GLNY-P-3.** The real-data control's own four checks matched to 0.0007deg–0.026deg — tighter
  than the registered 3° criterion by two to four orders of magnitude, and tighter than Galileo's
  own real-data control's own residuals (`SPEC-galileo-attitude.md` `GALY-Q-002`, 0.002deg–0.1deg) —
  not investigated further for a specific cause (both controls' own approximations, beta held
  constant and linear SP3 interpolation, are shared; the tighter GLONASS-M residual may simply
  reflect the deeper eclipse-season geometry chosen, beta essentially 0 rather than 0.5–1.1deg).

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `GLNY-F-001` | (forwarded, unchanged) `ATTD-F-001` — the Sun within `nominal_yaw_steering`'s own nadir-singularity tolerance | the diagnostic `nominal_yaw_steering` itself states |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `GLNY-A-001` | `DIL11`'s own Eq. 1 (native, un-negated sin(mu)) and this tree's own `psi_nominal` agree by the additive pi-minus identity, at six geometries; `glonass_m_yaw_attitude`, off both turns, returns EXACTLY what `nominal_yaw_steering` returns | agreement to 1e-9; max component difference < 1e-9 | `DIL11` Eq. 1, read directly in the test | 1e-9 | R-001, R-004 |
| `GLNY-A-002` | the shadow-crossing turn matches an INDEPENDENT reconstruction of `DIL11`'s own Eq. 11–14 (ramp then hold), at four geometries (both signs of beta, ramp and hold regions) | agreement to 1e-6 in each frame component | `DIL11` Sec.5.1, read directly in the test | 1e-6 | R-002 |
| `GLNY-A-003` | the noon turn matches an INDEPENDENT reconstruction of `DIL11`'s own Eq. 15–21 (four-iteration onset, single ramp), at four geometries | agreement to 1e-6 in each frame component | `DIL11` Sec.5.2, read directly in the test | 1e-6 | R-003 |
| `GLNY-A-004` | **the guard shown firing**: the shadow turn's own hold phase is genuinely flat (the intrinsic yaw angle, extracted through each point's own orbit triad, not the raw rotating GCRS frame) between two points spanning most of the hold region's own width; a deliberately-broken "keep tracking nominal" comparison shows a large (>1 rad) swing over the SAME two points, the defect this guard exists to catch | real: < 1e-6 rad difference; broken comparison: > 1 rad | `DIL11`'s own Eq. 13, Fig. 5 | 1e-6 | R-002 |
| `GLNY-A-005` | **time direction** (`TYAW-A-012`'s own shape): the shadow turn's own registered `mu_f` milestone is reached at the registered true elapsed time; shown firing on a reversed-velocity version, which reaches a materially different, out-of-window state | max component difference > 0.1 | `DIL11` Sec.5.1's own window | see test | R-006 |
| `GLNY-A-006` | **rotation sense** (`TYAW-A-015`'s own shape): well inside each turn's own ramp region (both shadow and noon), the built law's own rate has the same sign as `DIL11`'s own stated SIGN[R, psi_dot_n] rule; shown firing on a deliberately sense-flipped version | same sign (real); opposite sign (deliberately broken) | `DIL11`'s own Eq. 12/15, independently transcribed | — | R-006 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `GLNY-F-001` | Forwarded, unchanged, from `nominal_yaw_steering`'s own `ATTD-F-001` — that guard is already gated at its own source; re-testing the forwarding path adds no independent coverage `SPEC-thrust-yaw.md`'s own `TYAW-F-001` and `SPEC-galileo-attitude.md`'s own `GALY-F-002` (the identical shape, for GPS and Galileo) did not already establish is a reasonable pattern for this module to inherit. |
| `GLNY-R-005` | The real-data control is `tools/orbex_glonass_check.cpp`, deliberately NOT part of the automatic gate (the same treatment every other ORBEX control gets) — no pinned data present in CI, reproducible on demand. |

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as part of the L5 step 3 section: the rule-4/licence search for GLONASS-
  M's own attitude source (`DIL11`, freely hosted at the IGS ACC mirror); the reduction proof
  (`DIL11`'s own Eq. 1 is KOUBA09's own Eq. 4 "as printed," converting the same way); the shadow-
  crossing maneuver's own quoted mechanism (ramp then hold, confirmed against `DIL11`'s own real
  Fig. 5 data) and its own genuine distinctness from the noon turn (no hold, a four-iteration onset
  solve instead of a closed-form one); the ramp-sign conversion's own 200000-point numerical proof,
  including the beta=0 degeneracy found and how it was resolved (a test fix, not a production
  change); the real-data control's own day search (2023-10-07 unusable, DOY 266 partial, DOY 252
  found) and its own result.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `GLNY-Q-001` | **Original GLONASS and GLONASS-K's own eclipse-season attitude — NOT FOUND, not built.** No source naming a yaw-attitude law for these two blocks was found this session (`SPEC-spacecraft.md` §2's own rule-4 search covered GLONASS's data AND attitude together and found none for either non-M block). Both reduce to `nominal_yaw_steering` unconditionally in this version — a stated gap, not a silent one. Worth a follow-up search if a consumer specifically needs eclipse-season attitude for either block. |
| `GLNY-Q-002` | **The real-data control's own small residual (0.0007deg–0.026deg) is not investigated further** — tighter than Galileo's own control by roughly an order of magnitude, plausibly because the chosen crossings sit at beta essentially 0 (a sharper, more distinctive test of the mechanism) rather than because GLONASS-M's own law is inherently more precise. Worth a targeted comparison (same beta regime, both constellations) if a consumer needs to understand the gap. |
| `GLNY-Q-003` | **Only TWO satellites, ONE day, checked against real data** (R19, R20, 2023-09-09) — deliberately narrow, the same reasoning `GALY-Q-003` states for Galileo's own control. Worth extending if a specific consumer needs more real-data confidence. |
