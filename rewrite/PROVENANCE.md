# PROVENANCE — odl/self_built

The ledger required by rule **R5** of `../plan/PLAN.md`: every module traced to the
published sources it implements, every constant traced to the document it came from, every
dependency traced to its licence, and every comparison against the predecessor logged.

It is also the due-diligence pack. A reader who wants to know whether this tree is the
owner's to license should be able to answer that from this file alone.

**State of the tree at this revision, corrected 2026-09-24 — stale since P1, found while closing
L4 step 5 (§29), fixed here because a due-diligence reader starts at the top of this file and this
line is exactly the kind of claim that reader needs current, not historical.** The P1
specifications were **adopted by the manager on 2026-09-18**; implementation has since proceeded
through **L4 `forces-analytic` step 5** — L0 `foundation`, L1 `time-frames`, L2 `environment`
(`ephemerides`/`gravity`/`perturbations`/`atmosphere`), L3 `dynamics` (the force-plugin interface,
coefficients, the STM, the parameter registry, `drag`), and L4 steps 1–5 (`shadow`, `macromodel`,
`srp_analytic`, `attitude`/`srp`/`erp`) are all built and gated — `tools/ci.sh` exits 0, 297 tests,
as of commit `cb0f629`. §11 onward, in order, is the record of each. The **module register
immediately below (§1) is scoped to the original P1 modules only** (`core`/`time`/`ephemerides`/
`frames`/`eop`) and was never extended as later layers were built — each later module's own status
is instead recorded where it was built (§21 onward), not duplicated into this table. Rows below
whose "status" reads *specified* record what a module will implement when it is built, not what it
does today.

| | |
|---|---|
| **Seeded** | 2026-09-18, from `doc/REWRITE_PLAN.md` §8 |
| **Plan** | `../plan/PLAN.md` since 2026-09-23, when it moved from `doc/REWRITE_PLAN.md` and was split into one file per layer step, **section numbers unchanged**. Dated rows and history below cite the old path, which was true when they were written. |
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
| 2026-09-18 | **Two corrections of reasons rather than conclusions.** §18: the per-constituent resonance corrections ARE signed — P1's is (0,−1) — so what blocks `formula + correction = table` is only the untabulated δk^OT, and that **measures** it: 0.467–0.616 against "roughly half" and 1.002 against "about the same magnitude", with the direction determined by the spread. **ψ1 stays anomalous after the published correction**, at −1.539. And the L2 floors are measured at one radius in `tests/l2_floors.cpp`: the truncation floor is **2.46×** the smallest term kept, not the three orders `PERT-R-022a` claimed from a geostationary-against-LEO comparison. **10 gates, 196 tests.** |
| 2026-09-18 | **Step 3 reopened: chapter 6 prints a worked example and this tree said it did not.** §17: `PERT-A-029` uses TN36-6 §6.2.1's K1 example — non-circular, and **the only check that exercises the theta dependence**, which theta_f = 0 cannot touch. The Conventions' words about the ocean-tide term check at 0.906 and 0.598. **psi1 is rank 28 of 48 in the chosen statistic** — the median did not absorb it; the real-part ratio never looked at the imaginary part, so the statistic was blind rather than tolerant. The truncation's cost (3.94x) and its asymmetry stated. `PERT-Q-010` ruled and implemented as three accessors with three return types; doing its arithmetic found **`FRAME-Q-006`'s L_B figure wrong by a factor of a thousand — 2.32 km, not 2.3 m — in prose since v1.0**, now `FRAME-P-7` so gate 8 reaches it. And a stale-configure guard that could never fire, now a test proven both ways. **10 gates, 195 tests.** |
| 2026-09-18 | **L2 step 3 `perturbations` implemented as three link targets.** §16: the gate with both denominators (50 constituents exact; 14 of 48 rows constrain the internal relation); the three column orders of Tables 6.5a/b/c; **`PERT-A-025` checks the resonance structure and not δk, because TN36-6 defines δk as including an ocean-loading contribution (6.9) does not produce**; two statistics that were not what their names said — the ocean pole tide's variance (90.55% weighted against 75.8% raw) and the ocean tide's truncation criterion, which was self-referential and now measures **degree 89**; a parser that dropped 8 of 18 FES2004 waves on five-digit Doodson codes; `Ephemeris::state` typing a geocentric vector as barycentric (`PERT-Q-010`); and `--warn NoAssertions` proven by injection. `SPEC-perturbations` v1.2. **10 gates, 192 tests, 626 artefacts byte-identical.** |
| 2026-09-18 | **`SPEC-perturbations` adopted at v1.1; three amendments and eight rulings applied.** §15.6: `PERT-A-001` was overclaimed as category 1 when the printed amplitudes are the module's own input — decomposed honestly, with `PERT-A-025` (the resonance formula as a test-only cross-check) added as the one thing that verifies δk without an H_f catalogue. §15.7: `de440t.bsp` substituted for `de440.bsp` after verifying both DAFs' segment summaries (14 shared, identical; one extra, the TT−TDB record); constraint 9 re-ran step 1's full sweep unchanged at 1.06296 mm; and **`EPH-A-007` now runs for the first time — 53 epochs, worst 26.027 ns against 100 ns** — where it had been passing by executing zero times since step 1. §15.8: `speccheck.py`'s components now partition its denominator and **found three stale Coverage rows, two in specs adopted at L1**; `fetch.py` sniffs HTML error pages and file magic **before** hashing. **10 gates, 165 tests.** |
| 2026-09-18 | **L2 step 3 `perturbations` specification drafted.** §15: the sources including chapter 10's `tn36_c10.pdf` filename trap; the ocean pole tide chain **verified to every printed digit of TN36-6 (6.24) before being specified**; a THIRD kind of source defect — a document disagreeing with its own companion dataset, where §6.3.2 tells you to add Ω₁/Ω₂ that `FES2004-CS` already contains — and a fourth instance of the first kind, §6.4's cross-term ratio 0.0115 against the 0.011700 its own *k*₂ implies; the gate's two denominators; and a counting error of the author's that the manager corrected. Four manifest entries added, 22 in total. **10 gates, 165 tests.** |
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

## 15. L2 `environment` step 3 — `perturbations`: the specification

`spec/SPEC-perturbations.md` v1.0, drafted 2026-09-18, **awaiting manager review**. Nothing is
implemented. 99 own-prefix identifiers; 57 requirements and refusals, 42 discharged by an
acceptance row and 15 excused with a reason, 0 uncovered. Four new manifest entries, 22 in total.

### 15.1 Sources, and one filename that is not what the pattern says

| key | what | obtained |
|---|---|---|
| `TN36-6` | chapter 6 §§6.2–6.6 and Tables 6.3–6.8 | primary, already pinned |
| `TN36-7` | chapter 7 §7.1.4 (25), the wobble variables | primary, already pinned |
| `TN36-10` | chapter 10 §10.3 (10.12), the relativistic correction | primary, **newly pinned** |
| `TN36-1` | chapter 1, the numerical standards | primary, **newly pinned** |
| `FES2004-CS` | `fes2004_Cnm-Snm.dat`, 3 686 988 bytes | primary, **newly pinned** |
| `DESAI-CO` | `desaiscopolecoef.txt.gz`, 2 452 565 bytes, 65 340 rows to degree 360 | primary, **newly pinned** |
| `LYARD06`, `DESAI02`, `CT71`, `CE73`, `MATHEWS02` | the papers behind the models and the amplitude convention | **not obtained** — five of them |

**Chapter 10 is `tn36_c10.pdf`, not `icc10.pdf`.** Every other chapter of the Conventions is
`content/chapterN/iccN.pdf`; `content/chapter10/icc10.pdf` returns a 404 page, which `curl`
saves happily and `pdftotext` then reports as sixty syntax errors rather than as a wrong file.
`SPEC-time.md` had recorded the correct URL at L1 and looking there was faster than guessing.
The manifest entry now says so, because the next person will guess the same way.

`CT71` and `CE73` define the amplitude convention *H*_f that (6.8) is written in and neither was
obtained. Nothing in the specification depends on them as documents: every *H*_f the module needs
appears inside the Conventions' own printed products (*A*ₘ δ*k*_f *H*_f), so the convention never
has to be applied independently, and `PERT-F-006` refuses a constituent outside the tables rather
than guessing one.

### 15.2 The ocean pole tide chain, verified before it was specified

`TN36-6` (6.24) prints the degree-2 ocean pole tide coefficients. Computing them from
`TN36-1`'s constants, γ = 1 + *k*₂ − *h*₂ from §6.5, and `DESAI-CO`'s own (2, 1) row
(Ā₂₁ = −0.243 253 305 + 0.005 468 074 *i*, B̄₂₁ = 0.005 468 074 − 0.192 521 112 *i*):

| | computed here | `TN36-6` (6.24) prints |
|---|---|---|
| ΔC̄₂₁ | −2.177 813 × 10⁻¹⁰ (*m*₁ − 0.017 24 *m*₂) | −2.1778 × 10⁻¹⁰ (*m*₁ − 0.01724 *m*₂) |
| ΔS̄₂₁ | −1.723 155 × 10⁻¹⁰ (*m*₂ − 0.033 65 *m*₁) | −1.7232 × 10⁻¹⁰ (*m*₂ − 0.03365 *m*₁) |

Every printed digit, both coefficients and both cross terms. *R*₂ = 2.687 689 × 10⁻⁴. The gate
`PERT-A-006` is therefore known to be satisfiable before implementation rather than after, which
is a different thing from hoping it is.

### 15.3 A third kind of defect in a normative source

This project has logged two: `PROVENANCE.md` §8.2, a data product contradicting itself about its
own precession-nutation model, and §8.3, a reference routine omitting a correction the
Conventions require. Step 3's drafting found a third kind, and then a fourth instance of the
first kind.

**A document disagreeing with its own companion dataset about what the dataset contains.**
`TN36-6` §6.3.2 describes how to model the very long period waves Ω₁ (18.6 yr) and Ω₂ (9.3 yr)
as equilibrium waves and gives the equation. The first three lines of `FES2004-CS` say they are
**already in the file**. Reading the chapter alone — which is what a specification written from
the chapter alone would do — one implements the equation and doubles those waves. The same header
says two further things §6.3.2 does not: the **long-period band is from FES2002 to (50, 50)**,
not FES2004, and the **atmospheric tide is not included**. `PERT-R-020a` now requires the header
to be read, `PERT-F-011` refuses a file whose header differs from what the specification was
written against, and `PERT-R-025` forbids applying the equilibrium equation at all.

The §6.3.2 note that *there is an ongoing discussion on whether the phase change to π/2 …is
justified*, dated 2011-10-14 and still unresolved in the 2018 edition, therefore does not bind
this module — because the module never applies that equation.

**And a document disagreeing with itself, in one paragraph.** `TN36-6` §6.4 gives the solid Earth
pole tide as ΔC̄₂₁ = −1.333 × 10⁻⁹ (*m*₁ + 0.0115 *m*₂) with *k*₂ = 0.3077 + 0.0036 *i*. The
cross-term ratio is Im(*k*₂)/Re(*k*₂):

| | |
|---|---|
| from the printed *k*₂ | 0.0036 / 0.3077 = **0.011 700** |
| printed in §6.4 | **0.0115** |
| Im(*k*₂) the printed ratio implies | 0.003 539, against the printed 0.0036 |
| the difference, on ΔC̄₂₁ | **0.02 %** |

The leading coefficient itself reproduces: Ω² *a*_E³ *k*₂^R/(GM⊕√15) × 1″ = **1.333 237 × 10⁻⁹**
against the printed 1.333 × 10⁻⁹, which is what tells us the derivation is right and the
discrepancy is in the ratio rather than in the reading. `PERT-R-031` follows *k*₂ — the constant
is the input, the ratio is a convenience derived from it — and `PERT-A-005` **reports both**
rather than silently choosing.

### 15.4 The gate carries two denominators

The manager's condition on step 3, given when step 2 was accepted: *a count of passed terms with
no denominator is the failure mode that hid 868 cases last time.* `PERT-A-001` therefore reports
**the terms it checked and the terms of this module the Conventions print no expected value
for**, and §8 states the second list explicitly: Step 1's time-domain evaluation at any epoch,
the ocean tide sum at any epoch, the ocean pole tide above degree 2, the relativistic correction
as a vector rather than as the magnitudes and precession rates `TN36-10` states, and third-body
attraction, which the Conventions do not treat at all.

What the Conventions **do** print per term is the whole of Step 2: every constituent of Tables
6.5a, 6.5b and 6.5c, in-phase and out-of-phase. Evaluating one constituent at θ_f = 0 makes
(6.8b) with η₁ = −*i* return exactly the two printed amplitudes, so the gate is category 1 and
term by term without needing an *H*_f the Conventions do not print.

### 15.5 A counting error of the author's, corrected by the manager

The step 2 report and §14.11 both said *seven members across five archives* and then enumerated
four licence texts, three files compared against populated trees, and two extracted — which is
nine, and nine is what the manifest has. The enumeration was right and only the total was wrong.
The manager corrected §14.11. Recorded because gate 8 catches exactly this in a specification
table and cannot catch it in prose, and the rule that follows is worth more than the correction:
**when a count is written next to its own breakdown, add the breakdown.**

### 15.6 The amendment the review turned on, and it was an overclaim of the author's

`SPEC-perturbations` v1.0 §8 called `PERT-A-001` **category 1**, *most of the gate*, and *a
stronger position than either of the preceding steps had*. All three were wrong, in this
project's usual shape.

`PERT-R-014` extracts Tables 6.5a/b/c as generated source, so the **printed amplitudes are the
module's input**. `PERT-A-001` then compares the module's output against those same amplitudes
and therefore **cannot fail on a wrong amplitude**. The statement that was true of the smaller
thing — the numbers are published — was written as true of the larger one: that the gate checks
the module against them.

Compare the two preceding steps. `GRAV-A-001` checks the field against a closed-form identity the
synthesis does not use; `EPH-A-001` checks against JPL's own published residuals. Both are
expected values produced by a route the module does not take. Step 2's frequency corrections are
not, which makes this a **weaker** position than either, not a stronger one.

v1.1 decomposes it honestly, and the decomposition is not a weak result:

| test | fails on | cannot fail on |
|---|---|---|
| `PERT-A-002`, Amp(ip)·δ*k*^I = Amp(op)·δ*k*^R | everything independent the tables contain — *H*_f is recoverable from either amplitude column and this is the check that the two agree about it | a transcription error preserving the ratio |
| `PERT-A-025`, (6.9) against the tabulated δ*k*_f | **the δ*k*_f column itself**, by an independent route the production path does not use | the *H*_f implicit in the amplitudes |
| `PERT-A-001`, the θ_f = 0 evaluation | the wiring: swapped columns, wrong η_m, a sign, a mis-parse, a dropped constituent | a wrong amplitude |

`PERT-A-025` is the manager's ruling on `PERT-Q-003` and is a better answer than the one the
specification proposed: the resonance formula (6.9) with Table 6.4's parameters, **as a test-only
cross-check**, is the only thing that turns δ*k*_f from a transcription into a verified quantity
without an *H*_f catalogue this tree does not have.

The other two amendments were smaller and both were found by the manager reproducing the numbers
rather than reading the prose: (6.23a) is written for the wobble in **radians** and §6.5's printed
result for the wobble in **arcseconds**, two equations on one page with 206 264.8 between them;
and §8's denominator list was missing its own most important entry.

### 15.7 `de440t` substituted, verified, and the test it exists for now runs

Ruled at `PERT-Q-002`: substitute rather than add, **provided** de440t carries the same planetary
segments over the same span. Verified by reading both DAFs' segment summaries directly rather
than by trusting a filename:

| | |
|---|---|
| `de440.bsp` | 14 segments |
| `de440t.bsp` | 15 segments |
| in `de440.bsp` only | **none** |
| in `de440t.bsp` only | target 1000000001 about centre 1000000000 — the TT−TDB record |
| shared segments differing in frame, type or span | **0 of 14** |

**Note the host.** `de440t.bsp` is published by **JPL SSD**, not by NAIF: NAIF's
`generic_kernels/spk/planets/` carries de440, de440s, de441 and de442 and no de440t at all, and
the obvious URL there is a 404. The manifest entry says so.

**Plan §5 constraint 9** — changing a manifest entry re-runs every gate that consumed it, in the
step that changes it — so step 1's full sweep re-ran against the substituted kernel:
**11 354 of 13 201 checked, 1 847 not a body, 0 outside coverage, worst residual
7.105 43 × 10⁻¹⁵ AU = 1.062 96 mm.** Identical to de440.bsp's, digit for digit.

And the thing the substitution was for: **`EPH-A-007` now runs.** It could not before — the short
kernel carries no TT−TDB record, so its loop executed zero times, warned, and passed. *A test that
passes by not running* is the shape this tree keeps designing out, and this one had been sitting
there since step 1. On de440t: **53 epochs over a year, worst |kernel − series| = 26.027 ns
against a 100 ns budget.** The escape hatch is gone; the test now requires the comparison to
happen.

### 15.8 Two tools improved, and the first found three stale rows in adopted specifications

**`tools/speccheck.py` printed three numbers that read as a partition and were not.** Following
the manager's rule from §15.5 — *when a count is written next to its own breakdown, add the
breakdown* — the components were added up and did not reach the denominator. Two causes:

- `excused` matched **any** identifier mentioned anywhere in the §8 Coverage region, so an excuse
  reading *"discharged by `PERT-A-016`"* excused whatever it named in passing. It is now the
  **first cell of a Coverage row** only.
- An identifier can legitimately be **partly** excused — a requirement with three clauses of
  which one is structural. That is now an explicit `(partial)` marker, counted as tested and
  reported separately, rather than looking like a contradiction.

The checker now asserts `tested + excused − both + uncovered == requirements`, so the components
cannot drift from the total again. **On its first run it found three contradictions**, in three
different specifications and two of them adopted at L1: `EOP-R-009` and `TIME-R-010` were genuine
partial excuses now marked as such, and `PERT-R-022` was a stale Coverage row for something
`PERT-A-026` had started testing an hour earlier. None changed a verdict, which is exactly why
they survived: nobody adds up the components of a passing gate.

**`tools/fetch.py` sniffs before it hashes.** `content/chapter10/icc10.pdf` is a 404 — chapter 10
is `tn36_c10.pdf` where every other chapter is `iccN.pdf` — and `curl` saves the error page with
exit status 0. The SHA-256 catches that on the **second** fetch and not the first, and the first
fetch is the one whose hash goes into the manifest. The first block of every download is now
checked against two rules: anything beginning as an HTML document is refused whatever it was
supposed to be, because no input this tree declares is HTML; and the magic is checked against the
declared extension where there is one (`%PDF`, gzip, zip, `DAF/SPK`). Nine cases in
`tests/test_fetch.py`, six refusals and three acceptances plus one extension the check does not
second-guess.

## 16. L2 `environment` step 3 — `perturbations`: implemented

Three link targets per the manager's `PERT-Q-001` ruling — `tides`, `relativity`, `thirdbody` —
built and gated on 2026-09-18. **10 gates green offline, 192 tests, 0 failures, 626 artefacts
byte-identical.** `SPEC-perturbations` is at **v1.2**; six corrections, all found by a test.

### 16.1 The gate, with both of its denominators

| | |
|---|---|
| `PERT-A-001`, Tables 6.5a and 6.5c at θ_f = 0 | **50 constituents, worst residual 0** — exact |
| `PERT-A-003`, Table 6.5b through (6.8a) | 21 constituents |
| `PERT-A-002`, the internal relation | **14 of 48** diurnal rows constrain it and 34 are lost to the printed precision; **18 of 21** zonal. Worst residual 0.83 and 0.79 of its rounding bound |
| `PERT-A-005`, the solid Earth pole tide | −1.333 24 × 10⁻⁹ per arcsec against §6.4's printed −1.333 × 10⁻⁹ |
| `PERT-A-006`, the ocean pole tide | −2.177 81 × 10⁻¹⁰ (*m*₁ − 0.017 24 *m*₂) and −1.723 16 × 10⁻¹⁰ (*m*₂ − 0.033 65 *m*₁) against (6.24), every printed digit |
| `PERT-A-011`, relativity as precession | de Sitter **19.188 mas/yr**, independent of height to 10⁻¹²; Lense–Thirring **0.7548 mas/yr** at GEO and **181.7 mas/yr** at 6778 km, against `TN36-10`'s 0.8 and 180 |
| `PERT-A-027`, the two argument conventions | all **71** constituents agree to 10⁻⁹ rad |
| `PERT-A-028`, the GM file against `TN36-1` | the Sun's differs by **1.5505 × 10⁻⁸ = *L*_B to six digits** |

**What the Conventions print no expected value for** is reported on every run beside the count
that passed: step 1's time-domain evaluation at any epoch, the ocean tide sum at any epoch, the
ocean pole tide above degree 2, the relativistic correction as a vector, third-body attraction —
which the Conventions do not treat at all — and step 2's amplitudes as anything other than their
own input.

### 16.2 The tables, and three column orders

Tables 6.5a, 6.5b and 6.5c were extracted from the hash-pinned chapter by
`tools/tides_from_conventions.py --ch6` and committed as generated source (`PERT-R-014`).
**They do not share a shape**, and the differences are not cosmetic:

| | column order | δ*k* units |
|---|---|---|
| 6.5a, diurnal, 48 rows | … δ*k*^R δ*k*^I ip op | **10⁻⁵** |
| 6.5b, zonal, 21 rows | … δ*k*^R **ip** δ*k*^I **op** — interleaved | absolute |
| 6.5c, semidiurnal, 2 rows | … δ*k*^R ip — no imaginary column | absolute |

A parser assuming one shape would read 6.5b's in-phase amplitude as an imaginary Love number and
be wrong by five orders of magnitude **without failing**. The generator asserts each table's row
count rather than reporting it, and the counts were read off the PDF rather than guessed: the
first attempt expected 71 for Table 6.5a, which is the total across all three.

Reconstructing *H*_f from the printed amplitudes gives 0.3687, −0.2620 and −0.1220 m for K1, O1
and P1, within 0.1 % of their Cartwright–Tayler values. That is a diagnostic and **not a test**:
`CT71`/`CE73` were not obtained, so those comparison values have no citable source in this tree,
and `PERT-Q-004` stays open for exactly that reason.

### 16.3 `PERT-A-025` checks less than the ruling that created it assumed

The manager's `PERT-Q-003` ruling was that (6.9) with Table 6.4's parameters verifies the
tabulated δ*k*_f column. It cannot, and the reason is in the Conventions' own definition: the
text below (6.8e) defines δ*k*_f as *k*_f − *k*₂₁ **plus a contribution from ocean loading**, and
§6.2.1 says the load-resonance corrections are *"incorporated through equivalent corrections to
the body tide Love numbers … also included in the tables"*. Measured, (6.9) is **3.4 % low on the
real part and wrong by a factor of two on the imaginary one**.

What it does check is the **resonance structure**, and that is worth having. Because the loading
contribution shares the same denominators, the ratio of printed to formula must be constant:

| | |
|---|---|
| δ*k*_f's own range across the band | a factor of **2955** |
| printed ÷ formula, real part | median **1.0344**, spread **2.98 %** |
| the same with σ₂ moved from 1.002 318 1 to 1.003 000 0 | spread **654 %** |

The near-constant 3.4 % offset *is* the ocean-loading contribution. The negative control is what
makes the check a check.

### 16.4 Two statistics that were not what their names said

**The ocean pole tide's variance fraction.** `TN36-6` §6.5 says degree 2 provides *"approximately
90 % of the variance of the ocean pole tide potential"* and degree 10 approximately 99 %. The
first implementation reported the **raw coefficient** variance — 75.8 % and 92.7 % — under the
same word. Weighted by *R*ₙ², which is what turns coefficients into potential, it is **90.55 %**
and **99.79 %**, matching the Conventions. Both are now exposed with their formulas in their
names, as `SPEC-template.md` §8 requires of a statistic.

**The ocean tide truncation criterion.** The manager's `PERT-Q-007` ruling was *the truncation
error is below the smallest term this module computes and keeps, at the same evaluation point*.
Read as the smallest per-degree term among those **kept**, it is self-referential: keeping more
degrees lowers the bar, and the criterion chased itself to degree 99 and said "keep everything".
The fixed threshold is `TN36-6` §6.2.1's own cutoff for what the solid Earth tide includes,
3 × 10⁻¹² in C̄₄ₘ — 8.552 × 10⁻¹¹ m s⁻² at 7331 km. With that:

| radius | degree meeting the criterion |
|---|---|
| 7331 km | **36** |
| 300 km altitude | **89** |
| **the default, the larger of the two** | **89** |

No number was in the specification before the measurement, which is what the ruling required.

### 16.5 A parser that dropped eight of eighteen waves without failing

FES2004's long-period waves carry **five-digit** Doodson codes — `55.565` where a diurnal wave is
`165.555` — because their first multiplier is zero and the file does not print the leading digit.
A parser demanding six digits silently dropped **7 952 of 59 462 rows and 8 of the 18 waves**,
including every long-period constituent. Nothing failed: the sum was simply short. It was caught
by the row count, which the loader reports and now refuses below a floor.

### 16.6 What implementation found in two adopted specifications

**`Ephemeris::state` returns `State<Frame::BCRS>` whatever centre is asked for.** Asked for the
Moon about the Earth it returns a geocentric vector typed as barycentric. The *frame* is in the
type, which `FRAME-R-004` requires, but the **origin** is a runtime argument the type does not
carry, so the tag can say something false about the vector. `PERT-R-053` had assumed the
barycentric route; `thirdbody` now asks for Earth-centred vectors in one call — which is also
three digits more accurate than differencing two 1.5 × 10⁸ km vectors — and `PERT-Q-010` puts
the type question to the manager rather than working around it silently.

**The fundamental arguments lived in a private header.** `PERT-R-015` said they come from
`frames`; the tree had them in `eop`'s `src/`, where Tables 5.1 and 8.2/8.3 already use them.
They are promoted to `eop`'s public surface rather than copied, on the `GRAV-R-029` argument, and
`PERT-A-027` checks the Doodson and Delaunay conventions against each other on all 71
constituents of Tables 6.5a/b/c — which print both, so the conversion is checked against data
rather than asserted from a textbook.

### 16.7 What the three modules measure

**Relativity.** Schwarzschild is 1.81 × 10⁻⁹ of the main acceleration at 7331 km and
3.16 × 10⁻¹⁰ at geostationary; Lense–Thirring 2.63 × 10⁻¹¹ and 1.91 × 10⁻¹²; de Sitter
4.57 × 10⁻¹² and 6.30 × 10⁻¹¹ — each inside the bands `TN36-10` §10.3 states, and with the
crossover it describes: Lense–Thirring exceeds de Sitter below Lageos and not above.

**Third body.** Dropping the indirect term makes the Moon's contribution **28.5×** too large and
the Sun's **19 256×** too large. Battin's rearrangement, derived in the source rather than cited,
is better than the written form by 7.5× at the Moon, 183× at the Sun and **1063× at Jupiter** —
measured against the same expression in `long double`, so what it measures is the cancellation
and nothing else.

*And a claim of the author's that the test refused.* The first version asserted that Venus always
contributes more than the static field's degree-90 truncation error. At JD 2458849.5 it
contributes 5.3 × 10⁻¹³ m s⁻², below it; near closest approach it is about a hundred times larger
and above it. A planetary term is a function of the configuration, and the test now measures
rather than asserts a size.

### 16.8 `--warn NoAssertions`, proven rather than added

The manager's one CI item for this step. Every Catch2 binary now runs with it, so a test case
that executes no assertion **fails** rather than passing quietly. Proven by injection: an empty
test case exits 42 with *"No assertions in test case"*. It is a backstop and not a substitute for
plan §4 rule 3 — it would **not** have caught `EPH-A-007`, which asserted things outside its
empty loop.

## 17. L2 step 3 reopened — an absence asserted without a search

**10 gates green, 195 tests, 0 failures, 626 artefacts byte-identical.**
`SPEC-perturbations` v1.3, `SPEC-frames` v1.7, `SPEC-ephemerides` v1.3.

### 17.1 Chapter 6 prints a worked example, and this tree said it did not

`SPEC-perturbations` v1.0–v1.2 stated, in §8 and in `PERT-Q-008`, that **chapter 6 prints no
worked examples**, and that *H*_f is *"an `H`_f the Conventions never print"*. Both are false.
§6.2.1 prints a **worked example for K1** fifteen lines below the definition of δ*k*_f that this
specification quoted — *A*₁ = −3.1274 × 10⁻⁸, *H*_f = 0.36870, θ_f = θ_g + π,
*k*₂₁⁽⁰⁾ = 0.257 46 + 0.001 18 *i*, the nominal subtracted to give δ*k*_f = −0.040 84 +
0.002 62 *i*, and the two expressions (6.8b) yields.

The error is not that the claim was wrong; it is that **an absence was asserted without a
search**, in a document whose §2 exists to record what was and was not obtained. `PROVENANCE.md`
§15.4 repeated it. The manager's plan rule 4 now requires a finding of absence to carry the
search that established it — the terms and the count — exactly as rule 3 requires a statistic to
carry its formula. *"The source does not print X"* earns the same scrutiny as *"the source prints
X = 1.333 × 10⁻⁹"*.

**`PERT-A-029` now uses it, and it is the most valuable row in §8.** Two halves:

| | |
|---|---|
| from the published inputs alone, using nothing from the module's own table | *A*₁ δ*k*_f *H*_f = (470.915, −30.2105) × 10⁻¹², landing on Table 6.5a's printed (470.9, −30.2) |
| the printed expressions at **eight values of θ_g**, and θ_f for K1 | worst residual **5.2 × 10⁻²⁶**; θ_f = θ_g + π exactly |

It is **non-circular**, and it is **the only check anywhere that exercises the θ dependence**.
`PERT-A-001` evaluates at θ_f = 0, where a wrong sign on θ_g, a missing π or fundamental
arguments off by a constant all survive untouched.

### 17.2 The Conventions predicted the ocean-tide offset, in words, and the prediction checks

§6.2.1, below Table 6.5a's defining equation: *"Roughly half the value of the imaginary part
comes from the ocean tide term, and the real part contribution from this term is of about the
same magnitude."* Over the 24 constituents away from the FCN resonance:

| | measured | the source says |
|---|---|---|
| \|resid_R\| / \|resid_I\| | **0.906** | "about the same magnitude" |
| resid_I / δ*k*^I | **0.598** | "roughly half the value of the imaginary part" |

So the identification of the 3.4 % offset as δ*k*^OT stops being an inference and becomes a
measurement the source predicted.

### 17.3 ψ1, and a statistic that was blind rather than tolerant

The manager asked whether the median absorbed ψ1 — the constituent §6.2.1 singles out with a
resonance-formula correction of (244, 299) in units of 10⁻⁵, two orders above every other,
because it sits on the free core nutation resonance.

**It did not, and the truth is worse.** ψ1's real-part ratio is **1.0347 against a median of
1.0344 — rank 28 of 48**, 0.04 % away. It is not an outlier in that statistic at all. What the
real-part ratio did was **never look at the imaginary part**, where ψ1's residual is −252 against
a prediction of about +180: |resid_R|/|resid_I| = 3.03 where everything else is near 0.9, and
resid_I/δ*k*^I = −0.70 where the Conventions' "roughly half" predicts +0.5.

A robust statistic that absorbs an outlier is one failure; a statistic that never examines the
dimension the outlier lives in is a different one, and the second is harder to notice because the
first at least leaves a tail. Both directions are now asserted so neither can be forgotten.

**And `PERT-A-025` cannot be tightened to the printed corrections** (`PERT-Q-011`): §6.2.1 prints
their **magnitudes and not their signs**, and does not tabulate δ*k*^OT separately, so
"formula + printed correction = table" is not evaluable. Recorded rather than attempted.

### 17.4 The truncation's cost and its asymmetry

The ruling asked for both. Measured: **0.137 ms per evaluation at degree 36 and 0.541 ms at
degree 89, a factor of 3.94** — less than the quadratic estimate because `FES2004-CS` is sparse at
high degree. And the asymmetry, stated rather than left to be found: this floor is
8.552 × 10⁻¹¹ m s⁻², and this layer keeps relativistic terms three orders below it. Cheap
closed-form terms kept below a floor at which an expensive quadratic-cost series is truncated is
defensible engineering and indefensible if discovered later.

### 17.5 `PERT-Q-010` ruled, and *L*_B was wrong by a factor of a thousand

The ruling: the centre stops being a runtime argument on a frame-tagged return.
`barycentric_state` returns `State<Frame::BCRS>`, `geocentric_state` returns
`State<Frame::GCRS>` **and is** `FRAME-R-028`'s translation for an ephemeris body, and
`relative_state` returns an **untagged** `RelativeState` for any other centre. Option (c) —
redefining the tag to mean axes and not origin — was rejected explicitly, because *a tag that can
say something false about the value it labels is worse than no tag*. Plan §5 constraint 10.

The ruling also required the translation's omissions stated **with their arithmetic** rather than
asserted from memory, and doing that found something:

| | |
|---|---|
| `SPEC-frames.md` §3.5 and `FRAME-Q-006`, v1.0 to v1.6 | *L*_B is "2.3 m on an astronomical unit" |
| the arithmetic | 1.550 519 768 × 10⁻⁸ × 1.495 978 707 × 10¹¹ m = **2 319.5 m = 2.32 km** |

**A factor of a thousand, in an adopted specification, from v1.0.** It is the fifth instance of
the family that prompted `tools/budgetcheck.py` and **the first found outside a budget row** — it
sat in prose, where gate 8 does not reach. It is now `FRAME-P-7`, a budget row, so that the
checker evaluates it; that is the repair, and correcting the number alone would not have been.

And the answer the ruling wanted: on a geocentric vector the unapplied scaling is **5.960 m** at
the Moon's distance and **5.070 × 10⁻¹⁴ m s⁻²** as a third-body acceleration —
**5.9 × 10⁻⁴ of the smallest term L2 step 3 keeps**. It does not land above that floor, and
`EPH-P-5` now carries the arithmetic.

### 17.6 A guard that could not fire

The manager's stale-configure finding — their first check reported "165 of 165" from a build
directory that predated three modules — became a check in `tools/ci.sh`, placed just before
ctest. **It could never fire**: `ci.sh` configures at gate 3 and tests at gate 5, so by the time
the check ran the build system was always newer than the CMakeLists. Dead code that reads like
protection, and it passed its own first run, which is how it nearly stayed.

It is a **test** now, `build.configure_is_current`, because the failure mode is somebody running
`ctest` or a single binary against an old build directory and the only place to catch that is
inside the suite. Proven both ways: it passes after a configure and fails after touching a
`CMakeLists.txt`.

## 18. Two corrections of reasons rather than conclusions

**10 gates green, 196 tests, 628 artefacts byte-identical.** `SPEC-perturbations` v1.4,
`SPEC-ephemerides` v1.4.

### 18.1 The printed corrections are signed, and they measure what was said to be unmeasurable

`PERT-Q-011` at v1.3 said `PERT-A-025` could not be tightened to `TN36-6` §6.2.1's per-constituent
resonance corrections because they are printed *"as magnitudes and not signs"*. **They are
signed**: P1's is printed **(0, −1)**, and one minus sign settles that the unsigned entries are
positive values rather than magnitudes.

What actually blocks *formula + correction = table* is only that δ*k*^OT is not tabulated — and
that blocks far less than was concluded, because **subtracting the correction measures δ*k*^OT
per constituent**, against words the Conventions print for exactly that quantity. Over the nine
corrected constituents other than ψ1:

| | measured | `TN36-6` §6.2.1 |
|---|---|---|
| δ*k*^OT_I / δ*k*^I | **0.467 to 0.616** | "roughly half the value of the imaginary part" |
| \|δ*k*^OT_R\| / \|δ*k*^OT_I\|, median | **1.002** | "about the same magnitude" |

**The application direction is determined rather than chosen, and by the spread rather than by
one constituent.** The source's natural reading — the formula plus the correction gives the exact
body-tide value — holds the nine inside [0.467, 0.616]; the other direction scatters the same
nine over [0.181, 0.944], five times the range, and gives ψ1 a magnitude ratio of 21.5.

**And ψ1 remains anomalous after the published correction is applied.** Its δ*k*^OT_I/δ*k*^I is
**−1.539** — the wrong sign, and larger than the quantity — while its magnitude relation holds at
0.943. The published correction is 299 against δ*k*^I = 358, **83.5 % of the quantity**, and
applying it does not explain the constituent. That is a far stronger statement than "ψ1 is an
outlier in a statistic I chose", and it is the one the manager predicted might come out.

*It is also a fourth instance of the pattern named in §17.3, inside the analysis that named it:
the real-part ratio never looked at the imaginary part; the imaginary-part analysis then never
looked at the printed correction that is 83.5 % of the imaginary part.* Each new tool needs the
treatment the last one got.

### 18.2 Three numbers, three specifications, three reference points

`EPH-P-5` said the unapplied *L*_B scaling is 5.9 × 10⁻⁴ of *"the smallest term L2 step 3 keeps"*
— but the number it divided by was the **ocean-tide truncation floor**. `PERT-R-022a` said the
same layer keeps relativistic terms **three orders of magnitude below** that floor. Both could
not be true, and neither was right: the "three orders" compared a de Sitter term at
**geostationary** with a truncation floor at **LEO**.

Measured at one radius, from the modules, by `tests/l2_floors.cpp`:

| at *r* = 7331 km | |
|---|---|
| ocean-tide truncation floor (`TN36-6` §6.2.1's 3 × 10⁻¹² cutoff) | **8.552 × 10⁻¹¹ m s⁻²** |
| smallest relativistic term this layer computes (de Sitter) | **3.478 × 10⁻¹¹ m s⁻²** |
| unapplied *L*_B scaling, as a third-body acceleration | **5.070 × 10⁻¹⁴ m s⁻²** |

The floor is **2.46×** the smallest term kept — a factor of a few, not three orders and not the
twelve a reading of `TN36-10`'s stated band suggests — and the *L*_B effect is **0.146 %** of it.
The conclusion is unchanged; what changed is that it now rests on numbers that mean what they
say. The test links three modules on purpose: the error survived because the three numbers lived
in three specifications, each stated against its own reference point, so the comparison had to be
reconstructed rather than read.

---

## 19. L2 `environment` step 4 — `atmosphere`: the specification, and a gate that does not exist

**Date.** 2026-09-18. **Artefact.** `spec/SPEC-atmosphere.md` v1.0, draft for review.
Spec ID `ATMO`; 71 own-prefix identifiers; 41 requirements and refusals, 40 discharged by an
acceptance row and 1 excused; 0 uncovered.

### 19.1 The finding of absence, with the search that established it

Plan §3.3 step 4 gates this step on *"the model's published reference profiles"*; the L2 exit
gate on *"every model reproduces its published reference values"*; plan D2 on validation
*"on its packaged tests"*. Applying plan §4 rule 4 before designing anything:

**NRL publishes no reference profile, no reference value, and no expected output of any kind
for NRLMSISE-00.** The search, recorded here because rule 4 requires a finding of absence to
carry the search that established it:

`https://map.nrl.navy.mil/map/pub/nrl/NRLMSIS/NRLMSISE-00/` (HTTP 200, 2026-09-18) offers five
files. **All five were downloaded and all five were read**:

| file | bytes | SHA-256 | what it is |
|---|---|---|---|
| `NRLMSISE-00.FOR` | 114 981 | `cce0420e90781c256bc6705c4cc8056b054812d6308adccb7081bf09af0d44cb` | the model, 2437 lines, plus a test driver at 2438–2552 |
| `…-datavsmodels.txt` | 42 167 | `7946e4cb403935bbd7b11fb3647364c2c1a71384f71db41ec8c31a0b12bcb8aa` | 27 tables of data-minus-model statistics |
| `…-readme.txt` | 3 845 | `3a6e3d6ff89985d13fdbff4c348d9161bf10f50fcbba8f6d09b501b2625058dc` | the formulas for those statistics |
| `…_tables-datasets.doc` | 116 224 | `abe2b44c25706d3ab35acf82793a4b08851317e3894083664103b8df11124771` | the same 27 tables, Tables 1(a)…9(c) |
| `…_jgra16630.pdf` | 425 444 | `2ef966682587436ea5fba46180c82ab163bd92c18a864f1aa1dc6881b3839b38` | Picone et al. (2002) |

Counts in `NRLMSISE-00.FOR`, whole file: `PROGRAM` 0, `SUBROUTINE` 22, `GTD7` 19, `TEST` 10,
`EXPECTED` 0, `RANGE` 0, `LIMIT` 0, `PRINT` 0. Counts **after line 2438**, the driver's first
line: `OUTPUT` 0, `RESULT` 0, `SAMPLE` 0, `COMPARE` 0, `EXPECTED` 0, and scientific-notation
literals `E+1[0-9]` 0 — the driver's only numeric literals are its input `DATA` statements and
its `FORMAT` widths. In the paper (827 lines of extracted text): `Table [0-9]` 2, both referring
to the archive statistics; `reference profile` 0, `test case` 0, `sample output` 0,
`electronic supplement` 0, `auxiliary material` 0, `TINF` 0; `Figure` 26.

**The shape of it:** *the distribution publishes inputs without outputs (a driver with 17 fully
specified input cases and no expected output) and outputs without inputs (27 tables of
statistics against a database NRL does not distribute). Neither is a check, and no third thing
exists.* The manager's own guess — "a test driver with its own expected output" — was half
right and the wrong half; the answer to "which of the two do you have" is neither.

Three plan sentences are therefore wrong about their source. `SPEC-atmosphere.md` §0.5 proposes
replacements and `ATMO-Q-001` puts them to the manager, a plan change not being the executor's.

### 19.2 What stands in their place, and what that costs

NRLMSISE-00 is not defined by a published theory: `PICONE02` describes the fit and does not
define the function, which is ~1500 coefficients plus the code that combines them, both living
only in `NRLMSISE-00.FOR`. The reference implementation therefore **is** the model, as EGM2008's
coefficient file is the field, and comparing against it is comparing against the definition —
not the oracle comparison plan §4 rule 2 ranks last. `SPEC-atmosphere.md` §8 argues that reading
and states its cost: **this tree cannot check that the FORTRAN computes what the paper
describes**, and an error in the reference would be reproduced here and by every other user of
the model, consistently and invisibly.

### 19.3 Measurements taken during drafting

The reference was compiled (`gfortran-16` 16.0.1, `-std=legacy -fdec-char-conversions -O0`;
**unedited** — the flag exists because three `DATA` statements at lines 1670–1671 use the
FORTRAN 66 Hollerith idiom for the output header stamp `MSISE-00  01-FEB-02  15:49:27`, which
touches no arithmetic) and run over its own 17 published input cases (15 from the `DO` loop + 2
with the 7-element Ap and `SW(9) = −1`; 17 blocks confirmed in the run).

* **The reference is single precision throughout** — `DOUBLE PRECISION` 0, `REAL*8` 0,
  `IMPLICIT` 0, `.D0` 0. Its own value is therefore uncertain, and by how much was measured
  rather than guessed: the same source built again with `-freal-4-real-8`, compared over
  17 × 12 = 204 quantities, of which 20 have a zero denominator, leaving **184**. Median
  relative difference **3.816 × 10⁻⁷**, worst **7.671 × 10⁻⁶** (argon, 1000 km).
* **Anomalous oxygen at 100 km underflows in the reference and not in a double port**: single
  returns exactly 0, double returns 2.820 × 10⁻⁴² and 2.415 × 10⁻⁴² cm⁻³ (cases 4 and 17),
  against a smallest single normal of 1.175 × 10⁻³⁸. **2 of the 184 comparisons.** A correct
  double-precision port disagrees with the reference by 100 % at two published cases and is
  right to. This is the third appearance in this tree of *a quantity that exists in one
  precision and not another* — after `SPEC-gravity` §3.6a and the step-3 gate statistic — and
  the defence is again to count the classes rather than to widen a tolerance across them.
* **The documented zeros hold**: O, H, N and anomalous O are exactly zero below 72.5 km,
  5 cases × 4 quantities = **20 values** — which are exactly the 20 zero denominators above.
* **The documented total-density relation holds and is an independent check**: ρ computed from
  the species rather than copied from the reference's expression agrees to **3.280 × 10⁻¹⁶**
  (`GTD7`) and **3.871 × 10⁻¹⁶** (`GTD7D`) in double.
* **Mass selector 49 is accepted by the reference, behaves differently from 48 (it counts O₂
  twice, as oxygen atoms, at line 769) and is documented in zero comment lines.** `ATMO-Q-002`.
* **Three of the model's six `WRITE` statements print a diagnostic and continue** — a coincident
  spline node (1539), a non-positive density ratio before a logarithm (1591), and `GHP7`'s
  convergence trace (445). The caller gets a number computed after a condition the model itself
  called an error. `ATMO-R-030` makes each a refusal.

### 19.4 Space weather: measured, then policed

`CELESTRAK-SW` (`SW-All.csv`, 2 887 903 bytes, 2026-09-18 snapshot) is the tree's first
non-frozen input. It moves in three ways a hash cannot distinguish — extension, **revision** of
days already present, and **fifteen years of embedded forecast**: 25 413 rows to 2041-10-01,
classed `OBS` 25 129 + `INT` 60 + `PRD` 45 + `PRM` 179 = 25 413.

Recomputing the centred 81-day mean from `F10.7_OBS` and comparing against the file's own
`F10.7_OBS_CENTER81`, over the 25 333 rows with a full ±40-day window: **176 rows disagree by
more than 0.05 sfu (0.69 %), of which `PRD` 38 and `PRM` 138 and `OBS`/`INT` zero**; worst
**30.07 sfu** at 2035-01-01. The file's own column is exact on observation and arbitrary on
forecast, and nothing in the file says so.

Coverage is also **per quantity**: F10.7 runs to 2041-10-01 and Ap only to 2026-11-01 — fifteen
years apart in one file — so the usable end is Ap's, shrunk by 40 days for the centred mean:
**2026-09-22**.

The policy (`SPEC-atmosphere.md` §4.5): pin the snapshot by hash like every other input, never
fetch at run time, refuse outside usable coverage naming the quantity that ran out, refuse
predicted rows unless asked for by name, recompute the centred mean rather than read it, and
carry the snapshot's identity into every result — because revision means two results from
different snapshots are not comparable *even at an epoch both cover*. Updating is a manifest
change, and plan §5 constraint 9 already makes a manifest change re-run every gate that consumed
the entry. That is what lets a moving input coexist with a reproducible gate.

### 19.5 Verification of this entry

Every measured number above and in `SPEC-atmosphere.md` was **re-derived from the data after the
document was written**, not transcribed from the working notes: 26 checks, 26 agreeing. The
checkers pass — `speccheck.py` reports 0 uncovered for `ATMO`, `budgetcheck.py` 42 rows with 23
carrying arithmetic, all evaluating to what they state.

## 20. L2 step 4 — `atmosphere`: the port, and what building it found

**Date.** 2026-09-18. **State.** The port reproduces the reference implementation to
**2.33 × 10⁻¹⁶** — about one ulp — over **1238 material comparisons** across 125 cases
(17 published + a 108-point sweep crossing every branch boundary). 203/203 tests pass.

### 20.1 The two generated sources

`msis_coefficients.hpp` — 3 300 coefficients extracted from the pinned FORTRAN, never
transcribed. **The storage is COMMON aliasing**: `BLOCK DATA GTD7BK` fills sixty-four
50-element arrays `PT1`…`PAA2`, and every subroutine reads that same 3 200-double block as
`pt(150)`, `pd(150,9)`, `ps(150)`, `pdl(25,2)`, `ptl(100,4)`, `pma(100,10)`, `sam(100)`. The
boundaries do not fall where the letters do — `pd` consumes letters A through I, so `ps` begins
at `PJ1` — and an extractor trusting the DATA names would emit nine plausible arrays the model
never indexes. Verified value by value: 3 300 compared, 1 280 non-zero, **zero mismatches**.

`msis_reference_values.hpp` — the frozen oracle, with compiler, version, flags and source hash
in its header, and the single-versus-double measurement as the **derivation of the tolerance**
rather than a number asserted elsewhere. The gate runs offline with no Fortran compiler;
`gfortran` is a host tool, not a manifest pin.

### 20.2 What the sweep found, which the 17 published cases could not

Over the published cases the reference's single-precision artefact looks like one number: median
≈ 4 × 10⁻⁷, worst 7.7 × 10⁻⁶. Over the sweep the worst is **7.9 × 10⁻³** — a thousand times
larger — and every one of those is a quantity of order 10⁻³⁰ to 10⁻³⁷ approaching single
precision's smallest normal (1.18 × 10⁻³⁸).

**It is one phenomenon in three regimes**, not a tolerance with an exception. The class boundary
is therefore physical: a species whose mass contributes less than 10⁻¹⁵ of the total cannot
affect drag. Over 1500 comparisons: **A 1238** (worst 7.6706 × 10⁻⁶), **B 32**, **C 18**,
**Z 212**. The threshold is not tuned — class A's worst is unchanged from 10⁻¹² to 10⁻¹⁵ — and
**class A's worst is argon at 1000 km, published case 3**: 1056 further material comparisons
across every branch boundary left the bound exactly where the 17 cases put it.

**Correction (step 4's second closing review, 2026-09-23).** "Crossing every branch boundary" —
here, and in this section's own opening State line above — was FALSE when it was written, in the
same shape `SPEC-atmosphere`'s own later absence claim took: the 108-point sweep crossed every
REGIME §3.6 then described (the spline structure below 120 km), but never the seven species-
correction cutoffs `ATMO-R-037` later named, because nobody searching this module at the time knew
they existed — an unsearched absence, not a checked one. The inference this section drew from it —
that class A's worst staying at argon/1000 km/published case 3 across 1056 added comparisons
PROVED the 17 published cases were not a lucky subset — was consequently unsound, even though its
conclusion happened to be nearly right: actually crossing all seven cutoffs (`ATMO-A-028`, §28.10)
moved the worst by 2.8%, to 7.8881 × 10⁻⁶ at a sweep point (anomalous O, 240.01 km), not the large
revision a truly unsearched boundary might have hidden. Current figures are `SPEC-atmosphere`
§3.2/§3.3/§8's own; this section's numbers above are left as measured for the sweep that existed
when L2 closed, not rewritten to agree with the later one.

### 20.3 The one porting error, and the shape of it

Every species below 72.5 km came out high by **exactly the same factor** — 3.472 × 10⁻² at
0–50 km, 8.672 × 10⁻³ at 70 km. A single multiplicative error, therefore, in one shared term:
`DM28`, the mixed N₂ density. The reference passes it through `COMMON/DMIX`, set inside `GTS7`
early; the port recomputed it afterwards, by which time `DENSU` had written back into the
temperature-node arrays (`TN1`, `TGN1`) that every later species' call had further modified.
Captured at its source, the disagreement fell to 2.33 × 10⁻¹⁶.

*The diagnostic was the uniformity.* Every species and the total sharing one factor says the
error is in something they all multiply, which is a much smaller search than "the port is wrong
somewhere below 72.5 km".

### 20.4 Three guards this step changed, all in the same direction

**The licence allowlist refused the first design, and was right.** `gfz-kp-ap-f107` is CC BY 4.0
except its sunspot column, which is CC BY-NC 4.0. The first attempt declared the compound
licence `CC-BY-4.0 AND CC-BY-NC-4.0` — which would have put a **non-commercial term on the
permissive allowlist**, the exact thing that allowlist exists to prevent. An entry now declares
the licence **of what this tree consumes**, and names any other licence in the file under
`licence_excluded` with the columns carrying it. `tools/fetch.py` refuses an entry that excludes
a licence without declaring its columns, and refuses one that declares a column carrying an
excluded licence. Proven both ways.

**`tests/test_one_secular_pole.py` failed on a false positive** — `msis_thermosphere.hpp` carries
`55.0` as NRLMSISE-00's ZN2 mesospheric spline node, 55 km. The test substring-matched bare
numbers, and its own comment already recorded this happening once before (`relativity_tests.cpp`,
55.0 as an inclination); the fix then was to exclude the test directory, which was too narrow.
TN36-7 (21) is two linear expressions in four constants and **no file can restate one with a
single number**, so one of the four is a coincidence and **two or more together is the equation
copied**. The test now reports which files carry which constants and flags only files carrying
two or more. Proven both ways: exit 1 on a planted restatement naming all four it found.

**Three print-and-continue conditions became refusals** (`ATMO-F-012`): the coincident spline
node, the non-positive density ratio before a logarithm, and `GHP7`'s non-convergence. The
reference prints to standard output and returns a number anyway.

### 20.5 Measured, not assumed, while porting

* **The reference mutates its own coefficient array** at line 1107 —
  `IF(P(25).LT.1.E-4) P(25)=1.E-4` — which would change every later call. It is **dead for all
  eleven arrays `GLOBE7` is called with**: 8.667840 × 10⁻² for `PT`, `PS` and seven `PD` columns
  (867× the clamp, 2.938 decades), 8.310900 × 10⁻² for column 1 (831×), and **5.382050 × 10⁻²
  for column 2 — 538×, 2.731 decades, which is the binding margin**. The coefficients are `const`
  in the port because the worst column was checked, not a typical one.

  *Both this session and the manager wrote "four decades" for that margin before either computed
  it; 8.667840 × 10⁻² is 867 times 10⁻⁴, which is 2.94 decades, and the binding value is 2.73.
  A decade count is arithmetic and §4 rule 3 applies to it like any other statistic.*
* **`GLOB7S` reads `PLG` and never writes it**, so its result depends on `GLOBE7` having been
  called first with the same latitude and nothing in its signature says so. The shared state is a
  parameter here.
* **`T2` and `T(2)` are different variables** in the anomalous-oxygen block. Reading one as the
  other overwrites the temperature at altitude.
* **The switches collapse.** With none exposed, `SW` is all +1 except `SW(9) = ±1` and `SWC` is
  all 1 — exactly the two configurations the published cases exercise.

### 20.6 The ingestion layer, and the row it feeds

`SpaceWeatherTable` loads the GFZ table and verifies its redistributed F10.7 against DRAO's own.
Measured by the suite, not asserted here: **7 969 of 7 969 overlapping days exact, zero
differing**, with the **16** duplicate-timestamp dates resolved first-wins (`ATMO-R-033`).
Usable coverage is a **set**: 22 991 days with **25 breaks** inside 1956-10-14 … 2026-08-08,
because 6 178 rows carry the absence marker. The centred 81-day mean is recomputed and the test
checks it by an **independent route** — civil-date arithmetic against the loader's index walk —
rather than comparing the loader with itself.

`tests/l2_floors.cpp` gains the atmosphere row, at **both** radii because drag is the only term
in that table varying over orders of magnitude across the regime. Measured: ρ = 1.753 × 10⁻¹¹
kg m⁻³ at 300 km and 2.125 × 10⁻¹⁵ at 953 km, a ratio of **8 248**; the drag acceleration at a
stated C_D A/m = 0.01 m² kg⁻¹ is **61 175×** the ocean-tide floor at 300 km and **6.76×** it at
953 km. The uncertainty travels with it, carrying its altitude, activity level and epoch, and
labelled as an upper bound (§20.7).

### 20.7 Four corrections from review, and what they have in common

* **"Four decades" was wrong for every value.** The clamp margin on P(25) is 867× (2.938
  decades) typically and **538× (2.731 decades) at the binding column**. Both this session and
  the manager wrote "four decades" before either computed it. A decade count is arithmetic and
  §4 rule 3 binds it like any other statistic — *and a margin is always the worst column's*,
  which is the rule `SPEC-gravity` §3.6a already established.
* **The secular-pole guard was being narrowed toward uselessness.** Twice it answered a false
  positive by lowering its own sensitivity — first excluding the test directory, then requiring
  two constants rather than one. Each step was locally right; the trend converges on a guard
  that fires at nothing. **The discriminator was replaced rather than narrowed a third time**:
  a copy of TN36-7 (21) puts an offset and its rate *adjacent*, a coincidence scatters them, so
  the test now asks about proximity. Proven three ways — adjacent restatement fires, two
  constants 60 lines apart do **not** (the previous rule would have), the tree passes.
* **The licence allowlist refused its own author** (§20.4), which is the only real test of one.
* **The three-model correlation supports the weaker claim.** σ_N00 agreeing with σ_M90 to 5 %
  rules out a σ that is mostly *model-specific*; it does **not** show σ is data-dominated, since
  σ² = σ_data² + σ_model² and similar σ_model terms need not be small. `ATMO-P-4` now states
  what the measurement reaches and stops there.

What the four share is the shape this project keeps meeting: **a number or a check that passes
without examining what it claims about.** A decade count nobody divided; a guard that stopped
looking; an allowlist that would have admitted the thing it exists to exclude; a correlation
asked to carry a conclusion one step past its own algebra.

### 20.8 The submission criterion, changed because it failed

**`tools/ci.sh` exited 1 when this work was reported complete**, at gate 6: `NOTICE` had drifted
from the manifest, because five new entries were added and `NOTICE` is generated from it. The
individual checkers were green and the composed gate was red, and the report said "every
checker green" — true of the checkers that were run, and the gate that composes them was not
one of them.

The irony is worth stating rather than passing over: plan §5 constraint 9 says changing a
manifest entry re-runs every gate that consumed it; `NOTICE` consumes the manifest; **gate 6 did
catch it.** Nothing enforced the constraint except running the gate, and the gate was the thing
not run.

**The criterion is now the composed one: green means `tools/ci.sh` exits 0**, and the report
states that exit code. A list of individually-passing checkers is not a substitute, because the
one that fails will be the one not on the list.

Fixed in passing, and it was a real defect rather than a formality: `tools/notice.py` did not
know about `licence_excluded` or `columns`, so `gfz-kp-ap-f107`'s note said its non-commercial
column was "named in licence_excluded below" and **nothing was below**. A dangling
cross-reference in the user-facing licence document, promising exactly the disclosure the
mechanism exists to make. `NOTICE` now carries an `EXCLUDED` line naming CC BY-NC 4.0 and the
`SN` column, a sentence saying a reader who fetches the file is subject to it whether or not
this tree reads it, and the declared column list.

### 20.9 `EPH-Q-005` resolved, and reading beat citing

IAU 2012 Resolution B2 is obtained and pinned (`iau2012-b2`, SHA-256 `3489ebb1…c984`), and
`SPEC-ephemerides` §2's row is **primary** rather than cited through `PARK21`. L2 can close.

**It is served by SYRTE (Observatoire de Paris), not by the IAU**, whose own published location
no longer answers — five locators probed on 2026-09-18, all 404, recorded in the manifest entry
and in the specification so the next reader need not repeat the search. That bounds it; it does
not establish that the IAU publishes no copy anywhere.

Reading the resolution rather than citing it added something the secondary route did not carry.
Recommendation 1 is the exact metre value, which `PARK21` already gave. **Recommendation 2 is
that the definition "be used with all time scales such as TCB, TDB, TCG, TT, etc."** — the au
carries *no* time-scale dependence. In a layer that has spent this much effort on *L*_B, and in
which `PERT-A-028` turns on the TDB/TCB rate being exactly what separates two published GM
values, having that stated by the resolution itself is worth the retrieval.

## 21. L3 step 1 — the force plugin surface, and a gate that would have caught nothing

**Date.** 2026-09-18. **State.** `tools/ci.sh` exits 0: **11 gates, 221 tests**.
`SPEC-dynamics.md` v1.1, Spec ID `DYN`: 50 own-prefix identifiers, 26 requirements and refusals,
24 discharged by a test, 2 excused, 0 uncovered.

### 21.1 The gate the plan asked for would have fired fourteen times and caught nothing

Plan §3.4 step 1 asked for a CI gate that *"fails on a km↔m scaling written anywhere outside
`core/units.hpp`"*. The wording names a check whose feasibility is a measurement, so it was
measured before being specified. **Every literal whose value is 1000 or 1/1000** in the tree's
production sources — 62 at the time, tests excluded:

> **15 occurrences in 7 files. One is the crossing. Fourteen are not.**

g/cm³ → kg/m³; `fmod(yrd, 1000.0)` extracting a day-of-year from `YYDDD`; two `−1000.0` "no
longitude" sentinels; eight milliarcsecond and millisecond conversions in `eop`; a mas → rad
constant in `gravity`; a row-count threshold in `tides`. **Fourteen false positives to one true
positive, and the true positive is `kMetresPerKilometre` — the definition the gate must permit.**

The enumeration was then **confirmed by a second, independent method**: first by matching the
spellings of the 1e3 family, then by parsing every numeric literal and keeping those whose
*value* is 1000 or 1/1000. Both return the same 15. A register's worth is its completeness, and
one enumeration checking itself is not evidence of that.

### 21.2 Why it was replaced rather than narrowed

Excluding `eop`, or requiring proximity to a position-like identifier, or searching only inside
`dynamics`, would each have worked that day. This tree had just watched that approach fail
twice in one layer, and the trend — not either instance — was the defect. The plan now carries
the general form at §4 rule 5 with all three instances: **a licence denylist that passed CeCILL
because it contained no forbidden substring; `test_one_secular_pole.py` narrowed twice; and
this.** *A search converges on flagging nothing; a register converges on accounting for
everything.*

`DYN-R-040`: every such literal is either in `core/units.hpp` or carries a marker on its own
line or the line above. `tools/unitcheck.py` prints the whole register and fails on one that is
neither — the **licence allowlist's shape**, where adding an entry is a deliberate act with a
reason attached. It is `ci.sh` **gate 10**.

### 21.3 Two things the work found that the specification had not

**The register needed two markers, not one**, and that came out of re-deriving the fourteen from
what each line does rather than from the enumeration that produced them. **Four of the fourteen
convert nothing at all** — a date radix, two sentinels and a row count — and labelling those as
conversions would have made the register say something false. A reader's use of it is to scan
and see that no km↔m crossing hides among the entries; *one wrong label turns "fourteen
unexamined literals" into "fourteen literals someone said were fine", which is worse than no
register.*

**An annotation is not a permit** (`DYN-R-041`). The first version of the gate passed an injected
km↔m scaling that was *honestly annotated* `// UNIT-CROSSING: km -> m`, because it only checked
that a marker was present. That is the arm that matters most, and it was found by proving the
gate in all four states rather than only the two the plan asked for. No conversion outside
`core/units.hpp` may name kilometres: if kilometres are involved it **is** the crossing, and the
crossing has one site.

**The re-derivation also traced one literal to a real asymmetry between two products.**
`eop/src/series.cpp:103` scales a limit, not a value: `SPEC-eop` §3.5 records that EOP 20 C04
publishes δ*X*/δ*Y* in **arcsec** where `finals2000A` publishes them in **milliarcsec**, so the
C04 parser converts `kCipMas`'s 100 mas into arcsec while the `finals2000A` parser compares the
same limit unscaled.

### 21.4 The arithmetic offered for the Jacobian split, re-derived rather than patched

`DYN-Q-002` asked whether ∂a/∂state should be one 3×6 or two named 3×3 blocks. The ruling was to
split it, supported by an argument that SRP's velocity dependence — aberration, ∂a/∂v ≈ a_SRP/*c*
— is the same order as drag's on the high-area-to-mass object this project is aimed at.

**The figures reproduce exactly and compare two different vehicles.** The SRP number uses
*A·C*_R/*m* = 3.36 m² kg⁻¹, a sail; the drag number, 5.78 × 10⁻¹⁰ m s⁻², comes from
`tests/l2_floors.cpp`, which states *C*_D·*A*/*m* = **0.01** m² kg⁻¹ — a compact satellite, ~340×
smaller in area to mass. Like for like, one object, **drag's velocity derivative at 953 km
exceeds SRP's by 1078×**, not 0.33×.

**And the measurement gives a better argument than the one it displaces.** "Same order at 953 km"
would have been a coincidence of two vehicles. What is true is a *regime*: SRP's term is
altitude-independent and drag's decays, so they cross, and **at GNSS altitude SRP's velocity
derivative is five times drag's** — exactly where L4 is aimed. `DYN-P-3` and `DYN-P-4` carry the
arithmetic, and gate 8 evaluates it.

**Gate 8 refused the rows before it accepted them**, twice and correctly: first for unknown units
(`N`, `kg` — `budgetcheck.py` had no mass dimension, and one was added rather than rewriting the
specification around the checker's vocabulary), then on dimensions, because *"per unit A/m"* was
carrying a factor the expression did not show. The normalisation is now written into the
arithmetic.

### 21.5 The surface

`modules/dynamics`. A parameter's identity is issued by a registry and **cannot be made from an
integer or a string**; `ParameterSet` has no `operator[]` and no `data()`; both are
**compile-time** assertions, because a runtime refusal would mean the expression existed. The
absences are asserted through concepts and **the same concepts are asserted positively on
`std::vector`**, so the assertions are known to be testing something.

∂a/∂state is two named 3×3 blocks, and a force declaring no velocity dependence supplies the
**bound** on what it neglects, which the set sums into the caller's budget. The km↔m crossing
happens at exactly **two sites**, both in `dynamics.cpp`, both naming `core/units.hpp` — and the
register confirms it structurally: adding the module took the search from 62 production sources
to 66 with **still 15 literals**, because the first caller of the crossing introduced none.

## 22. L3 step 2 — coefficients proved rather than trusted, and a prediction made before the run

**Date.** 2026-09-18. **State.** `tools/ci.sh` exits 0: **12 gates, 228 tests**.
`SPEC-integrators.md` v1.0, Spec ID `INTG`: 31 own-prefix identifiers, 15 requirements and
refusals, 13 discharged, 2 excused, 0 uncovered.

### 22.1 The source prints everything and OCR destroys all of it

Fehlberg, NASA TR R-287 (1968), pinned as `fehlberg-tr-r-287`, prints Table X (RK7(8)), the
truncation term (134), Example (53) with a closed-form solution, and Table XI's accumulated
errors. `pdftotext` renders Table X's β block as `83_ = 841 = B_I = 8sl = B71 = Be1 = B_l =
81ol`, and mangles the contents page — `TABLE IN.` for III, `8O` for 80. **The page images at
300 dpi are exact**, and the coefficients there are **rationals**.

So they were read by eye. That is acceptable **here and would not be elsewhere**, and the reason
is plan §4 rule 6's converse: a Runge–Kutta tableau has an independent specification — the order
conditions, exact algebraic identities over the rationals. *The question is not whether a 1968
scan can be read reliably; it is whether what was read is checkable.*

**The coefficients being rationals is what makes this work.** A source published as decimals
satisfies the order conditions only to rounding, and a transcription error in the last digits is
then indistinguishable from that rounding.

| check, exact rational arithmetic | result |
|---|---|
| row-sum consistency Σⱼβᵢⱼ = αᵢ | **13/13 rows** |
| *c* through order 7 | **85 conditions, 0 violated** |
| *ĉ* through order 8 | **200 conditions, 0 violated** |
| *c* at order 8 | **40 of 115 violated** |

**The last line is an independent cross-check that never touches the coefficient digits.** There
are **115** rooted trees of order 8 — exactly the range of Fehlberg's *T*ᵥ — and Section XV says
in prose, on a different page from the table, that RK7(8) has *"only **40** non-zero error
coefficients"*. Prose on one page and mathematics applied to a table on another agree.

**The two checks are orthogonal and `SPEC-integrators` §3.3 says which establishes what.** The
order conditions prove the tableau is *a valid RK7(8) pair*; they cannot prove it is *Fehlberg's*,
because his derivation has free parameters that were chosen rather than forced. Table XI is the
only check of that claim.

### 22.2 The prediction, written before the first run, and what happened

`INTG-P-2` was written into the specification **before the integrator was run**, so the tolerance
could not be chosen from the result — the defect already ruled on when a number went into a
specification and was measured afterwards.

| | predicted, in advance | measured |
|---|---|---|
| order of magnitude | **10⁻¹⁴, within ×10** — the gate | Δ*y* = −1.465 × 10⁻¹⁴, Δ*z* = −1.976 × 10⁻¹⁴ against Fehlberg's −2.509 × 10⁻¹⁴ and −5.135 × 10⁻¹⁴: **×1.7 and ×2.6** |
| sign | **expected negative, explicitly NOT gated** (n = 4 is not a pattern) | **both negative**, as expected |
| leading digit | **expected NOT to match**; a four-figure match would mean something was wrong | 1.47 against 2.51, 1.98 against 5.14 — **does not match** |
| step count | within ×2 of 818, reported not gated | 1251 accepted, ×1.53 |

Every element was borne out, **including the one predicted to fail**. The sizing that produced
the prediction: at tolerance 10⁻¹⁸ over 818 steps, controlled truncation contributes at most
~8 × 10⁻¹⁶ — 31× less than printed — so the printed errors are accumulated *arithmetic*, and a
random-walk estimate over ~10⁵ operations gives 2 × 10⁻¹³, the same order.

### 22.3 What building it found

**The grouping of (134) decides whether the estimator's blindness is visible.** On a quadrature
problem the four evaluations are pairwise **bit-identical** (α₀ = α₁₁, α₁₀ = α₁₂, and the *y*
argument is ignored), so the cancellation is algebraically exact. Written as the report writes
it, `(f₀ + f₁₀) − f₁₁ − f₁₂` **rounds before it subtracts** and returns about 1.1 × 10⁻¹⁶
instead of zero.

That is not a rounding detail. **An estimate of exactly zero is honest about being blind**, and
the controller's behaviour is then obviously degenerate. An estimate of a few times 10⁻¹⁸ is
rounding noise *wearing the shape of an estimate*: the controller responds to it, the numbers
look plausible, and nothing announces that the quantity is meaningless. The implementation groups
the differences pairwise, which is also better conditioned in general.

**The exhibition is worse than the description.** `INTG-A-005` integrates *y*′ = cos *x* from 0
to 10 at a tolerance of **10⁻³⁰**: the integrator takes **four steps**, rejects **none**, reports
a worst estimate of **exactly zero**, and is wrong by **4.4 × 10⁻³**.

**A refusal fired on a real condition rather than a constructed one.** An oversized first step on
Example (53) drives *z* negative and log *z* is not finite, so `INTG-F-004` fires naming the
component and the abscissa. That is now `INTG-A-008`'s case; the rejection-counting test moved to
two-body, which has no domain restriction.

**Orders recovered rather than asserted**: fixed-step RKF7(8) slope **6.90**, RK4 slope **4.11**.
The first attempt measured 0.87, because at *period*/200 a 7th-order method on that orbit is
already round-off limited — ~2 × 10⁻⁷ m of accumulated arithmetic against a truncation term near
5 × 10⁻¹⁰ m. Halving the step there measures the round-off walk, not the order. Measured first,
then chosen.

### 22.4 `INTG-Q-001`'s added test, and why its first criterion was the thing that was wrong

The ruling on the controller's constants was *naming and measuring is enough, with one test
added*: the constants may change the **cost** and must not change the **answer**, which is a
property rather than an assumption. The criterion offered with it was that two runs agree to
within the requested tolerance or an arithmetic floor "measured at about 2 × 10⁻⁷ m", whichever
is larger, and — explicitly — to **stop and report rather than tighten anything** if they did
not.

**They did not.** Changing all four constants separated the final states by **5.65 × 10⁻⁷ m**,
against that floor of 2 × 10⁻⁷.

**The floor was wrong, not the controller, and the way to tell was not to adjust it.** The
2 × 10⁻⁷ figure came from a single *fixed-step* run in a different test; the separation of two
*independent* round-off walks is larger than either. So the question was settled by a route that
cannot involve the effect under test: runs differing **only in the initial step**, with all four
constants held fixed.

| what differs | accepted steps | separation from the reference run |
|---|---|---|
| initial step *P*/150 | 235 | 1.71 × 10⁻⁷ m |
| initial step *P*/300 | 235 | 4.47 × 10⁻⁷ m |
| initial step *P*/200 | 235 | **7.02 × 10⁻⁷ m** |
| the arc split into two halves | — | 5.29 × 10⁻⁷ m |
| **all four constants changed** | **282** | **5.65 × 10⁻⁷ m** |

Changing only where the sequence starts — same constants, same accepted-step count of 235 —
moves the answer **more** than changing every constant does. The controller is not participating;
the separation is the arithmetic of a different step sequence, and nothing else.

`INTG-A-011` therefore asserts a **relative** claim: the constants must not separate the answer
by more than a step-sequence shift that cannot involve them already does. The band is measured
inside the test from the same-controller family, so **there is no absolute number that could have
been chosen after seeing the result** — which is what an absolute floor, adjusted upward once it
failed, would have been.

## 23. L3 step 3 — the state transition matrix, and a threshold that could not be fitted

**Date.** 2026-09-18. **State.** `tools/ci.sh` exits 0: **12 gates, 233 tests**.
`SPEC-stm.md` v1.0, Spec ID `STM`: 24 own-prefix identifiers, 13 requirements and refusals, 11
discharged, 2 excused, 0 uncovered.

### 23.1 `STM-P-1`, written before the first run, and why the textbook figure would have failed it

The gate is agreement with finite differences, and the agreement depends on the perturbation
size — a free parameter. Plan §4 rule 7 therefore applies in its sharpest form, and the
criterion was settled first.

**A central difference of a *propagated* state carries three errors, not the two an analysis of
finite differencing gives**: truncation ~ *h*²/6, machine round-off ~ ε/*h*, and **the
integrator's own noise ~ τ/(2*h*)**. The third dominates, and it is the one a textbook treatment
omits: each perturbed trajectory carries τ independently and the difference divides by 2*h*, so
it is **amplified by the very step that suppresses truncation**.

Minimising *h*²/6 + τ/(2*h*) at τ = 10⁻¹² predicted **best agreement ≈ 6.6 × 10⁻⁹ at
*h* ≈ 1.1 × 10⁻⁴**. **If only round-off mattered the answer would be ε^⅔ ≈ 3.8 × 10⁻¹¹** — so a
criterion taken from the familiar figure would have been **wrong by three orders in the
direction that fails a correct implementation.**

**Measured: 1.98 × 10⁻⁸**, a factor of three from the prediction and nowhere near ε^⅔.

### 23.2 The threshold itself contains no absolute number

`STM-A-001` judges the analytic Φ against a **band measured in the same run** from
finite-difference estimates at several perturbation sizes — a family whose spread depends on the
differencing and not at all on whether the analytic derivative is right. `INTG-A-011`'s shape,
one level up.

**The h family is chosen from the prediction, not the result**: a decade centred on `STM-P-1`'s
optimal *h*. That matters in the unobvious direction — a *wider* family is a **weaker** test.
With *h* out to 10⁻³ the perturbation is 7 km, its own truncation inflates the band to
1.6 × 10⁻⁵, and `worst ≤ band` then passes trivially. Narrowed to the predicted optimum the band
is 1.96 × 10⁻⁶ against a best agreement of 1.98 × 10⁻⁸.

**One check was removed rather than adjusted.** A first version asserted `band < 1e-6`; the band
came out at 1.96 × 10⁻⁶. The bound was both redundant and unprincipled — **the band scales with
how wide the family is, which is a design choice**, so any bound on it is a number about the test
rather than about the code, and raising it once it failed would have been exactly the fitting
rule 7 forbids. Non-degeneracy is established instead by `STM-P-1`: a degenerate comparison would
not land within a factor of three of a figure predicted before anything ran.

### 23.3 An invariant the finite-difference comparison cannot see

`STM-A-005` checks **Liouville's theorem**: d(det Φ)/d*t* = tr(A) det Φ, so for a force with
tr(∂a/∂v) = 0 the determinant is 1 for all time. **No difference estimate enters it.** Measured
at 600, 3000 and 6246 s: det Φ = 1 to the integrator's own accuracy at every span.

That is why `STM-A-001` is not alone. A band built from finite differences bounds how well Φ
matches *a finite-difference estimate of itself*; Liouville is a property of the true Φ. For both
to pass while Φ is wrong, two unrelated failure modes would have to conspire.

### 23.4 `GRAV-Q-006` resolved, with both routes measured

The second derivative follows the same three-term recursion as the first — **one array and one
line**:

> d²P̄[n] = a(n,m)·(2·dP̄[n−1] + u·d²P̄[n−1]) − b(n,m)·d²P̄[n−2]

| degree | P̄, dP̄ | P̄, dP̄, d²P̄ | incremental | a second traversal |
|---|---|---|---|---|
| 180 | 0.035 ms | 0.037 ms | **+7.5 %** | +100 % |
| 360 | 0.146 ms | 0.151 ms | **+4.0 %** | +100 % |
| 2190 | 5.797 ms | 6.675 ms | **+15.2 %** | +100 % |

**Take it from the same recursion** — 6.6× to 25× cheaper. Two qualifications recorded rather
than smoothed over: at degree 2190 the increment is **15 %, which is not "negligible"**, the
extra array costing cache at that length; and **+100 % is a lower bound** on the alternative,
since a real second traversal repeats the harmonic sum as well as the column.

The measurement is **recorded, not asserted in CI**: a wall-clock assertion is machine-dependent,
and one tuned until it passed here would be a threshold chosen by whoever is judged by it — the
rule this step was told to apply.

## 24. L3 step 4 — the registry, and the half of the gate that is easy to fake

**Date.** 2026-09-18. **State.** `tools/ci.sh` exits 0: **12 gates, 236 tests**. L3's four steps
are done and its exit gate holds. `SPEC-sensitivities.md` v1.0, Spec ID `SENS`: 18 own-prefix
identifiers, 11 requirements and refusals, 10 discharged, 1 excused, 0 uncovered.

### 24.1 The warning was right, and the defect was already in the tree

*"It is easy to write a registry where adding a parameter works and something in the integrator
quietly knew the width all along."*

It did. `SPEC-integrators`' steppers were templated on `std::size_t N` and operated on
`std::array<double, N>` — a **compile-time** width. Every test at *n* = 1 and again at *n* = 2
would have passed, **by recompiling**, and nothing in the output would have distinguished that
from a width the integrator never knew. A change to the integrator wearing the costume of a
template argument.

The steppers are now generic over a `StateVector` **concept** — anything sized and indexable — so
`y0.size()` is the only answer available to them and **one instantiation serves every *n***.
`SENS-A-002` asserts that at compile time as well as behaviourally, because the behavioural half
alone would pass against the compile-time version too.

**That change belongs to step 4**, not to step 2: it is what makes the second clause of the gate
true rather than merely untested.

### 24.2 What the gate measured

**S** = ∂**x**/∂**p** satisfies d**S**/d*t* = A**S** + B with **S**(*t*₀) = **0** — zero because
the *initial* state does not depend on the parameters, which is a statement about what is being
differentiated rather than an initialisation convenience.

A registered parameter's column against finite differences, judged by `STM-A-001`'s same-run band
with the family chosen from the prediction: **band 1.13 × 10⁻⁹, best agreement 1.48 × 10⁻¹⁰**,
and the column is not trivially zero.

At *n* = 1, 2 and 3 the accepted step counts are **19, 19, 19** — reported and not asserted,
because equal counts are evidence that the sensitivity block did not disturb the controller *for
these forces*, and a parameter whose column grew faster than the state would change the count
legitimately.

### 24.3 A failing test found a refusal that named the wrong reason

`ParameterSet` takes its width from the registry **at construction**. A test declared a parameter
after building the set; the set refused, correctly — and said *"this ParameterId was not issued by
the registry this ParameterSet was built for"*, **which was false**. It *was* that registry. The
id was simply issued later.

**A refusal that names the wrong reason sends the reader to the wrong bug**: a caller told the
first message while suffering the second goes looking for a mixed-up registry that does not
exist. The two cases are now distinguished, and the second says which slot, what width, and what
to do. `SENS-A-004` asserts the message, not merely the identifier — the identifier was already
right and would have passed.

### 24.4 Liouville rewritten before drag arrives, not after

`STM-A-005` asserted **det Φ = 1**, true for every force L3 has and **false** the moment drag
arrives at L4 with tr(∂a/∂v) < 0. Whoever met that failure would have restricted the test to
conservative forces or deleted it — losing the only check on Φ that involves no difference
estimate, exactly when the dynamics get harder.

It is now **det Φ = exp(∫ tr A d*t*)**, with the integral carried as one extra scalar in the
variational state, so the conservative case is the *special case*. `STM-A-005b` makes it sharp
today rather than waiting: a dissipative force *a* = −*k***v** gives tr(A) = −3*k* exactly, and at
*t* = 600 s the integral is **−0.36 against a closed form of −0.36**, with det Φ = **0.6977** =
exp(−0.36) — the determinant genuinely moving rather than confirming a constant.

*A test that is trivially satisfied now and sharp later is worth more than one that has to be
rescued.*

### 24.5 The standing measurement, and which modules could never move it

The crossing register is **72 production sources, still 15 literals**, unchanged through four new
modules. Two of them could never have moved it and that is worth knowing before it does move:
`integrators` works on a bare vector and never sees a length at all, and **the variational
equations are structurally immune** — A's blocks are s⁻² and s⁻¹, both invariant under the
scaling that takes metres to kilometres, so A is the same matrix in either system and there is
nothing there to convert. When the pair finally moves, it will not have been these.

## 25. L4 step 1 — the shadow function, and a branch that is the opposite of the last one

`SPEC-shadow.md` v1.1, Spec ID `SHDW`. Both halves of `LI19` are implemented: the SECM in
`modules/shadow/src/conical.cpp` and the PPM in `perspective.cpp`, gated by `SHDW-A-001`…`-A-015`.

### 25.1 Retrieval, and what the `literature` manifest kind is for

`LI19` — Li, Ziebart, Bhattarai & Harrison (2019), *A shadow function model based on perspective
projection and atmospheric effect for satellites in eclipse*, Adv. Space Res. **63**(3) 1347–1359,
doi:10.1016/j.asr.2018.10.027. **The publisher's page and the UCL Discovery landing page both
return 403**; the accepted manuscript is reachable only at the path the open-access metadata
names. Pinned as `li-ziebart-2019-shadow`, SHA-256 `b07c8b89…97bd`.

It is a `literature` manifest entry, which is a **provenance record and not a dependency**: exempt
from the permissive-licence gate because nothing derived from it is a copy of it, excluded from
`NOTICE`, and unreachable from any build input — which `ci.sh` gate 11 proves by injection over
115 build inputs, 0 of which reach a literature entry. Its terms could not be established, and the
entry records **the search rather than the conclusion** (plan §4 rule 4).

`LI19` also publishes working code on GitHub. Under plan rule 8 that code is an **oracle and not
normative**, because the paper it implements is the specification; under the rule's converse,
where an independent specification exists the artefact's legibility stops mattering. It was not
sought, not retrieved and not read.

### 25.2 The five family choices, and a sixth the paper does not name

"Conical shadow" is a family. `LI19`'s SECM makes five choices — spherical Earth, a **disc** Sun,
umbra/penumbra/**annular**, *F*ₛ as the **true occulted-area ratio** (not linear, not a
smoothstep), and **no atmosphere** — and all five are named in `conical.hpp` beside the code that
makes them, so a reader who needs a different member sees which choice they are changing.

The PPM differs in every one of the five. And measuring the two against the **definition** — the
fraction of the solar disc's solid angle that is not occulted, computed with no projection in it
at all — exposes a **sixth choice neither §3 nor the paper names**: *where the ratio is taken*.
The SECM takes it on the **flat sky**, using angular radii as planar lengths; the PPM takes it in
a **perspective projection**, which maps the straight lines of the occultation to straight lines.

| at each orbit's terminator | \|SECM − definition\| | \|PPM − definition\| | oblateness |
|---|---|---|---|
| LEO, *r* = 7 331 km | **1.86 × 10⁻⁴** | 2.1 × 10⁻⁶ | **1.88 × 10⁻⁶** |
| GPS, *r* = 26 560 km | 4.0 × 10⁻⁵ | 1.8 × 10⁻⁷ | 1.34 × 10⁻⁵ |
| GEO, *r* = 42 164 km | 2.5 × 10⁻⁵ | 1.5 × 10⁻⁸ | 2.17 × 10⁻⁵ |

**At LEO the unnamed choice is 99 times the oblateness effect the PPM is adopted for.** A study
that swaps SECM for PPM and attributes the whole difference to the Earth's figure is wrong by two
orders of magnitude at LEO, and right to within a factor of two only at GEO. `SHDW-A-009` asserts
the ratio; `SHDW-Q-004` asks the manager whether it belongs in the discrepancy register, because
it is a property of two models neither of which claims to be the other.

### 25.3 Two branches that look alike in the source and are nothing alike

- **The annular branch is unreachable.** An annular eclipse of the Sun by the Earth needs the
  satellite beyond the umbra cone's apex, `SHDW-P-1` = *R*ₑ*d*/(*R*ₛ − *R*ₑ) = **1.3842 × 10⁶ km**.
  GNSS at 26 560 km is at 1.9 % of it and the umbra there is still 6 256 km across; the apex is 52×
  further out. The branch is implemented because it is in the model and exercised only from a
  **synthetic** geometry, and `SHDW-A-005`'s name says so.
- **The hyperbolic silhouette is the normal case at LEO.** `LI19` eq 24 sorts the projection by
  |*B*|, and the hyperbola is the paper's "partial image". For a sphere the condition is
  |cos ψ| < *R*ₑ/*r* while the terminator sits at ψ = asin(*R*ₑ/*r*); the windows overlap below
  about 8 400 km. Measured by bisection onto the degenerate point, the boundary at *r* = 7 331 km
  is at geocentric angle **0.5156 rad = 29.5°**, matching the closed form exactly.

  So ISS, LEO and Sun-synchronous orbits are **hyperbolic** in the penumbra and GPS, Galileo and
  GEO **elliptical**. Both branches carry real orbits, both are gated from real geometries, and
  `SHDW-A-008` asserts 3 hyperbolic and 6 elliptical so that a run exercising one branch cannot
  pass. This is recorded next to the annular branch precisely because the two look alike in the
  source — a rarely-taken `if` — and are opposite in every way that matters.

### 25.4 `LI19`'s five atmospheric cases do not exhaust the geometry above 1 983 km

Fig. 8 gives five relative positions of the solar disc, the Earth's image and the atmosphere's
image, and §2.6 a formula for each. In every one the disc meets **at most two** of {solid Earth,
atmosphere, clear sky}: eq 45 carries a term for the blocked area and one for the in-atmosphere
area and **none for a fully lit one**. That is complete exactly while

> asin((*R*ₑ + 50 km)/*r*) − asin(*R*ₑ/*r*)  ≥  2 asin(*R*ₛ/*d*)  =  9.301 × 10⁻³ rad,

which holds to ***r*** **= 8 361 km (altitude 1 983 km)** and fails above it. **GRACE, at 500 km,
is inside the valid region; Galileo, at 29 600 km, is not** — and `LI19` validates against both.

`SHDW-F-009` refuses there rather than invent a sixth case, because inventing one changes the
**model** and not this implementation of it (plan §5 constraint 4). The natural extension is one
term and is written out in `SHDW-Q-003` so the manager's decision is a decision and not a
rediscovery. **This is stated as a gap in the printed specification and nothing more**: the
published code may well close it, it was not read, and no claim is made about the paper's Galileo
results. Plan §4 rule 4 — the bar for the register is higher than the bar for flagging.

### 25.5 Table 2 was not attempted, and the obstacle is an account rather than the geometry

`LI19` Table 2 prints 16 eclipse events for GRACE-A on 2007-01-20 with penumbra entry and exit to
the second — **30 transitions**. The manager's premise that some were grazing was checked against
the printed data and **does not hold**: the transit durations are 27–33 s with a 21 % spread and
none is grazing, so all 30 are transverse crossings and all 30 would be reachable with an
ephemeris good to about a kilometre. The geometry is therefore cleared; the obstacle is elsewhere.
Five routes, each with what it returned:

| route | result |
|---|---|
| CelesTrak, current elements | none: GRACE-A deorbited in 2018 |
| CelesTrak, archived elements | behind `request.php` |
| GFZ ISDC | registration required |
| PODAAC | registration required |
| space-track.org | registration required |

All five need an account or a request form, and the standing prohibition on contacting anybody
covers all of them. Recorded as `SHDW-Q-001` so that it is not re-opened as a technical question:
it is the owner's decision, not a workaround.

### 25.6 A flag retracted before it entered the register

`LI19` Table 4 reports 36 where Table 3 reports 30, and this was raised as a discrepancy. **It was
wrong and was withdrawn.** The two are different measurements: 30 is GRACE-A against its
accelerometer (Table 3); 36 is Galileo E11 with PPM_atm as truth (Table 4) — different
satellites, different epochs, different reference standards, all stated in the paper. Recorded
here as a **resolution and not a discrepancy**, so that a later reader does not re-enter it. The
manager made the asymmetry plan §4 rule 4: *a defect register is weakened by a false entry more
than by a missing one, so the bar for entering it is higher than the bar for flagging.*

### 25.7 Three faults this step found, all the same shape

Every one is a check that passed, or would have passed, while never examining the thing it was
meant to examine.

1. **The comparator disagreed and refinement said why.** `SHDW-A-003` failed at 5.72 × 10⁻³
   against a 2 × 10⁻³ bound. The obvious reading was comparator coarseness, and **refinement
   disproved it**: grids of 1 000 … 16 000 gave 5.6723, 5.7029, 5.7184, 5.7236, 5.7264 × 10⁻³ —
   stable, not shrinking. So it was a real disagreement, and it was in the **test**: the satellite
   was placed by a *geocentric* angle and the model works in the angle *at the satellite* between
   the Earth's centre and the Sun. At LEO the Earth subtends 1.055 rad and the two are nowhere
   near equal. Corrected, the worst residual is 3.09 × 10⁻⁵. Had the tolerance been widened
   instead, a correct implementation would have carried a permanently loosened gate to hide a
   test's own error.
2. **A scan that never reached what it measured.** `SHDW-A-004` reported a penumbra 1.02 rad wide
   against a solar angular diameter of 9.3 × 10⁻³ rad. The scan ran over 0 … 0.02 rad while the
   penumbra sits near 1.03 rad, so the lower edge was never found and the reported width was
   `0.02 − (−1)` — the sentinel. Both corrected cases now **assert that the scan brackets the
   transition**: `SHDW-A-004` counts umbra and sunlight samples on either side, because a window
   lying wholly *inside* the penumbra would report the scan's own width and pass.
3. **A degeneracy threshold anchored to 1.** The parabola test compared |*B*| against
   `1e-18 * max(1.0, …)`. *k*₀, *k*₁, *k*₂ are quadratic forms of *A* = diag(*a*⁻², *a*⁻², *b*⁻²)
   and run at 10⁻²⁷, so **every geometry was called a parabola** — and the sweep that found it
   printed *"worst difference 0.000 × 10⁰ over 41 geometries"* while comparing none of them. The
   threshold is now relative to the coefficients' own scale, and **the compared count is asserted**
   wherever this is measured. It is the same unit error the crossing register of §21 exists to
   catch, in a quantity that has no name in `core/units.hpp`.

### 25.8 Three places the implementation departs from the printed derivation, and why

| eq | printed | here | reason |
|---|---|---|---|
| 15 | three-case distance test on *d*, *d*_S1, *d*_S2 | *g*ᵀ*M**g* ≥ 0 **and** *g*ᵀ*A**r* < 0 | the same fact from the quantity the area needs anyway; the second factor selects the forward nappe, which is the whole difference between a hyperbola's two branches |
| 32–33 | quartic in η, by Ferrari | the circle parametrised by its **angle** | eq 32 is the Weierstrass substitution η = tan(θ/2) and **has a pole at θ = π**, so (−*R*₀ + *t*ₓ/2, *t*ᵧ/2) is the image of no finite η and drops out of the root set. In θ it is a degree-two trigonometric polynomial — the same four roots, none missing, no resolvent |
| 36–39 | four cases, the arch from Hughes & Chraibi (2012) | **Green's theorem** once | ½∮(*x* d*y* − *y* d*x*) is ½*AB*Δψ on an ellipse and ½σ*AB*Δτ on a hyperbola, because cos² + sin² = 1 and cosh² − sinh² = 1 do the same job. The four inside/outside × ellipse/hyperbola cases collapse to one sum |

The third also settles a claim `SPEC-shadow` v1.0 made and v1.1 re-derives: v1.0 said `LI19` prints
the **full** derivation of both models, and it does not — eq 36–39 delegate the arch to a second
paper this tree does not hold. **The conclusion survives for a different reason than the one
given**: the delegation is not load-bearing, because an affine map scales every area by |det| and
the arch follows from the conic alone. Plan §4 rule 3 — a correction re-derives the whole
statement rather than patching one term.

### 25.9 What the PPM's gate rests on, and what it cannot catch

The comparator is **the definition, not the other model** (plan §4 rule 2). *F*ₛ is the fraction of
the solar disc's solid angle not occulted, and a direction is blocked iff *r* + *p**w* meets the
ellipsoid at some *p* > 0 — no projection anywhere in it. It is **exact in the radial direction**
(each blocked interval's ends by bisection, the radial integral as cos *a*₁ − cos *a*₂) and
discretised only in azimuth. That mattered: a **cell count converges like 1/n and, measured, could
not separate the two models at all** — at 150…2 400 cells both sat inside its own noise. The exact
version converges to ≈ 1 × 10⁻⁶ at 8 000 azimuths, which is 20× tighter than the SECM's departure
and so can tell them apart. `SHDW-A-008`'s 1 × 10⁻⁵ bound is that measured convergence, and the
SECM would fail it.

What the gate still cannot catch is a consistent misreading of `LI19`'s *geometry* that is
internally self-consistent, because Table 2's published times are unavailable (§25.5).
`SHDW-A-004` remains the strongest defence: the penumbra's angular width is a property of the Sun
being a disc, and no point-source implementation can produce it.

### 25.10 The reference definition was itself a family choice, and the largest one

The manager challenged `SHDW-A-008`'s agreement figure at the point where it was about to become
an accuracy figure: *F*ₛ measured as an occulted **area** ratio assumes a **uniformly bright**
solar disc, and the Sun is limb darkened. The premise was stated from the visible band; **for SRP
the quantity is bolometric**, and Eddington's grey atmosphere gives the emergent intensity as
*I*(µ)/*I*(1) = (2 + 3µ)/5 — the linear law with ***u*** **= 3/5 exactly**, hence
*I*(limb)/*I*(centre) = 0.4. The premise therefore holds, and for a better reason than the one
offered.

Measured at LEO across a penumbra passage, with the radial weight in closed form from
∫√(*c*² − *k*²) d*c* and checked against its own disc mean 1 − *u*/3 (agreeing to 4 × 10⁻⁷, the
residual being sphericity, since 1 − *u*/3 is the flat-disc limit):

| axis | peak effect on *F*ₛ | ratio to the next |
|---|---|---|
| the Sun's **brightness profile** | **1.97 × 10⁻²** | 83× |
| **where the ratio is taken** (§25.2) | 2.4 × 10⁻⁴ | 126× |
| the Earth's **figure** — the PPM's purpose | 1.9 × 10⁻⁶ | — |

The estimate offered was 6 × 10⁻³; the measurement is **1.97 × 10⁻², three times larger**. The
predicted structure is exactly right: the error is **antisymmetric about 50 % occultation**,
crossing zero there (measured −5.4 × 10⁻⁴ at 1 − *F*ₛ = 0.506) because a radially symmetric profile
puts exactly half its flux either side of a central chord. Peak at 1 − *F*ₛ = 0.818. Stable to six
figures over 1 000 … 8 000 azimuths.

**And one thing the estimate did not predict: 99.9 % of it cancels across a full passage.** The
net time integral is a few × 10⁻⁴ s of equivalent full sunlight against an absolute integral of
**1.142 × 10⁻¹ s, stable to four figures under refinement**. But the **running** integral swings
to **≈ 5.72 × 10⁻² s** mid-passage — **≈ 376× the net** — so it cancels *across* a passage and
not *within* one. Anything sampling inside a passage sees the full 1.97 × 10⁻². That is a
property of the passage, not of the model, and it is why an axis this large has been survivable.

**This swing ratio was corrected twice, and the second correction is the one that stands.**
First (2026-09-22): the section originally read 376×, from the ad hoc scratch investigation
that preceded `tools/penumbral_cancellation.py`'s existence. The manager ran the freshly
committed tool end to end — the first time a regenerator in this tree caught its own recorded
figure on its first run, §4 rule 3's own argument demonstrated rather than stated — and it
printed 367× at its default resolution, which this section then adopted.

**That adoption was itself premature, and the manager's own follow-up review caught it**: `net`
is a residual of two much larger, near-cancelling half-integrals, which makes it — and the
swing ratio built from it — far more sensitive to resolution than `abs` is, and the tool's
*default* resolution is not the tool's *converged* one. Extending the resolution check to this
baseline row (which the original check, run only on the two most extreme β rows, had not
covered) across five resolutions up to 3 200 samples × 1 600 azimuths gives:

| samples × azimuths | net (× 10⁻⁴ s) | abs (× 10⁻¹ s) | swing |
|---|---|---|---|
| 201 × 100 | −1.556 | 1.1427 | 367.6× |
| 401 × 200 | −1.543 | 1.1424 | 370.8× |
| 801 × 400 | −1.539 | 1.1423 | 371.7× |
| 1 601 × 800 | −1.522 | 1.1423 | 375.7× |
| 3 201 × 1 600 | −1.523 | 1.1423 | 375.6× |

`abs` is stable to four figures throughout (0.04 % spread); `net` and the swing ratio are not
(2.2 % spread) — but the two **finest** resolutions agree with each other to 0.03 %, far
tighter than any coarser pair, which is the signature of genuine convergence rather than noise.
**The converged value is ≈ 376×, not 367×**: the tool's own default (401 samples) sits near the
*low* end of the swept range, closer to where 367× came from than to where the sequence settles.
So the figure this document now carries is the original 376×, restored — but restored with a
five-point convergence study behind it, where before there was a single unchecked run. The
right general lesson is not "prefer the tool's default," it is **"a regenerator's own default
resolution needs the same convergence check any other quadrature does before its output is
quoted to more figures than it has earned"** — the same discipline `SHDW-A-003`'s original
2 000-azimuth grid needed and got, applied here to a tool built after that lesson was learned
and not, this time, before being trusted.

Neither model here is affected *relative to the other*, since both assume a uniform disc; what the
axis changes is the meaning of the agreement figure. **Agreement with the uniform-disc definition
is not accuracy**, and `SPEC-shadow` now says so wherever the figure appears. The ordering is
insensitive to the coefficient: over *u* ∈ [0.3, 0.9] the peak runs 8.8 × 10⁻³ … 3.4 × 10⁻², and
this axis dominates at every value.

Nothing implements it. **Ruled: defer** (`SHDW-Q-005`) — and, the manager was careful to specify,
**not for the reason `SHDW-Q-003` was refused**. `Q-003` had no published case to gate an
extension against; a bolometric limb-darkening law does not have that problem, since published
coefficients exist in quantity. The deferral is about **need**: nothing in this plan yet samples
*inside* a penumbra passage, which is the only regime §25.11 below finds the effect surviving in.
When a consumer that does exists — accelerometry or high-rate tracking, at L6/L7, or an L8
campaign — it is implemented against a published bolometric law with its own provenance and its
own gate, and Eddington becomes the cross-check rather than the source.

**A fourth instance of §25.7's fault, in the check written to prevent it.** The resolution study
for the peak first sampled a fixed fraction of the sweep window that landed **deep in the umbra**,
where both models return 0, and reported differences of 10⁻¹⁴ across four grids — a converged
agreement between two things that were not being compared. It now samples the recorded peak
location. The fault survives being named; only asserting what was compared kills it.

### 25.11 The 99.9 % figure was a property of one traversal, and now the traversal is named

The manager would not let the cancellation figure stand unqualified: *"the antisymmetry is in the
shadow function against occulted fraction; the integral that matters is over time. Those agree
only if the traversal is near-symmetric."* Right, and the passage behind §25.10's numbers had
never been named — LEO, *r* = 7331 km, **circular**, the shadow axis in the orbital plane
(β = 0°), one side of the penumbral transition, 12.0 s. That geometry is symmetric for two
reasons that happen to coincide there: a circular orbit crosses at exactly constant angular rate,
and β = 0 puts the crossing exactly on the sun-Earth line. Four candidate mechanisms were named as
capable of breaking it — high beta, a shallow crossing, an eccentric orbit, entry and exit at
different angles — and each was **measured**, with an actual two-body Kepler propagator (bisection
for Kepler's equation, not an approximation), rather than reasoned about by hand a second time
after the visible-band limb-darkening estimate had already shown where that goes wrong.

**Orbital-plane tilt, swept from β = 0 to the eclipse's own cutoff angle *a*ₑ.** The degradation is
**smooth**, not a cliff, and confined to a narrow band right at the cutoff:

| β/*a*ₑ | 0 | 0.3 | 0.6 | 0.8 | 0.9 | 0.95 | 0.97 | 0.98 | 0.99 | 0.995 | 0.998 | 0.9995 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| cancels | 99.87% | 99.85% | 99.75% | 99.49% | 98.94% | 97.83% | 96.35% | 94.47% | 88.66% | 74.68% | 65.33% | 48.32% |

Nine-tenths of that range (β up to 0.9*a*ₑ) barely moves the figure; the **last one percent**
before the exact grazing tangent is where it fails. β/*a*ₑ = 0.99 is **half a degree** short of
the cutoff at LEO — the dawn-dusk / eclipse-season-edge regime, not "high beta" in general, where
the 99.9 % figure is undisturbed. The two most extreme rows (74.68 %, 48.32 %) were checked by a
four-fold refinement in both time-sample count and azimuthal resolution and did not move past the
fourth decimal place before being recorded — §4 rule 5's diagnostic, applied before either number
went in the document rather than after a challenge.

**Eccentricity, eclipse at an apse — and this is where "an eccentric orbit breaks it" turned out
to be the wrong hypothesis, not a smaller effect.** Held at LEO perigee (*r* = 7331 km) with the
eclipse centred exactly on periapsis, cancellation across *e* = 0, 0.3, 0.5, 0.7, 0.85 measured
99.84–99.90 % — statistically indistinguishable from the circular baseline, at every eccentricity
tried, including a Molniya-like 0.85. The reason is not empirical: **an unperturbed two-body orbit
is exactly time-symmetric about apsis passage**, *r*(−*t*) = *r*(*t*) identically, for any *e*, so
an eclipse that happens to sit on an apse inherits that symmetry regardless of how eccentric the
orbit is. The first attempt at this measurement used exactly this configuration and found nothing
— which is correct, not a failed stress test, once the reason is seen: it was never actually
testing what it was built to test.

**Eccentricity, eclipse OFF an apse — the configuration actually behind "entry and exit at
different angles".** With the eclipse point offset from periapsis by 45° or 90° (genuine non-zero
radial velocity there, perigee held at a safe 600 km altitude so the orbit stays physical),
cancellation measured 99.37–99.84 % across *e* up to 0.85 and both offsets — **measurably worse
than the on-apsis case, but an order of magnitude short of the near-cutoff effect above.**

**So of the four candidates, one dominates, and it is a narrow one.** A beta angle within about a
degree of an orbit's own eclipse cutoff breaks the cancellation substantially; genuine eccentricity
away from an apse breaks it modestly; eccentricity at an apse — the configuration the phrase most
naturally suggests — does not break it at all, exactly, for a reason that is a fact about the
two-body problem rather than a property of this module. `SHDW-R-031` now states the one traversal
it measured; `SHDW-R-033` states what was found to move it and by how much. Neither is asserted by
a gated test — carrying a two-body propagator permanently to check a deferred feature's magnitude
would be its own kind of over-building — so both are recorded in `SPEC-shadow` §8's Coverage
exceptions rather than claimed as gated.

**The search itself needed the same discipline the whole investigation was about.** Its first
version centred the sampling window on the point of deepest occultation with a window far too
narrow to reach the penumbra at all (11.9 s of pure umbra, both models exactly 0, "cancellation"
of a comparison between nothing and nothing); its second, widened to find the transition
correctly, then diluted resolution by spreading samples over the **whole** eclipse including
twenty minutes of flat umbra dwell contributing nothing; its third made an eccentric orbit's
closest approach to the antisolar direction land inside the Earth, an unphysical orbit, caught by
comparing the reported radius to *R*ₑ by hand; and a fourth undershot the search window for an
off-apsis crossing — **stated backwards in the first draft of this sentence**, caught by the
manager: it is not the last few degrees before periapsis that take the missing time, that is
exactly where the motion is fastest (at *e* = 0.85 the last 5° take 0.06 % of the period, the
last 10° take 0.12 %). What takes the time is **reaching** that point at all, going forward from
the previous periapsis through the slow apoapsis arc — nu = −121° is not passed until 96.0 % of
the period has elapsed, at *e* = 0.85, because everything but the final ~121° of true anomaly is
swept slowly. The window search, going forward from t = 0, needed to run that long to reach it,
not merely past 90 %. Five faults, not four, in the tool built to state one number precisely: the
fifth being this sentence's own first draft, caught after the number it belonged to had already
been checked and was correct — the description of *why* the search needed fixing was wrong, not
the fix. Each caught before reaching a reader who would have taken it as the reason rather than
checking it.

## 26. L4 step 2 — the macromodel schema, and two force laws checked two ways each

`SPEC-macromodel.md` v1.0, Spec ID `MCRM`. `modules/macromodel`. Gated by `MCRM-A-001`…`-A-010`.

### 26.1 The same paper the plan names, reprinted in full where it could be read without an account

The plan cites Rodríguez-Solano, Hugentobler & Steigenberger (2012), *Adjustable box-wing model
for solar radiation pressure impacting GPS satellites*, Adv. Space Res. **49**(7):1113–1128 —
`RHS12`. The publisher copy sits behind ScienceDirect; ResearchGate's copies need an account. The
paper's **first author's own 2014 doctoral dissertation** at TU München reprints it in full,
unaltered, as Chapter P-II (pp. 85–101), and TUM's own repository (`mediatum.ub.tum.de`) serves
the PDF directly over HTTPS with no login and no request form. Retrieved as `RS14`, pinned by
hash, `literature` kind — the same footing as `LI19` (§25.1): a provenance record, not a
dependency, terms not established, the search recorded rather than a conclusion.

**This is also where Tables 1 and 2 live** — real, citable a priori optical properties and
dimensions for GPS Block II/IIA and Block IIR — which L5 will use to populate this schema and
which this step deliberately does not read into anything: `SPEC-macromodel` states no satellite's
actual mass, area or optical coefficient. The schema and the physics that RHS12's a priori tables
and this step's cannonball gate both rest on are the same; the populated values are L5's alone.

### 26.2 Two force laws, both re-derived rather than only cited

`RHS12` Eq. (6), the flat-surface law, is stated exactly as printed. Its **normal-incidence
special case**, needed for the sun-pointing solar-panel degenerate test, was re-derived
independently by momentum bookkeeping before being trusted algebraically: an absorbed photon
transfers momentum *p* = *E*/*c* (coefficient 1); a specularly-reflected one bounces straight
back, transferring 2*p* (coefficient 2); a diffusely-scattered one is absorbed then
Lambertian-re-emitted, carrying a mean recoil of (2/3)*p* along the outward normal on top of the
*p* already transferred by absorbing it (coefficient 1 + 2/3 = 5/3). Weighted by α, ρ, δ and using
α+ρ+δ = 1: α·1 + ρ·2 + δ·(5/3) = 1 + ρ + 2δ/3 — the same coefficient Eq. (6) gives at cos θ = 1,
by an independent route.

**The sphere's coefficient is not in `RHS12` at all and was derived here.** Integrating Eq. (6)
over a sphere's illuminated hemisphere (surface element at polar angle φ from the sub-solar
point, cos θ = cos φ, the local normal *e*ᴺ(φ,λ) varying with position): the transverse components
of ∫*e*ᴺ vanish over the full azimuth by symmetry, the ρ-dependent terms cancel **exactly**
(∫₀¹[−ρ*x* + 2ρ*x*³] d*x* = −ρ/2 + ρ/2 = 0, *x* = cos φ), and what survives is
∫₀¹[*x* + (2δ/3)*x*²] d*x* = 1/2 + 2δ/9, giving

> ***f*** = −(*A S*₀/*c*)(1 + 4δ/9) *e*ᴰ.

**ρ does not appear.** A perfectly specularly-reflecting sphere and a perfectly absorbing one
exert the same net force — the tangential components of specular reflection cancel around the
curved surface the same way a diffuse re-emission's do not, since Lambertian re-emission has a
genuine outward bias a specular bounce does not.

**Checked against Monte Carlo before being trusted**: 4 000 000-sample integration of Eq. (6) over
the hemisphere, at five (α, ρ, δ) triples spanning pure-absorbing, pure-specular, pure-diffuse and
two mixed cases, matched the closed form to 3–4 significant figures at every triple (pure-diffuse:
MC 1.4446, closed form 1.4444; the other four agree as closely). Substituting Fliegel's
δ = ν(1−µ) (§3, `RHS12`'s own notation dictionary) reproduces the plan's quoted
`(9 + 4ν(1−µ))/9` **exactly** — confirmation, not coincidence, since both formulas descend from
the same Milani et al. (1987) law `RHS12` states as its Eq. (6), integrated differently.

### 26.3 Why the plan's own aside is right, and not only by notation

The plan: *"a sphere's (9 + 4ν(1−μ))/9 is not a flat plate's 1 + ρₛ, and conflating them is a
factor of two in a recovered area."* With both formulas now derived: the sphere's diffuse
coefficient is 4δ/9; the flat plate's at normal incidence is 2δ/3 — **three times larger on the
diffuse term alone** — and the flat-plate form `1 + ρₛ` additionally assumes δ = 0 outright, a
second, separate simplification stacked on the first. Conflating a sphere's coefficient with a
flat plate's is not one error but two compounding, and `MCRM-A-005` asserts the gap between them
is exactly 2δ/9 at every δ tried, so neither can be silently substituted for the other in this
tree.

### 26.4 The schema: two surface kinds, and a mismatch a plain struct would have allowed

A macromodel is any number of surfaces, each a `FlatSurface` (a normal, either a stored
body-fixed constant or `sun_pointing` — defined to equal the Sun direction at every evaluation,
because a sun-tracking solar panel's whole point is that its normal follows the Sun by
construction, and storing a separate value for it would be a second, competing definition of the
same quantity) or a `SphericalSurface` (no normal field at all, because none exists), plus one
mass and one centre of mass. Every area, every optical coefficient, the mass and the centre of
mass are `Cited<T>`: constructible only through `cited()`, which refuses an empty or
whitespace-only citation, so "a value without a citation is a load error" is the type rather than
a comment next to it.

**`FlatSurface` itself went through a second pass.** The first version stored `NormalMode`
alongside an independent `optional<BodyDirection>` — a plain aggregate that let the two disagree:
`body_fixed` mode with no normal ever supplied, or `sun_pointing` mode carrying a stored normal
that would then be silently ignored. Nothing would have caught the mismatch until `srp_force`
dereferenced an empty optional, on some input a test happened not to construct. Replaced with two
named factories, `flat_surface_body_fixed` and `flat_surface_sun_pointing`, each the only source
of one `NormalMode` value — the pairing is a constructor-time fact instead of an invariant a
reader has to trust across two independent fields. Caught during implementation, before any test
was written against the first version, by asking what `flat->body_fixed_normal->vec()` does when
the optional is empty rather than assuming a caller would never do that.

### 26.5 The gate: two degenerate configurations, stated in the test, no library read

`MCRM-A-001`: a one-surface spherical cannonball, area and (α, ρ, δ) stated in the test, matches
the derived closed form to 1 × 10⁻¹² relative across five optical triples and four Sun directions
(20 combinations) — and points opposite the Sun at every one, the physical sign check a magnitude
comparison alone would miss. `MCRM-A-002` holds α+δ fixed and trades ρ alone across six values;
the force does not move, matching §26.2's finding that ρ has no term. `MCRM-A-003` evaluates the
same sphere at four differently-oriented "body-frame" Sun directions and finds the same magnitude
at every one — meaningless for a sphere, and proven meaningless rather than asserted so. `MCRM-A-004`
builds a one-surface sun-pointing black sail (α=1, ρ=0, δ=0) and checks it against the textbook
*f* = *S*₀*A*/*c* identity **and** against `MCRM-A-001`'s α=1 spherical case of the same area —
both reduce to the same identity, and agree to 10⁻⁹ N. `MCRM-A-005` is §26.3's assertion, gated.
`MCRM-A-006`/`-A-007` fire citation and unit-vector refusals one field at a time and show them not
firing on the adjacent valid input, this tree's standing refusal-testing discipline. `MCRM-A-010`
fires the cos θ < 0 branch specifically — a domain restriction with no test reaching it is the
same fault as a guard that cannot fire, in a different shape.

**None of this needs `RS14`'s Tables 1–2.** Every number in the gate is stated in the test file
itself, which is what keeps this layer's exit gate satisfiable with what this layer and the layers
below it have (plan §5 constraint 7) — the arc fit `srp-analytic` (step 3) eventually needs real
GPS optical properties for, but that population is L5's job and this step's whole point is to have
a contract ready for it before it exists.

## 27. L4 step 2 reviewed, corrected, and step 3 opened with the relocation it required

`SPEC-macromodel.md` to v2.0. `SPEC-srp-analytic.md` v1.0, Spec ID `SRPA`, `modules/srp_analytic`.
Gated by `SRPA-A-001`…`-A-008`; `modules/macromodel` gains `MCRM-A-011`/`-A-012`.

### 27.1 `srp_force` was physics living inside a data module, and moved

§26 built `srp_force` inside `modules/macromodel`. Review: L5 is designed as "data with
per-value citations, not as code" (`../plan/PLAN.md` §3.6), and a force computation inside
the schema module means every consumer of the macromodel schema — including L5's own
population code, which wants only the schema — links an SRP force whether it needs one or
not. `PERT-Q-001`'s precedent against exactly this shape, now applied to a sibling case.
`srp_force`, both force laws, and every test that computes a force moved to
`modules/srp_analytic`, which depends on `odl::macromodel`; `odl::macromodel` does not depend
on it, so the link boundary now says what the module boundary means.

**The schema gained a gate of its own once the force left**: `MCRM-A-011` builds a one-surface
`Macromodel`, every field with a distinct stated citation, and reads every value and every
citation back, asserting identity — a schema this general (N surfaces of either kind) needs a
check that does not depend on a consumer existing to exercise it indirectly. `MCRM-A-012` does
the same for both `FlatSurface` normal modes.

### 27.2 The factor-of-two guard tested the one case where there isn't one

`MCRM-A-005` (v1.0) asserted the flat-plate and sphere coefficients differ, at a stated δ > 0
with **ρ = 0 always**. The general gap is ρ + 2δ/9 (§26.2's two coefficients, subtracted); at
ρ = 0 that is at most 2/9 ≈ 0.22 for any δ ≤ 1 — small by construction. The dominant term is
ρ, not δ: at ρ = 0.9, δ = 0 (a specular sail) the ratio between the two coefficients is
**1.90**; at ρ = 0.5, δ = 0.2 (a mixed panel) it is **1.50**; the largest a δ-only sweep at
ρ = 0 can ever produce is **1.22**. A specular sail is exactly the shape of this project's one
confirmed error on real data (§20's 53%-vs-26% LightSail-2 reading, a reflectivity/specularity
mix-up) — the case the guard exists to catch, and the case ρ = 0 sets aside identically on
every row. `SRPA-A-005` corrects this: it asserts the gap **across a range of ρ including
ρ = 0.9**, and asserts explicitly that at least one case shows a ratio no ρ = 0 sweep could
produce (§8's own guard against a future edit narrowing the range back).

**The framing that produced the narrow test is recorded, not only the fix.** Reporting the
original result, the executor described the diffuse-term difference — 2δ/3 against 4δ/9 — as
"a factor of 3" (both in a message and, found on inspection, in the committed spec and
provenance text itself). The true ratio is (2/3)/(4/9) = **1.5**; "3" is what dividing the
denominators alone gives (9/3), not the coefficients. The diffuse term is also, structurally,
the *smaller* of the two terms in the gap — ρ + 2δ/9 — so a framing that inflated it drew
attention away from the term that actually carries the plan's warning. Neither the inflated
ratio nor the emphasis it produced was deliberate, and both are corrected here rather than
only in the number that follows from them.

### 27.3 The `FlatSurface` redesign, found before any test found it

Kept from §26.4 without change, since the review did not touch it: the first `FlatSurface`
paired a `NormalMode` with an independent `optional<BodyDirection>`, which let the two
disagree (`body_fixed` with no normal supplied, or `sun_pointing` carrying a normal that would
be silently ignored) — a mismatch nothing would have caught until `srp_force` dereferenced an
empty optional. Replaced with two named factories, each the only source of one `NormalMode`.
Named again here because it is the part of this step the manager's review called out
specifically as done right: an invalid state found by asking what a dereference does, before
any test found it by failing.

### 27.4 Two more things a reproducible tool and a second reviewer found, and one of the two took two passes

**The recorded swing ratio, caught twice.** The manager ran `tools/penumbral_cancellation.py`
end to end for the first time — the first time a regenerator in this tree has caught its own
recorded figure on its own first run, the demonstration plan §4 rule 3's "excused from the gate
is not excused from reproducibility" was written for — and found §25.10's 376× did not match
the committed tool's default-resolution output, 367×. That correction was adopted, and it was
itself premature: `net` is a residual of two much larger, near-cancelling half-integrals, far
more sensitive to resolution than `abs` (stable to four figures throughout), so a tool's
*default* resolution is not automatically its *converged* one. The manager's own follow-up
review said so directly and asked for the baseline row's own resolution check, which the
original check — run only on the two most extreme β rows — had not covered. Extended to five
resolutions up to 3 200 samples × 1 600 azimuths (§25.10's table), the two **finest** agree to
0.03 %, far tighter than any coarser pair: the converged value is **≈ 376×**, restoring the
original figure — now with a convergence study behind it rather than a single unchecked run
either time. Corrected in §25.10 and in `SPEC-shadow` `SHDW-R-031` to the resolution-supported
figure, with the caveat both now carry that `net` and the swing ratio built from it should never
be quoted to more precision than a convergence check has actually earned.

**`RS14` was described as pinned before it was.** §26.1 stated the retrieval route and said
`RS14` is a `literature`-kind manifest entry; the manifest entry itself was not actually added
until this review found the gap. Added now (`manifest/manifest.json`), with a terms search of
the same depth as `LI19`'s (§2): neither the PDF's own text nor the mediaTUM landing page states
a licence; the landing page's 18 "copyright" hits are all the same repeated template comment
about the mediaTUM repository software, not the thesis. Verified against `tools/fetch.py verify`
and `check-licences`.

### 27.5 What v1.0 of this section claimed, and what changed

`SRPA` v1.0 covered exactly what the relocation carried — the cannonball and flat-plate
force laws, already reviewed as part of step 2 — and stated box-wing itself (`SRPA-Q-001`)
as not yet attempted. §27.6 closes it. `RS14`'s Tables 1–2, the real GPS optical properties
a *populated* box-wing macromodel will need, are still read by no test in either module;
they remain L5's population, not step 3's.

### 27.6 Box-wing needed no new force law, because it needed no new physics

`SRPA` v1.1. `SRPA-Q-001` closed: `SRPA-R-008`, `SRPA-A-009`, `SRPA-A-010`.

**Rule 4's first half, applied to `RHS12` before writing a single line of gate.** Does the
paper print a closed-form, force-level expected value — a number, from stated a-priori
inputs, at a stated geometry — that a box-wing implementation could be checked against
directly? Read in full (§25.1's route, all of Chapter P-II, pp. 85–101): **no.** §8
"Reconstruction of SRP acceleration" (Fig. 11) plots reconstructed accelerations for PRN06
(Block IIA, doy 102) and PRN17 (Block IIR, doy 104) at β₀ ≈ 15° — the closest the paper comes
— but that reconstruction combines Table 1/2's a-priori *dimensions* with parameters
**estimated by fitting one year of real GPS tracking data** (Fig. 5), not the a-priori
optical properties alone, and states the result only as a graph, with no printed numeric
value to transcribe. Everything else in §§6–8 — pseudo-stochastic pulse reduction, orbit
overlap and 7-day prediction error, SLR-GPS radial bias — is an **orbit-level** residual,
several steps downstream of a raw force through a full numerical integration and a
comparison against independently-determined "truth," which this layer does not perform and
should not reach for just to manufacture a test case. **There is no category-1 published
test case for box-wing**, and none was invented.

**So the gate is composition, per plan rule 8's converse.** RHS12's own definition — "a
satellite bus (box shape) and solar panels" — is, in this schema, simply a `Macromodel` with
several `FlatSurface`s: one `sun_pointing` (the panels) and several `body_fixed` (the bus).
`srp_force` already sums its two force laws over every surface a `Macromodel` holds, for any
count — that loop was written generally in step 2 and had never been exercised past *N* = 1.
Box-wing is a sum of a law already derived from first principles and already gated
(`SRPA-A-001`…`-A-008`), which gives it an independent specification **by construction**:
checkable against arithmetic that involves no citation to `RHS12` at all, only to this
module's own already-reviewed R-001/R-003.

**Two composition properties, both closed-form identities (route 2), not self-consistency
checks (route 4):**

1. **Linearity** (`SRPA-A-009`): a 4-surface box-wing-shaped macromodel (one sun-pointing
   panel, three body-fixed bus faces — areas and optical properties stated in the test,
   loosely shaped like Table 1's Block IIA row but not equal to it, and not cited to
   `RS14`, precisely so a reader cannot mistake this for L5's population) evaluated whole,
   against the vector sum of the same four surfaces evaluated one at a time as their own
   single-surface macromodels. Exact to the bit at every Sun direction tried — unsurprising,
   since both routes execute the identical floating-point sum in the same order, but the
   **structural** claim (that the *N*-surface loop is not, say, silently short-circuiting,
   double-counting, or dividing by count instead of summing) is what a test with only
   single-surface macromodels before this step could not have shown.
2. **Mixed-domain composition** (`SRPA-A-010`): three body-fixed bus faces (+X, +Z, −Z —
   `SRPA-R-008a`'s own four-surface convention, minus the sun-pointing panel) at a Sun
   direction chosen so +X and +Z are lit and −Z is not, asserting the whole equals the sum
   of the two lit surfaces alone. **Not vacuous**: −Z's own isolated contribution is
   asserted exactly zero and +X's and +Z's are asserted strictly positive, in the same test,
   so a version of this test that accidentally lit every surface (or shadowed all of them)
   would fail its own precondition checks before ever reaching the composition assertion.

**Why RHS12's box-wing uses four surfaces and not six** (`SRPA-R-008a`): Tables 1 and 2 list
only *solar panels*, *+X bus*, *+Z bus*, *−Z bus* — no *−X* or *±Y* row. Under the nominal
Sun-tracking yaw attitude `RHS12` Fig. 1 defines, −X and both Y faces never face the Sun, so
they would be multiplied by zero at every evaluation and the paper simply does not state
optical properties for them. This schema did not need to be told that separately: a caller
who supplies only four `FlatSurface`s already gets this convention, because `SRPA-R-001`'s
own cos θ < 0 domain excludes whatever happens to face away, for any surface count.

**What this still does not claim.** The nominal attitude LAW — which body-fixed direction
each bus surface's normal points at a given orbit position, as a function of the D/Y/B
Sun-fixed frame `RHS12` Fig. 1 defines — is not implemented and is out of this spec's scope
regardless of surface count (§1); `srp_force` takes `sun_direction_body` as given, one
surface or several. And no real GPS satellite's dimensions or optical properties are stated,
cited, or pinned anywhere in this commit — `RS14`'s Tables 1–2 remain L5's, in full.

### 27.7 Composition cannot see the law it composes, and a tessellated sphere can

§27.6 closed box-wing with a composition gate and called it done. It was not: `SRPA-A-009`'s
two sides call the identical per-surface code, so a bug **in** `flat_force` appears
identically on both sides of the comparison and cancels — a wiring check, correctly, and
a wiring check cannot see the law it wires together. `SRPA-A-004`, the only single-surface
flat-plate test, made the gap worse than it looked: `sun_pointing` and black, so cos θ ≡ 1
and ρ = δ = 0 collapse every structural feature of Eq. (6) — the *e*ᴰ/*e*ᴺ split, and the
*extra* power of cos θ the specular term carries — onto one number. Found by the manager,
not by this session; the check is added here, `SRPA-R-009`, `-R-010`, `SRPA-A-011`…`-A-013`.

**Two closed-form single-plate checks, verified before being written into the gate.** A pure
absorber at oblique incidence gives *P A* cos θ along −*e*ᴰ; a pure specular reflector gives
2*P A* cos²θ along −*e*ᴺ. Both are re-derivable from the same momentum bookkeeping `SRPA-R-002`
already used (an absorbed photon transfers *p* = *E*/*c*; the flux crossing a tilted surface
is itself reduced by cos θ; a specular bounce off a tilted mirror reflects with an OUTGOING
angle equal to the incoming one, so the NORMAL component of momentum transferred per photon
carries a second cos θ, and the flux reduction a third — cos³θ per photon count, but the
recoil is along the normal and the flux-weighted count restores one power, giving cos²θ net
along −*e*ᴺ, the standard result for a tilted mirror). `SRPA-A-011`/`-A-012` check both
magnitude **and direction** exactly, since a term/direction swap (e.g. the absorber's force
accidentally computed along −*e*ᴺ) would leave a magnitude-only check unable to tell the
difference.

**The tessellated-sphere cross-check's discretisation error was measured before the gate was
written**, not assumed (plan §4 rule 7's middle form). A latitude/longitude grid of *n* polar
bands, each facet's normal at its cell centre and its area the cell's exact solid angle,
gives empirically **second-order** convergence — doubling *n* quarters the relative error,
measured at the sharpest (pure specular) case over five consecutive doublings:

| *n* (facets) | rel. error | error(*n*)/error(2*n*) |
|---|---|---|
| 4 (32) | 8.24 × 10⁻² | — |
| 8 (128) | 1.96 × 10⁻² | 4.21 |
| 16 (512) | 4.84 × 10⁻³ | 4.05 |
| 32 (2048) | 1.21 × 10⁻³ | 4.01 |
| 64 (8192) | 3.01 × 10⁻⁴ | 4.00 |
| 128 (32768) | 7.53 × 10⁻⁵ | 4.00 |

The ratio settles to 4.00 and stays there; pure-diffuse and mixed triples reproduce the same
table to three figures. `SRPA-A-013` uses *n* = 32 and 64 — inside the range this was
checked over, not beyond it — and asserts both the absolute error against a margin above
the prediction **and** the ratio against [3.0, 5.0]. The C++ gate's own numbers at *n* = 32
matched this Python prototype's to five decimal places (diffuse: 1.08265 × 10⁻³ both ways;
mixed: 1.15884 × 10⁻³ both ways) — an independent-language cross-check of the prediction
itself, before it was ever used to bound a test.

**The guard was proven by making it fire, in the place it fires from** (plan §4 rule 5).
`e_N_coeff`'s `rho * cos_theta` was changed to `rho` — dropping exactly the power of cos θ
the manager's rule-4 analysis named — and every other file left untouched. Result:
`SRPA-A-012` (the single-plate check) failed with a 2.86× magnitude error at one angle,
directly. `SRPA-A-013` (the tessellated sphere) failed both ways at once: pure-specular error
at *n* = 32 was 0.3344 — matching the hand-derived 1 + ρ/3 = 4/3 prediction (§27.6) to four
figures — **and** the convergence ratio collapsed from the predicted ~4.0 to 1.00, because a
bug in the LAW does not shrink with discretisation resolution the way genuine discretisation
error does; both resolutions carry the same ~33 % error, so their ratio is ~1. Either
assertion alone would have caught this specific bug; together they do not depend on which one
a different bug might happen to dodge. Reverted immediately after (`diff` against a saved
copy confirmed byte-identical restoration) and the full suite re-run clean, 92 636 assertions.

---

## 28. L4 step 4 — drag, and a Jacobian bug a finite difference caught that the closed form's own author could not see in it

`SPEC-drag.md` v1.1, Spec ID `DRAG`, `modules/drag`. Gated by `DRAG-A-001`…`-A-010`.
`SPEC-dynamics.md` amended to v1.2 (`DYN-R-051`, §28.8). Depends on `core`, `time`, `eop`,
`frames`, `atmosphere`, `dynamics`; nothing later built yet.

### 28.1 What was built, and the order it was built in

The drag force over L2's atmosphere (`atmosphere::for_drag`), with the drag coefficient a
**registered parameter** (`dyn::ParameterKind::drag_coefficient`, declared at L3 step 1 — before
this module existed, confirmed present by inspection rather than assumed) and never a compiled-in
constant. Two module-local utilities this module needed and no existing one provided, checked by
grep before being written: `geodetic::itrs_to_geodetic`/`geodetic_to_itrs` (Bowring's iterative
method, WGS84) and `detail::day_of_year` (the Gregorian cumulative-month-day calculation).

**Recorded plainly:** this step's implementation and test suite were written before
`SPEC-drag.md` itself, the reverse of every other L4 step's order. The design reasoning the
spec's §4/§9 describe — the Jacobian derivations, the co-rotating-atmosphere identity, the
provenance-carrying discipline — happened alongside the code, not as a separate checkpoint
before it. Flagged in the spec's own header and here, not smoothed over.

### 28.2 The rule-4 search: no clean published ballistic-coefficient case, and what was found instead

Before the gate was designed, per the manager's explicit instruction. Searched for a published,
force-level drag test case — a stated ballistic coefficient, atmosphere and state producing a
citable acceleration a test could assert equality against, the shape `RHS12`'s box-wing case
turned out NOT to have at step 3. Drag has the same absence, for the same reason: a real
satellite's drag acceleration is a fitted campaign result, not a closed-form published number.
DTIC's two named alternatives (ADA285118, AD0464391) both returned HTTP 403 — out of reach under
the no-contact-anybody constraint, not merely unread.

**What was found instead:** Sengers, Lin Wang, Kamgar-Parsi & Dorfman (2014), arXiv:1404.7826,
"Kinetic Theory of Drag on Objects in Nearly Free Molecular Flow." Table 4 prints the
free-molecular sphere drag coefficient *C*₀(*S*) as a function of speed ratio *S*, for
*S* = 0 … 50 and the *S* → ∞ limit (stated in the paper's own text to be exactly 2). This is a
genuine published drag-*model* case, but not a case THIS tree's force law can be checked
against directly: the paper characterises an idealised sphere in free-molecular flow in general,
not any specific satellite's macromodel, so no single printed number is "the" answer a force-law
test could assert equality against. Used instead as `DRAG-P-2`'s plausibility range (2.0–2.4 for
the LEO regime) on `DRAG-A-002`'s own **stated** *C*_D — a sanity check on the test's input, not
a registered test value, and named as such rather than dressed up as more than it is.

**The terms search found something, for the first time in this tree's three literature
entries.** `li-ziebart-2019-shadow` and `rodriguez-solano-2014-dissertation` both searched and
found nothing — no licence statement anywhere reachable. This one's search reached
arXiv's abstract page, which links "view license" to the standard arXiv non-exclusive
distribution license; that license's own text (fetched directly, not assumed from its name)
grants **only arXiv.org** the right to distribute, nothing to a third party. The exemption
(pinned by hash, bytes never redistributed) rests on the same footing as the other two entries,
now stated explicitly rather than inferred from an absence. Manifest entry
`sengers-2014-drag-coefficient`, `data/literature/sengers-2014-drag-coefficient/1404.7826.pdf`,
SHA-256 `614efc89…5d9f7c`.

### 28.3 The velocity Jacobian, verified by direct differentiation

`DRAG-R-003` claims ∂/∂*v*ⱼ(*vᵢ*|**v**|) = δᵢⱼ|**v**| + *vᵢvⱼ*/|**v**|. Verified here by direct
differentiation, not assumed from its stated form (the code comment's own promise): with
*f* = *vᵢ*|**v**| and |**v**| = √(*vₖvₖ*),

> ∂*vᵢ*/∂*vⱼ* = δᵢⱼ (trivially), ∂|**v**|/∂*vⱼ* = *vⱼ*/|**v**| (the standard derivative of a
> Euclidean norm), so by the product rule
> ∂*f*/∂*vⱼ* = (∂*vᵢ*/∂*vⱼ*)|**v**| + *vᵢ*(∂|**v**|/∂*vⱼ*) = δᵢⱼ|**v**| + *vᵢvⱼ*/|**v**|.

Since **a** = coeff · **v**_rel|**v**_rel| with coeff independent of **v**_rel (density is not a
function of velocity — the whole of the velocity dependence is through this one product), this
tensor times coeff is the *entire* velocity Jacobian, no approximation. `DRAG-A-004` checks it
against a central finite difference on the *C*_D column (a different, independently-checkable
column of the same `ParameterJacobian`/`StateJacobian` machinery) to 1.18 × 10⁻⁸ relative or
better; the tensor form itself was additionally hand-verified component-by-component against
this derivation before being trusted.

### 28.4 The position Jacobian's missing factor of |v_rel| — found by the finite difference this step's own coverage gap forced into existence

`tools/speccheck.py`, re-run after the spec was written, reported `DRAG-R-004` (the position
Jacobian) **uncovered** — no acceptance row's `discharges` column named it. `DRAG-A-005`
(Liouville) was not, despite appearances, a substitute: `tr(A)` for the augmented 6×6 matrix
`[[0, I], [da/dr, da/dv]]` picks up only the **diagonal** blocks, and `da/dr` sits **off** the
diagonal — Liouville is structurally blind to the position Jacobian's correctness. Nothing before
this point had actually exercised `DRAG-R-004` at all.

`DRAG-A-010` was written to close the gap: perturb the satellite's GCRS position by ±10 m along
the radial direction (the same physical direction in GCRS and ITRS, since the two frames share
an origin and differ only by a rotation), take a central finite difference of the **real**
`acceleration()` call, and compare against `Drag::accel`'s own reported `d_state.d_position()`
applied to that direction.

**First run: 99.99 % relative deviation — the two vectors agreed on essentially nothing.**
The finite difference's *y*-component was 4.669 × 10⁻¹⁰ s⁻²; the analytic prediction's was
6.374 × 10⁻¹⁴ s⁻² — a factor of **≈ 7330** too small. The orbital speed at the test's 300 km
case is ≈ 7726 m s⁻¹. Those two numbers are close enough that the ratio itself was the
diagnosis, not merely a symptom of one: `drag.cpp`'s `a_direction` (the factor `dadr_itrs` is
built from, meant to be ∂**a**/∂ρ) read

```cpp
const Vec3 a_direction = (coeff / rho) * v_rel;
```

with the comment "a = coeff*rho*v_rel, so d(a)/d(rho) = a/rho = coeff*v_rel" — but the actual
force law (`acceleration()`, and `coeff`'s own definition three lines above this one in the same
function) is **a** = coeff · |**v**_rel| · **v**_rel, not coeff · **v**_rel: `coeff` was
deliberately defined *without* a speed factor, for the velocity-Jacobian tensor's convenience
(§28.3), and that same bare `coeff` was reused here without reinstating the factor the
*different* derivative needs. Since ρ enters **a** only through `coeff`, linearly,
∂**a**/∂ρ = (coeff/ρ) · |**v**_rel| · **v**_rel — missing exactly the `* speed` this draft
omitted. Fixed by adding it; the comment's own false premise ("a = coeff\*rho\*v_rel") corrected
alongside the code, per rule 3 — the error was in the stated relationship the code followed, not
only in one term of the code.

**After the fix: 1.18 × 10⁻² relative deviation**, same sign, comfortably inside a tolerance
(5 × 10⁻²) chosen with a 4× margin above the measured value rather than loosened to whatever the
first passing run happened to produce. This residual is not itself a defect: a true finite
difference of `acceleration()` also picks up a channel `DRAG-R-004`'s analytic form does not
model at all — **v**_rel has its own weak dependence on position (the GCRS↔ITRS velocity
transform's transport term depends on **r**), which bumping only altitude at fixed **v**_rel
cannot see. §28.5 gives what can honestly be said about that residual's size.

**What this is, named plainly:** a real defect in a closed form the executor derived, wrote, and
found plausible on inspection, caught only because a finite difference of the actual running
code was compared against it rather than trusted by construction — and caught only because
`speccheck.py`'s coverage accounting forced the comparison to be written at all. Rule 4's own
register question — is the bar for an entry met — is met without qualification here: the
guard existed, was wrong, was silent (no test exercised it), and the wrongness was large (three
orders of magnitude, not a rounding disagreement).

**This was not the end of the story.** The 1.18 × 10⁻² residual and the 5 × 10⁻² tolerance set
from it (below, §28.5's original text) were themselves reviewed and found wanting — a second
defect, in the check rather than in `drag.cpp`. §28.5 is corrected, not silently replaced: what
it said, and why it was wrong, are both kept.

### 28.5 §28.4's own tolerance, corrected: what "1.18e-2, comfortably inside 5e-2" actually was, and what channel 2 turned out to require

**What v1.0 of this section said**, kept for the record: two channels `DRAG-R-004`'s
radial-only, fixed-**v**_rel approximation did not model — the lateral density gradient, bounded
by an order-of-magnitude length-scale argument at roughly two orders of magnitude below the
retained radial term; and **v**_rel's own weak dependence on position, for which "`DRAG-A-010`'s
post-fix residual, 1.18 × 10⁻² relative, is the best empirical bound this tree has on it today."
**That framing was the defect.** The residual was read as a bound on an unmodelled channel,
when it was in fact dominated by something neither named channel was.

**The manager's review, in order.** (1) A residual that had not moved from a forward difference
(the very first, `\|`**v**_rel`\|`-missing draft's own comparator) to this fix was suspicious on
its face: switching to a central difference should have landed near the textbook
(Δ/*H*)²/6 ≈ 1 × 10⁻⁴ at Δ = 1 km, an order of magnitude below what was measured and attributed
to "the channel this test cannot isolate." (2) The channel-2 (**v**_rel-transport) term's own
expected size, 2*ω H*/|**v**_rel| ≈ 8 × 10⁻⁴, did not match either explanation cleanly. (3) The
falsifiable test offered — halve the step; a first-order term should roughly halve — was run
before anything else, per instruction, and measured 3.79 × 10⁻³, not the ~5.9 × 10⁻³ a clean
first-order model predicts but a clear, large drop consistent with SOME step-dependent
mechanism, not a fixed floor.

**Isolating the two channels properly settled it.** Channel 2 was verified independently of
channel 1 — hold ρ fixed at its base value (no atmosphere re-sampling) and finite-difference
only **a**(**v**_ITRS(**r**)) through the REAL, non-approximated `to_itrs` velocity output — and
the analytic formula ∂**a**/∂**v**_rel · (−[**ω**]ₓ) matched this isolated true value to **6
significant figures**, settling that channel 2's own formula is exact, not merely small. A
direct comparison of `(gap = fd_da − channel1_computed, the CENTRAL-difference channel 1)`
against channel 2's own computed prediction, naively expected to explain that gap, instead showed
the two roughly ANTI-aligned and channel 2's magnitude about **10× LARGER than that (central-
difference) gap** — smaller than the ORIGINAL forward-difference residual (1.18 × 10⁻²) this
section's own v1.0 blamed it for, and larger than the gap left once channel 1 alone was already
switched to a central difference. Both comparisons are correct, against two different gaps; the
second is the one that mattered here — the tell that channel 1's own estimate, not channel 2,
still carried an error large enough to matter.

**A step sweep at 1, 0.5, 0.25, 0.1 and 0.05 km, on channel 1 alone (isolated the same way),
found why**, and it is not what either original explanation assumed. Channel 1's true error is
**non-monotonic**: 9.50 × 10⁻⁴ at 1 km, **worse** at 0.5 km (1.74 × 10⁻³) and 0.25 km
(3.47 × 10⁻³), then dropping sharply to 1.19 × 10⁻⁶ at 0.1 km and 2.97 × 10⁻⁷ at 0.05 km. Smooth
Taylor truncation is monotonic in the step by construction; this is not smooth truncation. The
match is `SPEC-atmosphere` §3.1's own description of NRLMSISE-00: a **fitted cubic spline** in
altitude. A 0.25–1 km step spans enough of that spline's own structure to alias against it; a
0.1 km step sits stably past it, past this Jacobian's own dominant remaining approximation (the
lateral gradient, still ~1 % by the order-of-magnitude argument above, kept from v1.0
unchanged), so refining further buys nothing this Jacobian can use.

**Fixed at the source, in `drag.cpp`, not by loosening the test.** Channel 1's altitude step
became a **central** difference at a 0.1 km half-step (`DRAG-P-1`, revised); channel 2 (the
**v**_rel-transport term, verified above) was added to the production Jacobian, not left as an
unmodelled bound. `DRAG-A-010` was rebuilt around a **stability check** — channel 1 recomputed
independently at the registered step and at half of it must closely agree — in place of the
formula-based prediction that turned out not to describe the real profile: stability is
operationally what "past the spline structure" means, and it is exactly the property whose
ABSENCE the 1–0.25 km sweep exhibited.

**Post-fix, both channels together, against the real finite difference: 1.19 × 10⁻⁶ relative
deviation** — four orders of magnitude tighter than the 1.18 × 10⁻² this section's own v1.0
called "comfortably inside" its tolerance, and consistent with channel 1's own isolated
0.1 km-step error (1.19 × 10⁻⁶, this section's own step-sweep figures above) to three
significant figures, meaning channel 2's contribution to the residual is not separately visible
at this precision —
the isolated 6-figure match above is the stronger statement about channel 2 on its own. The
stability check (0.1 km against 0.05 km) measured 8.95 × 10⁻⁷ relative difference, comfortably
inside its own 1 × 10⁻³ bound and nowhere near the ~10⁻¹-to-1 scale the non-converged 1–0.25 km
regime showed.

**The lateral-gradient bound (v1.0's other channel) is unchanged** by this correction: it was
never the residual's own explanation (a purely radial test perturbation at the equator cannot
excite it, argued geometrically, not merely by omission), and nothing in this review touched it.

**Correction (step 4's second closing review, 2026-09-23).** The paragraph naming `SPEC-atmosphere`
§3.1's "fitted cubic spline" as the cause of channel 1's non-monotonic 1–0.25 km error **is
itself wrong**, caught the same way this section's own v1.0 defect was: by checking the claim
against the primary source instead of trusting the explanation that fit the numbers. Two things
were wrong with it at once. First, the citation: the relevant text is `SPEC-atmosphere` §3.6, not
§3.1, and it says plainly that no spline node exists above 120 km — this test's own case sits at
299.863 km, well above that boundary, so a spline had no knots there to alias against. Second, and
more fundamentally, the arithmetic on this section's own step-sweep figures above rules out any
smooth structure, fitted or otherwise: computing `error × 2Δ/H` (*H* ≈ 42.4 km at this case) over
1, 0.5 and 0.25 km gives 4.06 × 10⁻⁵, 4.06 × 10⁻⁵ and 4.04 × 10⁻⁵ — CONSTANT to three figures. A
smooth term of any order leaves `error × 2Δ/H` varying with Δ; only a fixed jump *J* crossed by the
stencil leaves it constant, at *J*/ρ ≈ 4.05 × 10⁻⁵. That is the 1/Δ signature of a discontinuity,
not the Δ² signature truncation (spline-aliased or textbook) would leave, and it is what the
0.1/0.05 km pair's clean 4.01× ratio (the textbook second-order halving) confirms by contrast: past
the jump, ordinary truncation is exactly what remains.

A sweep-and-bisect diagnostic, run the way this correction's own review round required it — bounded
first, before anything in `drag.cpp` changed — found the jump at exactly 300.000 km, 300 km being
`DRAG-A-005`'s and `DRAG-A-010`'s shared test orbit's near neighbourhood (299.863 km, 137 m below
it) rather than its exact altitude, which is why 0.1 km's fixed half-step (100 m) happened to clear
it: 299.863 + 0.100 = 299.963 km, still 37 m short of 300.000 km. `DRAG-P-1`'s "0.1 km sits stably
past that structure" was correct about the number and wrong about why — it sits past THIS case's
own nearest cutoff by 37 m of margin this section never measured, not past a spline that recedes
everywhere a finer step is taken. A satellite 37 m closer would not have had that margin.

Reading `MSIS-FOR`'s own `DATA ALTL` (line 587) found the real mechanism: **seven** hard
species-correction cutoffs above 120 km, one per species, each switching a mixing-corrected density
below it for the raw diffusive-only value above — not a spline node, not an artefact of this port,
confirmed identical against the frozen reference to full double precision at every one. §28.10
below has the full mechanism, the jump measured at all seven (not only the one this section
bisected), the runtime fix `DRAG-R-004`'s channel 1 needed as a result, and the correction this
forced in `SPEC-atmosphere` §3.6 — a layer this project had already closed. Nothing else in this
section is affected: the channel-2 isolation, the step-sweep numbers themselves, the 0.1 km
choice, and the post-fix deviation all stand exactly as measured. Only the NAME of what caused the
1–0.25 km non-monotonicity was wrong, and only at this one case's own margin was it ever right by
more than luck.

### 28.6 DRAG-A-005's own history: an orbit too high to show what it was built to show

The first draft of the Liouville test used this tree's usual 7331 km test radius (the same one
`stm_tests.cpp`'s own `Setup`, `l2_floors.cpp`, and every other L4 step's tests use). At that
altitude (≈ 953 km) `DRAG-A-002` independently measured ρ ≈ 2 × 10⁻¹⁵ kg m⁻³ — thin enough that
even a deliberately aggressive 5 m² kg⁻¹ area-to-mass ratio left `det(Phi)` at 1 − 10⁻⁷ after
300–900 s of propagation, indistinguishable from the integrator's own 10⁻¹¹ tolerance without a
much longer run. The test was rebuilt at 300 km — a genuinely low, dense orbit — rather than the
threshold loosened to accept the thin-atmosphere result: `det(Phi)` then reads 0.999129 at 300 s
and lower at 900 s, a decay several orders of magnitude clear of integration noise, and a
monotonicity check (longer propagation, more decay) was added once the signal was large enough
for that comparison to mean something. The same discipline §27.6's bug-injection proof and
`SRPA-P-3`'s pre-registered convergence study already applied: a test whose passing condition the
configuration cannot fail to produce has not tested anything (plan rule 5's sibling, named this
session before drag existed).

### 28.7 Smaller things, caught before or during the run

- **A frame mismatch in this module's own test code, not in `drag.cpp`.** `DRAG-A-002`'s first
  draft compared `result->acceleration_m_s2` (GCRS, `DragResult`'s own stated convention)
  directly against an "expected direction" built from `v_rel` in the **ITRS** frame the force
  law is actually evaluated in — two different frames wearing the same units, differing by
  whatever the GCRS↔ITRS rotation happens to be at the test's epoch (effectively an arbitrary
  angle, the Earth having rotated many full turns since any fixed reference). `dir_dev` came
  back at 1.40, nowhere near the two vectors actually agreeing. Fixed by rotating the expected
  direction through the same `M`ᵀ `drag.cpp` itself uses before comparing — now stated as this
  module's own convention in `SPEC-drag` §3, so a future test does not repeat it.
- **The `dyn::` namespace, not `dynamics::`.** `modules/dynamics/include/odl/dynamics/*.hpp` all
  declare `namespace odl::dyn`, despite the module and directory being named `dynamics`. Caught
  by the compiler (≈15 errors) before anything ran, from a first draft written from memory of
  the module's shape rather than its headers.
- **A km/metre crossing, caught before building.** `frames::State<F>::position()`/`velocity()`
  return plain `Vec3` in **kilometres** (not a `Position<F>`/`Vector<F>` sub-object with its own
  `.metres()` accessor, which is `frames::Position<F>`'s and `frames::Acceleration<F>`'s shape,
  not `State<F>`'s); a first draft read them as if they were the latter. `DYN-R-011`'s own
  discipline — a named crossing at every site — is what made the mismatch visible on inspection
  rather than as a silent factor-of-1000. Fixed at both crossing sites in `drag.cpp` before the
  module was built.
- **Two tool summary lines, mislabelling every literature entry, including this step's new
  one.** `tools/fetch.py check-licences` and `tools/literaturecheck.py` both derive a literature
  entry's printed terms-summary from `e.get("licence", ...)` — a field literature entries never
  carry (they carry `terms`) — so both always printed the fallback ("none established"/"NOT
  established") regardless of what the entry's `terms` field actually said. Harmless for the
  two pre-existing entries, where "nothing found" happened to be the true state; actively
  misleading for `sengers-2014-drag-coefficient`, whose search **did** find and quote an
  explicit (non-open) licence. Both tools corrected to read `terms` and distinguish the two
  states this tree's entries actually have; no gate logic changed, only what the summary line
  says. Found only because this step's own new entry gave the pre-existing bug a case where its
  wrong answer was visibly wrong.
- **A citation corrected against the spec it was borrowed from, not re-derived.**
  `geodetic.hpp`'s comment cited WGS84's semi-minor axis *b* to "Table 3.1"; `SPEC-gravity`'s
  own §9 provenance register — the tree's existing, already-verified citation for the same
  constant — gives Table 3.6. Corrected to match rather than left as a second, disagreeing
  citation of the same number.

### 28.8 The provenance boundary stopped at the L3 plugin, and that was not this module's own gap to find alone

`ForceEvaluation` — `{acceleration, d_state, d_parameters}`, frozen before this module existed —
had no field for what a force's evaluation drew on. `DragResult::atmosphere_record` existed on
`acceleration()`, the free function; `Drag::accel`, the `Force`-plugin surface, had nowhere to
put the same information. Every consumer reaching drag only through the plugin — and L7's
estimator, when it is built, is exactly such a consumer, since it only ever sees a
`ForceEvaluation` — would have lost the snapshot identity and the verification flag entirely on
the way through, making `SPEC-atmosphere` `ATMO-R-031`'s mechanism decorative past this one
boundary. `DRAG-A-007`'s own v1.0 row documented the gap precisely rather than hiding it
("`Drag::accel` calls the free function with its default `require_verified=false`... a caller
wanting the refusal through the Force interface would need a variant that requests it") — visible
enough to raise as `DRAG-Q-001`, not visible enough, on its own, to be recognised as reaching all
the way back to a frozen L3 type.

**Why a trivial-force gate could not have caught this**, stated as the general lesson, not only
this instance: `SPEC-dynamics` §1 gates `Force` "with a TRIVIAL force so the surface is not shaped
by its first client." A trivial force proves a surface does not *require* what the trivial force
lacks — `ForceEvaluation`'s three original fields sufficed for a force with no parameters, no
provenance, nothing external to declare, and every test built against that trivial force passed.
It cannot prove the converse: that the surface *can carry* what a *real* force, drawing on another
layer's own provenance, must carry. That gap is invisible until a force exists that needs the
thing the trivial force never asked for — exactly the shape `DYN-R-026`'s own coverage-exclusion
row already named for the integrator ("there is no integrator until step 2... testing it here
would test a stub"), now found again one layer up, in the surface itself rather than in a
downstream consumer of it.

**Fixed additively**, under `DYN-Q-001`'s own already-ruled terms for editing a closed layer:
`SPEC-dynamics` `ForceEvaluation` gains an `optional<Provenance> provenance` field (`Provenance`
= `{source_id, source_sha256, verified_against_issuer}`), absent by default — `DYN-R-051`. Every
existing `ForceEvaluation{...}` construction site (six, all in `modules/dynamics/tests/`, all
3-argument aggregate initialisation) compiles unchanged, and the full `dynamics` suite was
re-run: 236 assertions, 17 test cases, behaviour unmoved. `Drag::accel` populates it from the
same `atmosphere_record` `acceleration()` already carries — one mapping, at the one place the
free function's richer record meets the plugin's narrower field, not a second computation.

**`require_verified` closed the same way, and for the same reason it had to close together with
the field rather than separately.** A construction-time `bool require_verified = false` was added
to `Drag`'s constructor (default preserves every existing call site); `Drag::accel` now calls the
free function with its own stored value instead of the free function's own default. A
construction-time flag ALONE — reachable refusal, without a readable field — would have let a
caller demand `DRAG-F-001` on the failing path while still losing the verification flag on the
far more common SUCCEEDING one; the provenance field alone would have carried the flag without
giving a caller any way to make the refusal itself reachable. Ruled and fixed together
(`DRAG-Q-001`), not as two independent tickets, because the review that found one found the other
by asking the same question of the same boundary: *what does a caller who only ever sees a
`ForceEvaluation` actually get?*

`DRAG-A-006` and `DRAG-A-007` were rewritten to test the closed boundary directly: `DRAG-A-006`
now checks `ForceEvaluation::provenance` in both the pre-2004/unverified and post-2004/verified
directions, alongside the free function's own `DragResult`; `DRAG-A-007` constructs `Drag` with
`require_verified=true` and asserts `DRAG-F-001` fires **through `accel()`**, not only through
the free function — the exact path `DRAG-Q-001` found unreachable.

### 28.9 DRAG-F-003's three causes, and what asking to test each of them actually found

`DRAG-Q-002` asked whether `DRAG-F-003`'s one id, covering three internally distinct failure
sites, needed to carry its cause as more than free text, and whether each of the three could
actually be fired. Both halves of the question changed something.

**The structured half.** `DragError` — this module's own error type on `acceleration()`, never
`dyn::DynError` (`SPEC-dynamics`'s frozen, shared one, left untouched) — gained a
`cause: Option<Diagnostic>` field, populated at all three `DRAG-F-003` sites with the underlying
`frames`/`time` diagnostic, structured rather than only folded into `message`'s prose (plan §5
constraint 10).

**The "fire each cause" half found that two of the three cannot be fired, for a reason worth
having looked for rather than assumed.** Read directly against `transform.cpp`'s own source: `
to_itrs(state, eop, leaps)` computes `gcrs_to_itrs(state.epoch(), eop, leaps)` **internally**,
with those exact arguments, before doing anything else. `acceleration()` calls `to_itrs` first
(line ~78) and, much later (line ~120), calls `gcrs_to_itrs(t, eop, leaps)` again, directly, with
the identical `t`/`eop`/`leaps` — a deterministic function of arguments already proven to
succeed. The "rotation failed" cause is **provably unreachable**, not merely untried, as long as
that internal delegation holds.

The "calendar failed" cause looked, before checking, like it might be independently reachable —
build a pre-1972 `Epoch` by `Duration` arithmetic on an otherwise-valid one (since
`Epoch::from_calendar` itself refuses to construct one directly), bypassing the constructor's own
range check, and see whether `.calendar(UTC, leaps)` fails on its own. It does refuse — but
`acceleration()` never reaches that line: `to_itrs`, called first, needs UT1 (= UTC + ΔUT1) for
its own precession/polar-motion chain, which needs the identical TAI↔UTC rendering
`.calendar(UTC, leaps)` performs, and fails there first. Checked directly (`DRAG-A-008`), not
assumed from the rotation case's own reasoning: a pre-1972 epoch through `acceleration()` refuses
`DRAG-F-003` with `cause.id == "TIME-F-002"` — the transform's own cause — never a
calendar-conversion id.

**So `DRAG-F-003`'s three nominal causes are, in this module's own current control flow, one.**
Both nominally-separate causes are shadowed by the same earlier, at-least-as-strict check the
transform call performs before either later site is ever reached. `DRAG-Q-002`'s own second
condition — fire each cause in a test, or record its absence with a reason — is satisfied by
demonstrating the shadowing directly rather than by writing two tests that could never have
passed for the reason they were meant to test. Recorded here, and in `SPEC-drag` §9, as a
structural finding about the code as written, not a permanent property of the problem: a future
refactor that reorders these calls, or that gives `to_itrs` a narrower internal requirement than
`.calendar()`'s own, would reopen exactly the question this section closes, which is why the
reasoning is written out in full rather than left as a one-line "unreachable."

### 28.10 Seven cutoffs, not one: the jump table, the runtime fix, its size, and the layer above

The diagnostic §28.5's correction points to (bounded, reported, and run before anything changed,
per instruction) found the 300 km jump exactly, confirmed it against the live FORTRAN reference at
identical inputs to full double precision, and named its mechanism as one entry in `MSIS-FOR`'s
`DATA ALTL` (line 587): `200.,300.,160.,250.,240.,450.,320.,450.` The manager read the same line
and found it names **seven** cutoffs, not one, driving branches at N2 160 km (line 679), He 200 km
(699), Ar 240 km (813), O2 250 km (775), O 300 km (730), H 320 km (844) and N 450 km (880) — every
line number checked directly against the pinned source in this review, not taken on trust. The
eighth `ALTL` slot, `ALTL(6) = 450`, is a **separate**, excluded branch (line 662,
`IF(Z.GT.ALTL(6).AND.MASS.NE.28.AND.MASS.NE.48)`): it never fires for `MASS = 48`, this module's
own total-density path, so it is named here for completeness and excluded from everything below.
All seven live branches are gated by `SW(15)` — six combined on the same line as their altitude
test (`.OR.SW(15).EQ.0.`), O2's by a separate, preceding `SW(15)` test just above its own branch
(line 774) — and this tree runs `SW(15) = 1` throughout, so all seven are live for every case this
module or `atmosphere`'s own suite evaluates. Naming only 300 km, or only the six sharing one
syntactic form, would have been rule 3's own patched-term error one level up.

Four items closed step 4 against this finding, in the order the manager set them.

**1. The jump table.** Every cutoff, measured the same way the 300 km one was diagnosed — density
1 m below and 1 m above, at a circular equatorial orbit at this test's own epoch, and the
acceleration `acceleration()` itself computes at the stated `DRAG-A-010` ballistic coefficient
(*C*_D = 2.2, area = 10 m², mass = 500 kg) — not re-derived from the density jump, which would be
circular, but queried independently through the production force-law call:

| species | cutoff | ρ relative jump | \|**a**\| | accel jump | accel relative jump |
|---|---:|---:|---:|---:|---:|
| N2 | 160 km | 2.834 × 10⁻⁴ | 1.317 × 10⁻³ m/s² | 3.734 × 10⁻⁷ m/s² | 2.834 × 10⁻⁴ |
| He | 200 km | 6.454 × 10⁻⁵ | 2.994 × 10⁻⁴ m/s² | 1.934 × 10⁻⁸ m/s² | 6.458 × 10⁻⁵ |
| Ar | 240 km | 5.475 × 10⁻⁵ | 9.118 × 10⁻⁵ m/s² | 4.996 × 10⁻⁹ m/s² | 5.479 × 10⁻⁵ |
| O2 | 250 km | 5.220 × 10⁻⁵ | 6.950 × 10⁻⁵ m/s² | 3.631 × 10⁻⁹ m/s² | 5.224 × 10⁻⁵ |
| O  | 300 km | 8.754 × 10⁻⁵ | 1.991 × 10⁻⁵ m/s² | 1.744 × 10⁻⁹ m/s² | 8.758 × 10⁻⁵ |
| H  | 320 km | 4.488 × 10⁻⁵ | 1.255 × 10⁻⁵ m/s² | 5.640 × 10⁻¹⁰ m/s² | 4.492 × 10⁻⁵ |
| N  | 450 km | 4.997 × 10⁻⁵ | 8.486 × 10⁻⁷ m/s² | 4.244 × 10⁻¹¹ m/s² | 5.001 × 10⁻⁵ |

(reproduced by `modules/drag/tests/drag_tests.cpp`'s `[.][scratch]`-tagged "the seven-cutoff jump
table" case, which `WARN`s this exact table rather than asserting a value against it, since it is
a measurement recorded here, not a gate — the gate is `ATMO-A-028`, item 4 below).

**This table's O/300 km row, 8.754 × 10⁻⁵, is not the same number as §28.5's own bisection,
4.085 × 10⁻⁵ at `DRAG-A-010`'s own point — and it should not be, which this entry says explicitly
so a reader does not have to guess which one is wrong.** They are the same jump measured at two
different conditions: §28.5's bisection ran at `DRAG-A-010`'s own equatorial-adjacent case
(lat 0.0869 rad, lon −89.17°, 137 m below the cutoff); this table runs at a circular equatorial
orbit at this test's own epoch (lat 0, local time fixed by the epoch, not `DRAG-A-010`'s). The
species-correction density itself depends on latitude and local time, not only altitude, so the
SAME cutoff's relative jump is not one universal number — a factor of 2.14 between the only two
conditions measured here is the size of that dependence, not a discrepancy. Consequently, the
"4–28 × 10⁻⁵" spread below is the range ACROSS SPECIES at this table's one condition (the
circular equatorial orbit), not the general range any one species' jump can take across location
and local time — a different reader computing O's own jump at a different latitude should expect
a different number in the same 10⁻⁵–10⁻⁴ neighbourhood, not 8.754 × 10⁻⁵ exactly.

The relative density jump is the same order at every cutoff, 4–28 × 10⁻⁵ at this table's one
condition — unsurprising, since each is the same kind of switch (a mixing correction turning
off) — but the ACCELERATION jump falls by four orders of magnitude from N2 to N, monotonically
with altitude, because |**a**| itself falls off exponentially and the relative jump does not
compound that fall, it rides on top of it. A jump that looks the same size in every row of
`ATMO-A-028`'s own reference comparison is not the same size to a force law built on top of it.

**2. The stencil never straddles, at runtime.** `DRAG-R-004`'s channel 1 named its central
difference's half-step as `DRAG-P-1`'s registered 0.1 km without asking whether that half-step
could land on both sides of a cutoff — it can, for any evaluation altitude within 100 m of one,
which `DRAG-A-010`'s own case avoided by 37 m of unmeasured margin (§28.5's correction above), not
by construction. `drag.cpp`'s channel-1 block now checks the evaluation altitude against
`kSpeciesCutoffsKm` (all seven, named directly rather than re-derived) before differencing: if the
half-step would straddle the nearest one, the central difference is replaced by a one-sided,
second-order difference walking AWAY from the cutoff, staying on the same side as the evaluation
point —

> *f*′(*x*₀) ≈ (−3*f*₀ + 4*f*₁ − *f*₂) / (2Δ), *f*₀ the already-computed density at *x*₀ itself,
> *f*₁ and *f*₂ at *x*₀ + sign·Δ and *x*₀ + sign·2Δ, sign chosen to walk away from the cutoff

— which costs the same two extra `atmosphere::for_drag` calls the central difference already
makes, not one more, and reduces to the standard forward or backward second-order formula
depending on which side of the cutoff the evaluation point is on (checked algebraically, not only
numerically). The smallest gap between any two of the seven cutoffs (Ar 240 / O2 250, 10 km) is
two orders of magnitude above the 0.1 km half-step, so a stencil built to avoid the ONE nearest
cutoff cannot walk into a second one — the loop that finds `nearest_cutoff` relies on exactly this
and is written to assume at most one candidate. New requirement `DRAG-R-012`; new test
`DRAG-A-011`, built the way `DRAG-A-010` could not test this (its own case does not straddle):
channel 1 evaluated 0.05 km to each side of two real cutoffs (O at 300 km, N2 at 160 km — the
worst and best rows of item 1's own table), where the registered 0.1 km half-step would straddle
if the switch did not fire, comparing production `Drag::accel` output against a TRUE same-side
finite difference (a 10 m step taken entirely on the evaluation altitude's own side of the
cutoff, which no straddling stencil could match by construction). All four cases (two species,
two sides) agree to ≈1.3 × 10⁻⁵ relative deviation, well inside the existing 1 × 10⁻⁴ `DRAG-A-010`
bound applied at a genuinely adversarial case rather than a comfortable one. That bound is itself
a separation between two regimes, not a margin read off this measurement after the fact — the
switched stencil's own predicted error and the straddling error it replaces sit roughly two orders
of magnitude apart, with 1 × 10⁻⁴ between them (`SPEC-drag` `DRAG-P-4` has the derivation).

**3. The integrator effect, sized rather than asserted.** `0.5 · Δ**a** · Δt²` at the stated
Δt = 60 s, against the tolerance premise this tree has stated before (10⁻¹² relative on a
~6.7 × 10⁶ m GCRS position, ≈ 6.7 × 10⁻⁶ m absolute), using item 1's own accel-jump column:

| species | cutoff | 0.5 · Δ**a** · Δt² | × the 6.7 × 10⁻⁶ m premise |
|---|---:|---:|---:|
| N2 | 160 km | 6.722 × 10⁻⁴ m | **100×** |
| He | 200 km | 3.481 × 10⁻⁵ m | **5.2×** |
| Ar | 240 km | 8.993 × 10⁻⁶ m | **1.3×** |
| O2 | 250 km | 6.536 × 10⁻⁶ m | 0.98× |
| O  | 300 km | 3.139 × 10⁻⁶ m | 0.47× |
| H  | 320 km | 1.015 × 10⁻⁶ m | 0.15× |
| N  | 450 km | 7.640 × 10⁻⁸ m | 0.01× |

**Three of the seven, not one, clear the stated premise** — N2, He and Ar, the three lowest and
largest-jump cutoffs — and a fourth, O2, sits at 0.98× it, close enough that a slightly different
Δt or a slightly eccentric orbit's own radial-rate term could carry it over too. A rough estimate
at 300 km alone (≈4 × 10⁻⁷ m, this table's O row at 0.47×) is consistent with the measurement here
but understates the finding by naming only the one cutoff this diagnostic happened to start from —
this session's own earlier report made exactly that error, naming N2 as the only cutoff clearly
over, when the table above already showed three (the attribution is corrected here since the
report is not the record).

**And "three" is itself a property of the test spacecraft, not of the seven cutoffs.** The
integrator effect above is linear in the ballistic coefficient *C*_D·*A*/*m*, computed at this
table's stated 0.044 m²/kg (*C*_D = 2.2, *A* = 10 m², *m* = 500 kg) — not the spacecraft this tree
exists for. LightSail-2 (32 m² sail, 4.93 kg) is 325× that ballistic coefficient face-on and 84×
it at the smaller effective area this project already fitted for SRP (§27), so every entry in the
table above scales the same way:

| species | cutoff | test s/c (0.044 m²/kg) | × effective (84×) | × face-on (325×) |
|---|---:|---:|---:|---:|
| N2 | 160 km | 100× | **8 400×** | **32 600×** |
| He | 200 km | 5.2× | 440× | 1 690× |
| Ar | 240 km | 1.3× | 110× | 440× |
| O2 | 250 km | 0.98× | 82× | 320× |
| O  | 300 km | 0.47× | 39× | 150× |
| H  | 320 km | 0.15× | 13× | 49× |
| N  | 450 km | 0.01× | 0.96× | 3.7× |

At the effective area, SIX of the seven clear the premise (only N stays under, and barely); at
face-on, all seven do. "Three clear it" is true of the test spacecraft this module's own gates use
and no other; the tree's own target mission sees this as six or seven cutoffs, not three, and
N2/160 km alone at up to 32 600× rather than 100×. This is **sized, not solved, here**: it is
`SPEC-drag`'s own force law behaving exactly as `ATMO-R-037` says the atmosphere does, correctly
propagated at whatever ballistic coefficient a caller states — the question of whether an
estimator's own step control or event location should know about these seven altitudes belongs to
L7, later, and is recorded here as a forward pointer (`SPEC-drag` §9) rather than answered inside
this module, which has no step-control surface to answer it from. L7 should read this table's
LightSail-2 column first, not its test-spacecraft one: the plan's own L7 entry already carries this
integrator hazard as central to what L7 must handle, not a corner case a 300 km rough estimate
could be read to suggest.

**4. The atmosphere-module promotion.** The diagnosis that found the 300 km jump was this
module's own — a Jacobian-verification test bisecting a residual `atmosphere` itself never had a
gate that could see, because `ATMO-A-001`'s point-value sweep, however many points it used, cannot
see a jump BETWEEN points (§4 rule 3's own general form, restated here since it is what this whole
finding is an instance of: a point-value gate's denominator is its points, and where the source is
piecewise, its boundaries must be found by search and tested on both sides). Leaving the diagnosis
as this module's own private knowledge would leave `SPEC-atmosphere` §3.6 stating a falsehood
("the model continues upward without a structural boundary") that nothing in that module's own
suite could ever catch. Corrected there instead, on `DYN-Q-001`'s own terms (additive, nothing
existing silently changed, re-derived from the actual need): `SPEC-atmosphere` §3.6 now states the
seven cutoffs by name with a full table (new requirement `ATMO-R-037`), and a new gated test,
`ATMO-A-028`, straddles all seven directly against FROZEN reference output — not the live FORTRAN
binary, which `ATMO-Q-005` already ruled CI must never need — confirming both that the port agrees
with the reference to < 10⁻¹² on each side of every cutoff AND that the reference itself jumps by
more than 10⁻⁶ at every one, so a future change that silently smoothed one of these seven away
(in the port, by a well-meaning interpolation "fix") would fail this test on the second condition
even if it still passed the first.

Building `ATMO-A-028` required extending `msis_reference.py`'s sweep to straddle each cutoff (14
new altitude points, 10 m to each side of all seven), which regenerated
`msis_reference_values.hpp` at 181 records / 2172 comparisons (was 125 / 1500) and, in doing so,
surfaced two more instances of this same project's recurring shape — a derived description that
does not update when what it describes changes — inside the generator itself, found by checking
rather than assuming the generator's own hand-written prose still matched its new output (the same
check that first caught `SPEC-atmosphere`'s stale absence claim, now turned on the tool that
maintains that module's own reference data):

- `emit()` hardcoded the worst class-A comparison's description ("argon at 1000 km, published case
  3") and a threshold-sensitivity claim, both computed once by hand when the file was first
  written and never made to track the data. The extended sweep moved the worst comparison to a
  sweep point (anomalous O at 240.01 km) — not merely a different number, but a REVERSED claim:
  v1.0's text asserted the sweep does not loosen the bound, when the true, current fact is that a
  wider sweep LOOSENS it, which is what a sweep whose job is to find worse comparisons ought to
  do. Fixed by adding a `describe()` function and parameterising `classify()` on an explicit
  threshold, so both pieces of text are computed FROM the same data the header's own table is,
  not typed once beside it.
- The newly-dynamic lines this produced ran 106–132 characters against the file's own ~80–90
  character prose convention; wrapped with `textwrap` (already imported, unused) rather than left
  to overflow, so the fix did not trade a wrong static line for a correctly-worded but
  badly-formatted one.

`SPEC-atmosphere` §3.2/§3.3/§6/§8's own class-A counts and worst-comparison figures were
themselves derived text of exactly this shape, and were updated for the same reason: 1894 material
(was 1238), 48 immaterial (32), worst 7.8881 × 10⁻⁶ (was 7.6706 × 10⁻⁶, now a sweep point, not one
of the 17 published cases), 2172 total (1500) = 181 × 12 (125 × 12). §19.3 above, this project's
own original L2 closure record, is left exactly as written — it correctly described the sweep's
size when L2 closed, and an append-only history should not be quietly rewritten to agree with a
later re-measurement. §20.2's **"crossing every branch boundary"** is a different kind of claim
and gets a different treatment: it was FALSE when written, the same unsearched absence as
`SPEC-atmosphere`'s own former §3.1/§3.6 claim, since the 108-point sweep it describes never
crossed the `ALTL` cutoffs nobody yet knew existed — corrected with a dated note at §20.2 itself,
pointing here, not rewritten and not left silently wrong. This paragraph, and `SPEC-atmosphere`'s
own current §3.2/§3.3/§8, are where the current figures now live. `SPEC-drag` `DRAG-R-008`'s own
inherited tolerance citation was updated to match, for the same reason one level up.

**On the methodology, recorded because it generalised twice inside one closing round.** The
manager's own review here — verify every cited line number against the primary source directly
rather than trust a summary of it (all nine of this round's citations were re-checked against
`NRLMSISE-00.FOR` directly and matched); when a number changes, ask what DERIVED TEXT elsewhere
describes it and check, rather than assume, that text still agrees — is the same discipline that
found the `msis_reference.py` generator bug above, unprompted, while applying it to update
`SPEC-atmosphere`'s own prose for reasons the manager had already given. Two unrelated pieces of
stale derived text, caught by the same check, in the same closing round: not a coincidence this
project's own rule 3 and rule 4 exist, but a demonstration of why they do.

---

## 29. L4 step 5 — SRP and ERP over one shared photon-pressure kernel

`SPEC-photon-pressure.md` v1.0 adopted (Spec ID `PHPR`), `SPEC-macromodel.md` amended to v2.1,
`modules/attitude`, `modules/srp`, `modules/erp` built, `modules/srp_analytic`/`modules/macromodel`
amended additively. Gated by `PHPR-A-001`…`-A-017` (`A-002` discharged by `SPEC-srp-analytic`'s own
pre-existing suite continuing to pass unchanged, no new test needed for it) and `MCRM-A-013`/`-A-014`.
297 tests pass tree-wide at this entry's own close (up from the tree's own count when step 4 closed).

### 29.1 The scope amendment

No step had ever composed L4's own existing shadow function (step 1) and SRP force law (step 3)
into a working `dyn::Force` — the only force in the tree was `Drag`, and L4's exit gate cannot be
reached without one. The manager's own diagnosis, reached while step 5 was scoped as "`erp` alone":
building ERP's own cap integral would need the SAME photon-pressure kernel SRP itself still lacked
a plugin for, so step 5 was widened to build BOTH `srp` and `erp` as plugins over ONE shared kernel,
SRP first — not two unrelated additions, one gap with two symptoms.

### 29.2 The kernel refactor, and the bit-identity proof that actually proves it

`srp_force`/`flat_force`/`spherical_force` (L4 step 3's own closed module) generalised into
`photon_force(model, irradiance, band, source_direction_body, sun_direction_body,
velocity_relative_to_source_body_m_per_s)` — irradiance, band and source direction all made
CALLER-SUPPLIED parameters (`PHPR-R-001`/`R-002`) rather than SRP's own file-local constant and
implicit Sun-only assumption, `srp_force` kept as a thin wrapper so every pre-existing caller and
every existing `SPEC-srp-analytic` §8 test is unaffected.

**Two claims, not one, and only one test proves the claim that matters** (found and corrected
inside this same step, not after it shipped): `SPEC-srp-analytic`'s own 92 636 pre-existing
assertions passing unchanged proves agreement WITHIN THEIR OWN TOLERANCES, which a refactor's own
characteristic hazard — reordering a floating-point expression — would still satisfy while changing
the last bit; and asserting `srp_force`'s output equals the exact `photon_force` call it now makes
proves nothing at all, a comparison of the wrapper against itself that cannot fail regardless of
what the underlying arithmetic does. `PHPR-A-001` is the test that actually proves bit-identity:
`srp_force`, compared with EXACT equality (`std::bit_cast<uint64_t>`, not `==`, which treats `-0.0`
and `+0.0` as equal), against `srp_force_golden.hpp` — 400 cases (five optical triples × ten
directions × three single-surface kinds, plus 5×5 bus/panel combinations), captured from a git
worktree built at the commit immediately before this refactor (`ea9462f`), standalone
(`g++ 15.2.0 Ubuntu, -std=c++20 -O2`, checked against this tree's own CMake flags for anything
that would change floating-point semantics — `-march`/`-ffast-math`/`-ffp-contract=fast` absent from
both), not by this file's own new code calling itself.

**Proven to have teeth, not merely written** (plan rule 5): the first injection attempted —
reassociating `(A*I/c)*coeff` to `A*(I/c)*coeff` — showed ZERO bit differences across all 400 cases.
Rather than accept a silent test as proof the check works, this was investigated as a finding about
the INJECTION (several of this suite's own stated areas are powers of two, which multiply exactly,
so this specific reassociation genuinely does not diverge here) rather than about the test, and a
second reassociation tried — `-(prefactor*cos_theta)*bracket` to `(-prefactor)*(cos_theta*bracket)`
— which DOES diverge: 42 of 400 cases, 61 of 3951 assertions, every one at exactly the last bit.
Reverted, suite re-run clean, both findings recorded in the test's own comment.

### 29.3 The back-face/band schema gap (`PHPR-R-004a`), `SPEC-macromodel` v2.1

Found while designing the kernel's own call from `cap_integral`: a sun-tracking panel's normal
tracks the Sun (`PHPR-R-002` lets it, correctly, for SRP), but Earth's own radiation reaches that
panel from a DIFFERENT direction in general — and when Earth is behind the panel's own front
(`cos θ < 0`), the pre-existing kernel read the surface as unlit, exactly as it must for a
surface with only a front. Over the Earth's day side, where reflected albedo comes from, Earth sits
on the panel's OWN LIT SIDE, opposite the Sun, most of the time — a one-sided panel therefore drops
most of ERP's own contribution from what is, on a real GNSS spacecraft, most of the spacecraft's own
area. `RS09` Table 3.1 (own title: "optical parameters, VISIBLE AND INFRARED") independently showed
the SAME schema also needed a spectral band axis: a real panel's front is measured at µ 0.85/ν 0.23
visible but µ 0.50/ν 0.20 infrared, real daylight between the two.

This is the SECOND time a contract frozen against a trivial/degenerate client met a real need at its
first real one (the first: `DYN-R-051`, L3 step 4's own provenance channel). `FlatSurface` gained an
optional back face (itself visible-required/infrared-optional) and every face gained an optional
infrared triple falling back to visible; `SphericalSurface` gained the analogous optional infrared
triple. `OpticalTriple`/`Band`/`BandedOptics` are new named types, not three more loose fields, so
"all or nothing" and "infrared falls back to visible" are compiler-enforced rather than documented
and trusted. Additive throughout (every new field optional, defaulting to prior behaviour exactly);
`PHPR-A-001`'s own bit-identity proof is the strongest available evidence this changed nothing for
an existing caller, since the schema itself has no force of its own to compare.

`SPEC-macromodel.md` — a CLOSED spec (L4 step 2, frozen at v2.0) — amended to v2.1 rather than left
undocumented in its own owning spec: `MCRM-R-013`/`R-014`/`R-015`, `MCRM-F-006` (a new opaque
`IrradianceWPerM2` unit-typed flux density also moved into this module, `srp_analytic`'s own
1367 W/m² constant no longer the only source of one), `MCRM-A-013`/`A-014` (schema-only round-trip
and fall-back tests, no force law involved — `BandedOptics.in()`'s own fall-back and
`FlatSurface.back()`'s own presence/absence, checked directly, separately from whether any force
law reads them correctly).

### 29.4 The aberration term (`PHPR-R-010`), and a factor-of-four caught before any code existed

`dyn::Force`'s own header (frozen at L3 step 1) already named the defect this term fixes: SRP is the
force a conventional model declares to have no velocity dependence, and the declaration is false —
aberration makes it depend on the spacecraft's own velocity. The manager's own review of v1.0 found
`DYN-Q-002`'s defect reversed: no aberration term existed in the force AT ALL, so `with_velocity`
would have been a declaration without a term behind it. `BLS79` (Burns, Lamy & Soter 1979, Icarus
40:1–48, pinned) Eq. (5), *m***v̇** = (SA/c)Q_pr[(1 − *ṙ*/c)**Ŝ** − **v**/c], **Ŝ** the unit vector
ALONG THE INCIDENT BEAM (source to particle) — this tree's own `e_D` points the OTHER way (particle
to source), so **Ŝ** = −e_D and *ṙ* = −(**v**·e_D), giving
F = −(SA/c)Q_pr[(1 + (**v**·e_D)/c)e_D + **v**/c], exactly the pre-existing steady force at **v** = 0
(`PHPR-R-003`'s own bit-identity). Applied through each surface's ALREADY-established coefficient —
exact for a sphere (`1 + 4δ/9` **is** BLS79's own *Q*_pr for an isotropic scatterer), the natural
minimal generalisation for a flat plate (no single BLS79-shaped *Q*_pr exists for two directional
coefficients; exact at the one geometry this tree's own sun-tracking panels always have, normal
incidence).

**The velocity-vs-source correction**, caught DURING drafting, before any code existed to carry the
error forward: the aberration term was first written against the spacecraft's own bare GCRS
velocity. Aberration depends on velocity relative to the irradiance SOURCE, and for sunlight that is
dominated by Earth's own ≈29.8 km/s heliocentric motion (the Sun's own GCRS velocity, sign-reversed)
— using the spacecraft's ≈7.4 km/s geocentric speed alone drops the dominant, nearly-constant
transverse part of the term, short by roughly a factor of four at this tree's own case. Named
explicitly in the kernel's own signature
(`velocity_relative_to_source_body_m_per_s`, plan §5 constraint 10) so the next caller cannot supply
the wrong one without writing the wrong name; `PHPR-A-006`'s own tolerance was set tight enough to
fail if the geocentric-only velocity were used by mistake, not merely to confirm an order of
magnitude — and does: the two differ by ≈4× at LEO.

### 29.5 The two exact identities, and what each actually measures

`PHPR-P-3`: a uniformly-emitting Lambertian sphere's net flux through any concentric sphere of
radius *r* is exactly *M*(R_E/r)², radial, at EVERY altitude — `PHPR-A-007` reproduces this on a
`SphericalSurface` spacecraft (albedo forced to zero, isolating the emitted term) to a converged
relative error of 2.6 × 10⁻⁴ at LEO and 2.8 × 10⁻⁵ at GNSS altitude (§29.6 has the convergence
history). The far-field albedo form — **`PHPR-A-008`'s own dimensional bug, caught while trying to
implement the test, not before**: an earlier draft of `PHPR-P-3` stated the far-field reflected
irradiance as (2·A_E/3)·S·(R_E/r)²·Φ(α), A_E Earth's own DISC AREA (an m², dimensionally wrong in an
irradiance formula) where it meant the Bond ALBEDO (dimensionless) — undetected through four spec
review rounds because no code had yet tried to COMPUTE anything with it. Corrected to
(2·α_A/3)·S·(R_E/r)²·Φ(α), re-verified by carrying the phase integral through explicitly:
*q* = 2∫Φ(α)sin α *d*α = 3/2 for a Lambertian sphere (a standard, checked result), giving
∮E(r,α) *dA* = α_A·S·π·R_E² exactly — the incident power on Earth's own disc times the Bond albedo,
confirming energy conservation holds with the corrected form, not merely dimensionally sensible.
`PHPR-A-008` then measures the cap integral's own departure from this corrected closed form at
three phase angles (30°/90°/150°) and three altitudes (GNSS, GEO, 100 000 km): departures shrink
monotonically at every step and every angle (≈0.15→0.10→0.04 at 30°; ≈0.15→0.09→0.04 at 90°;
≈0.87→0.67→0.34 at 150°, the largest angle's own departure shrinking more slowly in RELATIVE terms
because the closed form itself is small there — Φ(α) near its own new-phase zero — so the SAME
roughly fixed residual from `PHPR-Q-004`'s still-open terminator staircase is a bigger relative
share, a real and understood reason, not a defect) — characterised, not asserted to vanish, matching
what `PHPR-P-3` actually claims for this term.

### 29.6 The cap integral: a staircase diagnosed by reading the code, and a fix re-derived, not assumed

`PHPR-A-007`'s own first implementation (a global colatitude/longitude grid, gated to the visible
cap by a `continue` inside a [0,π]×[0,2π) domain) showed a three-point convergence ratio the manager
found unusable as `PHPR-P-1`'s own discriminator: 8.5× then 154× at LEO, 16× then 2× at GNSS —
monotonic, but not at one consistent order. Diagnosed by READING `erp.cpp` directly, not merely the
symptom: the cap's own boundary crosses the grid at an arbitrary tilt, so boundary cells are
included or excluded WHOLE as resolution changes, and the discretisation error's own coefficient
fluctuates with it.

**RS09's own literal grid was checked directly before assuming the fix**
(`data/literature/rodriguez-solano-2009-masters-thesis`, Eq. 2.41–2.44) rather than built from
memory of what "Knocke's own scheme" was expected to be: RS09's own (θ,φ) is a colatitude/longitude
pair about an axis PERPENDICULAR to the satellite–Earth–Sun plane (chosen so both **r̂** and **ŝ**
fall in one coordinate plane, keeping their own γ formula short), integrated over the same kind of
gated global domain the first implementation already matched — RS09 states no convergence-order
analysis to align with. The fix built here is therefore an independent numerical-analysis
improvement, not "align with the literal source": nadir-centred coordinates, colatitude *χ* from the
sub-satellite direction **r̂** (0 to *β*, `PHPR-A-010`'s own cap half-angle — not [0,π]), azimuth
about the same axis. Proven, not merely argued: cos θ_cell(χ) = (r_sat cos χ − R_E)/d(χ) has a
SIMPLE (transverse) zero exactly at χ = β by β's own definition, so integrating the cap's own domain
edge-to-edge removes the indicator-function kink the old [0,π] gate introduced, leaving a smooth
integrand a midpoint rule converges on at its own full order — predicted (a clean 4× per halving)
BEFORE the new implementation was run, then checked: 4.05× then 4.01× at LEO, 4.00× then 4.00× at
GNSS. Also fixes the old grid's own waste (a LEO cap is ≈3 % of the full sphere) and, as a
side-effect nobody had asserted before, sharpens `PHPR-A-007`'s own radial-direction check from
0.9896 (old grid, GNSS) to exact-to-1e-9 (new grid) — the old grid's own staircase carried a
directional bias too, not only a magnitude one. **Stated plainly, since the number alone
understates it (the manager's own point):** arccos(0.9896) is 8.3°. The OLD grid's own infrared
force was eight degrees off radial and PASSED, because `WithinRel(1.0, 1.0e-6)`'s own tolerance —
set to catch a wrong LAW, not to describe the geometry — is what decided what "exactly radial"
meant that round, not the force. An earlier report of this same row described its direction as
"exactly radial, to the tolerance checked" — true of the tolerance, not of the force it was
checked against, the manager's own distinction; recorded here so it is not lost a second time.

**The first version of this fix coupled the cap's own pole to `m_gcrs_to_body`'s rows** (reasoning:
in production, `Erp::accel_only`'s own attitude frame already has −**r̂** as its own *z*_body row, so
reading the triad off it seemed like reuse rather than a second construction) — wrong, caught by
`PHPR-A-007` itself failing outright (not a subtle drift) the first time it ran: that test passes
`m_gcrs_to_body = identity`, deliberately unrelated to `r_sat_gcrs_m`, specifically so the returned
body-frame force is directly comparable to a GCRS direction without an extra rotation in the test —
a legitimate use the OLD (global-grid) `cap_integral` never depended on this relationship for. Fixed
by computing **r̂** directly from `r_sat_gcrs_m` (this function's own argument, not a borrowed
convention) and building the azimuthal pair the ordinary always-defined way — crossing **r̂** with
whichever GCRS axis it is least aligned with, never degenerate, unlike the Sun-tied choice the first
attempt also briefly carried (exactly degenerate in that same test's own geometry, Sun and satellite
along the same direction).

**The albedo term's own terminator still staircases the new grid** (`PHPR-Q-004`, left open): χ
alone fixes the visibility boundary exactly, but the Sun-lit boundary is a function of both χ and
azimuth, with no reason to align with either grid axis. `PHPR-A-007` isolates the emitted term
specifically because it has no such gate; no converged-order claim is made for the reflected term.
Two directions for closing it, named but not chosen between: a sub-cell lit fraction at boundary
cells, or accepting the non-clean ratio and gating `PHPR-P-1` on an error bound rather than an
order there.

**A pre-existing, separate limitation, found while touching this code and flagged rather than
silently fixed**: `cap_integral`'s own `lat_rad`/`lon_rad` (fed to the `albedo`/`emissivity`
callbacks) are GCRS-frame spherical angles, not true Earth-fixed (ITRS/geodetic) ones — inert under
this step's own constant-albedo/emissivity functions, which ignore both arguments, but a real gap
before a lat/lon-varying model (`PHPR-Q-001`'s own "next refinement") can be plugged in without a
GCRS→ITRS rotation this function does not yet take (`PHPR-Q-005`). Out of this entry's own scope,
which was the coordinate system's own discretisation error, not this separate correctness question.

### 29.7 The velocity Jacobian made analytic, closing a tautology the manager caught by reading the code

`Srp::accel`'s own velocity Jacobian was originally a central finite difference of `accel_only`,
checked in `PHPR-A-006` against ANOTHER finite difference of the same call — the same tautology
shape plan rule 5 exists to catch, here found by the manager reading `Srp::accel` directly rather
than by a test failing. Per surface, `PHPR-R-010`'s own substitution is exactly AFFINE in velocity
(every direction and coefficient it uses is a function of geometry and optical properties alone,
never of velocity), so *d*F/*d*v is a CONSTANT matrix, exact at every velocity: for a flat surface,
−(prefactor·cos θ/c)·[steady_direction ⊗ e_D + drag_Q_pr·I]; for a sphere,
−(prefactor·Q_pr/c)·[e_D ⊗ e_D + I]. Both derived and verified — BEFORE either existed in production
code — against an independent central finite difference of the force formula in a standalone
numerical script: sphere case, max abs diff 9.34 × 10⁻²⁰; flat case, exactly 0.0. Implemented as
`photon_force_and_velocity_jacobian`, returning both the force and this Jacobian from one pass per
surface; `photon_force` itself is now a thin extraction of its own `.force` member, not a parallel
computation, so `PHPR-R-003`'s bit-identity carries over mechanically rather than by a second proof.
`Srp::accel_only` calls this directly, scales the Jacobian by the same shadow/mass factor that
scales the force to an acceleration, and rotates it body→GCRS on BOTH sides
(*d*a_gcrs/*d*v_gcrs = Rᵀ·(*d*a_body/*d*v_body)·R, a Jacobian's row space is its output's frame and
its column space its input's, both body frame here before rotation).

`PHPR-A-006` now checks this PRODUCTION value against an independent finite difference of the whole
public `Srp::accel` call — genuinely independent, since it never reads the analytic Jacobian at the
bumped points, only `.acceleration`. Because the force is exactly affine in velocity, this finite
difference has ZERO truncation error at any step (central difference is exact for an affine
function); the test's own step (10 m/s, chosen large specifically to shrink the remaining
floating-point cancellation, the only error source left) measures agreement to ≈1.5 × 10⁻⁹ relative
— exact to noise, not merely close. `Srp::accel`'s own cost also dropped: six `accel_only` calls per
evaluation for the position Jacobian (unchanged, no closed form exists for it) instead of twelve,
since d(a)/d(v) needs none of its own any more.

### 29.8 The remaining finite difference, sized rather than left provisional (`PHPR-A-017`)

`kPositionStepM` (100 m, `Srp::accel`'s own d(a)/d(r), the one Jacobian with no analytic form here)
had stood on a "first-pass, not measured" footing since it was written. `PHPR-A-017` sweeps this
step from 1 × 10⁴ m to 1 × 10⁻¹ m against the TRUE public pipeline (not `accel_only`'s own internal
step, differenced independently from outside): 100 m agrees with its own 30 m/1000 m neighbours to
≈1.4 × 10⁻⁸ and ≈5.7 × 10⁻⁹ relative respectively — a stable plateau — while the sweep's own small-*h*
end (0.1 m) shows visibly more step-to-step jitter (2.5 × 10⁻⁷), the expected shape (rule 7):
truncation error falling as *h* shrinks until floating-point round-off takes over. A back-of-envelope
cube-root balance (*h*_opt ≈ ε_mach^(1/3) × orbital position scale ≈ 60 m) had already put the
existing 100 m in roughly the right place; the sweep is what actually checked it. A step landing
across the shadow function's own penumbra/umbra boundary was also examined directly, not assumed
benign: `shadow/conical.cpp`'s own three branches (sunlit/penumbra/umbra) agree exactly at their own
shared edges by construction, so `fraction` is continuous there — a straddling bump sees at worst a
KINK (a possible slope discontinuity in an otherwise continuous function, the same shape drag's own
species cutoffs have), never a JUMP, bounded and localised, not yet isolated to a specific altitude
the way `DRAG-P-1`'s own jump was. `Erp::accel`'s own analogous step sizes remain on the original
"not yet measured" footing — out of this entry's own stated scope.

### 29.9 RS12 Fig. 2, checked directly, and what a plain sphere cannot show

`PHPR-A-011`'s own qualitative claim — radial maximum at Δu = 0°, minima at Δu = 90°/270°, a
secondary maximum at Δu = 180°, a cross-track sign flip with β₀ — was checked against `RS12`
directly (`data/literature/rodriguez-solano-2014-dissertation`, P-I, p. 77, Fig. 2 and its own
caption/text: "Impact of Earth radiation pressure on GPS position estimates," Rodríguez-Solano,
Hugentobler, Steigenberger & Lutz 2012 — a FOUR-author paper distinct from `RHS12`'s three-author
one, both reprinted in the same dissertation, at different page ranges, not conflated). RS12's own
stated relation, cos ψ = cos β₀ cos Δu, was verified by construction in the test geometry before
trusting it. A PLAIN SPHERE macromodel shows NONE of the secondary structure — checked directly,
not assumed: at β₀ = 0° the radial value decreases MONOTONICALLY from Δu = 0° to Δu = 180°, no
minima at 90°/270°, no secondary maximum — matching RS12's own text exactly ("This last feature
would not be present for a cannonball model with constant cross-section"). Reproduced only once the
test's own macromodel gained a sun-pointing panel WITH A BACK FACE (`PHPR-R-004a`, §29.3 — at
Δu = 0° the panel's own front points directly away from Earth, so only the back face sees Earth's
radiation at all there): radial 1.558 × 10⁻⁷ (Δu=0°) → 0.919 × 10⁻⁷ (45°) → 0.252 × 10⁻⁷ (90°,
local minimum) → 0.497 × 10⁻⁷ (135°) → 0.682 × 10⁻⁷ (180°, rising again — the secondary maximum),
mirrored exactly on the other half. Cross-track: exactly odd in β₀ at every checked Δu (equal
magnitude, opposite sign, `WithinRel` to 1 × 10⁻⁶) — a symmetry argument, not a geometry-sensitive
number, the one part of this row not tied to the specific box-wing chosen. The non-radial
component's own extremum LOCATION (RS12's own ≈35°/145°) is explicitly NOT tested — model-geometry
sensitive in a way the other claims are not, left unasserted rather than forced to a number this
entry cannot independently justify.

### 29.10 What this entry does not close

The `PHPR-Q` table (§10 of the spec) carries five open items this entry does not resolve: Knocke's
own zonal/seasonal albedo coefficients (`Q-001`, constant 0.3/0.7 stands); non-nominal
noon/midnight attitude (`Q-002`, ruled explicitly out of step 5's own scope, step 6's); the PPM
shadow (`Q-003`, inherited from L4 step 1, untouched here); the albedo term's own terminator
staircase (`Q-004`, §29.6); and the cap integral's GCRS-not-ITRS lat/lon (`Q-005`, §29.6). The
small-β₀ noon/midnight regime is documented (`SPEC-photon-pressure` §4.2) as a stated scope
boundary, not solved: the ideal yaw law stays mathematically defined through β₀ = 0, but its own
output there is the model's attitude, not a real spacecraft's, until step 6.

---

## 30. L4 step 6 — `thrust-yaw`: GPS eclipse-season yaw attitude and antenna thrust

**Date.** 2026-09-24. **Artefact.** `spec/SPEC-thrust-yaw.md` v1.0 adopted, Spec ID `TYAW`; 29
own-prefix identifiers, 9 requirements and refusals, 8 discharged by an acceptance row and 1
excused; 0 uncovered. `modules/attitude` gains `gps_yaw_attitude`; a new module,
`modules/antenna_thrust`, is built. Tree-wide: 13/13 `ci.sh` gates, 309 tests.

### 30.1 Scope narrowing to GPS, and why

L4-6.md's own scope boundary (2026-09-23) named the sources — Steigenberger 2018, Kouba 2009,
Montenbruck et al. 2015, the Galileo/GLONASS/BeiDou attitude documents — without naming which
constellation this step must build first. The manager narrowed it (2026-09-24) to GPS by reading
what the frozen baselines actually consume: `G-01`..`G-03` (`oracle/cases.tsv`) are PRNs G01 and
G05 in February 2023, and the `B-*` block statistics are GPS IIF/IIR-A/B/M/IIIA. So this step
built and gated GPS; Galileo, GLONASS and BeiDou are carried as a named follow-on, with sources
already located (§30.2).

A second, sharper finding followed from reading `validate_sp3.sh` directly (the manager's own
catch, not assumed): the frozen `G-*` fits are **cannonball** fits — a single `area`/`mass`, the
SRP scale the same `A·C_R/m` convention `B-*`'s own block means use — and consume **no surface
model and no attitude law at all** (`oracle/ORACLE.md` §6). So this step's own gate could not be
"reproduce the frozen residuals" the way earlier steps' gates were; it rests instead on published
turn behaviour the sources themselves state and print — the β₀ derived relation, continuity at
hand-over, the rate bound, and observed attitude from openly published ORBEX files (§30.2, §30.8).

### 30.2 The two-round rule-4 search, and what each source actually covers

**Round one, broad** (the four named sources plus what they led to): Kouba 2009 (`KOUBA09`) gives
II/IIA and IIR/IIR-M in full, with worked equations — but launched before IIF (2010) and IIIA
(2018), so it says nothing about either. Steigenberger 2018 (transmit power) proved unreachable at
this layer's own need (§30.2's own second finding, below, shows why that does not block the step).
Galileo (the operator's own metadata page), GLONASS-M (Dilssner et al. 2011), and BeiDou (the CSNO
standard, with a published observed deviation) all had real, locatable sources — carried forward,
not built, since the narrowed scope is GPS-only.

**Round two, narrow** (2026-09-24, once IIF/IIIA were found to be the load-bearing gap): the IGS
satellite metadata SINEX (`https://files.igs.org/pub/station/general/igs_satellite_metadata.snx`,
`SATELLITE/IDENTIFIER`/`SATELLITE/PRN` blocks) confirmed, checked rather than trusted, **G01 =
SVN63 = Block IIF**, **G05 = SVN50 = Block IIR-M**. IIF's own law came from two independent
primary sources: Dilssner 2010 (`DIL10`, the noon/night rate asymmetry, ≈0.11°/s and ≈0.06°/s, and
the "β greater than 8°" turn-suppression threshold) and the IGS `eclips` model's own documented
parameter changelog (§30.3 states its boundary). `TYAW-P-1`'s own derived-relation check
(tan β₀ = μ̇/R) reproduces `DIL10`'s own threshold for IIF's night rate: 7.93° against "greater
than 8 degrees," and, read again more closely on the manager's own later prompt (§30.5), a fifth
check against `DIL10`'s own words for the noon rate too — 4.35° against his own "below 4 degrees"
— alongside the other three against `KOUBA09`'s own printed II/IIA and IIR figures, 3.57°/4.87°/
2.39° against 3.6°/4.9°/2.4°.

**Steigenberger 2018 being unreachable does not block this step**, because antenna thrust is built
here as a force **law** (P/c along the boresight, P a caller-supplied test value) — per-satellite
transmit power is L5's own population data (`DYN-Q-001`'s L4/L5 split, already used for the
macromodel itself), not something this layer consumes. The IGS metadata SINEX's own `TX_POWER`
block (citing `[TP01]`, Steigenberger/Thoelert/Montenbruck 2017) is recorded here only as a
provenance-only plausibility anchor for `TYAW-P-4`'s own budget row (240 W, SVN63/G01) — its full
terms are not established by this specification, deferred to L5 where the SINEX becomes consumed
data (§9's own stated obligation).

### 30.3 The `eclips.f` boundary — the same discipline RKF7(8)'s own tableau already stood on

`eclips.f`, Kouba's own Fortran implementation, is **code** whose licence is not established
(`COPYRIGHT GEODETIC SURVEY DIVISION, 2011. ALL RIGHTS RESERVED`, no further terms found). Ruled:
usable only to **cross-check** already-sourced numbers — its own changelog independently confirms
the 0.06°/s IIF night rate and the −0.7° yaw bias (attributed there to Kuang et al. 2016,
postdating `DIL10`'s own 2010 text) — never as an implementation template. The manager named this
"dop853's line," the same boundary `SPEC-integrators`'s own RKF7(8) tableau already observed:
implement from published coefficients, never from a reference solver's own source.

### 30.4 The IIIA stopgap — ruled, and why it does not matter to any frozen baseline

`TYAW-R-004` models IIIA with `TYAW-R-003`'s own IIF law unchanged, named explicitly as the same
stopgap IGS analysis centres without a IIIA-specific model already use — not a claim that IIIA's
own hardware matches IIF's. The manager's own ruling (2026-09-24) rests on the §30.1 finding: since
no frozen baseline consumes any attitude law at all, this requirement serves a capability beyond
the predecessor's own baselines (box-wing modelling, this step's and step 5's own joint scope), not
a reproduction target. The stopgap's own known error is stated in the direction it is known: a 2023
source (Dilssner et al., GPS World and a companion paper, read at summary level only) reports true
IIIA behaves "similar to Block IIR but with a smaller maximum yaw rate and an earlier maneuver
onset" than IIR's own 0.20°/s — IIF's own rates (0.11°/s, 0.06°/s) are already smaller than IIR's,
the same direction, and IIF's own night-turn β₀ (7.93°) is larger than IIR's (2.39°), the same
"earlier onset in β" direction the 2023 source reports. Not asserted quantitatively close —
`TYAW-Q-001`, carried, closes that gap once the true law is read.

### 30.5 `gps_yaw_attitude` — a stateless reformulation, and two real bugs testing caught

`KOUBA09`'s own turn-timing equations (his Eq. 7-9, 15/16 for the noon/midnight turns; Eq. 17-22
for II/IIA's own shadow crossing) are stated in time, `t`. Reformulated here in the orbit angle
`μ` instead (μ evolves linearly with time at the fixed rate μ̇, so an angle-since-onset carries the
same information a time-since-onset would, without a remembered reference epoch) — `TYAW-R-007`'s
own statelessness requirement, satisfied by construction for turn ONSET (closed-form in the current
β) but not, it turned out, for turn END without real care.

**Bug 1, found by testing, not anticipated.** `KOUBA09`'s own text terminates a noon/midnight turn
*operationally* — "until the lagging angle catches up with the nominal yaw attitude" — not by a
closed form. The first stateless reformulation detected catch-up by comparing the ramp's own
accumulated ψ against ψ_nominal, wrapped to the branch NEAREST the ramp's own value. Verified once
against a ground-truth simulation of the operational rule at one rate (0.1°/s) with 0/3600
mismatches, then FAILED at a different, still-realistic case (II/IIA, β = 1°, R = 0.122°/s,
μ = 205°, 25° past noon): the ramp's own accumulated value had drifted so far (its own rate is
≈14.6× μ̇, so a modest Δμ produces a large Δψ) that "nearest branch" picked the wrong multiple of
2π, silently reporting an already-finished turn as still active (returning a frame 1.5 rad off from
nominal). Patched first with an explicit bound on the ramp's own accumulated swing (175°, chosen
because a fresh ground-truth sweep — six rates, β from 1% to 99% of β₀ — never needed more than
≈174°) and re-verified, 0/9000 mismatches.

**That fix rested on a false premise, found by the manager's own review, not by the grid that
first tested it — rule 7's own warning, encountered from the inside rather than merely stated.**
The manager's own simulation showed the true swing tends to 180° exactly as β → 0 (the nominal
law's own swing through a turn is 180° − 2β), reproducing it directly: 142.6° at β = 1°, 170.3° at
β = 0.1°, 174.8° at β = 0.03°, 175.8° at β = 0.02° (past the 175° bound), 177.1° at β = 0.01°. The
"~174°" finding was the smallest β the first grid happened to test, not a property of the physics
— a threshold FITTED to the grid that judged it. The damage was bounded (within a few degrees of
nominal, under a minute, once β drops below ≈0.025°) but the comment's own claim was false, and the
fix that produced it was corrected rather than merely patched again. **The exact, bound-free fix**
(the manager's own): read ψ_nominal's change since onset on the branch IN THE TURN'S OWN DIRECTION
— [0, 2π) for a positive turn, (−2π, 0] for a negative one — never on the branch nearest the ramp.
For any β > 0 the true swing is under 180°, so this branch is unambiguous regardless of how far the
ramp has itself run on; and once past catch-up, the ramp's own value keeps growing while the
directional value stays bounded, so a finished turn stays reported finished — no bound needed at
all. Re-verified with β from 0.01% to 99% of β₀ across all six rates this tree uses, 18 000 points,
0 mismatches, worst observed swing 179.44°. A dedicated regression test (β = 0.005°, II/IIA)
locks in the near-β=0 regime the first grid missed.

**Bug 2, in the executor's own test fixture, not production code.** `TYAW-A-006`'s own Sun-on-nadir
refusal fixture placed the Sun toward Earth, expecting the noon degenerate point — but Sun-toward-
Earth is the MIDNIGHT geometry, where II/IIA's own shadow-crossing (a *different* width formula,
E_sh² − β², nonzero at β = 0, unlike the turn's own β₀|β| − β², which correctly vanishes there) is
always active — so the refusal was never reached for that block. Fixed by using zenith instead.

A third finding, independent of both bugs: `x_body = −cos(ψ)·t̂ − sin(ψ)·n̂` (t̂ the prograde
tangential direction, n̂ the orbit normal) is the relation that reproduces `nominal_yaw_steering`'s
own independent z=−r̂/y=(z×ŝ)/x=y×z construction, verified numerically across many random
geometries to machine precision (max component error 4×10⁻¹⁶) before being trusted — a NEGATIVE
sign the naive "ψ measured from the along-track direction" reading would not have predicted,
caught by checking against the existing, already-correct construction rather than assumed.

**A fourth, confirming check made once the dust settled**: the manager asked whether `DIL10`'s own
printed β = 0° half-turn duration ("about 55 minutes") was ever reproduced. It had not been. Read
against the production code (not a hand computation): at β = 0.001° (a stand-in for exactly 0,
where the β₀-threshold's own √(β₀|β| − β²) term is singular), IIF's own modelled night-turn
duration is **49.82 min** — close enough to corroborate the implementation, recorded as `TYAW-P-1`'s
own fifth relation and locked in by `TYAW-A-001b`.

### 30.6 `TYAW-P-5`'s own reference point, corrected from LEO to GPS altitude

The antenna-thrust magnitude gap (P/c overstating the true recoil by 1.46%/3.02% at 13.9°/20° beam
half-angles, §30.7) was first ranked against `PERT-P-2`'s own 8.552×10⁻¹¹ m/s² floor — a LEO
number by construction (`TN36-6` §6.2.1's ocean-tide truncation cutoff at r = 7331 km, §18.2). The
manager named this the SAME fault §21.4 already recorded for the SRP/drag velocity-derivative
comparison: a defensible-looking number from an unstated, mismatched reference point. Rebuilt at
GPS altitude: the natural reference is the central acceleration itself, GM/r² ≈ 0.565 m/s²
(r ≈ 26 561 km). A constant, radially outward acceleration error acts like a fractional shift in
effective GM; a circular-orbit fit absorbs a constant fractional GM shift as a comparable
fractional shift in fitted radius — 0.34 mm (13.9° case) and 0.70 mm (20° case), both a
sub-millimetre fitted-position effect against what a GNSS orbit fit resolves. The conclusion is
unchanged; the reason is rebuilt on GPS's own numbers, stated as a one-pass estimate with its own
premise named (constant, radial, circular orbit).

### 30.7 `antenna_thrust` — a redesign, from an attitude-plumbed force to a position-only one

The first design gave `AntennaThrust` an `AttitudeProvider` callback plus `Ephemeris`/`LeapTable`
(to resolve the Sun's own direction for whichever attitude law was bound), with d(a)/d(r) a
central finite difference (`Srp`'s own remaining-Jacobian shape) and d(a)/d(v) declared absent with
a deliberately loose, conservative bound (2·a_max/|v|, "the whole force reversing over an
orbital-velocity-scale change"). The manager's own review found this entire design unnecessary:
**every attitude law this tree has sets z_body to geocentric nadir and never moves it** — yaw is
by definition a rotation ABOUT z_body, not of it — so the force, −P/c·z_body, is (P/(mc))·r̂, a
function of POSITION ALONE. Four corrections followed: (a) d(a)/d(v) = 0 EXACTLY, not a bound on a
term that does not exist for any provider this tree has; (b) d(a)/d(r) = (P/(mc|r|))(I − r̂r̂ᵀ),
the standard derivative of a normalised vector, ANALYTIC rather than a finite difference of a
function differentiable on sight — the same trap L3's own Liouville check was built to catch,
encountered again at L4; (c) the Sun ephemeris and the attitude-provider call were dependencies
this force never actually had, each its own refusal path for no reason — removed along with
`ATTD-F-001`'s own reachability through this force (nadir, and therefore the thrust, is perfectly
well defined exactly where the yaw singularity is not); (d) the free function's own parameter,
`m_gcrs_to_body: Mat3`, was replaced with `z_body_gcrs_unit: Vec3` — a second, later pass of the
manager's own review found the CLASS had been building a Mat3 with only row 2 populated to satisfy
that signature, "a 'frame' with two zero rows works only as long as nobody ever reads another row";
the function only ever needed the one axis, so its own signature now says so. `AntennaThrust` is
now `(p_watts, mass_kg)` only. `TYAW-R-006` amended to state the nadir-for-every-law argument
concretely rather than as a wiring/independence claim discharged by another row; it now has its own
acceptance row (`TYAW-A-011`, the analytic Jacobian checked against an independent central finite
difference, `PHPR-A-006`'s own precedent) rather than being excused. A `TYAW-F-003` the executor
had minted for the since-removed ephemeris-unavailable path (modelled on `PHPR-F-003`'s own
precedent for a genuinely different module's failure propagating up, correctly distinguished at the
time from `TYAW-F-001`'s own same-refusal-two-ways forwarding) was reverted along with the design
it was for, rather than left stranded in the spec.

### 30.8 `TYAW-A-009` — two registered shapes, a real epoch, and a verdict

**First pass and its own failure.** 2023-04-08, G01/SVN63, via CODE's own MGEX archive, β =
4.13–4.71° all day — comfortably below IIF's own night threshold (7.93°) but straddling its own
noon threshold (4.346°, §30.5's own fifth `TYAW-P-1` check, later withdrawn — §30.6), which turned
out to matter: a first attempt to identify real turns in that day's own ORBEX data, by a coarse
"elevated rate, contiguous run" search (run in a background agent — §30.9 records why that was
itself a process fault, independent of the physics), produced an internally impossible noon-turn
reading (a swing exceeding 180°, which `SPEC-thrust-yaw` itself bounds every turn under). Caught by
the manager's own review, not the search that produced it.

**What `DIL10`'s own printed numbers actually constrain, read in full rather than excerpted.** The
manager's own re-reading of the two-number midnight-turn passage (§30.6's own registered text, the
full passage with page numbers) found that NEITHER a fixed-0.06°/s β₀-threshold ramp (Shape F, the
law already implemented) NOR a single eclipse-duration-averaged rate (Shape E) fits both of
`DIL10`'s own printed numbers (0.06°/s; "about 55 minutes" at β = 0°) at once — 0.06°/s × 55 min =
198°, not the 180° a half-turn is. Each shape reproduces one number to about 1% and misses the
other by about 10%. Both shapes, and a discrimination criterion (extracted plateau rate and
turn-start time each within a quarter of the two shapes' own separation from the other) were
registered in `SPEC-thrust-yaw.md` §4.3 BEFORE any file was read for `TYAW-A-009` itself — `plan`
rule 7's own discipline, a tolerance set by the question, not by the answer.

**Verdict: Shape E, not Shape F, run by the executor in its own session** (the manager's own
correction of an executor process fault — a background research agent had been used for exactly
the handed-over, unsupervised work `~/.claude/CLAUDE.md` §1 reserves for a visible session;
§30.9). Real SP3 and ORBEX files fetched directly (CODE's own MGEX bucket, `COD0MGXFIN_
20230980000_01D_05M_ORB.SP3.gz` / `..._ATT.OBX.gz`); G01's ECEF positions converted to GCRS with
this tree's own `odl::frames::to_gcrs` (IAU 2006/2000A, the real C04 EOP series, not a standalone
approximation), inertial velocity by central-differencing the GCRS positions, β/μ by this tree's
own production formula, the Sun's own direction from this tree's own ephemeris.

One real bug caught before trusting any of it: the first attempt built the quaternion-to-matrix
conversion as body→ECEF (columns = body axes in ECEF), following a loose reading of the ORBEX
format spec's own prose. z_body · r̂ came out wrong at some epochs (0.99, 0.72, even sign flips) —
not the clean nadir alignment a GPS antenna keeps. Using ROWS instead (this tree's own `Mat3`
convention: row *i* is axis *i* of the target frame in source components) gave **−1.0000 exactly**,
every epoch tested — the file's own quaternion is ECEF→body. Fixed, then the WHOLE pipeline was
checked against real data far from any turn before looking at the turn itself: `psi_nominal(μ,β)`
matches the real ORBEX-derived yaw angle to **0.000–0.001°**, dozens of points, both of the day's
own passages.

**The midnight turn itself, checked at two independent crossings on the same day (different β,
same day's own data, no new fetch needed for the second):**

| crossing | β | real plateau rate | real duration | real onset (μ) | Shape E predicts | Shape F predicts |
|---|---|---|---|---|---|---|
| 1st (≈10.0 h UTC) | 4.218° | −0.0458°/s | 50.50 min | ≈ +13° | rate 0.0467°/s, duration 51.13 min, entry ±12.82° | rate 0.06°/s, onset −3.96° |
| 2nd (≈22.0 h UTC) | 3.923° | −0.0463°/s | 50.50 min | ≈ +12.3° | rate 0.0472°/s, duration 51.50 min, entry ±12.92° | rate 0.06°/s, onset −3.97° |

Both crossings land Shape E's own rate within 0.001°/s (criterion: 0.0034°/s) and duration within
about a minute (criterion: ≈4.4 min) of prediction; both land nowhere near Shape F's own 0.06°/s or
its own onset nine-plus degrees away from where the real rate actually starts rising. Two
independent instances, same day, different β, same clean verdict.

**The noon side, checked for comparison, is untouched.** Second noon crossing that day, β = 4.070°
(below the 4.346° noon threshold, so a real turn is expected): real data shows a genuine bounded
plateau, ≈7.00 min at 0.110–0.112°/s, smooth on both sides — Shape F's own prediction at this exact
β: 6.52 min, onset 1.06° before noon, exit 2.21° after (3.27° angular span predicted against 3.1°
observed). Close. The noon turn is the sensor-GUIDED case (Sun still visible, hardware chasing the
nominal rate as far as it can — Shape F's own mechanism, correctly). The midnight/shadow turn is
the sensor-BLIND case (Sun lost entirely, nothing left to chase, only the eclipse's own geometric
duration to define the maneuver — Shape E's own mechanism, not Shape F's).

**What an ORBEX file actually is, checked rather than assumed** (the manager's own further
question, since the executor's own earlier draft had called this "observed attitude" without
checking): `Loyer et al. 2021`'s own comparison across seven IGS analysis centres finds
"significant differences ... for GPS and GLONASS satellites" between their own products, and the
ORBEX format's own spec (`ORBEX009.pdf` §4.10) carries no observed/predicted flag for the ATT
record at all, unlike its own POS record. So this is not a ground-truth measurement; it is CODE's
own published attitude, one implementation's own reading of the literature, checked against this
one — `plan` rule 8's own case, stated as such in `TYAW-A-009`'s own row rather than left implied.

*[what remains: whether `TYAW-R-003`'s own IMPLEMENTED law changes from Shape F to Shape E for
IIF's night side is the manager's own verdict to give, not the executor's to take unilaterally —
holding the code and spec change for it. Once given, this subsection records the decision and
`TYAW-A-009`'s own frozen acceptance values; §30.10 records the model-form correction as its own
finding alongside the two real bugs already in §30.5.]*

### 30.9 A background agent used for handed-over work — the manager's own correction

The executor dispatched two background research agents (the Agent tool's own `fork` and a fresh
agent) for the A-009 epoch-finding and data-extraction work, before either had actually started
writing or building anything. Once the second one began writing scratch code and building it in
the shared working tree, its own eventual cleanup (a plain `git checkout`/`restore`-shaped revert,
never explicitly requested) took the executor's own uncommitted `attitude_tests.cpp` work back to
`HEAD` with it — caught, diagnosed and fixed within the session (§30.5's own account of the
attitude-test content is the recovered, not the lost, version). The manager's own review named the
deeper fault: `~/.claude/CLAUDE.md` §1 reserves hidden background agents for research whose own
tool output does not need keeping, never for handed-over, unsupervised WORK — a background agent
writing and building code in the shared tree is exactly the "user cannot watch it happen" case the
rule exists for, independent of whether that particular run also happened to damage something.
`TYAW-A-009`'s own extraction was redone by the executor directly, in its own session, once this
was raised — §30.8 records what that run found.

### 30.10 What this entry does not close

Galileo, GLONASS and BeiDou are carried, not built (§30.2's own sources). True IIIA's own law
(`TYAW-Q-001`) awaits reading the 2023 source in full. The IGS metadata SINEX's own full terms are
not established (§9's own stated deferral to L5, where `TX_POWER` becomes consumed data, pinning
`[TP01]` 2017 specifically, not the unreachable 2018 journal paper). The β ≈ 0° turn-sign
ambiguity (`TYAW-Q-002`) is carried with a stated convention (sign(β) at the query instant,
sign(0) := +1) rather than built with memory of a turn's own start; a stateless refinement using
sign(β) AT the turn's own start (itself geometry, via `KOUBA09`'s own closed-form onset condition,
not a remembered value) is recorded, not built. `TYAW-Q-003` (the A-009 epoch's own justification)
and `TYAW-Q-004` (agreed and carried, §10 of the spec) are addressed in §30.8/§30.2 respectively.

---

## Changelog

| date | change |
|---|---|
| 2026-09-24 | **Step 5's gate closes.** §29 added: `SPEC-photon-pressure` v1.0 adopted, `SPEC-macromodel` to v2.1, `modules/attitude`/`srp`/`erp` built over one shared `photon_force` kernel (`srp_analytic`, additively generalised, `PHPR-A-001`'s golden-file bit-identity proof against the pre-refactor commit, not a live tautology). Four rounds of manager review before adoption caught, in order: a missing FlatSurface back face and spectral band (`PHPR-R-004a`, `RS09` Table 3.1, real GPS panel data) and a missing aberration term (`PHPR-R-010`, `DYN-Q-002`'s defect reversed, `BLS79` Eq. 5) with the aberration term's own first draft using the spacecraft's bare GCRS velocity instead of velocity relative to the Sun -- wrong by ~4x, caught before any code existed to carry it. Two further, larger findings closed the gate itself: (1) the SRP velocity Jacobian made ANALYTIC (closed-form, since `PHPR-R-010`'s own substitution is exactly affine in velocity) rather than a finite difference checked against another finite difference -- the manager's own catch, reading `Srp::accel` directly, of the same tautology shape rule 5 exists to prevent; verified against an independent central finite difference before any production code existed (sphere case 9.34e-20, flat case exactly 0.0), `PHPR-A-006` now checks the real production value to ~1.5e-9 relative; (2) the ERP cap integral's own convergence ratio, found unusable (8.5x/154x at LEO, 16x/2x at GNSS -- monotonic, not one order) and diagnosed by the manager reading `erp.cpp` directly (a "staircase" cap boundary), fixed by reintegrating in nadir-centred coordinates whose own fix was independently re-derived and PREDICTED (a clean 4x per halving) before being run, then confirmed (4.05x/4.01x LEO, 4.00x/4.00x GNSS) -- RS09's own literal grid was checked directly first and found NOT to be this scheme, so the fix is an independent numerical-analysis improvement, not "matching the source." A dimensional bug (disc area where Bond albedo was meant) in the far-field albedo closed form was caught while implementing `PHPR-A-008`, the first time any code tried to compute with it, four review rounds after it was written; `PHPR-A-011`'s own RS12 Fig. 2 comparison was checked against the primary source directly and found to need a back-face panel (a plain sphere shows none of the claimed secondary structure, matching RS12's own text). Nine new acceptance rows written and passing (`PHPR-A-004/005/008/011/013/014/016/017`, `MCRM-A-013/014`); five items carried open (`PHPR-Q-001`..`Q-005`, §29.10). Tree-wide: 297 tests pass. |
| 2026-09-23 | **Step 4's gate closes.** §28.5 corrected in place (kept, not rewritten) and §28.10 added: the manager's own line-by-line read of `MSIS-FOR`'s `DATA ALTL` found channel 1's non-monotonic error was SEVEN hard species-correction cutoffs, not the one §28.5 v1 bisected and not `SPEC-atmosphere`'s cited "fitted cubic spline" (that structure does not exist above 120 km, §3.6, which §3.1's citation had misnamed). Four items closed it: (1) all seven measured directly, a density and acceleration jump table at the stated ballistic coefficient, monotonic in altitude across four orders of magnitude; (2) `drag.cpp`'s channel 1 now detects a straddled cutoff at runtime and switches to a one-sided second-order difference walking away from it (`DRAG-R-012`, its 1e-4 tolerance a derived separation between two regimes, `DRAG-P-4`), verified against a true same-side finite difference at an adversarial case `DRAG-A-010`'s own 37 m of unmeasured margin never tested (`DRAG-A-011`); (3) the 60 s-step integrator effect sized, not asserted, against the tree's 6.7e-6 m tolerance premise -- THREE cutoffs clear it at the test spacecraft's own ballistic coefficient, not the single one this session's own earlier report had named; scaled to LightSail-2, the mission this tree exists for, SIX to SEVEN of the seven clear it, N2/160 km by up to 32 600x, not 100x -- left as L7's own event-location question, which the plan's own L7 entry already treats as central; (4) the diagnosis promoted into `modules/atmosphere`'s own suite (`ATMO-R-037`, `ATMO-A-028`, gated against frozen reference, `ATMO-Q-005`'s CI-independence preserved), an L2 edit on `DYN-Q-001`'s own terms since the false claim lived in a layer this project had already closed. Extending the reference sweep to straddle every cutoff (125->181 records, 1500->2172 comparisons) surfaced a second, unrelated instance of this project's own recurring bug shape inside `tools/msis_reference.py` itself: a hardcoded worst-comparison description that had gone not merely stale but REVERSED (the sweep now loosens the class-A bound it once only failed to loosen), fixed by making the description and its sensitivity claim compute from the same data the table is, not typed once beside it. §20.2's own "every branch boundary" was the same shape of unsearched absence one layer down, corrected there with a dated note rather than rewritten. `SPEC-atmosphere` (§3.2/3.3/3.6/6/8, `ATMO-R-037`) and `SPEC-drag` (`DRAG-R-004/012`, `DRAG-P-1/4`, `DRAG-A-011`, §8/§9) both updated; `SPEC-drag` `DRAG-R-008`'s inherited tolerance citation follows the corrected figure. |
| 2026-09-22 | §28.8-28.9 added, SPEC-drag to v1.1, SPEC-dynamics to v1.2. Manager's review of v1.0 found two more things: (1) ForceEvaluation (frozen at L3, gated only by a trivial force that could not reveal what a REAL force must carry) had nowhere for Drag::accel to put atmosphere's provenance, silently losing it for any caller reaching drag only through the Force plugin -- fixed additively (DYN-R-051, DYN-Q-001's own terms), with require_verified made a Drag construction-time option in the same fix, closed together because one found the other. (2) DRAG-A-010's own 1.18e-2 "residual as empirical bound on the unmodelled v_rel(r) channel" was itself wrong: isolating channel 2 (holding rho fixed, differencing through the real to_itrs velocity) proved its formula exact to 6 figures and roughly 10x SMALLER than the gap it was blamed for; a step sweep on channel 1 alone found its true error NON-MONOTONIC across 1-0.25 km, then stably ~1e-6 at 0.1 km and below -- NRLMSISE-00's own fitted-spline structure aliasing against a too-coarse step, not smooth truncation. Fixed at the source (central difference at 0.1 km, channel 2 added exactly) and re-verified by an operational stability check rather than the formula-based prediction that turned out not to describe the real profile: post-fix full-Jacobian deviation 1.19e-6, four orders tighter than the number this section previously called comfortable. DRAG-Q-002 ruled: DragError gained a structured `cause` field, and asking to fire each of DRAG-F-003's three nominal causes found two are provably shadowed by the transform call's own stronger precondition, not merely hard to trigger. |
| 2026-09-22 | §28 added, SPEC-drag v1.0 adopted, modules/drag built and gated (DRAG-A-001..-A-010). C_D consumed as the ParameterKind::drag_coefficient registered at L3 step 1, never a constant. Rule-4 search found no clean published ballistic-coefficient case; Sengers et al. (2014, arXiv:1404.7826) Table 4 used instead as a plausibility range, not a registered value -- its terms search FOUND an explicit non-open arXiv distribution licence, the first of this tree's three literature entries where the search found something rather than nothing. DRAG-A-010, written only because speccheck.py flagged DRAG-R-004 (the position Jacobian) as discharged by no test, found a real defect: `a_direction` was missing a factor of \|v_rel\| (~7.7 km/s), a ~7300x error a finite difference caught that inspection of the closed form had not; fixed, and the residual after the fix (1.18e-2) is now the tree's own empirical bound on the terms the approximation still neglects. DRAG-A-005 (Liouville with real drag) needed rebuilding at 300 km after the tree's usual 7331 km test radius proved too thin an atmosphere to move det(Phi) measurably. Two smaller tool bugs fixed in passing: fetch.py and literaturecheck.py both mislabelled every literature entry's terms-summary from a field literature entries never carry. |
| 2026-09-18 | **L2 step 1 `ephemerides` implemented and gated.** §13 added: the `testpo.440` sweep with its denominators (11 354 of 13 201 body cases on the full kernel, 0 skipped for coverage, worst residual 1.06 mm against JPL's 15 mm tolerance), the units design, and six findings from implementation. §3 gains CALCEPH with **CeCILL-B chosen out of its triple licence** and the §5.3.4 obligations recorded. §8.12 records the licence denylist becoming an allowlist. `SPEC-ephemerides` amended to v1.2 (an SPK carries no constants) and `SPEC-frames` to v1.4 (`Frame::BCRS`). |
| 2026-09-22 | §27.7 added, SPEC-srp-analytic to v1.2: composition (SRPA-A-009/010) cannot see a bug in the per-surface law itself, since both sides call identical code and a shared bug cancels. Closed with two closed-form single-plate checks at oblique incidence (magnitude AND direction, from momentum bookkeeping) and a tessellated-sphere cross-check whose discretisation error was measured -- empirically second-order, ratio 4.00 -- BEFORE the gate was written (rule 7's middle form). Proved by injection (rule 5): dropping the specular term's cos(theta) power gave the single-plate check a direct 2.86x magnitude error and collapsed the tessellation's convergence ratio from ~4.0 to ~1.0, since a law-level bug does not shrink with resolution the way discretisation error does; reverted and the suite re-run clean. |
| 2026-09-22 | §27 added, SPEC-macromodel to v2.0, SPEC-srp-analytic v1.1: step 2 reviewed, step 3 continued with box-wing. srp_force moved out of the schema module to modules/srp_analytic (PERT-Q-001's precedent); the schema gained its own force-free round-trip gate (MCRM-A-011/012). MCRM-A-005 corrected: it tested the flat-plate/sphere gap only at rho=0, where the gap is smallest (<=0.22); the dominant term is rho, not delta -- 1.90 at a specular sail (rho=0.9), the shape of this project's one confirmed real-data error (LightSail-2). The "factor of 3" on the diffuse term was itself wrong (it is 1.5) and had reached the committed spec and provenance text, not only a message; corrected. The swing-ratio figure (SS25.10) was corrected twice -- 376x to 367x on the tool's first run, then back to ~376x once a resolution check was extended to the row that needed one, which the extreme-row check alone had missed; the second correction is the one that stands, with a five-point convergence study behind it. RS14 had been described as pinned before the manifest entry actually existed; added and verified. Box-wing (SRPA-Q-001) needs no new force law -- srp_force already sums over N surfaces; SRPA-A-009/010 prove the summation itself, since no test had exercised more than one surface before. |
| 2026-09-22 | §26 added: L4 step 2, the macromodel schema. RHS12 (the plan's own cited paper) reprinted in full in the author's open dissertation, no account needed. Two force laws re-derived independently (momentum bookkeeping for the flat plate, hemisphere integration for the sphere) and Monte Carlo-checked before being trusted; the sphere's coefficient 1+4delta/9 has no rho term, unlike the flat plate's 1+rho+2delta/3, confirming the plan's own "factor of two" warning is two compounding simplifications, not one. FlatSurface redesigned mid-implementation to two named factories after finding a plain struct would let NormalMode and its optional normal disagree. Gated entirely on two degenerate configurations (a spherical cannonball, a black sun-pointing sail), nothing read from RS14's real GPS tables, which stay for L5. |
| 2026-09-22 | §25.11 added, SPEC-shadow to v1.2 (R-031 re-derived, R-033 added): the 99.9 % cancellation figure was a property of ONE traversal (LEO, circular, beta=0), now named. Measured with an actual two-body propagator rather than assumed further: a beta angle within ~1 degree of the eclipse cutoff breaks it substantially (48-75% cancels there); genuine off-apsis eccentricity breaks it modestly (99.4-99.8%); eccentricity AT an apse does not break it at all, exactly, because the two-body problem is time-symmetric about apsis passage regardless of e — the configuration the phrase "an eccentric orbit" most naturally suggests turned out to be the wrong hypothesis, not a smaller effect. SHDW-Q-005 ruled: defer on NEED, not gateability, unlike Q-003. Four faults in the search tool itself along the way, all caught before the numbers were recorded. |
| 2026-09-19 | §25.10 added: the reference definition was itself a family choice. Bolometric limb darkening is 1.97e-2 at LEO — 83x the projection choice and 10 000x the oblateness the PPM is adopted for — but 99.9 % cancels across a passage and none within one. Agreement with the uniform-disc definition is not accuracy. A fourth instance of the empty-comparison fault, in the resolution check written to prevent it. |
| 2026-09-18 | §25 added: L4 step 1, both halves. The hyperbolic silhouette is the normal case at LEO — the opposite of the annular branch, and recorded beside it. An unnamed sixth family choice is worth 99× the oblateness the PPM is adopted for. LI19's five atmospheric cases do not cover the geometry above 1 983 km, which includes Galileo. Three faults found, all of them checks that examined nothing. |
| 2026-09-18 | **Four specification amendments applied**, at the manager's verdict, all three module specs to v1.3: `EOP-A-003` to TN36 §8.2's published tolerance; `FRAME-R-030` corrected to require the kinematic equation-of-equinoxes terms and `FRAME-A-001` restated at 25 mm with the unexplained z-rotation left recorded; `TIME-R-021a` for `eraDtdb`'s UT1 argument; and the prediction/leap-horizon interaction as `SPEC-eop` §4.6 and `SPEC-time` §4.9. §1 and §12.5 corrected: the ITRF-path agreement is **not** "the same algorithm" but two different algorithms whose model difference cancels because each is corrected onto the observed pole, and the kinematic terms are 4 % of T-01 rather than its cause. |
| 2026-09-18 | **L1 steps 1–4 executed and gated.** §12 added: what was built, the generated tidal tables with their agreement against the four IERS published test cases, the six errors the tests caught, the dependency capture, and the measurement showing plan §4 rule 1's required disagreement is absent on the ITRF↔GCRS path. §1 module register updated to *implemented* and gains `core`. §8.11 records the FetchContent capture. |
| 2026-09-18 | **L0 steps 3–7 executed and gated.** §11 added: the toolchain decisions with the rejected alternatives, what each step produced, the layering-as-link-boundary decision, and the platform properties now pinned by test. §3 dependency register populated and marked generated-not-maintained. §0.1 records that the clean-room discipline ended with the merge and that nothing written after it carries a derivation declaration. §8.10's block-recovery example restated one pair at a time with both formulas, having previously compared two ranges whose endpoints came from different block pairs. Moved the stranded R9 patent-search result into §9. |
| 2026-09-18 | Tree merged into the predecessor's repository at the owner's instruction: `/home/rog/odl-self_built` → `/home/rog/odl20Lite/rewrite`, one folder and one repository. Standalone history preserved at `doc/.history/odl-self_built.bundle`. |
| 2026-09-18 | D5/D6 recorded; tree created at `/home/rog/odl-self_built` (since merged, see above) and committed at `bdd80be`; oracle pointer added to §6; the unstated-denominator rule recorded at §8.10 and added to `SPEC-template.md` §8; the unnamed copyright holder raised as the one open title item. |
| 2026-09-18 | §24 added: L3 step 4 and the layer's exit gate. The integrator did know the width — templated on a compile-time size — and step 4 is what removed it. A failing test exposed a refusal naming the wrong reason. Liouville rewritten before drag arrives rather than after. |
| 2026-09-18 | §23 added: L3 step 3. STM-P-1 predicted the FD agreement before the run, including that the textbook figure would be wrong by three orders; the threshold is a band measured in the same run and contains no absolute number; Liouville checks what finite differences cannot; GRAV-Q-006 resolved on measured cost. |
| 2026-09-18 | §22 added: L3 step 2. RKF7(8) read from page images and proved against the order conditions; Fehlberg's own prose count of 40 error coefficients confirms the reading. Table XI's agreement predicted before the run and borne out, including the part predicted to fail. |
| 2026-09-18 | §21 added: L3 step 1. The crossing gate the plan asked for would have fired 14 times and caught nothing; replaced by a register, which is ci.sh gate 10. An annotation is not a permit. The SRP-versus-drag figure compared two vehicles. |
| 2026-09-18 | §20.8-20.9 added: ci.sh exited 1 on NOTICE drift while the individual checkers were green — the submission criterion is now the composed gate's exit code. EPH-Q-005 resolved: IAU 2012 B2 obtained and pinned, and L2 can close. |
| 2026-09-18 | §20.6-20.7 added: the ingestion layer verifies 7 969/7 969 against the issuing authority; l2_floors gains the atmosphere row at both radii; four review corrections, all of the same shape. |
| 2026-09-18 | §20 added: the port reproduces the reference to one ulp; the sweep showed the precision measurement was the wrong shape; the licence allowlist refused a compound licence and was right to. |
| 2026-09-18 | §19 added: L2 step 4's specification, and the finding that the gate the plan names does not exist — NRL publishes no reference value for NRLMSISE-00, with the search over all five distributed files. |
| 2026-09-18 | §8.9 closed: the plan moved into this tree as the canonical and only copy, resolving the stale-copy hazard structurally. Verified its contents; its absence from the predecessor tree is taken on report, because verifying it is the thing this session may not do. |
| 2026-09-18 | Manager audit of the v1.2 self-fix passed; denominator of 121 confirmed. Recorded the **R2 corollary** at §8.8 and flagged the stale local plan copy at §8.9. |
| 2026-09-18 | Acceptance coverage completed at spec v1.2 — twenty tests added, complete §8 coverage tables, no requirement changed. |
| 2026-09-18 | Manager verdict recorded: specs adopted at v1.1; §0 split into declarations, **disclosed context exposure** and adoption; plan rules **R11** and **R12** added at §8.6–8.7; the Horizons determination added to the oracle log; decision register added at §10; D5/D6 marked escalated. |
| 2026-09-18 | Seeded from `doc/REWRITE_PLAN.md` §8; populated for the P1 specification tranche. |
