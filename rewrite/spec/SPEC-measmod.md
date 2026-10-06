# SPEC-measmod — L6 step 4: measurement models against one interface, and the station and site registry

| | |
|---|---|
| **Spec ID** | `MEAS` |
| **Status** | **draft** 2026-10-06 — written on the manager's rulings R1–R12 of the same date (`plan/subplan_L6/L6-4.md` is the binding text), whose order (R12) sequences the build after this specification **without a review pause**; the manager's review of this text comes with Round 9, and a correction found then is made **here first** (`SPEC-template.md` §10) |
| **Version** | 1.0 |
| **Date** | 2026-10-06 |
| **Layer** | L6 `io-measurements` (`../plan/PLAN.md` §3.7), step 4 (`measmod`) |
| **Depends on** | `core` (`odl::Result`, `Vec3`, `Mat3`, `metres_from_km`), `time` (`Epoch`, `Duration`, `TimeScale`, `LeapTable`), `eop` (`EopSeries`), `frames` (`GcrsState`, `ItrsState`, `gcrs_to_itrs`, `to_gcrs`, `to_itrs`), `ephemerides` (the Earth's barycentric velocity), `io` (the parsed CRD, IOD, SP3 and SINEX structures); ERFA (private) |
| **Depended on by** | L7 `estimation` (the partials, mapped with the transition matrix), L8 `campaigns` (the SLR and optical campaigns) |

**Derivation declaration (plan R1).** Written from the documents listed in §2 and from no
implementation of this module.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`, `scripts/`,
`analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed, searched or
otherwise inspected during this specification's preparation; `oracle/capture.sh` was not opened.
The IERS Conventions' Fortran routines were handled as `plan/PLAN.md` §3.11 point 4 and the manager's
step-3 ruling require: each file was fetched to scratch and **only its comment prolog was kept**
(`PROVENANCE.md` §39 records which); no routine body was read, copied or translated, and what this
specification takes from them is the **printed test values**, a published observation. Two tarballs
that contain code (`orbitNP.py`, the CoM tables' `com_6multi.for`) were not opened beyond their
README and licence text. Everything else the rule-4 research of 2026-10-06 found — and every
absence, with the search that failed — is in `~/.claude/handover/2026-09-25-odl-rewrite-L6.REPORT.md`,
"Round 8", and is re-stated where this specification relies on it.

---

## 1. Purpose and scope

One interface through which a prediction of an observation and its partial derivatives are obtained,
for **four observation types**, with the station and site registry they need:

| type | what is observed | source of the observation |
|---|---|---|
| `RangeTwoWay` | the two-way time of flight of a laser pulse, as the one-way-equivalent range `c·ToF/2` | an ILRS CRD normal point (`CRD2` record `11`) |
| `PositionGeometric` | a target's geometric position at an epoch, in a named frame | an SP3 position record, or a Horizons vector-table record |
| `RaDec` | right ascension and declination of a target seen from a site, under a **declared reduction** (`Astrometric` or `Geometric`) | an IOD observation (`IODFMT`) |
| `AzEl` | azimuth and elevation, **apparent and refracted** (the one reduction an azimuth/elevation can mean) | an IOD observation |

and, for each, a **modelled value, the residual, a complete partials row, and the record of what
was applied** (§5). Light time, the observer's motion during it, tropospheric refraction and, for
angles, aberration and refraction **belong to the model, not to the caller**.

**The station and site registry:** SLR stations from SLRF2020 (release 2026.02.05) with the ILRS
eccentricities in up/north/east, refusing an unknown station, an epoch outside an eccentricity's
span, and a site after a post-seismic event; optical sites supplied by the caller with a citation.

**Not in scope** (each with the spec or step that owns it, or the reason it is out):

- **Station displacement** — the solid Earth tide, ocean tide loading, the pole tide, atmospheric
  loading — **is out of this step and out of the MVP** (the manager's ruling R2: no gate through L8
  needs it; L8 step 2's frozen residual is 57 m). It is **written down, with its magnitudes, in §6.4
  and in every modelled range's `Applied` record** (`MEAS-R-038`), so that the omission cannot be
  forgotten and a residual that contains it is never mistaken for a model defect. The solid Earth
  tide (four published cases, `TN36-7`'s `DEHANTTIDEINEL.F` prolog) and the pole tide are a **named
  follow-on**; ocean loading's coefficients come only from a request-form service (the Onsala OTL
  service) and cannot be sourced under this tree's rules.
- The estimation of anything — station coordinates, biases, orbit — is L7. The partials this
  module returns are with respect to the **target's state at the bounce or emission epoch** (§4.8);
  L7 maps them with the transition matrix Φ and adds nothing (`MEAS-R-060`).
- The ILRS Data Handling File (range, time and pressure biases, exclusion periods), the centre-of-mass
  *tables* (station-, epoch- and wavelength-dependent), and the ITRS post-seismic *corrections*:
  **deferred** (R3). The centre of mass is a **cited input** for a spherical target; a non-spherical
  target is refused until a campaign needs its array's offset (`MEAS-F-013`, R9).
- Fetching any file: the manifest and `tools/fetch.py` do that; this module reads text the caller
  already holds.
- One-way and transponder ranging, full-rate ranging (record `10`), the other CRD record kinds,
  CPF predictions as an observation source, Doppler, and any observation type not named above.
- Two-colour atmospheric correction, horizontal tropospheric gradients (up to 5 cm of delay at low
  elevation, `TN36-9` §9.1.3, are **omitted** and listed in §6.4), and the Sun's gravitational
  deflection and potential term in the aberration (≤ 0.4 µas, `ERFA` `eraAb`'s own note).

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `TN36-9` | Petit, G., Luzum, B. (eds.), IERS | *IERS Conventions (2010)*, chapter 9, **§9.1** *Tropospheric model for optical techniques* (§9.2 is the radio model and is **not** used) | updated text of 10 June 2013 | `https://iers-conventions.obspm.fr/content/chapter9/icc9.pdf`, fetched directly 2026-10-05, SHA256 `4bd0ef02008f02218af51eb1a293bda9849ca14f40e684a10fa80a6149d01903` (the file `tn36_c9.pdf` for the official 2010 text is a 404 — probed, not pursued) | **primary** | normative: the zenith-delay equations (9.1)–(9.7), the mapping function FCULa (9.8)–(9.10) and Table 9.1, the stated accuracies |
| `TN36-11` | Petit, G., Luzum, B. (eds.), IERS | *IERS Conventions (2010)*, chapter 11, **§11.2** *Ranging techniques* | 2010 | `https://iers-conventions.obspm.fr/content/chapter11/tn36_c11.pdf`, fetched directly 2026-10-05, SHA256 `ca147e67fd88e924b5603cea217541a53612493fa12f65bd485409ba2879ea2a` | **primary** | normative: eq. (11.17) the time of propagation and the Shapiro term; (11.18)–(11.19) TT-compatible coordinates; "for near-Earth satellites … only the Earth" |
| `TN36-1` | as above | chapter 1, Table 1.1 and §1.1 | 2010, update of 2017-11-06 | the tree's own pinned `tn36-chapter1` | **primary** | `c` = 299 792 458 m s⁻¹ (defining), `GM⊕` TT-compatible = 3.986 004 415 × 10¹⁴ m³ s⁻² (the TCG value 3.986 004 418 × 10¹⁴ × (1 − `L_G`)), `L_G` = 6.969 290 134 × 10⁻¹⁰ |
| `TN36-7` | as above | chapter 7, §7.1.1 (solid Earth tide, Table 7.2), §7.1.2 (ocean loading), §7.1.4 (pole tide), §7.3 | 2010, update of 2018-02-01 | the tree's own pinned `tn36-chapter7` | **primary** | informative: the sizes of the **omitted** station-displacement terms (§6.4); nominal degree-2 `h₂` = 0.6078, `l₂` = 0.0847; pole tide ≤ 25 mm radial, 7 mm horizontal; §7.3 says the SLR reference-point models are "kept and updated" by the ILRS |
| `MP04` | Mendes, V. B., Pavlis, E. C. | "High-accuracy zenith delay prediction at optical wavelengths", *Geophys. Res. Lett.* 31, L14602, doi:10.1029/2004GL020308 | 2004 | `https://ilrs.gsfc.nasa.gov/docs/2004/2004GL020308.pdf` (ILRS-hosted), fetched directly 2026-10-05, SHA256 `5ac3edf1c4a22f96d7e3111014e75d31e640b4daa39ecd4f00954976aa063564` | **primary** | the zenith-delay derivation; its equations agree with `TN36-9`'s (checked: the dispersion constants, 0.00002416579 for pressure in Pa ≡ 0.002416579 for hPa, 0.00028 per km ≡ 0.00000028 per m); "Copyright 2004 by the American Geophysical Union" — a `literature` source, pinned by URL and hash, never vendored |
| `MPPL02` | Mendes, V. B., Prates, G., Pavlis, E. C., Pavlis, D. E., Langley, R. B. | "Improved mapping functions for atmospheric refraction correction in SLR", *Geophys. Res. Lett.* 29(10), 1414, doi:10.1029/2001GL014394 | 2002 | `https://ilrs.gsfc.nasa.gov/docs/2001/2001GL014394.pdf` (ILRS-hosted), fetched directly 2026-10-05, SHA256 `603f0508717df576c540e5965a4116a9d25fd9d066e66b98b8dad9e7b7689daa` | **primary** | **FCULb**: Table 1 and eq. (6), which `TN36-9` does not print. The PDF's text layer **drops every minus sign** in Table 1 and renders φ_d² ambiguously; the signs and the square were read from the rendered page and **verified by reproducing the IERS `FCUL_B` printed case exactly** (`MEAS-A-023`). "Copyright 2002 by the American Geophysical Union" — `literature` |
| `MM73` | Marini, J. W., Murray, C. W. | *Correction of Laser Range Tracking Data for Atmospheric Refraction at Elevations above 10 Degrees*, NASA TM X-70555 | 1973 | `https://ntrs.nasa.gov/api/citations/19740007037/downloads/19740007037.pdf`, fetched directly 2026-10-05, SHA256 `06291d6a3063ea6509617c73ccdc69728bca6fe7c28373944f9fbd6eb7a1f1a5` | **primary** | eq. (22): the water-vapour pressure from relative humidity, `e = (Rh/100) · 6.11 · 10^{7.5 (T−273.15)/(237.3 + (T−273.15))}` mbar, `T` in K — read from the rendered page; **absent from `TN36-9` and from `MP04`** (searched). A NASA technical memorandum |
| `IERS-CASES` | IERS Conventions Centre | the comment **prologs** of `FCUL_ZD_HPA.F`, `FCUL_A.F`, `FCUL_B.F` (chapter 9 software) | 2009 | `https://iers-conventions.obspm.fr/content/chapter9/software/`; SHA256 of the files `92731affca053aad15a44be7db58dbf6df689e75cf2e1f3b39cb4d99a4da198b`, `fdeb39aee3c8d4c2d6eb6a7e743c420372e28da5b3e84942d09580a88847693a`, `97a25687c56264460ea3df73d6017bb60ce079d6d69ef23a0cca42a5c78c92aa` | **primary, partial** — prologs only | the **published test cases** (§8, `MEAS-A-020`…`-023`). **Terms:** the *IERS Conventions Software License* in each file — "Permission is granted to anyone to use the Software for any purpose, including commercial applications, free of charge, subject to the conditions and restrictions listed below"; a derived work must be identified as not IERS software, say how it differs, rename modified routines, include source for what it distributes, keep the notice intact; published results must acknowledge use. **Ruled R1: the printed test values are published observations, used with the routine named and cited; no routine text enters the tree** |
| `CRD2` | Ricklefs, R. L., ILRS Data Format and Procedures WG | *Consolidated Laser Ranging Data Format (CRD)*, v2.00/2.01 | 2019 | `https://ilrs.gsfc.nasa.gov/docs/2022/crd_v2.01e3.pdf`; SHA256 `9b7ef5ebbb573418ef3e0d8f89691011eb3afd111734bde96186d55ddaa848d3` (re-fetched 2026-10-05, equal to `SPEC-io-formats.md` §2) | **primary** | what a normal point is (§3.4); H2/H4/C0/`11`/`20` fields |
| `IODFMT` | Lewis, G. D. | *IOD Observation Format Description* | v0, 1998, clarified 2002 | `https://www.satobs.org/position/IODformat.html`; SHA256 `c781b04fcaccd66fea6f96d601a27c8b0e1d76121d20d212dee4b2381586fa04` (= `SPEC-io-formats.md` §2) | **primary** | the seven angle formats, the epoch code, the UTC time, the MX uncertainty encoding |
| `SP3D` | Hilla, S. (NGS/NOAA) | *The Extended Standard Product 3 Orbit Format (SP3-d)* | 2016 | `https://files.igs.org/pub/data/format/sp3d.pdf`; SHA256 `0809fe6571816a9b8394b46b8851b9bfbab956b46f7acab5af6eb62acee5ebbe` (= `SPEC-io-formats.md` §2) | **primary** | positions are geometric at the epoch; the five-character frame code; the header's time system; **silent on which point of the satellite a position refers to** |
| `CPF2` | Ricklefs, R. L. | *Consolidated Laser Target Prediction Format*, v2 | 2018 | `https://ilrs.gsfc.nasa.gov/docs/2018/cpf_2.00h-1.pdf`; SHA256 `36c70d0ad113e019e8c7c1600fd4e65c55eb31112297d74aa4f236ddc3d23181` (= `SPEC-io-formats.md` §2) | **primary** | informative: "Near-Earth artificial satellites are usually computed in the geocentric reference system and do not require the so-called stellar aberration" — true of *pointing a telescope*, **not** of comparing with a star-reduced position (§3.5); spherical satellites' positions refer to the centre of mass |
| `SLRF20` | ILRS Analysis Standing Committee (E. C. Pavlis, M. Blossfeld, F. Lemoine, et al.), from ITRF2020 (Z. Altamimi et al.) | *SLRF2020_POS+VEL*, SINEX 2.02 | release 2026.02.05 (file name; the file's own latest comment is dated 2026.02.06) | `https://edc.dgfi.tum.de/media/docs/SLRF2020_POS+VEL_2026.02.05.snx`, SHA256 `eabb7e3161ade8e8ae64698baa7617f020b9b9a3e507a5ef16abf7d02ed4a695` (manifest `ilrs-slrf2020-20260205`) | **primary** | the markers' positions and velocities. **Terms:** "The data and products are not copyrighted; however, in the event that you publish data or results using these data, we request that you include the following citation: Pearlman, M.R., Noll, C.E., Pavlis, E.C. et al. The ILRS: approaching 20 years and planning for the future. J Geod 93, 2161–2180 (2019)" (`ilrs.gsfc.nasa.gov/about/cite.html`, SHA256 `511652b55ecbdc6affd70ed55cee0654b1ba2fe071acc735b3379deaa9c9ac8d`); licence id `ILRS-PUBLIC`, pin-only |
| `ILRSECC` | ILRS (M. Blossfeld, DGFI-TUM, from the official ILRS eccentricity file) | *slrecc.260527.ILRS.une.snx* | file of 2026-05-27 | `https://edc.dgfi.tum.de/media/docs/slrecc.260527.ILRS.une.snx`, SHA256 `a66476b2f811f87073d0fcc8e7ae92a92d1b68c4f2deabbad317b1b5d5c625c5` (manifest `ilrs-slrecc-une-20260527`) | **primary** | `SITE/ECCENTRICITY` in UNE, "from the marker to the intersection of optical axis"; the header says to use UNE because the Cartesian version is "prone to large conversion errors". Terms as `SLRF20` |
| `PSD20` | Altamimi, Z., et al., IERS ITRS Center (IGN, IPGP) | *ITRF2020-psd-slr.dat* | ITRF2020 | `https://itrf.ign.fr/ftp/pub/itrf/itrf2020/ITRF2020-psd-slr.dat`, SHA256 `b3083748490b853f9e300124138bd72acde31f62705b65079635f5401c460798` (manifest `itrf2020-psd-slr`) | **primary** — used for its **event list only** | the (site, event epoch) pairs after which a linear SLRF2020 position is wrong. Terms: "When using data through this website, please cite: Altamimi, Z., Rebischung, P., Collilieux, X., Métivier, L., Chanard, K. (2022) ITRF2020 [Data set]. IERS ITRS Center Hosted by IGN and IPGP, https://doi.org/10.18715/IPGP.2023.LDVIOBNL"; `IERS-public`, pin-only |
| `ILRSDHF` | ILRS Analysis Standing Committee | *ILRS Data Handling File* | releases 2025-05-13 and 2026-05-27 | `https://ilrs.gsfc.nasa.gov/docs/2025/ILRS_Data_Handling_File_20250513.snx` (SHA256 `d4f0a69ba9d293e7c544b20dbabcc5b2a02f30924705582bda6e67b5442ba5ab`); `https://edc.dgfi.tum.de/media/docs/ILRS_Data_Handling_File_20260527.snx` (SHA256 `732295639a8059368ffab73f39b8267c819da8c8106c3de237c8653543224fe1`) | **primary, partial** — read to choose `MEAS-A-100`'s station and pass | range, time and pressure biases and exclusion periods. **Not applied** (§1); its hash is recorded where `G5` reads it (§8) |
| `ILRSCOM` | Rodríguez, J., Appleby, G., Otsubo, T. | the ILRS centre-of-mass (target-signature) models, `com6.260112.tar.gz` and the ILRS CoM pages | release 2026-01-12 | `https://edc.dgfi.tum.de/media/docs/com6.260112.tar.gz`, SHA256 `5cff31e1e6d4953973e64d8c49cd435193e1fda69017fa0c92513c4bd29b7e8e`; `https://ilrs.gsfc.nasa.gov/missions/spacecraft_parameters/center_of_mass.html` (SHA256 `9927d20deb132340…`) | **primary, partial** | **informative**: the CoM of a geodetic sphere is a table by station, epoch and wavelength (so a single constant is not "the" value — §6.4 sizes the spread). Its `readme.txt` asks for a citation of Rodríguez, Appleby & Otsubo (2019), *J. Geod.* 93, 2553, doi:10.1007/s00190-019-01315-0. Its Fortran reader was not opened |
| `ILRSORB` | ILRS Analysis Centres and Combination Centres | the ILRS weekly orbit products, SP3 (`ilrsa.orb.lageos1.260103.v80.sp3.gz` for `MEAS-A-100`) | weekly | `https://edc.dgfi.tum.de/pub/slr/products/orbits/lageos1/260103/`; SHA256 of the file `9326cd4becc844992b8f8b1dde723bcc380dd659bbddc289bfee3ad56b830463` | **primary** | the orbit `G5` compares against: 120 s sampling, time system **UTC**, frame "SLR20" (SLRF2020), satellite `L51`; the combination's stated accuracy is read from `README_CC.ilrsa` when the envelope is written (§8) |
| `HZAPI` | Solar System Dynamics Group, JPL | *Horizons API documentation and manual* | current | the tree's own pinned pages (`PROVENANCE.md` §38.12) | **primary** | `VEC_CORR` = NONE "(geometric states)", LT "(astrometric light-time corrected states)", LT+S "(astrometric states corrected for stellar aberration)"; the T-01 tables are geometric, ICRF, geocentre, TDB |
| `ERFA` | NumFOCUS Foundation / liberfa | ERFA, source and in-source documentation | v2.0.1 (SOFA 20231011) | the tree's own pinned tarball | **interface** | `eraAb` (the aberration formula this module re-implements and tests against), `eraRefco` (the refraction constants, "worst 62 mas, RMS 8 mas" for optical/IR at zenith distances up to 75°), the precession-nutation matrices for IOD "of date" |
| `FRAMES` | this tree | `SPEC-frames.md` (`FRAME-R-004`, `-R-033`, `-R-028`/`-029`) | 1.11 | `spec/SPEC-frames.md` | **interface** | the typed states and the rotation chain; there is **deliberately no** `to_gcrs(State<BCRS>)` |
| `IOFM` | this tree | `SPEC-io-formats.md` | 1.x (amended by this step) | `spec/SPEC-io-formats.md` | **interface** | the readers; this step adds `read_crd_passes`, the C0 wavelength accessor, the H2 time-scale map and the IOD angle decoder (§9.2) |

**Licence note, the format and model documents.** Every document above is a public document fetched by
plain HTTP GET, no login, no account, no request form (`CDDIS`, which would need an Earthdata login,
was probed — HTTP 401 — and is **not** a source here; the EDC mirrors carry the same ILRS files
anonymously). This module redistributes none of them: it implements the models the documents
print, and the registry reads files the caller holds. The `literature`-kind sources (`MP04`,
`MPPL02`) are pinned by URL and hash and never vendored.

---

## 3. Definitions and conventions

### 3.1 Units, frames, time

- The model works in **metres, seconds and radians**. A target's trajectory is a `GcrsState` (km, km/s,
  `SPEC-frames.md` §3.6); the one crossing to metres is `odl::metres_from_km` (`unitcheck`, gate 12).
- Every vector the model forms is in **GCRS axes** unless a type says otherwise. A station's position
  is formed from its ITRS position by the chain of `SPEC-frames.md` §4.1 **at the epoch the leg needs it**
  — not once for the observation — so the Earth's rotation during the light time is in the model and
  not in the caller (`MEAS-R-030`).
- **All light-time arithmetic is done in TT-compatible quantities**: lengths in TT-compatible metres
  (the SLRF2020 coordinates, the orbit, `GM⊕` = 3.986 004 415 × 10¹⁴ m³ s⁻² [TN36-1 §1.1] are TT-compatible)
  and times in TT seconds, so `c` is the defining 299 792 458 m s⁻¹ and **no TCG scaling is applied again**:
  [TN36-11 §11.2, eq. (11.19)] "the geocentric space coordinates are TT-compatible". Mixing a
  TCG-compatible length with a TT time costs `L_G` × range = 6.969 290 134 × 10⁻¹⁰ × 6.6 × 10⁶ m = **4.6 mm** on a LAGEOS range (§6.1).
  An observation's tag is resolved to an `Epoch` (a TAI-stored instant) at construction (§3.2).
- **Notation** for one two-way range: `t_t` the instant the pulse leaves the station's reference point,
  `t_b` the instant it is reflected, `t_r` the instant it returns; `s(t)` the station's reference-point
  position and `v_s(t)` its inertial velocity; `r(t)` the target's (centre of mass) position; `r_b = r(t_b)`;
  `ρ_u = |r_b − s(t_t)|`, `ρ_d = |r_b − s(t_r)|`; `ĝ_u = (r_b − s(t_t))/ρ_u`, `ĝ_d = (r_b − s(t_r))/ρ_d` (both
  pointing **from the station to the target**); `û(t)` the unit vector along the ellipsoid normal at the
  station (the local geodetic vertical); `Δ_u`, `Δ_d` the extra path length of each leg (atmosphere plus
  Shapiro, §4.3–4.4). `n̂` a unit direction, `β = v/c`.

### 3.2 Time tags are resolved where they are parsed

R8: each observation source resolves its tag's time scale **at construction** and refuses one it does
not know; there is no tree-wide `Epoch` wrapper (that would be an L1 redesign).
`Epoch` is scale-agnostic by design (TAI-stored; every constructor and accessor names a scale,
`SPEC-time.md` TIME-R-003), so "the scale is in the type" means here that **each observation type's
constructor names the scale of its tag** and **the output's epochs are reached through accessors that
name theirs** (`bounce_tt()`, `emission_tt()`, `epoch_utc()`). The tag scales:

| source | tag | scale | how |
|---|---|---|---|
| CRD normal point | `H4` start date + record `11` seconds of day | **UTC** | `H2` field 6 ("Station Epoch Time Scale") **3 = UTC(USNO), 4 = UTC(GPS), 7 = UTC(BIPM)**; the differences between these realisations are small (my estimate is ≤ 100 ns, 0.75 mm along-track at LEO speed; not measured here) and are not modelled; **every other code — 1–2, 5–6, 8–9 "reserved for … obsolete time scales", and 10 and above "UTC (Station Time Scales) USE ONLY WITH ANALYSIS STANDING COMMITTEE APPROVAL" — is refused by the reader** (`read_crd_passes`, `IOFM-F-015`, returned with its own id: ruling R8 puts the resolution at parse time) |
| IOD observation | cols 24–40 | **UTC** | "UTC date … UTC time … to a precision of 0.001 second" |
| SP3 position record | the epoch header | the header's time system | `GPS`, `TAI`, `UTC` through `io::to_time_scale`; `GLO`, `GAL`, `BDT`, `QZS` refused (`IOFM-F-003`, restated `MEAS-F-014`) |
| Horizons vector record | the printed time | **TDB** | `io::to_time_scale(HorizonsTimeSystem::Tdb)`; the reader already refuses any other token |

A CRD record's `seconds of day` is **modulo 86 400** [CRD2 §0]: the date is the `H4` start date, and when a
record's seconds of day is **smaller than the start's seconds of day** the pass has crossed 00:00 UTC and the
record is on the next day (`MEAS-R-011`). A pass must not exceed one day [CRD2 §0].

### 3.3 Sign conventions

- **Residual = observed − modelled**, in the observation's own unit and type.
- **Centre of mass.** `δ_com ≥ 0` is the distance by which the effective reflection point lies **nearer the
  station** than the centre of mass, one-way: ILRS defines the correction as the amount **added to the
  measured range to refer it to the centre of mass**, so the measured range of a spherical target is the
  geometric range to the centre of mass **minus** `δ_com`: `R_model = c·ToF_model/2 − δ_com` (`MEAS-R-033`).
- Right ascension is measured eastward from the x axis of the ICRS (GCRS) axes, in [0, 2π); declination in
  [−π/2, π/2]. Azimuth is measured from north through east, in [0, 2π); elevation from the local horizontal.

### 3.4 What a CRD normal point is (read from `CRD2`, then checked against a real file)

A record `11` carries the **seconds of day** (the normal point's mean epoch, station clock corrected), the
**time of flight** in seconds — "none, one-, or two-way depending on [H4's] range type indicator … should be
corrected for station system delay" — and the **Epoch Event**, field 5: `0` = ground receive time (at the
system reference point, two-way), **`1` = spacecraft bounce time**, **`2` = ground transmit time** (at the
SRP), `3`–`6` one-way; "currently, only 1 and 2 are used for laser ranging data". **A real file
(`lageos1_202601.np2`, Yarragadee, first record `11 7676.800587100000 0.051212898595 new 2 …`) tags the epoch with
event 2**, so the model must solve for the bounce time and may not treat the tag as the bounce.
The H4 indicators say what the stations have already applied: tropospheric refraction (field 16), centre of
mass (17), receive amplitude (18), **station system delay (19)**, spacecraft system delay (20), and the **range
type** (21: 2 = two-way). "For normal point records, stations … **must set the centre of mass applied and
refraction applied flags to false**"; the station system delay applied indicator is normally true. So a normal
point's range is the **uncorrected** two-way time of flight with the calibration applied: **the model applies
the troposphere and the centre of mass itself.** Times and ranges refer to the station's **system reference
point (SRP)**, "which in many cases is the telescope invariant point" — which the eccentricity file defines as
the intersection of the optical axes.

### 3.5 What an angle observation is, and why a reduction is declared

`IODFMT` states the coordinate system's epoch (col 46: 0/blank "of date", 1 = 1855, 2 = 1875, 3 = 1900,
4 = 1950, **5 = 2000**, 6 = 2050) and calls cols 48–61 "**Observed** RA or AZ … DEC or EL". **It is silent** on
whether a position is astrometric or apparent, on aberration, on light time, on refraction for azimuth/elevation
(and it prints no meteorology), on whether "of date" is mean or true, and on what a station number denotes.
An observer who reduces a satellite's position against catalogue stars (a plate or star-pair reduction) obtains
the satellite's direction **in the stars' frame**, which carries the stars' **annual aberration**. A
near-Earth satellite **does not share it**: it moves with the observer, so its direction in the geocentric frame
has only the diurnal aberration (≤ 0.32″). The astrometric place of the satellite therefore differs from its
GCRS topocentric direction by the stars' annual aberration — **up to v_E/c = 29.78 km s⁻¹ / c = 20.49″**
(`ERFA`, `TN36`) — a direction-dependent shift `−(v_E/c)⊥`. `CPF2`'s sentence quoted in §2 is correct for pointing a
telescope and wrong for this comparison. Measured in a scratch simulation for the rule-4 report (an orbit at 7000 km,
a rotating station, the Earth moving at 29.78 km s⁻¹): **18.12″** for a line of sight 62° from the Earth's velocity,
against the first-order −(v_E/c)⊥ = 18.12″ (agreement 8 × 10⁻⁴″). **So an angle observation DECLARES the reduction
it was made under, and there is no default** (`MEAS-R-050`): `Astrometric` (RA/Dec against catalogue stars),
`Geometric` (the GCRS topocentric direction, light-time corrected, no aberration), `ApparentRefracted`
(azimuth/elevation as seen). A comparison of two sides that used different reductions is a finding, not a residual
(`plan/subplan_L8/L8-3.md`'s carried note: `O-01`'s frozen 26.14″ against a 20.5″ effect).

### 3.6 The SINEX epoch format

`YY:DOY:SSSSS` — two-digit year, day of year (1 = 1 January), seconds of the UTC day. **Years 00–49 are 20YY and 50–99 are
19YY** (the files' data run from 1976 to 2026). `00:000:00000` as an **end** epoch means **open** (the span continues);
as a start epoch, unknown. A `SOLUTION/ESTIMATE` row's reference epoch (e.g. `15:001:00000`) is the epoch of the position;
velocities are m yr⁻¹ with the year of **365.25 days**.
**An epoch before 1972-01-01 cannot be a UTC epoch in this tree** (`TIME-F-…`: before 1972 UTC ran at another rate, so a second there is not an SI second). The pinned eccentricity file has spans from 1971 (SOD 79024201, `71:001:00000` to `75:365:86399`), so the registry reads them as follows: a span **start** before 1972 leaves the span unbounded below (every supported epoch is after it); a span whose **end** is before 1972 can contain no supported epoch and never matches; a position's reference epoch or a post-seismic event before 1972 refuses the build (`MEAS-F-020`).

---

## 4. Required behaviour

### 4.1 The registry

- **MEAS-R-001.** The SLR registry is built from two SINEX documents the caller holds (read by `io::read_sinex`): SLRF2020's `SITE/ID`,
  `SOLUTION/EPOCHS`, `SOLUTION/ESTIMATE` and the eccentricity file's `SITE/ID`, `SITE/ECCENTRICITY`. Epochs are converted with the supplied
  `LeapTable` at build. A malformed row, a position with no velocity, a solution (`SOLUTION/EPOCHS` or `SOLUTION/ESTIMATE`) for a marker that `SITE/ID` does not list, an estimate with no epoch row or the reverse, or an eccentricity row whose SOD is
  absent from the eccentricity file's own `SITE/ID`, or a SOD listed twice in one `SITE/ID` with different pad, point or DOMES number **refuses the build** (`MEAS-F-020`), naming the line; **an identical repeat is accepted** (the pinned eccentricity file lists `71100301` twice, the two rows equal). **A marker that `SITE/ID` lists and that has no solution does not refuse the build**: the pinned SLRF2020 lists marker `7307 B` (SOD 73071702) with no solution, so the registry **records it as having no coordinates** and its SODs refuse at lookup (`MEAS-F-001`, saying so, `MEAS-R-004`). The registry records the headers' own version strings and the
  SHA-256 of each document it was given (supplied by the caller from the manifest) so a run's provenance can name them.
- **MEAS-R-002.** The marker position at an epoch `t` is `x(t) = x_ref + v·(t − t_ref)` with `t − t_ref` the elapsed SI time between the two epochs (`Epoch::difference`, leap seconds included: 2 s over the pinned file's 11 years, 3 × 10⁻⁹ m at 50 mm yr⁻¹) in years of 365.25 × 86 400 s [§3.6].
  The marker is the SINEX `(pad, point)` of the SOD's `SITE/ID` row, and the `SOLN` used is the one of that marker whose `SOLUTION/EPOCHS` data span contains `t` (end `00:000:00000` open; 28 markers of the pinned release have more than one `SOLN`). An epoch **before the first span's start, after
  the last span's end, or in a gap between spans** has no solution and is refused (`MEAS-F-003`) — a position extrapolated beyond its data is not a
  position. A span is closed at its start; **an end whose seconds of day are 86 399 denotes the end of that day** (the file writes `…:86399` as the last second of the day and starts the next span at `…:00000` of the next, as the pinned eccentricity rows do), so the span extends to the following midnight, exclusive, and no one-second gap is opened between contiguous spans; every other end epoch is that instant, inclusive.
- **MEAS-R-003.** The system reference point is `s_itrs = x(t) + E·(U, N, E)ᵀ` where `E` rotates local up/north/east at the marker's **geodetic**
  latitude `φ` and longitude `λ` (WGS 84 ellipsoid, `ERFA` `eraGc2gd` with `n = 1`) to ITRS:
  `Δx = −sin λ·E − sin φ cos λ·N + cos φ cos λ·U`, `Δy = cos λ·E − sin φ sin λ·N + cos φ sin λ·U`, `Δz = cos φ·N + sin φ·U`.
  The row is the one whose `SOD` matches and whose span contains `t` (end `00:000:00000` open); **none** refuses (`MEAS-F-002`). The registry
  returns the marker, the SRP, the geodetic coordinates of the SRP, the eccentricity, the SOD, and the release strings. The Cartesian (XYZ)
  eccentricity file is not used (§2, `ILRSECC`).
- **MEAS-R-004.** A station is identified by the **SOD** = system identifier (the CDP pad, 4 digits) × 10⁴ + system number × 10² + system occupancy,
  from the `H2` record's fields 3–5 (the real file's `h2 YARL 7090 5 13 3` is SOD 70900513, which `SITE/ID` and the eccentricity rows carry). The pad,
  the SOD and an optical station number are **three distinct types** (a bare four-digit number does not say which namespace it is in; plan §5
  constraint 10). An unknown pad or SOD **refuses** (`MEAS-F-001`), naming the number and the release; there is no nearest-station or default.
  **A SOD is placed only when both documents list it in `SITE/ID`, agree on its pad, point and DOMES number, and its marker has a solution** (a disagreement refuses the build, `MEAS-F-020`): a coordinate is a property of a *marker*, and a SOD that is on another marker of the same pad must not borrow the first one's. The pinned eccentricity file knows **542 SODs and SLRF2020 lists 483**; of the 542, **482 are placed and 60 are not**: 57 SODs on 49 pads that SLRF2020 does not list at all, two on pad 7307 that it does not list (`73071701` on marker A, `73071703` on marker C — different markers from the ones it holds), and `73071702`, listed by both but on marker 7307-B, which has no solution. An unplaced SOD **refuses as `MEAS-F-001` saying that it has no coordinates**, not as "unknown".
- **MEAS-R-005.** The registry also reads `PSD20`'s event list: each (site, event epoch) pair. A lookup for a site at an epoch **at or after** one of
  its events **refuses** (`MEAS-F-004`), because the linear SLRF2020 position omits the post-seismic motion. The refusal has **exactly one** override,
  named `accept_linear_position_after_post_seismic_event`, set explicitly per call, never by default or environment, and **recorded in the returned
  site** (`post_seismic_override_used`) so a run's provenance carries it (`SPEC-template.md` R-ERR-3). The PSD *corrections* are not applied (R3).
  **The order of the checks** in `SlrRegistry::site` is fixed and stated, because a closed or unlisted site fails more than one: identity (`MEAS-F-001`), eccentricity span (`MEAS-F-002`), solution span (`MEAS-F-003`), post-seismic (`MEAS-F-004`); the first that fails is returned.
- **MEAS-R-006.** The registry exposes the number of distinct **pads**, of **placed SODs** and of **unplaced SODs** (`MEAS-R-004`). For the pinned release the header's "184 unique
  sites" is **reconciled, not assumed** (plan §4 rule 3): the file's own `FILE/COMMENT` history adds Xian (7329, 2025.04.08) and Ishioka (7317,
  2025.05.13) after the sentence was written, and both are in `SITE/ID`, so the **186** distinct codes = 184 + 2. The registry holds **186 pads and 482 placed SODs** (483 listed by SLRF2020); the eccentricity file knows **235 pads and 542 SODs**, and the 60 SODs that are not placed are the ones `MEAS-R-004` refuses (`MEAS-A-004`).
- **MEAS-R-007.** An **optical site** is supplied by the caller: a geodetic latitude, longitude and height (WGS 84) **and a citation**. A blank or
  whitespace-only citation is refused (`MEAS-F-006`, `macromodel::cited`'s rule: "a value without a citation is a load error"), and so is a latitude outside [−π/2, π/2], a longitude outside [−π, 2π] or a height outside [−1 000, 10 000] m (`MEAS-F-024`): a unit slip — degrees given as radians — must not become a station. A number added twice with different coordinates refuses (`MEAS-F-025`); an identical repeat is accepted. A site is held by
  its optical station number (the IOD column 17–20 number); an unknown number refuses (`MEAS-F-005`). No list of optical sites is bundled: the only
  public list is GPL-3.0-derived and carries observers' names and coordinates (rule-4 report, D2).

### 4.2 Time

- **MEAS-R-010.** Each observation source resolves its tag's scale per §3.2, at parse time, and refuses an unknown one: a CRD `H2` code other than 3, 4, 7 is refused by `read_crd_passes`
  (`IOFM-F-015`, carried by the builder with its own id); an SP3 header time system other than GPS/TAI/UTC (`MEAS-F-014`); a Horizons token other than TDB (`IOHZ-F-002`, unchanged). The pass the builder receives therefore carries its scale already resolved: no unresolved tag can reach a model.
- **MEAS-R-011.** A CRD record's epoch is the `H4` start date plus the record's seconds of day, **plus one day when the seconds of day is smaller
  than the start's seconds of day** (the pass crossed 00:00 UTC), converted from UTC with the leap table. A seconds of day that is negative or at least 86 401 refuses (`MEAS-F-021`); one in [86 400, 86 401) — `CRD2`'s "a leap second would use 86400" — is placed in the day's final minute as second 60.x and
  is accepted **only on a UTC day that ends in a positive leap second** (the leap table decides; `Epoch::from_calendar` refuses it otherwise, and that refusal is carried, `MEAS-A-016`).
  The epochs of a pass **must be non-decreasing** once the rollover is applied, and a pass may not span more than a day [CRD2 §0]; a pass that breaks either refuses (`MEAS-F-019`) — a rollover rule that is wrong, or a file out of order, must not become a residual.

### 4.3 The troposphere (SLR)

- **MEAS-R-020.** **Zenith delay** [TN36-9 §9.1.1, eqs (9.3)–(9.7); `MP04`], with `φ` the geodetic latitude, `H` the geodetic height in metres,
  `P_s` the surface pressure (hPa), `e_s` the water-vapour pressure (hPa), `λ` the wavelength (µm), `σ = 1/λ`:
  `f_s(φ,H) = 1 − 0.002 66 cos 2φ − 0.000 000 28 H`;
  `f_h(λ) = 10⁻² [ k₁* (k₀ + σ²)/(k₀ − σ²)² + k₃* (k₂ + σ²)/(k₂ − σ²)² ] C_CO2`, `k₀ = 238.0185`, `k₂ = 57.362`, `k₁* = 19 990.975`, `k₃* = 579.551 74` (µm⁻²),
  `C_CO2 = 1 + 0.534 × 10⁻⁶ (x_c − 450)` with `x_c = 375` ppm (`C_CO2 = 0.999 959 95`);
  `f_nh(λ) = 0.003 101 (ω₀ + 3 ω₁ σ² + 5 ω₂ σ⁴ + 7 ω₃ σ⁶)`, `ω₀ = 295.235`, `ω₁ = 2.6422`, `ω₂ = −0.032 380`, `ω₃ = 0.004 028`;
  `d_h = 0.002 416 579 · f_h/f_s · P_s` (m); `d_nh = 10⁻⁴ (5.316 f_nh − 3.759 f_h) · e_s/f_s` (m); `ZTD = d_h + d_nh`.
  The CO₂ content is the conventional 375 ppm [TN36-9]; it is not an input.
- **MEAS-R-021.** **Mapping function FCULa** [TN36-9 §9.1.2, eqs (9.8)–(9.10), Table 9.1]: `m(e) = (1 + a₁/(1 + a₂/(1 + a₃))) / (sin e + a₁/(sin e + a₂/(sin e + a₃)))`,
  `aᵢ = aᵢ₀ + aᵢ₁ t_s + aᵢ₂ cos φ + aᵢ₃ H` (`t_s` in °C, `H` in metres), coefficients exactly as printed in Table 9.1:
  `a₁₀ = 12100.8e-7, a₁₁ = 1729.5e-9, a₁₂ = 319.1e-7, a₁₃ = −1847.8e-11; a₂₀ = 30496.5e-7, a₂₁ = 234.6e-8, a₂₂ = −103.5e-6, a₂₃ = −185.6e-10; a₃₀ = 6877.7e-5, a₃₁ = 197.2e-7, a₃₂ = −345.8e-5, a₃₃ = 106.0e-9`.
  **FCULb** [`MPPL02` Table 1, eq. (6)]: `aᵢ = aᵢ₀ + (aᵢ₁ + aᵢ₂ φ_d²) cos(2π/365.25 · (doy − 28)) + aᵢ₃ H + aᵢ₄ cos φ`, `φ_d` in degrees, `doy` the decimal day of year,
  `a₁₀ = 11613.1e-7, a₁₁ = −933.8e-8, a₁₂ = −595.8e-11, a₁₃ = −2462.7e-11, a₁₄ = 1286.4e-7; a₂₀ = 29815.1e-7, a₂₁ = −56.9e-7, a₂₂ = −165.5e-10, a₂₃ = −272.5e-10, a₂₄ = 302.0e-7;
  a₃₀ = 68183.9e-6, a₃₁ = 93.5e-6, a₃₂ = −239.4e-9, a₃₃ = 30.4e-9, a₃₄ = −230.8e-5`. **The range model uses FCULa** (the CRD record `20` supplies the temperature); FCULb is implemented because it is
  a published case (`MEAS-A-023`) and for a station with no temperature.
- **MEAS-R-022.** **Water-vapour pressure from relative humidity** [`MM73` eq. (22)]: `e_s = (Rh/100) · 6.11 · 10^{7.5 t/(237.3 + t)}` hPa (≡ mbar), `t = T − 273.15` with `T` in K, `Rh` in per cent.
  This conversion is not in `TN36-9` or `MP04`; the IERS routine's header says only that its water-vapour pressure "was calculated from the surface temperature … and Relative Humidity". The
  CRD record `20` gives pressure in **millibar**, temperature in **K**, humidity in **%**.
- **MEAS-R-023.** The delay of a leg is `Δ_atm = ZTD · m(e)` (one-way, metres), with `e` the elevation of the leg's geometric direction above the station's local horizontal
  (`sin e = û·ĝ`). **Below 3° — the mapping function's stated validity [TN36-9 §9.1.2: "for elevation angles greater than 3 degrees"] — and for a target below the horizon, the model refuses**
  (`MEAS-F-010`). Horizontal gradients (up to 5 cm of delay at low elevation) are omitted (§6.4).

### 4.4 The relativistic range term

- **MEAS-R-024.** The **Shapiro** extra path of a leg between positions at geocentric distances `r₁` (station) and `r₂` (target) separated by `ρ` is, from [TN36-11 §11.2, eq. (11.17)] with the Earth the
  only body ("for near-Earth satellites … the only body to be considered is the Earth"),
  `Δ_S = (2 GM⊕/c²) · ln[(r₁ + r₂ + ρ)/(r₁ + r₂ − ρ)]` metres (the equation's time of propagation is `Δ_S/c`), `GM⊕` TT-compatible. It is evaluated per leg, `r₂ = |r_b|`, `r₁ = |s|` at the leg's station epoch.
  Its partials `∂Δ_S/∂ρ = (2GM/c²) · 2(r₁+r₂)/((r₁+r₂)² − ρ²)`, `∂Δ_S/∂r₂ = −(2GM/c²) · 2ρ/((r₁+r₂)² − ρ²)`, `∂Δ_S/∂r₁ = ∂Δ_S/∂r₂`.

### 4.5 The two-way range model

- **MEAS-R-030.** The model predicts the **two-way time of flight** `ToF = t_r − t_t` from the equations (all times TT, all positions GCRS)
  `c (t_b − t_t) = ρ_u + Δ_u`  and  `c (t_r − t_b) = ρ_d + Δ_d`,
  with the station at `s(t_t)` on the up leg and `s(t_r)` on the down leg — **the Earth's rotation between the two is therefore in the model, never in the caller**
  (`MEAS-A-030`, `-031` in closed form with the station in uniform motion; `MEAS-A-032` with the Earth-fixed station). **Event 2** (the tag is `t_t`): solve the up leg for `t_b`, then the down leg for `t_r`. **Event 1** (the tag is `t_b`): solve the up leg for `t_t` (the station
  at `t_b − τ_u`) and the down leg for `t_r`. Events 0 and 3–6 are refused (`MEAS-F-008`). The target is reached **only** through the trajectory abstraction (§5.1) at the epochs the equations
  need; the station only through the station abstraction.
- **MEAS-R-031.** Each leg's light time is found by **fixed-point iteration to the double-precision fixed point** — the change in the leg's duration at most 4 ulp of the duration, 1.4 × 10⁻¹⁷ s for a LAGEOS-like 22 ms leg (4 ulp of 22 ms) — starting from the
  geometric range at the tag; the contraction factor is the relative speed over `c` (about 2.5 × 10⁻⁵), so four passes suffice. **A hard limit of 20 passes per leg**; a leg that has not converged **refuses**
  (`MEAS-F-011`), naming the leg, the passes and the last change. The convergence criterion is not a tolerance chosen by a user: a looser one adds `c·ε_τ/h` to a finite-difference partial and ruins the gate (§6.2).
- **MEAS-R-032.** `R_model = c · ToF_model / 2 − δ_com` (metres, one-way-equivalent), and the observed `R_obs = c · ToF_obs / 2` with `ToF_obs` the record's time of flight in seconds. `residual = R_obs − R_model`.
  The centre of mass `δ_com` is a **cited input** for a **spherical** target, a number that is finite and not negative (the sign convention of §3.3 makes a negative one a sign error: `MEAS-F-017`) with a non-blank citation (`MEAS-F-006`); a non-spherical target (an array offset in the body frame plus attitude) is **refused until a campaign needs it** (`MEAS-F-013`).
  An observed time of flight that is not finite and positive refuses (`MEAS-F-018`).
  **The declaration names the satellite it was made for** — the ILRS satellite identifier of `H3` field 3 (7603901 for LAGEOS-1) — and a pass whose `H3` identifier differs refuses (`MEAS-F-007`): a centre of mass is a property of one target, and applying LAGEOS-2's to LAGEOS-1's ranges would be a plausible wrong number.
- **MEAS-R-033.** The leg delays are evaluated **per leg**: `Δ_atm` at each leg's own elevation from its own `ĝ` and the station's vertical at that leg's station epoch; `Δ_S` per leg; both inside the light-time equations (their
  dependence on the geometry is part of the fixed point and of the partials). The meteorological inputs are one set per observation (§4.5 `MEAS-R-035`).
- **MEAS-R-034.** Before any modelling a CRD normal point's session is **checked** against what the model will apply (§3.4): **range type** (H4 field 21) must be 2 (two-way); **refraction applied** and **centre of mass applied** must be 0;
  **station system delay applied** must be 1; the **Epoch Event** must be 1 or 2. Any other value refuses (`MEAS-F-008`/`-009`), naming the field and the value: a normal point already corrected would be corrected twice, and
  an uncalibrated time of flight would carry the system delay (tens of metres) into the residual.
- **MEAS-R-035.** The meteorology is the pass's CRD record `20` values (pressure mbar, temperature K, relative humidity %): **linearly interpolated between the two records that bracket the tag** ("a 2-point linear interpolation will usually suffice"
  [CRD2 §3.4 notes]); a tag **outside the records' span holds the nearest record's values** — the format writes a record only when a value changes "significantly" — and the output **says so** (`met_held`, with the distance in seconds), because the real file's
  first record can be a millisecond *after* the normal point it belongs to (met `7676.801` for a normal point at `7676.800587`). A pass with **no** record `20` refuses (`MEAS-F-012`); a pass with no normal point refuses (`MEAS-F-023`).
- **MEAS-R-036.** The wavelength is the pass's **C0** record, field 3 (nanometres), for the normal point's system configuration id (record `11` field 4); **a configuration id with no `C0` refuses** (`MEAS-F-012`).
  The io module exposes it as a typed accessor (§9.2).
- **MEAS-R-037.** The elevation, latitude and height the troposphere needs are the **SRP's**: `φ` and `H` the geodetic latitude and ellipsoidal height of the system reference point; `û` the ellipsoid normal there. `TN36-9` §9.1.1 defines `H` as "the geodetic height of the station" and notes that the formula "is insensitive to the difference" from the orthometric height (its footnote 1), so the ellipsoidal height is the right input; the sensitivity is measured, not assumed: a 30 m difference — a typical geoid undulation — changes the one-way delay at 15° elevation by 0.11 mm, both through `f_s` and through the mapping function's `a_i3 H` terms, against the zenith model's own 1 mm (`MEAS-A-024`).

### 4.6 The ephemeris-position model

- **MEAS-R-040.** The model of a `PositionGeometric` observation is the target's **geometric position at the observation epoch** — no light time, no aberration (an SP3 position is geometric at the epoch [SP3D]; a Horizons vector with
  `VEC_CORR` = NONE is "geometric states" [HZAPI]) — **expressed in the observation's frame**: GCRS (a Horizons ICRF table: GCRS and ICRF axes are the same to the model's precision) or ITRS (an SP3 file), the latter by the chain of
  `SPEC-frames.md` §4.1 at the epoch. `residual = observed − modelled`, a vector in the observation's frame, metres. A table read as the wrong time scale — the 69 s defect — is a rotation and a displacement the identities of `MEAS-A-060` detect.
- **MEAS-R-041.** An SP3 file's five-character coordinate-system code is mapped to a frame by an **explicit table**: `IGS05 IGS08 IGb08 IGS14 IGb14 IGS20 IGb20 SLR20` (the ILRS products' own code for SLRF2020) → ITRS. **Any other code refuses**
  (`MEAS-F-014`); the differences between ITRS realisations (millimetres to centimetres) are not modelled and are listed in §6.4. SP3 positions are in kilometres; velocities in decimetres per second.
  **`SLR20` is the ITRS as realised by SLRF2020**, mapped here from the ILRS product's own description — its header, which holds no record — **before any test or code of this step read a record of the file** (2026-10-06, on the manager's condition for the pin; the rule is the one above, not an exception to it). The pinned product,
  `ilrsa.orb.lageos1.260103.v80.sp3` (`ilrs-lageos1-sp3-260103`), writes the code in the coordinate-system field of its first line — `#cV2025 12 28  0  0  0.00000000    5040   SLR SLR20 FIT COMB` — and says, in its own comment line, `%/* ilrsa.orb.lageos1.260103.v80.sp3 Reference TRF: SLRF2020`;
  and SLRF2020's own file (`ilrs-slrf2020-20260205`, the file the station registry reads) describes itself as an "Expanded set of SLR stations in ITRF2020 frame", `OUTPUT SLRF2020 (SSC/SSV)`; and the ILRS's own Analysis Products page says of these orbits "The orbits are provided in an Earth-fixed frame, using the ITRF that is used at the time of generation of these orbits".
  The code therefore names the ILRS realisation of the ITRS in which the registry's station coordinates are expressed: the orbit and the stations share one frame. (The sibling products of the same week write `ECEF`, `ITRF14`, `ITRF2` and `SLR14` in that field; the table refuses them, as it refuses every code it was not given a reason for. `SPEC-io-formats.md` §3.2 records the search for the ILRS's description of the format of these files.)
  The seven other codes are the IGS products' names for their own realisations of the ITRS; they are listed by name, by the IGS convention, and the differences between the realisations are the §6.4 omission.
- **MEAS-R-042.** `SP3D` is **silent on which point of the satellite a position refers to** (centre of mass or antenna phase centre): the observation **carries that, supplied by the caller**, as `CentreOfMass`, `AntennaPhaseCentre` or `Unspecified`.
  The model accepts `CentreOfMass` only; the others refuse (`MEAS-F-014`) — an antenna offset is not modelled.

### 4.7 The angle models

- **MEAS-R-050.** An angle observation **declares its reduction**; the builder has **no default** and no overload without the argument (`MEAS-A-082`). The reductions: `Astrometric` and `Geometric` (observations of right ascension/declination), `ApparentRefracted` (azimuth/elevation). A reduction that does not fit the observation's coordinate kind — `Astrometric` or `Geometric` for an azimuth/elevation line, `ApparentRefracted` for a right-ascension/declination line — refuses (`MEAS-F-015`).
- **MEAS-R-051.** The **emission epoch** `t_e` solves `|r(t_e) − s(t_o)| = c (t_o − t_e)`, `t_o` the observation epoch (TT), the observer at `s(t_o)`: fixed-point iteration as `MEAS-R-031` (the same limit, the same refusal, `MEAS-F-011`). No Shapiro or atmospheric term enters a direction's light time (≤ 10⁻² mm of
  range, an angle of ≤ 10⁻⁹″).
- **MEAS-R-052.** `Geometric`: `n̂_geo = (r(t_e) − s(t_o))/|…|` in GCRS axes; right ascension `α = atan2(n_y, n_x)` (mod 2π), declination `δ = asin n_z`.
- **MEAS-R-053.** `Astrometric`: the **natural direction** `n̂_astr = D(n̂_geo; β_E)`, `β_E = v_E/c`, `v_E` the **Earth's barycentric velocity** in the GCRS axes at `t_o` (from `ephemerides`, supplied through a provider), with `D` the **exact** special-relativistic inverse of the aberration
  `A(n; β) = [ n/γ + (1 + (n·β)/(1 + 1/γ)) β ] / (1 + n·β)`, i.e. `D(n; β) = [ n/γ − (1 − (n·β)/(1 + 1/γ)) β ] / (1 − n·β)`, `γ = (1 − β²)^{−1/2}` — `eraAb`'s formula without its solar-potential term (≤ 0.4 µas, omitted, §6.4).
  The diurnal part cancels between the stars and the satellite; the cross term `β_E β_d` (≈ 1.5 × 10⁻¹⁰ rad = 0.03 mas) is omitted. Right ascension and declination as `MEAS-R-052`.
  **Equivalent, independent construction** (the test, `MEAS-A-090`): the direction from the observer's barycentric position at `t_o` to the target's barycentric position at the light-time-corrected emission, the Earth's barycentric position taken from `ephemerides` at both epochs.
- **MEAS-R-054.** `ApparentRefracted`: `n̂_app = A(n̂_geo; β_d)`, `β_d = v_s(t_o)/c` the observer's inertial velocity (the **diurnal** aberration); rotated to ITRS by the chain of §4.1 at `t_o` and to the local east/north/up at the site's geodetic latitude and longitude; azimuth
  from north through east; the **vacuum zenith distance** `z_v` from the vertical; then **refraction** by `z_v = z_o + A tan z_o + B tan³ z_o` with `A`, `B` from `eraRefco(P, t, Rh, λ)` — `ERFA`'s documented model, "worst 62 mas, RMS 8 mas" for optical/IR over zenith distances up to 75° —
  solved for the observed `z_o` (Newton, to 10⁻¹⁴ rad); elevation `= π/2 − z_o`. The **atmosphere is supplied by the caller** (an IOD line carries none): pressure hPa, temperature °C, relative humidity 0–1, wavelength µm; a non-positive pressure or wavelength, a temperature below −100 °C or above 60 °C, or a humidity outside [0, 1] refuses (`MEAS-F-022`). A **vacuum zenith distance above 75° refuses** (`MEAS-F-010`): outside the model's documented accuracy.
- **MEAS-R-055.** The IOD builder decodes the 14 columns 48–61 by the angle-format code (the seven formats, §9.2 / `IOFM-R-0xx`), reads the epoch code and applies this policy: **code 5 (2000) is read as ICRF** — the mean equator and equinox of J2000 differ from the ICRS by a frame bias of at most `√(ξ₀² + η₀² + dα₀²) = √(16.617² + 6.819² + 14.6²) mas = **23.2 mas**` [TN36-5], below the formats' own precision (format 1's finest unit is 0.1 s of right ascension = 1.5″);
  **codes 1, 2, 3, 4 and 6 (1855, 1875, 1900, 1950, 2050) are refused** (`MEAS-F-016`); **code 0 ("of date") is refused unless the caller declares `MeanEquinoxOfDate` or `TrueEquinoxOfDate`** (`MEAS-F-016`), when the direction is rotated to the declared system at the observation epoch with `ERFA`'s bias-precession (`eraPmat06`) or bias-precession-nutation (`eraPnm06a`) matrix; the code is blank for azimuth/elevation. A station number the caller's optical registry does not hold refuses (`MEAS-F-005`).
- **MEAS-R-056.** The angle residual is `observed − modelled` per component, **the right-ascension (or azimuth) difference wrapped to (−π, π]**; no `cos δ` scaling (a weighting is L7's).
- **MEAS-R-057.** *[added 2026-10-06, when the angle tests were written]* **A direction at the pole of its coordinate system refuses** (`MEAS-F-026`): within 1 × 10⁻⁹ rad of the celestial pole (right ascension and declination, in the observation's frame) or of the zenith (azimuth and elevation, by the vacuum zenith distance), the first angle is undefined and its partial, `1/(ρ sin z)`, is unbounded;
  the row would otherwise carry an azimuth of 0 and a partial of infinity or NaN into the caller's filter. A direction 1 × 10⁻⁶ rad from the pole is a valid one with a large, finite partial (0.83 rad m⁻¹ at ρ = 1.2 × 10⁶ m).

### 4.8 Partials, and the boundary with L7

- **MEAS-R-060.** Every modelled value returns a **complete partials row** with respect to the **target's inertial state `(r, v)` at the nominal bounce epoch `t_b*`** (range) or **emission epoch `t_e*`** (angles) or the **observation epoch** (position), GCRS axes, all in SI (m per m, m per m s⁻¹; rad per m, rad per m s⁻¹).
  "With respect to" means **the derivative under a time-fixed variation**: the trajectory is varied so that its state at the nominal epoch changes by `δ = (δr, δv)` and **the light time is re-solved for the varied trajectory**. To first order the target position at the varied bounce time is
  `r* + δr + v* δt_b` (the variation propagates as `Φ(t, t_b*)` and `Φ_rr − I`, `Φ_rv` are second order in the light-time shift). The bounce (emission) epoch's dependence on the trajectory is **folded into the row analytically — it is not exported**: the model owns light time, so
  **L7 maps the row with `Φ(t_b*, t₀)` and adds nothing** (the manager's R6, which amends the report's proposal to export `∂t_b/∂x`). Consequently **the velocity part of every row is zero to first order**, and for event 2 the target's velocity enters the **position** part through the light-time factors.
  The row's derivation, in this tree's notation (§3.1), for a leg's path length `L = ρ + Δ_S + Δ_atm`, with `Δ_atm = ZTD·m(sin e)`, `sin e = û·ĝ`: the leg's **gradient** with respect to the target position `a = ĝ (1 + ∂Δ_S/∂ρ) + (∂Δ_S/∂r₂) r̂_b + ZTD·m′(sin e)·(û − (û·ĝ)ĝ)/ρ`, and its derivative with respect to the **station's epoch** at fixed `r_b`
  `q = −(ĝ·v_s)(1 + ∂Δ_S/∂ρ) − ZTD·m′(sin e)·(û − (û·ĝ)ĝ)·v_s/ρ + (∂Δ_S/∂r₁)(ŝ·v_s) + ZTD·m′(sin e)·ĝ·(ω × û)`, `ŝ = s/|s|`, `ω` the Earth's rotation vector in GCRS (the last term is the vertical turning with the Earth, `dû/dt = ω × û`;
  `m′ = dm/d(sin e)` is the closed-form derivative of the continued fraction); `a_u, q_u` for the up leg, `a_d, q_d` for the down leg; **event 2** — `δt_b = a_u·δr/(c − a_u·v_b)`, `dr_b = δr + v_b δt_b`, `δt_r = (a_d·dr_b + c δt_b)/(c − q_d)`; **event 1** — `δt_b = 0`, `δt_t = −a_u·δr/(c + q_u)`, `δt_r = a_d·δr/(c − q_d)`; and `δR = (c/2)(δt_r − δt_t)` with `δt_t = 0` for event 2.
  **Angles**: `δt_e = −ĝ·δr/(c + ĝ·v_e)`, `dr_e = δr + v_e δt_e`, `δn̂_geo = (I − n̂ n̂ᵀ) dr_e/ρ`, then the reduction's own Jacobian (`D`'s for `Astrometric`; `A(·; β_d)`, the rotation, the local frame and `dz_o/dz_v = [1 + A sec²z_o + 3B tan²z_o sec²z_o]⁻¹` for `ApparentRefracted`;
  the rotation of `MEAS-R-055` for "of date"), and `dα = (n_x dn_y − n_y dn_x)/(n_x² + n_y²)`, `dδ = dn_z/cos δ`.
  **Position**: `[M 0]` with `M` the rotation GCRS → the observation's frame (identity for GCRS).
- **MEAS-R-061.** The rows are **analytic wherever derivable**, as `STM-R-003`'s rule: a finite-differenced row checked against finite differences would pass while checking nothing. The finite differences exist only in the gate (§6.2).

### 4.9 The interface

- **MEAS-R-062.** Each model returns one `Modelled<Obs>` carrying: the **modelled value** (unit in the accessor's name: `range_m()`, `ra_rad()`, `dec_rad()`, `azimuth_rad()`, `elevation_rad()`, `position_m()`), the **observed value**, the **residual**, the epoch at which the partials apply (`bounce_tt()` / `emission_tt()` / `epoch_utc()`), the **partials row** typed by the
  frame of the state it differentiates (`GCRS`), and the **`Applied` record** (for an angle: the reduction, the emission epoch and light time, the size of each aberration and of the refraction applied, the atmosphere and the frame). The partials, epochs and values are opaque to construction: only the models build them. No `Modelled` can be formed from raw numbers.
- **MEAS-R-063.** **No module-level or thread-local accumulator exists** (`SPEC-template.md` R-ERR-1); every failure is a returned `Diagnostic`; one of this module's own carries a `MEAS-F-nnn` id, and **one of a dependency** (`FrameError`, `EopError`, `EphError`, `IoError`s, the trajectory's own) **is returned with its own id** — the shared `Diagnostic` type makes a wrapper unnecessary, and re-labelling it would lose what the dependency knew — with this module's context (which observation, which leg) prepended to the message. A dependency's *warning* is mapped to one of §7's refusals or documented as ignorable (R-ERR-2).
- **MEAS-R-064.** The target, the station, the Earth's orientation and the Earth's motion are reached through four small abstractions, so that **a closed-form test can replace any of them** (`MEAS-A-030`, `-031`, `-093`): a **trajectory** (the target's `GcrsState` at an epoch, or a refusal), a **station track** (position, velocity, local vertical and its rate in GCRS at an epoch, or a refusal), an **Earth orientation** (the rotation GCRS → ITRS and the Earth's rotation vector at an epoch, or a refusal) and an **Earth motion** (the Earth's barycentric velocity and position at an epoch, or a refusal). The model never assumes the target is Keplerian or the station Earth-fixed.
- **MEAS-R-038.** Every modelled range's **`Applied` record** states what it contains: the geometric range of each leg, the light time of each leg, `Δ_atm` and `Δ_S` of each leg, `ZTD` and the mapping-function value, `δ_com` and its citation, the meteorology used and whether it was held, the registry's release strings and whether the post-seismic override was used,
  **and — always — the list of omitted terms with their magnitudes (§6.4)**: station displacement (solid Earth tide, ocean loading, pole tide, atmospheric loading), tropospheric gradients, ITRS-realisation differences, the Sun's term in the Shapiro delay, the data-handling biases and the CoM's station dependence. A range with an empty omitted list is **non-conforming**.

---

## 5. Interfaces, stated language-free

`Result<T>` is `odl::Result` (a value or a `Diagnostic` carrying a `MEAS-F-nnn` or a dependency's own id). No monadic chaining (plan §5 constraint 8). Every quantity carries its unit in the name of its accessor.

### 5.1 The four abstractions

```
Trajectory        state_at(t: Epoch) -> Result<GcrsState[km, km/s]>               // the target's centre of mass, TT-compatible GCRS
StationTrack      at(t: Epoch) -> Result<StationKinematics>                        // one reference point
                  StationKinematics { position_m, velocity_m_s, up (unit), up_rate_per_s = dû/dt }      // all GCRS axes
EarthOrientation  at(t: Epoch) -> Result<{ R_gcrs_to_itrs: Mat3, omega_gcrs_rad_s: Vec3 }>
EarthMotion       at(t: Epoch) -> Result<{ position_m, velocity_m_s }>             // the geocentre's, barycentric, GCRS axes
```

Production implementations: `EopEarthOrientation(EopSeries, EopPolicy, LeapTable)` wraps `frames::gcrs_to_itrs` (and refuses as `frames` and `eop` do); `EarthFixedStation(site: ReferencePoint[ITRS, m], EarthOrientation)` is a station fixed to the
Earth: position the ITRS point rotated to GCRS at the epoch, **velocity the transport term `ω × s` the frames module already forms**, vertical `û` the ellipsoid normal at the point rotated to GCRS, `dû/dt = ω × û`;
`EphemerisEarthMotion(Ephemeris, LeapTable)` takes the Earth's barycentric state from the planetary ephemeris. **A closed-form test replaces any of the four** (`MEAS-R-064`).

### 5.2 The registry

```
SlrRegistry::build(slrf: SinexFile, ecc: SinexFile, psd: text, leaps: LeapTable,
                   hashes: { sha256 of each document })        -> Result<SlrRegistry>      // MEAS-F-020
SlrRegistry::site(key: SodKey, when: Epoch, options: { accept_linear_position_after_post_seismic_event = false })
                                                               -> Result<SlrSite>          // F-001 unknown or no coordinates, F-002 eccentricity span, F-003 solution span, F-004 post-seismic
SlrRegistry::pad_count(), placed_sod_count(), unplaced_sod_count()    // 186, 482, 60 for the pinned release (MEAS-R-006)
SodKey            { pad, system, occupancy }                   // from CRD H2; PadId, SodKey and OpticalStationNumber are distinct types
SlrSite           { sod, pad, point, domes, name, marker_itrs_m, srp_itrs_m, eccentricity_une_m, srp_geodetic{lat_rad, lon_rad, height_m},
                    slrf_release, ecc_release, post_seismic_override_used }
OpticalRegistry::add(number: OpticalStationNumber, lat_rad, lon_rad, height_m, citation)   -> Result<void>      // F-006 blank citation, F-024 unit slip
OpticalRegistry::site(number)                                  -> Result<OpticalSite>                           // F-005
```

### 5.3 Observations and their builders (each resolves its tag's time scale)

```
range_observations(pass: CrdPass, registry: SlrRegistry, leaps: LeapTable, com: SphericalCentreOfMass, options)
        -> Result<vector<RangeObservation>>    // F-007 target, F-008/-009 indicators, F-012 meteorology/wavelength, F-018 time of flight, F-019 order, F-021 seconds, F-023 empty; the reader's IOFM-F-015 carried
RangeObservation  { epoch (the tag), event: Bounce | GroundTransmit, time_of_flight_s (observed, two-way),
                    wavelength_nm, meteorology{ pressure_hpa, temperature_k, rh_percent, held, held_distance_s },
                    site: SlrSite, com: SphericalCentreOfMass }
SphericalCentreOfMass::make(ilrs_satellite_id, metres, citation) -> Result<…>      // F-017 negative or not finite; F-006 blank citation
non_spherical_target(…)                       -> refusal MEAS-F-013

position_observation(header: Sp3Header, epoch: Calendar, record: Sp3PositionRecord, point: ReferencePoint, leaps: LeapTable)
        -> Result<PositionObservation>         // F-014 (time system, frame code, reference point), F-018 (not finite, or the format's absent-position marker)
position_observation(record: HorizonsStateRecord, point: ReferencePoint, leaps: LeapTable)
        -> Result<PositionObservation>         // IOHZ-F-002 unchanged, F-014 (reference point), F-018
PositionObservation { epoch (resolved), frame: GCRS | ITRS, position_m, point }       // a plain aggregate: the builders are the way to resolve a tag, not the only way to hold one (MEAS-A-060 builds a defective epoch on purpose)
                                               // [corrected 2026-10-06, when the builder was written: the first text took an `Sp3PositionRecord` and a header only, and gave no epoch and no leap table; a record carries
                                               //  no epoch (it belongs to the `Sp3Epoch` that holds it) and a GPS, TAI or UTC calendar needs the table to become an instant]

angle_observation(iod: IodObservation, sites: OpticalRegistry, reduction: AngleReduction,
                  of_date: optional<EquinoxOfDate>, atmosphere: optional<AngleAtmosphere>, leaps: LeapTable)
        -> Result<AngleObservation>            // F-005, F-015, F-016, F-022; there is no overload without `reduction` (MEAS-A-082); the decoders' IOFM-F-016/-F-017 with their own ids
AngleReduction { Astrometric, Geometric, ApparentRefracted }        EquinoxOfDate { Mean, True }        AngleAtmosphere { Atmosphere, wavelength_um }
AngleObservation { epoch (the UTC calendar of the line, resolved), kind: RaDec | AzEl, a_rad, b_rad, sigma_rad (optional), reduction, frame: Icrf | MeanOfDate | TrueOfDate | Local,
                   gcrs_to_frame (the rotation of an "of date" frame at the epoch, computed when the observation is built — the models take no leap table — and the identity otherwise), site, atmosphere (optional) }
                                               // [corrected 2026-10-06, when the builder was written: the first text had the atmosphere as an `Atmosphere` alone (the refraction needs the wavelength), no `Local` frame
                                               //  for azimuth and elevation, and no rotation for the "of date" frames]
```

### 5.4 The models

```
model_range(obs: RangeObservation, station: StationTrack, target: Trajectory)
        -> Result<Modelled<RangeTwoWay>>       // F-010, F-011, and a dependency's refusal with its own id
model_position(obs: PositionObservation, target: Trajectory, earth: EarthOrientation)
        -> Result<Modelled<PositionGeometric>>   // residual = observed − modelled, a vector in the observation's frame, metres; partials [M 0], M the rotation GCRS → the observation's frame at the epoch (the identity in GCRS);
                                                 // applied: the frame, M, and the omitted terms (the ITRS realisation differences of §6.4 for an ITRS observation)
model_radec(obs: AngleObservation, station: StationTrack, target: Trajectory, motion: EarthMotion)
        -> Result<ModelledRaDec>               // F-011, F-015, F-018, F-026, and a dependency's refusal with its own id: ra_rad(), dec_rad(), observed_…, residual_ra_rad() (wrapped), residual_dec_rad(), partials 2×6, applied
model_azel(obs: AngleObservation, station: StationTrack, target: Trajectory, earth: EarthOrientation)
        -> Result<ModelledAzEl>                // F-010, F-011, F-015, F-018, F-022, F-026: azimuth_rad(), elevation_rad(), … as above

Modelled<Obs>   { value (unit in the accessor's name), observed, residual, epoch (bounce_tt | emission_tt | epoch_utc),
                  partials: Partials<GCRS>       // 1×6 | 2×6 | 3×6, SI; the velocity columns zero to first order (MEAS-R-060)
                  applied: Applied }
Applied         { legs[2]{ geometric range, light time, Δ_atm, Δ_S, ĝ }, ztd, mapping, δ_com + citation, meteorology, held,
                  registry release strings, post_seismic_override_used, passes per leg,
                  omitted: list of { name, magnitude, source } — never empty for a range }
```

The **light-time core** (`solve_two_way(events, station, target, leg_delay)`, `solve_emission(…)`) is a public, tested building block: the full models pass the real leg delay (atmosphere plus Shapiro); the closed-form tests pass **zero** —
so no switch on a physical term exists for a caller to turn off.

### 5.5 The term functions (public, tested building blocks)

```
Atmosphere { pressure_hpa, temperature_c, relative_humidity (a fraction, 0–1) }
validate_atmosphere(Atmosphere, wavelength_um) -> Result<void>                                // F-022
water_vapour_pressure_hpa(relative_humidity, temperature_c) -> double                          // MEAS-R-022, Marini & Murray eq. (22)
zenith_delay(latitude_rad, height_m, pressure_hpa, water_vapour_hpa, wavelength_um)
        -> Result<ZenithDelay { total_m, hydrostatic_m, wet_m }>                               // MEAS-R-020; F-022 for non-physical inputs
mapping_fcula(sin_e, latitude_rad, height_m, temperature_c) -> Mapping { value, d_dsin_e }     // MEAS-R-021
mapping_fculb(sin_e, latitude_rad, height_m, day_of_year)   -> double                          // MEAS-R-021
TroposphereModel::make(latitude_rad, height_m, Atmosphere, wavelength_um) -> Result<TroposphereModel>      // F-022
TroposphereModel::leg(sin_e) -> Result<LegDelay { delay_m, d_delay_d_sin_e, ztd_m, mapping }>              // MEAS-R-023; F-010
shapiro_leg(r1_m, r2_m, rho_m) -> Result<ShapiroLeg { delay_m, d_rho, d_r1, d_r2 }>                         // MEAS-R-024; F-010
aberration_apply(n, beta) -> Vec3                      aberration_remove(n, beta) -> Vec3                      // MEAS-R-053: A and its exact inverse D = A(n; −β); with their Jacobians (3 × 3, on the tangent plane)
refraction_constants(AngleAtmosphere) -> Result<RefractionConstants { a, b }>                                   // MEAS-R-054: eraRefco; F-022
refracted_zenith_distance(z_vacuum, RefractionConstants) -> double                                                // the observed z_o of z_v = z_o + A tan z_o + B tan³ z_o (Newton, 1e-14); its derivative dz_o/dz_v
solve_emission(observation_epoch, observer_m, Trajectory, pass_limit) -> Result<Emission { epoch, light_time, range, r, v, direction, passes }>   // MEAS-R-051; F-011
wrap_to_pi(angle) -> double                                                                                        // MEAS-R-056: (−π, π]
```

The relative humidity of the CRD record `20` is in per cent and is divided by 100 where the record enters the model; the angle models take a fraction.

---

## 6. Precision and accuracy

### 6.1 Budgets

Each row names the physical quantity it is a budget for and writes its arithmetic where `tools/budgetcheck.py` evaluates it (the rows are the only numbers with no test behind them).

| id | quantity | budget | because |
|---|---|---|---|
| `MEAS-P-1` | a TT- against a TCG-compatible length, if the two were mixed, on a LAGEOS-like range | 6.969 290 134 × 10⁻¹⁰ × 6.6 × 10⁶ m = **4.6 mm** | `TN36-1`'s `L_G`: why the model keeps TT-compatible lengths and times throughout (§3.1) |
| `MEAS-P-2` | one leg's light time at a LAGEOS-like slant range | 6.6 × 10⁶ m × 3.335 640 952 × 10⁻⁹ s/m = **22.0 ms** | the quantity the fixed point iterates (1/*c* = 3.335 640 952 × 10⁻⁹ s m⁻¹) |
| `MEAS-P-3` | an equatorial station's displacement during a two-way flight | 0.4651 km s⁻¹ × 44.03 ms = **20.5 m** | why the station's position differs between transmit and receive and the Earth's rotation is in the model, not in the caller (`MEAS-R-030`) |
| `MEAS-P-4` | 1 ps of time-of-flight error, as a one-way range | 299 792 458 m s⁻¹ × 1 ps × 0.5 = **0.15 mm** | the normal-point precision class (`CRD2`: picosecond time of flight) |
| `MEAS-P-5` | 1 µs of UT1 or station-clock error, as an equatorial station's position | 0.4651 km s⁻¹ × 1 µs = **0.465 mm** | the surface speed at the equator |
| `MEAS-P-6` | the Shapiro scale 2 *GM*⊕/*c*² | 2 × 3.986 004 415 × 10¹⁴ m³ s⁻² × 1.112 650 056 × 10⁻¹⁷ s² m⁻² = **8.870 mm** | `TN36-11` eq. (11.17); the term is this scale times a logarithm of order 1 (5.8 mm at zenith, 9.9 mm at 10° for a LAGEOS-like geometry) |
| `MEAS-P-7` | the Earth's annual aberration, in radians | 29.78 km s⁻¹ × 3.335 640 952 × 10⁻⁶ s/km × 1 rad = **9.934 × 10⁻⁵ rad** | `v_E/c`: the largest astrometric-minus-geometric difference of a near-Earth target (§3.5) |
| `MEAS-P-8` | the same, in seconds of arc | 9.934 × 10⁻⁵ rad × 206 264.806 as/rad = **20.49 as** | as `MEAS-P-7`; the plan's "20.5″" |
| `MEAS-P-9` | the diurnal aberration at the equator, in radians | 0.4651 km s⁻¹ × 3.335 640 952 × 10⁻⁶ s/km × 1 rad = **1.551 × 10⁻⁶ rad** | what `ApparentRefracted` applies |
| `MEAS-P-10` | the same, in seconds of arc | 1.551 × 10⁻⁶ rad × 206 264.806 as/rad = **0.32 as** | as `MEAS-P-9` |
| `MEAS-P-11` | the zenith delay's own accuracy | "overall rms errors for the total zenith delay below 1 mm" [`TN36-9` §9.1.1] — **1 mm**, a figure from the source and not from any result | `MEAS-A-020`'s asserted tolerance |

### 6.2 The finite-difference gate (G1), **frozen before any run** (plan §4 rule 7; `STM-R-004`, `STM-R-005`, `STM-P-1`)

Written 2026-10-06, before the first run of the gate — before any measurement-model code exists — and reproduced by `tools/measmod_fd_sizing.py --scan --check` (`PROVENANCE.md` §39.4). **Nothing below is changed after a run**; a run that misses it is reported with the criterion as written
(rule 7: the thresholds are not chosen by whoever is judged), and a correction is a dated amendment that keeps the first text visible.

**What is compared.** For each observation type and each component *k* of the target's state — the three position directions, then the three velocity directions — the central difference
`f̂ₖ(h) = [ y(x + h eₖ) − y(x − h eₖ) ] / (2h)` of the **model's output** `y`, taken over a **trajectory family** that varies the target's state at the nominal bounce (emission, observation) epoch by `± h eₖ` — **time-fixed** (`MEAS-R-060`) — and **re-solves the light time for every varied trajectory**.
The family is a drift: `r(t) = r* + δr + (v* + δv)(t − t*) + ½ a* (t − t*)²`, with `a*` the nominal acceleration, so the variation propagates exactly (no dynamics enters; the largest `h` moves the bounce by 3.3 µs).
**The target's speed and its direction** — *[added 2026-10-06, before the first run; the first text named neither]*: 5.7 km s⁻¹ (LAGEOS-like) and 7.5 km s⁻¹ (LEO-like), each geometry in **two directions** — **across** the line of sight (a high pass) and **along** it (receding, the whole speed radial). The second is the geometry in which every velocity-dependent term of the row — the event-2 shift `a_d·v_b δt_b` and the `a_u·v_b` of the denominator — is at its largest, `v/c` = 1.9 × 10⁻⁵ (2.5 × 10⁻⁵ for LEO); across the line of sight they enter only at second order, and a gate that ran only there could not see a sign error in them. The frozen criteria and numbers are unchanged: `F`, `ν`, `ε(h)` and `B_pred` belong to the geometry, not to the direction of the velocity (the odd-order terms of a central difference are the geometry's; the light-time coupling adds a fraction `v/c` of them).
Five sizes for the position directions, `h ∈ {10, 30, 100, 300, 1000} m`, and four for the velocity directions, `h_v ∈ {0.01, 0.1, 1, 10} m s⁻¹`.
**The band** of an observation type is the spread of its five position estimates, `B = max over components of (max_h f̂ₖ − min_h f̂ₖ)`.

**What a central difference of this model carries — three errors, as `STM-P-1`'s argument, with one difference.** The estimate's error at step `h` is bounded by `εₖ(h) = Tₖ(h) + N(h)`:

| | size | origin |
|---|---|---|
| truncation `T(h)` | `h² F / 6` | the third derivative `F` of the output along the direction |
| noise `N(h)` | `ν / h` | `ν` the bound of one evaluation's round-off; each evaluation carries it independently and the difference divides by `2h`, so it is amplified by the very step that suppresses truncation |
| **light-time tolerance** | `c ε_τ / h` | **absent here, by construction**: the iteration runs to the double-precision fixed point (`MEAS-R-031`); a tolerance `ε_τ = 10⁻¹² s` would add 3.5 × 10⁻⁶ at the optimal step (`MEAS-P-16`), 3.6 × 10⁴ times the best agreement the gate can show |

**Where the noise bound comes from.** `ν = 3 ulp` **of the largest operand** — the target's position components, 1.227 × 10⁷ m for a LAGEOS-like orbit (ulp 2⁻²⁹ m = 1.8626 × 10⁻⁹ m), 7.4 × 10⁶ m for a LEO-like orbit (ulp 9.3 × 10⁻¹⁰ m) — not of the output (6.6 × 10⁶ m for a LAGEOS-like range): the
operands' ulp is the larger. **This refines the round-8 proposal** (`REPORT` D5, `fd_sizing.py`), which took three ulp of the range, `δ = 2.8 × 10⁻⁹ m`, and so understated the noise by a factor 2 for a LAGEOS-like geometry; the refinement is made here, before any run, and the round-8 figures are not used.
For an angle `ν_a = 3 [ulp(2π) + ulp(|r|)/ρ]`: the output's own ulp (right ascension runs to 2π) and the operand's ulp seen as an angle.

**The truncation coefficients.** For a **range**, a displacement `h` along a direction with `μ = e·ĝ`: `f‴ = 3 μ (1 − μ²)/ρ²`, at most `F = 1.1547/ρ²` (at `μ = 1/√3`; analytic). For an **angle** (right ascension, declination, azimuth, elevation), with the complex-logarithm expansion of `atan2` and an exact power-series scan over displacement directions and over lines of sight with |latitude| ≤ 30° (`tools/measmod_fd_sizing.py --scan`):
the third derivative of right ascension is `2 Im[(w₁/w₀)³]`, bounded by **`2/(ρ³ cos³ φ_max)`** (3.079/ρ³ at 30°; the scan's maximum 3.075/ρ³), and of declination 2.23/ρ³; so **`F_a = 2/(ρ³ cos³ φ_max)` is frozen as the angle coefficient with `φ_max` = 40°** (`F_a = 4.45/ρ³`), and the angle gates' test geometry is restricted to |declination| and |elevation| ≤ 40°.
The partial of an angle is `~1/(ρ cos φ)` (8.3 × 10⁻⁷ to 1.1 × 10⁻⁶ rad m⁻¹ at ρ = 1.2 × 10⁶ m).

**The frozen numbers** (the geometry column names the target's geocentric distance that, with the slant range `ρ`, gives the stated elevation by the law of cosines — *[corrected 2026-10-06, before any run: the first text labelled the first two rows "elevation ≈ 40°" and "≈ 15°" without saying which target distance that needs; with ρ = 6.6 × 10⁶ m and the target at 1.227 × 10⁷ m the elevation is 52°, and 15° at ρ = 1.5 × 10⁶ m needs 6.92 × 10⁶ m. Every number below depends on ρ, F and ν only, and ν on the operands' ulp, which the 6.92 × 10⁶ m and 7.2 × 10⁶ m targets share with the 7.4 × 10⁶ m the sizing used (one binade, 9.3 × 10⁻¹⁰ m): none changes]*; dimensionless for a range, rad m⁻¹ for an angle; `ν` and `F` as above; `T = h² F/6`, `N = ν/h`, `ε = T + N`):

| geometry | `ρ` | `F` | `ν` | `h` = 10 m | 30 m | 100 m | 300 m | 1000 m | optimal `h*` / best | **`B_pred = max ε`** | window [`B_pred`/10, 10 `B_pred`] |
|---|---|---|---|---|---|---|---|---|---|---|---|
| range, LAGEOS-like (target at 1.227 × 10⁷ m, so elevation ≈ 52°) | 6.6 × 10⁶ m | 2.651 × 10⁻¹⁴ m⁻² | 5.59 × 10⁻⁹ m | 5.6 × 10⁻¹⁰ | 1.9 × 10⁻¹⁰ | 1.0 × 10⁻¹⁰ | 4.2 × 10⁻¹⁰ | 4.42 × 10⁻⁹ | 86 m / 9.8 × 10⁻¹¹ | **4.42 × 10⁻⁹** | [4.4 × 10⁻¹⁰, 4.4 × 10⁻⁸] |
| range, LEO-like (target at 6.92 × 10⁶ m, so elevation ≈ 15°) | 1.5 × 10⁶ m | 5.132 × 10⁻¹³ m⁻² | 2.79 × 10⁻⁹ m | 2.9 × 10⁻¹⁰ | 1.7 × 10⁻¹⁰ | 8.8 × 10⁻¹⁰ | 7.7 × 10⁻⁹ | 8.55 × 10⁻⁸ | 25 m / 1.7 × 10⁻¹⁰ | **8.55 × 10⁻⁸** | [8.6 × 10⁻⁹, 8.6 × 10⁻⁷] |
| angle (RA, Dec, Az, El), LEO-like (target at 7.2 × 10⁶ m, so elevation 40°), \|φ\| ≤ 40° | 1.2 × 10⁶ m | 2.575 × 10⁻¹⁸ rad m⁻³ | 4.99 × 10⁻¹⁵ rad | 5.4 × 10⁻¹⁶ | 5.5 × 10⁻¹⁶ | 4.3 × 10⁻¹⁵ | 3.9 × 10⁻¹⁴ | 4.29 × 10⁻¹³ | 18 m / 4.2 × 10⁻¹⁶ | **4.29 × 10⁻¹³** | [4.3 × 10⁻¹⁴, 4.3 × 10⁻¹²] |
| position (linear) | — | 0 | 5.59 × 10⁻⁹ m | 5.6 × 10⁻¹⁰ | 1.9 × 10⁻¹⁰ | 5.6 × 10⁻¹¹ | 1.9 × 10⁻¹¹ | 5.6 × 10⁻¹² | — | 5.6 × 10⁻¹⁰ | upper half only |
| velocity columns (range, LAGEOS-like) | — | 0 (the analytic row is exactly 0) | 5.59 × 10⁻⁹ m | `h_v` = 0.01 m s⁻¹: 5.6 × 10⁻⁷ s | 0.1: 5.6 × 10⁻⁸ s | 1: 5.6 × 10⁻⁹ s | 10: 5.6 × 10⁻¹⁰ s | — | — | — | `ε` only |

| id | quantity | budget | because |
|---|---|---|---|
| `MEAS-P-12` | the per-evaluation noise bound of a LAGEOS-like range | 3 × 1.8626 × 10⁻⁹ m = **5.59 × 10⁻⁹ m** | three ulp of the largest operand, 2⁻²⁹ m |
| `MEAS-P-13` | the range coefficient `F` at ρ = 6.6 × 10⁶ m | 1.1547 × 2.2957 × 10⁻¹⁴ m⁻² = **2.651 × 10⁻¹⁴ m⁻²** | `1.1547/ρ²`, 1/ρ² = 2.2957 × 10⁻¹⁴ m⁻² |
| `MEAS-P-14` | the truncation error at `h` = 1000 m, LAGEOS-like | 1000 m × 1000 m × 2.651 × 10⁻¹⁴ m⁻² × 0.166 666 7 = **4.42 × 10⁻⁹** | `h² F/6`: the band's size, `B_pred` |
| `MEAS-P-15` | the noise at `h` = 10 m | 5.59 × 10⁻⁹ m × 0.1 m⁻¹ = **5.59 × 10⁻¹⁰** | `ν/h`, the first entry's `ε` |
| `MEAS-P-16` | what a 10⁻¹² s light-time tolerance would add at `h*` = 86 m | 10⁻¹² s × 299 792 458 m s⁻¹ × 0.011 66 m⁻¹ = **3.5 × 10⁻⁶** | `c ε_τ/h*`: why `MEAS-R-031` iterates to the fixed point |
| `MEAS-P-17` | the angle's truncation error at `h` = 1000 m | 1000 m × 1000 m × 2.575 × 10⁻¹⁸ rad m⁻³ × 0.166 666 7 = **4.29 × 10⁻¹³ rad m⁻¹** | `h² F_a/6` at ρ = 1.2 × 10⁶ m, φ_max = 40° |
| `MEAS-P-22` | one ulp of the Earth-rotation angle, at the station (Amendment A1, below) | 2.8422 × 10⁻¹⁴ × 5580552 m = **1.586 × 10⁻⁷ m** | `u R⊥`, `u` = 2⁻⁴⁵ = 2.8422 × 10⁻¹⁴ rad (an angle is a number of radians), `R⊥` the station's distance from the Earth's axis |
| `MEAS-P-23` | the chain's bound of one evaluation of the LEO-like range output, event 2 (Amendment A1) | 3 × 1.586 × 10⁻⁷ m × 0.9659 × 0.5 = **2.298 × 10⁻⁷ m** | `3 u R⊥ · cos 15° · ½`: three ulp, the line of sight along the error, one leg moving |

**If only machine round-off mattered**, the best agreement would be `ε^{2/3}` ≈ 3.7 × 10⁻¹¹ (`ε` = 2.2 × 10⁻¹⁶), and a tolerance taken from that figure would **fail a correct implementation** for the LEO-like range (best 1.7 × 10⁻¹⁰, truncation-dominated) — the trap `STM-P-1` names.

**The registered criteria**, restated verbatim in the test and not tuned after a run:

- **(a) the band is neither degenerate nor wild**: `B_pred/10 ≤ B ≤ 10 B_pred` for each row of the table with a window (the range and angle rows). For a position row — a linear function, truncation identically zero — only the upper half applies: there is no band to be degenerate, and the discriminating criterion is (b).
- **(b) the analytic row lies within the first-principles error of every estimate**: for every size `h` and every component *k*, `|f̂ₖ(h) − aₖ| ≤ εₖ(h)`, `aₖ` the analytic row. This is "the analytic row lies within the band", each end widened by the **prediction** of its own error — **a deviation from `STM-R-004`'s literal "inside the spread", named and reasoned**:
  `STM-A-001` compared a *propagated* state, whose integrator noise dominates and so brackets the true value on both sides. A finite difference of a smooth, deterministic function carries a **one-sided** truncation error, so the true derivative lies *beside* the spread (by up to `ε(h_min)`), and "inside the spread" would pass or fail a correct row on the sign of a rounding error at the smallest step.
  The test **reports** whether the analytic row also lies inside the raw spread; it is not a criterion.
- **(c) the velocity columns**: the analytic entries are exactly 0, and every estimate satisfies `|f̂ₖ(h_v)| ≤ ν/h_v` (the table's last row).

**The gate must be able to fail** (rule 5): `MEAS-A-044` runs it on a row with the light-time factors removed — the geometric `½(ĝ_u + ĝ_d)` (the station-motion factors, 1.5 × 10⁻⁶, and for event 2 the target's `ĝ·v/c`, 1.9 × 10⁻⁵, absent) — and on a row without the event-2 shift; both must fail (b) by at least 10³; and for the angles, without the aberration Jacobian, without `dz_o/dz_v`, and without the emission-epoch shift `δt_e`.

**First run of the range gate, 2026-10-06** — *recorded here, not an amendment: nothing above was changed by it.* The gate of `MEAS-A-042` … `-045` was run once, on the real Earth-orientation chain, in 8 cases (two geometries × two events × two velocity directions). **(a) held in 8 of 8** (`B/B_pred` 0.976 – 1.097) and **(c) held in 8 of 8** (every estimate exactly 0); **(b) missed in 7 of 8** (worst `|f̂ − a|/ε`: LAGEOS-like 0.98, 1.43, 1.18, 1.18; LEO-like 17.1, 5.9, 7.9, 7.9). Two causes were found, both in the sizing above and neither in the model. First, the real chain's Earth-rotation angle carries a double-precision floor: one ulp of an unreduced argument of about 171 rad, 2⁻⁴⁵ rad, which is 1.6 × 10⁻⁷ m of station position (measured peak 1.61 × 10⁻⁷ m against an exact rigid rotation of the same matrix) — and the noise bound `ν`, three ulp of the target's components, does not contain it. Second, `F` is the geometric range's third derivative alone, to which the mapping function at 15° adds 7.2 × 10⁻⁴ of itself (1.0007 `ε` at h = 1000 m, even over an exact rotation). Over an exact rigid rotation of the station the gate gives at most 0.983 `ε` (LAGEOS-like) and at most 0.999 `ε` for h ≤ 300 m (LEO-like). **The amendment follows** (ruled by the manager the same day); `PROVENANCE.md` §39.11 holds the data.

**Amendment A1, 2026-10-06 — made AFTER the first run's miss, so post hoc by its nature, on the manager's ruling (`plan/subplan_L6/L6-4.md`, ruled 2026-10-06); it changes the sizing only.** The structure of the gate, the criteria (a), (b) and (c), the geometries (and the azimuth rule that places them), the sizes and the velocity directions are exactly those frozen above, which stand as first written. The first run stays recorded as it ran: the frozen sizing missed (b) in 7 of 8 cases, (a) and (c) held, and the wrong rows failed. The sizing had left out two terms, each derived below from first principles and not read off a measurement. The Earth-rotation floor was predicted from the documented formula (1.59 × 10⁻⁷ m) before it was measured directly (1.61 × 10⁻⁷ m) — but after the gate run whose jumps pointed at it; the second term was found by the diagnostic that switches the atmosphere off. **A different geometry is not open** (a geometry chosen after a miss is what rule 7 forbids), and none was chosen.

**(i) `ν` gains the station chain's floor.** The real orientation chain forms the Earth-rotation angle unreduced (`2π (f + 0.779… + 0.0027378 t)`, between 128 and 256 rad for the years 2019 – 2039), so its last bit is `u` = 2⁻⁴⁵ rad, and a station at the distance `R⊥` from the Earth's axis moves by `u R⊥` between two adjacent values of the angle: **1.586 × 10⁻⁷ m** at Yarragadee (`R⊥` = 5 580 552 m, `MEAS-P-22`), along the station's velocity, `ê`. The sizing's own convention is three ulp, so one evaluation of the chain is bounded by `3 u R⊥` = 4.76 × 10⁻⁷ m. It enters the range output as it enters the light time: in **event 2** the up leg's station epoch is the tag, the same in every evaluation, so its error cancels exactly in a difference, and only the down leg's station epoch moves with the perturbation (weight ½ of the range); in **event 1** both legs' station epochs move (½ + ½). So

`ν_chain = 3 u R⊥ · |ĝ·ê| · w`, `w` = ½ (event 2) or 1 (event 1), `|ĝ·ê| = cos(el) |cos(az)|` for the geometry — 0.1593 for LAGEOS-like (52°, azimuth 105°), 0.9659 for LEO-like (15°, azimuth 0°; the azimuths are the ones the frozen rule selects) —

| | event 2 | event 1 |
|---|---|---|
| LAGEOS-like | 3.79 × 10⁻⁸ m | 7.58 × 10⁻⁸ m |
| LEO-like | 2.30 × 10⁻⁷ m (`MEAS-P-23`) | 4.60 × 10⁻⁷ m |

The noise bound of the **real-chain configuration** is `ν_real = ν + ν_chain` (the frozen `ν` of the table above plus this); that of the **rigid configuration** is the frozen `ν`.

**(ii) `F` is a bound over the stencil, for the whole modelled range.** A central difference's error is `h²/6 · f‴(ξ)` at some point `ξ` of `[x − h, x + h]`, so (b) is a bound only if `F` bounds `|f‴|` over the whole of that interval, not at its centre, and for the whole modelled range: **the geometry with the light-time coupling, the troposphere through the mapping function's derivatives, and the Shapiro term.**

`F(h) = F_geo(h) + 1.01 · F_trop(h)`

- `F_geo(h) = 1.1547005 (1 + κ)³ / (ρ (1 − κ) − h)²`, `κ = (v + 2 v_s)/c`, `v` the target's speed and `v_s` = 465.1 m s⁻¹, the equatorial surface speed, which no station exceeds (`κ` = 2.21 × 10⁻⁵ and 2.81 × 10⁻⁵): the analytic maximum of `3μ (1 − μ²)` at the nearest distance the stencil reaches, with the light-time coupling's `v/c`. **Verified** in 60-digit arithmetic against the closed-form two-way light time with both bodies in uniform motion (the two quadratics of `MEAS-A-030`/`-031`), by third differences over 168 displacement directions (the cone `μ = e·ĝ` from 0.45 to 0.70 at 24 azimuths) and every stencil point, both events, both velocity directions: the numerical supremum is **0.99989** (LAGEOS-like) and **0.99985** (LEO-like) of the formula at the worst size — the formula bounds it.
- `F_trop(h)` the supremum over **every** unit displacement direction (a 3° × 4° grid of the sphere; a 1° × 1.5° grid changes the result by 5 × 10⁻⁴ and 8 × 10⁻⁴) and every point of the stencil of `|d³/dξ³|` of one leg's `ZTD · m(sin e) + Δ_S` — the zenith delay of the first normal point's weather times the FCULa mapping function, plus the Shapiro term — by third differences of the formulas themselves, in an independent implementation (TN36 equations 9.3 – 9.10 and 11.17; its FCULa reproduces the IERS printed case to 2 × 10⁻¹⁶); the 1.01 allows for the grid. **Shapiro alone is 1.3 × 10⁻⁹ (LAGEOS-like) and 1.6 × 10⁻⁹ (LEO-like) of `F_geo`: negligible, and included.** The atmosphere alone is 3.7 × 10⁻⁶ of `F_geo` at 52° and **1.5 × 10⁻³ at 15°**. `F_trop` (m⁻²): LAGEOS-like 9.69 × 10⁻²⁰ at every size; LEO-like 7.73 × 10⁻¹⁶ (h = 10 m) to 7.80 × 10⁻¹⁶ (h = 1000 m).

**The amended frozen numbers** (`tools/measmod_fd_sizing.py --scan --check` reproduces them; `ε = h² F(h)/6 + ν/h`, with the frozen `ν` in the rigid column and `ν_real` in the real-chain columns):

| range, LAGEOS-like (azimuth 105°) | `F(h)` (m⁻²) | `ε`, rigid | `ε`, real chain, event 2 | `ε`, real chain, event 1 |
|---|---|---|---|---|
| `h` = 10 m | 2.6511 × 10⁻¹⁴ | 5.59 × 10⁻¹⁰ | 4.35 × 10⁻⁹ | 8.14 × 10⁻⁹ |
| 30 m | 2.6512 × 10⁻¹⁴ | 1.90 × 10⁻¹⁰ | 1.45 × 10⁻⁹ | 2.72 × 10⁻⁹ |
| 100 m | 2.6512 × 10⁻¹⁴ | 1.00 × 10⁻¹⁰ | 4.79 × 10⁻¹⁰ | 8.58 × 10⁻¹⁰ |
| 300 m | 2.6514 × 10⁻¹⁴ | 4.16 × 10⁻¹⁰ | 5.43 × 10⁻¹⁰ | 6.69 × 10⁻¹⁰ |
| 1000 m | 2.6519 × 10⁻¹⁴ | 4.43 × 10⁻⁹ | 4.46 × 10⁻⁹ | 4.50 × 10⁻⁹ |
| **`B_pred`** (window) | | **4.43 × 10⁻⁹** (4.4 × 10⁻¹⁰, 4.4 × 10⁻⁸) | **4.46 × 10⁻⁹** (4.5 × 10⁻¹⁰, 4.5 × 10⁻⁸) | **8.14 × 10⁻⁹** (8.1 × 10⁻¹⁰, 8.1 × 10⁻⁸) |

| range, LEO-like (azimuth 0°) | `F(h)` (m⁻²) | `ε`, rigid | `ε`, real chain, event 2 | `ε`, real chain, event 1 |
|---|---|---|---|---|
| `h` = 10 m | 5.1406 × 10⁻¹³ | 2.88 × 10⁻¹⁰ | 2.33 × 10⁻⁸ | 4.62 × 10⁻⁸ |
| 30 m | 5.1407 × 10⁻¹³ | 1.70 × 10⁻¹⁰ | 7.83 × 10⁻⁹ | 1.55 × 10⁻⁸ |
| 100 m | 5.1412 × 10⁻¹³ | 8.85 × 10⁻¹⁰ | 3.18 × 10⁻⁹ | 5.48 × 10⁻⁹ |
| 300 m | 5.1426 × 10⁻¹³ | 7.72 × 10⁻⁹ | 8.49 × 10⁻⁹ | 9.26 × 10⁻⁹ |
| 1000 m | 5.1475 × 10⁻¹³ | 8.58 × 10⁻⁸ | 8.60 × 10⁻⁸ | 8.63 × 10⁻⁸ |
| **`B_pred`** (window) | | **8.58 × 10⁻⁸** (8.6 × 10⁻⁹, 8.6 × 10⁻⁷) | **8.60 × 10⁻⁸** (8.6 × 10⁻⁹, 8.6 × 10⁻⁷) | **8.63 × 10⁻⁸** (8.6 × 10⁻⁹, 8.6 × 10⁻⁷) |

The velocity columns (c) take the configuration's `ν`: `|f̂ₖ(h_v)| ≤ ν/h_v`.

**Both configurations are asserted** (`MEAS-A-042` … `-045`). The **real chain** — the model `L7` will use, and the only place the real chain's station-velocity path meets a finite difference — with `ν_real`: at small `h` it is noise-limited, which is the truth of the model, and it must still separate a wrong row whose error is at the level of `v/c`. And an **exact rigid rotation** — the real chain's own matrix at the tag, carried to nearby epochs by a rigid rotation at the chain's own rate about its own axis, round-off only — with the frozen `ν`, the configuration the frozen noise term describes: it checks the formula at full tightness.

**The gate must still be able to fail, in both** (`MEAS-A-044`, `-045`, rule 5; written here, before the second run). In each configuration the model's row passes (b) and **every wrong row fails (b)** (a violation above 1): the geometric row and the other event's formula, in both events, both geometries and both velocity directions. **Every wrong row whose difference from the model's row, `|Δrow| / |row|`, is at least 1 × 10⁻⁵ — the target's `ĝ·v/c` (1.9 × 10⁻⁵ and 2.5 × 10⁻⁵), or the atmosphere's gradient at 15° — fails by at least 10³ `ε`**, the test computing that difference from the rows themselves; and in the rigid configuration the two wrong rows of every along-the-line-of-sight case fail by at least 10³ `ε`, as first written. The double count of `MEAS-A-045` (1.9 × 10⁻⁵ relative) is shown in both. A miss of any of these is stopped and reported, not tuned.

**(iii) The chain's floor, asserted on its own** (`MEAS-A-046`), and stated as the model's **output noise floor**: the real chain's station position differs from an exact rigid rotation of its own matrix by at most `3 u R⊥` = 4.76 × 10⁻⁷ m at every one of 601 epochs 1 × 10⁻¹⁰ s apart about the tag, and by **at least `0.5 u R⊥`** at the peak (so that the floor is shown to exist and the bound able to fail); the first measurement gave 1.61 × 10⁻⁷ m, 1.01 `u R⊥`. Every modelled range carries about **0.16 µm** of noise from the orientation chain's double-precision angle: one ulp of the argument of ERFA's Earth-rotation angle, not a property of the model's equations.

**(iv) The angle gate** (item 6; it has not run) takes the same two terms before it first runs, frozen with its numbers at item 6 and run once in the same two configurations. For (i), carried through the angle as it enters, the chain term is **zero**: an angle observation puts the observer at the **observation epoch** (`MEAS-R-051`, `-053`, `-054`), which no perturbation of the target moves — the emission epoch it solves for is the target's, evaluated through the `Trajectory` — so the station, the Earth's orientation and the Earth's motion are evaluated at the same epoch in every stencil evaluation and their floors cancel in the difference, exactly as event 2's up leg does; the gate shows it, the real-chain and rigid configurations having to agree to arithmetic noise. For (ii), `F_a` becomes a stencil bound for the whole modelled angle — the geometry, the aberration's `v/c` coupling, and the refraction (`ApparentRefracted`, through the derivatives of `A tan z + B tan³ z`) wherever the reduction applies it.

**(v) The angle gate, frozen 2026-10-06 — written and committed before any angle-gate code exists, and so before its first run** (item 6; `tools/measmod_fd_sizing.py --scan --check` reproduces every number below, `PROVENANCE.md` §39.13). It is the gate of this section — its structure, its criteria (a), (b) and (c), its sizes `h ∈ {10, 30, 100, 300, 1000} m` and `h_v ∈ {0.01, 0.1, 1, 10} m s⁻¹`, the trajectory family varied **time-fixed at the nominal emission epoch** with the light time re-solved for every varied trajectory — applied to the two outputs of an angle model, `y = (α, δ)` or `(A, E)`.
The central difference of a right ascension or an azimuth is the **wrapped** difference divided by `2h` (`MEAS-R-056`); the band `B` is the largest spread of the five estimates over **both outputs and the three components**; (b) is asserted for each output, each component and each size; (c) for each output and component. Both configurations of Amendment A1 are run: the **real chain**, and an **exact rigid rotation** of its own matrix whose reference epoch lies **0.01 s before** the observation epoch — so that the two observers differ, at the observation epoch, by the chain's floor and by the rigid approximation, a difference the gate **measures and asserts below 1 × 10⁻⁶ m before it compares anything**.

**The geometry — chosen by rule from the geometry alone, before any model is run.** Yarragadee through the real chain at the epoch of the first normal point; a target 30° above the horizon at the slant range ρ = 1.2 × 10⁶ m (7.053 × 10⁶ m from the geocentre — in the binade of the sizing's 7.2 × 10⁶ m, the same ulp, 2⁻³⁰ m) moving at 7.5 km s⁻¹ **across** the line of sight and **along** it; the observation's weather and wavelength are the first normal point's (976.70 hPa, 37.15 °C, 12 %, 0.532 µm: `eraRefco` gives `A` = 2.4844 × 10⁻⁴, `B` = −3.1238 × 10⁻⁷); the Earth's motion is de440s's.
The azimuth is the first of 0°, 15°, …, 345° (from north) whose line of sight has `|declination| ≤ 39.5°` in the frame of the observation (the sizing's class: 40° after the aberration's 0.012°; for azimuth and elevation the 30° elevation lies in the class 20° … 39.5° and every azimuth does) **and a visibility score of at least 0.3**, the score being the largest over the three GCRS axes of `|2 Im[(w₁/w₀)³]| / (2/cos³ 40°)` with `w₀ = g_x + i g_y` and `w₁ = e_x + i e_y` the line of sight and the displacement axis in the frame of the output — so that the band is at least 0.3 of its prediction, three times the lower limit of (a). The rule selects **azimuth 0°** in all four gates (the geometry test `MEAS-A-096g` computes it with no model):

| gate | outputs and frame | declination (elevation) | score |
|---|---|---|---|
| `MEAS-A-096` Astrometric | `(α, δ)`, the ICRF | +31.01° | 0.6548 |
| `MEAS-A-097` Geometric | `(α, δ)`, the ICRF | +31.01° | 0.6548 |
| `MEAS-A-098` ApparentRefracted | `(A, E)`, local north-east-up | 30° | 0.4853 |
| `MEAS-A-099` Astrometric, true equator and equinox of date | `(α, δ)`, of date | +30.95° | 0.6549 |

**The sizing** — `ε(h) = h² F_a(h)/6 + ν_a/h`, with the two terms Amendment A1 (iv) obliged it to take:
- **`ν_a = 3 [ulp(2π) + ulp(|r|)/ρ]` = 4.9928 × 10⁻¹⁵ rad**, as first frozen (`MEAS-P-25`, `-26`), **in both configurations: the chain's floor is zero.** The observer, the Earth's orientation and the Earth's motion are evaluated at the observation epoch and at no other, whatever the target's trajectory (`MEAS-A-089`, with a recording wrapper of each), so the same value enters every evaluation of a stencil and cancels in the difference — exactly as the up leg of event 2 does in the range — and `ν_real = ν_rigid = ν_a`.
- **`F_a(h) = F_pure(h) + F_extra/ρ³`**, a bound over the stencil for the whole modelled angle. `F_pure(h) = 2/[(ρ − h)³ cos³(φ_max + asin(h/ρ))]`, `φ_max` = 40°, is a **theorem** about the pure geometric direction: the third derivative of `atan2` along a unit displacement is `2 Im[(w₁/w₀)³]` with `|w₁| ≤ 1` and `|w₀| = ρ′ cos φ′`, where over the stencil `ρ′ ≥ ρ − h` and `|φ′| ≤ φ_max + asin(h/ρ)`; declination's coefficient is smaller. It is 2.5748 × 10⁻¹⁸ m⁻³ at `h` = 10 m and 2.5866 × 10⁻¹⁸ m⁻³ at 1000 m (`MEAS-P-27`).
  `F_extra` is **the supremum, over the class, of the difference between the third derivative of the full modelled angle and that of the pure direction of the same displaced target**: the light-time coupling re-solved for every displacement (`v/c` = 2.5 × 10⁻⁵ of the target; the acceleration included), the annual aberration `D(·; β_E)` with `β_E` = 1.0104 × 10⁻⁴ (the Earth's speed at the January perihelion), and — for azimuth and elevation — the diurnal aberration (`β_d` ≤ 1.55 × 10⁻⁶), the local frame and the refraction. It is computed by the **exact power series of the model itself** (no finite differences; the series is checked against 60-digit central third differences of an independent implementation, agreeing to 3 × 10⁻⁷), at the stencil's centre and its two ends, over 25 lines of sight, 342 displacement directions (a 10° grid of the half-sphere), both velocity directions and the aberration vector along each axis; times 1.05 for the grid (the coarser grid the reproducing check uses — 15°, six and nine lines of sight — gives 2.9 %, 1.6 % and 1.0 % less for the three, and a 20° grid up to 4.5 % less), rounded up:

  **`F_extra/ρ³` = 2.62 × 10⁻⁴ (Geometric), 1.43 × 10⁻³ (Astrometric, and of date — a rotation of the frame changes the class of lines of sight and not the function), 9.74 × 10⁻² (ApparentRefracted)**, that is 1.52 × 10⁻²², 8.28 × 10⁻²² and 5.64 × 10⁻²⁰ m⁻³ (`MEAS-P-28`): the refraction adds 2.1 % to `F_pure`. The scan's pure third derivatives reach 0.978 of `F_pure`, so the bound is nearly sharp.

| `h` | `F_a(h)` Geometric | `ε` | `F_a(h)` Astrometric, of date | `ε` | `F_a(h)` ApparentRefracted | `ε` | `ν_a/h` | agreement tolerance |
|---|---|---|---|---|---|---|---|---|
| 10 m | 2.57496 × 10⁻¹⁸ | 5.4220 × 10⁻¹⁶ | 2.57563 × 10⁻¹⁸ | 5.4221 × 10⁻¹⁶ | 2.63117 × 10⁻¹⁸ | 5.4314 × 10⁻¹⁶ | 4.9928 × 10⁻¹⁶ | 1.0007 × 10⁻¹⁵ |
| 30 m | 2.57519 × 10⁻¹⁸ | 5.5271 × 10⁻¹⁶ | 2.57587 × 10⁻¹⁸ | 5.5281 × 10⁻¹⁶ | 2.63141 × 10⁻¹⁸ | 5.6114 × 10⁻¹⁶ | 1.6643 × 10⁻¹⁶ | 3.3494 × 10⁻¹⁶ |
| 100 m | 2.57602 × 10⁻¹⁸ | 4.3433 × 10⁻¹⁵ | 2.57670 × 10⁻¹⁸ | 4.3444 × 10⁻¹⁵ | 2.63224 × 10⁻¹⁸ | 4.4370 × 10⁻¹⁵ | 4.9928 × 10⁻¹⁷ | 1.0194 × 10⁻¹⁶ |
| 300 m | 2.57839 × 10⁻¹⁸ | 3.8693 × 10⁻¹⁴ | 2.57907 × 10⁻¹⁸ | 3.8703 × 10⁻¹⁴ | 2.63461 × 10⁻¹⁸ | 3.9536 × 10⁻¹⁴ | 1.6643 × 10⁻¹⁷ | 3.5370 × 10⁻¹⁷ |
| 1000 m | 2.58671 × 10⁻¹⁸ | 4.3112 × 10⁻¹³ | 2.58739 × 10⁻¹⁸ | 4.3124 × 10⁻¹³ | 2.64292 × 10⁻¹⁸ | 4.4049 × 10⁻¹³ | 4.9928 × 10⁻¹⁸ | 1.2073 × 10⁻¹⁷ |
| **`B_pred`** (window) | | **4.3112 × 10⁻¹³** (4.311 × 10⁻¹⁴, 4.311 × 10⁻¹²) | | **4.3124 × 10⁻¹³** (4.312 × 10⁻¹⁴, 4.312 × 10⁻¹²) | | **4.4049 × 10⁻¹³** (4.405 × 10⁻¹⁴, 4.405 × 10⁻¹²) | | |

`F_a` in rad m⁻³, `ε` and the tolerance in rad m⁻¹ (the units of a partial). **(c)** takes `ν_a/h_v` = 4.9928 × 10⁻¹³, 4.9928 × 10⁻¹⁴, 4.9928 × 10⁻¹⁵, 4.9928 × 10⁻¹⁶ for `h_v` = 0.01, 0.1, 1, 10 m s⁻¹, and the analytic velocity entries exactly 0.

**The agreement of the two configurations**, written down and asserted (the chain term being zero, they must agree to arithmetic noise): for every output, component and size, `|f̂_real − f̂_rigid| ≤ 2 ν_a/h + 3 δs/(ρ(1 − κ) − h)²`, `κ` = 2.5 × 10⁻⁵ the target's `v/c` and `δs` = 1 × 10⁻⁶ m the asserted bound of the observers' difference. The first term is the two estimates' noise; the second is what a position difference `δs` of the observer does to a partial — the Hessian of an angle has the norm `1/(ρ′ cos φ′)² ≤ 1.7/ρ′²`, the factor 3 carrying as well the change of the partials' frame by the orientations' difference (below 1 × 10⁻¹³ rad) — 2.1 × 10⁻¹⁸ rad m⁻¹ at `h` = 10 m (the table's last column holds both terms).

**The gate must be able to fail, for angles** (`MEAS-A-099`, rule 5; written here, before the run). The rows below are assembled in the test from the term functions (the Jacobians of `MEAS-R-060`), a **reconstruction that must itself agree with the model's partials to 1 × 10⁻⁹ relative before any control is read**; in each configuration the correct row passes (b) and each wrong row fails it, the violation being `max |f̂ − a_wrong| / ε(h)` over outputs, components and sizes:
- without the annual aberration's Jacobian `D′` (Astrometric; Astrometric of date) — **10³**;
- without `dz_o/dz_v` (ApparentRefracted) — **10³**;
- without the emission-epoch shift `δt_e`, `K = I − v n̂ᵀ/(c + n̂·v)` replaced by `I` (all four gates), **in the across-the-line-of-sight geometry only**: along it `(I − n̂n̂ᵀ) v = 0` and the two rows are one row — **10³**;
- without the frame's rotation (the row of the ICRF for the of-date observation) — **10³**;
- without the diurnal aberration's Jacobian `A′(·; β_d)` (ApparentRefracted): its size is `β_d` = 1.55 × 10⁻⁶ of the row, 65 times less than the annual one's, and it is asked to fail by **10²** only.
A miss of any of these is stopped and reported, not tuned.

**What the run reports, whatever it finds**: per gate, configuration and velocity direction, `B/B_pred`, the worst `|f̂ − a|/ε` with its output, component and size, the worst `|f̂_v|/(ν_a/h_v)`, whether the analytic row lies inside the raw spread, the agreement of the configurations in units of the tolerance, the observers' difference, and every control's violation.

**Recorded after the angle gate's one run, 2026-10-06 — an amendment of (v), its first text kept above.** The gate ran once (`PROVENANCE.md` §39.13). Its criteria (a), (b), (c), the two configurations' agreement and four of the five controls held — the controls failing their wrong rows by 3.5 × 10⁴ to 4.2 × 10⁶ `ε`, against the 10³ asked. **The fifth, the row without the diurnal aberration's Jacobian, failed (b) by 0.473 `ε` (across) and 0.474 `ε` (along), not the 10² asked**, in both configurations. The cause was in the frozen text and not in the model: the rule selected a line of sight due north, the station moves east, and the first-order effect of `A′(·; β_d)` on an angular displacement is the scalar factor `1 − n̂·β_d`, which depends on the line-of-sight component of the station's velocity and is zero there; the second-order remainder (`β_d²/2` = 1.2 × 10⁻¹², observed 6.5 × 10⁻¹³ along and 3.4 × 10⁻¹¹ across, the latter the cross term with the emission-epoch shift) is 6 × 10⁻¹⁹ rad m⁻¹ against `ε` ≥ 5.4 × 10⁻¹⁶. The control was sized generically and not evaluated at the geometry it ran at: it had no power there, which is evidence about the control and not about the model. **By the manager's ruling (`plan/subplan_L6/L6-4.md`, `7baf3ad`) the run stands as run and the geometry is not re-selected; this control is withdrawn from the asserted list of `MEAS-A-098` — its measured violation is still computed and logged beside the others, not asserted — and its claim (that G1 catches a missing or mis-wired `A′(·; β_d)` in the azimuth/elevation row) is not dropped: it moves to `MEAS-A-098b`, (vi) below.** The gate's source was edited after its run for that one assertion only (`Control::asserted`); the log kept is the run of the source before that edit.

**(vi) The diurnal-aberration check, `MEAS-A-098b` — registered 2026-10-06, after the angle gate's one run, and committed before this check's code exists, so before it has run.** *The ruling:* the claim of the control that missed moves to a new check at a closed-form geometry with the station moving along the line of sight, sized by the procedure of (v) **evaluated at that geometry** — not the numbers of (v) carried over — with each control's power at that geometry computed and written down before the check runs; the lesson binds every check from here on.

**The geometry** (the closed-form station of `MEAS-A-087`, here along the line of sight; no real chain, so no second configuration): the identity Earth orientation (GCRS axes = ITRS axes: up = x, east = y, north = z), a site on the equator at longitude 0, the observer at (6 378 137, 0, 0) m with the inertial velocity of the equatorial surface, 465.1 m s⁻¹ (`MEAS-P-9`'s `v`), **along the line of sight — toward the target (`n̂·β_d = +β_d`) in one run and away from it (`−β_d`) in the other**; a static target (no light-time coupling to confuse the check) at azimuth 90° (east) and elevation 30°, slant range ρ = 1.2 × 10⁶ m; the atmosphere of the first normal point. Because `β_d ∥ n̂`, `A(n̂; ±β_d) = n̂`: the apparent direction is the geometric one, and **the model's row, the row without `A′` and the row with `A′(·; −β_d)` share one base direction and differ in the Jacobian alone** — which is what the check isolates. The central differences, the criteria and the sizes are those of (v) (five position sizes, four velocity sizes, the wrapped difference for the azimuth, the drift family varied time-fixed at the nominal emission epoch).

**The sizing — the procedure of (v), evaluated at this geometry:**
- `ν = 3 [ulp(output) + ulp(operand)/ρ]` with **this geometry's** output and operand: the azimuth is 1.5708 rad (ulp 2⁻⁵² = 2.2204 × 10⁻¹⁶, not (v)'s ulp of 2π), the target's largest position component 6.978 × 10⁶ m (ulp 2⁻³⁰ m, as in (v)): **`ν` = 2.9944 × 10⁻¹⁵ rad** (`MEAS-P-30`, `-26`; (v)'s was 4.9928 × 10⁻¹⁵). The chain's floor is zero (no chain; the observer is evaluated at the observation epoch).
- `F_a(h) = F_pure(h; φ) + F_extra/ρ³` with `F_pure(h; φ) = 2/[(ρ − h)³ cos³(φ + asin(h/ρ))]` at **this line of sight's own elevation, φ = 30°** (3.0792/ρ³ at the centre, attained exactly by the azimuth along the north axis, 1.7820 × 10⁻¹⁸ m⁻³) and `F_extra` from the exact series of the model at this line of sight (10° grid of displacement directions, the stencil's centre and its two ends, both signs of the observer's motion; times 1.05, rounded up): **`F_extra/ρ³` = 2.19 × 10⁻²** — all of it the elevation's, the refraction's third derivative (the azimuth's own is 9 × 10⁻⁶). The scan's pure third derivatives reach 1.000000 of `F_pure` (the north axis at the centre).

| `h` | 10 m | 30 m | 100 m | 300 m | 1000 m |
|---|---|---|---|---|---|
| `F_a(h)`, rad m⁻³ | 1.79469 × 10⁻¹⁸ | 1.79483 × 10⁻¹⁸ | 1.79532 × 10⁻¹⁸ | 1.79673 × 10⁻¹⁸ | 1.80166 × 10⁻¹⁸ |
| `ε(h)`, rad m⁻¹ | 3.2936 × 10⁻¹⁶ | 3.6904 × 10⁻¹⁶ | 3.0221 × 10⁻¹⁵ | 2.6961 × 10⁻¹⁴ | 3.0028 × 10⁻¹³ |

**`B_pred` = 3.0028 × 10⁻¹³** (window 3.003 × 10⁻¹⁴ … 3.003 × 10⁻¹²); (c) takes `ν/h_v` = 2.9944 × 10⁻¹³, 2.9944 × 10⁻¹⁴, 2.9944 × 10⁻¹⁵, 2.9944 × 10⁻¹⁶ for `h_v` = 0.01, 0.1, 1, 10 m s⁻¹.

**The controls' power at this geometry, written down before the run.** Two wrong rows, assembled in the test from the term functions after the row so assembled has been shown equal to the model's partials to 1 × 10⁻⁹ relative: **control 1, without `A′`** (the identity in its place), and **control 2, with `A′` of the wrong sign** (`A′(·; −β_d)`, the sign error). The Jacobian's first-order effect on a displacement tangent to the sphere is the scalar `bm1/(1 + n̂·β_d)` ≈ `1 − n̂·β_d`; here `n̂·β_d = ±β_d`, so a wrong row differs from the right one by `β_d` of it (control 1) or `2 β_d` of it (control 2). The largest entry of the model's row is the azimuth's along the north axis, 9.6225 × 10⁻⁷ rad m⁻¹, and the elevation's along the vertical, 7.2098 × 10⁻⁷ rad m⁻¹: **control 1 differs from the model's row by at most 1.4928 × 10⁻¹² rad m⁻¹ (`MEAS-P-31`), 4 533 `ε` at `h` = 10 m** (`MEAS-P-32`), and 4 045, 494, 55 and 5 `ε` at the other sizes; **control 2 by 2.9857 × 10⁻¹² rad m⁻¹, 9 065 `ε` at 10 m** (8 090, 988, 111 and 10 at the other sizes) — the same for both signs of the observer's motion. These are the first derivatives of the exact series of the model, an implementation independent of the C++ rows (`tools/measmod_fd_sizing.py --scan --check` reproduces them), and the test asserts the C++ rows' own differences equal them to 1 % before any finite difference is read. The measured violation, `max |f̂ − a_wrong|/ε(h)`, differs from the row difference's by at most the estimate's own deviation from the right row, 1 `ε`.

**The criteria, frozen:** for the model's row, in both signs of the observer's motion, **(a)** `B_pred/10 ≤ B ≤ 10 B_pred`, **(b)** `|f̂ − a| ≤ ε(h)` for each output, component and size, **(c)** the velocity columns exactly 0 and each velocity estimate within `ν/h_v`; and **control 1 and control 2 each fail (b) by at least 10³ `ε`** (predicted 4.5 × 10³ and 9.1 × 10³). A miss of any of these is stopped and reported, not tuned. Run once, alone, with `--list-tests` read first and the log kept.

**What it can show and what it cannot.** It shows that `A′(·; β_d)` is in the azimuth/elevation row, with the right sign and in the right place, at a geometry where its first-order effect is `β_d` of the row. It does not show the term's wiring at other geometries (its formula is one function everywhere, `MEAS-A-080`), and it does not restore any power to `MEAS-A-098`'s own geometry, which stays without it, as recorded.

**Run of the diurnal-aberration check, 2026-10-06 (03:02:02Z UTC, `11a58a7`, `odl_measmod_tests "[diurnalcheck]"` alone after `--list-tests`; log `g1_diurnal_check_run.log`, sha256 `7d5e34aaa482fcca9fce23acd88a57f1d68813174abc2a6c2d23b95d0f2c1176`) — held.** `MEAS-A-098b`: 345 assertions, none failing, in both signs of the observer's motion. (a) `B/B_pred` = 0.989 (toward) and 0.9889 (away); (b) the worst `|f̂ − a|/ε` = 0.9914 in both, at `h` = 300 m, the azimuth along the north axis — truncation-limited, the bound again not slack (the 0.989 I predicted was the figure at 1 000 m, the one size I evaluated: `ε`'s margin is smaller at 300 m); (c) every velocity estimate exactly 0. **Control 1, without `A′`, failed (b) by 4 532.7 `ε` (toward) and 4 532.5 `ε` (away)** — predicted 4 533, written before the run; **control 2, `A′` of the wrong sign, by 9 065.3 and 9 065.1 `ε`** — predicted 9 065; each against the 10³ asked. `A′(·; β_d)` is in the azimuth/elevation row, with the right sign, as the claim that moved here required; `MEAS-A-098`'s own geometry stays without power for the term, as recorded in (v). `PROVENANCE.md` §39.13 holds the account.

**Second run of the range gate, 2026-10-06 (after Amendment A1, in both configurations) — held.** `MEAS-A-042` … `-046`: 4 361 assertions, none failing. (a) held in 16 of 16 (`B/B_pred` 0.56 – 1.09); (b) in 16 of 16, the worst `|f̂ − a|/ε` 0.966 – 0.995 on the real chain and 0.983 – 0.998 over the exact rigid rotation — the LEO-like truncation reaches 0.9977 of its bound, so the bound is not slack; (c) in 16 of 16. Every wrong row failed (b) in both configurations: the least violation was 193 `ε`; the rows wrong by `v/c` or more failed by at least 2 688 `ε`; in the rigid configuration the along-the-line-of-sight rows by at least 3 410 `ε`; the weakest real-chain row, the LAGEOS-like event-1 geometric row (509.8 `ε`), differs from the model's by the atmosphere's gradient, 3.5 × 10⁻⁷, and is asked only to fail. The double count of 1.9 × 10⁻⁵ failed (b) by 3.4 × 10⁴ `ε` on the real chain; the chain's floor peaked at 1.017 `u R⊥`. `PROVENANCE.md` §39.11 holds the figures and how the run was executed.

### 6.3 Accuracy statements the model makes

- The range model's value is exact to its terms: the fixed-point light time (the iteration stops when a leg's duration changes by at most 4 ulp — 1.4 × 10⁻¹⁷ s, 4.2 × 10⁻⁹ m, for a LAGEOS-like leg — which is the **stopping step and not the error**: the contraction factor 2.5 × 10⁻⁵ makes the returned iterate's error 10⁻⁴ of it), the zenith delay to the source's 1 mm, the mapping function to its stated 1 mm, 4 mm and 16 mm rms at 15°, 10° and 6° [`MPPL02`; `TN36-9`], the Shapiro term to double precision. Beside these, the **omitted** terms of §6.4 set the residual a real pass carries.
- The range model's **output noise floor** is the Earth-orientation chain's (Amendment A1, §6.2): the real chain's station position carries one ulp of ERFA's unreduced Earth-rotation angle, **0.16 µm** (1.586 × 10⁻⁷ m at Yarragadee, `MEAS-P-22`), bounded by three ulp, 0.48 µm (`MEAS-A-046`); it enters a range as its projection on the line of sight, at most 0.48 µm. It is a property of the angle's double-precision argument, not of the model's equations, and no omitted term of §6.4 is that small.
- An angle's value is exact to its terms: the astrometric reduction to the neglected cross term (0.03 mas), the refraction to `ERFA`'s "worst 62 mas, RMS 8 mas".
- The model states no accuracy for a **residual** against real data: that is `G5`'s envelope (§8, `MEAS-A-100`/`-101`).

### 6.4 What is omitted, and how large (ruling R2) — in this specification and in every modelled range's `Applied` record (`MEAS-R-038`)

Each magnitude names whether it was **read** from a fetched text, **computed** from stated constants, or is **quoted from memory** (the last are labelled and serve only to size an omission, never to model one).

| term | magnitude | basis | where it would be modelled |
|---|---|---|---|
| **solid Earth tide**, radial | up to **0.32 m** at the mean distances (+0.32 m, −0.16 m); ≈ 0.38 m at lunar perigee and perihelion | **computed**: `h₂ (k_Moon + k_Sun)` = 0.6078 × 0.5230 m (`MEAS-P-20`); `k_j = (GM_j/GM⊕)(a_E/R_j)³ a_E` = 0.3584 m (Moon, `MEAS-P-18`) and 0.1646 m (Sun, `MEAS-P-19`) with `h₂` **read** from `TN36-7` §7.1.1 (nominal `h(0)` = 0.6078, `h(2)` = −0.0006), `a_E` = 6 378 136.6 m [`TN36-1`], the Moon's mean distance 384 400 km and the lunar-perigee and perihelion extremes **quoted from memory** | the named follow-on, with `TN36-7`'s `DEHANTTIDEINEL.F` prolog's four printed cases |
| solid Earth tide, horizontal | up to **0.066 m** at the mean distances (0.079 m at the extremes) | **computed**: `1.5 l₂ (k_Moon + k_Sun)` at ψ = 45° (`MEAS-P-21`), `l₂` = 0.0847 **read** from `TN36-7` | the same |
| permanent tide in the conventional tide-free system | "about −12 cm at the poles and about +6 cm at the equator" (radial) | **read**: `TN36-7` §7.1.1.2 — this is inside the solid-tide term above, listed because ITRF2020's coordinates are "conventional tide free" | not modelled |
| **pole tide** | radial **25 mm**, horizontal **7 mm** (maxima) | **read**: `TN36-7` §7.1.4 — "the maximum radial displacement is approximately 25 mm, and the maximum horizontal displacement is about 7 mm" | the named follow-on |
| **ocean tide loading** | "can reach 100 mm" in the worst coastal case [`TN36-7` §7.1.2, **read**]; typically centimetres at a coastal station. **No coefficient is obtainable under this tree's rules** (the Onsala service is a request form that answers by e-mail) | **read** (the bound); the value at the chosen station is bounded where `MEAS-A-100` is written, with its source | not modelled; not sourceable |
| atmospheric loading | not modelled by the ILRS either: "the non-tidal atmospheric loading effects on station positions were not modeled" | **read**: the ILRS contribution to ITRF2020 | — |
| tropospheric horizontal gradients | up to **5 cm** of delay at low elevation | **read**: `TN36-9` §9.1.3 | not modelled; the caller's elevation cut-off |
| mapping-function error | rms ≈ 1 mm / 4 mm / 16 mm at 15° / 10° / 6° | **read**: `MPPL02`, `TN36-9` | in the model; stated |
| ITRS **realisation** differences (IGS05 … IGb20, SLR20) | millimetres to centimetres | **quoted from memory**, not quantified here; the SP3 frame code is mapped to one ITRS (`MEAS-R-041`) | not modelled |
| the Sun's term in the aberration; the cross term `β_E β_d` | ≤ 0.4 µas; ≈ 0.03 mas | **read**: `eraAb`'s own note; **computed**: 1.5 × 10⁻¹⁰ rad (§4.7) | not modelled |
| gravitational **light deflection** by the Earth (angles) | up to **0.57 mas** for a ray that grazes the Earth (`4GM/(c² R)`, `MEAS-P-24`), less at the ranges of the campaigns | **computed**: `4 GM/(c² R)` with `GM` = 3.986 004 415 × 10¹⁴ m³ s⁻², `R` = 6 378 137 m | not modelled |
| the Sun's gravitational delay of a near-Earth range | not modelled: "for near-Earth satellites … the only body to be considered is the Earth" | **read**: `TN36-11` §11.2 | — |
| **Data Handling File** biases and the **centre-of-mass table's** station dependence | a station's range bias is typically millimetres to centimetres, and the CoM of a geodetic sphere varies with station, epoch and wavelength (the ILRS page: values differ from earlier modelling by ≈ 4.5 mm for LAGEOS) | `ILRSDHF`, `ILRSCOM` (not read for values here); the DHF is read at `MEAS-A-100` to choose a clean pass | deferred (R3) |
| EOP error | UT1: 0.465 mm per µs (`MEAS-P-5`); polar motion: ≈ 3 mm per mas at the surface | **computed**: `SPEC-template.md` §7's table | in `eop` |

| id | quantity | budget | because |
|---|---|---|---|
| `MEAS-P-18` | the Moon's degree-2 tide scale `k_Moon` at the mean distance | 0.012 300 0 × 4.568 × 10⁻⁶ × 6 378 136.6 m = **0.3584 m** | `(GM_M/GM⊕)(a_E/R)³ a_E`, 1/81.300 56 = 0.012 300 0, (6 378 136.6/384 400 000)³ = 4.568 × 10⁻⁶ |
| `MEAS-P-19` | the Sun's `k_Sun` at 1 AU | 332 946.05 × 7.750 × 10⁻¹⁴ × 6 378 136.6 m = **0.1646 m** | (6 378 136.6/1.495 978 707 × 10¹¹)³ = 7.750 × 10⁻¹⁴ |
| `MEAS-P-20` | the maximum radial solid tide at the mean distances | 0.6078 × 0.5230 m = **0.3179 m** | `h₂ (k_Moon + k_Sun)`, 0.3584 + 0.1646 = 0.5230 m |
| `MEAS-P-21` | the maximum horizontal solid tide at the mean distances | 1.5 × 0.0847 × 0.5230 m = **0.0664 m** | `1.5 l₂ k`, the maximum of `3 l₂ k sin ψ cos ψ` at ψ = 45° |
| `MEAS-P-24` | the light deflection of a ray that grazes the Earth, in radians | 4 × 3.986004415 × 10¹⁴ × 1.112650 × 10⁻¹⁷ × 1.567855 × 10⁻⁷ = **2.78 × 10⁻⁹** | `4GM/(c² R)` with 1/c² = 1.112650 × 10⁻¹⁷ s² m⁻² and 1/R = 1.567855 × 10⁻⁷ m⁻¹ (R = 6 378 137 m): 2.78 × 10⁻⁹ rad is 0.57 mas |
| `MEAS-P-25` | three ulp of an output angle up to 2π, the angle gate's `ν_a` (first term) | 3 × 8.8818 × 10⁻¹⁶ = **2.6645 × 10⁻¹⁵** | 2⁻⁵⁰ rad, the ulp of a double in [4, 8) |
| `MEAS-P-26` | three ulp of the target's position operand seen as an angle at ρ = 1.2 × 10⁶ m, `ν_a` (second term) | 3 × 9.3132 × 10⁻¹⁰ m × 8.3333 × 10⁻⁷ m⁻¹ = **2.3283 × 10⁻¹⁵** | 2⁻³⁰ m, the ulp in [4.19, 8.39] × 10⁶ m; the two terms make `ν_a` = 4.9928 × 10⁻¹⁵ rad |
| `MEAS-P-27` | the angle gate's truncation at `h` = 1000 m, the pure geometry | 1000 m × 1000 m × 2.5866 × 10⁻¹⁸ m⁻³ × 0.166 666 7 = **4.3110 × 10⁻¹³ m⁻¹** | `h² F_pure(1000)/6`, §6.2 (v) |
| `MEAS-P-28` | the refraction's addition to `F_a` | 9.74 × 10⁻² × 5.7870 × 10⁻¹⁹ m⁻³ = **5.6365 × 10⁻²⁰ m⁻³** | `F_extra/ρ³` for `ApparentRefracted`, 1/ρ³ = 5.7870 × 10⁻¹⁹ m⁻³ at ρ = 1.2 × 10⁶ m |
| `MEAS-P-29` | the noise term of the agreement tolerance at `h` = 10 m | 2 × 4.9928 × 10⁻¹⁵ × 0.1 m⁻¹ = **9.9856 × 10⁻¹⁶ m⁻¹** | `2 ν_a/h`, the two estimates' noise |
| `MEAS-P-30` | three ulp of the azimuth at the diurnal check's geometry, 1.5708 rad (`ν`'s first term there) | 3 × 2.2204 × 10⁻¹⁶ = **6.6612 × 10⁻¹⁶** | 2⁻⁵², the ulp of a double in [1, 2); with `MEAS-P-26` the two terms make `ν` = 2.9944 × 10⁻¹⁵ rad |
| `MEAS-P-31` | what dropping `A′(·; β_d)` does to the azimuth's largest partial at the diurnal check's geometry | 1.5514 × 10⁻⁶ × 9.6225 × 10⁻⁷ m⁻¹ = **1.4928 × 10⁻¹² m⁻¹** | `β_d` × the partial (`n̂·β_d = β_d`: first order), 1.5514 × 10⁻⁶ = 465.1 m s⁻¹ / `c` |
| `MEAS-P-32` | that change in units of the smallest `ε` of the check, at `h` = 10 m | 1.4928 × 10⁻¹² m⁻¹ × 3.0362 × 10¹⁵ m = **4.532 × 10³** | `MEAS-P-31` / `ε(10 m)` = 1 / 3.2936 × 10⁻¹⁶ m⁻¹: the control's predicted power |

---

## 7. Failure behaviour

| id | fires when | carries | never instead |
|---|---|---|---|
| `MEAS-F-001` | the SOD (pad, system, occupancy) is not in the registry — **including a SOD that is listed but has no coordinates** (60 in the pinned release: absent from SLRF2020, or on a marker with no solution; the diagnostic says which) | the three numbers, the SLRF2020 release and its hash, the number of pads and SODs held | the nearest station, a default, or the pad alone |
| `MEAS-F-002` | the epoch lies outside every eccentricity span of the SOD (or the SOD has none) | the SOD, the epoch, the spans the file holds | the nearest span, or a zero eccentricity |
| `MEAS-F-003` | the epoch is before the first solution's data start, after the last's end, or in a gap between spans | the marker, the epoch, the spans | extrapolating the linear position |
| `MEAS-F-004` | the site has a post-seismic event at or before the epoch, and `accept_linear_position_after_post_seismic_event` is not set | the site, the event epoch(s), the PSD file's hash | the linear position, silently |
| `MEAS-F-005` | an optical station number is not in the caller's registry | the number, how many sites it holds | any bundled list |
| `MEAS-F-006` | a citation (an optical site's, or a centre-of-mass value's) is blank or whitespace | which value | a placeholder citation |
| `MEAS-F-007` | the pass's `H3` ILRS satellite identifier is not the one the centre-of-mass declaration was made for | both identifiers | applying one satellite's centre of mass to another's ranges |
| `MEAS-F-008` | a CRD range type is not two-way, or an epoch event is not 1 or 2 | the field and its value | modelling it as two-way/bounce |
| `MEAS-F-009` | a CRD session says tropospheric refraction or centre of mass is already applied, or the station system delay is not | which flag | applying a correction twice, or skipping one |
| `MEAS-F-010` | the elevation is below 3° or the target below the horizon (range); the vacuum zenith distance exceeds 75° (azimuth/elevation) | the value and the limit with its source | extrapolating the mapping function or the refraction |
| `MEAS-F-011` | a light-time leg has not reached its fixed point in 20 passes | the leg, the passes, the last change in seconds | the last iterate |
| `MEAS-F-012` | a pass has no record `20`, or a configuration id has no `C0` | the pass, the id | a standard atmosphere, or 532 nm |
| `MEAS-F-013` | the target is not a sphere with a cited centre-of-mass correction | that an array offset is not implemented until a campaign needs it | a zero correction |
| `MEAS-F-014` | an SP3 header's time system is GLO/GAL/BDT/QZS; its coordinate-system code is not in the table; or the reference point is not `CentreOfMass` | the value and the accepted set | treating it as GPS time, ITRS, or the centre of mass |
| `MEAS-F-015` | an angle reduction does not fit the observation's coordinate kind (`Astrometric`/`Geometric` for azimuth/elevation, `ApparentRefracted` for right ascension/declination) | the reduction and the kind | reducing the wrong quantity |
| `MEAS-F-016` | an IOD epoch code is 1, 2, 3, 4 or 6, or is 0 with no declared mean/true equinox | the code and the policy | reading it as J2000 |
| `MEAS-F-017` | a centre-of-mass correction is negative or not finite | the value and the sign convention (§3.3) | the absolute value, or zero |
| `MEAS-F-018` | an observed time of flight is not finite and positive, or an observed angle or position is not finite, **or an SP3 position record is the format's marker for an absent position** (`SP3D` sets bad or absent positions to 0.000000: all three components exactly 0 — *added 2026-10-06 when the position builder was written*) | the value, and for the marker the satellite and the epoch | a residual of NaN, or a satellite at the Earth's centre |
| `MEAS-F-019` | a pass's epochs are not non-decreasing once the rollover rule is applied, or span more than a day | the two records | a pass that runs backwards |
| `MEAS-F-020` | a SLRF2020, eccentricity or PSD document is malformed or inconsistent (a position with no velocity; a solution for a marker SITE/ID does not list, or an estimate with no epoch row; an eccentricity SOD absent from its own SITE/ID; a SOD whose pad, point or DOMES number differs between the two files, or between two rows of one SITE/ID) | the line number and the text | skipping the row |
| `MEAS-F-021` | a CRD seconds of day is negative or ≥ 86 401, or ≥ 86 400 on a day with no leap second | the value, the day | wrapping it silently |
| `MEAS-F-022` | an atmosphere is non-physical (pressure or wavelength ≤ 0, temperature outside −100…60 °C, humidity outside 0–1) | the value | clamping |
| `MEAS-F-023` | a CRD pass has no normal point | the pass | an empty result that looks like success |
| `MEAS-F-024` | an optical site's latitude is outside [−π/2, π/2], its longitude outside [−π, 2π] or its height outside [−1 000, 10 000] m | the value and its limits | a station at a unit-slipped position |
| `MEAS-F-025` | an optical station number is added again with different coordinates | the number and the coordinates already held with their citation | silently replacing a station's position |
| `MEAS-F-026` | an angle model's direction is within 1 × 10⁻⁹ rad of the pole of its coordinate system: the celestial pole of the observation's frame, or the zenith (*added 2026-10-06, `MEAS-R-057`*) | the angle from the pole and the limit | an azimuth of 0 and a partial of infinity or NaN |
| (inherited) `R-ERR-1`/`-2`/`-3` | `SPEC-template.md` §5's standing rules | a dependency's refusal (`FRAME-F-…`, `EOP-F-…`, `EPH-F-…`, the trajectory's) is returned **with its own id** and the context in the message; no persistent error state; the one named override is `MEAS-F-004`'s | |

---

## 8. Acceptance tests

Every id below is the literal `TEST_CASE` name in `modules/measmod/tests/` (or, for `MEAS-A-102`, in `tests/l6_exit_gate.cpp`), one row per test. Columns are as `SPEC-template.md` §8. The **source** column names the rank of the evidence (published number, closed form, public measurement, self-consistency).
The reference generators it names (`tools/measmod_reference.py`) are created by the step that adds the test using them, each in the form of `tools/legendre_reference.py`: the **definition** evaluated independently (stdlib `decimal`, 50 digits), every value computed at two precisions that must agree, and a `--check` that the committed header equals the regenerated one.
**This section contains more than self-consistency**: every gate below has an anchor that is not this module (§8.0).

### 8.0 The gates, and the absence record

The manager's ruling R1 restated the plan's gate (`plan/subplan_L6/L6-4.md`) as what the sources support:

| gate | what | rows |
|---|---|---|
| **G1** | every observation type's partials against finite differences, in `STM-R-004`/`-005`'s form, sizing frozen in §6.2 before any run — **amended once, after the range gate's first run, by Amendment A1** —, the light time iterated to its fixed point | `MEAS-A-042`, `-043`, `-044`, `-045`, `-046`, `-062`, `-096`, `-097`, `-098`, `-099` |
| **G2** | the three published IERS cases: FCULa and FCULb exact, the Mendes–Pavlis zenith delay to 1 mm (the source's stated accuracy), the 3.8 µm by which the printed equations miss the printed number **reported beside it and not pursued** | `MEAS-A-020`, `-021`, `-023` |
| **G3** | a closed-form two-way light time, target **and station** in uniform motion, for both epoch events, to 1 × 10⁻⁸ m | `MEAS-A-030`, `-031` (and `-032` with the Earth-fixed station) |
| **G4** | the Shapiro term against an independent evaluation, **labelled not published** | `MEAS-A-025`, `-026` |
| **G5** | one real LAGEOS pass against the ILRS orbit product of its week; the envelope — every omitted term sized from a source or first principles — **committed before the comparison**, compared **once**, **no bias removed**; inside passes, outside is a finding and the step does not close on it | `MEAS-A-100`, `-101` |

**The absence record (plan §4 rule 4, ruling R1): no published end-to-end SLR range case exists in anything retrievable without an account.** Searched, with the result (the round-8 report, D10, and `PROVENANCE.md` §39):

1. the three IERS routine prologs — **term-level cases only** (printed to 19 digits, all at McDonald Observatory), no geometry, light time, Shapiro or centre of mass;
2. `TN36-11` — the formula (11.17), **no number**;
3. `CRD2`'s sample files (§6): real MLRS-7080 LAGEOS-2 records of 2006-11-13, but with values that cannot be physical (a one-way centre-of-mass correction of 1601 m in record 12) and no orbit;
4. the `CPF2` document; four of the eight web searches (benchmark, worked example, observation-model test case, open-source residual tools) — nothing with inputs **and** a printed modelled range;
5. the ILRS Analysis Standing Committee benchmark: the 2003 call for participation points to a software-benchmarking pilot-project page — five tries, "Empty reply from server"; the Internet Archive (one snapshot tried) 404; the current "Software Benchmarking and Orbits" page (last modified 2016-06-03) lists four presentations and **no data set and no expected output**; the ASC says its centres "completed successfully a benchmark process" — nothing published;
6. textbook worked examples (Montenbruck–Gill, Vallado, Tapley–Schutz–Born) — **not obtained**: no copy on this machine, none retrievable without purchase; their numbers are not cited from memory.

Other absences this specification works around, each with its search in the report's D10: a single FCULb source in `TN36` (it refers to the 2002 paper, which is pinned); relative humidity to water-vapour pressure in `TN36-9` and `MP04` (found in `MM73`); an authoritative public IOD station list (none; sites come from the caller); ocean-loading coefficients without a request form (none); the cause of the 3.8 µm gap in the published zenith-delay case (not looked for: it would mean reading the routine).

### 8.1 Registry

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-001` | `SlrRegistry::build` on hand-built SINEX fixtures whose rows are **quoted as facts** from the pinned files (Yarragadee 7090 with its 18 eccentricity rows; marker `7110 A` with its four solutions; the PSD events of 7110) reproduces pads, SODs, markers, spans, eccentricities; records both headers' own version strings and the supplied hashes | the fixtures' values | `SLRF20`, `ILRSECC` (a few rows quoted as facts, ruling R3; no file is vendored) | exact | R-001 |
| `MEAS-A-002` | the marker position `x_ref + v (t − t_ref)` with the year of 365.25 d: at `t_ref` exact; at `t_ref + 10 × 365.25 d` equal to `x_ref + 10 v`; at `t_ref + 1 s` equal to `x_ref + v/31 557 600`. The SINEX epoch format (§3.6): `49` is 2049, `50` is 1950, an end `00:000:00000` is open, `26:036:43200` is 2026-02-05 12:00:00 UTC (the pinned file's own creation stamp, whose date is its release date). The solution is the marker's `SOLN` whose span contains `t`; **an epoch 1 s inside a span is accepted, 1 s outside refused**; the end `…:86399` extends to the next midnight (no one-second gap between contiguous spans, tested at `98:233:86399.5` of the Yarragadee rows' spans); **a span starting before 1972 (the pinned file's `71:001:00000`) is unbounded below and one ending before 1972 never matches** | closed forms; the stamp | the formula and §3.6; the file's own stamp | 1 × 10⁻⁹ m (a product of m yr⁻¹ and seconds); epochs exact | R-002, F-003 |
| `MEAS-A-003` | the system reference point `marker + E·(U,N,E)`: at four closed-form sites — geodetic (φ, λ) = (0°, 0°), (0°, 90°), (90°, 0°) and (45°, 30°) with h = 100 m built in the test from the ellipsoid's own formulas — a unit up, north and east offset moves the point by exactly the local up, north and east of the **geodetic** normal. **With the geocentric latitude instead, the up offset points 0.1924° (3.36 mrad) away — 10.7 mm of Yarragadee's 3.185 m, 12.6 mm of a (3.185, 2) m up-and-north offset — shown failing** (rule 5) | exact trigonometry | closed form | 1 × 10⁻¹² m per metre of eccentricity | R-003 |
| `MEAS-A-004` | **on the pinned release** (`ilrs-slrf2020-20260205`, `ilrs-slrecc-une-20260527`, through the manifest): **186 pads**, 483 SODs listed and **482 placed**, **60 unplaced**; the eccentricity file knows **235 pads and 542 SODs**; each unplaced SOD refuses `MEAS-F-001` saying it has no coordinates — `73071701` (marker A) and `73071703` (marker C) absent from SLRF2020, `73071702` (marker B, listed with **no solution**), and a SOD on one of the 49 pads SLRF2020 does not list; **184 + 2 = 186**: removing pads 7329 (Xian) and 7317 (Ishioka) leaves exactly 184, the file's `FILE/COMMENT` history adding both in 2025 (rule 3: the header's "184 unique sites" is reconciled, not assumed) | the counts | the pinned files, counted independently by `tools/measmod_registry_facts.py` (stdlib only) | exact | R-004, R-006, F-001 |
| `MEAS-A-005` | refusals on the **real** pinned data, each fired **and shown not to fire on the adjacent input**: pad `9999` and SOD `99999999` refuse `F-001` (naming the number, the release, the hash, the counts held); SOD `70900513` at 1992-07-20 23:59:59 UTC (one second before its first eccentricity span, `92:203:00000`) refuses `F-002`, at `92:203:00000` it is accepted; SOD `71100411` (marker `7110 A`) at 1999-10-16 20:00:00 UTC, in the 8-minute gap between the marker's solutions 1 and 2, refuses `F-003`, and at 15:39:36 (the end of solution 1, inclusive) it is accepted; SOD `73588901` (marker `7358 A`, whose last solution ended 2019-12-14) at 2026-01-01 refuses `F-003`. **A registry altered to return the nearest or a default station is made to fail the same assertions** (rule 5): the identical check runs on a deliberately wrong lookup and must *fail* for it | the diagnostics | the pinned data's own spans | exact | F-001, F-002, F-003 |
| `MEAS-A-006` | the post-seismic refusal on the real events: SOD `71100412` (marker `7110 A`; event `10:094:81643`, 2010-04-04 22:40:43 UTC) is accepted at 22:40:42 and **refuses `F-004` at 22:40:43 and after**; with `accept_linear_position_after_post_seismic_event` set it is returned and `post_seismic_override_used` is true; **the same instants at Yarragadee (no event) are accepted and the flag is false**; the override is per call (a second call without it refuses again). At 2026-01-01 00:00:00 the four PSD sites whose solutions are still open — 7110 (`71100412`), 7237 (`72371901`), 7403 (`74031306`), 7838 (`78383603`) — each refuse `F-004`; the four whose solutions have ended (7308, 7328, 7358, 7405) refuse earlier, in the stated order of checks (identity, eccentricity, solution, post-seismic); **the first normal point of the real file's MONL (7110) session refuses `F-004`** and is accepted with the override | the diagnostics and flags | `PSD20`'s event list (the event epochs are read from the file; the eight sites 7110, 7237, 7308, 7328, 7358, 7403, 7405, 7838) | exact | R-005, F-004 |
| `MEAS-A-007` | optical sites: `add` with a citation, `site` returns it; a citation of `""`, spaces, a tab or a newline refuses `F-006` (and nothing is added); an unknown number refuses `F-005`, naming the number and how many sites are held; a latitude of 45 (degrees given as radians), a longitude of 400 and a height of 30 000 each refuse `F-024` and add nothing; the same number added again with different coordinates refuses `F-025`, with identical ones is accepted; **no bundled list exists** (the registry is empty on construction) | the diagnostics | `macromodel::cited`'s rule; the round-8 report's D2 | exact | R-007, F-005, F-006, F-024, F-025 |
| `MEAS-A-008` | `PadId`, `SodKey` and `OpticalStationNumber` are **three distinct types**: none converts to or is constructible from another, and `SlrRegistry::site` does not accept a `PadId` or an `OpticalStationNumber` (checked with `requires`-expressions at compile time) | the properties | plan §5 constraint 10 | compile time | R-004 |
| `MEAS-A-009` | build refusals `MEAS-F-020`, each alone and each naming the line: a position without its velocity; a solution for a marker `SITE/ID` does not list; an estimate with no epoch row; an eccentricity SOD absent from its own `SITE/ID`; a SOD whose DOMES number differs between the two files; a SOD listed twice in one `SITE/ID` with different content. **The real files build** (so the rules refuse neither marker `7307 B`, listed with no solution, nor the identical repeat of SOD `71100301` in the eccentricity file's `SITE/ID`, nor SOD `79024201`'s spans from 1971) | the diagnostics; the real build succeeds | the pinned files | exact | R-001, F-020 |
| `MEAS-A-010` | **real data**: Yarragadee `70900513` at the first normal point of the real file (2026-01-01 02:07:56.8005871 UTC): the marker is `x_ref + v · (t − t_ref)` from the six printed estimates, the eccentricity is `(3.1827, −0.0064, 0.0194)` m, the system reference point is 3.18275 m from the marker, its geodetic coordinates are those of an independent closed-form (Bowring) evaluation, `post_seismic_override_used` is false | the independent values from `tools/measmod_registry_facts.py` | the pinned files | 1 × 10⁻⁶ m (the file's own digits) | R-002, R-003 |

### 8.2 Time

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-012` | the target identity: a real LAGEOS-1 pass (`H3` identifier 7603901) with a centre-of-mass declaration for LAGEOS-1 builds; **the same pass with a declaration for LAGEOS-2 (9207002) refuses `F-007`**, naming both identifiers; and the reader's resolution of the `H2` time scale is carried: a pass whose `H2` code is 5 (a CRD fixture cut from the real file) is refused by `read_crd_passes` with `IOFM-F-015` **and the builder returns that id unchanged**, while codes 3, 4 and 7 resolve to UTC | the diagnostics | `CRD2` field 6 and `H3`; `IOFM-A-036` (the map's own test) | exact | R-010, R-032, F-007 |
| `MEAS-A-013` | the SP3 and Horizons tags: header systems `GPS`, `TAI`, `UTC` resolve through `io::to_time_scale` (a `GPS` epoch is TAI + 19 s later than its UTC reading by the leap table: 2026-01-03 00:00:00 GPS = 2026-01-03 00:00:19 TAI); `GLO`, `GAL`, `BDT`, `QZS` refuse `F-014`; the real ILRS file's header (`UTC`) resolves; a Horizons token other than `TDB` refuses `IOHZ-F-002` unchanged | the offsets | `SP3D`; `TIME-R-…` | exact | R-010, F-014 |
| `MEAS-A-014` | a CRD record's epoch is the `H4` start date plus the seconds of day: the **real** first normal point (`11 7676.800587100000 …`, start 2026-01-01) is 2026-01-01 02:07:56.8005871 UTC = 02:08:33.8005871 TAI (TAI − UTC = 37 s) to 1 ns; a pass with no normal point refuses `F-023` | the epoch | `CRD2` §0; the leap table | 1 ns | R-011, F-023 |
| `MEAS-A-015` | **a pass crossing 00:00 UTC** (start 23:59:40 on 2026-03-14; records at 86 395.2, 86 399.9, 0.3, 4.7 s of day): the epochs are non-decreasing, the last two on 2026-03-15; **the wrong rollover (every record on the start's date) is shown failing**: its epochs run backwards by a day and the same monotonicity assertion fails | the epochs | `CRD2` §0 | 1 ns | R-011, F-019 |
| `MEAS-A-016` | leap seconds: seconds of day 86 400.5 on 2016-12-31 (a UTC day ending in a positive leap second) is accepted as 23:59:60.5 UTC; **the same value on 2026-01-01 refuses** (`Epoch::from_calendar`'s refusal carried); 86 401 and negative values refuse `F-021` naming the value and the day | the epochs and diagnostics | `CRD2` ("a leap second would use 86400"); `TIME-R-…` | exact | R-011, F-021 |
| `MEAS-A-017` | a pass whose epochs run backwards after the rollover rule, or span more than a day, refuses `F-019` naming the two records; the in-order pass of `MEAS-A-015` does not | the diagnostics | `CRD2` §0 | exact | R-011, F-019 |

### 8.3 Troposphere and Shapiro

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-020` | **G2, the zenith delay**: lat 30.67166667°, ellipsoidal height 2010.344 m, P 798.4188 hPa, e 14.322 hPa, λ 0.532 µm gives ZTD **1.935225924846803114 m**, ZHD 1.932992176591644462 m, ZWD 0.2233748255158703871 × 10⁻² m. Asserted: each within **1 mm** (`MEAS-P-11`, the source's stated accuracy) of the printed value. **Reported beside it, not gated**: the printed equations give 1.935229724968 m, **+3.8 µm** (ZHD +3.796 µm, ZWD +4.5 nm) from the printed value, 2.0 ppm — the cause was not looked for (it would mean reading the routine). **Also asserted**: the implementation agrees with an independent 50-digit evaluation of the printed equations to 1 × 10⁻¹² m, so the 1 mm is not hiding an implementation error | the three printed values | `IERS-CASES` (`FCUL_ZD_HPA` prolog), cited with the routine's name; `TN36-9` eqs (9.3)–(9.7) | 1 mm to the printed value; 1 × 10⁻¹² m to the evaluation | R-020 |
| `MEAS-A-021` | **G2, FCULa**: lat 30.67166667°, height 2075 m, T 300.15 K, elevation 15° gives **3.800243667312344087** | printed | `IERS-CASES` (`FCUL_A` prolog); `TN36-9` Table 9.1 | 1 × 10⁻¹² (observed 0.0 at 15 digits) | R-021 |
| `MEAS-A-022` | the water-vapour pressure from relative humidity, `MM73` eq. (22): at (20 °C, 50 %), (0 °C, 100 %), (−10 °C, 30 %), (35 °C, 80 %) equal to an independent 50-digit evaluation of the printed formula; zero humidity gives zero; the value is monotonic in both arguments | the evaluation | `MM73` eq. (22); `tools/measmod_reference.py` | 1 × 10⁻¹² relative | R-022 |
| `MEAS-A-023` | **G2, FCULb**: lat 30.67166667°, height 2075 m, day of year 224, elevation 15° gives **3.800758725284345996**. **Negative control (rule 5):** the same evaluation with φ_d in place of φ_d² (the misreading the PDF text layer invites) misses by 2.5 × 10⁻⁴, and the assertion fails for it | printed | `IERS-CASES` (`FCUL_B` prolog); `MPPL02` Table 1, eq. (6), read from the rendered page | 1 × 10⁻¹² | R-021 |
| `MEAS-A-024` | the leg delay `ZTD · m(sin e)` at a stated geometry equals the product of the two tested terms; the elevation limit: 3.0° accepted, **2.99° refuses `F-010`**, a target below the horizon refuses; **the height sensitivity is measured**: changing `H` by 30 m (a typical geoid undulation) changes the one-way delay at 15° elevation by less than **0.2 mm** and by 100 m by less than **0.5 mm**, both under the zenith model's own 1 mm (predicted **0.115 mm** and **0.382 mm** by an independent evaluation, `tools/measmod_height_sensitivity.py`, written before the C++ ran). *[Corrected 2026-10-06, before the test was run, the first text kept: "changing `H` by 100 m changes the one-way delay … by less than 0.1 mm (predicted 21 µm from `a₁₃ ΔH` and `∂m/∂a₁ ≈ −50`)" — an estimate from one term of the mapping function that was off by a factor 8 and omitted the height dependence of the zenith delay's own `f_s` (2.8 × 10⁻⁷ per metre); the criterion is restated against the model's own 1 mm.]* **A non-physical atmosphere** (pressure ≤ 0, wavelength ≤ 0, temperature −100.1 °C or 60.1 °C, humidity −0.01 or 1.01, a non-finite value) **refuses `F-022`**, the same validator the angle refraction uses (`MEAS-A-086`) | the product; the diagnostics; the bounds | `TN36-9` §9.1.2 ("greater than 3 degrees") and its footnote 1 | 1 × 10⁻¹² (product); 0.2 mm and 0.5 mm (sensitivity) | R-023, R-037, F-010, F-022 |
| `MEAS-A-025` | **G4, Shapiro**: `(2GM/c²) ln[(r₁+r₂+ρ)/(r₁+r₂−ρ)]` at three geometries (LAGEOS-like zenith 5.80 mm; 10° elevation 9.88 mm; a LEO case, 2.25 mm) equals an independent 60-digit evaluation (the generator and the test share the three input triples, the doubles the test passes) to 1 × 10⁻¹² m — **labelled not published**: `TN36-11` prints the formula and no number | the evaluation | `TN36-11` eq. (11.17); `tools/measmod_reference.py` (two precisions agreeing) | 1 × 10⁻¹² m | R-024 |
| `MEAS-A-026` | the Shapiro term's partials `∂/∂ρ`, `∂/∂r₁`, `∂/∂r₂` equal the independent evaluation's high-precision derivatives to 1 × 10⁻¹⁰ relative; the term is symmetric in `r₁ ↔ r₂`, positive, and tends to 0 with ρ | the derivatives and identities | closed form; `tools/measmod_reference.py` | 1 × 10⁻¹⁰ relative | R-024 |

### 8.4 The range model

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-030` | **G3, event 2**: target **and station** in uniform motion (the station at 465 m s⁻¹, the target at 5.7 km s⁻¹, a LAGEOS-like geometry, a second case at LEO range), zero leg delay (`solve_two_way` with no delay): the model's time of flight equals the closed form from the two quadratics `(c² − v_r²) τ_u² − 2 (D·v_r) τ_u − D² = 0` (target moving, station at the transmit epoch) and `(c² − v_s²) τ_d² + 2 (E·v_s) τ_d − E² = 0` (station moving, target at the bounce), derived independently of the iteration and evaluated in 50 digits | the closed form | closed form; `tools/measmod_reference.py` | **1 × 10⁻⁸ m** of range | R-030, R-031 |
| `MEAS-A-031` | **G3, event 1** (the tag is the bounce): the up leg solved backwards, `(c² − v_s²) τ_u² − 2 (E·v_s) τ_u − E² = 0`; the same configurations | the closed form | as `MEAS-A-030` | 1 × 10⁻⁸ m | R-030 |
| `MEAS-A-032` | the **Earth-fixed station** through the real chain (`EarthFixedStation`, real EOP, the leap table): the time of flight agrees with an **independent bisection solver** that uses the same station track at arbitrary epochs (so only the solver is independent) to 1 × 10⁻⁹ m; **the Earth's rotation between transmit and receive is in the model**: freezing the station at the transmit epoch (the caller-side mistake) changes the range by more than 1 m at a 40° elevation east of the station (predicted 0.35 km s⁻¹ × 22 ms = 7.7 m), and the assertion fails for it | agreement; the effect | closed form | 1 × 10⁻⁹ m; the effect ≥ 1 m | R-030, R-033 |
| `MEAS-A-033` | the iteration: an ordinary LAGEOS leg converges in at most 5 passes (reported in `Applied`); **a target moving at 1.2 c away from the station** (contraction factor 1.2, no fixed point) refuses `F-011` after 20 passes, naming the leg, the passes and the last change — **the refusal shown firing** (rule 7); a trajectory that refuses mid-iteration returns its own refusal | the pass counts; the diagnostic | `MEAS-R-031` | exact | R-031, F-011 |
| `MEAS-A-034` | the CRD session checks on fixtures cut from the real file's `H4` (`0 0 0 0 1 0 2 0`): range type 1 refuses `F-008`; refraction-applied 1, centre-of-mass-applied 1 refuse `F-009`; **system-delay-applied 0 refuses `F-009`**; epoch events 0, 3, 4, 5, 6 refuse `F-008`; events 1 and 2 and the real header pass; each diagnostic names the field and the value | the diagnostics | `CRD2` §3.4, field numbers | exact | R-034, F-008, F-009 |
| `MEAS-A-035` | the meteorology: between two bracketing records `(1000 mbar, 280 K, 50 %)` at 100 s and `(990, 282, 60)` at 200 s the value at 150 s is `(995, 281, 55)`; a tag outside the records' span holds the nearest record and reports `held` with the distance; **the real file's first normal point (7676.8005871 s) is before its first record `20` (7676.801 s): held, 0.4129 ms**; a pass with no record `20` refuses `F-012` | the values and flags | `CRD2` §3.4 notes ("a 2-point linear interpolation will usually suffice"); the real file | 1 × 10⁻¹² (interpolation); 1 µs (distance) | R-035, F-012 |
| `MEAS-A-036` | the wavelength from `C0` field 3 for the normal point's configuration id: the real file's configuration `new` gives **532.000 nm**; with two `C0` records (`new` 532.0, `old` 1064.0) the record's own id selects; **an id with no `C0` refuses `F-012`**; the accessor lives in `io` (§9.2) | the values | `CRD2`; the real file | exact | R-036, F-012 |
| `MEAS-A-037` | the centre of mass: `R_model(δ) − R_model(0) = −δ` exactly for δ = 0.251 m (LAGEOS-like); **the wrong sign (adding δ) is shown failing**: it differs by 2δ = 0.502 m | −δ | `MEAS-R-032` and the sign convention of §3.3 (ILRS: added to the measured range to refer it to the centre of mass) | 1 × 10⁻¹² m | R-032 |
| `MEAS-A-038` | refusals around the centre of mass and the observed value: a non-spherical target refuses `F-013`; a correction of −0.001 m and of NaN refuses `F-017`; a blank citation refuses `F-006`; a time of flight of 0, −1, NaN and infinity refuses `F-018`; **0.251 m with a citation and a time of flight of 0.05 s pass** | the diagnostics | `MEAS-R-032` | exact | R-032, F-006, F-013, F-017, F-018 |
| `MEAS-A-039` | the observed one-way-equivalent range of the real first normal point: `c · 0.051212898595 / 2` = 7 676 …m (the 15-digit product, computed independently) | the product | the real file; `MEAS-R-032` | 1 × 10⁻⁹ m | R-032 |
| `MEAS-A-040` | the `Applied` record of a modelled range: the two legs' geometric ranges, light times (`c · τ = ρ + Δ` per leg to 1 × 10⁻⁹ m), `Δ_atm` and `Δ_S` (the two legs **differ**, from their own elevations and station epochs: `MEAS-R-033`), the ZTD and mapping-function value, `δ_com` with its citation, the meteorology and `held`, the registry release strings, `post_seismic_override_used`, the passes per leg; and the **omitted list, never empty**, containing exactly these names — `solid_earth_tide`, `ocean_tide_loading`, `pole_tide`, `atmospheric_loading`, `tropospheric_gradients`, `itrs_realisation`, `sun_shapiro_delay`, `data_handling_biases`, `com_station_dependence` — each with a non-empty magnitude and source (§6.4) | the structure and the sums | `MEAS-R-038`, `MEAS-R-033` | 1 × 10⁻⁹ m | R-033, R-038 |
| `MEAS-A-041` | **the analytic rows equal the closed-form derivatives**: for the G3 configurations the position part of the row equals the derivative of the closed-form time of flight with respect to the target position (differentiated in 50 digits by a step of 10⁻²⁰ m), for both events, to 1 × 10⁻¹² relative — three orders tighter than any differenced row could meet, so the rows are **analytic** (`MEAS-R-061`); the event-1 and event-2 rows differ at `v/c` as the formulas say | the derivatives | closed form; `tools/measmod_reference.py` | 1 × 10⁻¹² relative | R-033, R-060, R-061 |
| `MEAS-A-042` | **G1, range, LAGEOS-like** (ρ = 6.6 × 10⁶ m with the target at 1.227 × 10⁷ m, so elevation 52° — *corrected 2026-10-06, before any run: the first text said "elevation 40°" without a target distance, see §6.2*; speed 5.7 km s⁻¹ across the line of sight **and** along it; the full model with atmosphere and Shapiro; the Earth-fixed station), **events 1 and 2**: criteria (a), (b), (c) of §6.2 against the frozen row; the test holds the frozen figures as literals **and** recomputes them from the formulas and asserts they agree (and that the frozen `ν` bounds three ulp of the largest component of the geometry's target); reported: whether the analytic row also lies inside the raw spread. **Amendment A1 (2026-10-06, made after the first run's miss of (b)): run once more in both configurations** — the real chain with `ν_real = ν + ν_chain` and an exact rigid rotation with the frozen `ν` — against the amended `ε(h)` of §6.2 (`F` over the stencil); the test holds the amended figures as literals **and** recomputes them (`ν_chain` from `3 u R⊥ cos(el) cos(az) w`, `F_geo` from its formula, `F_trop` from its literals) and asserts they agree | pass | §6.2 (written before any run) and Amendment A1 | the amended `ε(h)`, `B_pred` | R-060 |
| `MEAS-A-043` | **G1, range, LEO-like** (ρ = 1.5 × 10⁶ m with the target at 6.92 × 10⁶ m, elevation 15°, so the mapping function's derivative matters; speed 7.5 km s⁻¹, both directions), both events: the same, **and, by Amendment A1, in both configurations** | pass | §6.2 and Amendment A1 | the amended `ε(h)`, `B_pred` | R-060 |
| `MEAS-A-044` | **the gate can fail** (rule 5), for the range, **in both configurations of Amendment A1**: the model's row passes (b) and every wrong row — the geometric row `½(ĝ_u + ĝ_d)` and the row of the other event's formula, in both events, both geometries and both velocity directions — **fails (b)**; every wrong row whose relative difference ‖Δrow‖/‖row‖ from the model's is at least 1 × 10⁻⁵ (the target's `ĝ·v/c`, 1.9 × 10⁻⁵ and 2.5 × 10⁻⁵, or the atmosphere's gradient at 15°) fails by at least 10³ times `ε`, the test computing that difference from the rows themselves; in the rigid configuration the two wrong rows of every along-the-line-of-sight case fail by at least 10³ times `ε`, as first written (the first run's figures: 1.8 × 10³ to 1.8 × 10⁵); every violation is reported | failures | §6.2 and Amendment A1 | exact | R-060 |
| `MEAS-A-045` | **the boundary with L7**: with a drift trajectory parametrised **at `t₀`** (state `x₀` at `t₀`, `Φ(t, t₀) = [[I, (t − t₀)I], [0, I]]` exact for the drift) the finite difference of the model with respect to `x₀` equals the row mapped as `row · Φ(t_b*, t₀)` — **the model's row is the whole of the model's contribution; L7 adds nothing** — within the frozen `ε(h)` (both events; the target receding at 5.7 km s⁻¹); and for event 2 a row that additionally exported `∂t_b/∂x` and was mapped with the same `Φ` would double-count: the difference is the range rate over `c`, **1.9 × 10⁻⁵ relative** for the 5.7 km s⁻¹ of the geometry (the test asserts it within 15 %, the station's own radial velocity being the rest) and fails (b) by at least 10³ times `ε` ; **Amendment A1: in both configurations**, the finite differences within the amended `ε(h)` (the velocity columns within `Δt ε(h)`), and the double count failing (b) by at least 10³ times `ε` in each | agreement within `ε(h)` | `MEAS-R-060`; ruling R6; Amendment A1 | the amended `ε(h)` | R-060 |
| `MEAS-A-046` | **the Earth-orientation chain's output noise floor** (Amendment A1 (iii)): the real chain's station position minus an exact rigid rotation of its own matrix at its own rate, at 601 epochs 1 × 10⁻¹⁰ s apart about the tag: every difference is at most `3 u R⊥` = 4.76 × 10⁻⁷ m (`u` = 2⁻⁴⁵ rad, `R⊥` the station's distance from the Earth's axis), and the peak is at least `0.5 u R⊥` = 7.9 × 10⁻⁸ m — the floor exists and the bound can fail; the peak is reported (first measured 1.61 × 10⁻⁷ m, 1.01 `u R⊥`) | the bounds | ERFA's documented angle formula; `MEAS-P-22` | 3 `u R⊥` above, 0.5 `u R⊥` below | R-060 |

### 8.5 The interface

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-050` | `Modelled<…>` cannot be formed from raw numbers (`!is_constructible` for any combination of doubles, epochs and rows); the accessors' names carry the unit and, for epochs, the scale (`bounce_tt()`, `emission_tt()`, `epoch_utc()`); a `RaDec` result has no `azimuth` accessor (`requires`-checked) | the properties | `MEAS-R-062` | compile time | R-062 |
| `MEAS-A-051` | a refusal of a dependency is returned **with its own id** and the context in the message: a trajectory that refuses (its own id), a station track that refuses, an `EarthOrientation` whose `EopSeries` refuses (`EOP-F-…`), an `EarthMotion` whose ephemeris refuses (`EPH-F-…`), a `LeapTable` that refuses (`TIME-F-…`); no persistent error state (a failing call followed by a good call gives a good result) | the ids | `MEAS-R-063`; `SPEC-template.md` §5 | exact | R-063 |
| `MEAS-A-052` | **substitutability** (R-064): the identical `model_range`/`model_radec`/`model_azel`/`model_position` code path runs with a closed-form trajectory, a closed-form station track, a constant-rotation `EarthOrientation` and a constant-velocity `EarthMotion` (as `MEAS-A-030`, `-031`, `-093` do) and with the production ones | the same results to the closed forms | `MEAS-R-064` | per row | R-064 |
| `MEAS-A-053` | **no module-level or thread-local mutable state**: the module's sources contain no `thread_local`, no non-`const` namespace-scope or function-local `static`, and no global error slot (a text scan of `modules/measmod/` in the manner of `ci.sh` gate 9); and every public function returns `odl::Result` (constraint 8's tree-wide check covers the module) | the scan finds none | `MEAS-R-063`; plan §5 constraint 8 | exact | R-063 |

### 8.6 The ephemeris-position model

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-060` | **the plumbing identities, on the real vendored table** `horizons-acs3-vectors` (5 real records at 30-minute steps; ICRF, geocentre, TDB), **never against an SGP4 propagation** (round-8 D7): (i) a GCRS observation against a trajectory that returns the table's own state: residual exactly 0; (ii) an ITRS observation built from the same state by the frames chain — the real chain with `finals2000A`, its predictions allowed, since the table's epochs lie past the observed data of the pinned series (*the first text did not say which series; the pinned C04 ends in 2026-01*): residual below 1 × 10⁻⁹ m; (iii) **the 69 s defect shown failing**: the printed times read as UTC instead of TDB (the epochs come out 69.18 s **late**: the same digits are a later instant in UTC, since the TDB clock runs 69.184 s ahead of UTC in 2026 — *the first text said "early"*) against a two-body propagation from the nearest record gives a residual above **100 km** (7.4 km s⁻¹ × 69 s = 510 km), and the identity assertion fails for it; (iv) **an error of 0.5 s in the Earth's rotation** — the orientation evaluated 0.5 s late, which is the rotation of a UT1 error of 0.5 s to 10⁻⁵ (*the first text said "in the EOP"; the series are read from fixed-width files and a modified copy would be a second parser's work*) — gives a residual of 0.5 s × 7.292 × 10⁻⁵ rad s⁻¹ × the satellite's distance from the Earth's axis — 265, 50, 242, 169 and 164 m for the five records, **above 200 m where that distance exceeds 5.5 × 10⁶ m** (two of the five; the first text's 255 m was the case of 7 × 10⁶ m) | 0; < 1 nm; > 100 km; > 200 m | closed identity; `HZAPI`; round-8 D7 | per row | R-040 |
| `MEAS-A-061` | the SP3 frame-code table: `IGS05 IGS08 IGb08 IGS14 IGb14 IGS20 IGb20 SLR20` map to ITRS (the real ILRS file's `SLR20` included, read from its header); `XYZ99`, an empty code and a lower-case code refuse `F-014`; the reference point: `CentreOfMass` accepted, `AntennaPhaseCentre` and `Unspecified` refuse `F-014`; SP3 positions are kilometres and velocities decimetres per second; a position of exactly (0, 0, 0), the format's marker for an absent position, refuses `F-018`, as does a NaN component | the table | `SP3D`; `MEAS-R-041`, `-042` | exact | R-041, R-042, F-014, F-018 |
| `MEAS-A-062` | **G1, position**: the row `[M 0]` against finite differences of the model — a linear function (`F` = 0), so the estimates carry only noise (`ν/h`, §6.2); (a) upper half and (b), (c) of §6.2 for the GCRS and for the ITRS observation, the nominal state the first record of the vendored table and the family the drift of §6.2 with the variation at the observation epoch; **no station chain and no epoch that moves with the perturbation enters** (the rotation is evaluated at the observation epoch, the same in every evaluation), so Amendment A1's chain term is zero and the frozen sizing applies unchanged (the manager's ruling, 2026-10-06), with the frozen `ν` bounding three ulp of the largest component of the nominal state, asserted; **a row without `M` (the identity, for the ITRS observation) fails (b) by at least 10⁶ times `ε`** (*the first text said "by a factor of order 1": the error of that row is of order 1 and `ε` is 10⁻⁹ or less*) *First and only run 2026-10-06, alone, log kept (`PROVENANCE.md` §39.12): (a) B = 0 (GCRS) and 3.1 × 10⁻¹¹ (ITRS) against the upper bound 5.6 × 10⁻⁹; (b) worst `|f̂ − a|/ε` 0 and 0.075; (c) every velocity estimate exactly 0; the identity row fails (b) by 1.0 × 10¹⁰ `ε` and `M` transposed by 2.0 × 10¹⁰ `ε` in the ITRS case.* | pass; the control fails | §6.2 | the frozen noise bound | R-060 |
| `MEAS-A-063` | the builders: a Horizons record becomes a GCRS observation at its TDB epoch (`io::to_time_scale`) with the position in metres through `odl::metres_from_km` (the one km/m crossing, `unitcheck` gate 12) — the vendored table's first record, 2026-09-25 00:00:00 TDB, 7 049.479204680989, −1 823.447981968663, 760.5020081570782 km, is 7 049 479.204680989 m, −1 823 447.981968663 m, 760 502.0081570782 m, 7 321.098 km from the centre; an SP3 record becomes an ITRS observation at its header-system epoch — the real ILRS file's first record (2025-12-28 00:00:00 UTC, `PL51 -11319.687869 -4845.099064 -497.695158`) is read as **12 323.069 km** from the centre (*the first text said 12 327 km: an arithmetic slip, found by the independent reference*) and its epoch is the UTC instant, not GPS time | the values | the files | 2 ulp of the component (1.9 × 10⁻⁹ m to 3.7 × 10⁻⁹ m at 7 × 10⁶ … 1.1 × 10⁷ m: *the first text's 1 × 10⁻⁹ m was below one ulp of an SP3 coordinate*) | R-040, R-010 |

### 8.7 The angle models

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-080` | the aberration `A(n; β)` and its exact inverse `D`: `A(D(n; β); β) = n` to 1 × 10⁻¹⁵ on a grid of directions and `β` up to 10⁻³; `β = 0` is the identity; the first-order shift is `β − (n·β) n` with error `β²`; **the wrong sign in `D` (the self-caught slip recorded in `PROVENANCE.md` §39) fails the inverse test** | the identities | closed form | 1 × 10⁻¹⁵ | R-053 |
| `MEAS-A-081` | `A` agrees with `ERFA`'s `eraAb` with the solar term switched off (a Sun distance of 10³⁰ AU) to 1 × 10⁻¹⁵ | agreement | `ERFA`'s formula | 1 × 10⁻¹⁵ | R-053 |
| `MEAS-A-082` | **no default reduction**: `angle_observation` is not invocable without the `reduction` argument (a `requires`-expression); a reduction that does not fit the observation's coordinate kind refuses `F-015` (`Astrometric` for an azimuth/elevation line, `ApparentRefracted` for a right-ascension/declination line) | the properties and diagnostics | `MEAS-R-050` | compile time; exact | R-050, F-015 |
| `MEAS-A-083` | the IOD angle decoder: the seven formats (RA/Dec formats 1, 2, 3, 7 and azimuth/elevation formats 4, 5, 6) against the document's own examples and hand-built lines; blanks are zero; the sign of the declination and the elevation; the `MX` uncertainty field → `sigma_rad`; right ascension in [0, 2π), azimuth in [0, 2π) | the decoded values | `IODFMT` | 1 × 10⁻¹⁵ rad | R-055 |
| `MEAS-A-084` | the epoch-code policy: **code 5 accepted as ICRF** (the 23.2 mas bias, below the format's precision, `MEAS-R-055`); codes 1, 2, 3, 4, 6 each refuse `F-016`; code 0 refuses without a declared equinox and is accepted with `MeanEquinoxOfDate` or `TrueEquinoxOfDate`, rotating the direction by the declared system at the observation epoch; the **size** of the rotation for mean-of-date at 2026.0 is the general precession (50.29″ yr⁻¹ × 26 yr = 1307″ in ecliptic longitude, within 1 %); at J2000.0 it is the frame bias (23 mas) | the policy; the sizes | `IODFMT`; `TN36-5`; the IAU precession rate | exact; 1 % | R-055, F-016 |
| `MEAS-A-085` | the refraction: `A` and `B` are `eraRefco`'s for the supplied atmosphere; the solved observed zenith distance satisfies `z_v = z_o + A tan z_o + B tan³ z_o` to 1 × 10⁻¹⁴ rad; at zenith distance 0 the refraction is 0; it is monotonic in zenith distance and of the order of 1′ at 45° (a magnitude, not a gate) | the equation | `ERFA` `eraRefco` | 1 × 10⁻¹⁴ rad | R-054 |
| `MEAS-A-086` | refusals: a vacuum zenith distance above 75° refuses `F-010` (74.9° accepted, 75.1° refuses); atmospheres with pressure ≤ 0, wavelength ≤ 0, temperature −100.1 °C or 60.1 °C, humidity −0.01 or 1.01 each refuse `F-022`; the accepted neighbours pass | the diagnostics | `MEAS-R-054` | exact | R-054, F-010, F-022 |
| `MEAS-A-087` | the diurnal aberration of `ApparentRefracted`: the apparent direction is displaced **toward the observer's velocity** (east for a station at rest in the Earth) by `0.32″ · sin θ` (`MEAS-P-10`) with θ the angle between the line of sight and the velocity, to 1 × 10⁻⁹ rad of the first-order value; the shift vanishes along the velocity | the size and sign | `MEAS-P-9`, `MEAS-P-10`; closed form | 1 × 10⁻⁹ rad | R-054 |
| `MEAS-A-088` | the emission epoch and the geometric direction in closed form: a uniformly moving target and observer, `solve_emission` equals the closed-form root of `(c² − v²) τ² − 2 (D·v) τ − D² = 0` to 1 × 10⁻¹⁵ s *[corrected 2026-10-06, when the test was written, first text kept: with `D = r(t_o) − s` the target's position at the observation epoch relative to the observer and `v` the target's velocity — the target at emission is at `r − v τ` — the middle term is `+ 2 (D·v) τ`, the root `τ = [−D·v + √((D·v)² + (c² − v²) D²)]/(c² − v²)`; a target receding along the line of sight gives `τ = D/(c + v)`, which only this sign does]*; the `Geometric` right ascension and declination equal `atan2` and `asin` of the closed-form direction to 1 × 10⁻¹⁵ rad | the closed form | closed form; `tools/measmod_reference.py` | 1 × 10⁻¹⁵ s, rad | R-051, R-052 |
| `MEAS-A-089` | *[added 2026-10-06, with Amendment A1 (iv)]* **the observer, the Earth's orientation and the Earth's motion are evaluated at the observation epoch and nowhere else**: a recording wrapper of each of the three abstractions, an angle model of each reduction (`Geometric`, `Astrometric`, `ApparentRefracted`) over a target whose trajectory is varied by 1 000 m and by 10 m s⁻¹ (the stencil of §6.2): every epoch the station was asked at, every epoch the orientation was asked at and every epoch the motion was asked at equals the observation epoch exactly, and the target was asked at epochs that move with the variation — this is **why the chain's double-precision floor cancels in the angle gate's difference** | the epochs asked | structure | exact | R-051, R-053, R-054 |
| `MEAS-A-090` | **the aberration test** (ruling R5): the `Astrometric` direction against an **independent construction** — the direction from the observer's barycentric position at `t_o` to the target's barycentric position at the light-time-corrected emission, the Earth's barycentric position from `ephemerides` at both epochs — agrees to **4.1 mas** (`2 β_E²`: the second-order terms by which the exact special-relativistic composition differs from the construction, written before the run); **the bare GCRS direction, with no aberration, is shown failing the same assertion** by 18″ to 20.5″ depending on the line of sight (`MEAS-P-8`) *[corrected 2026-10-06, when the test was written, first text kept: the shift of the bare direction is `β_E(t_o) sin θ` with `β_E` the Earth's speed at the epoch over `c` — 29.29 km s⁻¹ at the July aphelion to 30.29 km s⁻¹ at the January perihelion, 20.15″ to 20.84″; 20.49″ is the figure for the mean speed — and `θ` the angle between the line of sight and the Earth's velocity; the test's lines of sight at θ = 70°, 90° and 110° give 18.9″ to 20.84″, at two epochs, January and July]* | agreement within 4.1 mas; the control fails by ≥ 10″ | `TN36`; `ephemerides` (de440s); first principles | 4.1 mas | R-053 |
| `MEAS-A-091` | `Astrometric` minus `Geometric` for a set of lines of sight is `−β_E⊥` to first order (error `β_E²`), at most 20.49″, and zero along the Earth's velocity *[corrected 2026-10-06, when the test was written, first text kept: "at most `|β_E|`" — 20.15″ at the July aphelion to 20.84″ at the January perihelion; 20.49″ is the mean-speed figure of `MEAS-P-8`, which a perihelion line of sight at 90° exceeds by 1.7 %; the test asserts the shift against the ephemeris's own `β_E` at each of the two epochs, and that it exceeds 20.49″ in January and falls below it in July]* | the first-order shift | `MEAS-P-7`, `MEAS-P-8`; §3.5 | `β_E²` = 2.0 mas | R-053 |
| `MEAS-A-092` | the local-frame algebra of `ApparentRefracted` with a constant `EarthOrientation` (a test-supplied rotation): a target due north at 45° gives azimuth 0 and elevation 45° (vacuum); due east gives azimuth 90°; at the zenith the elevation is 90°; the sign conventions of §3.3 *[amended 2026-10-06, when the test was written, first text kept: a target 1 × 10⁻⁶ rad from the zenith gives an elevation of 90° less that angle, to 1 × 10⁻¹², and a finite partial; a target within 1 × 10⁻⁹ rad of the zenith refuses `F-026` (`MEAS-R-057`), as does a right-ascension and declination observation at the celestial pole of its frame]* | the angles; the refusal | closed form | 1 × 10⁻¹² rad | R-054, R-057, F-026 |
| `MEAS-A-093` | the same model with the **production** `EopEarthOrientation` and `EarthFixedStation` and the closed-form substitutes agree where they must (a station on the rotation axis; the equator at a stated epoch), so the closed-form tests of this section and the production path are the same code | agreement | `MEAS-R-064` | 1 × 10⁻⁹ rad | R-064 |
| `MEAS-A-094` | the `AngleObservation` carries its reduction, its frame (ICRF or of date), its site and its sigma; an observation built from a real IOD line of `iod_tests`'s fixtures (station number from the caller's registry) has the decoded values and a UTC epoch to 1 ms; the `Applied` record of an angle model carries the reduction, the emission epoch and light time, the aberration and refraction applied, the atmosphere and the frame | the values | `IODFMT` | 1 ms; 1 × 10⁻¹⁵ rad | R-055 |
| `MEAS-A-095` | the residual: `observed − modelled` per component, the right-ascension or azimuth difference **wrapped to (−π, π]** (359.9° observed against 0.1° modelled is −0.2°; 0.1° against 359.9° is +0.2°); no `cos δ` scaling | the residuals | `MEAS-R-056` | 1 × 10⁻¹⁵ rad | R-056 |
| `MEAS-A-096` | **G1, right ascension and declination, `Astrometric`** (ρ = 1.2 × 10⁶ m, declination within ±40°, Earth velocity from the ephemeris): criteria (a)–(c) of §6.2 against the frozen angle row — **sized with the two amended terms of Amendment A1 (§6.2 (iv)) before its first run, and run once in both configurations** | pass | §6.2 and Amendment A1 | the frozen `ε(h)`, `B_pred` | R-060 |
| `MEAS-A-096g` | **the angle gate's geometry rule** (§6.2 (v)): the line of sight of each of the four gates is selected from the geometry alone — no model is run — at azimuth 0° and elevation 30°, with the declination (or elevation) and the visibility score the specification states (+31.01°, 0.6548; +31.01°, 0.6548; 30°, 0.4853; +30.95°, 0.6549), the target 7.053 × 10⁶ m from the geocentre in the sizing's binade of the ulp, the slant range 1.2 × 10⁶ m, the observation's weather the first normal point's | the geometry | §6.2 (v) | the stated values | R-060 |
| `MEAS-A-097` | **G1, `Geometric`**: the same | pass | §6.2 | the frozen figures | R-060 |
| `MEAS-A-098` | **G1, azimuth and elevation, `ApparentRefracted`** (elevation within 20°…40°; the diurnal aberration, the local frame and the refraction's `dz_o/dz_v` in the row): the same *[amended 2026-10-06 after its one run, first text kept: the control without the diurnal aberration's Jacobian is computed and logged but not asserted — it had no power at the rule-selected geometry (0.47 `ε`, §6.2 (v)); its claim is `MEAS-A-098b`'s]* | pass | §6.2 | the frozen figures | R-060 |
| `MEAS-A-098b` | **the diurnal-aberration check** (§6.2 (vi), registered after the angle gate's one run and committed before its code exists): G1 for the azimuth/elevation row of `ApparentRefracted` at a closed-form geometry — the identity orientation, the observer at 465.1 m s⁻¹ **along the line of sight**, toward and then away from a static target at azimuth 90°, elevation 30°, ρ = 1.2 × 10⁶ m, the sizing of (v)'s procedure evaluated at that geometry (`ν` = 2.9944 × 10⁻¹⁵, `B_pred` = 3.0028 × 10⁻¹³): the model's row passes (a), (b), (c); **the row without `A′` and the row with `A′` of the wrong sign each fail (b) by at least 10³ `ε`** (predicted 4.5 × 10³ and 9.1 × 10³, written before the run); run once, alone | pass; the controls fail | §6.2 (vi) | the frozen figures | R-054, R-060 |
| `MEAS-A-099` | **G1 for "of date"** (the equinox rotation inside the row) and **the gate can fail** (rule 5) for angles: a row without the aberration Jacobian, one without `dz_o/dz_v`, and one without the emission-epoch shift `δt_e` each fail (b) by at least 10³ times `ε` while the correct row passes, in both configurations *[refined 2026-10-06 in §6.2 (v), before any angle-gate code, first text kept: the shift `δt_e` is omitted in the across-the-line-of-sight geometry only — along it `(I − n̂n̂ᵀ) v = 0` and the rows coincide; the diurnal aberration's Jacobian of `ApparentRefracted` is 65 times smaller than the annual one and is asked to fail by 10² only; a control without the frame's rotation is added for the of-date observation; each control's row is a reconstruction that must first agree with the model's partials to 1 × 10⁻⁹ relative; and the two configurations must agree within the tolerance of §6.2 (v)]* | pass; the controls fail; the configurations agree | §6.2 and Amendment A1 | the frozen figures | R-060 |

### 8.8 Real data, and the layer's exit gate

| id | what is checked | expected | source | tolerance | discharges |
|---|---|---|---|---|---|
| `MEAS-A-100` | **G5, the registered check** (ruling R1), in this order, each step a commit: **(1) data pinned first** — the daily or monthly normal-point file of the chosen station, the ILRS combined orbit SP3 of the week (`ILRSORB`), the DHF release read to choose and its hash recorded, the centre-of-mass value of the target with its citation — in `manifest/manifest.json`; the **station and pass are chosen from metadata only**, before any residual is formed, for **a small eccentricity uncertainty and a pass with no Data Handling File entry**, and the choice and its reasons are recorded; **(2) the envelope committed before anything is compared** (`MEAS-A-101`); **(3) the comparison once**: the model's residuals (observed − modelled, **no bias removed**, no outlier rejected beyond the normal point's own quality flag) over every normal point of the chosen pass, against the committed envelope; **inside the envelope passes; outside is a finding and the step does not close on it**; the result is reported with the envelope beside it, whichever way it falls | residuals inside the envelope | `ILRSORB`'s stated accuracy; the envelope's terms | the envelope | R-030, R-032, R-038 |
| `MEAS-A-101` | **the envelope**, committed as a test **before the comparison code exists**: every omitted term sized from a source or from first principles — the solid Earth tide at each normal point's epoch from `h₂`, `l₂` and the equilibrium tide with the Sun and Moon from `ephemerides` (a test-side helper, **not a model term**); the pole tide ≤ 25 mm radial and 7 mm horizontal (`TN36-7`); ocean loading at the chosen station, **bounded with its source** (the general bound of 100 mm from `TN36-7` if no station value is published); the orbit product's stated accuracy (from its own README, read when the envelope is written); the spread of the centre-of-mass value (the table's station dependence, ≈ 4.5 mm for LAGEOS); the mapping-function error (`MPPL02`); the eccentricity's stated precision; each term printed with its basis; the sum rule (worst-case linear sum, stated) | the envelope's numbers, printed | `TN36-7`, `ILRSORB`, `ILRSCOM`, first principles | — | R-038 |
| `MEAS-A-102` | **the L6 exit gate** (`tests/l6_exit_gate.cpp`): **every format round-trips** (CRD, IOD, SP3, SINEX, CPF, TLE, ANTEX, Horizons: `read(write(read(x))) == read(x)` on the real or published fixtures of `IOFM-A-013…`) **and a measurement model returns residual and partials for all three observation types** — a range (`model_range`), an ephemeris position (`model_position`) and an angle (`model_radec` and `model_azel`) — each from a real-format source, through the single interface, with a partials row of the stated shape (1×6, 3×6, 2×6) | the properties | plan §3.7's exit gate | exact | R-062 |

**Coverage.** Every requirement and refusal above is discharged by a row; none require excusing.

---

## 9. Provenance obligations

`PROVENANCE.md` §39 (numbered after §38, the last of step 3) records, as the module lands:

1. **Every source of §2**, with its hash, where it was fetched and when; the three registry pins and their licence identifier **`ILRS-PUBLIC`** — the ILRS page's own words quoted verbatim ("The data and products are not copyrighted; however … we request that you include the following citation: …"), the allowlist entry in `tools/fetch.py` with its reason, and the citations NOTICE carries (Pearlman et al. 2019; Altamimi et al. 2022; the ILRS contribution to ITRF2020's DOI).
   **The deviation from ruling R3 is flagged, not hidden**: `itrf2020-psd-slr` (2 001 bytes) is pinned **for its event list only**, because refusing a site after a post-seismic event (`MEAS-R-005`) needs the list; R3 had deferred the PSD pin.
2. **The handling of the IERS Fortran routines**: which files were fetched (`FCUL_ZD_HPA.F`, `FCUL_A.F`, `FCUL_B.F`, and `DEHANTTIDEINEL.F` for its prolog), that **only the comment prolog was kept**, the one disclosure (the first listing of `FCUL_ZD_HPA.F` printed every comment line, including fifteen in-body comment lines naming equations and constants, no code line), that no routine body was read, copied or translated, and that the printed test values enter the tree as published observations with the routine named and cited. The two tarballs that contain code (`orbitNP`, the CoM tables' reader) were not opened beyond their README and licence.
3. **The registry's data facts**, each reconciled (rule 3): 186 pads, 483 SODs listed, 482 placed, 60 unplaced; 235 pads and 542 SODs in the eccentricity file; marker `7307 B` listed with no solution; **184 + 2 = 186**; 28 markers with more than one solution and 48 gaps between consecutive solutions (the file's own); the PSD event list; the day-end convention `86399`.
4. **The frozen sizing of §6.2 as written before the first run**, with the outcome beside it once the gate has run, and **the refinement** of the round-8 proposal (the noise bound from the operands' ulp, not the output's) and why; the tool `tools/measmod_fd_sizing.py` (with its series scan) and its output.
5. **The G5 record**: the order of commits (data pinned → envelope → comparison), the station and pass chosen and the reasons, the DHF hash read, the envelope's numbers, the comparison's numbers and whether they fell inside — reported whichever way they fall.
6. **The self-caught slip** in the derivation of `D` (the sign of the `β` term in the first draft), kept visible with its correction (`MEAS-A-080` fails the wrong form).
7. **The absences and their searches** (§8.0), by reference to the round-8 report and `tools/measmod_reference.py`'s headers.

### 9.2 Amendments to `SPEC-io-formats.md` this step requires

`SPEC-io-formats.md` is amended in this same step, by version bump to 1.1 (its own changelog), with new `IOFM` rows and acceptance rows in `modules/io/tests/`:

- **`IOFM-R-008`, pass grouping** for CRD (`read_crd_passes`): one pass per `H4`…`H8` block, with the `H2`, `H3` and `C0` records in force when it opens, the normal points (`11`) and full-rate records (`10`) typed, the meteorology (`20`) typed, every other record kept in file order; structure refusals `IOFM-F-013`;
- **`IOFM-R-009`, the `C0` wavelength accessor** (field 3, nanometres, by configuration id; an id with no `C0` refuses `IOFM-F-014`);
- **`IOFM-R-010`, the `H2` time-scale resolution**: codes 3, 4, 7 resolve to UTC and every other code is refused by `read_crd_passes` (`IOFM-F-015`), so a pass carries its scale already resolved (ruling R8);
- **`IOFM-R-011`, the IOD angle decoder** (the seven formats, blanks as zero, the sign column) and **`IOFM-R-012`, the uncertainty decoder** (`M × 10^(X−8)` in the unit the format names); refusals `IOFM-F-016` and `IOFM-F-017`;
- **v1.2, item 5 — two departures of the real ILRS weekly orbit from `SP3D`, found by reading it:** its comment lines begin `%/*` (`IOFM-R-014`) and it has no `EOF` line (`IOFM-R-013`: read if the header's epoch count is met, otherwise refused as truncated under the new id `IOFM-F-018`; `Sp3File::eof_present`); `IOFM-A-042` … `-044`;
- the correction of `IOFM-Q-001`'s statement that this step does not need the configuration records: it needs the `C0` wavelength, and this amendment says so in the first text's place (kept visible);
- nothing in `io` interprets physics: the aberration, refraction, frame and epoch-code policies, and the ILRS-specific column reading of the SINEX blocks the registry uses (`SITE/ID`, `SOLUTION/EPOCHS`, `SOLUTION/ESTIMATE`, `SITE/ECCENTRICITY`, with their SOD extension), live here, in `measmod`, as `IOFM-Q-002` anticipated.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `MEAS-Q-001` | **The post-seismic sites after their events.** The eight PSD sites (7110, 7237, 7308, 7328, 7358, 7403, 7405, 7838) refuse from their events onwards — for 2026 data, every one of them — unless the single override is set per call: the linear SLRF2020 position then carries the unmodelled post-seismic motion (centimetres to decimetres for the 2011 events). The PSD corrections are the named follow-on with the solid Earth and pole tides. Recommendation: keep the refusal; schedule the PSD corrections with station displacement when a campaign needs a PSD site. |
| `MEAS-Q-002` | **`itrf2020-psd-slr` pinned for its event list only** (§9.1), against ruling R3's "PSD deferred". Recommendation: keep it — the refusal cannot exist without the list — and rule on whether the corrections' pin (the same file's parameters) should be treated as pinned already. |
| `MEAS-Q-003` | **G5's station and pass** are chosen by metadata at item 7, reading the DHF to choose and recording its hash (ruling R1). If no pass of the January file has a small eccentricity uncertainty and no DHF entry, which criterion relaxes first? Recommendation: the eccentricity's, since the file prints it to 1 mm and the DHF's biases are the larger unmodelled term. |
| `MEAS-Q-004` | **The angle gates' geometry** is restricted to \|declination\| and \|elevation\| ≤ 40° so that the third-derivative coefficient is bounded (§6.2); near the pole the right-ascension row's conditioning degrades as `1/cos δ` and no gate is claimed there. Recommendation: accept; a campaign near the pole would state its own bound. |

---

## Changelog

| version | date | change |
|---|---|---|
| 1.0 | 2026-10-06 | first draft, written on the manager's rulings R1–R12 (`plan/subplan_L6/L6-4.md`); the finite-difference sizing frozen in §6.2 before any run |
| 1.0a | 2026-10-06 | **before the first run of the range gate:** the elevation labels of the frozen table corrected (52° and 15° with the target distances that give them, 40° for the angle row) with the first text kept, and the target's speed and its two directions added to §6.2. **After it:** the first run recorded beside the frozen text (§6.2) and **Amendment A1** (the sizing's two omitted first-principles terms: the orientation chain's noise floor, and `F` as a bound over the stencil for the whole modelled range), the gate run in two configurations, the rule-5 criteria restated for both, `MEAS-A-046`, `MEAS-P-22`, `MEAS-P-23`, and the angle gate's sizing obliged to take both terms before it first runs |
| 1.0b | 2026-10-06 | **item 6, the angle models.** Spec first: `MEAS-R-057` and `MEAS-F-026` (a direction at the pole of its coordinate system refuses), `MEAS-A-089` (the epochs the abstractions are asked at), the angle API of §5.3–§5.5, `MEAS-R-053`'s construction, the light deflection of §6.4 and `MEAS-P-24`. Corrections made when the tests were written, each with its first text kept: `MEAS-A-088` (the sign of the middle term of the light-time quadratic), `MEAS-A-090` and `-091` (the Earth's aberration is 20.15″ to 20.84″ over the year, 20.49″ being the mean-speed figure), `MEAS-A-092` (the zenith). **§6.2 (v), the angle gate, frozen before any angle-gate code was written** (its geometry rule, `ν_a`, the stencil bound `F_a(h) = F_pure(h) + F_extra/ρ³` with the extra terms scanned from the exact series of the model, the tolerance of the two configurations' agreement, the controls and their multiples), `MEAS-A-096g`, `MEAS-P-25` … `-29` |
| 1.0c | 2026-10-06 | **after the angle gate's one run and the manager's ruling (`7baf3ad`):** §6.2 (v) amended with its first text kept — the diurnal-Jacobian control missed for want of power at the rule-selected geometry (0.47 `ε` for the 10² asked) and is withdrawn from `MEAS-A-098`'s asserted list, logged beside the others; **its claim moves to `MEAS-A-098b`, §6.2 (vi), registered before its code exists**: a closed-form geometry with the observer moving along the line of sight, the sizing of (v)'s procedure evaluated at that geometry (`ν` = 2.9944 × 10⁻¹⁵, `B_pred` = 3.0028 × 10⁻¹³), each control's power at that geometry computed and written down before the run (4.5 × 10³ `ε` without `A′`, 9.1 × 10³ with it reversed; each asked 10³), `MEAS-P-30` … `-32` |
