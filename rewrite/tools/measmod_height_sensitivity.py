#!/usr/bin/env python3
"""measmod_height_sensitivity.py — how much the one-way tropospheric leg delay moves when the station height H moves.

SPEC-measmod.md MEAS-A-024 / MEAS-R-037.  TN36 §9.1.1's footnote says the zenith-delay formula is "insensitive" to the
difference between geodetic and orthometric height; the model uses the ellipsoidal height, so the sensitivity is stated
as a number.  H enters twice: the zenith delay's own f_s(phi, H), and the mapping function's a_i3 H terms.

WRITTEN 2026-10-06 BEFORE the C++ test of MEAS-A-024 was run.  The specification's first text predicted 21 um for a 100 m change
from the mapping function's a_13 term alone; this evaluation (both terms, the printed equations of TN36 §9.1) gives 0.115 mm for
30 m and 0.382 mm for 100 m, and the specification was corrected, before the test, to say so.  Standard library only.

Usage:  measmod_height_sensitivity.py [--check]
Exit:   0 printed (and, with --check, the two figures reproduced)   1 not reproduced
"""
from __future__ import annotations

import argparse
import sys
from math import cos, radians, sin

LAT = radians(30.67166667)          # the IERS FCUL_A / FCUL_ZD_HPA test cases' station
PS, ES, LAM, TS = 798.4188, 14.322, 0.532, 15.0


def ztd(h: float) -> float:
    fs = 1 - 0.00266 * cos(2 * LAT) - 0.00000028 * h
    k0, k2, k1s, k3s = 238.0185, 57.362, 19990.975, 579.55174
    sig = 1 / LAM
    cco2 = 1 + 0.534e-6 * (375 - 450)
    fh = 1e-2 * (k1s * (k0 + sig**2) / (k0 - sig**2) ** 2 + k3s * (k2 + sig**2) / (k2 - sig**2) ** 2) * cco2
    fnh = 0.003101 * (295.235 + 3 * 2.6422 * sig**2 + 5 * (-0.032380) * sig**4 + 7 * 0.004028 * sig**6)
    return 0.002416579 * fh / fs * PS + 1e-4 * (5.316 * fnh - 3.759 * fh) * ES / fs


def fcula(s: float, h: float) -> float:
    a = {1: (12100.8e-7, 1729.5e-9, 319.1e-7, -1847.8e-11), 2: (30496.5e-7, 234.6e-8, -103.5e-6, -185.6e-10),
         3: (6877.7e-5, 197.2e-7, -345.8e-5, 106.0e-9)}
    c = {i: v[0] + v[1] * TS + v[2] * cos(LAT) + v[3] * h for i, v in a.items()}
    return (1 + c[1] / (1 + c[2] / (1 + c[3]))) / (s + c[1] / (s + c[2] / (s + c[3])))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    s, h0 = sin(radians(15.0)), 244.0
    got = {}
    for dh in (30.0, 100.0):
        d_leg = ztd(h0 + dh) * fcula(s, h0 + dh) - ztd(h0) * fcula(s, h0)
        d_map = ztd(h0) * (fcula(s, h0 + dh) - fcula(s, h0))
        got[dh] = d_leg
        print(f"dH = {dh:5.0f} m: dZTD = {ztd(h0 + dh) - ztd(h0):.3e} m; leg delay change at 15 deg = {d_leg:.3e} m (the mapping function alone {d_map:.3e} m)")
    if a.check:
        ok = abs(got[30.0] - 1.146e-4) < 5e-7 and abs(got[100.0] - 3.819e-4) < 5e-7
        print("ok       the predicted 0.115 mm and 0.382 mm reproduced" if ok else "FAILED   not reproduced")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
