# PROVENANCE — odl/self_built

The ledger required by rule **R5** of `doc/REWRITE_PLAN.md`: every module traced to the
published sources it implements, every constant traced to the document it came from, every
dependency traced to its licence, and every comparison against the predecessor logged.

It is also the due-diligence pack. A reader who wants to know whether this tree is the
owner's to license should be able to answer that from this file alone.

**State of the tree at this revision:** the P1 specifications exist and were **adopted by the
manager on 2026-09-18**; **no implementation code has been written**. Rows below whose "status"
reads *specified* record what a module will implement when it is built, not what it does today.

| | |
|---|---|
| **Seeded** | 2026-09-18, from `doc/REWRITE_PLAN.md` §8 |
| **Covers** | rewrite phase P1 — `time`, `frames`, `eop` (feature F3 and the time layer under it) |
| **Name** | **`odl/self_built`** — decision D6, taken by the owner 2026-09-18. It overrides the plan's original "no echo of ODL/SGNL" guidance deliberately; the concern that guidance protected against is recorded in `LICENSE` §4 rather than dropped. |
| **Licence** | **Decision D5 taken 2026-09-18: no grant, for now.** `LICENSE` is a bare copyright notice. It matches the predecessor's posture and explicitly **does not** copy its reason: the predecessor cannot grant a licence because nobody has established who may; this tree can and has chosen not to yet. Repeating the predecessor's reason would have asserted UCL origin for work written from the IERS Conventions. |
| **Repository** | `rewrite/` inside `/home/rog/odl20Lite`. First commit `bdd80be` (2026-09-18) — licence, plan, P1 specifications, provenance, frozen oracle. Plan rule R8 satisfied: the licence was present at commit one. |
| **Outstanding** | `LICENSE`'s copyright line is a **placeholder**: `Copyright (c) 2026 <OWNER — …>`. A due-diligence reader will look there first. See §9. |

---

## 0. Clean-room log (rules R1, R2)

The discipline is worth nothing unless it is recorded at the time, so it is recorded here
rather than asserted later.

### 0.1 Derivation declarations

| date | artefact | declaration | by |
|---|---|---|---|
| 2026-09-18 | `spec/SPEC-template.md`, `spec/SPEC-time.md`, `spec/SPEC-frames.md`, `spec/SPEC-eop.md` | Written from the sources in §4 below and from no implementation of the modules concerned. **No file under `/home/rog/odl20lite` was opened, read, listed, searched or otherwise inspected** during their preparation — not the source, not `res/`, not `analysis/`, and not `REVIVAL.md` or `PROVENANCE.md`. | executor session `odl Executor`, under handover `~/.claude/handover/2026-09-18-odl-rewrite-r1-p1-specs.md` |


### 0.2 Disclosed context exposure


"Did not read it" and "nothing about it reached me" are different claims, and only the first
is within an author's control. This register records the second, so that the declarations in
§0.1 can be checked rather than merely believed. Nothing listed here is expression, and
nothing listed here was used to derive any requirement.

| date | what reached the author's context | route | unbidden? | used for anything? |
|---|---|---|---|---|
| 2026-09-18 | A git-status snapshot of `/home/rog/odl20lite`: the branch name, the names of three untracked files, and the subject lines of the five most recent commits. | The session-start banner of a Claude Code session whose working directory is that tree. | Yes — it arrived before the handover did and was not sought. | No. The P1 tranche would be identical without it. |
| 2026-09-18 | Numeric acceptance targets from earlier measurement campaigns (round-trip 1.29 × 10⁻⁷ km; TEME residual 2.2 m / 3.6 m; the ≈ 0.064 arcsec attribution; the 69 s / ≈ 500 km timescale trap). | The handover document, §4, which extracted them deliberately so that the executor need not read the predecessor. | No — supplied on purpose. | Yes, as **behavioural observations against public data** (plan R4). They are measurements, not expression. Logged in §6. |
| 2026-09-18 | Two filenames the predecessor fetched: `EOP_14_C04_IAU1980` and `finals.all.iau1980`. | The manager's verdict message, reporting what it found when it verified correction §2.2 of the executor's report. | No — supplied in the verdict. | **No.** It arrived *after* the specifications were written. `SPEC-eop.md` §3.2's requirement was derived from the IERS format documents `readme.finals` and `readme.finals2000A`, independently, and was then corroborated by this. The order matters and is recorded for that reason. |

**This register exists because of a rule that came out of it** (plan **R2 corollary**, adopted
2026-09-18, §8.8): whoever has read the predecessor is the leak channel, and a handover or a
verdict can carry predecessor detail into a clean session without anyone reading anything.
Outbound documents are therefore audited for predecessor detail as carefully as inbound reading
is refused. The third row above is the case that produced the rule.

**Honest limit, recorded once and not repeated** (carried from plan R2): the owner, and
assistants working for the owner in earlier sessions, have previously read the predecessor's
source. The discipline above reduces copying risk; it does not eliminate it, which is exactly
why R1 (spec first, from papers) and R3 (a deliberately different architecture) exist
alongside it.

### 0.3 Adoption

| date | artefact | verdict | by |
|---|---|---|---|
| 2026-09-18 | `spec/SPEC-time.md`, `spec/SPEC-frames.md`, `spec/SPEC-eop.md` (v1.1) | **Adopted.** All six of the executor's corrections accepted; the three that change what gets built were independently verified at source before acceptance. All six design decisions adopted unchanged. Four open questions decided (§9). Two new plan rules created, **R11** and **R12** (§8.6, §8.7). | manager session `odl maintainer (Router+Executor)` |
| 2026-09-18 | `spec/SPEC-template.md` (v1.1) | Adopted for the P1 tranche; amended to carry plan rule R12 as `R-ERR-3`. | same |

**Defect found and corrected after adoption, v1.2 of the three module specs.** The adopted
v1.0/v1.1 documents asserted the template's coverage rule — every requirement and refusal
discharged by an acceptance test, with the uncovered ones listed — without meeting it: 51 of
121 requirements and refusals had no test, and the closing "not covered" notes named only a
handful. v1.2 adds twenty acceptance rows and a complete §8 *Coverage* table in each spec.
**No requirement was added, removed or changed**, so the requirement set the manager adopted
stands unaltered; only the evidence for it does. Recorded here rather than folded silently into
v1.1, because a spec that claims a discipline it does not keep is the specific failure this
tree's template exists to prevent.

---

## 1. Module register

| module | status | implements | spec | sources (keys as in §4) |
|---|---|---|---|---|
| `time` | **specified and adopted**, not implemented | TAI, TT, TCG, TDB, TCB, UTC, GPS; leap seconds; epoch representation; pre-1972 policy | `spec/SPEC-time.md` v1.2 | `TN36-1`, `TN36-10`, `LEAP`, `ERFA`, `CGPM27-4` |
| `frames` | **specified and adopted**, not implemented | GCRS ↔ ITRS via IAU 2006 precession / IAU 2000A nutation, CIO-based; TEME ↔ ITRS/GCRS; polar motion; RTN; DYB | `spec/SPEC-frames.md` v1.2 | `TN36-5`, `TN36-1`, `ERFA`, `VAL06`, (`BEU94`, `ARN15` — see §4 gaps) |
| `eop` | **specified and adopted**, not implemented | IERS EOP 20 C04 and `finals2000A` parsing; splicing; 4-point Lagrange interpolation; ocean-tide and libration restoration; coverage policy | `spec/SPEC-eop.md` v1.2 | `TN36-5` §§5.5.1/5.5.3, `TN36-8`, `C04`, `FINALS`, `INTERP`, `ORTHO`, `PMSD`, `UTLIBR` |

**Model declaration, carried in every run's output** (`SPEC-frames.md` `FRAME-R-002`):
precession **IAU 2006**, nutation **IAU 2000A**, origin **CIO-based**. This is a deliberate
upgrade from the predecessor's IAU-76/80 and changes numerical results by about 0.06–0.08
arcsec at present epochs — roughly 2.2 m at 7000 km. Any comparison against a pre-upgrade
baseline must record both model versions.

---

## 2. Parameter register

Every constant appearing in the P1 specifications, with the document it was taken from.

### 2.1 Time

| value | symbol | used by | source document | where |
|---|---|---|---|---|
| 32.184 s exactly | TT − TAI | `time` | IERS Conventions (2010), TN 36 ch. 10 | §10.1 |
| 6.969 290 134 × 10⁻¹⁰ | *L*_G (defining) | `time` | IERS Conventions (2010), TN 36 ch. 1 | Table 1.1 |
| 1.550 519 768 × 10⁻⁸ | *L*_B (defining) | `time` | IERS Conventions (2010), TN 36 ch. 1 | Table 1.1 |
| −6.55 × 10⁻⁵ s | TDB₀ (defining) | `time` | IERS Conventions (2010), TN 36 ch. 10 | eq. (10.3); IAU 2006 Res. B3 |
| JD 2443144.500 3725 | *T*₀ | `time` | IERS Conventions (2010), TN 36 ch. 10 | §10.1 |
| 19 s exactly | TAI − GPS | `time` | derived from `LEAP` (ΔAT = 19 s at MJD 44239.0 = 1980-01-01) plus the GPS epoch 1980-01-06T00:00:00 UTC. Definitive source IS-GPS-200 **not obtained** — see §4 and `TIME-Q-002`. | §4.4 of the spec |
| 28 rows, ΔAT 10 s (1972-01-01) → 37 s (2017-01-01) | ΔAT table | `time` | IERS `Leap_Second.dat`, updated through Bulletin C 72, July 2026; **expires 2027-06-28** | whole file |
| 1958-01-01T00:00:00 TAI | representation origin | `time` | this tree's choice, `TIME-Q-005` | — |
| 1972-01-01T00:00:00 UTC | pre-1972 refusal boundary | `time` | this tree's choice, `TIME-Q-001`, supported by `LEAP` (first integral entry) and ERFA `src/dat.c` note 1 | — |

### 2.2 Frames

| value | symbol | used by | source document | where |
|---|---|---|---|---|
| 0.779 057 273 264 0; 1.002 737 811 911 354 48 | ERA polynomial | `frames` | IERS Conventions (2010), TN 36 ch. 5 | eq. (5.14), (5.15) |
| 7.292 115 146 706 979 × 10⁻⁵ rad s⁻¹ | ω_E (ERA rate) | `frames` | derived: 2π × 1.002 737 811 911 354 48 / 86400, from TN 36 ch. 5 eq. (5.14) | §4.2 of the spec |
| 7.292 115 × 10⁻⁵ rad s⁻¹ | ω, nominal mean | (reference value only) | IERS Conventions (2010), TN 36 ch. 1 | Table 1.1 |
| *s′* = −47 µas · *t* | TIO locator | `frames` | IERS Conventions (2010), TN 36 ch. 5 | eq. (5.13) |
| *X*₀ = −0.016 617″, *Y*₀ = −0.006 951″ | CIP series constants (frame bias in the pole) | `frames` | IERS Conventions (2010), TN 36 ch. 5 | eq. (5.16) |
| IAU 2006/2000A *X*, *Y*, *s* series (full coefficient sets) | — | `frames` | IERS Conventions (2010), TN 36 ch. 5, Tables 5.2a–5.2d — **evaluated via ERFA**, not transcribed | §5.5.4, §5.5.6 |
| 67 310.548 41 s; 876 600 h + 8 640 184.812 866 s; 0.093 104; −6.2 × 10⁻⁶ | GMST-1982 polynomial | `frames` (TEME only) | Vallado, Crawford, Hujsak & Kelso, AIAA 2006-6753 | eq. (2) |

### 2.3 Earth orientation

| value | used by | source document | where |
|---|---|---|---|
| 71 diurnal + semidiurnal ocean-tide constituents, pole coordinates | `eop` | IERS Conventions (2010), TN 36 ch. 8 | Tables 8.2a, 8.2b |
| 71 diurnal + semidiurnal ocean-tide constituents, UT1 and LOD | `eop` | IERS Conventions (2010), TN 36 ch. 8 | Tables 8.3a, 8.3b |
| 10 near-diurnal libration terms, pole coordinates | `eop` | IERS Conventions (2010), TN 36 ch. 5 | Table 5.1a (the near-1-day rows only; see `EOP-R-044`) |
| 11 semi-diurnal libration terms, UT1 and LOD | `eop` | IERS Conventions (2010), TN 36 ch. 5 | Table 5.1b |
| 4-point Lagrange interpolation window | `eop` | IERS `interp.f` (`LAGINT`) — **algorithm read as documentation, code not used** | routine header |

**Transcription rule (`EOP-R-008`).** Every table above is transcribed from the Conventions
*document*, never from the Fortran, and every transcription is verified against the published
test case recorded in §5. A transcription that fails its test case is a build failure.

---

## 3. Dependency register (rule R7)

| dependency | version | licence | multi-licensed? | chosen option | notes |
|---|---|---|---|---|---|
| **ERFA** (Essential Routines for Fundamental Astronomy) | **v2.0.1**, released 2023-10-13, tracking SOFA "20231011" | **BSD 3-clause** | no | — | Mandated by plan R7. Chosen over SOFA specifically to avoid SOFA's rename clause; ERFA is derived from SOFA with the SOFA board's permission, recorded in its `LICENSE`. |

Routines this tree calls, and does not call:

- **Called:** `eraDtdb`; `eraCal2jd`, `eraJd2cal`; `eraXy06`, `eraS06`, `eraC2ixys`,
  `eraEra00`, `eraSp00`, `eraPom00`, `eraC2tcio`; `eraGmst82`.
- **Deliberately not called, with reasons recorded in the specs:**
  - `eraDat` — its leap-second table is compiled in, is stale by construction, and returns a
    *usable-looking number alongside a warning* for years ≥ release + 5, i.e. from 2028
    (`TIME-R-053`). UTC ↔ TAI is implemented in this tree from the IERS file instead.
  - `eraSetLeapSeconds` / `eraGetLeapSeconds` (`erfaextra.h`) — process-global mutable state,
    which conflicts with the requirement that state not leak between runs (`TIME-R-053`).
  - `eraC2t06a` — cannot accept δ*X*, δ*Y*, so it cannot implement `FRAME-R-012`. Used in the
    test suite only, as an independent check of the composed chain (`FRAME-R-017`).
- **Absent from ERFA, and therefore this tree's own code:** GPS ↔ TAI. ERFA v2.0.1's `erfa.h`
  declares no GPS routines (`TIME-R-024`).

No GPL, LGPL or AGPL component appears anywhere in the P1 design. The IERS Conventions Fortran
routines carry **no licence statement** and are therefore read as documentation only, never
translated or vendored (`EOP-R-007`).

---

## 4. Source register

Every document the P1 specifications rest on, with whether it was actually obtained. This
table is the answer to "is the derivation real?" and it is deliberately blunt about its gaps.

| key | document | obtained | retrieved | locator |
|---|---|---|---|---|
| `TN36-1` | IERS Conventions (2010) TN 36 ch. 1 — numerical standards | **primary** | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter1/icc1.pdf` |
| `TN36-5` | IERS Conventions (2010) TN 36 ch. 5 — ITRS ↔ GCRS | **primary** | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter5/icc5.pdf` |
| `TN36-8` | IERS Conventions (2010) TN 36 ch. 8 — tidal variations in Earth rotation | **primary** | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter8/icc8.pdf` |
| `TN36-10` | IERS Conventions (2010) TN 36 ch. 10 — relativistic models, time coordinates | **primary** | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter10/tn36_c10.pdf` |
| `LEAP` | IERS `Leap_Second.dat`, through Bulletin C 72 (July 2026) | **primary** | 2026-09-18 | `hpiers.obspm.fr/iers/bul/bulc/Leap_Second.dat` |
| `C04` | IERS EOP 20 C04 data file and its in-file header | **primary** | 2026-09-18 | `hpiers.obspm.fr/iers/eop/eopc04/eopc04.1962-now` |
| `C04-README` | `readme` of the C04 20 series | **primary** | 2026-09-18 | `hpiers.obspm.fr/iers/eop/eopc04_20_v2/readme` |
| `C04-UPD` | `updateC04.txt` — C04 change log | **primary** | 2026-09-18 | `hpiers.obspm.fr/iers/eop/eopc04/updateC04.txt` |
| `FINALS` | `readme.finals2000A` — format description | **primary** | 2026-09-18 | `maia.usno.navy.mil/ser7/readme.finals2000A` |
| `FINALS-80` | `readme.finals` — format description (cited to exclude it) | **primary** | 2026-09-18 | `maia.usno.navy.mil/ser7/readme.finals` |
| `INTERP` | `interp.f` — EOP interpolation reference routine | **primary** (read as documentation) | 2026-09-18 | `hpiers.obspm.fr/iers/models/interp.f` |
| `ORTHO` | `ORTHO_EOP.F` — ocean-tidal EOP variations | **primary** (test case only) | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter8/software/ORTHO_EOP.F` |
| `PMSD` | `PMSDNUT2.F` — diurnal pole libration | **primary** (test case only) | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter5/software/PMSDNUT2.F` |
| `UTLIBR` | `UTLIBR.F` — semi-diurnal UT1/LOD libration | **primary** (test case only) | 2026-09-18 | `iers-conventions.obspm.fr/content/chapter5/software/UTLIBR.F` |
| `ERFA` | ERFA source and in-source documentation, v2.0.1 | **primary** | 2026-09-18 | `github.com/liberfa/erfa` |
| `VAL06` | Vallado, Crawford, Hujsak & Kelso, *Revisiting Spacetrack Report #3*, AIAA 2006-6753 Rev 2 | **primary** | 2026-09-18 | `celestrak.org/publications/AIAA/2006-6753/`; DOI `10.2514/6.2006-6753` |
| `CGPM27-4` | CGPM (2022) Resolution 4 — future of UTC and leap seconds | **secondary** — consistent secondary reporting only; the resolution text was not retrieved | 2026-09-18 | `bipm.org/en/cgpm-2022/resolution-4` |
| `ARN15` | Arnold, Meindl, Beutler *et al.*, *CODE's new solar radiation pressure model for GNSS orbit determination*, J. Geodesy 89: 775–791 | **secondary** — abstract only; paywalled at Springer | 2026-09-18 | DOI `10.1007/s00190-015-0814-4` |
| `BEU94` | Beutler *et al.*, *Extended orbit modelling techniques at the CODE processing center …*, Manuscripta Geodaetica 19: 367–386 | **not obtained** — no accessible archive found | 2026-09-18 | — |
| `BERN52` | Dach, Lutz, Walser & Fridez (eds.), *Bernese GNSS Software Version 5.2* documentation | **not obtained** — `ftp.aiub.unibe.ch` refused the connection on both HTTP and HTTPS | 2026-09-18 | DOI `10.7892/boris.72297` |
| `ISGPS200` | IS-GPS-200, GPS Space Segment / Navigation User Interfaces | **not obtained** | 2026-09-18 | `gps.gov/technical/icwg/` |
| `RAY94` | Ray, Steinberg, Chao & Cartwright, Science 264: 830–832 (1994) | **not obtained** — its coefficients are reproduced in `TN36-8`, which was obtained | 2026-09-18 | DOI `10.1126/science.264.5160.830` |

**What rests on a gap, and how much.** Only one requirement in the P1 specs depends on a
source that was not obtained in primary form: the DYB frame definition of
`SPEC-frames.md` §4.7, whose origin is `BEU94`/`ARN15`. The definition there is stated from
first principles and is internally unambiguous, but the **sign of ê_D** and the
**construction of ê_Y** are conventions fixed by this spec rather than inherited from the
source. See `FRAME-Q-002`. Everything else — every equation, every constant, every format —
traces to a document held in primary form.

---

## 5. Published test values adopted as acceptance criteria (rule R4)

Behavioural observations from published sources. None of these is derived from the
predecessor; they are the strongest class of acceptance value per `SPEC-template.md` §8.

| source | input | published expected output |
|---|---|---|
| `ORTHO` (`ORTHO_EOP.F` header) | MJD 47100 | Δ*x* = −162.838 637 327 963 653 0 µas; Δ*y* = 117.790 752 584 266 897 4 µas; ΔUT1 = −23.390 923 706 098 082 14 µs |
| `PMSD` (`PMSDNUT2.F` header) | MJD 54335 | Δ*x* = 24.831 442 382 733 648 34 µas; Δ*y* = −14.092 406 920 418 376 61 µas |
| `UTLIBR` (`UTLIBR.F` header) | MJD 44239.1 | ΔUT1 = 2.441 143 834 386 761 746 µs; ΔLOD = −14.789 712 473 494 494 92 µs/day |
| `UTLIBR` (`UTLIBR.F` header) | MJD 55227.4 | ΔUT1 = −2.655 705 844 335 680 244 µs; ΔLOD = 27.394 458 265 998 469 67 µs/day |
| `VAL06` Appendix C | 2004-04-06T07:51:28.386 UTC; ΔUT1 = −0.439 961 s; ΔAT = 32 s; *x*_p = −0.140 682″; *y*_p = 0.333 309″; `r_ITRF` = (−1033.479 383 00, 7901.295 275 40, 6380.356 595 80) km; `v_ITRF` = (−3.225 636 520, −2.872 451 450, 5.531 924 446) km/s | `r_TEME` = (5094.180 107 20, 6127.644 705 20, 6380.344 532 70) km; `v_TEME` = (−4.746 131 494, 0.785 817 998, 5.531 931 288) km/s |
| `VAL06` Appendix C | TLE `00005`, day 182.784 950 62, TEME → J2000 (IAU-76/FK5) | `r_J2000` = (−9059.941 554 1, 4659.697 199 0, 813.956 940 2) km — **expected to differ from this tree's IAU 2006/2000A result by ≈ 3 m; see `FRAME-A-009`** |
| `VAL06` Appendix C | "of date" vs "of epoch" over 3 days, same example | 23.6 m |
| `C04` | first row of the series | 1962-01-01, MJD 37665.00, *x* = −0.012 700″, *y* = 0.213 000″, UT1−UTC = 0.032 633 8 s |
| `ERFA` test suite (`t_erfa_c.c`) | per-routine | ERFA's own published expected values for `eraXy06`, `eraS06`, `eraC2ixys`, `eraEra00`, `eraSp00`, `eraPom00`, `eraC2tcio`, `eraGmst82`, `eraDtdb` |

---

## 6. Oracle log (rule R4)

Comparisons of this tree's output against the predecessor's. Behavioural observation is not
copying; it is logged so that it is visibly not more than that.

| date | quantity | oracle value | this tree | verdict |
|---|---|---|---|---|
| — | — | — | — | *no comparison has been run; no implementation exists in this tree yet* |

**The oracle is frozen and captured** (manager, 2026-09-18): `oracle/` holds 24 measurements
with every input hashed and `capture.sh` reproducing them, and `ORACLE.md` governs their use.
Two points from it bear directly on the specifications above:

- **`ORACLE.md` §1 — running it is permitted to an executor; reading it is not.** A case that
  is missing is added to `capture.sh`, not obtained by opening the predecessor. The intent is
  that the predecessor never needs opening again: an oracle that has already answered every
  question cannot tempt anyone into looking inside. This is plan R2 discharged by the session
  that had read the tree, which is the only session that could — R1 required an author who had
  not, and R2 requires an operator, and the two rules partition the work by who has read what.
- **`ORACLE.md` §4 — some disagreements with this tree are *required*.** The predecessor
  computes with IAU-76/1980 and consumes the IAU 1980 product variants, so on those quantities
  agreement would be the failure, not the success. `SPEC-frames.md` `FRAME-A-009` is the shape
  for all of them: assert a **disagreement of a predicted size and direction**, and fail on
  agreement as well as on excess.

**Prior observations carried in from the plan and the handover**, recorded here so that they
are not mistaken for targets measured by this tree:

| quantity | predecessor's measured value | model under which it was measured | status as a target for this tree |
|---|---|---|---|
| ITRF ↔ GCRF round trip, LEO state | 1.29 × 10⁻⁷ km | IAU-76/80 | informative. This tree's gate is < 1 × 10⁻⁶ km, and the test is self-consistency, which proves nothing about accuracy (`FRAME-P-1`). |
| SGP4 TEME vs JPL Horizons, 5 h arc | 2.2 m mean / 3.6 m max | IAU-76/80 | informative. The gate is < 20 m. The plan's expectation of "< 1 m post-IAU2006" is **questioned** — see `FRAME-Q-001`. |
| attributed IAU-76/80 vs IAU-2006/2000A difference | ≈ 0.064 arcsec | — | **corroborated independently**: the IAU-76 precession rate error accumulates at ≈ 3 mas yr⁻¹, giving 0.06–0.08 arcsec by the mid-2020s, i.e. ≈ 2.2 m at 7000 km — which is the whole of the observed residual. |

**Determination about a third-party product, from public data** (manager, 2026-09-18), recorded
because it changes what one of the acceptance tests means:

| finding | evidence | consequence |
|---|---|---|
| The JPL Horizons ephemeris for the TEME comparison object, forward of a TLE epoch, **is that TLE**. | Kernel coverage ends at the TLE epoch + 15.000 days, matching to the millisecond; it tracks an independently-run SGP4 to 2–3 m flat, with no growth over five days. | `FRAME-A-017` measures the difference between two TEME→inertial **conventions**, not orbit accuracy. The 2–3 m observed is `FRAME-P-5`'s ≈ 3 m definitional floor, measured. "Expect < 1 m" struck from the plan; `FRAME-A-001` promoted to primary frames gate. |

---

## 7. Data manifest register (rule R6)

Every external data file, re-fetched from its origin. Nothing is carried over from the
predecessor's tree.

| file | origin | licence / terms | mutability | pinning |
|---|---|---|---|---|
| `Leap_Second.dat` | IERS Earth Orientation Centre, `hpiers.obspm.fr/iers/bul/bulc/` | IERS public product | appended when a leap second is announced; **carries its own expiry date** (currently 2027-06-28) | URL + SHA-256 + expiry; expiry enforced as a refusal (`TIME-R-051`) |
| `eopc04.1962-now` (EOP 20 C04) | IERS EOP Product Centre, `hpiers.obspm.fr/iers/eop/eopc04/` | IERS public product | **mutable at a fixed URL, and revised retroactively** — see §8.1 | **Plan rule R11**, split by purpose: baselines and acceptance tests pin an **IERS-archived** series (`…/eopc04_20_v2`, `_v3`, …) by path and SHA-256; operational runs may use the live series and must record its identity and hash. Hash mismatch on a pinned file is a hard failure (`EOP-F-010`). The IERS hosts the archives, so this tree redistributes nothing. |
| `finals2000A.all` | IERS Rapid Service / Prediction Centre (USNO), `maia.usno.navy.mil/ser7/` | IERS/USNO public product | rewritten daily | URL + SHA-256 + retrieval timestamp |

---

## 8. Standing notes

**8.1 The EOP products are mutable at a fixed URL and are revised retroactively.** Recorded
because it is the single largest threat to reproducibility in the P1 layer, and because it is
documented by the IERS itself in `updateC04.txt`:

| date | change |
|---|---|
| 2025-06-05 | pole coordinates and rates for 2021-01-01 – 2024-02-24 replaced by ITRF2020-u2023 values. Series name and path unchanged. Previous solution archived at `eopc04_20_v2`. |
| 2026-02-05 | pole coordinates and rates for 2021-01-01 – 2024-12-31 replaced by ITRF2020-u2024 values. Series name and path unchanged. Previous solution archived at `eopc04_20_v3`. |
| 2026-03-09 | pole rates for 1984 recomputed after an error was reported. |

Consequence: a fit re-run months later against a re-fetched file uses **different EOP for the
same historical epochs**, with nothing in the filename, path or header to say so. Every frozen
baseline must record the EOP hashes it was produced with (`EOP-R-004`).

**8.2 The C04 product contradicts itself about its own precession-nutation model.** The data
file's header says `Reference Precession-Nutation Model: IAU 2000`; the series `readme`
describes the same file's offsets as `dX, dY IAU 2006/2000A`. Recorded as-found and not
reconciled in code (`EOP-R-023`, `EOP-Q-002`).

**8.3 The IERS reference routine `interp.f` omits a correction the Conventions require.** It
applies ocean-tide corrections to *x*, *y*, UT1 and LOD and the diurnal libration to *x*, *y*,
but makes no call to `UTLIBR` — while TN 36 §5.5.3.1 says the semi-diurnal libration terms in
UT1 and LOD "should be included in that routine". This tree includes them (`EOP-R-043`), which
is a deliberate ≈ 1 mm deviation from the reference implementation.

**8.4 `finals.all` is the wrong product for this tree**; `finals2000A.all` is the right one.
The two have identical layouts and differ only in whether the celestial-pole-offset columns
carry dψ/dε (IAU 1980) or d*X*/d*Y* (IAU 2000A). Nothing detects the substitution at parse
time (`EOP-R-002`).

**8.5 ERFA's leap-second table is compiled in and stale by construction.** ERFA v2.0.1 dates
from 2023-10-13; its `eraDat` flags years from 2028 onwards as "dubious" and returns a number
anyway. This tree does not call it (`TIME-R-053`).

**8.6 Plan rule R11 — EOP pinning, split by purpose** (adopted 2026-09-18). Frozen baselines
and acceptance tests pin an **IERS-archived** series by path and SHA-256. Operational runs may
use the live series but must record its identity and hash in run provenance. A hash mismatch on
a pinned file is a **hard failure, never a warning**. Re-baselining is a deliberate, logged act
and is never a side effect of a re-fetch. Specified at `EOP-R-005`, `EOP-R-006`, `EOP-R-009`.
The rule is cheap because the IERS archives the superseded series itself at stable paths, named
in `updateC04.txt` as each is retired — pinning records a URL rather than hosting a snapshot.

**8.7 Plan rule R12 — one named, logged override per refusal** (adopted 2026-09-18). A refusal
in this tree may have at most **one** override; it must be named unmistakably, set explicitly
per run, and recorded in the run's provenance together with what it overrode. No default, no
environment variable, no configuration fall-through. Generalised from `TIME-R-052`
(`assume_no_further_leap_seconds`, the single logged way past the leap-table expiry) and
carried into every spec by `SPEC-template.md` `R-ERR-3`. Its purpose is that "refuse rather than
approximate" must not make the tool unusable for the legitimate case: the way to have both is to
make the exception expensive to take and impossible to take by accident.

**8.8 Plan rule R2, corollary — the leak channel is outbound, not only inbound** (adopted
2026-09-18). Clean-room discipline that only forbids *reading* the predecessor is incomplete:
whoever has read it can carry detail out of it in a handover, a nudge or a verdict, and the
receiving session acquires that detail without having read anything. So outbound documents are
audited for predecessor detail as carefully as inbound reading is refused, and where detail is
carried deliberately — as the handover's §4 acceptance numbers were — it is declared, with the
sequence relative to the work it might have influenced. The distinction the rule turns on:
*"did not read it"* and *"nothing about it reached me"* are different claims, and only the first
is within an author's control. §0.2 is the register that makes the second checkable.

**8.9 The plan moved into this tree on 2026-09-18, and is no longer duplicated.**
`doc/REWRITE_PLAN.md` here is now the **canonical plan and the only copy**; it was removed from
`/home/rog/odl20lite`. Recorded in the ledger because both reasons are provenance facts rather
than housekeeping:

- The canonical document was living inside the one tree executors are forbidden to read, so
  every update obliged the manager to carry a copy across — **the outbound leak channel of
  §8.8, running on the governing document itself.** One copy, where the people governed by it
  are allowed to look, removes the ferrying rather than detecting its staleness afterwards.
- A plan for reimplementing around unresolved rights had no business sitting in the public
  repository whose rights are unresolved, even untracked.

Verified from this session: the file at `doc/REWRITE_PLAN.md` carries R11, R12, the R2
corollary, and the own-prefix denominator requirement in F15, and its header states its
canonical location and why it moved — so a future reader who finds a second copy knows which
is real. **Not verified from this session, by design:** that the old copy is gone. Confirming
it would mean inspecting `/home/rog/odl20lite`, which §0.1 declares was not done. It is taken
on the manager's report, and the limit is stated rather than glossed, per §8.8.

*(This note supersedes a stale-copy hazard recorded earlier the same day, which the move
resolved structurally instead of by refreshing.)*

**8.10 A statistic cited as an acceptance value must carry its formula.** Two unstated
denominators surfaced within two days of each other, both small enough to read as a rounding
disagreement rather than an error:

| case | the two answers | the difference |
|---|---|---|
| Spec coverage count, 2026-09-18 | 121 against 128 requirements and refusals | own-prefix identifiers against all identifiers appearing in the file, which include cross-references to other specs |
| Blind block recovery, 2026-09-18 | 9.6–13.9 σ against 11.3–20.8 σ, same data, neither side in error | pooled standard deviation (an effect size) against standard error of the means (a *t*-statistic) — a factor of about 1.5 apart. The historical figure was pooled-SD. |

Neither was a mistake in arithmetic; both were two correct answers to two different questions
wearing the same notation. Both were caught by a person looking twice, and neither would have
been caught by a checker, because a checker asserts its denominator rather than noticing it.
Hence the rule, now in `SPEC-template.md` §8: **a statistic cited as an acceptance value carries
the formula it was computed with, not only its number.** The frozen oracle records both block-
recovery figures in `cases.tsv` with their formulas, and the plan's acceptance row names which
one it means.
---

## 9. Checks

| date | check | result |
|---|---|---|
| 2026-09-18 | P1 specifications reviewed and adopted by the manager? | **yes** — all three module specs adopted at v1.1; see §0.3 |
| 2026-09-18 | Context exposure about the predecessor disclosed, not only "not read"? | **yes** — three items registered in §0.2, none used to derive a requirement |
| 2026-09-18 | Every requirement and refusal discharged by an acceptance test, or listed as uncovered with a reason (`SPEC-template.md` §6)? | **yes**, at v1.2 — 105 of 121 tested, all 16 remaining listed explicitly. **No** at v1.0–v1.1; see §0.3 |
| — | Espacenet: "Ziebart" / "University College London" / "radiation pressure" (rule R9) | **pending** — required before phase P7 (ray tracer), not before P1 |
| 2026-09-18 | Every P1 normative source obtained in primary form? | **no** — four gaps, one of which a requirement depends on. See §4. |
| 2026-09-18 | Any GPL / LGPL / AGPL dependency in the P1 design (rule R7)? | **no** |
| 2026-09-18 | Any file copied from the predecessor's tree (rule R6)? | **no** — no file under `/home/rog/odl20lite` was read at all |
| 2026-09-18 | Licence chosen and in the tree at commit one (rule R8, decision D5)? | **yes** — `LICENSE`, a bare copyright notice granting nothing, present at `bdd80be` |
| 2026-09-18 | Does `LICENSE` avoid asserting the predecessor's *reason* for granting nothing? | **yes** — §1 states both postures and why they differ. Copying the predecessor's reason would have asserted UCL origin for work written from the IERS Conventions |
| 2026-09-18 | Name chosen (decision D6)? | **yes** — `odl/self_built`, with the overridden guidance recorded in `LICENSE` §4 |
| **open** | **Is the copyright holder named?** | **NO** — `LICENSE` carries the placeholder `Copyright (c) 2026 <OWNER — …>`. Everything else about title in this ledger is in order, and this one line is not. It is the first thing a counterparty will read and the only edit the file needs. **For the owner.** |

---

## 10. Decisions taken on the P1 tranche

| id | decision | date | recorded at |
|---|---|---|---|
| `EOP-Q-004` | **Resolved** — EOP pinning split by purpose; now plan rule **R11**. | 2026-09-18 | `SPEC-eop.md` `EOP-R-005`, `-R-006`, `-R-009`, `EOP-F-010`, `EOP-A-022`, `EOP-A-023`; §8.6 above |
| `FRAME-Q-001` | **Resolved** — the Horizons comparison ephemeris *is* the TLE, so the test is convention-matching, not accuracy. "< 20 m" retained as the gate; "expect < 1 m" struck; `FRAME-A-001` promoted to primary frames gate; the "assert a disagreement of a predicted size" pattern adopted as standing. | 2026-09-18 | `SPEC-frames.md` §4.5, `FRAME-P-6`, `FRAME-A-001`, `FRAME-A-009`, `FRAME-A-017`; §6 above |
| `FRAME-Q-005` | **Decided** — shape the ITRS type for a realisation tag now, carry the tag before P6. A live hazard: 20 C04 is ITRF2020 where 14 C04 was ITRF2014. | 2026-09-18 | `SPEC-frames.md` `FRAME-R-026` |
| `EOP-Q-002` | **Escalated to the owner** — resolving it means asking the IERS, which no session here may do. No code change needed either way: record as-found. | 2026-09-18 | `SPEC-eop.md` `EOP-R-023`, `EOP-Q-002` |
| `TIME-Q-001` | **Confirmed** — refuse UTC and UT1 before 1972; accept the uniform scales. | 2026-09-18 | `SPEC-time.md` §4.6 |
| `TIME-Q-005` | **Confirmed** — representation origin 1958-01-01 TAI. | 2026-09-18 | `SPEC-time.md` §4.2 |
| `FRAME-Q-003` | **Confirmed** — `frames` takes EOP as a passed-in record; no file access, no cache, no global state. | 2026-09-18 | `SPEC-frames.md` §5 |
| `EOP-Q-001` | **Accepted** — the plan's EOP acceptance row replaced by the four published tidal-routine test cases plus closed-form interpolation properties. | 2026-09-18 | `SPEC-eop.md` §8, §5 above |

**Still open**, carried forward: `TIME-Q-002` (obtain IS-GPS-200), `TIME-Q-003` (the `Site`
argument for TDB, touches `measmod`), `TIME-Q-004` (obtain the CGPM resolution text),
`TIME-Q-006` (re-apportion the microsecond budget when the ephemeris and measurement specs
exist), `FRAME-Q-002` (**obtain Arnold 2015 before P4** — the one requirement resting on a
source gap), `FRAME-Q-004` (the neglected Q̇ velocity term), `EOP-Q-003` (free core nutation),
`EOP-Q-005` (splice default on rapid values), `EOP-Q-006` (pole rates).

---

## Changelog

| date | change |
|---|---|
| 2026-09-18 | D5/D6 recorded; tree moved to `/home/rog/odl-self_built` and committed at `bdd80be`; oracle pointer added to §6; the unstated-denominator rule recorded at §8.10 and added to `SPEC-template.md` §8; the unnamed copyright holder raised as the one open title item. |
| 2026-09-18 | §8.9 closed: the plan moved into this tree as the canonical and only copy, resolving the stale-copy hazard structurally. Verified its contents; its absence from the predecessor tree is taken on report, because verifying it is the thing this session may not do. |
| 2026-09-18 | Manager audit of the v1.2 self-fix passed; denominator of 121 confirmed. Recorded the **R2 corollary** at §8.8 and flagged the stale local plan copy at §8.9. |
| 2026-09-18 | Acceptance coverage completed at spec v1.2 — twenty tests added, complete §8 coverage tables, no requirement changed. |
| 2026-09-18 | Manager verdict recorded: specs adopted at v1.1; §0 split into declarations, **disclosed context exposure** and adoption; plan rules **R11** and **R12** added at §8.6–8.7; the Horizons determination added to the oracle log; decision register added at §10; D5/D6 marked escalated. |
| 2026-09-18 | Seeded from `doc/REWRITE_PLAN.md` §8; populated for the P1 specification tranche. |

| 2026-09-18 | Patent search (R9), brought forward from its P7 gate | **clear** — assignee "University College London" + "solar radiation pressure" = 0 hits; "Ziebart" + radiation pressure = 13 hits, none in orbital mechanics |
