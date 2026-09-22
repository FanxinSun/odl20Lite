# SPEC-drag — atmospheric drag

| | |
|---|---|
| **Spec ID** | `DRAG` |
| **Status** | **draft** 2026-09-22, for review; **amended the same day** on the manager's review of v1.0: `DYN-Q-001` and `DRAG-Q-002` ruled, `DRAG-R-004` corrected |
| **Version** | 1.1 |
| **Date** | 2026-09-22 |
| **Layer** | L4 `forces-analytic`, step 4 (`doc/REWRITE_PLAN.md` §3.5) |
| **Depends on** | `core`, `time`, `eop`, `frames`, `atmosphere`, `dynamics` |
| **Depended on by** | L4's own exit gate (after step 7, `doc/REWRITE_PLAN.md` §3.5 — an arc fit with parameters stated in the test); nothing later is built yet |

**Derivation declaration (plan R1).** Written from the documents in §2, from `SPEC-atmosphere.md`,
`SPEC-dynamics.md` and `SPEC-frames.md` (the three specs this module's interfaces are built
against), and from no implementation of this module beyond what this document itself describes.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

**Process note.** Unlike this tree's other L4 steps, `modules/drag`'s header, implementation
and test suite were written before this document, not after — the design reasoning §4 and §9
describe (the Jacobian derivations, the co-rotating-atmosphere identity, the provenance-carrying
discipline) happened alongside the code rather than as a separate prior checkpoint. This document
is the record plan §3.11 requires, written to describe exactly what was built and verified, not
adjusted after the fact to make either side agree. Flagged here rather than left implicit, for
the manager's own judgement of whether the sequence matters for this step.

---

## 1. Purpose and scope

The drag force over L2's atmosphere (`atmosphere::for_drag`, `SPEC-atmosphere`), with the drag
coefficient *C*_D consumed as a **registered parameter** (`dyn::ParameterKind::drag_coefficient`,
declared at L3 step 1, before this module existed) and never a compiled-in constant — the plan's
own words for this step, and the reason `DRAG-R-006` exists.

Two things this module inherits and must not merely read past: **Liouville**
(`SPEC-stm.md`'s `det Phi = exp(integral of tr A dt)`, written to survive exactly this arrival —
drag is the first force in this tree with `tr(da/dv) < 0`, so the determinant's decay becomes a
real, physics-set rate rather than a constant 1) and **atmosphere's own provenance**
(`atmosphere::EvaluationRecord`'s verification flag and snapshot identity, which stop meaning
anything the moment a consumer drops them on the way to its own result).

**Not in scope:**

- **Any specific satellite's ballistic coefficient, area or mass.** `acceleration` and `Drag`
  take *C*_D, area and mass as caller-supplied; no value is read from a library or a fitted
  campaign. Plan §5 constraint 4 (refuse rather than approximate) and rule 4's finding (§4.4)
  both bear on why no such value is built in.
- **The space-weather data source itself.** `atmosphere::SpaceWeather` is `SPEC-atmosphere`'s
  own concern; this module consumes whatever instance it is given and propagates its provenance,
  never constructs or samples one.
- **Lift, or any non-drag aerodynamic effect.** The force law (`DRAG-R-001`) is drag only, along
  −*v*_rel; there is no lateral or normal aerodynamic term.
- **The lateral atmospheric density gradient.** `DRAG-R-004`'s position Jacobian is radial only;
  the neglected latitude/longitude/local-time gradient is named, not silently assumed zero
  (§6, §9).
- **Everything `SPEC-atmosphere` already refuses to do below 80 km or outside its verified
  window** — inherited by reference (`DRAG-F-005` names the refusal; this spec does not
  re-derive `atmosphere`'s own boundary).

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `SENG14` | Sengers, J. V., Lin Wang, Y.-Y., Kamgar-Parsi, B., Dorfman, J. R. | *Kinetic Theory of Drag on Objects in Nearly Free Molecular Flow* | arXiv:1404.7826 [physics.flu-dyn], submitted 2014-06-28 | `https://arxiv.org/pdf/1404.7826` | **primary** (Table 4 only) | informative — a printed, physically-grounded **range** for the free-molecular sphere drag coefficient *C*₀(*S*), used as `DRAG-A-002`'s plausibility check on the test's own stated *C*_D; not a registered test value (§4.4) |
| `WGS84` | NGA | *Department of Defense World Geodetic System 1984*, NGA.STND.0036_1.0.0_WGS84 | 1.0.0 | as pinned by `SPEC-gravity` (manifest id `wgs84-standard`) | reused, not re-pinned | normative for `DRAG-R-009`'s two ellipsoid constants — the same *a* and *b* `SPEC-gravity` `GRAV-R-006` and `GRAV-A-012`/§9 already cite, Table 3.6 for *b* |

`SENG14` is pinned as a `literature`-kind manifest entry (`sengers-2014-drag-coefficient`). Its
terms search, unlike `LI19`'s and `RS14`'s, **found** a statement rather than finding nothing:
arXiv's abstract page names the standard non-exclusive distribution license, whose own text
grants only arXiv.org the right to distribute — no third-party redistribution right. The
exemption therefore rests on this tree not redistributing, the same footing as the other two
literature entries, now stated rather than inferred from absence. See the manifest entry for the
retrieval account in full; not repeated here.

`WGS84`'s two constants are **reused from `SPEC-gravity`'s own citation**, not independently
re-pinned: a second citation of the same numbers from a second spec is how the two drift apart
(§3 below, and the reasoning `SPEC-srp-analytic` §3 already applied to `SPEC-macromodel`'s
conventions).

The force law itself (`DRAG-R-001`) is **not cited to a textbook**. It is dynamic pressure
(½ρ*v*²) times a reference area times a dimensionless coefficient, along the incoming relative
wind — the same shape aerodynamic drag takes in every treatment this tree could name
(Montenbruck & Gill, Vallado, Vallado & McClain), and dimensional analysis alone fixes that
shape up to the one coefficient *C*_D exists to carry. Re-derived rather than cited, per plan
rule 8's converse: an independent specification (the closed form, checkable by momentum
bookkeeping) makes a single citation's exact wording not load-bearing.

---

## 3. Definitions and conventions

Inherits, entire, and redefines none of:

- `SPEC-frames` §3's frame conventions — `Frame::GCRS`, `Frame::ITRS`, and `core/units.hpp`'s
  named km/metre crossing (`DYN-R-011`'s discipline: `frames::State<F>::position()`/`velocity()`
  are kilometres, `frames::Position<F>`/`Acceleration<F>` are metres, and every crossing between
  them is a named call to `km_from_metres`/`metres_from_km`, not an implicit conversion).
- `SPEC-dynamics` §3's `Force`/`ForceEvaluation`/`StateJacobian`/`ParameterJacobian` surface,
  frozen before this module existed (`SPEC-dynamics` §1). This module supplies one
  implementation of it and does not reinterpret it — the one exception, `ForceEvaluation`'s
  additive `provenance` field (`DYN-R-051`), was proposed by this module's own need but is
  `SPEC-dynamics`'s own amendment, ruled under `DYN-Q-001`'s terms, not a private extension this
  spec grants itself.
- `SPEC-atmosphere` §3's `SpaceWeather`, `Place`, `DragDensity`, `EvaluationRecord`,
  `Verification` and `DataClass` — this module is a consumer, never a second definition.

**This module's own convention, stated once here:** *v*_rel, the velocity relative to the
atmosphere, is computed in the **ITRS** frame (`DRAG-R-002`) and the force law (`DRAG-R-001`) is
evaluated there; the result is rotated to **GCRS** before being returned, because
`dyn::ForceEvaluation` and `DragResult` both state their acceleration in GCRS
(`SPEC-dynamics` §3). A comparison against this module's own output — a test's independently
computed expected direction, for instance — must perform the same rotation or is comparing two
different frames wearing the same units (found and fixed in this module's own test suite,
`DRAG-A-002`; recorded here so the mistake is not repeated by a future caller).

---

## 4. Required behaviour

- **DRAG-R-001.** The force law:

  > **a** = −0.5 ρ *C*_D (*A*/*m*) **v**_rel |**v**_rel|

  ρ = `atmosphere::DragDensity::total_mass_kg_m3` (never `NeutralDensity`'s — `SPEC-atmosphere`
  ATMO-R-004's distinction, inherited: this module calls `for_drag`, never `neutral`); *C*_D,
  *A*, *m* stated by the caller; **v**_rel the satellite's velocity relative to a **rigidly
  co-rotating** atmosphere; direction exactly −**v**_rel/|**v**_rel|. Momentum flux times area
  times a dimensionless coefficient, along the incoming relative wind — see §2 for why this is
  re-derived rather than cited.
- **DRAG-R-002.** **v**_rel is obtained via the same GCRS↔ITRS chain `SPEC-frames` already
  built and verified (`frames::to_itrs`), not a hand-rolled ω×**r**. This is exact, not an
  approximation: a point rigidly fixed to the solid Earth has **zero ITRS velocity by
  construction**, which is exactly what "velocity relative to a co-rotating atmosphere" means,
  so the ITRS-frame velocity component of the satellite's transformed state **is** **v**_rel,
  with no further correction term. The geodetic point under the satellite, for
  `atmosphere::Place`, is obtained from the same ITRS position via `DRAG-R-009`.
- **DRAG-R-003.** The velocity Jacobian is **exact and analytic**, never finite-differenced.
  Density does not depend on velocity, so **a**'s entire velocity dependence is through
  **v**_rel|**v**_rel|, whose exact derivative is the tensor

  > ∂/∂*v*ⱼ(*vᵢ*|**v**|) = δᵢⱼ|**v**| + *vᵢvⱼ*/|**v**|

  formed in ITRS (where **v**_rel lives) and chained through the rotation to GCRS as
  *M*ᵀ · (∂**a**/∂**v**)_ITRS · *M*, *M* the GCRS→ITRS rotation `frames::gcrs_to_itrs` returns
  (velocity transforms at fixed position under a pure rotation this way; `SPEC-frames` §4.1's
  own composed chain). Verified by direct differentiation, not assumed (§9).
- **DRAG-R-004.** The position Jacobian is **two channels**, not one, and both are checked
  against a real finite difference of `acceleration()` rather than trusted from their closed
  forms (§8's own finding — v1.0 of this row described one channel and an untested tolerance,
  corrected below).

  **Channel 1, radial, via density's altitude dependence.** A locally exponential atmosphere,
  scale height *H* = −ρ/(dρ/d(altitude)), with dρ/d(altitude) estimated by a **central**
  difference of two extra `atmosphere::for_drag` calls at altitude ± `DRAG-P-1`'s registered
  half-step. The **lateral** (latitude/longitude/local-time) gradient is **neglected**, named as
  neglected rather than silently taken as zero (§9 records its rough size). If either bumped
  call refuses, this channel's contribution is zero — **a declared, visible degradation**
  (`DRAG-A-005`'s and `DRAG-A-010`'s own bumped calls are asserted to succeed at the altitudes
  those tests use).

  **Channel 2, the velocity-transport term.** **v**_rel = **v**_ITRS itself depends on
  **r**_ITRS, through the same transport term the GCRS↔ITRS state transform carries
  (**v**_ITRS = *M*·**v**_GCRS − **ω**×**r**_ITRS, at fixed epoch and **v**_GCRS) — a channel a
  radial-altitude-only bump cannot see at all. **Exact and free**: no extra atmosphere call,
  since ∂**a**/∂**v**_rel is already `DRAG-R-003`'s own tensor and ∂**v**_rel/∂**r**_ITRS =
  −[**ω**]ₓ (the cross-product matrix) is constant at fixed epoch, so this channel is
  ∂**a**/∂**v**_rel · (−[**ω**]ₓ) by the chain rule. **ω** is `frames::gcrs_to_itrs`'s own
  `omega_rad_s`, used as if already expressed in ITRS components though it is strictly in the
  intermediate (TIRS) frame the full state transform forms it in — the sub-arcsecond
  polar-motion misalignment this introduces was itself checked (§9) rather than assumed
  negligible, by isolating this channel from channel 1 (holding ρ fixed, differencing only
  **a**(**v**_ITRS(**r**)) through the exact, non-approximated `to_itrs` velocity output) and
  finding this formula matches to 6 significant figures at that resolution.
- **DRAG-R-005.** ∂**a**/∂*C*_D is **exact**: **a** is linear in *C*_D, so ∂**a**/∂*C*_D =
  **a**/*C*_D, computed directly from the already-evaluated acceleration rather than by a
  division that would blow up as *C*_D → 0 (a value that is never physically valid, but the
  computation does not need to pass near it to avoid the formula that would).
- **DRAG-R-006.** *C*_D is consumed as a **registered** `dyn::ParameterKind::drag_coefficient`
  parameter (declared at L3 step 1, before this module existed — confirmed present in the
  registry's enumeration by inspection, not assumed), never a compiled-in constant.
  `Drag::consumes()` declares exactly this one `ParameterId`; a `ParameterSet` that does not
  carry a value for it refuses (`DRAG-F-006`) rather than defaulting. The free function
  `acceleration()` takes *C*_D directly as a `double`, for a caller (such as this module's own
  Jacobian-construction code, or a finite-difference comparator) that already has a value and is
  not asking the parameter system for one; `Drag::accel()` is the only path that resolves it
  from a `ParameterSet`.
- **DRAG-R-007.** `atmosphere::for_drag`'s own `EvaluationRecord` — verification flag and
  snapshot identity — reaches **both** of this module's surfaces, unchanged, not summarised or
  re-derived: `DragResult::atmosphere_record` for a caller of `acceleration()` directly, and
  `dyn::ForceEvaluation::provenance` (mapped into `dyn::Provenance`'s narrower shape — source id,
  source hash, a verified/not-verified flag) for a caller through the `Force` plugin.
  **The plugin surface did not carry this in v1.0** — `ForceEvaluation` had no field for it, and
  a caller reaching drag only through the plugin (L7's estimator among them) would have lost the
  snapshot identity and verification flag entirely, exactly the "decorative field" failure this
  requirement exists to prevent, at exactly the boundary it was found to be missing from.
  Corrected by amending `SPEC-dynamics` `ForceEvaluation` additively (`DYN-R-051`, `DYN-Q-001`'s
  own terms for a closed-layer edit) rather than working around the gap in this module alone.
- **DRAG-R-011.** `require_verified` is reachable through **both** surfaces, not only the free
  function: a plain argument on `acceleration()` (`DRAG-F-001`, naming the same reason
  `SPEC-atmosphere` `ATMO-F-015` refuses a direct `sample()` call), and a **construction-time**
  option on `Drag` (default `false`, so every pre-existing construction call stays valid
  unchanged), so a consumer needing verified inputs sets it once rather than per call. **v1.0's
  `Drag` had no such option** — `accel()` called the free function at its fixed default, so
  `DRAG-F-001` was structurally unreachable through the plugin regardless of what a caller
  wanted (`DRAG-Q-001`, ruled: closed by adding the option, not by declaring the free function
  the only route to the strict check).
- **DRAG-R-008.** This module's own precision claim is bounded by, and never exceeds,
  `SPEC-atmosphere`'s class-A tolerance (`ATMO-P-1`: 7.6706 × 10⁻⁶ worst-case relative, on total
  mass density, over every comparison that can affect a drag calculation at all). No figure in
  this spec, its tests, or its `PROVENANCE.md` entry is quoted to finer precision than that
  figure permits, because the density this module multiplies by cannot be known to better than
  it.
- **DRAG-R-009.** `geodetic::itrs_to_geodetic`/`geodetic_to_itrs`: Bowring's iterative method
  (6 fixed iterations — converges in 2–3 for any altitude a satellite occupies), WGS84
  (`kWgs84SemiMajorM` = 6378137.0 m, `kWgs84SemiMinorM` = 6356752.3142 m, §2's citation), with a
  closed-form special case within 1 m of the polar axis (where longitude is undefined and
  Bowring's own starting value divides by approximately zero). A **module-local utility**, not
  general-purpose: `atmosphere::Place` is the only consumer today, and promoting it to `frames`
  awaits a second one (plan §5 constraint 7's argument, one level below a layer).
- **DRAG-R-010.** `detail::day_of_year`: the standard cumulative-month-day Gregorian
  calculation, with the century exception (divisible by 100 but not 400 ⇒ not a leap year)
  applied alongside the ordinary one (divisible by 4 ⇒ leap, unless the century exception
  overrides it). Exposed in `odl::drag::detail` — not anonymous, and not part of this module's
  public contract — specifically so `DRAG-A-009` can check the century exception directly:
  1900 and 2100 are both outside what an `Epoch`/`LeapTable` can represent in UTC in this tree
  (before 1972, or past any realistic table expiry), so the only way to exercise that branch at
  all is to call the calculation directly rather than route it through a real epoch.

---

## 5. Interfaces, stated language-free

- `acceleration(t: Epoch, r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, c_d: f64, area_m2: f64,
  mass_kg: f64, sw: atmosphere::SpaceWeather, eop: EopRecord, leaps: LeapTable,
  require_verified: bool = false) -> Result<DragResult, DragError>` — the core computation,
  taking *C*_D as a plain value (`DRAG-R-006`).
  - `DragResult { acceleration_m_s2: Vec3 [GCRS], atmosphere_record: atmosphere::EvaluationRecord }`.
  - `DragError { id: str, message: str, cause: Option<Diagnostic> }` — this module's own error
    type, not `dyn::DynError` (`SPEC-dynamics`'s frozen, shared one). `cause`, when present,
    carries the UNDERLYING module's own diagnostic as data, not only folded into `message`'s
    free text (plan §5 constraint 10) — `DRAG-F-003`'s own three nominal causes are the reason
    this exists (`DRAG-Q-002`).
- `Drag` implements `dyn::Force` (`SPEC-dynamics` §3): bound at construction to one
  `ParameterId` (the *C*_D this instance resolves from a `ParameterSet`), one area, one mass, one
  `SpaceWeather`, one `EopRecord`, one `LeapTable`, and one `require_verified: bool = false`
  (`DRAG-R-011`) — the first five the shape every other fixed-physical-constant force in this
  tree already takes, since `ForceEvaluation` (frozen before this module existed, now amended
  additively — `DYN-R-051`) had no room for them. `consumes()` returns exactly the one
  `ParameterId`. `accel()` resolves *C*_D, calls `acceleration()` internally with its own
  construction-time `require_verified`, maps the result's `atmosphere_record` into
  `dyn::Provenance` for `ForceEvaluation::provenance`, and additionally returns
  `DRAG-R-003`/`-R-004`/`-R-005`'s Jacobians.

---

## 6. Precision

- **DRAG-P-1.** The position Jacobian's altitude half-step (`DRAG-R-004`'s channel 1) is
  **0.1 km**, revised from an initial 1 km by a measurement, not a preference. A step sweep
  (1, 0.5, 0.25, 0.1, 0.05 km), run after `DRAG-A-010` first measured a central difference's
  error an order of magnitude above the textbook (Δ/*H*)²/6 prediction, found channel 1's true
  error **non-monotonic** across 1–0.25 km and only stably small at 0.1 km and below — the
  signature of `atmosphere`'s own fitted cubic-spline structure in altitude (`SPEC-atmosphere`
  §3.1), not smooth Taylor truncation, at the coarser scale. 0.1 km sits stably past that
  structure (measured error ≈ 1.2 × 10⁻⁶ relative at this test's own case) and well below the
  ~1 % the still-neglected lateral gradient already costs this Jacobian, so a smaller step would
  buy precision this approximation cannot use. `DRAG-A-010`'s own stability check — comparing
  channel 1 at the registered step against half of it — is what would catch a return to the
  non-monotonic regime, and is the property a fixed formula-based tolerance could not.
- **DRAG-P-2.** `SENG14` Table 4 gives the free-molecular sphere drag coefficient *C*₀(*S*) in
  the range 2.0–2.4 for the speed ratios *S* typical of atomic oxygen at LEO altitudes.
  `DRAG-A-002` asserts its own **stated** *C*_D (2.2) lies in [2.0, 2.5] as a plausibility check
  on the test's own input, not as a registered test value: `SENG14` characterises an idealised
  sphere in free-molecular flow in general, not this tree's — or any — specific satellite, so no
  single printed number is "the" answer a force-law test could assert equality against (§4.4's
  rule-4 finding).
- **DRAG-P-3.** `DRAG-R-003`'s velocity-Jacobian tensor is checked against a central finite
  difference at *h* = *C*_D × 10⁻⁴ (`DRAG-A-004`). Because **a** is exactly **linear** in *C*_D
  (density and **v**_rel do not depend on it), the comparator carries none of the step-size bias
  a nonlinear function's finite difference would — plan rule 7's concern does not apply to a
  linear check the way it applies to `SPEC-srp-analytic`'s tessellated-sphere convergence study,
  and no convergence-ratio form is asserted here for that reason, stated rather than left absent
  without comment.

---

## 7. Failure behaviour

| id | when | diagnostic must name | never instead |
|---|---|---|---|
| `DRAG-F-001` | `require_verified=true` and the space-weather sample's `verification` is not `verified_against_issuer` | the same reason `ATMO-F-015` refuses a direct `sample()` call | computing silently on an unverified sample |
| `DRAG-F-002` | mass or area not strictly positive (either, same id) | which one and its value | a silent zero-force or a divide producing `inf`/`NaN` |
| `DRAG-F-003` | the GCRS↔ITRS transform fails, **or** the epoch fails to render to a UTC calendar, **or** the GCRS↔ITRS rotation fails (three internal call sites, one id, matching `DRAG-F-002`'s own precedent for mass/area — ruled sufficient, `DRAG-Q-002`) | the underlying `frames`/`time` diagnostic's own message, **and** that same diagnostic carried structured as `DragError::cause` (not only in the message text — plan §5 constraint 10) | a garbage acceleration from an unchecked transform result |
| `DRAG-F-004` | the ITRS-frame relative speed is not strictly positive | that drag has no well-defined direction at zero relative velocity | a `NaN` direction from dividing by zero |
| `DRAG-F-005` | `atmosphere::for_drag` itself refuses (e.g. `ATMO-F-001`, negative geodetic altitude) | the atmosphere module's own refusal message | proceeding with a stale or default density |
| `DRAG-F-006` | (`Drag::accel` only) the given `ParameterSet` carries no value for this instance's *C*_D `ParameterId` | that the parameter is unset, naming the underlying `DYN-F-002`-shaped cause | defaulting *C*_D to any constant (`DRAG-R-006`'s entire point) |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `DRAG-A-001` | geodetic round trip (5 latitude/longitude/altitude cases spanning both hemispheres and near-polar) and the closed-form equator/pole cases | round trip matches to rounding; equator gives exactly (*a*,0,0), pole exactly (0,0,*b*) | `DRAG-R-009`, ellipsoid symmetry (independent of the iteration) | 1×10⁻¹² rad / 1×10⁻⁶ m | R-009 |
| `DRAG-A-002` | the force law's magnitude and direction against an independently assembled closed form (same atmosphere call, hand-built `Place`), at a stated LEO state, *C*_D/area/mass/space-weather all stated in the test; expected direction rotated ITRS→GCRS before comparison (§3's own convention) | magnitude and direction match to rounding; stated *C*_D lies in `SENG14`'s plausibility range | R-001, R-002; `DRAG-P-2` | 1×10⁻⁹ relative / 1×10⁻⁹ direction | R-001, R-002 |
| `DRAG-A-003` | drag opposes relative velocity (dot product strictly negative), checked in the ITRS frame the force is built from | negative | R-001's stated direction | exact sign | R-001 |
| `DRAG-A-004` | *C*_D is registered (`DYN`-shaped refusal fires when unset, `DRAG-F-006`) and its Jacobian column matches a central finite difference | refusal fires; analytic and FD agree | R-005, R-006; `DRAG-P-3` | 1×10⁻⁸ relative | R-005, R-006, F-006 |
| `DRAG-A-005` | **Liouville with real drag**: a `TwoBody` + `Drag` `ForceSet`, at a **low, dense** orbit (300 km altitude — the file's usual 7331 km test radius measured too thin an atmosphere to move the determinant meaningfully), `det(Phi)` by direct Gaussian elimination against `exp(integral tr A dt)`; the integral strictly negative (dissipative, unlike every conservative force this tree had before); the determinant below 1 − 10⁻⁵ (four orders of margin above the integrator's own 10⁻¹¹ tolerance); a longer propagation showing strictly more decay than a shorter one | the two routes to `det(Phi)` agree to 1×10⁻⁷ relative; integral < 0; det < 1 − 10⁻⁵; monotonic in propagation time | `SPEC-stm` `STM-A-005b`'s own generalised form; R-001, R-003 | as stated | R-001, R-003 |
| `DRAG-A-006` | atmosphere provenance reaches **both** surfaces, in both directions: a pre-2004 epoch carries `unverified_redistribution` in `DragResult` and a `require_verified=true` free-function call on it refuses `DRAG-F-001`, **and** `Drag::accel`'s `ForceEvaluation::provenance` carries the same unverified flag and source id; a post-2004 epoch carries `verified_against_issuer` on both surfaces and the free-function call does not refuse | as stated, both directions, both surfaces | R-007; `SPEC-atmosphere` `ATMO-R-031`/`ATMO-A-021`'s own pattern; `DYN-R-051` | exact | R-007, F-001, DYN-R-051 |
| `DRAG-A-007` | `require_verified` as a **construction-time** option of `Drag`, through the `Force` plugin: the default (`false`) does not refuse an unverified sample, matching the pre-existing behaviour; constructed with `require_verified=true`, refuses `DRAG-F-001` through `accel()` on the same unverified sample; constructed the same way on a verified sample, does not refuse — shown adjacent per this project's refusal-catalogue discipline | as stated, all three cases | R-011 (ruled, `DRAG-Q-001`) | — | R-011, F-001 |
| `DRAG-A-008` | the refusal catalogue, fired on genuine adjacent-valid inputs, not contrived ones, with `DRAG-F-003`'s structured `cause` checked where it fires: `DRAG-F-002` (negative mass, zero area); `DRAG-F-003` via the transform (an `EopRecord` with `subdaily_applied=false`, `cause.id == FRAME-F-003`); `DRAG-F-005` (a satellite below the WGS84 ellipsoid, reached via `geodetic_to_itrs`→`to_gcrs`, a real negative-altitude `Place`, `ATMO-F-001`'s own trigger); and the adjacent valid input does **not** refuse. `DRAG-F-003`'s other two nominal causes are demonstrated absent, not merely untried: the **rotation** cause is checked directly against `transform.cpp`'s own source (`to_itrs` calls `gcrs_to_itrs` internally with identical arguments before this module's own separate call is ever reached, so the second call cannot fail if the first succeeded); the **calendar** cause is checked by construction (a pre-1972 epoch, built by `Duration` arithmetic since `from_calendar` itself refuses to construct one) — refuses `DRAG-F-003`, but with `cause.id == TIME-F-002` (the transform's own UT1 need, which runs first), not a calendar-conversion id, demonstrating the shadowing directly. `DRAG-F-004` is **not** fired: its guard is exact-zero ITRS relative speed, which no honest orbital state reaches, recorded as an acknowledged absence (plan rule 4) | each names its id and, where structured, its cause; the valid case succeeds | F-002, F-003 (both causes), F-005; `SPEC-atmosphere` `ATMO-F-001`; `SPEC-frames` `FRAME-F-003`; `SPEC-time` `TIME-F-002` | exact (id and cause.id match) | F-002, F-003, F-005 |
| `DRAG-A-009` | `detail::day_of_year` against the Gregorian rule's three branches, called directly (1900 not leap, 2000 leap, 2100 not leap, 2004 ordinary-leap, plus ordinary-year and both leap/non-leap year-end boundaries), **and** the wiring end-to-end at a representable date (no spurious `ATMO-F-002`) | every case matches the hand-computed day-of-year exactly; the end-to-end call succeeds | R-010 | exact | R-010 |
| `DRAG-A-010` | the **full** position Jacobian (both channels) against a real finite difference of the full `acceleration()` call (a genuine radial GCRS perturbation, not a synthetic one), at the same 300 km orbit `DRAG-A-005` uses, **plus a stability check**: channel 1 alone, recomputed independently at the registered 0.1 km step and at half of it, must closely agree — the property whose *absence* this row's own review caught at the coarser 1–0.25 km steps (`DRAG-P-1`). **Found two real defects across two review rounds, not a tolerance question**: (1) the first draft's `a_direction` omitted a factor of `\|`**v**_rel`\|` (the CO-ROTATING-frame relative speed, ~7.24 km/s at this case, not the ~7.73 km/s inertial orbital speed), a ~7330× error; (2) the first fix's own tolerance was set from a passing forward-difference residual (rule 7's defect), which review caught by a falsifiable step-halving prediction and which led to the channel-1/channel-2 split and the step-size finding above. Both recorded in full in `PROVENANCE.md` §28.4 | relative deviation < 1×10⁻⁴ (measured 1.2×10⁻⁶); stability between the two internal steps < 1×10⁻³ (measured ≈0); same-sign | R-004 (both channels); a real finite difference of R-001 | as stated | R-004 |

**Coverage.** Every requirement above is discharged by a row, except:

| id | why no test |
|---|---|
| `DRAG-R-008` | A precision-inheritance *statement*, not a computation: discharged by this document's own §6 and its `PROVENANCE.md` entry stating the bound explicitly, the same way `SPEC-srp-analytic`'s relocation obligation (`SRPA-R-007`) is discharged by documentation rather than a test. |
| `DRAG-F-004` | Its guard is exact-zero ITRS-frame relative speed. No honest orbital state reaches exactly zero: a real near-geostationary satellite lands at a small but nonzero residual, which is the *correct* non-refusing answer this force law should give there, not a gap in the check. `DRAG-A-008` records this as an acknowledged absence (plan §4 rule 4) rather than firing the refusal on a contrived, non-physical input just to close the row. |

---

## 9. Provenance obligations

`PROVENANCE.md` records, as a new numbered section:

- The plan-rule-4 search and its finding: no clean, force-level "published ballistic coefficient"
  case exists (the plan's own hint that it likely would not); `SENG14`'s Table 4 is the genuine
  published drag-*model* case found instead, used as a plausibility range (`DRAG-P-2`) rather
  than a registered test value, and why (§4.4).
- `DRAG-R-003`'s velocity-Jacobian tensor, verified by **direct differentiation** of
  **v**|**v**| component-by-component, not assumed from its stated form.
- `DRAG-R-004`'s position-Jacobian scale-height approximation, named as an approximation, with
  the neglected lateral-gradient term's rough size at least estimated (not left as an
  unquantified caveat) — a local-exponential radial partial is standard practice in real orbit
  determination software (GEODYN, Bernese-type tools use the same shape), and the lateral term
  it omits is smaller by roughly the ratio of the satellite's angular speed relative to the
  atmosphere's structure to its radial fall-off rate; the exact bound belongs in this entry, not
  asserted here without the arithmetic behind it.
- `DRAG-R-008`'s density-tolerance inheritance statement, so a later reader sees explicitly why
  no drag figure is more precise than 7.6706 × 10⁻⁶ relative.
- The atmosphere-provenance-propagation mechanism (`DRAG-R-007`) and why it matters: dropped at
  this module's boundary, `SPEC-atmosphere`'s verification flag becomes exactly the decorative
  field its own design was built not to be.
- `DRAG-A-005`'s own history: the first attempt, at this tree's usual 7331 km test radius,
  measured a determinant that moved by only ~10⁻⁷ — too thin an atmosphere to be a meaningful
  Liouville check — and the test was rebuilt at 300 km rather than the threshold loosened to fit
  the weak result, the same discipline `SPEC-srp-analytic`'s box-wing gap and this session's
  earlier steps have applied throughout.
- `DRAG-A-010`'s full history across two review rounds: the `\|`**v**_rel`\|` defect (found by
  comparing the analytic Jacobian against a real finite difference rather than trusting the
  closed form); the FIRST fix's own tolerance found wanting (set from a passing forward-difference
  residual, rule 7's defect in its plainest form — a threshold chosen from the result it judges);
  the falsifiable step-halving prediction that caught it; the step sweep (1, 0.5, 0.25, 0.1,
  0.05 km) that found channel 1's error non-monotonic across the coarser steps and traced it to
  `atmosphere`'s own fitted cubic-spline structure, not smooth truncation; and the exact,
  independently-verified channel-2 formula (matched to 6 figures against a rho-held-fixed
  isolation) that the first "fix" omitted entirely. The two candidate explanations offered for
  the residual before the true cause was found (the neglected lateral gradient; the channel-2
  omega approximation) are recorded as REJECTED, with the reasoning that ruled each out, not
  quietly dropped once the real cause was found.
- The `ForceEvaluation` provenance-boundary gap (`DRAG-R-007`, `DYN-R-051`) as its own finding,
  distinct from the mechanism it fixes: v1.0 built the free function's `DragResult` carefully but
  left the `Force` plugin surface — the ONLY surface L7's estimator will ever see — with no way
  to carry the same information, an omission a trivial-force-gated interface (`SPEC-dynamics`
  §1) could not have revealed and only a real, provenance-bearing force could.
- `DRAG-Q-002`'s resolution: `DRAG-F-003`'s three nominal causes reduce, in practice, to ONE
  independently reachable trigger (the transform's own `EopRecord.subdaily_applied` check) and
  TWO that are structurally shadowed by that same earlier, stronger check — the rotation cause
  by `to_itrs`'s own internal delegation to `gcrs_to_itrs` with identical arguments, and the
  calendar cause because `to_itrs` needs UT1, which needs the same UTC rendering
  `.calendar(UTC, leaps)` performs, and runs first. Both demonstrated, not merely argued
  (`DRAG-A-008`).

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `DRAG-Q-001` | **RULED: give `Drag` a construction-time flag.** `require_verified` is now a `Drag` constructor parameter (default `false`, every existing call site unchanged), and `accel()` passes it to the free function it wraps. `DRAG-R-011`, `DRAG-A-007`. Closed together with the `ForceEvaluation` provenance gap this same review found (`DRAG-R-007`, `DYN-R-051`): the construction-time flag alone would have let a caller REQUEST the refusal without giving that caller anywhere to read the verification flag on the SUCCEEDING path, which is the more common case. |
| `DRAG-Q-002` | **RULED: one id, with the underlying cause carried structured.** `DragError` gained a `cause: Option<Diagnostic>` field (plan §5 constraint 10), populated at all three `DRAG-F-003` sites. Investigating the three causes to test them found two are structurally shadowed by the transform call's own stronger precondition, not merely hard to trigger (§9, `DRAG-A-008`) — a finding the ruling's own "fire each cause or record its absence" condition surfaced, not merely a formatting change. |
