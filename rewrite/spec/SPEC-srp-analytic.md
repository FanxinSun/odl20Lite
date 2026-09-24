# SPEC-srp-analytic — analytic solar radiation pressure

| | |
|---|---|
| **Spec ID** | `SRPA` |
| **Status** | **draft** 2026-09-22, for review (v1.1 adds §4.1: box-wing is composition; v1.2 closes the oblique-incidence gap v1.1's composition check could not see, with two single-plate checks and a pre-registered tessellated-sphere cross-check; v1.3, 2026-09-24, L5 step 4: `SRPA-A-014`–`A-016` validate `flat_force` against a published, source-provided worked example — SPOT-5's own Appendix-1 SRP example — within the energy-conservation scope `SPEC-photon-pressure.md` §4.1 now states beside the formula, and `SPEC-macromodel.md`'s new `MCRM-F-007` guard now enforces) |
| **Version** | 1.3 |
| **Date** | 2026-09-24 |
| **Layer** | L4 `forces-analytic`, step 3 (`../plan/PLAN.md` §3.5) |
| **Depends on** | `core`, `macromodel` |
| **Depended on by** | every SRP model at L9; `ecom` (step 7) as its a priori term |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no
implementation of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read,
listed, searched or otherwise inspected during this specification's preparation.

**This spec opens with a relocation, not new work.** `../plan/PLAN.md`'s verdict on L4
step 2: `srp_force` and its force-law tests were built inside `SPEC-macromodel`'s own module,
which put physics in what L5 designs as data and made every consumer of the macromodel
library link an SRP force — `PERT-Q-001`'s precedent, now applied here. `MCRM-R-005`
through `MCRM-R-009` (the two force laws) and their acceptance rows move to this document
and this module; `SPEC-macromodel` keeps the schema and gains a pure round-trip test of its
own. Nothing below is newly derived that was not already reviewed as part of step 2; the
one substantive change is `SRPA-A-005` (§8), corrected from `MCRM-A-005` per the manager's
ruling that the original test could not see the error it existed for.

---

## 1. Purpose and scope

The direct solar radiation pressure force, over a `macromodel::Macromodel`
(`SPEC-macromodel`) of any number of surfaces. v1.0 covered the **spherical cannonball**
and a **single flat plate** — both closed forms over one surface. v1.1 answers
`SRPA-Q-001`: **box-wing needs no new force law**, because `srp_force` already sums
`SRPA-R-001`/`-R-003` over every surface a `Macromodel` holds, and RHS12's box-wing —
"a satellite bus (box shape) and solar panels" — *is* several `FlatSurface`s under one
`Macromodel`. What v1.0 had not done was prove that summation correct for more than one
surface; `SRPA-R-008` states the claim and `SRPA-A-009`/`-A-010` prove it.

**Not in scope:**

- **The nominal attitude law.** RHS12's box-wing assumes an ideal Sun-tracking yaw
  attitude and the D/Y/B Sun-fixed frame (Fig. 1) to derive *which* body-fixed direction
  each bus surface's normal points at a given orbit position. This spec's `srp_force`
  takes `sun_direction_body` as given (§3's own convention, unchanged): computing *that*
  vector from an orbit position and an attitude law is a different module's job, not
  this one's, whether the macromodel has one surface or several.
- **Real satellite data.** No test in this spec's v1.1 reads `RS14`'s Tables 1–2 as a
  library entry. §4.2's rule-4 finding is why: `RHS12` prints no closed-form force-level
  expected value from stated a-priori inputs — Fig. 11's reconstructed acceleration uses
  **fitted**, not a-priori, optical parameters, and is a graph, not a printed number — so
  there is no category-1 published test case for box-wing to gate against, and the values
  that would make one wait for L5 (`../plan/PLAN.md` §3.6).
- **Eclipse scaling** — `SPEC-shadow`'s *F*ₛ is not applied here; this spec's force laws
  assume full sunlight throughout, and a caller multiplying by *F*ₛ is composing two
  modules' outputs, not something either module does internally (plan §5 constraint 8's
  spirit: no module reaches into another's job).
- **The Y-bias, solar-panel rotation lag, or any other fitted/empirical term** `RHS12` §4–5
  describes for its *adjustable* model. This step is the a priori physics only.

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `RHS12` | Rodríguez-Solano, C. J., Hugentobler, U., Steigenberger, P. | *Adjustable box-wing model for solar radiation pressure impacting GPS satellites* | *Advances in Space Research* **49**(7):1113–1128, 2012, doi:10.1016/j.asr.2012.01.016 | reprinted in full, pp. 85–101, in `RS14` | **primary** | normative |
| `RS14` | Rodríguez Solano, Carlos Javier | *Impact of non-conservative force modeling on GNSS satellite orbits and global solutions* (doctoral dissertation, TU München) | 2014 | `https://mediatum.ub.tum.de/doc/1188612/719708.pdf` | **primary** | normative — carries `RHS12` as Chapter P-II |
| `MSPB87` | Milani, A., Nobili, A. M., Farinella, P. | *Non-Gravitational Perturbations and Satellite Geodesy* | Adam Hilger, 1987 | — | **not obtained** | informative — `RHS12`'s own attribution for its Eq. (6) |

`RS14` is pinned as a `literature`-kind manifest entry (`rodriguez-solano-2014-dissertation`),
the same footing as `LI19`: a provenance record, terms not established, unreachable from
any build input. See `SPEC-macromodel` §2 for the retrieval account; not repeated here.

---

## 3. Definitions and conventions

Inherits `SPEC-macromodel` §3 entire: the body frame, *e*ᴰ, *e*ᴺ, *S*₀, *c*, α/ρ/δ and
Fliegel's (ν, µ) notation. Nothing here redefines them — a second definition of the same
quantity is how two modules drift apart (`SHDW`'s own lesson, `GRAV-R-029`/`PERT-R-006`).

---

## 4. Required behaviour

*Every requirement below was reviewed already, as an `MCRM-R-nnn` in `SPEC-macromodel` v1.0;
the correspondence is given at the end of each bullet rather than at the start, so it reads
as a note about provenance and not as part of the requirement's own identifying pattern.*

- **SRPA-R-001.** For a `FlatSurface` illuminated at incidence angle θ (cos θ = *e*ᴰ·*e*ᴺ,
  zero force when cos θ < 0):

  > ***f*** = −(*A S*₀/*c*) cos θ [(1−ρ) *e*ᴰ + 2(δ/3 + ρ cos θ) *e*ᴺ]  [`RHS12` Eq. (6)]

  Force, not acceleration — dividing by the macromodel's cited mass is the caller's.
  *(Was `MCRM-R-005`.)*
- **SRPA-R-002.** At normal incidence, ***f*** = −*A S*₀/*c* · (1 + ρ + 2δ/3) · *e*ᴰ,
  independently re-derived from momentum bookkeeping: absorption transfers *p* = *E*/*c*
  (coefficient 1), specular reflection at normal incidence bounces straight back
  (coefficient 2), diffuse scattering absorbs then Lambertian-re-emits with a mean recoil
  of (2/3)*p* on top of the *p* already transferred (coefficient 5/3); α·1 + ρ·2 + δ·5/3 =
  1 + ρ + 2δ/3 when α+ρ+δ = 1. *(Was `MCRM-R-006`.)*
- **SRPA-R-003.** For a `SphericalSurface`, integrating Eq. (6) over the illuminated
  hemisphere (the transverse components of ∫*e*ᴺ vanish by symmetry, the ρ-dependent terms
  cancel exactly: ∫₀¹[−ρ*x* + 2ρ*x*³] d*x* = 0):

  > ***f*** = −(*A S*₀/*c*)(1 + 4δ/9) *e*ᴰ.

  **No ρ dependence at all.** `PROVENANCE.md` §26.2 for the arithmetic in full.
  *(Was `MCRM-R-007`.)*
- **SRPA-R-004.** Substituting Fliegel's δ = ν(1−µ) into R-003 reproduces the plan's quoted
  `(9 + 4ν(1−μ))/9` exactly, confirming it is the same law integrated differently, not a
  coincidence of notation. **The two coefficients' gap, at general ρ, is ρ + 2δ/9** —
  R-002 minus R-003's coefficient, (1 + ρ + 2δ/3) − (1 + 4δ/9) = ρ + 6δ/9 − 4δ/9 = ρ + 2δ/9
  — and **the dominant term is ρ, not δ**: at ρ = 0.9, δ = 0 (a specular sail) the ratio is
  1.90; at ρ = 0.5, δ = 0.2 (a mixed panel) it is 1.50; at δ > 0, ρ = 0 alone the ratio
  never exceeds 1 + 2δ/9 ≤ 1.22 for δ ≤ 1. **This spec's predecessor tested only the last
  of these three** — the case where the gap is smallest — and is corrected as `SRPA-A-005`
  (§8). *(Was `MCRM-R-008`, with the arithmetic corrected: v1.0 called the diffuse-term
  ratio 2δ/3 ÷ 4δ/9 "a factor of 3"; it is 1.5, and in any case the smaller of the two
  terms in the gap.)*
- **SRPA-R-005.** A `SphericalSurface`'s force does not depend on attitude, having no
  normal to be sensitive to one. *(Was `MCRM-R-009`.)*
- **SRPA-R-006.** `srp_force(macromodel, e_D_body) -> Result<Vec3, Diagnostic>` sums
  R-001/R-003 over every surface, body frame throughout, no monadic chaining. *(Was
  `MCRM-R-010`.)*

### 4.1 Box-wing, answering `SRPA-Q-001`

- **SRPA-R-008.** RHS12's box-wing model — "a satellite bus (box shape) and solar
  panels" — **is** a `Macromodel` of several `FlatSurface`s: a `sun_pointing` one for
  the solar panels, and `body_fixed` ones for the bus. It needs **no separate force
  law**: R-006 already sums R-001/R-003 over every surface a `Macromodel` holds, for
  any count. What v1.0 left unproven is that the sum is correct for more than one
  surface — every acceptance row through `SRPA-A-008` used exactly one.
  - **SRPA-R-008a.** RHS12's own bus convention uses **four** surfaces, not six: Table 1
    and Table 2 list only *solar panels*, *+X bus*, *+Z bus*, *−Z bus* — no *−X* or
    *±Y* row. Under the nominal Sun-tracking yaw attitude Fig. 1 defines, −X and both Y
    faces never face the Sun, so they contribute nothing through R-001's own cos θ < 0
    domain and RHS12 omits stating optical properties for surfaces that would always be
    multiplied by zero. This schema does not need to know that: a caller who supplies a
    `Macromodel` with only four `FlatSurface`s gets exactly this convention for free,
    because R-001's domain already excludes any surface that happens to face away.

**Rule 4's first half, applied to `RHS12` before designing this section's gate:** does
it print a closed-form, force-level expected value — an acceleration for a stated
geometry, from stated a-priori inputs — that a box-wing implementation could be checked
against directly? **No.** §8 "Reconstruction of SRP acceleration" (Fig. 11) plots
reconstructed accelerations for two named satellites at a stated β₀, but that
reconstruction combines Table 1/2's a-priori *dimensions* with **parameters estimated by
fitting real GPS tracking data** (Fig. 5) — not the a-priori optical properties alone —
and gives the result only as a graph, not a printed number. Every other quantity in §§6–8
is an orbit-level residual (pseudo-stochastic pulses, orbit overlap/prediction error, SLR
bias) several steps downstream of a raw force, through a full numerical orbit
integration this layer does not perform. **There is no category-1 test case for
box-wing.** The gate is therefore composition (`SRPA-A-009`, `-A-010`): box-wing's force
equals the sum of its surfaces' individually-derived-and-tested forces, which is exactly
what plan rule 8's converse offers here — box-wing has an independent specification by
*construction*, being a sum of a law already derived from first principles, checkable
against arithmetic that does not involve `RHS12` at all. What `RHS12` could still add is
only the specific numbers, and those — Tables 1–2 — remain L5's.

**But composition alone does not reach the flat-plate law's own oblique-incidence
structure**, and this matters because box-wing's bus surfaces are oblique for most of an
orbit. `SRPA-A-004`, the one single-surface flat-plate test, is `sun_pointing` and black:
cos θ ≡ 1, so *e*ᴺ ≡ *e*ᴰ, and α = 1 so ρ = δ = 0. Every structural feature of R-001 —
the split between the *e*ᴰ-aligned term (α + δ) and the *e*ᴺ-aligned term (2ρ cos θ +
2δ/3), and the *extra* power of cos θ the specular term carries through the leading
scale factor — is either zero or collapsed onto one vector there; any force law of the
right general shape gives the same answer. `SRPA-A-009`'s composition check does exercise
oblique incidence, but both sides of that comparison call the identical per-surface code,
so a bug *in* that code appears identically on both sides and cancels — a wiring check,
correctly, and it cannot see the law it wires together.

- **SRPA-R-009.** Two closed-form single-plate checks, from momentum rather than from
  Eq. (6) itself (the same independent route R-002 already used for normal incidence):
  a pure absorber (α=1) at incidence θ gives force exactly *P A* cos θ along **−*e*ᴰ**,
  independent of *e*ᴺ; a pure specular reflector (ρ=1) gives force exactly 2*P A* cos²θ
  along **−*e*ᴺ**, where *P* = *S*₀/*c*. Each isolates one term of R-001 and one power
  of cos θ, and a term/direction swap fails one without moving the other.
- **SRPA-R-010.** A **tessellated sphere** — many small `body_fixed` `FlatSurface`s
  whose normals tile a sphere — cross-checks R-001 against R-003 by an independent
  route: `srp_force` summing the *flat* law at every incidence angle from 0° to 90°
  over the tessellation must converge, as facet count grows, to the *spherical* law's
  closed form (1 + 4δ/9), which was itself derived by integrating R-001 analytically —
  independently of any per-facet code — and checked by Monte Carlo (`SRPA-P-1`) before
  being trusted. Plan rule 8's converse: an independent property the composition must
  satisfy. **The sharpest case is pure specular** (ρ=1, δ=0): the closed form is exactly
  1, with **no ρ dependence**, so any bug that gives the specular term the wrong power
  of cos θ is not a rounding-level disagreement. Verified by re-deriving the hemisphere
  integral with that specific bug substituted in (`PROVENANCE.md` §27.7): a specular
  term carrying cos θ instead of cos²θ integrates to 1 + ρ/3, **1.333 at ρ = 1** — a
  33 % error, not a subtle one.

---

## 5. Interfaces, stated language-free

- `srp_force(model: &macromodel::Macromodel, sun_direction_body: macromodel::BodyDirection)
  -> Result<Vec3 [newtons, body frame], Diagnostic>` — the module's entire public surface
  at this version. Reads a `Macromodel`; does not construct or mutate one (that stays
  `macromodel`'s alone, per the manager's ruling that this module is "the force plugin
  reading the macromodel").

---

## 6. Precision

- **SRPA-P-1.** R-003's coefficient is checked by Monte Carlo numerical integration of
  Eq. (6) over a sphere (4 × 10⁶ samples, five (α, ρ, δ) triples spanning pure-absorbing,
  pure-specular, pure-diffuse and two mixed cases) **before** being trusted, matching the
  closed form to 3–4 significant figures at that sample count — a self-consistency check
  (plan §template §8's route 4), corroborating but not replacing the algebraic derivation
  of R-003, which is route 2, the stronger of the two. *(Was `MCRM-P-1`, moved here with
  the derivation it corroborates.)*
- **SRPA-P-2.** No physical constant here carries a tolerance of its own:
  *S*₀ and *c* are stated exactly as `RHS12` and `core` give them, and both force laws are
  evaluated in closed form with no quadrature. *(Was `MCRM-P-2`.)*
- **SRPA-P-3.** **Pre-registered before `SRPA-A-013`'s gate was written** (plan §4 rule
  7's middle form — the criterion stated before measuring, with its reason): the
  tessellated-sphere cross-check's discretisation error, for a latitude/longitude grid
  of *n* polar bands (2*n* azimuthal, each facet's normal at its cell centre, each
  facet's area its cell's *exact* solid angle), is **empirically second order in *n*** —
  doubling *n* quarters the relative error in the recovered coefficient. Measured before
  the gate existed, at the sharpest (pure specular) case, five consecutive doublings:

  | *n* (facets) | rel. error | error(*n*)/error(2*n*) |
  |---|---|---|
  | 4 (32) | 8.24 × 10⁻² | — |
  | 8 (128) | 1.96 × 10⁻² | 4.21 |
  | 16 (512) | 4.84 × 10⁻³ | 4.05 |
  | 32 (2048) | 1.21 × 10⁻³ | 4.01 |
  | 64 (8192) | 3.01 × 10⁻⁴ | 4.00 |
  | 128 (32768) | 7.53 × 10⁻⁵ | 4.00 |

  the same to three figures for pure-diffuse and mixed triples. **The gate uses *n* = 32
  and *n* = 64** (predicted error 1.2 × 10⁻³ and 3.0 × 10⁻⁴), asserting both against a
  tolerance with margin above the prediction, **and** the ratio between them against
  [3.0, 5.0] — bracketing the measured 4.00 generously while still excluding the 33 %
  a term/power bug would produce at *either* resolution, which is the property this
  check exists for, not the exact constant.

---

## 7. Failure behaviour

| id | when | diagnostic must name | never instead |
|---|---|---|---|
| `SRPA-F-001` | `srp_force` given a macromodel with zero surfaces (was `MCRM-F-004`) | that there is nothing to evaluate | returning a silent zero, indistinguishable from a correct answer at zero incidence |

**`MCRM-F-005` is retired, not relocated.** It refused `srp_force` a non-unit
`sun_direction_body` — but `sun_direction_body`'s type is `macromodel::BodyDirection`,
which cannot be constructed non-unit in the first place (`MCRM-F-002`, enforced at
`body_direction()`). A second runtime check here would guard a state the type already
makes unreachable — the redundant-guard shape this tree has repeatedly found and removed
elsewhere, caught here before it was ever tested rather than after.

---

## 8. Acceptance tests

Every row moved from `SPEC-macromodel` v1.0 (its `MCRM-A-nnn` given in the "what is
checked" column, not the id column, so the id column matches exactly one requirement
per plan §template's own format) except `SRPA-A-005`, corrected, and `SRPA-A-008`, unchanged
in substance but renumbered along with everything around it.

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `SRPA-A-001` | (was `MCRM-A-001`) the cannonball round-trip: one `SphericalSurface`, area and (α,ρ,δ) stated in the test, against the closed form, at 5 optical triples × 4 Sun directions | matches to rounding; force opposes *e*ᴰ at every one | R-003 | 1×10⁻¹² relative | R-003, R-006 |
| `SRPA-A-002` | (was `MCRM-A-002`) ρ does not move a sphere's force: 6 ρ values, α+δ fixed | force constant to rounding | R-003 | 1×10⁻¹² relative | R-003 |
| `SRPA-A-003` | (was `MCRM-A-003`) attitude independence: one sphere, 4 differently-"oriented" Sun directions | identical magnitude to rounding | R-005 | 1×10⁻¹² relative | R-005 |
| `SRPA-A-004` | (was `MCRM-A-004`) the flat-plate degenerate case: one sun-pointing black (α=1) plate against *f* = *S*₀*A*/*c*, cross-checked against `SRPA-A-001`'s α=1 sphere of the same area | matches both to rounding | R-002; cross-check | 1×10⁻¹² relative / 1×10⁻⁹ N | R-001, R-002, R-006 |
| `SRPA-A-005` | **corrected, not merely moved** (its predecessor was `MCRM-A-005`): the flat-plate/sphere gap, ρ + 2δ/9, asserted **across a range of ρ including ρ = 0.9, δ = 0** — the specular-dominated case the original never tried — not only the ρ = 0 diffuse-only case, so the guard covers the term that actually carries the plan's "factor of two" | gap matches ρ + 2δ/9 exactly at every (ρ,δ) tried, and the ρ=0.9,δ=0 ratio is 1.90 ± 1×10⁻⁶ | algebra on R-002/R-003 | exact / 1×10⁻⁶ | R-004 |
| `SRPA-A-006` | (was `MCRM-A-008`) `srp_force` on an empty-surfaces macromodel refuses | the diagnostic | refusal catalogue | — | F-001 |
| `SRPA-A-007` | (was `MCRM-A-010`) the cos θ < 0 domain, fired: a body-fixed flat surface facing away from the Sun contributes nothing | force exactly zero | R-001's stated domain | exact | R-001 |
| `SRPA-A-008` | (was `MCRM-A-009`) constraint 8, `odl::Result` throughout, via `ci.sh` gate 9 | as stated | plan §5 constraint 8 | — | R-006 |
| `SRPA-A-009` | **composition**: a multi-surface `Macromodel` (3–4 `FlatSurface`s, stated areas/optical properties/normals, a mix of `body_fixed` and `sun_pointing`) evaluated by `srp_force`, against the VECTOR SUM of `srp_force` on each surface evaluated alone (as its own one-surface `Macromodel`) — proving R-006's sum is correct for *N* > 1, which no row through `SRPA-A-008` exercised | the multi-surface force equals the sum of the single-surface forces, at several Sun directions | R-001, R-003 (linearity of the sum itself) | 1×10⁻¹² relative | R-006, R-008 |
| `SRPA-A-010` | **mixed lit/shadowed composition**: a multi-surface `Macromodel` at a STATED Sun direction under which at least one surface is lit (cos θ > 0) and at least one is not (cos θ < 0) — asserting the total equals the sum of ONLY the lit surfaces' individual forces, and that at least one surface's own contribution really is zero at that direction (so the case is not vacuous) | total matches the lit-only sum; at least one surface contributes exactly zero | R-001's domain, composed | exact / 1×10⁻¹² relative | R-001, R-008a |
| `SRPA-A-011` | **pure absorber at oblique incidence**: a single `body_fixed` α=1 `FlatSurface`, several STATED incidence angles strictly between 0° and 90°, force checked against *P A* cos θ **exactly along −*e*ᴰ** (both magnitude and direction, not magnitude alone) | matches to rounding; direction exactly −*e*ᴰ | momentum bookkeeping (independent of R-001's algebra) | 1×10⁻¹² relative | R-009 |
| `SRPA-A-012` | **pure specular reflector at oblique incidence**: a single `body_fixed` ρ=1 `FlatSurface`, the same stated angles, force checked against 2*P A* cos²θ **exactly along −*e*ᴺ** | matches to rounding; direction exactly −*e*ᴺ | momentum bookkeeping | 1×10⁻¹² relative | R-009 |
| `SRPA-A-013` | **the tessellated-sphere cross-check**, gated on `SRPA-P-3`'s pre-registered prediction: a latitude/longitude-tessellated sphere (*n* = 32 and *n* = 64 polar bands) summing the FLAT law over every facet, at pure-specular (ρ=1, sharpest), pure-diffuse and mixed triples, against the SPHERICAL law's closed form — both resolutions' error within the predicted margin, **and** their ratio in [3.0, 5.0] | *n*=32 error < 5×10⁻³, *n*=64 error < 1.5×10⁻³, ratio ∈ [3.0, 5.0] | `SRPA-P-3`'s prediction, itself route 2 (R-003's algebraic derivation) cross-checked by an independent numerical route | as stated | R-009, R-010 |
| `SRPA-A-014` | **(v1.3, L5 step 4) validation against a published, source-provided case, within `flat_force`'s own stated scope** (`SPEC-photon-pressure.md` §4.1's own energy-conservation note): a general 3-coefficient radiation-momentum formula, re-derived from first principles (NOT copied from `flat_force`, so this is not a tautology, plan §4 rule 5), reproduces all 20 of a CNES technical note's own printed (azimuth, elevation) → force test vectors for SPOT-5's bus (Appendix 1, `SALP-NT-BORD-OP-16137-CN`) to the precision they are printed | agreement to 5e-4 (the source's own printed-precision ceiling), all 20 vectors | the source's own Appendix 1, `tests/spot5_appendix_tests.cpp` | 5e-4 | R-001 |
| `SRPA-A-015` | **the guard keeping non-conserving data out of the kernel, shown firing through the real construction path** (`MCRM-F-007`, `SPEC-macromodel.md`): SPOT-5's own six rows (which do NOT conserve energy, 0.499–0.912) are each refused by `flat_surface_body_fixed` itself — the exact factory a real spacecraft-data file calls — proving such data cannot reach `photon_force` silently; an adjacent, genuinely conserving triple is NOT refused | all 6 SPOT-5 rows refused (`MCRM-F-007`); the conserving control succeeds | `SPEC-macromodel.md` `MCRM-R-016` | — | R-001 |
| `SRPA-A-016` | **the real kernel against the general formula, for energy-conserving input** — Sentinel-6's OWN real macromodel (`odl::spacecraft::sentinel6()`, energy-conserving on every row) run through the REAL `photon_force`, matched against `SRPA-A-014`'s own general formula applied to the SAME data, at 40 geometries spanning the appendix's own (azimuth, elevation) sweep: together with `SRPA-A-014`/`A-015`, this is how `photon_force` is validated against a published case — through the formula, within the scope `flat_force`'s own energy-conservation note states, and no more than that | exact agreement (1e-9), all 40 geometries | `tests/spot5_appendix_tests.cpp`, cross-module with `odl::spacecraft` | 1e-9 | R-001 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `SRPA-R-007` | A documentation obligation: `PROVENANCE.md` must record the relocation's own story — distinct from `SPEC-macromodel`'s entry — including what `SRPA-A-005`'s correction found and why the original test could not have found it. Discharged by §9 and the entry a reviewer reads, the same pattern as `SHDW-R-020`. |

---

## 9. Provenance obligations

- **SRPA-R-007.** `PROVENANCE.md` records the relocation itself — why `srp_force` moved,
  what `SRPA-A-005`'s correction found and why the original test could not have found it —
  distinct from `SPEC-macromodel`'s own provenance entry, which keeps the schema's story.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `SRPA-Q-001` | **Closed in v1.1.** Box-wing needed no new force law (§4.1, `SRPA-R-008`): `srp_force` already sums over every surface, and RHS12's box-wing is several `FlatSurface`s under one `Macromodel`. Rule 4's first half found no force-level published case (`RHS12` prints only fitted-parameter reconstructions as a graph, and orbit-level residuals) — plan rule 8's converse answers it instead: a sum of an already-derived law has an independent specification by construction. `SRPA-A-009`/`-A-010` gate the composition. The nominal D/Y/B attitude LAW remains out of scope (§1) — this spec takes `sun_direction_body` as given regardless of surface count, and computing it from an orbit position is a different module's job. |
