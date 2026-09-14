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
