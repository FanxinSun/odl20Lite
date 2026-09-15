#!/usr/bin/env python3
"""Initial orbit determination from three optical angles taken on one pass,
under a circular-orbit assumption, so that an angles-only fit can be started
without a catalogue entry.

This is the step that makes the rest of it honest. fit_orbit_to_angles needs a
starting state, and taking one from a two-line element or from JPL Horizons
would quietly put the element set back into the chain the whole exercise is
trying to get out of. An object with no reflector and no catalogue entry has to
be started from its own observations, and this is how that is done.

The method. An angle gives a direction and says nothing about distance, so
three observations are three unit vectors from known places and six unknowns.
Assuming the orbit is circular closes it: the satellite sits where the line of
sight pierces a sphere of radius a,

    rho = -(R.L) + sqrt((R.L)^2 - |R|^2 + a^2)

which gives three positions for an assumed a. Herrick-Gibbs then gives the
velocity at the middle one, and the state that comes out has its own semi-major
axis. Where that equals the assumed a, the assumption is self-consistent. The
script brackets and bisects on that difference.

Circular is an assumption and a real orbit is not. For a low Earth orbit at a
few parts in a thousand of eccentricity the error is kilometres, which is a fine
starting guess and useless as an answer - which is the point: the fit that
follows is what determines the orbit, from the angles.

Usage:
    angles_iod.py <observations.angles> <sites.txt> [--night YYYY-MM-DD]
                  [--amin km] [--amax km]

Prints a config block for res/configOPS_*.txt.
"""

import datetime
import math
import os
import subprocess
import sys

MU = 398600.4418
BINDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bin")
A_WGS84 = 6378.137
F_WGS84 = 1.0 / 298.257223563


def site_ecef(lat_deg, lon_deg, alt_m):
    lat, lon = math.radians(lat_deg), math.radians(lon_deg)
    e2 = F_WGS84 * (2.0 - F_WGS84)
    n = A_WGS84 / math.sqrt(1.0 - e2 * math.sin(lat) ** 2)
    h = alt_m / 1000.0
    return ((n + h) * math.cos(lat) * math.cos(lon),
            (n + h) * math.cos(lat) * math.sin(lon),
            (n * (1.0 - e2) + h) * math.sin(lat))


def ecef_to_eci(p, mjd):
    """Through the tree's own ECEF2ECI, so the frame matches the propagator."""
    out = subprocess.run(
        [os.path.join(BINDIR, "ECEF2ECI"), "%.6f" % p[0], "%.6f" % p[1],
         "%.6f" % p[2], "0", "0", "0", "%.12f" % mjd],
        capture_output=True, text=True, cwd=BINDIR)
    for line in out.stdout.splitlines():
        if line.startswith("ECI:"):
            f = line.split()
            return (float(f[1]), float(f[2]), float(f[3]))
    raise SystemExit("ECEF2ECI failed at MJD %.6f:\n%s%s"
                     % (mjd, out.stdout, out.stderr))


def herrick_gibbs(r1, r2, r3, t1, t2, t3):
    d21, d32, d31 = t2 - t1, t3 - t2, t3 - t1
    n1 = math.sqrt(sum(c * c for c in r1))
    n2 = math.sqrt(sum(c * c for c in r2))
    n3 = math.sqrt(sum(c * c for c in r3))
    c1 = -d32 * (1.0 / (d21 * d31) + MU / (12.0 * n1 ** 3))
    c2 = (d32 - d21) * (1.0 / (d21 * d32) + MU / (12.0 * n2 ** 3))
    c3 = d21 * (1.0 / (d32 * d31) + MU / (12.0 * n3 ** 3))
    return tuple(c1 * r1[k] + c2 * r2[k] + c3 * r3[k] for k in range(3))


def read_sites(path):
    out = {}
    for line in open(path):
        if not line.strip() or line[0] in "#/":
            continue
        f = line.split()
        if len(f) < 5:
            continue
        out[f[0]] = site_ecef(float(f[2]), float(f[3]), float(f[4]))
    return out


def read_obs(path):
    rows = []
    for line in open(path):
        if not line.strip() or line[0] == "#":
            continue
        f = line.split()
        t = datetime.datetime(int(f[1]), int(f[2]), int(f[3]), int(f[4]),
                              int(f[5])) + datetime.timedelta(seconds=float(f[6]))
        rows.append((t, f[0], math.radians(float(f[7])),
                     math.radians(float(f[8]))))
    rows.sort()
    return rows


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    obs = read_obs(argv[1])
    sites = read_sites(argv[2])
    night = None
    amin, amax = 6500.0, 9000.0
    for i, a in enumerate(argv):
        if a == "--night":
            night = argv[i + 1]
        elif a == "--amin":
            amin = float(argv[i + 1])
        elif a == "--amax":
            amax = float(argv[i + 1])

    if night:
        d = datetime.datetime.strptime(night, "%Y-%m-%d").date()
        obs = [o for o in obs if o[0].date() == d]
    if len(obs) < 3:
        raise SystemExit("need three observations on one pass; have %d"
                         % len(obs))
    obs = obs[:3]

    epoch0 = datetime.datetime(1858, 11, 17)
    geo = []
    for t, site, ra, dec in obs:
        if site not in sites:
            raise SystemExit("no coordinates for site %s" % site)
        mjd = (t - epoch0).total_seconds() / 86400.0
        R = ecef_to_eci(sites[site], mjd)
        L = (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra),
             math.sin(dec))
        geo.append((t, R, L))

    t0 = geo[0][0]
    secs = [(g[0] - t0).total_seconds() for g in geo]

    def closure(a):
        """Assumed a in, resulting a out, minus the assumption."""
        pos = []
        for _, R, L in geo:
            rl = sum(R[k] * L[k] for k in range(3))
            r2 = sum(c * c for c in R)
            disc = rl * rl - r2 + a * a
            if disc < 0.0:
                return None
            rho = -rl + math.sqrt(disc)
            if rho <= 0.0:
                return None
            pos.append(tuple(R[k] + rho * L[k] for k in range(3)))
        v = herrick_gibbs(pos[0], pos[1], pos[2], secs[0], secs[1], secs[2])
        rn = math.sqrt(sum(c * c for c in pos[1]))
        vn2 = sum(c * c for c in v)
        denom = 2.0 / rn - vn2 / MU
        if denom <= 0.0:
            return None
        return (1.0 / denom) - a, pos[1], v

    # Bracket where the closure changes sign, then bisect. The function is
    # smooth in a, and one root is the orbit.
    lo, hi, f_lo = None, None, None
    steps = 250
    prev = None
    for i in range(steps + 1):
        a = amin + (amax - amin) * i / steps
        c = closure(a)
        if c is None:
            prev = None
            continue
        if prev is not None and prev[1] * c[0] < 0.0:
            lo, hi, f_lo = prev[0], a, prev[1]
            break
        prev = (a, c[0])

    if lo is None:
        raise SystemExit("no self-consistent circular orbit between %.0f and "
                         "%.0f km; widen --amin/--amax" % (amin, amax))

    for _ in range(80):
        mid = 0.5 * (lo + hi)
        c = closure(mid)
        if c is None:
            break
        if c[0] * f_lo < 0.0:
            hi = mid
        else:
            lo, f_lo = mid, c[0]
    a = 0.5 * (lo + hi)
    _, r, v = closure(a)

    # Herrick-Gibbs gives the velocity at the middle observation; step back to
    # the first one so that every observation of the pass lies inside the arc a
    # fit starting here will cover. Two-body over a minute is exact enough for
    # a starting guess.
    # 60 s of margin: the fit steps BACK from each observation by the light
    # time, so an arc that starts exactly at the first observation does not
    # contain it.
    dt = secs[0] - secs[1] - 60.0
    steps = 400
    h = dt / steps
    r = list(r)
    v = list(v)
    for _ in range(steps):
        def acc(p):
            m = math.sqrt(sum(c * c for c in p))
            return [-MU * c / m ** 3 for c in p]
        a1 = acc(r)
        r2 = [r[i] + 0.5 * h * v[i] for i in range(3)]
        v2 = [v[i] + 0.5 * h * a1[i] for i in range(3)]
        a2 = acc(r2)
        r3 = [r[i] + 0.5 * h * v2[i] for i in range(3)]
        v3 = [v[i] + 0.5 * h * a2[i] for i in range(3)]
        a3 = acc(r3)
        r4 = [r[i] + h * v3[i] for i in range(3)]
        v4 = [v[i] + h * a3[i] for i in range(3)]
        a4 = acc(r4)
        r = [r[i] + h / 6.0 * (v[i] + 2 * v2[i] + 2 * v3[i] + v4[i])
             for i in range(3)]
        v = [v[i] + h / 6.0 * (a1[i] + 2 * a2[i] + 2 * a3[i] + a4[i])
             for i in range(3)]

    t = geo[0][0] - datetime.timedelta(seconds=60.0)
    alt = math.sqrt(sum(c * c for c in r)) - A_WGS84
    print("Circular-orbit IOD from three angles on %s"
          % geo[0][0].strftime("%Y-%m-%d"))
    print("  semi-major axis  : %.3f km   (altitude %.1f km)" % (a, alt))
    print("  speed            : %.6f km/s" % math.sqrt(sum(c * c for c in v)))
    print("  ranges were solved from the circular constraint, not measured.")
    print()
    print("t0                  = %d,%02d,%02d,%02d,%02d,%09.6f"
          % (t.year, t.month, t.day, t.hour, t.minute,
             t.second + t.microsecond * 1e-6))
    for name, val in zip(("x0", "y0", "z0"), r):
        print("%-20s= %18.6f" % (name, val))
    for name, val in zip(("u0", "v0", "w0"), v):
        print("%-20s= %18.9f" % (name, val))


if __name__ == "__main__":
    main(sys.argv)
