# SPEC-srp-analytic — analytic solar radiation pressure

| | |
|---|---|
| **Spec ID** | `SRPA` |
| **Status** | **draft** 2026-09-22, for review — **cannonball and flat plate only**; box-wing is `SRPA-Q-001` |
| **Version** | 1.0 |
| **Date** | 2026-09-22 |
| **Layer** | L4 `forces-analytic`, step 3 (`doc/REWRITE_PLAN.md` §3.5) |
| **Depends on** | `core`, `macromodel` |
| **Depended on by** | every SRP model at L9; `ecom` (step 7) as its a priori term |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no
implementation of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read,
listed, searched or otherwise inspected during this specification's preparation.

**This spec opens with a relocation, not new work.** `doc/REWRITE_PLAN.md`'s verdict on L4
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

The direct solar radiation pressure force, for the two macromodel configurations this step
covers: a **spherical cannonball** and a **single flat plate**. Both are closed forms over
a `macromodel::Macromodel` (`SPEC-macromodel`).

**Not in scope:**

- **Box-wing** — summing several flat and/or spherical surfaces under a real satellite's
  nominal attitude, with the D/Y/B Sun-fixed frame `RHS12` Fig. 1 defines. `SRPA-Q-001`.
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
| `SRPA-Q-001` | **Box-wing itself** — summing several surfaces under `RHS12`'s nominal D/Y/B attitude — is the rest of step 3 and is not attempted here. This spec's version 1.0 is deliberately the smaller, already-reviewed cannonball/flat-plate content the relocation carries, not a claim that step 3 is complete. |
