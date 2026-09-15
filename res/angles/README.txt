Optical angles for NASA's ACS3 solar sail (2024-077B, NORAD 59588), from the
SeeSat-L archives.

  acs3.angles   26 observations, 2024-09-03 to 2025-10-24, all from site 2675

Source: the SeeSat-L mailing list archives, public since November 1994 at

  http://satobs.org/seesat/<Mon>-<YYYY>/

Regenerate with scripts/seesat2angles.py, which downloads and caches the
monthly archives and parses the observation lines. Observer coordinates are in
res/obs_sites.txt, taken from the observers' own postings.

WHY THIS DATA AND NOT LASER RANGING. A laser range is a hundred times better
than any of this, and res/slr has some - but laser ranging only works on an
object built to be ranged, with a retroreflector on it. ACS3 has none, and
neither does anything in the population this software is aimed at. Angles are
what those objects produce, and this is what angles look like: one observer,
a handful of nights, two or three points per pass, tens of arcseconds.

THE FORMAT WAS MEASURED, NOT ASSUMED. satobs.org publishes a U.K. format
specification and these lines do not follow it exactly - the right ascension
sits two columns early and no position-format code is written. Reading decimal
minutes as seconds would be a 20% error in the right ascension rate, absorbed
silently into the orbit. Over 4780 observation lines in the archive: the third
pair of digits of the right ascension never exceeds 59, so that field is
seconds; the fifth digit of the declination is uniform over 0-9, so that field
is tenths of an arcminute. See scripts/seesat2angles.py.

WHAT THIS DATA CAN AND CANNOT DO. It is enough to fit an orbit through one pass
- fit_orbit_to_angles reaches 26 arcseconds on the pass of 2024-09-03 - and it
is not enough to determine one. See REVIVAL.md for the numbers.
