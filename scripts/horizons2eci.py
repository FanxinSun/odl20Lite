#!/usr/bin/env python3
"""Convert a JPL Horizons vector table into the ECI text format the orbit fit
reads, so a mission-supplied trajectory can be used as a reference.

Horizons carries spacecraft ephemerides supplied by the missions themselves -
for ACS3 the header reads "{source: ACS3}", meaning an ACS3-specific kernel
rather than a propagated element set. That matters: a TLE contains no radiation
pressure at all, so it cannot be used to estimate one, whereas a real orbit
determination contains whatever the spacecraft actually did.

Fetch a table with, for a spacecraft at NAIF id -159588 (ACS3):

    https://ssd.jpl.nasa.gov/api/horizons.api?format=text
      &COMMAND='-159588'&EPHEM_TYPE=VECTORS&CENTER='500@399'
      &REF_PLANE='FRAME'&VEC_TABLE='2'&OUT_UNITS='KM-S'&CSV_FORMAT='YES'
      &TIME_TYPE='UT'
      &START_TIME='2026-09-10 00:00'&STOP_TIME='2026-09-12 00:00'&STEP_SIZE='15m'

**Request TIME_TYPE='UT'.** Horizons defaults vector tables to TDB, which in
2026 runs 69.184 s ahead of UTC. Feeding TDB timestamps to a tool expecting UTC
displaces the satellite by about 500 km along track - the fit still converges
and reports a plausible-looking residual, so nothing announces the mistake.
This script refuses a TDB table rather than convert it silently.

Usage:
    horizons2eci.py <horizons.txt> <out.eci> [sat-id]
"""

import re
import sys

MONTHS = {m: i + 1 for i, m in enumerate(
    "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec".split())}


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)

    src, dst = argv[1], argv[2]
    sat_id = int(argv[3]) if len(argv) > 3 else 1

    text = open(src, encoding="latin-1").read()

    # Refuse anything not on a UTC-like scale. The consequence of getting this
    # wrong is a ~500 km along-track offset that looks like a modelling error.
    head = text.split("$$SOE")[0]
    scale = None
    m = re.search(r"Start time\s*:\s*A\.D\.\s*\S+\s+\S+\s+(\w+)", head)
    if m:
        scale = m.group(1).upper()
    if scale != "UT":
        raise SystemExit(
            "Horizons table is on the %s timescale, not UT.\n"
            "Re-request it with TIME_TYPE='UT'. TDB runs ~69 s ahead of UTC,\n"
            "which is about 500 km along track for a low Earth orbit, and the\n"
            "fit will converge on it without complaint." % (scale or "unknown"))

    if "Reference frame : ICRF" not in head:
        print("warning: reference frame is not ICRF; check the frame matches "
              "the tool's ECI convention", file=sys.stderr)

    body = text.split("$$SOE")[1].split("$$EOE")[0]

    rows = []
    for line in body.splitlines():
        f = [c.strip() for c in line.split(",")]
        if len(f) < 8 or not f[0]:
            continue
        # f[1] is "A.D. 2026-Sep-10 00:00:00.0000"
        d = re.match(r"A\.D\.\s+(\d{4})-(\w{3})-(\d{2})\s+"
                     r"(\d{2}):(\d{2}):([\d.]+)", f[1])
        if not d:
            continue
        year, mon, day = int(d.group(1)), MONTHS[d.group(2)], int(d.group(3))
        hh, mm, ss = int(d.group(4)), int(d.group(5)), float(d.group(6))
        x, y, z, vx, vy, vz = (float(v) for v in f[2:8])
        rows.append("%3d %5d %3d %3d %3d %3d %10.6f %17.6f %17.6f %17.6f "
                    "%17.9f %17.9f %17.9f"
                    % (sat_id, year, mon, day, hh, mm, ss, x, y, z, vx, vy, vz))

    if not rows:
        raise SystemExit("no vector records found between $$SOE and $$EOE")

    with open(dst, "w") as fh:
        fh.write("\n".join(rows) + "\n")

    print("%d states -> %s" % (len(rows), dst))
    print("  first: %s" % rows[0][:60].strip())
    print("  last : %s" % rows[-1][:60].strip())


if __name__ == "__main__":
    main(sys.argv)
