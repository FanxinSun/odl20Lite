#!/usr/bin/env python3
"""forcemodel_fd_sizing.py — the finite-difference gate of the force plugins' Jacobians: its sizing, as the numbers it freezes.
SPEC-forcemodel.md §6.2.

WRITTEN 2026-10-06, BEFORE ANY RUN OF THE GATE (plan §4 rule 7), in L6's form (SPEC-measmod §6.2, STM-P-1).  A central difference
of a plugin's acceleration carries two errors,

    truncation   T(h) = h^2 F / 6      F a bound of |d^3 a_i / d x_j^3| over the whole stencil [x - h e_j, x + h e_j]
    noise        N(h) = nu / h         nu a bound of ONE evaluation's round-off, over every point of the stencil

    bound        eps(h) = T(h) + N(h)

F IS A BOUND, NOT AN ESTIMATE, and it is proved from three elementary facts, each used once:

  Lemma 1 (Leibniz).  For a homogeneous polynomial P of degree d with the l1 norm ||P|| of its coefficients, and q > 0,
        | d_e^3 ( P(x) r^-q ) |  <=  ||P|| r^(d-q-3) S3(d, q),      S3(d, q) = sum_{j=0..3} C(3,j) d^(falling 3-j) (q)_j
      because |d_e^k P| <= ||P|| d^(falling k) r^(d-k)  (each monomial is a product of d coordinates, each bounded by r, each
      derivative of a linear factor bounded by 1) and |d_e^j r^-q| <= (q)_j r^(-q-j) (the Gegenbauer function C_j^(q/2) peaks at +-1).
  Lemma 2 (the potential of one normalised coefficient).  V_nm = GM a_e^n N_nm q(x) / r^(2n+1), q the harmonic polynomial
        of GRAV §4.7 / gradient_reference.py, so  a_i = d_i V = GM a_e^n N_nm [ (d_i q) r^-(2n+1) - (2n+1) q x_i r^-(2n+3) ]
      and Lemma 1 applied to the two terms gives
        | d_e^3 a_i |  <=  GM a_e^n Phi_nm r^-(n+5),
        Phi_nm = N_nm max_i [ ||d_i q|| S3(n-1, 2n+1) + (2n+1) ||q|| S3(n+1, 2n+3) ]            (n >= 1)
      and for the point mass (n = 0, q = 1, s = 1)   Phi_0 = S3(1, 3) = 96.
  Lemma 3 (the stencil).  Every exponent above is negative, so the bound is largest at the smallest radius the stencil reaches,
        r_min = r - h_max.

THE LOOSENESS is measured by --scan: the true supremum of |d_e^3 a_i| over directions, positions on the sphere and the stencil, by
exact power series along the line (no finite differences), and F over it is printed; the gate's POWER — the smallest relative defect it
can see — is eps(h) / ||row|| at the best step, printed with it.

Usage:  forcemodel_fd_sizing.py [--scan] [--check]
          (no flag)  print the sizing;  --check  every frozen number reproduced;  --scan  the looseness scan (about a minute)
Exit:   0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced   2 an input file is missing
"""
from __future__ import annotations

import argparse
import math
import sys
from fractions import Fraction
from math import comb
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gradient_reference as gr   # noqa: E402   (the exact polynomial machinery; its tensors are not used here)

ROOT = Path(__file__).resolve().parent.parent
EGM = ROOT / "data" / "cache" / "egm2008-coefficients" / "extracted" / "EGM2008_to2190_TideFree"

EPS = 2.0 ** -52
HS = [10.0, 30.0, 100.0, 300.0, 1000.0]          # position steps, m
HVS = [0.01, 0.1, 1.0, 10.0]                      # velocity steps, m/s
H_MAX = 1000.0

GM = 3.986004415e14          # the field's (EGM2008, TT-compatible)
AE = 6378136.3
C = 299792458.0
GME_REL = 3.986004418e14     # relativity's own (TN36-1)
GMS = 1.32712442099e20
J_EARTH = 9.8e8

# L4's four reference points (tests/l4_ranking.cpp): name, geocentric radius in m
POINTS = [("GPS", 26561e3), ("LEO 300 km", 6378136.3 + 300e3), ("LEO 952.86 km", 6378136.3 + 952.86e3), ("sail 720 km", 6378136.3 + 720e3)]


def falling(d: int, k: int) -> int:
    out = 1
    for i in range(k):
        out *= d - i
    return out


def rising(q: int, k: int) -> int:
    out = 1
    for i in range(k):
        out *= q + i
    return out


def S3(d: int, q: int) -> int:
    """Lemma 1's constant: sum_{j=0..3} C(3,j) d^(falling 3-j) (q)_j."""
    return sum(comb(3, j) * falling(d, 3 - j) * rising(q, j) for j in range(4))


def l1(poly: dict) -> Fraction:
    return sum((abs(v) for v in poly.values()), Fraction(0))


def phi(n: int, m: int, kind: str) -> float:
    """Lemma 2's Phi_nm (n >= 2), exact polynomial algebra, one square root."""
    q = gr.solid_polynomial(n, m, kind)
    if not q:
        return 0.0
    s = 2 * n + 1
    dq = max(l1(gr.p_der(q, a)) for a in range(3))
    inner = dq * S3(n - 1, s) + s * l1(q) * S3(n + 1, s + 2)
    return math.sqrt(float(gr.norm_squared(n, m))) * float(inner)


PHI_POINT_MASS = float(S3(1, 3))          # 96


def coefficients(nmax: int):
    """EGM2008's tide-free C, S for 2 <= n <= nmax (the plugin's conventional values differ in five of them by 1e-5 relative or less)."""
    out = {}
    if not EGM.exists():
        return None
    for line in EGM.open():
        f = line.split()
        n, m = int(f[0]), int(f[1])
        if n > nmax:
            break
        out[(n, m)] = (float(f[2].replace("D", "E")), float(f[3].replace("D", "E")))
    return out


def f_gravity(nmax: int, r_min: float, coefs) -> float:
    """The registered bound for Gravity(N = nmax): the point mass and every coefficient 2 <= n <= nmax, times 1.01 (the conventional
    substitutions' difference from the file)."""
    total = GM * PHI_POINT_MASS / r_min ** 5
    for (n, m), (c, s) in coefs.items():
        total += GM * AE ** n * (abs(c) * phi(n, m, "C") + abs(s) * (phi(n, m, "S") if m > 0 else 0.0)) / r_min ** (n + 5)
    return 1.01 * total


def f_third_body(mu: float, d_min: float) -> float:
    return mu * PHI_POINT_MASS / d_min ** 5


def f_schwarzschild(r_min: float, v: float) -> float:
    """a_S = (GM/c^2) [ 4 GM x_i r^-4 - v^2 x_i r^-3 + 4 (x.v) v_i r^-3 ],  beta = gamma = 1."""
    k = GME_REL / C ** 2
    t1 = 4.0 * GME_REL * k * S3(1, 4) / r_min ** 6
    t2 = k * v * v * S3(1, 3) / r_min ** 5
    t3 = 4.0 * k * v * (math.sqrt(3.0) * v) * S3(1, 3) / r_min ** 5
    return t1 + t2 + t3


def f_lense_thirring(r_min: float, v: float) -> float:
    """a_LT = (1+gamma)(GM/c^2) [ 3 (x.J)(x x v)_i r^-5 + (v x J)_i r^-3 ], J along z."""
    k = 2.0 * GME_REL / C ** 2
    p1 = 3.0 * J_EARTH * (math.sqrt(3.0) * v)          # ||(x.J)(x x v)_i||_1 <= |J| * ||z (x x v)_i||_1 <= |J| sqrt(3)|v| (z and two terms)
    return k * (p1 * S3(2, 5) / r_min ** 6 + J_EARTH * v * S3(0, 3) / r_min ** 6)


# ---- the registered noise bounds (formulas; the test measures every stencil point against an extended-precision evaluation and asserts) ----
def nu_gravity(a_norm: float) -> float:
    return 64.0 * EPS * a_norm


def nu_third_body(mu: float, d: float, s: float) -> float:
    return 16.0 * EPS * mu * (1.0 / d ** 2 + 1.0 / s ** 2)


def nu_relativity(a_norm: float) -> float:
    return 32.0 * EPS * a_norm


def nu_tides(a_norm: float) -> float:
    return 128.0 * EPS * a_norm


def eps_h(f: float, nu: float, h: float) -> float:
    return h * h * f / 6.0 + nu / h


# the Earth-rotation angle (ERA) at the four reference epochs, from the IERS definition 2 pi (0.7790572732640 + 1.00273781191135448 (JD_UT1 - 2451545)),
# |UT1 - UTC| < 0.9 s changes it by under 0.004 deg: the epochs of tests/l4_ranking.cpp (GPS 2023-02-20 00h; LEO 2023-06-21 08h; sail 2019-07-01 00h)
ERA_DEG = {"GPS": 149.377, "LEO 300 km": 28.965, "LEO 952.86 km": 28.965, "sail 720 km": 278.513}
PHASE = {"GPS": 0.7, "LEO 300 km": 0.0, "LEO 952.86 km": 0.0, "sail 720 km": 1.2}     # the fixture's orbit phase, rad


def control_power():
    """The point-mass tensor's three wrong rows against eps at the best step, per geometry.  A prediction from the GCRS-ITRS rotation taken as R3(ERA)
    (the true chain adds precession, nutation and polar motion: 0.1-0.4 deg, which move these numbers by under one percent)."""
    out = []
    coefs = coefficients(4)
    for name, r in POINTS:
        mu_r3 = GM / r ** 3
        th = math.radians(ERA_DEG[name])
        ph = PHASE[name]
        ug = (math.cos(ph), math.sin(ph), 0.0)                    # unit vector, GCRS
        def rotz(a, v):
            return (math.cos(a) * v[0] - math.sin(a) * v[1], math.sin(a) * v[0] + math.cos(a) * v[1], v[2])
        def tensor(u):
            return [[mu_r3 * (3.0 * u[i] * u[j] - (1.0 if i == j else 0.0)) for j in range(3)] for i in range(3)]
        g_right = tensor(ug)
        ui = rotz(-th, ug)                                         # GCRS -> ITRS is R3(+ERA): the vector's ITRS direction is its GCRS angle minus ERA
        g_itrs = tensor(ui)
        g_unrot = g_itrs                                           # the ITRS tensor used as if it were GCRS
        uw = rotz(-2.0 * th, ug)                                   # the transposed rotation applied to the point-mass tensor: direction rotated by 2 ERA
        g_transposed = tensor(uw)
        g_sign = [[-x for x in row] for row in g_right]
        f = f_gravity(4, r - H_MAX, coefs)
        nu = nu_gravity(GM / r ** 2)
        best = min(eps_h(f, nu, h) for h in HS)
        row = {}
        for label, g in (("unrotated", g_unrot), ("transposed", g_transposed), ("sign flipped", g_sign)):
            row[label] = max(abs(g[i][j] - g_right[i][j]) for i in range(3) for j in range(3)) / best
        out.append((name, th, row))
    return out


def table():
    coefs = coefficients(4)
    if coefs is None:
        raise FileNotFoundError(str(EGM))
    rows = []
    for name, r in POINTS:
        v = math.sqrt(GM / r)
        a = GM / r ** 2
        rmin = r - H_MAX
        f = f_gravity(4, rmin, coefs)
        nu = nu_gravity(a)
        e = [eps_h(f, nu, h) for h in HS]
        g_scale = 2.0 * GM / r ** 3      # the largest eigenvalue of the point-mass tensor
        rows.append((name, r, f, nu, e, min(e) / g_scale))
    return rows


# ---- the looseness scan: the TRUE supremum of |d_e^3 a_i| by exact power series along the line ----
ORDER = 4


class Ser:
    def __init__(self, c):
        self.c = (list(c) + [0.0] * ORDER)[:ORDER]

    def __add__(self, o):
        o = o if isinstance(o, Ser) else Ser([o])
        return Ser([a + b for a, b in zip(self.c, o.c)])

    def __mul__(self, o):
        o = o if isinstance(o, Ser) else Ser([o])
        r = [0.0] * ORDER
        for i in range(ORDER):
            for j in range(ORDER - i):
                r[i + j] += self.c[i] * o.c[j]
        return Ser(r)

    __rmul__ = __mul__


def power_series(g: Ser, alpha: float) -> Ser:
    """(g(t))^alpha, g(0) > 0, by the standard recurrence."""
    f = [0.0] * ORDER
    f[0] = g.c[0] ** alpha
    for k in range(1, ORDER):
        s = 0.0
        for j in range(1, k + 1):
            s += (alpha * j - (k - j)) * g.c[j] * f[k - j]
        f[k] = s / (k * g.c[0])
    return Ser(f)


def poly_on_line(poly: dict, x0, e) -> Ser:
    coords = [Ser([x0[k], e[k]]) for k in range(3)]
    total = Ser([0.0])
    for (i, j, k), c in poly.items():
        term = Ser([float(c)])
        for axis, p in enumerate((i, j, k)):
            for _ in range(p):
                term = term * coords[axis]
        total = total + term
    return total


def third_derivative_a(terms, x0, e, i) -> float:
    """d_e^3 a_i at x0, for the field sum of terms [(coef, n, q_poly, norm)]."""
    r2 = Ser([x0[0] ** 2 + x0[1] ** 2 + x0[2] ** 2, 2.0 * sum(x0[k] * e[k] for k in range(3)), 1.0])
    total = 0.0
    xi = Ser([x0[i], e[i]])
    for coef, n, q, dq, s in terms:
        qs = poly_on_line(q, x0, e)
        dqs = poly_on_line(dq[i], x0, e)
        a = dqs * power_series(r2, -s / 2.0) + (-s) * (qs * xi * power_series(r2, -(s + 2) / 2.0))
        total += coef * 6.0 * a.c[3]
    return total


def scan(nmax: int, r: float, coefs, grid_lat=5, grid_lon=6, grid_dir=6):
    """The supremum of the TRUE |d_e^3 a_i| of the field (point mass included) over a grid of positions on the sphere of radius r,
    directions e, components i, and the three stencil points (-h, 0, +h) at h = 1000 m, in the field's own units."""
    terms = []
    ae = AE
    # point mass: V = GM / r  <->  q = 1, s = 1
    terms.append((GM, 0, {(0, 0, 0): Fraction(1)}, [{}, {}, {}], 1))
    for (n, m), (c, s_) in coefs.items():
        for kind, val in (("C", c), ("S", s_)):
            if val == 0.0 or (kind == "S" and m == 0):
                continue
            q = gr.solid_polynomial(n, m, kind)
            dq = [gr.p_der(q, a) for a in range(3)]
            norm = math.sqrt(float(gr.norm_squared(n, m)))
            terms.append((GM * ae ** n * norm * val, n, q, dq, 2 * n + 1))
    best = 0.0
    for ia in range(grid_lat):
        lat = (-math.pi / 2) + math.pi * (ia + 0.5) / grid_lat
        for ib in range(grid_lon):
            lon = 2 * math.pi * (ib + 0.5) / grid_lon
            x = (r * math.cos(lat) * math.cos(lon), r * math.cos(lat) * math.sin(lon), r * math.sin(lat))
            for ic in range(grid_dir):
                th = math.pi * (ic + 0.5) / grid_dir
                for idr in range(2 * grid_dir):
                    ph = math.pi * (idr + 0.5) / grid_dir
                    e = (math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th))
                    for step in (-H_MAX, 0.0, H_MAX):
                        x0 = tuple(x[k] + step * e[k] for k in range(3))
                        for i in range(3):
                            best = max(best, abs(third_derivative_a(terms, x0, e, i)))
    return best


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="forcemodel_fd_sizing.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--scan", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)

    bad = []

    def expect(name, got, want, tol=0.0):
        ok = abs(got - want) <= tol * abs(want) if tol else got == want
        print(("ok       " if ok else "MISMATCH ") + f"{name}: {got!r}" + ("" if ok else f"  (frozen {want!r})"))
        if not ok:
            bad.append(name)

    # Lemma 1's constants, by hand: S3(1,3) = 3*1*12 + 60 = 96;  S3(1,4) = 3*1*20 + 120 = 180;  S3(2,5) = 6*5 + 6*30 + 210 = 420;
    # S3(0,3) = (3)_3 = 60;  S3(3,7) = 6 + 126 + 504 + 504 = 1140;  S3(1,5) = 90 + 210 = 300
    for (d, q), want in (((1, 3), 96), ((1, 4), 180), ((2, 5), 420), ((0, 3), 60), ((3, 7), 1140), ((1, 5), 300)):
        expect(f"S3({d},{q})", S3(d, q), want)
    # Phi_20 = sqrt(5) (2 * 300 + 10 * 1140), derived by hand from q = (2z^2 - x^2 - y^2)/2, ||q|| = 2, max_i ||d_i q|| = 2
    expect("Phi_20", phi(2, 0, "C"), math.sqrt(5.0) * 12000.0, 1e-12)
    expect("Phi_point_mass", PHI_POINT_MASS, 96.0)

    rows = table()
    print("\nGravity(N = 4): rigorous F at r_min = r - 1000 m, the registered nu = 64 eps |a|, eps(h), and the best relative defect it can see")
    print(f"  {'point':16s} {'r (m)':>12s} {'F (m^-2 s^-2)':>16s} {'nu (m s^-2)':>13s} " + " ".join(f"eps({h:g}m)".rjust(11) for h in HS) + "   min eps / |G|")
    for name, r, f, nu, e, rel in rows:
        print(f"  {name:16s} {r:12.1f} {f:16.4e} {nu:13.3e} " + " ".join(f"{x:11.3e}" for x in e) + f"   {rel:.2e}")
    print("\nGravity(N = 4): the three wrong rows' predicted power, max |wrong - right| / (eps at the best step); the registered requirement is >= 1e3")
    for name, th, row in control_power():
        print(f"  {name:16s} ERA {math.degrees(th):8.3f} deg  |sin ERA| = {abs(math.sin(th)):.3f}  " + "  ".join(f"{k}: {v:.2e}" for k, v in row.items()))
        for k, v in row.items():
            if v < 1e3:
                bad.append(f"control {k} at {name} has predicted power {v:.2e} < 1e3")
    print("\nThird body (mu / d_min^5 x 96), the Sun and the Moon at a LEO point:")
    for body, mu, s in (("Sun", GMS, 1.4959787e11), ("Moon", 4.9028e12, 3.844e8)):
        f = f_third_body(mu, s - 7.0e6 - H_MAX)
        nu = nu_third_body(mu, s, s)
        print(f"  {body:5s} F = {f:.3e}  nu = {nu:.3e}  |G| ~ {2 * mu / s ** 3:.3e}  best eps = {min(eps_h(f, nu, h) for h in HS):.3e}  relative {min(eps_h(f, nu, h) for h in HS) / (2 * mu / s ** 3):.2e}")
    print("\nRelativity at the LEO 300 km point (v = sqrt(GM/r)):")
    r = POINTS[1][1]
    v = math.sqrt(GM / r)
    fs, fl = f_schwarzschild(r - H_MAX, v), f_lense_thirring(r - H_MAX, v)
    a_s = 3.0 * GME_REL ** 2 / (C ** 2 * r ** 3)
    a_l = 2.0 * GME_REL * J_EARTH * v / (C ** 2 * r ** 3)
    print(f"  Schwarzschild  F = {fs:.3e}  nu = {nu_relativity(a_s):.3e}   Lense-Thirring  F = {fl:.3e}  nu = {nu_relativity(a_l):.3e}   de Sitter  F = 0 (d a / d r = 0 identically)")

    if args.scan:
        coefs = coefficients(4)
        print("\nLooseness, the TRUE supremum of |d_e^3 a_i| against F (point mass + degrees 2..4 of EGM2008; coarse grid, exact series):")
        for name, r in POINTS:
            true = scan(4, r, coefs)
            f = f_gravity(4, r - H_MAX, coefs)
            print(f"  {name:16s} true sup {true:.3e}   F {f:.3e}   F / true sup = {f / true:.1f}")
    if args.check:
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
