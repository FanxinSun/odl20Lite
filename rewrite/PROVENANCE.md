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


**The clean-room discipline ended on 2026-09-18, deliberately.** The rewrite was merged into
the predecessor's own repository at the owner's instruction — one folder, one repository — and a
wall that held because the two trees were separate cannot hold inside one. The declaration above
stands for the four documents it names: they were written before the merge, and nothing reaches
backwards into how they were derived.

**Nothing written after the merge carries that declaration, and none is claimed.** The L0
foundation work below (§11) was written with the predecessor one `ls` away, and says so. It
costs nothing there — build systems, a manifest fetcher and a licence generator derive from no
one's science — and it is stated because the value of §0.1 is that it is exact about its scope.
It begins to matter at L1.

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
| `core` | **implemented** | `odl::Result` (D7's vehicle) and the small vector/matrix types. A **§2 addition, reported not assumed**: every module links it, and a loose shared header outside any target would be visible to everything by accident — the property `cmake/OdlModule.cmake` exists to prevent. | — (tooling) | D7 |
| `time` | **implemented**, gated | TAI, TT, TCG, TDB, TCB, UTC, GPS; leap seconds; epoch representation; pre-1972 policy | `spec/SPEC-time.md` v1.2 | `TN36-1`, `TN36-10`, `LEAP`, `ERFA`, `CGPM27-4` |
| `ephemerides` | **implemented**, gated | Sun, planets, Moon, Earth, barycentres in the BCRS, from SPK through CALCEPH | `spec/SPEC-ephemerides.md` v1.2 | `PARK21`, `SPK-RR`, `TESTPO`, `CALCEPH` |
| `frames` | **implemented**, gated | GCRS ↔ ITRS via IAU 2006 precession / IAU 2000A nutation, CIO-based; TEME ↔ ITRS/GCRS; polar motion; RTN; DYB | `spec/SPEC-frames.md` v1.2 | `TN36-5`, `TN36-1`, `ERFA`, `VAL06`, (`BEU94`, `ARN15` — see §4 gaps) |
| `eop` | **implemented**, gated | IERS EOP 20 C04 and `finals2000A` parsing; splicing; 4-point Lagrange interpolation; ocean-tide and libration restoration; coverage policy | `spec/SPEC-eop.md` v1.2 | `TN36-5` §§5.5.1/5.5.3, `TN36-8`, `C04`, `FINALS`, `INTERP`, `ORTHO`, `PMSD`, `UTLIBR` |

**Model declaration, carried in every run's output** (`SPEC-frames.md` `FRAME-R-002`):
precession **IAU 2006**, nutation **IAU 2000A**, origin **CIO-based**, with the ERFA version
string; `odl::frames::model_version()` emits it.

**Measured, and it contradicts what this ledger and plan §4 rule 1 assumed — though not for the
reason first recorded here.** The upgrade was expected to move results by 0.06–0.08 arcsec, about
2.2 m, against the predecessor. On the ITRF↔GCRS path it does not: oracle case F-01–F-03
reproduces **to 1.56 mm out of 7717 km**, 4.2 × 10⁻⁵ arcsec.

**Why**, corrected: the two chains are **not** running the same algorithm, and it is not the same
EOP. Each applies the celestial-pole offset series matched to its own model — the predecessor adds
dψ, dε to an IAU-1980 nutation, this tree adds δ*X*, δ*Y* to the IAU 2006/2000A CIP. Those series
exist precisely to bring each model onto the **observed** pole, so two different algorithms each
corrected onto the same physical pole must agree, and the model difference **cancels by
construction**. It was never going to appear on that path, and rule 1 ignored the correction
series. TEME is different because it is referred to the **mean equinox of date**, a model
construct with no correction series, so the difference appears undiluted — which is what oracle
T-01's 2.2 m is. See §12.5. Any comparison against a pre-upgrade
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

**The register is generated, not maintained.** `NOTICE` is a pure function of
`manifest/manifest.json` (`tools/notice.py`), CI fails if it has drifted, and the licence text
of each component is quoted verbatim out of the hash-pinned archive rather than paraphrased.
The table below is the human summary; the manifest governs.

| dependency | version | licence | multi-licensed? | chosen option | notes |
|---|---|---|---|---|---|
| **Catch2** | **3.16.0**, sha256 `0957cae5…af34` | **BSL-1.0** | no | — | Test framework, adopted at L0 step 3. Chosen over doctest (MIT) and GoogleTest (BSD-3) because its matcher vocabulary — `WithinAbs`, `WithinRel`, `WithinULP` — is the vocabulary the adopted L1 specifications already state their tolerances in: "< 1 mm", "relative 1e-6", "bit-comparable". `tests/toolchain_smoke.cpp` tests that correspondence rather than asserting it. BSL-1.0 additionally exempts machine-executable object code from the notice requirement, which is a convenience for a tree whose own posture is undecided. |
| **CMake** | ≥ 3.21 (4.2.3 in use) | BSD-3-Clause | no | — | Build system, host-provided. The floor is 3.21 because the tree reads its manifest with `string(JSON …)` (3.19) and relies on `FetchContent` behaviour settled by 3.21. |
| **Python 3** | ≥ 3.9 (3.13 in use) | PSF-2.0 | no | — | Runs the four build-time tools. Host-provided, linked into nothing. Stdlib only, deliberately: a fetcher that needs a package installed before it can fetch is a bootstrapping regress. |
| **CALCEPH** | **4.0.5**, June 2025, sha256 `3460d8a3…e1ea` | **CeCILL-B** | **YES — triple-licensed** | **CeCILL-B**, the BSD-like option | SPK reading. CALCEPH's own `LICENSE`: *"you have to choose one of the three licenses below to apply on the library."* CeCILL-C is close to the GNU LGPL and CeCILL v2.1 is GPL-compatible, so **CeCILL-B is the only one compatible with plan §5 constraint 3**. Note that neither of the other two contains the string "GPL", so the denylist this project used until 2026-09-18 would have passed both — see §8.12. **Obligation heavier than BSD's, binding a future distribution rather than this tree today:** CeCILL-B §5.3.4 CREDITS requires a distributor of *modified* software to state it is based on CALCEPH, reproduce the IP notice, make it reachable from the interface, and mention it on a freely accessible website for as long as it is distributed. This tree links it unmodified. **The predecessor links it statically and records no choice at all** — the situation this row exists not to inherit. |
| **ERFA** (Essential Routines for Fundamental Astronomy) | **v2.0.1**, released 2023-10-13, tracking SOFA "20231011" | **BSD 3-clause** | no | — | Mandated by plan R7. Chosen over SOFA specifically to avoid SOFA's rename clause; ERFA is derived from SOFA with the SOFA board's permission, recorded in its `LICENSE`. **Not yet acquired** — ERFA enters at L1, which does not open until L0's gate passes. |

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
| 2026-09-18 | **Step 2 accepted; its three conditions discharged.** §14.11: the km/metre crossing named once in `odl/core/units.hpp` with `FRAME-R-062` binding `SPEC-dynamics` to state where it happens (`SPEC-frames` v1.6); archive member hashing generalised to every archive with a `consumes` declaration the fetcher enforces; and the 10⁻²⁸⁰ scale's **bottom** margin measured at 26.0 decades over 74 802 values, none subnormal, with the |P̄′| ≥ |P̄| bound that makes it structural (`SPEC-gravity` v1.3, `GRAV-A-029`). The crossing's own test caught a false exactness claim in its comment on first run. **10 gates, 165 tests.** |
| 2026-09-18 | **L2 step 2 `gravity` implemented and gated.** §14.8–14.10 added: the six specification corrections implementation forced — the factored recursion overflows by 10¹⁵⁰ and then yields NaN, so both representations fail and the 10⁻²⁸⁰ scale with a Horner nest is required; C̄₀₀ = 1; the recursion's achievable accuracy is 6.1 × 10⁻¹¹ at degree 2190 and not 10⁻¹³; `GRAV-A-008` claimed coverage that does not exist; WGS 84's GM cannot be refused by value; and an off-by-one in `truncation_rms` caught only because its expected values came from an independent route. §14.9 records what the gate measured, including the two defects in the gate's own design. `SPEC-gravity` v1.2, `SPEC-frames` v1.5 with `Position<F>`/`Acceleration<F>`. `tools/fetch.py` extracts declared archive members by their own hash, so "`hsynth_WGS84.f` was not used" is checkable. **10 gates, 163 tests, 611 artefacts byte-identical.** |
| 2026-09-18 | **L2 step 2 `gravity` specification drafted; budget-row arithmetic made checkable.** §14 added: the sources obtained and the two that were not, the recursion derived from `TN36-6` (6.2b) and `DLMF-14` and **verified to 60 digits before being written down** because the Conventions print no recursion, the measured underflow of the classical form above 43.7° latitude at degree 2190, what the coefficient file's own structure is, the Table 6.1 conversion that fails and therefore gates nothing, and a claim about the figure-axis terms corrected during drafting. §8.13 records `tools/budgetcheck.py` — including its briefly acquiring the absolute-tolerance defect it was built to catch, found only by replaying the four historical errors. §9 gains two rows. |
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
| Blind block recovery, 2026-09-18 | **14.1 σ against 20.8 σ** for BLOCKIIF vs BLOCKIIR-A, and **9.8 σ against 11.3 σ** for BLOCKIIF vs BLOCKIIIA — same data, neither side in error | pooled standard deviation, `\|m1−m2\| / sqrt(((n1−1)s1²+(n2−1)s2²)/(n1+n2−2))`, an effect size; against standard error of the means, `\|m1−m2\| / sqrt(s1²/n1 + s2²/n2)`, a *t*-statistic. About 1.5× apart. The historical figure was pooled-SD. Both definitions are frozen with their formulas in `oracle/cases.tsv` rows `B-IIF-IIRA-pooled` / `-sem` and `B-IIF-IIIA-pooled` / `-sem`. |

An earlier revision of this note stated the block-recovery case as "9.6–13.9 σ against
11.3–20.8 σ", which pairs a historical *range* against a recomputed *range* whose endpoints come
from different satellite-block pairs. That is not wrong but it is not a like-for-like comparison,
and a note whose whole subject is unstated denominators had no business making one. The table
above now compares one pair at a time, with both formulas written out.

Neither was a mistake in arithmetic; both were two correct answers to two different questions
wearing the same notation. Both were caught by a person looking twice, and neither would have
been caught by a checker, because a checker asserts its denominator rather than noticing it.
Hence the rule, now in `SPEC-template.md` §8: **a statistic cited as an acceptance value carries
the formula it was computed with, not only its number.** The frozen oracle records both block-
recovery figures in `cases.tsv` with their formulas, and the plan's acceptance row names which
one it means.

**8.11 A transitive dependency can capture a pinned one, and did.** `FetchContent` honours the
**first** declaration of a given name and silently ignores every later one. That is the supported
way for a top-level project to pin a transitive dependency — and it is equally the way a
transitive dependency captures ours.

`tl::expected`'s own `CMakeLists.txt` contains

```cmake
FetchContent_Declare(Catch2 URL https://github.com/catchorg/Catch2/archive/v2.13.10.zip)
```

by URL, with **no hash**, naming a different major version of a dependency this tree pins. Before
the declaration order was fixed, adding `tl::expected` caused the build to fetch **Catch2 v2.13.10
over the network** while printing that it was using the pinned 3.16.0 from the cache. Everything
the manifest exists to guarantee was defeated by the second dependency ever added to it.

It surfaced only because Catch2 v2 puts its CMake helpers in `contrib/` and v3 in `extras/`, so an
`include()` failed. **A substitution within a major version would have built cleanly and the pin
would have been a fiction** — the plausible-wrong-number failure mode, arriving through the build
system rather than the physics.

Three changes, and the third is the one that matters:

1. `odl_declare_all_code()` declares every manifest pin **before anything is made available**, so
   this tree's declarations are always first.
2. A dependency's own test build is turned off, since that is what drags in its test
   dependencies.
3. **`tools/fetch.py verify-populated` compares a file inside the populated tree against the same
   file inside the hash-pinned archive**, at configure time, for every code dependency. Nothing is
   inferred from a version string; the comparison is against the bytes. Items 1 and 2 are
   preventive and could be got wrong again; item 3 is detective and fails loudly.

A first attempt at item 3 compared `${<Project>_VERSION}` against the manifest. It does not work —
`project()` sets that in the subdirectory scope `FetchContent` adds, invisible to the caller — and
it failed loudly rather than passing vacuously, which is the only reason the flaw was caught. A
verification that cannot see what it is verifying is worse than none, because it reports success.

**8.12 A denylist of forbidden licences can never be complete; an allowlist of permitted ones
can.** `tools/fetch.py check-licences` refused anything whose name contained `GPL`, `LGPL` or
`AGPL`. **CeCILL v2.1 contains none of those, and is GPL-compatible copyleft. Nor does CeCILL-C,
which is close to the LGPL.** Both would have passed while the gate printed "all permissive" — and
CALCEPH is triple-licensed across exactly those two and CeCILL-B. So would `MPL-2.0`, `EPL-2.0`,
`CDDL`, `SSPL` and `OSL`, measured before and after the change.

It is now an allowlist of sixteen identifiers, each carrying a one-line reason, and adding one is
a deliberate act. Negative-tested against eleven refusals plus `CeCILL-B` passing.

**This is the third appearance of one shape in three disguises**: the unstated denominator
(121 against 128), plan §4 rule 1 (true of the TEME path, read as true of all paths), and this —
each a true statement about a smaller thing, read as a statement about a larger one. The gate-7
narrowing of the same day is a fourth: `speccheck.py` printed a claim about the test suite while
reading only the specifications' own traceability.

**8.13 Budget rows were the only class of number in this tree with no test behind them, and
four of twenty-three were wrong** (adopted 2026-09-18, at the manager's proposal). The four
were out by factors of 1000, 1000, 1000 and 10⁶; three were in `SPEC-time.md`, which had been
adopted, and one in `SPEC-ephemerides.md`, which the manager had reviewed. `tests/toolchain_smoke.cpp`
had the same arithmetic right the whole time — code gets tested and prose does not.

`tools/budgetcheck.py` now evaluates the arithmetic of every `… = **result**` expression in
every `-P-` row, with a small units engine over length, time and angle. `SPEC-template.md` §1
row 6 requires the conversion to be *written out in a form the checker can evaluate*, not only
its result. It is gate 8 of `tools/ci.sh` and two ctest cases.

Three things it did on the way in, recorded because each is the point rather than a footnote:

- **It classifies rather than skips.** `checked` / `no-arithmetic` / **UNPARSEABLE, which is a
  build failure**. A checker that silently passed over rows it could not read would report
  success on the rows it understood and say nothing about the rest — the same shape as §8.12's
  fourth disguise, and the manager named it in advance as the thing that would make the tool
  worse than none.
- **It found a spec defect on its first run**: `EOP-P-5`'s conversion factor was written without
  units, so it stated a result that could not be checked.
- **It acquired, briefly, the exact defect it was built to catch.** Its first tolerance counted
  decimal places rather than significant figures, which is an *absolute* tolerance: a row
  stating `8 × 10⁻¹⁶ m` got a tolerance of 0.5 and an error of a factor of a thousand passed.
  Nothing revealed this except replaying the four historical errors against it — three failed as
  they should and the fourth did not. `tests/test_budgetcheck.py` keeps all four, plus four
  malformed rows that must be refused rather than skipped, so the tolerance cannot regress
  quietly.

The row shape is not assumed by column index, because the tables differ between specifications
— `SPEC-frames.md` has four columns where `SPEC-time.md` has five. Finding that out before
writing the parser is why it does not trust one.

---

## 9. Checks

| date | check | result |
|---|---|---|
| 2026-09-18 | P1 specifications reviewed and adopted by the manager? | **yes** — all three module specs adopted at v1.1; see §0.3 |
| 2026-09-18 | Context exposure about the predecessor disclosed, not only "not read"? | **yes** — three items registered in §0.2, none used to derive a requirement |
| 2026-09-18 | Every requirement and refusal discharged by an acceptance test, or listed as uncovered with a reason (`SPEC-template.md` §6)? | **yes**, at v1.2 — 105 of 121 tested, all 16 remaining listed explicitly. **No** at v1.0–v1.1; see §0.3 |
| 2026-09-18 | Espacenet: "Ziebart" / "University College London" / "radiation pressure" (rule R9) | **clear**, brought forward from its P7 gate — assignee "University College London" + "solar radiation pressure": 0 hits; "Ziebart" + radiation pressure: 13 hits, none in orbital mechanics. *(This row was stranded below the changelog as a broken table fragment; moved here, where §9 is the place anyone would look for it.)* |
| 2026-09-18 | Every P1 normative source obtained in primary form? | **no** — four gaps, one of which a requirement depends on. See §4. |
| 2026-09-18 | Any GPL / LGPL / AGPL dependency in the P1 design (rule R7)? | **no** |
| 2026-09-18 | Any file copied from the predecessor's tree (rule R6)? | **no** — no file under `/home/rog/odl20lite` was read at all |
| 2026-09-18 | Licence chosen and in the tree at commit one (rule R8, decision D5)? | **yes** — `LICENSE`, a bare copyright notice granting nothing, present at `bdd80be` |
| 2026-09-18 | Does `LICENSE` avoid asserting the predecessor's *reason* for granting nothing? | **yes** — §1 states both postures and why they differ. Copying the predecessor's reason would have asserted UCL origin for work written from the IERS Conventions |
| 2026-09-18 | Name chosen (decision D6)? | **yes** — `odl/self_built`, with the overridden guidance recorded in `LICENSE` §4 |
| 2026-09-18 | L0 steps 3–7 passed their gates? | **yes** — 8 gates, run offline from the cache in 32 s; see §11 |
| 2026-09-18 | Does any build step reach for the network? | **no** — proved, not assumed: `tools/ci.sh --prove-offline` re-runs every gate with all proxy variables pointed at a closed port |
| 2026-09-18 | Is the build reproducible? | **yes** — 115 artefacts byte-identical across two build directories, and across two *source* trees at paths 27 and 95 characters long |
| 2026-09-18 | Every budget row's arithmetic evaluated by a tool, not by a careful reader? | **yes** — 31 rows across five specifications: 16 carry arithmetic and all 16 evaluate; 15 carry none; **0 unparseable**, which is a build failure and not a skip. See §8.13 |
| 2026-09-18 | `SPEC-gravity.md` drafted from primary sources, and its requirements traceable? | **yes** — 90 own-prefix identifiers, 47 requirements and refusals, 41 discharged by an acceptance row and 6 excused with a reason, 0 uncovered. **Awaiting manager review**; not adopted, not implemented. See §14 |
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

## 11. L0 `foundation` — the toolchain and the data layer

**No derivation declaration attaches to anything in this section.** It was written after the
merge that ended the clean-room separation (§0.1), with the predecessor one `ls` away. That
costs nothing here — a build system, a manifest fetcher and a licence generator derive from
nobody's science — and it is stated because a ledger that quietly let the declaration's scope
creep would be worth less than one that does not.

### 11.1 The toolchain, decided at step 3

| decision | choice | licence | why, and what was rejected |
|---|---|---|---|
| Build system | **CMake ≥ 3.21 + Ninja** | BSD-3 | The obvious answer, and it earns its place for a specific reason: `string(JSON …)` lets CMake read the manifest natively, so the manifest format needs no parser dependency. |
| Dependency acquisition | **the manifest, consumed from a local cache, hash re-checked by CMake** | — | The binding requirement was URL-plus-hash pinning rather than version-range resolution. **vcpkg manifest mode** and **Conan 2** were judged against it and rejected: both *can* be made deterministic (a baseline commit, a lockfile), but in both the hash of the actual source archive is something a registry holds, not something this tree writes down, and re-resolution is their default rather than an error. Both would also need bootstrapping — a tool acquired outside the manifest in order to enforce the manifest. |
| Test framework | **Catch2 v3.16.0** | BSL-1.0 | See §3. doctest and GoogleTest both qualify on licence; the matcher vocabulary decided it. |
| NOTICE generation | **`tools/notice.py`, no dependency at all** | — | D1 removed the `cargo license` route and C++ has no equivalent. It needs none: the manifest already carries a licence and a note per entry because step 3 requires it, so NOTICE is a pure function of the manifest. Better than a scanner, which *infers* licences, where this *reads the declaration the fetcher already enforces*. |

### 11.2 What each step produced

| step | artefact | gate, and how it was shown |
|---|---|---|
| 3 | `manifest/manifest.json`, `manifest/MANIFEST.md`, `tools/fetch.py`, `cmake/OdlManifest.cmake`, `cmake/OdlModule.cmake`, `cmake/OdlWarnings.cmake`, `CMakeLists.txt` | Manifest verifies offline; a corrupted cache is **refused** with both hashes named; 20 fetcher checks including every refusal path; the build configures, builds and tests. |
| 4 | `tools/ci.sh`, `tools/bootstrap.sh`, `.github/workflows/rewrite-ci.yml` | 8 gates green with every proxy pointed at a closed port. Fetching is a separate job; the gates never touch the network. |
| 5 | `tools/notice.py`, `NOTICE` | `notice.py --check` is a gate; licence text is quoted verbatim out of the hash-pinned archive. |
| 6 | `tools/speccheck.py` | Reproduces the hand audit exactly: **121** requirements and refusals, 105 tested, 19 excused, 0 uncovered, 0 dangling. |
| 7 | `cmake/OdlReproducible.cmake`, `tools/reprocheck.py` | 115 artefacts byte-identical across two build paths, and across two source trees 27 and 95 characters long. |

**Exit gate, L0:** *a clean clone builds, tests and regenerates NOTICE with one command, CI
green from cached data alone.* Shown from a genuine clean copy with no cache and no build tree,
at an unrelated path: `tools/bootstrap.sh`, 36 s, 8/8.

### 11.3 The §2 layering, made a link boundary

D1 chose C++20 over Rust, and part of the Rust case was structural distance from a GNU-C++14
predecessor. With the same language and the same problem, **the decomposition is the only thing
left that distinguishes the two trees**, so §2's layering cannot remain a description.

In C++ a directory is not a boundary; a link target is. `cmake/OdlModule.cmake` therefore makes
every §2 module its own target with its own PUBLIC include directory, seeing exactly the modules
it names in `DEPENDS` and nothing else. A module that reaches across the layering fails to
compile. That is what Rust's crate boundaries would have given free and what a flat `src/` +
`include/` tree would have given up, and `tests/boundary/` proves it **both ways** — a declared
dependency compiles, an undeclared one is a compile failure asserted by a `WILL_FAIL` test.

### 11.4 Two things the toolchain now pins that the specifications assumed

`tests/toolchain_smoke.cpp` checks platform properties that the *adopted* specifications already
rest on, so that a toolchain change which would invalidate a spec fails a test instead of going
unnoticed:

- **`double` is IEEE 754 binary64 and there is no excess intermediate precision.** Every
  precision claim in `SPEC-time.md` §4.2 depends on it.
- **The representation arithmetic of `SPEC-time.md` §4.2 holds on this platform.** The spacing
  near JD 2.46 × 10⁶ is 2⁻³¹ d — 40.2 µs, 0.302 m at LEO — and near MJD 6.1 × 10⁴ is 2⁻³⁷ d,
  0.629 µs. Both disqualifications are now measured by a test rather than computed once in prose.

---

## 12. L1 `time-frames` — implemented

No derivation declaration attaches to this section either (§0.1): it was written after the merge
that ended the clean-room separation. The three module specifications it implements were written
before that merge and keep theirs.

### 12.1 What was built

| step | module | gate |
|---|---|---|
| 1 | `core` | `odl::Result` over vendored `tl::expected` (D7). A refusal round-trips; `tools/constraint8.py` enforces plan §5 constraint 8 mechanically and CI runs it. |
| 2 | `time` | Every timescale pair round-trips to under a nanosecond; ΔAT matches all 28 IERS rows; the 2016 leap second renders as `23:59:60` and two SI seconds elapse across the 61-second minute; TN36's published constants *L*_G, *L*_B and TDB₀ reproduce. |
| 3 | `eop` | All four IERS published test cases — `ORTHO_EOP`, `PMSDNUT2`, `UTLIBR` ×2 — inside the disagreement TN36 §8.2 itself documents; both products parse; interpolation reproduces a tabulated node exactly; coverage refuses with the two-day margin. |
| 4 | `frames` | Vallado's published ITRS↔TEME worked example to 13 mm out of 10 208 km; round-trip closure better than the predecessor's; ERFA's own verification suite runs as a CTest case, which discharges `FRAME-A-004` by construction. |

### 12.2 The tidal coefficient tables are generated, and verified

`modules/eop/src/tide_tables.hpp` is **generated** by `tools/tides_from_conventions.py` from the
hash-pinned Conventions PDFs — 178 constituents across Tables 8.2a, 8.2b, 8.3a, 8.3b, 5.1a and
5.1b. Hand transcription of ~1400 numbers would have been its own defect source.

The extraction is **not part of the build**: `pdftotext` output varies with the poppler version,
so a build that re-ran it would not be reproducible. What the build checks is the numbers, against
the routines' own published test cases (EOP-R-008).

Row counts came out exactly as the Conventions state — 41 + 30 ocean constituents in each of pole
and UT1/LOD, and 25 libration rows of which **10 are near-diurnal**, which is precisely the ten
`PMSDNUT2` uses.

| routine | quantity | published | this tree | TN36 §8.2's stated bound |
|---|---|---|---|---|
| `ORTHO_EOP` | Δ*x* | −162.8386 µas | −162.928 | "a few µas" |
| `ORTHO_EOP` | Δ*y* | 117.7908 µas | 118.131 | "a few µas" |
| `ORTHO_EOP` | ΔUT1 | −23.39092 µs | −23.3842 | "a few tenths of a µs" |
| `UTLIBR` | ΔUT1 at MJD 44239.1 | 2.441144 µs | 2.45079 | — |
| `UTLIBR` | ΔLOD at MJD 55227.4 | 27.39446 µs/d | 27.4666 | — |

### 12.3 Errors the tests caught

Recorded because the reason the plan implements L1 before writing further specifications is to
find this class of thing, and a list of them is the evidence that it worked.

| error | size | caught by |
|---|---|---|
| The two-part Julian date was formed as **one double and then split**, reintroducing the 2⁻³¹ d = 40 µs quantisation `SPEC-time` §4.2 exists to disqualify — inside the module built to avoid it | 1.9 × 10⁻⁵ s on TCB − TDB | comparing two routes to the same published quantity |
| `from_calendar` evaluated the rate-dependent offsets once at the naive epoch instead of iterating to a fixed point; the comment justifying it was out by nine orders of magnitude | 1.15 µs against a 1 ns budget | the all-scales round trip |
| `ut1_two_part_jd` added ΔUT1 to **TAI** rather than to UTC | 37 s | the pre-1972 refusal test |
| ω × r was formed in ITRS with ω along its z. The Earth spins about the **CIP**, which is TIRS's z, and polar motion separates them by ≈ 0.3″ | 0.96 mm s⁻¹ | Vallado's published velocity |
| The kinematic equation-of-equinoxes term was omitted from the TEME chain, `FRAME-R-030` v1.2 having forbidden it along with the equinox-based route it does not belong to | 85 mm | Vallado's published position |
| The EOP contiguity check compared against **stored** rows, so skipping any row reported a phantom gap | whole-file refusal | loading the real `finals2000A.all` |
| `FRAME-A-006` evaluated `eraXy06` at J2000 expecting the polynomial's constant term; the series **value** at *t* = 0 is −5.558″, not −0.016617″ | the test, not the code | itself |

### 12.4 The dependency capture

Recorded in full at §8.11. `tl::expected`'s own `CMakeLists.txt` declares Catch2 v2.13.10 by URL
with **no hash**; because `FetchContent` honours the first declaration of a name, adding the
second dependency to this tree made the build fetch that over the network while printing that it
was using the pinned 3.16.0 from the cache. Everything the manifest exists to guarantee, defeated
by the second entry ever added to it, and visible only because Catch2 v2 puts its CMake helpers
in a different directory from v3.

### 12.5 The required disagreement is absent on this path

Plan §4 rule 1: the predecessor computes IAU-76/1980 and this tree computes IAU 2006/2000A, so
"certain disagreements are required, of predictable size, and agreement would be the failure".

Measured against oracle F-01–F-03, at MJD 57372.37458333 with the input state from
`oracle/capture.sh`:

| | |
|---|---|
| separation from the predecessor | **1.6 mm** out of 7717 km |
| as an angle | 4.2 × 10⁻⁵ arcsec |
| predicted by rule 1 | ≈ 0.064 arcsec, ≈ 2.2 m |

**Rule 1 does not hold on the ITRF↔GCRS path**, and the mechanism — corrected after the manager
checked the arithmetic — is not the one first written here:

- The two chains run **genuinely different algorithms**. Each applies the celestial-pole offset
  series matched to its own model: dψ, dε onto an IAU-1980 nutation in the predecessor, δ*X*, δ*Y*
  onto the IAU 2006/2000A CIP here. Those series bring each model onto the **observed** pole, so
  the model difference cancels *by construction*. Rule 1 ignored the correction series.
- **TEME has no such series.** It is referred to the mean equinox of date, a model construct with
  nothing to reconcile two precession models against an observation, so the difference appears
  undiluted. Oracle T-01: 2.2 m at |r| = 7234 km is **0.0627 arcsec**, and the accumulated
  IAU-76-versus-IAU-2006 precession difference of 0.064 arcsec is **2.245 m** there — essentially
  all of it.
- **The kinematic equation-of-equinoxes terms are not the explanation.** They max at about
  0.0027 arcsec, which is **95 mm** at that radius — 4 % of T-01. An earlier draft of this
  section offered them as the cause; a gate set from them would have been **twenty-five times too
  small** and would have failed a correct implementation. They must still be carried
  (`FRAME-R-030`, amended); they are simply not why T-01 is 2.2 m.

The step-4 gate was specified as a required-disagreement test on the ITRF path and has been
restated as what the oracle actually supports: magnitudes agree, the separation is bounded well
above the measurement so a gross error still fails, and the round trip beats the predecessor's
closure. The required-disagreement gate moves to T-01 at L6, at **≈ 2.2 m**.

---

## 13. L2 `environment` step 1 — `ephemerides`

### 13.1 The gate, with its denominator

`testpo.440` is JPL's published verification set for DE440: 13 201 cases of
`(JD, target, centre, coordinate, value)` in AU and AU/day, in the **classic** body numbering.
`EPH-Q-003` was ruled that pinning both kernels is not enough — the full sweep must actually
**run**, and its case count be recorded here, because "pinned both, CI uses the short one" decays
into the full coverage being notional.

| kernel | span | **checked** | not a body (testpo targets 14–17) | outside coverage | worst residual |
|---|---|---|---|---|---|
| `de440s.bsp` | 1849–2150 | **3 099** | 1 847 | 8 255 | 7.105 × 10⁻¹⁵ AU = **1.06 mm** |
| `de440.bsp` | 1550–2650 | **11 354** | 1 847 | **0** | 7.105 × 10⁻¹⁵ AU = **1.06 mm** |

11 354 + 1 847 = 13 201: **every body case in the published set is exercised on the full kernel,
with nothing skipped for coverage.** JPL states its own tolerance as < 10⁻¹³ AU; this is fourteen
times inside it. The two skip reasons are counted separately on purpose — "not a body" is a
property of `testpo`, "outside coverage" is a property of the kernel, and lumping them would hide
which kernel was used.

### 13.2 Units: the factor of 1.5 × 10⁸ never appears in this tree's source

CALCEPH's default is AU and AU/day; this tree's canonical units are km and km s⁻¹. Rather than
write the conversion correctly, `EPH-R-010`/`-011` remove it: only `calceph_compute_unit` is
called, with `CALCEPH_UNIT_KM | CALCEPH_UNIT_SEC` named at the call site. `calceph_compute` is
never called — not because its documented AU default is wrong, but because it is **invisible at
the call site**, and a conversion written correctly today is still one somebody can edit.

The gate is built so a units error cannot cancel: `testpo` is in AU and the module returns km, so
the comparison passes through the AU — which `EPH-A-003` asserts exactly and `EPH-A-004` perturbs
by one part in a million, requiring the comparison to **fail**. It fails by four orders of
magnitude.

### 13.3 What implementation found

| finding | consequence |
|---|---|
| **An SPK kernel carries no constants at all** — `getconstantcount` returns 0 on `de440s.bsp`. `EPH-R-012`'s "read the AU from the kernel" was unsatisfiable on the route the plan mandates. | Spec amended at v1.2. Since **IAU 2012 Resolution B2 the au is a *defining* constant**, so the definition is the authority and a kernel that supplies one is *checked against* it. Whether it came from a kernel is recorded. |
| **The solar-system barycentre is the ROOT of an SPK's body tree** — it is the centre of every record and the target of none, so scanning for it finds nothing. | 868 `testpo` cases were being silently counted as "outside coverage". Fixed; the full sweep now skips **zero** for coverage. Found only because the denominator was being reported. |
| **CALCEPH's `src/CMakeLists.txt` declares `target_include_directories(calceph PUBLIC $<BUILD_INTERFACE:>)` — an empty build interface.** Its generated `calcephconfig.h` is then unreachable as a subproject and every translation unit fails. | Repaired from this tree's side rather than by patching the pinned bytes. **Third dependency whose own build system held a surprise**: ERFA ships no CMake at all, `tl::expected` captured our pinned Catch2, and this. |
| **`testpo`'s classic numbering is not what `SPEC-ephemerides` v1.0 §3.3 said.** 1 is Mercury **barycentre** (agreeing with NAIF), 3 is **Earth** (NAIF 3 is the Earth–Moon barycentre), 10 is the **Moon** (NAIF 10 is the **Sun**). | The table was corrected from CALCEPH's own documentation before implementation. Two of its three rows had been wrong. |
| **CALCEPH body 16 is TT−TDB**, the opposite sense to this tree's `tdb_minus_tt`. | `EPH-R-004` fixes one direction and the sign is flipped exactly once, in one place. This is precisely the trap the manager's correction 2 anticipated. |
| **`de440s.bsp` carries no TT−TDB record**, so `EPH-A-007` cannot compare two routes on it. | Reported rather than worked around; a `de440t` kernel would be needed, and TT−TDB is not consumed until L2 step 3. |

### 13.4 The licence denylist was a denylist

Recorded at §8.12. Prompted by CALCEPH being triple-licensed, and it generalises past it.


## 14. L2 `environment` step 2 — `gravity`: the specification

`spec/SPEC-gravity.md` v1.0, drafted 2026-09-18, **awaiting manager review**. Nothing is
implemented and nothing here is a gate yet. What this section records is the set of numbers the
specification asserts, and where each came from, because most of them were **measured from the
published sources during drafting** rather than quoted — and a measurement made while writing a
specification is provenance in exactly the way a measurement made while implementing one is.

### 14.1 Sources obtained, and the two that were not

| key | what | obtained |
|---|---|---|
| `TN36-6` | IERS Conventions chapter 6, update of 2018-02-01, sha256 `abb3c0b0…4388` | primary |
| `TN36-7` | IERS Conventions chapter 7 §7.1.4, the secular pole, sha256 `ffe8ffb1…043d` | primary |
| `EGM08` | NGA `EGM2008_Spherical_Harmonics.zip`, 109 351 360 bytes, sha256 `65a9072f…0fbd` | primary |
| `WGS84` | NGA.STND.0036_1.0.0_WGS84, sha256 `5edc1cf7…e512` | primary |
| `DLMF-14` | NIST DLMF §14.6, §14.10 | primary |
| `NASA-TP` | NASA/TP-2016-218604, sha256 `7aa56a42…87ef` | primary, informative |
| `HF02` | Holmes & Featherstone 2002, J. Geodesy 76:279–299 | **not obtained** — paywalled |
| `PAVLIS12` | Pavlis et al. 2012, JGR 117 B04406 | **not obtained** |

`HF02` is the standard citation for evaluating normalised Legendre functions at ultra-high
degree and it could not be read. Nothing in the specification rests on it: §4.4's recursion is
derived from `TN36-6` (6.2b) with `DLMF-14`, and the property that makes the ultra-high-degree
treatment necessary is measured here (§14.3) instead of cited.

### 14.2 The Conventions print no recursion

The step's scope says "recursions taken from the tables printed in the Conventions rather than
from anyone's code". **Chapter 6 prints no recursion.** It gives the expansion (6.1) and the
normalisation (6.2b); the words "recursion" and "recurrence" do not occur in it, nor in the
other chapters held in this tree. The instruction cannot be followed as worded, and
`GRAV-Q-003` asks for the substitute to be ratified.

What was done instead: the fully normalised forward-column recursion was **derived** from
`TN36-6` (6.2b) together with `DLMF-14` (14.6.1) and (14.10.3), and then **verified before being
written down**, in exact rational arithmetic carried to 60 significant digits, over
0 ≤ m < 40, m+2 ≤ n < 60 for the general step and m ≤ 60 for the seeds.

| step | worst relative disagreement with the definition |
|---|---|
| general step, P̄′ₙₘ = aₙₘ u P̄′ₙ₋₁,ₘ − bₙₘ P̄′ₙ₋₂,ₘ | 9.895 × 10⁻⁵⁸ |
| sectorial seed, m ≥ 2 | 9.597 × 10⁻⁶⁰ |
| first step, n = m+1 | 9.471 × 10⁻⁶⁰ |

All three are the 60-digit arithmetic's own rounding. The same run also established the trap
that `GRAV-R-033` exists for: the general sectorial seed is **wrong at m = 1** by exactly √2,
because (2 − δ₀ₘ) changes between m = 0 and m = 1. √3 = 1.732050807568877 is correct;
√(3/2) = 1.224744871391589 is what the general seed gives.

`DLMF-14` (14.6.1) carries the Condon–Shortley phase (−1)ᵐ and the geodesy convention does not.
Recorded because it is a sign flip on every odd order, and because a reference implementation
borrowed for a test will have one convention or the other and will not announce which.

### 14.3 Why the classical form cannot be used at degree 2190 — measured

Evaluating the seed product to m = 2190: the **factored** function P̄′ₘₘ = P̄ₘₘ/cosᵐφ is
bounded by **10.277 577** over the whole model. The **unfactored** one underflows the smallest
normal double (2.225 × 10⁻³⁰⁸) above latitude **82.0°** at m = 360 and above **43.7°** at
m = 2190 — more than half the Earth's surface by area, at the model's full order. The term is
lost rather than small, which is the whole reason `GRAV-R-030` mandates the factored recursion
and `GRAV-A-007` tests that doing it the other way is measurably worse.

`NASA-TP` §4.5 reaches a compatible conclusion from the singularity side — that the stability of
each of the three singularity-free algorithms it studies is set by the Legendre generator inside
it, and that normalisation *amplifies* whatever instability the generator has. Its own trend
study stops at degree 150, below where this difficulty begins.

### 14.4 What was measured from `EGM2008_to2190_TideFree` itself

| measured | value |
|---|---|
| records | 2 401 333 — the full triangle to (2190, 2190) less the three absent records of degrees 0 and 1 |
| degrees 0 and 1 | **absent from the file**; GM carries degree 0 and degree 1 vanishes for a geocentric origin |
| last record | (2190, 2190), and it is **zero** — the file is padded to a full triangle |
| highest order with a non-zero coefficient | 2159 at **odd** degrees above 2159, **2158** at even ones — the parity coupling of the ellipsoidal-to-spherical conversion behind EGM2008 |
| C̄₂₀ in the file (tide free) | −4.841 651 437 908 15 × 10⁻⁴ |
| conventional C̄₂₀ (zero tide), `TN36-6` Table 6.2 | −0.484 169 48 × 10⁻³ |
| conventional tide-free, from §6.2.2's −4.1736 × 10⁻⁹ | −0.484 165 306 4 × 10⁻³, reproducing the −0.484 165 31 × 10⁻³ the Conventions print |
| **conventional tide-free minus the file's** | **−1.6261 × 10⁻¹⁰**, which is 8× the 2 × 10⁻¹¹ uncertainty `TN36-6` §6.1 states |

The last row is why `GRAV-R-020`'s substitution is mandatory: the two values come from seventeen
years of SLR and four years of GRACE respectively, and a field that keeps the file's value is
not the conventional model.

The truncation errors of `GRAV-P-1` to `GRAV-P-3` were computed from the file's own degree
amplitudes by the exact identity of `GRAV-R-041`: **1.2174 × 10⁻¹⁰**, **9.9122 × 10⁻¹²** and
**2.3018 × 10⁻¹⁴ m s⁻²** at the three rows of `TN36-6` Table 6.1.

### 14.5 Table 6.1 is a statement about an orbit, and no gate here is set from it

Table 6.1 says its truncations give "3-dimensional orbit accuracy of better than 0.5 mm". The
obvious move is to turn that into an acceleration tolerance. Carried out — treating the
truncation error as a constant acceleration acting for one revolution — it gives **2.3753 mm**
for Starlette and **0.90677 mm** for Lageos against Table 6.1's 0.5 mm, and only GPS's
**0.021454 mm** comes in under. The conversion is what is wrong, not the model or the table:
truncation error at degree N oscillates at N cycles per revolution and does not accumulate
secularly.

So the specification sets **no gate from Table 6.1** and prints the gap instead. This is the
same class of mistake as §8.12's four disguises, running the other way: a true statement about a
larger thing — a fitted orbit — read as a statement about a smaller one. It cost nothing only
because the arithmetic was done before the gate was written.

### 14.6 A claim corrected during drafting

§3.7 of the draft first said the conventional C̄₂₁ is "of order 10⁻⁹, several times the value
the file carries". Computed, it is not: at J2000.0 it is −2.264 385 × 10⁻¹⁰ against the file's
−2.066 155 × 10⁻¹⁰ — a 10 % difference, not a factor of several. The statement was replaced by
the measured table and by the argument that actually holds, which is **epoch dependence**: the
file's values are fixed and the conventional ones move with the secular pole, so by 2026.0 the
differences reach 1.9822 × 10⁻¹⁰ and 2.8020 × 10⁻¹⁰, a degree-2 amplitude of 3.4322 × 10⁻¹⁰,
worth an RMS acceleration of 7.4627 × 10⁻⁹ m s⁻² at 7331 km — **sixty-one times** the degree-90
truncation error the same field accepts.

### 14.7 The gate rests on a closed form, because no published accelerations exist

No published table of geopotential accelerations was found: searched NGA's EGM2008 distribution,
ICGEM, `PAVLIS12`, `NASA-TP` (whose appendix B publishes *error magnitudes*, for the Moon) and
the harmonic-synthesis literature. `GRAV-A-001` is therefore a **category-2** test in
`SPEC-template.md` §8's ordering — a closed-form identity — and not a category-1 one, and §8 of
the specification says so plainly. `GRAV-Q-001` asks whether anyone knows of a published set.

One published point-wise reference does exist and was **not** adopted: `EGM08` contains
`INPUT.DAT` and `OUTPUT1.DAT`, six latitude/longitude pairs and their EGM2008 **geoid
undulations** to the millimetre, at both poles and at full degree 2190. Consuming it would need
the WGS 84 normal potential and Somigliana normal gravity in closed form, a second pinned 142 MB
expansion, and Pavlis's option conventions matched to 4 × 10⁻⁵ relative. `GRAV-Q-002` puts the
trade to the manager.

`hsynth_WGS84.f`, the FORTRAN harmonic-synthesis program in the same archive, **was not opened**.
It is exactly what the step's scope says not to take recursions from, and its presence in a
manifest-declared archive is recorded in the specification's front matter so that the declaration
is checkable rather than merely asserted.

---

### 14.8 Implementation, and the six things it corrected in the specification

`modules/gravity` built and gated on 2026-09-18, the same day the specification was adopted.
**10 gates green offline, 163 tests, 0 failures, 611 artefacts byte-identical across two build
paths.** `SPEC-gravity` is at **v1.2** and `SPEC-frames` at **v1.5**; every correction below was
found by a test rather than by review.

**1. The factored recursion is not bounded, and the specification said it was.** §14.3 measured
that the classical form underflows above 43.7° of latitude at order 2190, and that is right. §3.6
of the draft went on to say the factored form "never exceeds about 10.3 anywhere in the model",
which is true only of the **sectorial** seed. From the closed form
P̄′*ₙₘ*(1) = √((2*n*+1)(2−δ₀ₘ)) √((*n*+*m*)!/(*n*−*m*)!) / (2ᵐ *m*!):

| quantity | value |
|---|---|
| max over *m* of P̄′₂₁₉₀,ₘ(1) | **10⁴⁵⁷·⁸⁶⁴**, at *m* = 979 |
| largest double | 10³⁰⁸·²⁵ |
| overflow margin | **10¹⁵⁰** |

And it does not merely overflow: once two consecutive entries of a column are infinite, the
three-term recursion subtracts one from the other and the column becomes **NaN**, which
propagates silently into every sum it touches. Both representations fail, in opposite
directions, and neither works alone. What works is the pair — a global scale of 10⁻²⁸⁰ on every
associated Legendre function, and the powers of cos φ folded back in through a **Horner nest over
order** so that cosᵐφ is never formed as a number. That is `HF02`'s construction, and `HF02`
could not be obtained; 10⁻²⁸⁰ is adopted because the measurement above says it is right
(10⁴⁵⁷·⁹ ÷ 10²⁸⁰ = 10¹⁷⁷·⁹ at the top, 10⁻²⁸⁰ for a unit result at the bottom), not because it
was cited. `GRAV-A-007` now demonstrates both failures rather than describing them.

**2. C̄₀₀ is 1, not 0.** `GRAV-R-012` said the reader must supply degrees 0 and 1 as zero.
`TN36-6` (6.1) sums from *n* = 0 with C̄₀₀ carrying the two-body term. Setting it to 1 makes
`GRAV-R-027`'s exact GM/*r*² at degree 0 a consequence of the same sum rather than a separate
code path, and makes σ₀ = 1, which is what it should be. Degree 1 is zero, for the quite
different reason that the origin is the centre of mass.

**3. `GRAV-P-8`'s 10⁻¹³ is not achievable at degree 2190.** The three-term recursion accumulates
rounding over *n* − *m* steps with a mild cancellation at each. **Measured worst: 6.1 × 10⁻¹¹, at
degree 2190, order 0, on the polar axis.** The budget is now tiered — 10⁻¹³ to degree 360, which
covers every truncation Table 6.1 suggests, and 10⁻⁹ to 2190. The derivation itself still agrees
with the definition to 9.9 × 10⁻⁵⁸ at 60 digits, so what this budget measures is floating point
and not algebra.

**4. `GRAV-A-008` claimed coverage that does not exist.** It said a √2 error in the *m* = 1 seed
would fail the gate. It would not: the *m* = 1 share of σ*ₙ*² is about 7 × 10⁻¹² at degree 2,
because C̄₂₀ dominates that degree by six orders of magnitude, so the error moves no degree
variance measurably. It is caught by `GRAV-A-003`, whose reference set includes (2, 1) and
(2190, 1) from the definition. The claim was removed rather than left looking like coverage.

**5. WGS 84's GM cannot be refused by its value.** `GRAV-R-006` treated it as a distinct wrong
constant. It is not: 3 986 004.418 × 10⁸ m³ s⁻² **is** the TCG-compatible EGM2008 value under
another name. What is refusable is the realistic mistake — taking **both** constants from WGS 84,
whose semi-major axis is 6 378 137.0 m against this model's *a*ₑ = 6 378 136.3 m — so the pair is
checked as a pair, and the GM's declared time-scale compatibility is recorded. The consequence
the requirement exists for is now measured rather than asserted: 5.5821 × 10⁻⁹ m s⁻² at 7331 km,
108.91 mm over one revolution, **secularly**, which is the one place in this specification where
half-a-T-squared is the right instrument.

**6. An off-by-one in `truncation_rms`, caught only because the expected values came from
elsewhere.** The power (*a*ₑ/*r*)ⁿ was advanced before its first use instead of after, putting one
extra factor into every term — a clean 13 % error at 7331 km and 48 % at 12270 km. Both are the
sort of size that reads as a modelling difference. It was visible because `GRAV-A-013`'s expected
values were computed independently in Python from the file's own degree amplitudes, not from this
code.

### 14.9 What the gate measured

| | |
|---|---|
| `GRAV-A-001`, 7331 km | **2190 of 2190 degrees compared**, 1 skipped (degree 1 is identically zero), 300 sample points, band 5/√(2*K*) = 20 %, **mean ratio 0.99699** |
| `GRAV-A-001`, 12270 km | 964 of 1200 compared, 237 below the representable floor, mean ratio 0.99893 |
| `GRAV-A-001`, 26600 km | 441 of 600 compared, 160 below the floor, mean ratio 0.999139 |
| `GRAV-A-001b`, 7331 km | 60 degrees, **20 000 sample points**, band 2.5 %, **worst 8.4 × 10⁻⁶ at degree 35, mean ratio 1.000000** |
| `GRAV-A-027`, the J2 closed form | 32 points, **worst relative 2.5 × 10⁻¹⁶** |
| `GRAV-A-005`, gradient vs central differences | 12 points at degrees 2, 8, 90, 360; worst 1.8 × 10⁻¹⁰ |
| `GRAV-A-004`, orthonormality | 266 (*n*, *m*) pairs to degree 60, 256-point Gauss–Legendre |
| `GRAV-A-013`, truncation | 1.21736 × 10⁻¹⁰, 9.91217 × 10⁻¹², 2.3018 × 10⁻¹⁴ m s⁻², matching §14.4's independent computation |
| `GRAV-P-5`, cost | **26 ms** per full degree-2190 evaluation = 11 ns per coefficient pair, against a 100 ns budget |

**Two things about the gate's own design are worth keeping.** Its first version accumulated a
sum of squares of |**a**ₙ|: the degree-1900 acceleration at 7331 km is about 10⁻¹³⁰ m s⁻², which
is representable, but its **square is not**, so the statistic silently became zero and the test
reported a 100 % disagreement over half its range. Dividing by the prediction first keeps every
quantity at order unity. And an attempt to tighten the band at low degree on the reasoning that a
Fibonacci lattice is low-discrepancy was simply **wrong at K = 300** — degrees 19 to 27 missed a
1 % band. The right answer was more samples where they are cheap, not a smaller number: degree 60
is 1830 terms against 2.4 million, so `GRAV-A-001b` uses 20 000 points and gets 8.4 × 10⁻⁶.

### 14.10 Two additions outside `gravity`

**`SPEC-frames.md` v1.5 gains `Position<F>` and `Acceleration<F>`** (§3.6, `FRAME-R-060`/`-061`,
`FRAME-A-024`). `GRAV-R-050` assumed `frames` had frame-carrying vector types; it has `State`,
which is a position *and* a velocity *at* an epoch, in km, and a point at which to evaluate a
static field is none of those. They were added to `frames` rather than declared in
`SPEC-gravity.md`, on the manager's ruling for `EPH-Q-001`. They carry **metres** where `State`
carries km, with the unit in the accessor name on both sides. Raised for the manager's verdict as
`GRAV-Q-009`, because amending an adopted specification is the manager's.

**`tools/fetch.py` learned to extract declared archive members, each with its own SHA-256.** An
archive entry pinned by the hash of the whole archive says nothing about what is taken out of it.
EGM2008 ships as seven members of which this tree consumes two, and one of the five it does not
consume is `hsynth_WGS84.f`, the FORTRAN harmonic-synthesis program that plan §3.3 step 2 forbids
as a source of recursions. Declaring the consumed members by hash is what turns "we did not use
it" from an assertion into something a reviewer can check: the cache contains exactly what the
manifest names, and nothing else was unpacked.

### 14.11 The three conditions attached to accepting step 2

**The crossing is named, and there is one of it.** Ratifying `GRAV-Q-009` — km in `State`, metres
in `Position`/`Acceleration` — was made conditional on something the specification had not said:
the split was declared and the **crossing** was not. Somewhere a sum of accelerations in m s⁻²
becomes the derivative of a `State` in km, and a crossing left to arrive with the first force
arrives once per force. It is now `odl/core/units.hpp`:
`state_accel_km_s2_from_m_s2` and `field_position_m_from_state_km`, with its own test, no implicit
conversion and no generic `convert<>`. `SPEC-frames.md` v1.6 `FRAME-R-062` binds `SPEC-dynamics`
to state **where** the crossing happens before any force is integrated.

*Its test caught a false claim in its own comment on the first run.* The round trip was asserted
exact, on the reasoning that 1000 is a power of ten and therefore harmless. A power of ten is not
a power of two: 1e-9 fails it. The claim is now one ulp, with exactness asserted only for the
cases that arise here — a geocentric radius in km and an acceleration in m s⁻².

**Archive member hashing generalised.** The rule was prompted by EGM2008 and applies to every
archive: an archive's SHA-256 pins the container and says nothing about which members are read
out of it. Every manifest entry with `unpack` now declares `consumes` — `whole-tree` where the
archive is compiled entire, `declared-members` where it is not — and lists each individually-read
member with its own SHA-256. That is nine members across five archives: four licence texts
quoted verbatim into `NOTICE`, three files compared against populated FetchContent trees, and
EGM2008's two extracted into the cache. `tools/fetch.py verify` refuses an archive entry that
declares neither, and reads the non-extracted members straight out of the archive rather than
duplicating bytes to check them.

**The scale's bottom margin, measured rather than argued.** 10⁻²⁸⁰ was justified at the top —
10⁴⁵⁷·⁹ ÷ 10²⁸⁰ = 10¹⁷⁷·⁹, 130 decades of headroom — and at the bottom only by "a unit result
becomes 10⁻²⁸⁰", which is not the binding constraint: the binding constraint is the **smallest
intermediate the recursion forms**. Measured over 74 802 scaled values spanning orders 0 to 2159
and latitudes 0° to 90° (`GRAV-A-029`):

| | |
|---|---|
| range | 1.055 × 10⁻²⁸² to 7.313 × 10¹⁷⁷ |
| decades above the smallest normal double | **26.0** |
| decades below the largest | **130.1** |
| subnormal | **0** |

And the bound that makes 26 decades enough rather than merely observed: P̄′*ₙₘ* = P̄*ₙₘ*/cosᵐφ with
cos φ ≤ 1, so |P̄′| ≥ |P̄| everywhere. The scaled value can be subnormal only where P̄ itself is
below 10⁻²⁸, and a term that small sits beside terms of order 1 to 66 in the same degree's sum —
already below that sum's own rounding.

---

## Changelog

| date | change |
|---|---|
| 2026-09-18 | **L2 step 1 `ephemerides` implemented and gated.** §13 added: the `testpo.440` sweep with its denominators (11 354 of 13 201 body cases on the full kernel, 0 skipped for coverage, worst residual 1.06 mm against JPL's 15 mm tolerance), the units design, and six findings from implementation. §3 gains CALCEPH with **CeCILL-B chosen out of its triple licence** and the §5.3.4 obligations recorded. §8.12 records the licence denylist becoming an allowlist. `SPEC-ephemerides` amended to v1.2 (an SPK carries no constants) and `SPEC-frames` to v1.4 (`Frame::BCRS`). |
| 2026-09-18 | **Four specification amendments applied**, at the manager's verdict, all three module specs to v1.3: `EOP-A-003` to TN36 §8.2's published tolerance; `FRAME-R-030` corrected to require the kinematic equation-of-equinoxes terms and `FRAME-A-001` restated at 25 mm with the unexplained z-rotation left recorded; `TIME-R-021a` for `eraDtdb`'s UT1 argument; and the prediction/leap-horizon interaction as `SPEC-eop` §4.6 and `SPEC-time` §4.9. §1 and §12.5 corrected: the ITRF-path agreement is **not** "the same algorithm" but two different algorithms whose model difference cancels because each is corrected onto the observed pole, and the kinematic terms are 4 % of T-01 rather than its cause. |
| 2026-09-18 | **L1 steps 1–4 executed and gated.** §12 added: what was built, the generated tidal tables with their agreement against the four IERS published test cases, the six errors the tests caught, the dependency capture, and the measurement showing plan §4 rule 1's required disagreement is absent on the ITRF↔GCRS path. §1 module register updated to *implemented* and gains `core`. §8.11 records the FetchContent capture. |
| 2026-09-18 | **L0 steps 3–7 executed and gated.** §11 added: the toolchain decisions with the rejected alternatives, what each step produced, the layering-as-link-boundary decision, and the platform properties now pinned by test. §3 dependency register populated and marked generated-not-maintained. §0.1 records that the clean-room discipline ended with the merge and that nothing written after it carries a derivation declaration. §8.10's block-recovery example restated one pair at a time with both formulas, having previously compared two ranges whose endpoints came from different block pairs. Moved the stranded R9 patent-search result into §9. |
| 2026-09-18 | Tree merged into the predecessor's repository at the owner's instruction: `/home/rog/odl-self_built` → `/home/rog/odl20Lite/rewrite`, one folder and one repository. Standalone history preserved at `doc/.history/odl-self_built.bundle`. |
| 2026-09-18 | D5/D6 recorded; tree created at `/home/rog/odl-self_built` (since merged, see above) and committed at `bdd80be`; oracle pointer added to §6; the unstated-denominator rule recorded at §8.10 and added to `SPEC-template.md` §8; the unnamed copyright holder raised as the one open title item. |
| 2026-09-18 | §8.9 closed: the plan moved into this tree as the canonical and only copy, resolving the stale-copy hazard structurally. Verified its contents; its absence from the predecessor tree is taken on report, because verifying it is the thing this session may not do. |
| 2026-09-18 | Manager audit of the v1.2 self-fix passed; denominator of 121 confirmed. Recorded the **R2 corollary** at §8.8 and flagged the stale local plan copy at §8.9. |
| 2026-09-18 | Acceptance coverage completed at spec v1.2 — twenty tests added, complete §8 coverage tables, no requirement changed. |
| 2026-09-18 | Manager verdict recorded: specs adopted at v1.1; §0 split into declarations, **disclosed context exposure** and adoption; plan rules **R11** and **R12** added at §8.6–8.7; the Horizons determination added to the oracle log; decision register added at §10; D5/D6 marked escalated. |
| 2026-09-18 | Seeded from `doc/REWRITE_PLAN.md` §8; populated for the P1 specification tranche. |
