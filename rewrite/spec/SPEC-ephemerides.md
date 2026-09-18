# SPEC-ephemerides — positions of the Sun, Moon and planets

| | |
|---|---|
| **Spec ID** | `EPH` |
| **Status** | **draft, for manager review** — implementation follows a reviewed spec (plan §3.11 point 2) |
| **Version** | 1.0 |
| **Date** | 2026-09-18 |
| **Layer** | L2 `environment`, step 1 (`doc/REWRITE_PLAN.md` §3.3) |
| **Depends on** | `SPEC-time.md` (the TDB argument), `core` |
| **Depended on by** | third-body attraction (L2 step 3), radiation pressure (L4), light-time (L6) |

**Derivation declaration.** This specification was written from the documents listed in §2. No
file under the repository's `src/`, `include/`, `res/` or `scripts/` — the predecessor's code —
was opened, listed, searched or otherwise inspected during its preparation.

> **This is deliberately weaker than the declaration the P1 specifications carry, and the
> difference is not cosmetic.** Those were written while the predecessor lived in a separate tree
> and the separation was a property of the process. The trees were merged on 2026-09-18 at the
> owner's instruction, so that separation no longer exists and cannot be claimed. What is claimed
> above is what is true and checkable: the predecessor's source was not read. `SPEC-template.md`
> §2's declaration block is written for the clean-room era and has no post-merge variant; see
> `EPH-Q-006`.

---

## 1. Purpose and scope

This module answers one question: **where is a solar-system body, and how fast is it moving,
at a given instant, in the barycentric celestial frame.** It is the floor of L2 — everything
else in the layer either acts on a spacecraft or is a property of the Earth, and this is neither.

### In scope

- Position and velocity of the Sun, the planetary system barycentres, the individual planets
  where the kernel provides them, the Moon, the Earth, and the Earth–Moon barycentre.
- Both the barycentric and any body-centred vector the kernel can form.
- The TT−TDB series where the kernel carries it (DE440 does), as a cross-check on `time`.
- Reading SPK kernels through CALCEPH; declaring them in the manifest; refusing a request
  outside a kernel's coverage.

### Not in scope

| excluded | owned by |
|---|---|
| The geopotential and its coefficients | L2 step 2, `gravity` |
| Third-body **acceleration** — this module supplies positions; forces consume them | L2 step 3 |
| Solid Earth, ocean and pole tides | L2 step 3 |
| Atmospheric density | L2 step 4 |
| Light-time iteration and aberration | L6 `measmod` |
| Earth orientation, and anything that rotates a vector into or out of the ITRS | `SPEC-frames.md` |
| Lunar librations and the Moon's body frame | L4/L5, if ever needed |

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `PARK21` | R. S. Park, W. M. Folkner, J. G. Williams, D. H. Boggs | *The JPL Planetary and Lunar Ephemerides DE440 and DE441*, Astronomical Journal 161:105 (15 pp) | 2021 | DOI `10.3847/1538-3881/abd414`; the identical PDF is served by NAIF at `naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440_and_de441.pdf` (retrieved 2026-09-18) | **primary** — open access, "© 2021 The Author(s)" | normative |
| `SPK-RR` | NASA/JPL NAIF | *SPK Required Reading*, SPICE Toolkit documentation | current | `https://naif.jpl.nasa.gov/pub/naif/toolkit_docs/C/req/spk.html` (retrieved 2026-09-18) | **primary** | normative (format) |
| `TESTPO` | JPL Solar System Dynamics | `testpo.440` — the published verification set for DE440 | DE440 | `https://ssd.jpl.nasa.gov/ftp/eph/planets/ascii/de440/testpo.440` (retrieved 2026-09-18) | **primary** | normative (acceptance values) |
| `CALCEPH` | IMCCE / Observatoire de Paris | CALCEPH library, source, `LICENSE` and documentation | **4.0.5**, June 2025 | `https://www.imcce.fr/recherche/equipes/asd/calceph/` (retrieved 2026-09-18) | **primary** | interface — **triple-licensed; see §3.4** |
| `TN36-1` | IERS | *IERS Conventions (2010)* TN 36 ch. 1, Table 1.1 — numerical standards | 2010 | held; see `SPEC-time.md` §2 | primary | normative |
| `IAU2012-B2` | IAU | Resolution B2 (2012) — the astronomical unit is 149 597 870 700 m exactly | 2012 | cited **through** `PARK21` §2, which states the value and the adoption | **secondary** | normative for one constant — see `EPH-Q-005` |

---

## 3. Definitions and conventions

### 3.1 Frame and time argument

- **Frame.** DE440 is tied to the **ICRF3** [`PARK21` §2.1], which the IAU adopts. Vectors from
  this module are in the **BCRS**, axes aligned with the ICRS — the same axes as the GCRS of
  `SPEC-frames.md`, differing only in origin and in the relativistic scaling of the two systems.
- **Time argument.** The DE series is integrated in **TDB** [`PARK21` §2.3].

- **EPH-R-001.** Every call MUST take an `odl::time::Epoch` and render it to **TDB** through
  `SPEC-time.md`'s `two_part_jd(TimeScale::TDB, …)`. A bare Julian date MUST NOT be accepted.

  *There is deliberately no refusal for "the wrong time scale".* A draft of §7 had one, and
  writing §8 showed it could never fire: an `Epoch` stores TAI and has no scale to be wrong, and
  a bare number cannot be constructed into one (`TIME-R-003`). The condition is unrepresentable
  rather than refused, which is the stronger outcome and the one L1 was built for.
- **EPH-R-002.** The two-part split MUST be passed through to the library as two arguments.
  `CALCEPH`'s documentation is explicit that this is what the split is for: *"To get the best
  numerical precision for the interpolation, the time is splitted in two floating-point
  numbers."* Collapsing it to one argument reintroduces the quantisation `SPEC-time.md` §4.2
  disqualifies — 40 µs near JD 2.46 × 10⁶, which is 21 km of Earth's barycentric motion.
- **EPH-R-003.** UTC MUST NOT reach this module. `CALCEPH`'s own documentation carries a
  standing warning against it, and the failure is the one that cost this project 500 km once
  already (`SPEC-time.md` §3.4).

### 3.2 Units — the defect this step exists to design out

**This module's gate is a units gate**, and the reason is a measured defect rather than a
worry: the revival's ephemeris path had a routine whose comment claimed AU while it returned km,
and it survived because the place it was used was a round trip in which the error cancelled.

The facts:

- **`CALCEPH`'s default is AU and AU/day.** `calceph_compute` returns "the cartesian position
  (x,y,z), expressed in Astronomical Unit (au), and the velocity … in Astronomical Unit per day
  (au/day)" [`CALCEPH` doc], **whatever the file's own units are** — it normalises.
- **This tree's canonical units are km and km s⁻¹**, fixed at L1 by `odl::frames::State`.
- So **every call needs a conversion by a factor of 1.495 978 707 × 10⁸**, and the place that
  conversion lives is the place the defect lives.

- **EPH-R-010.** `calceph_compute` MUST NOT be called. Only `calceph_compute_unit` (or
  `calceph_compute_order`) MUST be used, with the unit flags given **explicitly at every call
  site**: `CALCEPH_UNIT_KM | CALCEPH_UNIT_SEC`. The reason is not that the default is wrong —
  it is documented and correct — but that it is *invisible at the call site*. A reader of
  `calceph_compute(eph, jd0, t, target, centre, pv)` cannot see what `pv` is in; a reader of the
  `_unit` form can.
- **EPH-R-011.** No unit conversion MAY be written in this tree's own code. Asking the library
  for km and seconds removes the factor of 1.5 × 10⁸ from our source entirely, which is better
  than writing it correctly.
- **EPH-R-012.** The astronomical unit MUST be read **from the loaded kernel's own constants**
  and MUST be checked against 149 597 870.700 km — the IAU 2012 value, which `PARK21` §2 states
  DE440 adopts. A kernel whose AU differs MUST be refused (`EPH-F-004`). This is what stops
  `EPH-A-002`'s conversion check from cancelling against a wrong constant.
- **EPH-R-013.** Position is returned in **km** and velocity in **km s⁻¹**, and the returned
  type MUST carry its frame in the type system exactly as `odl::frames::State` does. A bare
  `double[6]` MUST NOT cross this module's boundary.

### 3.3 Body identity — the second trap, and it is quieter

There are **two numbering schemes**, and CALCEPH will use either:

| scheme | selected by | 1 is | 3 is | 10 is |
|---|---|---|---|---|
| classic JPL/DE | the default | Mercury | Earth–Moon barycentre | Moon (geocentric) |
| NAIF integer ID | `CALCEPH_USE_NAIFID` | Mercury **barycentre** | — | **Sun** |

`TESTPO` uses the **classic** scheme; SPK kernels are indexed by **NAIF IDs**. So the same
integer means different bodies depending on a flag, and a mistake is not a factor of 10⁸ — it is
a plausible vector for the wrong planet, which no magnitude check catches.

- **EPH-R-020.** Bodies MUST be named by a **closed enumeration** in this module's own interface
  (`Body::Sun`, `Body::Moon`, `Body::EarthMoonBarycentre`, …), never by an integer crossing the
  boundary. The mapping to CALCEPH's integers is internal, is written once, and is tested.
- **EPH-R-021.** `CALCEPH_USE_NAIFID` MUST be set or not set **consistently and explicitly**,
  and which was chosen MUST be recorded in the run's provenance. The acceptance suite MUST
  exercise the mapping against `TESTPO`, which uses the other scheme — so the translation is
  tested rather than assumed.
- **EPH-R-022.** `Body::Earth` and `Body::EarthMoonBarycentre` MUST be distinct members and
  MUST NOT be convertible to one another. They differ by up to 4 700 km.

### 3.4 CALCEPH is triple-licensed, and the choice is ours to make

`CALCEPH`'s own `LICENSE` file states it plainly:

> "The library is 'triple-licensed' (CeCILL-C, CeCILL-B or CeCILL), you have to choose one of
> the three licenses below to apply on the library."
>
> CeCILL-C — "close to the GNU LGPL" · CeCILL-B — "close to the BSD" · CeCILL v2.1 —
> "compatible with the GNU GPL"

- **EPH-R-030.** This tree takes **CeCILL-B**, and the choice MUST be recorded in the manifest
  entry, in the dependency register of `PROVENANCE.md`, and in the generated `NOTICE`. It is the
  only one of the three compatible with plan §5 constraint 3: the other two are copyleft, one
  LGPL-like and one GPL-compatible.
- **EPH-R-031.** The obligations CeCILL-B attaches MUST be recorded with the choice, because
  they are heavier than BSD's and they bind a **future** distribution rather than this tree
  today. §5.3.4 CREDITS requires a distributor of *modified* software to state that it is based
  on CALCEPH, reproduce the intellectual-property notice, make that notice reachable from the
  software's interface, and **mention it on a freely accessible website** for as long as it is
  distributed. This tree links CALCEPH unmodified, so the lighter obligations apply — but the
  decision that would change that is a decision someone will take later, and they should find
  this written down.
- **EPH-R-032.** The predecessor links CALCEPH statically and **records no choice at all**
  (plan R7). That is the situation this tree exists not to inherit, and the choice above is what
  not inheriting it consists of.

---

## 4. Required behaviour

### 4.1 Kernels are manifest inputs like any other

- **EPH-R-040.** Every kernel MUST be a manifest entry with URL, SHA-256 and a licence note, and
  MUST be loaded from the cache (plan L0 step 3, rule R11).
- **EPH-R-041.** A kernel's **coverage interval** MUST be read from the kernel and exposed, and a
  request outside it MUST be refused (`EPH-F-002`) naming the requested epoch and the interval.
  There is no extrapolation, for the reason `SPEC-eop.md` §4.5 gives at length: a Chebyshev
  polynomial evaluated outside its interval does not degrade, it diverges, and it does so
  smoothly enough to look like an orbit.
- **EPH-R-042.** Where several kernels are loaded, the body-to-kernel resolution MUST be
  deterministic and MUST be reported. Two kernels covering the same body over the same interval
  with different data MUST be a refusal (`EPH-F-005`), not a priority rule — the plan's whole
  posture is that a silent choice between two disagreeing sources is the defect.

### 4.2 What the module computes

- **EPH-R-043.** The primary operation returns the state of a `Body` relative to a `Body`, at an
  `Epoch`, in km and km s⁻¹.
- **EPH-R-044.** TT−TDB MUST be obtainable where the kernel provides it, and MUST be compared
  against `SPEC-time.md`'s `tdb_minus_tt` in the acceptance suite. Two independent routes to the
  same quantity, disagreeing, is the check that caught the most instructive defect of L1.
- **EPH-S-045.** Acceleration (the second derivative) SHOULD be available through
  `calceph_compute_order`, because L2 step 3 will want it for the third-body partial derivatives
  and computing it by differencing positions is avoidable error.

---

## 5. Interfaces

```
Body        := Sun | Mercury | Venus | Earth | Mars | Jupiter | Saturn | Uranus
             | Neptune | Pluto | Moon | EarthMoonBarycentre
             | MercuryBarycentre | VenusBarycentre | MarsBarycentre | …
             | SolarSystemBarycentre
             -- a closed enumeration. Integers never cross this boundary (EPH-R-020).

Ephemeris   := opaque, immutable
Coverage    := { first: Epoch, last: Epoch }

open(bytes_or_paths, provenance)        -> Result<Ephemeris, EphError>
coverage(Ephemeris, Body)               -> Result<Coverage, EphError>
constant(Ephemeris, name)               -> Result<double, EphError>   -- "AU", "EMRAT", "GM*"
provenance(Ephemeris)                   -> [ { url, sha256, kernel_id, naifid_mode } ]

state(Ephemeris, Body target, Body centre, Epoch)
                                        -> Result<State<Frame::BCRS>, EphError>
acceleration(Ephemeris, Body, Body, Epoch)
                                        -> Result<Vec3, EphError>     -- km/s^2
tt_minus_tdb(Ephemeris, Epoch)          -> Result<Duration, EphError>
```

Notes for the manager's review:

- **`Frame::BCRS` is a new member of `SPEC-frames.md`'s `Frame` enumeration**, and adding it is a
  change to an adopted spec. It is the honest shape: a barycentric vector is not a GCRS vector,
  they differ by the Earth's barycentric position of order 1.5 × 10⁸ km, and L1's whole argument
  was that a frame confusion must be unrepresentable. See `EPH-Q-001`.
- **`open` takes bytes or paths.** SPK kernels are large — `de440.bsp` is about 114 MB — and
  CALCEPH memory-maps or streams them. This is the first input in the tree too large to hand
  around as a `string_view`, and it is the one place the "loaders take bytes" convention of
  `SPEC-eop.md` §5 does not carry over. See `EPH-Q-002`.
- **No global state.** CALCEPH's handle is owned by `Ephemeris`; two instances must coexist, as
  `EOP-A-020` and `TIME-A-019` require of their layers.

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `EPH-P-1` | agreement with `TESTPO` | **< 10⁻¹³ AU** | 15 µm | the tolerance JPL states for its own test set |
| `EPH-P-2` | AU constant read from the kernel | exactly 149 597 870.700 km | — | `PARK21` §2, IAU 2012 |
| `EPH-P-3` | TT−TDB against `SPEC-time.md` | ≤ 100 ns | 0.75 mm at LEO | two independent routes; the difference is the check |
| `EPH-P-4` | interpolation from a two-part epoch vs a collapsed one | the collapsed form MUST be measurably worse | — | `EPH-R-002`; the test demonstrates the reason for the split |

`EPH-P-4` is unusual and deliberate, in the shape of `FRAME-A-009`: it asserts that doing the
wrong thing gives a **worse** answer, which is the only way to show that a precaution is earning
its place rather than being cargo.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `EPH-F-002` | request outside a kernel's coverage | requested epoch, the kernel's interval, the kernel's identity | extrapolate the Chebyshev polynomial; clamp to the end |
| `EPH-F-003` | body not present in any loaded kernel | the body, and which kernels were searched | return a zero vector |
| `EPH-F-004` | the kernel's AU constant differs from 149 597 870.700 km | both values and the kernel | use it anyway; use the built-in constant |
| `EPH-F-005` | two kernels both cover the body and the epoch | both kernels and the interval | apply a priority rule silently |
| `EPH-F-006` | CALCEPH returns a non-zero status | the CALCEPH call, its status, the body and the epoch | use the returned array |
| `EPH-F-007` | a kernel whose hash does not match the manifest | delegated to the fetcher; the build does not reach this module | — |

---

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `EPH-A-001` | **THE GATE.** Every case in `testpo.440` that falls inside the loaded kernel's coverage, requested in **AU and AU/day**. The count exercised MUST be reported, so "the gate passed" carries its denominator. | the published value in column 7 | `TESTPO` — **JPL's own published verification set**, ~13 200 cases | < 1 × 10⁻¹³ AU | R-001, R-002, R-041, R-043, P-1 |
| `EPH-A-002` | **THE UNITS GATE.** The same cases requested in **km and km s⁻¹**, compared against the published AU value scaled by the AU read from the kernel | agreement | `TESTPO` + `EPH-R-012` | < 1 × 10⁻¹³ AU equivalent | R-010, R-011, R-013 |
| `EPH-A-003` | the AU constant read from the kernel | 149 597 870.700 km exactly | `PARK21` §2 / IAU 2012 | exact | R-012, P-2 |
| `EPH-A-004` | `EPH-A-002` cannot pass if the AU constant is wrong: substitute a kernel constant differing by 1 part in 10⁶ and confirm `EPH-A-002` fails | failure | this spec | — | R-012 |
| `EPH-A-005` | body identity: `Body::Earth` and `Body::EarthMoonBarycentre` differ by 4 600–4 700 km | as stated | `PARK21`; EMRAT from the kernel | 10 % | R-020, R-022 |
| `EPH-A-006` | the classic-to-NAIF mapping, exercised by running `EPH-A-001` — whose cases are in the classic scheme — through the enumeration | agreement | `TESTPO` | as `EPH-A-001` | R-021 |
| `EPH-A-007` | TT−TDB from the kernel against `SPEC-time.md`'s `tdb_minus_tt` over a year | agree | two independent routes | ≤ 100 ns | R-044, P-3 |
| `EPH-A-008` | **the two-part epoch earns its place**: the same state computed with `(JD, 0)` instead of `(integral, fraction)` is measurably worse against `TESTPO` | the collapsed form is worse by ≳ 10⁻¹² AU | `TESTPO` | — | R-002, P-4 |
| `EPH-A-009` | refusal: an epoch one day outside coverage | `EPH-F-002` naming the epoch and the interval | this spec | — | F-002, R-041 |
| `EPH-A-010` | refusal: a kernel whose AU constant is altered | `EPH-F-004` naming both values | this spec | — | F-004 |
| `EPH-A-011` | refusal: two kernels covering the same body and interval | `EPH-F-005` naming both | this spec | — | F-005, R-042 |
| `EPH-A-012` | no global state: two `Ephemeris` instances over different kernels answer independently, alternately, in one process | as stated | this spec | exact | R-042 |
| `EPH-A-013` | structural: an integer body number cannot cross the interface, and a returned state carries its frame in the type | compile failure in both cases | this spec | — | R-013, R-020 |
| `EPH-A-014` | refusal: a body absent from every loaded kernel | `EPH-F-003` naming the body and the kernels searched | this spec | — | F-003 |
| `EPH-A-015` | fault injection: CALCEPH made to return a non-zero status | `EPH-F-006` naming the call, the status, the body and the epoch; **the returned array is not used** | this spec | — | F-006 |
| `EPH-A-016` | every kernel the module loads came from the manifest cache: loading a path outside it is refused | as stated | this spec (plan rule R11) | — | R-040 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `EPH-R-003` | Structural: the interface takes an `Epoch`, and `SPEC-time.md` makes UTC a rendering rather than a storable scale, so a UTC value cannot reach here. Discharged by `TIME-A-021`/`-A-022`. |
| `EPH-R-030`, `EPH-R-031`, `EPH-R-032` | Licence obligations. Discharged by the manifest entry, the generated `NOTICE`, `PROVENANCE.md` §3, and `tools/fetch.py check-licences`, whose allowlist admits `CECILL-B` and refuses `CeCILL-C` and `CeCILL v2.1` — by absence, which is why it is an allowlist. `EPH-R-031`'s content is prose in the ledger and cannot be asserted by a program. |
| `EPH-S-045` | A recommendation; tested when L2 step 3 consumes it. |
| `EPH-F-007` | Delegated to the manifest fetcher and tested there (`fetcher.behaviour`). |

---

## 9. Provenance obligations

- **Module register:** `ephemerides` → `PARK21`, `SPK-RR`, `TESTPO`, `CALCEPH`.
- **Parameter register:** the AU (149 597 870.700 km), EMRAT, and every GM taken from a kernel —
  each cited to the kernel that supplied it, by hash, not to a textbook.
- **Dependency register:** CALCEPH 4.0.5, **CeCILL-B chosen out of a triple licence**, with the
  §5.3.4 obligations recorded and the note that the predecessor recorded no choice.
- **Data manifest:** every kernel and `testpo.440`, with URL, SHA-256 and retrieval date.
- **§3.11 point 4 check:** CALCEPH's own build system was examined for what it declares or
  fetches. **It declares nothing and fetches nothing** — checked across every `CMakeLists.txt`
  and `.cmake` file in the 4.0.5 tarball, not assumed.

---

## 10. Open questions for the manager

| id | question | recommendation |
|---|---|---|
| `EPH-Q-001` | **`Frame::BCRS` is an addition to an adopted spec.** `SPEC-frames.md`'s `Frame` enumeration is GCRS/CIRS/TIRS/ITRS/TEME. A barycentric vector belongs in the type system on exactly the argument L1 made. | Add it, amending `SPEC-frames.md` to v1.4. The alternative — returning a bare `Vec3` — reintroduces the class of confusion L1 spent a whole layer making unrepresentable, and the difference here is 1.5 × 10⁸ km. |
| `EPH-Q-002` | **Kernels are too large to pass as bytes.** `de440.bsp` is ~114 MB; `SPEC-eop.md` §5's "loaders take bytes" convention does not carry. | Let `open` take cache **paths**, and keep the manifest as the only source of those paths so nothing is read that was not declared. Worth deciding explicitly because it is the first departure from a convention adopted one layer down. |
| `EPH-Q-003` | **Which kernel?** `de440.bsp` is 114 MB and spans 1550–2650; `de440s.bsp` is ~32 MB and spans 1849–2150. `testpo.440` has cases outside the short kernel's span, so the short one would skip a large share of the gate. | Pin **both**: `de440s.bsp` for routine use and CI, `de440.bsp` for the full `EPH-A-001` sweep. Record how many `TESTPO` cases each exercises, so "the gate passed" carries its own denominator — the habit plan §4 rule 3 asks for. |
| `EPH-Q-004` | **Is CALCEPH needed at all?** `SPK-RR` is a published format specification and the tree already reads two IERS formats itself. CALCEPH brings a French triple licence, an autotools build, and a dependency whose own build must be audited. | Keep CALCEPH — the plan decided it, it is well tested, and its `_unit` API removes the unit conversion from our code. Recorded because the reasons against are real and someone will ask. Re-examine only if the licence choice is ever challenged. |
| `EPH-Q-005` | **IAU 2012 Resolution B2 is cited through `PARK21`**, not obtained directly. The constant is not in doubt — `PARK21` states both value and adoption — but §2 carries a `secondary` row, which the template says must appear here. | Retrieve the resolution text before L2 closes. Nothing in §§4–8 depends on it beyond a constant that `EPH-A-003` checks against the kernel itself. |
| `EPH-Q-006` | **`SPEC-template.md` has no post-merge derivation declaration.** Its block asserts the clean-room separation, which ended on 2026-09-18. Every spec from here cannot use it, and each author will improvise. | Add a second block to the template — what this document's front matter uses — so the weaker claim is *standard* rather than invented per spec. An improvised declaration is exactly the thing a ledger cannot rely on. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.0 | 2026-09-18 | First draft, L2 step 1, for manager review. |
