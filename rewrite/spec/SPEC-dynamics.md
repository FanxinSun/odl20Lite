# SPEC-dynamics — the force plugin surface

| | |
|---|---|
| **Spec ID** | `DYN` |
| **Status** | **draft** 2026-09-18, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-18 |
| **Layer** | L3 `dynamics`, step 1 (`doc/REWRITE_PLAN.md` §3.4) |
| **Depends on** | `SPEC-frames.md` (`State`, `Position`, `Acceleration`), `SPEC-time.md` (`Epoch`), `core` |
| **Depended on by** | every force in L4, the integrators (step 2), the STM (step 3), the registry (step 4), estimation (L7) |

**Derivation declaration (plan R1).** This specification was written from the documents listed
in §2 and from no implementation of this module. There is no external specification of this
interface — it is this tree's own design, and plan §4 rule 6's question ("is there a
specification the reference implements?") is answered in §2 rather than assumed.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation.

*Nothing about the predecessor reached the author during this specification's preparation.*

---

## 0. The crossing gate, as the plan words it, would fire fourteen times and catch nothing

Plan §3.4 step 1: *"a CI gate fails on a km↔m scaling written anywhere outside
`core/units.hpp`, and is demonstrated to fail by an injected one."*

The intent is right and the wording names a check whose feasibility is a measurement, so it was
measured before being specified — the same discipline §4 rule 4 applies to a source, applied to
a gate's own premise. **Every 1e3-family literal in the tree's production sources was
enumerated**: `1000`, `1000.0`, `1e3`, `1.0e3`, `0.001`, `1e-3`, excluding comment lines, over
62 production sources (12 test sources excluded).

**Fifteen occurrences, in seven files. One is the crossing. Fourteen are not.**

| file | what the literal is |
|---|---|
| `core/units.hpp` | `kMetresPerKilometre = 1000.0` — **the crossing itself** |
| `atmosphere/src/atmosphere.cpp` | `kGcm3ToKgm3 = 1.0e3` — g cm⁻³ → kg m⁻³ |
| `atmosphere/src/msis_thermosphere.hpp` | `std::fmod(yrd, 1000.0)` — day-of-year out of `YYDDD` |
| `atmosphere/src/msis_variation.hpp` ×2 | `long_deg > -1000.0` — the reference's "no longitude" sentinel |
| `eop/src/series.cpp` ×8 | milliarcseconds → radians, milliseconds → seconds |
| `gravity/src/secular_pole.cpp` | `kMasToRad` — milliarcseconds → radians |
| `tides/src/pole.cpp` | `out.rows_ < 1000` — a row-count sanity threshold |

A literal search is therefore **fourteen false positives to one true positive, and the one true
positive is the definition the gate must permit.** That is before L4 adds a dozen forces, each
with its own constants.

### 0.1 Why this is not fixed by narrowing the search

The obvious repairs are to exclude `eop`, or to require the literal to sit near a position-like
identifier, or to look only inside `dynamics`. Each would work today. **This tree has just
watched that approach fail twice in one layer**: `tests/test_one_secular_pole.py` matched a bare
`55.0`, was narrowed to exclude tests, matched a bare `55.0` again in production, and was
narrowed again — and the trend, not either instance, was the defect. *A guard that answers every
false positive by lowering its own sensitivity converges on a guard that fires at nothing.*

### 0.2 What replaces it: a register, not a search

- **DYN-R-040.** Every literal **whose value is exactly 1000 or 1/1000** — matched by value,
  not by spelling, because `1e3`, `1.0E+3`, `1000.` and `0.001` are the same number and a
  spelling list is a denylist wearing a different hat — is in a production source **either** in
  `core/units.hpp` **or** carries one of **two** markers, on its own line or the line
  immediately above it:

```
// UNIT-CROSSING: g/cm^3 -> kg/m^3            a genuine unit conversion
// NOT-A-UNIT-CROSSING: YYDDD radix           a factor of 1000 that converts nothing
```

  **The second marker came out of re-deriving the fourteen rather than trusting the enumeration
  that produced them**, which is what §0's own table would have let a reader skip. Four of the
  fourteen convert nothing at all — a date radix, two "no longitude" sentinels and a row-count
  threshold — and labelling those as conversions would have made the register say something
  false. A reader's use of the register is to scan it and see that no km↔m crossing hides among
  the entries; **one wrong label turns "fourteen unexamined literals" into "fourteen literals
  someone said were fine", which is worse than no register.**

  **One line of lookback, and no more.** Same-line alone would force a 145-character line at the
  one site where two literals share a long condition; a window of several lines would let an
  annotation drift onto a literal it was never written for, which is the attribution failure
  this register exists to avoid.

- **DYN-R-041.** **An annotation is not a permit.** No `UNIT-CROSSING` outside `core/units.hpp`
  may name kilometres. Injecting a km↔m scaling and labelling it honestly would otherwise
  satisfy a gate that only checks for the presence of a marker — and that is precisely what the
  gate exists to prevent. If kilometres are involved it **is** the crossing, and the crossing has
  one site. `DYN-A-010` proves this arm separately from the unannotated one.

The gate **prints the whole register** — every literal, its file, its line and its stated
conversion — and fails on one that is neither in `core/units.hpp` nor annotated. An unannotated
factor of a thousand cannot enter the tree, and a km↔m one cannot be annotated as anything else
without somebody writing the false sentence down.

This is the **licence allowlist's shape, not the denylist's**: adding an entry is a deliberate
act with a reason attached, and the gate converges on accounting for everything rather than on
flagging nothing. It also makes the fourteen visible **as a set**, which a search never does — a
reader can scan the register and see that no km↔m crossing hides among them.

**Consequence, stated because it reaches outside this layer.** Fourteen existing lines in
`atmosphere`, `eop`, `gravity` and `tides` acquire an annotation. That is mechanical and it is
the price of the gate being real; `DYN-A-010` is what proves it works, by injecting an
unannotated km↔m scaling and watching the gate go red.

---

## 1. Purpose and scope

### In scope

* **The one interface every force implements**: `accel(t, state, params)` returning an
  acceleration, `∂a/∂state`, and `∂a/∂params` — **defined and frozen before any force exists**,
  because retrofitting it is how the predecessor ended up unable to estimate drag at all.
* **Parameter identity**: what a registered parameter is, how a force asks for the one it wants,
  and why a bare index is refused.
* **The km↔m crossing**: where it happens, how many sites there are, and the gate that keeps it
  that way.
* **The force registry's composition**: summing accelerations, and reporting what each force
  contributed at a point.

### Not in scope

* **The integrators** — L3 step 2. This step defines what they integrate.
* **The state transition matrix** — L3 step 3, which consumes `∂a/∂state` from here.
* **The sensitivity registry's estimation behaviour** — L3 step 4.
* **Any actual force.** L4. This step is gated with a *trivial* force precisely so that the
  surface is not shaped by its first client.

---

## 2. Normative sources

| key | author / issuer | title | locator | obtained | role |
|---|---|---|---|---|---|
| `PLAN` | this tree | `doc/REWRITE_PLAN.md` §2, §3.4, §4 rules 1–6, §5 constraints 1–10 | in this repository at `1446dba` | primary | normative |
| `FRAMES` | this tree | `SPEC-frames.md` v1.7 — `State<F>` in km, `Position<F>`/`Acceleration<F>` in m, and `FRAME-R-062`'s crossing requirement | `spec/SPEC-frames.md` | primary | normative |
| `TIME` | this tree | `SPEC-time.md` — `Epoch` | `spec/SPEC-time.md` | primary | normative |
| `GRAV` | this tree | `SPEC-gravity.md` v1.3 — `GRAV-Q-006`, the gravity-gradient tensor, which step 3 resolves | `spec/SPEC-gravity.md` | primary | informative here, normative at step 3 |

**There is no external specification of this interface, and plan §4 rule 6 makes that a
statement to be recorded rather than assumed.** The searchable claim: no published standard
defines a force-model plugin signature for orbit determination. What exists in the literature is
*descriptions of particular implementations* — each codebase's own arrangement — and adopting
one would import its constraints without importing a specification. So this interface is this
tree's own design, it is normative on itself, and **nothing here can be checked against an
external authority.** What that costs: a design error is caught by this tree's tests or not at
all. §8 says which of the four L3 gates can and cannot catch which class of error.

---

## 3. Definitions and conventions

### 3.1 A parameter is not a position in an array

- **DYN-R-001.** A parameter has an **identity**, and a force asks for the parameter it wants by
  that identity. `params[3]` meaning *"the drag coefficient, by convention"* is refused.

This is plan §5 constraint 10 — *what a value means belongs in its type, never in the argument
that produced it* — at the scale where it does the most damage. The precedent is
`Ephemeris::state`, which returned `State<Frame::BCRS>` for any centre: the frame was in the
type as required, and the **origin** was a runtime argument the type did not carry, so a
geocentric vector came back labelled barycentric. A parameter vector indexed by convention is
the same defect with a dozen values instead of one, and with the convention written only in the
prose of whichever force was implemented first.

- **DYN-R-002.** A `ParameterId` is **issued by the registry** and is not constructible from an
  integer, a string, or anything else a caller can make up. `ParameterSet` has **no**
  `operator[](std::size_t)` and no `data()`. The registry's internal layout is dense — the
  integrator needs it to be — but that layout is **private**, and §3.3 states the one place it is
  visible.

- **DYN-R-003.** Registration states what the value **means**, not merely what it is called: a
  kind (`drag_coefficient`, `srp_scale`, `empirical_acceleration`, …), a unit, and a human name.
  Two parameters of the same kind on different spacecraft are different identities; a name alone
  would collide.

### 3.2 Nothing has a fixed width

- **DYN-R-004.** `∂a/∂params` is **dynamically sized from the registry**. No `std::array<double,
  N>`, no `kMaxParameters`, no compile-time extent anywhere on this path. Its width is
  `registry.size()` at the moment the propagation was set up, and it is recorded with the result
  so that a Jacobian cannot be silently read against a different registry.

This is the hinge of plan §2's design. The predecessor's fixed-width sensitivity block is why it
could grid-scan and not give a joint covariance; a `kMaxParameters` here reproduces that with a
larger constant, which is the same defect with a later onset.

- **DYN-R-005.** A `ParameterJacobian` is accessed **by `ParameterId`**, never by column number.
  `column(ParameterId)` refuses an id the jacobian was not built for, naming both registries.

### 3.3 The crossing, and the two sites

`SPEC-frames` puts `State<F>` in **kilometres** and `Position<F>`/`Acceleration<F>` in
**metres**, each following the units its normative source publishes. `core/units.hpp` names the
crossing and `FRAME-R-062` requires that no other site perform it — a requirement that has had
**nothing enforcing it**, because until this step there were no callers.

- **DYN-R-010.** The surface performs the crossing at exactly **two** sites, both in this
  module, both calling `core/units.hpp` by name:

* `field_position_m_from_state_km` — once, where a `State`'s position in km becomes the
  `Position<F>` in m that a force receives;
* `state_accel_km_s2_from_m_s2` — once, where the summed `Acceleration<F>` in m s⁻² becomes the
  velocity derivative of a `State` in km s⁻¹.

- **DYN-R-011.** A force **never** sees kilometres. It receives `Position<F>` and
  `Acceleration<F>`, both in metres, and cannot convert without naming `core/units.hpp`, because
  `Position` and `State` have no conversion between them and neither exposes a raw `Vec3`
  constructor that would make one silent.

§0.2's `DYN-R-040` is what keeps that true as L4 fills the tree with forces.

### 3.4 What `t` is

- **DYN-R-028.** **`GRAV-Q-006`'s structural half is settled here.** The gravity-gradient
  tensor ∂a/∂r is what step 3's variational equations need from `gravity`, and the question of
  whether it can be added later divides in two. **The additivity is decidable now and is
  decided: yes.** `SPEC-gravity` §5's entry points — `acceleration`, `potential`,
  `acceleration_by_degree`, `truncation_rms` — all **return values** and none fills a
  caller-supplied buffer, so a `gradient()` entry point breaks no caller and no existing
  signature changes. Step 3 therefore *has* a choice. **The cost comparison** — the same
  recursion yielding second derivatives at negligible extra cost, against a second traversal —
  needs step 3's code and is deferred there, and is restated at step 2's gate so it cannot be
  lost between the two.

- **DYN-R-012.** The time argument is an `Epoch`, not a double. A force that needs seconds since
  an arc epoch computes them from two `Epoch`s and says which scale it did it in; an integrator
  that carries a bare `double t` has put the time scale in a comment.

---

## 4. Required behaviour

- **DYN-R-020.** **The surface is frozen before any force exists.** Its gate uses a *trivial*
  force — a constant acceleration and a single parameter — so that the interface is not shaped
  by its first real client. The plan's reasoning is adopted rather than paraphrased: retrofitting
  is how the predecessor ended up unable to estimate drag at all.

- **DYN-R-021.** `accel` returns **all three** of the acceleration, `∂a/∂state` and
  `∂a/∂params`, or a refusal. A force that cannot supply a derivative **says so at registration**
  rather than returning zeros: a zero column is indistinguishable from "this parameter does not
  affect this force", and one of those is a fact while the other is a gap. `DYN-F-004`.

- **DYN-R-022.** `∂a/∂state` is **two named 3×3 blocks**, `∂a/∂r` and `∂a/∂v`, not one 3×6.
  A zero block and a forgotten block are indistinguishable, and a silently-zero derivative
  reading as a correct answer **is** the predecessor's drag defect that §3.4 exists to prevent.
  `DYN-R-021` already says an undeclared derivative is a refusal and not a zero column, for
  parameters; extending that to the state is consistency rather than expansion, and it is the
  same data under two names.

- **DYN-R-027.** **A force declaring no velocity dependence records the magnitude it is
  neglecting.** The declaration is not a boolean; it carries a bound, so that "no velocity
  dependence" cannot come to mean "nobody looked". The bound is a budget row, which gate 8
  evaluates like every other number in this tree's specifications (`DYN-P-3` is the worked case).
  A force that cannot bound it **refuses to register**, because an unbounded neglected term is
  not a modelling choice.

- **DYN-R-023.** **The registry sums, and reports what it summed.** `accelerations_at(state)`
  returns each active force's contribution *by force identity*, not merely the total. Plan §4
  rule 3's *quantities that must be compared are measured in one place at one reference point*
  makes this operational: `tests/l2_floors.cpp` reconstructed that comparison by hand across
  three specifications, and L4 has a dozen accelerations to rank. If the registry can produce
  the ranking, L4 inherits it instead of rebuilding it. **This is cheap and is therefore
  required rather than proposed**: the sum already visits every force, and reporting the terms
  costs the vector that holds them.

- **DYN-R-024.** Every signature returns `odl::Result` (constraint 4), and **no monadic
  chaining** (constraint 8). A refusal that unwinds is not a diagnostic the caller must consume.

- **DYN-R-025.** **No global state.** No registry singleton, no ambient "current parameter set".
  Two propagations with different registries run concurrently without interfering, and
  `DYN-A-009` asserts it rather than the comment claiming it.

- **DYN-R-026.** The integrator is **not aware of what a parameter means**. It sees a width and
  a block of doubles. This is L3's exit gate and it is a requirement here because the surface is
  what makes it possible or impossible.

---

## 5. Interfaces, stated language-free

**A force** is a thing with an identity, a declared list of `ParameterId`s it consumes, and an
evaluation taking `(Epoch, State<F>, ParameterSet)` and returning
`(Acceleration<F>, ∂a/∂state, ∂a/∂params)` or a refusal.

**A registry** issues `ParameterId`s from a declaration of kind, unit and name; reports its
size; and refuses an id it did not issue.

**A force set** holds forces, sums their accelerations, and reports the per-force contributions
alongside the total.

**A `ParameterSet`** maps `ParameterId` to value. No positional access exists.

**A `ParameterJacobian`** maps `ParameterId` to a 3-vector column, carries the registry identity
it was built against, and has a width but no public index.

---

## 6. Precision

| id | what | value | arithmetic | source |
|---|---|---|---|---|
| `DYN-P-1` | the km↔m round trip is **not** exact in general | exact only where the significand permits | 1000 is a power of ten and **not** a power of two | `core/units.hpp`'s own test, which the tree learned when a round-trip comment claimed exactness and 1e-9 failed it |
| `DYN-P-2` | `∂a/∂r`, `∂a/∂v` and `∂a/∂params` against finite differences | stated by the measurement at steps 3 and 4 | — | deferred deliberately: a tolerance asserted here would be a guess, and §8 records why the implementation must be analytic rather than finite-differenced |
| `DYN-P-3` | **SRP's velocity derivative** at *A*/*m* = 1 m² kg⁻¹, at any altitude | **3.1942 × 10⁻¹⁴ s⁻¹** | 4.56 × 10⁻⁶ N m⁻² × 2.1 × 3.335641 × 10⁻⁹ s m⁻¹ × 1 m² kg⁻¹ = **3.1942 × 10⁻¹⁴ s⁻¹** | aberration: ∂a/∂v ≈ a_SRP/*c*, with *C*_R = 2.1 and 1/*c* = 3.335641 × 10⁻⁹ s m⁻¹ |
| `DYN-P-4` | **drag's velocity derivative** at *A*/*m* = 1 m² kg⁻¹, at 953 km | **3.4441 × 10⁻¹¹ s⁻¹** | 2.123 × 10⁻¹⁵ kg m⁻³ × 7374 m s⁻¹ × 2.2 × 1 m² kg⁻¹ = **3.4441 × 10⁻¹¹ s⁻¹** | ∂a/∂v ≈ ρ*v C*_D, with ρ from `SPEC-atmosphere`'s model at 953 km and *C*_D = 2.2 |

### Why the velocity block is not a formality (`DYN-P-3` against `DYN-P-4`)

Solar radiation pressure is the force a conventional model declares to have no velocity
dependence, and the declaration is false: aberration makes the pressure depend on the
spacecraft's own velocity. **`DYN-P-3` is altitude-independent and `DYN-P-4` falls
exponentially, so a crossover exists with certainty**; only its altitude is uncertain.

The *A*/*m* = 1 m² kg⁻¹ normalisation is written into the arithmetic rather than left in the
prose, because gate 8's dimensional check rejected the row without it — correctly: "per unit
*A*/*m*" was carrying a factor the expression did not show, and the two sides then differed by
kg m⁻² s⁻¹. **The ratio below is independent of *A*/*m* and of the normalisation**; it depends
only on *C*_D/*C*_R, which is why one object is the whole of the comparison.

**Inside the model's domain**, measured against `SPEC-atmosphere`'s model with one object and
both coefficients, drag's velocity derivative exceeds SRP's by **9.3 × 10⁶ at 300 km** and
**1078 at 953 km**. NRLMSISE-00's data reach about 1200 km (`MSIS-STATS` Table 1's highest
band), and those two altitudes are inside it.

**Above that, only the existence of a crossover is claimed, not its altitude.** The argument
needs no model at all: `DYN-P-3` is altitude-independent — it is *P C*_R/*c*, with no density in
it — and `DYN-P-4` falls exponentially with density, so the two cross. On this model's analytic
continuation the crossing falls somewhere around 6000 km, **which is five times outside the
altitude range the model was fitted to**, and that figure is given to one significant digit
because it is worth one. At GNSS altitude the ratio is **of order 0.2** — SRP's term the larger
— on an extrapolation roughly twenty times outside the fitted domain.

**Two-figure ratios from a model evaluated far outside its domain would read as two-figure
facts**, which is why they are not printed here. What survives without the model is the whole of
the argument for `DYN-R-022`'s split: **SRP's aberration term becomes the dominant velocity
dependence once drag has decayed**, and *that* is a regime rather than a coincidence of two
vehicles.

---

## 7. Failure behaviour

| id | when | what it names |
|---|---|---|
| `DYN-F-001` | a `ParameterId` from a different registry | both registry identities, and the parameter's declared name |
| `DYN-F-002` | a `ParameterSet` missing a value a force declared it consumes | the force, the parameter's identity and kind |
| `DYN-F-003` | a `ParameterJacobian` read with an id it was not built for | the id, and the registry width the jacobian carries |
| `DYN-F-004` | a force registered without declaring whether it supplies each derivative | the force and the parameter |
| `DYN-F-005` | two forces registered with the same identity | the identity |
| `DYN-F-006` | a force returning a non-finite acceleration or derivative | the force, the state, and which component |

---

## 8. Acceptance tests

| id | what is checked | expected | source | discharges |
|---|---|---|---|---|
| `DYN-A-001` | **the surface compiles and carries a trivial force end to end**: constant acceleration, one registered parameter, acceleration and both Jacobians returned | as stated | this spec | R-020, R-021 |
| `DYN-A-002` | **a bare index cannot reach a parameter**: `ParameterSet` has no `operator[]`, `ParameterId` is not constructible from an integer or a string, and each is a **compile failure**, not a runtime refusal | compile failures | R-001, R-002 | R-001, R-002 |
| `DYN-A-003` | **identity, not name**: two parameters of the same kind and name on different bodies are different ids, and each resolves to its own value | distinct | R-003 | R-003 |
| `DYN-A-004` | **nothing has a fixed width**: registering a second parameter changes `∂a/∂params`'s width with no change to any declaration, and `grep` finds no `kMaxParameters`-shaped constant on this path | width follows the registry | R-004 | R-004 |
| `DYN-A-005` | **the jacobian refuses a foreign id**, naming both registries | `DYN-F-001`, `DYN-F-003` | R-005 | R-005, F-001, F-003 |
| `DYN-A-006` | **the velocity block is live**: a force whose acceleration depends on velocity produces a non-zero `∂a/∂v`, and the surface carries it | non-zero | R-022 | R-022 |
| `DYN-A-007` | **the registry reports its terms**: the per-force contributions sum to the total, and each is attributed | sum matches to 1 ulp per component | R-023 | R-023 |
| `DYN-A-008` | **an undeclared derivative is a refusal, not a zero column** | `DYN-F-004` | R-021 | R-021, F-004 |
| `DYN-A-009` | **no global state**: two force sets with different registries evaluated concurrently agree with serial evaluation | agreement | R-025 | R-025 |
| `DYN-A-010` | **THE CROSSING GATE**, per §0.2, proven in **four** states, because it has two arms and each needs both directions: **(a)** the tree as it stands is green; **(b)** an **unannotated** km↔m scaling injected into a production source turns it red; **(c)** the same scaling **annotated honestly** as `km -> m` *also* turns it red (`DYN-R-041`) — an earlier draft of this gate passed (c), which is the arm that matters most; **(d)** removing the injection returns it to green | the register, and red on both injections | plan §3.4 step 1, §4 rule 5 | R-040, R-041, R-010, R-011 |
| `DYN-A-011` | **exactly two crossing sites**, both naming `core/units.hpp`, asserted by the register of `DYN-A-010` rather than by a comment | 2 | R-010 | R-010 |
| `DYN-A-012` | refusals `DYN-F-002`, `-F-005`, `-F-006`, each fired **and shown not to fire** on the adjacent accepted input (plan §4 rule 5) | the diagnostics | this spec | F-002, F-005, F-006 |
| `DYN-A-013` | **`t` is an `Epoch`**: a force cannot be given a bare `double`, and a two-epoch difference names its time scale | compile failure | R-012 | R-012 |
| `DYN-A-014` | the km↔m round trip is **not** exact in general, against `core/units.hpp`'s own test | as that test records | P-1 | P-1 |
| `DYN-A-017` | **a neglected velocity dependence carries a bound**: a force declaring `∂a/∂v` absent supplies a magnitude, a force supplying neither refuses to register, and the two 3×3 blocks are separately addressable so "absent" and "zero" are different states in the type | `DYN-F-004`, and the two blocks distinguishable | R-022, R-027 | R-022, R-027 |
| `DYN-A-018` | **`gradient()` is additive**: `SPEC-gravity` §5's entry points all return values and none fills a caller-supplied buffer, so adding one breaks no caller — asserted against the header rather than the specification's prose, because it is the header a caller compiles against | every `ConventionalField` member returns `Result` | `SPEC-gravity` §5, `DYN-R-028` | R-028 |
| `DYN-A-015` | **constraint 4 and constraint 8 hold on this surface**: every public signature returns `odl::Result`, and `ci.sh` gate 9's tree-wide monadic-chaining check covers these sources as it covers every other module | as stated | plan §5 constraints 4, 8 | R-024 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `DYN-R-050` | A documentation obligation, not a behaviour: `PROVENANCE.md` must record §0's enumeration as the measurement that shaped the gate. Discharged by §9 and by the entry a reviewer can read. Listed rather than silently omitted, because the alternative — a test grepping `PROVENANCE.md` for a phrase — would pass on the phrase and not on the measurement. |
| `DYN-R-026` | *The integrator is not aware of what a parameter means* is **L3's exit gate**, and there is no integrator until step 2 and no second parameter until step 4. Testing it here would test a stub. It is a requirement on **this** step because the surface is what makes it possible or impossible, and `DYN-A-002` and `DYN-A-004` together are the part of it that is checkable now: no positional access exists and no width is fixed. The whole of it is step 4's own gate — *registering a second parameter requires no change to the integrator* — which this specification does not give an identifier to, because inventing a forward identifier for a test another step will write is how a dangling reference becomes a discharged-looking one. This row exists so the obligation is carried rather than quietly dropped between steps. |

### What these gates can and cannot catch

**There is no external specification of this interface** (§2), so plan §4 rule 6's cost
statement applies in its strongest form: **nothing in this tree can check that this surface is
the right shape.** It can check that it is self-consistent, that its constraints hold, and that
a force can be carried end to end. It cannot tell us that the decomposition is the one L4 and L7
will want — only building L4 on it can, and by then changing it is expensive. That is the
argument for freezing it against a *trivial* force rather than a real one: the trivial force
exercises the surface without negotiating with it.

**`DYN-A-010` is the only gate here that catches a defect in another layer's code**, and it is
the one the plan asked for. **A register that does not state its own residual is the thing it was
built to replace**, so:

* **A km↔m crossing written without a literal** — via a named constant defined elsewhere, or a
  `Vec3` scaled by a variable that happens to be 1000 — is out of the register's reach. The
  literal case is impossible and the named-constant case is visible at its definition; the
  variable case is not caught.
* **A MISLABELLED crossing passes both arms.** A km↔m scaling annotated as some other
  conversion — `// UNIT-CROSSING: mas -> arcsec` on a line that is nothing of the kind — carries
  a marker, so the first arm is satisfied, and does not name kilometres, so `DYN-R-041` is
  satisfied too. **Nothing mechanical will catch it.** The residual is held by the discipline
  that each entry is re-derived from what its line does rather than from the enumeration that
  found it — which is a **process, not a gate**, and which is exactly what produced this
  register's second marker and the `eop` limit-versus-value asymmetry that no enumeration could
  have shown.

This is plan §4 rule 6's third obligation — *state the cost where a reader will meet it* —
applied to a **guard** rather than to an acceptance value. A gate that reports only what it
catches invites the reader to believe it catches everything.

### The gate's denominator

`DYN-A-010` reports, and fails if it cannot report: production sources searched; literals whose
value is 1000 or 1/1000; how many are in `core/units.hpp`; how many are annotated
`UNIT-CROSSING`; how many `NOT-A-UNIT-CROSSING`; how many are accounted for by **neither**; and
how many name kilometres **outside** `core/units.hpp`. **The last two are the gate**; the rest
make it legible. Today: **62, 15, 1, 10, 4, 0, 0** — and 1 + 10 + 4 = 15, which the gate checks
rather than leaving to the reader.

**The pair of numbers is a measurement of the property, not only a guard on it.** Adding
`modules/dynamics` — the first caller of the crossing — took the search from **62 production
sources to 66 with still 15 literals**, because the crossing goes through `core/units.hpp` by
name. That pair is reported on every run and is expected to stay flat: **it is the number that
will move first if L4's dozen forces begin writing their own constants.**

The enumeration was confirmed by a **second, independent method** before the register was built:
first by matching the spellings of the 1e3 family, then by parsing every numeric literal in the
tree and keeping those whose **value** is 1000 or 1/1000. Both return the same 15. A register's
worth is its completeness, and one enumeration checking itself is not evidence of that.

---

## 9. Provenance obligations

- **DYN-R-050.** `PROVENANCE.md` records §0's enumeration — the 15 literals, the 7 files, and
  the finding that 14 of 15 are unrelated to the crossing — as the measurement that shaped the
  gate, in the form plan §4 rule 4 requires of a claim about what a check would see.

---

## 10. Questions, and the rulings on them

| id | question | state |
|---|---|---|
| `DYN-Q-001` | The register requires 14 existing lines in `atmosphere`, `eop`, `gravity` and `tides` to carry an annotation — L3 editing L1 and L2 files after those layers closed. | **RULED: do it here.** A gate that is not enforcing is not a gate, and report-only mode would make it green for a reason unrelated to the property it checks. Three conditions, all met: **annotation only** (nothing else changed; nothing needed changing — none of the fourteen is a hidden crossing and none is wrong); **each re-derived from what the line does**, not from the enumeration, which is how the two-marker design in `DYN-R-040` was found and how line 103's literal was traced to `SPEC-eop` §3.5 — C04 publishes δ*X*/δ*Y* in **arcsec** where `finals2000A` publishes them in **mas**, so the literal converts the *limit*, not the value; and **the suite was re-run**, 211 passing, behaviour unmoved. |
| `DYN-Q-002` | `∂a/∂state` as one 3×6, or two named 3×3 blocks? | **RULED: split it**, and take the strongest version — a force declaring no velocity dependence **records the magnitude it is neglecting**, which turns a type-level declaration into a budget row gate 8 evaluates. `DYN-R-022`, `DYN-R-027`, `DYN-P-3`, `DYN-P-4`. The supporting arithmetic was **measured rather than taken**, and it does not land where the message put it — see below. |
| `DYN-Q-003` | `GRAV-Q-006`'s interface decision at step 1, or with the cost comparison at step 3? | **RULED: split the question.** The **additivity is decided now and is yes** (`DYN-R-028`): `SPEC-gravity` §5's entry points all return values and none fills a caller-supplied buffer, so a `gradient()` entry point breaks no caller. `DYN-A-018` asserts it against the **header**, since that is what a caller compiles against. The **cost comparison** needs step 3's code, is deferred there, and is restated at step 2's gate so it cannot be lost between them. |

### `DYN-Q-002`'s arithmetic: the conclusion stands, the figure does not

The message's own numbers reproduce exactly — *a*_SRP = 4.56 × 10⁻⁶ × 3.36 = 1.5322 × 10⁻⁵
m s⁻², ∂a/∂v = *a*_SRP/*c* = 5.1107 × 10⁻¹⁴ s⁻¹, drag's 2*a*/*v* = 1.5677 × 10⁻¹³ s⁻¹, ratio
**0.326**, "about a third". The arithmetic is right.

**It compares two different vehicles.** The SRP figure uses *A·C*_R/*m* = 3.36 m² kg⁻¹, a sail.
The drag figure, 5.78 × 10⁻¹⁰ m s⁻², comes from `tests/l2_floors.cpp`, which states
*C*_D·*A*/*m* = **0.01** m² kg⁻¹ — a compact satellite, a factor of ~340 smaller in area to
mass. Like for like, **one** object, drag's velocity derivative at 953 km exceeds SRP's by
**1078×** (`DYN-P-3` against `DYN-P-4`), not 0.33×.

**And the measurement gives a better argument than the one it displaces.** "Same order at
953 km" would have been a coincidence of two vehicles. What is actually true is a *regime*:
SRP's term is altitude-independent and drag's decays, they cross at a few thousand kilometres,
and **at GNSS altitude SRP's velocity derivative is five times drag's**. That is where L4 is
aimed, and it is an argument for the split that survives knowing the correct numbers.

This is plan §4 rule 3's newest extension applied to a correction rather than to an original: the
ratio was not patched, the whole comparison was re-derived, which is what surfaced the vehicle
mismatch — a term that a patch would have carried across unexamined.

---

## Changelog

| version | date | change |
|---|---|---|
| 1.1 | 2026-09-18 | **All three questions ruled; register built and wired as `ci.sh` gate 10.** (1) The 14 annotations are in, each **re-derived from its own line**, which found that four of them convert nothing at all and produced `DYN-R-040`'s **second marker** — labelling a date radix or a sentinel as a conversion would have made the register say something false. (2) `DYN-R-041` added after the gate passed an injected km↔m scaling that was *honestly annotated*: an annotation is not a permit, and no conversion outside `core/units.hpp` may name kilometres. (3) `∂a/∂state` **split** into two named 3×3 blocks, with `DYN-R-027` requiring a neglected velocity dependence to carry a **bound**. (4) `DYN-P-3`/`DYN-P-4` measured: the SRP-versus-drag figure offered for the split compares a **sail's SRP against a compact satellite's drag**; like for like, drag's velocity derivative is 1078× SRP's at 953 km and 0.20× at GNSS altitude — a regime rather than a coincidence, and a better argument than the one it replaces. (5) `DYN-R-028`: `gradient()` is **additive**, decided from `SPEC-gravity` §5's existing interface. |
| 1.0 | 2026-09-18 | First draft, for review. Leads with §0: the crossing gate as the plan words it would fire on **15 literals of which 14 are unrelated** and one is the definition it must permit, measured before being specified. Replaced by a **register** — every factor of a thousand either in `core/units.hpp` or annotated with what it converts — which is the licence allowlist's shape rather than the secular-pole guard's, and which this layer has just twice watched fail by narrowing. |
