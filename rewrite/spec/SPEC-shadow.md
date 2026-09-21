# SPEC-shadow — the eclipse shadow function

| | |
|---|---|
| **Spec ID** | `SHDW` |
| **Status** | **draft** 2026-09-18, for review (v1.1 adds §3.2–§3.4 and the PPM throughout; v1.2 re-derives R-031 and adds R-033, naming the traversal behind the 99.9 % cancellation figure and measuring what moves it) |
| **Version** | 1.2 |
| **Date** | 2026-09-22 |
| **Layer** | L4 `forces-analytic`, step 1 (`doc/REWRITE_PLAN.md` §3.5) |
| **Depends on** | `SPEC-ephemerides.md` (the Sun), `SPEC-frames.md`, `core` |
| **Depended on by** | `srp-analytic` (step 2), `erp` (step 4), every SRP model at L9 |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no implementation
of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 1. Purpose

*F*ₛ, a unitless quantity in [0, 1] scaling the solar flux at a satellite during an eclipse. The
conical model first; the perspective-projection model with atmospheric effect after.

---

## 2. Normative sources

| key | what | obtained | role |
|---|---|---|---|
| `LI19` | Li, Ziebart, Bhattarai & Harrison (2019), *A shadow function model based on perspective projection and atmospheric effect for satellites in eclipse*, Adv. Space Res. **63**(3) 1347–1359, doi:10.1016/j.asr.2018.10.027 | **yes** — accepted manuscript, `li-ziebart-2019-shadow`, SHA-256 `b07c8b89…97bd` | **normative** |
| `GEOMETRY` | The conical eclipse geometry itself — a sphere occulting a disc | — | **normative, and it is mathematics** |

**`LI19` is a `literature` manifest entry** (plan §5 constraint 3): pinned by hash as a provenance
record, exempt from the permissive-licence gate because nothing derived from it is a copy of it,
and unreachable from any build input — which `ci.sh` gate 11 proves by injection. Its terms could
not be established; the entry records the search rather than a conclusion.

**Retrieval, recorded because it cost an hour** (rule 4 applied to a retrieval): the publisher's
page and the UCL Discovery landing page both return **403**. The manuscript is reachable only at
the path the open-access metadata names.

**Plan rule 8's question is answered for both halves — but not by the sentence v1.0 used, and the
whole statement is re-derived rather than one term patched.** v1.0 said `LI19` prints the *full*
derivation of both models. It does not quite: equations 36–39 delegate the area of the "elliptical
arch" *S*₁ and *S*₂ to **Hughes and Chraibi (2012)**, which this tree does not hold and did not
seek. The conclusion survives, for a different reason than the one given — **the delegation is not
load-bearing**. An ellipse is an affine image of a circle and a hyperbola of the unit hyperbola;
an affine map scales every area by |det|, and Green's theorem gives ½∮(*x* d*y* − *y* d*x*) as
½*AB* dψ on the one and ½σ*AB* dτ on the other. The arch is therefore elementary from the conic
alone, and §4's `SHDW-R-026` derives it in one line. So an independent specification exists for
the perspective half, and `LI19`'s own second reference is not needed to have one.

`LI19` also publishes working code. Under rule 8 that code is an **oracle and not normative**,
because the paper it implements is the specification; under the rule's converse, where an
independent specification exists the artefact's legibility stops mattering. It was not sought, not
retrieved and not read, and nothing here depends on it.

---

## 3. "Conical" is a family, not a model

A named model that is really a family is how two correct implementations disagree by a factor
nobody can find. `LI19`'s SECM — *Spherical Earth Conical Model* — makes five choices, and this
specification takes all five, each recorded with the paper's own words:

| choice | SECM's answer | the paper's words |
|---|---|---|
| the Earth's figure | **sphere** | the *S* in SECM; §1 notes "Earth is closer to an ellipsoid than a sphere", which is the PPM's entire purpose |
| the Sun | **a disc**, of finite angular radius | it has a "whole solar disk" area; not a point source |
| which states | **umbra, penumbra and annular** | Fig. 2: "it can describe all the possible eclipse states including annular umbra" — against the CYM, which "can only describe umbra" |
| *F*ₛ inside the penumbra | **the true occulted-area ratio** | "the ratio of the unblocked solar disk area to the area of the whole solar disk" — *not* linear in the occulted fraction, *not* a smoothstep |
| the atmosphere | **none** | SECM_atm is the separate variant; this step's conical half is the atmosphere-free model |

- **SHDW-R-001.** All five are implemented as stated, and each is named in the source beside the
  code that makes it, not only here.

### 3.1 The annular branch is unreachable from any orbit in this plan

An annular eclipse *of the Sun by the Earth* requires the satellite to be **beyond the umbra
cone's apex**, where the umbra has closed to a point:

  `SHDW-P-1` (§6) is that distance: 6378.137 km × 1.495978707 × 10⁸ km × 1.450749 × 10⁻⁶ km⁻¹
  = **1.3842 × 10⁶ km**.

Nothing this plan flies is remotely near it:

| | radius | as a fraction of the apex | umbra radius there |
|---|---|---|---|
| GRACE-A | 6 858 km | 0.0050 | 6 346 km (99.50 % of *R*⊕) |
| the sail / LEO | 7 331 km | 0.0053 | 6 344 km (99.47 %) |
| GNSS | 26 560 km | **0.0192** | 6 256 km (98.08 %) |
| the Moon | 384 400 km | 0.278 | — |

At GNSS the umbra has narrowed by under 2 % and is still 6 256 km across; the apex is **52×**
further out.

- **SHDW-R-002.** The annular branch is **implemented** — it is in `LI19`'s model and omitting it
  would be a different model — and its domain is stated **next to it**, and **its test is labelled
  synthetic**, because no orbit in this plan can reach it.

  This matters for a reason the branch itself cannot show: **a branch that passes because nothing
  reaches it is the guard that cannot fire wearing a different hat**, and whoever later notices it
  is never exercised must be able to tell that this is by design rather than by neglect.
  `SHDW-A-005` exercises it from a synthetic geometry at 2 × 10⁶ km and says so in its name.

### 3.2 The PPM is the other member, and it differs in every one of the five

| choice | SECM | PPM | where it shows |
|---|---|---|---|
| the Earth's figure | sphere | **ellipsoid**, *x*ᵀ*A**x* = 1, *A* = diag(*a*⁻², *a*⁻², *b*⁻²) | the model's entire purpose |
| the Sun | a disc of angular radius | **a sphere**, projecting to a circle of radius γ*R*ₛ/‖*r* − *r*ₛ‖ (eq 28) | eq 27–28 |
| which states | umbra, penumbra, **annular** | umbra, penumbra — **no annular branch**; the states come out of areas, not out of comparing angular radii | §3.1 |
| *F*ₛ in the penumbra | occulted-area ratio of two **circles** | occulted-area ratio of a **circle against a conic** | eq 22–24 |
| the atmosphere | none | **selectable**: PPM, or PPM_atm by eq 40–46 | §2.6 |

- **SHDW-R-021.** The PPM is stated in an **Earth-fixed** frame and the conical model in `GCRS`,
  and the two interfaces differ in their position type so that the mistake cannot compile.
  *A* = diag(*a*⁻², *a*⁻², *b*⁻²) is the Earth only in a frame that turns with it; in `GCRS` the
  same matrix is an ellipsoid fixed in inertial space, which is not a body. A sphere is a sphere
  in every frame, so the question never arises for the SECM — which is exactly why an
  interchangeable signature would be a trap. Plan §5 constraint 10.

**The silhouette is a hyperbola at LEO, and this is not an edge case.** `LI19` eq 24 sorts the
projection by |*B*|, and the hyperbolic branch is `LI19`'s "partial image": the satellite sees
only part of the Earth. For a sphere the condition reduces to |cos ψ| < *R*ₑ/*r*, with ψ the angle
between the satellite's position and the Sun–satellite line, while the terminator sits at
ψ = asin(*R*ₑ/*r*). The two windows overlap below about 8 400 km:

| orbit | *r* | silhouette in the penumbra |
|---|---|---|
| ISS, LEO, Sun-synchronous | 6 778 – 7 500 km | **hyperbola** |
| GPS, Galileo, GEO | 26 560 – 42 164 km | ellipse |

- **SHDW-R-022.** Both branches are reachable from real orbits, so both are gated from real
  geometries. **This is the opposite of §3.1's annular branch**, and the difference is recorded
  because the two look alike in the source and are not alike at all: one is unreachable and
  exercised synthetically, the other carries every low orbit this plan will fly.

### 3.3 The sixth choice, which `LI19` does not name: where the ratio is taken

*F*ₛ is the fraction of the solar disc's **solid angle** that is not occulted. Both models
approximate it and they do so by different routes, so neither is the other's oracle (plan §4
rule 2). The choice neither §3 nor the paper names is **where the ratio is taken**:

- the **SECM** takes it on the **flat sky**, using the angular radii *a*ₛ and *a*ₑ as if they were
  planar lengths and overlapping two circles;
- the **PPM** takes it in a **perspective projection**, which maps the straight lines of the
  occultation to straight lines and is therefore exact up to the disc-area-to-solid-angle ratio.

Measured at each orbit's terminator against the definition computed with no projection in it at
all (`SHDW-A-008`'s comparator):

| orbit | \|SECM − definition\| | \|PPM − definition\| | oblateness, \|PPM(WGS84) − PPM(sphere)\| |
|---|---|---|---|
| LEO, *r* = 7 331 km | **1.86 × 10⁻⁴** | 2.1 × 10⁻⁶ | **1.88 × 10⁻⁶** |
| GPS, *r* = 26 560 km | 4.0 × 10⁻⁵ | 1.8 × 10⁻⁷ | 1.34 × 10⁻⁵ |
| GEO, *r* = 42 164 km | 2.5 × 10⁻⁵ | 1.5 × 10⁻⁸ | 2.17 × 10⁻⁵ |

- **SHDW-R-023.** **At LEO the unnamed flat-sky choice is 99 times the oblateness effect the PPM
  is adopted for.** A comparison that swaps SECM for PPM and attributes the whole difference to
  the Earth's figure is therefore wrong by two orders of magnitude at LEO, and right to within a
  factor of two only at GEO. The ratio is asserted, not narrated, by `SHDW-A-009`.
- **SHDW-R-023a.** Both choices are stated **in the module headers**, beside the five, because
  that is where a user of the module meets them; §3 alone reaches only a reader of this document.

### 3.4 The seventh axis, which is larger than either: the Sun's brightness profile

*F*ₛ as an occulted-**area** ratio assumes a **uniformly bright** solar disc. The Sun is limb
darkened, and the law SRP wants is **bolometric** — the quantity is total radiant flux, not a
visible band. Eddington's grey atmosphere gives the emergent intensity as

> *I*(µ)/*I*(1) = (2 + 3µ)/5 = 1 − *u*(1 − µ),  ***u*** **= 3/5 exactly**,

so *I*(limb)/*I*(centre) = 0.4. Measured at LEO across a penumbra passage (`SHDW-A-016`), peak
effect on *F*ₛ:

| axis | peak effect | ratio to the next |
|---|---|---|
| the Sun's **brightness profile** — uniform disc vs limb darkened | **1.97 × 10⁻²** | 83× |
| **where the ratio is taken** — flat sky vs perspective (§3.3) | 2.4 × 10⁻⁴ | 126× |
| the Earth's **figure** — sphere vs WGS 84 (the PPM's purpose) | 1.9 × 10⁻⁶ | — |

- **SHDW-R-030.** **Both models in this module assume a uniform disc**, so this axis does not
  separate them and does not touch §3.3's comparison. What it touches is the meaning of
  `SHDW-A-008`'s figure: **agreement with the uniform-disc definition is not accuracy**, and
  neither model here claims it. The figure is labelled as agreement throughout.
- **SHDW-R-031.** **99.9 % of it cancels across ONE STATED PASSAGE, and the figure is stated
  against that passage rather than as a constant** (plan §4 rule 3: quantities compared are
  measured at one reference point, stated with it). The passage: LEO, *r* = 7331 km, a
  **circular** orbit with the shadow axis lying in the orbital plane (β = 0°), one side of the
  penumbral transition (outer edge to the umbra boundary), duration 12.0 s. There the error is
  antisymmetric about 50 % occultation — a radially symmetric profile puts exactly half its flux
  either side of a central chord, and a circular orbit crosses it at **exactly constant angular
  rate** — so the net time integral is 1.56 × 10⁻⁴ s of equivalent full sunlight against an
  absolute integral of 1.14 × 10⁻¹ s, while the **running** integral still swings to
  5.72 × 10⁻² s mid-passage, **367× the net** [`tools/penumbral_cancellation.py`'s own baseline
  output — corrected from an earlier 375×/376× that predates the committed, reproducible tool and
  does not survive being regenerated by it]. A consumer integrating whole passages barely sees it;
  anything sampling *inside* one — accelerometry, high-rate tracking — sees the full
  1.97 × 10⁻². The cancellation is a property of the passage, not of the model.
- **SHDW-R-033.** **What breaks R-031's cancellation, measured with an actual two-body Kepler
  propagator rather than assumed** — three families, because "an eccentric orbit... breaks
  that" is not one question but at least two, and they have different answers:
  - *Orbital-plane tilt, β toward the eclipse's own cutoff angle a*ₑ. Cancellation degrades
    **smoothly**, not sharply, as β → *a*ₑ: 99.87 % at β = 0, still 99.49 % at β = 0.8*a*ₑ, and
    only inside the **last 1 %** of β/*a*ₑ does it fail substantially — 88.7 % at 0.99*a*ₑ,
    74.7 % at 0.995*a*ₑ, 48.3 % at 0.9995*a*ₑ. In physical terms this is an orbit within roughly
    **half a degree of its own eclipse beta cutoff** — the dawn-dusk / eclipse-season-edge
    regime — not "high beta" generally, where the figure is essentially undisturbed.
  - *Eccentricity, eclipse AT an apse.* **Unchanged by eccentricity, exactly**, and not by
    coincidence of this geometry: an unperturbed two-body orbit is exactly time-symmetric about
    periapsis or apoapsis passage (*r*(−*t*) = *r*(*t*) identically), so an eclipse centred on an
    apse inherits that symmetry regardless of *e*. Measured at *e* = 0, 0.3, 0.5, 0.7, 0.85, all
    99.84–99.90 %, statistically indistinguishable from the circular baseline. **This is the
    configuration "an eccentric orbit breaks it" first suggests, and it does not break it at
    all** — a corrected hypothesis, not a smaller effect. **A null result here is not evidence of
    absence**, and is recorded as such rather than as a clean negative: the first attempt at this
    whole measurement used exactly this configuration, found nothing, and was right for a reason
    that meant it could not have found anything. That is the sibling of §4 rule 5's diagnostic —
    where a result *better* than the method can produce means the method did not run, a result a
    configuration *cannot fail* to produce means the configuration did not test.
  - *Eccentricity, eclipse OFF an apse* — genuine non-zero radial velocity at the crossing, the
    configuration actually behind "entry and exit at different angles." Measurably worse, but
    **modestly**: 99.37–99.84 % across *e* up to 0.85 and an apse offset up to 90°, an order of
    magnitude short of the near-cutoff effect above.

  So of the candidate mechanisms, measurement finds **one** dominant cause — a beta angle within
  about a degree of the eclipse cutoff — not four. The 74.7 % and 48.3 % figures were checked
  against a four-fold refinement in both sample count and azimuthal resolution and did not move
  past the fourth decimal place before being reported (§4 rule 5's diagnostic).
- **SHDW-R-032.** The Eddington law is a **model of the Sun and is stated as one**. Across
  *u* ∈ [0.3, 0.9] the peak runs 8.8 × 10⁻³ … 3.4 × 10⁻², so the ordering above is not sensitive
  to the coefficient: at every value in that range this axis still dominates the other two.

**Nothing in this specification implements limb darkening.** It is measured, named, ordered and
left out, because adding it changes the model and that is not this step's to change. Recorded as
`SHDW-Q-005`, **deferred on need rather than on gateability** — unlike `SHDW-Q-003`, published
bolometric limb-darkening coefficients exist to gate an implementation against; nothing in this
plan yet samples *inside* a penumbra passage, which R-031 is what makes visible as the only
regime where the effect survives. When a consumer that does — accelerometry or high-rate
tracking, at L6/L7, or an L8 campaign — exists, it is implemented against a published bolometric
law with its own provenance and its own gate, and Eddington becomes the cross-check rather than
the source.

---

## 4. Required behaviour

- **SHDW-R-010.** *F*ₛ ∈ [0, 1] always; exactly 1 in full sunlight and exactly 0 in the umbra.
- **SHDW-R-011.** *F*ₛ is **monotone non-increasing** as the satellite moves from sunlight through
  the penumbra into the umbra along any straight path in the shadow's transverse plane.
- **SHDW-R-012.** Continuity at both boundaries: *F*ₛ → 1 approaching the penumbral cone from
  inside, and → 0 approaching the umbral cone from outside.
- **SHDW-R-013.** `odl::Result` throughout; no monadic chaining; no global state.

The PPM adds:

- **SHDW-R-024.** A point of the image plane is blocked **iff** the ray from the satellite through
  it, continued forwards, meets the ellipsoid. From eq 17 that is *g*ᵀ*M**g* ≥ 0 **and**
  *g*ᵀ*A**r* < 0, exactly: the first factor is eq 18's discriminant, and the second selects the
  forward nappe of the tangent cone, because the roots' product (*r*ᵀ*A**r* − 1)/*g*ᵀ*A**g* is
  positive outside the Earth so they share the sign of −*g*ᵀ*A**r*. This replaces eq 15's
  three-case distance test with the quantity the area computation needs anyway, and it is what
  separates the two branches of a hyperbolic silhouette. It cannot cut a branch: on
  *g*ᵀ*A**r* = 0 the form is −*g*ᵀ*A**g*(*r*ᵀ*A**r* − 1) < 0, so the half-plane's boundary never
  touches the blocked set.
- **SHDW-R-025.** The disc/silhouette intersections are found by parametrising the **circle by its
  angle**, not by eq 32's η. The two are the same equation — eq 32 is the Weierstrass substitution
  η = tan(θ/2) — but that substitution has a pole at θ = π, so the point (−*R*₀ + *t*ₓ/2, *t*ᵧ/2)
  is **not the image of any finite η** and drops out of eq 33's root set. In θ the equation is a
  trigonometric polynomial of degree two, hence at most the quartic's four roots, with none
  missing and no Ferrari resolvent.
- **SHDW-R-026.** The occulted area is assembled by **Green's theorem** over the blocked part of
  the disc: ½∮(*x* d*y* − *y* d*x*) is ½[*R*²Δθ + *R*(*c*ₓΔsin θ − *c*ᵧΔcos θ)] on a circular arc,
  ½*AB*Δψ on an elliptical one and ½σ*AB*Δτ on a hyperbolic one. This computes the same area as
  eq 36–39 and collapses their four inside/outside × ellipse/hyperbola cases into one sum; see §2
  for why it needs no second reference.
- **SHDW-R-027.** γ, the image plane's distance, **cancels**, and is an argument only so that it
  can be varied in a test.
- **SHDW-R-028.** `Atmosphere::linear_toa` implements `LI19` eq 40–46 **as printed**, including
  that eq 43 pins *f* at µ₂ = 1 and eq 45 at µ₁ = 0. A mean reduction of one half over the annulus
  is *not* that: it agrees in the middle of a pass and disagrees at both ends, which is an error
  that integrates along-track and cancels nowhere.
- **SHDW-R-029.** Above the altitude at which `LI19`'s five atmospheric cases stop exhausting the
  geometry (§8), `SHDW-F-009` **refuses** rather than invent a sixth case, because inventing one
  is a change to the **model** and not to this implementation of it (plan §5 constraint 4). It is
  recorded as `SHDW-Q-003` for the manager, with the natural extension written out so the decision
  is a decision and not a rediscovery.
- **SHDW-R-021a.** The **polar radius reaches the arithmetic**. A PPM that silently ignored *b*
  would agree with the SECM to rounding and pass every other case here, so the oblateness effect
  is asserted strictly non-zero, and its **sign** is checked where it is unambiguous: over the
  pole an oblate Earth presents a shorter limb than a sphere of equatorial radius, blocks less,
  and must give a *higher* *F*ₛ.

---

## 5. Interfaces, stated language-free

`shadow_function(sun_position, satellite_position) -> Result<double>`, both `Position<GCRS>` in
metres, returning *F*ₛ. The occulting body's radius is a stated constant of the model, not an
argument, because a caller free to vary it is a caller free to make the model something else.

`perspective(sun_position, satellite_position, atmosphere) -> Result<{Fs, state, silhouette}>`,
both positions **Earth-fixed** (`SHDW-R-021`) and in metres. The ellipsoid's two radii are
constants for the same reason the sphere's one is. The silhouette's kind is returned because it is
not recoverable from *F*ₛ, and a run that never produced a hyperbola has not exercised half the
code.

The ellipsoid is nonetheless an **argument of an internal entry point**, and only so that the
degeneracy at *a* = *b* can be *demonstrated* rather than asserted: designed general, exercised
degenerate. The generality belongs to the test, not to the caller.

---

## 6. Precision

| id | what | value | arithmetic |
|---|---|---|---|
| `SHDW-P-1` | the umbra cone's apex distance | **1.3842 × 10⁶ km** | 6378.137 km × 1.495978707 × 10⁸ km × 1.450749 × 10⁻⁶ km⁻¹ = **1.3842 × 10⁶ km** |

- **SHDW-P-2.** The area-ratio integral is evaluated in closed form — two circular segments — and
  is exact to rounding. There is no quadrature and therefore no quadrature tolerance.
- **SHDW-P-3.** The PPM's area is likewise closed form; the only discretisation is the bracketing
  of at most four roots of a degree-two trigonometric polynomial, refined by bisection.
- **SHDW-P-4.** The **degeneracy test threshold is relative to the coefficients' own scale**.
  *k*₀, *k*₁ and *k*₂ are quadratic forms of *A* = diag(*a*⁻², *a*⁻², *b*⁻²), so they run at
  10⁻²⁷; a threshold anchored to 1 calls every geometry a parabola. It did: the first sweep
  reported "worst difference 0.000 × 10⁰ over 41 geometries" while comparing none of them, which
  is why the compared count is asserted wherever this is measured (`SHDW-A-008`, `-A-009`).
- **SHDW-P-5.** The comparator of `SHDW-A-008` is **exact in the radial direction** — each blocked
  interval's ends by bisection, the radial integral as cos *a*₁ − cos *a*₂ — and discretised only
  in azimuth. A cell count converges like 1/*n* and, measured, could not separate the two models
  at all; this one converged to ≈ 1 × 10⁻⁶ at 8 000 azimuths and can.

---

## 7. Failure behaviour

| id | when | what it names |
|---|---|---|
| `SHDW-F-001` | a zero-length Sun or satellite position | which vector |
| `SHDW-F-002` | a satellite inside the occulting body | the radius and the position |
| `SHDW-F-003` | a non-positive ellipsoid radius or image-plane distance | which one |
| `SHDW-F-004` | a satellite on or inside the Earth **ellipsoid**, where no silhouette exists | that there is none |
| `SHDW-F-005` | the silhouette is a **parabola**, which `LI19` §2.3 sets aside as "an instantaneous state in the variation from an ellipse to a hyperbola" | the paper's own words |
| `SHDW-F-006` | an odd number of disc/silhouette crossings | the count |
| `SHDW-F-007` | the top-of-atmosphere silhouette could not be formed | the inner diagnostic |
| `SHDW-F-008` | the depth into the atmosphere is not measurable along the line eq 40 defines it on | which line |
| `SHDW-F-009` | **the solar disc meets the solid Earth, the atmosphere and the clear sky at once** — see §8's note | the geometry, and that it is the model's gap and not this implementation's |

---

## 8. Acceptance tests

| id | what is checked | expected | discharges |
|---|---|---|---|
| `SHDW-A-001` | **the boundaries, in closed form**: *F*ₛ = 1 exactly outside the penumbral cone, 0 exactly inside the umbral cone, and strictly between on the boundary surfaces themselves | exact | R-010, R-012 |
| `SHDW-A-002` | **monotone across a transverse traversal**: sampling a straight path from sunlight to umbra, *F*ₛ never increases, and the count of samples is asserted | monotone over a stated number of samples | R-011 |
| `SHDW-A-003` | **the occulted-area ratio against the two-circle overlap**, computed independently: the lens area of two intersecting circles has a closed form, and *F*ₛ must equal 1 − (overlap / solar disc area) | agreement to rounding | R-001, P-2 |
| `SHDW-A-004` | **the penumbra's angular width against the Sun's own angular radius**: the transition spans twice the solar angular radius, which is a property of a disc Sun and would be **zero** for a point source — so this is the test that a point-source implementation fails | the solar angular diameter | R-001 |
| `SHDW-A-005` | **the annular branch, SYNTHETIC**: a geometry at 2 × 10⁶ km, beyond `SHDW-P-1`'s apex, where the Earth's disc is smaller than the Sun's. **No orbit in this plan reaches this** and the test name says so | 0 < *F*ₛ < 1 with the Earth wholly inside the solar disc | R-002 |
| `SHDW-A-006` | refusals `SHDW-F-001`, `-F-002`, fired **and shown not to fire** on the adjacent accepted input | the diagnostics | F-001, F-002 |

| `SHDW-A-007` | constraint 4 and constraint 8, via `ci.sh` gate 9 | as stated | R-013 |
| `SHDW-A-008` | **the PPM against the definition**, at nine penumbral geometries across LEO, GPS and GEO, with the **silhouette kind asserted** at each so that a run exercising only one branch cannot pass | worst \|PPM − **uniform-disc definition**\| < 1 × 10⁻⁵ (agreement, **not accuracy** — see §3.4), with 3 hyperbolic and 6 elliptical asserted | R-022, R-024, R-025, R-026 |
| `SHDW-A-009` | **§3.3's sixth choice, measured**: the SECM's and the PPM's departures from the definition and the oblateness effect, all at one reference point per orbit; the PPM nearer the definition than the SECM at every orbit; and the oblateness **strictly non-zero**, without which the case passes on a model that ignored *b* | ratio > 50 at LEO (measured 98.9) | R-023, R-021a |
| `SHDW-A-010` | **γ cancels**: *F*ₛ over four image-plane distances spanning 10⁹ | spread < 1 × 10⁻¹² | R-027 |
| `SHDW-A-011` | **oblateness has the right sign**: over the pole an oblate Earth presents a shorter limb than a sphere of equatorial radius, so it blocks less and *F*ₛ must come out higher | higher by more than 10⁻⁴ | R-021a |
| `SHDW-A-012` | **the atmosphere only ever dims and widens the penumbra**, over 81 geometries, with the strictly-dimmed count and both penumbral counts asserted | dimmer > 0 and penumbral samples strictly more with it than without | R-028 |
| `SHDW-A-014` | **the parabola refusal fired from the place it exists for**: the ellipse/hyperbola boundary is |cos ψ| = *R*ₑ/*r* for a sphere, so it is bisected onto rather than argued about, and both sides are required to resolve and to differ — a guard that fires on a neighbourhood is a wall, not a boundary | the boundary at 0.5156 rad = 29.5°, matching the closed form | F-005, R-022 |
| `SHDW-A-016` | **the seventh axis measured and ordered** (§3.4): over 31 penumbral geometries at LEO, the peak effect of the brightness profile, of where the ratio is taken, and of the Earth's figure, each against the same comparator; plus the comparator's own self-check that the linear law's disc mean is 1 − *u*/3, a closed form owing nothing to the ray casting | brightness > 10⁻², and each axis > 50× the next (measured 83× and 126×) | R-030, R-032, R-023a |
| `SHDW-A-015` | a non-positive ellipsoid radius or image-plane distance refused, **and shown not to fire** on the adjacent accepted input | the diagnostic | F-003 |
| `SHDW-A-013` | refusals `SHDW-F-004` and `-F-009` fired, the latter over a window **wide enough to leave the straddle on both sides** so that sunlight and umbra are both still resolved | the diagnostics, and refusals < all | F-004, F-009, R-029 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `SHDW-R-021` | Discharged by the **signature**, not by a run: the two models' position types do not overlap, so the mistake is a compile error. `perspective_tests.cpp` asserts it both ways with `static_assert` over a concept — the PPM accepts `ITRS` and rejects `GCRS`, `TIRS`, `TEME` and `BCRS`; the SECM accepts `GCRS` and rejects `ITRS` — because a concept false for every frame would satisfy the negative half alone. |
| `SHDW-F-006`, `SHDW-F-007`, `SHDW-F-008` | Defensive, and **not reachable from any geometry this tree can construct**: an odd crossing count contradicts the degree-two trigonometric polynomial of `SHDW-R-025`; `-F-007` needs the top-of-atmosphere silhouette to fail where the solid one did not, the two differing only by 50 km of radius; `-F-008` needs the Earth's centre to project nowhere while its limb projects somewhere. Each is recorded here rather than given a test that would have to fake its own precondition. |
| `SHDW-R-020` | A documentation obligation: `PROVENANCE.md` must record the retrieval route, the five family choices, the annular branch's domain, the Table 2 search with each route's result, and §3.3's measurement. Discharged by §9 and the entry a reviewer reads. |
| `SHDW-R-031`, `SHDW-R-033` | **Not asserted by a gated test.** Both rest on a one-off numerical study with an actual two-body Kepler propagator (`kepler_limb3.cpp`, scratch — a permanent gate would mean carrying orbital-mechanics machinery the module itself has no other use for, to characterise a feature `SHDW-Q-005` defers). Each figure was checked by refinement before being recorded here and in `PROVENANCE.md` §25.10 rather than being given a test that would have to reimplement the same propagator to check itself. |

### `LI19`'s five atmospheric cases do not exhaust the geometry above ≈ 1 983 km

`LI19` Fig. 8 gives five relative positions and §2.6 a formula for each. In each of them the solar
disc meets **at most two** of {solid Earth's image, atmosphere's image, clear sky}: eq 45 in
particular carries a term for the blocked area and one for the in-atmosphere area and **none for a
fully lit one**. That is complete exactly while the atmosphere's image is at least as thick as the
solar disc is wide,

> asin((*R*ₑ + 50 km)/*r*) − asin(*R*ₑ/*r*)  ≥  2 asin(*R*ₛ/*d*)  =  9.301 × 10⁻³ rad,

which holds to ***r*** **= 8 361 km (altitude 1 983 km)** and fails above it. GRACE, at 500 km, is
inside the valid region; **Galileo, at 29 600 km, is not**, and `LI19` validates against both.

**This is a gap in the printed specification, and it is stated as no more than that.** `LI19`
publishes working code, which may well close it; that code was not sought or read (§2), so no
claim is made about the paper's Galileo results. Plan §4 rule 4: the bar for the defect register
is higher than the bar for flagging, and what is asserted here is only what `SHDW-A-013` measures.

### What this gate can and cannot catch

**`LI19`'s Table 2 is not attempted, and the reason is an account rather than the geometry.** It
prints 16 eclipse events for GRACE-A on 2007-01-20 with penumbra entry/exit to the second — 30
transitions, and the transit durations (27–33 s, 21 % spread) show that **all 30 are transverse
crossings**, so all 30 would be reachable with an ephemeris good to about a kilometre. But every
route to one requires an account or a request form: CelesTrak has no current elements for a
satellite that deorbited in 2018 and archives behind `request.php`; GFZ ISDC, PODAAC and
space-track all require registration — five endpoints in all, since CelesTrak's current and
archived elements fail differently. The standing prohibition on contacting anybody covers all of
them, and `PROVENANCE.md` §25.5 tabulates each with what it returned.

**So this gate is closed-form geometry and not published times**, and what it therefore cannot
catch is a consistent misreading of `LI19`'s *geometry* that is nonetheless internally
self-consistent. `SHDW-A-004` is the strongest defence available: the penumbra's angular width is
a property of the Sun being a disc, and no point-source implementation can produce it.

---

## 9. Provenance obligations

- **SHDW-R-020.** `PROVENANCE.md` records the retrieval route, the five family choices, the
  annular branch's domain, the Table 2 search with what each of the five routes returned, §3.3's
  measurement of the sixth choice against oblateness, and the 36-vs-30 question's **resolution**
  — which is not a discrepancy and is recorded so that it is not re-entered as one.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `SHDW-Q-001` | **Table 2 costs an account, not a workaround.** The transit durations already cleared the geometry, so the only obstacle is registration at CelesTrak, GFZ, PODAAC or space-track. That is the owner's decision and not something to route around; recorded so it is not re-opened as a technical question. |
| `SHDW-Q-002` | **Closed.** The perspective-projection half is implemented, gated by `SHDW-A-008`…`-A-013`, and §2's rule-8 claim is re-derived with the one delegation `LI19` makes discharged independently. |
| `SHDW-Q-003` | **Ruled: refuse, and do not extend in this step.** There is no published case above 1 983 km to gate an extension against, precisely because `LI19`'s five atmospheric cases do not cover that geometry (§8) — so an extension built now would be **ungated**, which is worse than a refusal: a refusal tells a user at GNSS altitude the atmospheric variant is unavailable, where an ungated extension hands them a number with no published support. `SHDW-F-009` refuses there until further notice. The natural extension is written out so the decision is cheap to reverse when a consumer needs it, and it will then be this tree's own derivation with its own provenance and gate, not "from `LI19`": one term, *F*ₛ = [0·*A*ₛ + ½(*f*(*h*_G₁) + *f*(*h*_G₂))·*A*_atm + 1·*A*_clear]/π*R*₀², which reduces to all five printed cases. |
| `SHDW-Q-004` | **Closed, ruled not a register entry.** It is a property of two models neither of which claims to be the other, and no defect is established — the paper does not claim the SECM-to-PPM difference *is* the oblateness. It is recorded instead as §3.3's **sixth family choice**, with the three-radius table and the explicit caution, in the module headers as well as here (`SHDW-R-023a`). A register entry asserts a fault; naming an unnamed choice and costing it is a contribution. |
| `SHDW-Q-005` | **Ruled: defer, and — unlike `SHDW-Q-003` — not for gateability.** Published bolometric limb-darkening coefficients exist in quantity, so an implementation would not be ungated the way an extended `SHDW-F-009` would have been; the deferral is about **need**. `SHDW-R-031` is what makes the need legible: 99.9 % of the effect cancels across a whole passage on the geometry that dominates this plan, and nothing here yet samples *inside* one. The consumer that would change the answer — accelerometry or high-rate tracking within a passage, at L6/L7, or an L8 campaign — does not exist yet. When it does: implement against a published bolometric law with its own provenance and its own gate, with Eddington kept as the cross-check rather than the source, not the grey approximation used to measure the magnitude here. |
