# SPEC-atmosphere — NRLMSISE-00 and the space-weather inputs it runs on

| | |
|---|---|
| **Spec ID** | `ATMO` |
| **Status** | **draft** 2026-09-18, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-18 |
| **Layer** | L2 `environment`, step 4 (`doc/REWRITE_PLAN.md` §3.3) |
| **Depends on** | `SPEC-time.md` (the epoch and the day-of-year), `SPEC-frames.md` (geodetic latitude, altitude above the ellipsoid), `SPEC-gravity.md` (the reference ellipsoid), `core` |
| **Depended on by** | the drag force (L4, plan §3.5 step 3), the density campaign (L8 step 5) |

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

Three sentences of the plan are wrong about their source and want rewording. This specification
proposes the replacements and §10 `ATMO-Q-001` puts them to the manager, since a plan change is
the manager's and not this session's.

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
  co-rotating atmosphere. That is L4, plan §3.5 step 3, and it consumes this module.
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
| `CELESTRAK-SW` | CelesTrak (T. S. Kelso) | `SW-All.csv` — daily Kp/Ap, F10.7 observed and adjusted, 81-day centred and trailing means, with a per-row data-type flag | continuously updated; snapshot of 2026-09-18 covers 1957-10-01 … 2041-10-01, 25 413 rows | `https://celestrak.org/SpaceData/SW-All.csv`, retrieved 2026-09-18, 2 887 903 bytes | primary | normative (dataset) — **and the only non-frozen input in the tree, §4.5** |
| `NOAA-FLUX` | NOAA NGDC | the 10.7 cm flux archive named in `MSIS-FOR`'s own header comment | — | `ftp://ftp.ngdc.noaa.gov/STP/SOLAR_DATA/SOLAR_RADIO/FLUX/` | **not obtained** — the FTP host named in a 2002 comment is not reachable | informative |
| `DRAO` | Natural Resources Canada / DRAO | the Penticton 10.7 cm flux, the measurement `CELESTRAK-SW` redistributes | — | — | **not obtained** | informative |

**On `NOAA-FLUX`, said here rather than left to §10.** `MSIS-FOR`'s header names an FTP path as
the place to obtain both the observed and the 1-AU-adjusted flux. That host does not answer in
2026. Nothing in §§4–8 depends on it *as a document*: what it was cited for is the distinction
between the two flux classes, and that distinction is stated in `MSIS-FOR`'s own header text,
which is pinned. `CELESTRAK-SW` supplies both classes in named columns. The dead locator is
recorded because a reader checking this specification's citation would otherwise find a dead
link and not know whether it had ever been checked.

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
  build and the promoted-double build of the same source, over 17 cases × 12 quantities = 204
  comparisons, of which **20 have a zero denominator** (§3.4's documented zeros) leaving **184**:

| | |
|---|---|
| median | 3.816 × 10⁻⁷ ( ≈ 6 × single-precision ε) |
| worst, excluding the underflow class below | 7.671 × 10⁻⁶ — case 3, argon, 1000 km |
| exceeding 10⁻⁵ | 2, and both are the underflow class |
| single-precision ε | 5.960 × 10⁻⁸ |

The worst cases are the **heavy minor species at high altitude**, where diffusive equilibrium
carries a large exponential and single-precision rounding in its argument is amplified: argon at
1000 km moves by 7.7 × 10⁻⁶ between the two builds of one source. The model's own value is
therefore defined to about **six significant figures at 400 km and five at 1000 km**, and no
tolerance in §8 may be tighter than that.

### 3.3 The underflow class, which is not a tolerance question

- **ATMO-P-2.** Two of the 184 comparisons differ by exactly 1.000: anomalous oxygen at 100 km,
  in case 4 and in case 17. The single build returns **exactly zero**; the promoted-double build
  returns **2.820 × 10⁻⁴²** and **2.415 × 10⁻⁴²** cm⁻³. Single precision's smallest normal is
  1.175 × 10⁻³⁸, so the intermediate underflows to zero before a result can be formed, while in
  double it survives.

A double-precision port therefore **disagrees with the reference by 100 % at two of the
seventeen published cases**, and is right to. A relative-difference gate is undefined where the
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

### 4.3 The space-weather inputs, and three traps in them

The model takes three numbers. Each has a trap, and each trap is silent.

- **ATMO-R-016.** **Observed, not adjusted.** `MSIS-FOR`'s header: *"F107 and F107A values used to
  generate the model correspond to the 10.7 cm radio flux **at the actual distance of the Earth
  from the Sun** rather than the radio flux at 1 AU."* `CELESTRAK-SW` publishes both, in
  `F10.7_OBS` and `F10.7_ADJ`, adjacent columns 25 and 26. The two differ by (*r*/1 AU)², which
  runs from 0.967 at perihelion to 1.034 at aphelion — **up to 3.4 %, about 6.9 % peak to peak
  over a year**, with an annual period that a drag analysis would happily absorb into a fitted
  ballistic coefficient. This module reads the **observed** columns. `ATMO-A-007`.

- **ATMO-R-017.** **F10.7 is the previous day's value.** `MSIS-FOR`'s header: *"F107 - DAILY F10.7
  FLUX FOR PREVIOUS DAY."* Not the day of evaluation. An off-by-one day here is invisible in any
  single evaluation and systematic across a campaign.

- **ATMO-R-018.** **F10.7A is centred, not trailing.** *"81 day AVERAGE OF F10.7 FLUX (centered on
  day DDD)"*. `CELESTRAK-SW` publishes **both**: `F10.7_OBS_CENTER81` (column 28) and
  `F10.7_OBS_LAST81` (column 29). The trailing mean is the wrong one and is one column away.

### 4.4 Coverage is per quantity, and narrower than the file

A centred 81-day mean on day *D* needs days *D* − 40 … *D* + 40. So **the usable span is the
file's span shrunk by 40 days at each end**, and a file 81 days tall has exactly one usable day.

- **ATMO-R-021.** Usable coverage = [first + 40, last − 40], **intersected over every quantity
  the call needs**, because the quantities do not end together. Measured on the 2026-09-18
  snapshot of `CELESTRAK-SW`: 25 413 rows, 1957-10-01 … 2041-10-01, and

| quantity | last row carrying a value |
|---|---|
| `F10.7_OBS`, `F10.7_OBS_CENTER81` | 2041-10-01 |
| `AP_AVG`, `AP1` … `AP8` | **2026-11-01** |

F10.7 runs **fifteen years further than Ap in the same file**. A model call needs both, so the
usable end is Ap's, and a loader that reports "coverage 1957–2041" has told the caller something
true about the file and false about the model. Coverage belongs to the quantity, and therefore
to its type — plan §5 constraint 10, for the fourth time in this tree.

- **ATMO-R-020.** The type carries, per quantity, its first and last epoch **and** its data
  class (§4.5). A result carries the identity of the snapshot it was computed from.

### 4.5 The file is not frozen — the policy, and why it is this one

This is the first input in the tree that moves. The manager's framing was exact: *"'fetch the
latest' cannot coexist with a reproducible gate."* The answer is that **it is not one file that
moves in one way**, and once that is measured the policy follows.

`CELESTRAK-SW` changes in **three** distinct ways, and a hash distinguishes none of them:

1. **Extension.** New days are appended.
2. **Revision.** Days already present are rewritten — provisional Ap becomes definitive,
   interpolated flux is replaced by observation. A value for 2020 can differ between a 2021
   snapshot and this one.
3. **Prediction.** The file **already contains the future.** Its last row is 2041-10-01, fifteen
   years beyond today. Column 27, `F10.7_DATA_TYPE`, is the only thing distinguishing
   observation from forecast, and it takes four values:

   | class | rows | span |
   |---|---|---|
   | `OBS` observed | 25 129 | 1957-10-01 … 2026-09-17 |
   | `INT` interpolated | 60 | scattered, 1957-12-25 … 2026-05-09 |
   | `PRD` predicted, daily | 45 | 2026-09-18 … 2026-11-01 |
   | `PRM` predicted, monthly | 179 | 2026-12-01 … 2041-10-01 |

   25 129 + 60 + 45 + 179 = **25 413**, which is the row count.

**The third is the dangerous one, and it was measured rather than assumed.** Recomputing the
centred 81-day mean from `F10.7_OBS` — the mean over days *D* − 40 … *D* + 40, the formula
`ATMO-R-018` requires — and comparing it against the file's own `F10.7_OBS_CENTER81` on the same
row, over the **25 333** rows where a full 81-day window exists:

* **176 rows disagree by more than 0.05 sfu — 0.69 %.**
* **All 176 are predicted rows**: 38 `PRD`, 138 `PRM`. **Not one `OBS` or `INT` row disagrees.**
* Worst disagreement **30.07 sfu**, at 2035-01-01 — against an F10.7 range of roughly 65 to 300.

So the file's own centred column is exact on observation and arbitrary on forecast, the
monthly-granularity predictions being interpolated on a different rule. And the file publishes
`F10.7_OBS_CENTER81` on its **final row**, 2041-10-01, where a centred window would need 40 days
that do not exist in the file at all.

The policy, in four parts:

- **ATMO-R-025.** **Nothing is fetched at run time, ever.** The manifest pins a snapshot by
  SHA-256 exactly as EGM2008 and FES2004 are pinned (plan R11). There is no "latest" URL in the
  manifest and the loader has no network path. The gate is reproducible by the same mechanism as
  every other gate, not by a special case.
- **ATMO-R-024.** **The snapshot is part of the answer.** Every result records which snapshot it
  came from. Two results from different snapshots are **not comparable even at an epoch both
  cover**, because of revision. This is a stronger statement than "the file grew" and it is the
  one that matters for a campaign that spans a re-fetch.
- **ATMO-R-023.** **Predicted rows are refused by default.** Not silently used, not warned about.
  A caller who wants a forecast asks for one by name, receives it labelled, and the label
  survives into the result. `ATMO-F-005`.
- **ATMO-R-019.** **The centred mean is recomputed, not read.** The file's column is used only as
  a check on the recomputation, not as the source of the value — and the check is expected to
  disagree on predicted rows, which is why `ATMO-A-008` asserts *where* the disagreements are
  rather than that there are none. Reading the column would have been correct for 69 years of
  observation and wrong for the forecast, which is exactly the shape that survives review.

**So: carry the pinned copy, and be explicit that it is historical — where "explicit" means the
type says so and the refusal names it, not that a comment mentions it.** Updating is a manifest
change, and plan §5 constraint 9 already requires a manifest change to re-run every gate that
consumed the entry. Updating space weather is therefore not a quiet operation in this tree; it
is a gate re-run, by construction, and that is the property that lets a moving input coexist
with a reproducible gate.

- **ATMO-R-022.** The data class of every row used is carried through into the result. A density
  computed partly from `OBS` and partly from `PRD` rows says so.

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

- **ATMO-P-3.** **The gate's tolerance.** The port agrees with the promoted-double build of
  `MSIS-FOR` to **10⁻¹²** relative on every quantity above the underflow class, and with the
  single-precision build to **10⁻⁵**, which is looser than the 7.671 × 10⁻⁶ worst case of
  `ATMO-P-1` by a factor of 1.3. Both numbers are asserted; the first is the real check and the
  second exists so that a reader comparing this tree against any other user of `MSIS-FOR` has the
  figure. **The tolerance against the single build is a property of the reference, not of the
  port, and tightening it would be a mistake.**

- **ATMO-P-4.** **The model's accuracy against reality, which is not this tree's to improve.**
From `MSIS-STATS` with `MSIS-README`'s formulas: in the thermosphere the standard deviation of
log density between data and model is about **0.17 to 0.23**, i.e. roughly **17–25 %**, with
mean residuals of a few per cent. That is the accuracy of the atmosphere model a drag analysis
inherits. It is three to five orders of magnitude worse than anything else in L2, and L4's error
budget must be written with it in view rather than around it. It is quoted here with its source
and its formula — `"SD" = sigma_rho = [<log_e**2{rho_i(data)/rho_i(model)}> - log_e**2(beta_rho+1)]**(1/2)`,
`MSIS-README` — because plan §4 rule 3 requires a cited statistic to carry the formula it was
computed with, and because this one is easy to quote as "20 % accurate" without saying 20 % of
what, measured how.

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
| `ATMO-F-010` | mass selector 49 | that it is accepted by the reference, undocumented there, and excluded here — `ATMO-R-012` |
| `ATMO-F-011` | a space-weather sample built from bare numbers used where a pinned one is required | that the sample has no provenance |
| `ATMO-F-012` | any of `MSIS-FOR`'s three print-and-continue conditions (`ATMO-R-030`) | which condition, its operands, and that the reference would have returned a number here |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `ATMO-A-001` | **THE GATE.** The port against `MSIS-FOR`, compiled from the pinned source and executed, over **the 17 published input cases** (`MSIS-FOR` lines 2438–2552: 15 from the `DO` loop + 2 with the 7-element Ap and `SW(9) = −1`) **and** a domain-spanning sweep that exercises every spline regime of §3.6, both `GTD7` and `GTD7D`. Reports its counts, not its verdict — §8's denominator paragraph | agreement | the reference implementation, which **is** the definition (§3.1) | `ATMO-P-3` | R-001, R-002, R-003, R-004 |
| `ATMO-A-002` | **the reference's own precision, remeasured rather than trusted to this document**: the single build against the `-freal-4-real-8` build of the same source, over the same 17 cases | median 3.816 × 10⁻⁷, worst 7.671 × 10⁻⁶ (case 3, Ar, 1000 km), 184 comparisons of 204 with 20 zero denominators | measurement, `ATMO-P-1` | the measurement is the value; it is asserted to 2 significant figures so a compiler change is visible | P-1 |
| `ATMO-A-003` | **the underflow class, counted and not absorbed**: anomalous oxygen at 100 km, where the single build returns exactly 0 and double returns 2.820 × 10⁻⁴² and 2.415 × 10⁻⁴² cm⁻³. The test asserts that **exactly 2** of the 184 comparisons fall in this class, and that both are at 100 km | 2 | measurement, `ATMO-P-2` | exact on the count | P-2 |
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

- **ATMO-R-028.** **The oracle's output is frozen, with the recipe that produced it.** The gate
  must run where no Fortran compiler is present, so the reference's full-precision output over the
  17 published cases and the declared sweep is **committed as a data file**, and `ATMO-A-001`
  compares against that file always. Where a compiler *is* present the file is **regenerated and
  compared against the committed copy**, so the frozen artefact is itself under test rather than
  trusted.

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

## 10. Open questions for the manager

| id | question |
|---|---|
| `ATMO-Q-001` | **§0.5's three plan rewordings.** The step-4 gate, the L2 exit gate and D2 all name published values that do not exist. The proposed replacements are in §0.5. The exit-gate one generalises beyond this step and is the one worth ruling on carefully: L4's drag coefficient and L9's ray tracing are the same shape, and a rule that says *where no published value exists, record the search and name what stands in its place* would be better settled now than three layers later. |
| `ATMO-Q-002` | **Mass selector 49.** Accepted by the reference, behaves differently from 48 (it counts O₂ twice, as oxygen atoms), and documented in zero comment lines. This specification refuses it with a diagnostic that says *undocumented* rather than *invalid*. The alternative is to implement it and note that its meaning is inferred from the code. Ruling wanted, because it is the first place this tree implements behaviour that its source does not describe. |
| `ATMO-Q-003` | **`CELESTRAK-SW` is a redistributor, not the issuing authority.** The flux is DRAO's and the geomagnetic indices are GFZ Potsdam's; CelesTrak merges them, adds the centred means and the data-class flag, and publishes one convenient file. Convenience is why it was chosen. Against it: this tree has otherwise pinned primary sources throughout — IERS's own chapters, NAIF's own kernels. Is a redistributor acceptable for an input the tree cannot freeze anyway, or should §4.3 read the two primary series and compute the merge here? The second is more work and removes a dependency on a third party's interpolation rules — the same rules `ATMO-A-008` just measured disagreeing by 30 sfu on forecast rows. |
| `ATMO-Q-004` | **`ATMO-P-4` and L4's error budget.** The atmosphere is 17–25 % uncertain against data, against L2's other terms which are good to parts in 10⁹ or better. L4 will rank a dozen accelerations against each other; the drag term's *model* error swamps the ranking of everything below it. This is not a question about this step, but the number arrives at this step and plan §4 rule 3 now says quantities that must be compared are measured in one place — so it is put here rather than reconstructed at L4. Does `tests/l2_floors.cpp` acquire an atmosphere row? |
| `ATMO-Q-005` | **The Fortran toolchain.** `ATMO-R-028`'s frozen file means the routine gate needs no compiler, but the regeneration check does, and on this machine it is `gfortran-16` rather than `gfortran` — `gfortran` is not installed and `apt` reports no cached package. Should the compiler be a manifest entry with its version pinned, as `python3` and `cmake` already are? That would make `ATMO-A-002`'s measured figures reproducible rather than merely recorded. |
| `ATMO-Q-006` | **DTM-2013 and JB2008**, named in the plan's L2 source column and not taken at this step. Neither is needed for the MVP. Carried, not dropped. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.0 | 2026-09-18 | First draft, for review. Leads with §0's rule-4 finding: the NRL distribution publishes **no reference profile, no reference value and no expected output**, with the search over all five distributed files that established it, and the three plan sentences that need rewording as a result. The reference's single precision is measured rather than assumed (`ATMO-P-1`), as is the anomalous-oxygen underflow that makes a relative-difference gate undefined at 100 km (`ATMO-P-2`). The space-weather policy is stated in §4.5 against a measurement of how the file actually moves — extension, revision **and** fifteen years of embedded forecast, with the file's own centred column disagreeing with its definition on 176 rows, all of them predicted. |
