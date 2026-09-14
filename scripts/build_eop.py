#!/usr/bin/env python3
"""Build res/eopc04 from the IERS C04 series plus the finals (Bulletin A) file.

Why both. The C04 series is the definitive Earth orientation solution, but it
is only ever published for days that have already been measured and reprocessed,
so it ends well short of today. The finals file carries Bulletin A: measured
values up to a few days ago, and then roughly a year of *predictions*. Orbit
prediction needs those predictions - Frame_transform refuses any epoch past the
end of the table, so without them the software cannot propagate to today's date,
let alone forward.

So: take C04 where it exists, and finals.all for every day after it. That keeps
the definitive values for the past, including 1962-1972 which finals does not
cover, and extends the table about a year into the future.

Accuracy note. Predicted UT1-UTC drifts from truth as the lead time grows -
roughly tens of milliseconds a year out, and 1 ms of UT1 error is about 0.46 m
of position at the Earth's surface. Days taken from the prediction span are
therefore good for planning, not for precise work; re-run this script to pull
the measured values in once they exist.

Usage:
    scripts/build_eop.py <c04-file> <finals-file> <output>

Normally invoked by scripts/update_iersb.sh, which downloads both first.
"""

import sys

# Fixed-width fields of the IERS "finals" (Bulletin A) format, 0-based
# [start, end) slices. Empty fields are normal: LOD is not filled in on recent
# rows, and the whole line is blank past the end of the prediction span.
F_MJD = (7, 15)
F_PM_FLAG = (16, 17)
F_PM_X = (18, 27)
F_PM_Y = (37, 46)
F_UT1_FLAG = (57, 58)
F_UT1 = (58, 68)
F_LOD = (79, 86)
F_DPSI = (96, 106)     # milliarcseconds
F_DEPS = (117, 127)    # milliarcseconds

# Frame_transform skips exactly 14 lines before the first record, so this block
# must stay 14 lines long. It mirrors the layout of the IERS C04 file it
# replaces.
HEADER = """

                   INTERNATIONAL EARTH ROTATION AND REFERENCE SYSTEMS SERVICE
                        EARTH ORIENTATION PARAMETERS
             EOP (IERS) 14 C04 extended with Bulletin A predictions
                  built by scripts/build_eop.py - do not edit by hand

             FORMAT(3(I4),I7,2(F11.6),2(F12.7),2(F11.6),2(F11.6),2(F11.7),2(F12.6))
##################################################################################

      Date      MJD      x          y        UT1-UTC       LOD         dPsi      dEps       x Err     y Err     UT1-UTC Err  LOD Err   dPsi Err   dEps Err
                         "          "           s           s            "         "        "          "           s         s           "         "
     (0h UTC)

"""


def civil_from_mjd(mjd):
    """Calendar date for an integer MJD (civil-from-days)."""
    z = mjd - 40587 + 719468
    era = (z if z >= 0 else z - 146096) // 146097
    doe = z - era * 146097
    yoe = (doe - doe // 1460 + doe // 36524 - doe // 146096) // 365
    y = yoe + era * 400
    doy = doe - (365 * yoe + yoe // 4 - yoe // 100)
    mp = (5 * doy + 2) // 153
    d = doy - (153 * mp + 2) // 5 + 1
    m = mp + 3 if mp < 10 else mp - 9
    return (y + (1 if m <= 2 else 0)), m, d


def cut(line, span):
    return line[span[0]:span[1]].strip()


def read_c04(path):
    """MJD -> (x, y, ut1_utc, lod, dpsi, deps), all in arcsec/seconds."""
    rows = {}
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            parts = line.split()
            if len(parts) < 10:
                continue
            try:
                mjd = int(parts[3])
                vals = [float(v) for v in parts[4:10]]
            except ValueError:
                continue    # header lines
            if mjd > 30000:
                rows[mjd] = tuple(vals)
    return rows


def read_finals(path):
    """Same units as read_c04; milliarcsecond fields are scaled here."""
    rows = {}
    predicted = set()
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            if len(line) < 68:
                continue
            try:
                mjd = int(float(cut(line, F_MJD)))
                x = float(cut(line, F_PM_X))
                y = float(cut(line, F_PM_Y))
                ut1 = float(cut(line, F_UT1))
            except ValueError:
                continue    # past the end of the prediction span

            def opt(span, scale=1.0):
                text = cut(line, span)
                try:
                    return float(text) * scale
                except ValueError:
                    return 0.0

            rows[mjd] = (x, y, ut1,
                         opt(F_LOD, 1.0e-3),      # ms -> s
                         opt(F_DPSI, 1.0e-3),     # mas -> arcsec
                         opt(F_DEPS, 1.0e-3))
            if cut(line, F_PM_FLAG) == "P" or cut(line, F_UT1_FLAG) == "P":
                predicted.add(mjd)
    return rows, predicted


def main(c04_path, finals_path, out_path):
    c04 = read_c04(c04_path)
    finals, predicted = read_finals(finals_path)

    if not c04 and not finals:
        raise SystemExit("neither input produced any records")

    merged = dict(finals)
    merged.update(c04)          # the measured C04 solution wins where it exists

    # Frame_transform walks the table day by day and stops at the first gap, so
    # only a contiguous run is useful. Keep the longest one containing the
    # newest C04 day.
    keys = sorted(merged)
    start = keys[0]
    best = (keys[0], keys[0])
    for a, b in zip(keys, keys[1:]):
        if b != a + 1:
            if a - start > best[1] - best[0]:
                best = (start, a)
            start = b
    if keys[-1] - start > best[1] - best[0]:
        best = (start, keys[-1])

    first, last = best
    with open(out_path, "w") as fh:
        fh.write(HEADER)   # exactly 14 lines, as Frame_transform expects
        for mjd in range(first, last + 1):
            x, y, ut1, lod, dpsi, deps = merged[mjd]
            year, month, day = civil_from_mjd(mjd)
            fh.write("%4d%4d%4d%7d%11.6f%11.6f%12.7f%12.7f%11.6f%11.6f"
                     "%11.6f%11.6f%12.7f%12.7f\n"
                     % (year, month, day, mjd, x, y, ut1, lod, dpsi, deps,
                        0.0, 0.0, 0.0, 0.0))

    n_pred = sum(1 for m in range(first, last + 1)
                 if m in predicted and m not in c04)
    last_measured = max(c04) if c04 else first
    print("wrote %s" % out_path)
    print("  %d days, MJD %d to %d (%04d-%02d-%02d to %04d-%02d-%02d)"
          % (last - first + 1, first, last,
             *(civil_from_mjd(first) + civil_from_mjd(last))))
    print("  measured through MJD %d (%04d-%02d-%02d)"
          % ((last_measured,) + civil_from_mjd(last_measured)))
    print("  %d predicted days beyond that" % n_pred)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3])
