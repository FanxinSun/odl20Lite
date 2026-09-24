# SPEC-qzss-attitude — QZSS's own two-mode attitude law

| | |
|---|---|
| **Spec ID** | `QZSY` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.2 — two review rounds: the source basis for sharing the frame/law across QZSS satellites checked directly (§2 identical across all four documents; §3 is NOT uniformly shared, found reading all four rather than assumed); a day-by-day scan for QZS-1 found it absent throughout the reachable archive; an exploratory real-data check against QZS-3 (which shares QZS-1's own orbit-normal law word for word) found a mismatch, then DIAGNOSED it exactly — CODE's own analysis product does not implement that mode, modelling QZS-3 with generic yaw-steering instead (`QZSY-Q-004` closed) |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), step 3 (QZSS) — attitude LAW code, `modules/attitude`, beside GPS's own `SPEC-thrust-yaw.md`, Galileo's own `SPEC-galileo-attitude.md` and GLONASS-M's own `SPEC-glonass-attitude.md` |
| **Depends on** | `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering`, `signed_beta_rad`, `OrbitTriad`/`orbit_triad` (GPS's own shared machinery) |
| **Depended on by** | `srp_analytic`/`photon_force` (indirectly); L7's own box-wing fit; a future BeiDou "zero-bias" mode, which names the SAME mechanism `orbit_normal_attitude` builds here (`SPEC-spacecraft.md`'s own BeiDou entry) |

**Derivation declaration (plan R1).** Written from the source §2 names, and from no implementation
of this module beyond `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering`,
`signed_beta_rad` and `orbit_triad`.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or
otherwise inspected during this specification's preparation, per `../plan/PLAN.md` §3.6's own
explicit refusal of the predecessor's hand-built models.

---

## 1. Purpose and scope

**QZSS's own two attitude control modes**, as CODE (not cited data — `SPEC-spacecraft.md` is where
QZS-1's own citable macromodel DATA lives; this spec is where the constellation's own attitude
BEHAVIOUR is specified). The source is QZS-1's own SPI document, but the law itself — the frame
convention and the two modes' own construction — is stated as the QZSS bus family's general control
scheme, not a QZS-1-specific fact, and this spec's own real-data control (§4) confirms it against
TWO OTHER QZSS satellites' real attitude (J02, J04), not QZS-1 itself (absent from the checked
data).

**Not in scope.** The macromodel data itself (`SPEC-spacecraft.md`). GPS's, Galileo's and
GLONASS-M's own laws — unchanged, untouched by this spec. BeiDou's own attitude (carried, refused,
`SPEC-spacecraft.md`) — `orbit_normal_attitude` is built GENERALLY, reusable for BeiDou's own future
"zero-bias" mode, but this spec does not build or specify BeiDou's own dispatch logic or its own
sign convention, which may differ.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `SPI_QZS1_B` | Cabinet Office, Government of Japan, National Space Policy Secretariat | *QZS-1 Satellite Information* | rev. B, 2022-03-24 | `https://qzss.go.jp/en/technical/qzssinfo/khp0mf0000000wuf-att/spi-qzs1_b.pdf`, fetched directly 2026-09-24 | **primary**, obtained, "freely available to any user... shall indicate proper credit" (`qzss.go.jp/en/technical/qzssinfo/index.html`, quoted in full, `SPEC-spacecraft.md` §2) | its own §2 (reference frame), §3 (both attitude modes) — every equation and constant this spec states |

No second source was sought: `SPI_QZS1_B` prints the law's own two modes directly, in plain English
(no notation to resolve), and the manager's own instruction was to test the frame against the
document's own stated property and confirm with real data, not to find an independent literature
confirmation as well.

---

## 3. Definitions and conventions

- **QZSS's own native body frame (`SPI_QZS1_B` §2) is NOT this tree's own convention, and the
  mapping is TESTED, not trusted from prose alone (the manager's own instruction, and the Galileo/
  IIR precedents).** `SPI_QZS1_B` §2: origin at the launch-adapter-plane centre, +Z the L-ANT
  boresight (nadir, matching this tree's own +Z), +Y "parallel to the rotation axis of the solar
  panels," +X completing the right-handed system with +Y/+Z. No paired native/tree coordinate
  examples are printed (unlike `GALSC`), so the mapping cannot be checked the same way Galileo's
  was — instead, the source states a TESTABLE PROPERTY directly: §3(1) (yaw-steering mode), "The
  Sun is located in the negative hemisphere" of QZSS's own x. This tree's own `nominal_yaw_steering`
  ALWAYS has the Sun on the POSITIVE x side, by construction — so QZSS's own x is this tree's own x,
  negated. For the completion `+X = +Y × +Z` (§2's own words) to remain a PURE ROTATION (not a
  reflection) once x flips, y must flip too — the UNIQUE such rotation is 180° about Z, `(x,y,z) ->
  (-x,-y,z)`, the SAME form Galileo's own `galileo_frame_from_mechanical` has (independently derived
  here, not assumed shared, `SPEC-spacecraft.md`'s own top comment for `qzss.cpp`). **Confirmed by a
  SECOND, independent reading of the source** (§3(1)'s own y definition, "perpendicular to the plane
  made up by the Sun, Earth and satellite" — the SAME plane `nominal_yaw_steering`'s own `y_body =
  normalize(z_body × s_hat)` construction uses) and by the **real-data control** (`QZSY-R-004`
  below): four checks, two satellites, matched to 0.00003°–0.00019° — an order of magnitude tighter
  than any other constellation's own real-data control in this tree, the strongest confirmation any
  frame mapping here has had.
- **Yaw-steering mode (`SPI_QZS1_B` §3(1)) reduces to `nominal_yaw_steering` EXACTLY — PROVED by the
  frame-mapping identity above, not merely observed to look similar.** Once QZSS's own z (nadir),
  y (⊥ the Sun/Earth/satellite plane) and x (Sun-hemisphere-stated) are mapped through the 180°
  rotation, the result IS `nominal_yaw_steering`'s own construction — no separate `qzss_frame_from_
  psi`-style function exists, `qzss_yaw_attitude` calls `nominal_yaw_steering` directly in this
  branch, the SAME "deviation from an already-trusted law" pattern Galileo's and GLONASS-M's own
  nominal laws already are.
- **Orbit-normal mode (`SPI_QZS1_B` §3(2)) is a GENUINELY NEW mechanism — the satellite's own y-axis
  is FIXED to the orbit normal, never tracking the Sun at all.** Its own words: "+z pointing to the
  Earth. -y perpendicular to the orbital plane in the direction of the orbital angular momentum
  vector. +x completes the right hand system... (roughly oriented in the flight direction)." Mapped
  through the SAME 180° rotation, this becomes, in this tree's own convention: `z_body = -r_hat`,
  `y_body = n_hat` (the orbit normal), `x_body = -t_hat` — a closed-form construction taking NO Sun
  direction at all, verified right-handed (`x_body × y_body = z_body`) via the standard cyclic
  identity for this tree's own `OrbitTriad` (`t_hat × n_hat = r_hat`, proved from `t_hat = n_hat ×
  r_hat` by the BAC-CAB rule, `QZSY-A-001`). Its own tree-convention `x_body` is RETROGRADE
  (`-t_hat`), not prograde — QZSS's own "(roughly the flight direction)" describes its own NATIVE
  `x_qzss` (`=t_hat`, checked directly, before the frame map), a fact about a DIFFERENT axis than
  this tree's own `x_body` once remapped, not a contradiction of the source (found by an early
  version of this spec's own test asserting the wrong direction, caught by its own failing numbers).
  Built as `orbit_normal_attitude`, a GENERICALLY-named, PUBLIC function (the manager's own
  instruction) reusable for BeiDou's own future "zero-bias" mode — a DIFFERENT satellite's own sign
  convention, if built later, is that caller's own choice, not a change to this construction.
- **The mode-switch threshold (`SPI_QZS1_B` §3, "approx. 20°") is marked APPROXIMATE, deliberately
  no closed-form derivation the way GPS's/GLONASS-M's own onset angles are.** The manager's own
  ruling: the real switch is COMMANDED (a ground-operator decision, informed by beta but not a pure
  function of it), unlike GPS's/GLONASS-M's own rate-threshold onsets, which ARE closed-form
  consequences of a stated hardware rate. `kQzssBetaSwitchRad = 20°` is the source's own stated
  nominal figure, used as `qzss_yaw_attitude`'s own dispatch boundary, not re-derived from any
  underlying physical threshold.
- **The real-data control's own scope note, and the source basis for sharing a law across
  satellites — CHECKED DIRECTLY (the manager's own review, 2026-09-24), not assumed from QZS-1's
  own document alone.** `qzss_1()` (`SPEC-spacecraft.md`) builds QZS-1's own specific macromodel;
  QZS-1 itself (PRN J01) is absent from every file this session could reach (§3's own
  day-by-day-scan bullet below). Each of QZS-1/2/3/4's own SPI documents was fetched and read this
  round. **§2 (Reference Frame) is WORD-FOR-WORD IDENTICAL across all four** — quoted: "The QZS-N
  satellite coordinate system is aligned with the main body axes and originates at the center of
  the launch adapter plane... +Z... bore sight direction of the L-ANT antenna... +Y... parallel to
  the rotation axis of the solar panels... +X... constituted by a right handed system with +Y/+Z
  axis" — the frame mapping is safely constellation-wide, confirmed both by this quote and by the
  real-data match (`QZSY-R-004` below). **§3 (Attitude Law) is NOT uniformly shared — a finding,
  not an assumption.** QZS-1 switches between yaw-steering (`|beta|>~20°`) and orbit-normal
  (`|beta|<=~20°`). QZS-3's own words: "QZS-3 is CONTINUOUSLY controlled in the orbit normal mode"
  — ALWAYS orbit-normal, its own mode description word-for-word the SAME construction QZS-1's own
  document states (so `orbit_normal_attitude`'s own construction IS confirmed shared with QZS-3 by
  the source's own text). QZS-2 and QZS-4 instead "take always attitude of the yaw steering mode
  except the period that the orbit control maneuver is conducted" (both documents identical) —
  ordinary yaw-steering ALWAYS, using a DIFFERENT, rate-limited "pseudo-yaw-steering" correction
  near `beta=0` (their own stated formula, structurally like GPS's own noon/midnight catch-up ramp,
  NOT orbit-normal, NOT built in this tree — QZS-1-specific scope) rather than switching to
  orbit-normal the way QZS-1 and QZS-3 do. So `tools/orbex_qzss_check.cpp` checks the SHARED
  yaw-steering law and frame against QZS-2 (J02) and QZS-4 (J04) (the regime they actually operate
  in), and checks the orbit-normal construction, EXPLORATORY, against QZS-3 (J03) directly — not
  QZS-1's own specific macromodel, which this program does not touch.
- **Day-by-day scan for QZS-1, per the manager's own review — four sampled days is not a search.**
  ~Monthly sampling (45 dates, position-only, no quaternions read) across the ONLY archive this
  session could reach (a Swiss S3 mirror of CODE's own MGEX products, covering 2022-12-01 through
  2026-07-20) found QZS-1 (J01) present in ZERO of the 45 files — `any_qzss_records` confirms
  J02/J03/J04 correctly parse in every one of the same 45 files, so the absence is J01's own, not a
  scan defect. CODE's own true multi-year archive (`ftp.aiub.unibe.ch`) and CDDIS were both
  unreachable: direct FTP/HTTPS to AIUB's own server timed out (this environment's own network
  policy), and CDDIS redirects to an EarthData login this project's own standing no-account
  discipline does not cross — recorded as a genuine access limitation, not silently worked around.
  **RECORDED as the absence, per rule 4**: no window exists in the data this session can reach.
- **Orbit-normal mode checked against QZS-3 directly (EXPLORATORY, not the registered criterion) —
  found NOT a clean match, then DIAGNOSED (the manager's own second review): CODE's own analysis
  product does not implement it at all.** QZS-3 needs no low-beta window (it is ALWAYS in this
  mode), so `orbit_normal_attitude` was called directly (bypassing the beta-based dispatch) against
  QZS-3's own real attitude, 2023-10-07. First result: 170.9° off at 00:00, 13.5° off at 12:00; a
  30-minute sweep across the full day showed the error tracing a SMOOTH curve between the built
  construction and its own 180°-yaw-flipped counterpart, crossing near 90° twice and bottoming out
  at two closest approaches (6.56°, 6.69°) — never reaching the sub-0.03° floor every OTHER
  real-data control in this tree reaches, and NOT the clean, constant ~180°/~0° pattern a simple
  sign bug would produce (checked directly, not assumed). **The manager's own diagnosis, REGISTERED
  before being checked**: "CODE's J03 file is CODE's own MODEL of the satellite, not the satellite" —
  the mismatch's own SHAPE (independently verified: the angle between `nominal_yaw_steering`'s own
  x_body and `orbit_normal_attitude`'s own x_body, at a GEO's fixed beta, swings EXACTLY between
  `beta` and `180°-beta` over one orbit, confirmed to the thousandth of a degree at three test
  betas) is exactly what comparing two DIFFERENT laws at the SAME instants would produce. Compared
  CODE's own real J03 attitude against `nominal_yaw_steering` DIRECTLY instead (the SAME pipeline,
  the SAME frame mapping, L4's own already-gated code) — REGISTERED PREDICTION (stated before the
  comparison was run): if CODE models J03 with yaw-steering, the residual should sit at the
  pipeline's own usual floor, thousandths of a degree. **Confirmed exactly**: 0.00022° at 00:00
  (beta=6.45°), 0.00032° at 12:00 (beta=6.59°) — beta well inside QZS-1's own stated orbit-normal
  threshold (`|beta|<=~20°`), yet CODE still used yaw-steering. `orbit_normal_attitude`'s own
  construction is NOT contradicted by this data: the earlier mismatch is a comparison against a
  DIFFERENT law CODE happens to use for this satellite, not a defect. It remains independently
  verified correct algebraically (`QZSY-A-001`) and its own SHAPE confirmed shared with QZS-1 by
  QZS-3's own identical wording — but stays UNCONFIRMED by real-data agreement, since no analysis
  centre this session could reach appears to implement it for any satellite checked (`QZSY-Q-001`,
  narrowed to this precise basis; `QZSY-Q-004` CLOSED on this result).

---

## 4. Required behaviour

- **QZSY-R-001.** `qzss_yaw_attitude(r_gcrs_m, v_gcrs_m_per_s, sun_direction_gcrs) ->
  Result<Mat3, AttitudeError>`: dispatches to `orbit_normal_attitude` for `|beta| <=
  kQzssBetaSwitchRad` (20°, APPROXIMATE, §3), else `nominal_yaw_steering` directly.
- **QZSY-R-002.** `orbit_normal_attitude(r_gcrs_m, v_gcrs_m_per_s) -> Mat3` (never fails, no
  `Result`): the orbit-normal mode, §3's own closed-form construction (`z_body=-r_hat,
  y_body=n_hat, x_body=-t_hat`). Takes NO Sun direction — a genuine property of this law. Named and
  built GENERICALLY for reuse (the manager's own instruction).
- **QZSY-R-003.** Outside the switch, `qzss_yaw_attitude` returns EXACTLY what
  `nominal_yaw_steering` returns for the same `(r, sun)` — proved (§3), asserted as a standing
  regression guard (`QZSY-A-002`).
- **QZSY-R-004.** **Real-data control** (`tools/orbex_qzss_check.cpp`, reproducible on demand, NOT
  part of the automatic gate, the same treatment every other ORBEX control gets): CODE's own MGEX
  QZSS attitude, 2023-10-07 (DOY 280), two QZSS satellites (J02, J04), each at two epochs (four
  checks total), all in yaw-steering mode (beta 31°/−40°, no orbit-normal crossing found, §3).
  Predictions and a 2° relative criterion REGISTERED before the attitude quaternions were read.
  Result: all four matched, to 0.00003°–0.00019° — the tightest real-data agreement any attitude law
  in this tree has had, strong confirmation of the frame-mapping hypothesis (§3).
- **QZSY-R-005.** `qzss_yaw_attitude`'s own refusal (`QZSY-F-001`, forwarded unchanged as
  `ATTD-F-001`) is reached wherever `nominal_yaw_steering`'s own nadir-singularity guard would fire,
  in the yaw-steering branch only — `orbit_normal_attitude` never refuses.
- **QZSY-R-006.** Step 6's own two guards, adapted to this law's own STRUCTURE (a discrete
  mode-switch and a static orbit-normal construction, neither a smooth ramp the way GPS's/
  GLONASS-M's own turns are — `TYAW-A-012`/`TYAW-A-015`'s own literal ramp-rate shapes do not apply
  to a non-ramping law, so this spec adapts the UNDERLYING property each one protects rather than
  forcing a ramp onto a law that has none, stated explicitly, not silently reinterpreted):
  - **Mode-boundary correctness** (`QZSY-A-003`, `TYAW-A-012`'s own role — catching a wrong-direction
    or off-by-something threshold bug, adapted from "time" to "which mode" since there is no ramp to
    time): the dispatcher selects the correct mode on both sides of `beta=0` and near the ±20°
    boundary — shown firing on a deliberately reversed comparison, which disagrees with the real
    dispatcher at every geometry checked.
  - **Handedness/sign correctness** (`QZSY-A-001`, `TYAW-A-015`'s own role — catching a sign-flipped
    construction): `orbit_normal_attitude`'s own construction is independently reconstructed and
    matches exactly; a deliberately y-sign-flipped version produces the OPPOSITE (wrong) x_body
    direction, shown firing.

---

## 5. Interfaces, stated language-free

- `orbit_normal_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3) -> Mat3` — QZSY-R-002. Never fails.
- `qzss_yaw_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3) -> Result<Mat3, AttitudeError>` — QZSY-R-001.

A stateless provider, the same shape every other attitude function in this tree already is: every
quantity is computable from the CURRENT epoch's own state alone.

---

## 6. Precision and accuracy

- **QZSY-P-1.** No hardware rates or per-satellite parameters supplied by the caller: QZSS's own
  law, as `SPI_QZS1_B` states it, is fully closed-form and geometric (yaw-steering) or purely
  orbital (orbit-normal), needing nothing beyond the current state.
- **QZSY-P-2.** The real-data control's own four checks matched to 0.00003°–0.00019° — tighter than
  every other constellation's own real-data control in this tree by roughly an order of magnitude.
  Not investigated further for a specific cause; plausibly reflects QZSS's own slower orbital
  dynamics (a geosynchronous period, so the SP3's own 5-minute linear interpolation and betaheld-
  constant approximations both introduce less error per query than they do for GLONASS's or
  Galileo's own faster MEO orbits).

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `QZSY-F-001` | (forwarded, unchanged) `ATTD-F-001` — the Sun within `nominal_yaw_steering`'s own nadir-singularity tolerance, in the yaw-steering branch | the diagnostic `nominal_yaw_steering` itself states |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `QZSY-A-001` | `orbit_normal_attitude`, independently reconstructed from `SPI_QZS1_B`'s own words then mapped through the 180°-about-Z rotation, matches production exactly at four geometries; the result is orthonormal and right-handed; QZSS's own NATIVE x is prograde (checked directly); this tree's own tree-convention x_body is RETROGRADE (the verified, non-obvious relationship); **the guard shown firing**: a deliberately y-sign-flipped version produces the OPPOSITE x_body direction | exact match; orthonormal; native x prograde, tree x retrograde; broken version opposite sign | `SPI_QZS1_B` §3(2), read directly in the test | 1e-9 | R-002, R-006 |
| `QZSY-A-002` | outside the mode-switch, `qzss_yaw_attitude` returns EXACTLY what `nominal_yaw_steering` returns, at four geometries; the returned frame is orthonormal and right-handed | max component difference < 1e-9; orthonormal | `nominal_yaw_steering`, already gated | 1e-9 | R-003 |
| `QZSY-A-003` | **the guard shown firing**: the mode switch selects the correct mode at six geometries spanning both sides of `beta=0` and the ±20° boundary; a deliberately reversed threshold comparison disagrees with the real dispatcher at every one of the six | correct mode selected; broken comparison disagrees at all six | `SPI_QZS1_B` §3's own stated ~20° figure | — | R-001, R-006 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `QZSY-F-001` | Forwarded, unchanged, from `nominal_yaw_steering`'s own `ATTD-F-001` — already gated at its own source; re-testing adds no independent coverage the other three constellations' own identical forwarding pattern did not already establish. |
| `QZSY-R-004` | The real-data control is `tools/orbex_qzss_check.cpp`, deliberately NOT part of the automatic gate — no pinned data present in CI, reproducible on demand. |
| `QZSY-R-005` | Same reasoning as `QZSY-F-001` immediately above. |

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as part of the L5 step 3 section: the frame-mapping hypothesis (derived
  from the Sun-hemisphere property AND independently from the yaw-steering mode's own y-axis
  definition, two separate readings of the source agreeing) and its own real-data confirmation
  (four checks, two satellites, 0.00003°–0.00019°, the tightest in this tree); the orbit-normal
  mode's own closed-form derivation and its own right-handedness proof; the scope note explaining
  why the real-data control uses J02/J04 rather than QZS-1 itself; the orbit-normal mode's own
  real-data search across four days and its own non-result, reported not hidden; the "roughly the
  flight direction" mistaken test expectation, caught and corrected.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `QZSY-Q-001` | **Orbit-normal mode's own real-data confirmation — SOUGHT FURTHER TWICE (manager's own two review rounds, 2026-09-24), NOT CONFIRMED, on a NOW-PRECISE basis, not a puzzle.** QZS-1 (the satellite this mode is built for) is confirmed absent from every file this session could reach (a 45-date, ~monthly scan across the ONLY reachable archive's own full span, 2022-12 through 2026-07 — CODE's own true archive and CDDIS both unreachable, §3). QZS-3 shares this mode word-for-word by its own document and needs no low-beta window (always active) — checked directly, EXPLORATORY: NOT a clean match. DIAGNOSED (the manager's own second review): CODE's own real J03 attitude matches `nominal_yaw_steering` DIRECTLY to the SAME thousandths-of-a-degree floor every other yaw-steering check reaches (0.00022°/0.00032° at beta=6.5°, well inside the orbit-normal regime) — CODE's own analysis product simply does not implement QZS-3's own orbit-normal mode; the EXPLORATORY mismatch is explained, not a live discrepancy. The mode's own construction remains independently verified algebraically (`QZSY-A-001`) and its own shape confirmed shared with QZS-3 by the source's own text — genuinely unconfirmed by real DATA (no analysis centre this session could reach implements it for any satellite checked), not contradicted by any. |
| `QZSY-Q-002` | **The exact commanded date QZS-1's own operators switch modes remains undetermined** — no QZS-1 data was reachable at all (`QZSY-Q-001`), so this stays open. |
| `QZSY-Q-003` | **Only QZS-1's own macromodel is built (`SPEC-spacecraft.md`); QZS-2/3/4 and QZS-1R's own SPI documents were read this round for their own §2/§3 ONLY** (frame and attitude-law text, to check the source basis for sharing a law, `QZSY-R-004`'s own scope note) — their own §4 (mass/CoM) and §6 (geometry/optics) were NOT read, so no macromodel for any of them is built. Worth doing if L7 needs more than QZS-1. |
| `QZSY-Q-004` | **CLOSED (manager's own second review, 2026-09-24).** QZS-3's own real attitude, in CODE's own ORBEX solution, did not match `orbit_normal_attitude`'s own static construction, tracing a smooth day-periodic pattern (§3). DIAGNOSED, not left open: the manager's own registered prediction — that the mismatch's own shape is exactly what comparing `nominal_yaw_steering` against `orbit_normal_attitude` at a GEO's fixed beta produces (independently verified: the angle between the two swings EXACTLY between `beta` and `180°-beta`) — was checked directly and confirmed exactly (0.00022°/0.00032° residual against `nominal_yaw_steering`, the pipeline's own usual floor). CODE's own analysis product does not implement QZS-3's own orbit-normal mode at all; it models this satellite with generic yaw-steering regardless of beta. No further investigation needed on this specific question — resolved to "CODE doesn't model it," not "the construction might be wrong." |
