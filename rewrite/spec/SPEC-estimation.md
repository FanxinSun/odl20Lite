# SPEC-estimation — L7: fits over real data

| | |
|---|---|
| **Spec ID** | `EST` |
| **Status** | **draft v0.2** 2026-10-06 (first written as v0.1 the same day) — written **ahead of step 1** for exactly one purpose: to **register, before any fit exists, what the exit gate's "reaches its frozen residual" means numerically, with the statistic's formula named** (§8.1; plan §4 rule 3, the manager's handover `2026-10-06-odl-rewrite-L7.md`). Every other section is a stub that says when it is written. Step 1's rule-4 report is in `~/.claude/handover/2026-10-06-odl-rewrite-L7.REPORT.md` and its five rulings are pending; nothing of step 1 is specified here until the manager has ruled. **v0.2, the same day:** the manager has ruled on all five (R1 – R5) and has **answered `EST-Q-001`** (§8.1, amended; the first text kept). By R1 the force-model plugins of step 1 live in a module **of their own, `forcemodel`**, apart from the estimator, and step 1's specification is `SPEC-forcemodel.md` (written as that step builds); this document keeps steps 2 – 5 and the exit gate |
| **Version** | 0.2 |
| **Date** | 2026-10-06 |
| **Layer** | L7 `estimation` (`../plan/PLAN.md` §3.8), steps 1 – 5 and the exit gate |
| **Depends on** | the L2 – L6 modules and their specifications (`SPEC-gravity`, `SPEC-perturbations`, `SPEC-dynamics`, `SPEC-stm`, `SPEC-integrators`, `SPEC-measmod`, `SPEC-io-formats`) |
| **Depended on by** | L8 `campaigns` |

**Derivation declaration (plan R1).** Written from the documents listed in §2 and from no implementation of this module (none exists).

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was **opened**, and `oracle/capture.sh` was not opened. **One exposure, disclosed and registered at `PROVENANCE.md` §0.2:** to find where the frozen `G-*` statistic's formula is recorded, a repository-wide text search for the string `validate_sp3` matched and **printed single lines** of the predecessor's `REVIVAL.md` (257, 262, 263, 398), `PROVENANCE.md` (67), `res/configOPS_gnss.txt` (7), `res/configOPS_gps.txt` (4) and `res/LARGE_DATA.md` (56). What those lines say: the script "fits an orbit to an IGS final precise ephemeris", its default is "GPS G01, 2023-01-22", an example invocation is `validate_sp3.sh 2246 20230220000 G05`, and two configuration files are "used by scripts/validate_sp3.sh". **Nothing about the statistic's formula reached me**; the one fact used is the default day, which corroborated a reading of the `G-*` stamps (`YYYY DDD 0000`, day of year) that the stamp pattern and GPS-week arithmetic (week 2246 begins 2023-01-22) had already given. The frozen values are read from `oracle/cases.tsv` only, as the handover directs.

*(v0.2, 2026-10-06.)* **The manager has accepted the exposure as disclosed**: the five files are the revival's own documents, several written by the manager's session, so nothing of the predecessor's implementation came through; the rule stays as it is now applied — **searches are scoped to `rewrite/`**. And **`EST-Q-001` is answered from the manager's own record** (not from the predecessor's code, and not by this layer opening anything): `validate_sp3.sh` is the revival's script, written in the manager's session history; it still sits under `scripts/`, so the rule against opening it stands.

---

## 1. Purpose and scope

L7 turns the forces, the integrators, the variational equations (L3) and the measurement models (L6) into **fits**: the whole force model carried by the `dyn::Force` registry (step 1), batch least squares with the normal equations **scaled by default** (step 2), Levenberg–Marquardt (step 3), a priori constraints, isotropic and RTN (step 4), and the **joint covariance** over the state and every registered parameter (step 5). The exit gate (plan §3.8, amended 2026-09-24): *a fit over real data reaches its frozen residual and reports a joint covariance whose correlations are reproduced by finite differences*; cannonball fits with L4's forces reach the `G-*` frozen residuals with their force parameters stated in the test, and a box-wing fit reading L5's library reaches them too.

**Not in scope:** the campaigns (L8); a sail's own macromodel (§3.9); station displacement (outside the MVP, `SPEC-measmod`); the NRLMSISE-00 altitude cutoffs and the attitude discontinuities as integrator events are **carried into** this layer (plan §3.8) and are decided at the step that fits an arc through them.

**Stubs, and when each is written:** §3 (definitions) and §4 (required behaviour) per step, as each step opens, after its rule-4 report is ruled on; §5 (interfaces), §6 (precision), §7 (refusals) likewise; §9 (provenance) at each close. *(v0.2.)* **Step 1 is not specified here.** Ruling R1 (2026-10-06) puts the force-model plugins in a module of their own, `forcemodel` (`SPEC-forcemodel.md`, Spec ID `FMOD`), because the estimator of steps 2 – 5 depends only on `dyn`'s `Force` interface and not on `gravity`, `tides`, `thirdbody` or `relativity`; this document's §3 – §7 are written for steps 2 – 5 when each opens, and its §8.1 is the exit gate's, which spans all five.

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `GRAV`, `PERT`, `DYN`, `STM`, `MEAS` | this tree | the specifications of L2 – L6 | as adopted | `spec/` | primary | normative here for what each module returns; `GRAV-R-041` in particular, which step 1's truncation criterion uses |
| `CASES` | the predecessor (frozen measurement) | `oracle/cases.tsv`, rows `G-01` … `G-03`, and `oracle/ORACLE.md` §6 | frozen 2026-09-18; "recorded 2026-09-24" in §6 | `oracle/` | primary | **ranks last** (plan §4 rule 2): a gross-error detector for the exit gate's fits, never a sole gate |
| `IGSFIN` | IGS, via BKG | the IGS final precise orbits `IGS0OPSFIN_20230220000_01D_15M_ORB.SP3.gz` and `…20230230000…` (GPS week 2246; 15-minute grid) | 2023 | `https://igs.bkg.bund.de/root_ftp/IGS/products/2246/` — both answered `200` to a `HEAD` request on 2026-10-06 (97 869 and 97 823 bytes), no account | **not yet pinned** | the observations of the `G-*` fits; pinned (manifest, hash, licence note) before the exit gate's test is written |
| `MGR` | the manager (`odl maintainer`) | his record of the revival's `validate_sp3.sh` output, given in answer to `EST-Q-001` | 2026-10-06, cross-session message | his own session history; the script is under `scripts/` and **is not opened here** | relayed, as stated | **what the frozen `G-*` statistic is**: a 3-D position RMS (below, §8.1). It ranks with `CASES`, last; it is recorded here as what it is — a statement of the manager's, with the numbers he quotes, not a document this tree holds |
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
| `EST-P-1` | the frozen `G-01` value read as a per-component RMS, expressed as the 3-D RMS `S` it would correspond to for residuals isotropic over the three components | 0.0645 m × 1.7321 = **0.1117 m** | `S = √3 ×` the per-component RMS when the three components carry equal mean squares; the registered rule compares `S` with the printed number itself, so a per-component reading of the frozen number would make the rule stricter than its authors' by up to this factor (§8.1's contingency). **RETIRED 2026-10-06 (v0.2), kept visible:** the manager's answer to `EST-Q-001` settles that the frozen numbers are 3-D (§8.1), so the per-component reading is not a contingency; `S/√3` is still printed, beside `S`, and carries no pass or fail. Replaced by `EST-P-2` |
| `EST-P-2` | *(v0.2, replaces `EST-P-1`)* the window the **denominator's** unknown opens: `S` computed with `N − 7` against `S` computed with `N`, for the same residuals | the ratio of the two statistics, squared: 1.0386 × 1.0386 = **1.0787**, which is 96 over 89 to the digits printed (`N` = 96 for a 24-hour day-file on the 15-minute grid, `N − 7` = 89) | the divisor is computed inside the fitting program the frozen number came from, which neither the manager nor this layer opens, and the printed output cannot show it — it is a factor common to the three components (§8.1). If `S_N ≤ F < S_{N−7}`, the fit sits inside this window and §8.1's registered report applies; outside it (`S_{N−7} ≤ F` or `F < S_N`) the denominator does not matter |

## 7. Failure behaviour

*Stub — the refusal catalogue is written with each step.*

## 8. Acceptance tests

### 8.1 The exit gate: what "reaches its frozen residual" means — **registered 2026-10-06, before any fit runs** (plan §4 rule 3)

**The frozen rows, as `oracle/cases.tsv` prints them:** `G-01` **0.0645 m** (`validate_sp3.sh 2246 20230220000 G01`), `G-02` **0.0573 m** (`2246 20230230000 G01`), `G-03` **0.1870 m** (`2246 20230220000 G05`), each "GNSS 7-parameter fit residual RMS"; `oracle/ORACLE.md` §6 records them as **cannonball** fits whose seventh parameter is the SRP scale, against IGS final orbits. The stamps are read as the IGS long-name product stamp `YYYY DDD 0000` (GPS week 2246 begins 2023-01-22, day 22): **G01 on 2023-01-22 and on 2023-01-23, G05 on 2023-01-22**.

**The statistic.** For a fit of a 24-hour arc to the IGS file's positions `pᵢ` of the satellite (the file's own epochs `tᵢ` inside the arc, **`N` of them — the 15-minute grid, 96 epochs**; `pᵢ` in the ITRS in metres) with the fitted model's positions `qᵢ(θ̂)` at the same epochs, in the ITRS (the model's GCRS position rotated by the same chain as everywhere in this tree):

> **S = √( (1/N) Σᵢ₌₁..N ‖qᵢ(θ̂) − pᵢ‖² )**, in metres.

**Which components:** all three, as the 3-D Euclidean norm of the position difference at each epoch. **Which epochs:** every epoch of the file's grid inside the arc; **none rejected**, none skipped (an epoch the file marks absent or bad refuses the test rather than being dropped). **Which denominator:** `N`, the number of epochs — **not** `N − 7` and **not** `3N`; no degrees-of-freedom correction. *[v0.2, 2026-10-06, the first text kept: whether the frozen number's own denominator is `N` or `N − 7` is not known and cannot be told from its printed output; `S` with `N − 7` is reported beside `S` with `N`, and the amendment below governs the outcome.]* Reported beside `S`, never instead of it: `S/√3` (the per-component reading), the radial, along-track and cross-track RMS (each `√(mean of that component²)`), the largest epoch residual, `N`, and the fitted parameters.

**How the frozen numbers are used.** They **rank last** (plan §4 rule 2): they are the predecessor's output and detect only gross error — a sign, an axis, a factor of two. The weight of the exit gate rests on its second clause, the joint covariance's correlations reproduced by finite differences, whose checks are independent of the predecessor. **The registered pass rule: a fit reaches its frozen residual iff `S ≤ F`, `F` the row's printed value** — `0.0645`, `0.0573`, `0.1870` — for the cannonball fit, and the same rule for the box-wing fit reading L5's library.

**The prediction, written with its reason before any fit.** `S/F` between **0.6 and 1.1** for each of the three rows: the cannonball physics is that of L4's `Srp`/ECOM, with L2's field, third bodies, tides and relativity, which the predecessor's fit may not have carried in full; a ratio above 1.1 is a finding, brought to the manager unchanged, not tuned away. (`EST-A-900` states the prediction and the comparison separately; the prediction is not a criterion.)

**Two honest limits, registered with the rule.** (1) **The frozen number's own formula is not recorded anywhere in this tree** — the script is not open to this layer — so the reading above is the executor's. If the predecessor's was a per-component, or a degrees-of-freedom-corrected, RMS, then `S ≤ F` is stricter than its authors' by up to √3 (`EST-P-1`). **Contingency, written now so that it is not a post-hoc loosening:** if `S > F` and `S/√3 ≤ F`, the result is reported as "reaches only under the per-component reading" and **brought to the manager unchanged**; the executor does not call it a pass. (2) The seventh parameter and the cannonball's area and mass are *stated in the test* (plan §3.8); how the SRP scale is carried by L4's `Srp` — which consumes no registered parameter (`PHPR-R-005`) — is decided at the step that builds the fit, with its own ruling, **before** the first fit runs.

**Amendment, 2026-10-06 (v0.2): `EST-Q-001` answered by the manager from his own record; the first text above is kept and this block governs where they differ.** The revival's `validate_sp3.sh` runs its fit with a **900 s step, the IGS file's own sampling**, so the epochs of the statistic are the file's own (`N` = 96 for a 24-hour day-file) — as registered above. It prints a "position RMS" with "x / y / z RMS" beside it, and in the recorded runs **the first, squared, equals the sum of the three squared** (0.3432² = 0.2946² + 0.1451² + 0.0997², to four digits). Therefore:

1. **The frozen numbers are 3-D**, as §8.1 reads them. The per-component contingency (limit (1)'s second sentence, `EST-P-1`) is **resolved and retired**: `S/√3` is reported beside `S` and carries no pass or fail.
2. **The denominator, `N` or `N − 7`, is computed inside the fitting program, which neither the manager nor this layer opens**; the printed output cannot show it, because it is a factor common to the three components. **Registered now, before any fit runs:** the test prints **both**
   > **S_N = √( (1/N) Σᵢ ‖qᵢ(θ̂) − pᵢ‖² )** and **S_{N−7} = √( (1/(N − 7)) Σᵢ ‖qᵢ(θ̂) − pᵢ‖² ) = S_N · √(N/(N − 7))**,

   the 7 being the fit's seven parameters (the six of the state and the SRP scale), side by side and beside `F`. The three outcomes, by `F` the row's printed value:

   | outcome | condition | what the test and the report say |
   |---|---|---|
   | **reaches** | `S_{N−7} ≤ F` | the fit reaches its frozen residual **under either denominator** |
   | **reaches only under the `N` reading** | `S_N ≤ F < S_{N−7}` | stated as exactly that, **brought to the manager unchanged**; the executor does not call it a pass |
   | **does not reach** | `F < S_N` | a finding, brought to the manager unchanged; the first-text prediction (0.6 – 1.1) is compared beside it |

   `EST-P-2` is the width of the middle row, 3.86 % of `S_N`. The pass rule of the first text, `S ≤ F` with `S = S_N`, is the middle row's lower edge; **an unqualified "reaches" needs the first row.**
3. Nothing else of the frozen number's formula is recorded: which components (all three) and which epochs (the file's own) are as registered.

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `EST-A-900` | **the exit gate's fit statistic** (*not built; written when the step that builds the fits opens, and committed before the first fit runs*): the cannonball fits of `G-01`, `G-02`, `G-03` and the box-wing fit, `S` by the formula of §8.1, printed beside `F`, `S ≤ F` for each; the prediction `0.6 ≤ S/F ≤ 1.1` recorded beside the result as a prediction. *(v0.2:)* **both** `S_N` and `S_{N−7}` printed beside `F`, `S/√3` beside them, and the outcome row of §8.1's amendment named for each fit (`S_{N−7} ≤ F` is the unqualified pass) | `S ≤ 0.0645`, `0.0573`, `0.1870` m (first text); `S_{N−7} ≤` the same for an unqualified pass (v0.2) | `CASES` (ranks last); `IGSFIN`; `MGR`; this section | the printed digits | R-900 |

## 9. Provenance obligations

At each step's close, `PROVENANCE.md` records: the step's rule-4 sources and absences with their searches; the rulings and their dates; every frozen sizing before the run it governs, with the outcome beside it; the pins made. The exposure of the predecessor's lines named above is entered in `PROVENANCE.md` §0.2.

## 10. Open questions for the manager

| id | question | author's recommendation |
|---|---|---|
| `EST-Q-001` | **Which formula does the frozen `G-*` statistic carry** — the 3-D RMS over the epochs, a per-component RMS, a degrees-of-freedom-corrected one; which epochs; which denominator? The manager read the script at L4 step 7; this layer may not. | One line from the manager before the fits are built; until then §8.1's reading stands and its contingency is registered. **ANSWERED 2026-10-06 (v0.2), by the manager, from his own record:** 3-D (the printed "position RMS" squared equals the sum of the printed x / y / z RMS squared, to four digits), on the file's own 900 s sampling; the denominator, `N` or `N − 7`, is inside the fitting program and not shown by its output — **both are reported, and the three outcomes of §8.1's amendment are registered before any fit runs.** |
| `EST-Q-002` | Step 1's five rulings (plugin granularity and module; the two additive L2 entry points; L4's ranking relativity rows; the truncation criterion; the Jacobians' finite-difference gate), set out with recommendations in the rule-4 report. | As recommended there. **RULED 2026-10-06 (v0.2):** all five yes, with the manager's conditions — R1 the plugins in a module of their own (`forcemodel`); R2 the two entry points, checked by the field's own identities; R3 L4's ranking rows corrected as step 1's **first** commit, one band per term from sources independent of the corrected output, registered before the re-run, with a sweep of every consumer; R4 the criterion as proposed, RMS over the sphere with the orbit maximum beside it, a function and a table at L4's four points; R5 L6's form, with the comparator's own error, `ν` measured over the whole stencil and asserted point by point, the field's `F` a rigorous bound with its looseness stated, and every control's power written down before the run. F4 and F5 carried. Recorded at `PROVENANCE.md` §40.3 onward. |

---

## Changelog

| version | date | change |
|---|---|---|
| 0.1 | 2026-10-06 | first draft, written ahead of step 1 to register the exit gate's statistic before any fit exists; every other section a stub |
| 0.2 | 2026-10-06 | the manager's rulings on step 1 recorded (`EST-Q-002`); **`EST-Q-001` answered** — the frozen numbers are 3-D, the per-component contingency retired (`EST-P-1`), and the denominator contingency registered **before any fit runs**: `S_N` and `S_{N−7}` both reported, three outcomes named (`EST-P-2`, §8.1's amendment, `EST-A-900`); the predecessor-access paragraph carries the accepted exposure and the scoping rule; step 1's specification moved to `SPEC-forcemodel.md` by R1. The first texts are kept visible |
