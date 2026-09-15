#!/usr/bin/env python3
"""Find the orbit behind a set of optical angles by searching the range and
range-rate that the angles cannot see.

The problem this solves. An angle fixes a direction and says nothing about
distance. One short pass therefore gives an "attributable" - a sky position and
its rate, four numbers - and leaves the other two, range and range-rate,
completely open. Fitting cannot start until something picks them, and for an
object with no catalogue entry there is nothing to pick them from.

The method is the standard one for this case. Every pair (rho, rho-dot) that
gives a bound orbit is admissible:

    r = R + rho * L
    v = R-dot + rho-dot * L + rho * L-dot

so grid the pair, build the state each one implies, propagate it, and score it
against every other observation there is. Most of the grid predicts the object
somewhere it was not seen; what survives is the orbit. The later observations do
the work - one pass cannot tell a nearby slow object from a distant fast one,
and two passes a week apart can.

Propagation here is two-body plus the secular effect of J2, which over a couple
of weeks is good to a fraction of a degree. That is enough to find the right
basin and nowhere near enough to be an answer: hand the result to
fit_orbit_to_angles, which integrates properly.

Usage:
    angles_admissible.py <observations.angles> <sites.txt>
        [--night YYYY-MM-DD]      pass to build the attributable from
        [--rho MIN MAX N]         range grid, km       (default 400 4000 90)
        [--rhodot MIN MAX N]      range rate, km/s     (default -7 7 81)
        [--days D]                only score observations within D days
"""

import datetime
import math
import os
import subprocess
import sys

MU = 398600.4418
J2 = 1.08262668E-3
RE = 6378.137
OMEGA_E = 7.2921150E-5
BINDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bin")


def site_ecef(lat_deg, lon_deg, alt_m):
    lat, lon = math.radians(lat_deg), math.radians(lon_deg)
    f = 1.0 / 298.257223563
    e2 = f * (2.0 - f)
    n = RE / math.sqrt(1.0 - e2 * math.sin(lat) ** 2)
    h = alt_m / 1000.0
    return ((n + h) * math.cos(lat) * math.cos(lon),
            (n + h) * math.cos(lat) * math.sin(lon),
            (n * (1.0 - e2) + h) * math.sin(lat))


def ecef_to_eci(p, mjd):
    out = subprocess.run(
        [os.path.join(BINDIR, "ECEF2ECI"), "%.6f" % p[0], "%.6f" % p[1],
         "%.6f" % p[2], "0", "0", "0", "%.12f" % mjd],
        capture_output=True, text=True, cwd=BINDIR)
    for line in out.stdout.splitlines():
        if line.startswith("ECI:"):
            f = line.split()
            return (float(f[1]), float(f[2]), float(f[3]))
    raise SystemExit("ECEF2ECI failed at MJD %.6f" % mjd)


def elements(r, v):
    rn = math.sqrt(sum(c * c for c in r))
    vn2 = sum(c * c for c in v)
    a = 1.0 / (2.0 / rn - vn2 / MU)
    h = (r[1] * v[2] - r[2] * v[1], r[2] * v[0] - r[0] * v[2],
         r[0] * v[1] - r[1] * v[0])
    hn = math.sqrt(sum(c * c for c in h))
    inc = math.acos(max(-1.0, min(1.0, h[2] / hn)))
    raan = math.atan2(h[0], -h[1])
    rv = sum(r[k] * v[k] for k in range(3))
    e_vec = [((vn2 - MU / rn) * r[k] - rv * v[k]) / MU for k in range(3)]
    e = math.sqrt(sum(c * c for c in e_vec))
    p = hn * hn / MU
    # argument of latitude, then true anomaly
    u = math.atan2(r[2] / math.sin(inc) if abs(math.sin(inc)) > 1e-12 else 0.0,
                   r[0] * math.cos(raan) + r[1] * math.sin(raan))
    if e > 1e-10:
        nu = math.atan2(rv / math.sqrt(MU) * math.sqrt(p), p - rn)
    else:
        nu = 0.0
    argp = u - nu
    E = 2.0 * math.atan2(math.tan(nu / 2.0) * math.sqrt(1.0 - e),
                         math.sqrt(1.0 + e)) if e < 1.0 else 0.0
    M = E - e * math.sin(E)
    return a, e, inc, raan, argp, M


def state(a, e, inc, raan, argp, M):
    E = M
    for _ in range(60):
        dE = (E - e * math.sin(E) - M) / (1.0 - e * math.cos(E))
        E -= dE
        if abs(dE) < 1e-13:
            break
    nu = 2.0 * math.atan2(math.sqrt(1.0 + e) * math.sin(E / 2.0),
                          math.sqrt(1.0 - e) * math.cos(E / 2.0))
    p = a * (1.0 - e * e)
    rn = p / (1.0 + e * math.cos(nu))
    rp = (rn * math.cos(nu), rn * math.sin(nu), 0.0)
    vp = (-math.sqrt(MU / p) * math.sin(nu),
          math.sqrt(MU / p) * (e + math.cos(nu)), 0.0)
    co, so = math.cos(raan), math.sin(raan)
    cw, sw = math.cos(argp), math.sin(argp)
    ci, si = math.cos(inc), math.sin(inc)
    R = ((co * cw - so * sw * ci, -co * sw - so * cw * ci, so * si),
         (so * cw + co * sw * ci, -so * sw + co * cw * ci, -co * si),
         (sw * si, cw * si, ci))
    r = tuple(sum(R[i][j] * rp[j] for j in range(3)) for i in range(3))
    v = tuple(sum(R[i][j] * vp[j] for j in range(3)) for i in range(3))
    return r, v


def propagate(r0, v0, dt):
    """Two-body plus the secular J2 rates. Good to a fraction of a degree over
    a fortnight, which is what a search needs and not what an answer needs."""
    a, e, inc, raan, argp, M = elements(r0, v0)
    if a <= RE or e >= 1.0:
        return None
    n = math.sqrt(MU / a ** 3)
    p = a * (1.0 - e * e)
    k = 1.5 * J2 * (RE / p) ** 2 * n
    raan += -k * math.cos(inc) * dt
    argp += 0.5 * k * (5.0 * math.cos(inc) ** 2 - 1.0) * dt
    M += (n + 0.5 * k * math.sqrt(1.0 - e * e) *
          (3.0 * math.cos(inc) ** 2 - 1.0)) * dt
    return state(a, e, inc, raan, argp, M)


def read_sites(path):
    out = {}
    for line in open(path):
        if not line.strip() or line[0] in "#/":
            continue
        f = line.split()
        if len(f) >= 5:
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
    rho_grid = (400.0, 4000.0, 90)
    rd_grid = (-7.0, 7.0, 81)
    days = None
    for i, a in enumerate(argv):
        if a == "--night":
            night = argv[i + 1]
        elif a == "--rho":
            rho_grid = (float(argv[i + 1]), float(argv[i + 2]),
                        int(argv[i + 3]))
        elif a == "--rhodot":
            rd_grid = (float(argv[i + 1]), float(argv[i + 2]), int(argv[i + 3]))
        elif a == "--days":
            days = float(argv[i + 1])

    if night is None:
        night = obs[0][0].date().isoformat()
    day = datetime.datetime.strptime(night, "%Y-%m-%d").date()
    pas = [o for o in obs if o[0].date() == day]
    if len(pas) < 3:
        raise SystemExit("need three observations on %s; have %d"
                         % (night, len(pas)))
    pas = pas[:3]

    mjd0 = datetime.datetime(1858, 11, 17)

    def observer(t):
        mjd = (t - mjd0).total_seconds() / 86400.0
        R = ecef_to_eci(sites[pas[0][1]], mjd)
        # The observer's inertial velocity is the Earth turning under them.
        Rd = (-OMEGA_E * R[1], OMEGA_E * R[0], 0.0)
        return R, Rd

    # Attributable: sky position and rate at the middle observation, by fitting
    # a straight line in time through the three.
    t0 = pas[1][0]
    ts = [(o[0] - t0).total_seconds() for o in pas]
    ra0 = pas[1][2]
    ras = [o[2] - ra0 for o in pas]
    ras = [r - 2 * math.pi * round(r / (2 * math.pi)) for r in ras]
    decs = [o[3] - pas[1][3] for o in pas]

    def slope(y):
        n = len(ts)
        sx, sy = sum(ts), sum(y)
        sxx = sum(t * t for t in ts)
        sxy = sum(ts[k] * y[k] for k in range(n))
        return (n * sxy - sx * sy) / (n * sxx - sx * sx)

    ra_dot, dec_dot = slope(ras), slope(decs)
    ra, dec = pas[1][2], pas[1][3]

    L = (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra),
         math.sin(dec))
    L_a = (-math.cos(dec) * math.sin(ra), math.cos(dec) * math.cos(ra), 0.0)
    L_d = (-math.sin(dec) * math.cos(ra), -math.sin(dec) * math.sin(ra),
           math.cos(dec))
    Ld = tuple(ra_dot * L_a[k] + dec_dot * L_d[k] for k in range(3))

    R0, Rd0 = observer(t0)

    scored = []
    if days is not None:
        obs = [o for o in obs
               if abs((o[0] - t0).total_seconds()) <= days * 86400.0]
    geo = []
    for t, site, ora, odec in obs:
        R, _ = observer(t)
        geo.append(((t - t0).total_seconds(), R, ora, odec))

    print("attributable at %s: RA %.4f deg, dec %.4f deg, rates %.5f, %.5f "
          "deg/s" % (t0.isoformat(), math.degrees(ra), math.degrees(dec),
                     math.degrees(ra_dot), math.degrees(dec_dot)),
          file=sys.stderr)
    print("scoring against %d observations over %.1f days"
          % (len(geo), (geo[-1][0] - geo[0][0]) / 86400.0), file=sys.stderr)

    for i in range(rho_grid[2]):
        rho = rho_grid[0] + (rho_grid[1] - rho_grid[0]) * i / (rho_grid[2] - 1)
        r = tuple(R0[k] + rho * L[k] for k in range(3))
        if math.sqrt(sum(c * c for c in r)) < RE + 150.0:
            continue
        for j in range(rd_grid[2]):
            rd = rd_grid[0] + (rd_grid[1] - rd_grid[0]) * j / (rd_grid[2] - 1)
            v = tuple(Rd0[k] + rd * L[k] + rho * Ld[k] for k in range(3))
            a, e, _, _, _, _ = elements(r, v)
            if not (RE + 150.0 < a < 50000.0) or e >= 0.6:
                continue
            if a * (1.0 - e) < RE + 120.0:
                continue
            tot, n = 0.0, 0
            bad = False
            for dt, R, ora, odec in geo:
                st = propagate(r, v, dt)
                if st is None:
                    bad = True
                    break
                d = tuple(st[0][k] - R[k] for k in range(3))
                dn = math.sqrt(sum(c * c for c in d))
                cra = math.atan2(d[1], d[0])
                cdec = math.asin(d[2] / dn)
                dra = (ora - cra)
                dra -= 2 * math.pi * round(dra / (2 * math.pi))
                dra *= math.cos(cdec)
                ddec = odec - cdec
                tot += dra * dra + ddec * ddec
                n += 2
            if bad or n == 0:
                continue
            scored.append((math.degrees(math.sqrt(tot / n)), rho, rd, a, e))

    if not scored:
        raise SystemExit("nothing admissible on this grid")
    scored.sort()

    print("\n  rank   RMS (deg)    range km   range rate km/s      a km      e")
    for k in range(min(8, len(scored))):
        s = scored[k]
        print("  %4d %11.4f %11.1f %15.3f %10.1f %7.4f"
              % (k + 1, s[0], s[1], s[2], s[3], s[4]))

    best = scored[0]
    rho, rd = best[1], best[2]
    r = tuple(R0[k] + rho * L[k] for k in range(3))
    v = tuple(Rd0[k] + rd * L[k] + rho * Ld[k] for k in range(3))
    print("\nBest state, for res/configOPS_*.txt:")
    print("t0                  = %d,%02d,%02d,%02d,%02d,%09.6f"
          % (t0.year, t0.month, t0.day, t0.hour, t0.minute,
             t0.second + t0.microsecond * 1e-6))
    for name, val in zip(("x0", "y0", "z0"), r):
        print("%-20s= %18.6f" % (name, val))
    for name, val in zip(("u0", "v0", "w0"), v):
        print("%-20s= %18.9f" % (name, val))


if __name__ == "__main__":
    main(sys.argv)
