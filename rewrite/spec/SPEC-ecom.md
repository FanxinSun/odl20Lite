# SPEC-ecom — the empirical CODE orbit model (ECOM2)

| | |
|---|---|
| **Spec ID** | `ECOM` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-24 |
| **Layer** | L4 `forces-analytic`, step 7 (`../plan/PLAN.md` §3.5) |
| **Depends on** | `core`, `dynamics` (the `Force`/`ParameterRegistry` surface) |
| **Depended on by** | nothing yet; `srp_analytic` may serve as this force's own *a priori* term (Eq. 2's *a*₀) by registering both in the same `ForceSet` — this force needs no direct dependency on it to do that |

**Derivation declaration (plan R1).** Written from `ARN15` (§2) and from no implementation of
this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

**This spec's own gate is not the one `../plan/subplan_L4/L4-7.md` first stated.** "The
published parameterisation reproduced on a GNSS arc" means fitting ECOM's own parameters to
observations — L7's estimator, on L6's reader, neither of which exists yet — so it was not a
gate here (`../plan/PLAN.md` §3.5's own principle, and its own 2026-09-24 correction, found
while this spec was being scoped). The gate actually used is stated at each requirement below
and summarized in §8: properties the source's own formulas imply, at parameters stated in the
test, never an arc fit.

---

## 1. Purpose and scope

The Empirical CODE Orbit Model, second generation (**ECOM2**, `ARN15`): a solar-radiation-pressure
acceleration built from three empirical, Fourier-parametrized functions of one angle, resolved
along a Sun-oriented orthonormal frame (D/Y/B) rather than along the spacecraft body. Unlike
`SPEC-srp-analytic`'s own box-wing law, ECOM2 needs no macromodel, no optical coefficients and no
attitude model beyond the Sun-tracking geometry the D/Y/B frame itself encodes — it is the model
CODE's own IGS contributions have used operationally since 2015, standing beside the analytic
model, not replacing it. This step builds ECOM2 as `ARN15` states it, for general truncation
orders (*n*_D, *n*_B), with the nine-parameter **D4B1** solution CODE adopted as this module's own
default configuration.

**Not in scope.** Estimating ECOM's own parameters from real observations (L7). The original
9-parameter ECOM (`ARN15` Eq. 3) and the reduced 5-parameter ECOM (Eq. 4) as *standalone* laws —
Eq. 4 is used here only as the reduction target ECOM2 must collapse to at *n*_D = 0, *n*_B = 1
(`ECOM-A-005`), not as a second implementation. Box-wing SRP (`SPEC-srp-analytic`). Attitude
during eclipse or turns — `ARN15` itself assumes perfect yaw-steering throughout (§4, explicit)
and this spec inherits that assumption unchanged.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `ARN15` | D. Arnold, M. Meindl, G. Beutler, R. Dach, S. Schaer, S. Lutz, L. Prange, K. Sośnica, L. Mervart, A. Jäggi | *CODE's new solar radiation pressure model for GNSS orbit determination*, J. Geodesy 89(8): 775–791 | 2015 | DOI `10.1007/s00190-015-0814-4` | **primary** — authors' accepted manuscript (green OA, BORIS deposit 69654, fetched via CORE.ac.uk after the publisher, BORIS itself, ResearchGate and ADS all refused automated access); this is the pre-typeset manuscript, so page numbers may differ from the version of record — equation numbers cited here are the manuscript's own and are stable (`PROVENANCE.md` §30.21 has the full search record) | normative, in full |
| `SPEC-frames` | this tree | §4.7, the DYB frame | v1.3 | — | primary (already confirmed against `ARN15` Eq. 1, `FRAME-Q-002` CLOSED) | the frame this spec's own `ECOM-R-001` restates for this module's own consumption |
| `SPEC-dynamics` | this tree | `DYN-Q-002`, the velocity-dependence discipline | current | — | primary | governs `ECOM-R-006` |

**No worked numeric case.** `ARN15` prints parameter *counts* (Table 4) and *validation
statistics* — SLR residual means/RMS by candidate model (Table 9), geocenter-coordinate spectral
amplitudes (Table 5) — but no single (Δ*u*, *D*, *Y*, *B*) triple a test could reproduce without
either fitting parameters (not built) or transcribing Eq. 5 a second time under a different name
(not evidence, `../plan/subplan_L4/L4-7.md`'s own finding). The absence is recorded, not treated
as silence to fill in.

---

## 3. Definitions and conventions

- **The D/Y/B frame**, `ARN15` Eq. 1, restated from `SPEC-frames` §4.7 (already confirmed
  identical, `FRAME-Q-002`):
  ê_D = (r̂_Sun − r̂_sat) / |r̂_Sun − r̂_sat|, ê_Y = −(ê_r × ê_D)/|ê_r × ê_D|, ê_B = ê_D × ê_Y,
  where ê_r = r/|r| is the satellite's own geocentric radial direction. ê_D points
  **spacecraft → Sun** (`FRAME-R-050`); ê_Y is the solar-panel rotation axis under nominal
  yaw-steering (`FRAME-R-051`).
- **μ, this tree's own orbit angle from midnight** (`SPEC-thrust-yaw` §3, `PHPR-A-011`'s RTN):
  not reused by name or by calling into `attitude`'s own private functions — this module is
  independent — but the SAME (r̂, n̂, t̂) triad (n̂ = normalize(r×v), t̂ = n̂×r̂) underlies both, and
  **Δu below is μ + π (mod 2π)**, checked numerically, not assumed (§4, `ECOM-R-002`'s own
  derivation).
- **Δu, ECOM's own angular argument**, `ARN15`: *"the angular argument Δu ≐ u − u_s ... where u_s
  is the Sun's argument of latitude in the satellite's orbital plane"* and *u* is the satellite's
  own argument of latitude — both measured from whatever reference the orbital plane's own line
  of nodes provides, a reference this spec never needs explicitly (§4). Δu = 0 at the satellite's
  own **noon** point (co-located, in-plane, with the Sun's own projection); this is the SAME
  physical angle as this tree's own μ, offset by a constant π, not a new convention independently
  chosen.
- **The empirical functions** *D*(Δu), *Y*(Δu), *B*(Δu) — Fourier series in Δu, `ARN15` Eq. 5,
  §4 below.

---

## 4. Required behaviour

- **`ECOM-R-001`.** The D/Y/B frame is built exactly as `ARN15` Eq. 1 and `SPEC-frames` §4.7
  state it (§3 above). Refuses (`ECOM-F-001`) where ê_D ∥ ê_r — the spacecraft on the Earth–Sun
  line — the same condition `FRAME-R-052` already names, forwarded rather than re-derived
  (`DRAG-F-003`'s own precedent for a shared cause reached through more than one caller).

- **`ECOM-R-002`.** Δu is computed as a single angle, not as *u* and *u*_s separately reduced —
  no external reference (an ascending node, an epoch's own vernal equinox) is needed, because
  Δu is the in-plane angle from the Sun's own projection to the satellite:

  ```
  s_orb = ŝ − (ŝ·n̂)n̂            Sun's own projection onto the orbital plane
  Δu = ATAN2(−(ŝ_orb·t̂), ŝ_orb·r̂)
  ```

  where ŝ_orb = s_orb/|s_orb|. **Checked against Δu = μ + π** (this tree's own thrust-yaw μ, an
  independent construction) to 1.1×10⁻¹⁵ over 2000 random geometries, and against the direct
  case Δu = 0 exactly at r̂ = ŝ_orb (own script, `PROVENANCE.md` §30.21 has the record — not
  assumed from the algebra alone, the same discipline `mu_rad`'s own worked example used).
  **Independent of the coordinate system**, `ARN15`'s own stated property (§4.2: *"the angular
  argument Δu of the new ECOM is independent of the coordinate system used"*): the formula above
  is built entirely from dot and cross products of the physical vectors r, v, ŝ, so it is
  invariant under any common rotation of the frame those vectors are expressed in — checked
  directly (`ECOM-A-002`), not asserted from the source's own words alone.
  **Refuses** (`ECOM-F-002`) where ŝ ∥ n̂ — the Sun exactly on the orbit normal, β = ±90° exactly
  — the projection s_orb is undefined there, not merely small (mirroring `mu_rad`'s own
  degeneracy, `SPEC-thrust-yaw` §4.6, at a threshold this module states independently since it
  does not call into that module).

- **`ECOM-R-003`.** The general extended ECOM (`ARN15` Eq. 5), for caller-supplied truncation
  orders *n*_D ≥ 0, *n*_B ≥ 0:

  ```
  D(Δu) = D₀ + Σ_{i=1}^{n_D} [D_{2i,c}·cos(2i·Δu) + D_{2i,s}·sin(2i·Δu)]
  Y(Δu) = Y₀
  B(Δu) = B₀ + Σ_{i=1}^{n_B} [B_{2i-1,c}·cos((2i-1)·Δu) + B_{2i-1,s}·sin((2i-1)·Δu)]
  a = D(Δu)·ê_D + Y(Δu)·ê_Y + B(Δu)·ê_B
  ```

  *D* carries only even harmonics of Δu, *B* only odd — `ARN15`'s own theoretical derivation
  (§4.1, a terminator-frame symmetry argument for a yaw-steering cuboid) and its own empirical
  assessment (§5) agree on this structure; this spec takes the printed FORM as normative and
  states the symmetries it implies as tests (`ECOM-A-003`/`004`), not the theoretical derivation
  itself, which this tree does not need to re-derive to implement Eq. 5 correctly.
  No *a*₀ term (`ARN15`'s own selectable a priori model): CODE's own practice since July 2013 is
  *a*₀ = 0, and this force declares none — a caller wanting one registers `srp_analytic` (or any
  other force) alongside this one in the same `ForceSet`, which sums their outputs; ECOM2 needs
  no parameter or dependency for this.

- **`ECOM-R-004`.** **D4B1**, *n*_D = 2, *n*_B = 1 (nine parameters: *D*₀, *D*₂c, *D*₂s, *D*₄c,
  *D*₄s, *Y*₀, *B*₀, *B*₁c, *B*₁s) is this module's own default configuration, matching what CODE
  has run operationally since 2015-01-04 (`ARN15` §7). *n*_D and *n*_B remain caller-supplied,
  not hard-coded, so a consumer needing D2B1, D2B0 or the reduction case (`ECOM-R-005` below)
  can ask for them without a second implementation.

- **`ECOM-R-005`.** **The reduction**: at *n*_D = 0, *n*_B = 1, Eq. 5 above must equal the
  reduced ECOM (`ARN15` Eq. 4), *B*(*u*) = *B*₀ + *B*_c·cos(*u*) + *B*_s·sin(*u*), written in
  *u*, not Δu — once Eq. 4's own once-per-revolution coefficients are rotated by *u*_s:

  ```
  B_c = B_{1,c}·cos(u_s) − B_{1,s}·sin(u_s)
  B_s = B_{1,c}·sin(u_s) + B_{1,s}·cos(u_s)
  ```

  Derived, not assumed: substituting *u* = Δu + *u*_s into *B*_c·cos(*u*) + *B*_s·sin(*u*) and
  matching coefficients of cos(Δu)/sin(Δu) against *B*₁c·cos(Δu) + *B*₁s·sin(Δu) gives the
  INVERSE rotation (by −*u*_s); inverting that (rotation matrices are orthogonal — the inverse is
  the transpose) gives exactly the relation above, checked symbolically before being trusted
  (`PROVENANCE.md` §30.21). This is a genuine cross-check, not Eq. 5 transcribed twice: it tests
  the implementation's own Δu — its sign AND its origin — against a second, independently
  printed formula (Eq. 4) that does not itself mention Δu at all. `ECOM-A-005` computes *u* and
  *u*_s itself, not by calling this module's own Δu function.

- **`ECOM-R-006`.** **Velocity dependence, in closed form** (`DYN-Q-002`'s stronger path: a full
  Jacobian rather than a declared bound, taken because one is derivable here). Δu needs the
  orbital plane, n̂ = r×v/|r×v|, so Δu — and through it, D(Δu), Y(Δu)=Y₀ (unaffected, it carries
  no Δu dependence) and B(Δu) — depends on v. The D/Y/B frame vectors themselves (`ECOM-R-001`)
  do not: Eq. 1 uses r and ŝ only, so v enters the acceleration through exactly one channel, Δu.
  This force reports its own `StateJacobian` via `with_velocity(da_dr, da_dv)`
  (`dyn::StateJacobian`, `force.hpp`), with

  ```
  da/dv = [dD/d(Δu)·ê_D + dB/d(Δu)·ê_B] ⊗ (∂Δu/∂v)
  ```

  the outer product of the acceleration's own Δu-derivative (a Vec3; Y contributes none, `ARN15`'s
  own printed form) and Δu's own velocity gradient (a covector, here a Vec3 standing for a 1×3
  row), both closed-form. ∂Δu/∂v is built by mechanical chain rule through n̂'s own dependence on
  v (∂(r×v)/∂v is the linear map x ↦ r×x, i.e. `skew(r)`), the unit-vector projector
  (I − ûûᵀ)/|u| at each normalization the chain passes through, and the ATAN2 quotient rule —
  the same family of primitives `ECOM-R-001`'s own position Jacobian uses, applied against v
  instead of r. Verified against a central finite difference of Δu itself, own script, 5 random
  geometries, ~1e-14 absolute agreement (`PROVENANCE.md` §30.21) — not assumed from the calculus
  alone, the same discipline `mu_rad`'s own worked example used.

- **`ECOM-R-007`.** **Not quadrature-blind.** `PROVENANCE.md` §22.3 found that a force whose own
  contribution to the right-hand side is a function of time alone degenerates an RKF7(8)-style
  embedded error estimate's own cancellation structure — the same failure mode a pure-quadrature
  ODE exhibits. ECOM2's own acceleration is a function of **position** (through Δu, §`ECOM-R-002`,
  and through ê_D/ê_Y/ê_B themselves, §`ECOM-R-001`) and of the Sun's own direction, not of time
  directly — evaluating it at a fixed epoch but a displaced position gives a different result, so
  it does not blind the controller the way a genuinely time-only term would. Stated here because
  the concern is real for *some* empirical models and worth ruling out explicitly, not because
  this force needed a fix for it.

---

## 5. Interfaces, stated language-free

- `EcomOrder`: the truncation orders (*n*_D, *n*_B) alone. A named value, not derived from a
  coefficient vector's own length, so the shape a force was constructed with and the shape a
  caller's own coefficient values happen to have length-match can be checked against each other
  rather than silently identified (`ECOM-Q-001`).
- `EcomCoefficients`: the (2*n*_D + 2*n*_B + 2) scalar values (*D*₀, *D*₂c, *D*₂s, …, *Y*₀, *B*₀,
  *B*₁c, *B*₁s, …) alone, sized to whatever `EcomOrder` the caller is using them with — a
  caller-owned value type, not read from any library. `ECOM-R-004`'s own D4B1 is a named
  construction of `EcomOrder{2,1}` plus this type, not a special case in the force's own logic.
- `ecom_acceleration(r_gcrs, v_gcrs, sun_direction_gcrs, order, coefficients) ->
  Result<EcomResult, EcomError>`: the free function `ECOM-R-001`–`R-003` and `R-006` describe,
  callable without a `dyn::Force` (`ECOM-A-001`–`007` all call it directly). `EcomResult` carries
  the acceleration, ê_D/ê_Y/ê_B, Δu, one `EcomSensitivities` (∂*a*/∂(coefficient), `ECOM-R-005`'s
  own layout mirrored one Vec3 per scalar) and ∂*a*/∂v in closed form (`ECOM-R-006`). It does NOT
  carry ∂*a*/∂r: that half is not analytically tractable through this same construction (n̂'s own
  r-dependence compounds through the D/Y/B frame too), so a `dyn::Force` caller takes it by
  central finite difference at its own call site — `Srp::accel`'s own established split
  (`PHPR-P-6`), applied here rather than re-decided.
- `EcomParameterIds`: `EcomCoefficients`' own shape with each scalar replaced by the
  `ParameterId` a caller's own registry issued for it (`Drag`'s own one-`ParameterId`-per-
  `declare` precedent, `ParameterKind::empirical_acceleration`, extended to as many scalars as
  `EcomOrder` implies) — declared by the CALLER, not by this module, matching `ECOM-R-004`'s own
  "nothing here reads a library" for values extended to identities. `flattened()` returns them
  in Eq. 5's own natural order, the form `dyn::Force::consumes()` requires.
- `Ecom final : public dyn::Force`, constructed from `(EcomOrder, EcomParameterIds, const
  eph::Ephemeris&, odl::time::LeapTable)` — no construction-time coefficient VALUES, since every
  `accel()` call reads live values from the registry (`ECOM-R-005`) and a stored "initial value"
  nobody re-reads after the first call would only be a second place for them to drift out of
  sync. `accel()` resolves the Sun via the bound ephemeris, calls `ecom_acceleration` for the
  acceleration/Δu/coefficient- and velocity-Jacobians, takes the position Jacobian by central
  finite difference (above), and reports both blocks via `dyn::StateJacobian::with_velocity`.

---

## 6. Precision and accuracy

- **`ECOM-P-1`.** The velocity Jacobian (`ECOM-R-006`) is exact in closed form, not a bound:
  `ECOM-A-007` checks it against a central finite difference of the full acceleration with
  respect to v, at randomized parameter sets and geometries, to the finite difference's own
  truncation floor — a mismatch there is a coding defect, not a modelling one, the same
  discipline this spec's own `ECOM-A-006` already applies to the coefficient Jacobians.
- Every sensitivity column (`ECOM-R-005`'s own parameter Jacobians) is EXACT in closed form —
  the acceleration is linear in each coefficient by construction (Eq. 5) — so `ECOM-A-006`
  checks against finite differences to the difference's own truncation floor, not to a stated
  physical tolerance; a mismatch there is a coding defect, not a modelling one.

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `ECOM-F-001` | ê_D ∥ ê_r — the spacecraft on the Earth–Sun line | forwarded from `FRAME-F-005`'s own cause, unrelabelled |
| `ECOM-F-002` | ŝ ∥ n̂ — the Sun exactly on the orbit normal, Δu undefined | the two nearly-parallel directions and the tolerance |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `ECOM-A-001` | The D/Y/B frame: orthonormal, right-handed (det = +1), ê_D·(r_Sun−r_sat) > 0, and ê_Y matches `ARN15` Eq. 1's own sign exactly (−(ê_r×ê_D), not the unsigned perpendicular) | all hold | `ECOM-R-001`, `FRAME-A-014`'s own precedent | 1e-15 | R-001 |
| `ECOM-A-002` | Δu computed from the SAME physical geometry expressed in two frames related by a random rotation gives the SAME Δu | agreement | `ECOM-R-002`'s own coordinate-independence claim | 1e-12 rad | R-002 |
| `ECOM-A-003` | D(Δu + π) = D(Δu), for random *n*_D ∈ {0,1,2,3} and random coefficients | equality | the even-harmonic-only structure, Eq. 5 | 1e-12 | R-003 |
| `ECOM-A-004` | B(Δu + π) − B₀ = −(B(Δu) − B₀), for random *n*_B ∈ {0,1,2,3} and random coefficients | equality | the odd-harmonic-only structure, Eq. 5 | 1e-12 | R-003 |
| `ECOM-A-005` | The reduction at *n*_D=0, *n*_B=1: this module's own D4B1-family output at *n*_B=1 matches Eq. 4 evaluated independently at *u* = Δu + *u*_s (both computed fresh in the test, not via this module's own Δu), with *B*_c/*B*_s rotated from *B*₁c/*B*₁s by `ECOM-R-005`'s own formula | agreement | Eq. 4 vs Eq. 5, cross-checked, not the same formula twice | 1e-9 rad-equivalent | R-002, R-005 |
| `ECOM-A-006` | Every registered parameter's own sensitivity column against a central finite difference of the full acceleration | agreement | linearity in each coefficient, Eq. 5 | finite-difference floor | R-005 |
| `ECOM-A-007` | The analytic velocity Jacobian (`ECOM-R-006`) against a central finite difference of the acceleration w.r.t. v, at randomized coefficient sets and geometries | agreement | `ECOM-R-006`'s own closed form | finite-difference floor | R-006 |
| `ECOM-A-008` | `ECOM-F-001`/`ECOM-F-002` fire at their own stated degeneracies, forwarded/carrying correctly | the diagnostics | `ECOM-R-001`/`R-002` | — | F-001, F-002 |

**Coverage.** Every requirement and refusal above is discharged by a row; `ECOM-R-007`
(quadrature-blindness) is a documentation statement about this force's own already-tested
position dependence, not a separate property needing its own row.

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as a new numbered section: the rule-4 search for `ARN15` (the failed
  routes and the one that worked), the frame confirmation against `FRAME-Q-002`, the gate
  replacement and its own reasoning, the Δu = μ + π cross-check, the reduction identity's own
  derivation, and the velocity-Jacobian derivation.
- `../plan/subplan_L4/L4-7.md` already records the rule-4 search and the gate replacement
  (2026-09-24); this document does not repeat that narrative, only cites it.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `ECOM-Q-001` | **Carried, not built.** `ARN15`'s own theoretical derivation of the even/odd harmonic structure (§4.1, a terminator-frame box-wing symmetry argument) is cited but not re-derived or implemented here — this spec takes Eq. 5's own printed form as normative and tests the symmetries it implies (`ECOM-A-003`/`004`), which is sufficient to implement and gate the model but does not reproduce `ARN15`'s own physical argument for WHY that form is the right one. Flagged in case a future reviewer expects the theoretical derivation itself to be carried forward. |
