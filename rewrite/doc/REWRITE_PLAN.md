# Rewrite plan — a fully-owned reimplementation of the validated ODL pipeline

**Status:** foundations specified and adopted (`time`, `eop`, `frames` at v1.2); repository
live at `bdd80be`; oracle frozen (27 cases). **D1 decided 2026-09-18: C++20** (§7) — implementation is unblocked.
**Canonical:** `doc/REWRITE_PLAN.md` within this tree — this file, and there is no other
copy or companion. It is the only plan document; the ground rules that earlier drafts held as a
separate table are merged into the layer sequences of §3, where they are performed rather than
recited. Rule identifiers R1–R12, cited from the specifications, are indexed in the Appendix and
resolve into those steps.
**Basis:** `doc/ownership-analysis.md` (the layer-by-layer ownership analysis) and `oracle/`
(what the predecessor measurably does, frozen). The predecessor's own documents are history, not
inputs: nobody working from this plan reads that tree — see §0.
**Date:** redrafted 2026-09-18; supersedes the draft of the same date.

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

1. **DONE** — Repository created with the licence in the first commit (`bdd80be`), together
   with this plan, the provenance skeleton and the P1 specifications. The licence-first
   obligation is discharged and stays discharged.
2. **DONE** — Oracle frozen: 27 cases in `oracle/cases.tsv`, every input hashed. `capture.sh`
   regenerates the 16 reproducible ones; the `B-*` and `E-*` rows are transcribed from recorded
   sweeps and cited by hash (§4). The path broke when the tree moved and was repaired and
   re-verified on 2026-09-18 — all sixteen reproduce byte-identically.
3. **DONE** — Manifest format and fetcher: every external input declared with URL, SHA-256 and
   a licence note, fetched from origin into a cache, nothing entering the tree undeclared. The
   C++20 toolchain was fixed here because every later step consumes it: **CMake + Ninja**;
   **Catch2 3.16.0** (BSL-1.0), chosen because `WithinAbs`/`WithinRel`/`WithinULP` is the
   vocabulary the specs already state tolerances in and "bit-comparable" is `WithinULP(0)`;
   acquisition through the manifest with CMake re-checking `URL_HASH`. vcpkg and Conan were both
   rejected on the same ground — each can be made deterministic, but in both the archive hash is
   something a registry holds rather than something this tree writes down, and both need
   bootstrapping, which is a tool acquired outside the manifest in order to enforce the manifest.
4. **DONE** — NOTICE generation from the manifest. No tool was needed: NOTICE is a pure function
   of the manifest, and licence text is quoted verbatim out of the hash-pinned archive rather
   than paraphrased. `cargo license`'s absence under D1 cost nothing.
5. **DONE** — Spec-coverage checker, denominator pinned to OWN-PREFIX identifiers. Its own first
   run was wrong — the region ran to end-of-file and swallowed §10, inflating the excused count —
   and that was visible only because the checker prints its components rather than a verdict.
   It prints them for that reason.
6. **DONE** — Reproducible-build flags. The `-ffile-prefix-map` options were initially ordered
   wrongly (GCC applies them last-specified-first), leaking the build directory name into
   `DW_AT_comp_dir` in every object file; the reproducibility check caught it and caught it
   legibly, artefact sizes differing by exactly the directory-name length. **115 artefacts are
   byte-identical across two build directories and across two source trees at path lengths 27
   and 95**, Catch2 included — stronger than the gate asked for.
7. **DONE** — CI running the gates offline from the cache, so green means the frozen numbers
   hold and not that the network was up. Two entry points rather than one, which is how the exit
   gate's "one command" and this step's "offline" were reconciled without weakening either.

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

1. **DONE** — `odl::Result` over vendored `tl::expected` (D7), manifest-declared, NOTICE
   regenerated. The step also found the manifest's one real hole — a dependency's own build
   system fetching an unpinned transitive dependency — now closed and written into §3.11 point 4.
2. **DONE** — `time`. Six implementation defects were caught by the tests meant to catch them,
   one of which is worth keeping in the plan: the two-part Julian date was formed as a single
   double and then split, **reintroducing the 2⁻³¹ d ≈ 40 µs quantisation that SPEC-time §4.2
   exists to disqualify** — inside the module built to avoid it. Found by comparing two routes
   to TCB−TDB. Also: `from_calendar` evaluated rate-dependent offsets once instead of iterating
   (1.15 µs against a 1 ns budget, with a comment claiming femtoseconds); `ut1_two_part_jd`
   added ΔUT1 to TAI rather than UTC.
3. **DONE** — `eop`. `finals2000A.all` predicts about a year ahead and TIME-R-051 refuses UTC
   past the leap table's expiry, so loading it failed outright: two adopted specs colliding in
   a way neither anticipated. The series now truncates at the leap horizon and reports the
   count, so a caller meets EOP-F-007 naming coverage rather than a leap-table error three
   layers down.
4. **DONE** — `frames`. ω×r was being formed in ITRS when the Earth spins about the CIP, which
   is TIRS's z. The gate was restated: see §4 rule 1, which this step's measurement corrected.

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

1. **DONE** — `ephemerides`: planetary and lunar positions, SPK through CALCEPH, CeCILL-B taken
   and recorded. Gate passed on the **full** `testpo.440` sweep: 11 354 of 13 201 cases checked
   on `de440.bsp`, 0 outside coverage, 1 847 not body-position cases, worst residual
   7.105 × 10⁻¹⁵ AU = **1.06 mm** against JPL's own 10⁻¹³ AU = 14.96 mm. The units trap was
   designed out rather than tested for: CALCEPH is asked for km at every call site so no
   conversion factor appears in this tree's source at all, and a negative test perturbing the AU
   by one part in 10⁶ requires the comparison to fail.
   Two findings worth carrying. An SPK kernel carries **no constants** — `getconstantcount`
   returns zero — so EPH-R-012's "read the AU from the kernel" was unsatisfiable on the mandated
   route; since IAU 2012 Resolution B2 the au is a *defining* constant, so the definition is the
   authority and a kernel supplying one is checked against it. And the solar-system barycentre is
   the **root** of an SPK's body tree — the centre of every record and the target of none — so
   scanning for it as a target found nothing and 868 cases were being silently counted as
   outside coverage. The gate reporting its denominator is what exposed that.
2. **DONE** — `gravity`: the geopotential to full degree and order, EGM2008 to 2190. Gate
   passed on the degree-variance identity at three radii (2190 of 2190 degrees at 7331 km, 300
   points, mean ratio 0.99699) **and** the J2-only closed form point-wise, which agrees to
   2.5 × 10⁻¹⁶ over 32 points spanning both poles. The two are not redundant: the identity
   constrains the power per degree, the closed form constrains where on the sphere it sits.
   26 ms per full degree-2190 evaluation, 11 ns per coefficient pair against a 100 ns budget.

   **The instruction to derive rather than cite was right and the derivation was not enough.**
   Both representations of the normalised recursion fail, in opposite directions: the classical
   one underflows above 43.7° of latitude, and the factored one — asserted in the spec to stay
   near 10.3 — reaches 10^457.9 at degree 2190, order 979, overflowing a double by 10^150 and
   then turning the column to NaN through inf − inf in the three-term recursion. Neither works
   alone. What works is the pair: a global 10⁻²⁸⁰ scale with cos^m φ folded back through a Horner
   nest over order so it is never formed as a number. That is Holmes & Featherstone's
   construction, arrived at from measurement rather than from the paywalled paper, which is the
   stronger route and the reason the derive-don't-cite instruction stands.

   Five more corrections implementation forced, of which two are worth carrying: an off-by-one
   in the truncation statistic advancing (a_e/r)ⁿ before its first use rather than after — a
   clean 13% error at 7331 km, exactly the size that reads as a modelling difference, visible
   **only** because the expected values were computed independently in Python rather than from
   the code under test. And WGS 84's GM cannot be refused by its value, because it *is* the
   TCG-compatible EGM2008 value under another name; what is refusable is taking both constants
   from WGS 84, whose semi-major axis is 6 378 137.0 m against the model's 6 378 136.3 m — a
   relative 1.1 × 10⁻⁷ that the (a_e/r)ⁿ factor carries to 2.4 × 10⁻⁴ by degree 2190.
3. **DONE** — `tides-relativity-thirdbody`, built as **three link targets** (`tides`,
   `relativity`, `thirdbody`) from one specification, because their dependencies are disjoint and
   one target would make every consumer of third-body attraction link the ocean-tide tables.
   Gate: **every printed per-term value in the Conventions, and a statement of what they do not
   print** — 50 constituents of Tables 6.5a/6.5c and 21 zonal at θ_f = 0, the closed-form
   pole-tide coefficients of §6.4 and §6.5 to every printed digit, and chapter 10's precession
   rates (de Sitter 19.188 mas/yr and height-independent to 10⁻¹², Lense–Thirring 0.755 at GEO
   and 181.7 at 6778 km against the stated 0.8 and 180). Two counts reported on every run.

   **Three things this step establishes that outlive it.** The tabulated δ*k*_f is **not** what
   (6.9) generates: the Conventions define it under (6.8e) as the body-tide difference *"plus a
   contribution from ocean loading"*, and §6.2.1 folds the load resonances into the body-tide
   tables. So the formula cannot verify the column — what it verifies is the resonance
   *structure*, as a ratio constant to 2.98% across a band where δ*k* varies by 2955×, with the
   3.4% offset the loading term. A statistic can be misnamed as easily as a gate: §6.5's 90% is
   of the **potential**, and the raw coefficient variance under the same word is 75.8%. And a
   parser demanding six-digit Doodson codes dropped 7 952 of 59 462 rows and 8 of 18 long-period
   waves **without failing** — the row count caught it, which is §4 rule 3 paying for itself a
   third time.

   **The K₁ worked example, found late and now the best row in §8.** Chapter 6 prints a complete
   numerical case — inputs *A*₁, *H*_f = 0.36870, θ_f, *k*₂₁ and its nominal value; outputs both
   ΔC̄₂₁ and ΔS̄₂₁ lines — which `PERT-A-029` reproduces from the published inputs alone, using
   nothing from the module's own table: (470.915, −30.2105) × 10⁻¹² against the printed
   (470.9, −30.2), then both expressions at **eight values of θ_g**, worst residual 5.2 × 10⁻²⁶.
   It is the only check anywhere that exercises the **θ dependence**, which evaluating at
   θ_f = 0 cannot, and the only non-circular check of Step 2. It was missed at drafting by
   executor and manager alike; see §4 rule 4.

   **Open: `PERT-Q-011`, the printed resonance-formula corrections.** (1,1) for Q₁ through
   (244,299) for ψ₁, in units of 10⁻⁵. They are **signed** — the Conventions print (0, −1) for
   P₁ — so what blocks *formula + correction = table* is not the signs but that δ*k*^OT is not
   tabulated separately. What that leaves is better than it sounds: table − formula = correction
   + δ*k*^OT **measures** δ*k*^OT per constituent for the nine constituents where the correction
   is printed, against the Conventions' own words for it. ψ₁ is where it bites — its printed
   correction is 83.5% of its δ*k*^I where P₁'s is 1.3% — and it is the constituent whose
   imaginary residual is already flagged as anomalous.
4. **DONE** — `atmosphere`: NRLMSISE-00 from the NRL public-domain FORTRAN per D2, plus
   space-weather ingestion with its own manifest entries. The port reproduces the reference to
   **2.33 × 10⁻¹⁶ — about one ulp — over 1 238 material comparisons across 125 cases**, and the
   single porting error was diagnosed by its own signature: every species below 72.5 km high by
   *exactly the same factor*, which says the fault is in something they all multiply and turns
   "wrong somewhere below 72.5 km" into three candidates.

   **There are no published reference profiles.** Established by search, 2026-09-18, over all five files NRL distributes: the
   driver publishes 17 fully specified input cases and no expected output (0 occurrences each of
   OUTPUT, RESULT, SAMPLE, COMPARE, EXPECTED below line 2438); `datavsmodels.txt` and the
   companion `.doc` publish 27 tables of data-minus-model statistics, which are not model output
   and cannot be recomputed without the NRLMSIS database NRL does not ship; the paper's two
   numbered tables point at those same statistics, and its model output is in figures. *Gate:*
   the reference implementation's own output on its 17 published cases, frozen as generated
   source with the compiler, flags and source hash recorded — see §4 rule 6 for why that ranks
   as a published value here and not as an oracle — plus the header's documented total-density
   relation, which is checkable **without** the reference's arithmetic and is therefore a second
   axis rather than a second look. Out-of-range input refused with a diagnostic naming the
   request and the limit. The model gate exercises **none** of the space-weather layer, because
   the 17 cases carry F10.7 and Ap as literal constants: that layer is gated separately, on
   coverage, class and the recomputed centred mean.

   **Three findings from the step that outlive it.** The reference is **single precision
   throughout** — no `DOUBLE PRECISION`, no `REAL*8`, no `.D0` — so the model's own value is
   uncertain, and a sweep across every branch boundary put the worst single-vs-double difference
   at 7.9 × 10⁻³, a thousand times the 17 cases' figure. Every large one is a quantity of order
   10⁻³⁰ to 10⁻³⁷ approaching single's underflow: one phenomenon in three regimes, not a
   tolerance with an exception bolted on. The class boundary is therefore **physical** — a
   species contributing less than 10⁻¹⁵ of total density cannot affect drag — and it is stable
   across three decades of threshold, with class A's worst remaining argon at 1000 km, which is
   published case 3. Adding 1 056 comparisons left the bound where the 17 cases put it, which is
   a stronger result than "the sweep passed". That boundary is a **drag** boundary and the API
   says so: mass density always returns, and a number density the model does not resolve is a
   refusal.

   The coefficients are **stored under names the model never uses** — sixty-four 50-element
   arrays in `BLOCK DATA`, the same 3 200 words declared as `pt(150)`, `pd(150,9)`, `ps(150)`
   and the rest, with the boundaries not falling where the letters do. An extractor taking the
   DATA names at face value produces nine arrays the model never indexes and they look
   plausible. 2 020 of the 3 300 literals are **zero**, so a spot check lands on
   zero-against-zero more often than not; all 3 300 are verified, 1 280 of them non-zero.

   **Space weather is the first input in this tree that is not frozen**, and the answer needs no
   special case: pin by hash, never fetch at run time, refuse outside usable coverage naming
   *which quantity* ran out, recompute derived columns, carry the snapshot's identity into every
   result, and let updating be a manifest change that §5 constraint 9 already makes re-run every
   gate. GFZ is primary and DRAO the independent cross-check — **7 969 of 7 969 exact** over the
   overlap, with 16 duplicate-timestamp dates settled first-wins by the 16 cases that
   discriminate. CelesTrak is dropped: its own centred-81-day column is **wrong wherever it is
   predicted** — 176 of 25 333 rows, all of them PRD or PRM and none OBS or INT, worst 30.07 sfu
   — so it is exact for sixty-nine years of history and arbitrary for every forecast epoch, with
   nothing in the file saying so.

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
what stands in their place with the cost of the substitution stated (§4 rule 6); and every
external table in the layer is manifest-declared with a hash.

---

### 3.4 L3 `dynamics` — 0 of 4 done; **the open layer**

Small in code and the hinge of the design: this is where the predecessor's fixed-width
sensitivity block becomes a registry, which is what later gives a joint covariance instead of a
grid scan.

**Entry:** L2 exit gate.

1. **TODO** — Force plugin surface: the one interface every force implements,
   `accel(t, state, params) -> (a, da/dstate, da/dparams)`. Defined and frozen **before any
   force exists**, because retrofitting it is how the predecessor ended up unable to estimate
   drag at all.

   This step also discharges the half of L2 step 2's crossing condition that could not be
   discharged there. `odl/core/units.hpp` names the km/metre crossing and tests it, but it has
   no callers — nothing before L3 builds a field position from a state — so `FRAME-R-062`'s
   *no other site performs it* is a requirement with nothing enforcing it. The plugin surface is
   the first place a state in km meets an acceleration in m s⁻², and it is the site `SPEC-dynamics`
   must name. *Gate:* the surface states where the crossing happens; a CI gate fails on a km↔m
   scaling written anywhere outside `core/units.hpp`, and is demonstrated to fail by an injected
   one. A gate that is green because no second site exists yet is worth having for exactly that
   reason — it turns red on the day one is written, which is the day it matters.
2. **TODO** — Integrators: RK4 and a DP8(7)- or RKF7/8-class variable-order scheme. Gate: the
   analytic two-body solution, with step-size insensitivity demonstrated rather than assumed.
3. **TODO** — State transition matrix, integrated alongside the state. Gate: agreement with
   finite differences of the propagated state to a stated tolerance.
4. **TODO** — Parameter sensitivity registry: any registered parameter automatically gains a
   sensitivity column and a place in the joint covariance, for whatever set is registered, with
   no fixed width anywhere. Gate: a registered parameter's column matches finite differences,
   and registering a second parameter requires no change to the integrator.

**Exit gate:** the plugin surface carries a trivial test force end to end, sensitivities
included, with nothing in the integrator aware of what the parameter means.

---

### 3.5 L4 `forces-analytic` — 0 of 6 done

Every non-gravitational force that can be written in closed form. The ray-traced treatment of
the same physics is L9 and deliberately later: this layer must stand alone, because it is what
the MVP needs.

**Entry:** L3 exit gate. Step 5 additionally needs L5 step 1.

1. **TODO** — `shadow`: conical shadow first, then the perspective-projection model with
   atmospheric refraction. Source: Li, Ziebart, Bhattarai et al. 2019. Gate: the paper's
   published eclipse geometry cases.
2. **TODO** — `srp-analytic`: cannonball, flat plate, box-wing. Sources: Fliegel & Gallini;
   Rodríguez-Solano et al. 2012. The coefficient convention is stated per model and tested — a
   sphere's (9 + 4ν(1−μ))/9 is not a flat plate's 1 + ρ_s, and conflating them is a factor of
   two in a recovered area. That is an acceptance test, not a comment.
3. **TODO** — `drag`: the drag force over L2's atmosphere, with the drag coefficient a
   **registered parameter from the first commit**, never a constant. Gate: published ballistic
   coefficient cases, and the parameter's sensitivity column against finite differences.
4. **TODO** — `erp`: Earth albedo and infrared radiation pressure. Sources: Knocke et al. 1988;
   Rodríguez-Solano et al. 2012. Gate: the papers' published accelerations for a stated
   geometry.
5. **TODO** — `thrust-yaw`: antenna thrust and the yaw-attitude laws. Sources: Steigenberger
   2018; Kouba 2009; Montenbruck et al. 2015; the official Galileo, GLONASS and BeiDou
   attitude-law documents. Gate: published yaw angles through noon and midnight turns, per
   constellation.
6. **TODO** — `ecom`: the empirical SRP frame, D/Y/B. Source: Arnold et al. 2015. Gate: the
   published parameterisation reproduced on a GNSS arc.

**Exit gate:** a GNSS arc fit with this layer's forces reaches its frozen residual, every force
carrying its own published test case — the oracle ranks last (§4 rule 2).

---

### 3.6 L5 `spacecraft` — 0 of 5 done

The macromodel library as **data with per-value citations**, not as code.

**Entry:** L4 step 2 — the models that consume the library must exist to shape its schema.

1. **TODO** — Schema: surfaces, areas, normals, optical coefficients, mass, centre of mass,
   each value carrying a citation field. A value without a citation is a load error, not a
   warning.
2. **TODO** — GPS, from Fliegel & Gallini 1992/1996.
3. **TODO** — Galileo, from the ESA GSC metadata published in 2017 — dimensions, mass, centre
   of mass, optical coefficients and attitude law.
4. **TODO** — GLONASS, BeiDou and QZSS, from the IAC metadata, CSNO 2019 and the Cabinet Office
   release, consolidated through the IGS satellite metadata SINEX.
5. **TODO** — Altimetry: Jason from the CNES box-wing macromodel (Cerri et al. 2010),
   Sentinel-6 from the ESA/EUMETSAT metadata.

**The sharpest edge in the plan.** The predecessor's own surface models under `res/` and
`analysis/` — a hand-built hundred-plus-surface GPS-IIR model with material properties — are
**not** taken. A compilation of that size can attract a database right under UK and EU law even
where copyright in it is thin. Where a public source is coarser than the predecessor's model,
the answer is a model derived from published dimensions and imagery, which is this tree's own;
not a transcription, which is not.

**Exit gate:** every value resolves to a citation, and the library refuses to build if any does
not.

---

### 3.7 L6 `io-measurements` — 0 of 4 done

Formats in, measurements out. Public specifications throughout — nothing here is anyone's
intellectual property but the format authors'.

**Entry:** L1 exit gate.

1. **TODO** — Formats: SP3, TLE, CRD and CPF, the optical observation formats, SINEX and ANTEX.
   Each reader refuses a field it does not recognise rather than defaulting it. The predecessor
   read an SP3 interval from the wrong field and skipped a fixed header length; both are
   acceptance tests here.
2. **TODO** — Horizons client, with the timescale refusal built in: a table not on the
   requested scale is rejected, because 69 s of TDB is ≈ 518 km along track and a fit converges
   on it without complaint.
3. **TODO** — `sgp4`: this tree's own port written against the published test vectors per D4,
   with TEME handled through L1 rather than assumed inertial. Gate: the published SGP4
   verification vectors to their stated tolerance — **and the required-disagreement gate that
   §4 rule 1 moved here from L1**, against oracle T-01. TEME is referred to the mean equinox of
   date, so unlike ITRF↔GCRS there is no pole-offset series to reconcile two precession models
   and the ≈ 0.064″ ≈ 2.2 m difference appears undiluted. Assert size *and* direction; agreement
   is the failure. Vallado's kinematic equation-of-equinoxes terms must be carried but are
   ≈ 95 mm, 4% of it, not the explanation.
4. **TODO** — `measmod`: ephemeris-position, SLR range and optical angles against one
   interface, with the station and site registry. Light time, tropospheric refraction and the
   observer's own motion belong to the model, not to the caller. Gate: each measurement's
   partials against finite differences, plus a published SLR range case.

**Exit gate:** every format round-trips, and a measurement model returns residual and partials
for all three observation types.

---

### 3.8 L7 `estimation` — 0 of 4 done

Two of the predecessor's defects are design requirements here rather than lessons learned.

**Entry:** L3 and L6 exit gates.

1. **TODO** — Batch least squares with normal equations **scaled by default**, never as an
   option. The predecessor's columns spanned twelve orders of magnitude and its unscaled
   inverse was noise that presented as unobservability.
2. **TODO** — Levenberg–Marquardt, because a sail's trajectory over days is nothing like
   linear in its initial state and plain Gauss–Newton diverges from it.
3. **TODO** — A priori constraints, isotropic and RTN. The anisotropic form is necessary
   wherever a prediction is wrong almost entirely along track, which is every high
   area-to-mass object.
4. **TODO** — Joint covariance over the state and every parameter registered in L3. Gate: a
   joint confidence region for two correlated parameters recovered from one fit — the thing the
   predecessor could only approach by grid-profiling one against the other.

**Exit gate:** a fit over real data reaches its frozen residual and reports a joint covariance
whose correlations are reproduced by finite differences.

---

### 3.9 L8 `campaigns` — 0 of 5 done — **the MVP gate**

The validation campaigns as executable definitions, run by CI. The campaign *logic* of the
revival's `validate_*.sh` scripts carries over — already this tree's own work — re-pointed at
these binaries.

**Entry:** L7 exit gate, and L5.

1. **TODO** — GNSS against IGS precise orbits: the three frozen baselines.
2. **TODO** — Laser ranging: the LightSail-2 campaign against its frozen residual.
3. **TODO** — Optical angles: the sparse single-site campaign, with its condition number
   reported rather than hidden, because the residual alone flatters it.
4. **TODO** — Frame regression: the TEME→J2000 check against a mission ephemeris.
5. **TODO** — Atmosphere: density plausibility and diurnal variation.

**Exit gate — the MVP.** Every campaign meets its frozen number, each statistic **naming the
formula it was computed with**. Two unstated denominators produced defensible-looking wrong
numbers within two days of each other; §4 rule 3 exists because of it.

---

### 3.10 L9 `raytracer` — 1 of 4 done — *optional, the owner's call after L8*

The pixel-array method and thermal re-radiation. Strategically the most valuable layer and the
only one off the critical path.

**Entry:** L8 exit gate, plus an explicit decision to start.

1. **DONE** — Patent gate: searched before committing effort, **clear**, no encumbrance found
   on the method. Done first precisely so that the expensive layer cannot be started on an
   assumption.
2. **TODO** — Ray tracer: pixel-array acceleration computation. Sources: Ziebart 2001 (PhD),
   Ziebart 2004 (*J. Spacecraft & Rockets*), Ziebart et al. 2005.
3. **TODO** — Thermal re-radiation. Sources: Ziebart et al. 2005 (*Adv. Space Res.*), Adhya et
   al., Bhattarai et al. 2022.
4. **TODO** — Grid generation, format and interpolation. The format and the interpolation
   scheme are *ideas* and are this tree's; the predecessor's **generated grid files are not
   taken** and are regenerated by this tracer. That is wanted independently of ownership — the
   first question anyone asks of a grid is whose code made it.

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
6. **A reference implementation is an oracle when a specification exists that it implements, and
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
   a statistic is recorded with its formula under rule 3. "The source does not print X" earns
   the same scrutiny as "the source prints X = 1.333 × 10⁻⁹", because a gate is weakened as
   surely by evidence not looked for as by evidence read wrong. A gate reworded from the source
   is a correction to this plan and is recorded as one; a gate quietly satisfied by something
   else is not.

## 5. Design constraints, binding every layer

*(Numbered stably — the adopted specifications cite these as "plan §5 constraint n".)*

1. **ERFA, not SOFA** — BSD; SOFA's rename clause is the reason.
2. **IAU 2006/2000A**, CIO-based; not the equinox-based older chain.
3. **No GPL/LGPL/AGPL** anywhere in what could ship.
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
| D2 | NRLMSISE-00 route | Decided in plan: own port from the NRL public-domain FORTRAN (L2 step 4). **"Validated on its packaged tests" was wrong — there are no packaged tests**, only 17 input cases with no expected output; established by search 2026-09-18 and corrected in §3.3 step 4. Validation is against the reference's own output on those cases, frozen with its toolchain, per §4 rule 6. |
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

Committed in this repository, which is **local-only with no remote, deliberately**. Keep it off
any public remote: the predecessor's repository is public, its `NOTICE` says UCL's rights are
unresolved, and a plan for reimplementing around those rights has no business being published —
least of all there. A remote for this tree, if ever wanted, is a separate and explicit
owner decision.

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
