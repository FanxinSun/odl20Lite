# Rewrite plan — a fully-owned reimplementation of the validated ODL pipeline

**Status:** foundations specified and adopted (`time`, `eop`, `frames` at v1.2); repository
live at `bdd80be`; oracle frozen (27 cases). **D1 decided 2026-09-18: C++20** (§7) — implementation is unblocked.
**Canonical:** `/home/rog/odl-self_built/doc/REWRITE_PLAN.md` — this file, and there is no other
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

### 3.1 L0 `foundation` — 2 of 7 done

**Entry:** none, this is the floor. **Language: C++20** (D1, decided 2026-09-18) — steps 3–7
are unblocked and are the live work of the tree.

1. **DONE** — Repository created with the licence in the first commit (`bdd80be`), together
   with this plan, the provenance skeleton and the P1 specifications. The licence-first
   obligation is discharged and stays discharged.
2. **DONE** — Oracle frozen: 27 cases in `oracle/cases.tsv`, every input hashed, `capture.sh`
   reproducing them. This is the step that lets the predecessor serve as a test oracle without
   anyone reading it again — the numbers are captured once and the tree is finished with.
3. **TODO** — Manifest format and fetcher. Every external input declared with URL, SHA-256 and
   a licence note, fetched from origin into a cache. Nothing enters the tree undeclared, so
   provenance becomes a build artefact rather than a habit. This step also fixes the C++20
   toolchain — build system, dependency acquisition, test framework — because every later step
   depends on it and because the acquisition mechanism must be able to honour URL-plus-hash
   pinning rather than resolving a version range. Each choice is recorded with its licence.
4. **TODO** — CI running the gates offline from that cache, so a green build means the frozen
   numbers still hold and not that the network was up.
5. **TODO** — Licence and NOTICE generation, driven from step 3's manifest and never
   hand-maintained, because a hand-maintained NOTICE is wrong within two dependencies. D1
   removed the `cargo license` route, so the tool is the Executor's choice and is recorded with
   its licence like any other dependency.
6. **TODO** — Spec-coverage checker, with its denominator pinned to OWN-PREFIX identifiers.
   Specs cross-cite by design and a naive count gave 128 against a true 121 in hand audit:
   small enough to look like rounding, and a script would have asserted it.
7. **TODO** — Reproducible-build flags.

**Exit gate:** a clean clone builds, tests and regenerates NOTICE with one command, CI green
from cached data alone.

---

### 3.2 L1 `time-frames` — 0 of 3 done, all three specs adopted

**Entry:** L0 steps 1–4.

1. **TODO** — `time`: TT/TAI/UTC/UT1/GPS/TDB. *Specification adopted v1.2* from IERS
   Conventions TN36 ch. 10, the IERS leap-second table and the ERFA documentation, carrying two
   decisions worth keeping: the representation is i64 seconds plus f64 fraction in TAI from
   1958, because a bare f64 Julian Date quantises at ≈ 40 µs ≈ 0.30 m at LEO and is disqualified
   by arithmetic; and the leap-second table has an enforced expiry with exactly one named,
   logged escape hatch. **Implementation and gate remain.** Gate: the Conventions' published
   worked examples, every timescale pair.
2. **TODO** — `eop`: EOP 20 C04 and `finals2000A.all` ingestion, splice, interpolation, tidal
   terms. *Specification adopted v1.2.* It is its own module rather than part of `time` — it
   reads files and holds a coverage policy — and that boundary was argued by the spec author
   and adopted, not assumed. **Implementation and gate remain.** Gate: published IERS values at
   sampled epochs, plus refusal outside coverage with the escape hatch exercised and logged.
3. **TODO** — `frames`: GCRS↔ITRS by IAU 2006/2000A through ERFA, TEME↔GCRS, RTN and DYB.
   *Specification adopted v1.2.* **Implementation and gate remain.** Gate: round-trip closure,
   **and** the required-disagreement test against oracle case T-01 — the predecessor computes
   IAU-76/1980 and this tree computes IAU 2006/2000A, so a ≈ 0.064″ ≈ 2.17 m separation at
   7000 km must be present with the right sign and size, and agreement is the failure (§4
   rule 1).

**Exit gate:** all three gates passed, and a state cannot be built without a declared timescale
or reinterpreted between frames — enforced by the type system, not by a test.

---

### 3.3 L2 `environment` — 0 of 4 done

Everything the spacecraft moves through or is pulled by, with no reference to the spacecraft
itself. A gravity field is the environment; a drag force is a spacecraft property and lives in
L4.

**Entry:** L1 exit gate.

1. **TODO** — `ephemerides`: planetary and lunar positions, SPK through CALCEPH. Sources: the
   JPL DE documentation and the SPK format specification. Gate: published DE test values, with
   the revival's units trap — a routine whose comment claims AU while it returns km — as a
   named acceptance test rather than a comment.
2. **TODO** — `gravity`: the geopotential to full degree and order, recursions taken from the
   tables printed in the Conventions rather than from anyone's code. Gate: published
   coefficients' acceleration at sampled points, with truncation behaviour stated and tested.
3. **TODO** — `tides-relativity-thirdbody`: solid Earth, ocean and pole tides; the relativistic
   correction; third-body attraction. Gate: the IERS Conventions worked examples, term by term.
4. **TODO** — `atmosphere`: NRLMSISE-00 from the NRL public-domain FORTRAN per D2, plus
   space-weather ingestion with its own manifest entries. Gate: the model's published reference
   profiles; out-of-range input refused with a diagnostic naming the request and the limit.

**Exit gate:** every model reproduces its published reference values and every external table in
the layer is manifest-declared with a hash.

---

### 3.4 L3 `dynamics` — 0 of 4 done

Small in code and the hinge of the design: this is where the predecessor's fixed-width
sensitivity block becomes a registry, which is what later gives a joint covariance instead of a
grid scan.

**Entry:** L2 exit gate.

1. **TODO** — Force plugin surface: the one interface every force implements,
   `accel(t, state, params) -> (a, da/dstate, da/dparams)`. Defined and frozen **before any
   force exists**, because retrofitting it is how the predecessor ended up unable to estimate
   drag at all.
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
   verification vectors to their stated tolerance.
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
   from code.
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
input hashed, `capture.sh` reproducing them. The plan quotes them for readability; the file
governs. Three rules apply to all of them:

1. **Parity means matching public truth, not the predecessor bit-for-bit.** The predecessor
   computes with IAU-76/1980 and consumes the IAU 1980 data products; this tree uses
   IAU 2006/2000A and the IAU 2000A products, so **certain disagreements are required, of
   predictable size, and agreement would be the failure** — the accumulated IAU-76 precession
   error is ≈ 0.064″ ≈ 2.17 m at 7000 km, essentially the whole of oracle case T-01. The
   required-disagreement test (FRAME-A-009's pattern: assert size *and* direction, fail on
   agreement as well as on excess) is the standing shape for these.
2. **An oracle comparison is never a gate on its own** — it ranks last among acceptance-value
   sources per `SPEC-template.md`. Published worked examples and published test cases are the
   gates; the oracle catches gross error — a sign, an axis, a factor of two.
3. **A statistic cited as an acceptance value carries the formula it was computed with.** Two
   unstated denominators produced defensible-looking wrong numbers within two days (identifier
   counting, 128 vs 121; block separation, 20.8σ vs 14.1σ). L8's block gate names
   its definition for exactly this reason.

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
| D2 | NRLMSISE-00 route | Decided in plan: own port from the NRL public-domain FORTRAN, validated on its packaged tests (L2 step 4). |
| D3 | Ray tracer | Decided in plan: own implementation from the papers; `photonsXforce` as cross-oracle only (L9). Owner revisits after L8. |
| D4 | SGP4 | Decided in plan: own port from STR#3 + Vallado 2006 on the published vectors (L6 step 3). |
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
least of all there. A remote for `odl-self_built`, if ever wanted, is a separate and explicit
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
