# SPEC-io-horizons — L6 step 2: JPL Horizons vector tables, as read

| | |
|---|---|
| **Spec ID** | `IOHZ` |
| **Status** | **draft** 2026-09-25, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-25 |
| **Layer** | L6 `io-measurements` (`../plan/PLAN.md` §3.7), step 2 (Horizons client) |
| **Depends on** | `core` (`odl::Result`), `time` (`Calendar`, `TimeScale`) |
| **Depended on by** | L6 step 3 (`sgp4`) — oracle case `T-01`'s own required-disagreement gate compares this reader's own output against the TLE reader's (`SPEC-io-formats.md`) own SGP4-propagated, TEME-to-J2000-converted state |

**Derivation declaration (plan R1).** Written from the documents listed in §2 and from no
implementation of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation. `oracle/ORACLE.md` and
`oracle/cases.tsv` WERE read — `ORACLE.md` §1's own words are explicit that running and reading
the oracle's own frozen output is permitted to anyone ("observing what a program outputs is not
derived from its expression... comparing against it is legitimate at all") and that only its
*source* is forbidden. `oracle/capture.sh` was NOT opened, per that same section's own instruction
("do not go looking inside") — its role in this document is limited to the two facts §9 states,
both read from `environment.txt`'s own hash list, never from the script.

---

## 1. Purpose and scope

One reader: `read_horizons(text) -> Result<HorizonsEphemeris, HorizonsError>`, parsing an
already-fetched JPL Horizons **vector-table** response (`EPHEM_TYPE=VECTORS`, `VEC_TABLE=2`,
state vectors) into a time series of geocentric ICRF position and velocity. No file or network
access in this module — the same "parses text, does not open it" split every reader in
`SPEC-io-formats.md` already draws.

**Not in scope**: fetching a table over the network — no new fetch code is written; a Horizons
query is a URL like any other manifest entry's, so `manifest/manifest.json` plus the tree's own
existing `tools/fetch.py` (unchanged) is the whole mechanism, §9 states why. Observer tables
(RA/Dec) and element tables — `T-01`'s own comparison is a state-vector comparison, and nothing
else in this layer needs an angle or an orbital-element table from Horizons. SGP4 itself and the
TEME→J2000 conversion it needs (L6 step 3, `SPEC-sgp4.md`). The `T-01` gate itself, which
compares this reader's own output against the TLE reader's — that gate is step 3's own, since it
needs SGP4 to exist first; this step only has to deliver a reader step 3 can build it from.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `HZAPI` | Solar System Dynamics Group, Jet Propulsion Laboratory (NASA/Caltech) | *Horizons API Documentation* | current, fetched 2026-09-25 | `https://ssd-api.jpl.nasa.gov/doc/horizons.html`, fetched directly 2026-09-25, SHA256 `6ca45d9c222d6e5c425ae3e0a3c44225cf8b6d30c4a14892b0f1157b0ab4cfe4` | **primary**, obtained in full | the query parameters this reader's own client side states explicitly (`TIME_TYPE`, `VEC_CORR`, `VEC_TABLE`, `REF_SYSTEM`, `OUT_UNITS`, `CENTER`), and the vector-table output format this reader parses |

**A real query, run directly, not redistributed as a fixture pending §9's own open licence
question.** `https://ssd.jpl.nasa.gov/api/horizons.api` was queried directly this round
(2026-09-25) for object `-159588` (ACS3, the same real object oracle case `T-01` and `O-*` use,
§9) to verify `HZAPI`'s own documented format against real output before trusting it — the SAME
"published description, verified against a real response" discipline `SPEC-io-formats.md` §3.3/
3.4's own real CRD2/CPF2 samples already used. This confirmed: every data line ends with the
literal token `TDB` when `TIME_TYPE=TDB` is requested (§3.1 below relies on this); the header
prints `Output units`, `Output type` and `Reference frame` lines exactly matching what was
requested (`KM-S`, `GEOMETRIC cartesian states`, `ICRF`); records are bounded by literal `$$SOE`/
`$$EOE` lines; each record is three lines (a JD/calendar/time-system line, an X/Y/Z line, a
VX/VY/VZ line). The response text itself is **not** committed as a test fixture or manifest entry
this round — JPL's own redistribution terms for Horizons *output data* (distinct from `HZAPI`
itself, a public API description) were searched and found genuinely unclear (§9), and the manager
must rule before any real Horizons response is checked into this tree.

**Licence note, `HZAPI`.** A public, first-party API specification, fetched directly by plain
HTTP GET, no login, no account, no request form. States no redistribution terms for the document
itself, which this module does not redistribute — it implements a reader from the format's own
printed description, the same clean-room treatment every source in `SPEC-io-formats.md` §2
already receives.

---

## 3. Definitions and conventions

### 3.1 No internal `Epoch`, and only one time system is ever accepted

Like every reader in `SPEC-io-formats.md` (§3.1 there), this module never constructs an
`odl::time::Epoch` — no `LeapTable` is available here. Unlike those readers, though, this one
does not need a general time-system mapping: the client (§5) always requests `TIME_TYPE=TDB`
explicitly, never relying on Horizons' own default (which `HZAPI` states varies by `EPHEM_TYPE`),
so `TDB` is the only time system this reader is designed to see. `HorizonsTimeSystem` has two
members, `Tdb` and `Ut`, because `HZAPI` names both as valid for a vector table — but
`to_time_scale` accepts only `Tdb`; `Ut` refuses (`IOHZ-F-002`). This is deliberately narrower
than `SPEC-io-formats.md`'s own `to_time_scale`, which maps several real, expected codes: a vector
table printing `UT` here means the request was not honoured as asked, a genuine anomaly this
reader's own caller needs to know about, not a second legitimate case to support. `HZAPI` itself
does not define exactly what "UT" denotes at this API boundary (UT1, or a UTC-like civil time) —
refusing it outright avoids needing to resolve that ambiguity, since this reader never requests it
and constraint 4 (refuse rather than approximate) applies exactly here.

### 3.2 The header block — opaque, scoped, but three lines are checked

`Target body name`, `Center body name`, `Start time`/`Stop time`/`Step-size`, `Center geodetic`/
`Center cylindric`/`Center radii`, `Calendar mode`, `EOP file`/`EOP coverage` and the column-
meaning prose are carried opaque — read past, not modelled — the same "opaque, scoped, stated"
treatment `SPEC-io-formats.md` §3.3/§3.6 already give CRD's/CPF's own configuration records and
SINEX's own per-block fields, and for the same reason: no consumer this round needs them
(`IOHZ-Q-001`, §10). Three header lines ARE checked, because this reader's own one stated purpose
(a geocentric, geometric, ICRF state vector T-01 can compare against a TEME-derived one) depends
on them: `Output units` must read `KM-S` (`IOHZ-F-003`), `Reference frame` must read `ICRF`
(`IOHZ-F-004`), and `Output type` must read `GEOMETRIC cartesian states` (`IOHZ-F-005`) — the last
because `HZAPI`'s own `VEC_CORR` parameter can select light-time or stellar-aberration-corrected
output instead, a DIFFERENT physical quantity than the instantaneous geometric state SGP4's own
TEME output is, and comparing the two would measure a spurious light-time offset, not the frame
conversion `T-01` exists to check.

### 3.3 The data block — sentinel-bounded, three lines per record, the label checked every time

Records are found between the literal `$$SOE` and `$$EOE` lines, never by a fixed count
(`IOHZ-R-001`, the same discipline `IOFM-R-001` already states for SP3, for the identical reason:
the number of records depends on the requested time span and step size, which this reader does not
otherwise track). Each record is exactly three lines: a Julian-date/calendar/time-system line
(`2460615.500000000 = A.D. 2024-Nov-01 00:00:00.0000 TDB`, the real query's own literal output,
§2), an `X =... Y =... Z =...` line, and a `VX=... VY=... VZ=...` line. The time-system token at
the end of the FIRST line of every record is read and checked against `Tdb` (`IOHZ-R-002`,
`IOHZ-F-002`) — per record, not once for the whole file — because nothing in `HZAPI` states that a
single response cannot mix output conventions, and checking once and trusting the rest would be
exactly the "requested, not verified" gap §3.1 already refuses to leave open.

---

## 4. Required behaviour

- **IOHZ-R-001.** Records are found by their own `$$SOE`/`$$EOE` sentinel lines, never a fixed
  count — §3.3.
- **IOHZ-R-002.** Every record's own printed time-system token is read and checked to be `TDB`,
  independently of every other record — §3.1/§3.3.
- **IOHZ-R-003.** The header's own `Output units`, `Reference frame` and `Output type` lines are
  read and checked (`KM-S`, `ICRF`, `GEOMETRIC cartesian states`) before any record is trusted —
  §3.2.

---

## 5. Interfaces, stated language-free

- `HorizonsError = odl::Diagnostic`.
- `enum class HorizonsTimeSystem { Tdb, Ut }`.
- `to_time_scale(HorizonsTimeSystem) -> Result<odl::time::TimeScale, HorizonsError>` — a pure
  function, no `LeapTable` needed (§3.1); `Tdb` succeeds with `TimeScale::TDB`, `Ut` refuses
  (`IOHZ-F-002`).
- `struct HorizonsStateRecord { time::Calendar epoch; HorizonsTimeSystem time_system;
  odl::Vec3 position_km; odl::Vec3 velocity_km_s; }`.
- `struct HorizonsEphemeris { std::string target_body; std::string center_body;
  std::vector<HorizonsStateRecord> states; }` — `target_body`/`center_body` are the header's own
  `Target body name`/`Center body name` lines, carried verbatim (not parsed further) so a caller
  can at least log which object and center a table was actually for.
- `read_horizons(text: string) -> Result<HorizonsEphemeris, HorizonsError>`.
- **The query itself, stated for whoever runs the fetch (§9), not a function in this module**: 
  `EPHEM_TYPE=VECTORS`, `VEC_TABLE=2`, `VEC_CORR=NONE`, `REF_SYSTEM=ICRF`, `TIME_TYPE=TDB`,
  `OUT_UNITS=KM-S`, `CENTER=500@399` (Earth body center, geocentric — matching a TEME/J2000 state,
  never topocentric), `COMMAND` the object's own Horizons id.

Every returned structure is **immutable after construction**, matching every other reader in this
tree (`SPEC-io-formats.md` §5's own precedent).

---

## 6. Precision and accuracy

- **IOHZ-P-1.** Position and velocity fields are printed to Horizons' own full stated precision
  (`OUT_UNITS=KM-S`, ~15 significant digits observed in the real query, §2) and read to that full
  printed precision, not rounded further — the same stance `SPEC-io-formats.md` `IOFM-P-1` takes
  for SP3.

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `IOHZ-F-001` | `$$SOE` or `$$EOE` is missing entirely, a record between them is incomplete (fewer than three lines remain before `$$EOE`), or a record's own three lines do not match the documented `JD = calendar TIME_SYSTEM` / `X = Y = Z =` / `VX= VY= VZ=` shapes | the line number and the text found. Content BEFORE `$$SOE` or AFTER `$$EOE` is never inspected for this shape — a real response always carries substantial header and footer prose there, which is normal, not a violation |
| `IOHZ-F-002` | A record's own printed time-system token is not `TDB` | the token found, and the one token this reader accepts |
| `IOHZ-F-003` | The header's own `Output units` line does not read `KM-S` | the text found |
| `IOHZ-F-004` | The header's own `Reference frame` line does not read `ICRF` | the text found |
| `IOHZ-F-005` | The header's own `Output type` line does not read `GEOMETRIC cartesian states` | the text found, and why (§3.2: a light-time or aberration correction changes the physical quantity) |
| (inherited) `R-ERR-1`/`R-ERR-2`/`R-ERR-3` | `SPEC-template.md` §5's own standing rules | unchanged; no persistent error state, no silently-dropped dependency warning, no named override anywhere in this module |

---

## 8. Acceptance tests

Every id below is the literal `TEST_CASE` name in `modules/io/tests/horizons_tests.cpp`, one row
per test (`SPEC-io-formats.md` §8's own record of why a compressed range is refused here too).
Fixtures are HAND-BUILT at `HZAPI`'s own documented positions/tokens (rule 2's own weaker,
"built at the source's own stated shape" tier — the real query response itself is not committed,
§2/§9), not a published worked example in the strict rank-1 sense; each test's own `source` column
states this rather than overclaiming.

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `IOHZ-A-001` | `read_horizons` on a fixture built at the real query's own documented three-line record shape (§2/§3.3), two records, reproduces `target_body`, `center_body`, and every field of both records exactly | the fixture's own values | `HZAPI` + the real query's own confirmed shape, §2 | exact | R-001, R-002, R-003 |
| `IOHZ-A-002` | A record whose own time-system token reads `UT` instead of `TDB` refuses `IOHZ-F-002`, naming the token found | the refusal | `IOHZ-F-002` | — | R-002, F-002 |
| `IOHZ-A-003` | A header whose `Output units` line reads `AU-D` instead of `KM-S` refuses `IOHZ-F-003` | the refusal | `IOHZ-F-003` | — | R-003, F-003 |
| `IOHZ-A-004` | A header whose `Reference frame` line reads `B1950` instead of `ICRF` refuses `IOHZ-F-004` | the refusal | `IOHZ-F-004` | — | R-003, F-004 |
| `IOHZ-A-005` | A header whose `Output type` line reads `ASTROMETRIC cartesian states` (a `VEC_CORR=LT` response, `HZAPI`'s own documented alternative, §3.2) instead of `GEOMETRIC cartesian states` refuses `IOHZ-F-005` | the refusal | `IOHZ-F-005` | — | R-003, F-005 |
| `IOHZ-A-006` | A response missing its own `$$EOE` line entirely refuses `IOHZ-F-001`; ordinary header/footer prose surrounding `$$SOE`/`$$EOE` (present in every real response) is never mistaken for a malformed record | the refusal; the surrounding prose does not itself trigger a refusal | `IOHZ-F-001` | — | R-001, F-001 |
| `IOHZ-A-007` | `to_time_scale(HorizonsTimeSystem::Tdb)` succeeds with `TimeScale::TDB`; `to_time_scale(HorizonsTimeSystem::Ut)` refuses `IOHZ-F-002` | both outcomes | §3.1 | — | F-002 |
| `IOHZ-A-008` | **Real-data.** `read_horizons` on the manifest's own real, pinned ACS3 capture (`horizons-acs3-vectors`, `FACTUAL-DATA-CITED`, §9) parses cleanly: 5 real records, the real target/center body names, every position/velocity within a plausible LEO range (994×1023 km altitude) | 5 records parse; every value plausible | a real query, committed (§9) | plausibility bound, not exact | R-001, R-002, R-003 |

**Coverage.** Every requirement and refusal above is discharged by a row; none require excusing.

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as an extension of the L6 numbered section: `HZAPI` itself (fetched
  directly, hash-pinned); the real ACS3 query run to verify the documented format against real
  output; the fetch/parse split and why no new fetch code was written (a Horizons query is a URL
  like any other manifest entry's); the licence search and the manager's own ruling that resolved
  it (`IOHZ-Q-002`, §10) — the `search_recorded` pointer this spec's own manifest entry names is
  `PROVENANCE.md §37.4`; and the oracle case `T-01` provenance finding below, checked before step 3
  is designed rather than assumed.
- **The real capture is committed, `vendored: true`, not merely `upstream_mutable: true`.**
  `manifest/manifest.json`'s own `horizons-acs3-vectors` entry pins a real ACS3 query (`IOHZ-A-008`,
  §8 reads it directly). Verified directly, not assumed, that `upstream_mutable` alone was not
  enough: every live response embeds its own request-processing wall-clock timestamp, so no two
  live fetches of the identical query ever hash-match, even seconds apart — confirmed when a routine
  `fetch.py fetch` mismatched against an earlier `curl` fetch of the same query on its own FIRST
  attempt, not eventually. That means `data/cache/` (gitignored, populated by a live fetch) could
  never hold the pinned bytes on a fresh clone — nothing could re-fetch them — so gate 1 and
  `IOHZ-A-008` would fail for anyone, always, not merely until a deliberate re-pin.
  `manager's own second ruling` (`plan/subplan_L6/L6-2.md`): the bytes are committed as a TRACKED
  file, `data/vendored/horizons-acs3-vectors/`, never `data/cache/`; `fetch`/`fetch --refresh` never
  attempt to re-acquire a vendored entry (proved refusing to, by injection, `tests/test_fetch.py`),
  and `verify` (and `odl_manifest_get` in CMake) check the tracked copy directly — proved where it
  has to work (rule 5): `data/cache/horizons-acs3-vectors/` was deleted entirely and `ci.sh` re-run,
  passing on the tracked copy alone. `upstream_mutable: true` is KEPT alongside `vendored: true` as
  the documented reason vendoring was needed, not redundant with it — and, more slowly, the
  provenance finding immediately below is a second, independent reason the same flag states.
- **`T-01`'s own frozen 2.246 m is not reproducible by a fresh capture, and this round confirmed
  it rather than assumed it.** `oracle/environment.txt` (read; not `capture.sh`) records SHA-256
  hashes for the predecessor's own two `T-01` inputs: `res/teme_check/acs3.tle` and
  `res/teme_check/acs3_horizons_20260914.eci` (the filename's own date, 2026-09-14, already says
  this is a specific historical capture, not a standing fixture). A fresh ACS3 TLE, fetched
  directly this round (`celestrak.org`, NORAD 59588, epoch day 267 of 2026) hashes to
  `16449566a3cdaba5e2f1fc020839280a1d1189583318a13d14bef72645f3bd07`
  (full file) — **not** `fb20103bf60ece4ed7af67c0c8b0085303b717a0d5e2606a762128c33de9d0da`, the
  recorded value. The real Horizons query run for §2 above hashes to
  `d639914a7b82678b741962f61b4f872d42b651116965de21a1a737850511350c` — **not**
  `b9e73e297f9c1dd3fc99d479cdda38f284acc26e37b3903c90cb82293d1d58cb`, the recorded value. Both
  mismatches are expected and conclusive, not merely a formatting difference: `oracle/ORACLE.md`
  §6 itself states that `T-*` cases' own "comparison ephemeris is *not* independent — Horizons'
  ephemeris for that object forward of a TLE epoch **is that TLE**" — meaning Horizons' own output
  for ACS3, an actively-tracked LEO object with no independent high-precision solution beyond its
  own TLE, is only as current as whichever TLE Horizons itself last ingested, and a TLE for an
  active LEO object is reissued every one to a few days as new tracking data arrives. **The
  implication for step 3, stated now so it is not designed around a false assumption**: `T-01`'s
  own required-disagreement gate (`../plan/PLAN.md` §4 rule 1, "asserting size *and* direction... 
  at the ≈2.2 m the precession difference predicts") tests a property of the TEME↔J2000 frame
  conversion itself — the accumulated IAU-76-vs-IAU-2006 precession difference, a property of
  which two MODELS are used, not of which specific TLE — so the ~2.2 m size-and-direction assertion
  can still be built and tested on freshly-captured data. The literal frozen figure, 2.246 m, is
  specific to the predecessor's own 2026-09-14 capture and must not be asserted as the expected
  value for a new one.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `IOHZ-Q-001` | The header's own remaining metadata lines (`EOP file`/`EOP coverage`, `Calendar mode`, `Center geodetic`/`cylindric`/`radii`) are read opaque (§3.2); no current consumer needs them. Worth field-by-field treatment if a future caller needs, e.g., the EOP coverage bound Horizons itself states its own output is conditioned on. |
| `IOHZ-Q-002` | **Licence basis for a committed Horizons response — RESOLVED** (manager, 2026-09-25, `plan/subplan_L6/L6-2.md`). Ruled: computed positions are factual data, not an expression (RS14's own reasoning) — a new manifest licence basis, `FACTUAL-DATA-CITED`, is committed for exactly this shape, gated on a `search_recorded` pointer `fetch.py check-licences` enforces by injection (`tests/test_fetch.py`), so the basis cannot become a way round the gate. The real ACS3 capture is now pinned (`manifest/manifest.json`, id `horizons-acs3-vectors`, `upstream_mutable: true`) and read directly by `IOHZ-A-008`, §8. |
