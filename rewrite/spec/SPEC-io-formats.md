# SPEC-io-formats — L6 step 1: formats in, as read structures

| | |
|---|---|
| **Spec ID** | `IOFM` |
| **Status** | **draft** 2026-09-25, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-25 |
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
write, re-read, structures equal) is this step's own stated gate.

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
| `STR3` | Hoots, F. R., Roehrich, R. L. (US Air Force Aerospace Defense Command) | *Spacetrack Report No. 3: Models for Propagation of NORAD Element Sets* | December 1980 | `https://celestrak.org/NORAD/documentation/spacetrk.pdf`, fetched directly 2026-09-25, SHA256 `0ac48df7724b857c14431764a7baa0bf9aa64e570e3941b7244a15cdaea708bd` | **primary, partial** — the T-card/G-card element-set format sheet this 1980 typeset report itself refers to did not survive text extraction (a scanned tabular page; confirmed absent by search — zero hits for "T-CARD", "G-CARD" or "FORMAT SHEET" in the extracted text, the search and its terms recorded per plan §4 rule 4's own two-halves requirement); the physical constants (§12), propagation equations, and sample test cases (§13) extracted cleanly and are this tree's own primary source for L6 step 3's `sgp4` | normative for step 3's propagation equations and test vectors; NOT this reader's own source for the TLE column layout, which comes from `TLEFMT` instead |
| `TLEFMT` | CelesTrak (T.S. Kelso) | *NORAD Two-Line Element Set Format* | current, fetched 2026-09-25 | `https://celestrak.org/NORAD/documentation/tle-fmt.php`, fetched directly 2026-09-25, SHA256 `442053462ac8a23761fe27d519478331a1531bfd38d4a3082487628a0662794c` | **primary**, obtained in full | the TLE reader's own column-by-column layout for both lines, and the modulo-10 checksum algorithm — the same de facto format `STR3`'s own T-card describes, documented legibly where `STR3`'s own scanned sheet did not extract |
| `CRD2` | Ricklefs, R. L., for the ILRS Data Format and Procedures Working Group | *Consolidated Laser Ranging Data Format (CRD)*, Version 2.00/2.01 | 19 September 2019 | `https://ilrs.gsfc.nasa.gov/docs/2022/crd_v2.01e3.pdf`, fetched directly 2026-09-25, SHA256 `9b7ef5ebbb573418ef3e0d8f89691011eb3afd111734bde96186d55ddaa848d3` | **primary**, obtained in full | the CRD reader: every header record (`H1`–`H5`, `H8`, `H9`), configuration records (`C0`–`C7`), and data records (`10` full-rate range, `11` normal-point range, `12` range supplement, `20` meteorological, and the remainder read generically, §3.3 below) |
| `CPF2` | Ricklefs, R. L., for the ILRS Predictions Formats Study Group | *Consolidated Laser Target Prediction Format*, Version 2 | 28 February 2018 | `https://ilrs.gsfc.nasa.gov/docs/2018/cpf_2.00h-1.pdf`, fetched directly 2026-09-25, SHA256 `36c70d0ad113e019e8c7c1600fd4e65c55eb31112297d74aa4f236ddc3d23181` | **primary**, obtained in full | the CPF reader: header records `H1`–`H5`, `H9`, and position/velocity ephemeris entries (`10`, `20`) |
| `IODFMT` | Lewis, G. D. | *IOD Observation Format Description* | Version 0, 10 October 1998, clarified 24 February 2002 | `https://www.satobs.org/position/IODformat.html`, fetched directly 2026-09-25, SHA256 `c781b04fcaccd66fea6f96d601a27c8b0e1d76121d20d212dee4b2381586fa04` | **primary**, obtained in full | the IOD reader: the 80-column field layout, the seven RA/DEC and AZ/EL angle-format codes, the mantissa-exponent uncertainty encoding |
| `SINEX2` | IGS/IERS/ILRS/IVS SINEX Working Group | *SINEX — Solution (Software/technique) INdependent EXchange Format*, Version 2.02 | 1 December 2006 | `https://ivscc.gsfc.nasa.gov/products-data/sinex_v202.pdf`, fetched directly 2026-09-25, SHA256 `246f42b88032d3357289cd698224fed3f2ac5b360ecd27e8f2c3d95e54eb69b8` | **primary, partial** — the general file/block structure (§2), the header line (§3) and the `FILE/REFERENCE`/comment-line conventions extracted cleanly; the per-block field tables for the ~20 named blocks (`SITE/ID`, `SOLUTION/ESTIMATE`, etc.) were not individually transcribed this round, per this spec's own §1 scoping — a caller reads a specific block's own fields against `SINEX2` directly, the same way `IGSMETA`'s `SATELLITE/MASS` field was read at L5 | normative for the general reader (header line, block delimiters `+`/`-`, comment lines, footer); a specific block's own semantics are each block's own future consumer's responsibility |
| `ANTEX14` | Rothacher, M., Schmid, R. (TU München) | *ANTEX: The Antenna Exchange Format*, Version 1.4 | 15 September 2010 | `https://files.igs.org/pub/data/format/antex14.txt`, fetched directly 2026-09-25, SHA256 `86458154367916d2fa6282ca2f8e5137c3d9663d7c18eb44eaf344c8e7eba621` | **primary**, obtained in full | the ANTEX reader: the header block, the per-antenna PCO (north/east/up) and PCV (`NOAZI` and azimuth-dependent) grid records |

**Licence note, all eight sources.** Every document above is a public, first-party format
specification, fetched directly by plain HTTP GET, no login, no account, no request
form — the same retrievability bar every other rule-4 search in this tree applies. None
states redistribution terms for the DOCUMENT itself, which this module does not
redistribute: it implements a reader from the format's own printed description, the same
"clean-room, spec-derived code" treatment the IERS Conventions tables and the RKF7(8)
coefficients already receive in this tree (`../plan/PLAN.md` §3.11 point 4), not a
transcription of the document's own prose or a vendoring of its bytes. `IODFMT` alone
states a copyright ("Copyright (C) 1998, G. Lewis") on the document; the same reasoning
applies — implementing a parser for the FORMAT it describes is not a reproduction of the
document.

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
- `read_cpf(text: string) -> Result<CpfFile, CpfError>` / `write_cpf(CpfFile) ->
  Result<string, CpfError>`.
- `read_iod(line: string) -> Result<IodObservation, IodError>` / `write_iod
  (IodObservation) -> Result<string, IodError>` — one observation per line, matching the
  format's own one-line-per-observation shape.
- `read_sinex(text: string) -> Result<SinexFile, SinexError>` / `write_sinex(SinexFile)
  -> Result<string, SinexError>`.
- `read_antex(text: string) -> Result<AntexFile, AntexError>` / `write_antex(AntexFile)
  -> Result<string, AntexError>`.

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

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `IOFM-Q-001` | `CRD2`'s own configuration/calibration/statistics record kinds (`C0`–`C7`, `40`–`42`, `50`, `9X`) are read opaque (§3.3) rather than field-by-field, since this round's own consumer (L6 step 4's range residual) does not need them. Worth field-by-field treatment if a future consumer (e.g. a laser-system-specific correction) needs it. |
| `IOFM-Q-002` | `SINEX2`'s own ~20 named blocks are read at the general block-structure level only (§3.6); no block's own field semantics are modelled. Worth building specific block readers (`SITE/ID`, `SOLUTION/ESTIMATE`) if L7's estimator or L8's campaigns need to consume a SINEX solution rather than only this tree's own SINEX output (if any). |
| `IOFM-Q-003` | `STR3`'s own T-card/G-card format sheet could not be extracted from the fetched PDF (§2, §9). A cleaner scan or an alternative digitisation, if one is found later, would let this spec cite `STR3` directly for the TLE column layout too, alongside `TLEFMT`. Not pursued further this round since `TLEFMT` already gives a complete, primary, directly-fetched layout. |
