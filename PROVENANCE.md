# Provenance

An inventory of where the code and data in this tree came from, and under what
terms each third-party component is distributed.

This file records facts only. It does not assert ownership of anything, does
not grant or assume any licence for the first-party code, and is not legal
advice. A licence was briefly applied in September 2026 and then withdrawn in
favour of a bare copyright notice, because the question of who is entitled to
grant one is **open** - see the last section.

Compiled 2026-09-15. Final state.

One correction to the spacecraft property sets in `src/Resident_constants.cpp`,
recorded here because their contents were used as evidence about the population
this software was aimed at. Two entries were mislabelled: `Large_Sphere_HAMR`
was defined twice with the second unreachable and its own comment reading LAMR,
and `Medium_Sphere_LAMR` encoded the highest area-to-mass ratio in the file.
Both names now match their values. **The labels in that file were unreliable, so
any inference from it rests on the values, not the names** - which does not
change the conclusion, since the range is 0.016 to 6.3 m^2/kg, 1x to 305x GNSS,
whichever name sits on which entry.

## Third-party code, vendored under `external/`

| Component | Version | Origin | Terms | Licence text present? |
|---|---|---|---|---|
| Eigen | 3.4.0 | eigen.tuxfamily.org, copied from the system `/usr/include/eigen3` | MPL 2.0 (headers carry the notice) | in each header |
| CALCEPH | 4.0.5 (2025-06-03) | IMCCE / Observatoire de Paris | triple: CeCILL-C, CeCILL-B **or** CeCILL v2.1 - one must be chosen | `external/calceph/licence/` |
| NRLMSISE-00 | release 20041227 | model by Picone, Hedin & Drob (US Naval Research Laboratory); C translation by Dominik Brodowski | see note below | header comment only |
| FECsoft (`astrolib`) | `fecsoftc` | Joe Heafner, with modifications by Charles Gamble; companion code to *Fundamental Ephemeris Computations*, Paul J. Heafner, Willmann-Bell | **not stated in the tree** | none |
| SGP4 | - | companion code to *Fundamentals of Astrodynamics and Applications* (2007), David Vallado | **not stated in the tree** | none |
| png++ | 0.2.9 | Alex Shulgin | BSD-style, 3-clause | `external/png++-0.2.9/COPYING` |

Notes on the two that need attention:

- **CALCEPH is the sharpest question.** It is triple-licensed and a licensee
  must *choose* one. CeCILL-B is BSD-like (attribution), CeCILL-C is LGPL-like
  (changes to the library must be shared), CeCILL v2.1 is GPL-like (copyleft
  over the whole work). The Makefile links `libcalceph.a` **statically** into
  every utility, so under CeCILL-C or v2.1 that choice reaches the resulting
  binaries. No choice has been recorded anywhere in this tree. Whoever
  distributes a binary needs to make and record that choice. CALCEPH is
  optional - the build drops it automatically if `external/calceph` is absent -
  so the alternative is to ship without it.
- **FECsoft and SGP4 are both book companion code.** Both are widely
  redistributed and both are, in practice, treated as freely usable, but
  neither carries a licence statement in this tree and neither author's terms
  are recorded here. Someone should confirm the terms against the original
  distributions before this tree is passed on.
- **NRLMSISE-00**: the model itself is US government work; Brodowski's C
  translation is distributed from brodo.de and is generally treated as public
  domain, but the copy here carries no explicit licence file.

## Third-party data, under `res/`

| Data | Origin | Refresh |
|---|---|---|
| `eopc04` | IERS EOP 14 C04 + Bulletin A (`finals.all`) | `scripts/update_iersb.sh` |
| `SW-All.csv` | CelesTrak consolidated space weather | `scripts/update_spaceweather.sh` |
| `1980_2020` | JPL DE405, repacked by `scripts/jpl2fecsoft.py` | see REVIVAL.md |
| `gravity_fields/` | EGM96, EGM2008, JGM-3, GRIM5-C1/S1, GRACE GGM01C/GGM01S/GGM03C | static |
| `igrf11coeffs.txt`, `igrf12coeffs.txt` | IAGA International Geomagnetic Reference Field | IGRF-14 (2024) supersedes both |
| `tsi.dat` | total solar irradiance composite | static |
| `earth_radiation/` | NASA CERES | static |
| `Densities4854.txt`, `Heights4854.txt` | TIE-GCM model output, 12-15 June 2018 | not reproducible from this tree |
| `sp3/` | IGS final precise orbits, via BKG | `scripts/validate_sp3.sh` |

These are public scientific datasets, but several carry their own citation or
acknowledgement expectations (CERES and IGS in particular). None of those
requirements are recorded in this tree.

## First-party code

`src/`, `include/`, `scripts/` and the configuration and analysis files. By the
`@author` tags in the file headers:

| Author | Files |
|---|---|
| David Harrison | 101 |
| Santosh Bhattarai | 40 |
| Zhen Li | 4 |
| Stuart Grey | 2 |
| Alex Forsyth (with S. Bhattarai) | 2 |
| Marek Ziebart | 1 |
| John Keeling | NRLMSISE-00 drag class, rewritten here from his published sample |

The work is described in `README.md` as the UCL SGNL Orbit Prediction Software,
a 2014 rewrite of the pre-2010 UCL Orbit Dynamics Library, so it originates in
the Space Geodesy and Navigation Laboratory at University College London.

Changes made during the 2026 revival are described in `REVIVAL.md` and are not
separately attributed in file headers.

## What is missing

- **No licence or copyright statement for the first-party code existed anywhere
  in the tree** - not in `README.md`, not in any file header, and there was no
  `LICENSE` file. There is a `LICENSE` now and it grants nothing: a bare
  copyright notice that records the origin, states that nobody has established
  who may license the work, and declines to license it. A PolyForm
  Noncommercial grant was offered for one day in September 2026 and withdrawn,
  on the reasoning that an offer which may not be the offeror's to make is
  worse than no offer. **The question is unchanged by any of that** - settling
  it needs UCL, not a code change. See `NOTICE`.
- **No contribution or employment record** establishing whether rights sit with
  the named authors or with UCL.
- Two vendored components (FECsoft, SGP4) have no licence text in the tree.
- CALCEPH's three-way licence choice has not been made or recorded.
