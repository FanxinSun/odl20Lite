# SPEC-gravity — the Earth's static gravitational field

| | |
|---|---|
| **Spec ID** | `GRAV` |
| **Status** | **adopted** 2026-09-18; **amended by implementation the same day**, six corrections at v1.2 — see the changelog |
| **Version** | 1.3 |
| **Date** | 2026-09-18 |
| **Layer** | L2 `environment`, step 2 (`doc/REWRITE_PLAN.md` §3.3) |
| **Depends on** | `SPEC-frames.md` (the ITRS the field is fixed in), `SPEC-time.md` (the TT argument of the secular rates), `core` |
| **Depended on by** | the gravitational force (L4), the variational equations (L3/L7), tides (L2 step 3, which perturbs these same coefficients) |

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

**One exception to the "no implementation" clause, declared because it is a real one.**
`EGM2008_Spherical_Harmonics.zip` contains `hsynth_WGS84.f`, a FORTRAN harmonic-synthesis
program. It was **not opened**. Its presence is recorded in §2 and §10 `GRAV-Q-002` because
the same archive contains that program's published test output, and whether to use that
output is a decision for the manager — but the program itself is exactly what the step's
scope says not to take recursions from, and it was left closed.

---

## 1. Purpose and scope

This module answers one question: **what acceleration does the solid Earth's static mass
distribution produce at a point, and what is the potential there.** It is the largest force
on any Earth satellite after the two-body term, and the only one in L2 whose error budget is
set by how many terms are kept rather than by how well a model is known.

### In scope

- The geopotential expanded in fully normalised spherical harmonics to the **full degree and
  order the published model carries** — EGM2008, degree 2190, order 2159.
- Loading, checking and provenance of the published coefficient set.
- The **conventional** model of `TN36-6` §6.1: the low-degree substitutions of its Table 6.2,
  their secular rates, and the C̄21/S̄21 figure-axis terms of its equation (6.5).
- The potential, and its gradient, in the ITRS.
- Truncation: what it costs, stated and measured, at a caller-chosen degree and order.
- The numerical representation of the associated Legendre functions at ultra-high degree,
  which at 2190 is not a detail but the whole difficulty.

### Not in scope

| excluded | owned by |
|---|---|
| Solid Earth, ocean and pole **tides** — the time-varying part of these same coefficients | L2 step 3 |
| Third-body attraction, and the relativistic correction | L2 step 3 |
| Atmospheric density and drag | L2 step 4, L4 |
| Rotating the acceleration out of the ITRS | `SPEC-frames.md` |
| Positions of the Sun, Moon and planets | `SPEC-ephemerides.md` |
| The **gravity-gradient tensor** ∂a/∂r, needed by the variational equations | L3/L7 — see `GRAV-Q-006` |
| The geoid, height anomalies, the normal field as a *model* — `WGS84`'s normal gravity enters here only as one published number in one band check | nothing in this plan; see `GRAV-Q-002` |
| Gravity fields of bodies other than the Earth | not in this plan |

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `TN36-6` | IERS | *IERS Conventions (2010)*, Technical Note 36, **chapter 6, Geopotential** | update of 01 Feb 2018 | `https://iers-conventions.obspm.fr/content/chapter6/icc6.pdf`, retrieved 2026-09-18, SHA-256 `abb3c0b0…4388` | primary | normative |
| `TN36-7` | IERS | *IERS Conventions (2010)*, chapter 7, **Displacement of reference points**, §7.1.4 (the secular pole) | update of 01 Feb 2018 | `https://iers-conventions.obspm.fr/content/chapter7/icc7.pdf`, retrieved 2026-09-18, SHA-256 `ffe8ffb1…043d` | primary | normative |
| `TN36-1` | IERS | *IERS Conventions (2010)*, chapter 1 (the permanent tide and the two tide systems) | update of 2013 | `https://iers-conventions.obspm.fr/content/chapter1/icc1.pdf`, retrieved 2026-09-18 | primary | normative |
| `EGM08` | NGA | **EGM2008 spherical harmonic coefficients**, `EGM2008_to2190_TideFree`, inside `EGM2008_Spherical_Harmonics.zip` | 2008 (file dated 2008-03-28) | `https://earth-info.nga.mil/php/download.php?file=egm-08spherical`, retrieved 2026-09-18, SHA-256 `65a9072f…0fbd`, 109 351 360 bytes | primary | normative (dataset) |
| `EGM08-RM` | NGA | `README_WGS84_2.pdf`, inside the same archive — the scaling parameters, the record format and the tide system | 2008 | as `EGM08` | primary | normative |
| `WGS84` | NGA | *Department of Defense World Geodetic System 1984*, NGA.STND.0036_1.0.0_WGS84 | 1.0.0 | `https://earth-info.nga.mil/php/download.php?file=coord-wgs84`, retrieved 2026-09-18, SHA-256 `5edc1cf7…e512` | primary | normative (for the two numbers `GRAV-A-012` uses, and for nothing else) |
| `DLMF-14` | NIST | *Digital Library of Mathematical Functions*, chapter 14, **Legendre and related functions**, §14.6 and §14.10 | release 1.2.x, 2026-09-18 | `https://dlmf.nist.gov/14.6`, `https://dlmf.nist.gov/14.10` | primary | normative (the classical definition and recurrence the normalised recursion is derived from) |
| `NASA-TP` | Eckman, Brown, Adamo; reviewed by Gottlieb | *Normalization and Implementation of Three Gravitational Acceleration Models*, NASA/TP-2016-218604 | 2016 | `https://ntrs.nasa.gov/api/citations/20160011252/downloads/20160011252.pdf`, retrieved 2026-09-18, SHA-256 `7aa56a42…87ef` | primary | informative |
| `HF02` | Holmes, S. A. and Featherstone, W. E. | *A unified approach to the Clenshaw summation and the recursive computation of very high degree and order normalised associated Legendre functions* | J. Geodesy **76**(5):279–299, 2002, doi:10.1007/s00190-002-0216-2 | publisher's paywall | **not obtained** | informative |
| `PAVLIS12` | Pavlis, Holmes, Kenyon, Factor | *The development and evaluation of the Earth Gravitational Model 2008 (EGM2008)* | JGR **117**, B04406, 2012 | doi:10.1029/2011JB008916 | **not obtained** | informative |

Two rows deserve their own sentence, because the template says a spec that cites a paper its
author could not read is a weaker artefact than one that says so.

- **`HF02` is the standard citation for evaluating normalised Legendre functions at degree
  2190, and it was not obtained.** Nothing in §§4–8 rests on it. The requirement it would
  have supported — factor cos^m φ out of the recursion — is instead derived here from
  `TN36-6` (6.2b) and `DLMF-14`, and the property that makes it necessary is **measured** and
  stated as a number in §3.6 rather than cited. `HF02` is named because it is prior art and a
  reviewer will expect to see it, not because anything depends on it. See `GRAV-Q-003`.
- **`NASA-TP` is informative, not normative.** It is the only *obtained* source that treats
  the pole singularity and the stability of Legendre recursions together, and its conclusions
  are quoted in §3.6. Its algorithms are not adopted: its own study reaches degree 150, where
  the difficulty this module faces begins above 175 (§3.6).

---

## 3. Definitions and conventions

### 3.1 The expansion, and the frame it is fixed in

`TN36-6` (6.1): the geopotential at the point (r, φ, λ) expanded to degree *N*,

> *V*(*r*, φ, λ) = (GM / r) Σ*ₙ₌₀..ₙ* (*a*ₑ/*r*)ⁿ Σ*ₘ₌₀..ₙ* [ C̄*ₙₘ* cos *m*λ + S̄*ₙₘ* sin *m*λ ] P̄*ₙₘ*(sin φ), with S̄*ₙ*₀ = 0

where *r* is geocentric radius, φ geocentric **latitude** (not geodetic), λ longitude positive
east, and P̄*ₙₘ* the fully normalised associated Legendre functions of §3.3.

- **GRAV-R-001.** The coefficients are given **in an Earth-fixed frame**, so the module MUST take
  a position in the ITRS and return the acceleration in the ITRS. It MUST NOT rotate anything;
  that is `SPEC-frames.md`'s work and doing it in two places is how the two drift apart.
- **GRAV-R-002.** φ is **geocentric** latitude, defined by sin φ = *z*/*r*. A geodetic latitude
  reaching this module is a caller error, and §7 `GRAV-F-001` makes the interface unable to
  express it: the module takes a Cartesian ITRS position, never an angle pair.
- **GRAV-R-003.** The acceleration is **a = +∇V** with *V* as written above — the geodesy sign
  convention, in which *V* is positive and increases downwards. The interface MUST name this
  at the point of use. (The physics convention writes a = −∇Φ with Φ negative; both are right
  and mixing them is a sign error on the largest term in the system.)

### 3.2 The scaling parameters belong to the model

`TN36-6` §6.1 and `EGM08-RM` (2): the values distributed **with** EGM2008 are

| parameter | value | note |
|---|---|---|
| GM⊕ | 398 600.4415 km³ s⁻² | **TT-compatible**; the one to use here |
| *a*ₑ | 6 378 136.3 m | the model's reference radius, *not* a WGS 84 semi-major axis |
| GM⊕ | 398 600.4418 km³ s⁻² | TCG-compatible; for the two-body term when working in TCG |
| GM⊕ | 398 600.4356 km³ s⁻² | TDB-compatible |

- **GRAV-R-004.** GM and *a*ₑ are properties **of the coefficient set** and MUST travel with it.
  They MUST NOT be read from a general constants table, and the module MUST NOT expose a way
  to set one without the other.
- **GRAV-R-005.** The TT-compatible value is used, because `SPEC-frames.md` §3.5 makes the GCRS
  a TT-based frame and this tree integrates there. Choosing another MUST be explicit and MUST
  be recorded in the run's provenance.
- **GRAV-R-006.** `WGS84`'s GM = 3 986 004.418 × 10⁸ m³ s⁻² **is numerically the TCG-compatible
  value under another name**, so it cannot be refused on its value — v1.0 and v1.1 assumed it
  could and implementation found otherwise. What is refusable is the realistic mistake, which is
  taking **both** constants from WGS 84: its semi-major axis is 6 378 137.0 m against this
  model's *a*ₑ = 6 378 136.3 m. The pair MUST be checked as a pair (`GRAV-F-005`), and the GM's
  declared time-scale compatibility MUST be recorded in provenance, because a TCG-compatible GM
  used on a TT argument is wrong by an amount that matters (§6 `GRAV-P-4`) and no constant
  distinguishes the two.

### 3.3 Normalisation, and the one place it is not uniform

`TN36-6` (6.2a)–(6.2b): P̄*ₙₘ* = *N*ₙₘ *P*ₙₘ with

> *N*ₙₘ = √[ (*n*−*m*)! (2*n*+1) (2 − δ₀ₘ) / (*n*+*m*)! ] , δ₀ₘ = 1 if *m* = 0, else 0

and correspondingly C*ₙₘ* = *N*ₙₘ C̄*ₙₘ*.

- **GRAV-R-007.** The classical *P*ₙₘ is taken **without the Condon–Shortley phase**:
  *P*ₙᵐ(*x*) = (1−*x*²)^(*m*/2) dᵐ*Pₙ*(*x*)/d*x*ᵐ. `DLMF-14` (14.6.1) states the same definition
  **with** a leading (−1)ᵐ. Any reference implementation used in a test MUST have its
  convention established rather than assumed, because the two differ in sign on every odd
  order and the difference is invisible in the sum of |C̄| but not in the acceleration.
- The factor (2 − δ₀ₘ) is **not smooth in *m***: it is 1 at *m* = 0 and 2 for every *m* ≥ 1.
  That discontinuity is the trap of §3.5.

### 3.4 Tide systems

`TN36-1` and `TN36-6` §6.2.2 distinguish two conventions for the permanent deformation:

| system | C̄₂₀ contains the time-independent tidal contribution? |
|---|---|
| **zero tide** | yes |
| **conventional tide free** | no |

Measured from the sources, not restated from them:

| quantity | value | where from |
|---|---|---|
| C̄₂₀ in `EGM2008_to2190_TideFree` | −4.841 651 437 908 15 × 10⁻⁴ | the file's first record |
| C̄₂₀ of the conventional model, **zero tide** | −0.484 169 48 × 10⁻³ | `TN36-6` Table 6.2, Cheng et al. 2010 |
| C̄₂₀^zt − C̄₂₀^tf for EGM2008 | −4.1736 × 10⁻⁹ | `TN36-6` §6.2.2 |
| C̄₂₀ of the conventional model, tide free | −0.484 165 31 × 10⁻³ | `TN36-6` §6.2.2, and reproduced from the two rows above |
| conventional tide-free **minus the file's** | −1.6261 × 10⁻¹⁰ | computed here; 8× the 2 × 10⁻¹¹ uncertainty `TN36-6` §6.1 states |

The last row is the reason the substitution of `GRAV-R-020` is mandatory rather than cosmetic:
the Conventions' C̄₂₀ comes from seventeen years of SLR and EGM2008's from four years of GRACE,
and `TN36-6` §6.1 says plainly that they differ significantly. A model that keeps the file's
value is not the conventional model.

- **GRAV-R-008.** The tide system of the field the module produces MUST be stated in its output,
  not documented. L2 step 3 adds the tide variations, and `TN36-6` (6.13) requires the
  permanent part to be **removed** from those variations when the background field is zero
  tide. A consumer that cannot read the system off the field cannot get that right.

### 3.5 The one-line trap in the sectorial seed

The recursion of §4.4 starts from P̄*ₘₘ*. Its step,

> P̄*ₘₘ* = √[(2*m*+1)/(2*m*)] · cos φ · P̄*ₘ₋₁,ₘ₋₁*

is derived below and holds for every *m* ≥ 2. It does **not** hold at *m* = 1, because
(2 − δ₀ₘ) changes between *m* = 0 and *m* = 1. Verified here in 60-digit arithmetic:

| | value at *m* = 1 |
|---|---|
| the correct P̄₁₁ / cos φ | √3 = 1.732 050 807 568 877 |
| what the general seed gives | √(3/2) = 1.224 744 871 391 589 |
| ratio | √2 |

A √2 on every order-1 term, which includes C̄₂₁ and S̄₂₁ — the figure-axis terms of §3.7 — is
not a small error and it is not obviously wrong on inspection. `GRAV-A-008` exists for it.

### 3.6 Why the classical recursion cannot be used at degree 2190

This is the section that decides the implementation, so it states measurements rather than
citing a paper (see `GRAV-Q-003`).

The sectorial function carries a factor cosᵐφ. Evaluated as written, at high order it
**underflows a double** while the coefficient it multiplies does not, so the term is lost
rather than being small. Factoring that power out — writing P̄′*ₙₘ* = P̄*ₙₘ* / cosᵐφ, which is
what the recursion of §4.4 actually propagates — leaves a bounded quantity.

Measured here, by evaluating the seed product to *m* = 2190:

| quantity | value |
|---|---|
| P̄′₁₁ | 1.732 051 |
| P̄′₃₆₀,₃₆₀ | 6.547 027 |
| P̄′₂₁₅₉,₂₁₅₉ | 10.241 024 |
| P̄′₂₁₉₀,₂₁₉₀ | 10.277 577 |

**The *sectorial* function never exceeds 10.278 anywhere in the model. v1.0 and v1.1 of this
specification said "the factored function", and that is false away from the sectorial diagonal —
implementation found it and §3.6a now states it.** The unfactored function, at the same orders,
underflows the smallest normal double (2.225 × 10⁻³⁰⁸) at these latitudes:

| order *m* | classical P̄*ₘₘ* underflows above latitude |
|---|---|
| 360 | 82.0° |
| 2159 | 44.0° |
| 2190 | **43.7°** |

At the model's full order the classical form is lost over more than half the Earth's surface
by area. This is not an edge case at the poles; it is most of the planet.

`NASA-TP` §4.5 reaches a compatible conclusion from the other direction — that the stability
of every one of the three singularity-free algorithms it studies is set by the stability of
the Legendre generator inside it, and that *normalisation amplifies* whatever instability the
generator has. Its own trend study stops at degree 150, which is below where this begins.

### 3.6a Neither representation works alone, and the third thing that is needed

Away from the sectorial diagonal the factored function is **not** bounded. From the closed form
P̄′*ₙₘ*(1) = √((2*n*+1)(2−δ₀ₘ)) √((*n*+*m*)!/(*n*−*m*)!) / (2ᵐ *m*!), evaluated here:

| quantity | value |
|---|---|
| max over *m* of P̄′₂₁₉₀,ₘ(1) | **10⁴⁵⁷·⁸⁶⁴**, at *m* = 979 |
| largest double | 10³⁰⁸·²⁵ |
| overflow margin | **10¹⁵⁰** |

And it does not merely overflow. Once two consecutive entries of a column are infinite, the
three-term recursion subtracts one from the other and the column becomes **NaN** — a failure that
propagates silently into every sum it touches instead of announcing itself as a large number.

So the two representations fail in opposite directions and neither is usable on its own. What
works is both at once:

- **a global scale of 10⁻²⁸⁰ on every associated Legendre function**, which puts the range
  10⁻²⁷⁹ to 10¹⁷⁸ inside a double with room at both ends; and
- **folding the powers of cos φ back in through a Horner nest over order**, rather than forming
  cosᵐφ as a number at all.

There is no cancellation in that nest: the large intermediates are large precisely because they
are about to be multiplied by many factors of cos φ, and each step scales down rather than
subtracting.

10⁻²⁸⁰ is `HF02`'s constant and `HF02` could not be obtained. It is adopted here because the
measurement says it is right, and the measurement is of **both** ends. The top is the easy one:
10⁴⁵⁷·⁹ ÷ 10²⁸⁰ = 10¹⁷⁷·⁹, with 130 decades of headroom. The bottom is set by the **smallest
intermediate the recursion forms**, not by a unit result, and is the thinner side — so it is
measured rather than argued (`GRAV-A-029`):

| | |
|---|---|
| range of the scaled values over 74 802 of them | 1.055 × 10⁻²⁸² to 7.313 × 10¹⁷⁷ |
| decades above the smallest normal double | **26.0** |
| decades below the largest | **130.1** |
| subnormal values | **0** |

There is also a bound that does not need measuring, and it is what makes 26 decades enough
rather than merely observed. P̄′*ₙₘ* = P̄*ₙₘ*/cosᵐφ and cos φ ≤ 1, so **|P̄′| ≥ |P̄| everywhere**.
The scaled value can be subnormal only where P̄ itself is below 10⁻²⁸ — and a term that small
sits beside terms of order 1 to 66 in the same degree's sum, already far below that sum's own
rounding. Losing it costs nothing.

- **GRAV-R-030.** The recursion MUST propagate the factored function **scaled by 10⁻²⁸⁰**, and
  the powers of cos φ MUST be restored by a Horner nest over order. An implementation that forms
  P̄*ₙₘ* itself is non-conforming (it underflows above 43.7° at *m* = 2190); so is one that forms
  P̄′*ₙₘ* unscaled (it becomes NaN above degree ≈ 1800 at *m* = 979). `GRAV-A-007` demonstrates
  both failures rather than describing them.

### 3.7 The figure-axis terms need a pole model, and chapter 6 does not name which

`TN36-6` (6.5) gives the conventional C̄₂₁(*t*), S̄₂₁(*t*) in terms of xf(*t*), yf(*t*), "the
appropriate angular coordinates of the pole consistent with the mean figure axis corresponding
to the pole of the TRF defined in Chapter 4". It does not name a model. `TN36-7` §7.1.4, in
its 2018 update, **replaced** the "mean pole" of earlier Conventions with a linear **secular
pole** fitted to 1900–2017 observations:

> xs = 55.0 + 1.677 (*t* − 2000) mas, ys = 320.5 + 3.460 (*t* − 2000) mas, *t* in years of 365.25 days

Using it in (6.5) is the reading this specification takes, **ruled and confirmed on
2026-09-18** together with the condition of `GRAV-R-029`: the pole model is defined once, here,
and L2 step 3's pole tide consumes that definition rather than restating it. The term is worth having, and the reason is the **epoch
dependence** rather than its size at any one epoch — the file's C̄₂₁, S̄₂₁ are fixed while the
conventional ones move with the pole. Computed here from (6.5) and `TN36-7` (21):

| | C̄₂₁ | S̄₂₁ |
|---|---|---|
| as distributed in `EGM08` | −2.066 155 × 10⁻¹⁰ | +1.384 414 × 10⁻⁹ |
| conventional at J2000.0 | −2.264 385 × 10⁻¹⁰ | +1.299 633 × 10⁻⁹ |
| conventional at 2026.0 | −4.048 365 × 10⁻¹⁰ | +1.664 613 × 10⁻⁹ |
| difference from the file at 2026.0 | 1.9822 × 10⁻¹⁰ | 2.8020 × 10⁻¹⁰ |

At J2000.0 the substitution moves C̄₂₁ by 10 % and S̄₂₁ by 6 %, which is easy to dismiss. By
2026.0 the two differences together amount to a degree-2 amplitude of 3.4322 × 10⁻¹⁰, worth an
RMS acceleration of **7.4627 × 10⁻⁹ m s⁻²** at 7331 km by the identity of `GRAV-R-041` —
**sixty-one times the degree-90 truncation error** this module's LEO default accepts
(`GRAV-P-1`). A term sixty-one times the truncation budget is not one to leave to a later step.

### 3.8 Units and symbols

| symbol | meaning | unit |
|---|---|---|
| *r* | geocentric radius | m |
| φ, λ | geocentric latitude, east longitude | rad |
| *V* | gravitational potential | m² s⁻² |
| **a** | gravitational acceleration, +∇*V* | m s⁻² |
| C̄, S̄ | fully normalised coefficients | dimensionless |
| σ*ₙ* | degree amplitude, √(Σ*ₘ*(C̄²+S̄²)) | dimensionless |
| *N*, *M* | truncation degree and order in force | — |

---

## 4. Required behaviour

### 4.1 The coefficient set is a manifest input, and its file has a shape

- **GRAV-R-010.** The coefficient file is declared in the manifest with URL, SHA-256 and licence
  note (plan rule R11) and read from the cache. Reading one from anywhere else MUST be refused
  (`GRAV-F-007`).
- **GRAV-R-011.** The record format is `{n, m, C̄ₙₘ, S̄ₙₘ, σC̄ₙₘ, σS̄ₙₘ}` as `2i5, 2d25.15, 2d20.10`
  (`EGM08-RM` (3)). The exponent marker is FORTRAN `D`; a reader that only accepts `E` reads
  nothing. Free-format reading is permitted by the source and by this spec.
- **GRAV-R-012.** **Degrees 0 and 1 are absent from the file, and they are not both zero.**
  `TN36-6` (6.1) sums from *n* = 0 with C̄₀₀ carrying the two-body term, so **C̄₀₀ = 1**; the
  EGM2008 README writes the equivalent form with the 1 outside the sum and the sum starting at
  *n* = 2. Degree 1 is **zero**, for the quite different reason that the origin is the centre of
  mass. The reader MUST supply both, and MUST refuse a file that carries a non-zero degree-1
  coefficient (`GRAV-F-002`), since that would mean the coefficients are referred to some other
  origin. *(v1.0 and v1.1 said the reader must supply both as zero. Setting C̄₀₀ = 1 is what makes
  `GRAV-R-027`'s exact GM/r² at degree 0 a consequence of the same sum rather than a separate code
  path, and makes σ₀ = 1, which is what it should be.)*
- **GRAV-R-013.** **The file is padded to the full triangle with explicit zeros and the model's
  true extent cannot be read off its last record.** Measured here: the last record is
  (2190, 2190) and is zero; the highest order carrying a non-zero coefficient is 2159 at odd
  degrees above 2159 but **2158** at even ones. That parity pattern is a consequence of the
  ellipsoidal-to-spherical conversion behind EGM2008, which couples degrees of equal parity
  and preserves order. A reader MUST NOT infer *M* from the file.
- **GRAV-R-014.** The file contains **2 401 333** records, which is the full triangle to
  (2190, 2190) less the three absent records of degrees 0 and 1. The count MUST be checked on
  load and a mismatch refused (`GRAV-F-003`) — a truncated download that still parses is
  otherwise silent.
- **GRAV-R-015.** The distributed C̄₂₀ is in the **conventional tide-free** system
  (`EGM08-RM` (1)). The loader MUST record which file it read and in which system.

### 4.2 The conventional model of `TN36-6` §6.1

- **GRAV-R-020.** The coefficients C̄₂₀, C̄₃₀, C̄₄₀ of Table 6.2 **replace** the file's values:

  | coefficient | value at J2000.0 | rate / yr⁻¹ |
  |---|---|---|
  | C̄₂₀ (**zero tide**) | −0.484 169 48 × 10⁻³ | 11.6 × 10⁻¹² |
  | C̄₃₀ | 0.957 161 2 × 10⁻⁶ | 4.9 × 10⁻¹² |
  | C̄₄₀ | 0.539 965 9 × 10⁻⁶ | 4.7 × 10⁻¹² |

  propagated by (6.4), C̄*ₙ*₀(*t*) = C̄*ₙ*₀(*t*₀) + (dC̄*ₙ*₀/d*t*)(*t* − *t*₀), *t*₀ = J2000.0.
- **GRAV-R-021.** After the substitution the field is in the **zero-tide** system, and
  `GRAV-R-008` requires it to say so. The substitution MUST be explicit and recorded, never a
  silent edit of a loaded array.
- **GRAV-R-022.** C̄₂₁(*t*) and S̄₂₁(*t*) follow (6.5),

  > C̄₂₁ = √3 xf C̄₂₀ − xf C̄₂₂ + yf S̄₂₂ ,  S̄₂₁ = −√3 yf C̄₂₀ − yf C̄₂₂ − xf S̄₂₂

  with xf, yf in **radians**, and with C̄₂₀ = −0.484 169 48 × 10⁻³, C̄₂₂ = 2.439 383 6 × 10⁻⁶,
  S̄₂₂ = −1.400 273 7 × 10⁻⁶, which `TN36-6` §6.1 states is adequate for 10⁻¹⁴ accuracy in
  this equation.
- **GRAV-R-023.** xf, yf are taken from the secular pole of `TN36-7` §7.1.4 (§3.7), converted
  from milliarcseconds to radians. `GRAV-Q-005` asks the manager to ratify that reading.
- **GRAV-R-024.** The time argument of (6.4) and of the secular pole is **TT**, expressed in years
  of 365.25 days from J2000.0. It MUST arrive as an `Epoch` from `SPEC-time.md`, never as a
  bare year.
- **GRAV-R-025.** Every substitution the module makes MUST be enumerable at run time — which
  coefficient, from what value, to what value, on whose authority — and MUST appear in the
  run's provenance. A field that cannot say how it differs from the file it was loaded from is
  not auditable.

- **GRAV-R-029.** **The secular pole is defined once, in this module, and is exported.** L2
  step 3 needs the same four constants for the solid Earth and ocean pole tides and MUST consume
  this definition rather than restate it. Two definitions is how this module ends up secular and
  the pole tide ends up mean, and that inconsistency would be worth about what the substitution
  of `GRAV-R-022` is worth in the first place — which §3.7 measures at sixty-one times the
  truncation error the same field accepts. The export is part of this module's public surface
  (§5) for that reason and not because this module needs it to be.

### 4.3 Evaluation

- **GRAV-R-026.** The module MUST evaluate *V* and **a** at any point with *r* > 0, including on
  the polar axis, to any *N* ≤ 2190 and *M* ≤ min(*N*, 2159).
- **GRAV-R-027.** The two-body term is part of the field: at *N* = 0 the module MUST return
  exactly GM/*r*² directed at the origin.
- **GRAV-R-028.** *N* and *M* actually in force MUST be carried with every result. A caller
  cannot otherwise tell a degree-90 answer from a degree-2190 one, and every error budget in
  L4 depends on which it got.

### 4.4 The recursion

The Conventions print the expansion and the normalisation. **They print no recursion** — the
words "recursion" and "recurrence" do not occur in chapter 6 — so the recursion below is
derived from `TN36-6` (6.2b) together with the classical definition and degree recurrence of
`DLMF-14` ((14.6.1) with its Condon–Shortley phase removed per `GRAV-R-007`, and (14.10.3)
which for integer *n*, *m* reads (*n*−*m*)*P*ₙᵐ = (2*n*−1)*x* *P*ₙ₋₁ᵐ − (*n*+*m*−1)*P*ₙ₋₂ᵐ).

Writing *u* = sin φ, *c* = cos φ, and P̄′*ₙₘ* = P̄*ₙₘ* / *c*ᵐ:

| step | relation | valid for |
|---|---|---|
| seed | P̄′₀₀ = 1 | — |
| seed | P̄′₁₁ = √3 | *m* = 1 **only** (§3.5) |
| sectorial | P̄′*ₘₘ* = √[(2*m*+1)/(2*m*)] · P̄′*ₘ₋₁,ₘ₋₁* | *m* ≥ 2 |
| first step | P̄′*ₘ₊₁,ₘ* = √(2*m*+3) · *u* · P̄′*ₘₘ* | *m* ≥ 0 |
| general | P̄′*ₙₘ* = *aₙₘ* *u* P̄′*ₙ₋₁,ₘ* − *bₙₘ* P̄′*ₙ₋₂,ₘ* | *n* ≥ *m*+2 |

with *aₙₘ* = √[ (2*n*−1)(2*n*+1) / ((*n*−*m*)(*n*+*m*)) ] and
*bₙₘ* = √[ (2*n*+1)(*n*+*m*−1)(*n*−*m*−1) / ((2*n*−3)(*n*+*m*)(*n*−*m*)) ].

These were verified before being written down, at 60 significant digits against the definition
evaluated in exact rational arithmetic, over 0 ≤ *m* < 40, *m*+2 ≤ *n* < 60 for the general
step and *m* ≤ 60 for the seeds: worst relative disagreement 9.9 × 10⁻⁵⁸, which is the
arithmetic's own rounding.

- **GRAV-R-031.** The recursion above MUST be implemented as written, from these published
  sources. It MUST NOT be transcribed or adapted from any program, including the harmonic
  synthesis program shipped inside `EGM08` (front matter).
- **GRAV-R-031a.** The column is propagated **scaled**, per `GRAV-R-030`. The recursion is
  linear, so scaling the seed scales the column; the scale is removed once, per degree, **before**
  the (*a*ₑ/*r*)ⁿ factor is applied and not after — the other order underflows, because the scaled
  degree-2190 sum is about 10⁻²⁸⁹ and (*a*ₑ/*r*)²¹⁹⁰ at 7331 km is 10⁻¹³³.
- **GRAV-R-032.** *aₙₘ* and *bₙₘ* MUST be formed from exact integer expressions and then
  converted, not accumulated from previous values, so that no error compounds along a column.
- **GRAV-R-033.** The *m* = 1 seed MUST be separate from the general sectorial step (§3.5).
- **GRAV-R-034.** The summation over λ MUST fold *c*ᵐ back in progressively rather than forming
  *c*ᵐ and P̄*ₙₘ* separately and multiplying, since the whole purpose of §3.6 is that neither
  factor is separately representable.

### 4.5 The gradient, and the pole

In the factored form the gradient has **no** 1/cos φ anywhere:

- the λ-derivative contributes *m* *c*ᵐ⁻¹ after the 1/(*r* cos φ) of the spherical gradient,
  finite for *m* ≥ 1, and the *m* = 0 terms carry a factor *m* = 0 and vanish;
- the φ-derivative of *c*ᵐ P̄′*ₙₘ*(*u*) is −*m* *c*ᵐ⁻¹ *u* P̄′*ₙₘ* + *c*ᵐ⁺¹ dP̄′*ₙₘ*/d*u*, also finite.

- **GRAV-R-035.** The expression 1/cos φ MUST NOT appear in the implementation. The pole is an
  ordinary point of this module, not a special case, and `GRAV-A-006` evaluates there.
- **GRAV-R-036.** At *x* = *y* = 0 the longitude is undefined and MUST be set to zero, which is
  the convention `NASA-TP` §1.3.2 records and which gives the correct Cartesian components
  because the horizontal acceleration at the pole comes from the *m* = 1 terms alone.
- **GRAV-R-037.** The gradient MUST be analytic. A finite-difference gradient is a test
  (`GRAV-A-005`), never the implementation.

### 4.6 Truncation

- **GRAV-R-040.** Truncation is by **discarding** terms above *N*, *M*. No tapering, smoothing or
  windowing may be applied; a windowed field is a different field and no published number
  describes it.
- **GRAV-R-041.** For a field truncated at degree *N*, the RMS over a sphere of radius *r* of the
  discarded acceleration is, exactly,

  > rms‖**a** − **a**_*N*‖ = (GM/*r*²) · √( Σ*ₙ₌ₙ₊₁..₂₁₉₀* (*a*ₑ/*r*)^(2*n*) σ*ₙ*² (*n*+1)(2*n*+1) )

  which follows from the 4π normalisation alone: the mean square over the sphere of the
  degree-*n* angular factor is σ*ₙ*², the radial derivative contributes (*n*+1)² and the
  horizontal gradient contributes *n*(*n*+1). The module MUST be able to report this from the
  coefficients alone, without evaluating the field.
- **GRAV-R-042.** That identity is also the **gate** (`GRAV-A-001`): the field evaluated over a
  global sample must reproduce it degree by degree. It is the only check in this spec that
  reaches degree 2190, and it reaches every degree below it as well.
- **GRAV-S-043.** `TN36-6` Table 6.1's suggested truncations — degree 90 at 7331 km (Starlette),
  20 at 12 270 km (Lageos), 12 at 26 600 km (GPS) — SHOULD be the defaults offered. What is
  given up by ignoring them is the evaluation cost of §6 `GRAV-P-5`, not accuracy.

---

## 5. Interfaces

```
GravityModel::load(coefficients: Path[manifest cache],
                   scaling: ScalingParameters)          -> Result<GravityModel, GravityError>

GravityModel::conventional(base: GravityModel, at: Epoch[TT])
                                                        -> Result<ConventionalField, GravityError>

ConventionalField::acceleration(position: Position<Frame::ITRS>[m],
                                degree: Degree, order: Order)
                                                        -> Result<Acceleration<Frame::ITRS>[m/s^2], GravityError>

ConventionalField::potential(position: Position<Frame::ITRS>[m],
                             degree: Degree, order: Order)
                                                        -> Result<Potential[m^2/s^2], GravityError>

ConventionalField::acceleration_by_degree(position: Position<Frame::ITRS>[m],
                                          degree: Degree, order: Order)
                                                        -> Result<[Vec3][m/s^2]], GravityError>

ConventionalField::truncation_rms(radius: Length[m], degree: Degree)
                                                        -> Result<Acceleration[m/s^2], GravityError>

ConventionalField::tide_system()                        -> TideSystem
ConventionalField::substitutions()                      -> [Substitution]
GravityModel::provenance()                              -> Provenance
```

- **GRAV-R-049.** `acceleration_by_degree` returns the per-degree contributions, degree 0 first.
  It exists because `GRAV-A-001` checks the field against `GRAV-R-041`'s identity **degree by
  degree**, and forming those as differences of truncated evaluations would cost O(*N*²). It
  returns a value rather than filling a caller's buffer, per this section's convention.
- **GRAV-R-050.** `Position` and `Acceleration` are the frame-carrying types of
  `SPEC-frames.md`. A bare triple of doubles MUST NOT cross this interface in either
  direction, so a caller cannot hand in a GCRS position by mistake. *(`SPEC-frames.md` had no
  such types when this was written: it has `State`, which is a position AND a velocity AT an
  epoch, in km, and a point at which to evaluate a static field is none of those. `Position<F>`
  and `Acceleration<F>` were added to `frames` rather than declared here, on the manager's ruling
  for `EPH-Q-001` — the frame enumeration belongs to that specification and a second spec
  extending it silently is how enumerations drift. See `GRAV-Q-009`.)*
- **GRAV-R-051.** `Degree` and `Order` are distinct opaque types constructed through checked
  factories. They are both small integers and they are not interchangeable; passing (order,
  degree) where (degree, order) is meant MUST NOT compile.
- **GRAV-R-052.** `GravityModel` and `ConventionalField` are immutable after construction. The
  conventional substitutions produce a **new** value; they do not edit the loaded one, so the
  distributed coefficients remain available for `GRAV-A-009`.
- **GRAV-R-053.** `ScalingParameters` is constructible only as a matched (GM, *a*ₑ) pair
  (`GRAV-R-004`) and carries which time scale its GM is compatible with.
- **GRAV-R-054.** No global state: two `ConventionalField` values built at different epochs, or
  from different files, MUST answer independently and interleaved in one process.

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `GRAV-P-1` | truncation at Table 6.1's LEO row: degree 90 at *r* = 7331 km | RMS **1.2174 × 10⁻¹⁰ m s⁻²** | 1.2174 × 10⁻¹⁰ m s⁻² × 6246.8 s × 6246.8 s × 0.5 = **2.3753 mm**, which is the value for *T* = one revolution **if the error were secular**. It is not: its **spectral character is oscillatory at about ninety cycles per revolution**, amplitude *a*/(*Nn*)², which is 1.5 × 10⁻⁵ mm | computed here from `EGM08`'s own degree amplitudes by `GRAV-R-041` |
| `GRAV-P-2` | truncation at Table 6.1's Lageos row: degree 20 at *r* = 12 270 km | RMS **9.9122 × 10⁻¹² m s⁻²** | 9.9122 × 10⁻¹² m s⁻² × 13526.3 s × 13526.3 s × 0.5 = **0.90677 mm** on the secular reading over *T* = one revolution. **Spectral character: oscillatory at about twenty cycles per revolution**, amplitude 1.1 × 10⁻⁴ mm | as `GRAV-P-1` |
| `GRAV-P-3` | truncation at Table 6.1's GPS row: degree 12 at *r* = 26 600 km | RMS **2.3018 × 10⁻¹⁴ m s⁻²** | 2.3018 × 10⁻¹⁴ m s⁻² × 43175.1 s × 43175.1 s × 0.5 = **0.021454 mm** on the secular reading over *T* = one revolution. **Spectral character: oscillatory at about twelve cycles per revolution**, amplitude 7.5 × 10⁻⁶ mm | as `GRAV-P-1` |
| `GRAV-P-4` | using `WGS84`'s GM with EGM2008's coefficients | MUST be refused | 5.5821 × 10⁻⁹ m s⁻² × 6246.8 s × 6246.8 s × 0.5 = **108.91 mm** at 7331 km, from a GM differing by 3 × 10⁻⁴ km³ s⁻² over *T* = one revolution. **Spectral character: secular** — a constant fractional error in GM — so here ½*aT*² is the right instrument and not the wrong one | `TN36-6` §6.1 against `WGS84` Table 3.1 |
| `GRAV-P-5` | cost of one full-field evaluation | ≤ 100 ns per coefficient pair | 2 401 333 × 100 ns = **240.13 ms** per point at degree 2190. **Measured: 26 ms, or 11 ns per pair**, so the budget holds with a factor of nine in hand | `GRAV-R-014`'s record count; the reason `GRAV-S-043` exists |
| `GRAV-P-6` | agreement of the evaluated field with the degree-variance identity `GRAV-R-041` | **5/√(2*K*)** per degree, *K* stated by the test | the relative standard error of an RMS estimated from *K* samples is 1/√(2*K*); the band is five of them. At *K* = 300, 20 %; at *K* = 20 000, 2.5 % | `GRAV-A-001`, `GRAV-A-001b`; the row states its own formula per `SPEC-template.md` §8 |
| `GRAV-P-7` | the **sectorial** modified Legendre function over the whole model | bounded by **10.278** | the *non-sectorial* one is not bounded: max over *m* of P̄′₂₁₉₀,ₘ(1) is 10⁴⁵⁷·⁸⁶⁴, which overflows a double by 10¹⁵⁰ | measured, §3.6 and §3.6a |
| `GRAV-P-8` | agreement of the recursion with the definition in exact arithmetic | **≤ 10⁻¹³ to degree 360; ≤ 10⁻⁹ to degree 2190** | the three-term recursion accumulates rounding over *n* − *m* steps with a mild cancellation at each, so the achievable accuracy falls off with degree. **Measured worst: 6.1 × 10⁻¹¹, at degree 2190, order 0, on the polar axis.** Degree 360 covers every truncation Table 6.1 suggests, the largest being 90 | `GRAV-A-003`; the derivation itself agrees to 9.9 × 10⁻⁵⁸ at 60 digits, so what this budget measures is floating point and not algebra |

**Why `GRAV-P-1` to `GRAV-P-3` name a spectral character, and why no gate here comes from
Table 6.1.** `TN36-6` Table 6.1 states that these truncations give "3-dimensional orbit accuracy
of better than 0.5 mm" for the named satellites. The obvious move is to convert that into an
acceleration tolerance and gate on it. Carried out as ½*aT*² over one revolution it gives
**2.3753 mm** for Starlette and **0.90677 mm** for Lageos against Table 6.1's 0.5 mm — an
apparent failure by 4.8× and 1.8×, with only GPS's 0.021454 mm inside.

**It is the conversion that is wrong, and not by a little.** Truncation error at degree *N*
oscillates at about *N* cycles per revolution and does not accumulate secularly; treated as
oscillatory its amplitude is *a*/(*Nn*)², which for Starlette is **1.5 × 10⁻⁵ mm** — five orders
of magnitude inside Table 6.1, not marginally inside it. The model is right, the table is right,
and a gate built from ½*aT*² would have failed a correct implementation while looking like a
physics failure, which is the expensive kind of wrong. *(Both readings were computed
independently by the manager on adoption and agree to the digit.)*

So **no gate here is set from Table 6.1**; the orbit-level claim belongs to the layer that fits
orbits. `SPEC-template.md` §7 now carries the whole worked case as the reason acceleration has
no conversion row in this tree, and the rule that follows from it: an acceleration budget states
the acceleration, and if it also quotes a position consequence it must name the integration time
**and** the spectral character.

This is the same class of mistake this project has now made five times in five disguises,
running in the opposite direction: not a true statement about a smaller thing read as a
statement about a larger one, but a true statement about a larger thing — a fitted orbit —
read as a statement about a smaller one. It cost nothing this time only because the arithmetic
was done before the gate was written.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `GRAV-F-001` | *r* = 0, or a non-finite position component | the position as given | return zero; return the two-body term |
| `GRAV-F-002` | a coefficient file carrying a non-zero degree-1 term | the coefficient, its value, and that degree 1 must vanish for a geocentric origin | re-centre the field; ignore it |
| `GRAV-F-003` | record count, or any record's (*n*, *m*), not as `GRAV-R-011`/`-014` require | the expected and actual counts, and the first record that broke the sequence | accept a short file; fill the remainder with zeros |
| `GRAV-F-004` | requested *N* > 2190, or *M* > min(*N*, 2159) | the request and the model's limits, separately for degree and order | clamp silently to the maximum |
| `GRAV-F-005` | scaling parameters that are not the model's own **as a pair**: an *a*ₑ that is not 6 378 136.3 m, or a GM that is none of the model's three | both values, both sources, which time scale each GM is compatible with, and the consequence | use them; substitute the model's own; refuse on the GM alone |
| `GRAV-F-006` | the conventional field requested at an epoch outside the validity of `GRAV-R-020`'s linear rates or `TN36-7`'s secular-pole fit | the epoch, the fit's span (1900–2017 for the pole), and which term is out of range | extrapolate the linear model without saying so |
| `GRAV-F-007` | a coefficient file from outside the manifest cache | the path and the cache root | read it |
| `GRAV-F-008` | the tide system of the field and of a tide model asked to be added disagree | both systems and both sources | add them; convert silently |

`GRAV-F-006`'s override, and it is the only one in this spec, per `R-ERR-3`:
`extrapolate_secular_terms_beyond_fit`, set per run, recorded with the epoch it was used for.
The Conventions themselves warn that the low-degree trends "are not strictly linear in reality"
and "may not be consistent with more recent surface mass trends", so the refusal is the honest
default and the override is what makes the module usable for an epoch in 2030.

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `GRAV-A-001` | **THE GATE.** The evaluated field's degree-by-degree RMS acceleration over an equal-area global sample, at *r* = 7331, 12 270 and 26 600 km, against the exact identity of `GRAV-R-041`, for **every** degree 2…2190. The sample size and the per-degree residual MUST be reported, so "the gate passed" carries its denominator. | (GM/*r*²)(*a*ₑ/*r*)ⁿ σ*ₙ* √((*n*+1)(2*n*+1)) for each *n* | a closed-form consequence of the 4π normalisation of `TN36-6` (6.2b), evaluated on `EGM08`'s own coefficients | 5/√(2*K*) per degree, and the **mean ratio over all degrees within 1 %** (`GRAV-P-6`) | R-001, R-026, R-030, R-034, R-041, R-042, R-049, P-6 |
| `GRAV-A-001b` | **The same identity at low degree with a sample that can carry it.** Degrees 0–60 at *r* = 7331 km with *K* = 20 000 points, which costs almost nothing because degree 60 is 1830 terms against 2.4 million. | as `GRAV-A-001` | as `GRAV-A-001` | 5/√(2*K*) = 2.5 %; **measured worst 8.4 × 10⁻⁶, mean ratio 1.000000** | R-001, R-041, P-6 |
| `GRAV-A-027` | **THE GATE'S COMPANION, added at adoption.** The field truncated to degree 2, order 0, at points spanning latitude 0°–90° and longitude, against the **exact J2-only closed form** — a = −(GM/*r*³)·*x*·[1 + (3/2)J₂(*a*ₑ/*r*)²(1 − 5*z*²/*r*²)] in *x* and *y*, and with (3 − 5*z*²/*r*²) in *z*, with J₂ = −√5 C̄₂₀ = 1.082 635 869 911 × 10⁻³ | at *r* = 7331 km: **−7.425 827 533 237 m s⁻²** in *x* on the equator at λ = 0; **−7.398 476 898 171 m s⁻²** in *z* on the polar axis; (−5.234 736 488 403, 0, −5.247 629 701 420) at latitude 45°, λ = 0 | closed form, derived from `TN36-6` (6.1) and (6.2b) alone and cross-checked against a second algebraic route to machine precision | ≤ 10⁻¹³ relative | R-001, R-003, R-030, R-035, R-036 |
| `GRAV-A-002` | the two-body limit: *N* = 0 | exactly GM/*r*², directed at the origin | analytic | 4 ulp | R-027 |
| `GRAV-A-003` | P̄′*ₙₘ* from the recursion against the definition evaluated in ≥ 50-digit arithmetic, at sampled (*n*, *m*) including *n* = *m* = 2190 and *n* = 2190, *m* = 0, over latitudes 0°…90° | the definition's value | `TN36-6` (6.2b) + `DLMF-14` (14.6.1), de-phased | ≤ 10⁻¹³ relative (`GRAV-P-8`) | R-007, R-030, R-032, P-8 |
| `GRAV-A-004` | orthonormality by Gauss–Legendre quadrature: ∫₋₁¹ P̄*ₙₘ*(*t*)² d*t* | 2 for *m* = 0, 4 for *m* ≥ 1 | a closed-form consequence of `TN36-6` (6.2b) | 10⁻¹² relative | R-007 |
| `GRAV-A-005` | the analytic gradient against central differences of `potential()` at the same points, at degrees 2, 8, 90 and 360 | agreement | self-consistency of two independent routes through the same coefficients | ≤ 10⁻⁷ relative, the step-size floor | R-003, R-037 |
| `GRAV-A-006` | evaluation **on the polar axis**, φ = ±90°, at full degree: finite, and the limit of the field as φ → ±90° along two meridians 90° apart | continuity to the tolerance of the approach | analytic; the field has no singularity there | ≤ 10⁻⁹ relative | R-026, R-035, R-036 |
| `GRAV-A-007` | **the factored recursion earns its place**: the same field evaluated with cosᵐφ *not* factored out is measurably worse, and at *m* = 2190, |φ| > 43.7° returns zero where the factored form does not | the unfactored form loses the term entirely; the unscaled factored form becomes **NaN**, not merely large; the scaled one is finite and its magnitude matches the closed form | §3.6 and §3.6a, measured | — | R-030, R-031a, R-034, P-7 |
| `GRAV-A-008` | **the *m* = 1 seed**: substituting the general sectorial seed at *m* = 1 changes P̄₁₁ by exactly √2 | ratio √2 = 1.414 213 562 | §3.5, verified at 60 digits | 10⁻¹² relative | R-033 |
| `GRAV-A-009` | the coefficients as loaded, before substitution, against the values `TN36-6` prints: C̄₂₂ = 2.439 383 6 × 10⁻⁶, S̄₂₂ = −1.400 273 7 × 10⁻⁶ (§6.1), C̄₃₀ = 0.957 161 2 × 10⁻⁶, C̄₄₀ = 0.539 965 9 × 10⁻⁶ (Table 6.2) | as printed | `TN36-6` §6.1 and Table 6.2 — **published numbers checked against the published file** | the printed digits | R-011, R-052 |
| `GRAV-A-010` | the tide-system arithmetic: C̄₂₀^zt − (−4.1736 × 10⁻⁹) | −0.484 165 31 × 10⁻³, the tide-free value `TN36-6` §6.2.2 prints | `TN36-6` §6.2.2 | the printed digits | R-008, R-021 |
| `GRAV-A-011` | **the conventional substitution is not cosmetic**: the conventional zero-tide C̄₂₀ minus the file's tide-free C̄₂₀ | −4.3362 × 10⁻⁹, and after removing the tide-system difference −1.6261 × 10⁻¹⁰, which is 8× the 2 × 10⁻¹¹ uncertainty the Conventions state | `TN36-6` §6.1, Table 6.2, §6.2.2 and `EGM08` | 1 in the last printed digit | R-020, R-025 |
| `GRAV-A-012` | **the one external absolute anchor**: ‖**a**‖ on the polar axis at *r* = *b* = 6 356 752.3142 m, against WGS 84 normal gravity at the pole, where the centrifugal term vanishes identically | 9.832 184 9379 m s⁻² ± 0.005 | `WGS84` Table 3.6 | ± 0.005 m s⁻², a 500 mGal band; **what this checks is stated in the note below** | R-001, R-004, R-005, R-026 |
| `GRAV-A-013` | truncation: `truncation_rms()` at the three Table 6.1 rows, and that it decreases monotonically in *N* from 2 to 2190 at each radius | 1.2174 × 10⁻¹⁰, 9.9122 × 10⁻¹², 2.3018 × 10⁻¹⁴ m s⁻² | computed here from `EGM08` by `GRAV-R-041` | 1 % | R-040, R-041, S-043, P-1, P-2, P-3 |
| `GRAV-A-014` | the file's structure: 2 401 333 records; degrees 0 and 1 absent; the last record (2190, 2190) zero; the highest non-zero order 2159 at odd and 2158 at even degrees above 2159 | as stated | measured from `EGM08` | exact | R-012, R-013, R-014 |
| `GRAV-A-015` | refusal: a coefficient file with a non-zero C̄₁₁ | `GRAV-F-002` naming the coefficient and its value | this spec | — | F-002, R-012 |
| `GRAV-A-016` | refusal: degree 2191; order 2160; order > degree | `GRAV-F-004`, naming degree and order separately | this spec | — | F-004, R-026 |
| `GRAV-A-017` | refusal: `ScalingParameters` built from **both** of WGS 84's constants — its *a* = 6 378 137.0 m against the model's 6 378 136.3 m. The legitimate TCG pair is accepted and its compatibility recorded. The consequence is measured rather than asserted: 5.5821 × 10⁻⁹ m s⁻² at 7331 km, 108.91 mm over one revolution | `GRAV-F-005` naming both values, both time scales and the consequence | this spec, `GRAV-P-4` | — | F-005, R-004, R-006, R-053, P-4 |
| `GRAV-A-018` | refusal: a coefficient file outside the manifest cache; and the file's SHA-256 verified by the fetcher before the module sees it | `GRAV-F-007` naming the path and the cache root | this spec, plan rule R11 | — | F-007, R-010 |
| `GRAV-A-019` | refusal: the conventional field at 2035-01-01, outside `TN36-7`'s 1900–2017 fit, and that the single named override lets it through and is recorded | `GRAV-F-006` naming the epoch and the span | this spec, `R-ERR-3` | — | F-006, R-023, R-024 |
| `GRAV-A-020` | structural: `Degree` and `Order` cannot be exchanged; a bare `double[3]` cannot reach `acceleration()`; `ConventionalField` exposes no mutator | compile failure in all three | this spec | — | R-050, R-051, R-052 |
| `GRAV-A-021` | no global state: two `ConventionalField` values at different epochs, queried alternately in one process, give the epoch-appropriate C̄₂₀ each time | as stated | this spec | exact | R-054, R-024 |
| `GRAV-A-022` | the substitution register: `substitutions()` lists exactly the four coefficients `GRAV-R-020` and `GRAV-R-022` change, each with its from-value, to-value and source | as stated | this spec | exact | R-025, R-021 |
| `GRAV-A-023` | the sign convention: at a point above the equator the acceleration points towards the origin, and *V* increases downwards | as stated | `TN36-6` (6.1) | exact | R-003 |
| `GRAV-A-025` | **the figure-axis terms**: C̄₂₁(*t*), S̄₂₁(*t*) from (6.5) with the secular pole of `TN36-7` (21), at J2000.0 and at 2026.0 | −2.264 385 × 10⁻¹⁰ and +1.299 633 × 10⁻⁹ at J2000.0; −4.048 365 × 10⁻¹⁰ and +1.664 613 × 10⁻⁹ at 2026.0 | `TN36-6` (6.5) evaluated in closed form on the constants §3.7 lists | 1 in the last digit shown | R-022, R-023, R-024 |
| `GRAV-A-029` | **the scale's margin, at both ends**: the range of the scaled values the recursion forms, over a grid spanning orders 0 to 2159 and latitudes 0° to 90° | 1.055 × 10⁻²⁸² to 7.313 × 10¹⁷⁷ — 26.0 decades above the smallest normal double, 130.1 below the largest, **0 subnormal** | measured, §3.6a | none subnormal, none infinite | R-030, R-031a, P-7 |
| `GRAV-A-028` | **one pole model, not two**: the secular pole this module exports is the same object (6.5) consumes, checked by perturbing the exported definition and requiring C̄₂₁/S̄₂₁ to move with it; and the four constants appear in exactly one place in the module's source | as stated | this spec, `GRAV-R-029` | exact | R-029, R-023 |
| `GRAV-A-026` | the loader records **which file, in which tide system**: `tide_system()` reads back tide-free before the conventional substitution and zero-tide after it, and `provenance()` names the file by hash either way | as stated | `EGM08-RM` (1) and `TN36-6` Table 6.2 | exact | R-008, R-015, R-021 |
| `GRAV-A-024` | the Condon–Shortley convention: an odd-order term computed with the (−1)ᵐ phase differs in sign, and `GRAV-A-001` then fails | sign flip on every odd *m* | `DLMF-14` (14.6.1) against `TN36-6` (6.2a) | exact | R-007 |

**`GRAV-A-008`'s error would NOT be caught by the gate, and v1.1 of this specification said it
would.** The *m* = 1 share of σ*ₙ*² is tiny — at degree 2 it is (C̄₂₁² + S̄₂₁²)/σ₂² ≈ 7 × 10⁻¹²,
because C̄₂₀ dominates that degree by six orders of magnitude — so a √2 on every order-1 term
moves no degree variance measurably. It is caught by `GRAV-A-003`, whose reference set includes
(2, 1) and (2190, 1) evaluated from the definition. The claim was removed at v1.2 rather than
left to look like coverage that does not exist.

**What `GRAV-A-012` does and does not check, because a band this wide invites being
over-read.** It compares the full-field magnitude against a number published independently of
EGM2008. Its resolution is 5 × 10⁻⁴ relative, so it catches a wrong GM, a wrong *a*ₑ, a missing
or doubled degree-0 term, a normalisation off by any factor above 1.0005, and a sign error. It
cannot see anything subtler, and in particular it says nothing about degrees above about 4.
The difference between EGM2008's gravitation at the pole and WGS 84's normal gravitation there
is the gravity disturbance, which this specification does not know a published value for; the
band is set wide enough to contain any plausible one, and **the measured value MUST be recorded
in `PROVENANCE.md` so the band can be narrowed to what was actually observed.** It is the only
test here whose expected value comes from outside this tree's own mathematics, which is why it
is kept despite being coarse — see `GRAV-Q-001`.

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `GRAV-R-002` | Structural: the interface takes a Cartesian ITRS position, so no latitude of either kind can be passed. Discharged by `GRAV-A-020`. |
| `GRAV-R-028` | Discharged by `GRAV-A-013` and `GRAV-A-016`, which both read the degree and order back off a result. |
| `GRAV-R-031` | A provenance and process obligation, not a behaviour. Discharged by `PROVENANCE.md` §9 and by the derivation record of §4.4. |
| `GRAV-F-001` | Trivially testable and tested with `GRAV-A-002`'s harness; listed here because its *diagnostic* is what matters and a diagnostic test is not an accuracy test. |
| `GRAV-F-003` | Exercised by `GRAV-A-014` on the real file and by a truncated copy in the refusal test alongside `GRAV-A-015`. |
| `GRAV-F-008` | The consumer does not exist until L2 step 3. The refusal is specified now so that step inherits it rather than inventing it. |
| `GRAV-S-043` | A recommendation about defaults; its content is the numbers `GRAV-A-013` checks. |

**The gate has a blind spot, and `GRAV-A-027` is what covers it.** `GRAV-A-001` is a statement
about **means over the sphere**. A recursion that produced wrong angular structure while
preserving unit mean square would satisfy it — the identity constrains how much power each
degree carries, not where on the sphere it is. `GRAV-A-027` is point-wise against an exact
closed form and tests precisely what the identity cannot. The two together are the gate; neither
alone is. *(The blind spot was named by the manager at adoption, not by this author.)*

**This §8 is not built on a published table of accelerations, because there is none** — see
`GRAV-Q-001`. By the template's §8 ordering its rows draw on category 1 (published numbers:
`GRAV-A-009`, `-A-010`, `-A-011`, `-A-012`), category 2 (closed-form results: `GRAV-A-001`,
`-A-002`, `-A-003`, `-A-004`, `-A-013` and `-A-027`) and category 4 (self-consistency:
`GRAV-A-005`). The gate is category 2, not category 1, and the template requires that to be said
plainly. An oracle comparison against the predecessor's geopotential is available as a
category-5 cross-check and is **not** part of the gate; the manager offered it and did not
require it.

---

## 9. Provenance obligations

- **Module register:** `gravity` → `TN36-6`, `TN36-7`, `TN36-1`, `EGM08`, `EGM08-RM`, `WGS84`,
  `DLMF-14`, with `NASA-TP` and `HF02` as informative.
- **Parameter register:** GM = 398 600.4415 km³ s⁻² and *a*ₑ = 6 378 136.3 m cited to
  `TN36-6` §6.1 and `EGM08-RM` (2) **as a pair**; Table 6.2's three coefficients and three
  rates; C̄₂₂ and S̄₂₂ as used in (6.5); the secular pole's four constants from `TN36-7` (21);
  WGS 84's γ_p = 9.832 184 9379 m s⁻² and *b* = 6 356 752.3142 m, cited to `WGS84` Table 3.6
  and used only by `GRAV-A-012`.
- **Substitution register:** the four coefficients this module changes relative to the
  distributed file, each with from-value, to-value, authority and epoch dependence
  (`GRAV-R-025`). This is a register the earlier modules did not need and it exists because a
  conventional field is *not* the published file.
- **Data manifest:** `EGM2008_Spherical_Harmonics.zip` with URL, SHA-256, size, retrieval date
  and the terms under which NGA distributes it; and the record that only
  `EGM2008_to2190_TideFree` and `README_WGS84_2.pdf` are consumed from it, the other five
  members being present but unused — `hsynth_WGS84.f` and `hsynth_WGS84.exe` deliberately so.
- **Measured values to record, not merely to assert:** `GRAV-A-012`'s observed polar
  acceleration; `GRAV-A-001`'s sample size and worst per-degree residual; the three
  `GRAV-A-013` truncation numbers as the implementation computes them, against the values §6
  predicts from the coefficients alone.
- **§3.11 point 4 check:** `EGM08` is a data archive with no build system. That check applies
  to the module's code dependencies, of which this module has none beyond `core` — the whole
  point of `GRAV-R-031` is that this is written rather than acquired.
- **Derivation record:** §4.4's recursion, the sources it is derived from, and the 60-digit
  verification, so that the claim "not taken from anyone's code" is checkable rather than
  asserted.

---

## 10. Open questions for the manager

| id | question | recommendation |
|---|---|---|
| `GRAV-Q-001` | **There is no published table of geopotential accelerations to gate on.** The step's scope says "published coefficients' acceleration at sampled points". The coefficients are published; accelerations computed from them are not, in any form this author could find — searched: NGA's EGM2008 distribution, ICGEM, `PAVLIS12`'s abstract and figures, `NASA-TP` (whose appendix B publishes *error magnitudes*, not accelerations, and for the Moon), and the harmonic-synthesis literature. So `GRAV-A-001` is a closed-form identity rather than a published comparison. | **RULED 2026-09-18: accept the identity as the gate**, and the manager checked it rather than taking it — (*n*+1)² from the radial derivative plus *n*(*n*+1) from the tangential is exactly (*n*+1)(2*n*+1), and the 4π normalisation makes each normalised function's mean square unity. Every degree at three radii is stronger coverage than a handful of points. It stays marked **category 2**. If a published acceleration set ever turns up, the gate is promoted. |
| `GRAV-Q-002` | **NGA publishes a six-point reference inside the archive this spec already pins — of geoid undulation, not acceleration.** `INPUT.DAT` and `OUTPUT1.DAT` give six latitude/longitude pairs and their EGM2008 geoid undulations to the millimetre, including **both poles** and computed at **full degree 2190**. That is precisely the two hard cases. Consuming it costs: the WGS 84 normal potential and Somigliana normal gravity in closed form, a second pinned 142 MB expansion (the ζ*-to-N conversion to degree 2160), and matching Pavlis's exact option conventions to 4 × 10⁻⁵ relative. | **RULED 2026-09-18: do not take it**, for the reason given here — a mismatch would be a convention disagreement wearing the costume of a physics failure, and clearing that would cost days. **Recorded with that reason so it is not reopened cheaply in six months.** The archive is still pinned whole and `hsynth_WGS84.f` still not opened. |
| `GRAV-Q-003` | **The Conventions print no recursions, so the step's scope cannot be met as worded.** The scope says "recursions taken from the tables printed in the Conventions rather than from anyone's code". Chapter 6 prints the expansion (6.1) and the normalisation (6.2b) and nothing else; the words "recursion" and "recurrence" do not occur in it. The standard citation, `HF02`, is paywalled and was not obtained. | **RULED 2026-09-18: the substitute is accepted and the plan is corrected rather than this specification.** "Recursions taken from the tables printed in the Conventions" named a table that does not exist. Deriving from (6.2b) with `DLMF-14` — primary, free, and the successor to Abramowitz & Stegun — and verifying in exact rational arithmetic to 60 digits is an artefact **anyone can reproduce**, which citing a paywalled paper would not have been. `doc/REWRITE_PLAN.md` §3.3 step 2 now specifies this route. |
| `GRAV-Q-004` | **Table 6.1 cannot be converted into an acceleration gate**, worked through in §6. Two of its three rows fail a conservative conversion by factors of 4.8 and 1.8. | **RULED 2026-09-18: no gate is set from Table 6.1**, and the manager verified both readings independently — ½*aT*² reproduces 2.37, 0.90 and 0.021 mm to the digit, and the oscillatory amplitude *a*/(*Nn*)² puts Starlette at 1.5 × 10⁻⁵ mm, five orders of magnitude inside the table. The whole worked case is now in `SPEC-template.md` §7 as the reason acceleration has no conversion row in this tree. |
| `GRAV-Q-005` | **Which pole model feeds equation (6.5)?** Chapter 6 says "consistent with the mean figure axis corresponding to the pole of the TRF defined in Chapter 4" and names no model. Chapter 7's 2018 update replaced the "mean pole" of earlier Conventions with a linear **secular pole**, so what chapter 6's wording pointed at no longer exists under that name. The term is not negligible: §3.7 measures the substitution at sixty-one times the truncation error the same field accepts. Two sub-questions: (a) confirm the reading; (b) decide whether the secular pole is implemented **here** or at L2 step 3, which needs the same four constants for the pole tides. | **RULED 2026-09-18: the secular pole, confirmed, and implemented at step 2** — the 2018 update exists because the mean-pole model was diverging, and 7.46 × 10⁻⁹ m s⁻² at 7331 km is not a rounding. The conventional C̄₂₁/S̄₂₁ are the static field's own degree-2 order-1 terms and belong here. **Binding condition, now `GRAV-R-029`: the pole model is defined once, here, and L2 step 3's pole tide consumes that definition rather than restating it.** Two definitions is how this module ends up secular and step 3 ends up mean. |
| `GRAV-Q-006` | **The gravity-gradient tensor ∂a/∂r is not in this step's scope** and the variational equations of L3/L7 will need it. The second derivatives come from the same recursion at negligible extra cost if the interface allows for them, and at the cost of a second traversal if it does not. | Open, and deliberately so. Left out of step 2 on the one-step-at-a-time rule; §5's interface returns values rather than filling caller-supplied buffers, so adding `gradient()` is additive. Flagged so L3 meets it as a plan item rather than a surprise. |
| `GRAV-Q-007` | **`SPEC-template.md` §7's conversion table has no acceleration row**, so every acceleration-to-position conversion in this tree will be improvised. This spec writes its own out in full in §6, four times. | **RULED 2026-09-18, and deliberately not the row this specification asked for.** An acceleration-to-position conversion row would have institutionalised the exact error `GRAV-Q-004` had just caught. `SPEC-template.md` §7 instead carries **the prohibition and the worked example**: acceleration has no row, because every other row is a rotation or a rate where the consequence is a multiplication and this one is not; an acceleration budget states the acceleration, and one quoting a position consequence must name the integration time **and** the spectral character. §6 conforms at v1.1. |
| `GRAV-Q-009` | **`SPEC-frames.md` has no frame-carrying position or acceleration type**, and `GRAV-R-050` assumed it did. It has `State`, which is a position *and* a velocity *at* an epoch, in km; a point at which to evaluate a static field is none of those, and reusing it would have meant inventing a velocity and an epoch to discard. `Position<F>` and `Acceleration<F>`, in metres, were therefore added to `frames` rather than declared here. | **Implemented that way under your `EPH-Q-001` ruling** — *the frame enumeration belongs to that specification, and a second spec extending it silently is how enumerations drift* — and `SPEC-frames.md` amended to match. It is additive: nothing existing changed, and no existing test moved. **Flagged for your verdict because amending an adopted specification is yours, not mine.** The one wrinkle worth your eye: these carry **metres** where `State` carries km, and the unit is in the accessor name on both sides (`metres()`, `position()`), so a conversion cannot happen silently. |
| `GRAV-Q-008` | **The secular rates have no stated validity span**, and the Conventions warn in §6.1 that the low-degree trends "are not strictly linear in reality", that there may be "decadal variations that are not captured", and that they "may not be consistent with more recent surface mass trends due to increased ice sheet melting". `GRAV-F-006` therefore refuses outside the span, but the span for Table 6.2's rates is not published — only the secular pole's 1900–2017 fit is. | Standing as recommended: chapter 7's 1900–2017 is the only published span, it is named in the diagnostic rather than implied to cover the rates, and `GRAV-F-006`'s single named override is the way past it. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.3 | 2026-09-18 | **§3.6a gains the measured bottom margin**, at the manager's request on accepting step 2: the scale's top margin was justified when it was chosen and the bottom was not, and the bottom is the thinner side. Measured at 26.0 decades over 74 802 values with none subnormal (`GRAV-A-029`), together with the bound |P̄′| ≥ |P̄| that makes it structural rather than lucky. |
| 1.2 | 2026-09-18 | **Amended by implementation, six corrections.** (1) **§3.6a added: the factored function is not bounded** — only the sectorial seed is. Max over *m* of P̄′₂₁₉₀,ₘ(1) is 10⁴⁵⁷·⁸⁶⁴, overflowing a double by 10¹⁵⁰, after which the three-term recursion yields NaN rather than infinity. Both representations fail, in opposite directions; `GRAV-R-030` now requires the 10⁻²⁸⁰ scale **and** the Horner nest, with `GRAV-R-031a` on where the scale is removed. (2) `GRAV-R-012`: **C̄₀₀ = 1, not 0** — degree 0 is the two-body term inside the same sum. (3) `GRAV-P-8` tiered: 10⁻¹³ to degree 360, 10⁻⁹ to 2190; measured worst 6.1 × 10⁻¹¹. (4) **`GRAV-A-008`'s claim that the √2 error would fail the gate was false** — the *m* = 1 share of σ₂² is 7 × 10⁻¹² — and was removed rather than left looking like coverage. (5) `GRAV-R-006`/`-F-005`: WGS 84's GM **is** the TCG value numerically and cannot be refused by value; the pair is checked instead. (6) `GRAV-R-049`/§5: `acceleration_by_degree` added, and `GRAV-A-001b` with it. `GRAV-Q-009` opened on the frame-carrying position type. |
| 1.1 | 2026-09-18 | **Adopted**, with the addition the adoption carried and rulings on all eight questions. `GRAV-A-027`: a point-wise check against the exact J2-only closed form, covering the gate's blind spot — `GRAV-A-001` is a statement about means over the sphere, and a recursion with wrong angular structure but unit mean square would satisfy it. `GRAV-R-029` and `GRAV-A-028`: the secular pole is defined once here and L2 step 3 consumes that definition. §6 conformed to the amended `SPEC-template.md` §7 — every acceleration budget names its spectral character, and `GRAV-P-4` says explicitly that ½*aT*² *is* the right instrument for a secular error. §10 `GRAV-Q-005` also carried, into v1.0, the very claim §3.7 had corrected; repaired. |
| 1.0 | 2026-09-18 | First draft, L2 step 2, for manager review. |
