# SPEC-estimation — L7: fits over real data

| | |
|---|---|
| **Spec ID** | `EST` |
| **Status** | **draft v0.1** 2026-10-06 — written **ahead of step 1** for exactly one purpose: to **register, before any fit exists, what the exit gate's "reaches its frozen residual" means numerically, with the statistic's formula named** (§8.1; plan §4 rule 3, the manager's handover `2026-10-06-odl-rewrite-L7.md`). Every other section is a stub that says when it is written. Step 1's rule-4 report is in `~/.claude/handover/2026-10-06-odl-rewrite-L7.REPORT.md` and its five rulings are pending; nothing of step 1 is specified here until the manager has ruled |
| **Version** | 0.1 |
| **Date** | 2026-10-06 |
| **Layer** | L7 `estimation` (`../plan/PLAN.md` §3.8), steps 1 – 5 and the exit gate |
| **Depends on** | the L2 – L6 modules and their specifications (`SPEC-gravity`, `SPEC-perturbations`, `SPEC-dynamics`, `SPEC-stm`, `SPEC-integrators`, `SPEC-measmod`, `SPEC-io-formats`) |
| **Depended on by** | L8 `campaigns` |

**Derivation declaration (plan R1).** Written from the documents listed in §2 and from no implementation of this module (none exists).

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was **opened**, and `oracle/capture.sh` was not opened. **One exposure, disclosed and registered at `PROVENANCE.md` §0.2:** to find where the frozen `G-*` statistic's formula is recorded, a repository-wide text search for the string `validate_sp3` matched and **printed single lines** of the predecessor's `REVIVAL.md` (257, 262, 263, 398), `PROVENANCE.md` (67), `res/configOPS_gnss.txt` (7), `res/configOPS_gps.txt` (4) and `res/LARGE_DATA.md` (56). What those lines say: the script "fits an orbit to an IGS final precise ephemeris", its default is "GPS G01, 2023-01-22", an example invocation is `validate_sp3.sh 2246 20230220000 G05`, and two configuration files are "used by scripts/validate_sp3.sh". **Nothing about the statistic's formula reached me**; the one fact used is the default day, which corroborated a reading of the `G-*` stamps (`YYYY DDD 0000`, day of year) that the stamp pattern and GPS-week arithmetic (week 2246 begins 2023-01-22) had already given. The frozen values are read from `oracle/cases.tsv` only, as the handover directs.

---

## 1. Purpose and scope

L7 turns the forces, the integrators, the variational equations (L3) and the measurement models (L6) into **fits**: the whole force model carried by the `dyn::Force` registry (step 1), batch least squares with the normal equations **scaled by default** (step 2), Levenberg–Marquardt (step 3), a priori constraints, isotropic and RTN (step 4), and the **joint covariance** over the state and every registered parameter (step 5). The exit gate (plan §3.8, amended 2026-09-24): *a fit over real data reaches its frozen residual and reports a joint covariance whose correlations are reproduced by finite differences*; cannonball fits with L4's forces reach the `G-*` frozen residuals with their force parameters stated in the test, and a box-wing fit reading L5's library reaches them too.

**Not in scope:** the campaigns (L8); a sail's own macromodel (§3.9); station displacement (outside the MVP, `SPEC-measmod`); the NRLMSISE-00 altitude cutoffs and the attitude discontinuities as integrator events are **carried into** this layer (plan §3.8) and are decided at the step that fits an arc through them.

**Stubs, and when each is written:** §3 (definitions) and §4 (required behaviour) per step, as each step opens, after its rule-4 report is ruled on; §5 (interfaces), §6 (precision), §7 (refusals) likewise; §9 (provenance) at each close.

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `GRAV`, `PERT`, `DYN`, `STM`, `MEAS` | this tree | the specifications of L2 – L6 | as adopted | `spec/` | primary | normative here for what each module returns; `GRAV-R-041` in particular, which step 1's truncation criterion uses |
| `CASES` | the predecessor (frozen measurement) | `oracle/cases.tsv`, rows `G-01` … `G-03`, and `oracle/ORACLE.md` §6 | frozen 2026-09-18; "recorded 2026-09-24" in §6 | `oracle/` | primary | **ranks last** (plan §4 rule 2): a gross-error detector for the exit gate's fits, never a sole gate |
| `IGSFIN` | IGS, via BKG | the IGS final precise orbits `IGS0OPSFIN_20230220000_01D_15M_ORB.SP3.gz` and `…20230230000…` (GPS week 2246; 15-minute grid) | 2023 | `https://igs.bkg.bund.de/root_ftp/IGS/products/2246/` — both answered `200` to a `HEAD` request on 2026-10-06 (97 869 and 97 823 bytes), no account | **not yet pinned** | the observations of the `G-*` fits; pinned (manifest, hash, licence note) before the exit gate's test is written |
| *the estimator, the scaling, the damping strategy, the covariance* | — | — | — | — | — | filled by steps 2 – 5's rule-4 reports, each before its step is built |

## 3. Definitions and conventions

*Stub.* The exit gate's statistic is defined in §8.1 because that is where it is registered.

## 4. Required behaviour

*Stub — written per step.* The one requirement written now:

- **EST-R-900.** *(registered 2026-10-06, before any fit exists.)* The exit gate's fit residual is the statistic **S** of §8.1, computed from the fit's own output by the formula there; a fit **reaches** its frozen residual exactly when `S ≤ F` for its row of §8.1, to the digits `oracle/cases.tsv` prints. The test **prints** `S`, `N`, the per-component and radial/along/cross values and the largest epoch residual beside the frozen value, whichever way the comparison falls.

## 5. Interfaces · 6. Precision and accuracy

*Stubs.* One budget row is written now because the registered rule's reading of the frozen number depends on it:

| id | quantity | budget | because |
|---|---|---|---|
| `EST-P-1` | the frozen `G-01` value read as a per-component RMS, expressed as the 3-D RMS `S` it would correspond to for residuals isotropic over the three components | 0.0645 m × 1.7321 = **0.1117 m** | `S = √3 ×` the per-component RMS when the three components carry equal mean squares; the registered rule compares `S` with the printed number itself, so a per-component reading of the frozen number would make the rule stricter than its authors' by up to this factor (§8.1's contingency) |

## 7. Failure behaviour

*Stub — the refusal catalogue is written with each step.*

## 8. Acceptance tests

### 8.1 The exit gate: what "reaches its frozen residual" means — **registered 2026-10-06, before any fit runs** (plan §4 rule 3)

**The frozen rows, as `oracle/cases.tsv` prints them:** `G-01` **0.0645 m** (`validate_sp3.sh 2246 20230220000 G01`), `G-02` **0.0573 m** (`2246 20230230000 G01`), `G-03` **0.1870 m** (`2246 20230220000 G05`), each "GNSS 7-parameter fit residual RMS"; `oracle/ORACLE.md` §6 records them as **cannonball** fits whose seventh parameter is the SRP scale, against IGS final orbits. The stamps are read as the IGS long-name product stamp `YYYY DDD 0000` (GPS week 2246 begins 2023-01-22, day 22): **G01 on 2023-01-22 and on 2023-01-23, G05 on 2023-01-22**.

**The statistic.** For a fit of a 24-hour arc to the IGS file's positions `pᵢ` of the satellite (the file's own epochs `tᵢ` inside the arc, **`N` of them — the 15-minute grid, 96 epochs**; `pᵢ` in the ITRS in metres) with the fitted model's positions `qᵢ(θ̂)` at the same epochs, in the ITRS (the model's GCRS position rotated by the same chain as everywhere in this tree):

> **S = √( (1/N) Σᵢ₌₁..N ‖qᵢ(θ̂) − pᵢ‖² )**, in metres.

**Which components:** all three, as the 3-D Euclidean norm of the position difference at each epoch. **Which epochs:** every epoch of the file's grid inside the arc; **none rejected**, none skipped (an epoch the file marks absent or bad refuses the test rather than being dropped). **Which denominator:** `N`, the number of epochs — **not** `N − 7` and **not** `3N`; no degrees-of-freedom correction. Reported beside `S`, never instead of it: `S/√3` (the per-component reading), the radial, along-track and cross-track RMS (each `√(mean of that component²)`), the largest epoch residual, `N`, and the fitted parameters.

**How the frozen numbers are used.** They **rank last** (plan §4 rule 2): they are the predecessor's output and detect only gross error — a sign, an axis, a factor of two. The weight of the exit gate rests on its second clause, the joint covariance's correlations reproduced by finite differences, whose checks are independent of the predecessor. **The registered pass rule: a fit reaches its frozen residual iff `S ≤ F`, `F` the row's printed value** — `0.0645`, `0.0573`, `0.1870` — for the cannonball fit, and the same rule for the box-wing fit reading L5's library.

**The prediction, written with its reason before any fit.** `S/F` between **0.6 and 1.1** for each of the three rows: the cannonball physics is that of L4's `Srp`/ECOM, with L2's field, third bodies, tides and relativity, which the predecessor's fit may not have carried in full; a ratio above 1.1 is a finding, brought to the manager unchanged, not tuned away. (`EST-A-900` states the prediction and the comparison separately; the prediction is not a criterion.)

**Two honest limits, registered with the rule.** (1) **The frozen number's own formula is not recorded anywhere in this tree** — the script is not open to this layer — so the reading above is the executor's. If the predecessor's was a per-component, or a degrees-of-freedom-corrected, RMS, then `S ≤ F` is stricter than its authors' by up to √3 (`EST-P-1`). **Contingency, written now so that it is not a post-hoc loosening:** if `S > F` and `S/√3 ≤ F`, the result is reported as "reaches only under the per-component reading" and **brought to the manager unchanged**; the executor does not call it a pass. (2) The seventh parameter and the cannonball's area and mass are *stated in the test* (plan §3.8); how the SRP scale is carried by L4's `Srp` — which consumes no registered parameter (`PHPR-R-005`) — is decided at the step that builds the fit, with its own ruling, **before** the first fit runs.

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `EST-A-900` | **the exit gate's fit statistic** (*not built; written when the step that builds the fits opens, and committed before the first fit runs*): the cannonball fits of `G-01`, `G-02`, `G-03` and the box-wing fit, `S` by the formula of §8.1, printed beside `F`, `S ≤ F` for each; the prediction `0.6 ≤ S/F ≤ 1.1` recorded beside the result as a prediction | `S ≤ 0.0645`, `0.0573`, `0.1870` m | `CASES` (ranks last); `IGSFIN`; this section | the printed digits | R-900 |

## 9. Provenance obligations

At each step's close, `PROVENANCE.md` records: the step's rule-4 sources and absences with their searches; the rulings and their dates; every frozen sizing before the run it governs, with the outcome beside it; the pins made. The exposure of the predecessor's lines named above is entered in `PROVENANCE.md` §0.2.

## 10. Open questions for the manager

| id | question | author's recommendation |
|---|---|---|
| `EST-Q-001` | **Which formula does the frozen `G-*` statistic carry** — the 3-D RMS over the epochs, a per-component RMS, a degrees-of-freedom-corrected one; which epochs; which denominator? The manager read the script at L4 step 7; this layer may not. | One line from the manager before the fits are built; until then §8.1's reading stands and its contingency is registered. |
| `EST-Q-002` | Step 1's five rulings (plugin granularity and module; the two additive L2 entry points; L4's ranking relativity rows; the truncation criterion; the Jacobians' finite-difference gate), set out with recommendations in the rule-4 report. | As recommended there. |

---

## Changelog

| version | date | change |
|---|---|---|
| 0.1 | 2026-10-06 | first draft, written ahead of step 1 to register the exit gate's statistic before any fit exists; every other section a stub |
