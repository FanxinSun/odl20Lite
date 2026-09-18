# SPEC-sensitivities — the parameter sensitivity registry

| | |
|---|---|
| **Spec ID** | `SENS` |
| **Status** | **draft** 2026-09-18, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-18 |
| **Layer** | L3 `dynamics`, step 4 (`doc/REWRITE_PLAN.md` §3.4), and **the layer's exit gate** |
| **Depends on** | `SPEC-dynamics.md`, `SPEC-integrators.md`, `SPEC-stm.md` |
| **Depended on by** | estimation (L7), which forms the joint covariance this makes possible |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no implementation
of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 1. Purpose

**S** = ∂**x**(*t*)/∂**p**, integrated alongside the state and Φ, for **whatever set of
parameters is registered**, with no fixed width anywhere. This is what plan §2 says the layer
exists for: the predecessor's fixed-width sensitivity block is why it could grid-scan where this
tree should give a joint covariance.

---

## 2. Normative sources

| key | what |
|---|---|
| `PLAN` | `doc/REWRITE_PLAN.md` §2, §3.4 step 4, §4 rules 1–8, §5 constraints 1–10 |
| `DYN` | `SPEC-dynamics.md` — `ParameterId`, `ParameterJacobian`, `∂a/∂p` |
| `STM` | `SPEC-stm.md` — the variational state S joins |

The variational equation for **S** is classical; plan §4 rule 8's question is answered as
`SPEC-stm` §2 answers it — the specification is mathematics.

---

## 3. Definitions

### 3.1 What is integrated

> d**S**/d*t* = A(*t*) **S** + B(*t*),  **S**(*t*₀) = **0**,  B = [0 ; ∂a/∂**p**]

- **SENS-R-001.** **S**(*t*₀) = **0** because the *initial* state does not depend on the
  parameters. That is a statement about what is being differentiated, not an initialisation
  convenience, and it is why S and Φ have different initial conditions in the same system.

- **SENS-R-002.** The variational state is 6 + 36 + 1 + 6·*n*, with *n* = `registry.size()` **at
  run time**.

### 3.2 The half of the gate that is easy to fake

Plan §3.4 step 4: *a registered parameter's column matches finite differences, **and registering
a second parameter requires no change to the integrator***.

**The second clause is the one that gets faked, and a compile-time width is how.** An integrator
templated on `std::size_t N` passes every test at *n* = 1 and again at *n* = 2 — **by
recompiling**. That is a change to the integrator wearing the costume of a template argument, and
nothing in the test output distinguishes it from a width the integrator never knew.

- **SENS-R-003.** The integrator is generic over the **container**, not over a width. `y0.size()`
  is the only answer available to it, and one instantiation serves every *n*. `SENS-A-002`
  asserts that structurally, at compile time, as well as behaviourally.

This required changing `SPEC-integrators`' steppers from `std::array<double, N>` to a
`StateVector` concept. **That change belongs to this step**: it is what makes the second clause
true rather than merely untested.

### 3.3 Addressed by identity, never by column

- **SENS-R-004.** A sensitivity column is retrieved by `ParameterId`. There is no
  `column(std::size_t)`, and the storage's layout is not a caller's business — `DYN-R-001` at the
  scale it was written for.

---

## 4. Required behaviour

- **SENS-R-010.** Registering a parameter gives it a column **automatically**; no force, no
  integrator and no caller declares a width.
- **SENS-R-011.** The result carries the registry's width, and a column requested with an id from
  another registry is refused naming both.
- **SENS-R-012.** **A refusal names its own reason.** An id issued by the *right* registry but
  **after** the object carrying values was built is a different fault from an id from a
  *different* registry, and the two must not share a message. `SENS-F-002`.
- **SENS-R-013.** `odl::Result` throughout; no monadic chaining; no global state.

---

## 5. Interfaces, stated language-free

A propagation with sensitivities takes the force set, the parameter values, **the registry**, an
initial state, a span and a tolerance, and returns the propagated state, Φ, the width, a column
per registered parameter addressed by identity, the integrator's record, ∫ tr A d*t*, and the
neglected-velocity bound.

---

## 6. Precision

- **SENS-P-1.** The column's agreement with finite differences is judged exactly as `STM-P-1`
  judges Φ's: a band measured in the same run from a family that cannot be affected by whether
  the analytic column is right, with the perturbation family chosen from the prediction rather
  than from the result. **No absolute number appears in the assertion.**

---

## 7. Failure behaviour

| id | when | what it names |
|---|---|---|
| `SENS-F-001` | a column requested with an id from another registry | both registries and the width |
| `SENS-F-002` | an id issued by **this** registry but after the value-carrying object was built | that it *was* this registry, the id's slot, the object's width, and what to do — **never** the wrong-registry message |

---

## 8. Acceptance tests

| id | what is checked | expected | discharges |
|---|---|---|---|
| `SENS-A-001` | **a registered parameter's column against finite differences**, judged by a same-run band | measured: band 1.13 × 10⁻⁹, best agreement 1.48 × 10⁻¹⁰; and the column is **not trivially zero** | R-001, R-010, P-1 |
| `SENS-A-002` | **registering a second parameter requires no change to the integrator**: *n* = 1, 2 and 3 through the same code, each column present and correct, **and one integrator instantiation serves all three** (compile-time) | width follows the registry; **accepted steps 19, 19, 19** | R-002, R-003, R-010 |
| `SENS-A-003` | a column addressed by identity; a foreign id refused | `SENS-F-001` | R-004, R-011, F-001 |
| `SENS-A-004` | **the late-declaration refusal names its own reason** and not the wrong-registry one | `SENS-F-002`, whose message says *"AFTER this ParameterSet was built"* | R-012, F-002 |
| `SENS-A-005` | constraint 4 and constraint 8, via `ci.sh` gate 9 | as stated | R-013 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `SENS-R-020` | A documentation obligation: `PROVENANCE.md` must record that the steppers' width change belongs to this step, and the late-declaration diagnostic a failing test exposed. Discharged by §9 and the entry a reviewer reads. |

### What this gate can and cannot catch

**`SENS-A-002`'s structural half is the load-bearing one.** The behavioural half — *n* = 1, 2, 3
all working — would pass against a compile-time width too, because each *n* would instantiate its
own integrator and every one of them would be correct. What distinguishes the two is that here
there is **one** instantiation, and that is a compile-time fact rather than a runtime observation.

**The equal step counts are evidence, not proof.** 19 accepted steps at every *n* says the
sensitivity block did not disturb the step-size control *for these forces*, whose parameter
derivatives are small. A parameter whose column grew faster than the state would change the
count legitimately. The number is reported for that reason rather than asserted.

---

## 9. Provenance obligations

- **SENS-R-020.** `PROVENANCE.md` records the width change to the steppers as belonging to this
  step, and the late-declaration diagnostic that a failing test exposed.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `SENS-Q-001` | **A `ParameterSet` takes its width from the registry at construction**, so one built before a parameter is declared cannot hold it — and refuses, now saying so specifically (`SENS-F-002`). The alternative is for the set to track the registry and grow. Not taken: a set that silently gained a slot would make "which parameters does this set carry" a question with a time-dependent answer, and L7 will hold these across an estimation loop. Flagged because the refusal is the kind a caller meets early and finds annoying. |
