#!/usr/bin/env python3
"""Convert ILRS normal points in CRD format into the flat observation file
fit_orbit_to_slr reads.

Satellite laser ranging measures the round trip time of a laser pulse to a
retroreflector, so its accuracy is a property of the clock rather than of any
orbit model: a normal point is good to a centimetre or two. That makes it the
only reference in this tree that is independent of the two-line element
pipeline, which matters for objects whose only other trajectory comes from it.

Normal points are published openly by the EUROLAS Data Centre, no credentials:

    https://edc.dgfi.tum.de/pub/slr/data/npt_crd/<target>/<year>/

Usage:
    crd2obs.py <file.npt> [more.npt ...] <out.slrobs>

The output has one line per normal point:

    station  year mon day hh mm ss.ssssss  range_km  P_mbar T_K RH_pct  lambda_um  rms_ps

`range_km` is c*TOF/2, the usual one-way range observable, and the epoch is the
GROUND TRANSMIT time in UTC. CRD lets a file timestamp its normal points at the
transmit, bounce or receive event; this script normalises all three to transmit
and refuses anything it does not recognise, because the three differ by up to a
round trip - 10 ms, or 75 km of light travel - and nothing downstream could
tell which one it had been given.

Two corrections the CRD header says are NOT in the data and are left for the
range model: tropospheric refraction and the offset between the retroreflector
and the centre of mass. The script checks the header flags and warns if a file
turns up with them already applied, since applying them twice is silent.
"""

import sys

C_KM_S = 299792.458


def parse_file(path, rows, warn):
    station = None
    lam_um = 0.532
    trop_applied = com_applied = 0
    start = None
    for raw in open(path, encoding="latin-1"):
        f = raw.split()
        if not f:
            continue
        key = f[0].lower()
        if key == "h2":
            station = f[2]
        elif key == "h4":
            # h4 <type> <start y m d h m s> <end y m d h m s> <release>
            # <troposphere applied> <centre of mass applied> ...
            start = (int(f[2]), int(f[3]), int(f[4]),
                     int(f[5]) * 3600 + int(f[6]) * 60 + int(f[7]))
            trop_applied = int(f[15])
            com_applied = int(f[16])
            if trop_applied and "trop" not in warn:
                warn.add("trop")
                print("warning: %s has troposphere refraction already applied; "
                      "the range model would apply it twice" % path,
                      file=sys.stderr)
            if com_applied and "com" not in warn:
                warn.add("com")
                print("warning: %s has the centre of mass correction already "
                      "applied" % path, file=sys.stderr)
        elif key == "c0":
            lam_um = float(f[2]) / 1000.0
        elif key == "20":
            rows.setdefault("met", []).append(
                (start[:3], float(f[1]), float(f[2]), float(f[3]), float(f[4])))
        elif key == "11":
            if start is None or station is None:
                raise SystemExit("%s: a normal point before its h2/h4 header"
                                 % path)
            sod = float(f[1])
            tof = float(f[2])
            event = int(f[4])
            # Normalise to ground transmit time.
            if event == 2:
                pass
            elif event == 1:
                sod -= tof / 2.0
            elif event == 0:
                sod -= tof
            else:
                raise SystemExit(
                    "%s: epoch event %d is not a two-way ground/bounce/receive "
                    "time; this script will not guess" % (path, event))
            rows.setdefault("np", []).append(
                (station, start[:3], start[3], sod, tof, lam_um,
                 float(f[7]) if len(f) > 7 else -1.0))


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)

    rows = {}
    warn = set()
    for path in argv[1:-1]:
        parse_file(path, rows, warn)

    met = rows.get("met", [])
    out = []
    seen = set()
    for station, ymd, start_sod, sod, tof, lam, rms in rows.get("np", []):
        # A pass that crosses midnight keeps the header's date but its seconds
        # of day wrap, so a normal point earlier than the pass start is on the
        # next day.
        day_carry = 1 if sod < start_sod - 43200.0 else 0
        key = (station, ymd, day_carry, round(sod, 6))
        if key in seen:      # the daily and monthly files overlap
            continue
        seen.add(key)

        # Nearest met record on the same day; SLR met is logged once or twice a
        # pass, and the delay changes by millimetres over that.
        best = None
        for m_ymd, m_sod, p, t, h in met:
            if m_ymd != ymd:
                continue
            d = abs(m_sod - sod)
            if best is None or d < best[0]:
                best = (d, p, t, h)
        if best is None:
            print("warning: no meteorological record near %s %s; skipping"
                  % (ymd, sod), file=sys.stderr)
            continue

        y, mo, d = ymd
        total = sod + day_carry * 86400.0
        # Keep the day number and seconds of day separate: an epoch written as
        # a decimal MJD holds only microseconds, which is centimetres here.
        while total >= 86400.0:
            total -= 86400.0
            d += 1
        hh = int(total // 3600)
        mm = int((total - hh * 3600) // 60)
        ss = total - hh * 3600 - mm * 60
        out.append((y, mo, d, hh, mm, ss,
                    "%-6s %5d %3d %3d %3d %3d %13.9f  %17.9f  %7.2f %7.2f %6.1f"
                    "  %6.4f %8.1f"
                    % (station, y, mo, d, hh, mm, ss, C_KM_S * tof / 2.0,
                       best[1], best[2], best[3], lam, rms)))

    if not out:
        raise SystemExit("no normal points found")

    out.sort()
    with open(argv[-1], "w") as fh:
        fh.write("# SLR normal points, ground transmit epoch in UTC, "
                 "range = c*TOF/2\n")
        fh.write("# station year mon day  hh  mm       ss            "
                 "range_km        P_mbar    T_K    RH   lam_um  rms_ps\n")
        for row in out:
            fh.write(row[-1] + "\n")

    print("%d normal points -> %s" % (len(out), argv[-1]))
    print("  first: %04d-%02d-%02d %02d:%02d:%06.3f" % out[0][:6])
    print("  last : %04d-%02d-%02d %02d:%02d:%06.3f" % out[-1][:6])


if __name__ == "__main__":
    main(sys.argv)
