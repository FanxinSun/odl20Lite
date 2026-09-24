# SPEC-galileo-attitude — Galileo's own yaw-steering law

| | |
|---|---|
| **Spec ID** | `GALY` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.0 |
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
(`SPEC-thrust-yaw.md`) — unchanged, untouched by this spec. FOC's own "modified yaw steering law"
near colinearity (`GALY-Q-001`) — this version refuses there rather than building it. Antenna
thrust (`modules/antenna_thrust`) — GPS-specific, no Galileo equivalent built or named by the plan.

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

---

## 4. Required behaviour

- **GALY-R-001.** `galileo_psi_nominal` (internal): GSC's own shared nominal yaw angle, §3 above —
  proved (not assumed) identical for IOV's and FOC's own primary equations, `GALY-A-004`.
- **GALY-R-002.** `GalileoBlock::IOV`: near GSC's own named auxiliary region (§3.1.1: `|x_sun| <
  sin(15°)` AND `|y_sun| < sin(2°)`), substitutes GSC's own smooth auxiliary Sun reference vector
  for the real Sun direction before delegating to `nominal_yaw_steering` — continuous at the
  region's own boundary (`GALY-A-006`), and keeps the yaw rate bounded along a real propagated
  trajectory through β near zero at noon/midnight, shown against a deliberately broken (
  unsubstituted) version that does not (`GALY-A-007`).
- **GALY-R-003.** `GalileoBlock::FOC`: GSC's own primary formula (§3.1.2), via
  `nominal_yaw_steering`, OUTSIDE its own named near-colinearity switch-over region (|β| < 4.1°
  AND colinearity angle ε < 10°, GSC's own two-gate condition); REFUSES (`GALY-F-001`) inside it,
  shown firing exactly there and not just outside either gate (`GALY-A-008`) — GSC's own "modified
  yaw steering law" for that region is NOT built this version (`GALY-Q-001`).
- **GALY-R-004.** Outside both named regions, `galileo_yaw_attitude` returns EXACTLY
  `nominal_yaw_steering(r_gcrs_m, sun_direction_gcrs)` — the delegation this module's own design
  rests on, asserted as a standing regression guard (`GALY-A-005`), not merely true by construction
  and left unchecked.
- **GALY-R-005.** `galileo_yaw_attitude`'s own refusal (`ATTD-F-001`, forwarded unchanged as
  `GALY-F-002`) is reached wherever `nominal_yaw_steering`'s own nadir-singularity guard would fire
  — the same forwarding shape `SPEC-thrust-yaw.md`'s own `TYAW-F-001` already uses for GPS.

---

## 5. Interfaces, stated language-free

- `GalileoBlock { IOV, FOC }`
- `galileo_yaw_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3, block: GalileoBlock) -> Result<Mat3, AttitudeError>`
- `galileo_native_yaw_angle_pre_substitution(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3) -> double` — exposed for `GALY-A-004`/`GALY-A-005`/`GALY-A-007`'s own verification only; `galileo_yaw_attitude` itself never reads this scalar, only the frame it implies.

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
| `GALY-F-001` | `galileo_yaw_attitude(..., GalileoBlock::FOC)` called inside GSC's own named near-colinearity switch-over region (\|β\| < 4.1° AND colinearity ε < 10°) | the gate condition, and that GSC's own "modified yaw steering law" is not built this version |
| `GALY-F-002` | (forwarded, unchanged) `ATTD-F-001` — the Sun within `nominal_yaw_steering`'s own nadir-singularity tolerance | the diagnostic `nominal_yaw_steering` itself states |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `GALY-A-004` | GSC's own two printed forms (native §3.1, ANTEX-converted §3.2), independently transcribed, agree with each other by exactly π, at six geometries spanning β and μ; this module's own internal formula matches an independent transcription of the native form | `ψ_antex - ψ_native ≡ π` (mod 2π); production = independent, to 1e-9 | `GALSC` §3.1/§3.2, read directly in the test | 1e-9 | R-001 |
| `GALY-A-005` | outside both named regions, `galileo_yaw_attitude` (IOV and FOC alike) returns EXACTLY what `nominal_yaw_steering` returns for the same `(r, sun)`; the returned frame is orthonormal and right-handed | max component difference < 1e-12; orthonormal | `nominal_yaw_steering`, already gated | 1e-12 | R-004 |
| `GALY-A-006` | IOV's own auxiliary substitution is continuous at the named region's own boundary: stepping 0.002° across it changes the returned frame by no more than 1e-4 in any component | max component difference < 1e-4 | `GALSC` §3.1.1's own region definition | 1e-4 | R-002 |
| `GALY-A-007` | **the guard shown firing**: along a real propagated trajectory through β = 0.001° at noon, the SUBSTITUTED law's own frame-to-frame step stays small (< 0.05) while a deliberately broken version that skips the substitution (plain `nominal_yaw_steering` on the unmodified Sun direction) produces a step exceeding 0.5 — the exact defect the substitution exists to prevent, measured, not merely asserted to differ. Threshold found by tightening β until the broken version failed clearly, not chosen to pass a pre-picked bound | substituted step small; broken step large | `GALSC` §3.1.1's own auxiliary vector | see test | R-002 |
| `GALY-A-008` | FOC's own colinearity refusal (`GALY-F-001`) fires exactly inside GSC's own named region (both gates active) and does NOT fire just outside either gate alone (β = 10° with the same μ; μ = 90° with the same small β); IOV, the SAME deep-inside geometry, does not refuse (it substitutes instead) | refusal inside; success on both single-gate-relaxed cases; IOV succeeds | `SPEC-galileo-attitude.md` GALY-R-003's own domain | — | F-001, R-003 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `GALY-F-002` | Forwarded, unchanged, from `nominal_yaw_steering`'s own `ATTD-F-001` — that guard is already gated at its own source (`PHPR-A-003`, `SPEC-photon-pressure.md`); re-testing the forwarding path adds no independent coverage `SPEC-thrust-yaw.md`'s own `TYAW-F-001` (the identical shape, for GPS) did not already establish is a reasonable pattern for this module to inherit. |
| `GALY-R-005` | Same reasoning as `GALY-F-002` immediately above — this requirement states the forwarding, `GALY-F-002`'s own row explains why no dedicated test discharges it. |

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as part of the L5 step 2 section: the reduction proof (both of GSC's
  own printed forms are the same function as `nominal_yaw_steering` already implements), the IOV
  auxiliary-substitution implementation and its own stateless Γ approximation, the FOC
  near-colinearity refusal and the reasoning for not building the modified law, and the GALY-A-007
  test's own first (wrong) proxy-based attempt and the fix — an angle extracted from raw GCRS
  frame components is dominated by orbital rotation, not yaw, caught by the test's own numbers
  contradicting each other before being trusted.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `GALY-Q-001` | **FOC's own "modified yaw steering law" (§3.1.2's own near-colinearity ramp) is not built.** `galileo_yaw_attitude` refuses there instead (`GALY-F-001`). The condition is a narrow geometric window (β < 4.1°, colinearity ε < 10°); `SPCR-Q-005` (`SPEC-spacecraft.md`) asks the same question from the data side. Worth building before any consumer needs FOC attitude that close to colinearity, or acceptable to refuse there for now? |
