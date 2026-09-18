# SPEC-time — time scales, epochs, and their representation

| | |
|---|---|
| **Spec ID** | `TIME` |
| **Status** | **adopted** 2026-09-18 — manager verdict from session `odl maintainer (Router+Executor)`. Version 1.1 records the decisions taken in that verdict. |
| **Version** | 1.4 |
| **Date** | 2026-09-18 |
| **Layer** | `time` (`doc/REWRITE_PLAN.md` §2) |
| **Feature** | F3's foundation (plan §3) |
| **Depends on** | nothing in this tree |
| **Depended on by** | `frames`, `eop`, every layer above them |

**Derivation declaration (plan R1, R2).** This specification was written from the documents
listed in §2 and from no implementation of this module. Specifically, no file under
`/home/rog/odl20lite` was opened, read, listed, searched, or otherwise inspected during its
preparation. Numeric acceptance targets carried from prior measurement campaigns are
behavioural observations against public data (plan R4) and are marked as such where they
appear.

---

## 1. Purpose and scope

This module owns **the identification of an instant**: the scales in which an instant can be
expressed, the conversions between them, the leap-second table that makes UTC meaningful, and
the numeric representation in which an instant is carried through the rest of the pipeline.

It is the lowest layer in the tree and has no dependencies within it. Everything above it —
frames, ephemerides, forces, measurements, estimation — takes its epoch type from here, and
the correctness of everything above depends on two properties this module must guarantee:
that an instant cannot be mistaken for an instant in a different scale, and that no instant
loses more than a picosecond in the round trip from external data to internal use and back.

### In scope

- The time scales TAI, TT, TCG, TDB, TCB, UTC, GPS, and the treatment of UT1.
- Conversions among them, including the leap-second machinery.
- The numeric representation of an instant and of an interval.
- Calendar and Julian-date input and output, including the 61-second minute.
- The policy for epochs outside the range in which UTC is well defined.
- Loading, validating, and expiring the IERS leap-second table.

### Not in scope

| excluded | owned by |
|---|---|
| ΔUT1, polar motion, LOD, CIP offsets — their files, parsing, interpolation | `SPEC-eop.md` |
| Earth rotation angle, sidereal time, and anything that rotates a vector | `SPEC-frames.md` |
| Light-time iteration and signal-propagation delays | the measurement-model spec (F11) |
| Proper time of a clock on a moving platform | `SPEC-relativity` (F2), not yet written |

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `TN36-1` | G. Petit, B. Luzum (eds.), IERS | *IERS Conventions (2010)*, Technical Note 36, **chapter 1** — General definitions and numerical standards | 2010 | `https://iers-conventions.obspm.fr/content/chapter1/icc1.pdf` (retrieved 2026-09-18) | primary | normative |
| `TN36-10` | G. Petit, B. Luzum (eds.), IERS | *IERS Conventions (2010)*, Technical Note 36, **chapter 10** — General relativistic models for space-time coordinates and equations of motion | 2010 | `https://iers-conventions.obspm.fr/content/chapter10/tn36_c10.pdf` (retrieved 2026-09-18) | primary | normative |
| `LEAP` | IERS Earth Orientation Centre | `Leap_Second.dat` — value of TAI−UTC | updated through IERS Bulletin C 72, July 2026; **expires 28 June 2027** | `https://hpiers.obspm.fr/iers/bul/bulc/Leap_Second.dat` (retrieved 2026-09-18) | primary | normative (data) |
| `ERFA` | NumFOCUS Foundation / liberfa | ERFA — Essential Routines for Fundamental Astronomy, source and in-source documentation | **v2.0.1**, released 2023-10-13, tracking SOFA "20231011" | `https://github.com/liberfa/erfa` (retrieved 2026-09-18) | primary | interface — **BSD 3-clause** (plan R7) |
| `CGPM27-4` | CGPM / BIPM | Resolution 4 of the 27th CGPM (2022) — on the use and future development of UTC | 2022 | `https://www.bipm.org/en/cgpm-2022/resolution-4` | secondary (see `TIME-Q-004`) | informative |
| `IAU06-B3` | IAU | Resolution B3 (2006) — re-definition of Barycentric Dynamical Time, TDB | 2006 | reproduced in `TN36-10` §10.1 eq. (10.3) and Appendix A of TN36 | primary, via `TN36-10` | normative |
| `ISGPS200` | US Space Force / GPS Directorate | IS-GPS-200, Navstar GPS Space Segment / Navigation User Interfaces — definition of GPS system time | current revision | `https://www.gps.gov/technical/icwg/` | **not obtained** (see `TIME-Q-002`) | normative for one constant |

Licence note for the dependency register: ERFA is BSD 3-clause and is the mandated choice
under plan R7; SOFA is excluded because of its rename clause, notwithstanding that ERFA is
derived from it with the SOFA board's permission [`ERFA` README].

---

## 3. Definitions and conventions

### 3.1 Units and symbols

| symbol | quantity | unit |
|---|---|---|
| ΔAT | TAI − UTC | SI seconds, integral from 1972-01-01 |
| ΔUT1 | UT1 − UTC | SI seconds, supplied by `eop` |
| ΔT | TT − UT1 | SI seconds, derived |
| *L*_G | 1 − d(TT)/d(TCG) | dimensionless, **defining constant** 6.969 290 134 × 10⁻¹⁰ [`TN36-1` Table 1.1] |
| *L*_B | 1 − d(TDB)/d(TCB) | dimensionless, **defining constant** 1.550 519 768 × 10⁻⁸ [`TN36-1` Table 1.1] |
| TDB₀ | TDB offset | −6.55 × 10⁻⁵ s, **defining constant** [`TN36-10` eq. (10.3)] |
| *T*₀ | reference epoch of the rate relations | JD 2443144.500 3725, i.e. 1977-01-01T00:00:00 TAI [`TN36-10` §10.1] |

All durations are in **SI seconds**. There are no other time units inside the module; days,
minutes, hours, centuries and Julian dates exist only at the boundary, in the conversion
functions that name them.

### 3.2 The scales

- **TAI** — International Atomic Time. Uniform, continuous, no leap seconds. The internal
  scale of this tree (§4.1).
- **TT** — Terrestrial Time. `TT = TAI + 32.184 s` **exactly**; TAI is a realisation of TT
  apart from that constant offset [`TN36-10` §10.1]. The independent argument of the
  precession-nutation models and of the equations of motion in the geocentric frame.
- **TCG** — Geocentric Coordinate Time. `TCG − TT = L_G/(1 − L_G) × (JD_TT − T₀) × 86400 s`
  [`TN36-10` eq. (10.1)].
- **TDB** — Barycentric Dynamical Time. Defined as a linear transformation of TCB:
  `TDB = TCB − L_B × (JD_TCB − T₀) × 86400 s + TDB₀` [`TN36-10` eq. (10.3), `IAU06-B3`].
  In practice obtained as `TDB = TT + (TDB − TT)`, where the difference is periodic with
  maximum amplitude about 1.7 ms [`TN36-10` §10.1, on the non-linear terms *P(TT)*].
  **This is the time argument of the JPL planetary ephemerides.**
- **TCB** — Barycentric Coordinate Time. Supported for completeness; nothing in the P1–P6
  pipeline requires it.
- **UTC** — Coordinated Universal Time. From 1972-01-01, `UTC = TAI − ΔAT` with ΔAT integral
  [`LEAP`]. **UTC is not a uniform scale**: the interval between two UTC labels is not in
  general the number of SI seconds between the instants they denote. Before 1972 it was not
  even an integral offset — see §4.6.
- **UT1** — the rotation angle of the Earth expressed as time. **UT1 is not a time scale in
  this module's sense** and is handled separately; see §3.3.
- **GPS** — GPS system time. Continuous, no leap seconds, `TAI − GPS = 19 s` exactly (§4.4).

### 3.3 UT1 is an angle, not a time

UT1 is proportional to the Earth's rotation angle. It is not uniform, it is not predictable,
it is known only where the IERS has published ΔUT1, and it is never the independent variable
of a dynamical model. Treating it as "another time scale" invites two specific errors: the
propagation of a state in UT1, and the construction of a UT1 epoch in a context where no EOP
data exists to justify it.

- **TIME-R-001.** UT1 MUST NOT be representable as a stored epoch. A UT1 value MUST be
  producible only by an operation that takes ΔUT1 as an explicit argument, and MUST be
  consumed immediately by the caller that asked for it.
- **TIME-R-002.** No operation in this module MAY obtain ΔUT1 for itself. It is always
  passed in, by a caller that got it from `eop` with that layer's coverage and quality
  policy applied.

This is a deliberate structural choice and one of the places where this design differs from
the conventional one of "an enumeration of scales, all equal". The justification is in the
failure it removes: with UT1 as an ordinary scale, `epoch_in(UT1)` is reachable from anywhere
and silently defaults ΔUT1 to zero, which is an error of up to 0.9 s — 6.7 km of along-track
position at LEO.

### 3.4 Scale is carried, never inferred

- **TIME-R-003.** Every epoch that enters the system from outside — a file, a service, a
  command line, a configuration value — MUST be accompanied by an explicit time scale. There
  is no default scale, and no operation MAY guess one.
- **TIME-R-004.** An external source whose time scale cannot be established from the source
  itself MUST be refused (`TIME-F-001`), not defaulted.

The motivating case, from prior campaign experience on this pipeline (plan R4 observation,
carried in the handover): a JPL Horizons vector table defaults to the TDB time scale, which
in this era runs about 69 s ahead of UTC. Feeding those timestamps to a tool expecting UTC
displaces a low-Earth-orbit satellite by roughly 500 km along track — and an orbit fit will
converge on that displacement and report a plausible residual. The defect is not detectable
downstream; it must be made unrepresentable here.

---

## 4. Required behaviour

### 4.1 The internal scale is TAI

- **TIME-R-010.** The single stored representation of an instant MUST be in **TAI**. TT, TDB,
  TCG, TCB, UTC and GPS are *views* of a stored instant, produced on demand; they are not
  alternative storage.

Rationale, in the order the alternatives fail:

- Storing **UTC** makes arithmetic wrong across a leap second, because the difference of two
  stored values is not the elapsed SI time.
- Storing **TT** is defensible (it is uniform and is what the dynamics wants) but makes the
  leap-second boundary conversion carry an extra fixed offset for no gain; TAI is the scale
  the leap-second table is expressed against [`LEAP`], so UTC↔internal is one table lookup.
- Storing **TDB** is lossy: the TT↔TDB difference is a series evaluation, not an exact
  relation, so a stored TDB cannot be converted to TT and back without accumulating the
  series' own error.

TAI is uniform, continuous, unambiguous, and one exact addition away from TT.

### 4.2 Representation

The requirement that fixes this is the pipeline's precision budget: **roughly one microsecond
preserved over multi-decade spans** (§6). The candidates, evaluated against it:

| representation | spacing of adjacent representable values | along-track equivalent at 7.5 km s⁻¹ | verdict |
|---|---|---|---|
| `f64` Julian Date (JD ≈ 2.46 × 10⁶ d) | 2⁻³¹ d = 4.66 × 10⁻¹⁰ d = **40 µs** | **0.30 m** | **disqualified** |
| `f64` Modified Julian Date (MJD ≈ 6.1 × 10⁴ d) | 2⁻³⁷ d = 7.28 × 10⁻¹² d = **0.63 µs** | **4.7 mm** | **disqualified** — equal to the entire budget before any arithmetic |
| two-part `f64` JD as (2400000.5, MJD) | as MJD above: **0.63 µs** | 4.7 mm | disqualified |
| two-part `f64` JD as (integer JD, day fraction) | 2⁻⁵³ d = **9.6 ps** | 7 × 10⁻¹¹ m | adequate *per call*, but not canonical — see below |
| **`i64` whole SI seconds + `f64` fraction in [0,1)** | 2⁻⁵³ s = **0.11 fs** | 8 × 10⁻¹⁶ m | **required** |

- **TIME-R-011.** An instant MUST be represented as a pair: an `i64` count of whole SI seconds
  since a fixed integral-second origin on the TAI scale, and an `f64` fractional second
  normalised to [0, 1). The origin MUST be **1958-01-01T00:00:00 TAI**, the conventional
  origin of TAI.
- **TIME-R-012.** The representation MUST be canonical: every instant has exactly one
  representation. Operations that could leave the fraction outside [0, 1) MUST renormalise
  before returning.
- **TIME-R-013.** An interval MUST be represented as a signed pair of the same shape, in SI
  seconds. Interval arithmetic MUST be exact in the whole-second component.

The `i64` seconds field spans ±2.9 × 10¹¹ years, so range is not a consideration. The reason
for rejecting the two-part `f64` JD *as storage* despite its adequate resolution is that it is
not canonical: the same instant has unboundedly many (a, b) splittings, so equality, ordering,
and hashing are all ill-defined on it, and every one of those is needed by the layers above
(measurement sorting, arc boundaries, table lookup). It remains the right form at the **call
boundary** to ERFA, which is where it is used:

- **TIME-R-014.** Conversion to a dependency's two-part Julian date MUST place the integral
  Julian day in the first part and the day fraction in the second, giving the second part a
  magnitude below 1 and therefore a spacing of 2⁻⁵³ d ≈ 10 ps. Passing `(0, JD)` or
  `(2400000.5, MJD)` to a dependency is forbidden.

### 4.3 Conversions among the uniform scales

All of these are exact or series-based and involve no table:

| from → to | relation | source |
|---|---|---|
| TAI → TT | `TT = TAI + 32.184 s` exactly | [`TN36-10` §10.1] |
| TT → TCG | `TCG = TT + L_G/(1−L_G) × (JD_TT − T₀) × 86400 s` | [`TN36-10` eq. (10.1)] |
| TT → TDB | `TDB = TT + (TDB−TT)`, the difference evaluated as a periodic series | [`TN36-10` §10.1]; implemented by `eraDtdb` [`ERFA`] |
| TDB → TCB | invert `TDB = TCB − L_B × (JD_TCB − T₀) × 86400 s + TDB₀` | [`TN36-10` eq. (10.3)] |
| TAI ↔ GPS | `GPS = TAI − 19 s` exactly | §4.4 |

- **TIME-R-020.** TT↔TAI MUST be exact: implemented as an addition of the integral part
  32 s to the seconds field and 0.184 s to the fraction, not as a floating-point day offset.
- **TIME-R-021.** The TDB−TT difference MUST be obtained from `eraDtdb` [`ERFA`], which
  requires the observer's geocentric location (station longitude, and distances from the
  spin axis and equatorial plane) because the difference includes the observer's diurnal
  term. For a geocentric epoch those location arguments are zero; for a station epoch they
  MUST be supplied. The diurnal term reaches about 2.1 µs, which is 16 mm of along-track
  position at LEO and is therefore **not** negligible for SLR.
- **TIME-R-021a.** `eraDtdb` also takes **UT1 as a fraction of a day**, which §5's signature
  does not provide and versions 1.0–1.2 of this specification did not mention. UTC's day
  fraction MUST be used in its place, and the reason is quantitative: only the diurnal term
  depends on that argument, it has a one-day period and an amplitude of about 2.1 µs, so an
  error of up to 0.9 s in UT1 moves the result by **under 0.2 ns** — four orders below
  `TIME-P-5`'s 10 ns budget. Requiring ΔUT1 here would make a barycentric time conversion
  depend on the EOP layer for no measurable gain, which is a worse trade than the
  approximation. The substitution MUST be documented where the operation is implemented, so
  that nobody has to rediscover why no EOP is threaded through.
- **TIME-S-022.** TCG and TCB SHOULD be provided for completeness but need not be used by any
  P1–P6 module. Nothing in this tree's dynamics is formulated in TCB.

### 4.4 GPS time

GPS system time is continuous and free of leap seconds. It was aligned with UTC at
1980-01-06T00:00:00 UTC, at which date the leap-second table gives ΔAT = 19 s (the entry
`44239.0  1 1 1980  19` in [`LEAP`] is in force on that date and until 1981-07-01).
Consequently `TAI − GPS = 19 s` exactly, for all time.

- **TIME-R-023.** `TAI − GPS` MUST be the exact integer 19 s, applied to the seconds field.
- **TIME-R-024.** GPS↔TAI MUST be implemented in this tree. **ERFA provides no GPS time
  routines** — the header `erfa.h` of `ERFA` v2.0.1 declares no `eraTaigps`/`eraGpstai`
  pair, and no equivalent. This is an interface fact, recorded so nobody looks for them.
- **TIME-S-025.** GPS week number and time-of-week SHOULD be supported as an input and output
  format, with the week-number rollover handled by requiring an unambiguous (extended) week
  number on input and refusing a bare 10-bit week (`TIME-F-006`).

The 19 s constant's definitive source is `ISGPS200`, which was not obtained (see
`TIME-Q-002`). It is nevertheless derivable from a primary source in hand — [`LEAP`] — given
the publicly documented GPS epoch, and the derivation is recorded above so a reviewer can
check it without the paywalled document.

### 4.5 UTC and leap seconds, from 1972-01-01

From 1972-01-01T00:00:00 UTC, ΔAT is a step function taking integral values, changing only at
the start of a UTC day, in practice at the start of 1 January or 1 July [`LEAP`]. A positive
leap second appears as a UTC label with seconds field 60 in the last minute of the preceding
day; that label denotes a real instant and MUST be representable.

- **TIME-R-030.** UTC↔TAI MUST be computed from a leap-second table **loaded as data**
  (§4.7), not from any table compiled into a dependency.
- **TIME-R-031.** UTC calendar input MUST accept a seconds field in [0, 61) and MUST accept
  the value 60 **only** in the last minute of a day on which the table records an increase of
  ΔAT. A seconds field of 60 on any other day MUST be refused (`TIME-F-003`).
- **TIME-R-032.** UTC calendar output MUST render the leap second as `23:59:60`, never as
  `23:59:59` repeated or as `00:00:00` of the following day.
- **TIME-R-033.** Rounding a UTC time of day for display MUST NOT be able to produce
  `23:59:60` on a day without a leap second, nor `24:00:00` on any day.
- **TIME-R-034.** The difference of two instants MUST always be the elapsed SI seconds
  between them, including across a leap second. Because storage is TAI (`TIME-R-010`) this
  is automatic; it is stated because it is the property that motivates the choice.

Worked case for §8: 2016-12-31 carries the most recent leap second, ΔAT rising from 36 to 37.

| UTC label | TAI |
|---|---|
| 2016-12-31T23:59:59 | 2017-01-01T00:00:35 |
| 2016-12-31T23:59:60 | 2017-01-01T00:00:36 |
| 2017-01-01T00:00:00 | 2017-01-01T00:00:37 |

### 4.6 Before 1972: refuse

Between 1961 and 1972, UTC was not offset from TAI by an integral number of seconds. It ran at
a *different rate*, defined by piecewise-linear expressions of the form
`TAI − UTC = a + (MJD − MJD₀) × b` with drift coefficients of order 10⁻³ s d⁻¹, punctuated by
step adjustments of a fraction of a second in either direction. In that era a "UTC second" was
not an SI second. `ERFA`'s own `eraDat` implements those expressions back to 1961 and its
documentation states that "UTC began at 1960 January 1.0 (JD 2436934.5) and it is improper to
call the function with an earlier date", returning zero with a warning status if called
earlier [`ERFA` `src/dat.c`, note 1].

- **TIME-R-040.** Any operation that constructs or renders **UTC** before
  1972-01-01T00:00:00 UTC MUST be refused (`TIME-F-002`). The refusal MUST be on UTC and on
  anything derived from UTC (which includes UT1, since ΔUT1 before 1972 is referred to the
  rate-adjusted UTC of the day).
- **TIME-R-041.** The **uniform** scales MUST NOT be restricted by this policy. TAI, TT, TCG,
  TDB and TCB before 1972 are accepted; TAI is defined from 1958 and the relations of §4.3
  hold throughout. The refusal is about the meaning of a label, not about the existence of an
  instant.

Reasons, in order of weight:

1. **The claim would be false.** This module promises microsecond fidelity (§6). The pre-1972
   expressions realise UTC to no better than milliseconds, and the "seconds" they count are
   not the seconds the rest of the pipeline uses. Accepting the range would mean carrying a
   number whose stated accuracy the module cannot honour.
2. **Nothing in scope needs it.** The measurement campaigns this tree exists to run begin with
   IGS (1994), ILRS routine normal points, and modern optical archives. No P1–P6 acceptance
   test touches the pre-1972 era.
3. **Refusal is the plan's rule** (plan §5 constraint 4). The alternative — silently applying
   the historical expressions and reporting a microsecond-labelled result — is exactly the
   plausible-wrong-number class the rewrite exists to design out.
4. **The boundary is sharp and documented**, so the refusal message can be precise, which a
   "degraded accuracy" warning could not be.

This is a decision, not a derivation; it is recorded for the manager as `TIME-Q-001`.

### 4.7 The leap-second table is data with an expiry date

[`LEAP`] carries, in its own header, the line *"File expires on 28 June 2027"*, and is updated
through IERS Bulletin C. A leap second is announced roughly six months ahead; a table fetched
before the announcement cannot know about it. A missed leap second is a **1 s** error — 7.5 km
of along-track position at LEO. It is not a rounding problem; it is a catastrophic one, and it
is silent.

- **TIME-R-050.** The leap-second table MUST be loaded from the IERS file declared in the data
  manifest (plan §2, manifest-driven data layer), with its URL, retrieval date, and content
  hash recorded.
- **TIME-R-051.** The loader MUST parse the file's expiry date and MUST refuse any **UTC**
  operation at an epoch after that date (`TIME-F-004`). Uniform-scale operations are
  unaffected.
- **TIME-R-052.** `TIME-R-051` MAY be overridden by an explicit, per-run option named so that
  its meaning is unmistakable (for example `assume_no_further_leap_seconds`). When set, the
  option MUST be recorded in the run's provenance output alongside the table's identity and
  expiry. It MUST NOT be settable by default, by environment, or by a configuration
  fall-through.

  **This shape is now general.** The manager adopted it on 2026-09-18 as plan rule **R12**: a
  refusal in this tree may have **at most one** named override, that override must be
  unmistakable in its name, it must be set explicitly per run, and it must be recorded in the
  run's provenance. Refusals with no override, several overrides, an environment-settable
  override, or an unrecorded one are all forbidden. `SPEC-template.md` §5 `R-ERR-3` carries the
  rule for every other spec; this requirement is the case it was generalised from.
- **TIME-R-053.** The dependency's own compiled-in leap-second table MUST NOT be relied upon.
  `ERFA` v2.0.1 was released 2023-10-13; its `eraDat` carries a release-year constant and
  returns status `+1` — "dubious year", *together with a computed result* — for years five or
  more after that release, i.e. from 2028 [`ERFA` `src/dat.c`, note 1]. A value returned with
  a warning that the caller ignores is precisely the failure mode this tree is designed
  against. The tree therefore implements UTC↔TAI itself, from the loaded table.
- **TIME-R-054.** If the dependency's table is nevertheless consulted for any purpose, the
  loaded table MUST be compared against it at start-up and a mismatch MUST be a refusal, not
  a warning.

Consequence of `TIME-R-053`, stated so the implementer does not go looking: the operations
this tree implements for itself are UTC↔TAI, UTC calendar↔instant, and GPS↔TAI. ERFA is used
for TDB−TT (`eraDtdb`) and for calendar↔Julian-date arithmetic on the uniform scales
(`eraCal2jd`, `eraJd2cal`). `ERFA` does expose a runtime leap-second override
(`eraSetLeapSeconds`, in `erfaextra.h`), but it is **process-global mutable state**, which
conflicts with the plan's requirement that state not leak between runs (plan §2, §5
constraint 5); it is therefore not used.

### 4.8 The end of leap seconds

CGPM Resolution 4 (2022) resolved to stop the insertion of leap seconds by or before 2035, and
asked for a new, larger maximum value of |UT1 − UTC| to be proposed [`CGPM27-4`; obtained only
as secondary reporting, see `TIME-Q-004`].

- **TIME-R-055.** No interface, validation rule, or format in this module MAY assume
  |UT1 − UTC| < 0.9 s, and none MAY assume that further entries will ever be added to the
  leap-second table. A table whose last entry is decades old MUST be as valid as one updated
  yesterday, subject only to the expiry rule of `TIME-R-051`.

### 4.9 The expiry horizon binds layers above this one

`TIME-R-051`'s refusal is not confined to this module, and the collision it causes was found by
implementation rather than anticipated here. **IERS rapid-service products predict roughly a year
ahead** — `finals2000A.all` does — and routinely extend past the leap-second table's expiry. A
consumer that asked this module for ΔAT on every row of such a file was refused the whole file.

- **TIME-R-056.** A layer that ingests a product extending past the expiry MUST truncate it at
  that horizon, count what it excluded, and raise **its own** out-of-coverage diagnostic for
  epochs beyond it — not propagate `TIME-F-004` upward. A diagnostic about ΔAT, raised when the
  caller asked for something else, names the wrong thing and sends the reader to the wrong
  module. `SPEC-eop.md` §4.6 is the worked instance.
- **TIME-R-057.** Where `TIME-R-052`'s single named override is set for a run, it MUST be visible
  to those layers, so that one declared decision governs the horizon everywhere rather than
  several that can disagree about where the data ends.

This will recur at every layer that ingests a forecast, so it is stated here rather than left to
each of them.

The sanity check on parsed ΔUT1 in `SPEC-eop.md` is bounded at 1 s **for the historical era
only**, for exactly this reason.

---

## 5. Interfaces

Language-free, per `SPEC-template.md` §4. `Result<T, E>` denotes "a T or a diagnostic E,
where the diagnostic cannot be ignored silently".

### 5.1 Types

```
TimeScale   := TAI | TT | TCG | TDB | TCB | UTC | GPS          -- closed; UT1 is NOT a member
Epoch       := opaque { seconds: i64, fraction: f64 }          -- TAI, origin 1958-01-01T00:00:00 TAI
Duration    := transparent { seconds: i64, fraction: f64 }     -- signed SI seconds
LeapTable   := opaque, immutable                               -- loaded from the IERS file
Site        := transparent { east_longitude[rad], u[km], v[km] } -- for eraDtdb's diurnal term
```

- `Epoch` is **opaque**: it cannot be constructed from raw numbers, only through a constructor
  that names a scale. This is how `TIME-R-003` is enforced rather than merely requested.
- `Epoch` is totally ordered, with exact equality.
- `LeapTable` is **immutable after construction**; there is no mutating operation and no
  global instance.

### 5.2 Operations

```
load_leap_table(bytes, source_id)                -> Result<LeapTable, TimeError>
leap_table_expiry(LeapTable)                     -> Epoch                 -- UTC-valid horizon
leap_table_provenance(LeapTable)                 -> { url, retrieved, sha256, bulletin }

epoch_from_calendar(scale, y, m, d, h, min, s[SI s], &LeapTable)
                                                 -> Result<Epoch, TimeError>
epoch_from_two_part_jd(scale, part1[d], part2[d], &LeapTable)
                                                 -> Result<Epoch, TimeError>
epoch_from_gps_week(week, seconds_of_week[SI s]) -> Result<Epoch, TimeError>

calendar_of(Epoch, scale, ndp, &LeapTable)       -> Result<(y,m,d,h,min,s), TimeError>
two_part_jd_of(Epoch, scale, &LeapTable)         -> Result<(f64[d], f64[d]), TimeError>
                                                    -- part1 integral JD, part2 in [0,1)

delta_at(Epoch, &LeapTable)                      -> Result<Duration, TimeError>
tdb_minus_tt(Epoch, Option<Site>)                -> Duration

ut1_two_part_jd(Epoch, dut1: Duration, &LeapTable)
                                                 -> Result<(f64[d], f64[d]), TimeError>
                                                    -- the ONLY route to a UT1 value

add(Epoch, Duration)                             -> Epoch                 -- total, exact
difference(Epoch, Epoch)                         -> Duration              -- total, exact SI seconds
```

Notes on the shapes, for the manager's review:

- **Every operation that touches UTC takes the `LeapTable` explicitly.** There is no ambient
  table. This costs an argument at every call site and buys the guarantee that two runs with
  different tables cannot be confused, and that no run can proceed without declaring which
  table it used.
- **`add` and `difference` do not take a scale or a table** and cannot fail. That is the
  payoff of storing TAI: interval arithmetic is total and exact, and the leap-second
  machinery is confined to the boundary.
- **`ut1_two_part_jd` returns a two-part JD, not an `Epoch`.** It is the only UT1-producing
  operation, it demands ΔUT1 as an argument, and what it returns is immediately consumable by
  `frames` and by nothing else. `TIME-R-001` is enforced by the type, not by a convention.
- **`tdb_minus_tt` cannot fail** and returns a `Duration`; the `Site` is optional because the
  geocentric case is the common one, but the option is present so the SLR path can supply it
  rather than discovering later that it could not (§4.3, `TIME-R-021`).

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `TIME-P-1` | representation resolution | ≤ 1 ns, achieved 2⁻⁵³ s = 0.111 fs | 1 ns × 7.5 km s⁻¹ = **7.5 µm**; achieved 1.11 × 10⁻¹⁶ s × 7.5 km s⁻¹ = **8.3 × 10⁻¹³ m** | §4.2 |
| `TIME-P-2` | round trip external → internal → external | ≤ 1 ns | 1 ns × 7.5 km s⁻¹ = **7.5 µm** at LEO | the budget that makes file I/O lossless |
| `TIME-P-3` | TT ↔ TAI | exact | — | additive integer + 0.184 s |
| `TIME-P-4` | UTC ↔ TAI | exact, including the leap second itself | — | table lookup, integral |
| `TIME-P-5` | TDB − TT | ≤ 10 ns | 10 ns × 7.5 km s⁻¹ = **75 µm** at LEO | `eraDtdb`'s own accuracy for 1980–2050 |
| `TIME-P-6` | pipeline-level epoch fidelity over a multi-decade span | ≈ 1 µs | 1 µs × 7.5 km s⁻¹ = **7.5 mm** at LEO | the requirement this spec was given |

**Three of these rows were wrong until v1.4, all in the sub-microsecond range and all by a
factor of 1000 or more**: `TIME-P-1`'s 1 ns was given as 7.5 pm where it is 7.5 µm, its achieved
figure as 8 × 10⁻¹⁶ m where it is 8.3 × 10⁻¹³ m, and `TIME-P-2`'s as 7.5 nm where it is 7.5 µm.
The cause was scaling down from "7.5 mm per µs" and losing the prefix chain, in exactly the rows
whose numbers are small enough that nobody sanity-checks them. Every row now carries its
multiplication, per `SPEC-template.md` §1's rule for §6, because that is what makes such an error
visible. Note that `tests/toolchain_smoke.cpp` had the arithmetic right the whole time — the
defect was in the prose, where there was no test.

`TIME-P-6` is the stated pipeline requirement; `TIME-P-1` exceeds it by six orders of
magnitude, deliberately, because the cost of doing so is one `i64` and because the budget must
survive being spent by every layer above without this one contributing to it.

The 7.5 mm figure is what disqualifies the obvious representations: it sits below the ILRS
normal-point precision (millimetres) and below IGS final-orbit accuracy (a few centimetres),
which is the standard the campaigns of plan §4 are measured against. A bare `f64` Julian date,
at 0.30 m, is an order of magnitude *worse* than the measurements the pipeline is meant to fit.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `TIME-F-001` | external epoch with no established time scale | the source (file, service, field), and the scales that would have been acceptable | assume UTC; assume TT; assume the scale used last time |
| `TIME-F-002` | UTC or UT1 epoch before 1972-01-01T00:00:00 UTC | the requested epoch, the boundary, and that uniform scales remain available | apply the pre-1972 rate expressions; clamp to 1972; return ΔAT = 10 |
| `TIME-F-003` | seconds field 60 on a day with no leap second | the date, the seconds field, and the ΔAT in force on that date | accept it as 00:00:00 of the next day; accept it as 23:59:59 |
| `TIME-F-004` | UTC epoch after the loaded table's expiry | the requested epoch, the table's expiry date, the bulletin number, and the override option's name | extrapolate the last ΔAT; warn and continue |
| `TIME-F-005` | leap-second file unparseable, non-monotonic in MJD, or with a non-integral ΔAT at or after 1972 | the file identity, the offending line number and its content | skip the bad line; fall back to a built-in table |
| `TIME-F-006` | GPS week supplied without an unambiguous epoch (bare 10-bit week) | the week value and the rollover ambiguity it creates | pick the rollover nearest "now" |
| `TIME-F-007` | calendar field out of range (month 13, day 32, hour 24, negative seconds) | the field, its value, and its valid range | normalise by carrying into the next field |
| `TIME-F-008` | a dependency returns a non-zero status | the dependency, the routine, the status value, and the epoch | discard the status and use the returned number |

`TIME-F-008` is the general form of `R-ERR-2` from `SPEC-template.md` §5, and its concrete
instance is `eraDat`'s `+1` "dubious year", which returns a usable-looking number alongside the
warning [`ERFA` `src/dat.c`, note 1]. Under this spec the tree does not call `eraDat`
(`TIME-R-053`), but the rule binds every other dependency call.

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `TIME-A-001` | TT − TAI at 20 epochs spanning 1958–2050 | 32.184 s | `TN36-10` §10.1 — published constant | exact (bit-identical fraction) | R-020, P-3 |
| `TIME-A-002` | ΔAT at each of the 28 rows of the leap-second table (1972-01-01 through 2017-01-01), evaluated one second before and one second after each step | the table's values | `LEAP` — published table | exact | R-030, P-4 |
| `TIME-A-003` | TAI ↔ UTC round trip at 1-second steps across each of the 27 leap seconds, including the 23:59:60 label | identity | closed-form identity | exact | R-010, R-030, R-034 |
| `TIME-A-004` | the 2016-12-31 worked case of §4.5 | the three (UTC label, TAI) pairs tabulated there | derived from `LEAP` | exact | R-031, R-032 |
| `TIME-A-005` | `difference()` across the 2016 leap second, from 2016-12-31T23:59:59 UTC to 2017-01-01T00:00:00 UTC | 2 s | closed-form identity — two SI seconds elapse across the 61-second minute | exact | R-034 |
| `TIME-A-006` | TAI − GPS | 19 s, at 20 epochs from 1980 to 2050 | derived from `LEAP` (§4.4) + the GPS epoch | exact | R-023 |
| `TIME-A-007` | GPS 1980-01-06T00:00:00 GPS equals UTC 1980-01-06T00:00:00 | identity | definition of the GPS epoch | exact | R-023 |
| `TIME-A-008` | round trip `Epoch` → (scale, calendar, 12 decimal places) → `Epoch`, for 10⁵ pseudo-random epochs in 1972–2050 and each scale | identity | closed-form identity | < 1 ns max, < 0.1 ns RMS | P-2, R-011, R-012, R-013 |
| `TIME-A-009` | TDB − TT over 1980–2050, geocentric | matches `eraDtdb` | `ERFA` — the routine we call; this test pins the call convention, not the model | bit-identical | R-021, P-5 |
| `TIME-A-010` | TDB − TT peak-to-peak amplitude over one year | ≈ 3.4 ms peak-to-peak (±1.7 ms) | `TN36-10` §10.1, "maximum amplitude of around 1.6 ms" for *P(TT)* | within a factor 1.1 — a magnitude check, not a model check | R-021 |
| `TIME-A-011` | TDB − TT diurnal term for a station at 45° N, 0° E | differs from the geocentric value by ≈ 2 µs peak-to-peak | `eraDtdb` documentation; a magnitude check | within a factor 1.2 | R-021, R-021a |
| `TIME-A-012` | `two_part_jd_of()` output shape | part1 integral, part2 ∈ [0, 1) | `TIME-R-014` | exact | R-014 |
| `TIME-A-013` | refusal: UTC at 1971-12-31T23:59:59 | refusal `TIME-F-002` naming that epoch and the 1972 boundary | this spec | — | F-002 |
| `TIME-A-014` | acceptance: TT at 1965-06-01 | succeeds | this spec, `TIME-R-041` | — | R-041 |
| `TIME-A-015` | refusal: UTC at (table expiry + 1 day) | refusal `TIME-F-004` naming the expiry and the override option | this spec | — | F-004, R-051 |
| `TIME-A-016` | with `assume_no_further_leap_seconds` set, the same call succeeds and the run's provenance records the option, the table hash, and the expiry | as stated | this spec | — | R-052 |
| `TIME-A-017` | refusal: seconds field 60 on 2016-06-30 (no leap second) | refusal `TIME-F-003` | `LEAP` — there is no 2016-06-30 entry | — | F-003 |
| `TIME-A-018` | refusal: leap file with a duplicated or decreasing MJD | refusal `TIME-F-005` naming the line | this spec | — | F-005 |
| `TIME-A-019` | no global state: two `LeapTable`s differing in their last entry, used alternately in the same process, give the two corresponding answers on every call | as stated | this spec | exact | R-050, R-ERR-1 |
| `TIME-A-020` | monotonicity: for 10⁶ ordered pairs, `a < b` ⟺ `difference(b,a) > 0` | identity | closed-form | exact | R-012, R-013 |
| `TIME-A-021` | **structural:** an `Epoch` cannot be constructed without naming a scale, and no constructor supplies a default | compilation failure, or a boundary refusal, for every attempt | this spec | — | R-003, R-004 |
| `TIME-A-022` | **structural:** there is no operation producing a stored UT1 epoch, and every UT1-producing operation takes ΔUT1 as an argument | as stated — verified against the module's exported surface | this spec | — | R-001, R-002 |
| `TIME-A-023` | refusal: an external epoch presented with no time scale | refusal `TIME-F-001` naming the source and the acceptable scales | this spec | — | F-001 |
| `TIME-A-024` | refusal: calendar fields 2017-13-01, 2017-02-30, hour 24, seconds −1 | refusal `TIME-F-007` naming the field, value and valid range in each case; **no carrying into the next field** | this spec | — | F-007 |
| `TIME-A-025` | display rounding: render 2016-12-31T23:59:59.9999999 UTC and 2017-06-30T23:59:59.9999999 UTC to 3 decimal places | the first may render `23:59:60.000`; the second MUST NOT, and neither may render `24:00:00` | this spec, `TIME-R-033` | exact | R-033 |
| `TIME-A-026` | refusal: **UT1** requested at 1971-06-01 with a ΔUT1 supplied | refusal `TIME-F-002`, on UT1 and not only on UTC | this spec, `TIME-R-040` | — | R-040 |
| `TIME-A-027` | if the dependency's leap table is consulted at all, a deliberately divergent loaded table is a refusal at start-up, not a warning | refusal | this spec, `TIME-R-054` | — | R-054 |

**Coverage.** Every requirement and refusal in this spec is discharged by at least one row
above, except the following, which are listed in full so that the gap is a decision rather than
an oversight:

| id | why no test |
|---|---|
| `TIME-R-010` (partial) | "storage is TAI" is not observable from outside the module. `TIME-A-003` discharges it through its consequences and names it; this row records WHY a direct test is impossible, which the acceptance row cannot say in its own width. |
| `TIME-R-024` | An interface fact about ERFA (it declares no GPS routines), not a behaviour of this module. Discharged by the dependency register in `PROVENANCE.md` §3. |
| `TIME-R-053` | A design constraint — "do not call `eraDat`, do not use the global leap-table setters". Discharged by review of the dependency register, and by `TIME-A-019`, which fails if any global table is in play. |
| `TIME-R-055` | A negative property of the interface. The corresponding positive check is `SPEC-eop.md` `EOP-A-011`. |
| `TIME-R-056`, `TIME-R-057` | Obligations on the layers **above** this one, so they are discharged where they bind: `SPEC-eop.md` `EOP-A-033` and `EOP-A-034` test both against the real `finals2000A.all`. |
| `TIME-F-008` | The general form of `SPEC-template.md` `R-ERR-2`. It binds every dependency call and is discharged by review; the one concrete instance it was written for (`eraDat`'s `+1`) does not arise, because `TIME-R-053` removes the call. |
| `TIME-F-006`, `TIME-S-025` | GPS week/time-of-week support is optional in P1. Both are untested until a consumer exists, and MUST be tested when one does. |

---

## 9. Provenance obligations

When this module is implemented, the following are added to `PROVENANCE.md`:

- **Module register:** `time` → the sources of §2, at the versions recorded there, and this
  spec at the version implemented.
- **Parameter register:** 32.184 s; *L*_G; *L*_B; TDB₀; *T*₀; 19 s; the 28 rows of ΔAT; the
  leap file's expiry date and bulletin number — each with its source document and date.
- **Dependency register:** ERFA, version, BSD 3-clause, and the list of routines actually
  called (which under `TIME-R-053` excludes `eraDat`).
- **Data manifest entry:** `Leap_Second.dat` with URL, retrieval timestamp, SHA-256, and
  expiry.

---

## 10. Open questions for the manager

| id | question | recommendation / **resolution** |
|---|---|---|
| `TIME-Q-001` | **Pre-1972 UTC: refuse, or support with a stated accuracy?** §4.6 refuses. The cost is that an archival optical observation from the 1960s could not be ingested without a policy change. | **CONFIRMED 2026-09-18 — refuse.** No P1–P6 acceptance test needs the range, the accuracy claim could not be honoured, and the boundary is sharp enough for a precise diagnostic. If archival data later forces the issue, the right answer is a separate, explicitly-labelled "historical UTC" path, not a relaxation of this one. |
| `TIME-Q-002` | **IS-GPS-200 was not obtained.** The 19 s offset is derived in §4.4 from `LEAP` plus the publicly documented GPS epoch, which is sound, but the definitive statement lives in a document I did not retrieve. | Retrieve IS-GPS-200 before P5 (the SGP4/TLE and GNSS I/O phase) and cite it directly. The constant will not change; the citation's strength will. |
| `TIME-Q-003` | **`eraDtdb` needs the observer's site.** The SLR path must supply it (§4.3). Should `Site` be threaded through the epoch API, as specified, or should TDB be computed only in the measurement layer where the site is naturally in hand? | As specified — the optional argument here, defaulted to geocentric. Moving it to the measurement layer duplicates the series evaluation and makes the 2 µs diurnal term easy to forget. But this is an interface decision that touches `measmod`, so it is flagged rather than settled. |
| `TIME-Q-004` | **CGPM Resolution 4 (2022) was obtained only as secondary reporting.** The BIPM page is cited but its text was not retrieved; the statements in §4.8 rest on consistent secondary summaries. | Retrieve the resolution text before the spec is adopted. Nothing in §§4–8 *depends* on it — `TIME-R-055` is defensive either way — but a normative source in §2 marked `secondary` is a weakness the ledger should not carry unnecessarily. |
| `TIME-Q-005` | **Representation origin.** §4.2 fixes the origin at 1958-01-01 TAI. J2000 (2000-01-01T12:00:00 TT) would make `f64` interop marginally tidier and is the more common choice in astrodynamics. | **CONFIRMED 2026-09-18** as part of the adopted representation decision: 1958-01-01 TAI keeps the seconds count non-negative over the whole supported range, which removes a class of sign errors in serialisation and in integer division. The choice is invisible outside the module, so it is cheap to revisit. |
| `TIME-Q-006` | **Is "roughly a microsecond over multi-decade spans" the right budget?** §6 treats it as given. It was set before this analysis. | Keep it as the *pipeline* budget but note that this module now costs nothing against it (§6, `TIME-P-1`). The real consumers of that budget are the ephemeris interpolation and the measurement time-tagging, and the budget should be re-apportioned when those specs are written rather than spent here. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.4 | 2026-09-18 | **Precision audit.** `TIME-P-1` (twice) and `TIME-P-2` corrected — each was wrong by 1000× or more in the sub-microsecond rows. Every row in §6 now carries its multiplication rather than only its result. Found by auditing all twenty-three budget rows across the four specifications after one error was caught in `SPEC-ephemerides`. |
| 1.3 | 2026-09-18 | **Amended after implementation.** `TIME-R-021a` added: `eraDtdb` needs UT1, which §5's signature does not supply, and UTC's day fraction stands in for it at a cost of under 0.2 ns. §4.9 added — the expiry horizon binds the layers above, since rapid-service products routinely predict past it — with `TIME-R-056` and `TIME-R-057`, discharged in `SPEC-eop.md` where they bind. |
| 1.2 | 2026-09-18 | **Acceptance coverage completed.** Added `TIME-A-021` … `TIME-A-027` and the §8 *Coverage* table listing every requirement and refusal not discharged by a test, with the reason. v1.0–1.1 claimed the template's coverage rule without meeting it. **No requirement was added, removed or changed**; the adopted requirement set is exactly as at v1.1. |
| 1.1 | 2026-09-18 | **Adopted.** All six design decisions of the tranche adopted unchanged. `TIME-R-052`'s named-and-logged override shape generalised to plan rule **R12**, binding every refusal in the tree. `TIME-Q-001` (pre-1972 refusal) and `TIME-Q-005` (representation origin) confirmed; `TIME-Q-002`, `-003`, `-004`, `-006` remain open. |
| 1.0 | 2026-09-18 | First draft, P1 tranche, for manager review. |
