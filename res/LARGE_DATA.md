# Bulk data kept out of version control

`.gitignore` excludes some large files from git. They are still needed at run
time; this records what they are, how big, and where they have to sit.

The decision was to exclude them rather than use git-lfs, so a clone will not
carry them and they must be restored separately.

## Where they actually live

On this machine the bulk data has been **relocated off the Linux filesystem**
to `/mnt/d/odl20lite_data/`, with symlinks left in the tree so every hard-coded
`../res/...` path still resolves:

    res/Densities*.txt, res/Heights*.txt, res/Skimed*.txt -> /mnt/d/odl20lite_data/res/
    analyses/qbfanxin, analyses/qb50                      -> /mnt/d/odl20lite_data/analyses/

That took the working tree from 1.3 GB to 681 MB. The symlinks are specific to
this machine and are not committed. On a fresh clone either restore the files
into `res/` and `analyses/` directly, or recreate equivalent symlinks.

## TIE-GCM atmosphere tables — ~565 MB, not reproducible from this tree

| file | size | used by |
|---|---|---|
| `res/Densities4854.txt`, `res/Heights4854.txt` | 47 MB each | `Force_drag_tiegcm` (`drag = 2`) |
| `res/Densities.txt`, `res/Heights.txt` | 47 MB each | earlier variants, not referenced by current code |
| `res/SkimedDensities{,2,3,4}.txt`, `res/SkimedHeights{,2,3,4}.txt` | 47 MB each | earlier variants, not referenced by current code |

Only the `4854` pair is read by the current code. Each is 62208 rows by 72
columns: 288 fifteen-minute steps (three days from MJD 58281, 12 June 2018) by
24 latitude bands by 9 altitude levels, with heights in centimetres.

**These are output from a TIE-GCM model run and cannot be regenerated from
anything in this tree.** If they are lost, `drag = 2` cannot be used. That is
less serious than it sounds: the tables only cover three days in June 2018,
roughly 342-481 km altitude and +/-58 degrees latitude, and `drag = 3`
(NRLMSISE-00) has none of those limits and needs no tables. See REVIVAL.md.

## What is deliberately still in version control

These are large but either small enough to carry or essential to a working
tree:

| file | size | why kept |
|---|---|---|
| `res/1980_2020` | 7.2 MB | JPL DE405 ephemeris; nothing runs without it |
| `res/gravity_fields/EGM2008.txt` | 6.3 MB | default gravity model |
| `res/gravity_fields/EGM96.txt` | 5.2 MB | |
| `res/gravity_fields/GGM03C.GEO` | 5.1 MB | used by the shipped configs |
| `res/eopc04` | 3.1 MB | Earth orientation; nothing runs without it |
| `res/SW-All.csv` | 2.8 MB | space weather for NRLMSISE-00 |

## Regenerable, excluded

- `res/sp3/` — IGS precise orbits, fetched by `scripts/validate_sp3.sh`.
- `external/calceph/{bin,lib,include,share}` — built from upstream source; the
  Makefile detects its absence and builds without it.

Refresh scripts for the time-varying data are `scripts/update_iersb.sh` and
`scripts/update_spaceweather.sh`.
