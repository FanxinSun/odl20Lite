# SPEC-forcemodel — L7 step 1: L2's models as `dyn::Force` plugins

| | |
|---|---|
| **Spec ID** | `FMOD` |
| **Status** | **draft v0.1a** 2026-10-06 — written, with **every check's tolerance, case list and frozen sizing, before any line of the module exists** (the manager's ruling R1 – R5 on step 1, `plan/subplan_L7/L7-1.md`; plan §4 rules 3 and 7) |
| **Version** | 0.1a |
| **Date** | 2026-10-06 |
| **Layer** | L7 `estimation`, step 1 (`../plan/PLAN.md` §3.8); the module is `forcemodel`, **apart from the estimator** (ruling R1) |
| **Depends on** | `SPEC-dynamics` (the `Force` surface), `SPEC-gravity` (v1.4's gradient and views), `SPEC-perturbations` (tides, third bodies, relativity), `SPEC-ephemerides`, `SPEC-eop`, `SPEC-frames`, `SPEC-time`, `SPEC-measmod` §6.2 (the finite-difference form) |
| **Depended on by** | steps 2 – 5 of L7 and the exit gate's fits, which see these models only as `dyn::Force` |

**Derivation declaration (plan R1).** Written from the documents listed in §2 and from no implementation of this module (none exists). **Predecessor access:** none of the predecessor's `src/`, `include/`, `res/`, `scripts/` or `analysis/` was opened, and `oracle/capture.sh` was not; searches are scoped to `rewrite/`. The one exposure of single predecessor lines of 2026-10-06 is registered at `PROVENANCE.md` §0.2 and was accepted as disclosed.

---

## 1. Purpose and scope

L3 froze the plugin surface **after L2 existed**, and only L4's five forces were ever written as plugins: L2's models are evaluated directly (`tests/l2_floors.cpp`, `tests/l4_ranking.cpp`) and no step wrapped them. Every fit in L7 propagates the whole model, so the wrapping comes before the estimator.

**In scope.** Four families of `dyn::Force`: **`Gravity`** (the conventional EGM2008 field to a stated degree and order), **`ThirdBody`** (one plugin per body; the planets are the ephemeris's *barycentres*, with the GM `gm_de440.tpc` pins for them), **`Relativity`** (one plugin per term of `TN36-10` (10.12)) and **`Tides`** (one plugin: the increments' sum is formed once and applied once, `PERT-R-061`); the **`EarthOrientation`** helper that gives the ITRS plugins their rotation per call; the **truncation criterion** of ruling R4, as a function and as a table at L4's four reference points; and the **finite-difference gate** of the plugins' state Jacobians (ruling R5), its sizing frozen in §6.2.

**Not in scope.** The ephemerides, EOP, time and frames (infrastructure, not forces); the atmosphere model and the shadow function (L4's, consumed by L4's plugins); L4's own forces; any registered parameter (**no plugin of this step consumes one**: the SRP scale and the empirical accelerations belong to the plugins that already carry them); multi-day arcs' Earth-orientation handling beyond stating it (carried, finding F4).

## 2. Normative sources

| key | issuer | title / what it fixes here | obtained |
|---|---|---|---|
| `DYN` | this tree | `SPEC-dynamics`: the `Force` surface, `DYN-R-010`/`-011` (the crossing), `-021`/`-022` (the three outputs, the two blocks), `-027` (declaring absent velocity dependence with a bound), `-023` (the registry sums and attributes), `-051` (provenance) | primary |
| `GRAV` | this tree | `SPEC-gravity` v1.4: `gradient()`, `CoefficientView`, `acceleration_of`/`gradient_of`, `GRAV-F-008`'s consumer | primary |
| `PERT` | this tree | `SPEC-perturbations`: `PERT-R-050` – `-053` (third bodies), `-060` – `-062` (composition; the increments' sum once), `PERT-F-013` (the Sun and Moon cannot be omitted), (10.12) | primary |
| `MEAS62` | this tree | `SPEC-measmod` §6.2: the finite-difference gate's form — central differences, `ε(h) = h² F/6 + ν/h`, criteria (b) and (c), wrong rows that must fail | primary |
| `FRAME` | this tree | `SPEC-frames`: `gcrs_to_itrs`; `FRAME-A-005`'s 5 µas bound on the chain against `eraC2t06a` | primary |
| `ERFA` | SOFA / liberfa | `eraC2t06a`: the **independent composition** the wrapper's rotation is checked against, with dX = dY = 0 | pinned (`erfa`, BSD) |
| `TN36` | IERS | chapters 5, 6, 10 — through `FRAME`, `GRAV`, `PERT` | pinned |
| `WGS84` | NGA | normal gravity at the pole, 9.832 184 9379 m s⁻² (`GRAV-A-012`'s anchor) | pinned (via `GRAV`) |
| `L4RANK` | this tree | `tests/l4_ranking.cpp` after ruling R3: L4's four reference points and the per-term relativity bands | primary |

## 3. Definitions and conventions

### 3.1 The boundary, and the three things a wrapper adds

A plugin receives a GCRS position in **metres**, a velocity in m s⁻¹ and an `Epoch` (`DYN-R-011`) and returns a GCRS acceleration in m s⁻² with its Jacobians. L2's models do not all live there, so a wrapper adds **at most three things, and each is checked independently of the wrapper** (§8):

1. **A frame rotation** (`Gravity`, `Tides`): the field is fixed in the ITRS. The plugin forms `m = gcrs_to_itrs(t, record, leaps).m` (GCRS → ITRS, the whole chain) and returns `a_GCRS = mᵀ · a_ITRS(m · r_GCRS)`; the Jacobian is `∂a/∂r = mᵀ G m`, `G` the field's tensor at `m · r` (`GRAV-R-060`). No Coriolis term arises: the force model returns a force-model acceleration in an inertial frame, and the rotation depends on time only, so `∂a/∂v` is **exactly zero** and is declared (`StateJacobian::no_velocity_dependence(·, 0.0)`).
2. **A unit crossing at an L2 interface** (`Relativity`): `Correction::by_term` takes a typed `State` in **kilometres**. The plugin crosses by name (`odl::km_from_metres`, `core/units.hpp`), once per call, for the satellite's position and velocity; the Earth about the Sun is built from the typed `State<GCRS>` of the Sun (`geocentric_state`, km) negated — no crossing — as `tests/l4_ranking.cpp` does after ruling R3. The `ThirdBody` plugin needs the body's position in metres for its Jacobian and crosses by name once, at the ephemeris boundary, as `Attraction` does.
3. **A time-dependent model state** (`Gravity`, `Tides`): the conventional field at the epoch (`GravityModel::conventional(t, extrapolate)`), the Earth orientation at the epoch, the Sun and Moon at the epoch, the tide arguments.

### 3.2 Names, and what a result says about where it came from

`ForceId::name`: **`gravity`**, **`third_body.<body>`** (`<body>` the lower-case body name, e.g. `third_body.sun`, `third_body.jupiter`), **`relativity.schwarzschild`**, **`relativity.lense_thirring`**, **`relativity.de_sitter`**, **`tides`**. One `ForceSet` holds any selection of them (the identities are unique, `DYN-F-005`).

**Provenance** (`DYN-R-051`, finding F5). `dyn::Provenance::source_id` is a **semicolon-separated list of `key=value` pairs, the first pair `forcemodel=<plugin>`**, in the order this section lists, e.g.
`forcemodel=gravity;model=EGM2008_to2190_TideFree;system=zero-tide;N=36;M=36;secular_pole=extrapolated;orientation=series` and for tides `forcemodel=tides;orientation=series;system=zero-tide;models=<each of the increments' ModelRecords as `model (source; parameters)`, joined by ' | '>` — **`PERT-R-052` and `PERT-R-062` require the body list and the applied models "recorded with every result"**, and this is where. `source_sha256` is the **comma-separated** list of `<manifest id>=<sha256>` pairs the **caller** supplies for the tables the plugin reads, in the order the plugin names them (gravity: `egm2008-coefficients`; third body: `naif-gm-de440`, `de440s-spk`; relativity: `de440s-spk`; tides: `egm2008-coefficients`, `de440s-spk`, `fes2004-ocean-tide`, `desai-ocean-pole-tide`) (the plugin cannot know a file's manifest hash; it records what it was told and says `unsupplied` for a table it was told nothing of). **`verified_against_issuer` is `false`, always, for these plugins, and that is the specification's statement and not an omission:** the flag means *checked against the issuing authority's own record* (`SPEC-atmosphere` `ATMO-R-031`: a space-weather snapshot against the issuer's index); a table pinned by the manifest's hash is known to be **the byte-identical file this tree pinned**, which says nothing about whether the issuer still serves or endorses it, so setting the flag from a hash would let a hash stand in for a verification.

### 3.3 Earth orientation — finding F4

The plugins that rotate take an **`EarthOrientation`**, which is either a **series** (`EopSeries` with an `EopPolicy`, queried **per call**: polar motion, UT1 and the CIP offsets follow the epoch across an arc) or a **fixed record** (one `EopRecord`, the way L4's `Drag` holds one). **Each plugin states which it was built with** (`describe_orientation()`, and the `orientation=` key of its provenance). The size of the difference between the two over a 24-hour arc is measured and printed by `FMOD-A-030`, for the exit gate's fits to quote; multi-day arcs are carried.

### 3.4 The truncation criterion (ruling R4)

For the static field truncated at degree *N* at radius *r*, **the lowest *N* ≥ 2 at which `truncation_rms(r, N) ≤ F_min(r)`**, where `truncation_rms` is `GRAV-R-041`'s RMS over the sphere of the discarded acceleration (one instrument with `PERT-R-022a`), and **`F_min(r)` is the smallest non-zero contribution, through the registry at that point and in the same run, of the registered forces other than the truncated series** — `Gravity` itself is excluded; every other plugin in the set contributes its own acceleration's norm, and the one that sets the minimum is named. A comparator measured in the same run from a family the field's degree cannot move, not a number anyone chose (`PERT-R-022a`'s trap: a threshold that is "the smallest per-degree term kept" chases itself). **`F_min` depends on what is registered, so every row names the forces it compared against.** Two registered sets are tabulated: **set A** — the Sun, the Moon, `tides` and the three relativity terms (the model the plan's L4 ranking table described, which the manager's predictions of ruling R4 refer to) — and **set B** — set A and the five planets whose peak tidal acceleration at LEO exceeds 10⁻¹³ m s⁻² (Mercury 7 × 10⁻¹³, Venus 6 × 10⁻¹¹, Mars 4 × 10⁻¹², Jupiter 7 × 10⁻¹², Saturn 3 × 10⁻¹³; Uranus and Neptune are below 5 × 10⁻¹⁵). The **maximum along the test orbit is reported beside the RMS**, so that the factor between them is visible: the discarded acceleration `‖a₂₁₉₀ − a_N‖` over 96 points of one revolution of a circular orbit of inclination 55° at the GPS point and 98° at the three low points, uniform in argument of latitude, with the Earth rotating beneath it (ascending node at longitude 0° at the start); the ITRS latitude and longitude of each point are closed-form (§8, `FMOD-A-021`).

## 4. Required behaviour

- **FMOD-R-001.** The plugins live in the module `forcemodel`, which depends on `gravity`, `tides`, `thirdbody`, `relativity`, `ephemerides`, `eop`, `frames`, `time`, `dynamics` and `core`; the estimator of steps 2 – 5 depends only on `dynamics`' `Force` interface and sees none of them (ruling R1).
- **FMOD-R-002.** Names and provenance are as §3.2 states. No plugin consumes a registered parameter: `consumes()` is empty and the `ParameterJacobian` it returns has the registry's width, its columns never read.
- **FMOD-R-003.** **`Gravity`** evaluates the conventional field **at the call's epoch** to the (*N*, *M*) given at construction (typed `gravity::Degree`/`Order`), with the secular-pole override recorded when used, **in the ITRS** by `FMOD-R-010`'s rotation, and refuses what the field refuses (`GRAV-F-004`, `-006`, `-001`), unchanged and never as a zero.
- **FMOD-R-004.** **`ThirdBody`**, one per body: its acceleration is `thirdbody::Attraction::pair_stable` **for that one body** — the function `by_body` itself applies to each body of its list, so the L2 module's value *is* the plugin's value (in the rearranged form of `PERT-R-051`), direct minus indirect (`PERT-R-050`). *[Amended 2026-10-06, before any run; the first text, kept: "is `thirdbody::Attraction::by_body` for that one body". Found by the first smoke call: `by_body` refuses any list that lacks the Sun or the Moon (`PERT-F-013`, at the call), and a plugin is one body; the rule at the level of a registry is the factory's (`FMOD-R-005`), and `FMOD-A-004` holds the plugin to `by_body` of {Sun, Moon, the body}, 0 ulp.]* its `∂a/∂r` is the **tidal tensor in closed form** `μ [ 3 d dᵀ/|d|⁵ − I/|d|³ ]`, `d = s − r` in GCRS metres (`s` the body's geocentric position; the indirect term does not depend on `r`); `∂a/∂v` is absent with bound 0.
- **FMOD-R-005.** `forcemodel::third_bodies(list, …)` builds the plugins for a stated body list and **refuses a list without the Sun or without the Moon** (`PERT-F-013`), recording the list (`PERT-R-052`); the planets may be omitted and what is given up is stated in §6.
- **FMOD-R-006.** **`Relativity`**, one per term (`PERT-R-042`): its acceleration is `Correction::by_term(sat, earth_about_sun, Terms::only(term), ppn)` with the crossing of §3.1; its Jacobians are the closed forms of §6.3, **with velocity dependence declared as a block** (`with_velocity`) for every term (the Schwarzschild term is quadratic in the velocity, the Lense–Thirring and de Sitter terms linear).
- **FMOD-R-007.** **`Tides`**, exactly **one** plugin per force set: it forms the increments of the configured models **at the call's epoch** — the solid Earth tide (`TN36-6` §6.2, three steps, in the zero-tide system the conventional field is in, `GRAV-R-021`), the ocean tide (FES2004, to a stated degree), the solid Earth pole tide and the ocean pole tide (to a stated degree) — **sums them once** (`TideIncrements::sum`, which refuses a system mismatch) and **applies the sum once** through the field's `acceleration_of` / `gradient_of` (`GRAV-R-063`) (`PERT-R-061`); it records every model's `ModelRecord` and parameters in its provenance (`PERT-R-062`); and it offers **`by_model(t, r, v)`**, outside the `Force` interface, returning each model's acceleration separately for the ranking and the truncation table.
- **FMOD-R-008.** `EarthOrientation` is a series (per call) or a fixed record; the plugin that rotates uses `frames::gcrs_to_itrs`, so a record with the sub-daily terms not applied is refused (`FRAME-F-003`) unchanged; the plugin states which it holds (`describe_orientation()`).
- **FMOD-R-009.** Every refusal of a model, the ephemeris, the orientation or the frames is passed through **with its own identifier and message**; a plugin never returns a zero, the previous value or a default in its place.
- **FMOD-R-010.** A plugin is immutable after construction and holds no global state (`DYN-R-025`): two plugins built from different models or at different degrees answer independently and interleaved.
- **FMOD-R-011.** The truncation criterion is a **function** — `degree_meeting_criterion(field, radius, f_min)` — and the set's comparator is another, `smallest_nonzero_contribution(contributions, excluded names)`, which names the force that sets the minimum and refuses (`FMOD-F-003`) a set with none.
- **FMOD-R-012.** The Jacobian each plugin returns is the derivative of **the acceleration it returns** — the same truncation, the same models, the same epoch — so a gate against finite differences of the plugin's own `accel` is a gate of the whole.

## 5. Interfaces

```
EarthOrientation::from_series(series: EopSeries&, policy: EopPolicy, leaps: LeapTable&) -> EarthOrientation
EarthOrientation::from_record(record: EopRecord, leaps: LeapTable&)                      -> EarthOrientation
EarthOrientation::rotation_at(t: Epoch)                      -> Result<frames::Rotation, ForceModelError>
EarthOrientation::describe()                                 -> string      // "series" | "fixed record"

SourceRecord { id: string, sha256: string }                  // what the caller says it read, from the manifest

Gravity(model: GravityModel&, orientation: EarthOrientation, degree: Degree, order: Order,
        options { extrapolate_secular: bool, sources: [SourceRecord] })              : dyn::Force
ThirdBody(body: Body, ephemeris: Ephemeris&, gm: GravitationalParameters&, leaps: LeapTable&,
          sources: [SourceRecord])                                                   : dyn::Force
third_bodies(bodies: [Body], ephemeris&, gm&, leaps&, sources)  -> Result<[shared_ptr<ThirdBody>], ForceModelError>   // PERT-F-013
Relativity(term: Term, ephemeris: Ephemeris&, leaps: LeapTable&, ppn: PpnParameters = GR) : dyn::Force
Tides(model: GravityModel&, orientation, ephemeris&, leaps&,
      options { solid: bool, ocean: OceanTide* + ocean_degree, solid_pole: bool, ocean_pole: OceanPoleTide* + ocean_pole_degree,
                solid_target: TideSystem = ZeroTide, extrapolate_secular: bool, sources })   : dyn::Force
      // [v0.1a, 2026-10-06, before any run: the first text wrote `Tides(config { … })`; the code takes the four borrowed objects and an options struct.]
Tides::by_model(t, r, v)                                     -> Result<[{ name, acceleration }], ForceModelError>

degree_meeting_criterion(field: ConventionalField&, radius_m: f64, f_min: f64)       -> Result<TruncationChoice, ForceModelError>
smallest_nonzero_contribution(contributions: [Contribution], excluded: [string])     -> Result<{ name, value }, ForceModelError>
```

## 6. Precision and accuracy

### 6.1 Budgets

| id | quantity | budget | because |
|---|---|---|---|
| `FMOD-P-1` | the chain of `frames` against `eraC2t06a`, per element (`FRAME-A-005`'s own threshold) | 5 uas = **2.42 × 10⁻¹¹ rad** | measured 1.55 µas there; the threshold is the one registered |
| `FMOD-P-2` | the operator norm of the difference of two such rotations, at most three times a per-element bound | 3 × 2.42 × 10⁻¹¹ rad = **7.26 × 10⁻¹¹ rad** | Frobenius ≤ √9 × the largest element |
| `FMOD-P-3` | the registered bound of `Gravity` against an independently-composed rotation, relative to the acceleration | 3.1 × 7.26 × 10⁻¹¹ = **2.25 × 10⁻¹⁰** (registered **3 × 10⁻¹⁰**) | `‖Rᵀ − R′ᵀ‖ ‖a‖ + ‖G‖ ‖R − R′‖ r ≤ (1 + 2.1) × 7.26 × 10⁻¹¹ ‖a‖`, because `‖G‖ r ≤ 2.1 ‖a‖` for a field that is a point mass to 10⁻³ |
| `FMOD-P-4` | the registered bound of `Tides` against the same, relative to the tide's acceleration | **1 × 10⁻⁸** | the same argument with `‖G‖ r ≤ (n+2) ‖a‖` for a series to degree *n* ≤ 100 |
| `FMOD-P-5` | the relativity plugin against (10.12) evaluated by hand in metres, per term, relative | 64 × 2.22 × 10⁻¹⁶ = **1.42 × 10⁻¹⁴** | 64 ε: each term is a few dozen rounded operations on well-conditioned operands |
| `FMOD-P-6` | the third-body plugin against `pair_direct` in metres, absolute | 16 ε μ (1/\|s\|² + 1/\|d\|²) | the difference of two terms of order μ/\|s\|² each good to four ε |
| `FMOD-P-7` | the **predicted looseness** of the finite-difference gate's `F` against the true supremum, point mass and degrees 2 – 4, measured by `tools/forcemodel_fd_sizing.py --scan` on the coarse grid before any run | **4.5 – 5.3** | `F` is rigorous (§6.2); it is 5× the supremum |
| `FMOD-P-8` | **the smallest relative defect the gate can see** in `Gravity`'s Jacobian, at the best step, per geometry (GPS, LEO 300, LEO 952.86, sail) | **1.7 – 1.9 × 10⁻⁹** of `‖G‖` | `min ε(h) / (2 μ/r³)`, the tool |
| `FMOD-P-9` | the predicted power of `Gravity`'s three wrong rows (§6.2 (iv)) at the four geometries, `max \|wrong − right\| / ε` at the best step | **2.0 × 10⁸ to 1.1 × 10⁹** | the tool; the requirement is 10³ |

### 6.2 The finite-difference gate, **frozen before any run** (rule 7; `STM-R-004`, `STM-R-005`, `STM-P-1`, `MEAS62`)

Reproduced by `tools/forcemodel_fd_sizing.py --check`, which runs as the ctest `forcemodel.fd_sizing_reproduces` (`PROVENANCE.md` §40.6). **Nothing below is changed after a run**; a run that misses it is reported with the criterion as written (rule 7), and a correction is a dated amendment that keeps the first text visible.

**What is compared.** For each plugin, at each of L4's four reference points (§8 `FMOD-A-001`), for each component pair *(i, j)*: the central difference `f̂ᵢⱼ(h) = [ aᵢ(r + h eⱼ) − aᵢ(r − h eⱼ) ] / (rⱼ⁺ − rⱼ⁻)` of the plugin's **own `accel`** at the fixed epoch and velocity, over **the sizes `h ∈ {10, 30, 100, 300, 1000} m`** for the position columns (the spacing in the denominator is the *actual* difference of the two operands, exactly representable, so the operands' own rounding does not enter) and **`h_v ∈ {0.01, 0.1, 1, 10} m s⁻¹`** for the velocity columns.

**Criteria.**
- **(b)** for every size and component, `|f̂ᵢⱼ(h) − Aᵢⱼ| ≤ εᵢⱼ(h) = h² F/6 + ν/h`, `A` the analytic row (the velocity columns with `F = 0`).
- **(c)** where a block is **declared absent** (gravity, third bodies, tides: `∂a/∂v`) every velocity estimate is **exactly 0**; where it is **declared present** (relativity) it satisfies (b) with `F = 0`, `ν/h_v`.
- **(d) the gate can fail** (rule 5): the controls of (iv) each fail (b) **by at least 10³ `ε`** at the best step, at every geometry, the test computing the ratio from the rows themselves.
- **(e) the noise bound is asserted point by point** (the lesson of L6, where the Earth-rotation angle's last bit exceeded a frozen `ν` thirty-fold): at **every** stencil point of every size, `|a_double − a_extended| ≤ ν`, the extended evaluation being the plugin's own function evaluated in x87 `long double` (64 mantissa bits, unit roundoff 5.4 × 10⁻²⁰) from independent formulas (below) on the **same double inputs**; an evaluation outside `ν` **fails loudly at that point** and the gate does not proceed on it. The comparator's own error, stated: at most (operation count ≈ 2 × 10³ for the degree-4 field) × 5.4 × 10⁻²⁰ ≈ 10⁻¹⁶ relative, **10⁻³ of the registered `ν`**.

**(i) `F` is a bound, with three lemmas** (`tools/forcemodel_fd_sizing.py`, docstring): Leibniz's rule with `|∂ₑᵏ r^(−q)| ≤ (q)ₖ r^(−q−k)` (Gegenbauer) and `|∂ₑᵏ P| ≤ ‖P‖ d^(falling k) r^(d−k)` for a homogeneous polynomial `P`; the potential of one normalised coefficient as a harmonic polynomial over a power of `r`; and the monotonicity in `r`, so the bound is taken at `r_min = r − h_max`.
- **Gravity** (point mass and degrees 2 – *N*): `F = 1.01 · [ 96 GM / r_min⁵ + Σₙₘ GM aₑⁿ ( |C̄ₙₘ| Φᶜₙₘ + |S̄ₙₘ| Φˢₙₘ ) / r_min^(n+5) ]`, `Φₙₘ = Nₙₘ maxᵢ[ ‖∂ᵢq‖ S3(n−1, 2n+1) + (2n+1) ‖q‖ S3(n+1, 2n+3) ]` (exact; `Φ₂₀ = 26 832.8`), `S3(d, q) = Σⱼ C(3,j) d^(falling 3−j) (q)ⱼ` (`S3(1,3) = 96`, `S3(1,4) = 180`, `S3(2,5) = 420`, `S3(0,3) = 60`, `S3(3,7) = 1140`); the factor 1.01 allows for the conventional substitutions' difference from the file's coefficients.
- **Third body**: `F = 96 μ / d_min⁵`, `d_min = |s| − |r| − h_max`.
- **Relativity**: Schwarzschild `F = (GM/c²)[ 4 GM · 180 / r_min⁶ + (v² + 4√3 v²) · 96 / r_min⁵ ]`; Lense–Thirring `F = (1+γ)(GM/c²)[ 3 |J| √3 v · 420 + |J| v · 60 ] / r_min⁶`; de Sitter `F = 0` (its acceleration does not depend on `r`); the velocity columns `F = 0` for all three (quadratic or linear).
- **Tides** (configured, **for this gate only**, to degrees ≤ 4: the solid Earth tide, the solid pole tide, the ocean pole tide to degree 2 and the ocean tide to degree 4; the registry gate `FMOD-A-006` runs the full default): `F` = the gravity formula's sum over the increments' `|ΔC̄|`, `|ΔS̄|`, **recomputed from the plugin's own increments in the test**. The rigorous bound's constants grow with degree (`Φ₂₀` = 2.7 × 10⁴ against the point mass's 96; the tool prints `Φ` to degree 8) and with them the looseness, so the gate stops at degree 4, and the test prints the bound's power (`ε / ‖G‖` at the best step) for the tides at each point; **the wrapper's code path is the same at every degree** — the higher degrees reach the same `gradient_of`, which `GRAV-A-031` holds to the definition to degree 36.

**(ii) `ν` is a registered bound of ONE evaluation** (§6.2(e) asserts it): `Gravity` `ν = 64 ε ‖a‖`; `ThirdBody` `ν = 16 ε μ (1/|d|² + 1/|s|²)`; `Relativity` `ν = 32 ε ‖a_term‖`; `Tides` `ν = 128 ε ‖a_tide‖`; ε = 2⁻⁵², the norms those of the plugin's own acceleration at the stencil's centre.

**(iii) The frozen figures** (`tools/forcemodel_fd_sizing.py`; `Gravity`, degrees 2 – 4 of EGM2008): the table below; `ε(h)` in m s⁻² per m. The test holds the formulas, recomputes the figures from the coefficients and asserts agreement to 1 % — the figures are not tuned.

| point (`r`) | `F` (m⁻² s⁻²) | `ν` (m s⁻²) | `ε(10 m)` | `ε(100 m)` | `ε(1000 m)` | best `ε / ‖G‖` |
|---|---|---|---|---|---|---|
| GPS 26 561 km | 2.948 × 10⁻²¹ | 8.03 × 10⁻¹⁵ | 8.03 × 10⁻¹⁶ | 8.52 × 10⁻¹⁷ | 4.99 × 10⁻¹⁶ | 1.67 × 10⁻⁹ |
| LEO 300 km | 3.417 × 10⁻¹⁸ | 1.27 × 10⁻¹³ | 1.28 × 10⁻¹⁴ | 6.97 × 10⁻¹⁵ | 5.70 × 10⁻¹³ | 1.77 × 10⁻⁹ |
| LEO 952.86 km | 2.078 × 10⁻¹⁸ | 1.05 × 10⁻¹³ | 1.06 × 10⁻¹⁴ | 4.52 × 10⁻¹⁵ | 3.46 × 10⁻¹³ | 1.89 × 10⁻⁹ |
| sail 720 km | 2.466 × 10⁻¹⁸ | 1.12 × 10⁻¹³ | 1.13 × 10⁻¹⁴ | 5.24 × 10⁻¹⁵ | 4.11 × 10⁻¹³ | 1.85 × 10⁻⁹ |

**(iv) The controls**, each a wrong row that must fail (b) by at least 10³ `ε` at the best step (the transposed rotation's power is `2 |sin θ|` of the in-plane acceleration with θ the Earth-rotation angle — **GPS 149.4°, LEO 29.0° (just outside the 30° – 150° the manager named; `|sin θ|` = 0.48, power 10⁸ against the requirement), sail 278.5° (`|sin θ|` = 0.99)** — and it is measured, not assumed): `Gravity` — the **transposed rotation** (`m G mᵀ` for `mᵀ G m`), the **unrotated** tensor (the ITRS `G` as if it were GCRS) and the **sign-flipped** tensor; `ThirdBody` — the factor 3 replaced by 1 in `3 d dᵀ`; `Relativity` — the velocity block with the position block's sign; `Tides` — the same three as `Gravity`. The **km slip** is a *value* control and is run in `FMOD-A-005`.

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `FMOD-F-001` | a third-body list without the Sun or without the Moon (`PERT-F-013`) | which is missing, and that their accelerations at 7331 km are about 3 × 10⁻⁷ and 1 × 10⁻⁶ m s⁻² | treat them as optional like the planets |
| `FMOD-F-002` | the Earth orientation, the ephemeris, the field or a tide model refuses at the call (coverage, quality, degree, secular-pole span, tide system, `DYN-F-006`) | the refusal's own identifier and message, with the plugin's name | return zero; hold the previous value; substitute a default |
| `FMOD-F-003` | the truncation comparator is asked of a set in which no other force is non-zero | the set and the point | return degree 2; return the full degree |
| `FMOD-F-004` | a tide model and the field disagree about the tide system (`GRAV-F-008`) | both systems | convert silently |

## 8. Acceptance tests

**The fixture** (all rows): L4's four reference points, exactly `tests/l4_ranking.cpp`'s — **GPS** 26 561 000 m, 2023-02-20 00:00 UTC, phase 0.7 rad; **LEO 300 km** 6 678 136.3 m and **LEO 952.86 km** 7 330 996.3 m, 2023-06-21 08:00 UTC, phase 0; **sail** 7 098 136.3 m, 2019-07-01 00:00 UTC, phase 1.2 rad — each a circular orbit in the GCRS xy-plane, `v = √(3.986004415 × 10¹⁴ / r)` along the tangent; the EOP series is `eop-c04-20`; the secular terms are extrapolated (the epochs are after 2017) and **recorded**. `ε` = 2⁻⁵².

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `FMOD-A-001` | **the registry**: `Gravity` (36, 36), `third_body.sun`, `third_body.moon`, the five planets of set B, `tides`, and the three relativity terms added to ONE `ForceSet` at each of the four points; `contributions_at` returns every plugin's contribution **attributed by the §3.2 names**, each non-zero and finite, the count asserted (4 points × 12 plugins = 48) | the names; the count | §3.2 | exact | R-001, R-002 |
| `FMOD-A-002` | *[v0.1a note, before any run: for the **value**, a central field is invariant under the transposed pair of rotations (`R g(Rᵀ r) = g(r)` for `g` a central vector field), so the control's power here comes from the non-central terms — about 10⁻³ of \|a\|, some 3 × 10⁶ times the bound — and not from `2 \|sin θ\|`; for the **Jacobian** (§6.2 (iv)) a one-sided rotation changes the point-mass tensor by `\|sin 2θ\|`, as the tool predicts. The test prints both.]* **`Gravity`'s wrapper, rotation checked independently of the wrapper**: with a fixed record (dX = dY = 0, `subdaily_applied`), the contribution against `a_ref = R′ᵀ g(R′ r)` with `R′` from `eraC2t06a` assembled in the test (not through `frames`) and `g` the L2 field's own `acceleration`; **and the control**, the transposed rotation `R′ g(R′ᵀ r)`, which must differ from the plugin by at least **10³ × the tolerance** at every point (the test prints `2 \|sin θ\|`) | the independent value | `ERFA`, `L2`'s field | ≤ 3 × 10⁻¹⁰ \|a\| (`FMOD-P-3`) | R-003, R-008, R-012 |
| `FMOD-A-003` | **L2's gated values reproduced through the registry**: (a) `GRAV-A-012` — the plugin at (2190, 2159) at the GCRS point `mᵀ (0, 0, b)`, `b` = 6 356 752.3142 m, has \|a\| = 9.832 184 9379 ± 0.005 m s⁻² **and** equals the field's direct value within `FMOD-P-3`; (b) `GRAV-A-027` — the plugin at (2, 0) at three points spanning latitudes 0°, 45° and 80° equals the exact J₂-only closed form rotated by the independent chain, within `FMOD-P-3` | the bands; the closed form | `WGS84`, `GRAV-A-027` | as stated | R-003, GRAV-A-012, -027 |
| `FMOD-A-004` | **third bodies**: each plugin **equals `Attraction::by_body` of {Sun, Moon, its body} at its body's entry, 0 ulp**; equals `pair_direct` evaluated by hand in metres within `FMOD-P-6`; **`PERT-A-014`**: the plugin's value minus the direct-only attraction equals `−μ s/\|s\|³` within `FMOD-P-6` (the indirect term is in); `third_bodies` refuses a list without the Sun, and one without the Moon (`FMOD-F-001`), and records the list | the values | `PERT` | as stated | R-004, R-005, F-001 |
| `FMOD-A-005` | **relativity, the crossing checked independently of the wrapper**: per term at each point, (i) equal to `Correction::by_term` fed the same typed inputs, **0 ulp**; (ii) equal to **(10.12) evaluated by hand in metres** — the test's own code and its own `× 1000.0` — within `FMOD-P-5` per term; (iii) inside **`tests/l4_ranking.cpp`'s per-term closed-form bands** (R3: Schwarzschild and Lense–Thirring ± 10⁻⁶, de Sitter `[0.85, 1.06] A_c`); **the km slip control**: the plugin's inputs with the crossing applied twice (or not at all) differ from the plugin by at least 10³ × `FMOD-P-5` on every term whose value depends on the slipped input (Schwarzschild, Lense–Thirring on the satellite's position and velocity; de Sitter on the satellite's velocity) | the hand values; the bands | `TN36-10` (10.12), `L4RANK` | as stated | R-006 |
| `FMOD-A-006` | **tides**: ONE plugin; **`by_model` sums to the total**, \|total − Σ by_model\| ≤ 128 ε Σ\|aₖ\| (the sum is formed once, applied once); the plugin equals the **independent construction** — `TideIncrements::sum` of the four models built in the test from the ITRS positions of the Sun and Moon rotated by the independent chain, the field's `acceleration_of` at `R′ r`, rotated back — within `FMOD-P-4`; every model's contribution is non-zero and below 10⁻⁵ m s⁻²; the provenance carries every model's record; the ocean tide's degree is `OceanTide::degree_meeting_criterion(r)` at each point and the ocean pole tide's is 10 | the independent value | `PERT-R-061`, `-062` | as stated | R-007 |
| `FMOD-A-007` | **declarations**: names as §3.2; `consumes()` empty; the velocity block **absent with bound 0.0** for `Gravity`, `ThirdBody`, `Tides` and **present** for the three relativity terms; the provenance present with the stated key order, `verified_against_issuer == false`, `source_sha256` listing exactly what the caller supplied (`unsupplied` for a table not supplied); `describe_orientation()` says `series` or `fixed record` | as stated | §3.2, `DYN-R-027`, `-051` | exact | R-002, R-006, R-008, R-010 |
| `FMOD-A-008` | **refusals pass through unchanged** (`FMOD-R-009`, `FMOD-F-002`, `-004`): (a) `Gravity` at an epoch outside the EOP series' coverage refuses with the series' own identifier and message *[v0.1a, before any run: the first text said "before the coverage (1960-01-01)"; UTC before 1972 cannot be constructed, so the epoch is GPS week 2700, after it]*; (b) `Gravity` built on a fixed record whose sub-daily terms are not applied refuses with `FRAME-F-003`; (c) `Gravity` at 2035-01-01 without the secular override refuses with `GRAV-F-006`, and with it succeeds and records `secular_pole=extrapolated`; (d) `Gravity` at degree 2191 or order above 2159 is not constructible (`GRAV-F-004` at the typed factory); (e) `ThirdBody` at an epoch outside the ephemeris' coverage refuses with the ephemeris' own identifier and message *[v0.1a, before any run: the first text said "for a body with no pinned GM refuses with `PERT-F-012`"; every body the enumeration allows as a third body has a pinned GM, so that refusal cannot be reached through a valid plugin and is `thirdbody`'s own test's]*; (f) `Tides` whose solid Earth tide is requested in the tide-free system against the zero-tide field refuses with `GRAV-F-008` — and not with a zero; in every case the identifier and the message are the source's own and the plugin's name is prepended | as stated | `FMOD-R-009` | identifiers equal | R-009, F-002, F-004 |
| `FMOD-A-010` | **the finite-difference gate, `Gravity` (N = M = 4)**: criteria (b), (c), (d), (e) of §6.2 at the four points, sizes and noise bound as frozen, `F` recomputed from EGM2008's degrees 2 – 4 and asserted equal to the frozen figure to 1 % | pass | §6.2 | `ε(h)` | R-012 |
| `FMOD-A-011` | the same, **`ThirdBody`** — the Sun, the Moon and Jupiter, at the four points | pass | §6.2 | `ε(h)` | R-004, R-012 |
| `FMOD-A-012` | the same, **`Relativity`** — the three terms, both blocks, at the four points: Schwarzschild and Lense–Thirring position columns with their `F`, de Sitter's **exactly 0**, the velocity columns with `F` = 0 | pass | §6.2 | `ε(h)` | R-006, R-012 |
| `FMOD-A-013` | the same, **`Tides`** configured to degrees ≤ 4 as §6.2 says, `F` recomputed from the plugin's own increments | pass | §6.2 | `ε(h)` | R-007, R-012 |
| `FMOD-A-014` | **the gate can fail** (rule 5): the controls of §6.2 (iv), each at every geometry, fail (b) by at least 10³ `ε`; the ratios are printed beside `FMOD-P-9`'s predictions | ≥ 10³ | §6.2 | exact | R-012 |
| `FMOD-A-020` | **the truncation function**: at the degree it returns `truncation_rms(r, N) ≤ F_min` and at *N* − 1 it is above (the definition, checked); `F_min` is the smallest non-zero contribution **of the set without `Gravity`**, and the plugin that sets it is named; **changing `Gravity`'s degree in the registry does not change `F_min`** (the comparator is independent of the truncated series); a set with no non-zero force is refused (`FMOD-F-003`) | the definition | §3.4, `PERT-R-022a` | exact | R-011, F-003 |
| `FMOD-A-021` | **the truncation table**: at each of the four points, for **set A** and **set B**, the degree, `truncation_rms` at *N* and at *N* − 1, `F_min` and **the force that sets it**, the maximum of `‖a₂₁₉₀ − a_N‖` along the 96-point test orbit **beside** the RMS (the ratio printed), and the degree at which the orbit maximum would meet `F_min`; **the predictions, written before any measurement: set A — GPS ≈ 9 (8 – 11), 7331 km ≈ 98 (90 – 110), 720 km ≈ 125 (105 – 150), 300 km ≈ 275 (200 – 350) (the manager's, ruling R4); set B — at each of the three low points `N_B − N_A` ∈ [20, 100], at GPS `N_B − N_A` ∈ [0, 4]; the orbit maximum exceeds the sphere's RMS by a factor in [1.5, 8]** — a miss is reported as written | the predictions | §3.4 | recorded, not a criterion | R-011 |
| `FMOD-A-030` | **Earth-orientation handling, the size over an arc** (finding F4): at the GPS point, `Gravity` (36, 36) built from the series against the same built from the fixed record of the arc's middle epoch, at 25 epochs over 24 hours: the largest difference in the GCRS acceleration, absolute and relative, **printed** | reported | §3.3 | none (information for the exit gate) | F4 |

**Coverage.** `FMOD-R-010` is discharged by `FMOD-A-007` (two plugins built differently and interleaved); `FMOD-R-001` by the build (the module's link line names exactly its dependencies, and `estimation` is not among them); `FMOD-F-001` by `FMOD-A-004`, `FMOD-F-002` and `-004` and `FMOD-R-009` by `FMOD-A-008`, `FMOD-F-003` by `FMOD-A-020`.

## 9. Provenance obligations

At the step's close `PROVENANCE.md` §40.6 records: the sizing frozen before any run with the tool's output hash; each check's first run with its log hash; every registration defect found by a run and amended openly with the first text kept; the finite-difference gate's measured looseness against `FMOD-P-7`; the truncation table with each prediction beside its result; the three controls' measured powers beside `FMOD-P-9`'s.

## 10. Open questions for the manager

| id | question | author's recommendation |
|---|---|---|
| `FMOD-Q-001` | **Which registered set does the exit gate's gravity degree come from?** Set A's smallest force is relativity (Lense–Thirring at GPS, de Sitter at LEO); set B adds five planets, the smallest of which is below it by a decade or more at LEO. | The table states both; the exit gate's GPS fits are not sensitive (the two sets differ by at most a few degrees there) and use set A's degree or `TN36-6` Table 6.1's 12, whichever the table supports. |
| `FMOD-Q-002` | The ocean tide's degree and the ocean pole tide's are **constructor arguments** with `degree_meeting_criterion` supplying the ocean tide's default. | As stated; the exit gate states them. |

---

## Changelog

| version | date | change |
|---|---|---|
| 0.1 | 2026-10-06 | first draft, written ahead of the module with every check registered (ruling R1 – R5 of the manager, L7 step 1) |
| 0.1a | 2026-10-06 | **amended while the module was written, before any registered check ran, the first texts kept in brackets:** `FMOD-R-004` (`pair_stable` for `by_body`, which refuses a one-body list), the `Tides` constructor's shape, two of `FMOD-A-008`'s cases (an epoch that can be constructed; a refusal that can be reached), the `source_sha256` separator |
