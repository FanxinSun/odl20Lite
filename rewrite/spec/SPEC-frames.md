# SPEC-frames — reference frames and the transformations between them

| | |
|---|---|
| **Spec ID** | `FRAME` |
| **Status** | **adopted** 2026-09-18 — manager verdict from session `odl maintainer (Router+Executor)`. Version 1.1 records the decisions taken in that verdict. |
| **Version** | 1.5 |
| **Date** | 2026-09-18 |
| **Layer** | `frames` (`doc/REWRITE_PLAN.md` §2) |
| **Feature** | F3 (plan §3) |
| **Depends on** | `SPEC-time.md`, `SPEC-eop.md` |
| **Depended on by** | `env`, `forces`, `dynamics`, `measmod`, `io` |

**Derivation declaration (plan R1, R2).** This specification was written from the documents
listed in §2 and from no implementation of this module. Specifically, no file under
`/home/rog/odl20lite` was opened, read, listed, searched, or otherwise inspected during its
preparation. Numeric acceptance targets carried from prior measurement campaigns are
behavioural observations against public data (plan R4) and are marked as such where they
appear.

---

## 1. Purpose and scope

This module owns **the orientation of a vector**: which frame a position, velocity or
acceleration is expressed in, and how to move it to another frame without losing that
knowledge.

The single most expensive class of defect it is designed against is not numerical. A frame
error is a **pure rotation**: magnitudes are preserved, so every plausibility check based on
altitude, speed, energy or angular momentum passes, and only the direction is wrong. The
GCRS-to-TEME rotation grows at about 1.7 km per year of along-track displacement at 7000 km
(general precession in right ascension, ≈ 50.3 arcsec yr⁻¹, times a 7000 km radius), so by
2026 a TEME state mistaken for a GCRS state is displaced by tens of kilometres — and nothing
in the state's magnitude reveals it.

### In scope

- The frames GCRS, CIRS, TIRS, ITRS, TEME/PEF, and the local frames RTN and DYB.
- GCRS ↔ ITRS, position **and velocity**, via the IAU 2006/2000A CIO-based chain.
- TEME ↔ ITRS and TEME ↔ GCRS.
- Polar motion and the TIO locator.
- The rule that a state's frame is part of its type.

### Not in scope

| excluded | owned by |
|---|---|
| Obtaining *x*_p, *y*_p, ΔUT1, LOD, δ*X*, δ*Y* — files, splicing, interpolation, sub-daily terms | `SPEC-eop.md` |
| Time scales, the leap table, the epoch representation | `SPEC-time.md` |
| SGP4 itself — only the frame its output lives in is specified here | F14's spec |
| Body-fixed / attitude frames of a spacecraft, yaw-steering laws | F6, F13 |
| Topocentric and station frames, site displacement models | `measmod` (F11) |
| The geopotential's frame-dependent evaluation | F2 |

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `TN36-5` | N. Capitaine, P. Wallace, in G. Petit & B. Luzum (eds.), IERS | *IERS Conventions (2010)*, Technical Note 36, **chapter 5** — Transformation between the ITRS and the GCRS | 2010 | `https://iers-conventions.obspm.fr/content/chapter5/icc5.pdf` (retrieved 2026-09-18) | primary | normative |
| `TN36-1` | IERS | *IERS Conventions (2010)*, chapter 1 — numerical standards, Table 1.1 | 2010 | `https://iers-conventions.obspm.fr/content/chapter1/icc1.pdf` (retrieved 2026-09-18) | primary | normative |
| `ERFA` | NumFOCUS Foundation / liberfa | ERFA — Essential Routines for Fundamental Astronomy, source and in-source documentation | v2.0.1, 2023-10-13 (SOFA "20231011") | `https://github.com/liberfa/erfa` (retrieved 2026-09-18) | primary | interface — **BSD 3-clause** (plan R7) |
| `VAL06` | D. A. Vallado, P. Crawford, R. Hujsak, T. S. Kelso | *Revisiting Spacetrack Report #3*, AIAA/AAS Astrodynamics Specialist Conference, Keystone CO, AIAA 2006-6753 (Rev 2) | 2006, rev. | `https://celestrak.org/publications/AIAA/2006-6753/AIAA-2006-6753-Rev2.pdf` (retrieved 2026-09-18); DOI `10.2514/6.2006-6753` | primary | normative — TEME |
| `BEU94` | G. Beutler, E. Brockmann, W. Gurtner, U. Hugentobler, L. Mervart, M. Rothacher, A. Verdun | *Extended orbit modelling techniques at the CODE processing center …*, Manuscripta Geodaetica 19: 367–386 | 1994 | — | **not obtained** | normative for DYB — see `FRAME-Q-002` |
| `ARN15` | D. Arnold, M. Meindl, G. Beutler *et al.* | *CODE's new solar radiation pressure model for GNSS orbit determination*, J. Geodesy 89(8): 775–791 | 2015 | DOI `10.1007/s00190-015-0814-4` | **secondary** — abstract only; paywalled | normative for DYB — see `FRAME-Q-002` |

---

## 3. Definitions and conventions

### 3.1 Rotation matrices and sign

*R*₁, *R*₂, *R*₃ denote rotations by a **positive angle about axes 1, 2, 3 of the coordinate
frame** [`TN36-5` §5.4], i.e.

```
R₃(θ) = [  cos θ   sin θ   0 ]      R₁(θ) = [ 1     0        0    ]
        [ −sin θ   cos θ   0 ]              [ 0   cos θ    sin θ  ]
        [    0       0     1 ]              [ 0  −sin θ    cos θ  ]
```

and correspondingly for *R*₂. A matrix named `M_A_to_B` maps components expressed in frame A to
components expressed in frame B: `v_B = M_A_to_B · v_A`. All matrices in this spec are
orthogonal with determinant +1; their inverse is their transpose, and the implementation MUST
invert by transposition, never by numerical inversion (`FRAME-R-004`).

### 3.2 The frames

| frame | z-axis | x-axis | origin | note |
|---|---|---|---|---|
| **BCRS** | — | — | **solar-system barycentre** | Barycentric Celestial Reference System, axes aligned with the ICRS. The frame of the planetary ephemerides; see §3.5. |
| **GCRS** | — | — | geocentre | Geocentric Celestial Reference System; kinematically non-rotating, aligned with the ICRS. The inertial frame of this tree's dynamics. Colloquially "GCRF"/"J2000-ish"; **it is not the mean equator and equinox of J2000** — the two differ by the frame bias (§4.4), ≈ 23 mas ≈ 0.8 m at 7000 km. |
| **CIRS** | CIP | CIO | geocentre | Celestial Intermediate Reference System [`TN36-5` §5.4] |
| **TIRS** | CIP | TIO | geocentre | Terrestrial Intermediate Reference System [`TN36-5` §5.4] |
| **ITRS** | — | — | geocentre | International Terrestrial Reference System, realised by the ITRF the EOP series is consistent with — currently ITRF2020 (see `SPEC-eop.md` §2) |
| **TEME** | true equator of date | "uniform equinox" | geocentre | True Equator, Mean Equinox: the output frame of SGP4. §4.5. |
| **PEF** | true pole of date | Greenwich meridian of date | geocentre | Pseudo-Earth-Fixed, the equinox-based counterpart of TIRS. §4.5. |
| **RTN** | orbit normal | radial | the spacecraft | Local orbital frame. §4.6. |
| **DYB** | — | spacecraft→Sun | the spacecraft | Sun-oriented frame for radiation-pressure modelling. §4.7. |

### 3.3 The model version, stated explicitly

- **FRAME-R-001.** This module implements the **IAU 2006 precession with the IAU 2000A
  nutation**, in the **CIO-based** form, throughout [`TN36-5` §5.3.4, §5.6]. The equinox-based
  chain is not implemented, except for the single GMST-1982 rotation that the definition of
  TEME requires (§4.5).

This is a deliberate upgrade from the IAU-76/80 models used by the predecessor. It changes
numerical results, and the change is an improvement, not a regression:

- Frame bias (GCRS relative to the mean equator and equinox of J2000) is ≈ 23 mas — a pole
  offset of 18.0 mas, from the constant terms *X*₀ = −0.016 617″ and *Y*₀ = −0.006 951″ of
  [`TN36-5` eq. (5.16)], plus an equinox offset of ≈ −14.6 mas — and is absent from the older
  chain.
- The IAU-76 precession rate error accumulates at roughly 3 mas yr⁻¹, so by the mid-2020s the
  two chains differ by of order 0.06–0.08 arcsec — about **2.2 m at 7000 km**.

That figure is consistent with the residual the predecessor exhibited in its TEME comparison
against a JPL Horizons table (2.2 m mean at |r| ≈ 7234 km, i.e. 0.0627 arcsec; plan §4, carried
here as a behavioural observation under plan R4). The 0.064 arcsec precession difference is
2.245 m at that radius, so it is essentially the whole of it.

**But the model difference does not appear everywhere, and where it appears is not a matter of
taste.** This was measured during implementation and is the reason plan §4 rule 1 has been
narrowed:

- **On the ITRF ↔ GCRS path it cancels.** Each chain applies the celestial-pole offset series
  matched to its own model — the older chain adds dψ, dε to an IAU-1980 nutation, this one adds
  δ*X*, δ*Y* to the IAU 2006/2000A CIP (`FRAME-R-012`). Those series exist precisely to bring
  each model onto the **observed** pole, so two different algorithms each corrected onto the same
  physical pole must agree, and the model difference cancels **by construction**. Measured against
  oracle case F-01–F-03: **1.56 mm out of 7717 km**, which is 4.2 × 10⁻⁵ arcsec.
- **On the TEME path it does not.** TEME is referred to the **mean equinox of date**, which is a
  model construct with no correction series to reconcile two precession models against an
  observation. The difference therefore appears undiluted, and T-01's 2.2 m is it.

The consequence for anyone setting a threshold: a required-disagreement test belongs on the TEME
path and **not** on the ITRF path, where agreement is correct and a test demanding disagreement
would fail a correct implementation. The kinematic equation-of-equinoxes terms of §4.5 are worth
about 95 mm at that radius — 4 % of T-01 — and are **not** its explanation; setting the gate from
them would have set it twenty-five times too small.

- **FRAME-R-002.** Every frame-transformation result MUST be accompanied by the identity of
  the model version used, and that identity MUST appear in the run's provenance output. A
  numeric baseline frozen under one model version MUST NOT be compared against results from
  another without the comparison recording both.

### 3.4 Frame is carried in the type

- **FRAME-R-003.** A state MUST carry its frame and its epoch. The frame MUST be part of the
  value's type, such that supplying a state in one frame where another is required is rejected
  — statically where the implementation language allows it, and otherwise at the boundary of
  every operation — and is **never** silently accepted.
- **FRAME-R-004.** There MUST be no "reinterpret" or "relabel" operation that changes a
  state's frame without applying the corresponding transformation. Changing the frame tag is
  not an available primitive.
- **FRAME-R-005.** A state's epoch MUST be an `Epoch` in the sense of `SPEC-time.md`, never a
  bare number, and the transformation MUST derive the TT and UT1 arguments it needs from that
  `Epoch` rather than accepting them separately.

`FRAME-R-004` is the structural answer to the frame trap. A convention ("we always store
GCRS") is a comment; a type is a compiler error.

### 3.5 BCRS, and why it is not just another member of the enumeration

`Frame::BCRS` is added at L2's request (`SPEC-ephemerides.md` `EPH-Q-001`): a barycentric vector
is not a geocentric one, they differ by the Earth's barycentric position of order 1.5 × 10⁸ km,
and that is the largest-magnitude frame confusion available anywhere in this system. Leaving
ephemeris output as a bare `Vec3` would give up precisely the invariant §3.4 exists to establish.

But adding the tag is the easy half, and two things make BCRS unlike every other member:

- **BCRS ↔ GCRS is a TRANSLATION, not a rotation.** Every other transformation here is an
  orthogonal matrix and an angular velocity; this one is a vector subtraction whose magnitude
  comes from an ephemeris.
- **BCRS is TDB-based where GCRS is TT-based.** The rotations never raise a timescale question —
  they take an `Epoch` and render whatever they need. This one does: the two systems differ in
  the rate of their time coordinates as well as in their origin.

Hence:

- **FRAME-R-027.** There MUST NOT be a BCRS ↔ GCRS transform **shaped like the rotations** —
  nothing named `to_gcrs`/`to_bcrs` taking only an epoch and an EOP record. A function that looks
  like its neighbours will be used like them, and this one is not like them.
- **FRAME-R-028.** The translation MUST be expressed as an operation **taking the Earth's
  barycentric state as an explicit argument**, so the caller must have obtained it from
  `ephemerides` and cannot get a silent zero. Its name MUST say translation, not transform.
- **FRAME-R-029.** A `State<Frame::BCRS>`'s epoch MUST be rendered to **TDB** when used with the
  ephemerides and to **TT** when used with the geocentric chain, and any operation crossing
  between them MUST state which it used. The relativistic scaling between TDB- and TT-compatible
  quantities, *L*_B = 1.55 × 10⁻⁸ — 2.3 m on an astronomical unit — is **not** applied by this
  module; a consumer needing TDB-compatible lengths must say so. See `FRAME-Q-006`.

---

### 3.6 A position and an acceleration are not a `State`

Added at L2 step 2's request, and amended here rather than declared in `SPEC-gravity.md` for the
reason the manager gave when `Frame::BCRS` was added: the frame enumeration belongs to this
specification, and a second spec extending it silently is how enumerations drift. The same
argument covers the things the enumeration labels.

`State<F>` is a position **and** a velocity **at** an epoch, in km. A point at which to evaluate
a static field is none of those: it has no velocity, it needs no epoch — the field's epoch
dependence belongs to the field, not to the point — and reusing `State` would mean inventing a
velocity and an epoch to discard, which is how a zero becomes a silent default.

- **FRAME-R-060.** `Position<F>` and `Acceleration<F>` carry their frame as a template parameter,
  on exactly the argument `FRAME-R-004` makes for `State`: a runtime tag is a field anyone can
  assign. Neither is default-constructible and neither converts from a bare `Vec3`.
- **FRAME-R-061.** They are in **metres** and **m s⁻²**, where `State` is in km and km s⁻¹, and
  **the unit is named in the accessor** on both sides — `metres()` against `position()` — so that
  a conversion cannot happen by one value flowing into the other's argument. The difference is
  not gratuitous: `SPEC-ephemerides.md` §3.2 asks CALCEPH for km at every call site so that no
  conversion factor appears in this tree's source at all, and EGM2008's reference radius is
  published as 6 378 136.3 **m**. Each module uses the unit its normative source publishes, and
  the boundary between them is explicit.

## 4. Required behaviour

### 4.1 The GCRS → ITRS chain

The transformation is the product of three rotations [`TN36-5` §5.4, eq. (5.1) and the
component equations cited below]. This spec states the chain in the **GCRS → ITRS** direction,
which is the direction the interface exposes and the direction ERFA's matrices are built in;
`TN36-5` eq. (5.1) writes the inverse, `[GCRS] = Q(t) R(t) W(t) [ITRS]`, and the two are
transposes of one another.

```
M_GCRS_to_ITRS(t) = W(t) · R(t) · Q(t)

Q(t) : GCRS  → CIRS     precession-nutation and frame bias   [TN36-5 §5.4.4, eq. (5.10)]
R(t) : CIRS  → TIRS     Earth rotation                        [TN36-5 §5.4.2, eq. (5.5)]
W(t) : TIRS  → ITRS     polar motion                          [TN36-5 §5.4.1, eq. (5.3)]
```

**Q(t) — celestial motion of the CIP.** Parameterised by the CIP coordinates *X*, *Y* in the
GCRS and by the CIO locator *s* [`TN36-5` eq. (5.10)]:

1. Evaluate the IAU 2006/2000A series for *X*, *Y* and the series for *s* at the epoch, with
   **TT** as the argument. ERFA: `eraXy06` and `eraS06`; the combined `eraXys06a` is also
   available [`ERFA`].
2. Compute *s* from the **model** *X*, *Y*, before the observed offsets are applied
   (`FRAME-R-011` below).
3. Apply the observed celestial-pole offsets from the EOP series: `X ← X + δX`,
   `Y ← Y + δY`.
4. Form Q(t)ᵀ (the GCRS→CIRS matrix) from the corrected *X*, *Y* and *s*. ERFA: `eraC2ixys`.

- **FRAME-R-010.** The argument of the precession-nutation series MUST be **TT**, supplied as a
  two-part Julian date with the integral day in the first part (`SPEC-time.md` `TIME-R-014`).
- **FRAME-R-011.** *s* MUST be evaluated from the model *X*, *Y*, not from the offset-corrected
  values. `TN36-5` §5.5.3 states that in processing observational data the quantity *s* "must
  be considered as independent of observations". The difference is below 1 µas (*s* depends on
  *X*, *Y* only through an *XY*/2 term, and δ*X*, δ*Y* are of order 0.3 mas), i.e. below
  0.034 µm at 7000 km, so this is a convention requirement rather than an accuracy one — but
  it must be a *stated* convention, because two implementations differing on it will disagree
  at a level someone will later spend a day chasing.
- **FRAME-R-012.** δ*X*, δ*Y* MUST be applied when the EOP series supplies them, and their
  omission MUST be an explicit, recorded option, not a default. Their magnitude is a few
  tenths of a mas, i.e. of order 10 mm at 7000 km.

**R(t) — Earth rotation.** `R(t) = R₃(ERA)` in the CIRS→TIRS direction, from
`R(t) = R₃(−ERA)` for TIRS→CIRS in [`TN36-5` eq. (5.5)], with the Earth Rotation Angle

```
ERA(T_u) = 2π (0.779 057 273 264 0 + 1.002 737 811 911 354 48 · T_u)          [TN36-5 eq. (5.14)]
T_u = (Julian UT1 date) − 2451545.0
```

and, to reduce rounding error, the equivalent modulo-2π form of [`TN36-5` eq. (5.15)]:

```
ERA(T_u) = 2π ( frac(UT1 Julian day) + 0.779 057 273 264 0 + 0.002 737 811 911 354 48 · T_u )
```

- **FRAME-R-013.** ERA MUST be computed from **UT1**, obtained via `SPEC-time.md`'s
  `ut1_two_part_jd`, which requires ΔUT1 to be passed explicitly from the EOP layer. ERFA:
  `eraEra00`, which implements eq. (5.15) [`ERFA`].
- **FRAME-R-014.** The second (eq. 5.15) form MUST be used, or a dependency implementing it.
  Evaluating eq. (5.14) directly on a full Julian date loses precision in exactly the way
  `SPEC-time.md` §4.2 documents.

**W(t) — polar motion.** `W(t) = [R₃(−s′) · R₂(x_p) · R₁(y_p)]ᵀ` in the TIRS→ITRS direction,
from [`TN36-5` eq. (5.3)]. The TIO locator is

```
s′ = −47 µas · t          t in Julian centuries TT from J2000       [TN36-5 eq. (5.13)]
```

ERFA: `eraSp00` for *s′*, `eraPom00` for the matrix [`ERFA`].

- **FRAME-R-015.** *s′* MUST be included. It reaches ≈ 13 µas by 2026 — 0.43 mm at 7000 km —
  which is below the millimetre but above zero, and including it costs one multiplication.
- **FRAME-R-016.** *x*_p, *y*_p MUST be the values **after** the sub-daily ocean-tide and
  libration terms have been restored by the EOP layer (`SPEC-eop.md` §4.4). Using the raw
  interpolated IERS values omits up to ≈ 0.5 mas — about 17 mm at 7000 km.

**Composition.** ERFA offers `eraC2tcio(rc2i, era, rpom)` to compose the three, and
`eraC2t06a(tta, ttb, uta, utb, xp, yp)` to do the whole chain [`ERFA`].

- **FRAME-R-017.** `eraC2t06a` MUST NOT be used, because it does not accept δ*X*, δ*Y* and
  therefore cannot apply `FRAME-R-012`. The chain MUST be composed explicitly from `eraXy06`,
  `eraS06`, `eraC2ixys`, `eraEra00`, `eraSp00`, `eraPom00` and `eraC2tcio`. `eraC2t06a` MAY be
  called in the test suite, with δ*X* = δ*Y* = 0, as an independent check of the composition
  (`FRAME-A-004`).

### 4.2 Velocity

Position transforms by the matrix; velocity does not, because the target frame rotates.

```
r_ITRS = W · R · Q · r_GCRS
v_ITRS = W · [ R · Q · v_GCRS  −  ω⃗ × ( R · Q · r_GCRS ) ]
```

with ω⃗ = (0, 0, ω_E)ᵀ expressed in TIRS, and

```
ω_E = 7.292 115 146 706 979 × 10⁻⁵ rad s⁻¹ × (1 − LOD / 86400 s)
```

The leading constant is the rate of ERA per second of UT1, i.e.
2π × 1.002 737 811 911 354 48 / 86400, taken directly from [`TN36-5` eq. (5.14)]. The nominal
mean value of the Earth's angular velocity quoted in [`TN36-1` Table 1.1] is
7.292 115 × 10⁻⁵ rad s⁻¹; the two differ by 2 × 10⁻⁸ in relative terms, and the ERA-derived
value is the one consistent with the rotation actually applied.

- **FRAME-R-020.** The inverse transformation MUST be implemented as the exact algebraic
  inverse of the forward one — the same matrices transposed and the same ω⃗ — and not as an
  independently derived formula. This is what makes the round trip exact rather than merely
  close, and it is what `FRAME-A-003` measures.
- **FRAME-S-021.** The LOD correction to ω_E SHOULD be applied. With LOD of order 1–3 ms it
  changes ω_E by 1–3 × 10⁻⁸ in relative terms, i.e. about **6–18 µm s⁻¹** at 7000 km.
- **FRAME-R-022.** The neglected terms MUST be documented in the module's own output
  documentation, with their magnitudes:
  - **Ẇ** (polar motion rate, ≈ 0.3 arcsec yr⁻¹ ⇒ 4.6 × 10⁻¹⁴ rad s⁻¹): ≈ 0.3 µm s⁻¹ at
    7000 km. Negligible.
  - **Q̇** (precession-nutation rate, ≈ 50.3 arcsec yr⁻¹ ⇒ 7.7 × 10⁻¹² rad s⁻¹): ≈ **54 µm s⁻¹**
    at 7000 km.

  Note that the neglected Q̇ term is the **same size as, or larger than, the LOD correction of
  `FRAME-S-021`**. Applying the LOD correction while neglecting Q̇ is not wrong, but it is
  incoherent as an accuracy argument, and the documentation must say so rather than implying
  a velocity accuracy the chain does not have. The honest statement is: **ITRS velocity is
  specified to about 0.1 mm s⁻¹**, which is far below any measurement in the P1–P6 campaigns,
  and the round trip is exact regardless because of `FRAME-R-020`.

### 4.3 Which frame the dynamics runs in

- **FRAME-R-023.** Orbit integration MUST be performed in **GCRS**. The ITRS is a rotating,
  non-inertial frame; integrating in it would require centrifugal, Coriolis and Euler terms
  that this tree does not model and does not intend to.
- **FRAME-R-024.** Quantities naturally defined in the ITRS — station coordinates, the
  geopotential's spherical-harmonic coefficients — MUST be rotated into GCRS at the point of
  use, per step, and the same matrices MUST be reused by the partial derivatives so that the
  variational equations see the same rotation the state does.

### 4.4 Frame bias, and what "J2000" means

- **FRAME-R-025.** The name "J2000" MUST NOT appear in any interface of this module. It is
  ambiguous between the GCRS, the mean equator and equinox of J2000 (which differs by the
  frame bias, ≈ 23 mas ≈ 0.8 m at 7000 km — quantified below), and several looser usages. External data labelled
  "J2000", "EME2000" or "ECI" MUST be refused unless the source document establishes which is
  meant (`FRAME-F-001`).

  The frame bias is not negligible at the accuracies this tree works to: its pole component is
  18.0 mas, read directly off the constant terms of the CIP series, *X*₀ = −0.016 617″ and
  *Y*₀ = −0.006 951″ [`TN36-5` eq. (5.16)], and the equinox offset adds ≈ −14.6 mas about the
  pole. Together ≈ 23 mas, which is **0.8 m at 7000 km** — an order of magnitude above IGS
  final-orbit accuracy.

- **FRAME-R-026.** The ITRS frame type MUST be shaped so that an **ITRF realisation tag** can
  be added to it without breaking callers, and the tag MUST be carried before P6. This is a
  live hazard, not a hypothetical one: the EOP series this tree reads changed realisation
  under us. EOP 20 C04 is consistent with **ITRF2020** where the superseded 14 C04 was
  consistent with **ITRF2014** (`SPEC-eop.md` §3.1), so states derived from data of the two
  eras genuinely are in different realisations, and differencing them without a declared
  transformation is wrong by the difference between the two frames. Adding the tag itself is
  out of scope for P1; being unable to add it later without a breaking change is not
  acceptable now.

### 4.5 TEME

TEME — True Equator, Mean Equinox — is the frame in which SGP4 produces state vectors.
`VAL06` §D states plainly that "an exact operational definition of TEME is very difficult to
find in the literature", that its primary direction relates to the "uniform equinox", and that
the direction of that equinox "resides along the true equator *between* the origin of the
intermediate Pseudo Earth Fixed (PEF) and True of Date (TOD) frames". `VAL06` gives, as its
eq. (1):

```
r_PEF  = R₃(θ_GMST82) · r_TEME
r_TEME = R₃(−θ_GMST82) · r_PEF
```

and recommends "converting TEME to a truly standard coordinate frame before interfacing with
other external programs. The preferred approach is to rotate to PEF using Greenwich Mean
Sidereal Time (GMST), and then rotate to other standard coordinate frames."

GMST-1982, with *T*_UT1 in Julian centuries of UT1 from J2000 [`VAL06` eq. (2)]:

```
θ_GMST1982 = 67 310.548 41 s
           + (876 600 h + 8 640 184.812 866 s) · T_UT1
           + 0.093 104 · T_UT1²
           − 6.2 × 10⁻⁶ · T_UT1³
```

ERFA provides this as `eraGmst82(dj1, dj2)` taking a two-part UT1 Julian date [`ERFA`].

- **FRAME-R-030.** The TEME ↔ ITRS conversion MUST be implemented as `TEME → PEF → ITRS`, with
  the PEF↔TEME rotation being

      R₃( θ_GMST82(UT1) + EqEquinox1982_kinematic )

  and the PEF→ITRS rotation being the **same polar-motion matrix W(t) used by the CIO chain**
  (§4.1). The kinematic term is the two components introduced in 1997,

      0.002 64″ · sin Ω  +  0.000 063″ · sin 2Ω,

  with Ω the mean longitude of the Moon's ascending node. It **MUST** be included: `VAL06`
  eq. (C-1) carries it, and omitting it costs **85 mm** on that paper's own worked example —
  measured, which is how the first draft of this requirement was found to be wrong.

  The **equinox-based route through TOD** MUST NOT be implemented, and that is a different thing.
  `VAL06` §D enumerates three independent ambiguities in it — how many nutation terms are retained
  (4, 10 and 106 are all in use), which small-angle approximations are made, and whether the
  kinematic terms are included in the equation of the equinoxes *for that route* — and there is no
  public basis for choosing among them. **All three ambiguities live in the geometric nutation
  terms, which this chain never uses.** Version 1.2 of this specification forbade the kinematic
  terms along with the route, conflating the two; that was wrong.
- **FRAME-R-031.** The **"of date"** interpretation MUST be implemented: the TEME frame's epoch
  is the epoch of the state, not the epoch of the TLE. `VAL06` Appendix C states that
  "researchers generally believe the 'of date' option is correct, but confirmation from
  official sources is uncertain", and quantifies the disagreement at **23.6 m over three
  days** for its example object. The choice MUST be recorded in the module's provenance
  output, and the "of epoch" interpretation MUST NOT be silently available.
- **FRAME-R-032.** GMST-82 MUST be evaluated at **UT1**, per `VAL06` §D ("using UT1 as we
  discuss later"). `VAL06` §E notes that the TLE epoch's own time scale is *not* formally
  documented — "the time system is assumed here to be UTC, but no formal documentation
  exists" — and that "the error associated with approximating UT1 with UTC is within the
  theoretical uncertainty of the SGP4 theory itself". This module therefore treats a TLE
  epoch as UTC, treats that as an **assumption**, and requires it to be recorded as such
  wherever a TEME state's provenance is written.
- **FRAME-R-033.** A TEME state MUST carry an **uncertainty floor** reflecting the frame's
  definitional ambiguity. Identifying PEF with TIRS — which `FRAME-R-030` does, and which is
  unavoidable if TEME is to reach a modern frame at all — imports the difference between the
  IAU-76/80 realisation of the true pole and the IAU 2006/2000A CIP, of order 0.1 arcsec,
  i.e. of order **3 m at 7000 km**. This is a property of TEME, not a defect of the
  implementation, and no amount of care removes it.

  **This floor has been measured, not only estimated.** The JPL Horizons ephemeris for the
  comparison object forward of a TLE epoch *is that TLE*: the kernel's coverage ends at the
  TLE epoch plus 15.000 days, matching to the millisecond, and it tracks an independently-run
  SGP4 to **2–3 m flat, with no growth over five days** (manager's determination, 2026-09-18,
  from public data). A discrepancy that does not grow with propagation time is not a
  propagation error — it is a constant difference of convention, which is exactly what this
  requirement describes. The observed 2–3 m and the predicted ≈ 3 m are the same quantity.
- **FRAME-R-034.** TEME MUST NOT be an input to the dynamics. It is an ingestion frame only:
  states enter in TEME from SGP4, are converted immediately, and the TEME value is not
  retained as the state of record.

The consequence of `FRAME-R-033` for the plan's acceptance targets is discussed at
`FRAME-Q-001`.

### 4.6 RTN — the local orbital frame

Defined from a position and velocity in a common inertial frame (GCRS in this tree):

```
ê_R = r / |r|                                   radial, outward
ê_N = (r × v) / |r × v|                         normal, along the orbit angular momentum
ê_T = ê_N × ê_R                                 transverse, completing the right-handed set
```

- **FRAME-R-040.** The axis order MUST be **(R, T, N)** — radial, transverse, normal — and the
  matrix mapping inertial components to RTN components MUST have ê_Rᵀ, ê_Tᵀ, ê_Nᵀ as its rows.
- **FRAME-R-041.** The interface MUST name the frame `RTN` and MUST NOT offer `RSW`, `RIC`,
  `RTW`, `NTW` or `QSW` as aliases. Those names denote overlapping but not identical
  conventions in the literature, and an alias is an invitation to a silent axis swap. A source
  document using another name is translated explicitly at the point of ingestion.
- **FRAME-R-042.** The documentation MUST state that **ê_T is not the velocity direction**
  except for a circular orbit; the angle between them is the flight-path angle. Anyone
  decomposing a velocity or a thrust along "along-track" needs to know which of the two is
  meant, and this spec means ê_T.
- **FRAME-R-043.** Transforming a **velocity** into RTN MUST be documented as using the basis
  frozen at the epoch, with no account taken of the basis's own rotation (which is the orbital
  angular rate, ≈ 10⁻³ rad s⁻¹ — not a small quantity). RTN is for expressing *differences*,
  covariances and empirical accelerations at an epoch; it is not a frame to integrate in.
- **FRAME-R-044.** |r × v| = 0 (rectilinear motion, or a zero vector) MUST be a refusal
  (`FRAME-F-004`), not a fallback to an arbitrary normal.

### 4.7 DYB — the Sun-oriented frame

Used by the empirical solar-radiation-pressure models (ECOM and its successors) and by the
box-wing formulation. Defined from the spacecraft's geocentric position and the Sun's position
in the same inertial frame:

```
ê_D = (r_Sun − r_sat) / |r_Sun − r_sat|              spacecraft → Sun
ê_Y = (ê_D × ê_r) / |ê_D × ê_r|,   ê_r = r_sat/|r_sat|
ê_B = ê_D × ê_Y
```

- **FRAME-R-050.** The sense of ê_D MUST be **spacecraft → Sun**, and MUST be stated at every
  point where a DYB component is reported, because the opposite convention is also in use and
  the sign error it produces is a sign error in the estimated SRP scale — which a fit absorbs
  without complaint.
- **FRAME-R-051.** ê_Y MUST be constructed from the geocentric radial direction as above for
  the nominal (yaw-steering, nadir-pointing) case, which makes ê_Y the solar-panel rotation
  axis. Where a spacecraft's attitude is modelled explicitly (F6), ê_Y MUST instead be taken
  from the modelled body frame, and the two definitions MUST NOT be mixed within one arc. They
  differ during eclipse, during yaw manoeuvres, and at low Sun-elevation (β) angles — exactly
  the regimes where SRP mismodelling is largest.
- **FRAME-R-052.** ê_D ∥ ê_r — the spacecraft exactly on the Earth–Sun line — MUST be a refusal
  (`FRAME-F-005`), not a fallback. The condition is reachable, and the returned basis would
  otherwise be arbitrary.

The primary sources for this frame (`BEU94`, `ARN15`) could not be obtained in full text; the
definition above is stated from first principles so that it is unambiguous regardless, and the
gap is recorded at `FRAME-Q-002`.

---

## 5. Interfaces

```
Frame        := GCRS | CIRS | TIRS | ITRS | TEME            -- closed, global frames
State<F>     := { epoch: Epoch, r: Vec3[km], v: Vec3[km/s] } -- F is part of the type
EopAt        := { xp[rad], yp[rad], dut1[s], lod[s], dx[rad], dy[rad],
                  quality, subdaily_applied: bool }          -- from SPEC-eop
Basis        := Mat3                                         -- rows are the target frame's axes
```

```
-- the chain, exposed both as matrices and as a state transform
rotation(from: Frame, to: Frame, epoch: Epoch, eop: EopAt)
        -> Result<{ m: Mat3, omega: Vec3[rad/s] }, FrameError>

transform<F, G>(s: State<F>, eop: EopAt) -> Result<State<G>, FrameError>

-- the CIO chain's intermediate products, exposed because the partials need them
cip_and_cio(epoch: Epoch, dx, dy) -> { x, y, s, q: Mat3 }
era(epoch: Epoch, dut1: Duration)  -> Result<f64[rad], FrameError>
polar_motion(epoch: Epoch, xp, yp) -> Mat3

-- local frames: rotations only, no State type, because they are frames of a state
rtn_basis(r: Vec3, v: Vec3)             -> Result<Basis, FrameError>
dyb_basis(r_sat: Vec3, r_sun: Vec3)     -> Result<Basis, FrameError>

model_version() -> { precession: "IAU 2006", nutation: "IAU 2000A", origin: "CIO",
                     erfa: <version string> }
```

Notes for the manager's review:

- **`EopAt` is passed in, not fetched.** `frames` has no file access and no cache. Every
  transformation is a pure function of (epoch, EOP record, vectors). This is what keeps
  `FRAME-R-020`'s exact round trip achievable and what makes the module trivially testable
  with synthetic EOP.
- **`rotation()` returns ω⃗ alongside the matrix**, rather than a second matrix. The velocity
  transformation is not a matrix product (§4.2), and returning a 6×6 would suggest it is.
- **`transform` is generic over the frame tags**, which is how `FRAME-R-003` is enforced.
  In a language without generics, the same contract is met by one named function per ordered
  pair, and by there being no function that takes a runtime frame tag and a raw vector.
- **`rtn_basis` and `dyb_basis` return a rotation, not a `State`.** RTN and DYB are not frames
  a state is *stored* in (`FRAME-R-043`), so making them inhabitants of `Frame` would license
  exactly the misuse the spec forbids.
- **`model_version()` exists** so that `FRAME-R-002` is mechanical rather than a matter of
  someone remembering to write it down.

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `FRAME-P-1` | ITRS → GCRS → ITRS round trip, LEO state | < 1 × 10⁻⁶ km = **1 mm** | — | plan §4; a self-consistency bound, achievable far tighter given `FRAME-R-020` |
| `FRAME-P-2` | agreement with ERFA's published test values for the chain components | bit-comparable | — | plan/handover §4 |
| `FRAME-P-3` | GCRS ↔ ITRS orientation accuracy vs the IAU 2006/2000A model | 10 µas × 0.034 mm/µas = **0.34 mm** at 7000 km | the model's own realisation | `TN36-5`; limited by the EOP, not by this module |
| `FRAME-P-3b` | frame bias, if omitted | 23 mas × 34 mm/mas = **0.78 m** at 7000 km | — | §4.4; stated so omission is recognisable by its size |
| `FRAME-P-4` | ITRS velocity | ≈ 0.1 mm s⁻¹ | — | §4.2, `FRAME-R-022`; dominated by the neglected Q̇ term |
| `FRAME-P-5` | TEME → GCRS, definitional floor | ≈ 3 m at 7000 km | — | §4.5, `FRAME-R-033`; irreducible |
| `FRAME-P-5b` | agreement with `VAL06`'s published ITRS↔TEME example | **25 mm**, of which 13.3 mm is measured and characterised as a pure z-rotation | — | `FRAME-A-001`'s note |
| `FRAME-P-6` | SGP4 TEME state vs a JPL Horizons table, 5-hour arc | **< 20 m** | — | plan §4 gate, retained. **It measures convention agreement, not orbit accuracy** — the Horizons ephemeris is the same TLE (§4.5) — so the expected residual is `FRAME-P-5`'s ≈ 3 m floor. The plan's former "expect < 1 m" was struck on 2026-09-18; see `FRAME-Q-001`. |

Note that `FRAME-P-1` is a **self-consistency** test and proves nothing about accuracy: a
transformation that is consistently wrong round-trips perfectly. It is retained because it
catches a large class of implementation errors cheaply, and because the predecessor's measured
1.29 × 10⁻⁷ km gives a known-good scale — but the accuracy evidence is `FRAME-A-001` and
`FRAME-A-004`, not this.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `FRAME-F-001` | external state labelled only "J2000", "EME2000" or "ECI" | the label found, the source, and the frames it could mean | assume GCRS; assume mean-of-J2000 |
| `FRAME-F-002` | transformation requested at an epoch the supplied EOP record does not cover | the epoch and the EOP record's validity (delegated: the EOP layer refuses first) | zero the EOP; hold the last value |
| `FRAME-F-003` | EOP record supplied with `subdaily_applied = false` where §4.1 requires the restored values | which terms are missing and their magnitude | proceed with the raw interpolated values |
| `FRAME-F-004` | RTN basis requested with \|r × v\| below the threshold at which the normal is numerically meaningless | \|r\|, \|v\|, \|r × v\|, and the threshold | pick an arbitrary normal; return the previous basis |
| `FRAME-F-005` | DYB basis requested with ê_D ∥ ê_r | the angle between them and the threshold | pick an arbitrary ê_Y |
| `FRAME-F-006` | TEME conversion requested for an epoch outside the leap-second table's UTC validity | delegated to `SPEC-time.md` `TIME-F-004` | approximate UT1 by UTC |
| `FRAME-F-007` | a state whose frame tag is not one this operation accepts | both frames and the operation | transform "helpfully" via an assumed chain |
| `FRAME-F-008` | an ERFA routine returns a non-zero status | routine, status, epoch | use the returned value |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `FRAME-A-001` | **PRIMARY FRAMES GATE.** ITRS ↔ TEME on Vallado's worked example. 2004-04-06T07:51:28.386 UTC; ΔUT1 = −0.439 961 s; ΔAT = 32 s; *x*_p = −0.140 682″, *y*_p = 0.333 309″; LOD = 0.001 556 3 s. Given `r_ITRF = (−1033.479 383 00, 7901.295 275 40, 6380.356 595 80)` km and `v_ITRF = (−3.225 636 520, −2.872 451 450, 5.531 924 446)` km s⁻¹, produce TEME. | `r_TEME = (5094.180 107 20, 6127.644 705 20, 6380.344 532 70)` km; `v_TEME = (−4.746 131 494, 0.785 817 998, 5.531 931 288)` km s⁻¹ | `VAL06` Appendix C — **published worked example** | **25 mm** in position, 0.1 mm s⁻¹ in velocity; **and the residual MUST remain a pure rotation** — its radial component below 1 nm — see the note below | R-030, R-032, P-5 |
| `FRAME-A-002` | the same example in reverse, TEME → ITRS | the published ITRF vectors | `VAL06` Appendix C | 1 mm, 1 µm s⁻¹ | R-020, R-030 |
| `FRAME-A-003` | ITRS → GCRS → ITRS round trip on a LEO state, 10³ epochs over 1995–2035 | identity | closed-form identity | **< 1 mm** position, < 1 nm s⁻¹ velocity | P-1, R-020 |
| `FRAME-A-004` | each chain component against ERFA's own distributed test values: `eraXy06`, `eraS06`, `eraC2ixys`, `eraEra00`, `eraSp00`, `eraPom00`, `eraC2tcio`, `eraGmst82` | ERFA's published expected values | `ERFA` test suite (`t_erfa_c.c`) — **published verification values** | bit-comparable | P-2, R-001, R-010…R-017 |
| `FRAME-A-005` | full chain with δ*X* = δ*Y* = 0 against `eraC2t06a` at 10³ epochs | agreement | `ERFA` — an independent composition of the same components | < 1 µas equivalent | R-017 |
| `FRAME-A-006` | the CIP coordinates at J2000.0 TT, with all periodic terms and offsets zero | *X* = −0.016 617″, *Y* = −0.006 951″ | `TN36-5` eq. (5.16) — **published series constant terms**; these are the frame bias in the pole direction | 1 µas | R-025 |
| `FRAME-A-007` | *s′* at 2026.0 | ≈ −12 µas | `TN36-5` eq. (5.13), *s′* = −47 µas·*t* | within 1 µas | R-015 |
| `FRAME-A-008` | ω⃗ magnitude with LOD = 0 | 7.292 115 146 706 979 × 10⁻⁵ rad s⁻¹ | `TN36-5` eq. (5.14), differentiated | exact to f64 | S-021 |
| `FRAME-A-009` | **TEME → GCRS on Vallado's TLE example.** TLE `00005`, propagated to day 182.784 950 62, `r_TEME = (−9060.473 735 69, 4658.709 525 02, 813.686 731 53)` km. | `VAL06` publishes `r_J2000 = (−9059.941 554 1, 4659.697 199 0, 813.956 940 2)` km using the **IAU-76/FK5** chain. This implementation uses IAU 2006/2000A, so the expected result is Vallado's vector **displaced by the known model difference**. | `VAL06` Appendix C eq. (C-3) | agreement to **3–5 m**, *and* the residual direction consistent with a rotation of ≈ 0.06 arcsec about the pole. A residual below 1 m or above 10 m is a **failure**, in both directions — see note below. | R-031, P-5 |
| `FRAME-A-010` | "of date" vs "of epoch" on the same example | the two differ by ≈ 23.6 m over three days | `VAL06` Appendix C, which states that figure | within 20 % | R-031 |
| `FRAME-A-011` | RTN basis orthonormality and handedness on 10⁴ random states | ê_R·ê_T = ê_T·ê_N = ê_N·ê_R = 0; det = +1 | closed-form identity | 1e-15 | R-040 |
| `FRAME-A-012` | RTN on a circular orbit: angle between ê_T and v̂ | 0 | closed-form identity | 1e-12 rad | R-042 |
| `FRAME-A-013` | RTN on an eccentric orbit (e = 0.7) at true anomaly 60°: angle between ê_T and v̂ | the flight-path angle from the conic, arctan[e sin ν/(1+e cos ν)] = **24.18°** | closed-form (conic geometry) | 1e-9 rad | R-042 |
| `FRAME-A-014` | DYB basis orthonormality, handedness, and that ê_D points **towards** the Sun | det = +1; ê_D·(r_Sun − r_sat) > 0 | closed-form identity + `FRAME-R-050` | 1e-15 | R-050, R-051 |
| `FRAME-A-015` | refusals `FRAME-F-001`, `-F-004`, `-F-005`, `-F-007` each fire with the specified content | as tabulated in §7 | this spec | — | F-001, F-004, F-005, F-007, R-044, R-052 |
| `FRAME-A-016` | δ*X*, δ*Y* omission changes the result by the expected order | ≈ 10 mm at 7000 km for δ*X*, δ*Y* ≈ 0.3 mas | `TN36-5` §5.5.4 magnitudes | order of magnitude | R-012 |
| `FRAME-A-017` | SGP4 TEME state vs a JPL Horizons table, same object, 5-hour arc. **A convention-matching test, not an accuracy measurement** — the Horizons ephemeris forward of the TLE epoch is that TLE (§4.5). | **< 20 m**, with the residual expected at `FRAME-P-5`'s ≈ 3 m floor and **flat in propagation time** | JPL Horizons — public data; the table's own time scale must be established, not assumed (`SPEC-time.md` `TIME-R-004`) | 20 m; **and the residual MUST NOT grow with propagation time** — growth indicates a propagation or timescale error, not a convention difference | P-6 |
| `FRAME-A-018` | type-level: a TEME state cannot be passed where GCRS is required, and neither can a BCRS state | compilation failure, or a boundary refusal | this spec | — | R-003, R-004, R-005, R-034, R-041 |
| `FRAME-A-023` | structural: no BCRS↔GCRS function exists taking only an epoch and an EOP record; the translation's signature demands the Earth's barycentric state, and its name says translation | compilation failure for the rotation-shaped call | this spec | — | R-027, R-028 |
| `FRAME-A-019` | **structural:** the ITRS frame type admits a realisation tag without a breaking change — demonstrated by a branch that adds an ITRF2014/ITRF2020 tag and compiles every caller unchanged | as stated | this spec, `FRAME-R-026` | — | R-026 |
| `FRAME-A-020` | a state produced by TEME → GCRS carries a non-zero uncertainty floor of the order stated in `FRAME-P-5` | ≈ 3 m at 7000 km, present in the state, not in a comment | this spec, `FRAME-R-033` | order of magnitude | R-033 |
| `FRAME-A-021` | fault injection: an ERFA routine made to return a non-zero status | refusal `FRAME-F-008` naming routine, status and epoch; **the returned value is not used** | this spec | — | F-008 |
| `FRAME-A-024` | structural: a `Vec3` does not convert to a `Position<F>`, a `Position<GCRS>` does not convert to a `Position<ITRS>`, and neither is default-constructible | compile failure in all three | this spec, `FRAME-R-060` | — | R-060, R-061 |
| `FRAME-A-022` | the composed chain is inverted by transposition, not by numerical inversion — a matrix perturbed off orthogonality by 10⁻⁹ is refused rather than inverted | as stated | this spec, `FRAME-R-004` | — | R-004 |

**Note on `FRAME-A-001`'s tolerance, which is not the 1 mm version 1.2 asserted.** Achieved:
**13.3 mm** out of 10 208 km, i.e. 1.3 × 10⁻⁹ relative, and 8 µm s⁻¹ in velocity. Most of the
original discrepancy was the kinematic equation-of-equinoxes term, which `FRAME-R-030` v1.2
wrongly forbade; that was 85 mm.

**What remains is recorded as unexplained rather than tidied away.** It is a *pure rotation about
z* of 3.45 × 10⁻⁴ arcsec — radial component 2 × 10⁻⁸ mm, z component 0.05 mm — and it is not
explained by the choice of expression for the kinematic term, because the two-term form the paper
prints and ERFA's full complementary series `eraEect00` differ by only 8 × 10⁻⁶ arcsec at that
epoch where 3.45 × 10⁻⁴ is needed. It sits somewhere in `VAL06`'s own formulation of that term,
which the paper describes but does not print. The tolerance is set above the characterised
residual and the *shape* of the residual is asserted separately: a radial component would mean a
scale or a units error, which no rotation can produce, so that assertion still catches the class
of defect the tight tolerance was meant to catch.

**Note on `FRAME-A-009`.** This test is unusual and deliberately so: it asserts a
*disagreement* of a predicted size. Vallado's published J2000 vector was computed with
IAU-76/FK5; this tree uses IAU 2006/2000A; the two must differ, and the size and direction of
the difference are predictable from §3.3. A result that agreed to a millimetre would mean the
implementation had silently reproduced the older model — which is the failure this test
exists to catch, and which no "agrees with the published value" test could detect.

**Coverage.** Every requirement and refusal in this spec is discharged by at least one row
above, except the following, listed in full:

| id | why no test |
|---|---|
| `FRAME-R-002` | Provenance emission. Discharged by review of the run-output format and by `PROVENANCE.md` §1's model declaration. |
| `FRAME-R-022`, `FRAME-R-043` | Documentation requirements — that the neglected velocity terms and the frozen-basis caveat are stated with their magnitudes. Discharged by review of the module's output documentation. |
| `FRAME-R-029` | The timescale basis of a BCRS state, and the un-applied *L*_B scaling, are statements the module makes about itself. Discharged by review of the translation operation's documentation and by `FRAME-Q-006` remaining open; there is nothing to assert while the scaling is deliberately not applied. |
| `FRAME-R-023`, `FRAME-R-024` | Properties of the calling layers (integrate in GCRS; partials reuse the state's matrices). Discharged by review there, and they belong to F1's spec when it is written. |
| `FRAME-F-002`, `FRAME-F-003`, `FRAME-F-006` | **Delegated refusals**, tested where they are raised: `SPEC-eop.md` `EOP-A-015`/`-A-016` (coverage), `EOP-A-021` (sub-daily not applied) and `SPEC-time.md` `TIME-A-015` (leap-table expiry). This spec's obligation is to propagate them unchanged, which those tests observe from the caller's side. |

`FRAME-A-018`, `-A-019` and `-A-022` are stated as tests although in a statically-typed
implementation they are compile-time properties; they are then discharged by compile-fail test
cases rather than runtime ones.

---

## 9. Provenance obligations

- **Module register:** `frames` → `TN36-5`, `TN36-1`, `VAL06`, and ERFA at the version used.
- **Parameter register:** the ERA polynomial coefficients of eq. (5.14); ω_E; *s′* = −47 µas·*t*;
  the GMST-1982 coefficients of `VAL06` eq. (2); the frame-bias constants — each with source
  and date.
- **Dependency register:** ERFA, version, BSD 3-clause, and the list of routines called
  (explicitly noting that `eraC2t06a` is used in tests only, per `FRAME-R-017`).
- **Model declaration:** "IAU 2006 precession / IAU 2000A nutation, CIO-based", recorded once
  and emitted with every run, per `FRAME-R-002`.
- **Oracle log (plan R4):** the 2.2 m predecessor TEME residual and the 1.29 × 10⁻⁷ km
  predecessor round trip are prior behavioural observations, recorded as oracle rows with the
  note that they were measured under IAU-76/80 and are therefore not comparable targets.

---

## 10. Open questions for the manager

| id | question | recommendation / **resolution** |
|---|---|---|
| `FRAME-Q-001` | **The plan expects the TEME/Horizons residual to fall "below 1 m" after the IAU 2006/2000A upgrade (plan §4; handover §4 "expect better than 2.2 m"). §4.5 argues that TEME carries an irreducible definitional floor of order 3 m at LEO.** These are in tension. Two readings reconcile them, and they have different consequences: (a) the comparison is against a Horizons ephemeris that is *itself* SGP4-derived from the same TLE, in which case the test measures only the difference between two TEME→inertial conventions and sub-metre agreement is achievable **if and only if** Horizons' convention is matched exactly; (b) the comparison is against an independent precise ephemeris, in which case SGP4's own kilometre-class error dominates and neither 2.2 m nor 1 m is meaningful. | **RESOLVED 2026-09-18, and AMENDED the same day after implementation.** The Horizons ephemeris for the comparison object forward of a TLE epoch *is that TLE* — kernel coverage ending at TLE epoch + 15.000 days to the millisecond, tracking an independently-run SGP4 to 2–3 m flat with no growth over five days — so the test never measured orbit accuracy. **< 20 m stays the gate**, "expect < 1 m" is struck, `FRAME-A-001` is the primary frames gate, and `FRAME-A-009`'s assert-a-predicted-disagreement pattern is standing. **The amendment:** v1.2 described the residual as a difference of *convention*. It is not — it is a genuine **model** difference, the IAU-76 versus IAU-2006 precession difference of 0.064 arcsec, which is 2.245 m at T-01's 7234 km radius and therefore essentially all of the 2.2 m. It appears there and not on the ITRF path because TEME is referred to the mean equinox of date, a model construct with no celestial-pole offset series to reconcile two models against an observation (§3.3). The kinematic terms are ≈ 95 mm at that radius, 4 % of it, and are not the cause: a gate set from them would have been twenty-five times too small. |
| `FRAME-Q-002` | **`BEU94` and `ARN15`, the primary sources for the DYB frame, could not be obtained** — the 1994 paper is in a journal with no accessible archive, and the 2015 paper is paywalled at Springer (abstract only). §4.7's definition is stated from first principles and is internally unambiguous, but the **sign of ê_D** and the **construction of ê_Y** (geocentric radial vs modelled body axis) are conventions I fixed rather than inherited. | Obtain `ARN15` before P4 (the phase that implements ECOM). Until then the definition stands as written, and `FRAME-R-050`/`-051` require it to be restated at every reporting point so that a later correction is a one-line change rather than a hunt. If ECOM coefficients are ever compared against published CODE values, the conventions must be confirmed first — a sign disagreement in ê_D is invisible in a fit and visible only in the sign of the reported parameter. |
| `FRAME-Q-003` | **Is `frames` right to take EOP as a passed-in record rather than a queried service?** It makes the module pure and trivially testable, at the cost of every caller threading an `EopAt` through. | **CONFIRMED 2026-09-18, as specified.** The alternative puts a cache and a file dependency in the module that the variational equations call thousands of times per arc, and it makes the "which EOP did this run use" question un-answerable from the state alone. |
| `FRAME-Q-004` | **Velocity accuracy.** §4.2 neglects Q̇ (54 µm s⁻¹ at LEO) while applying a LOD correction of similar size. Including Q̇ is not hard — it is a numerical differentiation of Q over a few seconds, or the analytic CIP rate. | Leave it neglected for P1 and state the 0.1 mm s⁻¹ figure honestly. Revisit only if a measurement model in F11 turns out to need ITRS velocity better than that; none of the P1–P6 campaigns does. Recorded here so the decision is visible rather than accidental. |
| `FRAME-Q-006` | **The TDB/TT scaling between BCRS and GCRS quantities is not applied** (`FRAME-R-029`). IAU 2006 Resolution B3 makes TDB a linear transform of TCB, and lengths compatible with one differ from the other by *L*_B = 1.55 × 10⁻⁸ — 2.3 m on an astronomical unit, well above anything this tree measures. | Leave it unapplied at L2 and state it, as `FRAME-R-029` does: the ephemerides are TDB-compatible by construction and the geocentric chain never sees an astronomical unit, so nothing in L1–L4 crosses the boundary where it would matter. Revisit before L6, where a light-time solution spans both. Recorded so that if a metre-level discrepancy appears on a barycentric path, this is the first place to look. |
| `FRAME-Q-005` | **ITRF realisation.** The EOP series in use is consistent with ITRF2020 (`SPEC-eop.md` §2), and station coordinate sets (SLRF2020, IGS products) have their own realisations. Nothing in this spec pins them together. | **DECIDED 2026-09-18: shape now, tag before P6** — specified at `FRAME-R-026`. Sharper than first written: correction §3.1 of `SPEC-eop.md` makes this a live hazard rather than a hypothetical one, because EOP 20 C04 is ITRF2020 where the superseded 14 C04 was ITRF2014, so data of the two eras really is in two realisations. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.5 | 2026-09-18 | **`Position<F>` and `Acceleration<F>` added** at L2 step 2's request, §3.6, with `FRAME-R-060`/`-061` and `FRAME-A-024`. Amended here rather than declared in `SPEC-gravity.md`, on the same ruling that placed `Frame::BCRS` here. Additive: nothing existing changed. The one thing to read twice is that these carry **metres** where `State` carries km, with the unit in the accessor name on both sides. |
| 1.4 | 2026-09-18 | **`Frame::BCRS` added** at L2's request. Amended here rather than declared in `SPEC-ephemerides.md`, because the frame enumeration belongs to this specification and a second spec extending it silently is how enumerations drift. §3.5 carries the two things that make BCRS unlike the other members — the transformation is a **translation** and the timescale basis differs — as `FRAME-R-027`…`FRAME-R-029`, with `FRAME-A-023` and `FRAME-Q-006`. §6's rows carry their multiplication. |
| 1.3 | 2026-09-18 | **Amended after implementation.** `FRAME-R-030` corrected: the kinematic equation-of-equinoxes terms are **required**, not forbidden — v1.2 conflated them with the equinox-based TOD route, whose ambiguities are all in the geometric nutation terms this chain never uses. `FRAME-A-001`'s tolerance restated at 25 mm with a pure-rotation assertion and the residual 3.45 × 10⁻⁴ arcsec z-rotation recorded as unexplained. §3.3 gains the mechanism: the celestial-pole offset series cancel the model difference on the ITRF path by construction (measured, 1.56 mm), and TEME has no such series, which is why the difference appears there undiluted. `FRAME-Q-001` corrected from "convention" to "model difference". `FRAME-P-5b` added. |
| 1.2 | 2026-09-18 | **Acceptance coverage completed.** Added `FRAME-A-019` … `FRAME-A-022` and the §8 *Coverage* table listing every requirement and refusal not discharged by a test, with the reason. v1.0–1.1 claimed the template's coverage rule without meeting it. **No requirement was added, removed or changed**; the adopted requirement set is exactly as at v1.1. |
| 1.1 | 2026-09-18 | **Adopted.** Recorded the manager's decisions: `FRAME-Q-001` resolved — the Horizons comparison ephemeris is the TLE, so the test is convention-matching, "expect < 1 m" struck, `FRAME-A-001` promoted to primary frames gate, `FRAME-A-017` re-documented with a flatness condition, `FRAME-R-033` given the measured corroboration. `FRAME-Q-005` decided — `FRAME-R-026` added. `FRAME-Q-003` confirmed. Corrected a stale frame-bias figure in §4.4 (17 mas → 23 mas) left over from the v1.0 drafting. |
| 1.0 | 2026-09-18 | First draft, P1 tranche, for manager review. |
