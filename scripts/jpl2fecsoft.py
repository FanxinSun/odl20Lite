#!/usr/bin/env python3
"""Convert a JPL binary planetary ephemeris into the FECsoft container that
external/fecsoft/astrolib.cpp reads, optionally trimmed to a date range.

The bundled res/1980_2020 is a DE405 subset in FECsoft's own layout, which is
not the layout JPL publishes: dropping a stock JPL file in its place overflows
the reader. The Chebyshev coefficient records themselves are identical in both
formats, so only the header has to be repacked.

  JPL binary                        FECsoft container
  ----------------------------      ---------------------------------
  record 1 (KSIZE*4 bytes)          packed header, 317 + 14*ncon bytes
    ttl[3][84]                        ttl[3][65]        (truncated)
    cnam[400][6]                      cnam[ncon][6]     (only ncon kept)
    ss[3]      double                 ss[3]     double  (rewritten to span)
    ncon       int32                  ncon      int16
    au, emrat  double                 au, emrat double
    ipt[12][3] int32                  ipt[3][12] int16  (transposed!)
    numde      int32                  numde     int16
    lpt[3]     int32                  lpt[3]    int16
  record 2                            cval[ncon] double
    cval[ncon] double
  records 3..N                      coefficient records, copied verbatim
    coefficient records

Usage:
    scripts/jpl2fecsoft.py <jpl-binary> <output> [start-year] [end-year]

Example, covering 2000 to 2050 from JPL's DE405 Linux binary:
    curl -O https://ssd.jpl.nasa.gov/ftp/eph/planets/Linux/de405/lnxp1600p2200.405
    scripts/jpl2fecsoft.py lnxp1600p2200.405 res/1980_2020 2000 2050

Run it against res/1980_2020 and the utilities pick up the new range with no
code change; keep a copy of the original first.
"""

import struct
import sys

# A Julian date is easier to talk about as a calendar year here; these are
# deliberately crude (Jan 1 of the year, Gregorian) because the record
# boundaries are 32 days apart anyway and we always widen to whole records.
def jd_of_year(year):
    a = (14 - 1) // 12
    y = year + 4800 - a
    m = 1 + 12 * a - 3
    jdn = 1 + (153 * m + 2) // 5 + 365 * y + y // 4 - y // 100 + y // 400 - 32045
    return jdn - 0.5


def jd_to_iso(jd):
    z = int(jd + 0.5)
    a = z
    if z >= 2299161:
        alpha = int((z - 1867216.25) / 36524.25)
        a = z + 1 + alpha - alpha // 4
    b = a + 1524
    c = int((b - 122.1) / 365.25)
    d = int(365.25 * c)
    e = int((b - d) / 30.6001)
    day = b - d - int(30.6001 * e)
    month = e - 1 if e < 14 else e - 13
    year = c - 4716 if month > 2 else c - 4715
    return "%04d-%02d-%02d" % (year, month, day)


def convert(src_path, dst_path, start_year=None, end_year=None):
    with open(src_path, "rb") as fh:
        blob = fh.read()

    # --- JPL record 1 -------------------------------------------------------
    ttl = [blob[i * 84:(i + 1) * 84] for i in range(3)]
    cnam_all = blob[252:252 + 400 * 6]
    ss = list(struct.unpack("<3d", blob[2652:2676]))
    ncon = struct.unpack("<i", blob[2676:2680])[0]
    au = struct.unpack("<d", blob[2680:2688])[0]
    emrat = struct.unpack("<d", blob[2688:2696])[0]
    ipt_flat = struct.unpack("<36i", blob[2696:2840])   # 12 triplets
    numde = struct.unpack("<i", blob[2840:2844])[0]
    lpt = list(struct.unpack("<3i", blob[2844:2856]))

    if not (0 < ncon <= 400):
        raise SystemExit("unexpected constant count %d - is this a JPL binary?" % ncon)

    # JPL stores IPT(3,12) column-major: (start, ncf, na) per body.
    start_i = [ipt_flat[3 * j + 0] for j in range(12)]
    ncf_i = [ipt_flat[3 * j + 1] for j in range(12)]
    na_i = [ipt_flat[3 * j + 2] for j in range(12)]

    # Same arithmetic the reader uses, so the record size must agree.
    ncoeff = 0
    for j in range(12):
        ncoeff += ncf_i[j] * na_i[j] * (2 if j == 11 else 3)
    ncoeff += lpt[1] * lpt[2] * 3 + 2
    reclen = ncoeff * 8

    # --- locate the coefficient records ------------------------------------
    # Records 1 and 2 are header records of the same length.
    first = 2 * reclen
    n_records = (len(blob) - first) // reclen
    if n_records <= 0:
        raise SystemExit("no coefficient records found; wrong record length?")

    lo, hi = 0, n_records
    if start_year is not None:
        want = jd_of_year(start_year)
        while lo + 1 < n_records:
            jd0 = struct.unpack("<d", blob[first + (lo + 1) * reclen:
                                           first + (lo + 1) * reclen + 8])[0]
            if jd0 > want:
                break
            lo += 1
    if end_year is not None:
        want = jd_of_year(end_year)
        hi = lo
        while hi < n_records:
            jd1 = struct.unpack("<d", blob[first + hi * reclen + 8:
                                           first + hi * reclen + 16])[0]
            hi += 1
            if jd1 >= want:
                break

    kept = blob[first + lo * reclen:first + hi * reclen]
    ss[0] = struct.unpack("<d", kept[0:8])[0]
    ss[1] = struct.unpack("<d", kept[(hi - lo - 1) * reclen + 8:
                                     (hi - lo - 1) * reclen + 16])[0]

    # --- FECsoft header -----------------------------------------------------
    out = bytearray()
    for t in ttl:
        out += t[:65].ljust(65, b" ")
    out += struct.pack("<h", ncon)
    out += cnam_all[:ncon * 6]
    out += struct.pack("<3d", *ss)
    out += struct.pack("<d", au)
    out += struct.pack("<d", emrat)
    for row in (start_i, ncf_i, na_i):          # transposed: ipt[3][12]
        for v in row:
            out += struct.pack("<h", v)
    out += struct.pack("<h", numde)
    for v in lpt:
        out += struct.pack("<h", v)
    out += blob[reclen:reclen + ncon * 8]        # cval from JPL record 2

    expected = 317 + 14 * ncon
    if len(out) != expected:
        raise SystemExit("header is %d bytes, reader expects %d" % (len(out), expected))

    with open(dst_path, "wb") as fh:
        fh.write(bytes(out))
        fh.write(kept)

    print("%s -> %s" % (src_path, dst_path))
    print("  DE%-4d  ncon=%d  ncoeff=%d  record=%d bytes" % (numde, ncon, ncoeff, reclen))
    print("  header %d bytes, %d records" % (len(out), hi - lo))
    print("  covers JD %.1f to %.1f  (%s to %s)"
          % (ss[0], ss[1], jd_to_iso(ss[0]), jd_to_iso(ss[1])))


if __name__ == "__main__":
    if len(sys.argv) not in (3, 5):
        raise SystemExit(__doc__)
    sy = int(sys.argv[3]) if len(sys.argv) == 5 else None
    ey = int(sys.argv[4]) if len(sys.argv) == 5 else None
    convert(sys.argv[1], sys.argv[2], sy, ey)
