# SPEC-macromodel — the satellite macromodel schema

| | |
|---|---|
| **Spec ID** | `MCRM` |
| **Status** | **draft** 2026-09-24, for review (v2.0: `srp_force` and the two force laws relocated to `SPEC-srp-analytic`, per the manager's verdict on v1.0 — `../plan/PLAN.md`'s L4 step list, step 2's entry. v2.1: the band/face amendment §1's own "Not in scope" list already anticipated — L4 step 5's real need, found while building `SPEC-photon-pressure`, not spun ahead of it. v2.2: the energy-conservation guard, `MCRM-R-016`/`MCRM-F-007` — L5 step 4's own review, after a published worked example (SPOT-5's own Appendix-1 SRP case) was found to fall outside the kernel's own implicit scope, `SPEC-photon-pressure.md` §4.1) |
| **Version** | 2.2 |
| **Date** | 2026-09-24 |
| **Layer** | L4 `forces-analytic`, step 2 (`../plan/PLAN.md` §3.5) |
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
here. Population is L5's job (`../plan/PLAN.md` §3.6), and this step exists
specifically so that job has a contract to fill in, decided **before** its first real
consumer (`srp-analytic`, step 3) exists to shape it by negotiation.

**Not in scope:**

- **Real satellite data.** No area, mass, or optical coefficient for any actual spacecraft
  is stated or pinned here. `SPEC-macromodel` owns the *type*; the L5 library owns the
  *values*.
- **Every force law.** v1.0 of this document defined `srp_force` and both the flat-surface
  and spherical-surface force formulas here. Review found that wrong: it put physics in
  what L5 designs as data, and made every consumer of the macromodel schema link an SRP
  force whether it needed one or not (`PERT-Q-001`'s own precedent, applied here). Both
  force laws, `srp_force` itself, and every test that computes a force now live in
  `SPEC-srp-analytic` / `modules/srp_analytic`, which depends on this module — this module
  does not depend on it. What stays here is *why* the schema has two surface kinds (§3's
  note on the two force laws differing), not the laws themselves.
- **Attitude determination.** Every body-fixed quantity here is expressed **in the
  satellite's own body frame**; rotating a body-frame vector into or out of an inertial
  frame is the caller's problem, using whatever attitude source that caller has. `MCRM`
  never reads or computes an attitude.
- **Earth albedo/IR and antenna thrust surfaces' own physics** — steps 4 and 5 use this
  same schema, but the *forces* those steps compute (reflected sunlight, transmitted
  radio power) are theirs to specify. **§4.3 (v2.1) is the real need this bullet
  anticipated**: `PHPR-R-004a`/`R-004b` found the schema itself, not only the force law
  reading it, needed a back face and a spectral band before ERP's own albedo term could be
  correct — a `FlatSurface` amendment, additive, not a new force computed here.

## 1a. v2.1 amendment — scope

The face/band amendment (§4.3, `SPEC-photon-pressure`'s own `PHPR-R-004a`/`R-004b`) is the
FIRST time this schema was extended by a real consumer's real need rather than designed
ahead of one — `MCRM-R-001`'s own "designed general, gated on a cannonball" note applied a
second time, this time to the schema's own evolution rather than its first design. Nothing
existing changes in observable behaviour: every new field is optional, defaulting to the
exact prior behaviour (`DYN-Q-001`'s own terms, applied here as they were to `SPEC-dynamics`
in step 4). `PHPR-A-001`'s own bit-identity proof is the discharge that matters most for
this claim — not a schema-level assertion, since the schema itself has no force to compare,
but the strongest evidence available that this amendment changed nothing for an existing
caller.

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `RHS12` | Rodríguez-Solano, C. J., Hugentobler, U., Steigenberger, P. | *Adjustable box-wing model for solar radiation pressure impacting GPS satellites* | *Advances in Space Research* **49**(7):1113–1128, 2012, doi:10.1016/j.asr.2012.01.016 | reprinted in full, pp. 85–101, in the author's own doctoral dissertation (below); retrieved thence | **primary** | normative |
| `RS14` | Rodríguez Solano, Carlos Javier | *Impact of non-conservative force modeling on GNSS satellite orbits and global solutions* (doctoral dissertation, Technische Universität München) | 2014 | `https://mediatum.ub.tum.de/doc/1188612/719708.pdf` (retrieved 2026-09-22) | **primary** | normative — carries `RHS12` as its own Chapter P-II, plus Tables 1–2's a priori optical properties |
| `MSPB87` | Milani, A., Nobili, A. M., Farinella, P. | *Non-Gravitational Perturbations and Satellite Geodesy* | Adam Hilger, 1987 | — | **not obtained** | informative — the original source of the flat-surface interaction law `RHS12` states as its Eq. (6) and attributes by name; not sought separately because `RHS12` states the equation in full and this specification's requirements rest on that restatement, not on the book |
| `RS09` | Rodríguez-Solano, C. J. | *Impact of albedo modelling on GPS orbits* (Master's thesis) | TU München, 2009 | `https://mediatum.ub.tum.de/doc/1083571/1083571.pdf` (pinned as `rodriguez-solano-2009-masters-thesis`, retrieved for `SPEC-photon-pressure` L4 step 5) | **primary** (§4.3 only) | Table 3.1's own two axes — face (front/back) and spectral band (visible/infrared) — for real, measured GPS panel optical properties; `SPEC-photon-pressure` §2 carries the fuller citation and reasoning this spec does not repeat |

**`RS14` is a `literature`-kind manifest entry**, on the same footing as `LI19`
(`SPEC-shadow` §2): pinned by hash as a provenance record, exempt from the permissive-
licence gate because nothing derived from it is a copy of it, and unreachable from any
build input. A doctoral dissertation deposited at a German university's own repository is
not under a stated open licence in the way `LI19`'s UCL green-open-access route was; its
terms could not be established, and the entry records the search rather than a conclusion
(plan §4 rule 4). Retrieval needed no account and no request form: `mediatum.ub.tum.de`
serves the PDF directly over HTTPS with no authentication.

**Both force laws are re-derived, not only cited, in `SPEC-srp-analytic` §4** (relocated
there in v2.0 of this document; not restated here — see that spec), and independently
checked by Monte Carlo numerical integration before being trusted (`PROVENANCE.md` §26).
Rule 8's converse applies to each: an independent, closed-form derivation exists for both
the flat-surface and the spherical-surface force laws, so an implementation's legibility
does not matter and none was sought. `RS14` is read here only for §3's own purpose — that
the schema needs two surface kinds because two different force laws exist — not for its
Tables 1–2, which are `SPEC-srp-analytic` and L5's concern.

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
Fliegel's (ν, μ) optical-property notation is not used by this schema and is not defined
here; `SPEC-srp-analytic` §3 carries it, since that is the spec whose cannonball formula is
usually printed in it.

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

### 4.2 The round trip

- **MCRM-R-012.** Fresh in v2.0 — R-005 through R-010 are retired along with the force
  laws they described (see the Retired identifiers table below), not reused here for
  something unrelated.
  **Reading back a constructed `Macromodel` returns exactly what was put in** — every
  surface, in the kind and order it was added; every `Cited<T>`'s value and citation
  unchanged; the mass and centre of mass unchanged. This is the schema's own gate once
  §1's force laws left: a value this general (N surfaces of either kind) needs a check
  that does not depend on any consumer existing to exercise it indirectly.

### 4.3 The band/face amendment (v2.1)

- **MCRM-R-013.** An optical triple (α, ρ, δ) is now a named type, `OpticalTriple`, not
  three loose fields — one struct, so a caller who has two of three cannot leave the kernel
  to guess the third (the same "all or nothing, enforced by the type" discipline
  `MCRM-R-004` already applies to a `Cited<T>`'s value and citation). `Band` (`visible` /
  `infrared`) selects which of a surface's own triples applies; `BandedOptics` pairs a
  required visible triple with an optional infrared one, `.in(Band)` resolving the
  infrared request to the visible triple when none was separately stated — a documented
  fall-back (`RS09` Table 3.1, §2: a real GPS panel's own visible and infrared properties
  differ enough to matter), not a silent assumption that the two are equal.
- **MCRM-R-014.** `FlatSurface` gains an **optional back face** — itself a `BandedOptics`,
  the same visible-required/infrared-optional shape as the front. Absent, a surface is
  one-sided exactly as it was before this field existed (a bus face whose back is inside
  the body — nothing changes for it, and `PHPR-A-001`'s bit-identity proof is the evidence).
  Present, the consuming force law (`SPEC-photon-pressure` §4.1) evaluates it, with the
  reversed normal, whenever the illumination source is behind the front — this schema
  states only that the field exists and what it pairs with, not when a force law reads it,
  which stays that spec's own concern (§1's "not every force law" boundary, unchanged).
  `SphericalSurface` gains the analogous optional infrared triple (no back-face concept
  applies — a sphere has no front or back, `MCRM-R-003`'s own reasoning).
- **MCRM-R-015.** `IrradianceWPerM2`: an opaque, unit-typed flux density,
  `irradiance_w_per_m2(double) -> Result<IrradianceWPerM2, MacromodelError>`, refusing a
  negative, infinite or NaN value (`MCRM-F-006`) — the same "a value that carries its unit
  cannot silently become one that does not" discipline `BodyDirection` already established
  for a direction, applied here to a flux density so a force-law caller (`PHPR-R-001`)
  cannot pass a bare, unit-less `double` where an irradiance is required. Lives in this
  module, not `srp_analytic`, because it is schema-adjacent (a physical-quantity wrapper,
  the same kind of thing `Cited<T>`/`BodyDirection` already are here) rather than
  force-law logic — `SPEC-photon-pressure` §3 states what it is used for.

  Every field above is additive and optional, defaulting to prior behaviour exactly
  (`DYN-Q-001`'s own terms) — §1a states the claim this subsection's own requirements make
  true.

### 4.4 The energy-conservation guard (v2.2, L5 step 4's own review)

- **MCRM-R-016.** `flat_surface_body_fixed` and `flat_surface_sun_pointing` REFUSE
  (`MCRM-F-007`) any `OpticalTriple` — front visible, front infrared, back visible or back
  infrared, whichever are supplied — whose own `absorptivity + specular + diffuse` is more than
  1% relative away from 1. `SPEC-photon-pressure.md` §4.1's own kernel formula,
  `flat_force`'s `(1−ρ)*e_D + 2*(δ/3+ρ·cosθ)*e_N`, is the GENERAL three-coefficient
  radiation-momentum law, `(α+δ)*e_D + 2*(δ/3+ρ·cosθ)*e_N`, ONLY where `α+ρ+δ=1` — a fact
  found, not assumed, at L5 step 4: a published worked example (SPOT-5's own bus, a CNES
  technical note's Appendix 1) does NOT conserve energy, and the REAL kernel's own output,
  fed that data directly, disagreed with the example's own printed answer by the row's own
  energy gap (`SPEC-srp-analytic.md` `SRPA-A-014`/`A-015`). Every macromodel this tree has
  built through this version conserves energy exactly or to floating-point rounding — this
  guard makes that a CHECKED property of every surface this schema accepts from now on, not
  merely an unstated assumption every source so far happened to satisfy: a future
  non-conserving macromodel cannot reach `photon_force` silently. The 1% threshold sits with
  real margin on both sides of every value seen so far — an order of magnitude above the
  largest genuine rounding-level deviation this tree has accepted (Jason-2's/Jason-3's own
  infrared rows, up to 0.2%, `SPEC-spacecraft.md` `SPCR-P-5`) and almost an order of
  magnitude below the smallest genuine violation found (SPOT-5's own least-bad row, 8.8%
  short of 1) — not tuned to either boundary.
- **MCRM-R-017.** `SphericalSurface` is NOT covered by this guard: its own `absorptivity`/
  `specular` are stored for completeness but never read by `spherical_force`
  (`MCRM-R-007`'s own already-proven fact — only `diffuse`, through `Q_pr=1+4δ/9`, affects a
  sphere's net force), so the energy-conservation identity this guard enforces for
  `FlatSurface` does not describe a sphere's own force law the same way, and no constructor
  exists to attach a check to (`SphericalSurface` is a plain aggregate, `MCRM-R-003`). Not
  built speculatively for a surface kind no constellation in this tree has used
  (`SPCR-Q`-style "no consumer" reasoning, applied here to a schema gap rather than a data
  one).

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
- `NormalMode`: a sum type, `BodyFixed` or `SunPointing`.
- `OpticalTriple { absorptivity: Cited<double>, specular: Cited<double>, diffuse: Cited<double> }`
  (v2.1, `MCRM-R-013`) — a plain aggregate, all three fields required, no default.
- `Band`: a sum type, `visible` or `infrared` (v2.1).
- `BandedOptics { visible: OpticalTriple, infrared: optional<OpticalTriple> }` (v2.1) —
  `.in(Band) -> const OpticalTriple&`, resolving `infrared` to `visible` when absent.
- `FlatSurface`: **opaque**, constructed only through `flat_surface_body_fixed(area,
  normal, absorptivity, specular, diffuse, front_infrared = none, back = none)` or
  `flat_surface_sun_pointing(area, absorptivity, specular, diffuse, front_infrared = none,
  back = none)` — two named factories, one per `NormalMode`, rather than a struct pairing a
  mode with an independent optional normal that the two could disagree about. The two
  trailing parameters (v2.1) both default to absent, so every pre-v2.1 call compiles
  unchanged. Both factories REFUSE (v2.2, `MCRM-F-007`, `MCRM-R-016`) any supplied triple
  more than 1% short of energy conservation. Read accessors: `.area_m2()`, `.normal_mode()`, `.body_fixed_normal()`
  (populated iff the mode is `BodyFixed`), `.absorptivity()`, `.specular()`, `.diffuse()`
  (the front, visible triple — unchanged in meaning), `.front(Band) -> const
  OpticalTriple&` (v2.1, resolved), `.back(Band) -> optional<OpticalTriple>` (v2.1, absent
  iff no back face was supplied). **Immutable after construction**.
- `SphericalSurface { cross_section_area_m2: Cited<double>, absorptivity: Cited<double>,
  specular: Cited<double>, diffuse: Cited<double>, infrared: optional<OpticalTriple> =
  none (v2.1, trailing) }` — **immutable after construction**; a plain aggregate is safe
  here because every non-trailing field is a `Cited<T>` with no default, so aggregate
  initialisation cannot omit one, and the trailing `optional` defaults to `none` the way
  every C++ aggregate leaves an un-given trailing member. `.in(Band) -> OpticalTriple`
  (v2.1), the same resolution `BandedOptics::in` performs.
- `IrradianceWPerM2` (v2.1): **opaque**.
  `irradiance_w_per_m2(v: double) -> Result<IrradianceWPerM2, MacromodelError>` is the only
  constructor, refusing a negative, infinite or NaN `v` (§7). `.watts_per_m2()` is the read
  accessor.
- `Surface`: a sum type of the two surface kinds above.
- `Macromodel { surfaces: list<Surface>, mass_kg: Cited<double>, centre_of_mass_m:
  Cited<3-vector, NOT required unit length> }` — **immutable after construction**; built
  only through `MacromodelBuilder`, which validates that mass and centre of mass were
  both set before `build()` succeeds, so a partially-specified value is never observable,
  not even transiently.
- No force-computing function is declared here. `SPEC-srp-analytic` §5 has `srp_force`.

---

## 6. Precision

No physical constant or numerical tolerance is stated by this schema; every number this
document once carried in that role (`MCRM-P-1`, `-P-2` in v1.0) belonged to a force law
and moved with it. `SPEC-srp-analytic` §6.

---

## 7. Failure behaviour

| id | when | diagnostic must name | never instead |
|---|---|---|---|
| `MCRM-F-001` | `cited()` called with an empty or whitespace-only citation | which value was being cited | silently accepting an empty citation, or substituting a placeholder string |
| `MCRM-F-002` | a `BodyDirection` constructed from a non-unit vector | the vector and its actual norm | silently renormalising |
| `MCRM-F-003` | a `Macromodel` builder asked to finish with the mass or the centre of mass not yet set | which field(s) are missing | defaulting the citation to empty and proceeding |
| `MCRM-F-006` | (v2.1) `irradiance_w_per_m2` called with a negative, infinite or NaN value | the offending value | treating it as zero, or clamping to the nearest valid flux density |
| `MCRM-F-007` | (v2.2) `flat_surface_body_fixed`/`flat_surface_sun_pointing` called with any supplied `OpticalTriple` (front visible, front infrared, back visible or back infrared) more than 1% relative from summing to 1 | which band/face triple, its own actual sum, and the kernel-scope reasoning (`MCRM-R-016`) | silently accepting it and letting a mismatched force reach `photon_force` |

**`MCRM-F-004` and `-F-005` are retired, not relocated.** Both were about `srp_force`'s
own refusals; that function is no longer declared by this module, so there is nothing
here for them to guard. `SPEC-srp-analytic` §7 (`SRPA-F-001`) carries the surviving one;
the other (a non-unit sun direction) turned out to guard a state `BodyDirection`'s own
type already makes unreachable, and is not carried forward at all — see that spec's §7.

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `MCRM-A-011` | **the cannonball round-trip, schema-only**: a `Macromodel` built from exactly one `SphericalSurface`, its mass and its centre of mass, every field with a DISTINCT stated citation — read back and compared field by field, value and citation both | every value and every citation matches exactly what was constructed | identity — the strongest of the plan §template §8 routes when it applies at all | exact | R-001, R-003, R-004, R-012 |
| `MCRM-A-012` | **the flat-surface round trip, both `NormalMode`s**: `flat_surface_sun_pointing` carries no normal; `flat_surface_body_fixed` carries exactly the one supplied | `.normal_mode()` and `.body_fixed_normal()` agree with which factory was called, in both directions | `MCRM-R-002`'s stated pairing | exact | R-002, R-012 |
| `MCRM-A-006` | citation enforcement fires: `cited()` refused on an empty string, and a `Macromodel` builder refused when the mass or the centre of mass is left unset — **and shown not to fire** when both are set | the diagnostics, and success on the adjacent fully-cited input | the refusal catalogue | — | F-001, F-003 |
| `MCRM-A-007` | `BodyDirection` refuses a non-unit vector, **and does not refuse** a genuinely unit one adjacent to it | the diagnostics | the refusal catalogue | — | F-002 |
| `MCRM-A-013` | **(v2.1) the band/face round trip, schema-only, no force law involved**: (1) `BandedOptics.in(Band::infrared)` returns the stated infrared triple when one was supplied, and falls back to the visible triple when it was not, for both `FlatSurface` (front) and `SphericalSurface`; (2) `FlatSurface.back(Band)` is absent when no back face was supplied and returns the stated (band-resolved) triple when one was | as stated, all cases, both surface kinds | `MCRM-R-013`/`R-014`'s own stated fall-back | exact | R-013, R-014 |
| `MCRM-A-014` | **(v2.1)** `irradiance_w_per_m2` refuses a negative value and a NaN value, **and does not refuse** a genuine (including zero) non-negative finite one adjacent to them — whose `.watts_per_m2()` round-trips exactly | the diagnostics; the round-tripped value | the refusal catalogue; identity | exact (round trip) | F-006, R-015 |
| `MCRM-A-015` | **(v2.2) the guard shown firing through the real construction path, not a synthetic one**: `flat_surface_body_fixed`, called with a published, genuinely non-conserving triple (SPOT-5's own six Appendix-1 rows, 0.499–0.912) each refused with `MCRM-F-007`; an adjacent, genuinely conserving triple (Sentinel-6's own +X row) is NOT refused | all 6 SPOT-5 rows refused; the conserving control succeeds | `SPEC-srp-analytic.md` `SRPA-A-015`, `tests/spot5_appendix_tests.cpp` | — | R-016, F-007 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `MCRM-R-011` | A documentation obligation: `PROVENANCE.md` must record the retrieval route for `RS14`, that `RHS12` is reprinted in full within it, and the relocation this version records — what moved, why, and what the review found while it moved. Discharged by §9 and the entry a reviewer reads, the same pattern as `SHDW-R-020`. |
| `MCRM-R-005`, `MCRM-R-006`, `MCRM-R-007`, `MCRM-R-008`, `MCRM-R-009`, `MCRM-R-010`, `MCRM-F-004`, `MCRM-F-005` | **Retired, not live requirements of this document.** Each moved to `SPEC-srp-analytic` in v2.0 (`MCRM-F-005` alone was retired outright); the Retired identifiers table below names where. Listed here only so the tool's own denominator — which counts every `-R-`/`-F-` row the Retired table's format necessarily defines — does not report them as uncovered live requirements; none is discharged by a test IN THIS document because none is a requirement OF this document any longer. |
| `MCRM-R-017` | **(v2.2)** States a SCOPE BOUNDARY (the energy-conservation guard does NOT cover `SphericalSurface`), not a behaviour to exercise — there is no constructor to attach a test to (`SphericalSurface` is a plain aggregate), and no consumer in this tree has built one, `MCRM-R-017`'s own text. |

---

## 9. Provenance obligations

- **MCRM-R-011.** `PROVENANCE.md` records the retrieval route for `RS14` (mediaTUM, no
  account, hash pinned), that `RHS12` is reprinted in full within it, and — distinct from
  `SPEC-srp-analytic`'s own provenance entry, which keeps the force laws' derivation
  story — this version's relocation: what `v1.0` got wrong, what moved, and the
  `FlatSurface` redesign this review's own scrutiny found before any test found it.
- **v2.1 (§4.3).** `PROVENANCE.md`'s own L4 step-5 entry records this amendment alongside
  `SPEC-photon-pressure`'s own — the schema-level change (this document) and the
  force-law-level need that drove it (that one) are two different claims, kept as such
  rather than conflated (`PHPR-R-003`'s own bit-identity note names the same discipline).
- **v2.2 (§4.4).** `PROVENANCE.md`'s own L5 step-4 entry records the full finding this guard
  responds to: SPOT-5's own Appendix-1 worked example found NOT to conserve energy, the
  real kernel's own output quantifiably disagreeing with the example's own printed answer as
  a direct consequence, and the manager's own instruction that this become an ENFORCED
  schema property rather than a documented gap — plus the two pre-existing tests
  (`PHPR-A-011`/`PHPR-A-016`, in `erp`/`srp_analytic`) whose own synthetic, non-conserving
  test triples this guard newly refuses, fixed by choosing conserving values instead (the
  qualitative claims either test makes do not depend on the exact triple, checked before the
  fix, not merely assumed).

---

## Retired identifiers

Kept so that a reference in a changelog, a review note or an earlier draft resolves rather
than dangling — the same discipline `SPEC-eop.md` established for exactly this situation.
Every row below moved to `SPEC-srp-analytic` in v2.0 unless its "why" says otherwise.

| id | retired | replaced by | why |
|---|---|---|---|
| `MCRM-R-005` | v2.0 | `SRPA-R-001` | The flat-surface force law. Physics, not schema. |
| `MCRM-R-006` | v2.0 | `SRPA-R-002` | The flat-surface law's normal-incidence case. |
| `MCRM-R-007` | v2.0 | `SRPA-R-003` | The spherical-surface force law. |
| `MCRM-R-008` | v2.0 | `SRPA-R-004` | The two laws' coefficient comparison — corrected in its new home (§4's own note). |
| `MCRM-R-009` | v2.0 | `SRPA-R-005` | A sphere's attitude-independence, a property of the force law. |
| `MCRM-R-010` | v2.0 | `SRPA-R-006` | `srp_force` itself. |
| `MCRM-F-004` | v2.0 | `SRPA-F-001` | `srp_force`'s empty-macromodel refusal; the function moved with it. |
| `MCRM-F-005` | v2.0 | *(none)* | `srp_force`'s non-unit-sun-direction refusal. Not carried forward: `BodyDirection`'s own type already makes that state unreachable (`MCRM-F-002`), so a second check at the call boundary guarded nothing a caller could actually produce. |
| `MCRM-P-1` | v2.0 | `SRPA-P-1` | The sphere coefficient's Monte Carlo corroboration. |
| `MCRM-P-2` | v2.0 | `SRPA-P-2` | The force laws' own precision statement. |
| `MCRM-A-001` | v2.0 | `SRPA-A-001` | The cannonball round-trip *against a force*. `MCRM-A-011` (§8) is this schema's own, force-free replacement. |
| `MCRM-A-002` | v2.0 | `SRPA-A-002` | ρ-invariance of a sphere's force. |
| `MCRM-A-003` | v2.0 | `SRPA-A-003` | Attitude-independence of a sphere's force. |
| `MCRM-A-004` | v2.0 | `SRPA-A-004` | The flat-plate degenerate force case. |
| `MCRM-A-005` | v2.0 | `SRPA-A-005` | **Corrected**, not only moved: v1.0 tested only ρ = 0, where the gap the plan warns about is smallest; `SRPA-A-005` adds ρ = 0.9. |
| `MCRM-A-008` | v2.0 | `SRPA-A-006` | `srp_force`'s empty-macromodel test. |
| `MCRM-A-009` | v2.0 | `SRPA-A-008` | The constraint-8 discharge for `srp_force`. |
| `MCRM-A-010` | v2.0 | `SRPA-A-007` | The flat surface's cos θ < 0 domain test. |

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `MCRM-Q-001` | **`centre_of_mass_m`'s type.** §5 states it as a `Cited` 3-vector that is *not* required to be unit length (unlike `BodyDirection`), since a centre of mass is a position offset in metres, not a direction. It is written here as a distinct, weaker-constrained type rather than reusing `BodyDirection` with its unit-length refusal disabled, to avoid one type silently meaning two different things depending on context (plan §5 constraint 10). Naming it explicitly for review rather than leaving the choice implicit in the code. |
| `MCRM-Q-002` | **Whether `Cited<T>`'s citation string should be structured** (a manifest-entry id, a page/table reference, a free-form note) rather than a bare string, matching how `Provenance` structs elsewhere in this tree (`modules/gravity/include/odl/gravity/field.hpp`) name a specific file and hash. Left as a free-form string for this step because L5 — the actual populator — is better placed to know what structure its real citations need (a Table 1 row citing `RS14` by page differs in kind from a citation to a manifest-pinned SINEX file), and over-structuring it now risks shaping L5's schema before L5 exists, the exact trap this step's own re-siting from L5 was meant to avoid. |
