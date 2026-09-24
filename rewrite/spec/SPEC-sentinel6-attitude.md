# SPEC-sentinel6-attitude — Sentinel-6's own closed-form pointing law

| | |
|---|---|
| **Spec ID** | `S6AT` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), step 4 (altimetry) — attitude LAW code, `modules/attitude`, beside GPS's own `SPEC-thrust-yaw.md`, Galileo's own `SPEC-galileo-attitude.md`, GLONASS-M's own `SPEC-glonass-attitude.md` and QZSS's own `SPEC-qzss-attitude.md` |
| **Depends on** | `modules/attitude`'s own already-built, already-gated `OrbitTriad`/`orbit_triad` (GPS's own shared machinery) |
| **Depended on by** | `srp_analytic`/`photon_force` (indirectly, via `sentinel6()`'s own macromodel, `SPEC-spacecraft.md`) |

**Derivation declaration (plan R1).** Written from the source's own §2/§3 (quoted in full below), and from
no implementation of this module beyond `modules/attitude`'s own already-built, already-gated
`orbit_triad`.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or
otherwise inspected during this specification's preparation, per `../plan/PLAN.md` §3.6's own explicit
refusal of the predecessor's hand-built models.

---

## 1. Purpose and scope

**Sentinel-6 Michael Freilich's own attitude law** — a genuinely new mechanism in this tree: nadir-
pointing with small, closed-form roll/pitch/yaw oscillations in the *argument of latitude*, with **NO
Sun dependence at all** (every earlier law in this module — GPS, Galileo, GLONASS-M, QZSS — is a
function of the Sun direction; this one is not). No campaign consumes this law (L8's own laser-ranging
campaign is LightSail-2, `../plan/PLAN.md`), so it is built with step 6's own two guards, adapted to this
law's own structure, the same "adapted, not silently reinterpreted" treatment `SPEC-qzss-attitude.md`
`QZSY-R-006` already gives a law with no ramp to time.

**Not in scope.** The macromodel data itself (`SPEC-spacecraft.md`, `sentinel6()`). Every other
constellation's own law — unchanged, untouched by this spec. SWOT's own attitude law (the SAME source
document also gives SWOT's own coefficients and a backward-flying frame flip Sentinel-6's own section
does not have) — not built, no consumer in this tree.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `S6ATT` | Flavien Mercier, John Moyard (CNES) | *SWOT and Sentinel 6 attitude laws* | undated, fetched 2026-09-24 | `https://ids-doris.org/documents/BC/satellites/SwotAndSentinel6AttitudeLaws.pdf`, SHA256 `5f94f657413f2df3788c0b8074a8145d6e2fb00a2432b280dfe2102ed41e8ddf` (pinned: this note carries no printed edition/revision number of its own, unlike the macromodel note, so the file hash IS the pin) | **primary**, obtained, "External diffusion: web site of the International DORIS Service" (`ids-doris.org`), the same redistribution statement `SPEC-spacecraft.md`'s own `sentinel6()` entry records, including that site's own placeholder Legal Notice | its own §2 (frame definitions, the rotation matrix, the angle formulae and Sentinel-6's own coefficients) and §3 (the platform-axis identification) — every equation and constant this spec states |
| `SATMOD` | CNES | *DORIS satellites models implemented in POE processing* | Ed.1/Rev.20, 2026-09-09 | `SALP-NT-BORD-OP-16137-CN`, the SAME document `SPEC-spacecraft.md`'s own `sentinel6()`/`jason2()`/`jason3()` entries cite, SHA256 `c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619` | **primary**, obtained | §16.2, which points to `S6ATT` by name as Sentinel-6's own attitude source, and §16.3's own "in sat ref frame" naming (this spec's own §3 states why that frame is treated as `S6ATT`'s own platform axes) |

No second, independent source was sought: `S6ATT` prints its own rotation matrix, angle formulae and
coefficients directly, in closed form, with no notation needing a second reading to resolve the way
`RS14`'s own α/δ/ρ letters did.

---

## 3. Definitions and conventions

- **The orbital frame is `modules/attitude`'s own `OrbitTriad`, EXACTLY, checked by direct comparison
  of the two constructions, not assumed from matching prose.** `S6ATT` §2, quoted: *"Orbital frame:
  this is the radial, tangential, normal frame, defined by the three vectors: R for the radial, N
  defined by R ^ V normalized, with V the inertial velocity, and T = N ^ R."* — `R = r_hat`, `N =
  normalize(r x v) = n_hat`, `T = n_hat x r_hat = t_hat`: the SAME three vectors `orbit_triad()`
  already builds for GPS, in the SAME order and the SAME cross-product sense, re-derived here
  independently from `S6ATT`'s own words rather than assumed shared because the SYMBOLS happen to
  match.
- **The rotation, worked out in full from `S6ATT`'s own printed matrices — not merely quoted (this
  spec's own new derivation, `attitude.cpp`'s own header comment carries the full working).** `S6ATT`
  §2: *"Geodetic pointing frame: this frame is defined by a roll around T, then a pitch around the
  transformed N, and then a yaw around the transformed R... R = R2 R3 R1"* (Eq. 1), with R2 (roll,
  around T), R3 (pitch, around N) and R1 (yaw, around R) each a standard single-axis rotation matrix,
  printed in full, and *"R is the transformation matrix for coordinates from the satellite frame to the
  orbital frame."* Composing R2*R3*R1 against each satellite-frame basis vector in turn (component
  order R,T,N throughout, forced by which row/column each single-axis matrix leaves invariant) gives:

      Rsat = (c2*c3)*R + s3*T + (-s2*c3)*N
      Tsat = (s2*s1 - c2*s3*c1)*R + (c3*c1)*T + (s2*s3*c1 + c2*s1)*N
      Nsat = (c2*s3*s1 + s2*c1)*R + (-c3*s1)*T + (c2*c1 - s2*s3*s1)*N

  (`c_i = cos(alpha_i)`, `s_i = sin(alpha_i)`) — VERIFIED, not merely computed: at `alpha1=alpha2=
  alpha3=0` this reduces to `Rsat=R, Tsat=T, Nsat=N` exactly (the identity rotation, as it must), and
  `S6AT-A-003` checks the construction against an INDEPENDENTLY worked-out closed form at the four
  angles where two of the three oscillations vanish simultaneously (§ below).
- **The angles: `S6ATT` §2, quoted directly.** *"With theta the position on the orbit, relative to
  the ascending node, the expressions are: alpha2 = a2 sin(theta) [roll], alpha3 = a3 sin(2*theta)
  [pitch], alpha1 = a1 cos(theta) [yaw]."* Sentinel-6's own coefficients (`S6ATT`'s own table): `a2 =
  -0.111 deg, a3 = +0.138 deg, a1 = +4.225 deg`.
- **Theta is a NEW angular quantity in this tree — the argument of latitude, from the ASCENDING NODE,
  NOT from midnight the way `mu_rad` is.** Built as `sentinel6_argument_of_latitude_rad(r, v)`: the
  ascending-node direction is the standard orbital-mechanics node vector, `Z_hat x n_hat` normalized
  (`Z_hat` the GCRS pole) — in-plane by construction, and PROVED (not merely computed) to give theta=0
  at the node and theta increasing in the direction of motion, via the same "signed angle between two
  in-plane vectors" identity `mu_rad`'s own header already leans on for a different vector pair
  (`attitude.cpp`'s own header comment carries the full derivation). **Checked along a propagated
  trajectory, the manager's own instruction, "as mu's now is"**: `S6AT-A-001` verifies this function
  against a CLOSED-FORM circular orbit (chosen so the ground truth is exact, not a numerical
  propagation's own residual error) at eight angles spanning a full revolution, and `S6AT-A-001c`
  checks theta strictly increases as the satellite is advanced forward along that same orbit — the
  SAME forward-in-time property `mu_rad`'s own Sec.30.12/30.14 fix established for a different angle,
  applied here to this law's own different angular origin. **The guard shown firing** (`S6AT-A-001b`):
  a deliberately reversed (wrong-sign) version of the same construction disagrees with the closed-form
  ground truth at every angle checked, and gives a DECREASING theta forward in time instead of an
  increasing one.
- **`S6ATT` §3: "For Sentinel 6 the axes Rsat, Tsat, Nsat correspond to the platform axes,
  respectively -z, x, -y."** So `z_body = -Rsat`, `x_body = Tsat`, `y_body = -Nsat`. Right-handed by
  construction (`x_body x y_body = Tsat x (-Nsat)`, which at `alpha_i=0` reduces to `T x (-N) = -(T x
  N) = -R = z_body`, the SAME `OrbitTriad` cyclic identity `orbit_normal_attitude`'s own header already
  proves for a different pair of axes; `S6AT-A-002` checks orthonormality and right-handedness holds
  at every theta checked, not only theta=0).
- **FRAME IDENTIFICATION, STATED AS AN ASSUMPTION, NOT VERIFIED BY A PRINTED COORDINATE PAIR OR REAL
  DATA (`S6AT-Q-001`).** `SATMOD` §16.3's own face-normal table states its own normals are "in sat ref
  frame" — the SAME name `S6ATT`'s own platform axes use, and both documents are CNES's own, for the
  SAME satellite, with `SATMOD` §16.2 explicitly pointing at `S6ATT` as this satellite's own attitude
  source. Treated here as the SAME physical frame, no additional rotation applied — UNLIKE Galileo's
  own `galileo_frame_from_mechanical` (verified against three real printed coordinate pairs) or QZSS's
  own `qzss_frame_from_native` (confirmed against real attitude data), this mapping rests on the two
  documents' own shared naming and cross-reference alone. No open quaternion source was found for
  Sentinel-6 this round (§4 below) to confirm it further.
- **THE APPENDIX'S OWN NUMERICAL SRP EXAMPLE (`SATMOD` Appendix 1) DOES NOT PIN THIS ROTATION
  CONVENTION — the manager's own named alternative used instead.** The appendix's own worked example is
  for SPOT-5's BUS ALONE (no attitude, no yaw/roll/pitch mentioned at all, `tests/spot5_appendix_tests.
  cpp`'s own header comment) — it validates the photon-pressure KERNEL (`SPEC-srp-analytic.md`
  `SRPA-A-011..A-013`), not this attitude law. Per the manager's own ruling, this law's own rotation
  convention is instead checked against the property that the roll/pitch/yaw oscillations VANISH AT
  THEIR OWN NODES, against an INDEPENDENTLY worked-out closed form (`S6AT-A-003`, §3 above): at
  theta=0/180 deg, roll and pitch both vanish (`sin(theta)=0` and `sin(2*theta)=0` together), leaving a
  PURE ROTATION ABOUT R by the yaw angle alone; at theta=90/270 deg, yaw and pitch both vanish
  (`cos(theta)=0` and `sin(2*theta)=0` together), leaving a PURE ROTATION ABOUT T by the roll angle
  alone — both forms worked out independently (not read back from the production code) and checked
  exactly.

---

## 4. Real-data control

**Sought, not found, this round.** `SATMOD` names no quaternion ancillary file for Sentinel-6 the way
it does for Jason-1/-2/-3 (`ja1att.txt`/`ja2att.txt`/`ja3att.txt`, and CDDIS/IGN quaternion FTP
directories, `SPEC-jason-attitude.md` §4). The IGN FTP archive that DOES carry Jason's own real
quaternions (`ftp://doris.ign.fr/pub/doris/ancillary/quaternions/`, confirmed openly reachable, no
login, `SPEC-jason-attitude.md` §4) was checked for a Sentinel-6-named subdirectory; none was found
under the satellite-code prefixes this session tried. `S6AT-Q-002` records this as a genuine absence,
not a silently skipped search.

---

## 5. Required behaviour

- **S6AT-R-001.** `sentinel6_attitude(r_gcrs_m, v_gcrs_m_per_s) -> Mat3` (never fails, no `Result`):
  the complete closed-form law, §3 above. Takes NO Sun direction — a genuine, permanent property of
  this law (not a mode this satellite ever leaves, unlike QZSS's own eclipse-season switch).
- **S6AT-R-002.** `sentinel6_argument_of_latitude_rad(r_gcrs_m, v_gcrs_m_per_s) -> double`: the
  argument of latitude, §3 above. Exposed publicly for its own acceptance test's verification only —
  `sentinel6_attitude` itself never reads this scalar except through the frame it implies, the same
  treatment `galileo_native_yaw_angle_pre_substitution` already gets.
- **S6AT-R-003.** Step 6's own two guards, adapted to this law's own structure (a smooth, ALWAYS-
  ACTIVE oscillation with no discrete mode switch at all, unlike every earlier law in this module — so
  neither guard's own literal shape transfers unchanged, each adapted to the property it actually
  protects, stated explicitly):
  - **Time-direction correctness** (`S6AT-A-001`/`S6AT-A-001b`/`S6AT-A-001c`, `TYAW-A-012`'s own role):
    the argument of latitude increases forward in time along a propagated trajectory, checked against
    a closed-form circular orbit; the guard shown firing on a deliberately reversed version.
  - **Rotation-convention correctness** (`S6AT-A-003`, `TYAW-A-015`'s own role, adapted per the
    manager's own instruction since the appendix's own numerical example does not pin attitude, §3
    above): the oscillations vanish at their own nodes, checked against an independently worked-out
    closed form at four geometries.

---

## 6. Interfaces, stated language-free

- `sentinel6_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3) -> Mat3` — S6AT-R-001. Never fails.
- `sentinel6_argument_of_latitude_rad(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3) -> double` — S6AT-R-002.

A stateless provider, the same shape every other attitude function in this tree already is.

---

## 7. Precision and accuracy

- **S6AT-P-1.** No hardware rates or per-satellite parameters supplied by the caller: Sentinel-6's own
  law, as `S6ATT` states it, is fully closed-form and geometric, needing nothing beyond the current
  orbital state.
- **S6AT-P-2.** The oscillations themselves are small (`|a1|` the largest, 4.225 deg; `|a2|`, `|a3|`
  under 0.14 deg) — a near-nadir-pointing satellite with a small, periodic wobble, not a large-swing
  law the way GPS's own noon/midnight turns are.
- **S6AT-P-3.** No real-data control was found this round (§4) — this law is UNCONFIRMED by real
  attitude data, the same status QZSS's own orbit-normal mode carries for a different reason
  (`QZSY-Q-001`), not contradicted by any data found.

---

## 8. Failure behaviour

This law never fails (`S6AT-R-001` states it returns a bare `Mat3`) — no failure table.

---

## 9. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `S6AT-A-001` | `sentinel6_argument_of_latitude_rad` matches a closed-form circular orbit (i=53 deg) exactly, at eight angles spanning a full revolution; the orbit's own n_hat has a positive Z-component (genuinely prograde, not an accidental retrograde construction) | exact match (1e-9); n_hat.z > 0 | `S6ATT` §2's own theta definition, worked out independently in the test | 1e-9 | R-002, R-003 |
| `S6AT-A-001b` | **the guard shown firing**: a deliberately reversed (wrong-sign) version of the same construction disagrees with the closed-form ground truth, and gives a DECREASING theta forward in time | disagreement found; decreasing theta | this file's own §3 derivation | — | R-003 |
| `S6AT-A-001c` | forward-in-time: theta strictly increases as the satellite is advanced forward along the same closed-form orbit, at twelve points spanning a full revolution | strictly increasing (mod wrap) | `mu_rad`'s own established verification shape, applied to this law's own angle | — | R-002, R-003 |
| `S6AT-A-002` | `sentinel6_attitude` returns an orthonormal, right-handed frame at every geometry checked (a full revolution, sixteen points) | determinant = 1; orthonormal | this file's own §3 derivation | 1e-9 | R-001 |
| `S6AT-A-003` | **the guard shown firing (rotation-convention)**: at theta=0/180 deg, the built frame matches an independently worked-out PURE ROTATION ABOUT R by the yaw angle alone; at theta=90/270 deg, a PURE ROTATION ABOUT T by the roll angle alone | exact match (1e-9) at all four angles | this file's own §3 derivation, `S6ATT` §2's own formulae | 1e-9 | R-001, R-003 |

**Coverage.** Every requirement above is discharged by a row; this law has no refusal (`S6AT-R-001`'s
own "never fails").

---

## 10. Provenance obligations

- `PROVENANCE.md` records, as part of the L5 step 4 section: the rotation matrix's own full working
  (R2*R3*R1 expanded against each satellite-frame basis vector, checked to reduce to the identity at
  zero oscillation); the argument-of-latitude construction and its own forward-in-time proof; the
  frame-identification assumption (`S6ATT`'s platform axes = `SATMOD`'s own "sat ref frame", stated,
  not independently verified) and the real-data search that did not find a Sentinel-6 quaternion source
  to confirm it further; the manager's own instruction to check the rotation convention against the
  nodes-vanish property once the appendix was found not to pin attitude for this satellite.

---

## 11. Open questions for the manager

| id | question |
|---|---|
| `S6AT-Q-001` | **The frame identification (`S6ATT`'s own platform axes = `SATMOD`'s own "sat ref frame") is a STATED ASSUMPTION, not independently verified** by a printed coordinate pair (Galileo's own precedent) or real attitude data (QZSS's own precedent) — both documents are CNES's own, for the same satellite, cross-referencing each other by name, but no stronger check was found or built this round. Worth pursuing if a real quaternion source for Sentinel-6 specifically is later found. |
| `S6AT-Q-002` | **No open quaternion source for Sentinel-6 was found this round** (§4) — the IGN FTP archive that serves Jason's own real quaternions was checked and carries no Sentinel-6-named directory under the prefixes tried. Worth a further search (a different naming prefix, or a different archive entirely) if this law's own real-data confirmation becomes load-bearing. |
