# Reviving UCL ODL / SGNL OPS on Linux

This tree is the "lite" build of the UCL SGNL Orbit Prediction Software (the
2017 OPS, descended from the pre-2010 UCL Orbit Dynamics Library). It last
built in February 2025 on an Apple Silicon Mac. These notes record what it took
to build and run it on x86-64 Linux with GCC 15, and what is still limited.

Build and check:

    make rebuild
    ./scripts/smoke_test.sh

Use `make rebuild`, not a bare `make`: an incremental build does not reliably
relink the utilities against a changed library object, so edits to `src/*.cpp`
can silently fail to reach `bin/`.

The utilities resolve their data as `../res/...`, so run them from `bin/`:

    cd bin
    ./sgnlOPS ../res/configOPS.txt ../output/orbit.txt

## What was changed to make it build

1. `external/Eigen` - was Eigen 3.3.3 (2017), which GCC 15 cannot parse:
   `Transpositions.h` calls `derived()` on a type that does not have it, and
   newer GCC diagnoses that in an uninstantiated template body. Replaced with
   Eigen 3.4.0. Only `Eigen/Dense` is used, so the swap is contained.
   The old tree is kept in `.attic/Eigen-3.3.3`.

2. `Makefile` - `-std=c++14` to `-std=gnu++14` (and the Intel flags likewise).
   `include/constants.h` writes pi as hex-float literals (`0x1.921fb...p+1L`).
   Hex floats only entered standard C++ in C++17; GCC 15 no longer accepts them
   as a C++14 extension, so it parsed `...p` as the number and reported
   "exponent has no digits". `gnu++14` keeps the C++14 semantics the code was
   written against and restores the extension. `-std=c++17` also works.

3. `external/fecsoft/astrolib.h` - added `#include <cstdint>`. `uint32_t` used
   to arrive transitively through other headers; it no longer does.

4. `src/Force_drag_tiegcm.cpp` and its header - bounds checking, see below.

The prebuilt Mach-O binaries and the stale object files that were in the tree
are in `.attic/` rather than deleted.

## The TIE-GCM drag crash

`analyses/qbfanxin/ops_config_template.txt` selects `drag = 2` (TIE-GCM) and
used to segfault. `get_density()` collects the eight surrounding grid corners
and `interpolate()` reads `v[0]`..`v[7]` unconditionally; when the requested
point falls outside the tables no corner is ever appended and the read runs off
the end of an empty vector. The code already tested for `V.size() < 8` but only
printed the vector and then called `interpolate()` anyway.

That path now returns zero density with a diagnostic, and the grid accesses are
bounds-checked. The simulation then stops through the model's own error
reporting, which correctly says the request is out of range.

It is out of range because the bundled tables are narrow. `res/Heights4854.txt`
and `res/Densities4854.txt` are 62208 rows by 72 columns: 288 fifteen-minute
steps, i.e. three days from MJD 58281 (12 June 2018), heights roughly 342-481 km
(the values are in cm), and the model's own guards accept only 360-413 km and
-58.76 to +56.24 degrees latitude. The config's TLE is NSIGHT1/QB50 at 724 km
and 98.35 degrees inclination on 1 January 2013, which is outside all three.
Nothing is wrong with the propagator: the same config runs end to end with
`drag = 1` (USSA76), or with `drag = 3` below.

## NRLMSISE-00 drag (drag = 3)

`drag = 3` selects NRLMSISE-00, which reads no density grid and so has none of
the limits above: it covers the ground to the exosphere at any latitude and any
epoch. The model is the reference C implementation in `external/nrlmsise00`
(Picone, Hedin and Drob; C translation by Dominik Brodowski), compiled straight
into the library by the Makefile.

It is driven by solar and geomagnetic indices instead of a grid, read from
`res/SW-All.csv` - CelesTrak's consolidated series, observed from 1957 and
predicted to 2041. Refresh it with `scripts/update_spaceweather.sh`. Three
columns are used: `F10.7_OBS` of the *previous* day, `F10.7_OBS_CENTER81`, and
`AP_AVG`. The observed rather than the adjusted flux is deliberate - the model
is calibrated against flux at the Earth's actual distance from the Sun. A day
the file does not cover falls back to F10.7 = 150 and Ap = 4, the quiet values
NRLMSISE-00's documentation specifies, and it says so once.

Two choices in `src/Force_drag_nrlmsise00.cpp` worth knowing:

- `gtd7d`, not `gtd7`. Only `gtd7d` puts the effective total mass density for
  drag in `d[5]`, including the anomalous oxygen that matters above 500 km.
- `flags.switches[0] = 1`, which makes the model return metres and kilograms,
  so `d[5]` is kg/m^3 with no unit conversion.

The published reference for this, `John-Keeling/Force_drag_NRLMSISE00`, was used
as a specification rather than as code. It is an extract from a master's project
and is not reusable as it stands: it shells out to a separate NRLMSISE-00
executable through `popen()` once per integration step, with absolute
`/Users/johnkeeling/...` paths for that binary and for its two index files, and
`chdir()` calls to match; it passes every quantity as a formatted string and
parses the result back by character offset. What it does establish, and what was
kept, is which inputs the model needs and where they come from.

Sanity checks, at 725 km on 1 January 2013 (F10.7 = 113.6 the previous day,
81-day average 116.5, Ap = 1):

- Density is 4.1e-14 to 2.7e-13 kg/m^3, against 1.9e-13 to 2.8e-13 for USSA76.
  A static 1976 standard atmosphere reading high at this altitude in quiet
  conditions is expected.
- Binned by local solar time, density peaks in the 12-15 h bin and is lowest in
  the 00-03 h bin, which is the diurnal bulge in the right place. USSA76 is
  spherically symmetric and varies only with altitude, so the comparison is a
  usable check that time and position reach the model at all.

Note that both checks need a numerical propagator. The `analyses/qbfanxin`
config ships with `propagator = 2` (SGP4), which is analytical, so no force
model is integrated and the density column holds one value for the whole run.
`scripts/smoke_test.sh` switches to `propagator = 1` for this reason.

## Data currency

`res/eopc04` shipped ending 4 June 2019, and the utilities refuse any epoch
past the end of the table, so this file alone decides how far ahead anything
can be propagated. `scripts/update_iersb.sh` had a dead IERS URL (404); it now
rebuilds the table from two IERS products via `scripts/build_eop.py`:

| product    | what it gives                                    |
|------------|--------------------------------------------------|
| EOP 14 C04 | the definitive solution, but only measured days  |
| finals.all | Bulletin A: measured to ~now, then ~1 yr ahead   |

C04 alone is not enough. It is only published for days already measured and
reprocessed, and currently ends **2026-01-05** - eight months behind today, so
with C04 only the software cannot propagate to today's date at all. The merge
takes C04 where it exists (better solution, and it covers 1962-1972, which
finals does not) and finals for everything after.

The table now runs 1962-01-01 to 2027-09-18, and the last epoch that actually
propagates is **2027-09-16** - the reader needs a few days of margin past the
requested epoch. Re-running the script pulls newly measured days in over the
predicted ones.

Days inside the prediction span are for planning, not precise work: predicted
UT1-UTC drifts by roughly tens of milliseconds a year out, and 1 ms of UT1 is
about 0.46 m of position at the Earth's surface.

Keep `res/eopc04` as the **IAU1980 (dPsi, dEps)** one-file series.
`Frame_transform.cpp` skips exactly 14 header lines and then reads
`year month day MJD x y UT1-UTC LOD dPsi dEps`. The IAU2000A (dX, dY) series
and the newer EOP 20 C04 series have different columns and will not work.

`res/igrf12coeffs.txt` is the newest geomagnetic model present. IGRF-14 was
released in November 2024 and runs to 2030; the coefficient file has the same
layout, so adding it is mostly a matter of extending the model selection in the
config and `Force_lorentz`/magnetic setup.

## The planetary ephemeris

`res/1980_2020` shipped as a JPL DE405 subset covering 1 December 1979 to
**16 January 2020**. Frame transforms need it, not just third-body gravity, so
*any* epoch after January 2020 used to fail with "pleph: requested date not
covered by ephemeris file" even with `third_body = 0`. That, rather than the
EOP table, was what stopped the software being used for present-day work.

Dropping in a stock JPL binary such as `lnxp1600p2200.405` does not work - it
overflows a buffer - because `external/fecsoft/astrolib.cpp` does not read the
format JPL publishes. FECsoft uses its own container:

| field      | JPL binary          | FECsoft container         |
|------------|---------------------|---------------------------|
| titles     | 3 x 84 chars        | 3 x 65 chars              |
| ncon       | int32               | int16                     |
| cnam       | always 400 x 6      | ncon x 6                  |
| ipt        | int32, 12 triplets  | int16, transposed [3][12] |
| numde, lpt | int32               | int16                     |
| header     | two KSIZE*4 records | 317 + 14 * ncon bytes     |

The Chebyshev coefficient records after the header are byte-identical in both,
so only the header has to be repacked. `scripts/jpl2fecsoft.py` does that and
trims to a requested year range:

    curl -O https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de405/lnxp1600p2200.405
    scripts/jpl2fecsoft.py lnxp1600p2200.405 res/1980_2020 1979 2060

`res/1980_2020` now holds DE405 for **1978-12-14 to 2060-01-30** (7.1 MB), a
strict superset of what shipped. The file name is kept because
`external/fecsoft/astrolib.h` hard-codes it; it no longer describes the range.
The original is in `.attic/1980_2020.de405subset.orig`.

Two checks worth keeping in mind. The converter was verified against DE405 and
DE421, which have different constant counts (156 and 228) and therefore
different header sizes, both of which the reader handles. And for a date the
old file also covered, the output is bit-for-bit identical to before, so
widening the range changed no results - `scripts/smoke_test.sh` asserts this.

DE405 is kept as the default because that is what the tree shipped with and
what the GM constants in `Ephemeris.cpp` are quoted against. A newer DE can be
substituted with the same command.

The usable range is now bounded by `res/eopc04`, not the ephemeris: a date in
2055 stops with "Date requested too recent for EOPC04 file", which is correct,
since measured Earth orientation does not exist for future dates.

### Reading other ephemeris formats directly

The converter widens the range, but FECsoft still understands exactly one
container. [CALCEPH](https://www.imcce.fr/inpop/calceph) (IMCCE, CeCILL-C/B)
reads JPL DE binaries, INPOP files and SPICE SPK/BSP kernels behind one API,
and it is now wired in behind `Fecsoft::pleph`, so nothing in `src/` changed.

Build it, and the Makefile picks it up on its own:

    cmake -S <calceph-source> -B build \
          -DCMAKE_INSTALL_PREFIX=$PWD/external/calceph \
          -DENABLE_FORTRAN=OFF -DBUILD_SHARED_LIBS=OFF
    cmake --build build -j && cmake --install build
    make rebuild

`CALCEPHLIB = $(wildcard external/calceph/lib/libcalceph.a)` decides whether
`-DSGNL_USE_CALCEPH` is added, so a tree without it builds exactly as before.

Which reader is used is decided per file, not by configuration.
`looks_like_fecsoft()` re-derives the header arithmetic described above and
checks that the remaining bytes divide exactly by the record size; only a real
FECsoft container passes, and it is then read by the original code. Everything
else goes to CALCEPH. `res/1980_2020` therefore behaves exactly as it always
did, which `scripts/smoke_test.sh` checks.

To use another file, set `SGNL_EPHEMERIS`:

    cd bin
    SGNL_EPHEMERIS=/path/to/de440s.bsp ./sgnlOPS ../res/configOPS.txt out.txt

Two details were needed to make this correct, both worth knowing if the code is
touched again:

- **Units are km and km/sec, not AU.** The comment above `pleph()` claims AU
  and AU/day; it is inherited from stock FECsoft and is wrong for this tree.
  `state()` says km and km/sec further down, and `src/Ephemeris.cpp` uses the
  result directly against GM values in km^3/s^2. Asking CALCEPH for AU produced
  positions around 1e17 km once third-body gravity was switched on. It is easy
  to miss, because a frame transform alone barely notices: the ephemeris enters
  `ECEF2ECI` only through the TDB-TT relativistic term, so that test still
  agreed to 20 picometres while the units were wrong.
- **Bodies are requested by NAIF id.** A SPICE kernel is segmented that way -
  Earth is 399 relative to the Earth-Moon barycentre 3, and there is no segment
  for "3 relative to 12". `pleph` numbering is translated in a small table;
  as in the DE files, 1..9 are barycentres and 3 is the Earth itself.

Verified against four sources at once - the FECsoft container, stock JPL DE405
and DE421 binaries (the format that used to overflow a buffer), and the SPICE
kernel `de440s.bsp` - running `configOPS.txt` with `third_body = 1` so that the
Sun, Moon and eight planets all go through the translation. All four agree to
within the printed precision. `scripts/smoke_test.sh` will check any file you
point it at:

    SGNL_TEST_EPHEMERIS=/path/to/de440s.bsp ./scripts/smoke_test.sh

## Measured accuracy

`scripts/validate_sp3.sh` fits an orbit to an IGS final precise ephemeris and
reports the residuals. IGS finals are good to about 2.5 cm, so for this purpose
they are truth, and the residual RMS is the accuracy figure for these force
models over the arc.

    ./scripts/validate_sp3.sh                        # GPS G01, 2023-01-22
    ./scripts/validate_sp3.sh 2246 20230220000 G05   # another satellite

Across 57 arcs on 2023-01-22, one untuned configuration, no per-satellite
tuning, seven parameters (six state elements plus a radiation-pressure scale):

| constellation | n | converged | best | **median** | worst |
|---|---|---|---|---|---|
| GPS | 31 | 31/31 | 0.065 m | **0.140 m** | 0.490 m |
| Galileo | 26 | 26/26 | 0.153 m | **0.329 m** | 0.551 m |

**Quote the medians, not the best case.** The 0.065 m figure is real but it is
the best satellite in the better constellation, and quoting it alone
misrepresents the tree.

The six-parameter state-only fit is still available with `--six`. It needs the
spacecraft area hand-tuned per satellite to get anywhere - 0.343 m for G01 with
the area tuned, against 3.200 m for G05 with the same tuned value.

### What the estimated parameter is

`A*C_R/m`, an effective area-to-mass ratio with the reflectivity coefficient
inside it. A cannonball fit cannot separate the two. Sorting the 31 GPS
satellites by it reproduces their five IGS ANTEX hardware blocks with zero
misclassifications, and multiplying by published on-orbit masses the fit never
saw puts the three Block IIR variants - which share a bus - within 4% of a
common effective area. Details in the Phase 4 report under
`~/.claude/handover/`.

### Where it stops working

`scripts/degrade_eci.py` shortens, thins and adds noise to a reference arc.
Degrading one axis at a time is misleading: six observations over 22 hours give
0.35%, and a one-hour densely sampled arc holds 2.3%. Neither sparsity nor arc
length binds. **Noise binds, and far harder on a short arc:**

| arc, hourly sampling | 1 m | 3 m | 10 m |
|---|---|---|---|
| 22 h (n=22) | 1.3% | 4.2% | 14% |
| 6 h (n=7) | 23% | - | - |

against a 2.75% threshold for telling adjacent blocks apart.

The scatter of the estimate is proportional to tracking noise, independent of
the object's signal strength, and steep in arc length. Because the absolute
error does not depend on signal, the *fractional* error goes as 1/signal - which
inverts into a minimum object for a given tracking quality. At 10 m noise:

| arc | minimum A*C_R/m |
|---|---|
| 22 h | 0.033 |
| 6 h | 0.95 |

GNSS is 0.021, so ordinary satellites fail. The HAMR property sets in
`Resident_constants.cpp` are 0.16 to 6.3, so they pass by 5x to 190x. That
result is synthetic - the truth arcs were generated with the same force model
that then fitted them - and needs a real high-area-to-mass object to confirm.

### Uncertainty

The fit reports a formal 1-sigma on the estimate. **It is optimistic by 10 to
20x**: 0.01-0.035% formal against 0.29% empirical scatter within Block IIF and
0-0.5% for one satellite across two dates. The weight matrix is identity on
positions, asserting independent white noise, while the residuals are systematic
and correlated along the arc. Use the empirical scatter, not the formal sigma,
for any decision.

### Where the error comes from

Turning single force models off, on the G01 arc:

| change                        | position RMS |
|-------------------------------|--------------|
| full model                    | 0.34 m       |
| spacecraft area left at 10 m2 | 22.14 m      |
| solar radiation pressure off  | 31.51 m      |
| third-body gravity off        | 356.80 m     |
| relativistic correction off   | 22.14 m (\*) |
| gravity degree 4 instead of 12| 22.15 m (\*) |
| gravity degree 20             | 22.14 m (\*) |

(\*) measured before the area was corrected, so read these against 22.14 m.

Two things follow. Luni-solar attraction dominates at this altitude and is
modelled correctly - removing it costs 357 m. And the gravity field is
effectively a point mass up there: degree 4, 12 and 20 are indistinguishable,
so there is nothing to gain from a higher expansion for MEO work.

Everything else is swamped by solar radiation pressure. A sweep of the
spacecraft area finds a clear minimum at 34 m^2 against a mass of 1630 kg, an
area-to-mass ratio of 0.0209 m^2/kg - which is close to the published figure for
GPS. That the fit recovers the right physical number independently is a useful
check on the radiation pressure model.

The obvious next step is to estimate a radiation-pressure scale factor as a
seventh parameter in the fit rather than hard-coding an area per satellite,
which is what analysis centres do and what would bring G05 in line with G01.

## Things that are absent rather than broken

`SP3_to_eci` and `fit_orbit_to_sp3_v3` build and run but need IGS SP3 precise
ephemeris files, which are not in this tree; they report the missing input and
exit. The SRP/TRR integration the README describes (SRP_TRR_5_0_7) is likewise
not here.

## Upstream

The UCL SGNL public GitHub organisation does not publish OPS itself. The
closest public artefact is `John-Keeling/Force_drag_NRLMSISE00`, a 2021 drag
class written against this same OPS that calls NRLMSISE-00 instead of the
TIE-GCM tables. NRLMSISE-00 has no three-day, 481 km window, so that is the
natural replacement for the drag model whose limits are described above. The
author notes it does not compile outside the rest of the OPS sources.


---

# If you are picking this up cold

**What it is.** The UCL SGNL Orbit Prediction Software, a numerical orbit
propagator with a high-fidelity force model set, written 2014-2020 and revived
in September 2026. It builds and runs on current Linux with GCC 15. Start with
`make rebuild` then `./scripts/smoke_test.sh`.

**What it has been shown to do.** Fitted against IGS final precise orbits it
reproduces GPS satellites to a **median 0.14 m** over 22-hour arcs, 0.33 m for
Galileo, across 57 arcs with no per-satellite tuning. Adding a classical ECOM
empirical set brings the median to **0.036 m** and the worst case to 0.044 m.
Reproduce it with `./scripts/validate_sp3.sh`.

It also estimates an effective `A*C_R/m` from dynamics alone, and that estimate
sorts the GPS constellation into its five hardware blocks with no
misclassifications - a result that survives ECOM, reproduces across dates to
0.5%, and agrees with published on-orbit masses the fit never saw.

**What it cannot do.**

- It has no observation models. It fits to a precise ephemeris, not to radar,
  optical or ranging measurements.
- The `A*C_R/m` recovery does not survive realistic tracking of ordinary
  objects. At 10 m position noise an object needs `A*C_R/m` above ~0.033 m^2/kg
  on a 22-hour arc; GNSS is 0.021 and fails. High area-to-mass objects (0.16 to
  6.3 in the property sets here) pass comfortably - but that has only been
  shown synthetically.
- The formal uncertainty it reports is optimistic by 10-20x. Use the empirical
  scatter.
- Precision improves as 1/signal only down to a floor set by how well shape and
  attitude are known. An error that scales with radiation pressure - wrong
  reflectivity, wrong shape, unknown attitude - keeps a constant fractional size
  however strong the signal. Measured example: a +/-30% slow tumble biases the
  estimate 3.28% at every signal strength tested.
- The ray-traced SRP grid files the code expects do not exist anywhere, and
  neither does the ray-tracer that made them. `Force_rp_gridfile` is intact but
  has nothing to read.
- **A box-wing radiation-pressure model is present** (`rp_model = 3`,
  `Force_rp_box_wing.cpp`) and needs **no** grid files - only per-face areas and
  optical properties, which `Resident_constants.cpp` carries for the catalogued
  spacecraft. It is the natural instrument for testing sensitivity to shape,
  which the cannonball cannot do: in a cannonball, reflectivity and area enter
  as one scalar that the estimated scale absorbs exactly.

  It was not used anywhere in this work, and there are two things to get past
  before it will do anything useful.

  **It is silently disabled unless `srp` or `erp` is non-zero.**
  `Configuration.cpp` zeroes `rp_model` when both fluxes are off, and
  symmetrically zeroes the fluxes when `rp_model` is not 1, 2 or 3. Both rules
  are correct - a radiation pressure model with no flux has nothing to act on -
  but they used to apply silently, so setting `rp_model = 3` in a config with
  `srp = 0` gave a clean run with plausible output and no radiation pressure at
  all. Both overrides now say so.

  **Both shipped cubesat analyses are affected.** `analyses/qbfanxin` and
  `analyses/qb50` each set `srp = 1`, `erp = 1` and `rp_model = 0`, so their
  radiation-pressure settings have never had any effect. That is not a small
  omission at this altitude. Measured on the qbfanxin orbit - QB50 at 725 km,
  NRLMSISE-00 densities, six hours:

  | force switched on vs off | positional effect |
  |---|---|
  | drag | 9.71 m |
  | solar radiation pressure | **11.73 m** |

  Radiation pressure moves this orbit slightly *more* than drag does. The object
  is a 1.33 kg cubesat with an area-to-mass ratio of 0.0236 m^2/kg, which is
  high enough that radiation pressure is not a correction to drag but a peer of
  it. Any absolute trajectory from those two configs is missing a term the same
  size as the one it was studying.

  What this does not affect: the density work in this tree was about
  plausibility, out-of-range handling and diurnal variation, none of which
  depends on the trajectory being right, and `scripts/smoke_test.sh` checks
  density values rather than positions. Comparisons *between* drag models from
  those configs also stand, since radiation pressure was absent from all of them
  equally.

  **It needs real per-face properties.** With the default `testRSO` - which is
  not a named branch in `Resident_constants.cpp` at all, so it falls through to
  defaults - a 22-hour fit gives a 114 m residual at the first iteration and
  then diverges. Before September 2026 that surfaced as a segmentation fault
  deep in the ephemeris reader; it now reports what actually went wrong. Use
  `testbox` or one of the catalogued GNSS vehicles, which do set `face_area`
  and the per-face optical properties.
- There is no licence or copyright statement for the first-party code. See
  `PROVENANCE.md`; that question needs UCL, not a code change.

**Tested on a real high area-to-mass object, against a reference that turned out
to be weaker than advertised.** NASA's ACS3 solar sail (NORAD 59588) is an 80 m^2
sail on a 16 kg spacecraft - a geometric 5.00 m^2/kg, about 240x GNSS - and JPL
Horizons publishes its trajectory with no credentials.
`scripts/horizons2eci.py` converts a Horizons vector table into the format the
fit reads.

| arc | residual | recovered A*C_R/m |
|---|---|---|
| 48 h | 2458 m | 5.47 +/- 0.18 |
| 24 h | 683 m | 4.54 +/- 0.14 |
| 12 h | 449 m | 5.56 +/- 0.26 |
| 6 h | 292 m | 8.88 +/- 0.48 |
| 48 h, three weeks earlier | 1895 m | 6.35 +/- 0.14 |
| solar radiation pressure removed | diverges to NaN | - |

The estimator **recovers a real HAMR object's effective A\*C_R/m**, 4.5 to 5.6
over 12-48 hour arcs against a published geometric 5.00, and radiation pressure
is load-bearing: remove it and the fit diverges rather than degrades. The 6-hour
arc returns 8.88, outside the others, which is the observability limit the
degradation study put at that arc length showing up on real data.

**What the reference actually is.** Horizons labels this trajectory
`{source: ACS3}`, which reads like a mission-supplied kernel. It is not one.
Measured, not assumed:

- The kernel stops at **2026-09-29 17:27:31.8468 UT**, which is the epoch of the
  current TLE plus 15.000 days, matching the TLE epoch to the millisecond.
- Forward of that epoch it **is** that TLE: our SGP4 tracks it to 2-3 m, flat,
  for five days with no growth. An independently determined orbit for a decaying
  solar sail could not do that.
- Before that epoch it is **not** that TLE: propagating the same element set
  backwards diverges by 2.2 km after one day, 18 km after four and 57 km after
  ten. So the history comes from earlier element sets, not from this one.

It is a rolling, TLE-fed product - history from whatever elements were current
at the time, future from the latest set. Its accuracy is TLE class, kilometres,
not the centimetres of a precise orbit determination.

**That reclassifies the ACS3 result rather than voiding it.** Successive TLEs are
fitted to real tracking, so the trajectory does carry the sail's real dynamics;
it carries them at kilometre accuracy. The signal is large enough for that to
work: propagating ACS3 with and without radiation pressure, the two separate by
**23 km over 48 hours**, against a reference good to 1-3 km. A signal-to-noise
of roughly ten to twenty predicts a per-arc precision of five to ten percent -
and 5.47 +/- 0.18 with a 16% spread between arcs three weeks apart is exactly
that. The number is a real measurement of a real sail, at TLE accuracy.

Two earlier readings of this table were wrong and are withdrawn. The 16% spread
between arcs was attributed to ACS3's active attitude steering; reference noise
of the measured size accounts for it without any appeal to the spacecraft. The
eightfold fall in residual as the arc shortens was read as the signature of a
slowly varying effective area; a reference stitched from successive element sets
predicts the same fall, so the observation does not distinguish the two - and the
numbers favour the dull explanation, since 2458 m over 48 h against 292 m over
6 h is a factor of 8.4 for a factor of 8 in arc length, which is reference error
growing linearly in time.

**SGP4 output is TEME, and it is converted now.** `Prop_sgp4` used to feed SGP4's
state straight through as ECI. Measured against the Horizons ephemeris at the
TLE's own epoch, the position vectors differed by 34 km on average and 48 km at
worst while their magnitudes agreed to a metre - a rotation, not a different
orbit, growing with time since J2000. `Frame_transform` now supplies
`teme_to_eci_matrix()` as `PN^T * Rz(Eqeq)` with `Eqeq = GAST - GMST`, reusing
the precession and nutation it already computes rather than carrying a second
implementation, and `Resident_space_object::update_from_teme()` applies it -
including to the initial state, which does not pass through `step()` and was the
last 46 km of the error. After it, agreement is **2.2 m mean, 3.6 m max**.

That residual is itself explicable: 2.2 m at 7278 km is 0.064 arcsec, the size
of the difference between the IAU-76/80 precession-nutation implemented here and
the IAU-2006/2000A Horizons uses. `scripts/smoke_test.sh` asserts it stays under
20 m against the reference in `res/teme_check/`.

Two cautions from the measurement itself. The first comparison read the 34 km as
*TLE error*; it was almost entirely frame, and the real disagreement is metres.
And the conversion appeared to leave 26 m until the reference was regenerated at
the TLE's exact epoch: 26257.72745193 is 17:27:31.846752, and asking Horizons for
17:27:31.85 displaces it 3.2 ms, which at 7.4 km/s is 24 m. Ten milliseconds of
rounding in a timestamp is a ten-fold error here, and nothing announces it.

**Gap B closed: a real high area-to-mass object, measured rather than
predicted.** LightSail-2 (NORAD 44420) is a 32 m^2 solar sail on a 5.035 kg
CubeSat - 6.36 m^2/kg, about 300x a GNSS satellite - and it is **the only high
area-to-mass object in the entire public satellite laser ranging archive**.
Everything else the ILRS tracks is a dense sphere or a large spacecraft, because
geodetic targets are deliberately built to make non-gravitational forces small,
which is the opposite of what is wanted here. It carried no GPS receiver, so
laser ranging was its primary orbit determination.

That is the thing every earlier reference in this tree lacked. A laser normal
point is a measured round trip time, good to a centimetre or two, with no orbit
model behind it - where an IGS ephemeris is somebody's orbit determination and a
Horizons table for a small satellite is, as above, a two-line element in
disguise. `src/main_fit_orbit_to_slr.cpp` fits to the measurements themselves:
light time on both legs, station motion between transmit and receive,
tropospheric refraction by Marini-Murray, station coordinates from SLRF2020.

The record is thin. 25 normal points, 8 passes, 2 August to 20 September
2019, every one of them from Yarragadee. The densest cluster is 15 points over
the 72 hours from 31 August, and that is the arc fitted here.

| effective C_D | range residual RMS | recovered A*C_R/m |
|---|---|---|
| 0 (drag off) | 244 m | 1.53 +/- 0.74 |
| 0.20 | 155 m | 2.31 +/- 0.47 |
| 0.45 | 67 m | 3.22 +/- 0.21 |
| **0.52** | **57 m** | **3.36 +/- 0.18** |
| 0.65 | 85 m | 3.64 +/- 0.26 |
| 1.00 | 234 m | 4.34 +/- 0.73 |
| 2.20 (textbook) | 755 m | 6.49 +/- 2.38 |

**The fit reaches 57 m against measurements good to centimetres**, with a mean
bias of -1.8 m and no structure left inside a pass. Against a starting residual
of 1035 km, that is a real orbit determination of a real solar sail from real
measurements, and it is the first number in this tree about a high area-to-mass
object that was not generated by the same model that then fitted it.

**The recovered effective A\*C_R/m is 3.36 +/- 0.18 m^2/kg** against a geometric
6.36. The sail is therefore contributing 53% of its area, which is what a sail
that is being steered should do: LightSail-2 turned face-on for part of each
orbit and edge-on for the rest, and a cannonball fitted through that recovers
something near the duty cycle. The drag side agrees - an effective C_D of 0.52
on 32 m^2 is 16.8 m^2, or about 7.6 m^2 at a physical 2.2, a quarter of the sail.
Two independent numbers both saying the sail spends most of its time side-on.

**And the degeneracy is now measured rather than asserted.** Phase 6 said drag
and radiation pressure are near-degenerate for a LEO high area-to-mass object
because both scale as area over mass. They are: the recovered A\*C_R/m runs from
1.5 to 6.5 across the C_D column above, a factor of four. What breaks the tie is
that the residual does not - it has a sharp minimum, 57 m against 244 m and
755 m at the ends, so the data does separate them, but only because the arc is
long enough for the two to point in different directions. Take the first 48
hours instead of 72 and the answer moves to 3.59; that is the observability
limit showing up again.

Fitting is where it stops being comfortable. `--six` and a fixed scale needs a
prior on the state to work at all, and the prior has to be anisotropic - one
kilometre radially and across track, fifty along it - because one station
ranging one satellite sees nearly the same direction every pass. The scaled
condition number is still 8e8. Read the sigma above as conditional on C_D and on
that prior, not as an absolute.

**What the data cost to use, and three defects it exposed.** Real measurements
broke things that three years of synthetic tests had not:

- **Force model errors were never cleared.** `state->errors` accumulated for the
  life of the object and nothing emptied it, so the first trial orbit that
  dipped below 100 km made `Force_drag` file a complaint, and every propagation
  afterwards halted on that stale entry - including the good ones. The solver
  then found every direction impassable and stopped where it started, which is
  indistinguishable from convergence. `Resident_space_object::clear_errors()`.
- **The normal equations were never scaled.** A range moves by one metre per
  metre of initial position, a quarter of a million metres per metre-per-second
  of initial velocity, and a billion metres per unit of radiation pressure scale
  over three days. Those columns span twelve orders of magnitude and the normal
  matrix twenty-four, so its double precision inverse is noise - presenting as a
  condition number of 1e15 and a solver that cannot find a step, exactly as if
  the parameters were unobservable. They are observable; the arithmetic was not.
- **`effective A*C_R/m` was not A\*C_R/m.** Both fits reported the estimated
  scale times area over mass, while `Force_rp_analytic` builds its coefficient
  from (9 + 4*nu*(1-mu))/9 with nu and mu defaulting to 0.65 and 0.5. Every such
  figure was understated by C_R = 1.1444. The GNSS block separation is a ratio
  and is unaffected; the absolute values change, so G01 reads 0.02362 rather
  than 0.02064 m^2/kg.

Three more of the same kind this tree keeps producing: **not an error, a
plausible wrong number** - or, in the first case, an error raised once and then
answered forever, which is worse, because it reads as a fit that has settled.

**What the prediction was doing, as a free result.** The ILRS distributes the
predictions the stations point with, and comparing them to the measurements they
were made for says how predictable one of these objects is. For LightSail-2 the
error is almost entirely along track - as it must be, since what is mismodelled
is drag and radiation pressure and both act along the velocity - and it grows
from 4.3 s one day out to 15.0 s at four, about 3.5 s or 26 km a day. Take a
per-day time bias out and what remains is 94 m to 341 m. That is why laser
stations apply time biases, and it is a clean measurement of how fast a 6 m^2/kg
object becomes unpredictable.

**What is still untested.** The debris case proper: passive, tumbling, no
onboard receiver, no cooperative target, tracked sparsely by angles alone, with
shape and attitude unknown. LightSail-2 closes the *measurement* half of that -
real ranges, real high area-to-mass, no element set anywhere in the chain - and
`fit_orbit_to_slr` closes the machinery half, since it fits to observations
rather than to states. What it does not close is the uncooperative half: this
object carried a retroreflector, and the objects that would justify the work do
not. Angles-only observations need a different measurement model and, more
importantly, public astrometry of a high area-to-mass object to point it at.

**Where the detail is.** `~/.claude/handover/2026-09-15-odl-business-value-phase*.REPORT.md`,
seven phases, each leading with its failures. `analysis/` holds the sweep
outputs the tables above are computed from.
