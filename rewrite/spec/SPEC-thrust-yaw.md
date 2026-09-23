# SPEC-thrust-yaw — GPS eclipse-season yaw attitude and antenna thrust

| | |
|---|---|
| **Spec ID** | `TYAW` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.0 |
| **Date** | 2026-09-24 |
| **Layer** | L4 `forces-analytic`, step 6 (`../plan/PLAN.md` §3.5, `../plan/subplan_L4/L4-6.md`) |
| **Depends on** | `core`, `time`, `frames`, `ephemerides`, `attitude`, `dynamics`, `macromodel` |
| **Depended on by** | `srp`/`erp` (a real, non-ideal attitude at eclipse epochs), L5 (population), L8 (the GNSS arc-fit gate) |

**Derivation declaration (plan R1).** Written from the documents in §2 and from no implementation
of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or repo-root `PROVENANCE.md` was opened, read,
listed, searched or otherwise inspected during this specification's preparation.

---

## 1. Purpose and scope

Two things a real GNSS satellite does that the ideal nominal yaw-steering law (`odl::attitude`,
L4 step 5) cannot represent, plus the force `PHPR-R-004a`'s panel geometry exists to feel properly:

1. **Non-ideal yaw attitude near noon and midnight.** The ideal law's own required yaw rate
   diverges as *β* (the Sun's elevation above the orbital plane) → 0 at the orbit's noon or
   midnight point (`SPEC-photon-pressure` §4.2's own stated boundary). A real spacecraft's
   momentum wheels have a maximum hardware yaw rate; below a *β*-dependent threshold the vehicle
   cannot keep up and flies a **different, constellation- and block-specific law** instead. This
   step adds that law as a **provider** to the `odl::attitude` module step 5 created — it does not
   rebuild the ideal law, which stays the frame every provider hands off to and back to.
2. **Antenna thrust.** The recoil of the radiated navigation signal, a small, steady acceleration
   along the antenna boresight (`+z_body`, the same boresight direction `SPEC-macromodel`'s own
   body frame already names).

**This step builds GPS.** The frozen GNSS baselines this tree must reproduce (`oracle/cases.tsv`
`G-01`…`G-03`, `oracle/ORACLE.md` §6) are GPS only, and are **cannonball fits that consume no
attitude law at all** — recorded there 2026-09-24, after this step's own search found it, not
assumed. So this step's own gate is not "reproduce the frozen residuals" (nothing consumes this
law to reproduce), it is **published turn behaviour matched on its own terms** — §4's derived
relations, each checked against a printed number, and observed attitude where one is openly
published. Building this now serves `SPEC-photon-pressure`'s own box-wing modelling (a real panel
needs a real, non-ideal attitude at exactly the epochs its own force is largest relative to the
ideal case) and is a capability the frozen baselines do not themselves need.

**Not in scope:**

- **Galileo, GLONASS and BeiDou yaw laws.** Real, findable sources exist for each (§2's own
  carried-forward table) and are **carried**, not dropped — recorded here so a later step does not
  re-run this step's own search. Not built because no frozen gate this tree has needs them yet
  (`L5`'s and `L8`'s own gates are GPS too, `../plan/subplan_L4/L4-6.md`'s own 2026-09-24 entry).
- **GPS Block I/II/IIA's own shadow-crossing recovery detail beyond Kouba's own stated
  approximation.** Kouba 2009 itself calls the post-shadow recovery period "largely uncertain" and
  excludes it from precise modelling (§4.1). Carried at the same footing Kouba leaves it.
- **The true GPS Block IIIA law.** A 2023 source exists and is **located, not read in full**
  (§2's own carried-forward table) — this step models IIIA with the IIF law instead, named as the
  same stopgap IGS analysis centres without a IIIA model use (§4.4), because no frozen gate
  distinguishes IIIA from IIF-as-IIIA (`oracle/ORACLE.md` §6, `../plan/subplan_L4/L4-6.md`).
- **Per-satellite transmit power.** This step builds the antenna-thrust force **law** —
  *P*/*c* along the boresight, directivity stated — with *P* a caller-supplied value, stated in
  each test. Which watts belong to which SVN is population data, L5's job (`DYN-Q-001`'s own
  L4/L5 split, already used for the macromodel itself). The IGS satellite metadata SINEX (§2) is
  where L5 finds it.
- **Antenna gain pattern / directivity beyond a stated upper bound.** The radiated power leaves
  through a real, non-uniform antenna array (`SPEC-macromodel`'s own eventual concern, if ever
  needed) — this step's own force law states *P*/*c* as an UPPER BOUND on the net recoil, not its
  exact value (`TYAW-R-005`, `TYAW-P-5` quantifies the gap and why it is not modelled), the same
  footing `SRPA-R-001`'s own generalisation once stood on before `PHPR-R-010` extended it.

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `KOUBA09` | Kouba, J. | *A simplified yaw-attitude model for eclipsing GPS satellites* | GPS Solut. 13:1–12, 2009, doi:10.1007/s10291-008-0092-1 | `http://acc.igs.org/orbits/yaw-attitude_kouba_gpssoln09.pdf` | **primary** | GPS Block II/IIA and IIR/IIR-M nominal, noon-turn and shadow-crossing yaw laws, in full, with worked equations (`TYAW-R-001`/`R-002`) |
| `DIL10` | Dilssner, F. | *GPS IIF-1 satellite: antenna phase center and attitude modeling* | Inside GNSS 5(5):59–64, September 2010 | `https://www.insidegnss.com/auto/sep10-Dilssner.pdf` | **primary** | GPS Block IIF measured yaw rates (noon 0.11°/s, shadow/midnight 0.06°/s), the *β* < 8° eclipse-yaw threshold, turn durations (`TYAW-R-003`) |
| `MSGA15` | Montenbruck, O., Schmid, R., Mercier, F., Steigenberger, P., Noll, C., Fatkulin, R., Kogure, S., Ganeshan, A.S. | *GNSS satellite geometry and attitude models* | Adv. Space Res. 56:1015–1029, 2015, doi:10.1016/j.asr.2015.06.019 | `https://elib.dlr.de/97732/1/ASR_151015_GNSS_SatGeomAtt.pdf` | **primary** | the shared body-frame/yaw-steering-frame convention (`SPEC-photon-pressure` already cites its Eq. 4/5, `PHPR-R-004`); the GPS block survey (Fig. 4) confirming which laws apply to which block, and its own citations into `KOUBA09` and `DIL10` for eclipse detail it does not itself carry |
| `IGSMETA` | Steigenberger, P., Montenbruck, O. (maintainers); IGS | *IGS Satellite Metadata (SINEX)*, release 2434 | continuously updated; this pin 2026-09-24 | `https://files.igs.org/pub/station/general/igs_satellite_metadata.snx`, described by `Metadata_SINEX_1.10.pdf` at the same host | **primary**, provenance-only this step | the fact G01 = SVN 63 = Block IIF and G05 = SVN 50 = Block IIR-M, in February 2023 (`SATELLITE/IDENTIFIER`, `SATELLITE/PRN` blocks) — used here only to name which law each baseline satellite flies, not consumed as force-model data; becomes a real, licence-checked manifest dependency at L5, when its `SATELLITE/TX_POWER` block is population data a force actually reads |

**`eclips.f` (Kouba's own Fortran implementation of `KOUBA09`, later extended by the same author to
IIF, GLONASS, Galileo and BeiDou) is deliberately NOT in this table.** It is **code**, its own
licence is not established (`COPYRIGHT GEODETIC SURVEY DIVISION, 2011. ALL RIGHTS RESERVED`,
printed in the file itself, with no further terms found), and plan §5 constraint 3's own gate
(`tools/literaturecheck.py`) exists exactly to keep unlicensed code out of what this tree builds
from — the same boundary `SPEC-integrators`'s own RKF7(8) tableau observed by implementing from
the *published coefficients*, never a reference solver's source. `eclips.f` is used in this
specification's own preparation **only to cross-check numbers already sourced from `DIL10`**
(its own Sep-2011 changelog entry independently names the 0.06°/s shadow-crossing rate `DIL10`
measured; its own Feb-2017 entry names the −0.7° IIF yaw bias, attributed there to Kuang et al.
2016, a value `DIL10`'s own 2010 text predates) — never read as an implementation template. Every
requirement below cites `KOUBA09`, `DIL10` or `MSGA15` for its own law; `eclips.f` is
named only where a number it carries independently confirms one of those sources, so a future
implementer knows the cross-check exists without being tempted to open the file as a starting
point.

**Carried forward, not built this step, sources located and recorded so this search is not
repeated:**

| constellation / block | what the search found | why not built now |
|---|---|---|
| Galileo | The operator's own official metadata, `https://www.gsc-europa.eu/support-to-developers/galileo-satellite-metadata` (ESA/GSC) — states the FOC yaw law explicitly, including a "modified" law switching in below 4.1° Sun elevation, and a separate IOV formula | No frozen gate needs it (§1) |
| GLONASS-M | Dilssner, F., Springer, T., Gienger, G., Dow, J. (2011), *The GLONASS-M satellite yaw-attitude model*, Adv. Space Res. 47:160–171 — the constellation's own official documents are signal ICDs, not an attitude model; this is the reverse-engineered standard the field uses instead, closed copyright | No frozen gate needs it; not an official-operator document despite the plan's own original wording assuming one existed |
| BeiDou | China Satellite Navigation Office, *BD 420025-2019*, "Compass/GNSS satellite precision application parameters and definitions" (2019-11-07), mirrored at `https://ilrs.cddis.eosdis.nasa.gov/docs/2019/BeiDou_MetaData_191201.cn.en.pdf` — a genuine official document, with a light-pressure-parameters section and an attitude-control-mode section (not yet read past its own table of contents); Yang, Guo & Zhao, arXiv:2112.13252, cites the same standard **and** reports CAST/SECM satellites do not fully comply with it | No frozen gate needs it |
| GPS Block IIIA (the true law, not the IIF stopgap `TYAW-R-004` uses) | Dilssner, F., Springer, T., Gini, F., Schönemann, E., Enderle, W. (2023), *New type on the block: generating high-precision orbits for GPS III satellites*, GPS World, 15 May 2023 (a free trade magazine, located, not yet fetched in full), with a companion paper, *Improved attitude modeling for GPS III satellites in the eclipse season* (DLR-hosted and Adv. Space Res., located, not yet fetched) | Stated in the search's own summary to resemble Block IIR's law but with a smaller maximum yaw rate and an earlier maneuver onset — a real, different law, not yet read closely enough to implement |

---

## 3. Definitions and conventions

- **β, the Sun elevation above the orbital plane, and μ, the orbit angle from midnight** —
  `KOUBA09`'s own symbols, identical in meaning to `SPEC-photon-pressure`'s own *β*₀/Δ*u*
  (`PHPR-A-011`), *not* identical in symbol: this spec keeps `KOUBA09`'s own μ (orbit angle
  measured from **midnight**, μ = 0° there) rather than photon-pressure's own Δ*u* (measured from
  the **Sun's own projection**, Δ*u* = 0° there) because every equation below is transcribed
  directly from `KOUBA09`, and silently renaming its own variable invites a transcription error
  rule 8 exists to avoid. The two are related by a constant 180° offset at fixed β, not restated
  as a conversion here since no computation in this spec needs both at once.
- **ψ, the yaw angle** — the angle between the body **+x**-axis and the along-track (velocity)
  direction, `KOUBA09`'s own §"New yaw-attitude model" (his Eq. 4-ish text) and `MSGA15`'s own
  §2.4 Fig. 1 (there called ψ for the same quantity, body-fixed x vs. transverse **e**_T).
- **μ̇, mean orbital angular velocity** — GPS's own value, `KOUBA09`'s own stated 0.00836°/s
  (orbital period ≈ 11ʰ58ᵐ). Not GLONASS's or any other constellation's own value; this spec is
  GPS-only (§1), so no per-constellation table is needed here the way `MSGA15`'s own Table 1 (a
  foreign spec's own concern) carries one.
- **R, the block's own maximum hardware yaw rate**, and **β₀, the turn-onset threshold** — related
  by tan β₀ = μ̇/R (`KOUBA09`'s own Eq. 7), the **derived relation** `TYAW-P-1` below checks
  against every block's own printed β₀, rather than trusting either number transcribed alone.
- **Yaw bias, *b*** — a small, constant, block-specific offset added to the nominal yaw angle
  during shadow passage, disambiguating the direction of an otherwise-symmetric 180° flip
  (`KOUBA09`'s own §"New yaw-attitude model": +0.5° for II/IIA since November 1995; this spec's
  own `TYAW-R-003` states IIF's own −0.7°, `DIL10` cross-checked against `eclips.f`'s own
  changelog, §2).
- **Body frame** — unchanged from `odl::attitude`'s own convention (`ẑ_body` = nadir, i.e. `−r̂`)
  and from `SPEC-photon-pressure`'s own kernel: this spec's own laws all resolve to the SAME
  `Mat3` (GCRS→body) type `nominal_yaw_steering` already returns, only computed by a different
  rule near a turn. No new frame is introduced.
- **The time argument.** Every quantity above (β, μ, ψ, and hence the returned frame) is a
  function of the epoch `t` alone, given the satellite's own GCRS position and velocity and the
  Sun's own GCRS direction at that same `t` — **not of any remembered previous call.** §4.6 states
  the one exception this project's own sources found (a rare sign ambiguity at β ≈ 0°) and why it
  is not modelled this round.

## 4. Required behaviour

### 4.1 GPS Block II/IIA (`KOUBA09`)

- **TYAW-R-001.** Nominal yaw angle ψ_n = ATAN2(−tan β, sin μ). Nominal yaw rate
  ψ̇_n = μ̇ tan β cos μ / (sin²μ + tan²β). Hardware yaw rate *R* is per-satellite (`KOUBA09`'s own
  Table 1, mean 0.0838–0.1280°/s across the fleet he measured; this spec takes a single stated
  test value per `TYAW-A-001`, not the per-SVN table, since no per-SVN population exists at this
  layer — `DYN-Q-001`'s own L4/L5 split again). Below the resulting β₀ (`TYAW-P-1`), a noon turn
  (μ near 180°) or, for II/IIA specifically, a shadow-crossing maneuver (whenever the satellite is
  eclipsed, regardless of β, since the solar sensor loses the Sun) replaces the nominal law:

  - **Noon turn**: ψ(t) = ATAN2(−tan β, sin μ(t_s)) + SIGN(R, ψ̇_n(t_s))·(t − t_s), for t > t_s,
    until ψ(t) catches up with ψ_n(t) (`KOUBA09` Eq. 15).
  - **Shadow crossing**: a spin-up phase at yaw acceleration RR (`KOUBA09`'s own stated
    0.00165–0.00180°/s² for II vs. IIA) up to rate *R*, then constant-rate rotation, both signed
    by the permanent yaw bias *b* = +0.5° (`KOUBA09` Eq. 21/22) — the shadow entry/exit angles
    themselves from the Earth's own angular radius as seen from the satellite, *E*_sh = *R*_E/*r*_s
    (point-source; `KOUBA09` Eq. 17), widened to 13.5° to include the penumbra (`KOUBA09`'s own
    stated figure, attributed there to Bar-Sever 1996's own ~0.5° penumbra-width estimate — the
    same "the Sun is a disc, not a point" reasoning `SPEC-shadow`'s own `SHDW-A-004` tests for a
    different shadow model, not the same source).
  - The 30-minute post-shadow recovery period is **explicitly excluded from precise modelling** —
    `KOUBA09`'s own words, "largely uncertain," carried here rather than approximated past what he
    states (§1's own **Not in scope**).

### 4.2 GPS Block IIR / IIR-M (`KOUBA09`)

- **TYAW-R-002.** Same shape as `TYAW-R-001`, with the body **x**-axis convention reversed (IIR's
  own hardware has **+x** away from the Sun, not toward it): ψ_n = ATAN2(tan β, −sin μ), noon-turn
  formula with the same sign reversal (`KOUBA09` Eq. 16). Hardware yaw rate a single constant
  0.20°/s across the whole block (`KOUBA09`'s own stated figure, not block-mean like II/IIA's).
  **No shadow-crossing law**: `KOUBA09` states plainly that IIR satellites maintain the nominal yaw
  attitude even through shadow, "as if [the attitude control] saw through the Earth" (his own
  words, attributed to a private communication confirming the shadow-yaw-control mode IGSMAIL-1653
  once described is no longer in use) — so IIR's own eclipsing regime is *only* the noon and
  midnight turns, both using the noon-turn formula shape (`KOUBA09`'s own "midnight turn (for
  Block IIR only)").

### 4.3 GPS Block IIF (`DIL10`)

**`DIL10`'s own midnight-turn passage, verbatim, no ellipses (`InsideGNSS`, September 2010,
pp. 62–63, "GPS IIF-1: A Bad Attitude?"):**

> We found that the Block IIF-1 satellite, when passing through the Earth's shadow, behaves to a
> certain extent like a Block IIR vehicle. That means that the satellite is basically able to keep
> its nominal yaw-attitude even in the absence of sunlight.
>
> Initial comparisons between estimated and nominal yaw angle values have shown that the accuracy
> the spacecraft maintains its nominal yaw-attitude with during shadow crossings is better than
> ±3 degrees (RMS). However, this only holds as long as the elevation β of the Sun is greater than
> 8 degrees. If the craft enters the Earth's umbra at a β-angle smaller than 8 degrees, we clearly
> notice a linear drift in the estimated yaw angle (Figure 7).
>
> The slope of a straight line fit tells us that the satellite is now rotating around its z-axis
> ("yaw-axis") with a nearly constant rotation rate of 0.06 degree/second. The yaw angle catches
> up with the nominal yaw angle towards the end of the Earth's shadow. As evident from Figure 7
> (right), a short post-shadow maneuver might be needed in case the actual yaw attitude upon
> shadow exit differs from the required nominal yaw attitude.
>
> Compared to the Block II/IIA and Block IIR satellites that feature maximum hardware yaw rates of
> 0.10-0.13 degree/second and 0.20 degree/second, respectively, the yaw-motion of the Block IIF-1
> spacecraft during its Earth's shadow passage is surprisingly slow and consequently results in a
> relatively long-lasting maneuver. The duration of the maneuver increases as the β-angle
> decreases. A complete half turn, required under the condition that the Sun lies exactly in the
> satellite's orbital plane (β = 0°), lasts about 55 minutes.
>
> However, the rotation rate we found for the midnight-turn maneuver is apparently not the
> maximum hardware rate of the Block IIF-1 spacecraft, as the evolution of the yaw angle at the
> other side of the orbit reveals (Figure 8). We found that for a β-angle below 4 degrees, the
> satellite is rotating with a nearly constant rate of R = 0.11 degree/second in order to
> accomplish its required yaw-flip at orbit noon. In consequence, the noon-turn maneuver goes
> twice as fast as the midnight-turn maneuver, that is, it "only" lasts about 27 minutes at most.
>
> During the noon-turn and the midnight-turn maneuvers, the actual yaw angle may deviate from the
> nominal one by up to ±180 degrees and ±90 degrees, respectively.

**`MSGA15` searched specifically for an independent IIF eclipse description** (its own §3.1, GPS):
it states only "Specific aspects of the IIF attitude control law during the eclipse season are
likewise discussed in Dilssner (2010)" — a citation, not an independent description. `MSGA15`
carries no IIF shadow-law content of its own; the search confirms silence rather than assuming it.

**`DIL10`'s own two printed numbers do not both fit one simple constant-rate law, and the text
does not say which one to keep** (found on review, not assumed from either number alone). Two
candidate shapes both match "a nearly constant rotation rate":

- **Shape F** (this module's own noon/midnight-turn mechanism, `TYAW-R-002`'s own shape, applied
  here with IIF's own night rate): follow the nominal law until its own rate first reaches
  0.06°/s (the β₀-threshold onset, `TYAW-P-1`), then a CONSTANT 0.06°/s ramp until it catches up
  with the nominal law again (`evaluate_turn`'s own stateless characterisation, §4.6). At β = 0°
  exactly this predicts a duration of 180°/0.06°/s = 3000 s = **50.0 min** — `DIL10`'s own "about
  55 minutes" is 10% longer.
- **Shape E** (one constant rate for the WHOLE shadow passage): the satellite is actually eclipsed
  from shadow entry to shadow exit — a fixed geometric window, *E*_sh = 13.5° in place of β₀
  (`TYAW-R-001`'s own shadow-crossing geometry) — and rotates at the SINGLE rate that carries it
  from the nominal angle at entry to the nominal angle at exit over that window's own duration:
  rate = (nominal angle at exit − nominal angle at entry, unwrapped) ÷ (exit time − entry time).
  At β = 0° exactly this predicts a duration of 2·asin(*R*_E/*a*)/μ̇ = **55.4 min** (*R*_E =
  6378.137 km, *a* ≈ 26 561 km) — matching `DIL10`'s own "about 55 minutes" closely — but the
  RATE that duration implies, 180°/55.4 min = **0.054°/s**, is 10% below `DIL10`'s own printed
  "0.06 degree/second."

  So each shape reproduces one of `DIL10`'s own two numbers to about 1% and misses the other by
  about 10%; neither is a clean fit to both, and the text does not itself resolve which one a
  reader should keep. **This specification does not choose between them by inspection** (`plan`
  rule 7: a tolerance picked after seeing which shape it favours is not a check). `TYAW-R-003`
  below states Shape F as the IMPLEMENTED law (unchanged from the earlier draft, since it is what
  this module already builds on the same mechanism `TYAW-R-002` already established for IIR) —
  but names this a standing, registered choice, not a settled one, and states Shape E beside it as
  the live alternative `TYAW-A-009` is built to discriminate.

  **The onset comparison is not affected by this ambiguity.** `DIL10`'s own "greater than 8
  degrees" is presented as an independent OBSERVATION — "we clearly notice a linear drift" once β
  drops below it — not as a figure derived from the 0.06°/s rate via tan β₀ = μ̇/R; the two are
  compared in `TYAW-P-1` because they are two independent routes to the same number, not the same
  computation read twice.

- **TYAW-R-003.** Same mathematical shape as `TYAW-R-001` (IIF shares II/IIA's own **+x**-toward-
  Sun body convention, `MSGA15` Fig. 4) — `DIL10` does not restate `KOUBA09`'s own ATAN2 formulas,
  reporting instead the MEASURED parameters a `KOUBA09`-shaped law needs: noon-turn rate
  *R*_noon ≈ 0.11°/s, Shape F's own midnight-turn rate *R*_night ≈ 0.06°/s (two **different**
  rates for the two turn kinds, unlike II/IIA's and IIR's single rate — `DIL10`'s own reported
  asymmetry; NOT corroborated by `eclips.f`, which cites `DIL10` for this same figure rather than
  supplying an independent one — the two are one source, not two). Yaw bias *b* = −0.7°
  (`eclips.f`'s own Feb-2017 entry, attributed to Kuang et al. 2016; not present in `DIL10`'s own
  2010 text, which predates that value — this spec states the more recent, more specific number
  and names its own later provenance rather than the earlier paper's silence on it). The β₀
  relation applied to *R*_night = 0.06°/s gives **7.93°** (`TYAW-P-1`), matching `DIL10`'s own
  independently OBSERVED threshold, "as long as the elevation β of the Sun ... is greater than
  8 degrees," to two significant figures.

### 4.4 GPS Block IIIA (stopgap: modelled as IIF)

- **TYAW-R-004.** IIIA satellites are modelled with `TYAW-R-003`'s own IIF law, unchanged —
  **named as an approximation**, the same stopgap IGS analysis centres without a IIIA-specific
  model use, not a claim that IIIA's own hardware matches IIF's. Recorded here because a frozen
  gate (`B-IIIA`, `oracle/cases.tsv`) names the block even though it turns out not to consume any
  attitude law (`oracle/ORACLE.md` §6) — so this requirement exists for the box-wing/eclipse-yaw
  case a *future* consumer may build, stated honestly now rather than left for that consumer to
  discover the gap unwarned.
  **The stopgap's own known error, stated in the direction it is known** (the 2023 source located
  in §2, read only at summary level): true IIIA behaves "similar to Block IIR but with a smaller
  maximum yaw rate and an earlier maneuver onset" than IIR's own 0.20°/s. IIF's own rates
  (0.11°/s, 0.06°/s) are *already* smaller than IIR's 0.20°/s — the same **direction** the true
  IIIA/IIR difference reports, so IIF-as-IIIA is directionally consistent with the one comparison
  available, not merely an arbitrary stand-in — and so is the onset: IIF's own night-turn β₀
  (7.93°, `TYAW-P-1`) is larger than IIR's 2.39°, meaning EARLIER than IIR's own onset in β-angle
  terms, the same direction the 2023 source reports for true IIIA. **Not asserted to be
  quantitatively close** — the 2023 source states the comparison qualitatively, not with a number
  this spec could check the way `TYAW-P-1`'s own four rows do; `TYAW-Q-001` (§10) carries closing
  that gap once the true law is read.

### 4.5 Antenna thrust

- **TYAW-R-005.** A force along the body **+z**-axis (the antenna boresight, `SPEC-macromodel`'s
  own body-frame convention, unchanged), directed so as to push the spacecraft **away from** the
  boresight direction (recoil: the radiated photons carry momentum outward along +z, so the
  reaction on the spacecraft is along −z_body, the SAME sign convention `PHPR-R-010`'s own
  "radiation pressure pushes away from the source" already established for a different force,
  restated here because it is the same physics, not merely the same words) — magnitude *P*/*c*
  as a stated **upper bound**, not an exact value (*P* the L-band transmit power in watts, a
  caller-supplied value — §1's own **Not in scope**, `DYN-Q-001`'s L4/L5 split).
  **Direction and magnitude are NOT the same claim, and only one of them is exact.** For a beam
  with intensity *I*(θ) over solid angle (θ the angle from boresight), the net recoil is
  (1/*c*)∫*I*(θ)cos θ *d*Ω = (*P*/*c*)⟨cos θ⟩, *P* = ∫*I*(θ) *d*Ω the total radiated power. Any
  pattern AXISYMMETRIC about +z (not "isotropic" — a truly isotropic, uniform-over-4π radiator has
  ZERO net recoil by the same symmetry argument, the opposite claim) has its TRANSVERSE momentum
  components cancel exactly, so the recoil is exactly along −z_body regardless of the pattern's
  own shape — the direction claim, and it is exact. But ⟨cos θ⟩ ≤ 1 for any beam with nonzero
  width, so *P*/*c* — the value at ⟨cos θ⟩ = 1, a beam of zero width — OVERSTATES the true
  magnitude by a factor (1 − ⟨cos θ⟩) that depends on how wide the beam actually is: the magnitude
  claim is an upper bound, not exact, and stating it as exact would be the direction argument's own
  true reasoning silently carried over to a claim it does not support. `TYAW-P-5` quantifies the
  gap for a GPS-relevant beam width and shows it, at GPS altitude's own reference point, a
  sub-millimetre fitted-position effect — which is the actual reason *P*/*c* is used unadjusted
  here — not that the adjustment is exactly zero,
  which it is not, even for a boresight-centred beam, and any power reaching side or back lobes
  (`DIL10`'s own Figure 3 shows GPS IIF's own real array is not a single narrow beam) only widens
  the gap further, since larger θ means smaller (or negative) cos θ. No antenna gain pattern
  beyond this bound is modelled (`SPEC-macromodel`'s own eventual concern if ever needed, §1's own
  **Not in scope**); nor is any resulting torque, since this tree has no attitude dynamics to feel
  one (`SPEC-dynamics`'s own scope, kinematics only).
- **TYAW-R-006.** This force is **independent of the yaw-attitude law in force** — it acts along
  +z_body regardless of whether `TYAW-R-001`through `R-004` or the ideal `nominal_yaw_steering`
  currently defines that axis, since the antenna is body-fixed and its own boresight is z_body by
  definition in every regime. Concretely: **every attitude law this tree has sets z_body to
  geocentric nadir (−r̂) and never moves it** — `nominal_yaw_steering`'s own construction states
  this directly, and `gps_yaw_attitude`'s own turn laws only ever rotate x_body/y_body ABOUT
  z_body, since yaw is by definition a rotation about that axis. So z_body, for every provider
  this tree has, is a function of the spacecraft's own position alone, and this force is computed
  from the position alone — no Sun ephemeris, no yaw-attitude call, and no dependence on which
  block or law is in force, discharging the independence claim by construction rather than by
  branching on eclipse state and reaching the same z_body through a different path each time. If a
  future, non-nadir-z body mode is ever added, it is that mode's own job to supply this force an
  axis provider then, declaring whatever dependencies IT needs — not a reason to add one now for a
  mode this tree does not have.

### 4.6 Integration with `odl::attitude`

- **TYAW-R-007.** A new function, block-parameterised rather than one function per block (the
  four laws above share one mathematical shape — `KOUBA09`'s own ATAN2 turn form — differing only
  in body-x sign, hardware rate(s), yaw bias, and whether a shadow-crossing law exists at all;
  writing it once and parameterising it is `DYN-Q-001`'s own "additive, not four near-duplicates"
  reasoning, the same shape `photon_force`'s own single kernel already applies to SRP and ERP).
  Signature stated language-free in §5.
- **Statelessness, the one exception found, and the RULED convention for it.** Every quantity
  `TYAW-R-001` through `R-004` needs (β, μ, *E*, and hence *t*_s, *t*_m) is computable from the
  CURRENT epoch's own (**r**, **v**, sun direction) alone — no history of prior calls is required
  for the turn timing itself, since `KOUBA09`'s own *t*_s/*t*_m formulas (his Eq. 10/13/14) are
  closed-form in the CURRENT β. The one exception the search found: `eclips.f`'s own Feb-2017
  changelog entry names a genuine edge case — when |β| ≤ 0.07° a turn may straddle β's own sign
  change, and a STATELESS provider evaluated at the current instant alone cannot know which way
  the spacecraft already committed. **Carried, with a stated convention, not left unhandled**:
  `gps_yaw_attitude` uses sign(β) at the CURRENT query instant, with sign(0) taken as **+1**, a
  fixed, deterministic tie-break rather than remembered state — the same "no history of prior
  calls" property every other quantity here already has, extended to this one case rather than
  broken for it. **Why this is an edge case worth carrying, not a reason to add state**: β crosses
  zero only twice a year per orbital plane and moves by under a degree a day, so a turn (lasting up
  to roughly the 27–55 min
  `KOUBA09`/`DIL10` state, §4.1/§4.3) that actually straddles the ≤0.07° crossing window is a
  handful of turns a year at most, across the whole constellation. **A stateless refinement is
  possible and is recorded, not built**: taking sign(β) at the turn's own start *t*_s, rather than
  at the current query instant, would resolve the ambiguity correctly even when the sign has since
  flipped, since *t*_s is itself geometry (`KOUBA09`'s own Eq. 10, a function of β, not a
  remembered value) — worth building if a future campaign's own gate needs the rare straddling
  turns modelled correctly; `TYAW-Q-002` (§10) carries the option, ruled to be carried rather than
  built now.

## 5. Interfaces, stated language-free

- `GpsBlock`: a sum type, `II_IIA`, `IIR_IIRM`, `IIF`, `IIIA` (`IIIA` selecting `TYAW-R-004`'s own
  IIF-law stopgap, not a fifth implementation).
- `HardwareYawRates`: the per-block constants `TYAW-R-001`through `R-004` need, stated as data the
  caller supplies rather than compiled in (the same reasoning `PHPR-R-001` already gives for
  irradiance): a noon rate and a night/shadow rate in deg/s (equal for every block but IIF, which
  alone needs two, `TYAW-R-003`), a yaw bias in degrees, and — II/IIA only — the spin-up
  acceleration RR.
- `gps_yaw_attitude(r_gcrs_m: Vec3, v_gcrs_m_per_s: Vec3, sun_direction_gcrs: BodyDirection, block:
  GpsBlock, rates: HardwareYawRates) -> Result<Mat3, AttitudeError>` — same return type
  `odl::attitude::nominal_yaw_steering` already returns, so every existing consumer of that type
  (`Srp`, `Erp`) accepts this provider's own output without a second frame type. Refuses
  (`TYAW-F-001`) at the same Sun-on-nadir singularity `nominal_yaw_steering` itself refuses at,
  forwarded unchanged, since this function calls that one for the ideal-law value it hands back
  outside any turn.
- `antenna_thrust(p_watts: double, z_body_gcrs_unit: Vec3) -> Result<Vec3, DynError>`, returning
  the GCRS-frame force — takes the boresight axis DIRECTLY, a unit vector, not a full `Mat3`
  frame: this law needs exactly one thing, the direction +z_body points in, and a parameter typed
  as a "frame" invites a caller to build one that is not a complete, valid frame and pass it
  anyway, silently correct only because nothing reads the other rows (manager's own review).
  No `Macromodel` parameter, since `TYAW-R-005`'s own axisymmetric-recoil upper bound needs no
  surface geometry at all (§1's own **Not in scope**). A `dyn::Force`-conforming plugin wraps this
  (constructed once, `accel()` calling this and dividing by mass) — for `AntennaThrust`,
  simpler than `Srp`/`Erp`'s own shape, since `TYAW-R-006` (§4.5) makes z_body computable from the
  position alone, needing neither a `Macromodel`, an `Ephemeris`, nor a yaw-attitude call.

## 6. Precision and accuracy

**`TYAW-P-1` through `P-3` and `P-5` are DERIVED RELATIONS, an inequality, and a trigonometric
average, not multiplication-chain budgets** — `tan β₀ = μ̇/R` is evaluated through `atan`,
continuity/rate-bound are logical conditions, and (1 + cos α)/2 is evaluated through `cos`, all
outside `tools/budgetcheck.py`'s own supported grammar (checked directly: its own
`parse_term`/`EXPR` machinery reads chains of `quantity * quantity`, not a transcendental function
of one) — stated here as prose for a careful human reader, the same footing `SPEC-photon-pressure`'s
own `PHPR-P-1` (a convergence-ratio statement, not a product either) already stands on, rather than
forced into a table row the tool would then fail to parse. `TYAW-P-4` below is a real
multiplication chain and is a table row for exactly that reason.

- **TYAW-P-1.** The β₀ derived relation, tan β₀ = μ̇/R, checked against every block's own printed
  β₀, not merely computed and trusted: atan(0.00836°/s ÷ 0.134°/s) = **3.57°** (II/IIA, fast end)
  against `KOUBA09`'s printed 3.6°; atan(0.00836°/s ÷ 0.098°/s) = **4.87°** (II/IIA, slow end)
  against his printed 4.9°; atan(0.00836°/s ÷ 0.20°/s) = **2.39°** (IIR) against his printed 2.4°;
  atan(0.00836°/s ÷ 0.06°/s) = **7.93°** (IIF, night) against `DIL10`'s own independently OBSERVED
  threshold, "greater than 8 degrees" — a genuinely separate check, since `DIL10` reports that
  number from noticing where the drift begins, not from this same tan β₀ = μ̇/R computation.
  Four independent checks of the same relation, not one formula trusted once.

  **A fifth, noon-side check (atan(0.00836°/s ÷ 0.11°/s) = 4.35° against `DIL10`'s own "below
  4 degrees") and a duration cross-check against his own "about 55 minutes" at β = 0° were tried
  and withdrawn** (manager's own review): the noon check compares the SAME relation at the SAME
  rate `TYAW-R-003` already states, not an independent one, and no tolerance was written before
  computing it — exactly `plan` rule 7's own trap, a threshold that would have been tuned to
  pass. The duration check (49.82 min against "about 55 minutes") is worse than uninformative: at
  β = 0° the two `DIL10` numbers do not fit one simple rate at all (0.06°/s × 55 min = 198°, not
  180°), so the near-miss it reported was an artefact of comparing an identity to itself, not a
  corroboration (§4.3's own Shape E/Shape F registration states what this actually means and why
  it is not settled by inspection).
- **TYAW-P-2.** Continuity: at every hand-over between the ideal law and a turn law (both
  directions), |ψ_turn(t_handover) − ψ_ideal(t_handover)| must be small — `KOUBA09`'s own
  construction (the turn's own ψ(t) is DEFINED to start from ψ_n(t_s), the ideal value at the
  turn's own start) makes this exact at onset by construction, not merely approximately true; the
  RETURN to nominal is where `KOUBA09`'s own text (§4.1's "catches up with ψ_n(t)") states the
  hand-over condition, checked the same way.
- **TYAW-P-3.** |ψ̇(t)| ≤ *R* throughout every modelled turn (the spin-up/spin-down phases of
  the II/IIA shadow crossing included, where the instantaneous rate is *below* *R* by
  construction, not merely bounded near it) — a property the closed-form equations must exhibit
  by construction, checked by evaluating ψ̇ analytically or numerically across a stated turn.
- **TYAW-P-5.** *P*/*c*'s own overstatement of the true antenna-thrust magnitude (`TYAW-R-005`),
  quantified rather than asserted negligible: for a UNIFORM beam filling a cone of half-angle α,
  ⟨cos θ⟩ = (1 + cos α)/2 exactly (a solid-angle-weighted average over a cone, a closed form, not
  a numerical estimate). At α = 13.9°, the angular half-width of the Earth's own disc as seen from
  GPS altitude (arcsin(*R*_E/*r*), *R*_E = 6378.137 km WGS84 equatorial, *r* ≈ 26 561 km) —
  a GPS beam's own natural width, wide enough to illuminate the visible Earth and little more —
  ⟨cos θ⟩ = 0.98536, so *P*/*c* overstates by (1 − ⟨cos θ⟩) = **1.46 %**; at a wider α = 20°,
  ⟨cos θ⟩ = 0.96985, overstating by **3.02 %**. Against `TYAW-P-4`'s own 4.91 × 10⁻¹⁰ m/s²: the
  1.46 % case is 7.19 × 10⁻¹² m/s², the 3.02 % case 1.48 × 10⁻¹¹ m/s².

  **The reference point for that residual is GPS altitude, not `PERT-P-2`'s 8.552 × 10⁻¹¹ m/s²
  floor.** That number is `TN36-6` §6.2.1's ocean-tide truncation cutoff evaluated at *r* = 7331 km
  — a LEO radius by construction (`PROVENANCE.md` §18.2). Ranking a GPS-altitude residual against
  it is the same fault §21.4 already names for an SRP/drag comparison at one shared radius: a
  defensible-looking number from an unstated, mismatched reference point, not a second and
  unrelated mistake. The conclusion is unchanged; the reason is rebuilt on GPS's own numbers. At
  GPS altitude the natural reference is the central acceleration itself, *GM*/*r*² ≈ 0.565 m/s²
  (*GM* = 3.986004418 × 10¹⁴ m³ s⁻², *r* ≈ 26 561 km, the same radius used above for the beam
  half-angle). A constant, radially outward acceleration error acts like a fractional shift in
  effective *GM*: δ(*GM*)/*GM* ≈ δ*a*/(*GM*/*r*²) — 7.19 × 10⁻¹²/0.565 ≈ 1.27 × 10⁻¹¹ for the
  1.46 % case, 1.48 × 10⁻¹¹/0.565 ≈ 2.62 × 10⁻¹¹ for the 3.02 % case. A circular-orbit fit absorbs
  a constant fractional *GM* shift as a comparable fractional shift in fitted radius, *r* ×
  δ(*GM*)/*GM* — **0.34 mm** and **0.70 mm** for the two cases respectively, both a small fraction
  of a millimetre against what a GNSS orbit fit resolves. (One-pass estimate; premise stated:
  constant, radial, circular orbit.) This, not an exact-cancellation argument and not a LEO
  precision floor, is why *P*/*c* is used unadjusted: the gap is real and quantified, not zero, and
  at GPS altitude it is a sub-millimetre fitted-position effect — small enough to leave unmodelled,
  for the reason that actually applies at this altitude.

| id | what it is a budget for | headline result | the multiplication | discharges |
|---|---|---|---|---|
| `TYAW-P-4` | antenna-thrust plausibility, at SVN63/G01's own transmit power (`IGSMETA`, cited as a plausibility anchor only, not consumed as data), 1/*c* and a stated GPS-class mass's own reciprocal both pre-inverted so the row stays one multiplication chain | force **8.01 × 10⁻⁷ N**; acceleration **4.91 × 10⁻¹⁰ m/s²** | 240 W * 3.33564e-9 s/m = **8.01e-7 N**; 8.01e-7 N * 6.13497e-4 kg^-1 = **4.91e-10 m/s^2** — the same order of magnitude `KOUBA09`'s own introduction cites for the antenna-thrust literature (`SPEC-photon-pressure`'s own §4.1 cites 2.7 × 10⁻¹⁰ m/s² for Block IIA specifically, a different, smaller-power satellite; consistent in order of magnitude, not asserted equal) | R-005 |

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `TYAW-F-001` | `gps_yaw_attitude`'s own Sun-on-nadir singularity — forwarded unchanged from `odl::attitude::nominal_yaw_steering`'s own `ATTD-F-001`, never relabelled (`DRAG-F-003`'s own precedent for a cause reached through more than one caller, the SAME refusal reached two ways) | the two nearly-parallel directions and the tolerance |
| `TYAW-F-002` | `antenna_thrust` called with a negative or non-finite *P*_watts | the offending value |

## 8. Acceptance tests

| id | what is checked | expected value | source of the expected value | tolerance | discharges |
|---|---|---|---|---|---|
| `TYAW-A-001` | `TYAW-P-1`'s own four checks, computed in code, not transcribed by hand | 3.57°, 4.87°, 2.39°, 7.93° | derived, `KOUBA09`/`DIL10`'s own printed rates and β₀ | matching the printed figures to their own stated precision | P-1 |
| `TYAW-A-002` | II/IIA and IIR noon-turn continuity at hand-over (`TYAW-P-2`), both directions, at a stated small-β geometry | exact at turn onset (by construction); small at nominal-law re-entry | `KOUBA09`'s own construction | P-2's own stated tightness | R-001, R-002, P-2 |
| `TYAW-A-003` | \|ψ̇\| ≤ R throughout a stated II/IIA shadow-crossing maneuver, spin-up phase included | never exceeded | `TYAW-P-3` | exact (an inequality, not a tolerance) | R-001, P-3 |
| `TYAW-A-004` | IIF's own two-rate law (`TYAW-R-003`) produces DIFFERENT noon vs. midnight turn durations at the same β, matching the ~0.11/~0.06 ratio | noon turn measurably shorter than midnight turn, same β | `DIL10`'s own stated asymmetry | measured, order-of-magnitude consistent with the ~1.8x rate ratio | R-003 |
| `TYAW-A-005` | IIIA-as-IIF (`TYAW-R-004`) is bit-identical to `TYAW-R-003`'s own IIF output at the same inputs — the stopgap is exactly what it claims to be, not a fifth, subtly-different implementation | exact equality | `TYAW-R-004`'s own definition | exact | R-004 |
| `TYAW-A-006` | `TYAW-F-001` fires, forwarded with `ATTD-F-001`'s own id unchanged | the diagnostic | `odl::attitude`'s own existing refusal, reused | — | F-001 |
| `TYAW-A-007` | `TYAW-F-002` fires on a negative and a NaN *P*_watts, and does not fire on the adjacent valid (zero included) value | the diagnostics; success on the valid input | the refusal catalogue | — | F-002 |
| `TYAW-A-008` | `TYAW-R-005`'s own IMPLEMENTED formula: the function returns exactly *P*/*c* along −z_body, at a stated *P* — a check on the code computing its own stated upper bound correctly, not a claim that bound equals the true physical recoil (`TYAW-P-5` is that claim) | exact | `TYAW-R-005`'s own closed form | exact | R-005 |
| `TYAW-A-009` | **CODE's own published attitude, NOT called "observed"** (an ORBEX file's own ATT record carries whatever attitude an analysis centre's own processing used — `Loyer et al. 2021`'s own comparison across seven IGS centres finds "significant differences ... for GPS and GLONASS satellites," and ORBEX's own format spec, `ORBEX009.pdf` §4.10, carries no observed/predicted flag for the ATT record at all, unlike its own POS record — so this is `plan` rule 8's case, agreement with another implementation's own reading of `DIL10`, not a ground-truth measurement): CODE's real ORBEX file for G01/SVN63 at an epoch where β is inside IIF's own turn regime (\|β\| < 7.93°, `TYAW-P-1`), discriminating `TYAW-R-003`'s own registered Shape F against the Shape E alternative (§4.3) by the criterion §4.3 states, set before extraction | Shape F if the extracted plateau rate and turn-start time each fall within a quarter of the two shapes' own separation (≈0.0034°/s, ≈4.4 min at this step's own chosen epoch) of Shape F's own prediction; Shape E symmetrically; "neither" otherwise | CODE's own MGEX ORBEX product, compared against this specification's own Shape E/Shape F predictions, registered here before any file is read | the criterion itself, not a tolerance chosen after the result | R-003 |
| `TYAW-A-010` | `TYAW-P-5`'s own (1 + cos α)/2 average and the two stated overstatement figures, computed in code, not transcribed by hand | 0.98536/1.46 % at α = 13.9°, 0.96985/3.02 % at α = 20° | derived, a standard solid-angle average over a uniform cone | matching the stated figures to their own precision | P-5 |
| `TYAW-A-011` | `TYAW-R-006`'s own position-only claim, made concrete: `AntennaThrust::accel` declares d(a)/d(v) EXACTLY zero (not a bound on a nonzero term), and its own analytic d(a)/d(r), (*P*/(*mc\|r\|*))(I − r̂r̂ᵀ), matches an INDEPENDENT central finite difference of the whole plugin call | d(a)/d(v) absent with 0 s⁻¹ neglected; d(a)/d(r) matching to the finite difference's own truncation floor | `TYAW-R-006`'s own closed form, `r_hat`'s standard derivative | relative, at the difference's own achievable precision | R-006 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `TYAW-R-007` | A design/integration statement, discharged by every row above that exercises `gps_yaw_attitude` end to end, not by a row of its own. |

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as a new numbered section when this step is implemented: the scope
  narrowing to GPS (and why — the frozen baselines are GPS-only and, more precisely, cannonball
  fits that consume no attitude law at all, found by reading `validate_sp3.sh` rather than
  assumed); the two-round rule-4 search (broad, then narrow) and its own outcome per source,
  including the sources located but not built (§2's own carried-forward table) so a later step
  does not repeat the search; the `eclips.f` boundary (cross-check only, never a template, and
  why); the IIIA stopgap decision and its own named direction of error; and, once `TYAW-A-009` is
  completed, which epoch was chosen and the β computation that justified it.
- `IGSMETA`'s own full terms are **not** established by this specification — recorded here as
  provenance-only for the one fact this step needs (§2), with the manifest entry and licence
  check deferred to L5, where the same file becomes consumed data (`SATELLITE/TX_POWER`).

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `TYAW-Q-001` | **RULED: carry.** The true GPS Block IIIA law (Dilssner et al. 2023 and its companion paper, §2's own carried-forward table) is modelled with the IIF stopgap (`TYAW-R-004`) for now; the true law is carried beside Galileo, GLONASS and BeiDou with its source located, not read closely enough yet to implement. |
| `TYAW-Q-002` | **RULED: carry, with the stated convention.** The β ≈ 0° turn-start sign-ambiguity edge case (§4.6) is handled with a fixed, stateless tie-break (sign(β) at the query instant, sign(0) = +1) rather than remembered state — β crosses zero twice a year and moves under a degree a day, so a turn actually straddling the ≤0.07° crossing window is a handful a year at most across the constellation. A stateless refinement (sign(β) at the turn's own start *t*_s, itself geometry, not memory) is recorded in §4.6 as a future option, not built now. |
| `TYAW-Q-003` | **Not a question for the manager — settled by whoever builds `TYAW-A-009`.** Compute G01/SVN63's own β from this tree's own ephemeris and a real orbital state near a candidate date, find an epoch inside the 7.93° turn regime with a real turn in the CODE ORBEX record, and record why that date (§8, §9). Kept as a numbered row so the gap is not mistaken for an oversight, the same reasoning `SPEC-macromodel`'s own Retired identifiers table states for a different kind of removal. |
| `TYAW-Q-004` | **Agreed, carried.** Galileo/GLONASS/BeiDou's own carried-forward sources (§2) are located but none read past what the search's own summary level found. When this tree is ready to build them, each needs its own closer read the way `KOUBA09`/`DIL10` got this round, not an implementation from the search summary alone. |
