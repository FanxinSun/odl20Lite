#!/usr/bin/env python3
"""Extract optical angles for one object from the SeeSat-L archives and write
the observation file fit_orbit_to_angles reads.

Why bother with amateur data. Laser ranging measures a range to a centimetre,
and satellite laser ranging is the best tracking data that exists - but it only
works on an object built to be ranged, with a retroreflector bolted to it. The
objects this software is aimed at have no reflector, no transponder and no
cooperation of any kind. What exists for them is angles: somebody photographed
a moving dot against the stars and measured where it was.

SeeSat-L has carried those measurements since November 1994 and the archives are
public:

    http://satobs.org/seesat/<Mon>-<YYYY>/

Usage:
    seesat2angles.py <intl-designation> <out.angles> [options]

        --cache DIR     where to keep downloaded months (default ./seesat_cache)
        --from YYYY-MM  first month (default 2024-09)
        --to   YYYY-MM  last month  (default: this month)
        --sigma ARCSEC  observation uncertainty to record (default 60)

The designation is the UK-format seven-digit form: year, launch number, piece as
a number. ACS3 is 2024-077B, so 2407702. LightSail-2 is 2019-036AC, which is
piece 29, so 1903629.

THE FORMAT, AND HOW IT WAS ESTABLISHED
--------------------------------------
satobs.org publishes a U.K. format specification, and the observations in these
archives do not follow it exactly: the right ascension sits at columns 33-40
rather than 35-42, and no position-format code is written. Guessing wrong here
is not a small matter - reading decimal minutes as seconds is a 20% error in the
right ascension rate, which a fit would absorb into the orbit and report as a
converged solution.

So it was measured, over 4780 observation lines in the archive:

  * columns 37-38 of the right ascension - the third pair of digits - never
    exceed 59, not once. Under decimal minutes they would be uniform over
    00-99. So the field is HHMMSSss: seconds, then hundredths.

  * the fifth digit of the declination is uniform over 0-9. Under +DDMMSSs it
    would be the tens digit of arcseconds and would never exceed 5. So the
    field is +DDMMm: degrees, arcminutes, tenths of an arcminute.

That is a mixture of two of the specified codes, which is presumably why no code
is written. Resolution is therefore about 0.15 arcsec in right ascension and
6 arcsec in declination; ACCURACY is far coarser, and the observers' own figure
in columns 51-54 is what --sigma should be set from.
"""

import datetime
import os
import re
import sys
import html
import urllib.request

BASE = "http://satobs.org/seesat"
MONTHS = "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec".split()


def fetch(url, path):
    if os.path.exists(path):
        return open(path, encoding="utf-8", errors="replace").read()
    try:
        with urllib.request.urlopen(url, timeout=30) as fh:
            text = fh.read().decode("utf-8", "replace")
    except Exception as exc:                                  # noqa: BLE001
        return ""
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)
    return text


def body_of(page):
    m = re.search(r"<pre[^>]*>(.*?)</pre>", page, re.S)
    if not m:
        return ""
    return html.unescape(re.sub(r"<[^>]+>", "", m.group(1)))


def parse_line(line, want):
    """One U.K.-format observation line, or None."""
    if len(line) < 48 or not line.startswith(want):
        return None
    site = line[7:11].strip()
    date = line[11:17]
    tim = line[17:27]
    ra = line[32:40]
    dec = line[42:47]
    sign = line[42]
    if not (re.fullmatch(r"\d{6}", date) and re.fullmatch(r"\d{8}", ra)
            and sign in "+-" and re.fullmatch(r"\d{4}", line[43:47])):
        return None
    if not re.fullmatch(r"\d{6,10}", tim.rstrip()):
        return None

    # Trailing insignificant zeros are left blank in the time field.
    tim = (tim.rstrip() + "0000000000")[:10]
    year = 2000 + int(date[0:2])
    month, day = int(date[2:4]), int(date[4:6])
    hh, mm = int(tim[0:2]), int(tim[2:4])
    ss = int(tim[4:6]) + int(tim[6:10]) / 10000.0

    ra_deg = (int(ra[0:2]) + int(ra[2:4]) / 60.0 +
              (int(ra[4:6]) + int(ra[6:8]) / 100.0) / 3600.0) * 15.0
    dec_deg = int(line[43:45]) + (int(line[45:47]) +
                                  int(line[47]) / 10.0) / 60.0
    if sign == "-":
        dec_deg = -dec_deg

    # The observers' own accuracy figure, columns 51-54, in units that depend on
    # the position code; for this mixture it is arcminutes. Blank often enough
    # that the caller's --sigma has to stand in.
    acc = line[50:54].strip()
    acc_arcsec = float(acc) * 60.0 / 100.0 if acc.isdigit() else None

    return (year, month, day, hh, mm, ss, site, ra_deg, dec_deg, acc_arcsec)


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    want, out_path = argv[1], argv[2]
    cache = "./seesat_cache"
    t_from, t_to = (2024, 9), None
    sigma = 60.0
    for i, a in enumerate(argv):
        if a == "--cache":
            cache = argv[i + 1]
        elif a == "--from":
            t_from = tuple(int(v) for v in argv[i + 1].split("-"))
        elif a == "--to":
            t_to = tuple(int(v) for v in argv[i + 1].split("-"))
        elif a == "--sigma":
            sigma = float(argv[i + 1])
    if t_to is None:
        now = datetime.date.today()
        t_to = (now.year, now.month)
    os.makedirs(cache, exist_ok=True)

    months = []
    y, m = t_from
    while (y, m) <= t_to:
        months.append((y, m))
        m += 1
        if m == 13:
            y, m = y + 1, 1

    rows = []
    for y, m in months:
        tag = "%s-%d" % (MONTHS[m - 1], y)
        index = fetch("%s/%s/index.html" % (BASE, tag),
                      os.path.join(cache, "idx_%s.html" % tag))
        ids = sorted(set(re.findall(r'href="(\d+)\.html"', index)))
        if ids:
            print("  %s: %d messages" % (tag, len(ids)), file=sys.stderr)
        for mid in ids:
            page = fetch("%s/%s/%s.html" % (BASE, tag, mid),
                         os.path.join(cache, "%s_%s.html" % (tag, mid)))
            if want not in page:
                continue
            for line in body_of(page).splitlines():
                rec = parse_line(line.rstrip("\n"), want)
                if rec:
                    rows.append(rec)

    # The daily and monthly postings overlap, and observers repost corrections.
    rows = sorted(set(rows))
    if not rows:
        raise SystemExit("no observations of %s found" % want)

    with open(out_path, "w") as fh:
        fh.write("# optical angles for %s, from the SeeSat-L archives\n" % want)
        fh.write("# site  year mon day  hh  mm       ss          ra_deg"
                 "        dec_deg     sigma_arcsec\n")
        for (y, mo, d, hh, mm, ss, site, ra, dec, acc) in rows:
            fh.write("%-6s %5d %3d %3d %3d %3d %12.4f %14.7f %14.7f %10.1f\n"
                     % (site, y, mo, d, hh, mm, ss, ra, dec,
                        acc if acc else sigma))

    sites = sorted(set(r[6] for r in rows))
    nights = sorted(set((r[0], r[1], r[2]) for r in rows))
    print("%d observations -> %s" % (len(rows), out_path))
    print("  sites : %s" % " ".join(sites))
    print("  nights: %d, from %04d-%02d-%02d to %04d-%02d-%02d"
          % (len(nights), nights[0][0], nights[0][1], nights[0][2],
             nights[-1][0], nights[-1][1], nights[-1][2]))


if __name__ == "__main__":
    main(sys.argv)
