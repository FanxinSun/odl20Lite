# SPEC-atmosphere — NRLMSISE-00 and the space-weather inputs it runs on

| | |
|---|---|
| **Spec ID** | `ATMO` |
| **Status** | **adopted** 2026-09-18; **amended the same day** by the manager's rulings on all six questions |
| **Version** | 1.1 |
| **Date** | 2026-09-18 |
| **Layer** | L2 `environment`, step 4 (`../plan/PLAN.md` §3.3) |
| **Depends on** | `SPEC-time.md` (the epoch and the day-of-year), `SPEC-frames.md` (geodetic latitude, altitude above the ellipsoid), `SPEC-gravity.md` (the reference ellipsoid), `core` |
| **Depended on by** | the drag force (L4, plan §3.5 step 4), the density campaign (L8 step 5) |

**Derivation declaration (plan R1).** This specification was written from the documents listed
in §2 and from no implementation of this module. The NRL reference FORTRAN is itself one of
those documents, and §3.1 states why that is a different relationship from the one this tree
has with any other source, and what it costs.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 0. The gate this step was given does not exist — the finding, and the search

Plan §3.3 step 4 says: *"Gate: **the model's published reference profiles**."* Plan §3.3's exit
gate says: *"every model **reproduces its published reference values**."* Plan D2 says: *"own
port from the NRL public-domain FORTRAN, **validated on its packaged tests**."*

Rule 4's first half — *a gate's wording is checked against what the source actually prints* —
applies before any of it. It was applied. **NRL publishes no reference profile, no reference
value, and no expected output of any kind.** The rest of this section is the search that
established it, because rule 4's second half is that *a finding of absence is recorded with the
search that established it.*

### 0.1 What the distribution contains

`https://map.nrl.navy.mil/map/pub/nrl/NRLMSIS/NRLMSISE-00/` (HTTP 200, retrieved 2026-09-18)
offers **five** files. All five were downloaded and all five were read.

| file | bytes | SHA-256 (first 8) | what it is |
|---|---|---|---|
| `NRLMSISE-00.FOR` | 114 981 | `cce0420e` | the model, 2437 lines, **plus a test driver at lines 2438–2552** |
| `NRLMSISE-00_2002JA009430-datavsmodels.txt` | 42 167 | `7946e4cb` | 27 tables of **mean residual and standard deviation of _data_ relative to _model_** |
| `NRLMSISE-00_2002JA009430-readme.txt` | 3 845 | `3a6e3d6f` | defines those two statistics' formulas |
| `NRLMSISE-00_2002JA009430_tables-datasets.doc` | 116 224 | `abe2b44c` | the same 27 tables, Tables 1(a) … 9(c), in Word |
| `NRLMSISE-00_jgra16630.pdf` | 425 444 | `2ef96668` | Picone, Hedin, Drob & Aikin (2002), the paper |

### 0.2 The search over the FORTRAN

Counts in `NRLMSISE-00.FOR`, whole file: `PROGRAM` **0**, `SUBROUTINE` **22**, `GTD7` **19**,
`TEST` **10**, `EXPECTED` **0**, `DATA.*OUTPUT` **0**.

Counts **after line 2438**, which is the line reading `C      TEST DRIVER FOR GTD7 (ATMOSPHERIC
MODEL)`: `OUTPUT` **0**, `RESULT` **0**, `SAMPLE` **0**, `COMPARE` **0**, `EXPECTED` **0**, and
any literal in scientific notation `E+1[0-9]` **0**. The only numeric literals in the driver are
its input `DATA` statements and its `FORMAT` field widths.

The driver computes and writes. It ends `WRITE(6,300) … / STOP / END`. **There is no expected-output
table in it, and none anywhere else in the file.**

### 0.3 The search over the other four files

`datavsmodels.txt` and `tables-datasets.doc` carry the same 27 tables — 9 thermospheric
variables × 3 geomagnetic-activity levels — whose two columns are, in the readme's own
notation, `"MEAN" = beta_T = <T_i(data) - T_i(model)>` and `"SD" = sigma_T`. These are
**statistics of the model against the NRLMSIS database**. The database is not distributed. To
recompute one cell of Table 1(a) one needs the 6236 Jacchia drag points between 200 and 400 km,
which NRL does not ship. They are therefore **not reproducible by this tree, or by anyone
outside NRL**, and they are not model output.

The paper: 827 lines of extracted text. `Table [0-9]` **2**, and both occurrences refer to those
same archive statistics ("*Table 1. Statistical Comparison of Empirical Models to Jacchia…*",
"*…contributions to total mass density, Table 1 compares the…*"). `reference profile` **0**,
`test case` **0**, `sample output` **0**, `electronic supplement` **0**, `auxiliary material`
**0**, `TINF` **0**. `Figure` **26** — the paper's model output is in figures, at figure-reading
precision, and in no table.

### 0.4 What the finding is, exactly

> **The distribution publishes inputs without outputs, and outputs without inputs.**
>
> The test driver publishes 17 fully specified input cases and no expected output.
> The archive tables publish 27 tables of output statistics and no reproducible input.
> Neither is a check, and no third thing exists.

Both the plan's wording and the question the manager put to this step ("*what the NRL
distribution actually ships is a test driver with its own expected output — say which of those
two you have*") name something that is not there. The answer to the question as put is **neither**:
what is there is a test driver with published *inputs* and no output at all.

### 0.5 What follows for the plan

Three sentences of the plan were wrong about their source. **All three were reworded and pushed
(72c68ca), and the general form is now plan §4 rule 6**, which settles the question as a
distinction rather than an exception: *a reference implementation is an oracle when a
specification exists that it implements, and is itself normative when none does — and the
difference is a search, not a preference.* The proposed replacements, as adopted:

| where | present wording | proposed |
|---|---|---|
| §3.3 step 4 | "Gate: the model's published reference profiles" | "Gate: the reference implementation, compiled from the pinned public-domain source and executed on its own 17 published input cases and on a domain-spanning sweep; plus the model's documented invariants, computed independently" |
| §3.3 exit gate | "every model reproduces its published reference values" | "every model reproduces its published reference values **where its source publishes any**, and where none exist the specification records the search that established it and names what stands in their place" |
| D2 | "validated on its packaged tests" | "validated against the reference implementation on the published inputs of its packaged test driver, which publishes no outputs" |

The exit-gate change is the one that generalises. L2's other three steps all had published
values to hit; this one is the first source in the tree that has none, and it will not be the
last — `SPEC-drag`'s coefficient literature and L9's ray-tracing papers are the same shape.

---

## 1. Purpose and scope

### In scope

* The **NRLMSISE-00 neutral atmosphere**: number densities of He, O, N₂, O₂, Ar, H, N and
  anomalous oxygen, total mass density in its two distinct senses, exospheric temperature and
  temperature at altitude, from the surface to the lower exosphere.
* **Space-weather ingestion**: the solar radio flux F10.7 and its 81-day centred mean, and the
  geomagnetic index Ap in both conventions the model accepts — with the coverage, revision and
  prediction policy that a non-frozen input forces this tree to state (§4.4, §4.5).
* The **domain** of the model as this tree defines it, and the refusals at its edges.

### Not in scope

* **The drag force itself** — the coefficient, the area, the relative-wind velocity, the
  co-rotating atmosphere. That is L4, plan §3.5 step 4, and it consumes this module.
* **DTM-2013 and JB2008.** Named in the plan's L2 source column as alternatives; neither is
  taken at this step. `ATMO-Q-006`.
* **Winds** (HWM) and the **ionosphere** (IRI). NRLMSISE-00 is neutral-density only.
* **Thermospheric storm-time nowcasting.** The model takes Ap; it does not assimilate.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `MSIS-FOR` | NRL, E. O. Hulburt Center | `NRLMSISE-00.FOR` — the model's reference implementation, and the sole definition of the function (§3.1) | self-stamped `MSISE-00  01-FEB-02  15:49:27` | `https://map.nrl.navy.mil/map/pub/nrl/NRLMSIS/NRLMSISE-00/NRLMSISE-00.FOR`, retrieved 2026-09-18, SHA-256 `cce0420e…44cb`, 114 981 bytes, 2552 lines | primary | **normative — the definition** |
| `PICONE02` | Picone, Hedin, Drob, Aikin | *NRLMSISE-00 empirical model of the atmosphere: Statistical comparisons and scientific issues* | JGR **107**(A12), 1468, 2002 | doi:10.1029/2002JA009430; `…/NRLMSISE-00_jgra16630.pdf`, retrieved 2026-09-18, SHA-256 `2ef96668…3b38`, 425 444 bytes | primary | normative (describes; does not define — §3.1) |
| `MSIS-STATS` | NRL | `NRLMSISE-00_2002JA009430-datavsmodels.txt` — 27 tables of data-minus-model statistics | AGU electronic dataset archive, 2002 | `…-datavsmodels.txt`, retrieved 2026-09-18, SHA-256 `7946e4cb…b8aa`, 42 167 bytes | primary | informative (**not reproducible** — §0.3) |
| `MSIS-README` | NRL | `NRLMSISE-00_2002JA009430-readme.txt` — the formulas for `MEAN` and `SD` | 2002 | `…-readme.txt`, retrieved 2026-09-18, SHA-256 `3a6e3d6f…58dc`, 3 845 bytes | primary | normative (for §6's accuracy statement only) |
| `MSIS-TABLES` | NRL | `NRLMSISE-00_2002JA009430_tables-datasets.doc` — the same 27 tables | 2002 | `…_tables-datasets.doc`, retrieved 2026-09-18, SHA-256 `abe2b44c…4771`, 116 224 bytes | primary | informative (duplicate of `MSIS-STATS`) |
| `GFZ-KP` | GFZ Helmholtz Centre for Geosciences, Geomagnetic Observatory Niemegk | `Kp_ap_Ap_SN_F107_since_1932.txt` — Kp₁₋₈, ap₁₋₈, Ap, SN, F10.7obs, F10.7adj, and a definitive/preliminary flag | continuously updated; 2026-09-18 snapshot covers 1932-01-01 … 2026-09-17, 34 594 daily rows | `https://kp.gfz-potsdam.de/app/files/Kp_ap_Ap_SN_F107_since_1932.txt`, retrieved 2026-09-18, 5 504 038 bytes | primary | **normative (dataset), and the issuing authority for Kp, ap and Ap** |
| `DRAO-FLUX` | Dominion Radio Astrophysical Observatory / Natural Resources Canada | `fluxtable.txt` — the Penticton 10.7 cm flux, three readings a day, observed and adjusted and URSI-corrected | continuously updated; 2026-09-18 snapshot covers **2004-10-28** … 2026-09-17, 23 951 lines | `https://www.spaceweather.gc.ca/solar_flux_data/daily_flux_values/fluxtable.txt`, retrieved 2026-09-18, 2 179 541 bytes | primary | **normative (dataset), and the issuing authority for F10.7** — used as the independent cross-check of §4.3a |
| `TAPPING13` | Tapping, K. F. | *The 10.7 cm solar radio flux (F10.7)* | Space Weather **11**, 394–406, 2013 | doi:10.1002/swe.20064 | **not obtained** | informative — the reference `GFZ-KP` cites for the local-noon convention |
| `NOAA-FLUX` | NOAA NGDC | the 10.7 cm flux archive named in `MSIS-FOR`'s own header comment | — | `ftp://ftp.ngdc.noaa.gov/STP/SOLAR_DATA/SOLAR_RADIO/FLUX/` | **not obtained** — the FTP host named in a 2002 comment does not answer in 2026 | informative |

**CelesTrak was in this table at v1.0 and is gone.** `SW-All.csv` was chosen for convenience —
one file, both quantities, derived columns, a data-class flag. `ATMO-A-008`'s measurement removed
the reason: its derived centred column is exact on observation and wrong on every forecast row,
and the derived columns were the only thing convenience was buying, since `ATMO-R-019` recomputes
them anyway. A redistributor whose derived column is wrong wherever it is predicted adds error
rather than value. It is **not** retained alongside for comparability; where a redistribution is
wanted it is declared as one, names its upstreams, and is never primary.

**On `NOAA-FLUX`, said here rather than left to §10.** `MSIS-FOR`'s header names an FTP path as
the place to obtain both the observed and the 1-AU-adjusted flux. That host does not answer in
2026. Nothing in §§4–8 depends on it *as a document*: what it was cited for is the distinction
between the two flux classes, and that distinction is stated in `MSIS-FOR`'s own header text,
which is pinned, and again in `GFZ-KP`'s. The dead locator is recorded because a reader checking
this specification's citation would otherwise find a dead link and not know whether it had ever
been checked.

---

## 3. Definitions and conventions

### 3.1 What kind of source this is, and what that costs

Every other source in this tree **defines a function and publishes values of it**. `TN36-6`
gives (6.8) *and* Table 6.5a's amplitudes; EGM2008 gives coefficients *and* the summation
convention; DE440 gives an SPK *and* `testpo.440`. Wherever the two disagree, the definition
wins and the published value is the check.

NRLMSISE-00 is not like that. `PICONE02` describes what data were used, how the fit was done,
and how the result compares to observation. **It does not define the function.** The function is
roughly 1 500 fitted coefficients together with the code that combines them, and both live only
in `MSIS-FOR`. There is no closed form to implement independently, and there is no published
value to check against (§0).

**This is now plan §4 rule 6**, adopted 2026-09-18: *a reference implementation is an oracle when
a specification exists that it implements, and is itself normative when none does — and the
difference is a search, not a preference.* The rule carries three obligations, and this
specification discharges them in §0.2 (the search, with its counts), in §8 (the cost, stated
where a reader meets it) and in `ATMO-R-014` (a second axis — the documented total-density
relation, checkable without the reference's arithmetic).

- **ATMO-R-001.** For this module, *the model* means the function computed by `MSIS-FOR` at the
  pinned hash. `PICONE02` is normative for what the quantities *mean* — which index is which
  species, what "anomalous oxygen" denotes, what altitude regimes the fit was informed by — and
  is not normative for any number.

This has one consequence that must be stated plainly rather than discovered later:

> **This tree cannot check that `MSIS-FOR` computes what `PICONE02` describes.**
>
> For EGM2008 an independent evaluation of the published coefficients checks the implementation.
> Here the coefficients and the implementation are the same artefact. If `MSIS-FOR` contains an
> error, this tree reproduces it, and so does every other user of NRLMSISE-00 — the drag model
> would be *consistent with the field* and wrong together with it. No test in §8 can detect
> that, and §8 says so rather than implying otherwise.

- **ATMO-R-002.** The port is made from `MSIS-FOR` and from nothing else. No other
  implementation of NRLMSISE-00 — in any language — is consulted, read or compared against. The
  plan's §6 already dropped the Brodowski C port as unlicensed; this requirement is the positive
  form of that and covers implementations that *are* licensed, because a second port would give a
  false sense of independent confirmation while sharing `MSIS-FOR`'s every property.

- **ATMO-R-030.** Each of the three conditions `MSIS-FOR` prints and continues from — the
  coincident spline node, the non-positive density ratio before the logarithm, and `GHP7`'s
  non-convergence — is a **refusal** in this module, not a message. `ATMO-F-012`. They are
  reachable only through internal inconsistency rather than through caller input, so none is
  expected to fire; that is precisely why they must not be left as writes to standard output,
  which is where an unexpected condition goes to be unnoticed. `ATMO-A-019` fires each by
  construction, as plan §4 rule 5 requires of every guard in this tree.

- **ATMO-R-036.** **The materiality threshold of `ATMO-P-1` is a *drag* threshold, and it must
  not become the module's tolerance.** "A species whose mass contributes less than 10⁻¹⁵ of total
  density cannot affect drag" is a statement about drag, and this module returns **nine species
  densities**. A consumer taking anomalous oxygen at 110 km for surface erosion — not
  hypothetical; it is a mechanism that degrades a solar sail — would inherit a tolerance derived
  from a question it is not asking, and class C would hand it a **hard zero with no warning**.

  So the distinction is in the API, not only in the generated file's header:

  * `ATMO-P-3`'s tolerance is stated as applying to **total mass density and to drag**;
  * a **species** density carries whether the model resolves it. Its mass contribution is always
    available, because an unresolved species contributes a mass indistinguishable from zero and
    that *is* the right answer for drag;
  * its **number density** is a refusal where the model does not resolve it (`ATMO-F-017`). For
    that consumer class C is not a precision class, it is a refusal — the value is not a small
    density, it is *below what this model resolves*, and those are different claims.

  `ATMO-A-027`. Plan §5 constraint 10 again: what a value means belongs in its type.

- **ATMO-R-035.** **This layer's atmosphere row goes in `tests/l2_floors.cpp`**, where plan §4
  rule 3 requires quantities that must be compared to be measured in one place. Two things make
  this row unlike the others and both are required of it. It carries its **uncertainty as well as
  its magnitude** — for every other row those are the same question and here they are not, and it
  is the uncertainty that dominates L4's budget. And it is measured at **both radii**, because
  drag is the one term in that table that varies over orders of magnitude across the regime and a
  single reference point would make the table lie in the way rule 3 exists to prevent. Each row
  states its own reference point; that is the rule working, not a departure from it. The
  uncertainty is reported with the **altitude, activity level and epoch** it was measured at
  (`ATMO-P-4`), never as a bare percentage. `ATMO-A-026`.

- **ATMO-R-027.** `MSIS-FOR` is compiled **only** into the test suite, never into the library.
  The oracle is a check, not a dependency, and a production build must not require a Fortran
  compiler. §8 states how the suite behaves where none is present.

### 3.2 The reference's working precision, measured

`MSIS-FOR` is **single precision throughout**: `DOUBLE PRECISION` **0**, `REAL*8` **0**,
`IMPLICIT` **0**, `.D0` **0**. The only three `REAL` declarations (lines 454, 965, 1216) exist to
override FORTRAN's implicit-integer rule for variables named `LAT` and `LONG`. Every arithmetic
operation in the model is IEEE binary32.

So "the model's value" is itself uncertain, and by how much is a measurement, not a guess. It was
measured, by compiling the *same source* twice with `gfortran-16` — once as written, once with
`-freal-4-real-8`, which promotes every `REAL` to double — and evaluating both over the 17
published input cases (`ATMO-A-002`).

- **ATMO-P-1.** Relative difference *r* = |*x*ₛ − *x*_d| / |*x*_d| between the single-precision
  build and the promoted-double build of the same source, over the 17 published cases **and a
  sweep crossing every branch boundary of §3.6** — 125 records × 12 quantities = **1500**
  comparisons. Every one is classified by whether it can affect a drag calculation at all:

  | class | | count | worst *r* |
  |---|---|---|---|
  | **A** | material | 1238 | **7.6706 × 10⁻⁶**, median 2.8108 × 10⁻⁷ |
  | **B** | immaterial | 32 | 7.8739 × 10⁻³ |
  | **C** | underflowed in the reference | 18 | — (single returns exactly 0) |
  | **Z** | zero in both | 212 | — |

  **The sweep changed the shape of this measurement and is why it exists.** Over the 17
  published cases alone the artefact looks like a single number — median ≈ 4 × 10⁻⁷, worst
  7.7 × 10⁻⁶. Over the sweep the worst is **7.9 × 10⁻³, a thousand times larger** — and every one
  of those large differences is a quantity of order 10⁻³⁰ to 10⁻³⁷: anomalous oxygen at 110 km,
  argon at 2000 km, hydrogen at 72.5 km.

  **It is one phenomenon in three regimes, not a tolerance with an exception bolted on.** Single
  precision's smallest normal is 1.18 × 10⁻³⁸; these are values approaching that boundary, losing
  significance continuously on the way down, until they cross it and become §3.3's hard zeros.

  So the class boundary is drawn **physically, not numerically**: a species whose mass contributes
  less than 10⁻¹⁵ of the total density cannot affect drag at any precision. That threshold is
  **not tuned** — class A's worst is 7.671 × 10⁻⁶ at 10⁻¹², the same at 10⁻¹⁵, and only
  1.1 × 10⁻⁵ at 10⁻²⁰. Stable across three decades is what distinguishes a principled boundary
  from a fitted one.

  And the bound did not move: **class A's worst is argon at 1000 km, published case 3** — the
  same comparison the 17 cases alone produced. Adding 1056 material comparisons across every
  branch boundary left it exactly where it was, which is the strongest thing the sweep could
  have said.

### 3.3 The third regime: where the reference has a hole and a double port does not

- **ATMO-P-2.** **Eighteen** of the 1500 comparisons are class C: the single build returns
  **exactly zero** where the promoted-double build does not. Two are in the published set —
  anomalous oxygen at 100 km, cases 4 and 17, returning 2.820 × 10⁻⁴² and 2.415 × 10⁻⁴² cm⁻³ —
  and the other sixteen are the same species below 120 km across the sweep, down to
  1.006 × 10⁻⁶³. Single precision's smallest normal is 1.175 × 10⁻³⁸, so the intermediate
  underflows to zero before a result can be formed, while in double it survives.

A double-precision port therefore **disagrees with the reference by 100 % at eighteen places,
two of them among the seventeen published cases**, and is right to. A relative-difference gate is undefined where the
reference underflowed, and no choice of tolerance repairs it: loosen it to pass and the gate no
longer checks anything at 100 km; leave it tight and a correct port fails.

This is the third appearance of one shape in this tree. `SPEC-gravity` §3.6a: both Legendre
representations fail at degree 2190, one by underflow and one by overflow. `SPEC-perturbations`
§8: a gate statistic squared a representable quantity whose square underflows. Now: the
reference's own output has a hole in it that the port does not have. The shape is **a quantity
that exists in one precision and not in another**, and the defence is the same each time — make
the comparison's class explicit and *count the members of each class*, rather than choosing a
tolerance that spans both. `ATMO-A-002` does that, and §8's denominator paragraph states the
counts it must print.

### 3.4 The output, and the two total densities

`MSIS-FOR`'s `D(1..9)`, `T(1..2)`, in the source's own order: He, O, N₂, O₂, Ar, **total mass
density**, H, N, anomalous O; exospheric temperature, temperature at altitude.

- **ATMO-R-005.** The interface returns a named structure, not an array of nine. An index-6
  total density sitting between argon and atomic hydrogen is a positional convention with no
  meaning outside FORTRAN, and plan §5 constraint 10 applies: what a value means belongs in its
  type.

- **ATMO-R-004.** `MSIS-FOR` has **two** entry points whose sixth output is not the same
  quantity, which its header states in capitals — *"D(6), TOTAL MASS DENSITY, is NOT the same for
  subroutines GTD7 and GTD7D"*:

* `GTD7` — D(6) is the sum over He, O, N₂, O₂, Ar, H, N. **Anomalous oxygen excluded.**
* `GTD7D` — D(6) is the "effective total mass density for drag", the same sum **plus** anomalous
  oxygen.

These are different numbers with the same name, and the one a drag model wants is `GTD7D`'s.
They are therefore **distinct types at this interface**, not a boolean argument — the same
ruling `PERT-Q-010` reached for the ephemeris accessors, and the same constraint behind it. A
caller cannot silently receive the wrong one, and L4 cannot accept the wrong one.

- **ATMO-R-014.** The total density is **computed by this module from its own species
  densities**, not transcribed from the reference's expression. `MSIS-FOR` computes it at lines
  268, 352 and 921 as `1.66E-24*(4.*D(1)+16.*D(2)+28.*D(3)+32.*D(4)+40.*D(5)+D(7)+14.*D(8))`, and
  because the port computes it independently the relation becomes a **check rather than a
  copy** — one that does not go through the reference's arithmetic at all. Measured during
  drafting over all 17 cases: worst relative departure **3.280 × 10⁻¹⁶** in the double build
  (roundoff) and 8.326 × 10⁻⁸ in the single build (single-precision roundoff); with anomalous
  oxygen added, 3.871 × 10⁻¹⁶ and 1.130 × 10⁻⁷. `ATMO-A-004`.

- **ATMO-R-007.** `MSIS-FOR`'s header states *"O, H, and N are set to zero below 72.5 km"*.
  Confirmed in both builds across the five published sub-72.5 km cases: O, H, N **and** anomalous
  oxygen are exactly zero, 5 × 4 = **20 values**, which are exactly the 20 zero denominators
  excluded from `ATMO-P-1`. This is a published statement about the model, checkable without the
  reference's arithmetic, and it is a requirement here rather than an observation.

### 3.5 Units

`MSIS-FOR` returns CGS — cm⁻³ and g cm⁻³ — unless `METERS(.TRUE.)` has been called, which is
**global mutable state** setting a saved flag consulted on every later call.

- **ATMO-R-006.** The interface is SI throughout: m⁻³ and kg m⁻³. The CGS/SI switch is not
  exposed, no global flag exists, and the conversion happens once at the boundary. Plan §5's
  no-global-state constraint forbids reproducing `MSIS-FOR`'s `IMR` flag, and `SPEC-frames`
  `FRAME-R-062`'s km/metre crossing is the precedent: the crossing is named once, in one place.

### 3.6 The altitude branches, and where the model's own grid ends

`MSIS-FOR` is piecewise in altitude over three spline node sets, with a mixing height:

| | nodes (km) | line |
|---|---|---|
| `ZN1` | 120, 110, 100, 90, **72.5** | 588 |
| `ZN2` | 72.5, 55, 45, **32.5** | 153 |
| `ZN3` | 32.5, 20, 15, 10, **0** | 152 |
| `ZMIX` | 62.5 | 154 |

Above `ZN1(1)` = 120 km the profile is analytic (Bates) and has **no upper node**; the model
continues upward without a structural boundary. Below `ZN3(5)` = **0 km there is no node at
all** — the cubic spline is being evaluated outside its own knot range, which is extrapolation
of a fitted spline and is the one edge of the domain the source's own construction argues for.
§4.2 uses this and nothing else.

### 3.7 The two Ap conventions, and the switch that selects them

`MSIS-FOR` accepts `AP` as **either** a single daily value **or** a 7-element array — (1) daily
Ap, (2) the 3-hourly Ap at the epoch, (3)–(5) the 3-hourly values 3, 6 and 9 hours earlier,
(6) the mean of the eight 3-hourly values from 12 to 33 hours before, (7) the mean of the eight
from 36 to 57 hours before. **The array form is read only when `SW(9) = −1`**, set through
`TSELEC`. Cases 16 and 17 of the published input set exercise exactly this and nothing else.

This is a second piece of global mutable state selecting which of two incompatible readings a
caller's argument gets, and getting it wrong is silent: pass seven numbers with the switch
unset and the model reads the first and ignores six.

- **ATMO-R-009.** The two conventions are **two distinct input types**, and the switch does not
  exist at this interface. Supplying storm-time 3-hourly indices and supplying a daily index are
  different requests, and the type says which was made. `ATMO-F-008` covers the arrival of one
  where the other is required.

- **ATMO-R-010.** Local apparent solar time is **not a free parameter**. `MSIS-FOR`'s header
  notes that UT, longitude and local time enter the model independently and *"for the most
  physically realistic calculation these three variables should be consistent
  (STL=SEC/3600+GLONG/15)"*. A three-argument interface invites the inconsistent call and cannot
  detect it. This module computes STL from the epoch and the longitude. The independent form is
  reachable only through a separately named entry point whose name says what it is for, and which
  `PICONE02`'s own sensitivity studies are the use case for.

---

## 4. Required behaviour

### 4.1 Evaluation

- **ATMO-R-003.** The port computes in **double precision**. It is therefore not bit-identical
  to `MSIS-FOR` and must not claim to be; §3.2 measured the gap and §8 gates against it. Single
  precision was considered and rejected: it would reproduce the reference exactly, including its
  100 km anomalous-oxygen hole (§3.3), and would then carry 10⁻⁷ noise into a force model whose
  smallest kept term L2 has just measured at 3.478 × 10⁻¹¹ m s⁻² (plan §4 rule 3). Reproducing an
  artefact of a 2002 compiler is not fidelity to the model.

- **ATMO-R-011.** Day of year is 1 … 366, which `MSIS-FOR`'s header states. The year is
  **ignored by the model** — its header says so in parentheses — and this module therefore takes an
  epoch and derives the day of year from it rather than taking `IYD`, so that a caller cannot form
  the `YYDDD` integer wrongly. The discarded year is recorded in the result's provenance so that a
  reader can see what was and was not used.

- **ATMO-R-012.** The mass selector's accepted set is `MSIS-FOR` line 586's
  `MT/48, 0, 4, 16, 28, 32, 40, 1, 49, 14, 17/` — **eleven values**. Ten are ordinary: 0 is
  temperature only, 48 is all species, 17 is anomalous oxygen only, and the rest are the species
  masses. **49 is not documented anywhere in the file** — zero comment lines mention it — and it is
  not the same as 48: at line 769, inside `GTS7`, it accumulates twice the O₂ number density into
  the running total, which is an oxygen-atom-equivalent count rather than a species count.

This is a second rule-4 finding, inside the source rather than in the plan: *an accepted input
value with no documentation at all.* `ATMO-Q-002` puts the choice to the manager. This
specification's default is to expose the **ten documented values** and refuse 49 with a
diagnostic that says it is undocumented rather than invalid — a refusal that tells the truth
about why, since 49 is not a typo and a caller who used it meant something.

- **ATMO-R-013.** `MSIS-FOR`'s header: *"F107, F107A, and AP effects are neither large nor well
  established below 80 km and these parameters should be set to 150., 150., and 4.
  respectively."* Below 80 km this module **uses those three values**, whatever the caller
  supplied, and records in the result that it did. It is the source's own instruction, it is not
  optional, and a caller who passes storm-time indices for a 60 km evaluation is asking for
  something the model does not offer. `ATMO-F-009` covers the case where the caller supplied
  something else — the substitution is reported, not silent.

### 4.2 The domain: what is cited, and what is chosen

**`MSIS-FOR` refuses exactly one thing.** Whole-file counts: `RANGE` **0**, `LIMIT` **0**,
`PRINT` **0**. There is no altitude check, no latitude check, no epoch check and no flux check
anywhere in it; the **thirteen** altitude comparisons in the model — eleven of them of the form
`IF(ALT…)`, which is why a grep for that pattern undercounts them — are all branch selection
between the spline regimes of §3.6, and none of them precedes a diagnostic. The only input
validation is the mass number, which prints `MASS nnn NOT VALID`. The two `STOP`s are the
driver's own and an internal coefficient-set consistency check (line 1229), neither of which is
input validation.

**And three of the model's six `WRITE` statements print a diagnostic and then carry on.** Line
1539 `'BAD XA INPUT TO SPLINT'` fires when two spline nodes coincide, line 1591 `'DNET LOG
ERROR'` when a density ratio is non-positive before a logarithm, and line 445 is `GHP7`'s
convergence trace. None of the three stops, returns a status, or prevents a number being
produced: the caller receives a value computed after the condition the model itself called an
error, and learns of it only if standard output was being watched. A `Result` interface has no
such option, and `ATMO-R-030` says what these become.

The plan's step-4 gate asks for *"out-of-range input refused with a diagnostic naming the request
and the limit."* **The reference has no limits to cite.** So every limit below is this tree's,
and the table says for each one whether it comes from the source or from us. Rule 4's discipline
is that the distinction is written down, not that the limits are all citable.

| limit | value | cited or chosen | basis |
|---|---|---|---|
| day of year | 1 … 366 | **cited** | `MSIS-FOR` header |
| mass selector | the ten documented of line 586's eleven | **cited** (the set), **chosen** (excluding 49) | `MSIS-FOR` line 586, and `ATMO-R-012` |
| flux and Ap below 80 km | forced to 150, 150, 4 | **cited** | `MSIS-FOR` header |
| altitude ≥ 0 km | 0 km | **chosen**, with a structural argument | `ZN3`'s lowest node **is** 0 km (§3.6); below it the fitted cubic is extrapolated outside its own knots |
| altitude — upper | **none** | **chosen** (the absence is the choice) | above 120 km the profile is analytic with no node, and `MSIS-STATS` Table 1(a) carries a drag row above 1200 km, so the model is informed by data there. Refusing at 1000 km because the published driver stops there would refuse a domain the model covers |
| F10.7, F10.7A | > 0 | **chosen** | physical; the reference would compute happily with a negative flux |
| Ap | ≥ 0 | **chosen** | physical |
| epoch | within usable space-weather coverage | **chosen** | §4.4 |

- **ATMO-R-015.** This module refuses where the reference does not, and that is a deliberate
  divergence rather than an oversight. It has one consequence for §8 that must be stated here
  because it is easy to lose: **the comparison domain and the refusal domain are disjoint by
  construction.** The oracle gate compares only inside the domain this tree accepts. Where this
  tree refuses, there is nothing to compare, because the reference's answer there is a number
  produced by extrapolating a spline outside its knots and this tree declines to return it.

### 4.3 The three inputs, and the traps in each

The model takes three numbers. Each has a trap, and each trap is silent.

- **ATMO-R-016.** **Observed, not adjusted.** `MSIS-FOR`'s header: *"F107 and F107A values used to
  generate the model correspond to the 10.7 cm radio flux **at the actual distance of the Earth
  from the Sun** rather than the radio flux at 1 AU."* `GFZ-KP` publishes both, in adjacent
  columns, and says the same thing independently: *"For ionospheric and atmospheric studies the
  use of F10.7obs is recommended."* The two differ by (*r*/1 AU)², which runs 0.967 … 1.034 —
  up to 3.4 %, 6.9 % peak to peak over a year, with an annual period a drag analysis would
  absorb into a fitted ballistic coefficient. This module reads **observed**. `ATMO-A-007`.

- **ATMO-R-017.** **F10.7 is the previous day's value.** `MSIS-FOR`: *"F107 - DAILY F10.7 FLUX
  FOR PREVIOUS DAY."* Not the day of evaluation. An off-by-one here is invisible in any single
  evaluation and systematic across a campaign. `ATMO-A-018`.

- **ATMO-R-018.** **F10.7A is centred, not trailing** — *"81 day AVERAGE OF F10.7 FLUX (centered
  on day DDD)"* — and this module **computes** it from the daily series rather than reading any
  file's derived column. `ATMO-R-019`.

### 4.3a Where the numbers come from, and the redistribution measured rather than trusted

**The issuing authorities, both of them.** `GFZ-KP` for Kp, ap and Ap; `DRAO-FLUX` for F10.7.
CelesTrak is not used (§2).

`GFZ-KP` is the **primary**: it is the issuing authority for the geomagnetic indices, it carries
the **eight three-hourly ap values** that `ATMO-R-009`'s 7-element convention needs, it covers
1932-01-01 … 2026-09-17 in 34 594 daily rows, **it contains no forecast rows at all** — it ends
at the previous day — and it is CC BY 4.0. It also carries F10.7obs and F10.7adj, which it
attributes: *"provided by Dominion Radio Astrophysical Observatory and Natural Resources
Canada"*, citing `TAPPING13` for the **local-noon** convention.

`DRAO-FLUX` is pinned as the **independent cross-check** of that attribution. It is not a second
production route; it is the separate route that verifies the first, which is `PERT-A-002`'s
shape.

**The verification, measured.** `GFZ-KP`'s F10.7obs against `DRAO-FLUX`'s local-noon (20:00 UT)
observed flux, over the 7 969-day overlap 2004-10-28 … 2026-09-17:

> **7 969 of 7 969 exact. Zero differing.**

**And the tie-break the measurement exposed.** Sixteen dates carry **two** readings both stamped
20:00 UT. Taking the last gives 16 disagreements of up to 2.9 sfu (2022-10-23: 108.4 against
105.5); taking the **first** gives none. `GFZ-KP` takes the first on 16 of 16.

- **ATMO-R-033.** The daily flux is the **first** reading stamped 20:00 UT on that date, and the
  rule is stated because it is a choice: the 16 dates that distinguish it from last-wins are the
  only places the two rules differ, and everywhere else the wrong rule is invisible. Fourteen
  further dates carry **no** 20:00 reading at all, and are absent rather than substituted.
  `ATMO-A-020`.

**The unverified window, scoped to the search that bounds it.** `DRAO-FLUX` begins
**2004-10-28**, so the cross-check covers 2004–2026 and nothing earlier; F10.7 before that date
is available only through `GFZ-KP`'s redistribution. What was searched for a longer series, on
2026-09-18: the `fluxtable.txt` directory (**403**), the monthly-averages path (**404**), a
conjectured historical filename (**404**), the two `spaceweather.gc.ca` solar-flux pages, whose
HTML carries no link matching `flux|archiv|data|histor|download` other than a self-link, and the
Government of Canada open-data catalogue for *solar flux penticton* (**1** result, unrelated).

> **That bounds the search; it does not establish that NRCan publishes no historical series.**
> The claim this specification makes is the smaller one: *no pre-2004 daily series was found at
> the locators listed above.*

- **ATMO-R-031.** F10.7 carries a **verification class** — verified against the issuing
  authority, or not — and the flag is **load-bearing, not decorative**:
  * it propagates into every result that consumed the value;
  * `ATMO-A-021` asserts a pre-2004 result carries it **and that a post-2004 result does not**,
    because a flag that is always set is the same as no flag;
  * a consumer may **require** verified inputs, and gets `ATMO-F-015` naming the epoch and the
    verified window when it cannot have them.

  Without the third, "marked" degrades to "silent" for everyone who does not look, which is the
  whole reason this module does not simply serve the value quietly.

### 4.3b The licence is per column, so the columns are declared

`GFZ-KP` is **CC BY 4.0 except the sunspot-number column, which is CC BY-NC 4.0** — a
non-commercial term inside a file whose headline licence is permissive. A checker that reads a
file's headline licence passes this one while a non-commercial column sits inside it; this tree
has been past that shape once already, when a denylist saw no GPL string and CeCILL went
through.

- **ATMO-R-032.** The manifest entry **declares the columns consumed**, exactly as an archive
  entry declares its members, and **the loader refuses a column the manifest does not declare**.
  This module declares Kp₁₋₈, ap₁₋₈, Ap, F10.7obs, F10.7adj and the flag `D`; it does **not**
  declare `SN`, and reading it is `ATMO-F-016` rather than a note in a comment. The claim
  "this tree reads no sunspot number" is then checkable rather than asserted, which is the
  argument that produced member hashing in the first place. `ATMO-A-022`.

### 4.4 Coverage is per quantity, and it is a SET, not an interval

A centred 81-day mean on day *D* needs days *D* − 40 … *D* + 40, and the previous day's flux
needs *D* − 1. v1.0 of this specification therefore wrote usable coverage as the interval
[first + 40, last − 40]. **Measuring it showed that is wrong**, and the error is the kind that
passes review because the formula looks right.

`GFZ-KP`'s F10.7 column is **not dense**. Of 34 594 rows, **6 178** carry the sentinel −1.0:
**5 523** before Penticton began on 1947-02-14, and **655 interior**, in **459 separate runs** —
mostly single days, the longest six (1948-09-21 … 26). **The last interior gap is 2026-05-09**,
so the gaps are not a historical curiosity. Kp and ap, by contrast, have **zero** missing values
across all 34 594 rows.

Consequently:

- **ATMO-R-021.** Usable coverage is the **set of epochs whose own required window is complete**,
  and membership is decided per epoch — never an interval, and never a first/last pair.
  Measured on the 2026-09-18 snapshot: **22 991 days** have a complete centred 81-day window,
  running 1956-10-14 … 2026-08-08 **with 25 breaks inside that span** (one runs 1957-11-14 …
  1958-06-13). A loader reporting the endpoints would accept an epoch inside a hole.
  `ATMO-A-023`.

- **ATMO-R-034.** The sentinel is **not a value**. −1.0 for F10.7, −1.000 for Kp and −1 for ap
  and SN are absence, and a window containing one is incomplete rather than slightly wrong: a
  single −1 inside an 81-day mean moves it by (*F* + 1)/81 ≈ 1.9 sfu at *F* = 150, which is
  small enough to be invisible and is not an average of anything. `ATMO-F-014`, `ATMO-A-023`.

- **ATMO-R-020.** Coverage belongs to the **quantity**, not the file, and the type carries it
  per quantity along with its class. The quantities do not run out together: here Kp and ap are
  complete for 94 years while F10.7 has 459 holes in it.

### 4.5 The primary source is not frozen — the policy, and why it needs no exception

`GFZ-KP` moves in two ways a hash cannot distinguish — **extension**, and **revision** of rows
already present, which its own `D` flag records:

| `D` | meaning | rows | span |
|---|---|---|---|
| 2 | Kp and SN definitive | 34 424 | 1932-01-01 … 2026-03-31 |
| 1 | Kp definitive, SN preliminary | 153 | 2026-04-01 … 2026-08-31 |
| 0 | Kp and SN preliminary | 17 | 2026-09-01 … 2026-09-17 |

34 424 + 153 + 17 = **34 594**, the row count. It does **not** move in the third way CelesTrak
did: there are no forecast rows to mistake for observation.

The policy:

- **ATMO-R-025.** **Nothing is fetched at run time, ever.** The manifest pins each snapshot by
  SHA-256 exactly as EGM2008 and FES2004 are pinned (plan R11); there is no "latest" URL and the
  loader has no network path. The gate is reproducible by the same mechanism as every other
  gate, not by a special case.

- **ATMO-R-024.** **The snapshot is part of the answer.** Every result records the manifest id
  and SHA-256 it came from, and **two results from different snapshots cannot be compared
  silently** — not because they are far apart, but because revision means they may differ at an
  epoch both cover. This is a **refusal with a test**, `ATMO-F-013` and `ATMO-A-024`, not a
  sentence in a definitions section.

- **ATMO-R-022.** The `D` class of every row used is carried into the result, and a class value
  the loader does not recognise is **refused rather than defaulted** — `ATMO-F-005`.

- **ATMO-R-023.** A source carrying **forecast** rows may be loaded only through a path that
  labels them, and they are refused by default. `GFZ-KP` has none, so this guard cannot fire on
  the pinned data; plan §4 rule 5 requires it be fired anyway, and `ATMO-A-011` fires it against
  a constructed row rather than leaving it unproven.

- **ATMO-R-019.** The centred mean is **computed**, never read from a derived column. v1.0
  measured why on CelesTrak: its derived column was exact on 69 years of observation and wrong
  on every forecast row, worst 30.07 sfu. That source is gone, but the rule is not about that
  source — a derived column is someone else's arithmetic over someone else's window rule.

**So: pin the snapshot, name it in the result, and refuse across snapshots.** Updating is a
manifest change, and plan §5 constraint 9 already makes a manifest change re-run every gate that
consumed the entry. A moving input and a reproducible gate coexist by pinning and naming, not by
an exception for data that moves.

---

## 5. Interfaces, stated language-free

**Inputs.** An epoch; a geodetic position (latitude, longitude, altitude above the reference
ellipsoid, all from `SPEC-frames`); and a **space-weather sample** — not three loose numbers.
The sample is produced by the ingestion layer, carries its own provenance and data class, and
cannot be constructed from bare numbers without saying so through a separately named
constructor whose name records that the values did not come from a pinned file.

**Outputs.** A structure with each species named (§3.4), a temperature pair, and the provenance
of the evaluation: which snapshot, which data classes, whether the sub-80 km substitution of
`ATMO-R-013` was applied, and whether the year was discarded.

**Two entry points, two types.** `GTD7`'s total density and `GTD7D`'s are different quantities
(§3.4) and therefore different return types. A drag model at L4 accepts only the second. There
is no boolean.

**Two Ap conventions, two types** (§3.7). There is no switch argument and no global state.

**No global state at all** — no `METERS` flag, no `TSELEC` array, no saved `ALAST`. `MSIS-FOR`
caches across calls through `SAVE`d locals; a port that reproduces that reproduces a data race.
The port is re-entrant and its results do not depend on call order. `ATMO-A-015`.

---

## 6. Precision and accuracy requirements

**`ATMO-P-1`, `ATMO-P-2`** are stated in §3.2 and §3.3 and are requirements here, not
observations.

- **ATMO-P-3.** **The gate's tolerance, derived from `ATMO-P-1` and nowhere else.** On class-A
  comparisons the port agrees with the promoted-double build to **10⁻¹²** relative, and with the
  single-precision build to **10⁻⁵** — looser than class A's measured worst of 7.6706 × 10⁻⁶ by a
  factor of 1.3. Classes B, C and Z are **counted, not toleranced**: their counts are asserted
  exactly (32, 18, 212) and their values are not compared relatively at all, because a relative
  comparison against a number the reference computed at 10⁻³⁷ compares rounding noise.

  **The tolerance against the single build is a property of the reference, not of the port**, and
  tightening it would be a mistake. The derivation lives in the generated file's own header
  (`ATMO-R-028`) so that it is read next to the thing that produced it.

- **ATMO-P-4.** **What the model's accuracy against reality is, in the units it is published
  in.** This quantity has now been stated three times in this specification and the first two
  were wrong in the same way: neither carried the step that produced it. Plan §4 rule 3 is
  exactly that, so the derivation is here in full and the number stops moving.

  **The source statistic.** `MSIS-STATS` Table 1 (total mass density), data spanning
  **1963–1997**, column `SD`, defined by `MSIS-README` as

  > `"SD" = sigma_rho = [<log_e**2{rho_i(data)/rho_i(model)}> - log_e**2(beta_rho+1)]**(1/2)`

  and confirmed by `PICONE02` §15, which describes *"histograms of residuals of **log_e r**,
  where r is the total mass density"*. **It is a natural logarithm.**

  **The conversion, stated because it is the whole difficulty.** A fractional 1-σ density error
  is **e^σ − 1**. A σ of 0.43 reads as 43 %, 54 % or 169 % depending on whether the statistic is
  a plain ratio, a natural log or a log₁₀ — and it is a natural log. Every number below names
  the row it came from.

  | | σ | e^σ − 1 | row |
  |---|---|---|---|
  | best | 0.07 | **7.3 %** | 1(a) quiet, SETA 79 accel, 120–200 km, 3 792 pts |
  | points-weighted, all levels | 0.172 | **18.8 %** | 1(c), 791 314 pts over 30 rows |
  | worst, quiet | 0.43 | **53.7 %** | 1(a), accel 200–400 km, 57 258 pts |
  | LEO storm, high altitude | 0.47 | **60.0 %** | 1(b) high, drag 400–800 km, 105 pts |
  | **worst anywhere** | **0.97** | **163.8 %** | 1(b) high, **AE-C MESA accel, 200–400 km, 1 653 pts** |

  v1.0 said "17–25 %", which lands near the points-weighted 18.8 % **by accident**, reading a
  log-σ as a plain fraction. v1.1 said "7 % to 60 %", which converted correctly but quoted two
  particular corners as though they bounded the table; the table reaches 163.8 %.

  **And σ is not the model's uncertainty — it is an upper bound on it.** `PICONE02` says so:
  *"Interpretation of the standard deviation for a single model can be somewhat ambiguous,
  however, because s also reflects noise in the data sets"*, and further that a model which
  *"faithfully covers scales of true geophysical variability which have been filtered from the
  data"* can show a **larger** σ than a worse one.

  That is checkable, and it was checked rather than taken on the paper's word. `MSIS-STATS`
  prints σ for **three** models side by side — NRLMSISE-00 (2002), MSISE-90 and Jacchia-70
  (1970). Over the **86** rows where all three parse:

  * correlation of σ across models: **0.9945** (N00 vs M90), **0.9664** (N00 vs J70);
  * median |σ_N00 − σ_M90| = **0.010**, against a median σ of **0.185** — about **5 %**;
  * at the six rows with σ ≥ 0.40, the spread across all three models is **0.00 to 0.03**, and
    at the worst row of all N00 gives 0.97, M90 0.96 and J70 0.97.

  **What that measurement reaches, and what it does not.** It rules out a σ that is mostly
  *model-specific*: three models fitted decades apart do not produce σ agreeing to 5 % of its
  value if each is dominated by its own error. It does **not** establish that σ is
  data-dominated. Writing σ² = σ_data² + σ_model², the agreement says the three σ_model terms
  are **similar**, not that they are **small** — and three models fitted to overlapping
  databases can share a deficiency as easily as they can share the data's noise. The
  correlation cannot separate those, and this specification does not claim it can.

  The paper's own hedge is therefore the claim that stands, and it is the safe one:
  `ATMO-A-026`'s `l2_floors` row reports σ as *model-minus-data scatter including data noise,
  an upper bound on model error* — never as "the atmosphere is 19 % uncertain". The row exists
  to let L4 rank drag against terms computed to parts in 10¹², and that is precisely the
  comparison an overstatement would distort.

- **ATMO-P-5.** **The observed/adjusted flux difference**, for `ATMO-A-007`: (*r*/1 AU)² over a
year runs 0.967 … 1.034, so the two columns differ by up to 3.4 % and by 6.9 % peak to peak.

---

## 7. Failure behaviour

Every refusal names the request and the limit, and is a `Result` diagnostic — never an
exception, never a sentinel density, never a silently clamped input.

| id | when | what it names |
|---|---|---|
| `ATMO-F-001` | altitude below 0 km | the altitude requested, the limit, and that `ZN3`'s lowest node is 0 km so the model has no fit below it |
| `ATMO-F-002` | day of year outside 1 … 366 | the value and the range, citing `MSIS-FOR`'s header |
| `ATMO-F-003` | a mass selector outside the ten documented | the value and the accepted set |
| `ATMO-F-004` | epoch outside usable coverage | the epoch, **which quantity** ran out first, that quantity's own last epoch, and the 40-day shrink that produced the limit — not the file's span |
| `ATMO-F-005` | a predicted row required, without explicit opt-in | the epoch, its data class, and the last `OBS` epoch |
| `ATMO-F-006` | F10.7 ≤ 0 or Ap < 0 | the value |
| `ATMO-F-007` | a file whose hash is not the manifest's | both hashes and the manifest id |
| `ATMO-F-008` | an Ap convention mismatch (§3.7) | which convention was supplied and which the call needs |
| `ATMO-F-009` | below 80 km with flux or Ap other than 150/150/4 | what was supplied, what was substituted, and `MSIS-FOR`'s own sentence |
| `ATMO-F-010` | mass selector 49 | that the reference accepts it and **adds 2 × D(4), counting O₂ as two oxygen atoms, at line 769, described in no comment** — so the knowledge lives in the tree rather than only in a report — and that this module refuses it rather than inheriting undocumented behaviour |
| `ATMO-F-011` | a space-weather sample built from bare numbers used where a pinned one is required | that the sample has no provenance |
| `ATMO-F-012` | any of `MSIS-FOR`'s three print-and-continue conditions (`ATMO-R-030`) | which condition, its operands, and that the reference would have returned a number here |
| `ATMO-F-013` | two results from different space-weather snapshots compared | both manifest ids and hashes, and that revision means they may differ at an epoch both cover |
| `ATMO-F-014` | a required window contains a missing-data sentinel | the epoch, the sentinel's own date, and the window that needed it — never the mean of the rest |
| `ATMO-F-015` | verified inputs required, epoch outside the verified window | the epoch, the verified window `2004-10-28 … 2026-09-17`, and the issuing authority the window comes from |
| `ATMO-F-016` | a column read that the manifest entry does not declare | the column, and the entry's declared column list |
| `ATMO-F-017` | a **number** density requested for a species the model does not resolve at that state | the species, the altitude, the mass contribution, and that the reference returns a hard zero here rather than a small number |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `ATMO-A-001` | **THE GATE.** The port against `MSIS-FOR`, compiled from the pinned source and executed, over **the 17 published input cases** (`MSIS-FOR` lines 2438–2552: 15 from the `DO` loop + 2 with the 7-element Ap and `SW(9) = −1`) **and** a domain-spanning sweep that exercises every spline regime of §3.6, both `GTD7` and `GTD7D`. Reports its counts, not its verdict — §8's denominator paragraph | agreement | the reference implementation, which **is** the definition (§3.1) | `ATMO-P-3` | R-001, R-002, R-003, R-004 |
| `ATMO-A-002` | **the reference's own precision, remeasured rather than trusted to this document**: the single build against the `-freal-4-real-8` build of the same source, over the 17 published cases **and the branch-crossing sweep**, classified by materiality | **1500 comparisons: A 1238 (median 2.8108 × 10⁻⁷, worst 7.6706 × 10⁻⁶ — argon, 1000 km, published case 3), B 32 (worst 7.8739 × 10⁻³), C 18, Z 212.** The worst class-A comparison is the same one the 17 cases alone give: the sweep adds 1056 material comparisons and does **not** loosen the bound | measurement, `ATMO-P-1` | the counts exactly; the relatives to 2 significant figures so a compiler change is visible | P-1 |
| `ATMO-A-003` | **the three regimes, counted and not absorbed**: the test asserts each class's size exactly, and that **every** class-C member is anomalous oxygen at or below 120 km — the species and the altitude bound, not merely the count, because a count alone survives the class silently acquiring a member that mattered | **A 1238, B 32, C 18, Z 212, summing to 1500 = 125 × 12**; every class-C member anomalous oxygen, ≤ 120 km | measurement, `ATMO-P-2` | exact on every count | P-2 |
| `ATMO-A-004` | **the total density against the species sum, by a route that does not touch the reference**: ρ = 1.66 × 10⁻²⁴ (4·He + 16·O + 28·N₂ + 32·O₂ + 40·Ar + H + 14·N), and for `GTD7D` the same plus 16·anomalous O | **measured during drafting: worst 3.280 × 10⁻¹⁶ (`GTD7`) and 3.871 × 10⁻¹⁶ (`GTD7D`) in double; 8.326 × 10⁻⁸ and 1.130 × 10⁻⁷ in single** | `MSIS-FOR`'s header, **a published statement about the model** | 10⁻¹⁵ | R-014, R-004 |
| `ATMO-A-005` | **the documented zeros**: O, H, N and anomalous O are exactly zero below 72.5 km, over the five published sub-72.5 km cases | 20 values, all exactly 0.0 | `MSIS-FOR` header, *"O, H, and N are set to zero below 72.5 km"* | exact | R-007 |
| `ATMO-A-006` | `GTD7` and `GTD7D` differ by **exactly** 16 × 1.66 × 10⁻²⁴ × (anomalous O), and by nothing else; and that the L4-facing type is the second | as stated | `MSIS-FOR` header | 10⁻¹⁵ | R-004 |
| `ATMO-A-007` | **observed against adjusted flux**: the density computed from `F10.7_ADJ` rather than `F10.7_OBS` at perihelion and aphelion, so that taking the wrong column is *measured* rather than asserted to matter | the two fluxes differ by up to 3.4 %; the test reports the resulting density difference rather than predicting it | `MSIS-FOR` header + `CELESTRAK-SW` columns 25 and 26 | — | R-016, P-5 |
| `ATMO-A-008` | **the centred 81-day mean recomputed against the file's own column**, over every row with a full window. **Asserts where the disagreements are, not that there are none** | **measured: 25 333 rows checked, 176 disagree by more than 0.05 sfu (0.69 %), of which `PRD` 38 and `PRM` 138 and `OBS`/`INT` zero; worst 30.07 sfu at 2035-01-01** | `CELESTRAK-SW` against `ATMO-R-018`'s formula | 0.05 sfu, and the class counts exactly | R-018, R-019, R-022 |
| `ATMO-A-009` | **per-quantity coverage**: that F10.7 and Ap have different last epochs in one file, that the usable end is the earlier, and that it is shrunk by 40 days | **F10.7 to 2041-10-01, Ap to 2026-11-01 — 15 years apart**; usable end 2026-09-22 | `CELESTRAK-SW`, the 2026-09-18 snapshot | exact on the dates | R-020, R-021 |
| `ATMO-A-010` | **the four data classes partition the file**: `OBS` 25 129 + `INT` 60 + `PRD` 45 + `PRM` 179 = 25 413 = the row count. A class the loader does not recognise fails the sum and is not swept into a default | the sum | `CELESTRAK-SW` column 27 | exact | R-022, R-023 |
| `ATMO-A-011` | refusals, each fired: `ATMO-F-001`, `-F-002`, `-F-003`, `-F-004`, `-F-005`, `-F-006`, `-F-007`, `-F-008`, `-F-009`, `-F-010`, `-F-011`, each naming what §7 requires. **Each is proven both ways** — fired, and shown not to fire on the adjacent accepted input (plan §4 rule 5) | the diagnostics | this spec | — | F-001…F-011, R-012, R-015 |
| `ATMO-A-012` | **the Ap convention**: the 7-element array reaches the model only through the type that means it, and supplying seven values through the daily type is a compile failure rather than six ignored numbers. Cases 16 and 17 are the published cases that exercise it | as stated | `MSIS-FOR` §3.7, cases 16–17 | — | R-009, F-008 |
| `ATMO-A-013` | **the sub-80 km substitution**: at 60 km the result is unchanged by the caller's flux and Ap, and the result records that the substitution happened | identical densities | `MSIS-FOR` header | exact | R-013, F-009 |
| `ATMO-A-014` | every file came from the manifest cache and its hash was verified before this module saw a path; **and the snapshot is carried into the answer** — the result's provenance names the manifest id and SHA-256 actually used, and a second snapshot differing in one revised row produces a result that differs in that record, so two results from different snapshots are **distinguishable rather than merely documented as incomparable** | as stated, and the two records differ | plan R11, `ATMO-R-024` | exact on the recorded hash | R-024, R-026, F-007 |
| `ATMO-A-015` | **structural**: no global state — the same call in either order gives the same answer, concurrent evaluation agrees with serial, and there is no `METERS`/`TSELEC` flag; **local solar time is not a free parameter** — the ordinary entry point derives it, and the independent form is reachable only through the separately named one; **the loader has no network path at all**, which is what makes `ATMO-R-025` a property of the build rather than a promise; a `GTD7` density cannot be passed where a `GTD7D` one is required; a bare-number sample cannot reach a pinned-sample call site | compile failures, and the order-independence check | this spec, plan §5 | exact | R-005, R-006, R-010, R-025, R-027, F-011 |
| `ATMO-A-016` | **the oracle's absence is reported, not passed over**: where no Fortran compiler is configured, `ATMO-A-001` reports the count of cases it did **not** run alongside the count it did, and the suite fails if the skipped count is unacknowledged | 17 skipped, 0 run, and an explicit acknowledgement | plan §4 rule 3 | exact | R-027, R-028 |
| `ATMO-A-027` | **an unresolved species is a refusal, not a zero**: anomalous oxygen at 110 km, where the reference underflows to exactly 0, returns `ATMO-F-017` from the number-density accessor **and** a usable mass contribution from the drag accessor. **Proven both ways** — a resolved species returns its number density normally, so the refusal is not simply always on | the refusal, and a normal return at 400 km | this spec, `ATMO-R-036`, and the 18 class-C members of `ATMO-P-1` | exact | R-036, F-017 |
| `ATMO-A-026` | **the atmosphere's row in `tests/l2_floors.cpp`**: the drag-relevant density and the acceleration it implies, at **both** of the table's radii, each carrying `ATMO-P-4`'s uncertainty **with the altitude, activity level and epoch it was measured at**. The test asserts the magnitude *ratio* between the two radii, because that ratio is the fact L4 needs and a single point cannot carry it | measured, and reported against the layer's existing floor of 8.552 × 10⁻¹¹ m s⁻² and smallest kept term of 3.478 × 10⁻¹¹ | this spec, plan §4 rule 3 | the measurement is the value | R-035, P-4 |
| `ATMO-A-020` | **the daily-flux tie-break, on the cases that distinguish it**: `GFZ-KP`'s F10.7obs against `DRAO-FLUX`'s first 20:00 UT reading over the whole overlap, and against last-wins. **The 16 duplicate dates are asserted by name**, not merely counted, because they are the only places the two rules differ | **7 969 / 7 969 exact with first-wins; 7 953 with last-wins, the 16 differences being exactly the 16 duplicate dates; worst 2.9 sfu on 2022-10-23.** And 14 dates carry no 20:00 reading and are absent, not substituted | `GFZ-KP` against `DRAO-FLUX` — **two issuing authorities, independent routes** | exact | R-033, R-016 |
| `ATMO-A-021` | **the verification flag is load-bearing**: a pre-2004 epoch's result carries "not verified" **and a post-2004 epoch's result does not**; and a consumer requiring verified inputs gets `ATMO-F-015` for the first and a value for the second. A flag that is always set is the same as no flag, so both directions are asserted | as stated | this spec, `ATMO-R-031` | exact | R-031, F-015 |
| `ATMO-A-022` | **the declared-column rule fires**: reading `SN` — present in the file, not in the manifest entry's declared columns — is `ATMO-F-016`, and every column this module does read is declared. Proven both ways | the refusal, and a complete declared list | this spec, plan R11 | exact | R-032, F-016 |
| `ATMO-A-023` | **usable coverage is a set**: the count of epochs with a complete centred 81-day window, the span, and **the number of breaks inside it** — an interval would accept an epoch in a hole | **22 991 days, 1956-10-14 … 2026-08-08, with 25 breaks**; and 6 178 sentinel rows = 5 523 leading + 655 interior in 459 runs, last gap 2026-05-09; Kp and ap have 0 missing in 34 594 | `GFZ-KP`, the 2026-09-18 snapshot | exact on every count | R-021, R-034, R-020, F-014 |
| `ATMO-A-024` | **two snapshots cannot be compared silently**: results from snapshot A and snapshot B refuse comparison with `ATMO-F-013`, and results from the same snapshot compare normally. Proven both ways | the refusal, and a successful comparison | this spec, `ATMO-R-024` | exact | R-024, F-013 |
| `ATMO-A-025` | **the `D` class partitions the file**: 2 → 34 424, 1 → 153, 0 → 17, summing to 34 594 = the row count; and an unrecognised class value is refused rather than defaulted | the sum, and the refusal | `GFZ-KP` column `D` | exact | R-022, F-005 |
| `ATMO-A-019` | **the three print-and-continue conditions become refusals**: each of `MSIS-FOR`'s coincident-node, non-positive-log and non-convergence conditions is driven to fire through the module's own internal path, and each returns `ATMO-F-012` rather than a number. **Proven both ways** — fired, and shown not to fire anywhere across `ATMO-A-001`'s sweep | the refusal, and zero occurrences over the sweep | `MSIS-FOR` lines 445, 1539, 1591 | exact | R-030, F-012 |
| `ATMO-A-018` | **F10.7 is the previous day's**: for an epoch on day *D*, the value the sample carries is the file's row for *D* − 1, asserted against the file directly; and substituting day *D*'s value changes the density measurably, so the off-by-one is **detectable** rather than merely forbidden | the file's own *D* − 1 row, and a measured density difference | `MSIS-FOR` header + `CELESTRAK-SW` | exact on the flux, reported on the density | R-017 |
| `ATMO-A-017` | the year is discarded by the model: two epochs differing only in year give identical densities, which is `MSIS-FOR`'s documented behaviour and a trap for anyone assuming a solar cycle is modelled | identical | `MSIS-FOR` header | exact | R-011 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `ATMO-R-029` | A documentation obligation, not a behaviour: `PROVENANCE.md` must record §0's search as a finding of absence. Discharged by §9 and by the entry itself, which a reviewer can read. It is listed here rather than silently omitted because the alternative — a test that greps `PROVENANCE.md` for a phrase — would pass on the phrase and not on the search. |

### What `ATMO-A-001` can fail on, and what it cannot

The gate is an oracle comparison, and plan §4 rule 2 says an oracle comparison is never a gate
on its own. That rule is not being waived here; it is being read against a source it was not
written for, so the reading is set out rather than assumed.

Rule 2's oracle is **an independent implementation of a published theory** — ERFA, CALCEPH. Its
weakness is that it can be wrong about the theory in the same way you are, or in a different
way, and the comparison cannot tell you which; so the published value, not the oracle, is the
gate. **NRLMSISE-00 has no published theory to implement independently and no published value**
(§0, §3.1). `MSIS-FOR` is therefore not *an* implementation of the model — it *is* the model, in
the sense that EGM2008's coefficient file is the field. Comparing against it is comparing
against the definition.

That reading is only honest if what it costs is stated too:

**`ATMO-A-001` can fail on** any porting error whatever — a mistranscribed coefficient, a wrong
spline node, a branch boundary off by a kilometre, an inverted switch, a species index, a unit,
a recursion written from the wrong end. That is the whole class of errors this step can
plausibly introduce, and the gate catches all of it.

**`ATMO-A-001` cannot fail on**:

* an error **in `MSIS-FOR`**. Reproduced faithfully, invisibly, and in company with every other
  user of the model;
* a **divergence between `MSIS-FOR` and `PICONE02`**. Nothing in this tree compares the code to
  the paper, and nothing can, short of refitting the model;
* anything about whether **NRLMSISE-00 is a good model of the atmosphere**. `ATMO-P-4` is what
  is known about that, and it comes from `MSIS-STATS`, which this tree cannot reproduce;
* **anything in §§4.3–4.5 at all.** This is the one that would otherwise be assumed, so it is
  stated: the 17 published cases supply F10.7, F10.7A and Ap **as literal constants in the
  driver's `DATA` statements**. They never touch a file, a column, a date, a coverage window or
  a data class. The model gate therefore exercises **none** of the space-weather ingestion, and
  the ingestion's gates are `ATMO-A-007` through `-A-010`, which are independent of it and of
  each other. A reader who took `ATMO-A-001` passing as evidence that the space-weather layer
  works would be making exactly the `PERT-A-001` mistake this tree has already made once.

### The gate's denominator

`ATMO-A-001` reports, and fails if it cannot report, **five counts**:

1. cases run, against **17** — the published set, 15 + 2, asserted as a number and not as a
   loop that might run zero times (`EPH-A-007`);
2. sweep points run, against the sweep's own declared size;
3. quantities compared per case, against **12**;
4. comparisons in the **underflow class** (§3.3), separately, with their locations — expected
   **2** in the published set;
5. comparisons with a **zero denominator** (§3.4's documented zeros), separately — expected
   **20** in the published set.

Counts 4 and 5 are the point. A single pass/fail over 204 comparisons with one tolerance would
either fail on two anomalous-oxygen values that a correct port is right to produce, or pass with
a tolerance so loose it stops checking 100 km. Neither is a gate. Printing the classes and their
sizes is what makes it one — and it is the same defence §0's finding required in a different
place: *make the tool print its components rather than its verdict.*

---

## 9. Provenance obligations

- **ATMO-R-026.** `MSIS-FOR`, `MSIS-STATS`, `MSIS-README`, `MSIS-TABLES`, `PICONE02` and the
  `CELESTRAK-SW` snapshot each get a manifest entry with URL, byte count and SHA-256. Changing
  any of them re-runs every gate that consumed it (plan §5 constraint 9), which for
  `CELESTRAK-SW` is the whole of §4.3–4.5 and is the mechanism §4.5 relies on.

- **ATMO-R-028.** **The oracle's output is frozen as generated source, with the recipe that
  produced it**, exactly as `solid_tide_tables.hpp` records its own extraction. The gate runs
  **offline from the cache and needs no Fortran compiler at all**, which is what gate 4 requires.
  `gfortran` is therefore a **declared host tool** alongside `python3` and `cmake`, not a
  hash-pinned manifest entry: pinning a compiler means vendoring one or declaring a host version
  that is not a pin, and neither is worth the pretence. Where a compiler *is* present the file is
  **regenerated and compared against the committed copy**, so the frozen artefact is itself under
  test rather than trusted.

  The header records the single-versus-double measurement **as the derivation of the tolerance**,
  so that `ATMO-P-3`'s numbers are read next to the thing that produced them rather than looked
  up; and the two anomalous-oxygen cases stay **their own counted class** rather than being
  absorbed into a widened tolerance (§3.3).

That file is **this tree's artefact, not a published one**, and must be labelled as such
wherever it appears. It records:

* the source: `NRLMSISE-00.FOR`, SHA-256 `cce0420e…44cb`, unedited;
* the compiler: `gfortran-16` 16.0.1 20260322 (`GNU Fortran (Ubuntu 16-20260322-1ubuntu1)`);
* the flags: `-std=legacy -fdec-char-conversions -O0`, and for the promoted build additionally
  `-freal-4-real-8`;
* the driver: this tree's, replicating the published `DATA` statements exactly and differing
  from `MSIS-FOR`'s only in output `FORMAT` — because the published driver prints `1P8E9.2`,
  **three significant figures**, and its summary table `1PE12.3`, four, neither of which is
  enough to gate a port against a model defined to six (§3.2).

Three points of that recipe are requirements rather than incidentals:

* **`-O0` is deliberate.** Optimisation may reassociate single-precision arithmetic and move the
  reference's own value by more than `ATMO-P-1`'s median. The oracle is built unoptimised so
  that what it produces is the source's arithmetic and not the compiler's.
* **`-fdec-char-conversions` is why `MSIS-FOR` is not edited.** Modern gfortran rejects three
  `DATA` statements at lines 1670–1671 which initialise the `INTEGER` arrays `ISDATE`, `ISTIME`
  and `NAME` with `CHARACTER(4)` literals — a FORTRAN 66 Hollerith idiom. Those three lines
  carry the model's own output header stamp, `MSISE-00  01-FEB-02  15:49:27`, and touch no
  arithmetic whatever. A compiler flag accepts them; editing the reference to satisfy a compiler
  would have compromised the only thing this step has to check against, so it was not done and
  must not be.
* **The reference is compiled from the pinned bytes**, not from a copy with line endings or
  tabs normalised.

- **ATMO-R-029.** `PROVENANCE.md` records §0's search — the five files, their hashes, the
  counts, and the conclusion that no published reference value exists — as a **finding of
  absence**, in the form plan §4 rule 4 requires. It is the fourth entry of that kind in this
  tree and the first where the absence is of the gate itself.

---

## 10. Questions, and the rulings on them

| id | question | state |
|---|---|---|
| `ATMO-Q-001` | §0.5's three plan rewordings — the step-4 gate, the L2 exit gate and D2 all named published values that do not exist. | **RULED 2026-09-18, all three reworded and pushed (72c68ca), and the general form adopted as plan §4 rule 6** — settled as a *distinction* rather than an exception, because an exception invites a second one. A reference implementation is an oracle when a specification exists that it implements and is itself normative when none does, and which it is, is a search. Three obligations come with claiming it: record the search, state the cost where a reader meets it, and find a second axis if one exists. It binds L4's drag coefficient and L9's ray tracing. |
| `ATMO-Q-002` | Mass selector 49: accepted by the reference, undocumented, not the same as 48. | **RULED: refuse it, and say in the refusal what the reference does.** Constraint 4 is refuse rather than approximate, and implementing undocumented behaviour because the reference tolerates it is how one inherits somebody else's accident with no way to tell later whether it was one. None of the 17 cases uses it, so refusing costs nothing at the gate. `ATMO-F-010` carries *"adds 2 × D(4), counting O₂ as two oxygen atoms, line 769, described in no comment"* so the knowledge is in the tree. |
| `ATMO-Q-003` | `CELESTRAK-SW` is a redistributor, not the issuing authority. | **RULED: go to the issuing authorities — DRAO for F10.7, GFZ for Kp and Ap — and drop CelesTrak entirely**, not keep it alongside. Its one contribution was derived columns `ATMO-R-019` recomputes anyway, and `ATMO-A-008` measured that column wrong wherever it is predicted: a redistributor whose derived column is wrong there adds error rather than value. §4.3a reports the cost rather than absorbing it: `DRAO-FLUX` begins 2004-10-28, so the cross-check covers 22 years and the earlier F10.7 is `GFZ-KP`'s redistribution — **verified over the overlap at 7 969 / 7 969 exact**. Pre-2004 is **served with the provenance marked** (`ATMO-R-031`), not refused, and the flag is load-bearing. |
| `ATMO-Q-004` | An atmosphere row in `tests/l2_floors.cpp`. | **RULED: yes, and it is the most informative row in the table** — a term 17–25 % uncertain beside a de Sitter term computed to parts in 10¹² is exactly the comparison L4 needs to decide what is worth modelling. Two conditions: carry the **uncertainty as well as the magnitude**, and measure at **both radii**. `ATMO-R-035`, `ATMO-A-026`. A third followed from checking the figure: the uncertainty must name the altitude, activity level and epoch it was measured at, and doing so showed v1.0's "17–25 %" understated the storm-time high-altitude case by about three (`ATMO-P-4`). |
| `ATMO-Q-005` | Pin the Fortran compiler in the manifest? | **RULED: no — freeze the outputs instead.** Generate the reference output once and commit it as generated source carrying the compiler, its version, the flags and the source hash, as `solid_tide_tables.hpp` records its extraction. The gate then runs offline from the cache with no Fortran compiler, which is what gate 4 requires, and `gfortran` is a declared host tool like `python3` and `cmake`. Pinning a compiler means vendoring one or declaring a host version that is not a pin. `ATMO-R-028`. |
| `ATMO-Q-006` | DTM-2013 and JB2008. | **CARRIED, not opened** — a question for whoever needs them. |
| `ATMO-Q-007` | **Open.** The pre-2004 F10.7 window is unverifiable because `DRAO-FLUX` begins 2004-10-28, and the search for a longer series was bounded (§4.3a) rather than exhaustive: six locators and one catalogue query, recorded with their results. If NRCan does publish the 1947-onward daily series somewhere not probed, the cross-check extends to 1947 and `ATMO-R-031`'s unverified class empties. Worth one more search by whoever next has reason to look; not worth a hunt now. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.1 | 2026-09-18 | **Adopted, with rulings on all six questions folded in.** (1) Plan §4 rule 6 now carries the oracle/normative distinction; §3.1 points at it and §8 discharges its three obligations. (2) Mass 49 is refused, the refusal naming what the reference does with it. (3) **CelesTrak is dropped for the two issuing authorities**, and the redistribution is *measured* rather than trusted: `GFZ-KP`'s F10.7obs equals `DRAO-FLUX`'s first 20:00 UT reading on **7 969 of 7 969** overlapping days, with the 16 duplicate-timestamp dates identified as the only cases distinguishing first-wins from last-wins. (4) §4.4 is rewritten: **usable coverage is a set, not an interval** — v1.0's [first + 40, last − 40] was wrong, because the F10.7 column has 655 interior gaps in 459 runs and 25 breaks inside the usable span. (5) `ATMO-P-4` is corrected: "17–25 %" was an aggregate quoted as a range and understates the storm-time high-altitude case by about three; the figure now carries its altitude, activity level and epoch. (6) The snapshot-comparability rule became `ATMO-F-013` with a test rather than a sentence, the verification flag became load-bearing with both directions asserted, and the per-column licence became a declared-columns rule the loader enforces. |
| 1.0 | 2026-09-18 | First draft, for review. Leads with §0's rule-4 finding: the NRL distribution publishes **no reference profile, no reference value and no expected output**, with the search over all five distributed files that established it, and the three plan sentences that need rewording as a result. The reference's single precision is measured rather than assumed (`ATMO-P-1`), as is the anomalous-oxygen underflow that makes a relative-difference gate undefined at 100 km (`ATMO-P-2`). The space-weather policy is stated in §4.5 against a measurement of how the file actually moves — extension, revision **and** fifteen years of embedded forecast, with the file's own centred column disagreeing with its definition on 176 rows, all of them predicted. |
