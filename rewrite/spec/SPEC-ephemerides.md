# SPEC-ephemerides — positions of the Sun, Moon and planets

| | |
|---|---|
| **Spec ID** | `EPH` |
| **Status** | **adopted** 2026-09-18, conditional on three corrections, which v1.1 applies |
| **Version** | 1.4 |
| **Date** | 2026-09-18 |
| **Layer** | L2 `environment`, step 1 (`../plan/PLAN.md` §3.3) |
| **Depends on** | `SPEC-time.md` (the TDB argument), `core` |
| **Depended on by** | third-body attraction (L2 step 3), radiation pressure (L4), light-time (L6) |

**On `IAU2012-B2`'s locator, said here rather than left to §10.** The resolution is served by
**SYRTE (Observatoire de Paris)**, not by the IAU. The IAU's own published location no longer
answers: searched 2026-09-18, `https://www.iau.org/static/resolutions/IAU2012_English.pdf`
returns 404 over both http and https, as do `.../IAU_2012_English.pdf`,
`https://iau.org/administration/resolutions/`,
`https://www.iau.org/administration/resolutions/general_assemblies/` and `.../ga2012/`. That
bounds the search; it does not establish that the IAU publishes no copy anywhere. SYRTE hosts
the IERS Conventions Centre and serves the individual resolution text, and what is pinned is
that text — read, not merely fetched: recommendation 1 is the exact metre value and
recommendation 2 the time-scale independence.

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

*Nothing about the predecessor reached the author during this specification's preparation.
The revival's units defect, cited in §3.2 as the reason for that section, came through the
manager's handover as a described failure mode, not as anything from the tree.*

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
| `IAU2012-B2` | IAU | Resolution B2 (2012) — the astronomical unit is 149 597 870 700 m exactly, **and is used with all time scales** | adopted by the XXVIII General Assembly, Beijing, 30 August 2012 | `https://syrte.obspm.fr/IAU_resolutions/Res_IAU2012_B2.pdf`, retrieved 2026-09-18, SHA-256 `3489ebb1…c984`, 107 641 bytes. **Served by SYRTE, not by the IAU** — see below | primary | normative |

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
- **EPH-R-012.** *(Amended at v1.2, by implementation.)* **An SPK kernel carries no constants at
  all** — measured: `calceph_getconstantcount` on `de440s.bsp` returns **zero**, and
  `getconstant("AU")` fails. So v1.0–1.1's "read the AU from the loaded kernel's own constants"
  is **not satisfiable on the SPK route the plan mandates**; the AU lives in the DE ascii header
  or a text PCK, not in a `.bsp`.
  
  The resolution is better than the requirement it replaces: since **IAU 2012 Resolution B2 the
  astronomical unit is a DEFINING constant**, exactly 149 597 870 700 m. Reading a defined
  constant out of a file was never more authoritative than the definition. Therefore:
  - The defining value is used, and whether it came from a kernel MUST be recorded in run
    provenance (`astronomical_unit().from_kernel`).
  - Where a loaded kernel **does** supply an AU — DE ascii and binary files do — it MUST be
    **checked against** the defining value and a mismatch refused (`EPH-F-004`).
  
  The non-cancellation argument is unaffected: the constant is fixed and asserted by `EPH-A-003`,
  and `EPH-A-004` perturbs it and requires the km comparison to fail.
- **EPH-R-013.** Position is returned in **km** and velocity in **km s⁻¹**, and the returned
  type MUST carry its frame in the type system exactly as `odl::frames::State` does. A bare
  `double[6]` MUST NOT cross this module's boundary.

### 3.2a Sign convention for time-scale differences

**One direction, everywhere: `tdb_minus_tt`.** v1.0 of this specification declared
`tt_minus_tdb` in §5 while `EPH-R-044` and `SPEC-time.md` said `tdb_minus_tt` — opposite signs,
named in adjacent sentences, inside the single check built to catch defects by comparing two
routes to one quantity.

- **EPH-R-004.** Every time-scale difference in this module MUST be named and returned in the
  sense **later minus earlier-named**, i.e. `tdb_minus_tt` is TDB − TT, and the opposite spelling
  MUST NOT appear. `SPEC-time.md` already uses that sense; this module follows it rather than
  inventing a second.

A sign error here would present as `EPH-A-007`'s two routes disagreeing by **exactly twice** the
value, which reads like a factor-of-two bug and sends the reader looking for one.

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
- **EPH-R-041.** Coverage is **per body per kernel**, not per kernel: a single kernel covers
  different bodies over different spans, and `de440s` does. It MUST be read from the kernel for
  the body asked about and exposed that way — which is what `coverage(Ephemeris, Body)` already
  is — and a request outside it MUST be refused (`EPH-F-002`) naming the requested epoch, the
  body, and that body's interval.
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
- **EPH-R-044.** TDB−TT MUST be obtainable where the kernel provides it, and MUST be compared
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

open(cache_paths, provenance)           -> Result<Ephemeris, EphError>  -- paths, see below
coverage(Ephemeris, Body)               -> Result<Coverage, EphError>
constant(Ephemeris, name)               -> Result<double, EphError>   -- "AU", "EMRAT", "GM*"
provenance(Ephemeris)                   -> [ { url, sha256, kernel_id, naifid_mode } ]

state(Ephemeris, Body target, Body centre, Epoch)
                                        -> Result<State<Frame::BCRS>, EphError>
acceleration(Ephemeris, Body, Body, Epoch)
                                        -> Result<Vec3, EphError>     -- km/s^2
tdb_minus_tt(Ephemeris, Epoch)          -> Result<Duration, EphError>   -- TDB − TT (EPH-R-004)
```

Notes for the manager's review:

- **`Frame::BCRS` is a new member of `SPEC-frames.md`'s `Frame` enumeration**, and adding it is a
  change to an adopted spec. It is the honest shape: a barycentric vector is not a GCRS vector,
  they differ by the Earth's barycentric position of order 1.5 × 10⁸ km, and L1's whole argument
  was that a frame confusion must be unrepresentable. See `EPH-Q-001`.
- **`open` takes cache PATHS, and this is the tree's one exception to "loaders take bytes"**
  (`EPH-Q-002`, ruled). SPK kernels are large — `de440.bsp` is about 114 MB — and CALCEPH
  memory-maps them; handing that around as a `string_view` costs more than the purity buys. The
  exception is **for kernels and nothing else**. The hash is verified by the fetcher *before* a
  path is handed over and `provenance()` records it, so the property the convention protected —
  nothing is read that was not declared — is preserved by a different mechanism rather than
  given up.
- **No global state.** CALCEPH's handle is owned by `Ephemeris`; two instances must coexist, as
  `EOP-A-020` and `TIME-A-019` require of their layers.

---

## 6. Precision and accuracy requirements

| id | quantity | budget | physical consequence | basis |
|---|---|---|---|---|
| `EPH-P-1` | agreement with `TESTPO` | **< 10⁻¹³ AU** | 10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 mm** | the tolerance JPL states for its own test set |
| `EPH-P-2` | AU constant read from the kernel | exactly 149 597 870.700 km | — | `PARK21` §2, IAU 2012 |
| `EPH-P-3` | TDB−TT against `SPEC-time.md` | ≤ 100 ns | 100 ns × 7.5 km s⁻¹ = **0.75 mm** at LEO | two independent routes; the difference is the check |
| `EPH-P-4` | interpolation from a two-part epoch vs a collapsed one | the collapsed form MUST be measurably worse | — | `EPH-R-002`; the test demonstrates the reason for the split |
| `EPH-P-5` | what `geocentric_state` does **not** include | the *L*_B scaling between TDB-compatible and TCB-compatible lengths, **not applied** | 1.550519768 × 10⁻⁸ × 3.844 × 10⁸ m = **5.960 m** on the Moon's geocentric distance; as a third-body acceleration, 3 × 1.550519768 × 10⁻⁸ × 1.09 × 10⁻⁶ m s⁻² = **5.070 × 10⁻¹⁴ m s⁻²**. **Against the smallest term L2 step 3 actually computes — the de Sitter correction, 3.478 × 10⁻¹¹ m s⁻² at 7331 km — that is 0.146 %.** v1.3 compared it against the ocean-tide truncation floor instead and called the ratio 5.9 × 10⁻⁴; the floor and the smallest term kept are different numbers, and `tests/l2_floors.cpp` now measures all three at one radius so that the comparison is read rather than reconstructed | `PERT-Q-010`'s ruling required this stated with its arithmetic rather than asserted from memory. It does **not** land above that floor, and now it is known rather than assumed |

**`EPH-P-1` said "15 µm" until v1.1, where it is 15 mm** — a factor of a thousand, in a budget
column, where a reader takes it for the precision the module achieves and sizes every later
tolerance against it. Every row now carries its multiplication (`SPEC-template.md` §1). Auditing
the other twenty-two budget rows in this tree afterwards found three more of the same class, all
in `SPEC-time.md` §6 and all in the sub-microsecond rows; they are corrected at its v1.4.

`EPH-P-4` is unusual and deliberate, in the shape of `FRAME-A-009`: it asserts that doing the
wrong thing gives a **worse** answer, which is the only way to show that a precaution is earning
its place rather than being cargo.

---

## 7. Failure behaviour

| id | condition | diagnostic must name | never instead |
|---|---|---|---|
| `EPH-F-002` | request outside the coverage **of that body in that kernel** | requested epoch, the body, that body's interval, the kernel's identity | extrapolate the Chebyshev polynomial; clamp to the end |
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
| `EPH-A-003` | the astronomical unit in force, **and that an SPK supplies none** | 149 597 870.700 km exactly, `from_kernel` false, and `constant("AU")` refusing | `PARK21` §2 / IAU 2012 Res. B2 | exact | R-012, P-2 |
| `EPH-A-004` | `EPH-A-002` cannot pass if the AU constant is wrong: substitute a kernel constant differing by 1 part in 10⁶ and confirm `EPH-A-002` fails | failure | this spec | — | R-012 |
| `EPH-A-005` | body identity: `Body::Earth` and `Body::EarthMoonBarycentre` differ by 4 600–4 700 km | as stated | `PARK21`; EMRAT from the kernel | 10 % | R-020, R-022 |
| `EPH-A-006` | the classic-to-NAIF mapping, exercised by running `EPH-A-001` — whose cases are in the classic scheme — through the enumeration | agreement | `TESTPO` | as `EPH-A-001` | R-021 |
| `EPH-A-007` | **TDB−TT** from the kernel against `SPEC-time.md`'s `tdb_minus_tt` over a year, in that sense | agree | two independent routes | ≤ 100 ns | R-004, R-044, P-3 |
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

| id | question | **resolution** |
|---|---|---|
| `EPH-Q-001` | **`Frame::BCRS` is an addition to an adopted spec.** `SPEC-frames.md`'s `Frame` enumeration is GCRS/CIRS/TIRS/ITRS/TEME. A barycentric vector belongs in the type system on exactly the argument L1 made. | **RULED 2026-09-18: add it, and amend `SPEC-frames.md` rather than declaring it here** — the frame enumeration belongs to that specification, and a second spec extending it silently is how enumerations drift. Done at `SPEC-frames.md` v1.4 §3.5, which carries the two conditions attached to the ruling: BCRS↔GCRS is a **translation**, so it must not be given a function shaped like the rotations (`FRAME-R-027`/`-028`), and BCRS is TDB-based where GCRS is TT-based, so the timescale must be stated (`FRAME-R-029`). |
| `EPH-Q-002` | **Kernels are too large to pass as bytes.** `de440.bsp` is ~114 MB; `SPEC-eop.md` §5's "loaders take bytes" convention does not carry. | **RULED 2026-09-18: paths, for kernels specifically, and the exception is named here so it does not spread.** The loaders-take-bytes convention exists to keep loaders pure and testable; at 114 MB memory-mapped it costs more than it buys. The hash remains the fetcher's job **before** the path is handed over, and `provenance()` records it — so nothing is read that was not declared, which is the property the convention was protecting. **This exception is for kernels and for nothing else:** every other loader in this tree continues to take bytes. |
| `EPH-Q-003` | **Which kernel?** `de440.bsp` is 114 MB and spans 1550–2650; `de440s.bsp` is ~32 MB and spans 1849–2150. `testpo.440` has cases outside the short kernel's span, so the short one would skip a large share of the gate. | **RULED 2026-09-18: pin both, and the full sweep must actually RUN at this step's gate** — not merely be possible. `de440s.bsp` for routine use, `de440.bsp` for the complete `EPH-A-001` sweep, and **the case count each exercises is recorded in `PROVENANCE.md`**. "Pinned both, CI uses the short one" decays into the full coverage being notional within two layers; the gate already reports its denominator, so saying this costs nothing. |
| `EPH-Q-004` | **Is CALCEPH needed at all?** `SPK-RR` is a published format specification and the tree already reads two IERS formats itself. CALCEPH brings a French triple licence, an autotools build, and a dependency whose own build must be audited. | **RULED 2026-09-18: keep CALCEPH.** The plan decided it, it is well tested, and its `_unit` API removes the unit conversion from this tree's code entirely. Recorded because the reasons against are real and someone will ask. Re-examine only if the CeCILL-B choice is ever challenged. |
| `EPH-Q-005` | **IAU 2012 Resolution B2 was cited through `PARK21`**, not obtained directly. | **RESOLVED 2026-09-18, before L2 closes, as its disposition required.** The resolution is obtained and pinned (`iau2012-b2`), and §2's row is primary. Reading it rather than citing it added something the secondary route did not carry: recommendation 2 says the definition is **used with all time scales such as TCB, TDB, TCG, TT** — so the au carries *no* time-scale dependence, unlike the GM values of `PERT-A-028` where the TDB/TCB rate *L*_B is precisely what separates two published numbers. That is worth having stated by the resolution itself in a layer that has spent this much effort on *L*_B. |
| `EPH-Q-006` | **`SPEC-template.md` has no post-merge derivation declaration.** Its block asserts the clean-room separation, which ended on 2026-09-18. Every spec from here cannot use it, and each author will improvise. | **RESOLVED 2026-09-18 at the template**, not per spec. `SPEC-template.md` §2 now carries a **Predecessor access** block which drops the impossibility claim and keeps a checkable one, and the manager extended the forbidden list beyond what this spec had proposed: `analysis/`, `analyses/`, `REVIVAL.md` and `PROVENANCE.md` describe the predecessor's internals as directly as its source does. This specification's front matter uses that block **verbatim**. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.4 | 2026-09-18 | **`EPH-P-5`'s reference point was the wrong one.** It compared the unapplied *L*_B scaling against the ocean-tide truncation floor while calling that "the smallest term L2 step 3 keeps"; the smallest term the layer actually computes is the de Sitter correction, 3.478 × 10⁻¹¹ m s⁻² at 7331 km, and the ratio is 0.146 % rather than 5.9 × 10⁻⁴. The conclusion is unchanged and now rests on a number that means what it says. `tests/l2_floors.cpp` measures the floor, the smallest term kept and the *L*_B effect **at one radius, from the modules**. |
| 1.3 | 2026-09-18 | **`state()` split by centre**, on `PERT-Q-010`'s ruling: it returned `State<Frame::BCRS>` whatever centre was asked for, so a geocentric vector came back typed as barycentric — the frame in the type, the **origin** in a runtime argument the type did not carry. Now `barycentric_state` returns `State<Frame::BCRS>`, `geocentric_state` returns `State<Frame::GCRS>` and **is** `FRAME-R-028`'s translation for an ephemeris body, and `relative_state` returns an **untagged** `RelativeState` for any other centre. Plan §5 constraint 10: *what a value means belongs in its type, never in the argument that produced it.* `EPH-P-5` added, stating what the geocentric call does not include, with its arithmetic. |
| 1.2 | 2026-09-18 | **Amended by implementation.** `EPH-R-012` corrected: an SPK kernel carries **no constants**, so "read the AU from the kernel" was unsatisfiable on the mandated route. Replaced by the IAU 2012 defining value, with a kernel-supplied AU *checked against* it where one exists, and `from_kernel` recorded either way. `EPH-A-003` now asserts the absence as well as the value. |
| 1.1 | 2026-09-18 | **Adopted, with the three corrections the adoption was conditional on.** `EPH-P-1` corrected from 15 µm to **15 mm** and every §6 row given its multiplication. §3.2a added: one sign convention, `tdb_minus_tt`, replacing v1.0's `tt_minus_tdb` in §5 — opposite signs in adjacent sentences inside the very check built to catch that class. `EPH-R-041` and `EPH-F-002` restated as **per body per kernel**. Four questions ruled: `Frame::BCRS` added at `SPEC-frames.md` v1.4 with its two conditions; kernels take **paths**, named as this tree's one exception; **both** DE kernels pinned with the full sweep required to RUN at the gate and its case count recorded; CALCEPH kept. The derivation declaration now uses the template's **Predecessor access** block verbatim. |
| 1.0 | 2026-09-18 | First draft, L2 step 1, for manager review. |
