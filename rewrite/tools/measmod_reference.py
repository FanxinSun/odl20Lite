#!/usr/bin/env python3
"""measmod_reference.py — reference values for SPEC-measmod.md's acceptance tests, from the DEFINITION.

Each section computes what a measmod test compares against, by a route that does NOT go through the code
under test (the form of tools/legendre_reference.py): the definition evaluated independently, in
standard-library arithmetic only, and written into a header the tests include.  `--check` regenerates the
header and fails if the committed one differs, so a reference value cannot drift away from its generator.

Sections (each added by the step that needs it; the spec names the row that uses it):
  registry   MEAS-A-010   Yarragadee (SOD 70900513) at the first normal point of the real January 2026 LAGEOS-1 file:
                          the marker from x_ref + v (t - t_ref), the system reference point from the eccentricity, and the
                          geodetic coordinates of both by an ITERATIVE geodetic solution (not ERFA's closed form).
  shapiro    MEAS-A-025/026  the Shapiro term 2GM/c^2 ln[(r1+r2+rho)/(r1+r2-rho)] and its three partials, in 60-digit
                          arithmetic at three geometries (LAGEOS-like zenith and 10 deg, a LEO case); the partials by the closed form AND by a
                          60-digit central difference, which must agree before either is emitted (the two-method guard).
  vapour     MEAS-A-022   e = Rh * 6.11 * 10^(7.5 t / (237.3 + t)) (Marini & Murray 1973, eq. 22) at four (t, Rh), with 10^x as exp(x ln 10).
  zenith     MEAS-A-020   the zenith delay of TN36 ch.9 eqs (9.3)-(9.7) at the IERS FCUL_ZD_HPA prolog's printed inputs, evaluated in 60
                          digits (the IMPLEMENTATION is compared with this; the PRINTED value is within 1 mm of it, 3.8 um away).

Usage:  measmod_reference.py [--check] [--header PATH]
Exit:   0 written / matches   1 --check found a difference   2 an input file is missing
"""
from __future__ import annotations

import argparse
import math
import sys
from decimal import Decimal, getcontext
from pathlib import Path

getcontext().prec = 60
ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "modules/measmod/tests/measmod_reference.hpp"
SLRF = ROOT / "data/cache/ilrs-slrf2020-20260205/SLRF2020_POS+VEL_2026.02.05.snx"
ECC = ROOT / "data/cache/ilrs-slrecc-une-20260527/slrecc.260527.ILRS.une.snx"

# WGS 84 (ERFA's n = 1, the ellipsoid the registry uses): a and 1/f, exactly as defined
WGS84_A = Decimal("6378137")
WGS84_INV_F = Decimal("298.257223563")


def block(lines: list[str], name: str) -> list[str]:
    out, on = [], False
    for ln in lines:
        if ln.startswith("+" + name):
            on = True
            continue
        if ln.startswith("-" + name):
            break
        if on and not ln.startswith("*"):
            out.append(ln)
    return out


def geodetic(x: Decimal, y: Decimal, z: Decimal):
    """Geodetic latitude, longitude, height by the fixed-point iteration tan(phi) = (z + e2 N sin(phi)) / p, in 60 digits;
    the angles are returned as doubles (the trigonometry at the end is the platform's: the arithmetic that matters,
    the iteration's convergence and the height, is not)."""
    f = 1 / WGS84_INV_F
    e2 = f * (2 - f)
    p = (x * x + y * y).sqrt()
    lon = math.atan2(float(y), float(x))
    phi = math.atan2(float(z), float(p) * (1 - float(e2)))   # a start; the iteration does the work
    h = Decimal(0)
    for _ in range(60):
        sphi = Decimal(math.sin(phi))
        n = WGS84_A / (1 - e2 * sphi * sphi).sqrt()
        new = math.atan2(float(z + e2 * n * sphi), float(p))
        if abs(new - phi) < 1e-17:
            phi = new
            break
        phi = new
    sphi, cphi = Decimal(math.sin(phi)), Decimal(math.cos(phi))
    n = WGS84_A / (1 - e2 * sphi * sphi).sqrt()
    h = p * cphi + z * sphi - WGS84_A * WGS84_A / n      # h = p cos(phi) + z sin(phi) - a^2 / N
    return phi, lon, float(h)


def registry_section() -> dict[str, float]:
    slrf = SLRF.read_text(encoding="utf-8").splitlines()
    ecc = ECC.read_text(encoding="utf-8").splitlines()
    est = {}
    for ln in block(slrf, "SOLUTION/ESTIMATE"):
        f = ln.split()
        if f[2] == "7090" and f[3] == "A" and f[4] == "1":
            est[f[1]] = Decimal(f[8])
    assert set(est) == {"STAX", "STAY", "STAZ", "VELX", "VELY", "VELZ"}, est
    # the eccentricity of SOD 70900513 whose span is open-ended (14:080:00000 ... 00:000:00000)
    rows = [ln.split() for ln in block(ecc, "SITE/ECCENTRICITY") if ln.split()[-1] == "70900513" and ln.split()[5] == "00:000:00000"]
    assert len(rows) == 1, rows
    u, n, e = (Decimal(rows[0][i]) for i in (7, 8, 9))
    # the reference epoch is 15:001:00000 = 2015-01-01 00:00:00 UTC; the epoch is 2026-01-01 02:07:56.8005871 UTC.
    # Elapsed SI time = elapsed UTC time + the leap seconds between: TAI-UTC was 35 s on 2015-01-01 (2012-07-01) and
    # 37 s from 2017-01-01, so 2 s (the 2015-06-30 and 2016-12-31 leap seconds).
    days = Decimal((__import__("datetime").date(2026, 1, 1) - __import__("datetime").date(2015, 1, 1)).days)
    elapsed_si = days * 86400 + Decimal("7676.8005871") + 2
    years = elapsed_si / (Decimal("365.25") * 86400)
    mx, my, mz = (est["STAX"] + est["VELX"] * years, est["STAY"] + est["VELY"] * years, est["STAZ"] + est["VELZ"] * years)
    phi, lon, h = geodetic(mx, my, mz)
    sp, cp, sl, cl = Decimal(math.sin(phi)), Decimal(math.cos(phi)), Decimal(math.sin(lon)), Decimal(math.cos(lon))
    sx = mx + cp * cl * u - sp * cl * n - sl * e
    sy = my + cp * sl * u - sp * sl * n + cl * e
    sz = mz + sp * u + cp * n
    sphi, slon, sh = geodetic(sx, sy, sz)
    return {
        "yarl_elapsed_years": float(years),
        "yarl_marker_x_m": float(mx), "yarl_marker_y_m": float(my), "yarl_marker_z_m": float(mz),
        "yarl_marker_lat_rad": phi, "yarl_marker_lon_rad": lon, "yarl_marker_h_m": h,
        "yarl_srp_x_m": float(sx), "yarl_srp_y_m": float(sy), "yarl_srp_z_m": float(sz),
        "yarl_srp_lat_rad": sphi, "yarl_srp_lon_rad": slon, "yarl_srp_h_m": sh,
        "yarl_srp_minus_marker_m": float(((sx - mx) ** 2 + (sy - my) ** 2 + (sz - mz) ** 2).sqrt()),
    }


PI = Decimal("3.14159265358979323846264338327950288419716939937510582097494459")
LN10 = Decimal(10).ln()


def d_cos(x: Decimal) -> Decimal:
    """cos by its Taylor series in 60 digits (|x| < 4 here)."""
    term, total, n = Decimal(1), Decimal(1), 0
    while abs(term) > Decimal("1e-70"):
        n += 2
        term = -term * x * x / (n * (n - 1))
        total += term
    return total


def d_pow10(x: Decimal) -> Decimal:
    return (x * LN10).exp()


def shapiro_section() -> dict[str, float]:
    gm = Decimal("3.986004415e14")                     # TN36-1, TT-compatible
    c = Decimal(299792458)
    k = 2 * gm / (c * c)
    r1 = Decimal(6378137)                              # a station on the ellipsoid, spherical stand-in

    def geometry(r2: Decimal, sin_e: Decimal):
        rho = -r1 * sin_e + (r1 * r1 * sin_e * sin_e + r2 * r2 - r1 * r1).sqrt()
        return float(r1), float(r2), float(rho)

    out: dict[str, float] = {"shapiro_two_gm_over_c2_m": float(k)}
    cases = {
        "zenith": geometry(Decimal(12270000), Decimal(1)),
        "el10": geometry(Decimal(12270000), Decimal(math.sin(math.radians(10.0)))),
        "leo30": geometry(Decimal("7400000"), Decimal(math.sin(math.radians(30.0)))),
    }
    for name, (a, b, rho) in cases.items():
        x1, x2, xr = Decimal(a), Decimal(b), Decimal(rho)          # the DOUBLES the test passes, exactly
        delay = k * ((x1 + x2 + xr) / (x1 + x2 - xr)).ln()
        s = x1 + x2
        d_rho = k * 2 * s / (s * s - xr * xr)
        d_r = -k * 2 * xr / (s * s - xr * xr)                       # d/dr1 = d/dr2
        # the two-method guard: a 60-digit central difference must agree with the closed form to 1e-30 relative
        h = Decimal("1e-20")
        f = lambda p1, p2, pr: k * ((p1 + p2 + pr) / (p1 + p2 - pr)).ln()
        n_rho = (f(x1, x2, xr + h) - f(x1, x2, xr - h)) / (2 * h)
        n_r1 = (f(x1 + h, x2, xr) - f(x1 - h, x2, xr)) / (2 * h)
        n_r2 = (f(x1, x2 + h, xr) - f(x1, x2 - h, xr)) / (2 * h)
        for closed, num in ((d_rho, n_rho), (d_r, n_r1), (d_r, n_r2)):
            assert abs(closed - num) <= abs(closed) * Decimal("1e-30"), (name, closed, num)
        out.update({f"shapiro_{name}_r1_m": a, f"shapiro_{name}_r2_m": b, f"shapiro_{name}_rho_m": rho,
                    f"shapiro_{name}_delay_m": float(delay), f"shapiro_{name}_d_rho": float(d_rho), f"shapiro_{name}_d_r": float(d_r)})
    return out


def vapour_section() -> dict[str, float]:
    out: dict[str, float] = {}
    for tag, t, rh in (("a", 20.0, 0.5), ("b", 0.0, 1.0), ("c", -10.0, 0.3), ("d", 35.0, 0.8)):
        dt, drh = Decimal(t), Decimal(rh)                           # the doubles the test passes, exactly
        e = drh * Decimal("6.11") * d_pow10(Decimal("7.5") * dt / (Decimal("237.3") + dt))
        out.update({f"vapour_{tag}_t_c": t, f"vapour_{tag}_rh": rh, f"vapour_{tag}_e_hpa": float(e)})
    return out


def zenith_section() -> dict[str, float]:
    # the printed inputs of the IERS FCUL_ZD_HPA prolog's test case (a published observation, the routine named and cited)
    lat_deg, height, ps, es, lam = Decimal("30.67166667"), Decimal("2010.344"), Decimal("798.4188"), Decimal("14.322"), Decimal("0.532")
    lat = lat_deg * PI / 180
    lat_d = Decimal(float(lat))                                     # the double the test passes
    fs = 1 - Decimal("0.00266") * d_cos(2 * lat_d) - Decimal("0.00000028") * height
    k0, k2, k1s, k3s = Decimal("238.0185"), Decimal("57.362"), Decimal("19990.975"), Decimal("579.55174")
    sig2 = (1 / lam) ** 2
    cco2 = 1 + Decimal("0.534e-6") * (375 - 450)
    fh = Decimal("1e-2") * (k1s * (k0 + sig2) / (k0 - sig2) ** 2 + k3s * (k2 + sig2) / (k2 - sig2) ** 2) * cco2
    w0, w1, w2, w3 = Decimal("295.235"), Decimal("2.6422"), Decimal("-0.032380"), Decimal("0.004028")
    fnh = Decimal("0.003101") * (w0 + 3 * w1 * sig2 + 5 * w2 * sig2 ** 2 + 7 * w3 * sig2 ** 3)
    zh = Decimal("0.002416579") * fh / fs * ps
    zw = Decimal("1e-4") * (Decimal("5.316") * fnh - Decimal("3.759") * fh) * es / fs
    return {"zenith_lat_rad": float(lat_d), "zenith_height_m": float(height), "zenith_p_hpa": float(ps), "zenith_e_hpa": float(es),
            "zenith_lambda_um": float(lam), "zenith_hydrostatic_m": float(zh), "zenith_wet_m": float(zw), "zenith_total_m": float(zh + zw)}



def render(sections: dict[str, dict[str, float]]) -> str:
    out = ["// GENERATED by tools/measmod_reference.py — do not edit; `python3 tools/measmod_reference.py --check` verifies it.",
           "// Reference values computed independently of the code under test (see the generator's header).",
           "#pragma once", "", "namespace odl::measmod::ref {", ""]
    for name, vals in sections.items():
        out.append(f"// section: {name}")
        for k, v in vals.items():
            out.append(f"inline constexpr double {k} = {v!r};")
        out.append("")
    out.append("}  // namespace odl::measmod::ref")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--header", type=Path, default=HEADER)
    a = ap.parse_args()
    try:
        text = render({"registry": registry_section(), "shapiro": shapiro_section(), "vapour": vapour_section(), "zenith": zenith_section()})
    except OSError as exc:
        print(f"measmod_reference.py: {exc} (run `python3 tools/fetch.py fetch`)", file=sys.stderr)
        return 2
    if a.check:
        if not a.header.exists() or a.header.read_text(encoding="utf-8") != text:
            print(f"FAILED   {a.header} differs from what the generator writes", file=sys.stderr)
            return 1
        print(f"ok       {a.header} matches the generator")
        return 0
    a.header.parent.mkdir(parents=True, exist_ok=True)
    a.header.write_text(text, encoding="utf-8")
    print(f"wrote    {a.header}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
