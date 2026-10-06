#!/usr/bin/env python3
"""gradient_reference.py — reference tensors for SPEC-gravity's gradient acceptance rows,
from the DEFINITION of the potential, by exact polynomial algebra.

WHAT IS BEING CHECKED.  `ConventionalField::gradient_of` returns the Cartesian second-derivative tensor of the
potential of ONE normalised coefficient (n, m, C or S, amplitude 1), computed by the recursion of SPEC-gravity
§4.4 extended to the second derivative (§4.7).  Its reference must not use that recursion, or any formula of
§4.7, or the test would compare the implementation with itself.

THE DEFINITION, with no Condon-Shortley phase (GRAV-R-007), in the units GM = a_e = 1 scaled out:

    V_nm = (GM a_e^n / r^(n+1)) Pbar_nm(z/r) [C cos(m lambda) + S sin(m lambda)],
    Pbar_nm(u) = N_nm (1-u^2)^(m/2) P_n^(m)(u),   N_nm^2 = (2 - delta_0m)(2n+1)(n-m)!/(n+m)!   (TN36-6 (6.2b)).

With (1-u^2)^(m/2) cos(m lambda) = Re (x + i y)^m / r^m this is a POLYNOMIAL over a power of r,

    V_nm = GM a_e^n N_nm  q(x, y, z) / r^(2n+1),
    q    = [ r^(n-m) P_n^(m)(z/r) ] * [ C Re (x+iy)^m + S Im (x+iy)^m ],

and r^(n-m) P_n^(m)(z/r) = sum_j e_j z^(n-2j-m) (x^2+y^2+z^2)^j is a polynomial because P_n^(m) has the parity of
n - m.  q is a homogeneous harmonic polynomial of degree n.  Its Hessian over r^s, s = 2n+1, is

    d_ij (q / r^s) = q_ij r^-s - s (q_i x_j + q_j x_i + q delta_ij) r^-(s+2) + s (s+2) q x_i x_j r^-(s+4),

which is exact calculus.  Everything but the final square roots (r, and N_nm) is exact rational arithmetic;
those are taken at 70 digits.  The dimensionless reference emitted is R_ij = G_ij / (GM / a_e^3) at the point
(x, y, z) given in METRES as the exact double the test will pass, so no position rounding enters.

THE SECOND CHECK, `--check`.  The same tensors are assembled from the formulas SPEC-gravity §4.7 prescribes — the
local (radial, north, east) Hessian from P'_nm, dP'_nm/du and d2P'_nm/du2 and the Horner-form sums of §4.7 — in
90-digit arithmetic and compared with the polynomial route.  That is how §4.7's formulas were verified before they
were written down, and why the specification can say so.  The two routes share nothing but the definition.

Usage:  gradient_reference.py [--check] [--header PATH]
Exit:   0 emitted / verified   1 a value failed its two-precision agreement, or the two routes disagree
"""

from __future__ import annotations

import argparse
import sys
from decimal import Decimal, localcontext
from fractions import Fraction
from math import comb, factorial
from pathlib import Path

AE = Fraction(63781363, 10)        # EGM2008's a_e in metres, the value ScalingParameters carries

# (n, m, kind) with kind 'C' or 'S'.  Every term to degree 4, then a spread of higher ones: sectorial, near-sectorial,
# zonal, and interior, so the m-dependent nests (m = 0, 1, >= 2) and the degree factors are all exercised.
CASES = ([(n, m, k) for n in (2, 3, 4) for m in range(n + 1) for k in "CS" if not (m == 0 and k == "S")]
         + [(6, 3, "C"), (6, 5, "S"), (10, 7, "C"), (10, 10, "S"), (20, 0, "C"), (20, 13, "C"),
            (36, 17, "S"), (36, 36, "C")])

# Positions in units of a_e, as exact binary doubles after multiplication by a_e: the equator on the x axis, a
# general mid-latitude point, a point in the southern hemisphere in another octant, the pole itself, and a point a
# whisker off it (where cos(phi) is tiny and any 1/cos(phi) in an implementation would announce itself).
POINTS_XI = [(1.05, 0.0, 0.0), (0.62, 0.55, 0.71), (-0.43, 0.81, -0.74), (0.0, 0.0, 1.07), (1.0e-3, -2.0e-3, 1.1)]


# ------------------------------------------------------------------------------------------------------------
# exact polynomials in x, y, z:  {(i, j, k): Fraction}
# ------------------------------------------------------------------------------------------------------------

def p_add(a: dict, b: dict) -> dict:
    out = dict(a)
    for k, v in b.items():
        s = out.get(k, 0) + v
        if s == 0:
            out.pop(k, None)
        else:
            out[k] = s
    return out


def p_mul(a: dict, b: dict) -> dict:
    out: dict = {}
    for ka, va in a.items():
        for kb, vb in b.items():
            k = (ka[0] + kb[0], ka[1] + kb[1], ka[2] + kb[2])
            s = out.get(k, 0) + va * vb
            if s == 0:
                out.pop(k, None)
            else:
                out[k] = s
    return out


def p_scale(a: dict, c) -> dict:
    return {k: v * c for k, v in a.items()} if c != 0 else {}


def p_der(a: dict, axis: int) -> dict:
    out: dict = {}
    for k, v in a.items():
        if k[axis] > 0:
            nk = list(k)
            nk[axis] -= 1
            out[tuple(nk)] = v * k[axis]
    return out


def p_eval(a: dict, pt) -> Fraction:
    total = Fraction(0)
    for (i, j, k), v in a.items():
        total += v * pt[0] ** i * pt[1] ** j * pt[2] ** k
    return total


X, Y, Z = {(1, 0, 0): Fraction(1)}, {(0, 1, 0): Fraction(1)}, {(0, 0, 1): Fraction(1)}
ONE = {(0, 0, 0): Fraction(1)}
R2 = p_add(p_add(p_mul(X, X), p_mul(Y, Y)), p_mul(Z, Z))


def p_pow(a: dict, e: int) -> dict:
    out = ONE
    for _ in range(e):
        out = p_mul(out, a)
    return out


# ------------------------------------------------------------------------------------------------------------
# the definition
# ------------------------------------------------------------------------------------------------------------

def norm_squared(n: int, m: int) -> Fraction:
    return Fraction(factorial(n - m) * (2 * n + 1) * (2 - (1 if m == 0 else 0)), factorial(n + m))


def legendre_derivative_coeffs(n: int, m: int) -> dict[int, Fraction]:
    """P_n^(m)(t) = sum_e coeff[e] t^e, exactly, from P_n(t) = 2^-n sum_j (-1)^j C(n,j) C(2n-2j,n) t^(n-2j)."""
    out: dict[int, Fraction] = {}
    j = 0
    while n - 2 * j - m >= 0:
        e = n - 2 * j - m
        c = Fraction(((-1) ** j) * comb(n, j) * comb(2 * n - 2 * j, n) * factorial(n - 2 * j), factorial(e) * 2 ** n)
        out[e] = c
        j += 1
    return out


def solid_polynomial(n: int, m: int, kind: str) -> dict:
    """q / N_nm: [ r^(n-m) P_n^(m)(z/r) ] * [ Re or Im (x+iy)^m ], exactly."""
    pi: dict = {}
    for e, c in legendre_derivative_coeffs(n, m).items():
        j = (n - m - e) // 2
        pi = p_add(pi, p_scale(p_mul(p_pow(Z, e), p_pow(R2, j)), c))
    a, b = ONE, {}
    for _ in range(m):
        a, b = p_add(p_mul(a, X), p_scale(p_mul(b, Y), -1)), p_add(p_mul(a, Y), p_mul(b, X))
    return p_mul(pi, a if kind == "C" else b)


def reference_tensor(n: int, m: int, kind: str, pos_m: tuple[Fraction, Fraction, Fraction], prec: int):
    """R_ij = G_ij / (GM / a_e^3) for the potential of the single coefficient (n, m, kind) = 1, at the exact point."""
    with localcontext() as ctx:
        ctx.prec = prec
        q = solid_polynomial(n, m, kind)
        qi = [p_der(q, a) for a in range(3)]
        qij = [[p_der(qi[a], b) for b in range(3)] for a in range(3)]
        d = lambda f: Decimal(f.numerator) / Decimal(f.denominator)
        xs = [d(c) for c in pos_m]
        r2 = pos_m[0] ** 2 + pos_m[1] ** 2 + pos_m[2] ** 2
        r = d(r2).sqrt()
        s = 2 * n + 1
        qv = d(p_eval(q, pos_m))
        qiv = [d(p_eval(qi[a], pos_m)) for a in range(3)]
        qijv = [[d(p_eval(qij[a][b], pos_m)) for b in range(3)] for a in range(3)]
        nsq = norm_squared(n, m)
        norm = (Decimal(nsq.numerator) / Decimal(nsq.denominator)).sqrt()
        scale = norm * d(AE ** (n + 3))
        out = [[Decimal(0)] * 3 for _ in range(3)]
        for i in range(3):
            for j in range(3):
                f = (qijv[i][j] * r ** (-s)
                     - s * (qiv[i] * xs[j] + qiv[j] * xs[i] + (qv if i == j else 0)) * r ** (-(s + 2))
                     + s * (s + 2) * qv * xs[i] * xs[j] * r ** (-(s + 4)))
                out[i][j] = scale * f
        return out


# ------------------------------------------------------------------------------------------------------------
# the formulas of SPEC-gravity §4.7, in Decimal, for the second check
# ------------------------------------------------------------------------------------------------------------

def local_formula_tensor(n: int, m: int, kind: str, pos_m, prec: int):
    """The tensor from P'_nm, dP'_nm/du, d2P'_nm/du2 and the sums of §4.7, with GM = a_e = 1 (so position in a_e)."""
    with localcontext() as ctx:
        ctx.prec = prec
        d = lambda f: Decimal(f.numerator) / Decimal(f.denominator)
        xi = [d(c / AE) for c in pos_m]
        rho2 = xi[0] ** 2 + xi[1] ** 2
        r = (rho2 + xi[2] ** 2).sqrt()
        u = xi[2] / r
        c = rho2.sqrt() / r
        if rho2 > 0:
            cl, sl = xi[0] / rho2.sqrt(), xi[1] / rho2.sqrt()
        else:
            cl, sl = Decimal(1), Decimal(0)
        nsq = norm_squared(n, m)
        norm = (Decimal(nsq.numerator) / Decimal(nsq.denominator)).sqrt()

        def poly(order_extra: int) -> Decimal:
            tot = Decimal(0)
            for e, coef in legendre_derivative_coeffs(n, m + order_extra).items():
                tot += d(coef) * (u ** e if e else Decimal(1))
            return norm * tot

        # P'_nm = N_nm P_n^(m)(u);  dP'/du = N_nm P_n^(m+1)(u);  d2P'/du2 = N_nm P_n^(m+2)(u)
        p, p1, p2 = poly(0), poly(1), poly(2)
        # cos(m lambda), sin(m lambda) by the exact angle-addition recursion
        cm, sm = Decimal(1), Decimal(0)
        for _ in range(m):
            cm, sm = cm * cl - sm * sl, sm * cl + cm * sl
        C, S = (Decimal(1), Decimal(0)) if kind == "C" else (Decimal(0), Decimal(1))
        w = C * cm + S * sm
        wp = S * cm - C * sm

        def cp(k: int) -> Decimal:                   # c^k, with 0^0 = 1 and no negative power ever formed
            assert k >= 0
            return Decimal(1) if k == 0 else c ** k

        md = Decimal(m)
        A0 = cp(m) * p * w
        Bphi = w * ((-md * cp(m - 1) * u * p) if m >= 1 else Decimal(0)) + w * cp(m + 1) * p1
        Blam = (md * cp(m - 1) * p * wp) if m >= 1 else Decimal(0)
        Cpp = w * (((md * (md - 1)) * cp(m - 2) * u * u * p) if m >= 2 else Decimal(0)) \
            + w * (-md * cp(m) * p - (2 * md + 1) * cp(m) * u * p1 + cp(m + 2) * p2)
        Cpl = md * wp * ((-(md - 1) * cp(m - 2) * u * p) if m >= 2 else Decimal(0)) + md * wp * cp(m) * p1
        Cll = w * (-md * cp(m) * p - u * cp(m) * p1) \
            + w * ((-(md * (md - 1)) * cp(m - 2) * p) if m >= 2 else Decimal(0))
        g = (Decimal(1) / r ** 3) * (Decimal(1) / r) ** n          # GM / r^3 times (a_e / r)^n, GM = a_e = 1
        nd = Decimal(n)
        H = [[g * (nd + 1) * (nd + 2) * A0, -g * (nd + 2) * Bphi, -g * (nd + 2) * Blam],
             [-g * (nd + 2) * Bphi, g * (Cpp - (nd + 1) * A0), g * Cpl],
             [-g * (nd + 2) * Blam, g * Cpl, g * (Cll - (nd + 1) * A0)]]
        # T: columns e_r, e_north, e_east
        T = [[c * cl, -u * cl, -sl],
             [c * sl, -u * sl, cl],
             [u, c, Decimal(0)]]
        out = [[Decimal(0)] * 3 for _ in range(3)]
        for i in range(3):
            for j in range(3):
                tot = Decimal(0)
                for a in range(3):
                    for b in range(3):
                        tot += T[i][a] * H[a][b] * T[j][b]
                out[i][j] = tot
        return out


def pos_fraction(xi) -> tuple[Fraction, Fraction, Fraction]:
    """The exact value of the DOUBLE that the C++ test forms as a_e * xi (one rounded multiplication)."""
    ae_d = float(AE)
    return tuple(Fraction(ae_d * v) for v in xi)


def two_precision(n, m, kind, pos):
    a = reference_tensor(n, m, kind, pos, 70)
    b = reference_tensor(n, m, kind, pos, 110)
    worst = Decimal(0)
    scale = max(abs(b[i][j]) for i in range(3) for j in range(3)) or Decimal(1)
    with localcontext() as ctx:
        ctx.prec = 60
        for i in range(3):
            for j in range(3):
                worst = max(worst, abs(+a[i][j] - +b[i][j]) / scale)
    if worst > Decimal("1e-50"):
        raise ValueError(f"({n},{m},{kind}) at {pos}: the 70- and 110-digit tensors differ by {worst}")
    return b


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="gradient_reference.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="also verify SPEC-gravity §4.7's formulas against the definition")
    ap.add_argument("--header", type=Path, default=None, help="write a C++ header here instead of printing")
    ap.add_argument("--verify", type=Path, default=None,
                    help="compare this committed header with what the generator emits; exit 1 if they differ")
    args = ap.parse_args(argv)

    rows = []
    worst_formula = Decimal(0)
    for (n, m, kind) in CASES:
        for xi in POINTS_XI:
            pos = pos_fraction(xi)
            try:
                ref = two_precision(n, m, kind, pos)
            except ValueError as exc:
                print(f"REFUSED  {exc}", file=sys.stderr)
                return 1
            if args.check:
                form = local_formula_tensor(n, m, kind, pos, 90)
                with localcontext() as ctx:
                    ctx.prec = 100
                    scale = max(abs(ref[i][j]) for i in range(3) for j in range(3)) or Decimal(1)
                    diff = max(abs(form[i][j] - ref[i][j]) for i in range(3) for j in range(3)) / scale
                worst_formula = max(worst_formula, diff)
                if diff > Decimal("1e-80"):
                    print(f"DISAGREE ({n},{m},{kind}) at {xi}: formulas differ from the definition by {diff}",
                          file=sys.stderr)
                    return 1
            rows.append((n, m, kind, [float(v) * float(AE) for v in xi], ref))
    if args.check:
        print(f"ok       §4.7's formulas agree with the definition over {len(rows)} cases; "
              f"worst relative disagreement {worst_formula:.3E} (the formulas at 90 digits, the definition at 110)", file=sys.stderr)

    lines = [
        "// GENERATED by tools/gradient_reference.py — do not edit.",
        "//",
        "// Reference second-derivative tensors for SPEC-gravity's gradient acceptance rows, computed from the DEFINITION",
        "// of the potential of ONE normalised coefficient by exact polynomial algebra (a homogeneous harmonic polynomial",
        "// over a power of r, differentiated exactly), never from §4.4's recursion or §4.7's formulas.  Each tensor was",
        "// evaluated at 70 and at 110 digits and is emitted only if the two agree to 50.",
        "//",
        "// `g` is G_ij / (GM / a_e^3) for the potential of the single coefficient (n, m, kind) = 1 at the position (x, y, z)",
        "// in metres — the exact double the test passes — in the order xx, xy, xz, yy, yz, zz.  The tensor is symmetric",
        "// by calculus; the test compares all NINE entries of the implementation's, so a pair that disagrees fails.",
        "#pragma once",
        "",
        "namespace odl::gravity::reference {",
        "",
        "struct GradientCase {",
        "    int n;",
        "    int m;",
        "    char kind;          ///< 'C' or 'S'",
        "    double x, y, z;     ///< metres",
        "    double g[6];        ///< xx, xy, xz, yy, yz, zz of G / (GM / a_e^3)",
        "};",
        "",
        "inline constexpr GradientCase kGradient[] = {",
    ]
    for n, m, kind, pos, ref in rows:
        hexes = [float(ref[i][j]).hex() for (i, j) in ((0, 0), (0, 1), (0, 2), (1, 1), (1, 2), (2, 2))]
        lines.append(f"    {{{n}, {m}, '{kind}', {pos[0]!r}, {pos[1]!r}, {pos[2]!r}, "
                     f"{{{', '.join(hexes)}}}}},")
    lines += ["};", "", "}  // namespace odl::gravity::reference", ""]
    text = "\n".join(lines)
    if args.verify:
        ok = args.verify.read_text(encoding="utf-8") == text
        print("ok       the committed header is exactly what the generator emits" if ok
              else "MISMATCH the committed header differs from the generator's output", file=sys.stderr)
        return 0 if ok else 1
    if args.header:
        args.header.write_text(text, encoding="utf-8")
        print(f"wrote {len(rows)} tensors to {args.header}", file=sys.stderr)
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
