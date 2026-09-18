# SPEC-perturbations — tides, the relativistic correction, and third-body attraction

| | |
|---|---|
| **Spec ID** | `PERT` |
| **Status** | **adopted** 2026-09-18; **amended by implementation the same day**, six corrections at v1.2 |
| **Version** | 1.2 |
| **Date** | 2026-09-18 |
| **Layer** | L2 `environment`, step 3 (`doc/REWRITE_PLAN.md` §3.3) |
| **Depends on** | `SPEC-gravity.md` (the field these perturb, and the secular pole it defines), `SPEC-ephemerides.md` (the Sun and Moon), `SPEC-eop.md` (polar motion), `SPEC-time.md`, `SPEC-frames.md`, `core` |
| **Depended on by** | the force model (L4), the variational equations (L3/L7) |

**Derivation declaration (plan R1).** This specification was written from the documents
listed in §2 and from no implementation of this module.

**Predecessor access.** The predecessor shares this repository as of 2026-09-18, so the
claim earlier specifications could make — that it lived in a separate tree and was not
reachable — is no longer available to any specification written after that date, and must
not be implied. What is claimed instead, and what is checkable: no file under the
repository root's `src/`, `include/`, `res/`, `scripts/`, `analysis/`, `analyses/`,
`REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or otherwise inspected
during this specification's preparation. Anything about the predecessor that did reach the
author is listed below with its route, and repeated in the exposure register at
`PROVENANCE.md` §0.2.

Numeric acceptance targets carried from prior measurement campaigns are behavioural
observations against public data (plan R4) and are marked as such where they appear.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 1. Purpose and scope

Step 2 built the Earth's **static** field. This step covers everything else in L2 that acts
on a spacecraft without being a property of it: the parts of the Earth's field that **move**,
the correction general relativity makes to the equations of motion, and the attraction of the
other bodies in the solar system.

### Why one document for three things

They are specified together because the manager reviews a step, and because two of the three
are small enough that a separate document would be mostly front matter. They are **not**
necessarily one module: `thirdbody` needs `ephemerides` and nothing else, `relativity` needs
neither `ephemerides` nor `gravity`, and `tides` needs both plus `eop`. Bundling them into one
link target would make every consumer of third-body attraction link the ocean-tide tables.
`PERT-Q-001` puts the split to the manager; the document is written so that it divides along
its own §4 boundaries if the answer is three.

### In scope

- **Solid Earth tides** — the degree-2 and degree-3 tidal potential's effect on C̄*ₙₘ*, S̄*ₙₘ*,
  in the Conventions' three steps, including the frequency-dependent corrections.
- **Ocean tides** — the periodic variation of the same coefficients, from a published ocean
  tide model.
- **The solid Earth pole tide** and **the ocean pole tide** — the centrifugal effect of polar
  motion on the solid Earth and on the oceans.
- **The relativistic correction** to the acceleration of an Earth satellite in the GCRS:
  Schwarzschild, Lense–Thirring, de Sitter.
- **Third-body attraction** — the direct and indirect terms of the Sun, Moon and planets.

### Not in scope

| excluded | owned by |
|---|---|
| The static geopotential and its recursion | `SPEC-gravity.md` |
| **The secular pole** — defined once in `gravity` and consumed here, never restated | `SPEC-gravity.md` `GRAV-R-029` |
| Station displacement by tides (chapter 7's solid Earth, ocean loading and pole-tide *displacements*) | not in this plan; this module perturbs the **potential**, not a site |
| Atmospheric density and drag | L2 step 4, L4 |
| Solar radiation pressure, Earth radiation pressure | L4 |
| Relativistic corrections to the **measurement** model and to time transformation | `SPEC-time.md` (time), L6 (measurements) |
| The barycentric *n*-body equations of motion (`TN36-10` §10.4) | nothing here integrates in the BCRS |
| Numerical integration of the resulting acceleration | L3 `dynamics` |

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `TN36-6` | IERS | *IERS Conventions (2010)*, chapter 6 — §6.2 solid Earth tides, §6.3 ocean tides, §6.4 solid Earth pole tide, §6.5 ocean pole tide, §6.6 tidal amplitude conventions, Tables 6.3–6.8 | update of 01 Feb 2018 | `https://iers-conventions.obspm.fr/content/chapter6/icc6.pdf`, SHA-256 `abb3c0b0…4388` | primary | normative |
| `TN36-7` | IERS | *IERS Conventions (2010)*, chapter 7 §7.1.4 — the secular pole, and equation (25) relating the wobble variables to polar motion | update of 01 Feb 2018 | `https://iers-conventions.obspm.fr/content/chapter7/icc7.pdf`, SHA-256 `ffe8ffb1…043d` | primary | normative |
| `TN36-10` | IERS | *IERS Conventions (2010)*, chapter 10 §10.3 — equations of motion for an artificial Earth satellite, equation (10.12) | 2010 | `https://iers-conventions.obspm.fr/content/chapter10/tn36_c10.pdf`, SHA-256 `8be36ed5…de54` | primary | normative |
| `TN36-1` | IERS | *IERS Conventions (2010)*, chapter 1 — the numerical standards table | update of 06 Nov 2017 | `https://iers-conventions.obspm.fr/content/chapter1/icc1.pdf`, SHA-256 `a1ed81de…5c9f` | primary | normative |
| `FES2004-CS` | IERS / Lyard et al. | `fes2004_Cnm-Snm.dat` — the FES2004 geopotential harmonic amplitudes for direct use in (6.15) | Conventions supplementary material | `https://iers-conventions.obspm.fr/content/chapter6/additional_info/tidemodels/fes2004_Cnm-Snm.dat`, retrieved 2026-09-18, SHA-256 `620dc48f…94cf`, 3 686 988 bytes | primary | normative (dataset) |
| `DESAI-CO` | IERS / Desai | `desaiscopolecoef.txt.gz` — the self-consistent equilibrium ocean pole tide coefficients Ā*ₙₘ*, B̄*ₙₘ* to degree and order 360 | Conventions supplementary material | `https://iers-conventions.obspm.fr/content/chapter6/additional_info/desaiscopolecoef.txt.gz`, retrieved 2026-09-18, SHA-256 `075efd2e…d3dc`, 2 452 565 bytes, 65 340 rows to degree and order 360 | primary | normative (dataset) |
| `LYARD06` | Lyard, Lefevre, Letellier, Francis | *Modelling the global ocean tides: modern insights from FES2004* | Ocean Dynamics **56**, 394–415, 2006 | doi:10.1007/s10236-006-0086-x | **not obtained** | informative |
| `DESAI02` | Desai, S. D. | *Observing the pole tide with satellite altimetry* | JGR **107**(C11), 3186, 2002 | doi:10.1029/2001JC001224 | **not obtained** | informative |
| `CT71` | Cartwright, D. E. and Tayler, R. J. | *New computations of the tide-generating potential* | Geophys. J. R. astr. Soc. **23**, 45–74, 1971 | doi:10.1111/j.1365-246X.1971.tb01803.x | **not obtained** | informative |
| `CE73` | Cartwright, D. E. and Edden, A. C. | *Corrected tables of tidal harmonics* | Geophys. J. R. astr. Soc. **33**, 253–264, 1973 | doi:10.1111/j.1365-246X.1973.tb03420.x | **not obtained** | informative |
| `MATHEWS02` | Mathews, Herring, Buffett | *Modeling of nutation and precession* | JGR **107**(B4), 2068, 2002 | doi:10.1029/2001JB000390 | **not obtained** | informative |

**Four sources are not obtained and one of them matters, so it is said here rather than left
to §10.** `CT71` and `CE73` define the amplitude convention *H*ₖ that `TN36-6` (6.8) is written
in, and neither was retrieved. Nothing in §§4–8 depends on them **as documents**: every *H*ₖ
this module needs is either printed in `TN36-6`'s own tables or derivable from them, and
`PERT-A-001` is built so that it uses the Conventions' printed amplitudes rather than an
independently computed harmonic expansion. `PERT-Q-004` records what would change if a
constituent outside those tables were ever wanted. `LYARD06` and `DESAI02` describe how their
datasets were made; this module consumes the datasets, which are pinned by hash.

---

## 3. Definitions and conventions

### 3.1 What this module produces, and what it does not

- **PERT-R-001.** The tide models produce **increments to the normalised geopotential
  coefficients**, ΔC̄*ₙₘ* and ΔS̄*ₙₘ*, at an epoch. They MUST NOT produce an acceleration
  directly: the acceleration is `gravity`'s synthesis applied to the perturbed coefficients,
  and computing it twice by two routes is how the two drift apart.
- **PERT-R-002.** The relativistic correction and third-body attraction produce an
  **acceleration** in the GCRS. They are not coefficient increments and there is no geopotential
  expansion of either.
- **PERT-R-003.** The two kinds MUST NOT share an interface. A caller adding a tide to a field
  and a caller adding an acceleration to a sum are doing different things, and a type that
  serves both would let one be mistaken for the other.

### 3.2 The tide system, inherited

`SPEC-gravity.md` `GRAV-R-008` requires the static field to carry its tide system, and
`GRAV-F-008` refuses to add a tide model whose system disagrees. That refusal is consumed here.

- **PERT-R-004.** `gravity`'s conventional field is **zero tide** (`GRAV-R-021`). `TN36-6`
  (6.13) therefore requires the permanent part of the solid Earth tide to be **removed** from
  the variation this module computes: ΔC̄₂₀^zt = ΔC̄₂₀ − ΔC̄₂₀^perm, with

  > ΔC̄₂₀^perm = *A*₀ *H*₀ *k*₂₀ = (4.4228 × 10⁻⁸)(−0.31460) *k*₂₀ [`TN36-6` (6.14)]

  This is Step 3 of the Conventions' three-step computation and it is **not optional**: a
  zero-tide background field plus an unadjusted tide model counts the permanent deformation
  twice.
- **PERT-R-005.** The tide system of every increment MUST be carried with it and checked
  against the field's before addition. `PERT-F-001` refuses a mismatch.

### 3.3 The wobble variables, and where the pole comes from

`TN36-7` (25):

> *m*₁ = *x*_p − *x*_s ,  *m*₂ = −(*y*_p − *y*_s)

where (*x*_p, *y*_p) is polar motion from `SPEC-eop.md` and (*x*_s, *y*_s) is the **secular
pole**.

- **PERT-R-006.** The secular pole is taken from `gravity`'s `SecularPole`, which
  `GRAV-R-029` makes the single definition of it in this tree. This module MUST NOT restate
  `TN36-7` (21)'s four constants. *Two definitions is how the static field ends up secular and
  the pole tide ends up mean, and the inconsistency would be worth about what the figure-axis
  substitution itself is worth.*
- **PERT-R-007.** Note the **sign on *m*₂**: it is minus (*y*_p − *y*_s), not plus. A sign
  error here is invisible in the magnitude of the pole tide and inverts its phase.
- **PERT-R-008.** *m*₁ and *m*₂ are dimensionless angles. `TN36-6` §6.4's printed coefficients
  are written for *m* in **seconds of arc** and §6.5's equation (6.23a) for *m* in **radians**.
  The two are used in adjacent equations of the same chapter. The module MUST carry one
  representation internally, name its unit at every interface, and the acceptance tests check
  both printed forms.

### 3.4 Constants, and two that share a name with something else

| symbol | value | source | note |
|---|---|---|---|
| *G* | 6.674 28 × 10⁻¹¹ m³ kg⁻¹ s⁻² | `TN36-1` | |
| GM⊕ | 3.986 004 418 × 10¹⁴ m³ s⁻² | `TN36-1` | **the TCG-compatible value** |
| *a*_E | 6 378 136.6 m | `TN36-1` | **not** EGM2008's *a*ₑ = 6 378 136.3 m |
| *g*_E | 9.780 3278 m s⁻² | `TN36-1` | mean equatorial gravity |
| Ω | 7.292 115 146 706 979 × 10⁻⁵ rad s⁻¹ | `TN36-1` dθ/d*t* | as `SPEC-template.md` §7 |
| ρ_w | 1025 kg m⁻³ | `TN36-6` §6.5 | density of sea water |
| *c* | 299 792 458 m s⁻¹ | defining | |
| \|**J**\| | ≈ 9.8 × 10⁸ m² s⁻¹ | `TN36-10` §10.3 | Earth's angular momentum per unit mass |

- **PERT-R-009.** `TN36-1`'s *a*_E and GM⊕ are **not** EGM2008's scaling parameters.
  6 378 136.6 m against 6 378 136.3 m, and the TCG-compatible GM against the TT-compatible one.
  `SPEC-gravity.md` `GRAV-R-004` makes the model's pair travel with the model; the same
  discipline applies in reverse here, and the tide formulae MUST use `TN36-1`'s values because
  that is what the Conventions wrote them in. Neither set may be substituted for the other, and
  `PERT-F-002` refuses a mixed pair.

---

## 4. Required behaviour

### 4.1 Solid Earth tides (`TN36-6` §6.2)

Three steps, in the Conventions' own order.

**Step 1 — the time-domain evaluation with nominal Love numbers.** `TN36-6` (6.6):

> ΔC̄*ₙₘ* − *i*ΔS̄*ₙₘ* = (*k*ₙₘ/(2*n*+1)) Σ*ⱼ₌₂,₃* (GM*ⱼ*/GM⊕) (*R*ₑ/*r*ⱼ)^(*n*+1) P̄*ₙₘ*(sin Φ*ⱼ*) e^(−*im*λ*ⱼ*)

over the Moon (*j* = 2) and the Sun (*j* = 3), with Φ*ⱼ* and λ*ⱼ* the **body-fixed** geocentric
latitude and east longitude. `TN36-6` (6.7) gives the degree-4 changes the degree-2 tides
produce through *k*₂ₘ^(+), of the same form with *k*ₙₘ replaced.

- **PERT-R-010.** Step 1 MUST be evaluated for *n* = 2 and *n* = 3, all *m*, and the degree-4
  changes of (6.7) for *m* = 0, 1, 2.
- **PERT-R-011.** The nominal Love numbers are `TN36-6` Table 6.3's **anelastic Earth** column,
  which is complex. The elastic column is offered by the Conventions for comparison and MUST
  NOT be the default; which column is in force MUST be recorded.
- **PERT-R-012.** Φ*ⱼ* and λ*ⱼ* are body-fixed, so the Sun and Moon positions MUST be rotated
  into the ITRS by `frames` before this step. Using GCRS coordinates gives a field that rotates
  with the sky instead of with the Earth and is wrong by the whole of the diurnal signal.
- **PERT-R-013.** P̄*ₙₘ* is the same normalised function `gravity` defines (`GRAV-R-007`,
  without the Condon–Shortley phase) and MUST be obtained from `gravity`'s recursion, not
  reimplemented. Two normalisation conventions in one tree is the defect `GRAV-A-024` exists
  to prevent.

**Step 2 — the frequency-dependent corrections.** `TN36-6` (6.8a)–(6.8e), summed over the
constituents of `TN36-6` Tables 6.5a (diurnal, *m* = 1), 6.5b (long period, *m* = 0) and
6.5c (semidiurnal, *m* = 2), with

> *A*₀ = 4.4228 × 10⁻⁸ m⁻¹,  *A*ₘ = (−1)^*m* 3.1274 × 10⁻⁸ m⁻¹ (*m* ≠ 0),  η₁ = −*i*, η₂ = 1

and the tidal argument θ_f = **n̄** · **β̄** = *m*(θ_g + 180°) − **N̄** · **F̄**, where **β̄** is
Doodson's six fundamental arguments, **F̄** the five Delaunay variables and θ_g Greenwich Mean
Sidereal Time in angle units.

- **PERT-R-014.** Tables 6.5a, 6.5b and 6.5c MUST be **extracted from the hash-pinned PDF**, as
  `tools/tides_from_conventions.py` already does for the chapter 5 and chapter 8 tables, and
  committed as generated source. They MUST NOT be hand-transcribed: there are several hundred
  numbers and transcription would be its own defect source. The extraction is not part of the
  build, because `pdftotext` output varies with the poppler version and a build that re-ran it
  would not be reproducible.
- **PERT-R-015.** θ_g is Greenwich Mean Sidereal Time and the Delaunay variables are the ones
  the nutation model uses. They MUST come from **one** definition, consumed rather than restated,
  on the same argument as `GRAV-R-029`. *This requirement said "from `frames`"; the tree has them
  in `eop`, where Tables 5.1 and 8.2/8.3 already use them, and implementation promoted them to
  that module's public surface rather than moving them or making a second copy. The principle
  holds; the address was wrong.* `PERT-A-027` checks the two argument conventions against each
  other on the 71 constituents of Tables 6.5a/b/c, which print both.
- **PERT-R-016.** The Conventions state the diurnal Love numbers as a **resonance formula**
  (6.9) with the parameters of Table 6.4, and Tables 6.5a/c give the resulting δ*k*_f directly.
  **Production uses the tabulated δ*k*_f. The resonance formula is implemented as a test-only
  cross-check of that column** (`PERT-A-025`), which is the manager's ruling on `PERT-Q-003` and
  is a better answer than the one this specification first gave. It is not two answers to one
  question: it is one production route and one independent check, the same pattern as
  `PERT-A-002`, and **it is the only part of Step 2 the Conventions let this tree verify without
  an *H*_f catalogue.** It turns δ*k*_f from a transcription into a verified quantity.
- **PERT-R-017.** The resonance formula's σ_α are `TN36-6` (6.10) and its *L*₀, *L*_α are
  Table 6.4; σ = *f*/(15 × 1.002 737 909) converts a tidal frequency in degrees per hour to
  cycles per sidereal day, with **positive frequencies retrograde** — the opposite of the sign
  convention used in analytical theories of nutation, which `TN36-6` §6.2.1 says in one sentence
  and which is the kind of sentence a reader skips.

**Step 3 — the permanent tide**, `PERT-R-004`.

### 4.2 Ocean tides (`TN36-6` §6.3)

`TN36-6` (6.15):

> [ΔC̄*ₙₘ* − *i*ΔS̄*ₙₘ*](*t*) = Σ_f Σ_± (*C*^±_{f,nm} ∓ *i S*^±_{f,nm}) e^(±*i*θ_f(*t*))

- **PERT-R-020.** The model is **FES2004**, the Conventions' recommendation, consumed as the
  published geopotential harmonic amplitudes of `FES2004-CS` — not as tide heights converted
  here. The conversion from cotidal grids to harmonic amplitudes was done by the Conventions
  and redoing it would be a different model.
- **PERT-R-020a.** **The file is not quite what chapter 6 describes, and its own header is the
  authority on what it contains.** `FES2004-CS`'s first three lines state: the unit is 10⁻¹¹;
  the model runs to degree and order **(100, 100)**; the **long-period band is from FES2002, to
  (50, 50)**, not from FES2004; the equilibrium Ω₁/Ω₂ waves are **already included**; and the
  **atmospheric tide is not**. Each of those changes what a consumer may do, and none of them is
  in §6.3.2. The loader MUST read and record the header rather than assume the chapter's
  description, and MUST refuse a file whose header does not match what this specification was
  written against (`PERT-F-011`).
- **PERT-R-021.** **The zonal terms carry a convention that will otherwise be wrong by a factor
  of two.** `TN36-6` §6.3.2: FES2004 sets the retrograde coefficients *C*^−_{f,n0}, *S*^−_{f,n0}
  to zero and **doubles** the prograde ones, so after applying (6.15) the ΔC̄*ₙ*₀ have their
  expected value but **ΔS̄*ₙ*₀ MUST be set to zero**. A reader who applies (6.15) uniformly gets
  a spurious ΔS̄*ₙ*₀ and no warning.
- **PERT-R-022.** The maximum degree and order consumed MUST be a stated parameter and MUST be
  recorded with every result, as `GRAV-R-028` requires of the static field.
- **PERT-R-022a.** **The default is set by a criterion stated here, not by a number chosen here.**
  The criterion: *the truncation error of the ocean tide sum is below the smallest term this
  module computes and keeps, at the same evaluation point* — where **"the smallest term this
  module keeps" is a FIXED threshold and not a function of the ocean tide's own truncation**.
  Read the other way, as the smallest per-degree term among those kept, it is a moving target:
  keeping more degrees lowers the bar, and the criterion chases itself to "keep everything".
  Implementation found that. The fixed threshold is `TN36-6` §6.2.1's own cutoff for what the
  solid Earth tide includes — changes *"exceeding 3 × 10⁻¹²"* in C̄₄ₘ, which this module keeps
  and which are the smallest thing it deliberately keeps — evaluated at the same radius by
  `GRAV-R-041`'s identity, which is 8.552 × 10⁻¹¹ m s⁻² at 7331 km (`PERT-P-2`).

  **Measured, and therefore now the default: degree 89.** At 7331 km the criterion is met at
  degree 36, at 300 km altitude at degree 89, and the default is the larger. It MUST be measured at **two radii**,
  because (*a*ₑ/*r*)ⁿ makes the answer strongly altitude-dependent — at 7331 km degree 50 is
  attenuated by 9.5 × 10⁻⁴ and degree 100 by 8.9 × 10⁻⁷, while at 300 km altitude the same
  degrees are attenuated by 0.10 and 0.010 — and **the default is the larger of the two degrees
  the criterion returns**. No number appears in this requirement, deliberately: a number in a
  specification acquires authority whatever the changelog says about how it got there, so the
  measurement is made first and reported, and the default is then written down with its
  measurement beside it.
- **PERT-R-023.** The admittance interpolation of (6.16) for secondary waves MAY be applied,
  with `TN36-6` Table 6.7's pivot waves. **S₁ and S₂ MUST NOT be used as pivot waves**
  (`TN36-6` §6.3.2): they are radiational tides whose altimetric amplitudes include atmospheric
  pressure loading, and Table 6.7 excludes them for that reason.
- **PERT-R-024.** The separately distributed mean S₁ wave SHOULD be applied only when no ocean
  circulation model is also being applied, because such models already carry the S₁ signal.
  What is given up by omitting it is stated in §6.
- **PERT-R-025.** **Ω₁ and Ω₂ MUST NOT be added separately.** §6.3.2 describes how to model
  the very long period waves Ω₁ (18.6 yr) and Ω₂ (9.3 yr) as equilibrium waves, and reading the
  chapter alone one would implement it. `FES2004-CS`'s header says they are **already in the
  file**. Adding them again doubles them. This is why `PERT-R-020a` requires the header to be
  read: the instruction and the data disagree, and the data is what gets summed.

  The dispute §6.3.2 records — whether the phase change to π/2 introduced in its 2011-09-23
  update is justified, flagged as unresolved on 2011-10-14 — therefore does not bind this
  module at all, because this module never applies that equation. It is recorded at
  `PERT-Q-005` because it tells a reader something about the provenance of the Ω₁/Ω₂ amplitudes
  that are in the file.

### 4.3 The solid Earth pole tide (`TN36-6` §6.4)

The centrifugal potential of polar motion, (6.22), deforms the Earth; with *k*₂ = 0.3077 +
0.0036 *i* the Conventions print

> ΔC̄₂₁ = −1.333 × 10⁻⁹ (*m*₁ + 0.0115 *m*₂),  ΔS̄₂₁ = −1.333 × 10⁻⁹ (*m*₂ − 0.0115 *m*₁)

with *m*₁, *m*₂ in seconds of arc.

- **PERT-R-030.** The implementation computes from *k*₂ and the constants of §3.4 rather than
  from the printed shorthand, and `PERT-A-005` checks that it reproduces the printed leading
  coefficient. Derived here before being written down:
  Ω² *a*_E³ *k*₂^R /(GM⊕ √15) × (1″ in rad) = **1.333 237 × 10⁻⁹**, against the printed
  1.333 × 10⁻⁹.
- **PERT-R-031.** **The Conventions' two printed numbers are not mutually consistent, and the
  implementation follows *k*₂.** The ratio multiplying the cross term is Im(*k*₂)/Re(*k*₂) =
  0.0036/0.3077 = **0.011 700**, where §6.4 prints **0.0115**; the printed ratio implies
  Im(*k*₂) = 0.003 539 against the printed 0.0036. The discrepancy is 1.7 % of the imaginary
  part and **0.02 % of ΔC̄₂₁**, far below anything this tree measures, but it is a disagreement
  between two numbers in one paragraph of a normative document and is recorded as such
  (`PERT-Q-006`). The stated constant is the input and the ratio is a convenience derived from
  it, so the constant wins.

### 4.4 The ocean pole tide (`TN36-6` §6.5)

`TN36-6` (6.23a) with (6.23b):

> (ΔC̄*ₙₘ*, ΔS̄*ₙₘ*) = *R*ₙ [ (Ā^R, B̄^R)(*m*₁γ₂^R + *m*₂γ₂^I) + (Ā^I, B̄^I)(*m*₂γ₂^R − *m*₁γ₂^I) ]
>
> **with *m*₁, *m*₂ in RADIANS here, and in ARCSECONDS in §6.5's printed result below — a factor
> of 206 264.8 between two equations on one page.**
>
> *R*ₙ = (Ω² *a*_E⁴ / GM) (4π*G*ρ_w / *g*_E) ((1 + *k*′ₙ)/(2*n*+1))

with γ = 1 + *k*₂ − *h*₂ = 0.6870 + 0.0036 *i*, *m* in **radians**, and the load deformation
coefficients *k*′₂ = −0.3075, *k*′₃ = −0.195, *k*′₄ = −0.132, *k*′₅ = −0.1032, *k*′₆ = −0.0892.

- **PERT-R-032.** The coefficients Ā*ₙₘ*, B̄*ₙₘ* come from `DESAI-CO`, declared in the manifest
  with URL and SHA-256 like any other input, to degree and order 360.

  **This chain has been verified end to end before being specified.** With *R*₂ computed from
  `TN36-1`'s constants, γ from §6.5 and the file's own (2, 1) row
  (Ā₂₁ = −0.243 253 305 + 0.005 468 074 *i*, B̄₂₁ = 0.005 468 074 − 0.192 521 112 *i*):

  | | computed here | `TN36-6` (6.24) prints |
  |---|---|---|
  | ΔC̄₂₁ | −2.177 813 × 10⁻¹⁰ (*m*₁ − 0.017 24 *m*₂) | −2.1778 × 10⁻¹⁰ (*m*₁ − 0.01724 *m*₂) |
  | ΔS̄₂₁ | −1.723 155 × 10⁻¹⁰ (*m*₂ − 0.033 65 *m*₁) | −1.7232 × 10⁻¹⁰ (*m*₂ − 0.03365 *m*₁) |

  Every printed digit, both coefficients and both cross terms. `PERT-A-006` is therefore a gate
  that is already known to be satisfiable, which is worth knowing before implementation rather
  than after.
- **PERT-R-033.** `TN36-6` §6.5 states that degree 2 carries about 90 % of the variance and
  degree 10 about 99 %, but that *representing the continental boundaries* needs high degree.
  The truncation degree MUST be a stated parameter recorded with every result, and the module
  MUST report the variance fraction its truncation retains rather than leaving the reader to
  the figure.
- **PERT-R-034.** **The degree-1 components MUST NOT be used for station displacement.** The
  Conventions say so explicitly. This module does not compute station displacement, so the
  requirement is that degree 1 be **excluded from anything this module exports** unless a
  caller asks for it by a name that says what it is.

### 4.5 The relativistic correction (`TN36-10` §10.3)

`TN36-10` (10.12), the correction to the acceleration in the GCRS, in three lines:

> Δ**r̈** = (GM_E/(*c*²*r*³)) { [2(β+γ)GM_E/*r* − γ(**ṙ**·**ṙ**)]**r** + 2(1+γ)(**r**·**ṙ**)**ṙ** }  *(Schwarzschild)*
> + (1+γ)(GM_E/(*c*²*r*³)) { (3/*r*²)(**r** × **ṙ**)(**r**·**J**) + (**ṙ** × **J**) }  *(Lense–Thirring)*
> + (1+2γ) { **Ṙ** × (−GM_S **R**/(*c*²*R*³)) } × **ṙ**  *(de Sitter / geodesic)*

- **PERT-R-040.** β = γ = 1 (general relativity). They MUST be named parameters with those
  defaults rather than folded into the arithmetic, because a PPN test is the only reason this
  form is written with them and folding them away removes the reason.
- **PERT-R-041.** **R** is the position of the Earth with respect to the **Sun**, and **Ṙ** its
  velocity — a barycentric quantity from `ephemerides`, not a geocentric one. **r** and **ṙ**
  are geocentric. The two appear in adjacent lines of one equation.
- **PERT-R-042.** The three terms MUST be individually selectable and individually reportable.
  `TN36-10` gives their relative magnitudes separately, the acceptance tests check them
  separately, and an analyst asking "how much is frame dragging worth here" is asking a
  question the module should answer.
- **PERT-R-043.** The independent variable MAY be TT or TCG; the distinction does not affect
  this correction but does affect the Newtonian part, and which is in force MUST be recorded.
  `TN36-10` §10.3 says exactly this.
- **PERT-R-044.** The relativistic effect of the Earth's oblateness is **not** included, as
  `TN36-10` §10.3 states, and the module MUST say so rather than leave it to be assumed.

### 4.6 Third-body attraction

- **PERT-R-050.** For a body *j* with position **r**ⱼ relative to the Earth and a satellite at
  **r**, the acceleration in the GCRS is
  GM*ⱼ* [ (**r**ⱼ − **r**)/\|**r**ⱼ − **r**\|³ − **r**ⱼ/\|**r**ⱼ\|³ ].
  The second term is the **indirect** term — the Earth's own acceleration towards the body —
  and omitting it is the classic error: it leaves a term of the same order as the direct one.
- **PERT-R-051.** The difference **r**ⱼ − **r** MUST be formed before cubing, and the
  expression MUST NOT be rearranged into a form that differences two nearly equal large
  numbers. At the Moon's distance \|**r**ⱼ\| and \|**r**ⱼ − **r**\| agree to about 2 %, and at
  the Sun's to 5 × 10⁻⁵; the standard remedy is Battin's form of the difference, and
  `PERT-A-010` measures whether it is needed here before it is adopted.
- **PERT-R-052.** Which bodies are included MUST be a stated list recorded with every result.
  The Sun and Moon are not optional; the planets are, and what is given up by omitting them is
  stated in §6.
- **PERT-R-053.** Body positions are obtained from `ephemerides` **relative to the Earth, in one
  call per body.** *This requirement said they come in the BCRS and must be translated by
  `FRAME-R-028`, and implementation found two things wrong with that. First, differencing two
  barycentric vectors to get a geocentric one subtracts two 1.5 × 10⁸ km quantities to obtain a
  3.8 × 10⁵ km one and throws away three digits for nothing, where the kernel can supply the
  difference directly. Second, `Ephemeris::state` returns `State<Frame::BCRS>` whatever centre is
  asked for, so its frame tag says "barycentric origin" about a geocentric vector — the frame is
  in the type, as `FRAME-R-004` requires, but the ORIGIN is a runtime argument the type does not
  carry.* See `PERT-Q-010`.

### 4.7 Composition

- **PERT-R-060.** The tide increments are added to `gravity`'s **conventional** coefficients,
  after its substitutions, never to the distributed file's.
- **PERT-R-061.** The sum of increments MUST be formed once and applied once. A caller that
  adds solid Earth tides to a field, synthesises, then adds ocean tides to the same field and
  synthesises again is computing something else.
- **PERT-R-062.** The perturbed field MUST carry the list of what was applied to it, with each
  model's own parameters, so that a result can say which tide models produced it.

---

## 5. Interfaces

```
// --- the coefficient perturbations -----------------------------------------
TideIncrements                                          // opaque, immutable
  ::system()                                            -> TideSystem
  ::at(degree: Degree, order: Order)                    -> (dC: f64, dS: f64)
  ::max_degree()                                        -> Degree
  ::models()                                            -> [ModelRecord]

SolidEarthTide::increments(at: Epoch[TT],
                           sun: Position<Frame::ITRS>[m],
                           moon: Position<Frame::ITRS>[m],
                           gmst: Angle, delaunay: DelaunayArguments,
                           love: LoveNumberSet)          -> Result<TideIncrements, PertError>

OceanTide::increments(at: Epoch[TT], gmst: Angle, delaunay: DelaunayArguments,
                      degree: Degree, order: Order)      -> Result<TideIncrements, PertError>

SolidEarthPoleTide::increments(wobble: Wobble)           -> Result<TideIncrements, PertError>
OceanPoleTide::increments(wobble: Wobble, degree: Degree)-> Result<TideIncrements, PertError>

Wobble::from(polar_motion: PolarMotion, secular: gravity::PoleCoordinates)
                                                         -> Wobble       // TN36-7 (25)

PerturbedField::of(base: gravity::ConventionalField, increments: [TideIncrements])
                                                         -> Result<PerturbedField, PertError>
PerturbedField::acceleration(at: Position<Frame::ITRS>[m], Degree, Order)
                                                         -> Result<Acceleration<Frame::ITRS>, PertError>

// --- the accelerations ------------------------------------------------------
Relativity::acceleration(state: State<Frame::GCRS>,
                         earth_in_bcrs: State<Frame::BCRS>,
                         terms: RelativityTerms, ppn: PpnParameters)
                                                         -> Result<Acceleration<Frame::GCRS>, PertError>
Relativity::by_term(...)                                 -> Result<[NamedAcceleration], PertError>

ThirdBody::acceleration(at: Position<Frame::GCRS>[m], bodies: [Body], eph: Ephemeris,
                        at_epoch: Epoch[TDB])            -> Result<Acceleration<Frame::GCRS>, PertError>
ThirdBody::by_body(...)                                  -> Result<[NamedAcceleration], PertError>
```

- **PERT-R-070.** `TideIncrements` and the acceleration types are **different types with no
  conversion between them** (`PERT-R-003`).
- **PERT-R-071.** `Wobble` is constructible only through `Wobble::from`, which demands the
  secular pole as an explicit argument. There is no default and no zero: a pole tide computed
  against an implicit zero secular pole is wrong by the whole secular drift, which by 2026 is
  larger than the wobble itself.
- **PERT-R-072.** `RelativityTerms` is a set of three named flags, not an integer, and
  `PpnParameters` defaults to β = γ = 1 and cannot be constructed without stating them.
- **PERT-R-073.** `Body` is the enumeration `SPEC-ephemerides.md` already defines
  (`EPH-R-020`); an integer NAIF code MUST NOT cross this interface.
- **PERT-R-074.** Everything is immutable after construction and there is no global state; two
  `PerturbedField`s at different epochs MUST answer independently and interleaved.
- **PERT-R-075.** Angles crossing these interfaces carry their unit in the type or in the
  parameter name. §3.3's two representations of *m*₁, *m*₂ — arcseconds in §6.4's printed form,
  radians in (6.23a) — are the reason.

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `PERT-P-1` | degree-2 solid Earth tide, the largest term this module produces | ΔC̄₂ₘ, ΔS̄₂ₘ **> 10⁻⁸** | 7.41683 m s⁻² × 0.756952 × 1 × 10⁻⁸ × 3.872983 = **2.174 × 10⁻⁷ m s⁻²** at 7331 km, by `GRAV-R-041`'s identity — **1786 times** the degree-90 truncation error the static field accepts | `TN36-6` §6.2.1, "can exceed 10⁻⁸ in magnitude" |
| `PERT-P-2` | degree-4 changes produced by the degree-2 tides through *k*₂ₘ^(+) | **> 3 × 10⁻¹²** | 7.41683 m s⁻² × 0.572976 × 3 × 10⁻¹² × 6.708204 = **8.552 × 10⁻¹¹ m s⁻²** | `TN36-6` §6.2.1, which sets this as its own cutoff for what to include |
| `PERT-P-3` | the Schwarzschild terms | a few parts in 10⁹ of the main Newtonian acceleration at low orbit | 7.41683 m s⁻² × 1 × 10⁻⁹ = **7.417 × 10⁻⁹ m s⁻²** at 7331 km. Neglecting them while adjusting orbit parameters appears as a **4 mm** reduction in orbit radius, at all heights | `TN36-10` §10.3 |
| `PERT-P-4` | Lense–Thirring and de Sitter | 10⁻¹¹ to 10⁻¹² of the main acceleration | 7.41683 m s⁻² × 1 × 10⁻¹¹ = **7.417 × 10⁻¹¹ m s⁻²**; as orbital-plane precession, **0.8 mas/yr** (geostationary) to **180 mas/yr** (low orbit) for Lense–Thirring and **19 mas/yr** for de Sitter, independent of height | `TN36-10` §10.3 |
| `PERT-P-5` | the solid Earth pole tide at a typical wobble of 0.3″ | 1.333 × 10⁻⁹ × 0.3 = **3.999 × 10⁻¹⁰** in ΔC̄₂₁ | 5.61369 m s⁻² × 3.999 × 10⁻¹⁰ × 3.872983 = **8.6945 × 10⁻⁹ m s⁻²** at 7331 km | `TN36-6` §6.4 |
| `PERT-P-6` | the ocean pole tide, degree 2 | ΔC̄₂₁ ≈ −2.1778 × 10⁻¹⁰ *m*₁ | about a sixth of the solid Earth pole tide, and of the opposite sense in its cross term | `TN36-6` (6.24) |
| `PERT-P-7` | ocean tides, integrated | several cm, reaching **20 cm**, over one day for a satellite at 800 km | the FES2004 main waves are about **80 %** of that; the admittance waves of `PERT-R-023` are most of the rest | `TN36-6` §6.3.2, "Influence of tidal models" |
| `PERT-P-8` | agreement with the term-by-term published amplitudes of Tables 6.5a and 6.5c | the printed digits, which are 0.1 × 10⁻¹² | — | `PERT-A-001` |
| `PERT-P-9` | third-body attraction, the indirect term | **not optional** — at the Moon it is of the same order as the direct term | — | `PERT-R-050` |

**Every one of these is larger than the static field's truncation error, and `PERT-P-1` is
larger by more than three orders of magnitude.** That is the argument for this step: a degree-90
static field accepts 1.2 × 10⁻¹⁰ m s⁻² of truncation, and the degree-2 solid Earth tide alone is
2.2 × 10⁻⁷ m s⁻². The spectral character differs — the tide is periodic at diurnal and
semidiurnal frequencies where truncation error is periodic at *N* per revolution — so neither
converts to a position error by the same rule, and `SPEC-template.md` §7 forbids pretending
otherwise.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `PERT-F-001` | a tide increment whose tide system disagrees with the field it is being added to | both systems, both sources, and which step of `TN36-6` §6.2.2 reconciles them | add them; convert silently |
| `PERT-F-002` | scaling constants mixed between `TN36-1` and EGM2008 — *a*_E = 6 378 136.6 m against *a*ₑ = 6 378 136.3 m, or the TT and TCG GM⊕ | both values and both sources | use either; assume they are the same number |
| `PERT-F-003` | a `Wobble` requested without a secular pole | that the secular pole is required and where it comes from | default it to zero |
| `PERT-F-004` | polar motion requested outside the EOP series | delegated to `SPEC-eop.md`'s own refusal | extrapolate |
| `PERT-F-005` | an ocean tide degree or order beyond what `FES2004-CS` carries | the request and the file's own limit, separately for degree and order | clamp silently |
| `PERT-F-006` | a tidal constituent requested that is in neither the tables nor the admittance list | the constituent and the two lists searched | interpolate from an arbitrary neighbour |
| `PERT-F-007` | S₁ or S₂ offered as a pivot wave for admittance interpolation | the wave, and that `TN36-6` §6.3.2 excludes the radiational tides from that role | use it |
| `PERT-F-008` | third-body attraction requested for a body outside the loaded ephemeris | delegated to `SPEC-ephemerides.md` `EPH-F-003` | return zero |
| `PERT-F-009` | a relativistic correction requested with PPN parameters that are not stated | that β and γ must be given explicitly | default them silently |
| `PERT-F-010` | a coefficient or model file from outside the manifest cache | the path and the cache root | read it |
| `PERT-F-011` | an ocean tide file whose header does not state the model, units, degree, order and inclusions this specification was written against | the header as found and the header expected, line by line | trust the chapter's description of the file over the file's own |
| `PERT-F-012` | a body for which no gravitational parameter is pinned | the body, its NAIF id, the file searched and how many bodies it carries | guess one; a third-body term with a guessed GM is a wrong number of the right size |
| `PERT-F-013` | third-body attraction requested without the Sun or without the Moon | which is missing, and that their accelerations at 7331 km are about 3 × 10⁻⁷ and 1 × 10⁻⁶ m s⁻² | treat them as optional like the planets |

`PERT-F-005`'s override, and it is the only one in this specification, per `R-ERR-3`:
`truncate_ocean_tide_to_file_limit`, set per run and recorded with the degree and order it was
used for. The refusal is right by default because a silently clamped ocean tide is a different
model; the override exists because asking for degree 120 from a degree-100 file is a reasonable
thing to do once, deliberately, and be told about.

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `PERT-A-001` | **THE GATE, term by term.** Every constituent of `TN36-6` Tables 6.5a and 6.5c, evaluated at θ_f = 0 so that (6.8b) with η₁ = −*i* returns ΔC̄₂₁ = Amp(op) × 10⁻¹² and ΔS̄₂₁ = Amp(ip) × 10⁻¹². **The gate reports two counts, not one: the terms it checked, and the terms of this module that the Conventions do not print an expected value for.** A count of passed terms without its denominator is what hid 868 ephemeris cases at step 1. | the printed in-phase and out-of-phase amplitudes | `TN36-6` Tables 6.5a, 6.5c — **published, per constituent** | the printed digit, 0.1 × 10⁻¹² | R-014, R-016, P-8 |
| `PERT-A-002` | the extracted tables against the printed ones: an independently hand-audited subset of at least twenty rows spanning all three tables, **and** the internal relation Amp(ip)·δ*k*_f^I = Amp(op)·δ*k*_f^R on **every** row, which no column mis-parse survives | agreement | `TN36-6` Tables 6.5a–c | the printed digits | R-014 |
| `PERT-A-003` | `TN36-6` Table 6.5b, the zonal band, likewise at θ_f = 0 through (6.8a) | the printed amplitudes | `TN36-6` Table 6.5b | the printed digit | R-014 |
| `PERT-A-004` | **Step 3**: the permanent tide removed, ΔC̄₂₀^perm = (4.4228 × 10⁻⁸)(−0.31460)*k*₂₀, and that a zero-tide field plus an unadjusted tide model differs from a correct one by exactly that | as printed | `TN36-6` (6.13), (6.14) | the printed digits | R-004, R-005 |
| `PERT-A-005` | **the solid Earth pole tide, reproduced from its constants**: Ω² *a*_E³ *k*₂^R/(GM⊕√15) × 1″ | **1.333 237 × 10⁻⁹**, against the printed 1.333 × 10⁻⁹; and the cross-term ratio Im(*k*₂)/Re(*k*₂) = 0.011 700 against the printed 0.0115, **which the test reports rather than reconciles** | `TN36-6` §6.4 with `TN36-1`'s constants | 10⁻³ relative on the leading coefficient | R-030, R-031, R-008 |
| `PERT-A-006` | **the ocean pole tide, reproduced from its file**: *R*₂ and γ with `DESAI-CO`'s Ā₂₁, B̄₂₁ returning the printed (6.24) coefficients | **−2.1778 × 10⁻¹⁰** and **−1.7232 × 10⁻¹⁰** **per second of arc of *m*₁, *m*₂**, with the cross terms −0.017 24 and −0.033 65. (6.23a) is written for *m* in **radians** and §6.5's printed result for *m* in **arcseconds**; the factor between them is 206 264.8 and a reviewer reproducing this lost minutes to it | `TN36-6` (6.24) — published, and **already reproduced to every printed digit** during drafting, §4.4 | the printed digits | R-032, R-033 |
| `PERT-A-025` | **the δ*k*_f column verified, not transcribed**: `TN36-6` (6.9) with Table 6.4's resonance parameters and (6.10)'s σ_α, evaluated at each diurnal constituent's own frequency, against the δ*k*_f^R and δ*k*_f^I printed in Table 6.5a. **The count of constituents checked and the worst disagreement MUST be reported**, and constituents the formula does not cover — the Conventions say it is inexact for a few — MUST be counted separately rather than dropped | the printed δ*k*_f | `TN36-6` (6.9), (6.10), Table 6.4 — **an independent route the production path does not use** | stated by the measurement, since `TN36-6` says (6.9) is inexact for some constituents and does not say by how much | R-016, R-017 |
| `PERT-A-027` | **the two argument conventions agree**: for all 71 constituents of Tables 6.5a/b/c, which print both the Doodson and the Delaunay multipliers, θ_f computed each way agrees modulo 2π. This is what makes one set of fundamental arguments serve a Doodson-indexed ocean tide and a Delaunay-indexed solid Earth tide | agreement | `TN36-6` Tables 6.5a/b/c, which print both | 10⁻⁹ rad | R-015 |
| `PERT-A-028` | **the GM file against `TN36-1`**: GM_sun from `gm_de440.tpc` against `TN36-1`'s, and GM_moon against μ × GM_earth. The first differs by **exactly *L*_B**, the TDB/TCB rate — a time-scale convention, the same family as `GRAV-R-006`'s TT/TCG trap, and not an error | relative difference 1.5505 × 10⁻⁸, matching *L*_B to six digits | `TN36-1` against NAIF's `gm_de440.tpc` | 1 % of *L*_B | R-050, F-012 |
| `PERT-A-024` | **the ocean tide file's header is read, not assumed**: unit 10⁻¹¹, degree and order (100, 100), long period from FES2002 to (50, 50), Ω₁/Ω₂ included, atmospheric tide excluded — and that Ω₁/Ω₂ are **not** added a second time | as the file's own first three lines state | `FES2004-CS`'s header | exact | R-020a, R-025, F-011 |
| `PERT-A-026` | **the ocean tide truncation default, measured against its stated criterion**: the truncation error of the sum against the fixed threshold of `PERT-R-022a`, at 7331 km and at 300 km altitude, with the degree each radius returns and the default the larger | **measured: degree 36 and degree 89, so the default is 89.** No number was in this specification before the measurement | `PERT-R-022a`'s criterion | the criterion itself | R-022, R-022a |
| `PERT-A-007` | **the zonal ocean-tide convention**: ΔS̄*ₙ*₀ is zero after (6.15), and ΔC̄*ₙ*₀ is **not** additionally halved. A build that applies (6.15) uniformly produces a non-zero ΔS̄*ₙ*₀, and the test asserts that this one does not | ΔS̄*ₙ*₀ = 0 exactly | `TN36-6` §6.3.2 | exact | R-021 |
| `PERT-A-008` | **the wobble sign**: *m*₂ = −(*y*_p − *y*_s). Flipping it inverts the phase of the pole tide while leaving its amplitude unchanged, so the test compares a **phase**, not a magnitude | inverted phase | `TN36-7` (25) | — | R-007 |
| `PERT-A-009` | **one secular pole**: the pole this module uses is `gravity`'s object. Perturbing `gravity`'s definition moves this module's pole tide, and the four constants of `TN36-7` (21) appear nowhere in this module's source | as stated | this spec, `GRAV-R-029` | exact | R-006 |
| `PERT-A-010` | relativity, by term, against the magnitudes `TN36-10` §10.3 states: Schwarzschild a few parts in 10⁹ at low orbit, Lense–Thirring and de Sitter 10⁻¹¹ to 10⁻¹² of the main acceleration | as stated | `TN36-10` §10.3 | order of magnitude, which is what the source states | R-040, R-042, P-3, P-4 |
| `PERT-A-011` | relativity, as **secular precession** rather than acceleration: the Lense–Thirring nodal rate between 0.8 mas/yr at geostationary and 180 mas/yr at low orbit, and the de Sitter rate 19 mas/yr **independent of height** — the height independence is the sharper of the two checks | as stated | `TN36-10` §10.3 | 20 %, the precision of the source's own statement | R-040, R-042 |
| `PERT-A-012` | PPN β and γ are live: setting γ = 1.1 changes the Schwarzschild term measurably, so they are parameters and not decoration | as stated | `TN36-10` (10.12) | — | R-040, R-072 |
| `PERT-A-013` | relativity: **R** is heliocentric and **r** geocentric. Substituting one for the other changes the de Sitter term by orders of magnitude, and the test asserts the correct one is in use by checking the term against `PERT-A-011` | as stated | `TN36-10` §10.3 | — | R-041 |
| `PERT-A-014` | **the third-body indirect term is not optional**: the acceleration with and without it, for the Moon and for the Sun, at 7331 km | the difference is of the same order as the direct term itself | analytic | — | R-050, P-9 |
| `PERT-A-015` | third body against the closed form for a distant point mass, and the **cancellation measured rather than assumed**: the relative error of the naive difference against a rearrangement that avoids differencing nearly equal quantities, for the Moon and the Sun | measured, and reported | analytic | — | R-051 |
| `PERT-A-016` | third body: the body list is recorded, **the Sun and Moon cannot be omitted** (`PERT-F-013`), the planets can, and positions arrive through the BCRS→GCRS translation rather than by any other route | as stated | this spec, `FRAME-R-028` | — | R-052, R-053, R-073, F-013 |
| `PERT-A-017` | composition: increments are summed once and applied once, to the **conventional** coefficients; and a field records which models were applied with their parameters | as stated | this spec | exact | R-060, R-061, R-062 |
| `PERT-A-018` | refusal: a tide system mismatch; a `Wobble` without a secular pole; an ocean tide degree beyond the file, with and without the named override; a constituent in neither the tables nor the admittance list | `PERT-F-001`, `-F-003`, `-F-005`, `-F-006`, each naming what §7 requires | this spec | — | F-001, F-003, F-005, F-006, R-071 |
| `PERT-A-019` | refusal: constants mixed between `TN36-1` and EGM2008; S₁ offered as a pivot wave; PPN parameters unstated | `PERT-F-002`, `-F-007`, `-F-009` | this spec | — | F-002, F-007, F-009, R-009 |
| `PERT-A-020` | structural: a `TideIncrements` cannot be used where an acceleration is wanted; an integer body code cannot cross the interface; `RelativityTerms` is not an integer; no global state | compile failure, and the runtime independence check | this spec | — | R-003, R-070, R-072, R-073, R-074 |
| `PERT-A-021` | every model file came from the manifest cache and its hash verified before this module saw a path | as stated | plan rule R11 | — | F-010 |
| `PERT-A-022` | Step 1 is evaluated in the **ITRS**: substituting GCRS coordinates for the Sun and Moon changes the diurnal signal completely, and the test asserts the ITRS route is in use | as stated | `TN36-6` (6.6) | — | R-012 |
| `PERT-A-023` | the normalised Legendre functions used in Step 1 are `gravity`'s, checked by the same reference values `GRAV-A-003` uses | agreement | `SPEC-gravity.md` §4.4 | ≤ 10⁻¹³ to degree 4 | R-013 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `PERT-R-001`, `PERT-R-002` | Structural, and discharged by `PERT-A-020`: the two kinds have no common type and no conversion. |
| `PERT-R-010`, `PERT-R-011` | Exercised by `PERT-A-001` and `PERT-A-004`, which drive Step 1 and Step 2 together; the Love-number column in force is recorded and checked by `PERT-A-017`'s model record. |
| `PERT-R-020` | Discharged by `PERT-A-007` and by the model record of `PERT-A-017`. *(`PERT-R-022` was in this row until v1.1 added `PERT-A-026`, which tests it directly; leaving it here would have been an excuse for something a test covers.)* |
| `PERT-R-023`, `PERT-R-024` | Optional models. `PERT-A-019` tests the one hard rule among them (S₁/S₂ as pivots); the rest are tested when a consumer asks for them. Ω₁/Ω₂ moved out of this row at drafting: `PERT-R-025` now forbids applying the chapter's equilibrium equation because the file already contains those waves, and `PERT-A-024` tests it. |
| `PERT-F-004`, `PERT-F-008` | Delegated, and tested where they live: polar motion outside the EOP series is `SPEC-eop.md`'s refusal and a body absent from every kernel is `EPH-F-003`, which `EPH-A-014` exercises. Re-testing another module's refusal here would assert that this module forwards it, which `PERT-A-016` and `PERT-A-018` already do for the cases that reach this interface. |
| `PERT-R-034` | The consumer that would misuse the degree-1 terms is station displacement, which is not in this plan. The requirement is that they are not exported unnamed; discharged structurally. |
| `PERT-R-043`, `PERT-R-044` | Statements recorded with the result, not behaviours. Discharged by `PERT-A-017`'s model record and by §9. |
| `PERT-R-075` | Discharged by `PERT-A-020` and by `SPEC-frames.md` `FRAME-A-025`, which owns the unit-crossing rule. |

### What `PERT-A-001` can fail on, and what it cannot

**v1.0 of this section called `PERT-A-001` category 1 and "a stronger position than either of the
preceding steps had". That was wrong, and wrong in this project's usual shape.** `PERT-R-014`
extracts Tables 6.5a/b/c as generated source, so the printed amplitudes are this module's
**input**. `PERT-A-001` then compares the module's output against those same amplitudes. It
therefore **cannot fail on a wrong amplitude**. The statement that was true of the smaller thing
— the numbers are published — was read as true of the larger one: that the gate checks the module
against them. It does not.

What it does check is worth a gate and is not a weak result, provided it is decomposed honestly:

| test | what it can fail on | what it cannot |
|---|---|---|
| `PERT-A-002`, the internal relation Amp(ip)·δ*k*_f^I = Amp(op)·δ*k*_f^R | **everything independent the tables contain.** δ*k*_f is printed and *H*_f is not, so *H*_f = Amp(ip)/(*A*ₘ δ*k*_f^R) is recoverable from either column, and this is the check that the two columns agree about it — on every row, for a quantity the Conventions never print | a transcription error that happens to preserve the ratio |
| `PERT-A-025`, (6.9) against the tabulated δ*k*_f | **the resonance structure** — σ_α and *L*_α — by an independent route the production path does not use. *Measured: printed ÷ formula on the real part is 1.0344 with a spread of 2.98 %, across a band where δ*k*_f itself varies by a factor of 2955. A negative control moving σ₂ by 7 × 10⁻⁴ takes that spread to 654 %.* | **the value of δ*k*_f**, which it cannot reproduce — see below |
| `PERT-A-001`, the θ_f = 0 evaluation | **the wiring**: a swapped in-phase and out-of-phase column, a wrong η_m, a sign, a mis-parsed field, a constituent dropped from the sum | a wrong amplitude, because the amplitude is what it was given |

**And `PERT-A-025` checks less than the ruling that created it assumed, for a reason in the
Conventions' own definition.** The text below (6.8e) defines δ*k*_f as the difference between
*k*_f and the nominal *k*₂₁ **plus a contribution from ocean loading**, and §6.2.1 says the
load-resonance corrections are *"incorporated through equivalent corrections to the body tide
Love numbers … also included in the tables"*. Equation (6.9) with Table 6.4's *k*₂₁ parameters
produces only the first part, so it **cannot** reproduce the tabulated δ*k*_f, and a test
demanding that it should would compare a partial quantity with a complete one. Measured, the
formula is 3.4 % low on the real part and wrong by a factor of two on the imaginary one.

What it can check, and does, is that the residual **retains no resonance**: because the loading
contribution shares the same denominators, the ratio of printed to formula must be constant
across the band. It is, to 2.98 %, while δ*k*_f varies by a factor of 2955. That is a sharp
statement about σ_α and *L*_α and it is not the statement the ruling asked for, so it is written
down as what it is.

By `SPEC-template.md` §8's ordering, then: category 1 for `PERT-A-004`, `-A-005`, `-A-006`,
`-A-010` and `-A-011` — the pole tides and the relativistic magnitudes, where the expected value
comes from a route this module does not use. `PERT-A-025` is category 2, an independent formula.
`PERT-A-001`, `-A-002` and `-A-003` are **category 4, self-consistency** — necessary, and the
template says plainly that a spec whose §8 contains only self-consistency tests is incomplete.
This one does not, but the distinction has to be visible or the count is a fiction.

Compare what the preceding two steps had. `GRAV-A-001` compares the field against a closed-form
identity the synthesis does not use; `EPH-A-001` compares against JPL's own published residuals.
Both are expected values produced by a route the module does not take. Step 2's frequency
corrections are not, and this is therefore a **weaker** position than either, not a stronger one.

### The gate's denominator

The Conventions print a per-term expected value for the **frequency-dependent corrections** of
Step 2 — every constituent of Tables 6.5a, 6.5b and 6.5c — and a closed-form expected value for
the **degree-2 pole tides** of §6.4 and §6.5. They print **no** expected value for:

- Step 1's time-domain evaluation at any epoch;
- the ocean tide sum at any epoch;
- the ocean pole tide above degree 2;
- the relativistic correction as a vector, as opposed to the magnitudes and precession rates of
  `PERT-A-010` and `-A-011`;
- third-body attraction, which the Conventions do not treat at all;
- and **Step 2's amplitudes as anything other than their own input** — the item above, stated
  here because it belongs in this list and not only in the paragraph that explains it.

**The one thing that would move Step 2's amplitudes out of that list is an independent *H*_f
catalogue**, from `CT71`/`CE73` or an equivalent harmonic expansion of the tide-generating
potential. Neither was obtained (`PERT-Q-004`), and that question stays open rather than closed:
if a later layer ever depends on Step 2 at a level where self-consistency is not enough, this is
where it reopens.

**The gate reports both counts on every run**, so that "the gate passed" never stands for more of
this module than the Conventions actually constrain.

---

## 9. Provenance obligations

- **Module register:** `tides`, `relativity`, `thirdbody` (or one module, per `PERT-Q-001`) →
  `TN36-6`, `TN36-7`, `TN36-10`, `TN36-1`, `FES2004-CS`, `DESAI-CO`.
- **Parameter register:** Table 6.3's Love numbers with the column in force; Table 6.4's
  resonance parameters if used; *k*₂ = 0.3077 + 0.0036 *i* for the pole tide; γ = 0.6870 +
  0.0036 *i* and the load deformation coefficients *k*′₂…*k*′₆ for the ocean pole tide;
  ρ_w = 1025 kg m⁻³; `TN36-1`'s *G*, GM⊕, *a*_E, *g*_E, Ω — **each cited to `TN36-1` and
  explicitly distinguished from EGM2008's pair**, which is `PERT-R-009`'s whole point.
- **Generated-source register:** Tables 6.5a, 6.5b, 6.5c and 6.7, extracted from the pinned
  `TN36-6` PDF by `tools/tides_from_conventions.py`, with the extraction's own hash of the
  source and the count of rows extracted from each table.
- **Data manifest:** `FES2004-CS` and `DESAI-CO` with URL, SHA-256, size and retrieval date,
  and `TN36-1`, `TN36-10` added to the chapters already pinned.
- **The discrepancy register:** `PERT-R-031`'s 0.0115 against 0.011 700 is a disagreement inside
  a normative document and belongs beside `PROVENANCE.md` §8.2 (the C04 product contradicting
  itself) and §8.3 (`interp.f` omitting a correction the Conventions require). It is the third
  of its kind and the pattern is worth naming.
- **§3.11 point 4 check:** `FES2004-CS` and `DESAI-CO` are data files with no build system.
  The Conventions' Fortran for chapter 6 is **not** consumed, on the standing ground recorded in
  `tools/tides_from_conventions.py`: it carries no licence at all, and the tables are the
  content of the standard, published so that they can be implemented.

---

## 10. Open questions for the manager

| id | question | **resolution** |
|---|---|---|
| `PERT-Q-001` | **One module or three?** `thirdbody` needs `ephemerides` and nothing else; `relativity` needs neither `ephemerides` nor `gravity`; `tides` needs both plus `eop` and two large coefficient files. The plan names one step, but a step is not a module. | **RULED 2026-09-18: three link targets, one specification.** Disjoint dependencies, and L0 settled that *in C++ a directory is not a boundary, a link target is*. One step in the plan with three targets in it is no conflict with the sequence rule. |
| `PERT-Q-002` | **`de440t` and TT−TDB resolve here**, as agreed when step 1 closed: `de440s.bsp` carries no TT−TDB record, so `EPH-A-007` could not compare two routes on it, and this is the step where TT−TDB is first consumed. | **RULED 2026-09-18: `de440t.bsp` SUBSTITUTED for `de440.bsp`, not added**, the condition being that it carry the same planetary segments over the same span — **verified, not assumed**: reading both DAFs' segment summaries directly, de440t has de440's 14 segments with identical frame, type and span, plus one more (target 1000000001 about centre 1000000000, the TT−TDB record). The manager's reasoning is not the one this question assumed: accuracy is not the argument — at 1 µs of TDB−TT error the Sun moves 30 mm and the third-body acceleration by 3.5 × 10⁻¹⁵ m s⁻², which nothing here can see — the argument is that `tdb_minus_tt` had **no independent check anywhere**, and the failure it is exposed to is a wrapping, unit or sign failure, where 1 ms puts the Sun 30 m out. Under **plan §5 constraint 9** the change re-ran step 1's full sweep in this step: 11 354 of 13 201, 1.06296 mm, unchanged. **`EPH-A-007` now runs for the first time: 53 epochs, worst 26.027 ns against a 100 ns budget.** Note the host — de440t is published by JPL SSD, not NAIF, whose planetary directory has no such file. |
| `PERT-Q-003` | **The tabulated δ*k*_f, or the resonance formula (6.9)?** The Conventions give both: Table 6.4's resonance parameters generate the frequency dependence, and Tables 6.5a/c print the resulting corrections. | **RULED 2026-09-18: the tables in production, (6.9) in the test.** Better than what this specification proposed. It is not two answers to one question but one production route and one independent check — `PERT-A-002`'s pattern — and it is **the only part of Step 2 the Conventions let this tree verify without an *H*_f catalogue.** It turns δ*k*_f from a transcription into a verified quantity. `PERT-R-016`, `PERT-R-017` and `PERT-A-025`. |
| `PERT-Q-004` | **`CT71` and `CE73` were not obtained.** They define the amplitude convention *H*_f that (6.8) is written in. | **RULED 2026-09-18: no action, and do not close it.** §8's denominator now records that an independent *H*_f catalogue is the one thing that would make Step 2's amplitudes checkable rather than self-referential. If a later layer ever depends on Step 2 at that level, this is where it reopens. |
| `PERT-Q-005` | **The chapter tells you to add something the data file already contains.** §6.3.2 describes modelling Ω₁ and Ω₂ as equilibrium waves and gives the equation; `FES2004-CS`'s own header says they are already in it. Reading the chapter alone, one implements the equation and doubles them. The chapter also records an unresolved dispute — dated 2011-10-14 — about whether the π/2 phase change introduced in its 2011-09-23 update is justified. | **RULED 2026-09-18 as recommended, and the manager verified the header independently** — rows `55.565 Om1` and `55.575 Om2` are the file's first two data lines. Do not apply the equation; read the header. `PERT-R-020a`, `PERT-R-025` and `PERT-F-011` stand, and it goes into the discrepancy register **named as its own kind** rather than folded into the other two. |
| `PERT-Q-006` | **Two numbers in one paragraph of `TN36-6` §6.4 disagree.** The printed cross-term ratio 0.0115 implies Im(*k*₂) = 0.003 539; the printed *k*₂ is 0.3077 + 0.0036 *i*, giving 0.011 700. 1.7 % of the imaginary part, 0.02 % of ΔC̄₂₁. | **RULED 2026-09-18 as recommended: follow *k*₂, report both, record it.** The manager reproduced both numbers independently. |
| `PERT-Q-007` | **How far to take the ocean tides?** `FES2004-CS` carries degree and order 100. Full degree is 100× the work of degree 10 and the Conventions say only that the main waves are about 80 % of a 20 cm effect. | **RULED 2026-09-18: the method approved, the number refused.** *A number in a specification acquires authority whatever the changelog says about how it got there*, so the criterion is stated **before** the measurement and the measurement sets the default. The criterion, now `PERT-R-022a`: the truncation error is below the smallest term this module computes and keeps, at the same evaluation point, measured at **two radii** because (*a*ₑ/*r*)ⁿ makes it strongly altitude-dependent, and the default is the larger of the two degrees. If it says 30, it is 30. |
| `PERT-Q-010` | **`Ephemeris::state` returns `State<Frame::BCRS>` whatever centre is asked for.** Asked for the Moon about the Earth it returns a geocentric vector typed as barycentric. The frame is in the type, which `FRAME-R-004` requires, but the ORIGIN is a runtime argument the type does not carry, so the tag can say something false about the vector. Found while implementing `PERT-R-053`, which had assumed the barycentric route. | Not this step's to fix, and flagged rather than worked around: `thirdbody` asks for Earth-centred vectors and converts them itself, with the reason in the source. The choices are to make the centre part of the type, to refuse a non-SSB centre, or to say in `SPEC-ephemerides.md` that the tag means axes and not origin — which would weaken what `FRAME-R-004` promises everywhere else. **Yours, because it touches an adopted specification's public surface.** |
| `PERT-Q-008` | **The gate's wording again.** The plan says *the IERS Conventions worked examples, term by term*. Chapter 6 prints **no worked examples** — no numerical case with inputs and outputs. What it does print, per constituent, is Tables 6.5a/b/c's amplitudes, and, in closed form, §6.4's and §6.5's pole-tide coefficients. | **RULED 2026-09-18: the plan is corrected and this reading adopted** — the tables meet the gate better than a worked example would, because an example checks one epoch and Table 6.5a checks every constituent separately. Plan §4 now carries **rule 4**, naming all three instances: *where a step's gate names a form of evidence, the first thing its specification does is say whether the source prints that form, and if not, propose what it prints instead.* §3.3's step 3 gate is reworded to two counts. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.2 | 2026-09-18 | **Amended by implementation, six corrections.** (1) **`PERT-A-025` checks less than the ruling that created it assumed**: `TN36-6` defines δ*k*_f as the body-tide difference *plus an ocean-loading contribution*, so (6.9) cannot reproduce it — measured, 3.4 % low on the real part and a factor of two on the imaginary. What it does check is the resonance structure, and §8 now says so, with a negative control. (2) `PERT-R-022a`'s threshold was self-referential and degenerated to "keep everything"; it is now `TN36-6` §6.2.1's own 3 × 10⁻¹² cutoff, and **the measured default is degree 89**. (3) `PERT-R-015` said the fundamental arguments come from `frames`; they live in `eop` and were promoted to its public surface, with `PERT-A-027` checking the two conventions against each other. (4) `PERT-R-053` said third-body positions come through the barycentric translation; they are taken Earth-centred in one call, and `PERT-Q-010` records why. (5) `PERT-F-012` and `PERT-F-013` added. (6) `PERT-A-028` added: the pinned GM file differs from `TN36-1` by **exactly *L*_B**. |
| 1.1 | 2026-09-18 | **Adopted, with the three required amendments and rulings on all eight questions.** (1) **§8's claim for `PERT-A-001` was wrong and is rewritten.** `PERT-R-014` makes the printed amplitudes this module's *input*, so the test cannot fail on a wrong amplitude; calling it category 1 and "a stronger position than either of the preceding steps had" was the project's usual shape — true of the smaller thing, read as true of the larger. §8 now decomposes what each of `PERT-A-001`, `-A-002` and `-A-025` can and cannot fail on, and marks the first two category 4. (2) `PERT-A-006` and §4.4 now state the **unit**: (6.23a) is in radians and §6.5's printed result in arcseconds, a factor of 206 264.8 between two equations on one page, which cost the reviewer minutes. (3) §8's denominator gains Step 2's amplitudes-as-their-own-input, and the note that an independent *H*_f catalogue is what would remove them from it. `PERT-R-016`/`-017` and `PERT-A-025` add the resonance formula as a test-only cross-check; `PERT-R-022a` and `PERT-A-026` state the ocean-tide truncation **criterion** without stating a number. |
| 1.0 | 2026-09-18 | First draft, L2 step 3, for manager review. |
