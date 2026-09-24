# Rewrite plan — a fully-owned reimplementation of the validated ODL pipeline

**Status:** L0–L4 closed; **L5 open** — steps 1–4 to come; L6–L9 not started. **D1 decided 2026-09-18: C++20** (§7).
**Canonical:** `plan/PLAN.md` at the repository root — this file — with one file per layer step
under `plan/subplan_L0/` … `plan/subplan_L9/`, laid out by the owner's plan-file rule of
2026-09-23. This project has a single outcome, so it has one plan and one execution order: §3,
L0 → L9, each layer's steps in order, each step in its own file. The plan moved here that day
from `rewrite/doc/REWRITE_PLAN.md` with **every section number unchanged**, so a citation such as
"plan §3.5 step 4" or "§4 rule 7" still resolves — a layer's entry in §3 names each step and
points at its file. There is no other plan document. The ground rules that earlier drafts held as
a separate table are merged into the layer sequences of §3, where they are performed rather than
recited. Rule identifiers R1–R12, cited from the specifications, are indexed in the Appendix and
resolve into those steps. **Paths** in this plan and its step files are relative to `rewrite/`,
the rewrite's tree, unless they begin with `plan/`.
**Basis:** `doc/ownership-analysis.md` (the layer-by-layer ownership analysis) and `oracle/`
(what the predecessor measurably does, frozen). The predecessor's own documents are history, not
inputs: nobody working from this plan reads that tree — see §0.
**Date:** redrafted 2026-09-18, superseding the draft of the same date; moved and split into
step files 2026-09-23, content unchanged.

The governing principle, from the ownership analysis: **copyright protects expression, not
ideas.** Every method in the predecessor lives in published papers and public standards. A tree
rebuilt from those sources — never from UCL's files — is the author's own. This plan turns that
principle into one build order, where every layer is a single sequence of steps and the legal
hygiene is inside the steps, so following the sequence *is* keeping the discipline.

Usual caveat: none of this is legal advice, and UK database right is a live consideration for
compiled datasets (it is why L5's Data step rebuilds from publishers — §3.6).

---

## 0. What this is, what it is not, and how it is built

**It is** a clean reimplementation of the *validated core* — the subset of the predecessor that
the 2026 revival measured and documented, plus the capabilities the revival added (laser and
optical measurement fits) — from public sources only, in a new architecture, under the owner's
own licence, with every module and constant traceable to a public document.

**It is not** a port. Roughly half the predecessor is dead weight the revival never validated or
found broken (TIE-GCM table drag, magnetic/charge forces, a diverging box-wing path, the
`analyses/` attic, the FECsoft ephemeris container, the PNG output path). None of it is
rewritten. Scope = what was measured, plus what the science needs next.

**What it fixes:** title, completely, for the new tree. It can be licensed, sold, published with
a clean code-availability statement, or given away, at the owner's sole discretion. **What it
does not fix:** the evidence base (LightSail-2 stays n=1 for the HAMR case until someone supplies
a second precise trajectory) and the landscape (UCL SGNL's differentiable ray-tracing work is
public, open-sourced, award-winning, and ahead of L9 here). Ownable, not more novel.

**How it is built — the discipline in four sentences.** The predecessor is *run, never read*:
its entire role is the frozen oracle under `oracle/`, already run to exhaustion so that nobody
ever needs to touch it again, and no file ever crosses over from it — numbers cross (facts about
what a program did, logged), files do not. Every module begins as a specification written from
named public sources by someone with nothing of the predecessor open, and implementation follows
the adopted spec — which is why the specs are written in the clean session and the oracle was
operated here: the two jobs partition by who has read what, and neither session could do the
other's. Both the owner and the assistant *have* read the predecessor, so the discipline reduces
copying risk rather than eliminating it — and whoever has read the tree is the leak channel, so
outbound handovers are audited for predecessor detail as carefully as inbound reading, and
anything that arrives in the clean session's context is registered in the ledger's exposure
section, because "did not read it" and "nothing about it reached me" are different claims and
only the first is the executor's to control. Three one-time obligations are already discharged:
the licence has been in the tree since commit one (`bdd80be`); the patent check is done and
clear (L9 step 1 records it); the oracle is frozen.

---

## 1. The layers

The work is cut into **ten layers**, stacked so that each one uses only what is below it. The
cut follows where the *specification actually lives* in the open literature, because that is
what decides whether a module can be written and owned outright: copyright covers expression,
not the physics, so anything rebuilt from the papers and the public standards is this tree's.
The column headed "the spec lives in" is therefore not a reading list — it is the ownership
argument, per layer.

| # | layer | what it is | the spec lives in | what must be shown |
|---|---|---|---|---|
| **L0** | `foundation` | repository, build, CI, the manifest data layer, provenance automation | nothing external — written here | **nothing** — no antecedent; original work |
| **L1** | `time-frames` | timescales, Earth orientation, GCRS↔ITRS, TEME, RTN/DYB | IERS Conventions (TN36); ERFA | the derivation trail |
| **L2** | `environment` | planetary ephemerides, geopotential, solid/ocean/pole tides, relativity, third bodies, atmosphere | IERS Conventions 2010; EGM2008/GOCO; JPL DE; NRLMSISE-00, DTM-2013, JB2008 | the derivation trail |
| **L3** | `dynamics` | equations of motion, integrators, state transition, parameter sensitivities | Montenbruck & Gill; Vallado | the derivation trail |
| **L4** | `forces-analytic` | shadow, analytic SRP, drag, Earth albedo/IR, antenna thrust, yaw attitude, ECOM | Li/Ziebart/Bhattarai 2019; Fliegel & Gallini; Rodríguez-Solano 2012; Knocke 1988; Steigenberger 2018; Kouba 2009; Montenbruck 2015; Arnold 2015 | the derivation trail |
| **L5** | `spacecraft` | macromodel library — geometry and optical properties, as data | Fliegel & Gallini 1992/1996; ESA GSC Galileo metadata 2017; IAC; CSNO 2019; Cabinet Office; IGS metadata SINEX; CNES Jason macromodel (Cerri 2010) | the derivation trail **and** per-value citations — see L5's note on database right |
| **L6** | `io-measurements` | SP3, RINEX, SINEX/ANTEX, CPF, ILRS normal points, TLE, Horizons; SGP4; measurement models | published format specifications (IGS, ILRS); Vallado/Hoots for SGP4 | the derivation trail |
| **L7** | `estimation` | batch least squares, Levenberg–Marquardt, joint covariance | textbook | the derivation trail |
| **L8** | `campaigns` | the real-data validation campaigns — **the MVP gate** | this tree's own harness | **nothing** — original, but see the note below |
| **L9** | `raytracer` | pixel-array ray tracing, thermal re-radiation, grid files — *optional, after L8* | Ziebart 2001/2004; Ziebart et al. 2005; Adhya; Bhattarai et al. 2022 | the derivation trail — the method is published in enough detail to reimplement |

**Reading the last column.** It is not a grade of ownership — every layer here ends up owned.
It says what you would have to produce if someone asked you to prove it.

*Nothing* means there is no antecedent: the layer is original work, so derivation cannot arise
and the commit is the whole answer. *The derivation trail* means UCL implemented the same
physics, and the claim is that a fresh implementation from the sources named alongside is yours
regardless — which is sound, but is a claim, and rests entirely on how the code was produced.
Identical code by a different route gives a different answer. That is why those layers carry
§3's spec-first step and the provenance ledger, and why L0 and L8 do not.

**The note on L8.** Its campaign logic and the revival-era Python tooling come from the
predecessor's tree (§6), and they are yours on a *different* argument from every other row:
they are new files written in 2026, not modifications to UCL's, and UCL's rights attach to the
2014–2020 work. Had they been edits to existing files they would be derivative and could not
carry over. The distinction is load-bearing — keep new work in new files.

Three things are **not** rebuildable from the literature and are therefore never taken: the
predecessor's source text; its authored spacecraft surface models and generated grid files,
which a substantial compilation right can protect even where copyright is thin (L5, L9); and
its documentation prose. Each is replaced from the sources named above rather than worked
around.

### The rule — one layer, one sequence

**A layer has exactly one ordered sequence of steps, and at most one step of it is open at a
time.** Concretely:

1. Every step appears in exactly one layer. Nothing is worked on in two places.
2. A step runs the whole of §3.11 — spec, review, data, dependencies, implement, test,
   ledger, gate — before the next step of that layer opens.
3. A layer's sequence does not open until the layer below has passed its exit gate.
4. Work that does not fit the open step is written down and left; it does not become a second
   front inside the layer.

**Why the rule exists.** The predecessor's whole failure mode was the plausible wrong number,
and every instance of it was an interface believed rather than checked. A dependency graph with
several items open at once is how that happens: an interface is used by the next item before
the first has been through its gate, and when it turns out wrong the error is already inside
two modules. A single sequence per layer costs parallelism and buys the property that at any
moment there is exactly one thing that could be wrong, and it is the thing being worked on.

The earlier draft of this plan ran twenty features against a dependency graph. It is the same
work; it is now ten sequences instead of twenty fronts, and §3 records which features each
layer absorbed so nothing was quietly dropped.

**Order, effort and the one blocker.** L0→L8 is the critical path; L9 is the owner's call after
L8. Interfaces are validated bottom-up: the P1 specifications found six errors in this plan, and
implementation will find the same class of error in the specifications, so the three adopted
specs of L1 are *implemented* before further specs are written — writing another twelve on
unvalidated interfaces banks twelve specs' worth of undetected error. **D1 is settled (C++20),
so nothing blocks L0**, and L0 steps 3–7 are the live work.

*Effort, labelled judgement:* L0–L8 is roughly eleven M-class steps and a handful of S-class
ones — order of **3–5 focused months** solo-with-assistant. L9 adds **1–2 months**. No more
precision than that is honest yet.

---

## 2. Architecture — the "different design" made concrete

A reviewer comparing the two trees must see two designs, not one design twice; this section is
that requirement made specific, and conformance to it is an adoption criterion in every
step's Review step. The layers of §1 are these modules; names indicative:

```
core      the vocabulary every module above shares and nothing below: odl::Result and its
          error type (D7), Vec3/Mat3, the unit and angle types. Added at L1, reported rather
          than assumed; it exists because a Result alias that lived in `time` would have made
          every module depend on `time` to report an error.
time      TT/TAI/UTC/UT1/GPS/TDB, leap-second table with enforced expiry; no file access
frames    GCRS <-> ITRS (IAU 2006/2000A via ERFA), TEME <-> GCRS, RTN and DYB rotations
eop       EOP 20 C04 + finals2000A.all ingestion, splice, interpolation, tidal terms;
          its own module, NOT part of time - it parses files and holds a coverage policy
env       planetary ephemerides (SPK via CALCEPH), geopotential + solid/ocean/pole tides,
          atmosphere (NRLMSISE-00), space-weather ingestion
forces    the plugin surface: each force implements
              accel(t, state, params) -> (a, da/dstate, da/dparams)
dynamics  integrators (RK4; DP8(7) or RKF7/8 class), state-transition and parameter
          sensitivities integrated generically for WHATEVER parameter set is registered -
          no fixed-width sensitivity block
measmod   measurement models against one interface; station/site registry
estimate  batch LSQ, Levenberg-Marquardt, scaled normal equations BY DEFAULT, a priori
          (isotropic and RTN), joint covariance over state + any registered parameters
io        format readers/writers and the Horizons client; own grid format
sc        spacecraft macromodel library AS DATA, per-value citations
harness   the validation campaigns of L8 as executable definitions; CI runs them
```

Design points that are both improvements and the independence evidence:

- **Generic parameter framework.** Any force parameter (SRP scale, C_D, ECOM terms, per-face
  optical coefficients) registers itself and automatically gains a sensitivity column and a
  place in the joint covariance — natively delivering the joint (C_D, A·C_R/m) region the
  publication route needs and the predecessor could only grid-profile.
- **Two predecessor defects become design requirements.** Error state is scoped to one
  propagation run and cannot leak into the next; normal equations are solved scaled, always.
- **Manifest-driven data layer.** Every external file is declared with URL, SHA-256 and licence
  note; a fetcher populates a cache; provenance becomes a build artefact instead of a habit.
- **Typed, validated configs.** An unknown key or silently-ignored setting is an error — half
  the predecessor's "plausible wrong number" incidents began as silent config fall-throughs.
- **Epochs and frames live in the type system.** No default timescale, no way to build an epoch
  from a bare number, no frame-reinterpret operation. The two most expensive traps of the
  revival (a 69 s TDB/UTC confusion ≈ 518 km; a TEME/GCRS mix-up ≈ 46 km that preserves every
  magnitude) become unrepresentable rather than discouraged.

---

## 3. The layers — one sequence each

Ten layers, ten sequences. **Each layer below is a single numbered list, worked top to bottom,
one step open at a time.** Every step is marked **DONE** or **TODO**: a step is DONE only when
its gate has passed, so a written-and-adopted specification with no implementation behind it is
still TODO, and the explanation says so. §3.11 says how any one step is executed.

---

### 3.1 L0 `foundation` — **7 of 7 done; exit gate passed 2026-09-18**

**Entry:** none, this is the floor. **Language: C++20** (D1, decided 2026-09-18).

*Steps 4–7 were reordered on 2026-09-18: CI was step 4 and could not close before the things it
runs existed. It is now last, which is what was actually executed.*

1. **DONE** — Repository created with the licence in the first commit (`bdd80be`) → [`subplan_L0/L0-1.md`](subplan_L0/L0-1.md)
2. **DONE** — Oracle frozen → [`subplan_L0/L0-2.md`](subplan_L0/L0-2.md)
3. **DONE** — Manifest format and fetcher → [`subplan_L0/L0-3.md`](subplan_L0/L0-3.md)
4. **DONE** — NOTICE generation from the manifest → [`subplan_L0/L0-4.md`](subplan_L0/L0-4.md)
5. **DONE** — Spec-coverage checker → [`subplan_L0/L0-5.md`](subplan_L0/L0-5.md)
6. **DONE** — Reproducible-build flags → [`subplan_L0/L0-6.md`](subplan_L0/L0-6.md)
7. **DONE** — CI running the gates offline from the cache → [`subplan_L0/L0-7.md`](subplan_L0/L0-7.md)

**Exit gate — passed.** A clean clone builds, tests and regenerates NOTICE with one command,
CI green from cached data alone. Verified independently 2026-09-18 from a clean copy of the
tree: **9 of 9 tests pass**, including `boundary.undeclared_dependency_refused`.

**What L0 did about C++20's structural risk, and it is the part to show a reviewer.** In C++ a
directory is not a boundary; a link target is. Every §2 module is its own CMake target with its
own `PUBLIC` include directory, seeing exactly what it names in `DEPENDS`, so reaching across
the layering **fails to compile**. `tests/boundary/` proves it in both directions — declared
compiles, undeclared is a `WILL_FAIL` compile test — because asserting that something is
impossible needs a failing test for the same reason FRAME-A-009 does. That is the C++ recovery
of what Rust's crate boundaries would have given free, and it is why no §2 amendment was needed.

---

### 3.2 L1 `time-frames` — **4 of 4 done; exit gate passed 2026-09-18**

1. **DONE** — `odl::Result` → [`subplan_L1/L1-1.md`](subplan_L1/L1-1.md)
2. **DONE** — `time` → [`subplan_L1/L1-2.md`](subplan_L1/L1-2.md)
3. **DONE** — `eop` → [`subplan_L1/L1-3.md`](subplan_L1/L1-3.md)
4. **DONE** — `frames` → [`subplan_L1/L1-4.md`](subplan_L1/L1-4.md)

**Exit gate — passed.** Verified independently 2026-09-18: **58 of 58 tests pass**, 384
artefacts byte-identical. The type-level requirement holds and holds structurally — **frame is a
template parameter**, so `State<Gcrs>` and `State<Teme>` are unrelated types and there is no tag
to reassign. That is the answer to the risk flagged at L0, and it is the second time a C++20
"optional at implementation" hazard was closed by making the compiler the enforcer.

**Specification amendments this layer required — all applied 2026-09-18, specs at v1.3.**

- `SPEC-eop` EOP-A-003 now takes TN36 §8.2's own bound (1 µas, 0.05 µs) instead of "the last
  published digit", with the contradiction against EOP-R-007 written out so the next reader sees
  why it changed rather than only that it did.
- `SPEC-frames` FRAME-R-030 corrected: the kinematic equation-of-equinoxes terms are **required**,
  and v1.2's conflation of them with the equinox-based TOD route is named as the error — all
  three of Vallado's ambiguities are in the geometric nutation terms this chain never uses.
  FRAME-A-001 restated at 25 mm **with the pure-rotation assertion kept as a separate check**,
  because a radial component would mean a scale or units error, which no rotation can produce.
  The 3.45 × 10⁻⁴ arcsec z-rotation stays recorded as unexplained, including that `eraEect00`
  and the two-term form differ by only 8 × 10⁻⁶ arcsec, so neither accounts for it.
- `SPEC-time` TIME-R-021a records `eraDtdb`'s UT1 argument and its 0.2 ns justification.
- Both specs record the prediction/leap-horizon interaction as rules rather than as notes about
  one product (SPEC-eop §4.6, SPEC-time §4.9, TIME-R-056/057, EOP-R-054/056).
- FRAME-Q-001 corrected from "convention" to **model difference**, carrying the arithmetic and
  the observation that a gate set from the kinematic terms would have been 25× too small.

Two new tests were needed to discharge the new requirements, and writing one of them taught
something: built as a UTC calendar epoch, a request past the leap horizon is refused by
TIME-F-004 before the EOP layer sees it at all — correct behaviour, so the test goes through a
uniform scale, which is also the realistic path since the dynamics works in TT/TAI.

---

### 3.3 L2 `environment` — **4 of 4 done; exit gate passed 2026-09-18**

Everything the spacecraft moves through or is pulled by, with no reference to the spacecraft
itself. A gravity field is the environment; a drag force is a spacecraft property and lives in
L4.

**Entry:** L1 exit gate — **passed 2026-09-18**, amendments applied the same day, so this
layer is open.

1. **DONE** — `ephemerides` → [`subplan_L2/L2-1.md`](subplan_L2/L2-1.md)
2. **DONE** — `gravity` → [`subplan_L2/L2-2.md`](subplan_L2/L2-2.md)
3. **DONE** — `tides-relativity-thirdbody` → [`subplan_L2/L2-3.md`](subplan_L2/L2-3.md)
4. **DONE** — `atmosphere` → [`subplan_L2/L2-4.md`](subplan_L2/L2-4.md)

**Exit gate — PASSED 2026-09-18.** `tools/ci.sh` exits 0: 10 gates, 211 tests, 634 artefacts
byte-identical, 29 manifest entries verifying, NOTICE regenerated from the manifest. `EPH-Q-005`
resolved before the close as its disposition required — IAU 2012 Resolution B2 obtained and
pinned rather than cited through a secondary, which turned out to carry something the citation
did not: the au is used with **all** time scales, so unlike the GM values of `PERT-A-028` it has
no TDB/TCB dependence. The criterion for a step being reported complete is now that exit code,
not a list of individually passing tools — the one that fails is the one not on the list.

*The gate as originally written:* every model reproduces its published reference values **where
they exist**, and
where they do not, the specification records the search that established their absence and names
what stands in their place with the cost of the substitution stated (§4 rule 8); and every
external table in the layer is manifest-declared with a hash.

---

### 3.4 L3 `dynamics` — **4 of 4 done; exit gate passed 2026-09-18**

Small in code and the hinge of the design: this is where the predecessor's fixed-width
sensitivity block becomes a registry, which is what later gives a joint covariance instead of a
grid scan.

**Entry:** L2 exit gate.

1. **DONE** — Force plugin surface → [`subplan_L3/L3-1.md`](subplan_L3/L3-1.md)
2. **DONE** — Integrators → [`subplan_L3/L3-2.md`](subplan_L3/L3-2.md)
3. **DONE** — State transition matrix → [`subplan_L3/L3-3.md`](subplan_L3/L3-3.md)
4. **DONE** — Parameter sensitivity registry → [`subplan_L3/L3-4.md`](subplan_L3/L3-4.md)

**Exit gate — PASSED 2026-09-18.** `tools/ci.sh` exits 0: 12 gates, 236 tests. The plugin
surface carries a trivial test force end to end, sensitivities included, with nothing in the
integrator aware of what the parameter means — which was **false when step 4 opened** and is the
reason the second clause of that sentence is a gate and not a description.

> **Correction found at L4 step 4: the surface was frozen without a provenance channel.**
> `ForceEvaluation` carries an acceleration and two Jacobians and nothing else, so the first force
> whose result carries provenance — drag, with the space-weather snapshot identity and the
> verification flag that `ATMO-R-031` requires in *every* result — has to drop it at the plugin
> boundary, and everything estimated through the plugin loses it. The requirement was on the books
> at L2 step 4, **before** this surface was frozen, and the manager's L3 handover did not carry it
> into the surface's design. Freezing against a trivial force could not have revealed it, and that
> is the honest limit of the strategy: a trivial force proves a surface does not **require** what
> a trivial force lacks, and cannot reveal what a real force must **carry**. Amended additively at
> L4 step 4 — a typed provenance field, empty for forces that have none — with every existing
> force and test unchanged, on the same terms as `DYN-Q-001`'s edit of closed layers.

---

### 3.5 L4 `forces-analytic` — **7 of 7 done; exit gate passed 2026-09-24**

Every non-gravitational force that can be written in closed form. The ray-traced treatment of
the same physics is L9 and deliberately later: this layer must stand alone, because it is what
the MVP needs.

**Entry:** L3 exit gate.

> **This layer's boundary with L5 does not close, and it is found here because L4 is where it
> first has to be executed.** §3.5 says step 5 needs L5 step 1; §3.6 says L5 opens at L4 step 2;
> §5 constraint 7 says a layer does not open until the one below has passed its **exit gate**.
> Those three cannot all hold. Worse, L4's exit gate as written — *a GNSS arc fit with this
> layer's forces* — needs a **populated** macromodel, which is L5 steps 2–3, so the gate that
> releases L5 cannot be reached without L5.
>
> **The principle, which is what constraint 7 exists to protect: a layer's exit gate must be
> satisfiable with what that layer and the layers below it have.** A gate that needs the layer
> above is not a gate.
>
> **RESOLVED 2026-09-18**, proposed by the executor and adopted. The macromodel **schema** moves
> to L4 as a step of its own; L5 keeps the **population**; the arc fit that needs a populated
> library becomes L5's exit gate, and L4's asks for each force's own published case plus an arc
> fit whose three parameters are **stated in the test**, not read from the library — which is
> what stops the circularity returning in smaller form.
>
> *The refinement is the executor's and it corrects the manager's own argument.* Moving the
> schema was justified by L3 step 1's *freeze the contract before its consumers multiply* — but
> that argument has a second half the move would have dropped: the force surface was frozen
> against a **trivial** force, deliberately, so that it was not shaped by its first client. A
> schema landing in L4 and immediately consumed by box-wing is shaped by its first client, which
> is the thing the argument exists to prevent. So the schema step comes **before** `srp-analytic`
> and its gate is a **cannonball round-trip**. Note what that does and does not mean: the schema
> is *designed* for the general case — N surfaces, each with a normal and optical coefficients,
> mass and centre of mass — and *gated* on the degenerate one, because a cannonball round-trip
> proves the schema does not **require** what a cannonball has not got. Freezing a schema that
> only a cannonball fits would be the opposite error and is not what this says.

1. **DONE** — `shadow` → [`subplan_L4/L4-1.md`](subplan_L4/L4-1.md)
2. **DONE** — **Macromodel schema** → [`subplan_L4/L4-2.md`](subplan_L4/L4-2.md)
3. **DONE** — `srp-analytic` → [`subplan_L4/L4-3.md`](subplan_L4/L4-3.md)

4. **DONE** — `drag` → [`subplan_L4/L4-4.md`](subplan_L4/L4-4.md)
5. **DONE** — `srp` and `erp` as force plugins over one photon-pressure kernel → [`subplan_L4/L4-5.md`](subplan_L4/L4-5.md)
6. **DONE** — `thrust-yaw` → [`subplan_L4/L4-6.md`](subplan_L4/L4-6.md)
7. **DONE** — `ecom` → [`subplan_L4/L4-7.md`](subplan_L4/L4-7.md)

**Exit gate:** every force reproduces **its own published test case** — the oracle ranks last
(§4 rule 2) — or, where the source prints none and the search that established the absence is
recorded (§4 rule 4), the independent properties its specification names — **and the layer's
ranking table**: every force of this layer measured in one place, through `DYN-R-023`'s registry,
beside L2's own terms computed at the same points in the same test, at reference points each row
states (§4 rule 3), so the layers above inherit what is modelled and
at what size rather than reconstructing it. **Amended 2026-09-24:** the arc fit with parameters
stated in the test, which this gate carried, moves to L7's exit gate. *The ranking table is added
the same day:* the L4 handover of 2026-09-18 called it "the one thing L4 owes every layer above
it", but it never entered this plan, so no step and no gate carried it through seven steps and none
was built — an obligation stated only where a plan's reader would not look, by the manager.

> **Correction found at L4 step 7: the 2026-09-18 resolution did not close.** It moved the arc
> fit's *force* parameters into the test so that no library is read — but an arc fit also
> estimates the initial state from the observations, and nothing in L0–L4 estimates anything: the
> estimator is L7's and the SP3 reader L6's, both later in the one execution order. The frozen
> `G-*` cases record each fit's residual RMS, not a fitted state a test could state instead. So
> L4's arc fit, and L5's, still needed a layer above them — the defect the principle exists to
> stop, removed in its large form and left in a smaller one. Both arc fits move to L7's exit gate,
> where the estimator, the reader, this layer's forces and L5's library all exist, and L4 and L5
> exit on what they can reach. Found by the executor's rule-4 search for step 7, whose own gate had
> the same shape; the resolution it corrects was the manager's.

**Exit gate — PASSED 2026-09-24.** `tools/ci.sh` exits 0: 13 gates, 326 tests, 687 artefacts
byte-identical — verified by the manager on `05f5377`, with `cc157d1` changing comments only.
Every force's evidence is audited in the L4 report (§10). The published cases this layer
reproduces are Kouba's printed turn thresholds; every other force rests on properties its
specification names, each absence of a published case recorded with its search; and the strongest
external evidence — CODE's published attitude and Dilssner's digitised Figure 8 — is reproducible,
not gated. The ranking table (`tests/l4_ranking.cpp`) measures every L4 force through the registry
beside L2's terms at three stated points — GPS, LEO drag at two radii, LightSail-2 — and its one
pre-registered expectation missed by more than 10× stays as written. Carried: the true IIIA law;
Galileo, GLONASS and BeiDou attitude; II/IIA's post-shadow recovery as an integrator event; and
L2's models as plugins, now L7 step 1.

---

### 3.6 L5 `spacecraft` — 0 of 4 done; **the open layer**

The macromodel library as **data with per-value citations**, not as code.

**Entry:** L4 exit gate — **amended 2026-09-18**; it read *L4 step 2*, which conflicts with
§5 constraint 7 and with L4's own exit gate. See the note in §3.5. The argument the old entry
carried is sound and survives in the other direction: one consumer must exist to shape the
schema, not all of them, so the schema goes to L4 and the population stays here.

1. **TODO** — GPS → [`subplan_L5/L5-1.md`](subplan_L5/L5-1.md)
2. **TODO** — Galileo → [`subplan_L5/L5-2.md`](subplan_L5/L5-2.md)
3. **TODO** — GLONASS → [`subplan_L5/L5-3.md`](subplan_L5/L5-3.md)
4. **TODO** — Altimetry → [`subplan_L5/L5-4.md`](subplan_L5/L5-4.md)

**The sharpest edge in the plan.** The predecessor's own surface models under `res/` and
`analysis/` — a hand-built hundred-plus-surface GPS-IIR model with material properties — are
**not** taken. A compilation of that size can attract a database right under UK and EU law even
where copyright in it is thin. Where a public source is coarser than the predecessor's model,
the answer is a model derived from published dimensions and imagery, which is this tree's own;
not a transcription, which is not.

**Exit gate:** every value resolves to a citation, and the library refuses to build if any does
not. **Amended 2026-09-24:** the arc fit that reads the library, which this gate carried, needs
L7's estimator and moves to L7's exit gate (§3.5's correction).

---

### 3.7 L6 `io-measurements` — 0 of 4 done

Formats in, measurements out. Public specifications throughout — nothing here is anyone's
intellectual property but the format authors'.

**Entry:** L1 exit gate.

1. **TODO** — Formats → [`subplan_L6/L6-1.md`](subplan_L6/L6-1.md)
2. **TODO** — Horizons client → [`subplan_L6/L6-2.md`](subplan_L6/L6-2.md)
3. **TODO** — `sgp4` → [`subplan_L6/L6-3.md`](subplan_L6/L6-3.md)
4. **TODO** — `measmod` → [`subplan_L6/L6-4.md`](subplan_L6/L6-4.md)

**Exit gate:** every format round-trips, and a measurement model returns residual and partials
for all three observation types.

---

### 3.8 L7 `estimation` — 0 of 5 done

Two of the predecessor's defects are design requirements here rather than lessons learned.

**Entry:** L3 and L6 exit gates.

> **Carried from L4 step 4: step control across NRLMSISE-00's seven altitude cutoffs.** The
> density is discontinuous at N₂ 160, He 200, Ar 240, O₂ 250, O 300, H 320 and N 450 km (§3.3
> step 4's correction), so drag is too, and one fixed 60 s step spanning a cutoff moves the
> position by ½·Δa·Δt². For L4's compact test spacecraft (*C*_D·*A*/*m* = 0.044 m²/kg) that clears
> a 10⁻¹² relative tolerance on 6.7 × 10⁶ m at **three** cutoffs, up to 100× at N₂. The effect is
> linear in the ballistic coefficient, and **the object this tree exists for is not compact**: for
> LightSail-2 (32 m², 4.93 kg) it is 84× larger with the effective area this project fitted and
> 325× face-on — **six or seven of the seven** cutoffs clear the tolerance, N₂ by 8 000 to 32 000×.
> So event location (or an equivalent) is central here, not a corner case. It is decided at L7,
> and it may need an integrator amendment in L3, made on `DYN-Q-001`'s terms.

1. **TODO** — The whole force model through the registry → [`subplan_L7/L7-1.md`](subplan_L7/L7-1.md) — *added 2026-09-24*
2. **TODO** — Batch least squares with normal equations **scaled by default** → [`subplan_L7/L7-2.md`](subplan_L7/L7-2.md)
3. **TODO** — Levenberg–Marquardt → [`subplan_L7/L7-3.md`](subplan_L7/L7-3.md)
4. **TODO** — A priori constraints → [`subplan_L7/L7-4.md`](subplan_L7/L7-4.md)
5. **TODO** — Joint covariance over the state and every parameter registered in L3 → [`subplan_L7/L7-5.md`](subplan_L7/L7-5.md)

**Exit gate:** a fit over real data reaches its frozen residual and reports a joint covariance
whose correlations are reproduced by finite differences. **Amended 2026-09-24**, taking the two
arc fits L4 and L5 could not reach (§3.5's correction): cannonball fits with L4's forces reach
the `G-*` frozen residuals with their force parameters stated in the test, and a box-wing fit
reading the L5 library reaches them too — the stronger claim, since the frozen fits are
cannonball fits (`oracle/ORACLE.md` §6).

---

### 3.9 L8 `campaigns` — 0 of 5 done — **the MVP gate**

The validation campaigns as executable definitions, run by CI. The campaign *logic* of the
revival's `validate_*.sh` scripts carries over — already this tree's own work — re-pointed at
these binaries.

**Entry:** L7 exit gate, and L5.

1. **TODO** — GNSS against IGS precise orbits → [`subplan_L8/L8-1.md`](subplan_L8/L8-1.md)
2. **TODO** — Laser ranging → [`subplan_L8/L8-2.md`](subplan_L8/L8-2.md)
3. **TODO** — Optical angles → [`subplan_L8/L8-3.md`](subplan_L8/L8-3.md)
4. **TODO** — Frame regression → [`subplan_L8/L8-4.md`](subplan_L8/L8-4.md)
5. **TODO** — Atmosphere → [`subplan_L8/L8-5.md`](subplan_L8/L8-5.md)

**Exit gate — the MVP.** Every campaign meets its frozen number, each statistic **naming the
formula it was computed with**. Two unstated denominators produced defensible-looking wrong
numbers within two days of each other; §4 rule 3 exists because of it.

---

### 3.10 L9 `raytracer` — 1 of 4 done — *optional, the owner's call after L8*

The pixel-array method and thermal re-radiation. Strategically the most valuable layer and the
only one off the critical path.

**Entry:** L8 exit gate, plus an explicit decision to start.

1. **DONE** — Patent gate → [`subplan_L9/L9-1.md`](subplan_L9/L9-1.md)
2. **TODO** — Ray tracer → [`subplan_L9/L9-2.md`](subplan_L9/L9-2.md)
3. **TODO** — Thermal re-radiation → [`subplan_L9/L9-3.md`](subplan_L9/L9-3.md)
4. **TODO** — Grid generation → [`subplan_L9/L9-4.md`](subplan_L9/L9-4.md)

**Exit gate:** grids regenerated from this tree alone, reproducing the published results of the
sources above, with L4's analytic models as the coarse cross-check.

---

### 3.11 How a step is executed

The eight points below are not a second sequence — they are what doing any one step of §3.1–
§3.10 consists of, and they are where the plan's former ground rules now live. A point with no
content for a step ("Data: —") is stated, not skipped, so absence is visible.

**Points 1 and 2 do not apply to L0 or L8.** Those layers are tooling and harness, not science:
there is no published source to specify from, so there is nothing for a spec to declare and
nothing for Review to check against. Their steps begin at point 3. Every other layer runs all
eight, and §1's last column says which is which.

1. **Spec** — write `spec/SPEC-<step>.md` from *only* the sources that step names, to
   `SPEC-template.md`: derivation declaration, an obtained-`primary/secondary/not` column on
   every source, open questions raised to the manager rather than resolved into guesses.
2. **Review** — the manager adopts or returns. Adoption criteria: sources primary or flagged;
   conforms to §2; every requirement discharged by a test or individually excused; no porting
   of the predecessor's revival-era extensions — those algorithms are re-derived from the same
   public sources the revival itself cites.
3. **Data** — manifest entries (URL, SHA-256, licence note), fetched from origin. Baselines and
   acceptance tests pin **IERS-archived** series by path and hash: the IERS revises EOP
   retroactively at unchanged URLs (three documented instances: 2025-06-05, 2026-02-05,
   2026-03-09) and archives the superseded series at stable paths, so pinning records a URL and
   redistributes nothing. Operational runs may use live data but record identity and hash in
   run provenance; a pinned-hash mismatch is a hard failure; re-baselining is deliberate and
   logged, never a side effect of a re-fetch.
4. **Dependencies** — permissive licences only (BSD/MIT/Apache-class), recorded at adoption
   with any multi-licence choice named; no GPL/LGPL/AGPL in anything that could ship.
   Unlicensed normative code (the IERS Conventions Fortran) is never vendored or translated:
   implement from the tables printed in the Conventions and verify against the routines'
   published test cases — using a published expected output is observation, not derivation
   from code. **A dependency's own build system counts as a fetcher.** `tl::expected`'s
   `CMakeLists.txt` declares Catch2 v2.13.10 by URL with no hash, and CMake's `FetchContent`
   honours the *first* declaration it sees — so a build printed that it was using the pinned
   3.16.0 from cache while fetching an unpinned v2 over the network. The pin was a fiction and
   was visible only by accident (v2 puts its CMake helpers in `contrib/`, v3 in `extras/`; a
   substitution *within* a major version would have built cleanly). Every dependency is checked
   for what its own build declares, and the populated tree is verified against the hash-pinned
   archive at configure time, not trusted. **Read it for what it gets wrong, too.** Three
   dependencies in a row carried a build-system surprise: ERFA ships no CMake at all,
   `tl::expected` captured our Catch2 as above, and CALCEPH's `src/CMakeLists.txt` declares
   `target_include_directories(calceph PUBLIC $<BUILD_INTERFACE:>)` — empty — so its generated
   config header is unreachable as a subproject and every translation unit fails. Repaired from
   our side with the pinned bytes untouched, which is the only acceptable shape: patch the
   consumer, never the hashed archive.
5. **Implement** — in the §2 module, to the adopted spec, under the §5 constraints. Every
   refusal ships with exactly one named, logged escape hatch, set explicitly per run and
   recorded in run provenance with the relevant table's hash. The default refuses, and the
   escape cannot arrive through a configuration fall-through.
6. **Test** — the spec's acceptance suite gated on *published* values; then the oracle rows,
   each comparison logged (date, quantity, oracle value, new value, verdict) and **never the
   sole gate** — §4 says why some oracle rows must *disagree*.
7. **Ledger** — module and parameter register rows in `PROVENANCE.md`, plus the oracle-log rows.
8. **Gate** — the single criterion that closes the step, with numbers frozen in
   `oracle/cases.tsv` or in a named publication and statistic definitions named.

---

## 4. Gates — how to read the frozen numbers

**The authority for every predecessor-derived number is `oracle/cases.tsv`** — 27 cases, every
input hashed. **`capture.sh` regenerates 16 of them**; the eleven `B-*` and `E-*` rows come from
recorded sweeps too expensive to re-run and are cited by source hash rather than reproduced, as
`oracle/ORACLE.md` §6 and §7 set out. Verified on 2026-09-18 after the tree moved: all sixteen
reproduce byte-identically. The plan quotes them for readability; the file
governs. Three rules apply to all of them:

1. **Parity means matching public truth, not the predecessor bit-for-bit** — but *where* a
   disagreement is required is narrower than this rule first claimed, and the correction is
   measured. The predecessor computes IAU-76/1980 and this tree IAU 2006/2000A, and the
   original reading was that every predecessor-derived number must therefore differ by about
   0.064″ ≈ 2.2 m at 7000 km. **On the ITRF↔GCRS path that is false.** Measured 2026-09-18:
   the two agree to **1.559 mm at |r| = 7716.93 km, 4.17 × 10⁻⁵ arcsec** — 1500× smaller than
   this rule predicted.

   The mechanism is the part worth keeping. Each chain applies the **celestial-pole offset
   series matched to its own model** — the predecessor adds dΨ/dε to an IAU-1980 nutation, this
   tree adds dX/dY to the IAU-2006/2000A CIP — and those series exist precisely to bring each
   model onto the *observed* pole. Two different algorithms, each corrected onto the same
   physical pole, must agree; the model difference cancels by construction. It was never going
   to appear here.

   **It does appear on the TEME path**, because TEME is referred to the mean equinox of date,
   which is a model construct with no correction series to reconcile two models. Oracle T-01's
   2.2 m at 7234 km is 0.0627″, and the accumulated IAU-76 vs IAU-2006 precession difference of
   0.064″ is 2.24 m there — essentially all of it. Vallado's kinematic equation-of-equinoxes
   terms (0.00264″ sin Ω + 0.000063″ sin 2Ω) are real and must be carried, but they are ≈ 95 mm
   at that radius, **4% of T-01, not its explanation.**

   So the required-disagreement gate belongs at **T-01, in L6**, where the TLE and the Horizons
   table live — asserting size *and* direction and failing on agreement as well as on excess
   (FRAME-A-009's pattern), at the ≈ 2.2 m the precession difference predicts. L1 step 4's gate
   is what the oracle actually supports on its own path: magnitudes agree, separation bounded,
   round trip better than the predecessor's closure.

2. **An oracle comparison is never a gate on its own** — it ranks last among acceptance-value
   sources per `SPEC-template.md`. Published worked examples and published test cases are the
   gates; the oracle catches gross error — a sign, an axis, a factor of two.
3. **A statistic cited as an acceptance value carries the formula it was computed with.** Two
   unstated denominators produced defensible-looking wrong numbers within two days (identifier
   counting, 128 vs 121; block separation, 20.8σ vs 14.1σ). L8's block gate names
   its definition for exactly this reason.

   **Gated and reproducible are different properties, and a recorded number needs the second
   even when it does not warrant the first.** A characterisation of a deferred feature — L4 step
   1's measurement of where the penumbral cancellation breaks — is rightly kept out of CI:
   carrying a two-body propagator in the suite to assert the magnitude of something not built is
   over-building. But the numbers it produced went into the specification with nothing in the
   tree able to regenerate them, which is `oracle/capture.sh`'s defect after the merge — frozen
   figures with no route back to what made them. **Excused from the gate is not excused from
   reproducibility**: the tool that produced a recorded number is committed under `tools/`, run by
   hand, and named next to the number. **And a regenerator is authoritative only once it has converged.** A
   regenerator disagreeing with a recorded number says that one of them is wrong, not which: the
   first time this rule fired, the record was right and the unconverged regenerator was not, and
   treating the tool as ground truth "corrected" a correct figure. So a regenerated figure that is
   a residual, a ratio to a residual, or anything else resolution-sensitive carries its
   convergence study, and a disagreement with the record is a question to resolve, not a verdict.

   **A correction re-derives the whole statement; it does not patch one term of it.** Twice a
   review here has fixed one part of a claim and carried the rest of it across unchecked: the
   manager recorded "chapter 6 prints no worked examples" into this plan while writing the rule
   against exactly that, and later corrected *which* coefficient binds a clamp margin while
   repeating the executor's "four decades", which is 2.94 for the value quoted and 2.73 for the
   one that binds. Patching inherits everything not patched, and the unpatched part arrives
   carrying the authority of the correction. So a corrected number is recomputed from its inputs,
   in full, and the recomputation is what gets written down.

   **Quantities that must be compared are measured in one place, at one reference point.** L2's
   three floors lived in three specifications — the ocean-tide truncation floor in
   `SPEC-perturbations`, the relativistic terms in the same document at a different radius, the
   unapplied *L*_B scaling in `SPEC-ephemerides` — so the comparison between them had to be
   **reconstructed** by whoever needed it, and both executor and manager reconstructed it wrong
   in opposite directions before it became contradictory enough to force a measurement. Measured
   together at 7331 km by `tests/l2_floors.cpp`, which links three modules on purpose: the floor
   is 8.552 × 10⁻¹¹ m s⁻², the smallest term the layer actually computes is de Sitter at
   3.478 × 10⁻¹¹, and the *L*_B scaling is 5.070 × 10⁻¹⁴ — **the floor is 2.46× the smallest
   term kept**, not three orders above it and not twelve times it. This binds L4 hardest, where
   a dozen accelerations have to be ranked against each other to decide what is modelled.

   **And the denominator of a point-value gate is its points.** A model checked against its
   reference at 125 cases is checked at 125 cases and nowhere between them; a discontinuity lying
   between two of them is invisible to the gate however tight its tolerance. Where the source is
   piecewise, its boundaries are part of the model: they are found by searching the source, and
   each is tested on **both sides** against the reference — NRLMSISE-00's seven `ALTL` cutoffs
   are the case, found at L4 step 4 after L2's gate had passed at one ulp.

   **The same rule binds a test's own case count.** A test driven by data — rows of a file,
   segments of a kernel, constituents of a table — MUST assert how many cases it ran before it
   trusts that they passed, and print the number. `EPH-A-007` is why: it compared two TT−TDB
   routes over a year, the short kernel carried no such record, its loop ran **zero times**, and
   it warned and passed inside a gate that was accepted. A count of passed cases with no
   denominator hid 868 ephemeris cases at step 1 and an entire test at the same step; it is one
   failure, in a statistic and then in a suite. Audited across all 165 tests on 2026-09-18:
   `EPH-A-007` was the only instance — every other loop is over a compile-time array, or is
   already guarded the way `EOP-A-014` guards its 19 000 rows.
5. **A guard is proven by making it fire, in the place it will have to fire from.** The
   stale-configure check was put in `tools/ci.sh` between gate 3 and gate 5 — where CI configures
   before it tests, so the build system is always newer than the `CMakeLists.txt` and the
   condition can never be true. It passed its own first run, which is how dead code that reads
   like protection survives. It belongs in the **suite**, because the failure it guards against
   is running `ctest` or one binary against an old build directory, and that happens outside CI
   by definition. Every guard in this tree is demonstrated **both ways** before it counts:
   passing when it should, and failing when the thing it guards against is injected.

   **And a guard that enumerates what is permitted beats one that searches for what is
   forbidden.** Three times now: the licence **denylist** passed CeCILL because it carried no
   GPL string, and the allowlist catches it by construction; `test_one_secular_pole` searched for
   constants, was narrowed twice, and was fixed only by replacing the discriminator; and L3's
   crossing gate, worded in §3.4 as a search for km↔m scalings, was **measured before being
   built** — 15 literals of the 1000 family in 62 production sources, of which **14 are not
   crossings and the one that is, is the definition the gate must permit.** Fourteen false
   positives to one true positive, today, before L4 adds a dozen forces each with its own
   constants. The repair is a **register**: every such literal either lives in `core/units.hpp`
   or carries an annotation naming what it converts; the gate prints the whole register with its
   denominator and fails on a literal that is neither. A search converges on flagging nothing; a
   register converges on accounting for everything, and it makes the permitted set **legible as a
   set**, which is what lets a reader see that nothing is hiding in it.

   **And an estimate that is exactly zero is safer than one that is rounding noise.** RKF7(8)'s
   error estimate cancels identically on a quadrature problem — α₀ = α₁₁ and α₁₀ = α₁₂, and the
   *y* argument is ignored, so the four evaluations are pairwise bit-identical. Written the way
   `TR R-287` writes it, `(f0 + f10) - f11 - f12` **rounds before it subtracts** and returns
   ≈ 1.1 × 10⁻¹⁶ instead of 0. An exactly-zero estimate is honestly blind and the controller is
   visibly degenerate; noise *wearing the shape of an estimate* is responded to, looks plausible,
   and announces nothing. **Group so that a degenerate quantity comes out degenerate** — which is
   also better conditioned near the degeneracy, where the naive ordering loses most.

   **And an agreement far better than the computation's own noise floor is a symptom, not a
   success.** Twice now: `PERT-A-001`'s *worst residual zero*, which is what a transcription check
   looks like when the expected value is also the input; and L4 step 1's resolution study
   reporting agreement to 10⁻¹⁴ across four grids because it sampled deep in the umbra, where
   both models return 0 — a converged agreement between two things that were not being compared,
   found in the check written to prevent exactly that fault, and caught only because someone
   looked at the magnitude and did not believe it. **A result too good for the method that
   produced it is evidence that the method did not run.** Asserting what was compared is the fix;
   disbelieving a good number is how you find out you need to.
7. **A threshold is not chosen by whoever will be judged by it.** Three ways to satisfy that,
   in increasing strength, and this project has now used all three.

   *Weakest — state the criterion before measuring, and let the measurement set the value.* L2
   step 4's ocean-tide truncation degree: the criterion was written into the specification first
   and the degree came out of the measurement, because a number written first acquires authority
   whatever the changelog says about how it got there.

   *Stronger — pre-register the expected result with its reason.* L3 step 2's comparison against
   Fehlberg's 1968 Table XI: the expected order of magnitude, the sign, and **whether the
   leading digit should match**, all written down with the sizing argument before the first run.
   A tolerance chosen from its own result tests nothing, and a prediction that includes what
   should *fail* cannot be read backwards from the outcome.

   *Strongest — make the threshold relative to a comparator measured in the same run, from a
   family that cannot be affected by what is under test.* L3 step 2's controller-insensitivity
   test is the case, and it arrived by the absolute form failing. The manager set an absolute
   floor of 2 × 10⁻⁷ m, taken from a **single fixed-step** run's accumulated arithmetic; the test
   failed at 5.65 × 10⁻⁷, because the separation of **two independent** round-off walks is not
   the size of one. Adjusting the floor upward at that point would have been the exact defect the
   instruction *stop and report rather than tighten* existed to prevent. What the executor did
   instead was find a comparator immune to the effect under test — runs differing only in where
   the step sequence starts, all four constants held fixed — and it turned out that **changing
   only the initial step moves the answer more (7.02 × 10⁻⁷) than changing every constant does
   (5.65 × 10⁻⁷)**, at the same accepted-step count. The criterion is now that relation, and
   there is no absolute number in it that anyone could have fitted afterwards.

   *The strongest form has one loophole and it must be closed explicitly.* The number cannot be
   fitted, but **the comparator family can** — and widening it weakens the test, which is the
   unobvious direction. L3 step 3 is the case: with the finite-difference perturbation taken out
   to *h* = 10⁻³ the perturbation is 7 km, its own truncation inflates the band to 1.6 × 10⁻⁵,
   and `worst ≤ band` passes on anything. Narrowed to a decade around the **predicted** optimum
   the band is 1.96 × 10⁻⁶ against a best agreement of 1.98 × 10⁻⁸. So the family is chosen from
   a prediction made before the run, by the middle form above, and a threshold is not fitted at
   one level by being made relative at another.

   *And a convergence ratio separates a discretisation error from a wrong model, which a single
   tolerance cannot.* Where a discretised computation is gated against a closed form, assert the
   error's **ratio** under refinement as well as its size: discretisation error shrinks at the
   method's order, and an error in the law being discretised does not shrink at all. L4 step 3's
   tessellated sphere is the case — an injected law error left the ratio at 1.00 where the
   prediction was 4 — and L4 step 5's Earth radiation pressure, an integral over the visible
   Earth, has the same structure.

8. **A reference implementation is an oracle when a specification exists that it implements, and
   is itself normative when none does — and the difference is a search, not a preference.** Rule
   2 ranks an oracle last because an independent description of the same computation exists to
   check it against: ERFA is an oracle precisely because the Conventions define what it computes.
   NRLMSISE-00 is not. Its ~1500 fitted coefficients and the code combining them exist **only**
   in the FORTRAN; the paper describes how the fit was made and does not define the function. So
   the reference *is* the model, exactly as EGM2008's coefficient file *is* the field, and its
   output on its own published inputs is a category-1 acceptance value rather than a
   last-ranked oracle comparison.

   Three obligations come with claiming this, and a gate that claims it without them is claiming
   a stronger position than it holds. **The search is recorded** — the terms and the counts, per
   rule 4 — because "no independent specification exists" is the whole of the argument.
   **The cost is stated where a reader will meet it**: where the reference is normative, nothing
   in this tree can check that it computes what its paper describes, and an error in it is
   reproduced here consistently and invisibly, as it is by every other user of the model. **A
   second axis is found if one exists** — a documented relation among the outputs, a conservation
   law, a limit — because a relation the source states about its own results is checkable without
   the source's arithmetic and is not a second look at the same thing.

   **The converse is the useful half, and L3 step 2 is the case.** Where an independent
   specification *does* exist, it need not be a document — the Runge–Kutta **order conditions**
   are exact algebraic identities that any correct coefficient set satisfies, so a transcription
   is checkable against mathematics that does not involve the source at all. That changes the
   question from *can this source be read reliably* to *is what I read checkable*, and the second
   question has an answer that does not depend on the first. Fehlberg's `TR R-287` is a 1968 scan
   whose OCR renders a coefficient row as `83_ = 841 = B_I = 8sl`, and it does not matter: the
   coefficients are printed as **exact rationals**, read from the page images, and either satisfy
   the order conditions in exact rational arithmetic or do not. A source published as decimals
   satisfies them only to rounding, and a transcription error in the last digits is then
   indistinguishable from it. So when choosing among sources, **ask what independent property the
   thing must satisfy before asking how cleanly it is published.**

   This binds **L4's drag coefficient** and **L9's ray tracing**, which are the same shape, and
   it is settled here rather than three layers later for that reason.
4. **A gate's wording names what this plan wanted; the source prints what it prints — and a
   claim that it prints nothing is a claim, not an observation.** Twice the plan asked for
   something the source does not carry: rule 1 above described a disagreement on a path that
   cannot carry one, and step 2's instruction sent the executor to recursions "printed in the
   Conventions", which chapter 6 does not contain (verified 2026-09-18: zero occurrences of
   *recursion* or *recurrence* in the extracted text). **The third instance went the other way
   and is the instructive one.** Step 3's gate said *worked examples*; the executor reported
   that chapter 6 prints none, this plan recorded that as a third correction — and chapter 6
   prints one, for K₁, fifteen lines below the definition the same report had quoted, with
   *H*_f = 0.36870 among its inputs, which the same report said the Conventions never print. It
   reproduces exactly: *A*₁ δ*k*_f *H*_f = (470.9 − 30.2*i*) × 10⁻¹², and (6.8b) with η₁ = −*i*
   returns both printed lines. Neither of us had searched; we had each read the section we
   needed and stopped.

   So the rule has two halves. **Where a step's gate names a form of evidence, the first thing
   its specification does is say whether the source prints that form** — and **a finding of
   absence is recorded with the search that established it**, the terms and the count, the way
   a statistic is recorded with its formula under rule 3. **The count comes from the search, not
   from a listing of it.** At L4 step 2 the manager told the executor that an error "went into
   neither document" on a `grep … | head -6` whose seventh hit was that error, with a pattern
   that did not include the other document's wording, *a factor of 3*. A truncated listing
   establishes nothing about what it cut off; `grep -c` would have said two where one had been
   accounted for. "The source does not print X" earns
   the same scrutiny as "the source prints X = 1.333 × 10⁻⁹", because a gate is weakened as
   surely by evidence not looked for as by evidence read wrong. A gate reworded from the source
   is a correction to this plan and is recorded as one; a gate quietly satisfied by something
   else is not.

   **And a defect register is weakened by a false entry more than by a missing one, so the bar
   for entering it is higher than the bar for flagging.** L4 step 1: the executor flagged a
   paper's "36 penumbra transitions" against another table's 30; the manager, instead of asking
   whether it was established, told them to record it in `PROVENANCE.md`'s discrepancy register
   and built an inference on top of it. The sentence that resolves it is **two lines from the
   number** — two satellites, two epochs, two reference standards, all stated plainly, and the
   different populations are the paper's design rather than its error. The register's whole value
   is that a reader trusts what is in it, so a non-defect recorded there costs more than a defect
   missed. **A finding of defect is recorded with the reading that established it, and the
   adjacent sentences are part of that reading.** What goes in instead, when a flag resolves, is
   the resolution — so the next reader who notices the same two numbers does not re-open it.

## 5. Design constraints, binding every layer

*(Numbered stably — the adopted specifications cite these as "plan §5 constraint n".)*

1. **ERFA, not SOFA** — BSD; SOFA's rename clause is the reason.
2. **IAU 2006/2000A**, CIO-based; not the equinox-based older chain.
3. **No GPL/LGPL/AGPL** anywhere in what could ship — and **the licence gate's scope is what
   this tree redistributes, which is code and data, not what it reads.** Adopted 2026-09-18 at
   L4 step 1, because a commercially published paper is the first manifest entry that is neither.
   A manifest entry of kind **`literature`** is pinned by URL and SHA-256 like everything else,
   for the same reason — so that *"this was derived from that"* is checkable by a future reader
   who fetches the same hash — and is **exempt from the permissive-licence gate**, because it is
   a **provenance record, not a dependency**: nothing derived from it is a copy of it, and
   copyright does not reach the mathematics a paper describes. `dop853.f` is not the same case
   and stays dropped; it was code to be incorporated, and incorporation is what the gate exists
   for.

   **The exemption is earned by a checked property, never by the label.** Three conditions, all
   mechanical: a `literature` entry is fetched to a path no build target and no test references;
   a gate fails if any build input reads that path, **demonstrated by injecting one** (rule 5);
   and the repository holds the URL and the hash, never the bytes, so this project redistributes
   nothing. It also does **not** appear in `NOTICE` among the licences, because NOTICE's claim is
   that every licence in it is permissive and an entry with no licence would make that claim need
   an exception; it belongs in `PROVENANCE.md`'s source register, where a citation belongs.

   Where a paper's terms cannot be established, **record the search rather than the conclusion**
   (rule 4): the exemption rests on this tree not redistributing, not on a grant nobody found.
4. **Refuse rather than approximate.** Out-of-range data, an unknown timescale, unparseable
   input: a diagnostic naming the request and the limit — never a silent fallback. The
   predecessor's most expensive defect class was not a crash but a plausible wrong number.
5. **Error and diagnostic state is scoped to one run** and cannot survive into the next.
6. **The §2 layering is mandatory**; departures are argued at Review (the `eop`-as-own-module
   split is the precedent: proposed by the spec author with reasons, adopted, plan amended).
7. **One layer, one sequence** (§1). One step open at a time; a layer does not open until the
   layer below has passed its exit gate; nothing is worked on in two layers.
8. **Every signature names `odl::Result`, never the underlying type**, and **monadic chaining
   is not used** — no `and_then`, `or_else`, `transform` or `transform_error`. D7 vendors
   `tl::expected` under C++20, and construction, checking and unwrapping are where it and
   `std::expected` are interchangeable; the monadic operations are where they diverge. The
   one-line migration D7 was chosen for holds exactly as long as this constraint does.
10. **What a value means belongs in its type, never in the argument that produced it.** The
    frame is in the type, the timescale is in the type, the unit is in the accessor's name — and
    the **origin** was a runtime argument to `Ephemeris::state`, which returned
    `State<Frame::BCRS>` whatever centre was asked for, so a geocentric Moon vector came back
    labelled barycentric. A tag that can say something false about the value it labels is worse
    than no tag, because the whole of `FRAME-R-004`'s value is that it cannot. Where the tree has
    a name for the thing, the name goes in the type; where it does not, the call is refused or
    returns a type that carries no tag at all.
9. **Changing a manifest entry re-runs every gate that consumed it**, in the step that changes
   it, and the frozen numbers are restated from the re-run rather than carried forward. A
   pinned input is pinned because the numbers depend on it; swapping one and keeping the old
   figures would leave a frozen number whose source no longer exists. This binds a substitution
   as much as an upgrade — `de440.bsp` → `de440t.bsp` at L2 step 3 re-runs step 1's full
   `testpo.440` sweep, and reproducing 1.06 mm on the new kernel is itself worth having.

## 6. What carries over, what is dropped

Carried over as-is (already owned; never linked against predecessor code): the revival-era
Python tooling — `horizons2eci.py` (with its TDB refusal), `crd2obs.py`, `seesat2angles.py`,
`cpf2eci.py`, `angles_iod.py`, `angles_admissible.py`, `build_eop.py`, `update_iersb.sh`,
`update_spaceweather.sh`, `degrade_eci.py` — and the *campaign logic* of the `validate_*.sh`
scripts, re-pointed at this tree's binaries (L8). `jpl2fecsoft.py` retires with the
FECsoft container. Dropped, with reasons: FECsoft astrolib (unlicensed → ERFA + CALCEPH);
Brodowski NRLMSISE C port (unlicensed → D2's public-domain FORTRAN port); vendored SGP4 (terms
unstated → D4's own port on published vectors); png++ (no consumer); TIE-GCM tables
(non-redistributable model output; NRLMSISE covers the need).

## 7. Decisions

| # | Decision | State |
|---|---|---|
| D1 | **Core language** | **Decided 2026-09-18: C++20**, by the owner. Two consequences are recorded rather than quietly dropped. (a) The Rust recommendation rested partly on *structural distance* from the GNU-C++14 predecessor; C++20 does not supply that for free, so §2's "two designs, not one design twice" is now carried entirely by the architecture and is checked harder at Review — same language, same problem, so only the decomposition distinguishes them. (b) `cargo license` is unavailable, so the NOTICE generation of L0 step 5 needs a C++ equivalent driven from the manifest of L0 step 3. Specs remain language-free. |
| D2 | NRLMSISE-00 route | Decided in plan: own port from the NRL public-domain FORTRAN (L2 step 4). **"Validated on its packaged tests" was wrong — there are no packaged tests**, only 17 input cases with no expected output; established by search 2026-09-18 and corrected in §3.3 step 4. Validation is against the reference's own output on those cases, frozen with its toolchain, per §4 rule 8. |
| D3 | Ray tracer | Decided in plan: own implementation from the papers; `photonsXforce` as cross-oracle only (L9). Owner revisits after L8. |
| D4 | SGP4 | Decided in plan: own port from STR#3 + Vallado 2006 on the published vectors (L6 step 3). |
| D7 | `Result<T,E>` under C++20 | **Decided 2026-09-18: stay C++20, vendor `tl::expected`** (CC0-1.0, header-only) behind a tree-local `odl::Result`. Chosen on reversibility: C++20 → C++23 later is two CMake lines and deleting the shim, while C++23 → C++20 means hunting every C++23 feature that crept in over months. The migration is near-free *here specifically* because the specs use `Result` only as a plain return type — no monadic chaining anywhere — which §5 constraint 8 now keeps true. Exceptions were excluded: a refusal that unwinds is not a diagnostic the caller must consume, which is the whole of constraint 4. |
| D5 | Licence | **Done** 2026-09-18: no grant, as the owner chose — implemented as posture, not text; `LICENSE` says why the predecessor's reason must not be copied. |
| D6 | Name | **Done** 2026-09-18: `odl/self_built`, directory `odl-self_built`. Overrides the earlier no-echo naming guidance deliberately; recorded in `LICENSE` §4. |
| — | **Copyright holder name** | **OPEN, owner-only, thirty seconds:** `LICENSE` line 4 is a marked placeholder — the single open title item, and the first thing a counterparty reads. |

## 8. The ledger

Live at `PROVENANCE.md` since commit `bdd80be` (this section held its seed template, consumed
there): module, parameter, oracle-log, dependency, checks and disclosed-context-exposure
registers.

## 9. Traceability

To the ownership analysis (`doc/ownership-analysis.md`), which `chat.md` is the source of, and
to the first draft's feature numbers:

| ownership-analysis row | layer | step | was |
|---|---|---|---|
| Integrator / EOM / variational | **L3** `dynamics` | §3.4 all | F1 |
| Gravity, tides, relativity, third-body | **L2** `environment` | §3.3 steps 1–3 | F2 |
| Frames, precession–nutation, EOP | **L1** `time-frames` | §3.2 all | F3 |
| Atmospheric drag | **L2** / **L4** | §3.3 step 4 (model), §3.5 step 3 (force) | F4 |
| Albedo/IR | **L4** `forces-analytic` | §3.5 step 4 | F5 |
| Antenna thrust, yaw laws | **L4** | §3.5 step 5 | F6 |
| SRP models; ECOM | **L4** | §3.5 steps 2 and 6 | F7 |
| Shadow function | **L4** | §3.5 step 1 | F8 |
| SRP ray-tracing / pixel array; TRR; grid concept | **L9** `raytracer` | §3.10 all | F9 |
| OD / least squares; measurement models | **L7** `estimation`, **L6** | §3.8 all, §3.7 step 4 | F10, F11 |
| File I/O | **L6** `io-measurements` | §3.7 steps 1–2 | F12 |
| GNSS / altimetry spacecraft data | **L5** `spacecraft` | §3.6 all | F13 |
| SGP4/TLE | **L6** | §3.7 step 3 | F14 |
| Build/CI/tooling | **L0** `foundation` | §3.1 all | F15 |
| Validation / provenance practice | **L8** `campaigns`, §8 | §3.9 all | F16 |
| "Cannot take" items 1–5 | §0 discipline; **L5** and **L9** notes; §6 | §3.6, §3.10 | — |
| Patent check | **L9** | §3.10 step 1 | R9 |

Read across: the ownership analysis has one row per body of published literature, and the
layers are that list grouped by what depends on what. Nothing in the analysis is unclaimed by a
layer, and no layer claims anything the analysis did not place.

## Handling of this document

Committed in the public `odl20Lite` repository, which is where the owner put it. Earlier drafts
kept this plan in a separate, local-only tree and off any public remote, because the predecessor's
`NOTICE` says UCL's rights are unresolved and a plan for reimplementing around them seemed no
business of a public page. On 2026-09-18 the owner consolidated everything into one repository,
public, and the plan has been published with it since. That is the owner's decision; this section
records it rather than re-arguing it, and was stale until 2026-09-23.

---

## Appendix — rule identifiers cited by the specifications

The adopted specs and the ledger cite "plan rule Rn" / "plan Rn" from earlier drafts, whose
rules table an earlier draft dissolved into §3.11. The identifiers stay stable and
resolve as — note that §3's subsection numbers now address layers, not the first draft's
twenty features:

| id | now lives at |
|---|---|
| R1 spec-first | §3.11 point 1 |
| R2 run-never-read | §0 discipline — **discharged**: the oracle is frozen (27 cases); the leak-channel corollary is §0's closing sentences |
| R3 different design | §2, checked at §3.11 point 2 |
| R4 oracle logged, never the sole gate | §3.11 point 6 and §4 rule 2 |
| R5 provenance ledger | §3.11 point 7 and §8 |
| R6 no file crosses over | §0 discipline and §3.11 point 3 |
| R7 permissive dependencies, choices recorded | §3.11 point 4 |
| R8 licence-first | L0 step 1 (§3.1) — **discharged** at `bdd80be` |
| R9 patent gate | L9 step 1 (§3.10) — **discharged, clear** |
| R10 revival-era work re-derived, not ported | §3.11 point 2 and L6 step 4 (§3.7) |
| R11 mutable public data pinned | §3.11 point 3 (the policy, verbatim) |
| R12 one named, logged escape hatch per refusal | §3.11 point 5 (the pattern: `time`'s leap-table expiry, L1 step 1) |
