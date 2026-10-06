# SPEC-io-formats — L6 step 1: formats in, as read structures

| | |
|---|---|
| **Spec ID** | `IOFM` |
| **Status** | **draft** 2026-09-25, for review |
| **Version** | 1.3 |
| **Date** | 2026-10-06 (v1.3, v1.2, v1.1; v1.0 2026-09-25) |
| **Layer** | L6 `io-measurements` (`../plan/PLAN.md` §3.7), step 1 (Formats) |
| **Depends on** | `core` (`odl::Result`), `time` (`Epoch`, `TimeScale`) |
| **Depended on by** | L6 step 2 (Horizons client, its own table format), L6 step 3 (`sgp4`, the TLE reader's own output), L6 step 4 (`measmod`, the CRD/CPF readers' own output); `tools/` — the SP3 reader here becomes the tree's one SP3 reader, re-pointing every tool that currently parses SP3 ad hoc |

**Derivation declaration (plan R1).** Written from the documents listed in §2 and from no
implementation of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read,
listed, searched or otherwise inspected during this specification's preparation — the
handover's own instruction, independently matching `SPEC-template.md` §2's own standing
rule. The existence of `scripts/horizons2eci.py`, `crd2obs.py`, `cpf2eci.py`,
`seesat2angles.py`, `angles_iod.py`, `angles_admissible.py` (named in `../plan/PLAN.md`
§6 as carried-over revival-era tooling) is known from the plan text alone; none of these
files was opened. Every format below is read from its own public specification, obtained
directly this round, not from how any existing reader — this tree's predecessor or
otherwise — happens to read it.

---

## 1. Purpose and scope

Seven file-format readers, each returning a fully-parsed, in-memory structure from
already-read text (no file access in this module — the same "parses files, does not open
them" split `eop` already draws for its own EOP series): **SP3** (GNSS/LEO precise
orbits), **TLE** (NORAD two-line elements), **CRD** (ILRS laser-ranging data), **CPF**
(ILRS laser-ranging predictions), **IOD** (optical angle observations), **SINEX**
(solution/parameter exchange, read at the general block-structure level), **ANTEX**
(antenna phase-centre calibration). Each reader also writes: round-tripping (read,
write, re-read, structures equal) is this step's own stated gate. One further facility,
added after this step's own first round (`plan/subplan_L6/L6-1.md`, ruled 2026-09-25):
**`Sp3Ephemeris`** (§3.8), a single satellite's own interpolated position between SP3
samples, the tree's one interpolation facility — data access, not a measurement model,
so it lives beside the reader it consumes rather than in `measmod` (L6 step 4), which
uses it.

**Not in scope**: the Horizons client and its own ephemeris-table format (L6 step 2,
`SPEC-io-horizons.md`); SGP4 propagation itself, which consumes this module's own `Tle`
structure but does not live here (L6 step 3, `SPEC-sgp4.md`); the measurement models
that consume CRD/CPF/IOD data (L6 step 4, `SPEC-measmod.md`); the station/site registry
those models need (also L6 step 4) — this module parses a CRD `H2` station-identifier
record's own numeric fields, but does not resolve them to a station's real Earth-fixed
coordinates, which is a registry lookup, not a format-reading concern; SINEX blocks
beyond the general block structure (e.g. interpreting `SOLUTION/ESTIMATE` rows) — a
caller with a specific need parses a specific block's own fields from the generic
`SinexBlock` this module returns, the same way `IGSMETA`'s own `SATELLITE/MASS` field
was read directly, ad hoc, at L5, rather than through a general parser that did not yet
exist.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `SP3D` | Hilla, S. (National Geodetic Survey, NOAA) | *The Extended Standard Product 3 Orbit Format (SP3-d)* | 21 February 2016 | `https://files.igs.org/pub/data/format/sp3d.pdf`, fetched directly 2026-09-25, SHA256 `0809fe6571816a9b8394b46b8851b9bfbab956b46f7acab5af6eb62acee5ebbe` | **primary**, obtained in full | the entire SP3-d reader: header record layout (columns given explicitly, every field), epoch/position/velocity/clock record layout, the `%c` time-system field |
| `SP3C` | Hilla, S. (National Geodetic Survey, NOAA) | *The Extended Standard Product 3 Orbit Format (SP3-c)* | 17 August 2010 | `https://files.igs.org/pub/data/format/sp3c.txt` — the document the ILRS's Formats page names as the format of its orbit products — fetched directly 2026-10-06, SHA256 `ac844f38cd1c1a1eafbaa45d7ba0baf89b0787cf307fb42ba645b03dc195da9d` | **primary**, read for two statements only | "SP3 Lines Nineteen to Twenty two: Columns 1-2 `/*`, Columns 4-60 Comment" and "SP3 Line 22+NUMEPS*(NUMSATS+1)+1 (i.e., The Last Line): Columns 1-3 End of File `EOF`" — the two places where the pinned ILRS orbit departs from it (§3.2, `IOFM-R-013`, `IOFM-R-014`) |
| `STR3` | Hoots, F. R., Roehrich, R. L. (US Air Force Aerospace Defense Command) | *Spacetrack Report No. 3: Models for Propagation of NORAD Element Sets* | December 1980 | `https://celestrak.org/NORAD/documentation/spacetrk.pdf`, fetched directly 2026-09-25, SHA256 `0ac48df7724b857c14431764a7baa0bf9aa64e570e3941b7244a15cdaea708bd` | **primary, partial** — the T-card/G-card element-set format sheet this 1980 typeset report itself refers to did not survive text extraction (a scanned tabular page; confirmed absent by search — zero hits for "T-CARD", "G-CARD" or "FORMAT SHEET" in the extracted text, the search and its terms recorded per plan §4 rule 4's own two-halves requirement); the physical constants (§12), propagation equations, and sample test cases (§13) extracted cleanly and are this tree's own primary source for L6 step 3's `sgp4` | normative for step 3's propagation equations and test vectors; NOT this reader's own source for the TLE column layout, which comes from `TLEFMT` instead |
| `TLEFMT` | CelesTrak (T.S. Kelso) | *NORAD Two-Line Element Set Format* | current, fetched 2026-09-25 | `https://celestrak.org/NORAD/documentation/tle-fmt.php`, fetched directly 2026-09-25, SHA256 `442053462ac8a23761fe27d519478331a1531bfd38d4a3082487628a0662794c` | **primary**, obtained in full | the TLE reader's own column-by-column layout for both lines, and the modulo-10 checksum algorithm — the same de facto format `STR3`'s own T-card describes, documented legibly where `STR3`'s own scanned sheet did not extract |
| `CRD2` | Ricklefs, R. L., for the ILRS Data Format and Procedures Working Group | *Consolidated Laser Ranging Data Format (CRD)*, Version 2.00/2.01 | 19 September 2019 | `https://ilrs.gsfc.nasa.gov/docs/2022/crd_v2.01e3.pdf`, fetched directly 2026-09-25, SHA256 `9b7ef5ebbb573418ef3e0d8f89691011eb3afd111734bde96186d55ddaa848d3` | **primary**, obtained in full | the CRD reader: every header record (`H1`–`H5`, `H8`, `H9`), configuration records (`C0`–`C7`), and data records (`10` full-rate range, `11` normal-point range, `12` range supplement, `20` meteorological, and the remainder read generically, §3.3 below) |
| `CPF2` | Ricklefs, R. L., for the ILRS Predictions Formats Study Group | *Consolidated Laser Target Prediction Format*, Version 2 | 28 February 2018 | `https://ilrs.gsfc.nasa.gov/docs/2018/cpf_2.00h-1.pdf`, fetched directly 2026-09-25, SHA256 `36c70d0ad113e019e8c7c1600fd4e65c55eb31112297d74aa4f236ddc3d23181` | **primary**, obtained in full | the CPF reader: header records `H1`–`H5`, `H9`, and position/velocity ephemeris entries (`10`, `20`) |
| `IODFMT` | Lewis, G. D. | *IOD Observation Format Description* | Version 0, 10 October 1998, clarified 24 February 2002 | `https://www.satobs.org/position/IODformat.html`, fetched directly 2026-09-25, SHA256 `c781b04fcaccd66fea6f96d601a27c8b0e1d76121d20d212dee4b2381586fa04` | **primary**, obtained in full | the IOD reader: the 80-column field layout, the seven RA/DEC and AZ/EL angle-format codes, the mantissa-exponent uncertainty encoding |
| `SINEX2` | IGS/IERS/ILRS/IVS SINEX Working Group | *SINEX — Solution (Software/technique) INdependent EXchange Format*, Version 2.02 | 1 December 2006 | `https://ivscc.gsfc.nasa.gov/products-data/sinex_v202.pdf`, fetched directly 2026-09-25, SHA256 `246f42b88032d3357289cd698224fed3f2ac5b360ecd27e8f2c3d95e54eb69b8` | **primary, partial** — the general file/block structure (§2), the header line (§3) and the `FILE/REFERENCE`/comment-line conventions extracted cleanly; the per-block field tables for the ~20 named blocks (`SITE/ID`, `SOLUTION/ESTIMATE`, etc.) were not individually transcribed this round, per this spec's own §1 scoping — a caller reads a specific block's own fields against `SINEX2` directly, the same way `IGSMETA`'s `SATELLITE/MASS` field was read at L5 | normative for the general reader (header line, block delimiters `+`/`-`, comment lines, footer); a specific block's own semantics are each block's own future consumer's responsibility |
| `ANTEX14` | Rothacher, M., Schmid, R. (TU München) | *ANTEX: The Antenna Exchange Format*, Version 1.4 | 15 September 2010 | `https://files.igs.org/pub/data/format/antex14.txt`, fetched directly 2026-09-25, SHA256 `86458154367916d2fa6282ca2f8e5137c3d9663d7c18eb44eaf344c8e7eba621` | **primary**, obtained in full | the ANTEX reader: the header block, the per-antenna PCO (north/east/up) and PCV (`NOAZI` and azimuth-dependent) grid records |
| `SCHEN03` | Schenewerk, M. | *A brief review of basic GPS orbit interpolation strategies*, GPS Solutions 6:265–267 | 2003 | `http://www.acc.igs.org/orbits/orbit-interp_gpssoln03.pdf`, fetched directly 2026-09-25, SHA256 (recorded in `PROVENANCE.md`, this round's own entry) | **primary**, obtained in full | §3.8's own interpolation order: a real IGS 15-minute file, ECF coordinates, 9–13 term polynomials adequate, 11 terms (10th order) best |
| `HORA06` | Horemuž, M., Andersson, J. V. | *Polynomial interpolation of GPS satellite coordinates*, GPS Solutions 10:67–72 | 2006 | `http://www.acc.igs.org/orbits/orbit-interp_gpssoln06.pdf`, fetched directly 2026-09-25, SHA256 (recorded in `PROVENANCE.md`, this round's own entry) | **primary**, obtained in full | §3.8's own interpolation order: a second, independent real IGS 15-minute file; order 10 already negligible Runge-phenomenon error |
| `ZEIT24` | Zeitlhöfler, J., Alkahal, R., Rudenko, S., Bloßfeld, M., Seitz, F. | *Performance assessment of interpolation methods for orbits of altimetry satellites*, Earth, Planets and Space 76:158, DOI `10.1186/s40623-024-02102-8` | 2024 | `https://earth-planets-space.springeropen.com/articles/10.1186/s40623-024-02102-8`, fetched directly 2026-09-25 (open access), SHA256 (recorded in `PROVENANCE.md`, this round's own entry) | **primary**, obtained in full | §3.8's own interpolation order for the LEO/altimetry case (Jason-2-class, 30–120 s step): degree 8–11 sub-millimetre, degrees above 13 show Runge-phenomenon growth |

**Licence note, the eight format specifications.** Every document in the first eight rows
above is a public, first-party format specification, fetched directly by plain HTTP GET,
no login, no account, no request form — the same retrievability bar every other rule-4
search in this tree applies. None states redistribution terms for the DOCUMENT itself,
which this module does not redistribute: it implements a reader from the format's own
printed description, the same "clean-room, spec-derived code" treatment the IERS
Conventions tables and the RKF7(8) coefficients already receive in this tree
(`../plan/PLAN.md` §3.11 point 4), not a transcription of the document's own prose or a
vendoring of its bytes. `IODFMT` alone states a copyright ("Copyright (C) 1998, G. Lewis")
on the document; the same reasoning applies — implementing a parser for the FORMAT it
describes is not a reproduction of the document.

**`SCHEN03`/`HORA06`/`ZEIT24`.** Three further sources, added when §3.8's own
interpolation order needed a stated reason rather than an assertion. Each is a normal,
publicly-indexed journal article (GPS Solutions, Springer; Earth, Planets and Space, a
SpringerOpen open-access journal), fetched directly with no login, cited here for a
specific STATED FINDING quoted in §3.8 (an order, an achieved accuracy) — the same
citation-not-redistribution treatment this tree's own literature entries already receive
(plan §5 constraint 3): no table, figure or extended passage is reproduced, and this
module builds no data structure from their contents the way it does from the eight format
specifications above.

---

## 3. Definitions and conventions

### 3.1 Common conventions

- **Every reader's own function signature is `read_<format>(text: string) -> Result<Xxx,
  XxxError>`** — text already in memory, no path argument, matching `eop::EopSeries::
  load_c04`'s own established split between parsing (this module) and file access (the
  caller). `XxxError = odl::Diagnostic`, this module's own alias, matching every other
  module.
- **Every reader's own companion `write_<format>(Xxx) -> Result<string, XxxError>`**
  exists for round-tripping (§8) and is not a claim of byte-identical reproduction of an
  arbitrary real file (a real file may use whitespace or optional records this module's
  own writer does not reproduce stylistically) — the round-trip claim is **semantic**:
  `read(write(read(text))) == read(text)`, field for field.
- **A reader refuses a field it does not recognise (plan §5 constraint 4), rather than
  defaulting it.** Concretely: an enumerated code (SP3's Time System, CRD/CPF's own
  record-type tag, IOD's angle-format code) outside its own documented domain is refused,
  named, with the offending value quoted — never silently mapped to the nearest known
  value or ignored. §7 states each such refusal.
- **Time scale lives in the type, but this module never constructs an `odl::time::Epoch`
  itself.** `Epoch::from_calendar`/`from_two_part_jd` each require a `const LeapTable&`
  (`time/epoch.hpp`), and a `LeapTable` is loaded from a file — this module's own "no
  file access" rule (above) means it cannot hold one. The established split (already
  the shape every real-data tool in this tree uses by hand, e.g. `tools/
  doris_jason_check.cpp`'s own `build_ephem`/`load_env`) is kept explicit here instead
  of repeated ad hoc: **each reader returns the format's own raw calendar fields
  (`time::Calendar`, itself a plain, table-free struct) tagged with the format's own
  native time-system enum, plus a pure, table-free `to_time_scale(...) ->
  Result<odl::time::TimeScale, XxxError>` mapping function** — GPS/TAI/UTC succeed;
  where the format's own code does not map onto one of `TimeScale`'s own seven values
  (SP3's `GLO`/`GAL`/`BDT`/`QZS`), the mapping REFUSES rather than silently treating
  GLONASS/Galileo/BeiDou/QZSS system time as GPS time, which it is not (GLONASS time is
  UTC-referenced with a constant offset; Galileo System Time and BeiDou Time each have
  their own epoch and a small, published, time-varying offset from GPS time). The
  caller — which DOES have a `LeapTable`, having read one to get this far — then calls
  `Epoch::from_calendar` itself. This is a genuine, stated scope limit, not an
  oversight: extending `TimeScale` to carry GLONASS/Galileo/BeiDou/QZSS system time is
  future work with its own consumer, not invented here for one reader.

### 3.2 SP3

`Sp3TimeSystem { GPS, GLO, GAL, BDT, TAI, QZS, UTC }` — the seven codes `SP3D`'s own `%c`
line, columns 10–12, can carry, read as the raw 3-character code. `Sp3PosVelFlag {
Position, Velocity }` — `SP3D`'s own column-3 flag on the first header line ('P' or 'V'),
naming whether the FIRST epoch's own record type is position-only or position-plus-
velocity (later epochs may still carry `V`/`EV` records even when the file is declared
`P`, per `SP3D`'s own Example 2). Every numeric field is read at the column positions
`SP3D` states exactly (fixed-column format, not whitespace-delimited — unlike every ILRS
format below); a line shorter than a field's own column range, or a non-numeric value in
a numeric field's own column range, is `IOFM-F-001`.

**Two departures of the real ILRS orbit from `SP3D`, both accepted by rule (v1.2).** The ILRS weekly orbit `ilrsa.orb.lageos1.260103.v80.sp3` (SP3-c, 5040 epochs) was refused by the reader on the day it was first read (2026-10-06), because it departs from `SP3D` in two ways, and the one
the refusal named, "does not end with 'EOF'", was the later of them:
- **Its comment lines begin `%/*`, not `/*`** (`IOFM-R-014`): a comment line is a line that begins `/*` or `%/*`, and its text is what follows the marker and one blank. These lines carry the product's own statement of its reference frame (`Reference TRF: SLRF2020`, `MEAS-R-041`), so they are read, not skipped.
- **It has no `EOF` line** (`IOFM-R-013`). `SP3D` ends a file with `EOF`, and a file that stops without one may have been cut short; such a file is complete **only if it holds exactly the number of epoch records its first line declares** (columns 33–39, "Number of Epochs") — the header's own count is a stronger check of
  completeness than a terminator. It is then read, and `Sp3File::eof_present` is false so that a caller can see the terminator was missing; a file without the terminator and with another count is refused as truncated (`IOFM-F-018`). A file that has the terminator is read as before (the declared count has never been enforced on it, and is not now).
Both rules were added to the one SP3 reader and not worked around in a test, because the reader that refuses the one product L6's last gate compares against is the defect.
**What the rules rest on (rule 4, searched 2026-10-06).** The ILRS's own pages name SP3 version c as the format of its orbit products — "They are archived in SP3c format on both ILRS DCs" (Analysis Products page), and the Formats page lists "SP3 version c" and links `SP3C` — and `SP3C` says the comment lines begin `/*` (columns 1–2) and the last line is `EOF`. **No ILRS page or readme describes either departure.** Searched: the ILRS Products, Formats, Analysis Products and SP3C ID Correspondence Table pages; the EDC's `orbits/00readme_4_orbits` (which still says "SLRF2008") and `README_AC.asi`; and `README_CC.ilrsa`, which the product's own header names ("Combination details in README_CC.ilrsa"), at three EDC locations (`products/orbits/`, `products/`, `products/orbits/lageos1/`): each answers "File or directory not found!". CDDIS, where the ILRS keeps its archive, needs an account and is not searched. So both rules rest on **what is observed in the pinned product** (`ilrs-lageos1-sp3-260103`, sha256 `9326cd4b…`), read against `SP3C`, and on four sibling products of the same week looked at for their headers and last lines only (nothing else of them was read, none is pinned, none enters a comparison): the primary combination's own `ilrsa` file has `%/*` comments and no `EOF`; the backup combination's `ilrsb` file has `%/*` comments and **has** `EOF`; the `asi`, `bkg` and `gfz` files have `/*` comments and `EOF`. **The missing `EOF` is therefore a property of the one product this tree reads, not of ILRS orbit files in general**, which is why the rule accepts it only with the header's own epoch count (the `ilrsb` file, with `EOF`, declares 5041 epochs and holds 5040, and is read as any file with `EOF` is: the count is not enforced there). Both rules stand.

**Two more departures, found by four of the nine other products of the same week, accepted by rule (v1.3).** The orbit term of G5 (`SPEC-measmod.md` §8.9) is sized from the nine other products of the ILRS week 260103 (`v80`, 120 s, `L51`), each read by this one reader; **four of them were refused on the day they were first read (2026-10-06, `SP3D`'s fixed columns)**: the backup combination `ilrsb` (line 23: its epoch line is 30 characters where the seconds field ends at column 31 — every field sits one column to the left, `* 2025 12 28  0  0  0.00000000`), and `gfz` and `nsgf` (line 24: the `P` line stops at column 46, no clock) and `jcet` (line 24: columns 47–60 blank, no clock). Their `V` lines are the same. The other five (`asi bkg cnes dgfi esa`) read as they stand; `esa` pads its lines to 80 columns, `cnes` writes hours and minutes with a leading zero, `jcet`'s seconds field sits one column to the right (read by the columns, its last decimal cut: it holds `0.00000000` throughout, and the check below agrees).
- **The clock of a `P` record and the clock rate of a `V` record may be absent or blank** (`IOFM-R-015`): a line that stops before column 47, or whose columns 47–60 are all blank, holds no clock, and the value is **999999.999999**, which is what `SP3C` says an absent clock is to be set to ("Bad or absent clock values are to be set to _999999.999999", and the same for the clock rate-of-change; `SP3C`'s own text, hash above, re-read for this rule). The coordinates (columns 5–46) are mandatory as ever; a clock field that is there and cut short (the line ends inside columns 47–60 with digits in them) or that is not a number is `IOFM-F-001` as before. `Sp3File::clock_fields_absent` counts them.
- **An epoch line is read at the fixed columns, and by its blank-separated fields when the columns do not read** (`IOFM-R-016`): `*` and exactly six numeric fields (year, month, day, hour, minute as integers, the second as a number). Where the columns read **and** the fields read, they must agree (the calendar fields exactly, the second to 1 × 10⁻⁷ s, the width of the last decimal of a field one column off) and the fields' reading is used; **where they read differently the line is refused (`IOFM-F-019`)** — a shifted line padded with blanks to 31 columns or more reads as year 13 at the fixed columns, and a reader that took the columns would have misread it silently; where only the fields read, the line is read by them and `Sp3File::epoch_lines_by_fields` counts it.
Neither rule changes what a conforming file reads as. The reader's header read is unchanged: the backup combination's first line writes its frame as `ITRF14` in the format's five columns (47–51), so `coordinate_sys` reads `ITRF1`; `MEAS-R-041` maps no such code and refuses it, and the orbit term uses the positions alone.

### 3.3 CRD

`CrdRecordKind` — one tag per record type `CRD2` §1–§4 defines: `Format` (`H1`),
`Station` (`H2`), `Target` (`H3`), `Session` (`H4`), `Prediction` (`H5`), `EndOfSession`
(`H8`), `EndOfFile` (`H9`), eight configuration kinds (`C0`–`C7`), and the data records
`FullRateRange` (`10`), `NormalPointRange` (`11`), `RangeSupplement` (`12`),
`Meteorological` (`20`), `SkyQuality` (`21`), `Angles` (`30`), three calibration kinds
(`40`–`42`), `SessionStatistics` (`50`), `Compatibility` (`60`, `CRD2`'s own words:
"obsolete"), `UserDefined` (`9X`, ten variants) and `Comment` (`00`). **Header, session,
and the two range record kinds — `H1`–`H5`, `H8`, `H9`, `10`, `11`, `12`, `20` — are
parsed field by field**, per §3.1's own refusal rule. **The configuration, calibration,
statistics and user-defined kinds are parsed as a record-type tag plus an ordered list of
whitespace-delimited tokens, opaque to this reader** — `SPEC-measmod.md` (L6 step 4) is
this tree's own first consumer of range data, and does not need laser-configuration or
calibration-statistic detail to compute a range residual; a future consumer that does
reads those tokens against `CRD2` directly, the same shape this spec's own §1 already
states for SINEX blocks. This is a scope decision, stated here, not a silent gap: a
record whose own TYPE TAG is not one of the twenty-eight `CRD2` names at all is still
refused (`IOFM-F-004`), only its own field-level content is passed through opaque for
the kinds named above.

#### 3.3.1 Pass grouping, the `C0` wavelength and the `H2` time scale (v1.1, 2026-10-06; L6 step 4's needs)

> **[Superseded 2026-10-06 (v1.1), kept visible: the paragraph above says `SPEC-measmod.md` "does not need laser-configuration or calibration-statistic detail to compute a range residual".]**
> It needs **one** configuration field: the transmit wavelength, field 3 of `C0`, which the troposphere's dispersion depends on (`MEAS-R-036`) — found when the model was specified, against a real file. The rest of the paragraph stands: the other configuration, calibration, statistics and user-defined records stay opaque.

`CRD2` §4 permits a common `H1`/`H2`/`H3` set followed by several `H4`…`H8` blocks, and a file of several complete `H1`…`H8` sets (§4.4); the real monthly LAGEOS-1 file read for this step has 807 of the latter, each with one `H2`, `H3`, `H4`, `C0` and `H8`, and one `H9` at its end. The flat `CrdFile` of §5 loses which normal points belong to which pass, so a **view by pass** is added, over the same text:

- `CrdPass` — one `H4`…`H8` block (a session or pass segment, `CRD2` §1.4) together with the `H2`, `H3` and `C0`–`C7` records **in force when it opens**: `station` (`H2`), `target` (`H3`), `session` (`H4`), `time_scale` (resolved, below), `configs` (each `C0` typed: `config_id` field 4, `wavelength_nm` field 3, the remaining tokens verbatim), the full-rate and normal-point records (`10`, `11`) in file order as `CrdRangeRecord`, the meteorology (`20`) typed as `CrdMeteorology { seconds_of_day, pressure_mbar, temperature_k, relative_humidity_percent, origin }`, and every other record of the block, opaque, in file order.
- `CrdPass::wavelength_nm(config_id)` returns field 3 of the `C0` whose field 4 is `config_id` (nanometres); an id with no `C0` in force refuses (`IOFM-F-014`).
- **The `H2` epoch-time-scale code** (field 6, "Station Epoch Time Scale") is resolved **at parse time** (ruling R8 of L6 step 4): **3 = UTC(USNO), 4 = UTC(GPS), 7 = UTC(BIPM)** resolve to `TimeScale::UTC` (the differences among the three realisations, ≤ 100 ns by estimate, are not modelled); **every other code is refused** (`IOFM-F-015`) — 1–2, 5–6 and 8–9 are "reserved for … obsolete time scales" and 10 and above are "UTC (Station Time Scales) USE ONLY WITH ANALYSIS STANDING COMMITTEE APPROVAL". The flat `read_crd` stays lenient (it reads the code as an integer, as v1.0 does); only the pass view resolves it.
- **Structure refusals** (`IOFM-F-013`): a data record (`10`, `11`, `12`, `20`, `21`, `30`, `50`) outside an `H4`…`H8` block; an `H4` with no `H2` or no `H3` in force; an `H4` opened before the previous block's `H8`; an `H8` with no block open; an `H9` inside a block; a block with no `H8` before the end of the text (naming the line of its `H4`); two `C0` records of one id in one pass. **Header-level records are accepted**: `CRD2` §4's examples put `C0`…`C7` and a calibration record (`40`) before `H3` and `H4`, and a `C0` there is inherited by every block under that `H2` (a block's own `C0` is not inherited by the next). Record tags are case-insensitive, as in the real file (`h8` and `H8` both occur).

### 3.4 CPF

`CpfDirectionFlag { Common, Transmit, Receive }` — `CPF2`'s own three-way flag on every
position/velocity record. `CpfReferenceFrame { BodyFixed, TrueOfDate, MeanOfDateJ2000 }`
— `CPF2`'s own `H2` record field 542–545 (0/1/2). Position records (`10`) are read in
full (MJD, seconds-of-day UTC, leap-second flag, geocentric X/Y/Z); velocity records
(`20`) likewise.

### 3.5 IOD

`IodAngleFormat` — the seven RA/DEC and AZ/EL encodings `IODFMT` names by its own single-
digit code (column 45); an eighth, undocumented digit is refused (`IOFM-F-005`). The
observer's own station is a bare 4-digit number (`IODFMT`'s own columns 17–20) — this
reader returns that number; resolving it to a real station location is the station
registry's own job (L6 step 4, out of this spec's own scope, §1). Uncertainty fields use
`IODFMT`'s own mantissa-exponent encoding, `value = M × 10^(X−8)`, read directly, not
approximated.

#### 3.5.1 Decoding the angle field and the uncertainties (v1.1, 2026-10-06; L6 step 4's needs)

`IodObservation::angle_raw` (columns 48–61) is decoded by the format code of column 45 into two angles. Columns 48–54 hold the first coordinate (right ascension or azimuth), column 55 the sign of the second (declination or elevation), columns 56–61 the second.
**Blanks in the digit positions are zeros** (`IODFMT`: "NON-SIGNIFICANT DIGITS MAY BE ZERO (0), NON-ZERO (1-9) or BLANK"). The sign column must be `+` or `-`.

| format | first coordinate (cols 48–54) | second coordinate (cols 56–61) | uncertainty `MX` is in |
|---|---|---|---|
| 1 | RA `HH MM SS s`: hours, minutes, seconds, tenths of a second | `DD MM SS`: degrees, arcminutes, arcseconds | seconds of arc |
| 2 | RA `HH MM mmm`: hours, minutes, thousandths of a minute | `DD MM mm`: degrees, arcminutes, hundredths of an arcminute | minutes of arc |
| 3 | RA `HH MM mmm` | `DD dddd`: degrees, ten-thousandths of a degree | degrees of arc |
| 4 | AZ `DDD MM SS` | `DD MM SS` (elevation) | seconds of arc |
| 5 | AZ `DDD MM mm`: degrees, arcminutes, hundredths of an arcminute | `DD MM mm` (elevation) | minutes of arc |
| 6 | AZ `DDD dddd`: degrees, ten-thousandths | `DD dddd` (elevation) | degrees of arc |
| 7 | RA `HH MM SS s` | `DD dddd` | degrees of arc |

Right ascension is converted from hours (× 15°) and returned in [0, 2π); azimuth in [0, 2π); declination and elevation within [−π/2, π/2]. A minutes or seconds field of 60 or more, an hours field of 24 or more, a degrees field above 360 (azimuth) or above 90 (declination, elevation), or a non-digit that is not a blank refuses (`IOFM-F-016`).
**The uncertainties** (`IODFMT`): the time uncertainty (columns 42–43) and the positional uncertainty (columns 63–64) are `MX` with `M` the mantissa and `X` the exponent digit, valued `M × 10^(X−8)` — seconds for the time, and seconds, arcminutes or degrees of arc by the format for the position, "assumed to apply equally to both components". Two blanks mean "not reported" (no value); a digit and a blank, or a non-digit, refuses (`IOFM-F-017`). The epoch code (column 46) is read as it is (0 or blank, 1…6, blank for azimuth/elevation); **what to do with each code is a policy of the consumer**, not of this reader.

### 3.6 SINEX

`SinexBlock { name: string, lines: vector<string> }` — one entry per `+BLOCKNAME`…
`-BLOCKNAME` region `SINEX2` §2 defines, the enclosed data lines held verbatim (leading
single space already part of `SINEX2`'s own line convention, §2: `" "` marks a data
line). `SinexHeader` — the fields of `SINEX2` §3's own header line (`%=SNX` through the
solution-contents field). A line whose first character is none of `SINEX2` §2's own five
reserved characters (`%`, `*`, `+`, `-`, `` ` `` — a data line's own leading space) is
refused (`IOFM-F-006`).

### 3.7 ANTEX

`AntexPcv { noazi: vector<double>, azimuth_dependent: optional<matrix<double>> }` per
antenna, per frequency — `ANTEX14`'s own `NOAZI` row (always present) plus the optional
azimuth-dependent grid (`ZEN1`/`ZEN2`/`DZEN` from the header define the zenith-angle
axis; the azimuth axis is 0–360° in `ANTEX14`'s own fixed 5° steps where present).
`AntexPco { north_mm: double, east_mm: double, up_mm: double }`.

### 3.8 SP3 ephemeris — interpolated position

`plan/subplan_L6/L6-1.md`, ruled 2026-09-25, after this spec's own first round already
built the seven readers: the comparison tools' own three position-lookup defects
(nearest-sample selection, elapsed time in place of a calendar date, an assumed frame)
lived in their own per-tool interpolation code, and each tool still interpolated SP3
positions in its own way (linear for the GNSS-comparison tools that need it at all;
`doris_jason_check.cpp`'s own separate fix for Jason). **Interpolation is data access,
the same kind of thing §3.1's `to_time_scale` already is, not a measurement model** —
so it is built once, here, and every tool that needs a position between SP3 samples uses
it, rather than carrying its own copy.

`Sp3Ephemeris` is built from an already-read `Sp3File` and one satellite id: it extracts
that satellite's own position record from every epoch that carries one, in file order
(`IOFM-R-001` already guarantees this is chronological), and is ready for repeated
interpolation. Like every reader in §3.1–§3.7, it never constructs an `odl::time::Epoch`
— no `LeapTable` is available here — so time is measured as **elapsed calendar seconds**
from the ephemeris's own first sample (`calendar_elapsed_seconds`, a pure function of the
`Calendar` fields, a proleptic-Gregorian day count plus time-of-day). This is EXACT for a
continuous time system — TAI, GPS, the system every real SP3 file this tree holds
actually uses — and a stated, bounded approximation for a UTC-tagged file: off by at
most the leap seconds actually crossed, and only across the instant of the leap second
itself, never silently assumed away.

**The order, per sampling interval, and why (`IOFM-R-003`).** A Lagrange polynomial
through 11 points (10th order) is used by default, for both a 15-minute GNSS file and a
60-second LEO file — the SAME order, not a formula that varies with the interval, because
three independent sources, spanning both regimes on real IGS/GSFC data, converge on
essentially this same range:

- Schenewerk, M. (2003), *"A brief review of basic GPS orbit interpolation strategies,"*
  GPS Solutions 6:265–267 (fetched directly, `acc.igs.org`, no login) — a real IGS rapid
  ephemeris (`igr11472.sp3`, 15-minute), ECF coordinates: 9–13 term (8th–12th order)
  polynomials are "more than adequate," under 9 terms "unable to match the more subtle
  variations," over 13 terms have "too much freedom and overreact" — 11 terms (10th
  order) gives 0.2 cm population SD.
- Horemuž, M., Andersson, J. V. (2006), *"Polynomial interpolation of GPS satellite
  coordinates,"* GPS Solutions 10:67–72 (fetched directly, `acc.igs.org`) — a second real
  IGS file (`igs13036.sp3`, 15-minute), independently: *"results were already negligibly
  small ... for polynomial order 10."*
- Zeitlhöfler, J., Alkahal, R., Rudenko, S., Bloßfeld, M., Seitz, F. (2024), *"Performance
  assessment of interpolation methods for orbits of altimetry satellites,"* Earth, Planets
  and Space 76:158, DOI `10.1186/s40623-024-02102-8` (open access, fetched directly) —
  Jason-2-class LEO orbits at 30/60/120 s step sizes: degree 8 (Newton, the same
  interpolating polynomial a Lagrange fit through the same points is) already reaches
  sub-millimetre accuracy at up to 120 s step size; degree 13 shows Runge-phenomenon
  growth at the window edges, so *"we recommend degrees up to 11."*

Order 10 (11 points) sits inside every one of these three ranges, for a file whose own
sample interval is 900 s or 60 s alike — the convergence itself, not merely a citation,
is why one order serves both regimes (measured directly against real data, `IOFM-A-030`/
`031`, §8). When a file offers fewer than 11 epochs for the named satellite (a short or
hand-built fixture; every real file this tree holds offers far more), the order actually
used is reduced to `sample_count − 1` rather than refusing outright — interpolation with
fewer points, strictly inside the sampled span, is still interpolation, not the
extrapolation `IOFM-R-004` refuses.

**Never extrapolates (`IOFM-R-004`).** A query outside `[first sample, last sample]`
refuses (`IOFM-F-009`) rather than returning a value nothing in the file supports.

**Refuses across a gap or a manoeuvre (`IOFM-R-005`).** The window of points an
interpolation query would use is checked, before fitting: if any two consecutive points
in that window are separated by more than 1.5× the file's own stated epoch interval (a
missed epoch — a genuine gap, not floating-point jitter on an otherwise uniform grid), or
if any point in that window is itself flagged `M` (Maneuver Flag, `SP3D` column 79,
already read into `Sp3PositionRecord::maneuver` by §3.2's own reader), the query refuses
(`IOFM-F-010`/`IOFM-F-011`) rather than fitting a smooth polynomial across a real
discontinuity a satellite's own position does not smoothly interpolate through.

**Velocity is not `Sp3Ephemeris`'s own scope** — the ruling's own words are "interpolated
POSITION." `central_difference_velocity_km_s` (§5) is offered alongside it, not as a
class member, as the position facility's own obvious derivative (a central finite
difference over a 1 s step, refusing whenever either probe it needs does), for a caller
needing velocity at an arbitrary query time rather than only at a real sample (where the
SP3 file's own `V` record already states it directly). `GALY-Q-002` (`SPEC-galileo-
attitude.md`) names the OLD two-real-sample central difference (a span of one whole SP3
interval, not one second) as a suspect for a small unexplained residual; re-pointing onto
this function's own much shorter step is the direct test of that suspicion, reported in
`PROVENANCE.md`, this round's own entry, not re-litigated here.

---

## 4. Required behaviour

Each reader's own required behaviour is stated by its §3 definitions above (the field
layout IS the requirement, per this format-reading module's own nature — there is no
further mathematics to state beyond "read exactly what the source specifies, at the
positions it specifies, refusing what it does not"). Two requirements are common to all
seven and stated once here rather than seven times:

- **IOFM-R-001.** Every reader determines its own record/line boundaries by the
  record-type marker the format itself defines (SP3's own two-character column-1–2 code;
  CRD/CPF's own record-type field; SINEX's own leading reserved character), never by a
  fixed line count or fixed record count — `SP3D`'s own "IMPLEMENTATION CONSIDERATIONS"
  section states this explicitly for SP3 (read `+` records until the first `++`, `++`
  records until the first `%c`, comments until the first epoch header) and it is this
  reader's own required algorithm, not an option among several: **this is precisely the
  predecessor's second defect, a fixed header length skipped past a file whose own header
  is longer** (`IOFM-A-002` shows it firing).
- **IOFM-R-002.** Every reader determines a record's own numeric field from the SAME
  column or token position the record's own type says to use, never from a
  position that merely looks plausible for a same-shaped but different record type —
  **this is precisely the predecessor's first defect, an interval read from the wrong
  field** (`IOFM-A-001` shows it firing, against SP3's own Epoch Interval, `SP3D` line
  two, columns 25–38, not the GPS-week/seconds-of-week pair that precede it on the same
  line).

§3.8's `Sp3Ephemeris` states three more, its own:

- **IOFM-R-003.** Position at an epoch between two SP3 samples is a Lagrange polynomial
  of a STATED order (default 10, 11 points), the order stated and justified per sampling
  interval, not silently varying by file — §3.8's own three-source convergence.
- **IOFM-R-004.** Interpolation never extrapolates: a query outside the sampled span
  refuses (`IOFM-F-009`).
- **IOFM-R-005.** Interpolation refuses rather than fitting across a real discontinuity:
  a query whose own interpolation window spans a gap larger than 1.5× the file's stated
  interval (`IOFM-F-010`), or includes a manoeuvre-flagged sample (`IOFM-F-011`).
- **IOFM-R-006.** Elapsed time between two epochs is computed by exact proleptic-
  Gregorian calendar arithmetic (a day count plus time-of-day), never a `LeapTable` —
  correct across a month, a leap-year February, and a year boundary alike.
- **IOFM-R-007.** Velocity at an arbitrary query time, where one is needed, is a central
  finite difference of `position_km_at` over a short (1 s default) step, refusing
  whenever either position probe it needs does — never a wider, less accurate
  difference between two real, possibly far-apart samples.

- **IOFM-R-008.** (v1.1) The CRD pass view groups a file's records into `CrdPass` blocks as §3.3.1 states, in file order, and the flattened content of the passes agrees with the flat `read_crd` of the same text (the same normal-point records, the same sessions) — the view is an indexing of the file, not a second reading of it.
- **IOFM-R-009.** (v1.1) `CrdPass::wavelength_nm(config_id)` returns the nanometre value of field 3 of the `C0` record whose field 4 is `config_id`, to the printed digits; an id with no `C0` in force refuses (`IOFM-F-014`).
- **IOFM-R-010.** (v1.1) The `H2` epoch-time-scale code is resolved when the pass is built: 3, 4 and 7 to `TimeScale::UTC`, every other code refused (`IOFM-F-015`).
- **IOFM-R-011.** (v1.1) `decode_iod_angles` converts the 14 angle columns by the format of §3.5.1, blanks as zeros, to radians, within the ranges stated there, refusing what is outside them (`IOFM-F-016`).
- **IOFM-R-012.** (v1.1) `decode_iod_time_uncertainty` and `decode_iod_position_uncertainty` evaluate `M × 10^(X−8)` in seconds, and in radians by the format's unit (seconds, arcminutes or degrees of arc); two blanks give no value (`IOFM-F-017` for anything else that is not two digits).
- **IOFM-R-013.** (v1.2) An SP3 file that ends without its `EOF` line is read **only if it holds exactly the number of epochs its first line declares**, and then `Sp3File::eof_present` is false; with any other number of epochs it refuses as truncated (`IOFM-F-018`). A file that has the `EOF` line is read as before. `write_sp3` always writes the terminator, and `Sp3File`'s equality does not compare the flag (it records how the text ended, not what it says).
- **IOFM-R-014.** (v1.2) An SP3 comment line is a line that begins `/*` (`SP3D`) or `%/*` (the ILRS combined orbits); its text is what follows the marker and the one blank after it, and `Sp3Header::comments` holds the text of both kinds alike (`write_sp3` writes the `/*` form).
- **IOFM-R-015.** (v1.3) The clock field of an SP3 `P` record (columns 47–60) and the clock-rate field of a `V` record may be absent (the line stops before column 47) or blank (columns 47–60 all blank): the value is then 999999.999999, `SP3C`'s own value for an absent clock, and `Sp3File::clock_fields_absent` counts the records; the coordinates (columns 5–46) stay mandatory, and a clock that is present and cut short or not a number refuses `IOFM-F-001`.
- **IOFM-R-016.** (v1.3) An SP3 epoch line is read at the fixed columns and, as well, by its blank-separated fields (`*` and six numeric fields). Where only the fields read, the epoch is theirs and `Sp3File::epoch_lines_by_fields` counts the line; where both read and agree (calendar fields exactly, the second to 1 × 10⁻⁷ s) the fields' reading is used; where both read and differ the line refuses `IOFM-F-019`; where the fields do not read, the columns' result stands, the epoch or its `IOFM-F-001`.

---

## 5. Interfaces, stated language-free

- `Sp3Error / TleError / CrdError / CpfError / IodError / SinexError / AntexError =
  odl::Diagnostic`, one alias per format, matching every other module's established
  pattern (a shared alias would blur which reader a caller is looking at in a
  stack of nested `Result`s).
- `read_sp3(text: string) -> Result<Sp3File, Sp3Error>` / `write_sp3(Sp3File) ->
  Result<string, Sp3Error>`. `to_time_scale(Sp3TimeSystem) -> Result<odl::time::
  TimeScale, Sp3Error>` is a pure function, no `LeapTable` needed (§3.1) — a caller
  builds its own `Epoch` from an `Sp3Epoch`'s own `time::Calendar` and this scale.
- `read_tle(line1: string, line2: string) -> Result<Tle, TleError>` / `write_tle(Tle) ->
  Result<(string, string), TleError>` — two lines in, two lines out, matching the
  format's own two-line unit; `tle_checksum(line_without_checksum: string) -> int` is a
  pure function, `TLEFMT`'s own modulo-10 rule, exposed separately so `IOFM-A-0nn` can
  show a mismatched checksum refused without constructing a whole `Tle`.
- `read_crd(text: string) -> Result<CrdFile, CrdError>` / `write_crd(CrdFile) ->
  Result<string, CrdError>`.
- `read_crd_passes(text: string) -> Result<vector<CrdPass>, CrdError>` (v1.1, §3.3.1) with `CrdPass::wavelength_nm(config_id: string) -> Result<double, CrdError>`; the `time_scale` of a pass is an `odl::time::TimeScale`, already resolved.
- `decode_iod_angles(IodObservation) -> Result<IodAngles { kind: RaDec | AzEl, first_rad, second_rad }, IodError>` and, taking the format code as the character of column 45 so that a blank one can be refused as the line it came from would have it (`IOFM-F-016`; `read_iod` itself refuses such a line under `IOFM-F-005`), `decode_iod_angles(format_code: char, angle_raw: string_view)`; `decode_iod_time_uncertainty(IodObservation) -> Result<optional<double>, IodError>` (seconds); `decode_iod_position_uncertainty(IodObservation) -> Result<optional<double>, IodError>` (radians) (v1.1, §3.5.1).
- `read_cpf(text: string) -> Result<CpfFile, CpfError>` / `write_cpf(CpfFile) ->
  Result<string, CpfError>`.
- `read_iod(line: string) -> Result<IodObservation, IodError>` / `write_iod
  (IodObservation) -> Result<string, IodError>` — one observation per line, matching the
  format's own one-line-per-observation shape.
- `read_sinex(text: string) -> Result<SinexFile, SinexError>` / `write_sinex(SinexFile)
  -> Result<string, SinexError>`.
- `read_antex(text: string) -> Result<AntexFile, AntexError>` / `write_antex(AntexFile)
  -> Result<string, AntexError>`.
- `Sp3EphemerisError = odl::Diagnostic`. `Sp3Ephemeris::build(Sp3File, satellite_id: string,
  target_order: int = 10) -> Result<Sp3Ephemeris, Sp3EphemerisError>` /
  `.position_km_at(t_s: double) -> Result<Vec3, Sp3EphemerisError>` (§3.8).
  `calendar_elapsed_seconds(from: Calendar, to: Calendar) -> double` is a pure function,
  no `LeapTable`, the same "pure mapping, no table" shape `to_time_scale` already is.
  `lagrange_interpolate(points: vector<pair<double, Vec3>>, t_query: double) -> Vec3` is
  the raw fit with no span/gap/manoeuvre policy at all, exposed separately so the accuracy
  self-check (`IOFM-A-030`/`031`) can measure the polynomial's own error against a real,
  individually held-out sample without that sample's own absence being read as a data gap
  — a different, and deliberately stricter, question `position_km_at` alone answers for a
  caller who does not already know the answer.
  `central_difference_velocity_km_s(Sp3Ephemeris, t_s: double, h_s: double = 1.0) ->
  Result<Vec3, Sp3EphemerisError>` is `position_km_at`'s own obvious derivative (§3.8),
  not a class member — velocity is not this round's own stated scope for the class itself.

Every returned structure is **immutable after construction** (matching `Macromodel`'s own
`SPEC-macromodel.md` `MCRM-R-001` precedent) — a reader either succeeds with a complete,
internally-consistent structure or refuses; there is no partially-built, still-mutable
state a caller could observe mid-parse.

---

## 6. Precision and accuracy

- **IOFM-P-1.** SP3-d position fields are printed to `F14.6` (micrometres in km), read
  to full printed precision, not rounded further. 1 mm of SP3 position error is,
  directly, 1 mm of position error at the epoch it is stamped — no propagation or
  projection is done by this reader.
- **IOFM-P-2.** TLE angle fields (inclination, RAAN, argument of perigee, mean anomaly)
  are printed to `TLEFMT`'s own stated precision (four decimal degrees, 8 characters);
  1 × 10⁻⁴ deg at LEO altitude (7000 km) is **34 mm × (1×10⁻⁴ × π/180) ≈ 0.061 mm** —
  negligible against SGP4's own accuracy (metres to kilometres, L6 step 3's own concern),
  stated here only so a future reader does not wonder whether this reader's own
  print-precision handling is the dominant error source; it is not.
- **IOFM-P-3.** CRD range fields are printed to `F18.12` seconds of time-of-flight;
  at the speed of light this is **2.998 × 10⁸ m/s × 1×10⁻¹² s = 0.3 mm** of one-way
  range resolution — the format's own stated precision floor, not this reader's.

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `IOFM-F-001` | SP3 record's own numeric field, at the column position `SP3D` states, is not parseable as that field's own type (blank-filled, non-numeric, or the line is too short to contain the column range) | the record's own line number, the field name, the column range, and the text actually found there |
| `IOFM-F-002` | SP3's own `%c` line, columns 10–12, is not one of `GPS`, `GLO`, `GAL`, `BDT`, `TAI`, `QZS`, `UTC` | the text found, and the seven codes `SP3D` permits |
| `IOFM-F-003` | `to_time_scale` is called on an `Sp3TimeSystem` of `GLO`, `GAL`, `BDT` or `QZS` | the time system found, and that `odl::time::TimeScale` has no variant for it (§3.1) |
| `IOFM-F-004` | A CRD or CPF record's own leading type tag is not one of the record types `CRD2`/`CPF2` respectively define | the tag found, the line number, and the record types that ARE recognised |
| `IOFM-F-005` | An IOD line's own angle-format code (column 45) is not one of `IODFMT`'s own seven digits | the digit found |
| `IOFM-F-006` | A SINEX line's own first character is not one of `%`, `*`, `+`, `-`, or a space | the character found (or its absence, for an empty line), and the line number |
| `IOFM-F-007` | A TLE line's own trailing checksum digit does not match `tle_checksum` computed over the rest of the line | both digits, and which line |
| `IOFM-F-008` | An ANTEX antenna record's own `PCV TYPE` is neither `A` (absolute) nor `R` (relative) | the character found |
| `IOFM-F-009` | `Sp3Ephemeris::position_km_at` is called with `t_s` outside `[first sample, last sample]` | the query time and the sampled span's own two endpoints |
| `IOFM-F-010` | The interpolation window a query would use spans a gap more than 1.5× the file's stated epoch interval | the query time, the gap's own size, the file's own stated interval, and the two bracketing sample times |
| `IOFM-F-011` | The interpolation window a query would use includes a sample flagged `M` (Maneuver Flag) | the query time and the flagged sample's own time |
| `IOFM-F-012` | `Sp3Ephemeris::build` is called with a `satellite_id` that has fewer than 2 samples in the file | the satellite id and how many samples were found |
| `IOFM-F-013` | (v1.1) `read_crd_passes` finds a data record outside an `H4`…`H8` block, an `H4` with no `H2` or no `H3` in force, an `H4` before the previous block's `H8`, a block with no `H8` before the end of the text, or two `C0` records of one id in one pass | the line number, the record tag and which rule |
| `IOFM-F-014` | (v1.1) `CrdPass::wavelength_nm` is asked for a configuration id with no `C0` in force | the id, and the ids the pass does hold |
| `IOFM-F-015` | (v1.1) a pass's `H2` epoch-time-scale code is not 3, 4 or 7 | the code and the three accepted |
| `IOFM-F-016` | (v1.1) `decode_iod_angles` finds a non-digit that is not a blank, a sign that is not `+` or `-`, a minutes or seconds field of 60 or more, an hours field of 24 or more, a degrees field above 360 (azimuth) or 90 (declination, elevation), or no position reported (column 45 blank) | the field, the text found and the limit |
| `IOFM-F-017` | (v1.1) an uncertainty field is not two digits and not two blanks | the columns and the text found |
| `IOFM-F-018` | (v1.2) an SP3 file ends without its `EOF` line and holds a different number of epochs from the one its first line declares (refused as `IOFM-F-001` before v1.2); a line after the last epoch that is neither an epoch header nor `EOF` is still `IOFM-F-001` | the number of epochs held and the number declared | accepting a truncated file, or refusing a complete one |
| `IOFM-F-019` | (v1.3) an SP3 epoch line reads at the fixed columns and by its blank-separated fields to different epochs (a field a column off, which the columns alone would misread silently) | the line number, the line | reading the columns where a field is shifted, or refusing a line the fields read alone |
| (inherited) `R-ERR-1`/`R-ERR-2`/`R-ERR-3` | `SPEC-template.md` §5's own standing rules | unchanged; no persistent error state, no silently-dropped dependency warning, at most one named override anywhere in this module (none is needed by any reader here — every refusal above is a genuine format violation, not a legitimate operational case needing a documented escape hatch) |

---

## 8. Acceptance tests

Each format's own worked example is `SP3D`/`CRD2`/`CPF2`'s own PRINTED example file,
embedded verbatim as a test fixture — rule 2's own strongest source, a published worked
example, independent of anything this reader computes — not a self-consistency check
alone (which §8 rule 2 of `SPEC-template.md` requires stating explicitly where it would
otherwise be the only evidence; it is not, here).

Every id below is the literal `TEST_CASE` name in `modules/io/tests/`, one row per test, not a
planned/compressed range or an endpoint pair with an en dash between them — the first draft of
this table used exactly that shorthand for the round-trip rows, which `speccheck.py`'s own
discharge parser reads as neither endpoint, since a table cell's first identifier must stand
alone. `speccheck.py` (gate 7) caught this directly (three refusals reported UNCOVERED that this
table's own first draft never named in any row at all) and, once every row below was written
out individually, caught a SECOND, deeper defect it does not exist to be blind to either:
gate 7's own acceptance-row finder required a bare `-A-nnn` id with no lettered suffix, so a
properly-written row like `IOFM-A-004b` was invisible to it — the exact amendment-lettering
convention `SPEC-ephemerides.md`'s `EPH-A-001b` and this tree's own others already rely on,
simply never exercised on the ACCEPTANCE side of a discharge before this table needed it. Fixed
in `speccheck.py` itself (`tools/speccheck.py`, the row-finding regex), not worked around here;
§9 carries the full story.

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `IOFM-A-001` | **The predecessor's first defect, shown firing.** A reader that reads SP3's own Epoch Interval from the GPS-week/seconds-of-week columns instead of `SP3D` line two's own columns 25–38 is constructed as a deliberately-broken test double and shown to disagree with the real reader on `SP3D`'s own Example 1 (interval 900.0 s) | the broken double reads a different value than the real reader | `SP3D` Example 1, line 2 | exact | R-001, R-002 |
| `IOFM-A-002` | **The predecessor's second defect, shown firing.** A reader that assumes a fixed header line count (rather than reading until each sentinel record type) is shown to mis-parse `SP3D`'s own Example 1, which has more than 85 satellites and hence more header lines than the minimal five `+`/five `++` records | the fixed-count double fails to reach the first epoch's own real data; the real reader does | `SP3D` Example 1 | exact | R-001, R-002 |
| `IOFM-A-003` | `read_sp3` on `SP3D`'s own Example 1 (140 satellites, 5 comments) reproduces every header field and the first epoch's own position records exactly | the printed values | `SP3D` Example 1 | exact (transcription) | R-001 |
| `IOFM-A-004` | `read_sp3` on `SP3D`'s own Example 2 (26 satellites, P/V/EP/EV all present) reproduces the position, velocity, and both correlation records exactly, and the `%c` Time System field reads `GPS` | the printed values | `SP3D` Example 2 | exact | R-001 |
| `IOFM-A-004b` | An unrecognised CRD record-type tag refuses `IOFM-F-004` | the refusal | `CRD2` §1–§4's own 28 record types | — | F-004 |
| `IOFM-A-004c` | An unrecognised CPF record-type tag refuses `IOFM-F-004` | the refusal | `CPF2`'s own record types | — | F-004 |
| `IOFM-A-005` | `to_time_scale(Sp3TimeSystem::GLO)` refuses `IOFM-F-003`, likewise `GAL`/`BDT`/`QZS`; `GPS`/`TAI`/`UTC` each succeed with the matching `TimeScale` | the refusal, all four; the three successes | `IOFM-F-003` | — | F-003 |
| `IOFM-A-005b` | An unrecognised IOD angle-format code (column 45, not 1–7) refuses `IOFM-F-005` | the refusal | `IODFMT`'s own seven codes | — | F-005 |
| `IOFM-A-006` | `read_tle` on `STR3`'s own §13 sample element set (satellite 88888) reproduces every field, including the legitimately-absent international designator; the checksum validates | the printed values | `STR3` §13 | exact | R-001 |
| `IOFM-A-006b` | `tle_checksum` matches `TLEFMT`'s own modulo-10 rule on an independent hand-computed string | the computed checksum | `TLEFMT` | exact | R-001 |
| `IOFM-A-006c` | A corrupted TLE checksum digit refuses `IOFM-F-007` | the refusal | `IOFM-F-007` | — | F-007 |
| `IOFM-A-006d` | Disagreeing satellite numbers between a TLE's own two lines refuse | the refusal | `TLEFMT` (both lines carry the same field) | — | R-001 |
| `IOFM-A-007` | `read_crd` on `CRD2`'s own printed sample file (§6, LAGEOS2, MLRS station) reproduces `H1`–`H4` and every `12`/`20`/`30`/`40` record's own fields | the printed values | `CRD2` §6 sample | exact | R-001 |
| `IOFM-A-008` | `read_crd` on `CRD2`'s own second printed sample (normal-point data, record type `11`) reproduces every field of every `11` record | the printed values | `CRD2` §6 sample | exact | R-001 |
| `IOFM-A-008b` | An ANTEX `PCV TYPE` that is neither `A` nor `R` refuses `IOFM-F-008` | the refusal | `ANTEX14`'s own two codes | — | F-008 |
| `IOFM-A-009` | `read_cpf` on `CPF2`'s own printed sample header and position records (`gps35`) reproduces every field | the printed values | `CPF2` sample | exact | R-001 |
| `IOFM-A-010` | `read_iod` on a fixture built at `IODFMT`'s own documented column positions (§2's own licence note: not a literal transcription of a page-printed example, see the fixture's own header comment) reproduces every field, including the mantissa-exponent-adjacent uncertainty fields carried raw (§3.5) | the constructed values | `IODFMT`'s own column table | exact | R-001 |
| `IOFM-A-011` | `read_sinex` on a minimal synthetic file (header line, one `FILE/REFERENCE` block, footer) built strictly to `SINEX2` §2/§3's own stated grammar recovers the header fields and the one block's own lines verbatim; a line beginning with an unrecognised character refuses `IOFM-F-006` | the recovered fields; the refusal | `SINEX2` §2/§3 | exact | R-001, F-006 |
| `IOFM-A-012` | `read_antex` on a column-verified fixture (same construction discipline as `IOFM-A-010`) recovers the `NOAZI` row and the header/antenna/frequency fields | the constructed values | `ANTEX14`'s own column table | exact | R-001 |
| `IOFM-A-013` | SP3 round-trips: `read(write(read(fixture))) == read(fixture)`, for both `SP3D` examples | structural equality | self-consistency (rule 4 of `SPEC-template.md` §8 — stated explicitly: weak alone; not alone here, `IOFM-A-003`/`004` already anchor these same two fixtures to a published source) | exact | R-001 |
| `IOFM-A-013b` | The same round-trip property (`IOFM-A-013`'s own words), TLE, anchored to `IOFM-A-006`'s own `STR3` fixture | structural equality | self-consistency, same reasoning as `IOFM-A-013` | exact | R-001 |
| `IOFM-A-013d` | The same round-trip property, CRD, anchored to `IOFM-A-007`/`008`'s own `CRD2` fixtures | structural equality | self-consistency, same reasoning as `IOFM-A-013` | exact | R-001 |
| `IOFM-A-013e` | The same round-trip property, CPF, anchored to `IOFM-A-009`'s own `CPF2` fixture | structural equality | self-consistency, same reasoning as `IOFM-A-013` | exact | R-001 |
| `IOFM-A-013f` | The same round-trip property, IOD, anchored to `IOFM-A-010`'s own fixture | structural equality | self-consistency, same reasoning as `IOFM-A-013` | exact | R-001 |
| `IOFM-A-013g` | The same round-trip property, SINEX, anchored to `IOFM-A-011`'s own `SINEX2`-grammar fixture | structural equality | self-consistency, same reasoning as `IOFM-A-013` | exact | R-001 |
| `IOFM-A-013h` | The same round-trip property, ANTEX, anchored to `IOFM-A-012`'s own fixture | structural equality | self-consistency, same reasoning as `IOFM-A-013` | exact | R-001 |
| `IOFM-A-013c` | A hand-built `Tle` (not read from any fixture) round-trips | structural equality | self-consistency alone (no published-example anchor for this specific synthetic case; `IOFM-A-013b` already anchors TLE round-tripping to `STR3`) | exact | R-001 |
| `IOFM-A-014` | A hand-built, multi-epoch, multi-satellite `Sp3File` round-trips | structural equality | self-consistency alone (same reasoning as `IOFM-A-013c`; `IOFM-A-013` already anchors SP3 round-tripping to `SP3D`) | exact | R-001 |
| `IOFM-A-020` | A truncated SP3 line refuses `IOFM-F-001`, naming the field | the refusal | `IOFM-F-001` | — | F-001 |
| `IOFM-A-021` | An unrecognised SP3 Time System code refuses `IOFM-F-002` | the refusal | `IOFM-F-002` | — | F-002 |
| `IOFM-A-022` | **A real-world finding, not from either spec example.** An entirely BLANK `++` accuracy line (found on a real GSFC-produced Jason-3 SP3 file, `gscja3`, 2025-12 — not only `SP3D`'s own `0`-filled examples) is accepted, every slot reading `0`, the same "no accuracy given" meaning explicit zero-padding already states | every slot reads 0 | a real SP3 file this tree already holds (`PROVENANCE.md`, this round's own entry) | exact | R-001 |
| `IOFM-A-023` | `Sp3Ephemeris::position_km_at` reproduces every one of its own 15 sample nodes exactly, on a hand-built fixture | the fixture's own values | self-consistency (a Lagrange polynomial is exact at its own nodes, a property of the mathematics, checked directly rather than assumed) | 1e-8 km | R-003 |
| `IOFM-A-024` | A query before the first sample and a query after the last both refuse; the span's own two endpoints do not | the refusal, both directions; the two successes | `IOFM-F-009` | — | R-004, F-009 |
| `IOFM-A-025` | A query whose own window would span a genuine 120 s gap (a file with epoch index 6 of a 60 s series entirely absent) refuses; the same file interpolates cleanly clear of the gap | the refusal; the two successes | `IOFM-F-010` | — | R-005, F-010 |
| `IOFM-A-026` | A query whose own window includes a manoeuvre-flagged sample refuses; the same file interpolates cleanly clear of it | the refusal; the two successes | `IOFM-F-011` | — | R-005, F-011 |
| `IOFM-A-027` | `build` on a satellite id absent from the file, and on one present only once, both refuse | the refusal, both cases | `IOFM-F-012` | — | F-012 |
| `IOFM-A-028` | `build` with `target_order=10` on a 5-epoch file uses order 4 (`sample_count-1`), not the unreachable target, and still interpolates between nodes at that reduced order | `order() == 4`; a successful interior query | self-consistency | exact (order); a value returned | R-003 |
| `IOFM-A-029` | `calendar_elapsed_seconds` is exact across a plain day, a non-leap-year February (2026, 28 days), a leap-year February (2028, 29 days — the SAME nominal dates giving a DIFFERENT elapsed time), a year boundary, and a sub-minute fraction; antisymmetric when `from`/`to` are swapped | 86400 s / 86400 s / 172800 s / 86400 s / 29.5 s / −86400 s | self-consistency (proleptic Gregorian calendar arithmetic, a property of the algorithm) | exact | R-006 |
| `IOFM-A-032` | `central_difference_velocity_km_s` matches a synthetic fixture's own KNOWN analytic derivative at an interior, non-node query time; a query within `h_s` of the sampled span's own edge refuses, propagating `position_km_at`'s own `IOFM-F-009` | the analytic derivative; the refusal | self-consistency (the fixture's own closed-form derivative, a property of the polynomial, not assumed) | 1e-6 km/s | R-007, F-009 |
| `IOFM-A-030` | **Real-data accuracy.** An 11-point Lagrange fit against 13 real, individually held-out GPS G01 samples (IGS rapid combined solution, 900 s interval, `igs.bkg.bund.de`, no login) stays under 1 cm — measured 1.07 mm RMS, 1.47 mm max | error < 1 cm | a real IGS rapid-product SP3 file (`PROVENANCE.md`, this round's own entry); the 1 cm bound from Schenewerk (2003) / Horemuž & Andersson (2006), §3.8 | < 1 cm | R-003 |
| `IOFM-A-031` | **Real-data accuracy.** The same methodology against 13 real, individually held-out Jason-3 L39 samples (GSFC SLR+DORIS dynamic orbit, 60 s interval, `doris.ign.fr`, anonymous FTP) stays under 1 cm — measured 2.23 mm RMS, 4.43 mm max | error < 1 cm | a real GSFC dynamic-orbit SP3 file (`PROVENANCE.md`, this round's own entry); the 1 cm bound from Zeitlhöfler et al. (2024), §3.8 | < 1 cm | R-003 |
| `IOFM-A-033` | (v1.1) **pass grouping on a hand-built multi-session file**: `H1 H2 H3 H4 … H8` three times (the acceptable method of `CRD2` §4.4.2) and `H1 H2 H3 H4 … H8 H3 H4 … H8 H9` (the preferred method §4.4.1, the second session with its own `H3`): the passes' stations, targets, sessions, normal points and meteorology records are those of the fixture, in file order; the `H2`, `H3` and `C0` in force are the right ones in the second case | the fixture's values | `CRD2` §4.4's two orderings | exact | R-008 |
| `IOFM-A-034` | (v1.1) **real data**: the real monthly LAGEOS-1 file (EDC `lageos1_202601.np2`, 1 474 762 bytes, SHA-256 `c08df9c3dcb2156d…`, pinned in the manifest as `ilrs-lageos1-np-202601` when the pass view lands) gives **807 passes**, 4 628 normal points and 4 317 meteorology records in total, every pass with exactly one `C0`, one `H4` and at least one normal point; the first pass is Yarragadee (`7090 5 13`), 2026-01-01 02:07:49, its first normal point `7676.800587100000`, `0.051212898595` s, configuration `new`, event 2; **the flattened passes agree with the flat `read_crd` of the same text** (same normal points, same sessions) | the counts and the first pass | the real file, counted independently with `awk` | exact | R-008 |
| `IOFM-A-035` | (v1.1) `wavelength_nm`: the real first pass's configuration `new` gives **532.000**; a fixture with two `C0` records (`new` 532.000, `old` 1064.000) selects by the record's own id; an id with no `C0` refuses `IOFM-F-014` naming the id and the ids held | the values; the refusal | the real file; `CRD2` `C0` | exact | R-009, F-014 |
| `IOFM-A-036` | (v1.1) **the `H2` time-scale resolution**: codes 3, 4, 7 resolve to `TimeScale::UTC`; **0, 1, 2, 5, 6, 8, 9, 10 and 11 each refuse `IOFM-F-015`** from `read_crd_passes`, naming the code and the three accepted; the flat `read_crd` still reads code 5 | the table | `CRD2` field 6 | exact | R-010, F-015 |
| `IOFM-A-037` | (v1.1) the meteorology record typed: the real file's `20  7676.801  976.70 310.30  12. 0` is 7676.801 s, 976.70 mbar, 310.30 K, 12 %, origin 0 | the values | the real file; `CRD2` record `20` | exact | R-008 |
| `IOFM-A-038` | (v1.1) the structure refusals, each alone and each naming its line: a `11` before any `H4`; an `H4` with no `H3`; an `H4` opened before the previous `H8`; a block with no `H8` at the end of the text; two `C0` of the same id; **the real file is accepted** (so none of these fires on it) | the refusals; the acceptance | `CRD2` §1.4, §4 | exact | R-008, F-013 |
| `IOFM-A-039` | (v1.1) the IOD angle decoder against **the document's own four examples** (formats 1, 2, 3 and 7: `1122334+112233` is 11 h 22 m 33.4 s, +11° 22′ 33″; `1122   +1122  ` is 11 h 22.000 m, +11° 22.00′ with the blanks zero; `11223  +112   ` is 11 h 22.300 m, +11.2000°; `1122334+112222` is 11 h 22 m 33.4 s, +11.2222°) and hand-built lines for formats 4, 5, 6 to the printed layout (weaker: the document prints no azimuth/elevation example); the sign, the wrap of azimuth into [0, 2π) | the decoded radians | `IODFMT`'s examples | 1 × 10⁻¹⁵ rad | R-011 |
| `IOFM-A-040` | (v1.1) the decoder's refusals: minutes 60, hours 24, azimuth 361°, declination 91°, a letter in a digit position, a blank sign, a blank format code each refuse `IOFM-F-016`; the adjacent in-range values pass (minutes 59, hours 23, azimuth 360°, declination 90°) | the refusals | `IODFMT` | exact | R-011, F-016 |
| `IOFM-A-041` | (v1.1) the uncertainties: the document's own `MX` examples — 15 is 0.001, 56 is 0.05, 17 is 0.1, 97 is 0.9, 18 is 1, 28 is 2, 58 is 5, 19 is 10, 99 is 90 — in the time unit, and in each format's angle unit (seconds, arcminutes, degrees) converted to radians; two blanks give no value; `1 ` and `ab` refuse `IOFM-F-017` | the values | `IODFMT`'s examples | 1 × 10⁻¹⁵ relative | R-012, F-017 |
| `IOFM-A-042` | (v1.2) the terminator and the count: a hand-built SP3 text of three epochs whose first line declares three and that has no `EOF` reads, with `eof_present` false and the three epochs; the same text with the `EOF` line reads with `eof_present` true; the same text declaring four epochs and without `EOF` refuses `IOFM-F-018` naming 3 and 4; a text with a stray line after the last epoch refuses `IOFM-F-001`; `write_sp3` of the first writes the `EOF` line, and the round trip equals it | the values | `SP3D`; the real ILRS file's shape | exact | R-013, F-018, F-001 |
| `IOFM-A-043` | (v1.2) **real data:** the pinned ILRS weekly orbit of LAGEOS-1 (`ilrs-lageos1-sp3-260103`, SP3-c) reads whole: 5040 epochs of 120 s, the one satellite `L51`, the coordinate system `SLR20`, the time system `UTC`, `eof_present` false, the first epoch 2025-12-28 00:00:00 and the last 2026-01-03 23:58:00, and its `%/*` comment lines read, one of them `Reference TRF: SLRF2020` | the values | the file; `MEAS-A-063` | exact | R-013 |
| `IOFM-A-044` | (v1.2) a comment line `%/* text` reads as the comment `text`, as `/* text` does: a hand-built SP3 text whose comment lines are written in the two forms gives the same `comments` as the same text written all in the `/*` form; a bare `%/*` and a bare `/*` give an empty comment | the values | `SP3D`; the real ILRS file | exact | R-014 |
| `IOFM-A-045` | (v1.3) the epoch line: a hand-built text of three epochs written in the backup combination's form (`* 2013  4  3  0  0  0.00000000`, one column to the left, 30 characters) reads, with the same epochs as the standard text and `epoch_lines_by_fields` 3; a seconds field one column to the right agrees and reads with the count 0; one with a nonzero last decimal (`5.00000001`) reads to 1 × 10⁻¹² s; a shifted line with a non-numeric field, or with five fields, refuses `IOFM-F-001`; the shifted line padded with blanks to 80 columns refuses `IOFM-F-019` (the columns read it as year 13) | the values; the refusals | `SP3D`, `SP3C`; the real backup combination | exact; 1 × 10⁻¹² s | R-016, F-019, F-001 |
| `IOFM-A-046` | (v1.3) the clock: the same `P` and `V` records written to column 46, and with blank columns 47–60 (the sigma columns after them still reading: 18, 219, 191), read with 999999.999999 and the count 2; the same padded to 80 columns; a clock cut at column 55, a non-numeric clock, and a record cut inside its z coordinate refuse `IOFM-F-001` (the first naming the clock, the last the z); the round trip of the reading equals it, the writer writing the number | the values; the refusals | `SP3C`; the real GFZ, JCET and NSGF products | exact | R-015, F-001 |
| `IOFM-A-047` | (v1.3) **real data:** four more of the week's products read whole — `ilrsb` (5040 epochs, every epoch line by fields, 10 080 absent clocks, frame field `ITRF1`), `gfz` (5041, last epoch 2026-01-04 00:00, 10 082), `jcet` (5040, 10 080), `nsgf` (5043, last epoch 2026-01-04 00:04, 10 086) — each the one satellite `L51`, time system `UTC`, first epoch 2025-12-28 00:00:00, one record per epoch, the first position and velocity to the printed digits, every clock the marker | the values | the files | exact | R-015, R-016 |

**Coverage.** Every requirement and refusal above is discharged by a row; none require excusing.

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as a new numbered section: the eight sources above, each
  fetched directly, hash-pinned, with `STR3`'s own partial-extraction finding (the
  scanned T-card/G-card format sheet did not survive `pdftotext`, recorded with the
  search that established its absence, per plan §4 rule 4); the decision to read
  `TLEFMT` for the TLE column layout instead of `STR3`'s own unreadable sheet, and why
  that is not a departure from D4 (D4 names `STR3` for SGP4's own equations and test
  vectors, not for the TLE format, which `STR3` never claims to define more precisely
  than `TLEFMT` does); the CRD/SINEX scope decisions (§3.3, §3.6) naming exactly which
  record/block kinds are parsed field-by-field versus carried opaque, and why; the SP3
  GLO/GAL/BDT/QZS refusal design (§3.1) and its own reasoning.
- The same section also records §3.8's own round: the three interpolation-order sources
  (`SCHEN03`, `HORA06`, `ZEIT24`) and their own converging findings; the real GNSS and
  Jason-3 SP3 files obtained for `IOFM-A-030`/`031` (host, path, licence/retrievability
  basis, SHA256); the measured holdout accuracy on each (RMS and max, both under 1 cm);
  and the six comparison tools re-pointed at `Sp3Ephemeris`, retiring their own per-tool
  lookup and interpolation code.

- **v1.1 (2026-10-06)** is recorded in the same section's extension for L6 step 4: the correction of §3.3's "does not need … detail" (found against a real file, with the file's hash), the real file's structure counts, and the IOD decoders' sources (`IODFMT`'s tables and examples).

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `IOFM-Q-001` | `CRD2`'s own configuration/calibration/statistics record kinds (`C0`–`C7`, `40`–`42`, `50`, `9X`) are read opaque (§3.3) rather than field-by-field, since this round's own consumer (L6 step 4's range residual) does not need them. Worth field-by-field treatment if a future consumer (e.g. a laser-system-specific correction) needs it. *[Amended 2026-10-06 (v1.1), the text before kept: L6 step 4 does need one field — the `C0` wavelength — which §3.3.1 now types; the rest stays opaque.]* |
| `IOFM-Q-002` | `SINEX2`'s own ~20 named blocks are read at the general block-structure level only (§3.6); no block's own field semantics are modelled. Worth building specific block readers (`SITE/ID`, `SOLUTION/ESTIMATE`) if L7's estimator or L8's campaigns need to consume a SINEX solution rather than only this tree's own SINEX output (if any). |
| `IOFM-Q-003` | `STR3`'s own T-card/G-card format sheet could not be extracted from the fetched PDF (§2, §9). A cleaner scan or an alternative digitisation, if one is found later, would let this spec cite `STR3` directly for the TLE column layout too, alongside `TLEFMT`. Not pursued further this round since `TLEFMT` already gives a complete, primary, directly-fetched layout. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.0 | 2026-09-25 | first draft: the eight readers, the SP3 interpolation of §3.8 |
| 1.1 | 2026-10-06 | L6 step 4's needs (`SPEC-measmod.md` §9.2): the CRD pass view, the `C0` wavelength accessor, the `H2` time-scale resolution (`IOFM-R-008`…`R-010`, `F-013`…`F-015`); the IOD angle and uncertainty decoders (`IOFM-R-011`, `R-012`, `F-016`, `F-017`); acceptance rows `IOFM-A-033`…`A-041`; §3.3's statement that no configuration field is needed superseded and kept visible; `IOFM-Q-001` annotated |
| 1.2 | 2026-10-06 | L6 step 4 item 5, found by the real ILRS weekly orbit, which departs from `SP3D` in two ways: its comment lines begin `%/*` (`IOFM-R-014`) and it has no `EOF` line; a file without the terminator is read if it holds the number of epochs its header declares, otherwise refused as truncated, as before but under a new id (`IOFM-R-013`, `IOFM-F-018`; `Sp3File::eof_present`); `IOFM-A-042` … `-044` |
| 1.3 | 2026-10-06 | L6 step 4 item 7, found by four of the nine other products of the ILRS week that sizes G5's orbit term, which `SP3D`'s fixed columns refused: an absent or blank clock and clock rate read as `SP3C`'s own 999999.999999 (`IOFM-R-015`, `Sp3File::clock_fields_absent`); an epoch line whose columns do not read is read by its six blank-separated fields, and one whose two readings differ is refused (`IOFM-R-016`, `IOFM-F-019`, `Sp3File::epoch_lines_by_fields`); `IOFM-A-045` … `-047`, the last on the four real products |
