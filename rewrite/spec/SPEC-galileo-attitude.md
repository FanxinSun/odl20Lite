# SPEC-galileo-attitude — Galileo's own yaw-steering law

| | |
|---|---|
| **Spec ID** | `GALY` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.1 — FOC's own "modified yaw steering law" built (`GALY-R-003`, closed-form, `GALY-Q-001` resolved); step 6's own two REAL guards (time direction, rotation sense) built for both blocks, replacing an earlier round's own substitutes; a real-data control against CODE's own Galileo attitude (`tools/orbex_galileo_check.cpp`) |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), step 2 (Galileo) — attitude LAW code, `modules/attitude`, beside GPS's own `SPEC-thrust-yaw.md` |
| **Depends on** | `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering` (`SPEC-photon-pressure` `PHPR-R-004`) |
| **Depended on by** | `srp_analytic`/`photon_force` (indirectly, through whichever module supplies a body frame); L7's own box-wing fit |

**Derivation declaration (plan R1).** Written from the source §2 names, and from no implementation
of this module beyond `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering`
and `gps_yaw_attitude`.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or
otherwise inspected during this specification's preparation, per `../plan/PLAN.md` §3.6's own
explicit refusal of the predecessor's hand-built models.

---

## 1. Purpose and scope

**Galileo's own yaw-steering law**, as CODE (not cited data — `SPEC-spacecraft.md` is where
Galileo's own citable macromodel DATA lives; this spec is where its own attitude BEHAVIOUR is
specified, the same split GPS already has between `SPEC-macromodel`/`SPEC-spacecraft` and
`SPEC-thrust-yaw`). Covers both of the European GNSS Service Centre's own published blocks, IOV
and FOC (`GALSC`, `SPEC-spacecraft.md` §2.3's own rule-4/licence search — not repeated here).

**Not in scope.** The macromodel data itself (`SPEC-spacecraft.md`). GPS's own laws
(`SPEC-thrust-yaw.md`) — unchanged, untouched by this spec. Antenna thrust
(`modules/antenna_thrust`) — GPS-specific, no Galileo equivalent built or named by the plan.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `GALSC` | European GNSS Service Centre (GSC); EUSPA/EU | *Galileo Satellite Metadata* | continuously updated; this pin 2026-09-24 | `https://www.gsc-europa.eu/support-to-developers/galileo-satellite-metadata` | **primary**, first-party, open with attribution — full rule-4/licence search in `SPEC-spacecraft.md` §2.3, not repeated | its own §3 ("Attitude Law"): the yaw-steering law's own equations, for IOV (§3.1.1) and FOC (§3.1.2), and again converted to the GPS/ANTEX frame convention (§3.2) |

No second source was sought: `GALSC` prints the law's own equations directly, with worked
parameters, and the manager's own instruction was to implement and check it against `GALSC`'s own
two printed forms, not to find an independent literature confirmation as well.

---

## 3. Definitions and conventions

- **The orbital reference frame `GALSC` states its own law in** (§3.1): +Z toward Earth centre
  (nadir), +Y perpendicular to the orbital plane ("across-track"), +X completing the right-handed
  system, "pointing mainly in the flight direction" (along-track). Expressed in terms of
  `modules/attitude`'s own already-built `OrbitTriad` (`r_hat` radial, `n_hat = r×v` normalized
  orbit normal, `t_hat = n_hat×r_hat` prograde tangential, `SPEC-thrust-yaw.md`'s own basis):
  `Z_orb = -r_hat`, `X_orb = t_hat`, `Y_orb = Z_orb × X_orb = -n_hat` — the unique right-handed
  completion of the other two, `GALSC` naming no separate sign for Y beyond "completes the system."
- **`GALSC`'s own two printed forms of the SAME law REDUCE to one shared formula, proved not
  assumed (`GALY-A-004`).** IOV's own primary equation (§3.1.1), `ψᵣ = atan2(-Sᵧ/√(1-S_z²),
  -Sₓ/√(1-S_z²))`, and FOC's own (§3.1.2), `ψ(t) = atan2(s·n, s·(r×n))` with `n` the orbit
  normal — the `√(1-S_z²)` denominator is a positive scalar for any physical `S` (`|S_z|<1`) and
  cancels inside `atan2`; and `s·n̂ = -Sᵧ`, `s·(r̂×n̂) = s·(-t̂) = -Sₓ` (since `t̂ = n̂×r̂` here,
  `r̂×n̂ = -(n̂×r̂) = -t̂`) — giving `atan2(-Sᵧ, -Sₓ)` for BOTH, algebraically identical. One
  function, `galileo_psi_nominal`, serves both blocks' own nominal (pre-adjustment) law.
- **This shared nominal law is, itself, exactly what `nominal_yaw_steering` already builds** (a
  nadir-pointing, Sun-tracking-panel frame, `PHPR-R-004`) — GALILEO'S own case of the SAME general
  law GPS's own ideal attitude already is, in a different axis-labelling convention. So this
  module's own new work is NOT a frame-construction formula (that already exists, trusted, gated) —
  it is exactly the two blocks' own DEVIATIONS from it: IOV's own near-singularity Sun-vector
  substitution (§3.1.1's own auxiliary region) and FOC's own near-colinearity switch-over (§3.1.2's
  own named region, where this version refuses rather than building the modified law).
- **§3.2's own "ANTEX Reference Frame Convention" form is `GALSC`'s own SECOND printed statement of
  the SAME angle, offset by π** — its own words: "change the sign... in order to meet the
  standard," negating BOTH `atan2` arguments, which shifts the angle by exactly π. `GALY-A-004`
  checks this relationship holds for an independent transcription of both forms, at a spread of
  geometries — the manager's own instruction that two printed forms are an independent check GPS's
  own single-form laws never had.
- **`galileo_yaw_attitude` returns `M_gcrs_to_body` in THIS TREE's OWN convention** (matching
  `nominal_yaw_steering`/`gps_yaw_attitude`), not `GALSC`'s own native frame —
  `odl::spacecraft::galileo_frame_from_mechanical` (`SPEC-spacecraft.md` `SPCR-R-008`) is the
  UNRELATED, data-side mapping for the macromodel's own face normals; this function's own frame
  convention is verified independently (`GALY-A-005`), not by appeal to that other mapping.
- **IOV's own auxiliary-region Γ (the sign of `S_oy` "at the beginning of the auxiliary region")
  is read at the CURRENT query instant, stateless** — β moves on the orbital-precession timescale,
  far slower than the η-driven transit through this narrow region, so its sign does not change
  within one transit except exactly at β = 0. The SAME stateless tie-break shape
  `gps_yaw_attitude`'s own `sign(beta)` already uses (`SPEC-thrust-yaw.md` `TYAW-Q-002`'s own ruled
  convention), not a new one invented here.
- **`galileo_frame_from_psi`: this tree's own "frame from yaw angle" for Galileo, DERIVED not
  guessed (`GALY-A-009` proves it).** Substituting `S_X = -√(1-S_z²)·cos(ψ)`, `S_Y =
  -√(1-S_z²)·sin(ψ)` (ψ's own definition, §3 above) into `nominal_yaw_steering`'s own x_body
  construction, expanded in `(t̂, n̂, r̂)`, gives `x_body = -cos(ψ)·t̂ + sin(ψ)·n̂` exactly — the
  `√(1-S_z²)` factor cancels. The `+sin(ψ)` term's own sign is OPPOSITE `gps_yaw_attitude`'s own
  `frame_from_yaw` (`-sin(ψ_KOUBA09)`): a DIFFERENT ψ convention (this file's own
  `atan2(-S_Y,-S_X)`, not KOUBA09's Eq. 4/5), not a transcription slip.
- **GSC's own colinearity angle ε depends on μ ALONE, independent of β — PROVED (`GALY-A-008`), the
  fact that makes FOC's own "modified yaw steering law" computable statelessly at all.** Expanding
  ε's own defining construction (`x = n̂×s`, `y = n̂×x`, `ε = fold(arccos(r̂·ŷ))`) with `S_X =
  cos(β)·sin(μ)`, `S_Z = cos(β)·cos(μ)` gives `cos(raw_ε) = S_Z/cos(β) = cos(μ)` exactly — the
  `cos(β)` factor cancels, so `raw_ε = |μ|` and `ε = fold(|μ|)`, `fold(x) = x` for `x ≤ 90°` else
  `180° − x`. **This is what makes the switch-over window's own entry μ a FIXED closed-form
  constant** (`±10°` near midnight, `170°`/`190°` near noon), never a remembered crossing — the
  manager's own instruction, "compute that moment from the geometry, as the IIF shadow turn
  computes its shadow entry and exit."
- **`t_mod` (elapsed time since the window's own entry) uses the CURRENT `(r, v)`'s own
  instantaneous osculating angular rate, `|r×v|/|r|²`, not a fixed constant.** GPS's own
  `kMuDotRadPerS` is a stated AVERAGE for GPS's own orbit specifically (`KOUBA09` Eq. 6); Galileo's
  own orbit is a different altitude and period entirely, so this module computes the rate fresh
  from the caller's own state each call — exact for a circular orbit (`|r×v|/r² = dθ/dt`
  identically when `r⊥v`), and the SAME quantity a real orbit's own instantaneous angular rate is
  in general, keeping the interface stateless without needing a Galileo-specific constant at all.

---

## 4. Required behaviour

- **GALY-R-001.** `galileo_psi_nominal` (internal): GSC's own shared nominal yaw angle, §3 above —
  proved (not assumed) identical for IOV's and FOC's own primary equations, `GALY-A-004`.
- **GALY-R-002.** `GalileoBlock::IOV`: near GSC's own named auxiliary region (§3.1.1: `|x_sun| <
  sin(15°)` AND `|y_sun| < sin(2°)`), substitutes GSC's own smooth auxiliary Sun reference vector
  for the real Sun direction before delegating to `nominal_yaw_steering` — continuous at the
  region's own boundary (`GALY-A-006`).
- **GALY-R-003.** `GalileoBlock::FOC`: GSC's own "modified yaw steering law" (§3.1.2) INSIDE its
  own named near-colinearity switch-over region (`|β| < 4.1°` AND colinearity `ε < 10°`, GSC's own
  two-gate condition, §3's own `ε = fold(|μ|)` proof); GSC's own primary formula, via
  `galileo_frame_from_psi`, OUTSIDE it. **Ruled 2026-09-24** (manager, after reviewing an earlier
  version's own refusal there, `GALY-F-001`, now retired): the window is Galileo's own noon/
  midnight turn, not a rare corner, and GSC prints the law — built via the closed-form window entry
  §3 derives, matching `evaluate_shadow_crossing`'s own shape for GPS's IIF shadow. `psi_init =
  galileo_psi_nominal_beta_mu(β, μ_s)` (the window's own fixed entry `μ_s`, current β); `t_mod =
  (μ_current − μ_s) / (|r×v|/|r|²)`; `ψ_mod(t_mod) = 90°·sign(ψ_init) + (ψ_init −
  90°·sign(ψ_init))·cos(2π/5656s · t_mod)`, GSC's own printed formula, transcribed exactly.
  Verified against an independent transcription of GSC's own formula, `μ_s`, `ψ_init` and `t_mod`
  each computed fresh (`GALY-A-010`) and against CODE's own real Galileo attitude for a real FOC
  satellite through two real crossings (`GALY-R-006`).
- **GALY-R-004.** Outside both named regions, `galileo_yaw_attitude` returns EXACTLY
  `nominal_yaw_steering(r_gcrs_m, sun_direction_gcrs)` — the delegation this module's own design
  rests on, asserted as a standing regression guard (`GALY-A-005`), not merely true by construction
  and left unchecked.
- **GALY-R-005.** `galileo_yaw_attitude`'s own refusal (`ATTD-F-001`, forwarded unchanged as
  `GALY-F-002`) is reached wherever `nominal_yaw_steering`'s own nadir-singularity guard would fire
  — the same forwarding shape `SPEC-thrust-yaw.md`'s own `TYAW-F-001` already uses for GPS.
- **GALY-R-006.** **Real-data control** (`tools/orbex_galileo_check.cpp`, reproducible on demand,
  NOT part of the automatic gate — plan §4's own "gated and reproducible are different properties",
  the same treatment GPS's own `orbex_noon_check.cpp`/`orbex_shape_e_check.cpp` get): CODE's own
  MGEX Galileo attitude (2023-10-07, DOY 280), one IOV satellite (E11) and one FOC satellite (E33),
  each at a real noon AND a real midnight crossing (four checks total), all at low β (0.5°–1.1°).
  Predictions and a 2° relative criterion REGISTERED before the attitude quaternions were read
  (the tool's own header comment states the registration in full). Result: all four matched, IOV's
  own two crossings to a few thousandths of a degree, FOC's own two (the newly-derived modified
  law) to about a tenth of a degree — the first real-data confirmation either law has had.
- **GALY-R-007.** Step 6's own two guards, applied to BOTH `GalileoBlock`s (ruled 2026-09-24, the
  manager's own correction: an earlier round's own "continuity" and "bounded rate" checks are real
  properties but are NOT these two, and neither sees the defect classes below):
  - **Time direction** (`TYAW-A-012`'s own shape, `GALY-A-011`): a REGISTERED geometric milestone
    (the window's own physical centre) is reached at the true elapsed time the geometry predicts,
    for a REAL propagated trajectory (Kepler's own rate at Galileo's own radius) — shown firing on
    a reversed-velocity version, which reaches a DIFFERENT state at the same registered time. For
    IOV specifically, reversing velocity is a PROVED exact symmetry of the substitution (§3's own
    algebra: the sign flips in `Γ` and in `(S_X, S_Y)` cancel exactly, `nominal_yaw_steering`
    itself never reading `v` at all) — a real, reported finding, not a defect forced to look
    different — so IOV's own guard instead exercises `Γ`'s own stateless approximation directly.
  - **Rotation sense** (`TYAW-A-015`'s own shape, `GALY-A-012`): through the window's own entry
    (well inside it, not exactly at `t_mod = 0`, where the cosine ramp's own rate is exactly zero
    by construction — an earlier version of this test evaluated there and found nothing to
    compare), the smoothed/modified law's own rate has the SAME SIGN as the nominal law's own rate
    at the SAME point — shown firing on a deliberately sense-flipped version (`sign_init` negated).

---

## 5. Interfaces, stated language-free

- `GalileoBlock { IOV, FOC }`
- `galileo_yaw_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3, block: GalileoBlock) -> Result<Mat3, AttitudeError>` — FOC now BUILDS inside its own named region (`GALY-R-003`), not refuses.
- `galileo_native_yaw_angle_pre_substitution(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3) -> double` — exposed for testing only; `galileo_yaw_attitude` itself never reads this scalar, only the frame it implies.

A stateless provider, the same shape `gps_yaw_attitude`'s own header states (`SPEC-thrust-yaw.md`
`TYAW-R-007`): every quantity is computable from the CURRENT epoch's own `(r_gcrs_m,
v_gcrs_m_per_s, sun_direction_gcrs)` alone, no history of a prior call required — including IOV's
own Γ (§3 above, a stateless approximation of a stateful definition, not a remembered value).

---

## 6. Precision and accuracy

- **GALY-P-1.** No hardware rates, no caller-supplied constants (unlike GPS's own
  `HardwareYawRates`): `GALSC`'s own law is fully closed-form and geometric, needing nothing beyond
  the current state.
- **GALY-P-2.** `GALY-A-004`'s own numeric agreement between the two independently-transcribed
  printed forms is checked to 1e-9 rad — limited by floating-point round-off through several
  `atan2`/trigonometric calls, not by any approximation in either formula itself.

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `GALY-F-001` | **retired, does not fire in this version** — was `galileo_yaw_attitude(..., GalileoBlock::FOC)` called inside GSC's own named near-colinearity switch-over region; withdrawn 2026-09-24 when `GALY-R-003` was ruled and built. Kept documented, not deleted, for traceability. | — |
| `GALY-F-002` | (forwarded, unchanged) `ATTD-F-001` — the Sun within `nominal_yaw_steering`'s own nadir-singularity tolerance | the diagnostic `nominal_yaw_steering` itself states |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `GALY-A-004` | GSC's own two printed forms (native §3.1, ANTEX-converted §3.2), independently transcribed, agree with each other by exactly π, at six geometries spanning β and μ; this module's own internal formula matches an independent transcription of the native form | `ψ_antex - ψ_native ≡ π` (mod 2π); production = independent, to 1e-9 | `GALSC` §3.1/§3.2, read directly in the test | 1e-9 | R-001 |
| `GALY-A-005` | outside both named regions, `galileo_yaw_attitude` (IOV and FOC alike) returns EXACTLY what `nominal_yaw_steering` returns for the same `(r, sun)`; the returned frame is orthonormal and right-handed | max component difference < 1e-12; orthonormal | `nominal_yaw_steering`, already gated | 1e-12 | R-004 |
| `GALY-A-006` | IOV's own auxiliary substitution is continuous at the named region's own boundary: stepping 0.002° across it changes the returned frame by no more than 1e-4 in any component | max component difference < 1e-4 | `GALSC` §3.1.1's own region definition | 1e-4 | R-002 |
| `GALY-A-007` | **the guard shown firing**: along a real propagated trajectory through β = 0.001° at noon, the SUBSTITUTED law's own frame-to-frame step stays small (< 0.05) while a deliberately broken version that skips the substitution (plain `nominal_yaw_steering` on the unmodified Sun direction) produces a step exceeding 0.5 — the exact defect the substitution exists to prevent, measured, not merely asserted to differ. Threshold found by tightening β until the broken version failed clearly, not chosen to pass a pre-picked bound | substituted step small; broken step large | `GALSC` §3.1.1's own auxiliary vector | see test | R-002 |
| `GALY-A-008` | GSC's own colinearity ε depends on μ alone, PROVED: an independent vector-based transcription of ε's own construction matches `fold(\|μ\|)` at four β (fixed μ) and four μ (fixed β) | agreement to 1e-6° | `GALSC` §3.1.2, read directly in the test | 1e-6° | R-003 |
| `GALY-A-009` | `galileo_frame_from_psi`, fed the unmodified nominal ψ, reproduces `nominal_yaw_steering`'s own output exactly | max component difference < 1e-9 | this file's own §3 derivation | 1e-9 | R-002, R-003 |
| `GALY-A-010` | FOC's own modified yaw steering, built, matches an INDEPENDENT transcription of GSC's own printed formula — the window's own entry μ, ψ_init and t_mod each computed fresh, not read back from the code under test, at four geometries (both windows, both sides of centre) | agreement to 1e-6 in each frame component | `GALSC` §3.1.2, read directly in the test | 1e-6 | R-003 |
| `GALY-A-011` | **time direction** (`TYAW-A-012`'s own shape): FOC's own registered geometric milestone is reached at the registered true elapsed time; shown firing on a reversed-velocity version, which reaches a materially different state. IOV: reversed velocity is PROVED an exact symmetry (not a defect); the substitution is shown genuinely active (differs from plain nominal) at the probed point instead | FOC: max component difference > 0.1 between forward and reversed; IOV: max component difference > 1e-6 from plain nominal | `GALSC` §3.1.2's own window; this file's own IOV symmetry proof | see test | R-002, R-003, R-007 |
| `GALY-A-012` | **rotation sense** (`TYAW-A-015`'s own shape): well inside each window (not at the zero-rate entry point), the smoothed/modified law's own rate has the same sign as the nominal law's own rate at the same point, for FOC and IOV alike; shown firing on a deliberately sense-flipped version of the modified law | same sign (real); opposite sign (deliberately broken) | this file's own §3 derivation, independently transcribed | — | R-002, R-003, R-007 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `GALY-F-001` | Retired, §7: withdrawn 2026-09-24 when `GALY-R-003` was ruled and built. No code path returns it any more, so no test can discharge it; kept documented, not deleted, for traceability against the earlier version that did fire it. |
| `GALY-F-002` | Forwarded, unchanged, from `nominal_yaw_steering`'s own `ATTD-F-001` — that guard is already gated at its own source (`PHPR-A-003`, `SPEC-photon-pressure.md`); re-testing the forwarding path adds no independent coverage `SPEC-thrust-yaw.md`'s own `TYAW-F-001` (the identical shape, for GPS) did not already establish is a reasonable pattern for this module to inherit. |
| `GALY-R-005` | Same reasoning as `GALY-F-002` immediately above — this requirement states the forwarding, `GALY-F-002`'s own row explains why no dedicated test discharges it. |
| `GALY-R-006` | The real-data control is `tools/orbex_galileo_check.cpp`, deliberately NOT part of the automatic gate (its own header comment: plan §4's own "gated and reproducible are different properties") — no pinned data present in CI, reproducible on demand, the same treatment GPS's own ORBEX controls get. |

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as part of the L5 step 2 section: the reduction proof (both of GSC's
  own printed forms are the same function as `nominal_yaw_steering` already implements), the IOV
  auxiliary-substitution implementation and its own stateless Γ approximation, the GALY-A-007
  test's own first (wrong) proxy-based attempt and the fix — an angle extracted from raw GCRS
  frame components is dominated by orbital rotation, not yaw, caught by the test's own numbers
  contradicting each other before being trusted; the ε-depends-on-μ-alone proof and its own role in
  making FOC's own modified law's window-entry computable from geometry; the FOC modified law's own
  derivation and build, ruled after an earlier round's own refusal; the two corrected step-6 guards
  and the reasoning for why an earlier round's own "continuity"/"bounded rate" pair were real but
  insufficient; the reversed-velocity-is-a-symmetry finding for IOV, reported rather than forced
  into a test that would have asserted something false; the real-data control's own registration,
  method and result.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `GALY-Q-001` | **FOC's own "modified yaw steering law" — RESOLVED** (manager, 2026-09-24). Ruled and built: the window's own noon/midnight regime is Galileo's own real turn, not a corner case, and its own entry is computable from geometry alone (§3's own ε-depends-on-μ-alone proof), so the earlier refusal (`GALY-F-001`) is withdrawn. Verified against an independent transcription (`GALY-A-010`) and CODE's own real attitude (`GALY-R-006`). |
| `GALY-Q-002` | **The real-data control's own small residual (FOC: ~0.06–0.10°; IOV: ~0.002°) is not explained further.** Plausible sources: β held at its own single-epoch value rather than truly time-varying across the ~5-10 minute window; the satellite's own state linearly interpolated between 5-minute SP3 points rather than a true propagated ephemeris; CODE's own real attitude solution carrying its own estimation noise. Not investigated further this round — both are well inside the registered 2° criterion, and IOV's own near-exact match suggests the residual is FOC-specific (the newly-built modified law, or its own β-held-constant approximation across a slightly wider effective window) rather than a shared systematic error. Worth a tighter investigation if L7 needs sub-0.1° Galileo attitude specifically. |
| `GALY-Q-003` | **Only ONE IOV and ONE FOC satellite, ONE day, checked against real data.** A broader real-data sweep (more satellites, more crossings, more seasons) was not performed — `GALY-R-006`'s own scope is deliberately narrow (plan §4's own reasoning for why a real-data control is reproducible, not gated, and does not need to be exhaustive to be useful). Worth extending if a specific consumer needs more real-data confidence than four crossings give. |
