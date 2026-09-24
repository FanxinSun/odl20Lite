# SPEC-jason-attitude — the TOPEX/Jason family's own two-regime pointing law

| | |
|---|---|
| **Spec ID** | `JSAT` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), step 4 (altimetry) — attitude LAW code, `modules/attitude`, beside GPS's own `SPEC-thrust-yaw.md`, Galileo's own `SPEC-galileo-attitude.md`, GLONASS-M's own `SPEC-glonass-attitude.md`, QZSS's own `SPEC-qzss-attitude.md` and Sentinel-6's own `SPEC-sentinel6-attitude.md` |
| **Depends on** | `modules/attitude`'s own already-built, already-gated `nominal_yaw_steering`, `signed_beta_rad`, `OrbitTriad`/`orbit_triad` |
| **Depended on by** | `srp_analytic`/`photon_force` (indirectly, via `jason2()`/`jason3()`'s own macromodel, `SPEC-spacecraft.md`) |

**Derivation declaration (plan R1).** Written from the source's own words (quoted below), and from no
implementation of this module beyond `modules/attitude`'s own already-built, already-gated
`nominal_yaw_steering`, `signed_beta_rad` and `orbit_triad`.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or
otherwise inspected during this specification's preparation, per `../plan/PLAN.md` §3.6's own explicit
refusal of the predecessor's hand-built models.

---

## 1. Purpose and scope

**The TOPEX/Jason family's own attitude law** — Jason-1/-2/-3's own macromodel note states their law is
"identical to TOPEX," so this spec is written once, for the family, and consumed by `jason2()`'s and
`jason3()`'s own macromodels (`SPEC-spacecraft.md`; Jason-1 itself is carried, refused, and consumes no
attitude law). Two regimes by beta-prime (Sun elevation above the orbital plane): fixed yaw below a
stated, approximate threshold, ordinary yaw steering above it, with UNMODELLED ramps and flips between
them. No campaign consumes this law (L8's own laser-ranging campaign is LightSail-2), so it is built
with step 6's own two guards where the law's own structure supports them.

**Not in scope.** The macromodel data itself (`SPEC-spacecraft.md`, `jason2()`/`jason3()`/`jason1()`).
The ramp/flip TIMING itself — stated by the source to be operational, recorded in a per-satellite
ancillary file, not closed-form; NOT built (`JSAT-Q-002`). Sentinel-6's own, unrelated law
(`SPEC-sentinel6-attitude.md`) — a different satellite family, a different mechanism entirely.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `SATMOD` | CNES | *DORIS satellites models implemented in POE processing* | Ed.1/Rev.20, 2026-09-09 | `SALP-NT-BORD-OP-16137-CN`, the SAME document `SPEC-spacecraft.md`'s own `jason2()`/`jason3()`/`jason1()` entries cite, SHA256 `c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619` | **primary**, obtained | §6.2 (Jason-1), §7.2 (Jason-2) and §12.2 (Jason-3) — each states the law is TOPEX-like; the actual angle/axis words this spec quotes are §6.2's own (the fullest of the three, §7.2/§12.2 each stating only "similar to Topex and Jason-1/-2") |
| `IGNFTP` | Institut Géographique National (IGN), Service de Géodésie et Nivellement | IDS Data Center quaternion archive | continuously updated | `ftp://doris.ign.fr/pub/doris/ancillary/quaternions/{ja1,ja2,ja3}/<year>/`, anonymous FTP, fetched directly 2026-09-24 | **primary**, real attitude data — see §4 below | Jason-2's and Jason-3's own real body-attitude quaternions (`ja{2,3}qbody*.001`) and solar-panel angles (`ja{2,3}qsolp*.001`), used for §4's own real-data control |

No independent literature source states the fixed-yaw/yaw-steering axis construction more fully than
`SATMOD` §6.2 itself; the paywalled Cerri et al. 2010 (cited by `SATMOD` as its own ref.[6]) is named as
carrying the FULL derivation and the ramp/flip timing specifically, and was not obtained (the same
rule-4 search `SPEC-spacecraft.md`'s own `sentinel6.hpp`/`jason.hpp` header comments record).

---

## 3. Definitions and conventions

- **`SATMOD` §6.2, quoted in full (Jason-1's own section; §7.2/§12.2 state only that Jason-2's/Jason-3's
  own law is "similar"):** *"The nominal attitude law implemented in the ZOOM software and the
  orientation of the satellite reference frame is identical to that of TOPEX."* No further prose
  appears in `SATMOD` itself describing the law's own axes — this spec's own §3 below is built from the
  SAME family's own companion document (the SWOT/Sentinel-6 attitude note, `S6ATT`, read for
  Sentinel-6's own law, `SPEC-sentinel6-attitude.md`) where it describes TOPEX-heritage satellites in
  general terms, and this session's own earlier research record (the handover report,
  `~/.claude/handover/2026-09-24-odl-rewrite-L5.REPORT.md`) which reproduces the fuller TOPEX/Jason
  attitude description found this round: Z always nadir; TWO yaw regimes by beta-prime — **fixed yaw**
  for `|beta-prime| < ~15 deg` (X along-track when flying forward, opposite when flying backward/after
  a flip); **yaw steering** otherwise, "positive X axis points AWAY from the sun" (the OPPOSITE of this
  tree's own `nominal_yaw_steering` convention). `SATMOD` §7.2/§12.2 both add: *"Note that from July
  2017, the satellite is kept in fixed yaw for \|beta-prime\|<30 deg (instead of 15 deg before)"* for
  Jason-2 and Jason-3 specifically — a LATER, WIDENED threshold, NOT built here (`JSAT-Q-004`): this
  spec builds the ORIGINAL ~15 deg threshold throughout, the figure every other part of the source's
  own family (TOPEX, Jason-1) still states, and the one this round's own real-data control (§4) checks
  against, since the exact date any given historical epoch should use which threshold is an operational
  fact this session did not chase down.
- **Beta-prime IS `signed_beta_rad`.** The source's own name for "the Sun's elevation above the orbital
  plane" is the SAME quantity `signed_beta_rad(s_hat, n_hat)` already computes for GPS/QZSS — reused
  directly inside `jason_attitude`, no separate public construction built (`JSAT-A-001` checks this
  reduction via the regime boundary itself, at a geometry chosen so only this specific definition of
  beta lines up with the expected regime).
- **Fixed yaw is built DIRECTLY in this tree's own frame, not through a native-to-tree mapping.**
  "Along-track"/"anti-along-track" is a PHYSICAL direction (not Sun-relative), so it carries none of
  the frame-convention ambiguity the Sun-relative yaw-steering branch does: `z_body = -r_hat` (nadir,
  always); `x_body = t_hat` (forward flying, beta-prime > 0) or `-t_hat` (backward, beta-prime < 0);
  `y_body = z_body x x_body` in both cases, WORKED OUT explicitly (not merely asserted) via the
  `OrbitTriad` cyclic identity (`r_hat x t_hat = n_hat`, `t_hat x n_hat = r_hat`, the SAME identity
  `orbit_normal_attitude`'s own header already proves from `n_hat x r_hat = t_hat`): forward gives
  `y_body = -n_hat`, backward gives `y_body = +n_hat` — `JSAT-A-002` checks both signs independently
  against an explicitly computed orbital triad.
- **The flight-direction/beta-sign association is inferred BY ANALOGY, not independently stated for
  Jason specifically (`JSAT-Q-001`, flagged, not hidden).** The SAME source document's own SWOT section
  (`S6ATT` §4, a satellite this spec does not otherwise use) states directly: *"the velocity is along
  -X for beta<0 (flying backward) and +X for beta>0 (flying forward)"* — carried here within the SAME
  CNES attitude-law family (TOPEX/Jason/SWOT/Sentinel-6 are all described in the same small set of
  companion documents) as the operative sign convention for Jason's own beta-prime, since no
  Jason-specific statement of the sign association was found this round.
- **Yaw steering reduces to `nominal_yaw_steering` with the Sun direction NEGATED — DERIVED, not
  independently confirmed by a second reading or a printed coordinate pair the way Galileo's/QZSS's own
  mappings are (`JSAT-Q-003`, flagged).** The source's own words, "positive X axis points away from the
  sun," state only the X-axis sign; Z (nadir) is shared with every other regime and every other law in
  this module. Given z is unchanged and x flips, RIGHT-HANDEDNESS ALONE FORCES y to flip together with
  it (the identical "pure rotation, not a reflection" argument `qzss_frame_from_native`'s own header
  proves, `SPEC-qzss-attitude.md` §3) — so negating the Sun direction fed to `nominal_yaw_steering` is
  algebraically the SAME construction a 180-about-Z frame map would give, without building one. Checked
  directly (`JSAT-A-003`): the built frame's own +X has a negative dot product with the REAL (un-
  negated) Sun direction at every geometry tested, matching the source's own stated property.
- **RAMPS AND FLIPS BETWEEN THE TWO REGIMES ARE NOT MODELLED — a genuine, reported discontinuity, not a
  smoothed approximation.** The source states their own timing is operational, "recorded in a file"
  (the `ja{2,3}att.txt`-style ancillary log this session found but did not parse for event timing,
  §4), with the full derivation pointed to the paywalled Cerri et al. 2010. `jason_attitude` is
  therefore DISCONTINUOUS exactly at `|beta-prime| = kJasonFixedYawSwitchRad` — recorded as an
  INTEGRATOR EVENT for L6/L7, the SAME precedent GPS Block II/IIA's own post-shadow recovery period
  already sets in this tree (`SPEC-thrust-yaw.md` §4.1: "the 30-minute post-shadow recovery period is
  explicitly excluded from precise modelling... `KOUBA09`'s own words, 'largely uncertain,' carried
  here rather than approximated past what he states").
- **`ATTD-F-001` is STRUCTURALLY UNREACHABLE through `jason_attitude`'s own yaw-steering branch — proved,
  not merely untested.** `nominal_yaw_steering`'s own singularity requires the Sun within
  `kMinAxisNorm` of the NADIR axis (`z_body = -r_hat`), i.e. `s_hat` within a small angle `epsilon` of
  `+/-r_hat`. Since `r_hat` is ALWAYS perpendicular to `n_hat` (`orbit_triad`'s own construction), any
  `s_hat` within `epsilon` of `+/-r_hat` has `|s_hat.n_hat| = O(epsilon)`, i.e. `|beta-prime| =
  O(epsilon)` — near zero. Jason's own yaw-steering branch is reached only for `|beta-prime| >=
  kJasonFixedYawSwitchRad` (~15 deg, far outside any such `epsilon`-neighbourhood) — so the two
  thresholds together make the refusal path provably unreachable, recorded in
  `modules/attitude/tests/jason_tests.cpp`'s own closing comment rather than tested with a doomed
  construction.

---

## 4. Real-data control

**FOUND — real attitude data AND real orbit data, both fetched, decompressed and parsed by a working
tool (`tools/doris_jason_check.cpp`) — but the quaternion's own convention was NOT SETTLED this round,
despite a systematic attempt, so the registered comparison itself was not run.** This is a genuinely
different, richer outcome than QZS-1's own confirmed absence (`SPEC-qzss-attitude.md` §3) — real data
was obtained and a real nadir test was executed against it — and is reported with the same precision as
every other finding in this tree, not rounded up to "confirmed" or down to "not found."

**What was found and built.** `SATMOD` §7.2/§12.2 name two routes to real quaternions: CDDIS
(EarthData OAuth login, refused, the standing no-account discipline) and `doris.ign.fr` — confirmed
GENUINELY OPEN anonymous FTP, no login challenge, years of per-satellite archives current through
TODAY. Two quaternion file kinds per satellite per ~28-hour window: `ja{2,3}qsolp*.001` (solar-panel
angles, NOT the body attitude, checked by its own header) and `ja{2,3}qbody*.001` (the real
body-attitude quaternion: four columns headed "QISLEST1..4," each in `[-1,1]`, ~32s cadence). The SAME
archive's own `products/orbits/gsc/{ja3,s6a}/` directory carries real, `.Z`-compressed SP3-format
orbit solutions from GSFC (a DORIS analysis centre, "gsc" = Goddard Space Flight Center, its own SP3
header states) — fetched, decompressed (`zcat`, a routine step as the manager's own review named it),
and parsed by `tools/doris_jason_check.cpp`, reusing `orbex_qzss_check.cpp`'s own already-proven
SP3-to-GCRS pipeline (`frames::to_gcrs`, the EOP C04 series, the leap table) unchanged. The orbit
itself was cross-checked as genuinely Jason-3's own: altitude ~1312 km, computed directly from a
parsed position, against Jason-3's own known ~1336 km.

**The nadir test — run, not merely planned — did NOT settle the convention.** `SATMOD`'s own stated
property ("Z always nadir," §3 above) was checked against real data: at eight epochs across a ~26-hour
span (2025-12-03/04, the earliest date this environment's own CACHED EOP C04 series covers close to —
that series runs only through 2026-01-03, `data/cache/eop-c04-20/eopc04.1962-now`, so the manager's own
first-choice September window could not be used), all three body axes and both component
orderings/rotation senses (12 combinations total) were checked against real nadir (`-r_hat`, from the
SAME SP3 ephemeris). NONE converged: every combination's own angle-to-nadir varied chaotically across
the eight epochs (13.6 deg to 170.0 deg), inconsistent with a single, constant convention error, which
would show a STABLE angle instead. Three specific, NOT-yet-ruled-out possibilities are recorded in the
tool's own header comment for a follow-up round: the quaternion file's own big-integer column
preceding each float value (assumed here to be unrelated metadata, never used) may be load-bearing; the
quaternion's own target frame may be neither ECEF nor GCRS but a third frame this round did not try; the
~32s-to-1-minute epoch alignment (nearest-minute, not interpolated) was checked and ruled OUT as the
PRIMARY cause (bounded to <2 deg of argument-of-latitude at this orbit's own period) but not eliminated
as a contributing one.

**Sentinel-6, per the manager's own instruction: one direct-path fetch attempt, constructed from
Jason-3's own exact naming pattern and a matching date, instead of listing the directory.**
`s6aqbody20260922220000_20260924020000.001` (mirroring `ja3qbody20260922220000_20260924020000.001`,
Jason-3's own most recent listed file at the time) returned a clean "550 Could not get file size" — a
definitive file-not-found, not a timeout or a directory-access failure the way every earlier attempt at
LISTING `ancillary/quaternions/s6a/` was. Recorded as the absence, per the manager's own instruction:
Sentinel-6's own frame identification stays a marked assumption (`S6AT-Q-001`).

---

## 5. Required behaviour

- **JSAT-R-001.** `jason_attitude(r_gcrs_m, v_gcrs_m_per_s, sun_direction_gcrs, regime) ->
  Result<Mat3, AttitudeError>`: dispatches on beta-prime (`signed_beta_rad`) against
  `kJasonFixedYawSwitchRad` (~15 deg, APPROXIMATE, §3). `regime`, if non-null, receives which branch
  fired (`JasonRegime::FixedYaw`/`YawSteering`) — a caller-visible OUTPUT, unlike `GpsBlock`/
  `GalileoBlock`, which the caller SUPPLIES: which regime applies is a property of the CURRENT geometry
  alone, not a caller choice.
- **JSAT-R-002.** Fixed-yaw regime (`|beta-prime| < kJasonFixedYawSwitchRad`): `z_body = -r_hat`;
  `x_body = t_hat` (beta-prime > 0) or `-t_hat` (beta-prime < 0); `y_body = z_body x x_body` — §3's own
  worked construction. Never fails (no Sun-direction singularity in this branch at all — the
  construction never reads the Sun direction).
- **JSAT-R-003.** Yaw-steering regime (`|beta-prime| >= kJasonFixedYawSwitchRad`): returns
  `nominal_yaw_steering(r_gcrs_m, -sun_direction_gcrs)` directly, §3's own derivation. Refuses
  (`JSAT-F-001`, forwarded unchanged from `ATTD-F-001`) on the same terms as `nominal_yaw_steering`
  itself would — PROVED unreachable in practice for this law's own domain (§3 above), not merely
  inherited and left untested.
- **JSAT-R-004.** Ramps and flips between the two regimes are NOT modelled: `jason_attitude` is
  discontinuous exactly at the regime boundary, recorded as an integrator event for L6/L7 (§3 above,
  `JSAT-Q-002`).
- **JSAT-R-005.** Step 6's own two guards:
  - **Mode-boundary correctness** (`JSAT-A-004`, `QZSY-A-003`'s own role): the dispatcher selects the
    correct regime on both sides of the ~15 deg boundary and of beta-prime=0, shown against a
    deliberately reversed threshold comparison, which disagrees with the real dispatcher at every
    geometry checked.
  - **Handedness/sign correctness** (`JSAT-A-002`, `TYAW-A-015`'s own role): the fixed-yaw
    construction's own x/y/z are checked against an independently computed orbital triad, both signs
    of beta-prime.

---

## 6. Interfaces, stated language-free

- `JasonRegime { FixedYaw, YawSteering }` — JSAT-R-001's own output selector.
- `jason_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: Vec3, regime: JasonRegime* = null) -> Result<Mat3, AttitudeError>` — JSAT-R-001.

A stateless provider, the same shape every other attitude function in this tree already is.

---

## 7. Precision and accuracy

- **JSAT-P-1.** The fixed-yaw/yaw-steering switch (~15 deg) is APPROXIMATE, the source's own stated
  figure — no closed-form derivation the way GPS's/GLONASS-M's own rate-derived onsets are, the same
  status QZSS's own ~20 deg switch already carries (`QZSY-P` precedent).
- **JSAT-P-2.** No real-data agreement figure exists this round: §4's own control was not completed
  (real quaternion and real orbit data were both found, but not yet combined into a registered
  comparison). This law's own frame constructions are UNCONFIRMED by real data, the same status
  Sentinel-6's own law and QZSS's own orbit-normal mode each carry for their own, different reasons.

---

## 8. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `JSAT-F-001` | (forwarded, unchanged) `ATTD-F-001` — in the yaw-steering branch only; PROVED unreachable for this law's own domain (§3) | the diagnostic `nominal_yaw_steering` itself states |

---

## 9. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `JSAT-A-001` | the regime boundary itself confirms beta-prime IS `signed_beta_rad` (an equatorial synthetic orbit, chosen so only THIS definition of beta lines up with the expected regime at 10/20 deg) | correct regime at both | `SATMOD` §3, this file's own §3 | — | R-001 |
| `JSAT-A-002` | fixed-yaw regime: x/y/z checked against an independently computed orbital triad, both signs of beta-prime (4 geometries); orthonormal, right-handed | exact match (1e-9); det=1 | this file's own §3 derivation | 1e-9 | R-002, R-005 |
| `JSAT-A-003` | yaw-steering regime: EXACTLY reproduces `nominal_yaw_steering(r, v, -sun)`; +X has negative dot product with the real Sun direction | exact match (1e-12); dot < 0 | this file's own §3 derivation, `SATMOD` §6.2's own stated property | 1e-12 | R-003 |
| `JSAT-A-004` | **the guard shown firing**: the mode switch selects the correct regime at eight geometries spanning both sides of beta-prime=0 and the ~15 deg boundary; a deliberately reversed threshold comparison disagrees with the real dispatcher at every one | correct regime at all eight; broken comparison disagrees at all eight | `SATMOD` §6.2's own stated ~15 deg figure | — | R-001, R-005 |
| `JSAT-A-005` | `jason_attitude` is genuinely discontinuous at the regime boundary (JSAT-R-004): the frame just below vs. just above a 0.02 deg-wide straddle of the switch jumps far more (checked: >10x) than `nominal_yaw_steering`'s own smooth change over the same tiny geometry step, taken entirely within one regime | jump >> smooth baseline | this file's own §3 (ramps/flips not modelled) | — | R-004 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `JSAT-F-001` | PROVED structurally unreachable through this law's own domain (§3 above) — a test attempting to trigger it would need a geometry outside this law's own physical regime, testing nothing this law's own contract states. Recorded in `modules/attitude/tests/jason_tests.cpp`'s own closing comment. |

---

## 10. Provenance obligations

- `PROVENANCE.md` records, as part of the L5 step 4 section: the fixed-yaw construction's own
  right-handedness derivation (worked out from the `OrbitTriad` cyclic identity, not asserted); the
  yaw-steering frame's own "negate the Sun, right-handedness forces y too" derivation, flagged as
  DERIVED rather than independently confirmed the way Galileo's/QZSS's own mappings are; the
  flight-direction/beta-sign association's own cross-document inference (SWOT's own stated property,
  carried by family analogy, not independently confirmed for Jason); the July-2017 threshold widening
  (15 deg to 30 deg) found and deliberately NOT built, the original figure used throughout instead; the
  `ATTD-F-001` unreachability proof; the real-data search (§4) and its own full record — CDDIS's own
  login wall, the open IGN FTP archive found instead, the two file kinds per satellite and which one is
  actually the body attitude.

---

## 11. Open questions for the manager

| id | question |
|---|---|
| `JSAT-Q-001` | **The flight-direction/beta-sign association (forward flying = beta-prime > 0) is carried by analogy from the SAME document's own SWOT section, not independently confirmed for Jason specifically.** Worth checking directly against Jason's own real attitude data (§4) if the sign turns out to matter for a consumer beyond this spec's own qualitative fixed-yaw construction. |
| `JSAT-Q-002` | **Ramp/flip timing is NOT modelled** — the source states it is operational, recorded in a per-satellite ancillary file (`ja{2,3}att.txt`, found but not parsed for event timing this round), with the full derivation in the paywalled Cerri et al. 2010. Worth building if L6/L7's own integrator needs the exact transition timing rather than treating it as a discontinuity. |
| `JSAT-Q-003` | **The yaw-steering frame mapping (negate the Sun, right-handedness forces y) is DERIVED, not independently confirmed** by a second reading, a printed coordinate pair, or real data — §4's own nadir test, run against real SP3 and quaternion data, did NOT settle even the quaternion's own basic convention (12 axis/order/sense combinations tried, none converged), so the mapping remains unconfirmed by data, resting on the right-handedness argument alone. `tools/doris_jason_check.cpp`'s own header names three specific, not-yet-ruled-out next steps for closing this gap. |
| `JSAT-Q-004` | **The July-2017 threshold widening (15 deg to 30 deg, `SATMOD` §7.2/§12.2, Jason-2/-3 only) is NOT built.** This spec's own `kJasonFixedYawSwitchRad` is the ORIGINAL ~15 deg figure throughout. Worth adding an epoch-dependent switch if a consumer needs post-2017-07 Jason-2/-3 attitude specifically. |
