# SPEC-io-sgp4 — L6 step 3: SGP4/SDP4, TEME output

| | |
|---|---|
| **Spec ID** | `IOSG` |
| **Status** | **draft** 2026-09-29, revised 2026-10-06, for review — written after the port (plan §3 step 1's own order reordered, not skipped: requirements describe behaviour, not code structure, and a first-principles port this size carried too many open questions to write requirement IDs against in advance; manager's own instruction, `plan/subplan_L6/L6-3.md`). The 2026-10-06 revision records the branch-by-branch fixes the manager's later ruling asked for (§9.2) and what they did not resolve (§10). **Second revision, 2026-10-06, same day:** the comparison reader was found defective — eight revolution numbers glued to their mean-motion field were read as extra decimals of it (§9.2, `IOSG-A-009`) — which removed five of the eleven "unexplained" residuals and let the six that were real be worked; they were resonance, now fixed (§3.8). The gate is met; what stays open is §10. **Third revision, 2026-10-06:** `T-01`'s gate (§9.3, `IOSG-R-012`) — the element set and the two Horizons tables vendored, the prediction written and frozen (`IOSG-A-010`) before either table is compared with anything; the comparison is the next commit |
| **Version** | 1.3 |
| **Date** | 2026-10-06 |
| **Layer** | L6 `io-measurements` (`../plan/PLAN.md` §3.7), step 3 (`sgp4`) |
| **Depends on** | `core` (`odl::Result`, `Vec3`), `time` (`Calendar`, `Epoch`, `LeapTable`), `frames` (`TemeState`, `to_gcrs` — TEME conversion is L1's, never this module's own) |
| **Depended on by** | L6 step 4 (`measmod`); oracle case `T-01`'s own required-disagreement gate |

**Derivation declaration (plan R1).** Written from `STR3` and `VAL06` (§2) and from no other
implementation of SGP4/SDP4 — specifically, never from Vallado's own 2006 reference code (the
archive's `sgp4/cpp` etc., and the source reprinted in `VAL06`'s own later appendix, found but not
opened — §2). Ruled (`plan/subplan_L6/L6-3.md`, manager, 2026-09-29): `STR3`'s own FORTRAN IV
listing is part of its specification, a public-domain 1980 government report defining SGP4 by what
its own code does (rule 8), not a secondary implementation of laws published elsewhere the way
`eclips.f` was — so it is read and ported from directly, equations and FORTRAN together, with every
correction `VAL06`'s own text describes applied on top and cited to its own location.

**One exposure, recorded rather than omitted (2026-10-06).** A whole-file `grep` over `VAL06`'s
extracted text, run to find the phrase "drop" in the prose, also matched two lines of the code
appendix and printed them: `if (satrec.ecco > 1.0e-4)`, twice. Nothing else of that appendix or of
the archive's code was displayed. The threshold they carry is the one the verification file's own
comment on satellite `28057` already states ("ecc = 8.84E-5 (< 1.0e-4)"), read before the match, and
the value used (`kLowEccTol`) is cited to that comment, not to the matched lines. Every later search
of that text was bounded to the prose (lines before the appendix).

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened. `oracle/ORACLE.md` and
`oracle/cases.tsv`/`environment.txt` were read (permitted, `ORACLE.md` §1); `oracle/capture.sh` was
not.

---

## 1. Purpose and scope

One propagator: a TLE (`odl::io::Tle`, `SPEC-io-formats.md`) and a target time in, a TEME position
and velocity out. `sgp4_init` (once per TLE) plus `sgp4_propagate` (per query) are the algorithm's
own two-phase shape, `STR3`'s own `IFLAG` init/non-init split expressed as a returned value instead
of a `COMMON` block — no global state anywhere in this module. `propagate_teme` is the ergonomic
entry point: a `time::Epoch` and a `LeapTable` in, a `frames::TemeState` out, `tsince` computed as
calendar-elapsed time (§3.4), never through `Epoch`'s own TAI arithmetic.

**Not in scope**: SGP, SGP8, SDP8 (`STR3`'s own other three models — D4 names `sgp4` only, and no
oracle case or battery case needs them). WGS-84 constants (`VAL06` §VI.B names them as an
alternative but states "we use WGS-72 as the default value", matching the FAQ's own recommended
AFSPC-compatible settings, `PROVENANCE.md` §38.1). Frame conversion beyond raw TEME — `to_gcrs`
(L1, `SPEC-frames.md` `FRAME-R-030`) is the only route onward, and this module performs no rotation
of its own. `T-01`'s own gate (its own design is ruled, `../plan/PLAN.md` §4 rule 1 and
`plan/subplan_L6/L6-3.md`; building it is this step's own remaining work, §9.3).

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `STR3` | Hoots, F. R., Roehrich, R. L. | *Spacetrack Report No. 3: Models for Propagation of NORAD Element Sets* | December 1980 | `https://celestrak.org/NORAD/documentation/spacetrk.pdf`, SHA256 `0ac48df7724b857c14431764a7baa0bf9aa64e570e3941b7244a15cdaea708bd` | **primary**, obtained in full, ruled normative including its own FORTRAN (above) | §6 SGP4, §7 SDP4, §10 the shared deep-space subroutine `DEEP` (`DPINIT`/`DPSEC`/`DPPER`), §11 WGS-72 constants and the driver's own near-earth/deep-space dispatch, §13 the published test cases this spec's own acceptance tests use directly (§8) |
| `VAL06` | Vallado, D. A., Crawford, P., Hujsak, R., Kelso, T. S. | *Revisiting Spacetrack Report #3*, AIAA 2006-6753 Rev 2 | 2006 | `https://celestrak.org/publications/AIAA/2006-6753/AIAA-2006-6753-Rev2.pdf`, SHA256 `538a5c0ea174eb569bbc258011717142a26d72871ab07cc64ee5fa3773164b16` | **primary**, obtained in full (its own later-appendix reference-code pages found but not read, `PROVENANCE.md` §38.4; the one exposure is declared above) | §II.E time system, §II.F GHA calculation choice, §III corrections list (seven bullets), §VI.A-E named corrections and their own locations, §VII comparison analyses and conclusions, Table 1 (§V) the battery's own case descriptions and expected error-trap behaviour, Appendix C the worked TEME example, Appendix D the verification-file trailer format, Appendix E's header (the results were produced with the paper's `'a'`, `'72'` options "to best emulate AFSPC operation") |
| `vallado-sgp4-verification-vectors` | Vallado et al. | `SGP4-VER.TLE` + 32 `.e` STK ephemerides | distributed with `VAL06` | `manifest.json`, `VALLADO-UNRESTRICTED`, `PROVENANCE.md` §38.2 | **primary**, the published verification battery; `SGP4-VER.TLE`'s own `#` comment lines are the authors' words on what each case exercises and are cited as such | §8's own `IOSG-A-001` and `IOSG-A-007` |

Both `STR3` and `VAL06` are `kind: literature` in `manifest.json` (`PROVENANCE.md` §38.1) — cited,
never redistributed; any printed constant or test value this module's own code or tests carry is
hand-transcribed with its own citation, the paper itself is never a build input.

---

## 3. Definitions and conventions

### 3.1 No internal `Epoch`; the TLE's own epoch-year pivot is this module's own choice, not part
### of SGP4's mathematical definition

Like every reader in `SPEC-io-formats.md`, `sgp4_init`/`sgp4_propagate` never construct an
`odl::time::Epoch`. `tle_epoch_calendar` converts `Tle::epoch_year`/`epoch_day` into a
`time::Calendar` by pivoting the two-digit year at 57 (00-56 -> 2000-2056, 57-99 -> 1957-1999) —
`VAL06` §II.E states plainly this choice is "only peripherally related to SGP4... not part of the
mathematical definition" and itself names no single settled value ("less than 50, 57, or some other
value"); 57 is chosen because no real TLE epoch can predate Sputnik 1 (1957), the one of `VAL06`'s
own two named options with a physical justification the other lacks.

### 3.2 The near-earth/deep-space dispatch, and the resonance sub-dispatch, are threshold tests on
### the RECOVERED mean motion, computed once

`STR3` §11: period >= 225 minutes (`2*pi/xnodp >= 225`, after the shared mean-motion recovery,
§3.5) selects deep-space. Within deep-space, `STR3` §10's own two numeric bands select resonance:
`n` in `(0.0034906585, 0.0052359877)` rad/min (~1436 min, sidereal-day period) is synchronous (24 h,
geostationary); `n` in `[8.26e-3, 9.24e-3]` rad/min with `e >= 0.5` is the 12-hour (Molniya-style)
case; neither band is no resonance. `STR3`'s own driver recomputes this same mean-motion-recovery
formula a second time (once for dispatch, once inside whichever of SGP4/SDP4 it calls) — this
module computes it once, in `sgp4_init`, and dispatches from that single result (`VAL06` §III's own
praised "merging" of SGP4 and SDP4 initialization, not a formula change).

### 3.3 The deep-space periodics: three inclination tests, and where the sign swap sits

Three DIFFERENT inclination-dependent decisions live in the deep-space code, and each reads a
different inclination; conflating them is the risk this section exists to prevent.

1. **`DPINIT`'s low-inclination node-term zeroing** (`STR3` §10, `IF(XQNCL.LT.5.2359877E-2)
   SH=0.0`): the EPOCH inclination, tested once, below ~3 deg; never re-evaluated.
2. **`DPPER`'s Lyddane choice** (labels 218/220): inclination `< 0.2` rad (~11.46 deg) takes the
   Lyddane/`ACTAN` route, specifically to avoid a divide-by-near-zero the direct route's own
   `PH/SINIQ` step would hit there; `>= 0.2` rad takes the direct route. `STR3` tested the fixed
   epoch value (`XQNCL`); `VAL06` §III fifth bullet and its "Option (b)" test the PERTURBED
   inclination at every call. "Perturbed" here includes the periodic `PINC` (secular-only and
   epoch-only each put the two battery cases that straddle 0.2 rad — `04632`, `14128` — 7.7 km and
   1.7 km out).
3. **The negative-inclination swap** (`STR3`'s own `XINC=-XINC, XNODES+PI, OMGASM-PI`): `STR3`
   applied it inside `DPSEC` to the secular inclination alone. `VAL06` §VI.C moves it "before the
   'long period periodics' section" — after `DPPER`, on the fully perturbed inclination — and calls
   swapping early "correcting negative inclination prematurely" (its satellites `25954`, `28626`).

**`DPPER`'s own sines and cosines of the inclination** (`SINIS`/`COSIS` in the Lyddane route,
`SINIQ`/`COSIQ` in the direct route) and **the short- and long-period coefficients the tail takes
from the epoch inclination** (`XLCOF`, `AYCOF`, `X3THM1`, `X1MTH2`, `X7THM1`, `COSIO`, `SINIO`) are
all taken from the PERTURBED inclination: `VAL06` §III seventh bullet, "the initialization of
deep-space terms based on perturbed values... corrected in the DPPER and SGP4 routines... includes
any terms based on the Keplerian orbit being re-computed based on the new perturbed values."

**The Lyddane route's node.** `XNODES` is reduced mod 2*pi into `[0, 2*pi)` before use (so the
`COSIS*XNODES` and `PINC*XNODES*SINIS` terms, which use it outside a trigonometric expression, see
the same node on both sides), the new node is taken from `ATAN2` into `[0, 2*pi)`, and then moved by
+/-2*pi to the representation nearest the reduced original — `VAL06` §III fifth bullet (the GSFC
code's "IF statements at the end of the 'apply periodics' section... evaluate the relative quadrant
of the resulting angle", "a similar problem exists with the modulo 2*pi reduction of the XNODE
variable") and its "intrinsic functions" bullet.

### 3.4 `tsince` is calendar-elapsed time, not TAI-elapsed time

`propagate_teme` computes `tsince_minutes` via `calendar_elapsed_seconds` (`sp3_ephemeris.hpp`) on
the TLE's own epoch calendar and the target `Epoch`'s own UTC calendar — never via `Epoch::difference`
(TAI seconds). `VAL06` §II.E: "It is doubtful that a leap-second capability was implemented into the
peripheral software for SGP4 since the historical source code uses relative 'time since epoch'... any
such addition is clearly outside the mathematical formulation of SGP4." Going through TAI would
silently shift `tsince` by one second across a leap second the two epochs straddle — a real,
findable defect this convention exists to avoid rather than to state as a caveat.

### 3.5 Constants and the recovered mean motion

WGS-72 (`VAL06` Table 2). Derived constants at full double precision, per `VAL06` §III second
bullet ("the move to double-precision code throughout... corresponding increase in accuracy for
certain astrodynamic constants"): `XKE` and `THDT`/`RPTIM` as `VAL06` Table 2 gives them, and
`TOTHRD = 2/3` rather than `STR3`'s `.66666667` (invisible in a low orbit; 0.4 m at geostationary
radius — every near-GEO case in the battery carried exactly that offset until this changed).
Embedded deep-space constants (`ZNS`, `C1SS`, `ROOT22`, ...) stay at `STR3`'s own printed precision
(`VAL06` §VI.B: "We do not believe any update has occurred to any of the embedded constants in the
deep-space portions").

**The semi-major axis `AODP`.** `STR3` writes `AODP=AO/(1.-DELO)`. This module takes the
Kepler-consistent `(XKE/XNODP)^(2/3)` of the same recovered mean motion. The two agree to ~1e-13
for every ordinary orbit and differ at `O(DELO^2)` only when the J2 correction is large; satellite
`33333` (e = 0.995, `DELO` = -0.108) is the one battery case where they differ visibly (`PINVSQ`
0.142% apart), and only the Kepler-consistent value reproduces its rows. **This is determined by
the vectors; neither text states it** — §10 `IOSG-Q-003`. Not resting on `33333` alone: with `STR3`'s
definition `29141` sits 1.6 mm, `28350` 0.22 mm, `22312` 43 µm, `28623` 19 µm and `28872` 16 µm from their
published rows, and with this one all five sit at 6-7 µm (the files' print resolution).

### 3.6 The Kepler solve

Tolerance 1e-12 (`VAL06` §III third bullet: `STR6` "changed the tolerance to 10-12 (commensurate
with double-precision work)"); each Newton correction limited to +/-0.95 (`VAL06` §VI.E: "an even
simpler option fixes a limit of 0.9 - 1.0 for the maximum correction"; 0.9 and 1.0 give bit-identical
results over the battery); `STR3`'s own 10-iteration budget. `VAL06` §VI.E's other option, Crawford's
+/-e bound, was tried first and is wrong as written for this port: at that point `e` is the
drag-decayed eccentricity, but the bound belongs to the effective eccentricity `sqrt(axn^2+ayn^2)`,
which the long-period `aynl` term can push above `e` for near-circular decaying satellites.

### 3.7 Eccentricity handling

Three behaviours `STR3` does not have, in the order they fire:

1. **Low-eccentricity drag terms.** Below `e = 1.0e-4`, `C3` (and `OMGCOF` through it) and `XMCOF` —
   the drag terms that divide by the eccentricity — are zero. `VAL06` Table 1, satellite `28057`:
   "certain drag terms are set to zero to avoid math errors / loss of precision"; the verification
   file's own comment: "ecc = 8.84E-5 (< 1.0e-4) / drop certain normal drag terms". WHICH terms is
   this module's reading (the two that divide by the eccentricity), and the vectors confirm it now
   that `28057` is read correctly: zeroing exactly those two leaves `28057` 7 µm from its published
   rows; none zeroed leaves 16 mm, `C3` alone 14 mm, `XMCOF` alone 1.7 mm, those two plus `C5` 31 mm.
2. **Floor and trap on the drag-modified eccentricity** (`E=EO-TEMPE`, deep space `E=EM-TEMPE`
   before `DPPER`): below `-0.001` the call is refused (`IOSG-F-003`); between `-0.001` and `1.0e-6`
   the eccentricity is held at `1.0e-6`. The trap value is read off `VAL06` Table 1 ("propagation
   beyond approximately 1460 minutes should result in error trap (modified eccentricity too low)")
   against the published rows (§8 `IOSG-A-007`); the floor value is determined by the vectors
   (`22312`, `33335`) and is sharp: 5% either side is 4 m off at `33335` — §10 `IOSG-Q-002`; `28350`,
   whose own residual was the reader defect until 2026-10-06, is a third satellite on the same V
   (0.64 m at 9.5e-7 and at 1.05e-6, 6 µm at 1.0e-6).
3. **After `DPPER`**, a perturbed eccentricity outside `[0, 1)` is refused (`IOSG-F-004`), and in the
   shared tail a semi-latus rectum not greater than zero is refused (`IOSG-F-005`) — the
   verification file's own comments on `33334` ("check error code 3... ep never goes below zero")
   and `33333` ("check error code 4").

### 3.8 Geopotential resonance (12 h and 24 h)

`STR3` §10's resonance blocks in `DPINIT` and `DPSEC`'s integrator are ported as printed, with two
exceptions. Both were found by the 2 cm gate once the comparison reader was repaired (§9.2); both
are recorded with their evidence in `PROVENANCE.md` §38.8.

1. **The Greenwich hour angle at epoch (`THGR`) is `VAL06` §II.F's 1970-epoch form**, not `STR3`'s
   `THETAG` (1950 epoch, linear in `DS50`): `THGR = DMOD(THGR70 + C1*DS70 + C1P2P*TFRAC +
   TS70*TS70*FK5R, TWOPI)` with the constants §II.F prints, `DS70`/`TFRAC` the whole and fractional
   days since 1970 Jan 0.0, and `TS70 = DS70 + TFRAC` (the paper does not define `TS70`; that
   reading agrees with the IAU-1982 GMST polynomial, eq. (2), to 1.4e-9 rad at every epoch tried).
   §II.F says the versions "yield 'essentially' the same values"; they differ by 6.75e-6 rad
   (0.0004 deg) at a 2006 epoch and 4.3e-6 rad at 1980, and the resonance phases read it. **Cited to
   the text, and confirmed by the vectors**: a free additive shift fitted to each 12 h case alone
   returned 6.751, 6.746, 6.746 and 6.748 e-6 rad against 6.748e-6 computed from the two printed
   formulas, and with the formula all twelve resonant satellites agree with their published rows to
   0.07 mm or better (from 2.8 to 36 cm).
2. **`G520`'s e^2 coefficient in the `e <= 0.65` branch is 5740.032, not the printed 5740.**
   **Determined by the vectors, not by either text.** `STR3` prints `-5740*EQSQ` (checked on the page
   image, p. 63); `VAL06` is silent. A one-parameter fit of that coefficient alone to the one
   satellite that exercises the branch, `26975`, from a blind scan of 5739.9-5740.1, is V-shaped with
   its minimum at 5740.0320 +/- 0.0001, where the satellite sits 14 µm from its published rows (7.6 cm
   with 5740). One satellite is the only evidence — §10 `IOSG-Q-006`.

The integrator is `VAL06` §VI.D's: restarted from the epoch on every call, 720-minute steps, a final
partial step by the same two-term Taylor series (a full step and a partial step of the same length are
algebraically identical, so a time at an exact multiple of 720 minutes carries no special case).
`THDT` (`RPTIM`) cancels from the mean longitude and enters only the resonance phase.

---

## 4. Required behaviour

- **IOSG-R-001.** The near-earth/deep-space and resonance dispatches are computed once, from the
  shared mean-motion recovery, in `sgp4_init` — §3.2.
- **IOSG-R-002.** The three deep-space inclination decisions (§3.3) read the inclinations §3.3
  names — the low-inclination node-term zeroing the epoch value once, the Lyddane choice the
  perturbed inclination on every call, the sign swap the perturbed inclination after the periodics —
  not conflated.
- **IOSG-R-003.** `tsince` is calendar-elapsed time between the TLE's own epoch and the target,
  never `Epoch`'s own TAI difference — §3.4.
- **IOSG-R-004.** `sgp4_init` carries no state across calls; `sgp4_propagate`'s own deep-space
  resonance integration is re-derived from `ATIME=0` on every call, never resuming a prior call's
  own integrator state (`VAL06` §VI.D, also structurally forced by `IOSG-R-004` itself).
- **IOSG-R-005.** TEME is this module's only output frame. Conversion onward is `odl::frames::to_gcrs`
  (L1); no frame rotation of any kind is performed here.
- **IOSG-R-006.** `DPPER`'s own inclination sines and cosines, and the tail's inclination-dependent
  coefficients, come from the perturbed inclination; the Lyddane route reduces the node mod 2*pi
  and returns it in the quadrant nearest the original — §3.3.
- **IOSG-R-007.** Constants at the precision §3.5 states, the recovered semi-major axis as §3.5
  states, the Kepler solve as §3.6 states.
- **IOSG-R-008.** The eccentricity behaviours of §3.7, in that order.
- **IOSG-R-009.** No call returns a non-finite state as success: every call either returns a fully
  finite state or refuses with a named `IOSG-F-...` diagnostic.
- **IOSG-R-010.** The Greenwich hour angle at epoch is `VAL06` §II.F's 1970-epoch form, not `STR3`'s
  `THETAG` — §3.8.
- **IOSG-R-011.** The `e <= 0.65` branch of the 12-hour resonance takes `G520`'s e^2 coefficient as
  5740.032 — §3.8 (determined by the vectors, one satellite).
- **IOSG-R-012.** **`T-01`'s gate.** This tree's SGP4 → TEME → GCRS chain, applied to a fresh ACS3 element set, is compared with a JPL
  Horizons vector table of the same object; the difference, fitted as a rotation vector Ω (GCRS axes) and a time shift τ, is
  classified against four cases predicted, and frozen (`IOSG-A-010`), before either table was compared with anything: **A** (agreement:
  a consistent, pole-corrected legacy chain), **B** (a 50 mas tilt of the pole: no offsets), **C** (a 113 mas rotation about the pole:
  offsets in the nutation, the equation of the equinoxes from the model) and **D** (the manual's own −53 mas at face value: a 53 mas
  rotation about the pole, westward, which follows from no consistent chain). **A match within the pre-registered 2.0 mas band (§9.3,
  `PROVENANCE.md` §38.12) to A, B or C passes** — the tree's pipeline has predicted Horizons' output and the capture names Horizons' chain;
  **a match to D, or to none, is a finding** reported to the manager; **a fit residual above 0.25 m rms is inconclusive**. Which table, and
  τ (reported, not gated), are as `PROVENANCE.md` §38.12 fixes them. The committed comparison pins the observed case by name, as a
  regression test; the verdict is the classification, in the report.
  > *Superseded by the manager's ruling `e49b33b` (2026-10-06, before any state vector was compared), kept visible — the requirement as first
  > frozen (`a862214`):* "MUST land within the pre-registered 2.0 mas band of one of four predicted cases — A …, B …, C … or D (… — the
  > documented case, the one asserted) — … Matching B or C instead of D is a finding, not a pass; agreement (A) where a disagreement was
  > required is a failure of the required disagreement; both are reported as that to the manager, not absorbed into the band."

---

## 5. Interfaces, stated language-free

- `Sgp4Error = odl::Diagnostic`.
- `tle_epoch_calendar(Tle) -> time::Calendar` — §3.1.
- `Sgp4InitialState` — every quantity `STR3`'s own `COMMON` blocks hold, by value; opaque in
  intent (field names match `STR3`'s own FORTRAN variables so the port is checkable line by line,
  not because a caller is meant to read them).
- `sgp4_init(Tle) -> Result<Sgp4InitialState, Sgp4Error>` — refuses `IOSG-F-001` near 180 deg
  inclination.
- `sgp4_is_deep_space(Sgp4InitialState) -> bool`.
- `struct Sgp4RawState { double x_km, y_km, z_km, xdot_km_s, ydot_km_s, zdot_km_s; }`.
- `sgp4_propagate(Sgp4InitialState, tsince_minutes: double) -> Result<Sgp4RawState, Sgp4Error>` —
  refuses `IOSG-F-001` (perturbed inclination near 180 deg), `-002` (decay), `-003`, `-004`, `-005`
  (§7).
- `propagate_teme(Tle, time::Epoch, time::LeapTable) -> Result<frames::TemeState, Sgp4Error>`.

---

## 6. Precision and accuracy

- **IOSG-P-1.** `VAL06` states no single numeric tolerance for agreement with its own published
  `.e` verification files — checked directly, not assumed absent (§2). `STR3` §13's own footnote,
  for its OWN printed test cases specifically, states: "generated on a machine with 8 digits of
  accuracy. After a one day prediction, the test cases have only 5 to 6 digits of accuracy". This
  spec holds the port to **50 m** against `STR3`'s own printed values (satellites `88888`, `11801`,
  §8): 6 digits of `11801`'s ~4e4 km magnitude is 40 m. The published `.e` rows for those same two
  cases sit 0.4-9.6 m (`88888`) and 5-29 m (`11801`) from `STR3`'s printed values (computed
  2026-10-06), and this port reproduces the `.e` rows to under 1 mm, so it sits that far from `STR3`'s
  print too — `STR3`'s own 1980 numerics and the corrections `VAL06` §III describes, not a defect.
  50 m still catches a real error: the `T2COF` scoping defect (§9.1) moved `11801` by hundreds of
  metres.
- **IOSG-P-2.** Against the `VAL06`-generated `.e` battery, where no source states a tolerance,
  **the gate is 2 cm**, ruled by the manager on 2026-10-06 and set before the residuals were
  re-measured (`plan/subplan_L6/L6-3.md`): every comparable satellite within 2 cm of every published
  row over its own span, velocity within 1 cm/s. A bound drawn round measured residuals accepts them
  by construction (plan §4 rule 7), which is why the gate is a ruling and not a measurement. **Met by
  all 31 comparable satellites:** `23333` 4.1 mm (radial, unexplained, §10 `IOSG-Q-007`); `20413` 0.4
  mm, over a 3.5-year span; the 12 resonant satellites 7-71 µm; every other satellite 6-8 µm (the
  `.e` files print positions to 1e-8 km); worst velocity 1.8 µm/s. The test also holds each
  satellite to a tighter **regression bound** (1 mm; `23333` 5 mm), measured maximum plus margin,
  **not a tolerance**: its only job is to stop a satellite moving from micrometres to centimetres
  unseen. `33334` is not a comparable satellite: its one published row is `33333`'s last row copied
  verbatim, the reference run's own stale output after the case failed (§8 `IOSG-A-007`).

  > **Superseded 2026-10-06, kept as written.** *"Against the `VAL06`-generated `.e` battery, where no
  > source states a tolerance, the gate is MEASURED, in two tiers stated per satellite in the test
  > (`position_tolerance_km`): **20 of the 31 comparable satellites at 2 cm** (13 of them agree to under
  > 0.5 mm; the other seven to 4-13 mm), and **11 at 1 m** (`00005 08195 09880 16925 21897 22674 23599
  > 26900 26975 28057 28350`; measured residuals 3-94 cm) whose residual neither text explains — §10
  > `IOSG-Q-001`. **Velocity: 1 cm/s for every satellite** (the worst residual is 0.57 mm/s)."* Wrong
  > in two ways: the tiers were measurements, which the ruling above forbids as a gate; and five of the
  > eleven (`00005 16925 23599 28057 28350`) were not residuals of the port at all but of the test's own
  > reader (§9.2), the other six being resonance (§3.8). An earlier draft of that text and of the test
  > also claimed "1 cm / 1 cm/s" while the code carried 1e-2 *km* (10 m) — a unit slip.
---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `IOSG-F-001` | Inclination within ~0.086 deg of 180 deg — at init on the TLE's own value, and in `sgp4_propagate` on the perturbed value after the periodics | `VAL06` §VI.A: "Inclination values near 180.0 degrees can cause divide-by-zero problems in the initialization and the routine operation... fixed by setting a tolerance in both routines" |
| `IOSG-F-002` | Computed radius (`rk`, AE units) drops below one Earth radius | `VAL06` §VI.A: "the decay condition simply checks the position magnitude on each step" |
| `IOSG-F-003` | The drag-modified eccentricity is below -0.001 | `VAL06` Table 1, satellites `28350` and `22312`: "error trap (modified eccentricity too low)" |
| `IOSG-F-004` | The periodics-perturbed eccentricity is outside `[0, 1)` | `SGP4-VER.TLE`'s own comment on `33334`: "try to check error code 3 looks like ep never goes below zero, tied close to ecc" |
| `IOSG-F-005` | The semi-latus rectum is not positive | `SGP4-VER.TLE`'s own comment on `33333`: "check error code 4" |
| (inherited) `R-ERR-1`/`R-ERR-2`/`R-ERR-3` | `SPEC-template.md` §5's own standing rules | unchanged |

`F-004`/`F-005` are this module's own reading of those two comments (the code the comments name is
not open): the comments say what the test exercises, not the condition. The conditions are chosen
to fire exactly where the published files stop (`33333`'s rows end at 20 min; the first refusal here
is at 21) and where the reference run failed (`33334`, t=0), and to fire nowhere inside any
published row (`IOSG-A-001`).

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `IOSG-A-001` | The full published battery (`SGP4-VER.TLE`, every `.e` file but `33334`'s) — every TLE propagated to every one of its own file's own times, within its own trailer's stated MFE range | the `.e` file's own printed position/velocity | `VAL06`'s own verification battery, §2 | IOSG-P-2: the 2 cm gate for all 31, a 1 mm regression bound (`23333`: 5 mm), 1 cm/s | R-001, R-002, R-003, R-004, R-006, R-007, R-008, R-010, R-011, P-2 |
| `IOSG-A-002` | Satellite `00005` at MFE 4320 min — the same TLE and epoch `FRAME-A-009` (`SPEC-frames.md`) already uses, `VAL06` Appendix C's own worked example | `r_TEME = (-9060.473 735 69, 4658.709 525 02, 813.686 731 53)` km | `VAL06` Appendix C | the 2 cm gate (measured < 0.01 mm) | R-001, R-005 |
| `IOSG-A-003` | `sgp4_init` refuses `IOSG-F-001` at inclination 179.95 deg (within tolerance of 180); succeeds at 90 deg (rule 5, both ways) | the refusal; then success | `IOSG-F-001` | — | F-001 |
| `IOSG-A-004` | `sgp4_init`/`sgp4_propagate` on satellite `28872`'s own real TLE (`VAL06` Table 1: "sub-orbital case... used to test error handling") refuses `IOSG-F-002` within 120 min | the refusal fires | `IOSG-F-002`, a real battery case | — | F-002 |
| `IOSG-A-005` | SGP4, satellite `88888`, `STR3` §13's own printed case, all five printed times | `STR3` §13's own printed X/Y/Z | `STR3` §13 — rule 2's rank-1 tier | IOSG-P-1 (50 m) | R-001, P-1 |
| `IOSG-A-006` | SDP4, satellite `11801`, `STR3` §13's own printed case (the original 1980 test, kept for continuity), all five printed times | `STR3` §13's own printed X/Y/Z | `STR3` §13 — rule 2's rank-1 tier | IOSG-P-1 (50 m) | R-001, R-002, R-004, P-1 |
| `IOSG-A-007` | The traps the sources name: `28350` accepted at its last published time (1440 min) and refused `IOSG-F-003` first between 1455 and 1480 min; `22312` accepted at 474.2 and refused `-003` at 500; `33333` accepted at 20 min and refused `-005` at 21, 25, 30; `33334` refused `-004` at t=0, and its `.e` row byte-identical to `33333`'s last | the refusals at those times | `VAL06` Table 1; `SGP4-VER.TLE`'s own comments | — | F-003, F-004, F-005, R-008 |
| `IOSG-A-008` | Every TLE in the verification file at t = -20000, -5000, -1440, -1, 0, 1, 20, 100, 1440, 5000, 20000 min | a fully finite state, or a refusal whose id begins `IOSG-F-` | — | — | R-009 |
| `IOSG-A-009` | The verification-file reader, on the eight line 2s whose five-digit revolution number abuts the mean motion with no space (`00005 08195 09880 16925 21897 23599 28057 28350`): mean motion (F11.8), revolution number (I5) and checksum recovered separately; the line's own mod-10 checksum, which covers the revolution digits, valid; and the old whitespace-token reading shown to differ from the published mean motion on all eight, by 7e-11 to 1.1e-9 relative. Also: every line 2 carries exactly eight decimals, and every checksum holds but `33333`'s and `33335`'s (the authors' hand-edited variants) | the published fields, hand-transcribed from the file | `SGP4-VER.TLE` itself; the TLE format's own column definitions | exact | — |
| `IOSG-A-010` | **`T-01`'s prediction, frozen before either Horizons table is compared with anything.** This tree's chain against three readings of a legacy IAU-76/80 chain, and the manual's own quoted number, (ERFA: `eraPmat76`, `eraNut80`, `VAL06` Rev 3's (C-3)) at the capture's 61 epochs (2026-10-05 07:30–12:30 TDB, 5 min), from the vendored element set, the EOP and leap-second pins and a time grid alone — the test's build supplies no path to either table. Each case's difference fitted as Ω × r + τ v. | **A** Ω = (0.00, 0.00, −0.03) mas, \|d\| ≤ 1.1 mm (with the kinematic terms in TEME → TOD: Ω_z = −1.51 mas, mean 34 mm, max 53 mm); **B** Ω = (−10.60, +49.03, +0.00) mas, \|d\| mean 1.716 m, max 1.774 m; **C** Ω = (+0.30, +0.01, +112.98) mas, \|d\| mean 2.580 m, max 4.001 m; **D** (the manual's −53 mas at face value) Ω = (0.00, 0.00, −53.00) mas, \|d\| mean 1.210 m, max 1.877 m; band 2.0 mas (70.6 mm at 7279 km); τ < 0.2 µs and fit rms < 0.3 mm in every case | computed from the two frame models alone, never from any table (`PROVENANCE.md` §38.12) | ±0.05 mas in each frozen rotation component, ±5 mm in the sizes | `IOSG-R-012` |

**Coverage.** `IOSG-R-005` (TEME-only output) is discharged by review — every acceptance row's own
expected value is stated in TEME, and no function in §5 returns any other frame — not by a
dedicated row.

---

## 9. Provenance obligations

`PROVENANCE.md` §38 records: the sources (§38.1-38.3), the corrections applied and their own
citations, the branch table (§38.4-38.6), the confirmed defect found and fixed in round one (§9.1),
and the table of what each later correction did to the battery (§9.2).

### 9.1 A real defect, found by comparing against `STR3`'s own printed cases, not by re-reading

`t2cof` (`STR3` §6 AND §7 both compute `T2COF=1.5*C1`, identically, immediately after `XNODCF` in
both listings) was scoped in a first draft as near-earth-only, alongside genuinely near-earth-only
terms (`C3`, `C5`, the `ISIMP`-gated `D2`/`D3`/`D4`). Every deep-space case therefore used
`t2cof = 0` (the struct's own default), zeroing `TEMPL` and silently dropping SGP4/SDP4's own
drag contribution to mean longitude for every deep-space orbit. Found by comparing this port's own
output for satellite `11801` against `STR3` §13's own printed SDP4 reference (the manager's own
instruction, `plan/subplan_L6/L6-3.md`) — the miss (963 km at MFE 1440) was against `STR3`'s OWN
original 1980 numbers, not only `VAL06`'s corrected ones, meaning the defect was in the base port,
not in how a correction was applied — confirmed by an independent Python re-derivation of the full
`DPINIT`/`DPSEC`/`DPPER` chain that matched this port everywhere except `TEMPL`. Fixed (`t2cof`
moved to the shared block); the full battery's own failure count dropped from 402 to 346 (of 641 rows).

### 9.2 The branch-by-branch fixes, in the order made, with the whole battery re-run after each

Rows of the 641 compared; the columns count position residuals above each bound
(`PROVENANCE.md` §38.8 has the per-satellite numbers). **Recomputed 2026-10-06 with the repaired
reader** — see the note below the table and the superseded table after it.

| step | fix | where it comes from | > 1 cm | > 2 cm (the gate) | > 1 m |
|---|---|---|---|---|---|
| S0 | the state at the start of the round (`T2COF` fix in) | — | 587 | 568 | 500 |
| S1 | negative-inclination swap after `DPPER` | `VAL06` §VI.C (text) | 587 | 568 | 500 |
| S2 | Lyddane route's sines/cosines from the perturbed inclination | `VAL06` §III seventh bullet (text) | 587 | 568 | 488 |
| S3 | direct route's sines/cosines likewise | same | 587 | 568 | 490 |
| S4 | tail coefficients from the perturbed inclination | same ("and SGP4 routines") | 587 | 568 | 281 |
| S5 | Kepler tolerance 1e-12 | `VAL06` §III third bullet (text) | 562 | 534 | 145 |
| S6 | Kepler step limit 0.95, not +/-e | `VAL06` §VI.E (text; the +/-e reading was the error) | 550 | 522 | 133 |
| S7 | Lyddane node reduced mod 2*pi, relative-quadrant fix | `VAL06` §III fifth bullet and "intrinsic functions" bullet (text) | 550 | 522 | 117 |
| S8 | eccentricity floor 1e-6, trap -0.001 | trap: Table 1 + file extents; floor: **vectors** | 550 | 522 | 39 |
| S9 | `TOTHRD` exactly 2/3 | `VAL06` §III second bullet (text, "certain constants") | 85 | 59 | 4 |
| S10 | `AODP` Kepler-consistent | **vectors** (`33333`, corroborated by five others) | 81 | 55 | 0 |
| S11 | low-eccentricity drag terms zeroed | Table 1 + file comment (text); the vectors confirm which terms | 74 | 55 | 0 |
| S12 | Greenwich angle at epoch: `VAL06` §II.F's 1970 form | `VAL06` §II.F (text), confirmed by the vectors | 12 | 6 | 0 |
| S13 | `G520`'s e^2 coefficient 5740.032 | **vectors** (`26975`, one satellite) | 0 | 0 | 0 |

**Three defects in the comparison, all found by being asked what the comparison fed the port.**
(1) The test's position bound was written as `1.0e-2` km under a comment saying 1 cm, so every
failure count the record carried before 2026-10-06 — 402, 346 — was counted against 10 m. (2) A NaN
residual compared `> tol` is false, so non-finite rows passed silently (`IOSG-A-008`). (3) **The
reader took eight revolution numbers glued to their mean-motion field as further decimals of it.**
`SGP4-VER.TLE` line 2 is fixed-column (TLE format: mean motion F11.8 at columns 53-63, revolution
number I5 at 64-68, checksum at 69); eight of its 33 line 2s have a five-digit revolution number, so
"10.82419157413667" is one whitespace token, and `std::stod` of the whole token is off by 7e-11 to
1.1e-9 relative (`IOSG-A-009` shows both the old reading and the right one on all eight). That was the
"relative mean-motion error of 1e-10 to 1e-9" an earlier version of this spec reported as an
unexplained algorithmic difference, and the "fitted mean-motion scale" figures in `PROVENANCE.md`
§38.6 *are* those glued digits. With the reader repaired, 00005, 16925, 23599, 28057 and 28350 were
exact (< 0.05 mm) at once; the six that remained were all resonance, §3.8.

> **Superseded 2026-10-06, kept as written.** The table as first committed (`38f2ffc`), computed
> through the defective reader (the "> 1 cm" and "> 1 m" columns of S0-S11 were 587/499, 587/499,
> 587/487, 587/488, 587/280, 561/146, 559/134, 559/118, 559/40, 185/4, 181/0, 183/0), and the note
> that followed it: *"(Two defects in the test found on the way, both in this round: the position bound
> was written as `1.0e-2` km under a comment saying 1 cm, so every failure count this module's record
> carried before today — 402, 346 — was counted against 10 m, not 1 cm; and a NaN residual compared
> `> tol` is false, so non-finite rows passed silently. Both fixed — §6, §8 `IOSG-A-008`.)"* Correct
> as far as it went; the third defect, above, was not yet known.

### 9.3 `T-01`'s own gate — the data pinned, the prediction frozen, the comparison next

**Status (2026-10-06).** The element set (CelesTrak, no login, cited to CelesTrak and Space-Track) and two Horizons tables of it — the object `-159588` and a user-input TLE, the control for which element set the object table used — are vendored (`PROVENANCE.md` §38.11, committed first, data only). The prediction is `IOSG-A-010` and `PROVENANCE.md` §38.12: three cases, each with its size and direction, a 2.0 mas band built from the named smaller contributions, and the rules for choosing the table and judging the fit — frozen in a commit that gives the test no path to either table. The comparison, with its own acceptance row, is the next commit. Two things the plan's earlier design did not have: the prediction states its **condition** (the required disagreement exists only against a legacy chain without the celestial-pole offsets, `SPEC-frames.md` §3.3 v1.10), and it is made for each reading of what Horizons does, because its manual does not say how an SGP4 state is converted to ICRF.

> *Earlier text of this section, left visible (design ruled, not yet built):* `../plan/PLAN.md` §4 rule 1 and `plan/subplan_L6/L6-3.md`: a fresh TLE (CelesTrak, no login, cited
> to both CelesTrak and Space-Track) and a matching-window Horizons capture, both vendored; the
> predicted size (~2.2 m, from the accumulated IAU-76-vs-IAU-2006 precession difference — not
> Vallado's own kinematic equation-of-equinoxes terms, ~95 mm, ~4% of it) and direction registered
> BEFORE the comparison runs, from the two frame models' own evaluation at the capture's own epochs,
> never from the residual itself; the frozen 2.246 m recorded beside the result, compared, not
> asserted. Not started — gated on the manager's own ruling on §10.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `IOSG-Q-001` | **RESOLVED 2026-10-06.** The residual an earlier version of this row called unexplained (11 satellites, 3-94 cm) was two things: the test's own reader (five satellites, §9.2) and resonance (six, §3.8: the Greenwich angle at epoch, from `VAL06` §II.F; and one printed coefficient, `IOSG-Q-006`). The cross-check the manager ruled on 28057 was withdrawn when 28057 turned out to be a reader artefact. Nothing in the battery now exceeds 4.1 mm (`IOSG-Q-007`). |
| `IOSG-Q-002` | **Determined by the vectors, not the texts:** the eccentricity floor, 1.0e-6. No text states it. Alternatives and their effects — maximum position residual over all published rows, with the floor off / 5e-7 / 9.5e-7 / **1.0e-6** / 1.05e-6 / 1.5e-6 / 1e-5: `22312` 279 m / 5.2 m / 0.52 m / **7 µm** / 0.52 m / 5.2 m / 93 m; `28350` 7.8 km / 6.4 m / 0.64 m / **6 µm** / 0.64 m / 6.4 m / 116 m; `33335` 51 m / 42 m / 4.2 m / **8 µm** / 4.2 m / 42 m / 759 m. `33335` (TLE e = 4.0e-7) is the case that settled the value and is not independent evidence for it; `22312` and `28350` are separate satellites that each show the same V alone, so the value rests on three. The trap value (-0.001) is read from Table 1 and the files' extents. |
| `IOSG-Q-003` | **Determined by the vectors, not the texts:** `AODP` as the Kepler-consistent axis (§3.5). Alternative (`STR3`'s `AO/(1-DELO)`) and its effect: `33333` 3398 km (7 µm with the Kepler-consistent value; this is the case that settled it and is not independent evidence), `29141` 1.6 mm (7 µm), `28350` 0.22 mm (6 µm), `22312` 43 µm (7 µm), `28623` 19 µm (6 µm), `28872` 16 µm (7 µm), and no other satellite changes by more than 1e-6 km — so five satellites other than `33333` corroborate it, each by being nearer its published rows with it. A free-factor fit of the three secular-rate terms to `33333` gave (1.001423, 1.001421, 1.002847) before the hypothesis was formed — `PINVSQ` x 1.001421, the hypothesis predicting 1.001422. |
| `IOSG-Q-004` | **RULED 2026-10-06: the vectors govern.** `VAL06` Table 1 says the `22312` trap fires "approximately 2840 min"; the published rows end at 474.2 min and this port's first refusal is at 489.15 min. `28350`: Table 1 "approximately 1460"; rows end at 1440; first refusal 1472.15 — reproduces. (The file's own trailers say `22312` was meant to run to 1440 min and `28350` to 2880, so neither Table 1 figure is a trailer length either.) Both figures are recorded. |
| `IOSG-Q-005` | `IOSG-F-004`/`-005` are readings of two comments in the verification file, and `-001` is raised in operation on the perturbed inclination where the reference sets a tolerance in a denominator instead (`VAL06` §VI.A: "setting a tolerance in both routines"). They stay labelled as readings. No battery row exercises the in-operation `-001`. |
| `IOSG-Q-006` | **Determined by the vectors, not the texts, on ONE satellite:** `G520`'s e^2 coefficient, 5740.032 where `STR3` prints 5740 (§3.8). Only `26975` has 0.5 <= e <= 0.65, so only it exercises the branch: blind scan 5739.9-5740.1 is V-shaped (2.4e-3 km per unit), minimum 5740.0320 +/- 0.0001; at 5740 the satellite is 7.6 cm out, at 5739.99 10 cm, at 5740.01 5.2 cm, at 5740.1 16 cm, at 5740.032 14 µm. It is not independent evidence for itself. Whether other printed `STR3` coefficients are similarly truncated cannot be told from this battery: the other `G520` branches, and every other coefficient in the 12 h block, are exercised by satellites that agree to 8-61 µm with the printed values. |
| `IOSG-Q-007` | The largest remaining residual: `23333` (WIND, e = 0.973, period 13.7 days) 4.1 mm at t=0, radial, falling to 1 mm by 360 min and staying there (R = 4, 2, 2, 1, 1... mm; T <= 2 mm; N = 0); `20413` 0.4 mm, over a 3.5-year span. Tried without effect on `23333`: the Kepler iteration cap (10 to 50), the trig terms from the final iterate, every subset of the three perturbed-coefficient groups (all three are needed: any subset leaves 4-52 km). Not chased further: 5x inside the gate, one satellite. |

> **Superseded 2026-10-06, kept as written.** The previous `IOSG-Q-001`: *"**The unexplained residual.**
> Eleven satellites agree with the published rows to 3-94 cm, the other twenty to 13 mm or better (thirteen
> to under 0.5 mm). Every resonant satellite carries a residual (4 mm to 94 cm); five non-resonant ones carry
> 10-65 cm (`00005`, `16925`, `23599`, `28057`, `28350`) and a sixth, `23333`, 4 mm. The residual is dominated
> by along-track error growing roughly linearly with time (`28057`: linear to 0.3 mm rms) or quadratically
> (`28350`), equivalent to a relative mean-motion error of 1e-10 to 1e-9; for the resonant cases it has a
> radial part too. A least-squares inversion of six constants (J2, J3, J4, Earth radius, XKE, a mean-motion
> scale) against every residual vector leaves the rms unchanged (0.0655 -> 0.0650 m); neither is it GMST at
> epoch, `G520`'s one rounded coefficient, the eccentricity floor, the Kepler solve, nor the low-eccentricity
> drag terms. [...] Is a recorded cross-check of ONE tier-B case's intermediate numbers (suggest `28057`) against
> Vallado's code allowed?"* Wrong: the "relative mean-motion error of 1e-10 to 1e-9" was the glued revolution
> digits (§9.2); the inversion could not remove it because it is not a constant; "neither is it GMST at epoch"
> and "`G520`'s one rounded coefficient" were tested through that reader and both are in fact the resonance
> fixes (§3.8). The same applies to the previous `IOSG-Q-002` ("Accept as recorded?") and `-003`, whose evidence
> stands but which now carry the alternatives and effects the manager asked for, and to `-004`, now ruled.
