# SPEC-macromodel — the satellite macromodel schema

| | |
|---|---|
| **Spec ID** | `MCRM` |
| **Status** | **draft** 2026-09-22, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-22 |
| **Layer** | L4 `forces-analytic`, step 2 (`doc/REWRITE_PLAN.md` §3.5) |
| **Depends on** | `core` only |
| **Depended on by** | `srp-analytic` (step 3), `erp` (step 4), `thrust-yaw` (step 5), the L5 macromodel library, every SRP model at L9 |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no
implementation of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read,
listed, searched or otherwise inspected during this specification's preparation.

---

## 1. Purpose and scope

The **shape** that a satellite's non-conservative-force-relevant physical description
takes: how many surfaces it has, what each one is made of, and how heavy the whole thing
is — with every value traceable to where it came from. This is a **schema**, not a
populated model: no satellite's actual dimensions, mass or optical properties are stated
here. Population is L5's job (`doc/REWRITE_PLAN.md` §3.6), and this step exists
specifically so that job has a contract to fill in, decided **before** its first real
consumer (`srp-analytic`, step 3) exists to shape it by negotiation.

**Not in scope:**

- **Real satellite data.** No area, mass, or optical coefficient for any actual spacecraft
  is stated or pinned here. `SPEC-macromodel` owns the *type*; the L5 library owns the
  *values*.
- **The box-wing force model itself** — the full multi-surface accumulation used by
  `srp-analytic`, including nominal attitude and the D/Y/B frame. `SPEC-srp-analytic`
  (step 3) owns that; this spec's evaluator is exercised only by the two degenerate,
  citation-free configurations §8 states, both chosen so that "designed general, gated
  on a cannonball" (the plan's own phrase for this step) is provably true rather than
  asserted.
- **Attitude determination.** Every body-fixed quantity here is expressed **in the
  satellite's own body frame**; rotating a body-frame vector into or out of an inertial
  frame is the caller's problem, using whatever attitude source that caller has. `MCRM`
  never reads or computes an attitude.
- **Earth albedo/IR and antenna thrust surfaces' own physics** — steps 4 and 5 use this
  same schema, but the *forces* those steps compute (reflected sunlight, transmitted
  radio power) are theirs to specify.

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `RHS12` | Rodríguez-Solano, C. J., Hugentobler, U., Steigenberger, P. | *Adjustable box-wing model for solar radiation pressure impacting GPS satellites* | *Advances in Space Research* **49**(7):1113–1128, 2012, doi:10.1016/j.asr.2012.01.016 | reprinted in full, pp. 85–101, in the author's own doctoral dissertation (below); retrieved thence | **primary** | normative |
| `RS14` | Rodríguez Solano, Carlos Javier | *Impact of non-conservative force modeling on GNSS satellite orbits and global solutions* (doctoral dissertation, Technische Universität München) | 2014 | `https://mediatum.ub.tum.de/doc/1188612/719708.pdf` (retrieved 2026-09-22) | **primary** | normative — carries `RHS12` as its own Chapter P-II, plus Tables 1–2's a priori optical properties |
| `MSPB87` | Milani, A., Nobili, A. M., Farinella, P. | *Non-Gravitational Perturbations and Satellite Geodesy* | Adam Hilger, 1987 | — | **not obtained** | informative — the original source of the flat-surface interaction law `RHS12` states as its Eq. (6) and attributes by name; not sought separately because `RHS12` states the equation in full and this specification's requirements rest on that restatement, not on the book |

**`RS14` is a `literature`-kind manifest entry**, on the same footing as `LI19`
(`SPEC-shadow` §2): pinned by hash as a provenance record, exempt from the permissive-
licence gate because nothing derived from it is a copy of it, and unreachable from any
build input. A doctoral dissertation deposited at a German university's own repository is
not under a stated open licence in the way `LI19`'s UCL green-open-access route was; its
terms could not be established, and the entry records the search rather than a conclusion
(plan §4 rule 4). Retrieval needed no account and no request form: `mediatum.ub.tum.de`
serves the PDF directly over HTTPS with no authentication.

**Both formulas this spec relies on are re-derived, not only cited** (§4), and independently
checked by Monte Carlo numerical integration before being trusted (`PROVENANCE.md` §26,
once this step lands). Rule 8's converse applies to each: an independent, closed-form
derivation exists for both the flat-surface and the spherical-surface force laws, so an
implementation's legibility does not matter and none was sought.

---

## 3. Definitions and conventions

- **Body frame.** Every direction and position in this schema — a surface's normal, the
  satellite's centre of mass — is expressed in the satellite's own body-fixed frame,
  a Cartesian frame fixed to the spacecraft structure and defined by whoever populates a
  given macromodel (conventionally the body axes a GNSS attitude law like `RHS12`'s
  Fig. 1 uses, but this schema does not require that convention). It is **not** one of
  `frames::Frame`'s cases, because it is satellite-specific rather than a shared
  reference system, and `BodyDirection` (§5) exists precisely so a caller cannot pass an
  inertial-frame vector where a body-frame one is required without the type system
  objecting.
- **Sun direction, *e*ᴰ.** A unit vector from the satellite toward the Sun, in the body
  frame. Supplied by the caller at evaluation time; this schema does not compute it.
- **Surface normal, *e*ᴺ.** A unit vector, outward from the surface. For a `FlatSurface`
  in `body_fixed` mode it is a stored body-frame constant; in `sun_pointing` mode it is
  **defined to equal *e*ᴰ at every evaluation**, because a sun-tracking solar panel's
  whole point is that its normal follows the Sun by construction (`RHS12` §1, Fig. 1) —
  storing a separate body-fixed value for it would be storing a second, competing
  definition of the same thing (`SHDW`'s own lesson, `GRAV-R-029`/`PERT-R-006`, applied to
  a different pair of modules).
- ***S*₀**, the solar irradiance at 1 AU, 1367 W m⁻² [`RHS12` Eq. (6)'s own value].
- ***c***, the speed of light in vacuum, `core`'s own constant.
- **α, ρ, δ** — absorbed, specularly reflected, diffusely scattered fractions of incident
  radiation, `RHS12`'s own symbols. **Not constrained to sum to 1** (`RHS12` §4,
  discussing Eq. 7): the adjustable model deliberately allows α+ρ+δ ≠ 1 as extra degrees
  of freedom when *fitting* real tracking data. This schema stores the three
  independently for the same reason — the constraint belongs to a fit, not to the type.
- **Fliegel's (ν, μ) notation**, used by the sources `RHS12` cites for Block I/II/IIA/IIR
  a priori values, relates to (α, ρ, δ) by α = 1−ν, ρ = μν, δ = ν(1−μ) [`RHS12` §3, citing
  Fliegel et al. 1992]. Not used by this schema's own interface — only recorded here
  because §4's cannonball formula is usually printed in that notation and a reader
  comparing the two needs the dictionary.

---

## 4. Required behaviour

### 4.1 The schema: N surfaces, two kinds, every value cited

- **MCRM-R-001.** A macromodel is a list of **surfaces**, each **either** a `FlatSurface`
  **or** a `SphericalSurface` — freely mixed, any count including zero of either kind —
  plus one **mass** and one **centre of mass** for the whole satellite. This is the
  "designed general" half of the plan's instruction: nothing here bounds the surface
  count or requires every surface to be the same kind.
- **MCRM-R-002.** A `FlatSurface` carries: an area (m²), a normal mode (`body_fixed` with
  a stored unit `BodyDirection`, or `sun_pointing` with none), and α, ρ, δ.
- **MCRM-R-003.** A `SphericalSurface` carries: a cross-sectional area (m², i.e. π*r*²
  for a sphere of radius *r* — the schema stores the area a beam actually sees, not the
  radius, because the force law needs only the former), and α, ρ, δ. It carries **no
  normal**, because none exists: a sphere presents the same silhouette from every
  direction, which is the entire reason the cannonball model is simpler than a box-wing
  one, and giving it a normal field would misstate that.
- **MCRM-R-004.** **Every one of those values is a `Cited<T>`: the value together with a
  non-empty citation string.** `Cited<T>` cannot be constructed with an empty citation —
  the plan's "a value without a citation is a load error, not a warning" is enforced by
  the type, not by a check a caller could skip. §7's refusal is what firing that
  enforcement looks like.

### 4.2 The flat-surface force law

- **MCRM-R-005.** For a `FlatSurface` illuminated at incidence angle θ (cos θ = *e*ᴰ·*e*ᴺ,
  and the surface receives no force when cos θ < 0 — it faces away from the Sun):

  > ***f*** = −(*A S*₀/*c*) cos θ [(1−ρ) *e*ᴰ + 2(δ/3 + ρ cos θ) *e*ᴺ]  [`RHS12` Eq. (6)]

  where *A* is the surface's stated area. This is stated exactly as `RHS12` prints it,
  including that it omits the mass division present in `RHS12`'s Eq. (9) variant (which
  folds in re-radiated heat and is `srp-analytic`'s concern, not this schema's); this
  spec's evaluator returns **force**, and dividing by the macromodel's own cited mass to
  get an acceleration is left to the caller, because the caller may want the force for
  other purposes (torque, e.g.) first.
- **MCRM-R-006.** At normal incidence (cos θ = 1, always true for a `sun_pointing`
  surface, since there *e*ᴺ ≡ *e*ᴰ by definition), Eq. (6) collapses to a scalar times
  *e*ᴰ: ***f*** = −*A S*₀/*c* · (1 + ρ + 2δ/3) · *e*ᴰ. **Re-derived independently from
  momentum bookkeeping, not only algebra on Eq. (6)**: an absorbed photon transfers
  momentum *p* = *E*/*c* (coefficient 1); a specularly-reflected photon bounces straight
  back, transferring 2*p* (coefficient 2); a diffusely-scattered photon is absorbed and
  then Lambertian-re-emitted, whose emission carries a mean recoil of (2/3)*p* along the
  outward normal, on top of the *p* already transferred by absorbing it (coefficient
  1 + 2/3 = 5/3). Weighted by α, ρ, δ: α·1 + ρ·2 + δ·5/3 = (α+ρ+δ) + ρ + 2δ/3 = 1 + ρ +
  2δ/3 when α+ρ+δ = 1 — the same coefficient, by an independent route.

### 4.3 The spherical-surface force law

- **MCRM-R-007.** For a `SphericalSurface`, the **net** force (the transverse components
  of every surface element's contribution cancel by the sphere's own symmetry, leaving
  only the component along *e*ᴰ) is

  > ***f*** = −(*A* *S*₀/*c*) (1 + 4δ/9) *e*ᴰ

  where *A* is the stated cross-sectional area. **Derived here** by integrating Eq. (6)
  over a sphere's illuminated hemisphere (surface element at polar angle φ from the
  sub-solar point, cos θ = cos φ, *e*ᴺ(φ,λ) varying with position): the transverse
  components of ∫*e*ᴺ integrate to zero over the full azimuth, the ρ-dependent terms
  cancel exactly (∫₀¹ [−ρ*x* + 2ρ*x*³] d*x* = −ρ/2 + ρ/2 = 0, *x* = cos φ), and what
  remains is ∫₀¹ [*x* + (2δ/3)*x*²] d*x* = 1/2 + 2δ/9, giving the factor above after the
  2π azimuthal integral and dividing by the hemisphere's π steradian... arithmetic in
  `PROVENANCE.md` §26. **ρ does not appear**: a perfectly specularly-reflecting sphere
  and a perfectly absorbing one exert the same net force, which is why `MCRM-A-002`
  varies ρ alone and asserts the result does not move.
- **MCRM-R-008.** Fliegel's printed cannonball coefficient, (9 + 4ν(1−µ))/9, is the same
  formula in his notation: substituting δ = ν(1−µ) (§3) gives 1 + 4ν(1−µ)/9 =
  (9 + 4ν(1−µ))/9 exactly. **This is why the plan's own aside — "a sphere's
  (9 + 4ν(1−μ))/9 is not a flat plate's 1 + ρₛ" — is correct and not a coincidence of
  notation**: the two formulas differ in their δ-dependence (4δ/9 for a sphere, 2δ/3 for
  a flat plate at normal incidence — a factor of 3 apart on the diffuse term alone) and
  the flat-plate form `1 + ρₛ` additionally assumes δ = 0 outright, which is a second,
  separate simplification. Conflating a sphere's coefficient with a flat plate's is not
  one error but a compounding of two, and `MCRM-A-001`/`-A-002` gate each formula
  separately so neither can be silently substituted for the other.
- **MCRM-R-009.** A `SphericalSurface`'s force **does not depend on the satellite's
  attitude**, because it has no normal to be sensitive to one. A macromodel consisting
  only of spherical surfaces gives the same force for *any* body-frame orientation of
  *e*ᴰ that corresponds to the same *inertial* Sun direction.

### 4.4 Evaluation

- **MCRM-R-010.** `srp_force(macromodel, e_D_body) -> Result<BodyDirection-scaled force,
  Diagnostic>` sums MCRM-R-005/007 over every surface, in the body frame throughout, per
  §3's convention. No monadic chaining (plan §5 constraint 8); the sum is a loop, not a
  pipeline.

---

## 5. Interfaces, stated language-free

- `Cited<T>`: **opaque**. `cited(value: T, citation: string) -> Result<Cited<T>, Diagnostic>`
  is the only constructor; it refuses an empty or whitespace-only citation. `.value()`
  and `.citation()` are read accessors.
- `BodyDirection`: **opaque**, a unit 3-vector tagged as body-frame (not one of
  `frames::Frame`'s cases — see §3). Constructed only from a 3-vector the caller asserts
  is already unit length and body-frame; construction from a non-unit vector is refused
  (§7), because a silently-renormalised "unit vector" is exactly the kind of plausible
  wrong number plan §5 constraint 4 refuses rather than approximates.
- `NormalMode`: a sum type, `BodyFixed(BodyDirection)` or `SunPointing` (no payload).
- `FlatSurface { area_m2: Cited<double>, normal: NormalMode, absorptivity: Cited<double>,
  specular: Cited<double>, diffuse: Cited<double> }` — **immutable after construction**.
- `SphericalSurface { cross_section_area_m2: Cited<double>, absorptivity: Cited<double>,
  specular: Cited<double>, diffuse: Cited<double> }` — **immutable after construction**.
- `Surface`: a sum type of the two above.
- `Macromodel { surfaces: list<Surface>, mass_kg: Cited<double>, centre_of_mass_m:
  Cited<BodyDirection-like 3-vector, NOT required unit length> }` — **immutable after
  construction**; built via a builder that validates every citation before the
  `Macromodel` exists, so a partially-cited value is never observable, not even
  transiently.
- `srp_force(model: &Macromodel, sun_direction_body: BodyDirection) -> Result<Vec3
  [newtons, body frame], Diagnostic>` — §4.4.

---

## 6. Precision

- **MCRM-P-1.** MCRM-R-007's coefficient is checked by Monte Carlo numerical integration
  of Eq. (6) over a sphere (4 × 10⁶ samples, five (α, ρ, δ) triples spanning pure-
  absorbing, pure-specular, pure-diffuse and two mixed cases) **before** being trusted,
  matching the closed form to 3–4 significant figures at that sample count — a
  self-consistency check (plan §template §8's route 4), corroborating but not replacing
  the algebraic derivation of R-007, which is route 2, the stronger of the two.
- **MCRM-P-2.** No physical constant here carries a tolerance of its own: `S₀` and `c`
  are stated exactly as `RHS12` and `core` give them, and the force law is evaluated in
  closed form with no quadrature.

---

## 7. Failure behaviour

| id | when | diagnostic must name | never instead |
|---|---|---|---|
| `MCRM-F-001` | `cited()` called with an empty or whitespace-only citation | which value was being cited | silently accepting an empty citation, or substituting a placeholder string |
| `MCRM-F-002` | a `BodyDirection` constructed from a non-unit vector | the vector and its actual norm | silently renormalising |
| `MCRM-F-003` | a `Macromodel` builder asked to finish with any surface, the mass, or the centre of mass not yet cited | which field(s) are missing | defaulting the citation to empty and proceeding |
| `MCRM-F-004` | `srp_force` given a macromodel with zero surfaces | that there is nothing to evaluate | returning a zero force silently, which is indistinguishable from a correct answer at zero incidence |
| `MCRM-F-005` | `srp_force` given a non-unit `sun_direction_body` | the vector and its norm | silently renormalising (same discipline as F-002, at the call boundary instead of construction) |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `MCRM-A-001` | **the cannonball round-trip**: a `Macromodel` with exactly one `SphericalSurface` (area, α, ρ, δ **stated in the test**, no library read), evaluated at several Sun directions and several (α, ρ, δ) triples spanning the five MCRM-P-1 cases, against the closed form 1 + 4δ/9 | force matches `−(A S₀/c)(1+4δ/9) e_D` to rounding | closed-form derivation, `MCRM-R-007` (route 2) | 1 × 10⁻¹² relative | R-001, R-003, R-007, R-010 |
| `MCRM-A-002` | **ρ does not move a sphere's force**: holding α+δ fixed and varying ρ alone (trading it against α) leaves the computed force unchanged | force constant to rounding across the sweep | `MCRM-R-007`'s derivation, which has no ρ term | 1 × 10⁻¹² relative | R-007 |
| `MCRM-A-003` | **attitude independence**: a spherical-only macromodel evaluated with the SAME *e*ᴰ expressed in several different (arbitrarily rotated) body-frame conventions gives the same force magnitude and the same direction relative to *e*ᴰ | identical to rounding across rotations | `MCRM-R-009` | 1 × 10⁻¹² relative | R-009 |
| `MCRM-A-004` | **the flat-plate degenerate case**: a `Macromodel` with exactly one `SunPointing` `FlatSurface` (α=1, ρ=0, δ=0 — a black, fully-absorbing sail — stated in the test), evaluated at several Sun directions, against the textbook radiation-pressure identity *f* = *S*₀*A*/*c* | force matches to rounding, **and matches `MCRM-A-001`'s α=1 spherical case of the same area to rounding** (both reduce to the same textbook identity) | closed-form derivation, `MCRM-R-006`; cross-checked against `MCRM-A-001`'s α=1 row | 1 × 10⁻¹² relative | R-002, R-005, R-006, R-010 |
| `MCRM-A-005` | **the two coefficients genuinely differ**: at a stated δ > 0, ρ = 0, the flat-plate normal-incidence coefficient (1 + 2δ/3) and the sphere's (1 + 4δ/9) are computed from the SAME δ and asserted **not equal** — the guard against silently substituting one for the other (`MCRM-R-008`) | `\|(1+2δ/3) − (1+4δ/9)\| > 0`, and specifically equal to 2δ/9 | algebra on R-006 and R-007 | exact | R-008 |
| `MCRM-A-006` | citation enforcement fires: `cited()` refused on an empty string, and a `Macromodel` builder refused when any one of {a surface's area, its α, its ρ, its δ, the mass, the centre of mass} is left uncited — one case per field, **and shown not to fire** when all are cited | the diagnostics, and success on the adjacent fully-cited input | the refusal catalogue | — | F-001, F-003 |
| `MCRM-A-007` | `BodyDirection` and `srp_force`'s sun-direction argument refuse a non-unit vector, **and do not refuse** a genuinely unit one adjacent to it | the diagnostics | the refusal catalogue | — | F-002, F-005 |
| `MCRM-A-008` | `srp_force` on an empty-surfaces macromodel refuses | the diagnostic | the refusal catalogue | — | F-004 |
| `MCRM-A-009` | constraint 8, `odl::Result` throughout, no monadic chaining, via `ci.sh` gate 9 | as stated | plan §5 constraint 8 | — | R-010 |
| `MCRM-A-010` | **the cos θ < 0 domain, fired**: a body-fixed flat surface with the Sun placed exactly behind it contributes nothing — the branch R-005 states but no other row exercises, and a domain restriction with no test reaching it is the same fault as a guard that cannot fire | force exactly zero | R-005's own stated domain | exact | R-005 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `MCRM-R-004` | The type-level guarantee itself (`Cited<T>` uncomstructible with an empty citation) is discharged by `MCRM-A-006`; the *documentation* half of R-004 — that this is a deliberate reading of the plan's "load error, not a warning" instruction — has no test of its own beyond A-006 firing. |
| `MCRM-R-011` | A documentation obligation: `PROVENANCE.md` must record the retrieval route, that `RHS12` and `RS14`'s Chapter P-II are the same text, and both force-law derivations with their checks. Discharged by §9 and the entry a reviewer reads, the same pattern as `SHDW-R-020`. |

---

## 9. Provenance obligations

- **MCRM-R-011.** `PROVENANCE.md` records: the retrieval route for `RS14` (mediaTUM, no
  account, hash pinned), that `RHS12` is reprinted in full within it (so the paper the
  plan names and the dissertation chapter read are the same text), the two force-law
  derivations with their Monte Carlo corroboration, and the momentum-bookkeeping
  cross-check of MCRM-R-006.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `MCRM-Q-001` | **`centre_of_mass_m`'s type.** §5 states it as a `Cited` 3-vector that is *not* required to be unit length (unlike `BodyDirection`), since a centre of mass is a position offset in metres, not a direction. It is written here as a distinct, weaker-constrained type rather than reusing `BodyDirection` with its unit-length refusal disabled, to avoid one type silently meaning two different things depending on context (plan §5 constraint 10). Naming it explicitly for review rather than leaving the choice implicit in the code. |
| `MCRM-Q-002` | **Whether `Cited<T>`'s citation string should be structured** (a manifest-entry id, a page/table reference, a free-form note) rather than a bare string, matching how `Provenance` structs elsewhere in this tree (`modules/gravity/include/odl/gravity/field.hpp`) name a specific file and hash. Left as a free-form string for this step because L5 — the actual populator — is better placed to know what structure its real citations need (a Table 1 row citing `RS14` by page differs in kind from a citation to a manifest-pinned SINEX file), and over-structuring it now risks shaping L5's schema before L5 exists, the exact trap this step's own re-siting from L5 was meant to avoid. |
