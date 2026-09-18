# SPEC-eop — Earth orientation parameters: products, parsing, interpolation, coverage

| | |
|---|---|
| **Spec ID** | `EOP` |
| **Status** | **adopted** 2026-09-18 — manager verdict from session `odl maintainer (Router+Executor)`. Version 1.1 records the decisions taken in that verdict. |
| **Version** | 1.4 |
| **Date** | 2026-09-18 |
| **Layer** | `time` / `io` boundary (`doc/REWRITE_PLAN.md` §2) |
| **Feature** | F3 (plan §3), F12's EOP readers |
| **Depends on** | `SPEC-time.md` |
| **Depended on by** | `SPEC-frames.md`, and through it everything above |

**Derivation declaration (plan R1, R2).** This specification was written from the documents
listed in §2 and from no implementation of this module. Specifically, no file under
`/home/rog/odl20lite` was opened, read, listed, searched, or otherwise inspected during its
preparation. Numeric acceptance targets carried from prior measurement campaigns are
behavioural observations against public data (plan R4) and are marked as such where they
appear.

---

## 1. Purpose and scope

This module owns **the measured orientation of the Earth**: the five quantities that the
frame transformation of `SPEC-frames.md` cannot compute from any model, and that must be
read from an IERS product — the pole coordinates *x*_p, *y*_p; the Earth rotation phase
ΔUT1 = UT1 − UTC; the length-of-day excess LOD; and the celestial pole offsets δ*X*, δ*Y*.

It also owns the three questions that surround those numbers and that are more dangerous than
the numbers themselves: **which product**, **how to get a value between the tabulated ones**,
and **what happens at the edge of the table**.

### In scope

- Parsing the IERS EOP 20 C04 series and the IERS Rapid Service `finals2000A` series.
- Splicing final, rapid and predicted values into one series with per-epoch quality.
- Interpolation to an arbitrary epoch.
- Restoring the sub-daily ocean-tide and libration signals that the published series have had
  removed.
- Unit normalisation, range validation, and the coverage policy.
- The provenance record that makes a run reproducible despite the products being mutable.

### Not in scope

| excluded | owned by |
|---|---|
| The leap-second table and ΔAT | `SPEC-time.md` |
| Applying the parameters to rotate a vector | `SPEC-frames.md` |
| Estimating EOP from observations | out of scope for the whole tree |
| Solid-Earth, ocean and pole tides in the **geopotential** (a different use of the same tide models) | F2 |
| Fetching files over the network | the manifest fetcher (plan §2, P0) |

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `TN36-5` | IERS | *IERS Conventions (2010)* TN 36, chapter 5, §§5.5.1, 5.5.3 — sub-daily variations in pole coordinates and in UT1/LOD, and Tables 5.1a, 5.1b | 2010 | `https://iers-conventions.obspm.fr/content/chapter5/icc5.pdf` (retrieved 2026-09-18) | primary | normative |
| `TN36-8` | IERS | *IERS Conventions (2010)* TN 36, chapter 8 — Tables 8.2a, 8.2b (diurnal and semidiurnal variations in pole coordinates) and 8.3a, 8.3b (in UT1 and LOD) | 2010 | `https://iers-conventions.obspm.fr/content/chapter8/icc8.pdf` (retrieved 2026-09-18) | primary | normative |
| `C04` | IERS EOP Product Centre (Paris Observatory) | **IERS EOP 20 C04** series, consistent with ITRF2020 — data file and in-file header | current series; **v4 update effective 2026-02-05** | `https://hpiers.obspm.fr/iers/eop/eopc04/eopc04.1962-now` (retrieved 2026-09-18) | primary | normative (data + format) |
| `C04-README` | IERS EOP Product Centre | `readme` for the C04 20 series | v2 snapshot, 2023-12-12 → 2025-06-05 | `https://hpiers.obspm.fr/iers/eop/eopc04_20_v2/readme` (retrieved 2026-09-18) | primary | informative — **and demonstrably inconsistent with the data file, see §4.2** |
| `C04-UPD` | IERS EOP Product Centre | `updateC04.txt` — change log of the C04 series | entries to 2026-03-09 | `https://hpiers.obspm.fr/iers/eop/eopc04/updateC04.txt` (retrieved 2026-09-18) | primary | normative for the mutability policy (§3.3) |
| `FINALS` | IERS Rapid Service / Prediction Centre (USNO) | `readme.finals2000A` — format of `finals2000A.data`, `.daily`, `.all` | current | `https://maia.usno.navy.mil/ser7/readme.finals2000A` (retrieved 2026-09-18) | primary | normative (format) |
| `FINALS-80` | IERS Rapid Service / Prediction Centre (USNO) | `readme.finals` — format of `finals.data`, `.daily`, `.all` | current | `https://maia.usno.navy.mil/ser7/readme.finals` (retrieved 2026-09-18) | primary | normative — cited to establish what this tree does **not** use (§3.2) |
| `INTERP` | Ch. Bizouard, IERS EOP Product Centre | `interp.f` — reference routine for interpolating EOP and restoring sub-daily terms | coded 2002-11, corrected 2007-09 | `https://hpiers.obspm.fr/iers/models/interp.f` (retrieved 2026-09-18) | primary | normative for the algorithm; **not** for its code — see §3.4 |
| `ORTHO` | IERS Conventions Centre | `ORTHO_EOP.F` — ocean-tidal variations in *x*, *y*, UT1 (71 constituents), after Ray, Steinberg, Chao & Cartwright (1994) | Conventions (2010) software | `https://iers-conventions.obspm.fr/content/chapter8/software/ORTHO_EOP.F` (retrieved 2026-09-18) | primary | normative for its **published test case**; see §3.4 |
| `PMSD` | IERS Conventions Centre | `PMSDNUT2.F` — diurnal libration in pole coordinates (10 terms), after Brzeziński | Conventions (2010) software | `https://iers-conventions.obspm.fr/content/chapter5/software/PMSDNUT2.F` (retrieved 2026-09-18) | primary | normative for its **published test case** |
| `UTLIBR` | IERS Conventions Centre | `UTLIBR.F` — semi-diurnal libration in UT1 and LOD (11 terms), after Brzeziński & Capitaine (2003) | Conventions (2010) software | `https://iers-conventions.obspm.fr/content/chapter5/software/UTLIBR.F` (retrieved 2026-09-18) | primary | normative for its **published test case** |
| `RAY94` | R. D. Ray, D. J. Steinberg, B. F. Chao, D. E. Cartwright | *Diurnal and Semidiurnal Variations in the Earth's Rotation Rate Induced by Ocean Tides*, Science 264: 830–832 | 1994 | DOI `10.1126/science.264.5160.830` | **not obtained** | informative — the model behind `ORTHO`, whose coefficients are reproduced in `TN36-8` |

---

## 3. Definitions, conventions, and three corrections to the plan

### 3.1 The C04 series is **20** C04, not 14 C04

`doc/REWRITE_PLAN.md` §3 (F3) and the handover both name "IERS C04" / "EOP 14 C04". The
14 C04 series was **replaced by 20 C04 in February 2023**, and the two differ in more than a
version number:

- 20 C04 is consistent with **ITRF2020**; 14 C04 was consistent with ITRF2014. The pole
  coordinates are frame-dependent, so this is a change in the numbers, not a re-issue.
- The format changed: 20 C04 adds the pole **rates** *x*_rt, *y*_rt, and carries formal errors
  for every field [`C04`, `C04-README`].

- **EOP-R-001.** The final series MUST be **IERS EOP 20 C04**. The 14 C04 series MUST NOT be
  used, and a file whose header identifies it as 14 C04 MUST be refused (`EOP-F-001`).

### 3.2 The rapid series is `finals2000A.*`, not `finals.*`

`doc/REWRITE_PLAN.md` §3 (F3) and §2 (`io`) name `finals.all`. **`finals.all` is the wrong
file for this tree.** The two products have identical layouts but different contents in the
celestial-pole-offset columns:

| file | columns 98–134 and 166–185 carry |
|---|---|
| `finals.all` | dψ and dε — offsets with respect to the **IAU 1980** nutation theory [`FINALS-80`] |
| `finals2000A.all` | d*X* and d*Y* — offsets with respect to the **IAU 2000A** nutation [`FINALS`] |

`SPEC-frames.md` implements IAU 2006/2000A and consumes δ*X*, δ*Y* (`FRAME-R-012`). Feeding
it dψ, dε would not fail; it would apply numbers of the right magnitude and the wrong meaning.

- **EOP-R-002.** The rapid/predicted series MUST be **`finals2000A.all`** (or `.data` /
  `.daily` of the same family). `finals.all` MUST NOT be read, and a loader MUST distinguish
  them — they cannot be told apart by layout, only by which file was requested, so the
  manifest entry's identity is the only guard and MUST be treated as load-bearing
  (`EOP-F-002`).

### 3.3 The products are **mutable at a fixed URL**, and revised retroactively

This is the finding with the largest consequence for reproducibility, and it is documented by
the IERS itself. From `C04-UPD`:

- **2026-02-05**: pole coordinates and rates for **1 January 2021 – 31 December 2024** were
  replaced by the ITRF2020-u2024 values. "The name of the updated series remains unchanged as
  well as their path". The previous solution was archived at `…/eopc04_20_v3`.
- **2025-06-05**: the same operation for 2021-01-01 – 2024-02-24, against ITRF2020-u2023;
  previous solution archived at `…/eopc04_20_v2`.
- **2026-03-09**: pole **rates** for 1984 recomputed after an error was reported.

So: a fit run in January 2026 and the same fit re-run in March 2026, against a file re-fetched
from the same URL, use **different EOP for the same historical epochs** — and nothing in the
filename, the path, or the file header says so.

- **EOP-R-003.** Every loaded EOP file MUST be recorded in the run's provenance with its URL,
  the timestamp of retrieval, and a **SHA-256 of the file contents**. The hash is the only
  identifier that distinguishes the revisions.
- **EOP-R-004.** A frozen numerical baseline (plan §4's acceptance envelope) MUST record the
  hashes of the EOP files it was produced with. A baseline whose EOP hash is unknown MUST NOT
  be used as a regression target — it cannot be reproduced, and a disagreement against it
  cannot be attributed.
- **EOP-R-005.** The manifest MUST support pinning an **archived** series as well as the live
  one, so that a published result can be reproduced after the live series moves under it.
  **The IERS maintains the superseded series itself, at stable paths**, and names each one in
  `updateC04.txt` as it retires it — *"The former solution is archived in the repository
  `https://hpiers.obspm.fr/iers/eop/eopc04_20_v3`"* [`C04-UPD`]. Pinning therefore means
  recording an IERS URL, not hosting a snapshot: this tree redistributes nothing, and the
  question of whether it may does not arise.
- **EOP-R-006.** A **pinned** file whose SHA-256 does not match its manifest entry MUST be a
  refusal (`EOP-F-010`) — never a warning, and never "continuing with the newer file". A
  **live** file whose hash differs from the one last recorded MUST have the difference
  reported, naming both hashes and both retrieval dates, and MUST NOT be silently accepted as
  equivalent.
- **EOP-R-009.** The pinning policy is **split by purpose** — plan rule **R11**, adopted
  2026-09-18:
  - **Frozen baselines and acceptance tests MUST use a pinned, IERS-archived series**,
    identified by path and SHA-256.
  - **Operational runs MAY use the live series**, and MUST record the series identity and hash
    in the run's provenance.
  - **Re-baselining MUST be a deliberate, logged act.** It MUST NOT be reachable as a side
    effect of a re-fetch, and no code path may update a recorded baseline hash automatically.

### 3.4 The IERS reference routines are read as documentation, not as code

`INTERP`, `ORTHO`, `PMSD` and `UTLIBR` are Fortran routines published by the IERS. They carry
no licence statement.

- **EOP-R-007.** The models MUST be implemented from the **published coefficient tables in the
  Conventions document** — Table 5.1a (pole libration), Table 5.1b (UT1/LOD libration)
  [`TN36-5`], Tables 8.2a, 8.2b (pole, ocean tides) and 8.3a, 8.3b (UT1/LOD, ocean tides)
  [`TN36-8`] — and not by translating, transcribing or vendoring the Fortran (plan R6, R7).
  All of those tables are printed in the Conventions document; this was verified before the
  requirement was written.
- **EOP-R-008.** Transcription of a coefficient table MUST be verified against that routine's
  **published test case** (§8). A transcription that fails its published test case is a build
  failure, not a warning. Using a published expected output as an acceptance value is
  behavioural observation (plan R4), not derivation from the code.

This is the one place in the P1 tranche where a licence-clean implementation path is
non-obvious, and it is worth stating why the chosen path works: the coefficients are the
*content of a standard*, published so that they can be implemented; the Fortran is one
expression of them, and this tree does not need it.

### 3.5 Units: normalise at the boundary

The two products disagree about units in two fields, and the disagreements are factors of
1000 — large enough to be caught by a range check, small enough that an unvalidated value
still looks like a plausible EOP.

| quantity | `C04` [from the file's own header line] | `finals2000A` [`FINALS`] |
|---|---|---|
| *x*_p, *y*_p | arcsec | arcsec |
| UT1 − UTC | **seconds** | seconds |
| δ*X*, δ*Y* | **arcsec** | **milliarcsec** |
| LOD | **seconds** | **milliseconds** |
| *x*_rt, *y*_rt | arcsec/day | — (not carried) |

- **EOP-R-010.** Parsed values MUST be converted to a single canonical internal unit set at
  the parser boundary: **radians** for all angles and angular rates (per second), **seconds**
  for all times. No file's native units may propagate past the parser.
- **EOP-R-011.** Every parsed field MUST be range-checked before conversion, and a value
  outside its range MUST be refused (`EOP-F-004`). Ranges, for the historical era:
  |*x*_p|, |*y*_p| < 1 arcsec; |ΔUT1| < 1 s; |LOD| < 10 ms; |δ*X*|, |δ*Y*| < 100 mas. A
  mas/arcsec confusion in δ*X* moves the value by 1000× and trips the check immediately.
- **EOP-R-012.** The |ΔUT1| < 1 s check MUST be applied **only up to the last epoch at which
  leap seconds are known to have been in force**, and MUST be configurable, because CGPM
  Resolution 4 (2022) will raise the permitted maximum (`SPEC-time.md` §4.8, `TIME-R-055`).
  Hard-coding 0.9 s as an invariant would make this module fail on data it will be given.

---

## 4. Required behaviour

### 4.1 Reading the C04 20 series

The file is whitespace-delimited with a six-line comment header beginning `#`. Its own header
declares the Fortran format

```
format(4(i4),f10.2,2(f12.6),f12.7,2(f12.6),2(f12.6),f12.7,2(f12.6),f12.7,2(f12.6),2(f12.6),f12.7)
```

and the column meanings

```
YR  MM  DD  HH   MJD   x(")  y(")  UT1-UTC(s)  dX(")  dY(")  xrt(")  yrt(")  LOD(s)
                       x Er  y Er  UT1-UTC Er  dX Er  dY Er  xrt Er  yrt Er  LOD Er
```

The series begins **1962-01-01, MJD 37665.00**, sampled at **0h UTC** [`C04`].

- **EOP-R-020.** The parser MUST read by whitespace tokenisation, not by fixed columns. The
  declared format is fixed-width, but nothing guarantees it survives a future field widening,
  and the values are unambiguous when split on whitespace.
- **EOP-R-021.** The parser MUST verify that MJD is strictly increasing and that consecutive
  rows are one day apart, and MUST refuse a file failing either (`EOP-F-003`). A gap in the
  series would otherwise be interpolated across silently.
- **EOP-R-022.** The parser MUST cross-check the (YR, MM, DD, HH) fields against the MJD field
  and refuse a disagreement. The redundancy is in the file; using it costs nothing.
- **EOP-R-023.** The header line beginning `# Reference Precession-Nutation Model:` MUST be
  read and recorded. Note that in the file retrieved for this spec it reads
  `IAU 2000`, while `C04-README` describes the same file's offsets as
  `dX, dY IAU 2006/2000A`. The two statements are not identical and the product's own
  documentation does not reconcile them. The recorded value MUST be carried into provenance
  as-found, and MUST NOT be "corrected" by the parser. (`EOP-Q-002`.)

### 4.2 Reading `finals2000A`

Fixed-width, per `FINALS`. The fields this tree uses:

| cols | format | field |
|---|---|---|
| 1–6 | 3 × I2 | year (add 1900 for MJD ≤ 51543, 2000 for MJD ≥ 51544), month, day |
| 8–15 | F8.2 | MJD (UTC) |
| 17 | A1 | `I` (IERS) or `P` (prediction) flag for the Bulletin A polar motion |
| 19–36 | 2 × F9.6 | Bulletin A *x*_p and its error, arcsec |
| 38–55 | 2 × F9.6 | Bulletin A *y*_p and its error, arcsec |
| 58 | A1 | `I`/`P` flag for the Bulletin A UT1−UTC |
| 59–78 | 2 × F10.7 | Bulletin A UT1−UTC and its error, seconds |
| 80–93 | 2 × F7.4 | Bulletin A LOD and its error, **milliseconds** — *"NOT ALWAYS FILLED"* |
| 96 | A1 | `I`/`P` flag for the Bulletin A nutation values |
| 98–134 | 4 × F9.3 | Bulletin A d*X*, err, d*Y*, err, **milliarcsec**, w.r.t. IAU 2000A, **free core nutation not removed** |
| 135–165 | F10.6, F10.6, F11.7 | Bulletin B *x*_p, *y*_p, UT1−UTC |
| 166–185 | 2 × F10.3 | Bulletin B d*X*, d*Y*, **milliarcsec** |

- **EOP-R-024.** The parser MUST be fixed-width for this file. Its numeric fields abut without
  separators (columns 19–27 and 28–36, for example), so whitespace tokenisation mis-parses it.
  This is the opposite of `EOP-R-020` and the difference is a property of the two formats, not
  an inconsistency.
- **EOP-R-025.** Blank fields MUST be represented as *absent*, never as zero. The LOD field is
  documented as not always filled [`FINALS`], and the Bulletin B block is blank for epochs
  Bulletin B has not yet reached. A blank LOD read as 0.0 is a defensible approximation; a
  blank ΔUT1 read as 0.0 is a 0.9 s error.
- **EOP-R-026.** The `I`/`P` flags MUST be parsed and MUST become the record's quality tag.
  They are per-quantity, not per-row: polar motion, UT1 and nutation each have their own
  flag, and a row can be observed in one and predicted in another.
- **EOP-R-027.** The two-digit year MUST be resolved using the rule stated in the format
  document — add 1900 for MJD ≤ 51543, 2000 for MJD ≥ 51544 — and MUST be cross-checked
  against the MJD field, which is the authoritative one.
- **EOP-R-028.** The d*X*, d*Y* values in this file have **free core nutation not removed**
  [`FINALS`]. This MUST be recorded with the value. FCN is a free (unmodelled) motion of the
  CIP of order 0.1–0.3 mas; whether it is wanted depends on whether the consumer also applies
  an FCN model [`TN36-5` §5.5.5]. Applying both is double-counting.

### 4.3 Splicing

- **EOP-R-030.** The assembled series MUST prefer, at each epoch, in this order: **C04 final**
  → **`finals2000A` Bulletin B** → **`finals2000A` Bulletin A observed (`I`)** →
  **`finals2000A` Bulletin A predicted (`P`)**.
- **EOP-R-031.** Every returned record MUST carry a **quality tag** of `Final`, `BulletinB`,
  `Rapid` or `Predicted`, derived per quantity, not per epoch.
- **EOP-R-032.** The consumer MUST be able to set a policy that refuses `Predicted` (and
  optionally `Rapid`) values, and the default for a **reproducible campaign run** MUST be to
  refuse `Predicted` (`EOP-F-005`). Prediction is legitimate for planning and illegitimate for
  a published fit, and the difference must be a declared choice.
- **EOP-R-033.** At each splice boundary the loader MUST compute the discontinuity in each
  spliced quantity and MUST report it. A discontinuity above a configured threshold (default:
  1 mas in *x*_p or *y*_p, 0.1 ms in ΔUT1) MUST be a refusal (`EOP-F-006`), because at that
  size it indicates mismatched products — different ITRF realisations, or a C04 file from
  before a retroactive revision spliced against a current `finals2000A`.
- **EOP-R-034.** Splicing MUST NOT blend or taper across the boundary by default. A blend
  hides the discontinuity that `EOP-R-033` exists to expose. If a blend is ever wanted it is
  an explicit, recorded option.

### 4.4 Interpolation, and the four corrections that must follow it

The published series are **regularised**: the sub-daily variations caused by ocean tides and
by tidal gravitation ("libration") are *not* present in them, and must be added back to obtain
the instantaneous orientation. `TN36-5` §5.5.1 states this for the pole,

> (*x*_p, *y*_p) = (*x*, *y*)_IERS + (Δ*x*, Δ*y*)_ocean tides + (Δ*x*, Δ*y*)_libration — eq. (5.11)

and §5.5.3 states the corresponding thing for UT1 and LOD. The reference routine `INTERP`
performs: a **4-point Lagrange interpolation** of *x*, *y* and UT1−UTC (`LAGINT`), then adds
`PMUT1_OCEANS` (71 constituents, giving corrections to *x*, *y*, UT1 **and** LOD) and
`PM_GRAVI` (10 diurnal libration terms, *x* and *y* only) [`INTERP`].

- **EOP-R-040.** Interpolation of the tabulated series MUST be **4-point Lagrange** — the
  cubic through the two tabulated points each side of the requested epoch — matching `INTERP`.
- **EOP-R-041.** The interpolation MUST require **two tabulated points before and two after**
  the requested epoch. Where fewer are available the request MUST be refused (`EOP-F-007`).
  This is stricter than `INTERP`, whose `LAGINT` clamps its window at the ends of the array;
  clamping is a silent reduction in interpolation order and, at the very end of the table, a
  silent extrapolation. Plan §5 constraint 4 forbids both.
- **EOP-R-042.** The following MUST be added after interpolation, all four of them:

  | # | correction | applies to | source of coefficients | magnitude, as position at 7000 km |
  |---|---|---|---|---|
  | 1 | ocean tides, diurnal + semidiurnal | *x*_p, *y*_p | `TN36-8` Tables 8.2a, 8.2b (71 constituents) | up to ≈ **6 mm** |
  | 2 | ocean tides, diurnal + semidiurnal | ΔUT1, LOD | `TN36-8` Tables 8.3a, 8.3b (71 constituents) | up to ≈ **12 mm** |
  | 3 | libration (tidal gravitation), diurnal | *x*_p, *y*_p | `TN36-5` Table 5.1a (the 10 near-diurnal terms) | up to ≈ **1 mm** |
  | 4 | libration, semi-diurnal | ΔUT1, LOD | `TN36-5` Table 5.1b (11 terms) | up to ≈ **1 mm** |

  (Magnitudes are computed from the routines' own published test-case outputs, §8, at
  34 µm per µas of pole error and 0.51 mm per µs of UT1 error.)

- **EOP-R-043.** Correction **4** is required although the IERS reference routine omits it.
  `INTERP` calls `PMUT1_OCEANS` and `PM_GRAVI` only; it has no call to `UTLIBR`. `TN36-5`
  §5.5.3.1 says of the UT1/LOD libration terms that they "should be included in that
  routine" — so the Conventions require what the routine does not do. At about 1 mm of
  satellite position this is not negligible against ILRS normal points or IGS final orbits,
  and the deviation from `INTERP` MUST be documented in the module's output documentation so
  that a comparison against another implementation that follows `INTERP` literally is
  explicable rather than alarming.
- **EOP-R-044.** Only the **sub-daily** terms are added. `TN36-5` §5.5.1.1 is explicit that
  "the long-period terms, as well as the secular variation of the libration contribution, are
  already contained in the observed polar motion" — adding the full Table 5.1a, rather than
  its 10 near-diurnal rows, double-counts.
- **EOP-R-045.** The returned record MUST carry a boolean stating whether the sub-daily terms
  have been applied, and `SPEC-frames.md` MUST refuse a record for which they have not
  (`FRAME-F-003`). A flag is needed because the raw tabulated values are legitimately wanted
  in some contexts (comparing against IERS-published daily values) and are wrong in the one
  that matters.
- **EOP-S-046.** δ*X* and δ*Y* SHOULD be interpolated with the same 4-point Lagrange scheme.
  They carry no sub-daily correction; their own variation is slow and the interpolation error
  is far below their uncertainty.
- **EOP-R-047.** Because `LAGINT` is a Lagrange interpolant rather than a spline, it is **not**
  C¹ at the tabulated nodes: the derivative jumps as the four-point window slides. Anything
  differentiating EOP with respect to time — a variational equation, a numerical partial —
  MUST NOT do so by differencing this interpolant across a node boundary. The rate quantities
  the tree needs are taken from LOD and from the tabulated *x*_rt, *y*_rt, not from
  differentiating the interpolated series.

### 4.5 Coverage: refuse, never extrapolate

- **EOP-R-050.** A request outside the assembled series' coverage MUST be refused
  (`EOP-F-007`). The diagnostic MUST name the requested epoch in both UTC and MJD, the
  coverage interval in both, and which products contributed to each end of it.
- **EOP-R-051.** There MUST be no extrapolation of any kind: no holding the last value, no
  linear continuation, no zeroing, no falling back to a model. Not as a default, not as an
  option, not behind a flag.
- **EOP-R-052.** The coverage interval reported by the series MUST account for
  `EOP-R-041`'s two-point margin: the usable interval is two tabulated steps inside the data's
  own extent, and the reported coverage MUST be the usable one.
- **EOP-R-053.** Coverage MUST be queryable **before** a run begins, so that an arc whose span
  exceeds the data can be rejected at configuration time rather than partway through an
  integration.

### 4.6 The prediction horizon and the leap-second horizon do not coincide

**Two adopted specifications collide here and neither anticipated it.** `finals2000A.all` predicts
roughly **a year ahead**. `SPEC-time.md` `TIME-R-051` **refuses UTC past the leap-second table's
expiry**, currently 2027-06-28. The product routinely extends past the table, and a loader that
asked for ΔAT on every row refused the whole file.

- **EOP-R-054.** A row whose epoch lies beyond the leap-second table's usable horizon MUST be
  **excluded at load**, counted, and the count exposed — exactly as the pre-1972 rows are
  (`SPEC-time.md` `TIME-R-040`). The series is truncated at the horizon where UTC is still well
  defined, and `coverage()` reports the truncated interval.
- **EOP-R-055.** The refusal a caller meets beyond that horizon MUST therefore be `EOP-F-007`
  naming the coverage, and **not** a leap-table error surfacing from three layers down. A
  diagnostic about ΔAT, raised when the caller asked for Earth orientation, names the wrong thing
  and sends the reader to the wrong module.
- **EOP-R-056.** Where the leap table's single named override (`TIME-R-052`) is set for a run it
  MUST extend this horizon too, so that one declared decision governs both modules rather than two
  that can disagree.

This will recur at every layer that ingests a forecast, so it is stated as a rule rather than as a
note about one product.

The justification for the absoluteness of `EOP-R-051` is the shape of the error it prevents.
Running a day past the end of `finals2000A` with the last ΔUT1 held constant produces an error
that grows at the rate of LOD — of order 1 ms per day — which is 0.5 m of position per day at
LEO, growing quadratically in the number of days. An orbit fit absorbs that into its estimated
parameters and reports a residual that looks fine.

---

## 5. Interfaces

```
Quality     := Final | BulletinB | Rapid | Predicted
EopRecord   := { xp[rad], yp[rad], dut1[s], lod[s], dx[rad], dy[rad],
                 xrt[rad/s], yrt[rad/s],
                 quality: { pole: Quality, ut1: Quality, nutation: Quality },
                 subdaily_applied: bool,
                 fcn_removed: bool }
EopSeries   := opaque, immutable
EopPolicy   := { max_quality: Quality,          -- worst acceptable; default BulletinB… Rapid
                 apply_subdaily: bool,          -- default true
                 splice_discontinuity_limit: { pole[rad], ut1[s] },
                 dut1_range_limit[s] }          -- EOP-R-012
```

```
load_c04(bytes, source_id, &LeapTable)       -> Result<EopSeries, EopError>
load_finals2000a(bytes, source_id, &LeapTable)
                                             -> Result<EopSeries, EopError>
splice(final: EopSeries, rapid: EopSeries, EopPolicy)
                                        -> Result<EopSeries, EopError>   -- reports discontinuities

coverage(EopSeries)                     -> { first: Epoch, last: Epoch }  -- already margined
provenance(EopSeries)                   -> [ { url, retrieved, sha256, product, header_model } ]

eop_at(EopSeries, Epoch, EopPolicy)     -> Result<EopRecord, EopError>
eop_raw_at(EopSeries, Epoch, EopPolicy) -> Result<EopRecord, EopError>    -- subdaily_applied = false
```

Notes for the manager's review:

- **The leap table is taken at LOAD, not at the query** — versions 1.0–1.2 put it on neither, and
  the query needs it implicitly: an `Epoch` must become the UTC MJD the tables are indexed by,
  which needs ΔAT. Converting each row once at load leaves `eop_at` the pure function this section
  wanted, and the spacing between stored rows then **is** the actual length of that UTC day, 86400
  or 86401 s, so a leap second is handled without the query knowing about one.
- **`EopSeries` is immutable and carries its own provenance.** Two series loaded from
  different revisions of the same URL are different values with different hashes; nothing can
  confuse them, and `provenance()` answers "which EOP did this run use" from the value itself
  rather than from a log.
- **`EopPolicy` is passed per call, not stored in the series.** The same series can legitimately
  be queried under different policies (a planning query accepting predictions, a campaign
  query refusing them) in one process, and a policy stored in the series would make that a
  reload.
- **`eop_raw_at` exists** only so that `EOP-A-002` — agreement with the IERS-published daily
  values — is expressible. It returns `subdaily_applied = false`, which `SPEC-frames.md`
  refuses (`FRAME-F-003`), so it cannot leak into a transformation.
- **Quality is per-quantity**, matching the products, which flag polar motion, UT1 and nutation
  separately [`FINALS`].
- **No network, no cache, no global state.** The loaders take bytes. Fetching is the manifest
  fetcher's job (plan §2), which keeps this module testable from fixtures and keeps the
  "which file" question outside it.

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `EOP-P-1` | parse fidelity | exact to the last digit printed | — | both products are decimal text; nothing is lost |
| `EOP-P-2` | interpolation at a tabulated node reproduces the node | exact (to f64) | — | Lagrange property |
| `EOP-P-3` | interpolation between nodes | ≤ 10 µas in pole, ≤ 1 µs in ΔUT1 | 10 µas × 0.034 mm/µas = **0.34 mm**; 1 µs × 0.51 mm/µs = **0.51 mm** at 7000 km | the scheme `INTERP` uses; dominated by the true sub-daily signal, which §4.4 restores separately |
| `EOP-P-4` | sub-daily corrections vs their published test cases | agreement to the last published digit | — | §8 |
| `EOP-P-5` | the EOP's own uncertainty, C04 era | ≈ 30–100 µas in pole, ≈ 5–20 µs in ΔUT1 | 30–100 µas × 0.034 = **1.0–3.4 mm**; 5–20 µs × 0.51 = **2.6–10.2 mm** at 7000 km | the formal errors carried in the C04 file itself |

`EOP-P-5` is the honest floor on everything above this module: the frame transformation of
`SPEC-frames.md` is specified to 10 µas (`FRAME-P-3`) but is *delivered* to the accuracy of
these measurements, a few times worse. That is a property of the world, not a defect, and
`SPEC-frames.md` §6 says so.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `EOP-F-001` | a C04 file identifying as 14 C04 (or any series other than 20 C04) | the header line found, the series expected | read it anyway; the columns partly line up |
| `EOP-F-002` | a `finals` file where `finals2000A` was expected, detected by the manifest identity or by δ-column magnitudes inconsistent with mas-scale IAU 2000A offsets | the file identity and which nutation theory its offsets belong to | use dψ, dε as if they were d*X*, d*Y* |
| `EOP-F-003` | non-monotonic MJD, a gap, or a duplicate epoch | the line numbers and the two MJDs | interpolate across the gap |
| `EOP-F-004` | a field outside its documented range (§3.5) | field, value, range, line number | clamp; convert as if the units were the other product's |
| `EOP-F-005` | a `Predicted` value returned under a policy that refuses it | the epoch, the quantity, its quality tag, and the last epoch with an acceptable tag | downgrade the policy for this call |
| `EOP-F-006` | splice discontinuity above the threshold | both products, the boundary epoch, the discontinuity per quantity, the threshold | taper across it; take the mean |
| `EOP-F-007` | requested epoch outside the usable coverage (§4.5) | requested epoch (UTC **and** MJD), usable coverage (UTC **and** MJD), the contributing products, and whether the margin of `EOP-R-052` is what excluded it | hold the last value; extrapolate; zero the EOP |
| `EOP-F-008` | requested epoch before 1972-01-01 | delegated to `SPEC-time.md` `TIME-F-002` — ΔUT1 before 1972 is referred to rate-adjusted UTC | return the tabulated value as if it were comparable |
| `EOP-F-009` | a coefficient table whose transcription fails its published test case | the table, the test case, expected and obtained | ship it with a warning |
| `EOP-F-010` | a **pinned** file whose SHA-256 does not match its manifest entry | the manifest path, the expected hash, the obtained hash, both retrieval dates | warn and continue; accept the newer file; update the manifest automatically |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `EOP-A-001` | parse the whole C04 20 file: first row | 1962-01-01, MJD 37665.00, *x* = −0.012 700″, *y* = 0.213 000″, UT1−UTC = 0.032 633 8 s | `C04` — **the published file** | exact | R-020, R-024, P-1 |
| `EOP-A-002` | MJD monotone, one-day spacing, YR/MM/DD consistent with MJD, across every row | holds | `C04` | exact | R-021, R-022 |
| `EOP-A-003` | **`ORTHO_EOP` published test case**: MJD 47100 | Δ*x* = −162.838 637 327 963 653 0 µas; Δ*y* = 117.790 752 584 266 897 4 µas; ΔUT1 = −23.390 923 706 098 082 14 µs | `ORTHO` — published test case in the reference routine's own header | **The tolerance the Conventions themselves publish**, not the routine's printed digits: TN36 §8.2 states the table implementation and `ORTHO_EOP.F` "agree at the level of a few microarcseconds in polar motion and a few tenths of a microsecond in UT1". So **1 µas** in Δ*x*, Δ*y* and **0.05 µs** in ΔUT1 — inside that bound and above the achieved 0.09–0.34 µas and 0.007 µs. See the note below. | R-007, R-008, R-042(1,2), P-4 |
| `EOP-A-004` | **`PMSDNUT2` published test case**: MJD 54335 (2007-08-23) | Δ*x* = 24.831 442 382 733 648 34 µas; Δ*y* = −14.092 406 920 418 376 61 µas | `PMSD` — published test case | to the last published digit | R-007, R-008, R-042(3), P-4 |
| `EOP-A-005` | **`UTLIBR` published test case a**: MJD 44239.1 (1980-01-01 02:24:00) | ΔUT1 = 2.441 143 834 386 761 746 µs; ΔLOD = −14.789 712 473 494 494 92 µs/day | `UTLIBR` — published test case | to the last published digit | R-007, R-008, R-042(4), R-043 |
| `EOP-A-006` | **`UTLIBR` published test case b**: MJD 55227.4 (2010-01-31 09:35:59) | ΔUT1 = −2.655 705 844 335 680 244 µs; ΔLOD = 27.394 458 265 998 469 67 µs/day | `UTLIBR` — published test case | to the last published digit | R-007, R-008, R-042(4) |
| `EOP-A-007` | interpolation evaluated exactly at a tabulated node, with sub-daily corrections disabled | the tabulated value | Lagrange property — closed-form identity | exact to f64 | R-040, P-2 |
| `EOP-A-008` | `eop_at` at a node **with** corrections enabled minus `eop_raw_at` at the same node | equals the sum of the four corrections of `EOP-R-042` at that epoch | closed-form identity, given A-003…A-006 | exact | R-042, R-045 |
| `EOP-A-009` | interpolation order: halve the node spacing on a synthetic smooth series; the error falls by ≈ 16 | 4th-order convergence | closed-form property of the 4-point Lagrange interpolant | factor 16 ± 20 % | R-040 |
| `EOP-A-010` | **cross-product agreement**: for 200 days covered by both, parse the same epoch from C04 and from `finals2000A` Bulletin B | agree within the products' stated formal errors | the two published products — this is the test that catches the mas/arcsec and ms/s traps of §3.5 | pole < 0.5 mas, ΔUT1 < 50 µs | R-010, R-011, F-004 |
| `EOP-A-011` | ΔUT1 range check is configurable and does **not** hard-code 0.9 s | a synthetic record with ΔUT1 = 3 s is accepted under a policy permitting it | `SPEC-time.md` `TIME-R-055` / `CGPM27-4` | — | R-012 |
| `EOP-A-012` | `finals2000A` parse: a row with a blank LOD field yields *absent*, not 0.0 | absent | `FINALS` — "NOT ALWAYS FILLED" | — | R-025 |
| `EOP-A-013` | `finals2000A` parse: the `I`/`P` flags at columns 17, 58 and 96 become three independent quality tags | as in the file | `FINALS` | exact | R-024, R-026, R-031 |
| `EOP-A-014` | `finals2000A` parse: two-digit year resolution at the MJD 51543/51544 boundary | 1999-12-31 and 2000-01-01 | `FINALS` | exact | R-027 |
| `EOP-A-015` | refusal: request one day past the usable coverage | `EOP-F-007`, with the requested epoch and the coverage both in UTC and MJD, and the products named | this spec | — | F-007, R-050, R-051 |
| `EOP-A-016` | refusal: request 1.5 days inside the raw data's end, i.e. inside the two-point margin | `EOP-F-007`, naming the margin as the cause | this spec | — | R-041, R-052 |
| `EOP-A-017` | refusal: `Predicted` under the campaign policy | `EOP-F-005`, naming the last epoch with an acceptable quality | this spec | — | R-032, F-005 |
| `EOP-A-018` | refusal: splice a C04 file against a `finals2000A` whose Bulletin B differs by 5 mas at the boundary | `EOP-F-006`, naming the discontinuity | this spec | — | R-033, F-006 |
| `EOP-A-019` | provenance: two series loaded from byte-different files have different SHA-256 and both are reported | as stated | this spec | exact | R-003, R-005 |
| `EOP-A-020` | no global state: two `EopSeries` from different C04 revisions, queried alternately, give their own answers on every call | as stated | this spec | exact | R-ERR-1 |
| `EOP-A-021` | `subdaily_applied = false` records are rejected by `SPEC-frames.md` | `FRAME-F-003` | this spec + `SPEC-frames.md` | — | R-045 |
| `EOP-A-022` | refusal: load a pinned file whose bytes have been altered by one character | `EOP-F-010`, naming both hashes and both retrieval dates | this spec (plan R11) | — | R-006, F-010 |
| `EOP-A-023` | a baseline recorded against a pinned series is unchanged by a re-fetch of the live series | the baseline's recorded hash is untouched | this spec (plan R11) | exact | R-004, R-009 |
| `EOP-A-034` | the leap table's single named override (`TIME-R-052`) extends this horizon too: the same file loads with no rows excluded and the series reaches past the table's expiry | as stated | this spec §4.6 | — | R-056 |
| `EOP-A-033` | loading the real `finals2000A.all`, which predicts past the leap table's expiry, succeeds; the excluded rows are counted; an epoch beyond the horizon is refused by `EOP-F-007` naming the coverage, not by a leap-table error | as stated | this spec §4.6 | — | R-054, R-055 |
| `EOP-A-024` | refusal: a C04 file whose header identifies it as **14 C04** | `EOP-F-001`, naming the header line found and the series expected | this spec, §3.1 | — | R-001, F-001 |
| `EOP-A-025` | refusal: `finals.all` supplied where `finals2000A.all` was expected | `EOP-F-002`, naming which nutation theory the offsets belong to. Detected by manifest identity, and independently by the δ-columns being inconsistent with mas-scale IAU 2000A offsets | this spec, §3.2 | — | R-002, F-002 |
| `EOP-A-026` | refusal: a C04 file with a one-day gap, with a duplicated epoch, and with a decreasing MJD — three cases | `EOP-F-003`, naming the line numbers and the two MJDs in each case | this spec | — | R-021, F-003 |
| `EOP-A-027` | the header line `# Reference Precession-Nutation Model:` is recorded verbatim in provenance and is **not** rewritten by the parser | the string exactly as it appears in the file | this spec, `EOP-R-023`; see `EOP-Q-002` | exact | R-023 |
| `EOP-A-028` | a record parsed from `finals2000A` reports `fcn_removed = false` | as stated | `FINALS` — "Free Core Nutation NOT Removed" | exact | R-028 |
| `EOP-A-029` | **splice preference order**: at an epoch covered by all four sources the returned record is the C04 final value; removing each source in turn walks the preference down to Bulletin B, then Rapid, then Predicted | the order tabulated in `EOP-R-030` | this spec | exact | R-030 |
| `EOP-A-030` | **no blending**: at the splice boundary the value returned on each side is exactly that side's product value, with no taper | exact equality with the source | this spec, `EOP-R-034` | exact | R-034 |
| `EOP-A-031` | the pole-libration correction uses exactly the 10 near-diurnal rows of Table 5.1a — including the long-period rows must break `EOP-A-004` | `EOP-A-004` fails when the extra rows are included | `TN36-5` §5.5.1.1 — "the long-period terms … are already contained in the observed polar motion" | exact | R-044 |
| `EOP-A-032` | `coverage()` is answerable before any epoch is requested, and the interval it reports is already margined by two tabulated steps | as stated | this spec, `EOP-R-052`, `-R-053` | exact | R-053 |

**Not covered by a test.** The handover's acceptance row *"EOP interpolation at epochs where
IERS publishes interpolated values | matches published values"* cannot be discharged as
written: the IERS publishes the **daily tabulated** series and the reference **routines**, but
not a table of interpolated intermediate values to compare against. `EOP-A-003`…`EOP-A-006`
are the strongest available substitute — they check the part that is genuinely hard (the
71-constituent and 10/11-term tidal series) against values the IERS does publish — and
`EOP-A-007`…`EOP-A-009` check the interpolation itself against closed-form properties. This
is recorded at `EOP-Q-001` rather than silently re-scoped.

**Note on `EOP-A-003`'s tolerance, and a contradiction versions 1.0–1.2 carried.**
`EOP-R-007` requires the tidal models to be implemented **from the tables printed in the
Conventions**, because the IERS Fortran carries no licence. `EOP-A-003` required matching
`ORTHO_EOP`'s test case **"to the last published digit"**. Those cannot both hold, and TN36 §8.2
says so outright:

> "Because these tables cannot be found in the code of 'ORTHO EOP.F', the IERS Earth Orientation
> Center has implemented them in the alternative software 'interp.f'. **The two routines agree at
> the level of a few microarcseconds in polar motion and a few tenths of a microsecond in UT1.**"

The specification demanded of one requirement what another forbade. The tolerance is now that
published bound. `EOP-A-004`…`EOP-A-006` are unaffected: Tables 5.1a and 5.1b are implemented
directly rather than re-derived, so those agree far more closely.

**Coverage.** Every requirement and refusal in this spec is discharged by at least one row
above, except the following, listed in full:

| id | why no test |
|---|---|
| `EOP-R-047` | "Do not differentiate the interpolant across a node boundary" is a prohibition on the *callers*. Discharged by review of the variational-equation and partial-derivative code when it exists, and structurally by `SPEC-frames.md` taking rate quantities from LOD and the tabulated pole rates instead. |
| `EOP-R-009` (third bullet only) | "No code path may update a recorded baseline hash automatically" is a negative structural property, discharged by review of the manifest and baseline tooling. `EOP-A-023` checks its observable half. |
| `EOP-F-008` | **Delegated** to `SPEC-time.md` `TIME-F-002` and tested there (`TIME-A-013`, `TIME-A-026`). This spec's obligation is to propagate it unchanged. |
| `EOP-F-009` | Not a runtime refusal but a **build** failure: it *is* the failure mode of `EOP-A-003`…`EOP-A-006`, the transcription checks, per `EOP-R-008`. |

---

## 9. Provenance obligations

- **Module register:** `eop` → `TN36-5` §§5.5.1/5.5.3 and Tables 5.1a/5.1b; `TN36-8` Tables
  8.2a/8.2b/8.3a/8.3b; `INTERP` (algorithm only); the format documents `C04`, `FINALS`.
- **Parameter register:** every row of Tables 5.1a (10 diurnal terms used), 5.1b (11 terms),
  8.2a, 8.2b, 8.3a, 8.3b, cited to the Conventions with the transcription-verification test
  that passed.
- **Data manifest entries:** the C04 file and the `finals2000A` file, each with URL, retrieval
  timestamp, **SHA-256**, the header model string found in it, and — for C04 — whether it is
  the live series or a pinned IERS archive path (`EOP-R-005`), together with which purpose the
  run served, since baselines and operational runs are pinned differently under plan R11
  (`EOP-R-009`).
- **A standing note** in the ledger recording §3.3: that the C04 URL is mutable and that
  historical values have been revised retroactively on at least three recorded occasions
  (2025-06-05, 2026-02-05, 2026-03-09 [`C04-UPD`]).
- **Dependency register:** none. This module has no third-party dependency by design — the
  IERS Fortran is read as documentation, not linked (`EOP-R-007`).

---

## 10. Open questions for the manager

| id | question | recommendation / **resolution** |
|---|---|---|
| `EOP-Q-001` | **The handover's EOP acceptance row is not achievable as written.** "EOP interpolation at epochs where IERS publishes interpolated values" — the IERS does not publish interpolated values; it publishes the daily series and the routines that interpolate it. | Replace that row in the acceptance envelope with `EOP-A-003`…`EOP-A-006` (the four published test cases of the tidal routines) plus `EOP-A-007`…`EOP-A-009` (closed-form interpolation properties). This is stronger, not weaker: it pins the hard part against published numbers. If a numerical comparison against `INTERP` itself is wanted, it is an **oracle** comparison under plan R4 — legitimate, logged, and not a gate. |
| `EOP-Q-002` | **The C04 product contradicts itself about its own model.** The data file's header says `Reference Precession-Nutation Model: IAU 2000`; the series `readme` says the offsets are `dX, dY IAU 2006/2000A`. δ*X*, δ*Y* offsets are defined relative to a specific precession-nutation model, so this is not cosmetic — though the difference between "IAU 2000" and "IAU 2006/2000A" in the *X*, *Y* series is at the sub-mas level for the epochs in question. | **ESCALATED TO THE OWNER, 2026-09-18.** Resolving it means asking the IERS EOP Product Centre, which neither the manager nor the executor session may do. **No code change is needed either way**: record as-found and forbid the parser from correcting it (`EOP-R-023`), which is correct under both readings. The manager's reading, offered but explicitly not asserted as settled, is that the header's "IAU 2000" is loose shorthand for IAU 2006/2000A — the precession is 2006 and the nutation 2000A, and the elision is common — rather than a real contradiction. Recorded as-found precisely because that is a reading and not a fact. |
| `EOP-Q-003` | **Free core nutation.** `finals2000A`'s d*X*, d*Y* have FCN **not** removed [`FINALS`]; `TN36-5` §5.5.5 provides an FCN model. Applying both double-counts a 0.1–0.3 mas signal (3–10 mm at 7000 km). C04's treatment of FCN is not stated in the file header. | For P1: do **not** apply an FCN model; use the offsets as published, and record `fcn_removed = false`. Revisit before P6 only if a campaign residual shows an unexplained signal at the FCN period (≈ 430 days retrograde). Recorded so that if such a signal appears, the first place to look is written down. |
| `EOP-Q-004` | **Which C04 revision should the frozen baselines of plan §4 use?** §3.3 shows the series is revised retroactively, so "the current C04" is not a stable target. | **RESOLVED 2026-09-18 — plan rule R11, and split by purpose rather than answered once.** Baselines and acceptance tests pin an **IERS-archived** series by path and SHA-256; operational runs may use the live series but must record its identity and hash; a hash mismatch on a pinned file is a hard failure, never a warning; re-baselining is a deliberate logged act, never a side effect of a re-fetch. Specified at `EOP-R-005`, `EOP-R-006`, `EOP-R-009`; refused at `EOP-F-010`; tested by `EOP-A-022`, `EOP-A-023`. The manager added the fact that makes this cheap: **the IERS archives the superseded series itself at stable paths**, so pinning records a URL rather than hosting a snapshot, and no redistribution question arises. |
| `EOP-Q-005` | **Should the splice default refuse `Rapid` as well as `Predicted`?** §4.3 defaults to refusing only `Predicted`. Rapid (Bulletin A observed) values are typically within 0.1 mas of the eventual final values, but they do change. | Refuse `Predicted` by default, permit `Rapid`, and require the quality tags to be reported in every campaign output so that a result computed partly on rapid EOP is identifiable after the fact. A stricter default would make it impossible to process data from the last month, which is the common case. |
| `EOP-Q-006` | **Pole rates.** 20 C04 carries *x*_rt, *y*_rt; `finals2000A` does not. The interface exposes them (§5) but nothing in P1 consumes them, and after a splice they are absent for part of the series. | Carry them as optional, as specified. They are the correct source for a polar-motion rate if `FRAME-Q-004` (the neglected Ẇ term) is ever revisited, which is why they are in the record rather than dropped at the parser. |

---

## Retired identifiers

Kept so that a reference in a changelog, a review note or an earlier draft resolves rather
than dangling.

| id | retired | replaced by | why |
|---|---|---|---|
| `EOP-S-006` | v1.1, 2026-09-18 | `EOP-R-006` | It was a SHOULD: "the loader SHOULD warn when the live series' hash differs from the one recorded in the manifest". Plan rule R11 makes the pinned case a refusal, so the recommendation became a requirement and changed identifier class. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.4 | 2026-09-18 | §6's rows carry their multiplication, per `SPEC-template.md` §1. Both were audited and were already correct; the form changed, not the numbers. |
| 1.3 | 2026-09-18 | **Amended after implementation.** `EOP-A-003`'s tolerance set to the bound TN36 §8.2 itself publishes, resolving a contradiction with `EOP-R-007` that versions 1.0–1.2 carried. §4.6 added — the prediction horizon and the leap-second horizon do not coincide — with `EOP-R-054`…`EOP-R-056` and `EOP-A-033`. §5's loaders take the leap table, which the query then does not need. |
| 1.2 | 2026-09-18 | **Acceptance coverage completed.** Added `EOP-A-024` … `EOP-A-032` and the §8 *Coverage* table listing every requirement and refusal not discharged by a test, with the reason. v1.0–1.1 claimed the template's coverage rule without meeting it. **No requirement was added, removed or changed**; the adopted requirement set is exactly as at v1.1. |
| 1.1 | 2026-09-18 | **Adopted.** Recorded the manager's decisions: `EOP-Q-004` resolved as plan rule R11 (`EOP-R-005` strengthened with the IERS archive fact; `EOP-R-006` and `EOP-R-009` added; `EOP-F-010`, `EOP-A-022`, `EOP-A-023` added); `EOP-Q-002` escalated to the owner. `EOP-S-006` (a SHOULD to warn on hash difference) **retired** and replaced by `EOP-R-006`, which makes the pinned case a refusal. |
| 1.0 | 2026-09-18 | First draft, P1 tranche, for manager review. |
