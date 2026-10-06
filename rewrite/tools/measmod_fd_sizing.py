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

AMENDED 2026-10-06, AFTER THE GATE'S FIRST RUN MISSED ITS PER-SIZE BOUND (SPEC-measmod.md §6.2, "Amendment"; plan/subplan_L6/L6-4.md,
the manager's ruling).  The text above is the sizing as it was frozen and run once; it is kept as it was.  The cause of the miss was in the sizing,
and the amendment changes the sizing only, by two first-principles terms it left out (the section "THE AMENDED RANGE SIZING" below):
  (i)  nu gains the Earth-rotation angle's double-precision floor of the real orientation chain (3 x 2^-45 rad x the station's axis distance, carried
       through the range as it enters for each epoch event) — in the real-chain configuration only;
  (ii) F becomes a bound over the stencil [x-h, x+h] for the whole modelled range: geometry (with the light-time coupling), troposphere through the
       mapping function's derivatives, and the Shapiro term.

Usage:  measmod_fd_sizing.py [--scan] [--check]
          (no flag)  print the frozen sizing and the amended one;   --check  every frozen number reproduced (fast);
          --scan     the exact angle-series scan AND the amended range scans (the stencil bounds, about ten seconds)
Exit:   0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced   2 an input file is missing
"""
from __future__ import annotations

import argparse
import math
import sys

HS = [10.0, 30.0, 100.0, 300.0, 1000.0]

# the geometries and the figures SPEC-measmod.md §6.2 freezes: (name, rho, |r_target|, kind, B_pred, h*)
CASES = [
    ("range, LAGEOS-like", 6.6e6, 1.227e7, "range", 4.42e-9, 86.0),
    ("range, LEO-like", 1.5e6, 6.92e6, "range", 8.55e-8, 25.0),                     # target at 6.92e6 m: elevation 15 deg at rho 1.5e6 m
    ("angle, LEO-like, |phi| <= 40 deg", 1.2e6, 7.2e6, "angle", 4.29e-13, 18.0),    # target at 7.2e6 m: elevation 40 deg at rho 1.2e6 m
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


# =====================================================================================================================================
# THE AMENDED RANGE SIZING — 2026-10-06, MADE AFTER THE FIRST RUN OF THE GATE MISSED CRITERION (b) (post hoc, and labelled so)
# =====================================================================================================================================
# The first run missed (b) in 7 of 8 cases because the sizing above left out two terms, each derivable from first principles and neither
# read off the measurement:
#   (i)  the real orientation chain's Earth-rotation angle is formed unreduced (2 pi (f + 0.779... + 0.0027378 t), about 171 rad for 2019-2039), so
#        its last bit is 2^-45 rad: at the station's distance from the Earth's axis a position noise of 2^-45 rad x R_perp = 1.59e-7 m per evaluation.
#        The frozen sizing's own convention is three ulp: nu_chain = 3 x 2^-45 x R_perp, carried through the range as it enters — in event 2 the
#        up leg's station epoch is the tag, the same in every evaluation, so only the down leg enters (weight 1/2 of the range); in event 1 both legs
#        move (weights 1/2 + 1/2) — times |g . east|, the cosine between the line of sight and the direction of the error.
#   (ii) F is a bound over the whole stencil [x-h, x+h], for the whole modelled range: a central difference's error is h^2/6 f'''(xi) at some xi
#        inside it, so (b) is a bound only if F bounds |f'''| everywhere in it:
#            F(h) = F_geo(h) + 1.01 F_trop(h)
#            F_geo(h) = 1.1547005 (1 + kappa)^3 / (rho (1 - kappa) - h)^2,  kappa = (v_target + 2 v_station,max) / c
#                       the analytic maximum of 3 mu (1 - mu^2) over the smallest distance of the stencil, with the light-time coupling's v/c
#                       (VERIFIED below against the closed-form two-way light time in 60 digits over the cone of directions where the maximum lives)
#            F_trop(h)  the supremum over every unit displacement direction and every point of the stencil of |d^3/dxi^3| of the leg's
#                       zenith delay x FCULa mapping function plus Shapiro term, by third differences of the model's own formulas (an independent
#                       implementation: TN36 equations 9.3-9.10 and 11.17), the 1.01 being the allowance for the grid's discretisation.
# The geometries, the azimuth rule, the sizes, the velocity directions and the criteria (a), (b), (c) are the frozen ones.

C_LIGHT = 299792458.0
U_ERA = 2.0 ** -45                                  # rad: one ulp of an Earth-rotation angle between 128 and 256 rad (2019-2039)
V_STATION_MAX = 7.2921150e-5 * 6378137.0            # m/s: the equatorial surface speed, an upper bound of any station's
K_SHAPIRO = 2.0 * 3.986004415e14 / C_LIGHT**2       # m: 2 GM/c^2 (TN36 eq. 11.17, GM = 3.986004415e14 m^3/s^2)
R_STATION = 6.373e6                                 # m: the station's geocentric distance, used only for the size of the Shapiro term
# the first normal point's surface weather (record 20 of lageos1_202601.np2) and the transmit wavelength of its configuration
P_HPA, T_KELVIN, RH_PERCENT, LAMBDA_UM = 976.70, 310.30, 12.0, 0.532

# name, rho, elevation (deg), the azimuth the frozen geometry rule selects (deg), the target's speed (m/s), the frozen noise bound nu (m)
AMENDED_CASES = [
    ("LAGEOS-like", 6.6e6, 52.0, 105.0, 5700.0, 3 * math.ulp(1.227e7)),
    ("LEO-like", 1.5e6, 15.0, 0.0, 7500.0, 3 * math.ulp(6.92e6)),
]
XIS = [0.0, 10.0, 30.0, 100.0, 300.0, 1000.0]       # the stencil's points (and their mirror images): the sizes of the gate

# F_trop(h), m^-2: the scan's result written down (the section's own `--scan` recomputes it and `--check` compares), by case
FROZEN_F_TROP = {
    "LAGEOS-like": {10.0: 9.6904e-20, 30.0: 9.6905e-20, 100.0: 9.6910e-20, 300.0: 9.6924e-20, 1000.0: 9.6973e-20},
    "LEO-like": {10.0: 7.7299e-16, 30.0: 7.7314e-16, 100.0: 7.7365e-16, 300.0: 7.7512e-16, 1000.0: 7.8028e-16},
}
# THE AMENDED NUMBERS, frozen 2026-10-06 BEFORE THE GATE'S SECOND RUN (the section's own output, 4 significant digits): the chain's bound of one evaluation of the
# range output for each epoch event, eps(h) for the rigid configuration (nu as first frozen) and for the real chain (nu + nu_chain) in each event, and B_pred = max eps
AMENDED_FROZEN = {
    "LAGEOS-like": {
        "nu_chain": {2: 3.7910e-08, 1: 7.5821e-08},
        "eps": {"rigid": [5.5924e-10, 1.9024e-10, 1.0007e-10, 4.1633e-10, 4.4255e-09],
                "real2": [4.3503e-09, 1.4539e-09, 4.7917e-10, 5.4270e-10, 4.4634e-09],
                "real1": [8.1413e-09, 2.7176e-09, 8.5827e-10, 6.6907e-10, 4.5013e-09]},
        "b_pred": {"rigid": 4.4255e-09, "real2": 4.4634e-09, "real1": 8.1413e-09},
    },
    "LEO-like": {
        "nu_chain": {2: 2.2981e-07, 1: 4.5961e-07},
        "eps": {"rigid": [2.8796e-10, 1.7024e-10, 8.8481e-10, 7.7232e-09, 8.5794e-08],
                "real2": [2.3269e-08, 7.8305e-09, 3.1829e-09, 8.4892e-09, 8.6024e-08],
                "real1": [4.6249e-08, 1.5491e-08, 5.4809e-09, 9.2553e-09, 8.6253e-08]},
        "b_pred": {"rigid": 8.5794e-08, "real2": 8.6024e-08, "real1": 8.6253e-08},
    },
}


def fcula(s, cphi, height, t_c, num=float):
    """TN36 Table 9.1 / eq. (9.9), as printed; `num` is float or Decimal."""
    n = num
    a1 = n("12100.8e-7") + n("1729.5e-9") * t_c + n("319.1e-7") * cphi - n("1847.8e-11") * height
    a2 = n("30496.5e-7") + n("234.6e-8") * t_c - n("103.5e-6") * cphi - n("185.6e-10") * height
    a3 = n("6877.7e-5") + n("197.2e-7") * t_c - n("345.8e-5") * cphi + n("106.0e-9") * height
    return (1 + a1 / (1 + a2 / (1 + a3))) / (s + a1 / (s + a2 / (s + a3)))


def zenith_total(lat_rad, height, ps, es, lam):
    """TN36 eqs (9.3)-(9.7) in double precision (the sizing needs it to a part in 10^3 only)."""
    fs = 1 - 0.00266 * math.cos(2 * lat_rad) - 0.00000028 * height
    sig2 = (1 / lam) ** 2
    fh = 1e-2 * (19990.975 * (238.0185 + sig2) / (238.0185 - sig2) ** 2 + 579.55174 * (57.362 + sig2) / (57.362 - sig2) ** 2) * (1 + 0.534e-6 * (375 - 450))
    fnh = 0.003101 * (295.235 + 3 * 2.6422 * sig2 + 5 * -0.032380 * sig2**2 + 7 * 0.004028 * sig2**3)
    return 0.002416579 * fh / fs * ps + 1e-4 * (5.316 * fnh - 3.759 * fh) * es / fs


def station_inputs():
    """The station's axis distance, geodetic latitude and ellipsoidal height of the first normal point, from the independent registry reference."""
    import measmod_reference as mr
    r = mr.registry_section()
    return math.hypot(r["yarl_srp_x_m"], r["yarl_srp_y_m"]), r["yarl_srp_lat_rad"], r["yarl_srp_h_m"]


def atmosphere_inputs():
    r_perp, lat, height = station_inputs()
    t_c = T_KELVIN - 273.15
    e = (RH_PERCENT / 100.0) * 6.11 * 10.0 ** (7.5 * t_c / (237.3 + t_c))
    return r_perp, lat, height, t_c, zenith_total(lat, height, P_HPA, e, LAMBDA_UM)


def iers_fcula_check() -> float:
    """The Decimal FCULa at the IERS FCUL_A prolog's printed case (lat 30.67166667 deg, H 2075 m, T 300.15 K, elevation 15 deg): the printed 3.800243667312344087."""
    from decimal import Decimal
    import measmod_reference as mr
    lat = Decimal("30.67166667") * mr.PI / 180
    sin15 = (Decimal(6).sqrt() - Decimal(2).sqrt()) / 4
    return float(fcula(sin15, mr.d_cos(lat), Decimal(2075), Decimal("300.15") - Decimal("273.15"), Decimal) - Decimal("3.800243667312344087"))


def f_geo(h, rho, speed):
    kappa = (speed + 2 * V_STATION_MAX) / C_LIGHT
    return 1.1547005383792515 * (1 + kappa) ** 3 / (rho * (1 - kappa) - h) ** 2


def nu_chain(event, r_perp, el_deg, az_deg):
    """The chain's bound of one evaluation of the range output: three ulp of the angle at the axis distance, times the cosine between the line of sight and the error's
    direction (east), times 1/2 for the one leg that moves in event 2 and 1 for the two legs that move in event 1."""
    g_east = abs(math.cos(math.radians(el_deg)) * math.cos(math.radians(az_deg)))
    return 3 * U_ERA * r_perp * g_east * (0.5 if event == 2 else 1.0), g_east


def eps_amended(h, f_trop, rho, speed, nu):
    return h * h * (f_geo(h, rho, speed) + 1.01 * f_trop[h]) / 6 + nu / h


# ---- the stencil bound of the troposphere and Shapiro terms: third differences of the model's own formulas, over every direction ----------------------------------
def _norm(v):
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def trop_shapiro_scan(rho, el_deg, atm, step_deg=(3.0, 4.0), delta=2000.0):
    """sup over unit displacement directions and the stencil's points of |d3/dxi3| [ZTD m(sin e) + Shapiro] along the displacement, for one leg whose line of sight
    makes `el_deg` with the horizon at slant range `rho`. Returns (F_trop for each h, the Shapiro term's own sup, the atmosphere's own sup)."""
    _, lat, height, t_c, z = atm
    cphi = math.cos(lat)
    el = math.radians(el_deg)
    p0 = (rho * math.cos(el), 0.0, rho * math.sin(el))                   # target relative to the station, local frame (up = z)

    def total(p, with_atm, with_shapiro):
        r = _norm(p)
        out = 0.0
        if with_atm:
            out += z * fcula(p[2] / r, cphi, height, t_c)
        if with_shapiro:
            r2 = _norm((p[0], p[1], p[2] + R_STATION))
            out += K_SHAPIRO * math.log1p(2 * r / (R_STATION + r2 - r))
        return out

    def third(e, xi, which):
        f = []
        for k in (-2, -1, 1, 2):
            t = xi + k * delta
            f.append(total((p0[0] + t * e[0], p0[1] + t * e[1], p0[2] + t * e[2]), *which))
        return (f[3] - 2 * f[2] + 2 * f[1] - f[0]) / (2 * delta**3)

    sup = {h: 0.0 for h in HS}
    sup_s = 0.0
    sup_a = 0.0
    n_th, n_ph = int(180 / step_deg[0]) + 1, int(360 / step_deg[1])
    for it in range(n_th):
        th = math.radians(step_deg[0] * it)
        for jp in range(n_ph):
            ph = math.radians(step_deg[1] * jp)
            e = (math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th))
            for sign in (1.0, -1.0):
                for xi_abs in XIS:
                    xi = sign * xi_abs
                    v = abs(third(e, xi, (True, True)))
                    for h in HS:
                        if xi_abs <= h:
                            sup[h] = max(sup[h], v)
                    if xi_abs == 1000.0 or xi_abs == 0.0:
                        sup_s = max(sup_s, abs(third(e, xi, (False, True))))
                        sup_a = max(sup_a, abs(third(e, xi, (True, False))))
    return sup, sup_s, sup_a


# ---- the closed-form two-way light time with both bodies in uniform motion (60 digits): the geometry and the coupling, over the cone where the maximum lives -------------
def coupling_scan(rho, el_deg, az_deg, speed, radial, event, delta=100.0):
    """sup over the cone of displacement directions (mu = e.g in 0.45 ... 0.70, 24 azimuths) and the stencil's points of the third derivative of the closed-form
    two-way range, for each h."""
    from decimal import Decimal
    import measmod_reference as mr
    el, az = math.radians(el_deg), math.radians(az_deg)
    g = (math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))
    up = (0.0, 0.0, 1.0)
    across = (g[1] * up[2] - g[2] * up[1], g[2] * up[0] - g[0] * up[2], g[0] * up[1] - g[1] * up[0])
    n = _norm(across)
    across = tuple(x / n for x in across)
    vt = tuple(speed * (math.sqrt(1.0 - radial * radial) * across[i] + radial * g[i]) for i in range(3))
    s0, vs = (0.0, 0.0, 0.0), (V_STATION_MAX, 0.0, 0.0)
    r0 = tuple(Decimal(rho * g[i]) for i in range(3))
    a_ax = tuple((up[i] - g[2] * g[i]) for i in range(3))                # up minus its component along g
    na = _norm(a_ax)
    a_ax = tuple(x / na for x in a_ax)
    b_ax = (g[1] * a_ax[2] - g[2] * a_ax[1], g[2] * a_ax[0] - g[0] * a_ax[2], g[0] * a_ax[1] - g[1] * a_ax[0])
    c = Decimal(299792458)
    dl = Decimal(delta)

    def y(r):
        tu, td = mr.lt_closed_form(event, s0, vs, r, vt)
        return c * (tu + td) / 2

    sup = {h: 0.0 for h in HS}
    for mu in (0.45, 0.50, 0.55, 1 / math.sqrt(3), 0.60, 0.65, 0.70):
        sn = math.sqrt(1 - mu * mu)
        for k in range(24):
            ph = math.radians(15.0 * k)
            e = tuple(mu * g[i] + sn * (math.cos(ph) * a_ax[i] + math.sin(ph) * b_ax[i]) for i in range(3))
            ed = tuple(Decimal(x) for x in e)
            for sign in (1.0, -1.0):
                for xi_abs in XIS:
                    xi = Decimal(sign * xi_abs)
                    ys = []
                    for kk in (-2, -1, 1, 2):
                        t = xi + kk * dl
                        ys.append(y(tuple(r0[i] + t * ed[i] for i in range(3))))
                    d3 = abs(float((ys[3] - 2 * ys[2] + 2 * ys[1] - ys[0]) / (2 * dl**3)))
                    for h in HS:
                        if xi_abs <= h:
                            sup[h] = max(sup[h], d3)
    return sup


def amended(scan: bool, check: bool) -> int:
    """Print the amended sizing; with `scan` recompute the stencil bounds (slow) and verify the closed form bounds the coupled geometry; with `check` fail on a miss."""
    bad = 0
    r_perp, lat, height, t_c, z = atmosphere_inputs()
    floor = U_ERA * r_perp
    print("\n=== THE AMENDED RANGE SIZING (2026-10-06, made after the first run's miss) ===")
    print(f"the chain's floor: 2^-45 rad = {U_ERA:.4e} rad, axis distance {r_perp:.1f} m, one ulp = {floor:.4e} m, three ulp = {3 * floor:.4e} m;  ZTD = {z:.6f} m, lat {math.degrees(lat):.4f} deg, H {height:.1f} m, t = {t_c:.2f} C")
    if check:
        d = iers_fcula_check()
        ok = abs(d) < 1e-12
        print(f"the Decimal FCULa at the IERS FCUL_A printed case differs from the printed 3.800243667312344087 by {d:.2e}:", "ok" if ok else "NOT REPRODUCED")
        bad += 0 if ok else 1
        ok = abs(floor / 1.59e-7 - 1) < 0.01
        print(f"one ulp of the chain's angle at the station = {floor:.3e} m against the 1.59e-7 m predicted before it was measured:", "ok" if ok else "NOT REPRODUCED")
        bad += 0 if ok else 1
    for name, rho, el, az, speed, nu in AMENDED_CASES:
        if scan:
            f_trop, f_shap, f_atm = trop_shapiro_scan(rho, el, (r_perp, lat, height, t_c, z))
        else:
            f_trop = FROZEN_F_TROP[name]
            f_shap = f_atm = float("nan")
        print(f"--- {name}: rho = {rho:.3g} m, elevation {el:.0f} deg, azimuth {az:.0f} deg, speed {speed:.0f} m/s, nu(frozen) = {nu:.4g} m")
        if scan:
            print(f"    Shapiro alone: sup |d3| = {f_shap:.3e} m^-2 = {f_shap / f_geo(0.0, rho, speed):.2e} of F_geo;  atmosphere alone: {f_atm:.3e} = {f_atm / f_geo(0.0, rho, speed):.2e} of F_geo")
        print("    h        F_geo        F_trop(sup)   F(h)=F_geo+1.01 F_trop   eps_rigid(h)      eps_real event 2      eps_real event 1")
        nu2, ge = nu_chain(2, r_perp, el, az)
        nu1, _ = nu_chain(1, r_perp, el, az)
        for h in HS:
            fg = f_geo(h, rho, speed)
            ft = f_trop[h]
            print(f"    {h:6.0f}   {fg:.5e}  {ft:.4e}   {fg + 1.01 * ft:.5e}   {eps_amended(h, f_trop, rho, speed, nu):.4e}   {eps_amended(h, f_trop, rho, speed, nu + nu2):.4e}        {eps_amended(h, f_trop, rho, speed, nu + nu1):.4e}")
        for label, nu_extra in (("rigid", 0.0), ("real, event 2", nu2), ("real, event 1", nu1)):
            b = max(eps_amended(h, f_trop, rho, speed, nu + nu_extra) for h in HS)
            print(f"    {label:14s}: nu = {nu + nu_extra:.4e} m   B_pred = {b:.4e}   window [{b / 10:.3e}, {b * 10:.3e}]")
        print(f"    |g . east| = {ge:.4f}: nu_chain event 2 = {nu2:.4e} m, event 1 = {nu1:.4e} m")
        if scan:
            for radial, rname in ((0.0, "across"), (1.0, "along")):
                for event in (2, 1):
                    sup = coupling_scan(rho, el, az, speed, radial, event)
                    worst = max(sup[h] / f_geo(h, rho, speed) for h in HS)
                    ok = worst <= 1.0
                    print(f"    closed-form two-way light time, {rname:6s} event {event}: max over the cone of |y'''| / F_geo(h) = {worst:.6f}  (the formula bounds the coupled geometry: {'yes' if ok else 'NO'})")
                    bad += 0 if ok else 1
        if check:
            lit = FROZEN_F_TROP[name]
            ok = all(abs(f_trop[h] / lit[h] - 1) < 0.02 for h in HS)
            print("    frozen F_trop reproduced:" if scan else "    frozen F_trop in use:", "ok" if ok else "NOT REPRODUCED")
            bad += 0 if ok else 1
            fz = AMENDED_FROZEN[name]
            got = {"rigid": [eps_amended(h, lit, rho, speed, nu) for h in HS],
                   "real2": [eps_amended(h, lit, rho, speed, nu + nu2) for h in HS],
                   "real1": [eps_amended(h, lit, rho, speed, nu + nu1) for h in HS]}
            ok = all(abs(a / b - 1) < 0.002 for k in got for a, b in zip(got[k], fz["eps"][k]))
            ok = ok and abs(nu2 / fz["nu_chain"][2] - 1) < 0.002 and abs(nu1 / fz["nu_chain"][1] - 1) < 0.002
            ok = ok and all(abs(max(got[k]) / fz["b_pred"][k] - 1) < 0.002 for k in got)
            print("    the frozen amended eps(h), nu_chain and B_pred reproduced:", "ok" if ok else "NOT REPRODUCED")
            bad += 0 if ok else 1
    return bad


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
    try:
        bad += amended(a.scan, a.check)
    except FileNotFoundError as err:
        print(f"missing input: {err}", file=sys.stderr)
        return 2
    if a.check:
        print("ok       every frozen number reproduced" if not bad else f"FAILED   {bad} frozen number(s) not reproduced")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
