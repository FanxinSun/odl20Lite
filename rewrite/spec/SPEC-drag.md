# SPEC-drag — atmospheric drag

| | |
|---|---|
| **Spec ID** | `DRAG` |
| **Status** | **draft** 2026-09-22, for review |
| **Version** | 1.0 |
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
  implementation of it; it does not extend or reinterpret it.
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
- **DRAG-R-004.** The position Jacobian's **radial** part is a **named approximation**: a
  locally exponential atmosphere, scale height *H* = −ρ/(dρ/d(altitude)), with dρ/d(altitude)
  itself estimated by **one** extra `atmosphere::for_drag` call at altitude + 1 km
  (`DRAG-P-1`'s pre-registered step size). The **lateral** (latitude/longitude/local-time)
  gradient is **neglected**, named as neglected rather than silently taken as zero (§9 records
  its rough size). If the bumped-altitude call itself refuses, the position Jacobian's block is
  zero — **a declared, visible degradation** (`DRAG-A-005`'s own un-bumped call is asserted to
  succeed at the altitude that test uses, so this fallback is not silently exercised there).
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
  snapshot identity — is carried into `DragResult::atmosphere_record` **unchanged**, not
  summarised or re-derived. `require_verified` (default `false`) refuses (`DRAG-F-001`, naming
  the same reason `SPEC-atmosphere` `ATMO-F-015` refuses a direct `sample()` call) rather than
  computing silently, when set and the sample's `verification` is not
  `verified_against_issuer`. This makes `SPEC-atmosphere`'s provenance mechanism reachable
  *through* a force, not only through a direct call to `atmosphere::for_drag` — the property
  that keeps it from becoming, in the manager's words, "the decorative field it was designed not
  to be."
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
  require_verified: bool = false) -> Result<DragResult, Diagnostic>` — the core computation,
  taking *C*_D as a plain value (`DRAG-R-006`).
  - `DragResult { acceleration_m_s2: Vec3 [GCRS], atmosphere_record: atmosphere::EvaluationRecord }`.
- `Drag` implements `dyn::Force` (`SPEC-dynamics` §3): bound at construction to one
  `ParameterId` (the *C*_D this instance resolves from a `ParameterSet`), one area, one mass, one
  `SpaceWeather`, one `EopRecord`, one `LeapTable` — the shape every other fixed-physical-constant
  force in this tree already takes, since `ForceEvaluation` (frozen before this module existed)
  has no room for them. `consumes()` returns exactly the one `ParameterId`. `accel()` resolves
  *C*_D, calls `acceleration()` internally (with `require_verified` at its default — `DRAG-Q-001`
  records the open question this leaves), and additionally returns `DRAG-R-003`/`-R-004`/`-R-005`'s
  Jacobians.

---

## 6. Precision

- **DRAG-P-1.** The position Jacobian's altitude step (`DRAG-R-004`) is **1 km**,
  pre-registered before `DRAG-A-004`/`DRAG-A-005` were run, not fitted to make either pass —
  small relative to the scale heights this tree's LEO test cases occupy (tens of km), and large
  relative to the metre-level rounding `atmosphere::for_drag`'s own arithmetic carries.
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
| `DRAG-F-003` | the GCRS↔ITRS transform or rotation fails, **or** the epoch fails to render to a UTC calendar (three internal call sites, one id — the attached message names which; `DRAG-Q-002` records whether this should split) | the underlying `frames`/`time` diagnostic's own message | a garbage acceleration from an unchecked transform result |
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
| `DRAG-A-006` | atmosphere provenance reaches `DragResult` in both directions: a pre-2004 epoch carries `unverified_redistribution` and a `require_verified=true` call on it refuses `DRAG-F-001`; a post-2004 epoch carries `verified_against_issuer` and the same call does not refuse | as stated, both directions | R-007; `SPEC-atmosphere` `ATMO-R-031`/`ATMO-A-021`'s own pattern | exact | R-007, F-001 |
| `DRAG-A-007` | `require_verified` through the `Force` plugin: `Drag::accel` calls the free function at its default (`false`), so an unverified sample does **not** refuse through this path — the boundary stated precisely rather than left to be discovered (`DRAG-Q-001`) | succeeds (does not refuse) | R-007's stated default | — | documents a boundary, discharges nothing new |
| `DRAG-A-008` | the refusal catalogue, fired on genuine adjacent-valid inputs, not contrived ones: `DRAG-F-002` (negative mass, zero area); `DRAG-F-003` (an `EopRecord` with `subdaily_applied=false`, `FRAME-F-003`'s own trigger); `DRAG-F-005` (a satellite below the WGS84 ellipsoid, reached via `geodetic_to_itrs`→`to_gcrs`, a real negative-altitude `Place`, `ATMO-F-001`'s own trigger); and the adjacent valid input does **not** refuse. `DRAG-F-004` is **not** fired: its guard is exact-zero ITRS relative speed, which no honest orbital state reaches (a physical near-geostationary state lands at a small but nonzero residual — the *correct* non-refusing answer, not a gap), recorded as an acknowledged absence (plan rule 4) rather than a contrived pass | each names its id; the valid case succeeds | F-002, F-003, F-005; `SPEC-atmosphere` `ATMO-F-001`; `SPEC-frames` `FRAME-F-003` | exact (id match) | F-002, F-003, F-005 |
| `DRAG-A-009` | `detail::day_of_year` against the Gregorian rule's three branches, called directly (1900 not leap, 2000 leap, 2100 not leap, 2004 ordinary-leap, plus ordinary-year and both leap/non-leap year-end boundaries), **and** the wiring end-to-end at a representable date (no spurious `ATMO-F-002`) | every case matches the hand-computed day-of-year exactly; the end-to-end call succeeds | R-010 | exact | R-010 |
| `DRAG-A-010` | the position Jacobian's radial part against a real finite difference of the full `acceleration()` call (a genuine radial GCRS perturbation, not a synthetic one), at the same 300 km orbit `DRAG-A-005` uses; **found a real defect before this row could pass**: the first draft's `a_direction` omitted a factor of `\|`**v**_rel`\|` (~7.7 km/s at this case), a ~7300× error caught by comparing against the finite difference rather than trusting the closed form — fixed in `drag.cpp`, recorded in `PROVENANCE.md` | relative deviation 1.18×10⁻² after the fix (was 1.0, i.e. no agreement, before it); same-sign | R-004; a real finite difference of R-001 | 5×10⁻² relative, dot product > 0 | R-004 |

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

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `DRAG-Q-001` | `Drag::accel` calls `acceleration()` at `require_verified`'s default (`false`), so `DRAG-F-001` is **not** reachable through the `Force` plugin surface today (`DRAG-A-007` documents this precisely rather than leaving it to be discovered). Should `Drag` gain a variant, or a constructor flag, that requests the refusal through this path too — matching `SPEC-atmosphere`'s own point that the mechanism must be reachable "through the force," which today it only half is — or is direct use of the free function the intended route for a caller that needs the strict check? |
| `DRAG-Q-002` | `DRAG-F-003` covers three internally distinct causes (GCRS↔ITRS transform failure, epoch-to-UTC-calendar failure, GCRS↔ITRS rotation failure) under one diagnostic id, distinguished only by the attached free-text message — the same shape `DRAG-F-002` already takes for mass/area. Is one id sufficient here (matching precedent), or does a caller need to distinguish these three programmatically, which would mean splitting the id? |
