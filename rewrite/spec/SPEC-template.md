# SPEC-template — the shape every specification in this tree follows

| | |
|---|---|
| **Spec ID** | `TEMPLATE` |
| **Status** | adopted for the P1 tranche; subject to manager review |
| **Version** | 1.2 |
| **Date** | 2026-09-18 |
| **Layer** | — (meta) |
| **Implements** | rule R1 of `../plan/PLAN.md` §1 |

This document is not a specification of a module. It is the **form** that
`SPEC-<module>.md` takes, and the **rules** those documents obey. It exists because R1
makes the spec the legal artefact that demonstrates independent derivation; an artefact
with that job has to be uniform, dated, and auditable, not merely informative.

---

## 0. Why the form matters

Two readers are served by every spec in this tree, and the form is a compromise between
them.

- **The implementer**, who must be able to write the module from the spec alone, having
  never seen any implementation of it. If a spec leaves a sign, a unit, an argument's
  time scale, or an out-of-range behaviour to be inferred, it has failed.
- **The reviewer of title**, who must be able to check that every statement in the spec
  traces to a public document, and that the document was available and was actually
  consulted. That reader cares about §2 and §10 more than about the mathematics.

The second reader is why §2 is mandatory and why "obtained?" is a column in it. A
specification that cites a paper its author could not read is a weaker artefact than one
that says so.

---

## 1. Required sections, in order

Every `SPEC-<module>.md` has these sections, numbered as here, in this order. A section
that does not apply says so in one line; it is not deleted, because a missing number
reads as an omission rather than a decision.

| § | Section | Must contain |
|---|---|---|
| — | Front matter | The header table and the derivation declaration (§2 below) |
| 1 | Purpose and scope | What the module does; an explicit **Not in scope** list naming the spec that does own each excluded item |
| 2 | Normative sources | The source table (§3 below). The legal artefact. |
| 3 | Definitions and conventions | Symbols, units, sign conventions, frames, the time argument of every time-dependent quantity |
| 4 | Required behaviour | The mathematics, stated so an implementer who has seen no implementation can write one |
| 5 | Interfaces | The module's public surface, language-free (§4 below) |
| 6 | Precision and accuracy | Numbers, with the physical quantity each number is a budget for — **and the conversion written out in a form `tools/budgetcheck.py` can evaluate**, not only its result: `100 ns × 7.5 km s⁻¹ = **0.75 mm**`. The result goes in `**bold**`, every factor carries its units, and CI evaluates the left side and compares it. A row that looks like arithmetic and will not parse is a build failure, never a skip. `SPEC-ephemerides`'s first draft stated 10⁻¹³ AU as "15 µm" where it is 14.96 mm; the arithmetic is one multiplication and writing it down is what makes a factor of a thousand visible. |
| 7 | Failure behaviour | The refusal catalogue (§5 below) |
| 8 | Acceptance tests | Concrete, checkable, each naming the **source of its expected values** |
| 9 | Provenance obligations | What the implementation must add to `PROVENANCE.md` when it lands |
| 10 | Open questions for the manager | Everything not settled from public sources, with the author's recommendation |

---

## 2. The derivation declaration

Immediately below the front-matter table, every spec carries this block, filled in
truthfully:

```
**Derivation declaration (plan R1).** This specification was written from the documents
listed in §2 and from no implementation of this module.

**Predecessor access.** The predecessor shares this repository as of 2026-09-18, so the
claim earlier specifications could make — that it lived in a separate tree and was not
reachable — is no longer available to any specification written after that date, and must
not be implied. What is claimed instead, and what is checkable: no file under the
repository root's `src/`, `include/`, `res/`, `scripts/`, `analysis/`, `analyses/`,
`REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or otherwise inspected
during this specification's preparation. Anything about the predecessor that did reach the
author is listed below with its route, and repeated in the exposure register at
`PROVENANCE.md` §0.2.

Numeric acceptance targets carried from prior measurement campaigns are behavioural
observations against public data (plan R4) and are marked as such where they appear.
```

If any part of that is untrue for a given spec, the block is edited to say what was
actually done. **An inaccurate declaration is worse than no declaration**: the value of
the whole exercise rests on this sentence being checkable and true.

---

## 3. The normative sources table

§2 of every spec is a table with exactly these columns:

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|

- **key** — a short tag (`TN36-5`, `ERFA`, `VAL06`) used to cite the source inline in the
  rest of the document. Every equation, constant, and threshold in §§3–8 carries one.
- **locator** — URL or DOI. A URL is recorded with the date it was retrieved, because
  several of the sources this tree depends on are *mutable at a fixed URL* (see
  `SPEC-eop.md` §2 for the worked example).
- **obtained** — one of:
  - `primary` — the normative document itself was retrieved and read.
  - `primary, partial` — retrieved, but only part of it was readable or relevant.
  - `secondary` — only a summary, abstract, slide deck, or third-party restatement was
    available. **Any requirement resting on a `secondary` source must also appear in
    §10.**
  - `not obtained` — cited for completeness, not relied upon. Nothing in §§4–8 may depend
    on it.
- **role** — `normative` (the spec's requirements derive from it), `informative`
  (background), or `interface` (documents a dependency we call rather than a model we
  implement).

Rules:

1. **Every numeric constant in the document carries a source key inline**, at the point of
   use, not only in a table at the end. `32.184 s [TN36-10 §10.1]` — not `32.184 s`.
2. **A constant with no public source is not a constant**; it is an open question (§10).
3. Where a source is multi-licensed, or where its licence constrains use, the table's row
   says so. This feeds the dependency register of `PROVENANCE.md` (plan R7).
4. Where the source is a *dataset* rather than a document, the row records the licence or
   terms of use, because plan rule R6 requires every dataset to be re-fetched from origin
   and every origin has terms.

---

## 4. Interfaces, stated language-free

Decision D1 (implementation language) is open, and specs must not pre-empt it. The
convention:

- Describe **operations**, their **inputs with units and time scales**, their **outputs**,
  and their **failure modes**. Use the form
  `operation_name(input: Type[unit, scale], …) -> Result<Output[unit], ErrorKind>`.
- `Result<T, E>` means "either a T or a diagnostic E" — it is a statement about the
  contract, not about Rust. In a language without sum types the same contract is met by
  an out-parameter plus a status, provided the status **cannot be ignored silently**.
- Say which types are **opaque** (the caller may not construct them from raw numbers) and
  which are **transparent**. Opacity is how the frame-and-scale invariants are enforced,
  so it is a requirement, not an implementation note.
- Say what is **immutable after construction**. Shared mutable state is how errors leak
  between runs (plan §5 constraint 5); a spec that permits it has to justify it.
- Do **not** specify memory layout, allocation, threading, or naming style.

---

## 5. The refusal catalogue

Plan §5 constraint 4: *refuse rather than approximate*. §7 of every spec makes that
concrete as a table:

| id | condition | diagnostic must name | never instead |
|---|---|---|---|

- **id** — `<SPEC>-F-nnn`, referenced from the acceptance tests.
- **diagnostic must name** — the specific values the message carries. "Out of range" is
  not a diagnostic; "requested 2031-04-02T00:00:00 UTC (MJD 62867.0); table covers
  1962-01-01 to 2026-09-17 (MJD 37665.0–61300.0)" is. The rule: **the message must contain
  enough to diagnose the fault without re-running**.
- **never instead** — the plausible-but-wrong behaviour that is forbidden here. This
  column exists because the expensive defects in the predecessor were not crashes; they
  were plausible wrong numbers, and the way to design them out is to name the temptation.

Two standing requirements apply to every spec:

- **R-ERR-1.** Diagnostics are returned values. No module-level, thread-local, or
  otherwise persistent error or warning accumulator exists. A diagnostic that is not
  consumed by the caller is lost, not remembered — never carried into a later operation.
- **R-ERR-2.** A warning status from a dependency is either mapped to one of this spec's
  own refusals or explicitly documented as ignorable, with the reason. Silently
  discarding a dependency's warning is forbidden. (`SPEC-time.md` §7 has the motivating
  case: ERFA returns `+1` for a "dubious year" and still returns a number.)
- **R-ERR-3.** *(plan rule **R12**, adopted 2026-09-18.)* A refusal MAY have **at most one**
  override, and that override MUST be (a) **named unmistakably**, so that reading the name
  tells a reviewer what safety is being given up; (b) **set explicitly per run** — never by
  default, never from the environment, never through a configuration fall-through; and (c)
  **recorded in the run's provenance**, together with the state it was overriding. A refusal
  with no override, with several, or with an unrecorded one is non-conforming.

  The pattern comes from `SPEC-time.md` `TIME-R-052`, where the leap-second table's expiry
  refuses by default and `assume_no_further_leap_seconds` is the single logged way past it.
  Its point is that "refuse rather than approximate" must not make a tool unusable for the
  legitimate case, and the way to have both is to make the exception expensive to take and
  impossible to take accidentally.

---

## 6. Requirement identifiers and traceability

Numbered identifiers, so that review, implementation, and tests can refer to the same
thing:

- `<SPEC>-R-nnn` — a requirement (MUST / MUST NOT).
- `<SPEC>-S-nnn` — a recommendation (SHOULD), always with the magnitude of what is given
  up by not doing it.
- `<SPEC>-F-nnn` — a refusal, from §7.
- `<SPEC>-A-nnn` — an acceptance test, from §8.
- `<SPEC>-Q-nnn` — an open question, from §10.

`MUST`, `MUST NOT`, `SHOULD`, `SHOULD NOT`, and `MAY` are used in the sense of RFC 2119 as
updated by RFC 8174, and only in upper case.

Every `-R-` and every `-F-` is discharged by at least one `-A-`. A requirement no test
exercises is a comment; §8 states, at its end, which requirements are **not** covered by a
test and why.

---

## 7. Precision statements

§6 of every spec gives numbers, and each number is tied to a physical consequence rather
than left as a bare tolerance. The form:

> *Quantity*: budget *X*, which corresponds to *Y* of *(along-track position at 7000 km,
> range, …)*, chosen because *Z*.

The conversion factors used throughout this tree, so that every spec computes the same
way:

| from | to | factor | at |
|---|---|---|---|
| 1 µs of epoch error | along-track position | 7.5 mm | LEO, 7.5 km s⁻¹ |
| 1 µs of UT1 error | position | 0.51 mm | 7000 km geocentric |
| 1 µas of pole/CIP error | position | 0.034 mm | 7000 km geocentric |
| 1 mas of pole/CIP error | position | 34 mm | 7000 km geocentric |
| 1 arcsec of frame error | position | 34 m | 7000 km geocentric |

(The UT1 row uses the Earth rotation rate 7.292 115 146 706 979 × 10⁻⁵ rad s⁻¹; the
angular rows use 1 as = 4.848 137 × 10⁻⁶ rad.)

**Acceleration has no row, and that is deliberate.** Every entry above is a rotation or a
rate, where the consequence is a multiplication. An acceleration budget is not: what a
given acceleration error does to a position depends on *how it is distributed in time*,
and the obvious conversion is wrong by orders of magnitude for the case this tree cares
about most.

Worked, because it nearly became a gate. `SPEC-gravity`'s degree-90 truncation error for
Starlette is 1.2174 × 10⁻¹⁰ m s⁻², and ½aT² over one revolution gives **2.37 mm** against
IERS Table 6.1's stated "3-D orbit accuracy better than 0.5 mm" — apparently a failure by
4.8×. It is not. Truncation error at degree *N* oscillates at about *N* cycles per
revolution and does not accumulate secularly; treated as oscillatory its amplitude is
a/(Nn)² ≈ **1.5 × 10⁻⁵ mm**, five orders of magnitude smaller and far inside the table. The
model was right, the table was right, and a gate built from ½aT² would have failed a
correct implementation.

So an acceleration budget states the acceleration. If it also quotes a position
consequence it MUST name the integration time **and the spectral character of the error** —
secular, once-per-revolution, or high-frequency — and a spec that cannot say which states
the acceleration alone and leaves the orbit-level claim to the layer that fits orbits.

Stating the consequence is what lets a reviewer reject a tolerance as too loose or
dismiss a term as negligible without re-deriving it.

---

## 8. Acceptance tests

Each row of §8 is:

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|

**A statistic cited as an acceptance value carries its formula, not only its number.** An
expected value that is a standard deviation, a sigma level, an RMS, a residual, a condition
number or a confidence interval states *how it was computed* in the row, because the same
notation routinely covers two correct answers to two different questions. Two cases inside two
days made the rule: a coverage count of 121 against 128, differing by whether cross-referenced
identifiers were included; and a blind-block-recovery figure of 9.6–13.9 σ against 11.3–20.8 σ
on identical data, differing by pooled standard deviation against standard error of the means —
a factor of about 1.5, and neither side in error. Both gaps were small enough to read as a
rounding disagreement, and both were caught by someone looking twice. **A checker cannot catch
this class**: it asserts its denominator rather than noticing it. Only the row carrying the
formula makes it visible.

The **source of the expected value** column is the one that matters. Acceptable sources,
in descending order of strength:

1. **A published number in a normative document** — a worked example in a standard, a
   test case in a reference routine's own documentation, a table of verification values.
   Strongest: independent of everything this tree does.
2. **A closed-form or analytic result** — an identity that must hold exactly.
3. **A public measurement** — an IGS orbit, an ILRS normal point, a JPL Horizons table.
   Strong, but carries the measurement's own error budget, which the row must state.
4. **A self-consistency property** — a round trip, an invariant, a convergence order.
   Necessary but weak on its own: a round trip passes for a transformation that is
   consistently wrong. A spec whose §8 contains *only* self-consistency tests is
   incomplete, and §8 must say so.
5. **An oracle comparison against the predecessor** (plan R4). Legitimate, logged in
   `PROVENANCE.md`, and **never a gate on its own** — the predecessor is a black box
   under test, not truth.

---

## 9. What a spec does not contain

- **Implementation code.** Pseudocode is permitted where it removes an ambiguity that
  prose cannot; a compilable artefact is not. If pseudocode is longer than about fifteen
  lines, the prose above it was insufficient — fix the prose.
- **Code, comments, identifiers, file layouts, or prose from any other implementation.**
- **Language, build-system, or dependency-version choices**, except where a dependency is
  itself normative (ERFA is, per plan R7, and appears in §2 with role `interface`).
- **Anything that would have to change if the answer to an open question changed.** If it
  would, it belongs in §10 with a recommendation, not in §4 as a requirement.

---

## 10. Lifecycle

- A spec is **drafted**, then **reviewed by the manager**, then **adopted**. Only an
  adopted spec is implemented (R1).
- A spec changes by version bump, with a dated changelog line at the foot. The versions
  matter because `PROVENANCE.md` records *which version of which spec* a module
  implements.
- When implementation reveals the spec to be wrong, **the spec is corrected first** and
  the correction is reviewed. Code that disagrees with an adopted spec is a defect in one
  of the two, and which one is a decision for the manager, not the implementer.

---

## Changelog

| version | date | change |
|---|---|---|
| 1.2 | 2026-09-18 | Added the rule in §8 that a statistic cited as an acceptance value must carry its formula, prompted by two unstated denominators surfacing within two days (`PROVENANCE.md` §8.10). |
| 1.1 | 2026-09-18 | Added `R-ERR-3` to §5, carrying plan rule **R12** — the named, per-run, provenance-recorded single override — to every spec in the tree. |
| 1.0 | 2026-09-18 | First issue, for the P1 tranche. |
