# SPEC-stm — the state transition matrix

| | |
|---|---|
| **Spec ID** | `STM` |
| **Status** | **draft** 2026-09-18, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-18 |
| **Layer** | L3 `dynamics`, step 3 (`../plan/PLAN.md` §3.4) |
| **Depends on** | `SPEC-dynamics.md` (`∂a/∂r` and `∂a/∂v`), `SPEC-integrators.md`, `SPEC-gravity.md` (`GRAV-Q-006`) |
| **Depended on by** | the sensitivity registry (step 4), estimation (L7) |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no implementation
of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 1. Purpose and scope

**In scope.** Φ(*t*, *t*₀) = ∂**x**(*t*)/∂**x**(*t*₀), integrated **alongside** the state; the
variational equations it satisfies; and the gate that checks it against finite differences.
Also `GRAV-Q-006`, which resolves here.

**Not in scope.** Parameter sensitivities — step 4. Φ is the state block only.

---

## 2. Normative sources

| key | what | role |
|---|---|---|
| `PLAN` | `../plan/PLAN.md` §3.4 step 3, §4 rules 1–8, §5 constraints | normative |
| `DYN` | `SPEC-dynamics.md` — `∂a/∂r` and `∂a/∂v` as two named 3×3 blocks | normative |
| `INTG` | `SPEC-integrators.md` — what propagates Φ | normative |
| `GRAV` | `SPEC-gravity.md` `GRAV-Q-006` and §5's interface | normative |

The variational equations are classical and this specification states them rather than citing a
document; plan §4 rule 8's question is answered the same way `SPEC-integrators` §3.2 answers it —
**there is an independent specification and it is mathematics**, so correctness is checkable
without an authority.

---

## 3. Definitions and conventions

### 3.1 What is integrated

With **x** = (**r**, **v**) and **x**′ = (**v**, **a**(*t*, **r**, **v**)):

> A(*t*) = ∂**x**′/∂**x** = ⎡ 0    I  ⎤
>                          ⎣ ∂a/∂r  ∂a/∂v ⎦
>
> dΦ/d*t* = A(*t*) Φ,   Φ(*t*₀, *t*₀) = I

- **STM-R-001.** Φ is integrated **alongside** the state by the same stepper, not by a separate
  pass over a stored trajectory. 6 + 36 = 42 components advance together, so Φ is evaluated at
  exactly the states the trajectory actually visited.

- **STM-R-002.** The two blocks of A come from `DYN`'s `∂a/∂r` and `∂a/∂v` and from nowhere else.
  A force that declares no velocity dependence contributes a zero `∂a/∂v` block **and** its
  neglected bound (`DYN-R-027`), which is reported with the result rather than discarded.

### 3.2 The implementation is analytic wherever the derivative is derivable

- **STM-R-003.** ∂a/∂r and ∂a/∂v are **analytic** wherever they can be derived. A
  finite-differenced implementation is refused.

This is not a preference about elegance. **The gate is agreement with finite differences**, and a
finite-difference implementation checked against finite differences *would pass while checking
nothing* — the two would share every error: the same step size, the same cancellation, the same
truncation. It is the shape this project has met repeatedly and now has a rule for: a check that
passes without examining the thing it was supposed to examine.

For a point mass, ∂a/∂r is the gravity-gradient tensor and is elementary:

> **a** = −μ **r**/*r*³,  ∂a/∂r = −μ ( I/*r*³ − 3 **r** **r**ᵀ/*r*⁵ )

### 3.3 The comparison's threshold, settled before it is run

Plan §4 rule 7: *a threshold is not chosen by whoever will be judged by it.* The finite-difference
agreement is exactly the threshold that gets fitted, because **the perturbation size is a free
parameter and the agreement depends on it** — a criterion chosen after seeing the numbers is a
criterion chosen from them.

**The strongest form is available here, so it is used.** A comparator measured in the same run,
from a family that cannot be affected by what is under test:

- **STM-R-004.** The finite-difference Jacobian is computed at **several perturbation sizes**,
  and the **spread among those estimates** is the band. The analytic Φ must lie within it.
  The spread depends only on the differencing — the step size, the truncation, the integrator's
  noise — and **not at all on whether the analytic derivative is correct**, so it cannot be
  fitted to the result it judges. This is `INTG-A-011`'s shape applied one level up.

**And the middle form as a cross-check, because a pathological band would otherwise pass.**
`STM-P-1` predicts the band's size from first principles, before the first run.

- **STM-P-1.** **A central difference of a *propagated* state carries three errors, not the two a
  textbook analysis gives**:

  | | size |
  |---|---|
  | truncation | ~ *h*²/6 × \|third derivative\| |
  | machine round-off | ~ ε/*h* |
  | **integrator noise** | ~ **τ/(2*h*)**, with τ the integrator's own error in each propagated state |

  The third is the one that dominates and the one an analysis of finite differencing alone
  omits. Each perturbed trajectory carries τ **independently**, and the difference divides by
  2*h* — so it is **amplified by the very step that suppresses truncation**.

  Minimising *h*²/6 + τ/(2*h*) gives *h* ≈ (3τ/2)^⅓ and an agreement of:

  | propagation tolerance τ | optimal *h* | predicted best agreement |
  |---|---|---|
  | 10⁻¹⁰ | 5.3 × 10⁻⁴ | 1.4 × 10⁻⁷ |
  | **10⁻¹²** | **1.1 × 10⁻⁴** | **6.6 × 10⁻⁹** |
  | 10⁻¹⁴ | 2.5 × 10⁻⁵ | 3.0 × 10⁻¹⁰ |

  **If only machine round-off mattered the answer would be ε^⅔ ≈ 3.8 × 10⁻¹¹**, and a criterion
  set from that figure would be **wrong by three orders in the direction that fails a correct
  implementation**. That is why this is written down before the first run rather than after.

- **STM-R-005.** The gate asserts the analytic Φ lies within the measured band (`STM-R-004`),
  **and** that the band itself is within an order of magnitude of `STM-P-1`'s prediction. The
  first is the check; the second is what stops a degenerate band from passing it.

### 3.4 `GRAV-Q-006` resolves here

`SPEC-gravity` §5's entry points all return values and fill no caller-supplied buffer, so a
`gradient()` entry point is **additive** — decided at step 1 as `DYN-R-028`. What was deferred to
here is the **cost**.

- **STM-R-006.** Both routes are **measured, not argued** (the handover's words): the second
  derivatives taken from the **same** forward-column recursion that already computes the
  acceleration, against a **second traversal** of the field.

  **The measurement.** The recursion already carries P̄ and its first derivative; the second
  follows the same three-term pattern and costs **one array and one line**:

  > d²P̄[n] = a(n,m)·(2·dP̄[n−1] + u·d²P̄[n−1]) − b(n,m)·d²P̄[n−2]

  | degree | P̄, dP̄ | P̄, dP̄, d²P̄ | incremental | a second traversal |
  |---|---|---|---|---|
  | 180 | 0.035 ms | 0.037 ms | **+7.5 %** | +100 % |
  | 360 | 0.146 ms | 0.151 ms | **+4.0 %** | +100 % |
  | 2190 | 5.797 ms | 6.675 ms | **+15.2 %** | +100 % |

  **So the same-recursion route wins by between 6.6× and 25×**, and the choice follows that.
  Two honest qualifications: at degree 2190 the increment is **15 %, which is not "negligible"**
  — the phrase in the handover is right in direction and optimistic in size, because the extra
  array costs cache at that length; and **+100 % is a lower bound on the alternative**, since a
  real second traversal repeats the harmonic sum as well as the Legendre column.

  `GRAV-Q-006` is therefore resolved: **take the second derivatives from the same recursion**,
  and `SPEC-gravity`'s `gradient()` entry point is additive (`DYN-R-028`) so nothing else moves.

---

## 4. Required behaviour

- **STM-R-010.** Φ(*t*₀, *t*₀) = I exactly, and a propagation of zero length returns it unchanged.
- **STM-R-011.** The result carries the **neglected-velocity bound** summed over forces that
  declared one (`DYN-R-027`), because a Φ whose ∂a/∂v block is partly absent is not the same
  object as one whose block is zero.
- **STM-R-012.** Every signature returns `odl::Result`; no monadic chaining; no global state.

---

## 5. Interfaces, stated language-free

**A propagation with sensitivities** takes an initial state, a `ForceSet`, a `ParameterSet`, a
target epoch and a tolerance, and returns the propagated state, **Φ**, the integrator's record,
and the neglected bound.

---

## 6. Precision

`STM-P-1` (§3.3) is the whole of it, and it is a prediction rather than a tolerance.

---

## 7. Failure behaviour

| id | when | what it names |
|---|---|---|
| `STM-F-001` | a force supplies neither a `∂a/∂v` block nor a bound | the force |
| `STM-F-002` | Φ becomes non-finite | the component and the epoch |
| `STM-F-003` | the integrator refuses mid-propagation | the integrator's diagnostic, unaltered |

---

## 8. Acceptance tests

| id | what is checked | expected | discharges |
|---|---|---|---|
| `STM-A-001` | **Φ against finite differences, judged by a band measured in the same run.** FD Jacobians at several perturbation sizes; the analytic Φ must lie inside their spread | agreement within the band; **no absolute number appears in the assertion** | R-003, R-004, R-005 |
| `STM-A-002` | **the band is not pathological**: its size against `STM-P-1`'s prediction | within an order of magnitude of 6.6 × 10⁻⁹ at τ = 10⁻¹² | P-1, R-005 |
| `STM-A-003` | **Φ(t₀,t₀) = I exactly**, and a zero-length propagation returns it | exact | R-010 |
| `STM-A-004` | **the analytic gravity gradient against its own finite difference**, for the point-mass form, judged the same way | within a same-run band | R-003 |
| `STM-A-005` | **Φ's determinant against Liouville's theorem, in the form that survives drag**: d(det Φ)/d*t* = tr(A) det Φ, integrated as **det Φ = exp(∫ tr A d*t*)**, with ∫ tr A d*t* carried alongside Φ as one extra scalar. An invariant the finite-difference comparison cannot see, because no derivative estimate enters it | det Φ = exp(∫ tr A d*t*) to the integrator's accuracy; and for L3's forces the integral is **exactly zero**, so the conservative case is the *special case* rather than the statement | R-001 |
| `STM-A-005b` | **the same law where the determinant MOVES**: a dissipative force *a* = −*k***v** gives tr(A) = −3*k* exactly, so det Φ = exp(−3*kt*) in closed form | measured: ∫ tr A d*t* = −0.36 against a closed form of −0.36, det Φ = 0.6977 = exp(−0.36) at *t* = 600 s — **not the trivial det = 1 case** | R-001 |
| `STM-A-007` | refusals `STM-F-001` … `-F-003`, fired **and shown not to fire** on the adjacent input | the diagnostics | F-001…F-003 |
| `STM-A-008` | the neglected-velocity bound survives into the result | as stated | R-011 |
| `STM-A-009` | **A's blocks come from `DYN` and nowhere else**: perturbing a force's `∂a/∂r` moves Φ, and the module contains no derivative of its own — structural, and asserted by there being no second path to a Jacobian | as stated | R-002 |
| `STM-A-010` | **constraint 4 and constraint 8**: every public signature returns `odl::Result`, and `ci.sh` gate 9's tree-wide check covers these sources | as stated | R-012 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `STM-R-006` | **A timing measurement, recorded rather than asserted.** §3.4 carries both routes' costs at three degrees, and the resolution follows them. It is **not** in the automated suite: a wall-clock assertion is machine-dependent, and one tuned until it passed on this machine would be a threshold chosen by whoever is judged by it — plan §4 rule 7, which this very step was told to apply. What *is* automated is the consequence: `SPEC-gravity`'s interface remains additive (`DYN-A-018`, step 1), so the decision can be acted on without disturbing a caller. |
| `STM-R-020` | A documentation obligation: `PROVENANCE.md` must record `STM-P-1` **as written before the first run**, with the outcome beside it, and `GRAV-Q-006`'s two measured costs. The pre-registration is the part that cannot be reconstructed afterwards, which is why it is written down rather than tested for. |

### What these gates can and cannot catch

**`STM-A-005` is written in the general form deliberately, and that is not tidiness.** "det Φ = 1"
is true for every force L3 has, and becomes **false** the moment drag arrives at L4 with
tr(∂a/∂v) < 0. Whoever met that failure would restrict the test to conservative forces or delete
it — losing the only check on Φ that involves no difference estimate, *at exactly the moment the
dynamics get harder*. In the integrated form the conservative case is the special case, and at L4
the check gets **stronger** rather than merely surviving: with drag the determinant actually
moves, so it verifies the dissipation rate instead of confirming a constant. `STM-A-005b` makes
it sharp today rather than waiting for L4 to do so. **A test that is trivially satisfied now and
sharp later is worth more than one that has to be rescued.**

**`STM-A-001` alone would not be enough**, and `STM-A-005` is why it is not alone. A band
measured from finite differences bounds how well Φ matches *a finite-difference estimate of
itself*; Liouville's theorem is an **invariant of the true Φ** that no difference estimate
enters. If both hold, the two failure modes that would have to conspire are unrelated.

---

## 9. Provenance obligations

- **STM-R-020.** `PROVENANCE.md` records `STM-P-1` **as written before the first run**, with the
  outcome beside it, and the `GRAV-Q-006` cost measurement with both routes' numbers.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `STM-Q-001` | **Φ is propagated in the same units the state is** — kilometres — so its position-position block is dimensionless while its position-velocity block is in seconds. That is the conventional choice and it means Φ's condition number carries the time span. An alternative is to scale Φ by the state's own magnitudes. Not taken; flagged because L7 will invert things built from it. |
