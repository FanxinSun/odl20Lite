# SPEC-photon-pressure — solar and Earth radiation pressure, over one shared kernel

| | |
|---|---|
| **Spec ID** | `PHPR` |
| **Status** | **draft** 2026-09-23, for review (v1.1, 2026-09-24: `flat_force`'s own energy-conservation scope stated beside `PHPR-R-010`'s formula, per L5 step 4's own SPOT-5 finding, `SPEC-srp-analytic.md` `SRPA-A-014`/`A-015`) |
| **Version** | 1.1 |
| **Date** | 2026-09-24 |
| **Layer** | L4 `forces-analytic`, step 5 (`../plan/PLAN.md` §3.5, `../plan/subplan_L4/L4-5.md`) |
| **Depends on** | `core`, `time`, `eop`, `frames`, `ephemerides`, `shadow`, `macromodel`, `srp_analytic`, `dynamics` |
| **Depended on by** | L4's own exit gate (after step 7 — an arc fit with this layer's forces, unreachable without a working `dyn::Force` for SRP) |

**Derivation declaration (plan R1).** Written from the documents in §2, from `SPEC-macromodel.md`,
`SPEC-srp-analytic.md`, `SPEC-shadow.md`, `SPEC-ephemerides.md` and `SPEC-dynamics.md` (the specs
this module's interfaces are built against), and from no implementation of this module beyond what
this document itself describes.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

**Scope amendment, recorded because it changed what this step builds.** The plan named this step
`erp` alone. Drafting it found that L4 steps 1 and 3 built the eclipse shadow function and the SRP
force law and no step composed them into a force the propagator can use — the only `dyn::Force` in
the tree is `Drag`, and L4's exit gate cannot be reached without an SRP plugin. `../plan/subplan_L4/L4-5.md`
(amended 2026-09-23) now carries the corrected scope: this step builds **both** plugins over
**one** photon-pressure kernel, SRP first because a single source direction gives closed-form
checks of the plumbing before ERP's integral over the visible Earth.

---

## 1. Purpose and scope

**In scope.**

- The **kernel refactor**: `srp_analytic`'s force law (`SRPA-R-001`–`R-004`) generalised to take
  irradiance as a parameter, unit-typed; to distinguish the irradiance **source** direction from
  the **Sun** direction a sun-tracking surface's normal still follows regardless of which body's
  radiation is being computed; to carry a **back face** on `FlatSurface`, so a sun-tracking panel
  illuminated from behind (the day-side case, where most of ERP's own signal is) is not silently
  dropped; and to carry a first-order **aberration** term, the one real velocity dependence this
  whole force family has (§4.1).
- **`SPEC-macromodel`'s `FlatSurface`/`SphericalSurface`** gain an optional back face and an
  optional infrared triple, additive (§4.1) — the same discipline `DYN-Q-001` established, applied
  to the schema this force family's first real (non-cannonball) consumer needs and the trivial one
  it was frozen against could not reveal.
- The **nominal attitude** law — Earth-pointing, Sun-tracking-panel — as a pure function of
  position and the Sun's direction, computing the `BodyDirection`s both plugins need and which
  `SRPA-Q-001` named as "a different module's job" (§4.2).
- The **SRP plugin** (`odl::srp::Srp`, a `dyn::Force`): Sun position from `ephemerides`, the
  distance-scaled irradiance, the eclipse shadow factor, nominal attitude, the kernel (§4.3).
- The **ERP plugin** (`odl::erp::Erp`, a `dyn::Force`): the same kernel summed over the visible
  cap of the Earth, with reflected and emitted irradiance per cap element, constant albedo and
  emissivity in this step (§4.4–4.5).
- `dyn::ForceEvaluation::provenance` becoming a collection (§9), approved on `DYN-Q-001`'s terms
  for a reason independent of this step's own data (below).

**Out of scope, named rather than silently absent.**

- **Knocke's own zonal/seasonal albedo and emissivity model.** Both accessible sources agree
  Knocke et al. (1988) is not constant — second-degree zonal, with an annual term — but the paper
  itself could not be obtained in full text (§2), so its coefficients have no citable source this
  tree can use. Carried as the next refinement (§10), not built here. Constant 0.3/0.7 is used
  instead (§4.4), and is *also* the better gate order: the infrared identity this step gates on is
  exact only for uniform emission (§6), so the strongest available check belongs to the constant
  case, before generalising to whatever loses it.
- **A gridded (CERES-style) albedo/emissivity product.** A new external data product — its own
  manifest entry, licence search and loader — not something either paper prints.
- **Empirical bias models** (Y-bias, ECOM-style terms). Not photon pressure; a later step's own
  scope.
- **`perspective.hpp`'s ellipsoidal, atmosphere-aware shadow (PPM/PPM_atm).** `conical.hpp`'s SECM
  is used (§4.3) because it shares GCRS with everything else this step touches; the PPM's
  ellipsoidal refinement is available and is a later precision step, not required to close L4's
  exit gate.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `RS09` | Rodríguez-Solano, C. J. | *Impact of albedo modelling on GPS orbits* (Master's thesis) | TU München, 2009 | `https://mediatum.ub.tum.de/doc/1083571/1083571.pdf` | **primary** | the from-first-principles derivation of the Earth radiation model (Ch. 2, Eq. 2.1–2.50) and the box-wing satellite force law applied to it (Ch. 3) — the equations neither `RS12` nor `RHS12` print |
| `RS12` | Rodríguez-Solano, C. J., Hugentobler, U., Steigenberger, P., Lutz, S. | *Impact of Earth radiation pressure on GPS position estimates* | J. Geodesy 86(5):309–317, 2012 | reprinted in full in `rodriguez-solano-2014-dissertation` (already pinned, L4 steps 2–3) | secondary | states the model **is** Knocke's ("the mathematical formulation ... is the same as the one proposed by Knocke et al. (1988)"); Fig. 2's structural claims used as a qualitative check (§8); prints no force-level number for a stated geometry |
| `RHS12` | Rodríguez-Solano, C. J., Hugentobler, U., Steigenberger, P. | *Adjustable box-wing model for solar radiation pressure impacting GPS satellites* | Adv. Space Res. 49(7):1113–1128, 2012 | `rodriguez-solano-2014-dissertation`, as pinned for L4 step 3 | secondary | `srp_analytic`'s own existing normative source (`SPEC-srp-analytic` §2); unchanged by this spec |
| `KRT88` | Knocke, P. C., Ries, J. C., Tapley, B. D. | *Earth radiation pressure effects on satellites* | AIAA/AAS Astrodynamics Conf., 1988, AIAA-88-4292 | **unreachable** | — | the paper both `RS09` and `RS12` cite as the source of the mathematical formulation. Search recorded: NASA NTRS (citation 19880063192) lists the record with no download; AIAA's own copy (`doi:10.2514/6.1988-4292`) requires institutional access this tree does not have. No account, request form or paywall was crossed, per the standing prohibition. |
| `BLS79` | Burns, J. A., Lamy, P. L., Soter, S. | *Radiation forces on small particles in the solar system* | Icarus 40:1–48, 1979 | `https://www-users.cse.umn.edu/~gehrz001/ASTRO_2001_Spring_Semester_2020/10_Burns_Lamy_Soter.pdf` | **primary** | the classic first-order (*v*/*c*) statement of aberration and Poynting–Robertson drag (§4.1), Eq. (5): *m***v̇** = (*SA*/*c*)*Q*_pr[(1 − *ṙ*/*c*)**Ŝ** − **v**/*c*], with *Q*_pr = 1 for a perfect absorber and *Q*_pr = *Q*_abs + *Q*_sca for an isotropic scatterer — the same value, 1, since *Q*_abs = 0, *Q*_sca = 1 for a lossless scatterer; a particle that perfectly backscatters (the near-normal-incidence specular case a flat mirror approaches) has **twice** that drag |
| `KVP10` | Kezerashvili, R. Ya., Vázquez-Poritz, J. F. | *The Poynting–Robertson Effect on Solar Sails* | arXiv:1012.3574, 2010 | `https://arxiv.org/pdf/1012.3574` | secondary | an independent derivation of the aberration force for a solar sail directly facing the Sun (Eq. 4–6) — cross-checks `BLS79`'s own normal-incidence case for a partially-reflective surface, but does not reach oblique incidence either, so it does not close `PHPR-R-010`'s own flat-plate bound any tighter |

`RS09`, `BLS79` and `KVP10` are pinned as `literature`-kind manifest entries
(`rodriguez-solano-2009-masters-thesis`, `burns-lamy-soter-1979-radiation-forces`,
`kezerashvili-vazquez-poritz-2010-pr-solar-sails`). `RS09`'s terms search found nothing in the
PDF's own text and could not complete on the mediaTUM landing page, now served behind an automated
bot challenge — recorded as a narrower finding than "searched and found nothing," not conflated
with it. `BLS79`'s terms search **found** an explicit statement ("Copyright © 1979 by Academic
Press, Inc.", printed on the paper's own first page) rather than finding nothing — the same
footing `sengers-2014-drag-coefficient` already established. `KVP10`'s own terms are the same
arXiv distribution licence already found and recorded in full for `sengers-2014-drag-coefficient`,
not re-fetched as a separate finding.

**What the box-wing literature says about panel backs — and bands, which the citation's own
subscript already said.** `RS09` Table 3.1's own title is "Main dimensions and optical parameters
(**visible and infrared**) for GPS satellites," with columns *µ*_VIS, *ν*_VIS, *µ*_IR, *ν*_IR — a
front/back distinction **and** a visible/infrared one, both real, both measured, and neither
optional to model correctly: the panel front is *µ* 0.85 / *ν* 0.23 visible but *µ* 0.50 /
*ν* 0.20 infrared, the back computed as *ν* = 1 − *ε* from its own emissivity ("in general the
back of the solar panels absorb most of the radiation"), *ε* = 0.80 in the infrared specifically.
Reflected (albedo) light is visible-band; the Earth's own emission is thermal, infrared-band — one
triple per face would put visible properties on the infrared term, e.g. a panel front's
specularity read as 0.85 where the source states 0.50 for that band. None of Table 3.1's own
numbers are this tree's data (LightSail-2 is not a GPS satellite); it is evidence that both axes
— face and band — are real, measured, and different enough to matter for a real spacecraft, not a
refinement this tree invents ahead of any need.

**The two exact identities this spec gates on (§6) are not cited to any of the above.** They are
derived from radiative-transfer first principles — flux conservation through concentric spheres
for the infrared case, and the phase-integral of a Lambertian sphere's reflected radiance for the
albedo case — independently of `RS09`'s own equations, per plan rule 8's converse: checking an
implementation against a paper's derivation checks transcription, and an error in the paper would
be reproduced faithfully. Where the independently-derived far-field albedo form coincides with
`RS09` Eq. (2.40)'s ERM-A, that is a cross-check the two routes give each other, not evidence for
either — recorded in `PROVENANCE.md` when measured (§9).

---

## 3. Definitions and conventions

Inherits, entire, and redefines none of:

- `SPEC-frames` §3's frame conventions and `core/units.hpp`'s named km/metre crossing.
- `SPEC-dynamics` §3's `Force`/`ForceEvaluation`/`StateJacobian`/`ParameterJacobian` surface.
  `ForceEvaluation::provenance` becomes `std::vector<Provenance>` (§9) — `SPEC-dynamics`'s own
  amendment, ruled under `DYN-Q-001`'s terms, not a private extension this spec grants itself.
- `SPEC-macromodel` §3's `Cited<T>`, `BodyDirection`, `NormalMode`, `FlatSurface`,
  `SphericalSurface`, `Macromodel` — this module is a consumer.
- `SPEC-srp-analytic` §3–4's force law (`SRPA-R-001`–`R-006`) and its own conventions — amended,
  not redefined, by §4.1 below.
- `SPEC-shadow` §3's `conical`/`ShadowResult`/`State` — a consumer, via the SECM (§1).
- `SPEC-ephemerides` §3's `Ephemeris`/`Body`/`geocentric_state` — a consumer.

**This module's own conventions, stated once here:**

- ψ, the **Sun–Earth–satellite angle**: cos ψ = **r̂**_sat · **ŝ**, both geocentric unit vectors
  (`RS09` Eq. 2.38). This is Earth radiation's own governing angle, distinct from *α*, the phase
  angle a distant observer of the reflecting sphere would use — the two coincide only in the exact
  far-field limit this spec's albedo identity is stated in (§6); the cap integral (§4.4) is
  parameterised in ψ throughout, not *α*, since ψ is well-defined at every altitude and *α* is a
  far-field construct.
- **Irradiance carries its unit in its type** (plan §5 constraint 10): `macromodel::IrradianceWPerM2`,
  constructed only through a validating free function (refusing negative or non-finite values), the
  same discipline `BodyDirection` already applies to a unit vector. A caller who wants a bare
  double has to say so by calling `.watts_per_m2()`, the same friction `BodyDirection::vec()`
  already imposes and for the same reason.
- **Source direction vs. Sun direction are two different `BodyDirection`s**, never one parameter
  doing both jobs (§4.1). For SRP's own call they happen to be the same vector; the kernel does not
  assume this, so ERP's call — where they are not the same vector — is not a special case the
  kernel has to detect, only a different pair of arguments.

---

## 4. Required behaviour

### 4.1 The kernel: `srp_analytic::photon_force`

`SRPA-R-001`/`R-003`'s force laws are generalised into one entry point:

```
photon_force(model: Macromodel, irradiance: IrradianceWPerM2, band: Band,
             source_direction_body: BodyDirection, sun_direction_body: BodyDirection,
             velocity_relative_to_source_body_m_per_s: Vec3)
  -> Result<Vec3, SrpError>
```

- **PHPR-R-001.** Irradiance is a caller-supplied, unit-typed parameter (`macromodel::IrradianceWPerM2`,
  §3), not `srp_force`'s file-local `kSolarIrradianceAt1AuWPerM2 = 1367.0`. Required for SRP
  itself, not only for ERP's reuse: the Sun–spacecraft distance varies by +3.43 % / −3.26 % over
  the year (Earth's own orbital eccentricity), which a constant at 1 au cannot represent. For the
  tree's own target spacecraft (LightSail-2, a sail) this is a seasonal ±5 × 10⁻⁷ m/s² term that a
  single fitted *C*_R cannot absorb over a long arc — it is periodic at the wrong frequency for a
  constant to hide.
- **PHPR-R-002.** The irradiance **source** direction and the **Sun** direction are two separate
  `BodyDirection` parameters, found necessary while designing this refactor rather than merely
  convenient. `srp_force`'s own `flat_force` sets a sun-tracking surface's normal
  (`NormalMode::sun_pointing`) equal to `e_D`, the single direction it takes — correct for SRP,
  where the irradiance source **is** the Sun, and wrong in general: a solar panel tracks the
  **Sun** regardless of which body's radiation is currently being computed. ERP's own call needs
  `source_direction_body` = Earth's direction (the illumination geometry, `cos θ`, and `e_D` in
  every force term) while `sun_direction_body` (used **only** to resolve `NormalMode::sun_pointing`)
  stays the Sun's. `SphericalSurface`s ignore `sun_direction_body` entirely, having no normal to
  resolve (`MCRM-R-003`'s own reasoning, one level up).
- **PHPR-R-003.** `srp_force(model, sun_direction_body)` becomes exactly
  `photon_force(model, kSolarIrradianceAt1AuWPerM2, Band::visible, sun_direction_body, sun_direction_body, Vec3{0,0,0})`
  — the same constant, the visible band (what every triple meant before `PHPR-R-004a`), the same
  single direction supplied twice, zero velocity so `PHPR-R-010`'s aberration term vanishes
  identically (it is proportional to velocity, not merely small at zero), so every existing call
  computes the identical arithmetic in the identical order. **Two claims, not one, and only one
  test proves the claim that matters.** `SPEC-srp-analytic` §8's existing 92 636 assertions
  passing unchanged proves agreement WITHIN THEIR OWN TOLERANCES (1 × 10⁻¹² relative and similar)
  — which a refactor's own characteristic hazard, reordering floating-point operations, would
  still satisfy while changing the last bit. And asserting `srp_force`'s output equals
  `photon_force` called with these exact arguments proves nothing at all once `srp_force` **is**
  that call — a comparison of the wrapper against itself, which cannot fail regardless of what the
  underlying arithmetic does. `PHPR-A-001` is the test that actually proves bit-identity: `srp_force`
  compared, with exact equality, against a golden file captured from the **pre-refactor source
  directly** (a worktree built at the last commit before this file's own generalisation,
  `srp_force_golden.hpp`'s own header names it) — a value only the old code, not this file's own
  new code calling itself, can produce. `srp_force` itself remains, as a thin wrapper, so every
  existing caller and every existing test in `SPEC-srp-analytic` §8 is
  unchanged.
- **PHPR-R-004a.** One amendment, two axes — face and band — because `RS09` Table 3.1 (§2) showed
  both are real and neither is optional to model correctly. `FlatSurface` gains an **optional back
  face** (itself visible-required/infrared-optional, below), and every face gains an **optional
  infrared triple** falling back to its own visible one; `SphericalSurface` gains the same optional
  infrared triple. `OpticalTriple` (`absorptivity`/`specular`/`diffuse`, all `Cited<double>`,
  all-or-nothing — `MCRM-R-002`'s own pairing discipline, extended) is the unit both axes are built
  from, so "all or nothing" is a type the compiler enforces, not an invariant documented and
  trusted. A `SPEC-macromodel` amendment, additive, the same footing as `DYN-R-051`/`DYN-R-052`
  below.

  **Why the back face is needed, not merely wanted.** `PHPR-R-002`'s own fix lets a sun-tracking
  panel's normal track the Sun while a different body's radiation reaches it — but when that other
  body is *behind* the panel (`cos θ < 0`), today's kernel treats the surface as unlit, exactly as
  it must for a surface with only a front. Over the Earth's day side — where reflected albedo
  comes from — Earth is on the panel's own **lit** side, opposite the Sun, most of the time; a
  sun-tracking panel has no reason to face the source of a *different* body's radiation. A
  one-sided panel therefore drops most of its own contribution to ERP, and panels are most of a
  GNSS-class spacecraft's area. Absent, a surface is one-sided as today (a bus face whose back is
  inside the body — nothing changes for it). Present, `photon_force` evaluates the back face, with
  the **reversed** normal and the back's own (band-resolved) triple, whenever the source is behind
  the front (`cos θ < 0` on the front normal).

  **Why the band matters, not merely exists.** Reflected sunlight (SRP, and ERP's own albedo
  contribution) is visible-band; the Earth's own emission (ERP's infrared contribution) is thermal.
  `RS09` Table 3.1's own printed values put real daylight between the two for a real spacecraft
  (front: *µ* 0.85/*ν* 0.23 visible vs. *µ* 0.50/*ν* 0.20 infrared) — one triple for both bands
  would read a panel's own infrared response off its visible numbers. Absent an infrared triple,
  the visible one is used for both (a stated approximation, not silently assumed equal); present,
  each cap cell's reflected contribution (§4.4) routes `Band::visible` and its emitted contribution
  routes `Band::infrared`.

  Both axes are inert for SRP: the Sun never lights a sun-pointing panel's own back, and SRP is
  always `Band::visible` (`PHPR-R-003`), so bit-identity still holds exactly. `RS09` Table 3.1 (§2)
  is evidence real, precedented spacecraft carry measurably different values on both axes, not a
  reason to assume one triple suffices for either.
- **PHPR-R-010.** A first-order aberration term, `BLS79` Eq. (5)'s own form
  `m**v̇** = (SA/c)Q_pr[(1 − ṙ/c)**Ŝ** − **v**/c]`, *ṙ* = **v**·**Ŝ**, **Ŝ** the unit vector ALONG
  THE INCIDENT BEAM (source to spacecraft) — this kernel's own `e_D` points the other way
  (spacecraft to source, so the steady force `−coeff·e_D` pushes away from the source, as it must),
  so **Ŝ** = −`e_D` and *ṙ* = −(**v**·`e_D`), giving
  `F = −(SA/c)·Q_pr·[(1 + (v·e_D)/c)·e_D + v/c]` — at **v** = 0 exactly the pre-existing steady
  force (`PHPR-R-003`'s bit-identity), with a Doppler factor on the steady term and a drag term
  opposing **v** regardless of `e_D`, both the behaviour BLS79's own physics requires.
  **`v` here is velocity relative to the irradiance SOURCE, not the spacecraft's frame velocity in
  general** — named `velocity_relative_to_source_body_m_per_s` in the kernel's own signature (plan
  §5 constraint 10) so the next caller cannot pass the wrong one without writing the wrong name.
  Applied through each surface's already-established radiation-pressure coefficient rather than
  re-derived as a new scalar: for a `SphericalSurface`, whose existing coefficient `(1 + 4δ/9)`
  (`SRPA-R-003`/`R-005` — *ρ* already proven absent, `SRPA-A-002`) **is** *Q*_pr in `BLS79`'s own
  sense exactly (an isotropic scatterer's *Q*_pr = *Q*_abs + *Q*_sca, which a diffusely-scattering
  sphere is), the substitution above is exact, not approximate. For a `FlatSurface`, whose two
  directional coefficients (`e_D`'s `(1−ρ)`, `e_N`'s `2(δ/3+ρcosθ)` — `SRPA-R-001`) have no single
  `BLS79`-shaped *Q*_pr, the same Doppler factor is applied to the whole steady bracket (the
  incoming photon flux is enhanced by the same factor regardless of which term the momentum ends
  up in) and one combined drag term uses the two coefficients' own sum as its effective *Q*_pr —
  stated as the natural, minimal generalisation, not an independent flat-plate relativistic
  rederivation. It is **exact** at the one geometry this tree's own sun-tracking panels always
  have — normal incidence (cos θ = 1) — where a perfectly specular reflector's effective *Q*_pr
  reduces to exactly 2, `BLS79`'s own "perfectly backscatters" case: twice the drag of absorption,
  not the same, because a mirror reverses the photon's momentum rather than merely stopping it —
  the case this tree's own target spacecraft (a near-specular sail near normal incidence) needs,
  named explicitly rather than left to whichever case the formula happened to fall into. Where it
  is not exact (oblique incidence on a body-fixed surface), it is bounded by the term's own size,
  `PHPR-P-5`'s ~1.5 × 10⁻⁹ m/s² — `KRT88` covers only normal incidence too (`RS09`/`RS12`'s own
  scope), and `KVP10`'s independent solar-sail derivation, checked for the general oblique case,
  turns out to cover the same normal-incidence geometry `BLS79` already does exactly, not a wider
  one, so no accessible source closes this bound tighter than stating it.

  **THE KERNEL'S SCOPE: `flat_force`'s own `(1−ρ)` form is the general, three-coefficient
  radiation-momentum law — `(α+δ)*e_D + 2*(δ/3+ρ·cosθ)*e_N` — ONLY where `α+ρ+δ=1` (energy
  conservation).** The two are algebraically identical exactly there (`α+δ = 1−ρ` is that
  identity restated) and genuinely DIFFERENT otherwise — this is a stated SCOPE of the kernel,
  not a rounding-level approximation, found and quantified at L5 step 4 (`SPEC-srp-analytic.md`
  `SRPA-A-014`/`A-015`): a CNES technical note's own worked SRP example (Appendix 1,
  `SALP-NT-BORD-OP-16137-CN`) for SPOT-5's bus does NOT conserve energy (its own six rows sum to
  0.499–0.912) and the kernel's own `(1−ρ)` output, fed that data directly, disagrees with the
  example's own printed answer by roughly the row's own energy gap (`1−α−ρ−δ`) — at the
  clearest single-face case, ~49%. Every macromodel this tree has built through L5 step 4
  conserves energy exactly or to floating-point rounding (`SPCR-A-001` and its own successors);
  SPOT-5's own table is the first, and so far only, example this tree has seen OUTSIDE that
  scope. `SPEC-macromodel.md`'s own `MCRM-R-016`/`MCRM-F-007` now REFUSES a surface whose
  optics sum to anything more than 1% away from 1, IN EITHER DIRECTION — an over-unity triple
  (e.g. summing to 1.05) is exactly as non-physical, and exactly as far outside this formula's
  own shape, as an under-unity one, and is refused on the same terms (`MCRM-A-016` shows this
  directly, not only the under-unity SPOT-5 case) — so a future non-conserving macromodel cannot
  reach this kernel un-refused, from either side, and the scope this paragraph states is
  therefore enforced, not merely documented.

  **The residual WITHIN the accepted 1% band, stated as a number, not left implicit.** The
  kernel's own `e_D`-direction coefficient is `(1-ρ)`; the general formula's is `(α+δ)`. Their
  difference is EXACTLY `(1-ρ)-(α+δ) = 1-α-ρ-δ = -(sum-1)` — an EXACT algebraic identity, not an
  approximation, so a triple accepted by `MCRM-F-007` (within 1% of conserving) has its own
  `e_D`-coefficient residual bounded by that SAME 1%, absolute, directly. The live case in this
  tree: Jason-2's/Jason-3's own infrared rows, the only accepted triples found so far that do not
  conserve exactly — their own sums run 0.998–1.002 (`SPEC-spacecraft.md` `SPCR-P-5`), so their
  own `e_D`-coefficient residual is bounded by 0.2% absolute, well inside the 1% band and far
  below the ~49% SPOT-5 itself would have produced were it not refused outright.

  **The Jacobian this term implies is ANALYTIC, not a finite difference of the kernel.** Per surface,
  the substitution above is exactly AFFINE in **v** — every direction and coefficient it uses
  (`e_D`, `e_N`, the steady-force directions and coefficients) is a function of geometry and optical
  properties alone, never of velocity — so *d*F/*d*v is a CONSTANT matrix, exact at every velocity,
  not a local linearisation: `−(prefactor·cos θ/c)·[steady_direction ⊗ e_D + drag_Q_pr·I]` for a
  flat surface, `−(prefactor·Q_pr/c)·[e_D ⊗ e_D + I]` for a sphere (⊗ the outer product, I the 3×3
  identity), both derived here and verified, before either existed in production code, against an
  independent central finite difference in a standalone numerical check (agreement to ~1e-19,
  i.e. exact to floating-point noise). `photon_force_and_velocity_jacobian` returns this alongside
  the force itself, summed per surface the same way `photon_force` sums the force (front, back,
  spherical); `photon_force` is a thin extraction of its own `.force` member, not a parallel
  computation, so `PHPR-R-003`'s bit-identity carries over mechanically. This exists because a
  Jacobian checked only against another finite difference of the same quantity is `PHPR-A-006`'s own
  earlier defect (plan §4 rule 5's tautology, caught by the manager reading `Srp::accel`, not by a
  test failing) — with an analytic production value, that test finally checks something.

This is `DYN-Q-001`'s footing throughout: additive (`srp_force` kept, not removed; `FlatSurface`'s
new fields optional), nothing existing changes in observable behaviour (proved by `PHPR-R-003`'s
equality, not merely claimed), re-derived from an actual, stated need in every case (the seasonal
term, the day-side back-face loss, the real aberration magnitude) rather than speculative
generality. It touches L4 step 3's closed module (`srp_analytic`) and `SPEC-macromodel`'s own
closed schema, both recorded here and in `PROVENANCE.md` (§9) the same way `DYN-R-051` was.

### 4.2 Nominal attitude

Neither plugin has an attitude *history* to consult — there is no attitude module in this tree
(`SRPA-Q-001`'s own open item) and none is built here either. What both plugins need is smaller:
the standard nadir-pointing, Sun-tracking-panel law used throughout `RS09`/`RS12`/`RHS12`
("ensuring that the navigation antennas always point to the geocenter and that the solar panels
always point to the Sun," `RS09` §3.2.1) is a **pure function of instantaneous geometry**, not a
state that evolves — no separate attitude history to carry, only a rotation to compute at the
epoch each force evaluation already has.

- **PHPR-R-004.** `nominal_attitude(r_gcrs_m: Vec3, sun_direction_gcrs: BodyDirection) -> Frame`,
  a new module, `odl::attitude`, rather than kept private to either plugin — on its own merits, not
  a quoted rule: both `Srp` and `Erp` need it from the day this step opens (two real consumers, not
  a speculative one), and step 6 (`../plan/subplan_L4/L4-6.md`, "thrust-yaw") adds providers to this
  same module rather than rebuilding the law, so the shared home is load-bearing immediately, not
  merely tidy. **Ruled** (`PHPR-Q-002`): step 5 builds the **ideal nominal** yaw-steering law only;
  everything beyond it — noon/midnight turns, constellation-specific laws, antenna thrust — is step
  6's, adding to this module rather than replacing its law. The law: **ẑ_body = −r̂** (nadir),
  **ŷ_body = (ẑ_body × ŝ)/‖ẑ_body × ŝ‖** (the panel rotation axis, perpendicular to the Sun),
  **x̂_body = ŷ_body × ẑ_body**. Refuses (`ATTD-F-001`, this module's own id, forwarded unchanged
  by both plugins rather than relabelled — the same structured-cause discipline `DRAG-F-003`
  already established for a shared cause reached through more than one caller) when **ŝ** is
  within a stated tolerance of
  **ẑ_body** (Sun on the nadir axis — the panel axis is undefined, not merely small), rather than
  returning a degenerate frame silently.
- **The ideal law is not always a realisable one.** Near *β*₀ ≈ 0 (the Sun close to the orbit
  plane, `RS12`'s own noon/midnight-turn regime), the standard law's required yaw rate diverges as
  the spacecraft crosses noon or midnight — a real spacecraft's own reaction wheels cannot follow
  it, and every real constellation flies a modified law there instead (`RS12`'s own subject). Step 5
  computes **only** the ideal law (`PHPR-Q-002`, ruled above); at small *β*₀ its own output is the
  ideal model's attitude, not the flown spacecraft's, so `Srp`/`Erp` results in that regime describe
  the model, not the vehicle, until step 6 adds a real noon/midnight law. Not a refusal — the ideal
  law stays mathematically defined right through *β*₀ = 0 — a documented scope boundary instead.

### 4.3 The SRP plugin

- **PHPR-R-005.** `class Srp final : public dyn::Force`, the same shape `Drag` already takes
  (fixed physical properties bound at construction — the `Macromodel`, a `C_R`-shaped registered
  parameter if one is declared, an `Ephemeris` reference). Per evaluation, `accel()`:

  1. The Sun's GCRS **state** (position **and** velocity — the velocity was fetched and discarded
     before this correction) from `ephemerides::Ephemeris::geocentric_state(Body::Sun, t, leaps)`.
  2. The distance *r* = ‖**r**_sun,gcrs‖ and the irradiance 1367 × (`Ephemeris::kAstronomicalUnitKm`
     × 1000 / *r*)² — `PHPR-R-001`'s parameter, not `srp_force`'s fixed constant.
  3. **v**_rel,gcrs = **v**_sat,gcrs − **v**_sun,gcrs — velocity relative to the irradiance
     **source**, not the spacecraft's raw GCRS velocity. Aberration depends on velocity relative to
     the source; for sunlight that is dominated by Earth's own ≈ 29.8 km/s heliocentric motion (the
     Sun's own GCRS velocity, sign-reversed), not the spacecraft's ≈ 7.4 km/s geocentric orbital
     speed alone — using the latter drops the dominant, nearly-constant transverse part of the
     term, roughly a factor of four short of `PHPR-P-5`'s own sizing. Nominal attitude (§4.2)
     resolves the Sun's GCRS direction **and** this relative velocity into body frame —
     `sun_direction_body` and `velocity_relative_to_source_body_m_per_s`, the kernel's own named
     parameter (`PHPR-R-010`). The Jacobian is unaffected by this correction:
     *d*(**v**_rel)/*d*(**v**_sat) is the identity, subtracting the epoch's own constant
     **v**_sun,gcrs.
  4. The shadow factor *F*_s from `shadow::conical(sun_position, sat_position)` (§1: SECM, sharing
     GCRS with every other input here) — multiplying the kernel's returned force, **never** passed
     into the kernel itself.
  5. `photon_force(model, irradiance, Band::visible, sun_direction_body, sun_direction_body,
     velocity_relative_to_source_body_m_per_s)`, scaled by *F*_s, divided by mass, rotated
     GCRS-out.
- **PHPR-R-006.** The shadow factor belongs to this plugin, not the shared kernel: the Earth does
  not eclipse its own surface, and a spacecraft in the Earth's shadow still receives the Earth's
  infrared, so a shared "attenuate by shadow" step inside the kernel would be wrong for ERP by
  construction, not merely unnecessary for it.
- **The SECM's own limb-darkening dependency is inherited, not silent.** `shadow::conical` assumes
  a uniformly bright solar disc (`SHDW-R-031`'s own argument); the true, limb-darkened Sun makes
  *F*_s swing by up to 1.97 × 10⁻² within one eclipse passage, of which about 99.9 % cancels across
  a full passage away from the eclipse cutoff. `SPEC-shadow`'s own `SHDW-Q-005` (limb darkening)
  remains open; this plugin's own dependence on that gap is stated here so it is visible on the
  force that actually uses the shadow function, not only in `SPEC-shadow` itself.
- **PHPR-R-007.** The velocity Jacobian is **not** a declaration without a term behind it
  (`DYN-Q-002`'s own defect, sign reversed): `PHPR-R-010` puts a real, computed aberration term in
  the force (§4.1), so `Srp::accel` returns `StateJacobian::with_velocity`, its Jacobian **derived
  from that term** — analytically where `PHPR-R-010`'s own substitution is closed-form, verified
  against a real finite difference of `Srp::accel` regardless (`PHPR-A-006`). `dyn::Force`'s own
  header names this force by name for exactly this reason: aberration is a real, altitude-independent
  dependency (`SPEC-dynamics` DYN-P-3/P-4), and a `with_velocity` block that is actually zero would
  be a declaration this force does not honour — the same defect in the opposite direction from
  declaring `no_velocity_dependence` on a force that does depend on velocity.

### 4.4 The ERP cap integral

- **PHPR-R-008.** For a spacecraft at geocentric position **r** (‖**r**‖ = *r* > *R*_E, refusing
  otherwise — `PHPR-F-005`), the visible cap is every point on the Earth's surface with
  **r̂** · **n̂** ≥ *R*_E/*r* (`RS09` Eq. 2.24's own visibility condition, restated as the cap's
  defining inequality) — equivalently, within angular radius *β* = arccos(*R*_E/*r*) of the
  sub-satellite point (`PHPR-P-4`'s own *β*). Partitioned in **nadir-centred** coordinates: colatitude
  *χ* from the sub-satellite direction **r̂**, 0 to *β* (**not** RS09's own literal (θ,φ) about an
  axis perpendicular to the satellite–Earth–Sun plane, and not the wider [0,π] a naive
  Earth-centred grid would need — `erp.cpp`'s own header comment derives why the cap's own boundary
  becomes the domain's own edge this way, making the integrand smooth with a simple zero there
  rather than gated by an indicator function inside a larger domain: the fix for `PHPR-P-1`'s own
  discriminator, found unusable in the first grid tried and diagnosed by reading that code, not
  merely by its symptom). Each cell's reflected and emitted irradiance contribution at the satellite
  is `RS09` Eq. (2.24)/(2.31) (reflected/emitted, per surface element), unchanged by the
  reparameterisation. Each cell's contribution is summed through `photon_force` (§4.1), with
  `source_direction_body` the cell's own direction and `sun_direction_body` the Sun's — the two
  differ in general, which is exactly `PHPR-R-002`, and `PHPR-R-004a`'s back face is exactly why a
  sun-tracking panel still receives most cells on the Earth's day side rather than reading as unlit.
  `velocity_relative_to_source_body_m_per_s` is the spacecraft's own GCRS velocity, unchanged
  (`PHPR-R-010`): the source is the Earth, and GCRS is geocentric by definition, so no subtraction is
  needed the way `PHPR-R-005`'s Sun case requires.
- **The albedo term's own terminator still staircases this grid** (`PHPR-Q-004`, open): *χ* alone
  fixes the visibility boundary exactly, but the Sun-lit/night boundary (cos *γ* = 0, gating the
  reflected term only) is a curve in (*χ*, azimuth) with no reason to align with either grid axis in
  general. `PHPR-A-007` isolates the emitted term alone (no cos *γ* gate at all) specifically because
  it is not subject to this; no convergence-order claim is made here for the reflected term.
- **Band routing, not one triple for two sources in two bands.** Reflected (albedo) light is
  sunlight, `Band::visible`; the Earth's own emission is thermal, `Band::infrared` — `RS09`
  Table 3.1's own title ("optical parameters (visible **and infrared**)") and its printed values
  (GPS panel front: *µ* 0.85/*ν* 0.23 visible, *µ* 0.50/*ν* 0.20 infrared) are a real, measured
  case where the two bands differ enough to matter, not a refinement. Each cap cell passes its own
  band to `photon_force` — the reflected contribution `Band::visible`, the emitted contribution
  `Band::infrared` — through `PHPR-R-004a`'s own routing, not a second mechanism this module
  invents.
- **PHPR-R-009.** Albedo *α* and emissivity *ε* are **functions of position and time** in the
  interface, constant 0.3 and 0.7 in this step (both `RS09` and the independent DORIS/IDS-workshop
  source state the same two numbers), so `KRT88`'s own zonal/seasonal model, or a later gridded
  product, plugs in by supplying a different function without the cap integral itself changing.
- **Grid refinement is asserted by ratio, not merely by decreasing error** (§4 rule 7,
  `SPEC-srp-analytic`'s own tessellated-sphere precedent, and the same discriminator that closed L4
  step 4: a wrong law does not shrink when the grid does, only a truncation error does, and only
  the *ratio* under refinement tells the two apart). `PHPR-P-1` (§6) states the required order.

### 4.5 The ERP plugin

- **PHPR-R-011.** `class Erp final : public dyn::Force`, `Srp`'s own shape. Per evaluation: the
  cap integral (§4.4) at the spacecraft's own position and epoch, summed via `photon_force` with
  `velocity_relative_to_source_body_m_per_s` the spacecraft's own GCRS velocity, unchanged
  (`PHPR-R-008`'s own note — the source is the Earth, and GCRS already is relative to it), divided
  by mass, rotated GCRS-out. No shadow factor (`PHPR-R-006` — Earth's own radiation is not
  self-shadowed by the geometry this integral already restricts to the visible cap). Velocity
  dependence: named and bounded (`no_velocity_dependence`) unless `PHPR-P-2`'s own measurement
  during drafting shows it large enough to carry as `with_velocity` instead — Earth-relative
  aberration is a real term here too, smaller than SRP's by the ratio of Earth's own irradiance to
  the Sun's, sized rather than assumed negligible.

---

## 5. Interfaces, stated language-free

- `photon_force(Macromodel, IrradianceWPerM2, Band, source_direction_body: unit vector,
  sun_direction_body: unit vector, velocity_relative_to_source_body: m/s) -> force in newtons,
  body frame, or a refusal` — the parameter is named for what it is, not merely "velocity", so a
  caller cannot supply the wrong one without writing the wrong name.
- `srp_force(Macromodel, sun_direction_body) -> photon_force(…, kSolarIrradianceAt1Au,
  Band::visible, sun_direction_body, sun_direction_body, zero velocity)`, unchanged in signature
  and in every returned value.
- `nominal_attitude(position: GCRS metres, sun_direction: GCRS unit vector) -> a frame in which a
  GCRS vector can be expressed in body coordinates, or a refusal at the degenerate geometry`
- `FlatSurface`'s optional face/band structure: a required front triple (visible, what the type
  already held), an optional front-infrared triple falling back to visible, and an optional whole
  back face (itself visible-required/infrared-optional the same way), all independent of each
  other.
- `Srp`, `Erp`: both `dyn::Force`. Constructed with the fixed properties `Drag`'s own constructor
  pattern already establishes (a `Macromodel`, an `Ephemeris` reference, an optional registered
  parameter id for `C_R`/`C_ERP` if one is declared).

---

## 6. Precision

- **PHPR-P-1.** The cap integral's discretisation error, at fixed *r* and grid orientation, over
  successive halvings of Δ*χ* (and Δaz), must show the ratio the integration scheme's own order
  predicts (rectangle/midpoint rule: error ∝ *h*², ratio 4× per halving) — **measured, not
  asserted a priori**, following `DRAG-P-1`'s own precedent of a step size set by measurement.
  **Measured** (`PHPR-A-007`, the infrared identity, the one term not also subject to `PHPR-Q-004`'s
  own open terminator question): 4.05× then 4.01× at LEO (300 km), 4.00× then 4.00× at GNSS
  (~20 000 km) — the predicted 4× to within a few percent at every step, not merely a monotonic
  improvement. This is the SECOND grid tried: the first (a global colatitude/longitude grid over the
  whole sphere, gated to the cap rather than restricted to it) gave 8.5× then 154× at LEO and 16×
  then 2× at GNSS — monotonic but not at one order, an unusable discriminator, diagnosed as a
  "staircase" cap boundary and replaced by the nadir-centred grid `erp.cpp` now uses (§4.4), not
  patched in place.
- **PHPR-P-2.** ERP's neglected velocity-dependence bound (§4.5), sized against SRP's own
  aberration term by the ratio of typical Earth-irradiance to solar irradiance at the target
  altitude — measured during drafting, recorded here.
- **PHPR-P-3.** The two exact identities (§1, §2, restated precisely): a uniformly-emitting
  Lambertian sphere's net flux through any concentric sphere of radius *r* is exactly *M*(*R*_E/*r*)²,
  radial, at **every** altitude (not only far field) — so the cap integral on a `SphericalSurface`
  spacecraft, constant emissivity, must reproduce *C*_R·*A*·*M*·(*R*_E/*r*)²/(*c*·*m*) at LEO as
  well as at GNSS altitude, to the tolerance `PHPR-P-1`'s own converged grid achieves. A Lambert
  sphere's reflected irradiance tends, in the far field, to (2*α*_A/3)·*S*·(*R*_E/*r*)²·Φ(α)
  (*α*_A the Bond albedo, `PHPR-R-009`'s own constant 0.3 in this step — **corrected here**:
  an earlier draft of this row wrote *A*_E, Earth's disc area (an *m*², dimensionally wrong in an
  irradiance formula), where it meant the albedo; caught while implementing `PHPR-A-008`, not
  before, plan rule 5's own point about a claim only being tested once something is built to fail
  it), Φ(α) = [sin α + (π − α) cos α]/π — the classical Lambert phase function (Φ(0) = 1, full
  phase; Φ(π) = 0, new phase) — whose phase integral over the sphere, *q* = 2∫Φ(α)sin α *d*α from
  0 to π, is exactly 3/2 for a Lambertian sphere (a standard, checked result: geometric albedo
  *p* = (2/3)*α*_A follows from it), so the far-field form conserves the reflected power exactly:
  *α*_A·*S*·π*R*_E² (Bond albedo times the incident power on Earth's own disc, *A*_E = π*R*_E²) —
  verified directly here, not merely asserted, by carrying *q* = 3/2 through: ∮ *E*(*r*,α) *dA* over
  a concentric sphere of radius *r* reduces to 2π·(2*α*_A/3)·*S*·*R*_E²·(*q*/2) =
  *α*_A·*S*·π*R*_E² exactly. The cap integral must **approach** this as *r* grows, at several phase
  angles, with the near-field departure at LEO and GNSS **characterised** (a measured number,
  `PHPR-A`'s own table) rather than asserted to vanish.
- **PHPR-P-4.** The visible cap's own size at GNSS altitude: 76.02°, 37.92 % of the Earth's
  surface (`RS09` Eq. 2.25–2.26, independently reproduced and matching to the stated precision) —
  checked directly, not inferred from the force values being reasonable.
- **PHPR-P-5.** `PHPR-R-010`'s aberration term, sized rather than left to the implementation to
  reveal: order (*v*/*c*) times the steady SRP acceleration it corrects, *v* the spacecraft's
  velocity **relative to the Sun**, dominated by Earth's own ≈ 29.8 km/s heliocentric orbital
  motion (`PHPR-R-005`'s own **v**_rel = **v**_sat,gcrs − **v**_sun,gcrs, not the spacecraft's
  ≈ 7.4 km/s geocentric speed alone, which would be short by roughly a factor of four), *v*/*c* ≈
  1 × 10⁻⁴ — of order 1.5 × 10⁻⁹ m/s² at LightSail-2's own stated ballistic and optical
  properties, above L2's own 8.552 × 10⁻¹¹ m/s² floor and above the de Sitter term this tree
  already models, so it is not a term this tree can treat as beneath its own stated precision. The
  exact figure, at the properties `PHPR-A-006` states, is **measured, not asserted a priori**,
  following `PHPR-P-1`'s own precedent — and `PHPR-A-006`'s own tolerance is set tight enough to
  fail if the geocentric-only velocity were used by mistake, not merely to confirm the right order
  of magnitude.
- **PHPR-P-6.** The remaining finite-difference step sizes (every Jacobian not covered by
  `PHPR-R-010`'s own analytic one), sized by measurement, `DRAG-P-1`'s own precedent, not guessed —
  each dimension's own status stated rather than assumed uniform. `Srp::accel`'s own
  position-Jacobian step, 100 m: **measured** (`PHPR-A-017`), a stable plateau against its own
  neighbours (1e-8 relative at 30 m/1000 m) with visibly more jitter at the sweep's small-*h* end
  (2.5e-7 at 0.1 m) — inside the balance a central difference always strikes between shrinking
  truncation error and growing floating-point cancellation. `Erp::accel`'s own position-Jacobian
  step (100 m) and its own neglected-velocity-bound step (0.01 m/s, `PHPR-P-2`): **not yet
  measured** — carried at the same first-pass footing `Srp`'s own steps started from, not solved by
  this entry, which scoped itself to the cap integral's own coordinate system and `Srp`'s own
  velocity Jacobian, not every remaining finite difference in this force family.

---

## 7. Failure behaviour

Two of the four failure modes this force family can exhibit are **not** this
spec's own ids: they are foreign, structured causes forwarded unchanged by
both plugins (`DRAG-F-003`'s own precedent for a cause reached through more
than one caller), not relabelled into a `PHPR-F-` id that would then disagree
with what a caller actually sees thrown. Only `PHPR-F-003`/`PHPR-F-004` are
minted directly by this spec's own code.

| id | fires when | carries |
|---|---|---|
| `ATTD-F-001` | nominal attitude's Sun direction within a stated tolerance of the nadir axis (§4.2) — `odl::attitude`'s own id, forwarded unchanged by `Srp`/`Erp`, never relabelled | the two nearly-parallel directions and the tolerance |
| `SRPA-F-001` | `photon_force` called on a `Macromodel` with zero surfaces — the shared kernel's own id, reused, not a second definition of the same refusal | reuses `SRPA-F-001`'s own reasoning, through the shared kernel |
| `PHPR-F-003` | the Sun's ephemeris state unavailable at the requested epoch (`ephemerides` coverage) | the epoch and the coverage window |
| `PHPR-F-004` | `cap_integral` called with a non-positive grid dimension (`n_chi` or `n_az` ≤ 0) | the offending dimension |
| `PHPR-F-005` | `cap_integral` called with the satellite at or below the spherical Earth's own radius — *β* = arccos(*R*_E/*r*) is undefined there | the satellite's own radius and *R*_E |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `PHPR-A-001` | `srp_force`'s output, over the five optical triples, ten directions and every surface kind (single sphere, single body-fixed flat, single sun-pointing flat, and 5×5 box-wing bus/panel/sphere combinations — 400 cases), against `srp_force_golden.hpp`'s own values, **captured from a worktree built at the pre-refactor commit, never regenerated from this file's own code** — the comparison a live self-call cannot make, since it would compare the wrapper against the call it makes | **equality**, bit-for-bit, not tolerance | measurement, against the captured golden file (`PHPR-R-003`) | exact | R-001, R-002, R-003 |
| `PHPR-A-002` | `SPEC-srp-analytic` §8's own existing acceptance rows (`SRPA-A-001`–`A-010`), re-run against `photon_force`'s wrapper, unchanged | identical to `SPEC-srp-analytic`'s own recorded values | `SPEC-srp-analytic` §8 | exact | R-003 |
| `PHPR-A-003` | nominal attitude reproduces the standard yaw-steering law at a stated geometry (an equatorial, a polar, and an eclipse-season case) and refuses at the degenerate Sun-on-nadir geometry | as stated; the refusal | this spec, §4.2 | — | R-004, F-001 |
| `PHPR-A-004` | the SRP plugin's distance-scaled irradiance, at perihelion and aphelion dates, differs from the fixed-1367 value by the stated +3.43 % / −3.26 % | as stated | Earth's own orbital eccentricity, independently computed from `ephemerides` | measured | R-001, R-005 |
| `PHPR-A-005` | the SRP plugin's shadow factor, fired and shown not to fire, at a stated eclipse and a stated sunlit geometry | as stated, both directions | `shadow::conical`, reused | exact | R-006 |
| `PHPR-A-006` | `Srp::accel`'s own velocity Jacobian is **analytic** (`PHPR-R-010`'s own force law is exactly affine in velocity, so its derivative is a closed-form constant matrix, not a local linearisation), checked against an INDEPENDENT central finite difference of the whole public `Srp::accel` call — never the same computation repeated (plan §4 rule 5) — proving `PHPR-R-010`'s aberration term is actually in the force, not only in the Jacobian's own formula, **and that it uses velocity relative to the Sun, not the spacecraft's bare GCRS velocity**: the two give results differing by roughly a factor of four at this tree's own case (`PHPR-P-5`). Checked on **both** surfaces the closed form branches on — a sun-pointing flat panel (`steady_direction ⊗ e_D + drag_Q_pr·I`) and a sphere (`e_D ⊗ e_D + I`) — not the flat case alone: the two are different closed forms, and a panel-only model cannot exercise the sphere's own branch | agreement to better than 1e-6 relative on each surface kind (measured ~1.5e-9 flat, ~8.3e-10 sphere, at a 10 m/s test step chosen because the finite difference has ZERO truncation error here — the function being differenced is exactly affine — so a larger step only reduces floating-point cancellation, the sole remaining error source) | analytic vs. an independent finite difference of `Srp::accel`; `BLS79` Eq. (5); `PHPR-P-5` | measured, tight enough to catch the factor of four (and far tighter than that) | R-007, R-010 |
| `PHPR-A-007` | **the infrared exact identity** (`PHPR-P-3`), at LEO and at GNSS altitude, on a `SphericalSurface` spacecraft, constant emissivity — the cap integral's own definition (`PHPR-R-008`), proven by what it must reproduce, **and** its own three-point grid refinement (30/60/120) checked against the predicted 4× ratio (`PHPR-P-1`), not merely required to improve | *C*_R·*A*·*M*·(*R*_E/*r*)²/(*c*·*m*), exact at every altitude; ratio within 3.5–4.5× at every step | derived, `PHPR-P-3`; `PHPR-P-1` | `PHPR-P-1`'s converged grid, < 1e-3 | P-1, P-3, R-008 |
| `PHPR-A-008` | **the albedo far-field limit** (`PHPR-P-3`), the cap integral's own departure from (2*α*_A/3)·*S*·(*R*_E/*r*)²·Φ(α) measured and shown to shrink as *r* grows, at several phase angles | the departure, characterised, not asserted to vanish; shrinking with *r* | derived, `PHPR-P-3` | not a converged-order claim (`PHPR-Q-004`: the terminator still staircases this grid) — trend only | P-3, R-008 |
| `PHPR-A-009` | grid-refinement convergence **ratio** for the **albedo** far-field identity (`PHPR-A-008`) — the infrared identity's own ratio is `PHPR-A-007`'s, measured there, since that term alone is not subject to `PHPR-Q-004`'s own terminator-staircase question | as `PHPR-P-1` states, once `PHPR-A-008` exists to measure it against | `PHPR-P-1` | exact ratio, if `PHPR-Q-004` does not prevent one | P-1 |
| `PHPR-A-010` | the visible cap's own size at GNSS altitude | 76.02°, 37.92 % | `PHPR-P-4` | matching stated precision | P-4 |
| `PHPR-A-011` | Fig. 2's (`RS12`, its own Δ*u*/*β*₀ frame, `cos ψ = cos β₀ cos Δu` checked against the source directly) qualitative RADIAL structure: global maximum at Δ*u* = 0° (*β*₀ = 0°), local minima at Δ*u* = 90°/270°, a secondary local maximum at Δ*u* = 180° (absent for a cannonball model, RS12's own text — this row's own macromodel is a sun-pointing panel with a back face, `PHPR-R-004a`, not a sphere alone) — **and** a cross-track sign flip with *β*₀'s own sign, exact (odd), at two stated Δ*u*. The non-radial component's own extremum LOCATION (RS12's own ≈35°/145°) is **not** tested here — a model-geometry-sensitive claim this row does not attempt to pin down; the sign-flip claim (a symmetry argument, not a geometry-sensitive number) is | as stated, qualitatively (radial structure, cross-track flip); extremum location not asserted | `RS12` Fig. 2, checked directly (`data/literature/rodriguez-solano-2014-dissertation`, P-I, p.77) | qualitative | — |
| `PHPR-A-012` | refusals fired, each proven both ways: `ATTD-F-001` (forwarded), `SRPA-F-001` (reused), `PHPR-F-003`, `PHPR-F-004`, `PHPR-F-005` | the diagnostics, with the two forwarded ones' own id checked to confirm they surface unrelabelled | this spec, §4.2/§4.1/§4.4 | — | F-003, F-004, F-005 |
| `PHPR-A-013` | **albedo and emissivity are genuinely functions, not a constant wearing a function-shaped interface**: the cap integral called with a stated *non*-constant test function (varying with latitude) produces a measurably different result from the constant-0.3/0.7 call, at the same geometry | as stated, both differing measurably | this spec, `PHPR-R-009` | measured | R-009 |
| `PHPR-A-014` | ERP's own neglected velocity-dependence bound (`PHPR-P-2`), sized against a real finite difference of `Erp::accel` at a stated case | as stated | `PHPR-P-2` | measured | P-2 |
| `PHPR-A-015` | `Erp::accel` wired end-to-end at a stated LEO case: `dyn::Force`'s own interface reached, a non-zero result returned, and its direction and order of magnitude checked against the jump-table-style figures §1/§6 already state for Earth-radiation accelerations, not merely that it returns without refusing | a non-degenerate result, right order of magnitude and general direction | this spec | plausibility, not exact | R-011 |
| `PHPR-A-016` | **`PHPR-R-004a`'s two axes, each checked separately then together**: (1) face — a two-sided `FlatSurface` with the source behind its front normal receives a force through the back triple's own optical properties; the same geometry on a one-sided surface receives nothing, `cos θ < 0` unchanged from today; (2) band — a surface with a stated infrared triple different from its visible one gives a measurably different `photon_force` result under `Band::infrared` than `Band::visible`; a surface with no infrared triple gives the *same* result under both, the stated fall-back; (3) both together — a two-sided surface with band-differing back optics only, front optics identical in both bands | as stated, all three cases | this spec, §4.1 | exact | R-004a |
| `PHPR-A-017` | `Srp::accel`'s own remaining finite-difference step, the position-Jacobian one (`PHPR-P-6`), is SIZED, not guessed (plan §4 rule 7): a step sweep (1e4 m down to 1e-2 m) of `Srp::accel`'s own public `d(a)/d(r)`, measured independently of `accel_only`'s own internal step, shows a stable plateau containing the production 100 m value, with visibly more step-to-step jitter at the sweep's small-*h* end | 100 m agrees with its 30 m/1000 m neighbours to better than 1e-3 relative; jitter at 0.1 m exceeds that | measured, against the true pipeline | measured | P-6 |
| `PHPR-A-018` | **(v1.1, L5 step 4's own review — a genuine copy-paste bug caught by `speccheck.py`'s new duplicate-`TEST_CASE` check, not written fresh)** `Srp::accel` wired end-to-end at a stated LEO case: `dyn::Force`'s own interface reached, a non-zero result returned, right order of magnitude — `Srp`'s own counterpart to `PHPR-A-015` (`Erp`'s), which `modules/srp/tests/srp_tests.cpp` had claimed instead, unnoticed until this round: nothing before this round's own new check could tell two different modules' own tests, in two different files, apart merely for sharing a string | a non-degenerate result, right order of magnitude | this spec | plausibility, not exact | R-005 |

---

## 9. Provenance obligations

- **`DYN-R-052`** (proposed, `SPEC-dynamics` amendment under `DYN-Q-001`'s terms — recorded here,
  ruled there). `ForceEvaluation::provenance` becomes `std::vector<Provenance>` — empty meaning
  "none declared," the same discipline the `std::optional` it replaces already carried. `Provenance`
  itself is unchanged; `source_id` already names which product a given entry is about (it already
  does this job for `Drag`'s single entry today), so no new discriminating field is needed. **Not
  because this step's own data needs it**: with constant albedo and emissivity, this step declares
  **zero** provenance entries, having no live external source to snapshot. The reason is
  `ForceEvaluation` being the contract every force fills, with producers multiplying now (`Srp`,
  `Erp`, and named-but-not-built thrust/ECOM models later) while today only `Drag` sets the field —
  changing the type once, with one producer, is the cheapest this migration will ever be.
- `PROVENANCE.md` records, as a new numbered section: the scope amendment and why (§1); the kernel
  refactor, with `PHPR-A-002`'s 92 636 pre-existing assertions passing unchanged (tolerance-bounded
  agreement with the physics) and `PHPR-A-001`'s own golden-file capture (bit-exact agreement with
  the pre-refactor code itself, at the commit named in `srp_force_golden.hpp`) — two different
  claims, recorded as such rather than one conflated with the other, since a review round found
  they had been — that the refactor is behaviour-preserving, since it touches L4 step 3's own
  closed module; the
  `NormalMode::sun_pointing` finding (§4.1) and why it is required, not merely convenient, for
  ERP's correctness; the face/band schema gap (`PHPR-R-004a`) — found at ERP, this force family's
  first consumer with a real need for either axis, the second time a contract frozen against a
  trivial/degenerate client met that need at its first real one (after L3's provenance channel,
  `DYN-R-051`) — and that it touches `SPEC-macromodel`'s own closed schema, additively, alongside
  `srp_analytic`; the aberration term (`PHPR-R-010`), the `BLS79` and `KVP10` searches and their
  outcomes (both found, both copyright held, neither redistributed), and the reasoning from
  `BLS79`'s own *Q*_pr cases to the one this tree's target spacecraft needs (a near-specular
  reflector near normal incidence, not an absorber or an isotropic scatterer); the velocity-vs-source
  correction (`PHPR-R-005` step 3) — the aberration term first written against the spacecraft's own
  bare GCRS velocity, wrong by roughly a factor of four at this tree's own case, caught before any
  code existed to carry the error forward, not after; the two exact identities' own derivations,
  independent of `RS09`, with the measured departures `PHPR-A-007`/`A-008` produce; the `KRT88`
  search and its outcome (unreachable); `RS09`'s own terms search, including the landing-page
  bot-challenge finding, distinct in kind from "searched and found nothing."

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `PHPR-Q-001` | **Knocke's own zonal/seasonal coefficients need an accessible, citable source before this tree uses them** (§1). Carried as the next refinement on constant albedo/emissivity, not solved here. If a citable secondary source (a paper that reprints the coefficients, not merely the model's shape) turns up, it is this question's answer; if none does, the alternative is a CERES-derived product built and pinned as its own data source. |
| `PHPR-Q-002` | **RULED: `attitude` is its own module now.** Step 5 builds the ideal nominal yaw-steering law only, refusing at the Sun-on-nadir singularity (`PHPR-A-003`). Everything beyond ideal nominal — noon/midnight turns, constellation-specific laws, antenna thrust — is step 6's (`../plan/subplan_L4/L4-6.md`, "thrust-yaw"), adding providers to this module rather than rebuilding its law. Two real consumers from day one settled it; no speculative third consumer was needed. |
| `PHPR-Q-003` | The PPM's ellipsoidal, atmosphere-aware shadow (§1) is available and unused. Worth a later precision step, or is SECM's own accuracy (`SPEC-shadow`'s own measured figures) sufficient for this tree's stated tolerances indefinitely? Carried, not solved here. |
| `PHPR-Q-004` | **The albedo term's own terminator still staircases the nadir-centred cap grid** (§4.4): nadir-centring fixed the visibility boundary (a function of colatitude *χ* alone) but not the Sun-lit boundary (a function of both *χ* and azimuth), so `PHPR-A-008`/`A-009`'s own reflected-term convergence order is not yet established the way `PHPR-A-007`'s infrared one now is. Two directions, not chosen between here: (a) a sub-cell lit fraction at the boundary cells (more accurate, a real quadrature-refinement project of its own), or (b) accept the non-clean ratio and gate `PHPR-P-1`'s own claim on an ERROR BOUND rather than an order, the way `PHPR-A-007`'s own predecessor briefly did. Carried, not solved here. |
| `PHPR-Q-005` | **The cap integral's own `lat_rad`/`lon_rad` (fed to the `albedo`/`emissivity` callbacks) are GCRS-frame spherical angles, not true Earth-fixed (ITRS/geodetic) ones** — inert under this step's own constant-albedo/emissivity functions (§4.4), which ignore both arguments, but a real gap before `PHPR-Q-001`'s own zonal/seasonal or gridded model (which needs true geography, not an inertial-frame direction that drifts under it as Earth rotates) can be plugged in. Fixing it needs a GCRS→ITRS rotation this function does not yet take — a second, separable change, not folded into this step's own scope. Carried, not solved here. |
