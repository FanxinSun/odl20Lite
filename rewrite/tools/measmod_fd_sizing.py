#!/usr/bin/env python3
"""measmod_fd_sizing.py — the finite-difference gate's sizing, SPEC-measmod.md §6.2, as the numbers it freezes.

WRITTEN 2026-10-06, BEFORE ANY RUN OF THE GATE (plan §4 rule 7).  A central difference of the measurement model's
output carries three errors (the argument of SPEC-stm `STM-P-1`, applied to measmod):

    truncation   T(h) = h^2 F / 6      F the third derivative of the output along the displacement direction
    noise        N(h) = nu / h         nu the bound of ONE evaluation's round-off (3 ulp of the largest operand)
    light-time tolerance               absent: the iteration runs to the double-precision fixed point

    bound        eps(h) = T(h) + N(h);  band prediction B_pred = max over the five sizes of eps(h)

Range: F = 3 mu (1 - mu^2) / rho^2, at most 1.1547 / rho^2 (mu = cos of the angle between the displacement and the line of sight).
Angle: F_a = 2 / (rho^3 cos^3 phi_max).  The right-ascension coefficient is the analytic 2 Im[(w1/w0)^3] with w0 = n_x + i n_y, so it is
bounded by 2/cos^3(dec); `--scan` confirms it, and the declination coefficient, by an exact truncated power series over displacement
directions and lines of sight (no finite differences, no mpmath: the series coefficients are O(1) and carry no cancellation).

Usage:  measmod_fd_sizing.py [--scan] [--check]
Exit:   0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced
"""
from __future__ import annotations

import argparse
import math
import sys

HS = [10.0, 30.0, 100.0, 300.0, 1000.0]

# the geometries and the figures SPEC-measmod.md §6.2 freezes: (name, rho, |r_target|, kind, B_pred, h*)
CASES = [
    ("range, LAGEOS-like", 6.6e6, 1.227e7, "range", 4.42e-9, 86.0),
    ("range, LEO-like", 1.5e6, 7.4e6, "range", 8.55e-8, 25.0),
    ("angle, LEO-like, |phi| <= 40 deg", 1.2e6, 7.4e6, "angle", 4.29e-13, 18.0),
]


def ulp(x: float) -> float:
    return math.ulp(x)


def sizing(rho: float, rt: float, kind: str, phi_max_deg: float = 40.0):
    if kind == "range":
        nu = 3 * ulp(rt)
        f = 1.1547005383792515 / rho**2
    else:
        nu = 3 * (ulp(2 * math.pi) + ulp(rt) / rho)
        f = 2 / (rho**3 * math.cos(math.radians(phi_max_deg)) ** 3)
    t = [h * h * f / 6 for h in HS]
    n = [nu / h for h in HS]
    e = [a + b for a, b in zip(t, n)]
    hstar = (3 * nu / f) ** (1 / 3)
    return nu, f, t, n, e, hstar, hstar**2 * f / 6 + nu / hstar


# ---- the exact series scan (truncated power series in h) ------------------------------------------------------------------------------
ORDER = 4


class S:
    def __init__(self, c):
        self.c = (list(c) + [0.0] * ORDER)[:ORDER]

    def __add__(self, o):
        o = o if isinstance(o, S) else S([o])
        return S([a + b for a, b in zip(self.c, o.c)])

    def __sub__(self, o):
        o = o if isinstance(o, S) else S([o])
        return S([a - b for a, b in zip(self.c, o.c)])

    def __mul__(self, o):
        o = o if isinstance(o, S) else S([o])
        r = [0.0] * ORDER
        for i in range(ORDER):
            for j in range(ORDER - i):
                r[i + j] += self.c[i] * o.c[j]
        return S(r)

    def inv(self):
        r = [0.0] * ORDER
        r[0] = 1.0 / self.c[0]
        for n in range(1, ORDER):
            r[n] = -sum(self.c[k] * r[n - k] for k in range(1, n + 1)) / self.c[0]
        return S(r)

    def __truediv__(self, o):
        return self * (o if isinstance(o, S) else S([o])).inv()

    def sqrt(self):
        r = [0.0] * ORDER
        r[0] = math.sqrt(self.c[0])
        for n in range(1, ORDER):
            r[n] = (self.c[n] - sum(r[k] * r[n - k] for k in range(1, n))) / (2 * r[0])
        return S(r)

    def deriv(self):
        return S([k * self.c[k] for k in range(1, ORDER)])

    def integ(self, c0):
        return S([c0] + [self.c[k] / (k + 1) for k in range(ORDER - 1)])


def atan_series(q: S) -> S:
    return (q.deriv() / (S([1.0]) + q * q)).integ(math.atan(q.c[0]))


def third_derivatives(g, e):
    """f''' (rho = 1) of right ascension and declination for the line of sight g displaced along e (g_x > 0)."""
    x, y, z = S([g[0], e[0]]), S([g[1], e[1]]), S([g[2], e[2]])
    ra = atan_series(y / x)
    dec = atan_series(z / (x * x + y * y).sqrt())
    return 6 * ra.c[3], 6 * dec.c[3]


def scan(phi_deg: float = 30.0):
    best = [0.0, 0.0]
    for dec_deg in (-30, -20, -10, 0, 10, 20, 30):
        dec = math.radians(dec_deg)
        for k in range(12):
            ra = 2 * math.pi * k / 12 + 0.3
            g = (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))
            if g[0] <= 0:
                continue
            for it in range(90):
                th = math.pi * (it + 0.5) / 90
                for jt in range(180):
                    ph = 2 * math.pi * jt / 180
                    e = (math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th))
                    a, d = third_derivatives(g, e)
                    best[0], best[1] = max(best[0], abs(a)), max(best[1], abs(d))
    return best


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--scan", action="store_true", help="the exact series scan of the angle third derivatives (a few seconds)")
    ap.add_argument("--check", action="store_true", help="exit 1 unless every number SPEC-measmod.md §6.2 freezes is reproduced")
    a = ap.parse_args()
    bad = 0
    for name, rho, rt, kind, b_frozen, h_frozen in CASES:
        nu, f, t, n, e, hstar, best = sizing(rho, rt, kind)
        print(f"--- {name}: rho = {rho:.3g} m, F = {f:.4g}, nu = {nu:.3g}")
        for h, ti, ni, ei in zip(HS, t, n, e):
            print(f"    h = {h:6.0f}   T = {ti:.2e}   N = {ni:.2e}   eps = {ei:.2e}")
        print(f"    h* = {hstar:.0f}, best = {best:.1e}, B_pred = {max(e):.3g}, window [{max(e) / 10:.2g}, {max(e) * 10:.2g}]")
        if a.check:
            ok = abs(max(e) / b_frozen - 1) < 0.005 and abs(hstar - h_frozen) < 1.0
            print(f"    frozen B_pred {b_frozen:.3g}, h* {h_frozen:.0f}:", "reproduced" if ok else "NOT REPRODUCED")
            bad += 0 if ok else 1
    eps = 2.0**-52
    print(f"--- if only machine round-off mattered: eps^(2/3) = {eps ** (2 / 3):.2e}")
    if a.scan:
        ra, dec = scan()
        bound = 2 / math.cos(math.radians(30.0)) ** 3
        print(f"--- series scan, |dec| <= 30 deg: max |f'''| RA {ra:.4f}, Dec {dec:.4f} (rho = 1); analytic RA bound 2/cos^3 = {bound:.4f}")
        if a.check:
            ok = ra <= bound * 1.001 and dec <= bound
            print("    the frozen angle coefficient 2/(rho^3 cos^3 phi_max) bounds both:", "yes" if ok else "NO")
            bad += 0 if ok else 1
    if a.check:
        print("ok       every frozen number reproduced" if not bad else f"FAILED   {bad} frozen number(s) not reproduced")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
