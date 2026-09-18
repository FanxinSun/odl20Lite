# SPEC-integrators — RK4 and RKF7(8)

| | |
|---|---|
| **Spec ID** | `INTG` |
| **Status** | **draft** 2026-09-18, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-18 |
| **Layer** | L3 `dynamics`, step 2 (`doc/REWRITE_PLAN.md` §3.4) |
| **Depends on** | `SPEC-dynamics.md` (the force surface it integrates), `SPEC-frames.md`, `SPEC-time.md`, `core` |
| **Depended on by** | the STM (step 3), the sensitivity registry (step 4), every campaign at L8 |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no implementation
of this module. The coefficients were read from `FEHLBERG`'s page images and then **proved**
against the order conditions; §3.2 says why that ordering is the point rather than an apology.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 1. Purpose and scope

### In scope

* **RK4**, fixed step, as the simple reference.
* **RKF7(8)**, Fehlberg's embedded pair, with step-size control.
* The **coefficients**, how they were obtained, and — separately — how they were *proved*.
* One property of this pair that is not a defect and must not be rediscovered as one: **its
  error estimator is identically zero on a quadrature problem** (§3.4).

### Not in scope

* **The state transition matrix** — step 3, which integrates alongside.
* **Dense output.** RKF7(8) has no standard continuous extension. Stepping to each observation
  time is the answer this tree takes; `INTG-Q-002` records when that would need revisiting.
* **Any force.** The integrator sees `SPEC-dynamics`'s surface and nothing about what a force
  means.

---

## 2. Normative sources

| key | author / issuer | title | locator | obtained | role |
|---|---|---|---|---|---|
| `FEHLBERG` | E. Fehlberg, NASA Marshall | *Classical fifth-, sixth-, seventh-, and eighth-order Runge-Kutta formulas with stepsize control*, NASA TR R-287, October 1968 | `https://ntrs.nasa.gov/api/citations/19680027281/downloads/19680027281.pdf`, SHA-256 `5553a2a3…a0c8`, 2 625 098 bytes, retrieved 2026-09-18 | primary | **normative** — Table X (RK7(8)), (134), Example (53), Table XI |
| `ORDER-CONDITIONS` | — | The Runge–Kutta order conditions, one per rooted tree: Φ(*t*) = 1/γ(*t*) | classical; no document is pinned because **none is needed** — see §3.2 | — | **normative, and it is mathematics rather than a document** |

**`FEHLBERG` prints everything this step needs, and OCR destroys all of it.** `pdftotext`
renders Table X's β block as `83_ = 841 = B_I = 8sl = B71 = Be1 = B_l = 81ol = _m = _12_ = 0`,
and mangles even the contents page — `TABLE IN.` for III, `SectionXVHI` for XVIII, `8O` for 80,
which are the I/1 and O/0 confusions that matter most for digits. **The page images at 300 dpi
are exact**, and the coefficients there are **rationals**. The manifest entry records this so
that the next reader does not repeat the discovery.

**Hairer's `dop853.f` was considered for DP8(7) and not taken.** It states **no licence terms at
all** — zero occurrences of *licen*, *copyright*, *permission*, *redistribut* or *warranty* in
the whole file — which is the same footing on which plan §6 dropped the Brodowski NRLMSISE C
port and the FECsoft astrolib. **That is recorded as consistent and explicitly not as decisive**;
the deciding ground is §3.2's.

---

## 3. Definitions and conventions

### 3.1 The tableau

Thirteen stages. α, β, the propagating weights *c* (order 7) and the companion *ĉ* (order 8),
and the truncation term `FEHLBERG` (134):

> TE = (41/840)(f₀ + f₁₀ − f₁₁ − f₁₂) *h*

- **INTG-R-001.** The coefficients are **generated source**, emitted by
  `tools/rk_coefficients.py`, never hand-typed into C++. The tool holds the rationals, proves
  them, and writes the doubles.

### 3.2 Why reading numbers off a 1968 scan is acceptable here, and would not be elsewhere

This is plan §4 rule 6's **converse**, and it is the deciding ground for taking RKF7(8):

> Where an independent specification exists — and it need not be a document — the artifact's
> legibility stops mattering. Ask what the thing must satisfy before asking how cleanly it
> prints.

`SPEC-atmosphere` faced the opposite case: NRLMSISE-00 has no specification its reference
implements, so the reference was normative and **nothing in this tree could check it**. A
Runge–Kutta tableau is the other extreme. The order conditions are **exact algebraic identities
over the rationals**, one per rooted tree, and a misread digit fails them. The question is
therefore not whether a 1968 scan can be read reliably; it is whether **what was read is
checkable**, and it is, completely.

**The coefficients being exact rationals is what makes this work**, and it is why this ground
outranks the licence one. A source published as *decimals* satisfies the order conditions only
to rounding, and a transcription error in the last digits is then indistinguishable from that
rounding. Rationals admit no such ambiguity: the identity either holds exactly or it does not.

- **INTG-R-002.** The tableau is verified in **exact rational arithmetic**, never floating
  point, and the verification runs in CI rather than once at drafting.

### 3.3 The two checks are orthogonal, and neither substitutes for the other

This distinction is stated here because the two would otherwise read as belt and braces.

| check | what it establishes | what it cannot |
|---|---|---|
| **the order conditions** (`INTG-A-001`) | the tableau is **a valid RK7(8) pair** | that it is **Fehlberg's**. His derivation has free parameters that were *chosen* rather than forced, so a misreading that still satisfied all 200 conditions would be a different valid method with different error constants — implausible as an OCR failure, but not excluded by this check |
| **Table XI** (`INTG-A-004`) | the tableau is **the one Fehlberg published**, because the accumulated errors depend on the actual coefficients and not only on the order to which they are correct | anything about validity that the order conditions do not already settle, and nothing to four significant figures (§6) |

- **INTG-R-003.** `INTG-A-004` is **the only check of the claim `INTG-A-001` cannot reach**, and
  §8 labels it that way rather than as a third confirmation of the same thing.

**A third check exists and is free**, and it is the strongest evidence that the transcription is
Fehlberg's: the count of **violated order-8 conditions**. There are **115** rooted trees of
order 8, which is exactly the range of `FEHLBERG`'s error coefficients *T*ᵥ (ν = 1 … 115), and
Section XV says in **prose, on page 66, a different page from the table on page 65**: *"our
formula RK7(8) contains only **40** non-zero error coefficients *T*ᵥ"*. The transcription
violates exactly **40**. Prose on one page and mathematics applied to a table on another agree,
and **neither is the scan's rendering of the digits**.

- **INTG-R-004.** That count is asserted, not merely reported. It also proves the pair is a real
  embedding: *c* fails order 8 in exactly 40 places, so the estimate measures something rather
  than being two names for one method.

### 3.4 The estimator is identically zero on a quadrature problem

From the tableau alone: **α₀ = α₁₁ = 0 and α₁₀ = α₁₂ = 1**. So for a right-hand side depending
on *x* alone, (134)'s four evaluations are f(x), f(x+h), f(x), f(x+h) and

> f₀ + f₁₀ − f₁₁ − f₁₂ = 0, **identically**.

Not small — **zero**. The controller on such a system is not conservative and not noisy; it is
**blind**, and will take arbitrarily large steps while reporting no error.

**Orbit propagation is not pure quadrature, so this does not bite the main case.** It bites the
moment *any component* of the right-hand side is a function of time alone — a constant empirical
acceleration, a tabulated thrust or manoeuvre profile, any time-series force — and L4 and L5
will have those. That component then contributes nothing to the norm the controller uses, and
the blindness is **partial and silent**.

- **INTG-R-005.** The property is **recorded next to the integrator**, with its consequence, and
  `INTG-A-005` **exhibits** it rather than describing it (plan §4 rule 5). This is the case where
  a guard nobody builds becomes a defect nobody attributes: the symptom appears in whatever force
  is added later, not in the integrator.

It is Fehlberg's known family property and not a defect in the transcription.

---

## 4. Required behaviour

- **INTG-R-014.** **The 7th-order solution is the one that propagates.** An 8th-order companion
  is computed at every step and used **only** for the estimate. Propagating with it instead —
  *local extrapolation* — costs nothing, gains an order, and is how every modern embedded pair is
  used. **It must not be done here without replacing the gate**: `INTG-A-004` compares accumulated
  errors against Table XI, and that is the only check tying this tableau to *Fehlberg's* method
  rather than merely to a valid RK7(8). Local extrapolation propagates a different solution, so
  those errors would no longer be the quantity Table XI reports — and the gate would go on passing
  its order-condition arm while having quietly stopped checking what it was built for. The choice
  is recorded **next to the stepper**, not only here, because that is where someone will notice
  the wasted order. `INTG-A-006`'s measured slope of 6.90 rather than 7.9 is what says which is
  in use.

- **INTG-R-010.** RK4, fixed step, and RKF7(8) with step-size control, both integrating
  `SPEC-dynamics`'s `StateDerivative` and nothing else. The integrator is **not aware of what a
  parameter means** (`DYN-R-026`).

- **INTG-R-011.** Step-size control acts on the (134) estimate with a safety factor and bounded
  growth and shrink per step, all **stated as named constants** rather than buried.

- **INTG-R-012.** A step whose estimate exceeds the tolerance is **rejected and retried**, and
  the count of rejections is reported with the result. `FEHLBERG`'s own Table XI reports
  evaluations exactly equal to steps × substitutions for both methods — 818 × 13 = 10 634 and
  1423 × 17 = 24 191 — so his counts are of *accepted* steps. A count that conflated the two
  would not be comparable with his.

- **INTG-R-013.** Every signature returns `odl::Result` (constraint 4), no monadic chaining
  (constraint 8), and there is no global state.

---

## 5. Interfaces, stated language-free

**An integrator** takes an initial `State`, a `ForceSet`, a `ParameterSet`, a target epoch and a
tolerance, and returns the propagated `State` together with a **record**: steps accepted, steps
rejected, evaluations, the largest and smallest step taken, and the worst error estimate seen.

**RK4** takes a fixed step instead of a tolerance, and its record carries no rejections.

---

## 6. Precision, and what is predicted **before** the first run

- **INTG-P-1.** The order conditions hold **exactly** in rational arithmetic; there is no
  tolerance on them and any nonzero residual is a transcription error.

- **INTG-P-2.** **Table XI, pre-registered.** What follows was written into this specification
  **before the integrator was first run**, for the reason stated below the table.

`FEHLBERG` Table XI gives, for RK7(8) on Example (53) at *x* = 5 and **tolerance 10⁻¹⁸** (read
at 900 dpi; the exponent is 18 and not 16):

| | steps | evaluations | Δ*y* | Δ*z* |
|---|---|---|---|---|
| RK7(8) | 818 | 10 634 | −0.2509 × 10⁻¹³ | −0.5135 × 10⁻¹³ |

**What this specification expects, written down before the integrator was run, with reasons.**
Otherwise the tolerance is chosen from what came out, which is the defect already ruled on when
a number was written into a specification and measured afterwards.

| quantity | expectation | reason |
|---|---|---|
| **order of magnitude of Δ*y*, Δ*z*** | **10⁻¹⁴, within a factor of 10** — this is the gate | at tolerance 10⁻¹⁸ over 818 steps, controlled truncation could contribute at most ~8 × 10⁻¹⁶, **31× smaller than what is printed**. So the printed errors are accumulated *arithmetic*, and a random-walk estimate √n·ε·\|y\| over ~10⁵ operations gives 2 × 10⁻¹³ — the same order. The IBM 7094's double precision (54-bit fraction) is comparable to IEEE double, so the order should reproduce |
| **sign** | **expected negative for both, and NOT asserted as a gate** | all four published values, for both methods, are negative. Four negatives is weak evidence of a *systematic* component rather than pure round-off, but round-off sign is not a property of the method, and predicting it would be reading a pattern from n = 4 |
| **leading digit** | **NOT expected to match, and a four-figure match would be evidence that something is wrong** | these are a 1968 machine's accumulated arithmetic, in a different operation order, over a step sequence produced by a controller the report describes in prose. Four significant figures of that cannot survive |
| **step count** | **within a factor of 2 of 818**, reported and not gated | the step sequence depends on the safety factor and the growth bounds, which `FEHLBERG` gives in prose rather than as constants |

- **INTG-P-3.** **Step-size insensitivity is demonstrated, not assumed** (plan §3.4 step 2):
  the two-body solution integrated at a sequence of tolerances, with the observed error scaling
  against the tolerance reported, and the **order** recovered from the slope rather than
  asserted.

---

## 7. Failure behaviour

| id | when | what it names |
|---|---|---|
| `INTG-F-001` | the step size underflows the epoch's resolution | the step, the epoch, and the tolerance that demanded it |
| `INTG-F-002` | a step is rejected more than a stated number of times in succession | the count, the last estimate and the tolerance |
| `INTG-F-003` | a force returns a refusal mid-step | the force's diagnostic, unaltered, and the stage it failed at |
| `INTG-F-004` | a non-finite state or estimate | which component, and the step that produced it |
| `INTG-F-005` | a tolerance that is not positive, or a target epoch on the wrong side of the start | the value |

---

## 8. Acceptance tests

| id | what is checked | expected | what it establishes | discharges |
|---|---|---|---|---|
| `INTG-A-001` | **the order conditions, in exact rational arithmetic**: row-sum consistency on all 13 rows; *c* through order 7; *ĉ* through order 8; and *c* at order 8 | 13/13; **85** conditions, 0 violated; **200**, 0 violated; **40 of 115** violated | **that the tableau is a valid RK7(8) pair** — *not* that it is Fehlberg's (§3.3) | R-001, R-002, P-1 |
| `INTG-A-002` | **the 40 against `FEHLBERG`'s own prose**: 115 is the number of order-8 rooted trees and Section XV prints 40 non-zero *T*ᵥ | 40 = 40 | that the transcription is Fehlberg's, **by a route that never touches the coefficient digits** | R-004 |
| `INTG-A-003` | **ĉ − c equals (134)**: exactly (41/840) at positions 0, 10, 11, 12 with signs (−, −, +, +), and zero elsewhere | exact | that the weight columns and the printed truncation term are consistent — **two printed objects checking each other** | R-001 |
| `INTG-A-004` | **Example (53) to *x* = 5 against the closed form**, and the accumulated errors against Table XI | `INTG-P-2`'s pre-registered expectations | **that the tableau is the one Fehlberg published** — the only check of the claim `INTG-A-001` cannot reach | R-010, P-2 |
| `INTG-A-005` | **THE ESTIMATOR IS BLIND ON QUADRATURE, exhibited**: integrate *y*′ = f(*x*) with a known exact integral; the estimate is **exactly zero** while the true error is not | estimate 0.0 exactly; true error non-zero and reported | that §3.4's property is real and present in this implementation, not merely in Fehlberg's algebra | R-005 |
| `INTG-A-006` | **two-body against the analytic solution**, and **step-size insensitivity demonstrated**: error against tolerance over a sequence, with the order recovered from the slope. **The controller's constants are named in the assertion**, so a later change to one of them fails a test that states what it was | the slope, reported and asserted to be consistent with order 7 | plan §3.4 step 2's gate | R-010, R-011, P-3 |
| `INTG-A-007` | **RK4 against two-body**, and its error scaling as *h*⁴ over a step sequence | slope 4, from the measurement | R-010 | R-010 |
| `INTG-A-008` | refusals `INTG-F-001` … `-F-005`, each fired **and shown not to fire** on the adjacent accepted input | the diagnostics | plan §4 rule 5 | F-001…F-005 |
| `INTG-A-009` | **rejected steps are counted separately from accepted ones**, so a count is comparable with `FEHLBERG`'s | evaluations = accepted × 13 + rejected × 13 | R-012 | R-012 |
| `INTG-A-011` | **the controller's constants change the cost and not the answer**: the same arc propagated with all four constants changed, against a **band measured from runs that differ only in the initial step** — same constants throughout, so controller participation is impossible by construction. The claim is relative and cannot be fitted | **step counts 235 and 282 — the cost moved; separation 5.65 × 10⁻⁷ m against a band of 7.02 × 10⁻⁷ m — the answer did not** | R-011, and `INTG-Q-001`'s ruling | R-011 |
| `INTG-A-012` | **the 7th-order solution propagates**, evidenced by `INTG-A-006`'s measured slope being 6.90 and not 7.9 | slope < 7.5 | R-014 | R-014 |
| `INTG-A-010` | **constraint 4 and constraint 8 hold**: every public signature returns `odl::Result`, and `ci.sh` gate 9's tree-wide monadic-chaining check covers these sources as it covers every other module | as stated | plan §5 constraints 4, 8 | R-013 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `INTG-R-003` | A **labelling** requirement on §8 rather than a behaviour: `INTG-A-004` must be presented as the only check of the claim `INTG-A-001` cannot reach, not as a third confirmation. Discharged by §3.3's table and by §8's own wording, which a reviewer reads. A test asserting that a document says something would pass on the sentence and not on the distinction. |
| `INTG-R-020` | A documentation obligation: `PROVENANCE.md` must record the OCR finding, the read-then-prove ordering, the 40-of-115 agreement, and `INTG-P-2`'s expectations **as recorded before the first run**. Discharged by §9 and the entry itself. The pre-registration is the part that cannot be reconstructed afterwards, which is exactly why it is written down rather than tested for. |

### What these gates can and cannot catch

`INTG-A-001` and `INTG-A-004` establish **different claims** and §3.3 says which. Reading them
as two confirmations of one thing would leave the free-parameter gap uncovered while appearing
to have covered it twice.

**`INTG-A-004` cannot be tightened**, and that is a property of the evidence rather than a
weakness in the test. Table XI's four-figure values are a 1968 machine's accumulated arithmetic;
`INTG-P-2` records what is expected of them **before** the first run precisely so that the
tolerance cannot be fitted to the result afterwards. A gate whose tolerance was chosen from its
own output tests nothing.

**The step and evaluation counts are weakly reproducible, and the reason is in the
specification** rather than only the split: `FEHLBERG` describes the step-size control procedure
**in prose**, not as constants, so a different safety factor or growth bound gives a different
step sequence at the same tolerance. A later reader who finds the step count not reproducing
must not "fix" the controller to match a number that was never a gate.

---

## 9. Provenance obligations

- **INTG-R-020.** `PROVENANCE.md` records: that `FEHLBERG`'s text layer is unusable and its page
  images are not; that the coefficients were read from the images and proved rather than trusted;
  the 40-of-115 agreement with the report's own prose; and `INTG-P-2`'s expectations **as
  recorded before the first run**, with the outcome beside them.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `INTG-Q-001` | The controller's constants are this tree's, not Fehlberg's — he gives the procedure in prose. | **RULED: naming and measuring is enough, with one test added, and the test is what makes it enough.** The constants may change the **cost** and must not change the **answer**, which is a property rather than an assumption and is therefore testable. `INTG-A-011`. **The first attempt at it failed**, against an absolute floor of 2 × 10⁻⁷ m carried over from a different run — and the failure was the floor, not the controller: changing **only the initial step**, with the constants held fixed and the accepted-step count unchanged at 235, separates the answer by up to **7.02 × 10⁻⁷ m**, which is *more* than changing all four constants does. So the bound is now measured from same-controller runs, where controller participation is impossible by construction, and the claim under test is relative. An absolute number chosen after seeing the separation would have been a tolerance fitted to its own result. |
| `INTG-Q-002` | **When DP8(7) would need revisiting**, recorded so the question closes rather than lingering: if L6 or L7 needs **dense output** — a continuous extension for evaluating the state at an observation time without stepping to it. RKF7(8) has no standard one. **Stepping to each observation is a perfectly good answer and is what this tree does**; this is a condition, not a plan. |
| `INTG-Q-003` | **RK8(9) is in the same report** (Table XII) at the same cost of transcription, and the same proof would apply. Not taken: the plan asks for one variable-order scheme and two would double the surface for no stated need. Carried. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.0 | 2026-09-18 | First draft, for review. Records that `FEHLBERG`'s OCR is unusable and its page images are exact; that the coefficients are therefore **read and then proved**, which is plan §4 rule 6's converse; that the **order conditions and Table XI establish different claims**, with the free-parameter gap named; the **40-of-115** agreement between the report's prose and mathematics applied to its table; the estimator's **identical vanishing on quadrature**, with a test that exhibits it; and `INTG-P-2`'s expectations for Table XI **written before the first run**. |
