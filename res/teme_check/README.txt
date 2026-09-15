Reference for the TEME->J2000 regression check in scripts/smoke_test.sh.

acs3.tle                        NASA ACS3 solar sail, CelesTrak, epoch
                                2026-09-14 17:27:31.8468 UTC
acs3_horizons_20260914.eci      the same object over the following 5 hours from
                                JPL Horizons, spacecraft -159588, geocentric
                                ICRF, requested on the UT timescale

SGP4 works in TEME and everything else here is J2000. Without the conversion the
two disagree by 34 km on average while their magnitudes agree to a metre, which
is a rotation and not a different orbit. With it they agree to 2.2 m.

Two traps, both of which produce a plausible wrong number rather than an error:

  Timescale. Request TIME_TYPE='UT'. Horizons defaults to TDB, which runs 69 s
  ahead of UTC - about 500 km along track here.

  Epoch rounding. The TLE epoch 26257.72745193 is 17:27:31.846752, and the
  sample times must be asked for at that precision. Rounding the request to
  17:27:31.85 shifts the reference 3.2 ms along track, which at 7.4 km/s is
  24 m - ten times the real disagreement, and it looks like a modelling error.

This is NOT an independent check of the orbit. Horizons' ACS3 ephemeris forward
of a TLE epoch IS that TLE: the kernel ends at epoch + 15.000 days, matching the
TLE epoch to the millisecond, and it tracks our SGP4 to 2-3 m flat for five days
with no growth. What the 2.2 m therefore measures is the frame conversion alone,
which is exactly what this check is for. 2.2 m at 7278 km is 0.064 arcsec, the
size of the difference between the IAU-76/80 precession-nutation this code
implements and the IAU-2006/2000A Horizons uses.

Before the TLE epoch the same kernel is independent of that TLE - propagating it
backwards diverges by 2.2 km after one day and 57 km after ten - so the history
comes from earlier element sets. It is a rolling TLE-fed product, accurate to
TLE class, not a mission-determined orbit. See REVIVAL.md.

Regenerate with scripts/horizons2eci.py.
