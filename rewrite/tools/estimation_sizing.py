#!/usr/bin/env python3
"""estimation_sizing.py — the FROZEN FIGURES of SPEC-estimation §6 (L7 step 2: the batch estimator, normal equations scaled by default).

The estimator's gate is registered BEFORE any code of it exists (plan §4 rule 7).  This tool is where the numbers the registration needs come from, in exact
arithmetic, and it is run by a ctest (`estimation.sizing_reproduces`) so that no figure in the specification is a number nobody can regenerate (plan §4 rule 3).

WHAT IT COMPUTES, for each dataset the gate is registered against (NIST StRD's eleven linear regression datasets, read from the PINNED CACHE and never copied
into the tree; Demmel's worked example; the synthetic cases of SPEC-estimation §8.2):

  * the rows the C++ tests will build, as DOUBLES, by the recipe of §8.2 (the decimal parsed in x87 long double, the powers formed in long double by repeated
    multiplication, rounded to double once) — and a checksum of their bit patterns, which the test asserts equal, so that every figure below applies to the very
    rows the test feeds the estimator;
  * the exact (rational) normal equations, the exact solution, the exact inverse of the SCALED matrix, the exact eigenvalues of the unit-diagonal matrix A and of
    the scaled matrix (80-digit decimal Jacobi), and from them kappa(N_s), kappa_i = N_ii (N^-1)_ii, ||Q_s||_1, ||A^-1||_2;
  * the derived bounds of SPEC-estimation §4/§6 — the constants come from Demmel (LAPACK Working Note 14) Lemma 2.1 and the proof of Theorem 2.1 and from the
    standard dot-product and substitution bounds, with u = 2^-53:
        gamma_k   = k u / (1 - k u)
        chi_n     = 1 / (1 - (n+1) u)
        Theta_s   = chi_n [ (3 n^2 + n) u + n^3 u^2 ]                       the solve (Cholesky and two substitutions), scaled, in the 2-norm
        Theta_N   = n gamma_m (1 + gamma_m) + Theta_s                       plus the forming error of N
        B         = 4 ||Q_s||_1 ( Theta_N + sqrt(n) gamma_m ||r|| / ||D x|| )   the scaled forward error the solve CERTIFIES (code: eta_rep = 0)
    and the test-level versions that add the REPRESENTATION error of decimal data (eta_rep = 2.01 u) and the certified values' own last-digit uncertainty;
  * the figures of the unit-invariance control (the unscaled decision rule), of Filip's registered refusal, of Demmel's example, and of the synthetic cases.

Usage:  estimation_sizing.py [--cache DIR] [--report] [--check] [--header PATH] [--verify PATH]
Exit:   0 ok   1 a frozen figure differs from its regeneration / the committed header is not what the generator emits   2 usage or input error
"""
from __future__ import annotations

import argparse
import math
import re
import struct
import sys
from decimal import Decimal, getcontext
from fractions import Fraction as F
from pathlib import Path

import numpy as np

getcontext().prec = 90

HERE = Path(__file__).resolve().parent
DEFAULT_CACHE = HERE.parent / "data" / "cache"

U = 2.0 ** -53                  # the unit roundoff of IEEE binary64, round to nearest
ETA_REP = 2.01 * U              # relative representation error of a decimal datum rounded to double through x87 long double (u + 4e-19, doubled for a product)
CERT_RELATIVE = 1.0e-14         # one unit of the 15th printed digit, worst case (leading digit 1): the certified values' own uncertainty


def gamma(k: int) -> float:
    return k * U / (1.0 - k * U)


def chi(n: int) -> float:
    return 1.0 / (1.0 - (n + 1) * U)


def theta_solve(n: int) -> float:
    return chi(n) * ((3 * n * n + n) * U + n ** 3 * U * U)


def theta_N(n: int, m: int, eta_rep: float = 0.0) -> float:
    return n * (gamma(m) * (1.0 + gamma(m)) + eta_rep) + theta_solve(n)


# ----------------------------------------------------------------------------------------------------------------------------------------------- the NIST files

NIST = [  # name, parameters, observations
    ("Norris", 2, 36), ("Pontius", 3, 40), ("NoInt1", 1, 11), ("NoInt2", 1, 3), ("Filip", 11, 82), ("Longley", 7, 16),
    ("Wampler1", 6, 21), ("Wampler2", 6, 21), ("Wampler3", 6, 21), ("Wampler4", 6, 21), ("Wampler5", 6, 21),
]
SERVED = [n for n, _, _ in NIST if n != "Filip"]       # the ten the certified-value gate is registered against; Filip is registered as REFUSED


def read_nist(cache: Path, name: str) -> dict:
    path = cache / f"nist-strd-lls-{name.lower()}" / f"{name}.dat"
    lines = path.read_text(encoding="latin-1").splitlines()
    head = "\n".join(lines[:12])
    c0, c1 = (int(v) for v in re.search(r"Certified Values\s+\(lines (\d+) to (\d+)\)", head).groups())
    d0, d1 = (int(v) for v in re.search(r"Data\s+\(lines (\d+) to (\d+)\)", head).groups())
    cert, sd = [], []
    for ln in lines[c0 - 1:c1]:
        mm = re.match(r"\s+B(\d+)\s+(\S+)\s+(\S+)\s*$", ln)
        if mm:
            cert.append(mm.group(2)); sd.append(mm.group(3))
    block = "\n".join(lines[c0 - 1:c1])
    rsd = re.search(r"Residual\s*\n\s*Standard Deviation\s+(\S+)", block).group(1)
    rows = [ln.split() for ln in lines[d0 - 1:d1] if ln.split()]
    return dict(cert=cert, sd=sd, rsd=rsd, rows=rows)


def design(name: str, rows, exact: bool):
    """The design matrix and the response.  exact=True: rationals; exact=False: the DOUBLES of the C++ test (long double recurrence, rounded once to double)."""
    ld = np.longdouble
    X_out, y_out = [], []
    for r in rows:
        vals = [F(Decimal(t)) for t in r] if exact else [ld(t) for t in r]
        one = F(1) if exact else ld(1)
        y = vals[0]
        if name in ("NoInt1", "NoInt2"):
            X = [vals[1]]
        elif name == "Longley":
            X = [one] + list(vals[1:])
        elif name == "Norris":
            X = [one, vals[1]]
        elif name == "Pontius":
            X = [one, vals[1], vals[1] * vals[1]]
        elif name == "Filip":
            X, p = [one], one
            for _ in range(10):
                p = p * vals[1]; X.append(p)
        else:                                            # Wampler1 .. 5
            X, p = [one], one
            for _ in range(5):
                p = p * vals[1]; X.append(p)
        X_out.append(X if exact else [float(v) for v in X])
        y_out.append(y if exact else float(y))
    return X_out, y_out


def checksum(X, y) -> int:
    s = 0
    for row, yy in zip(X, y):
        for v in row:
            s = (s + struct.unpack("<Q", struct.pack("<d", v))[0]) & 0xFFFFFFFFFFFFFFFF
        s = (s + struct.unpack("<Q", struct.pack("<d", yy))[0]) & 0xFFFFFFFFFFFFFFFF
    return s


# ------------------------------------------------------------------------------------------------------------------------------- exact linear algebra

def dec(q: F) -> Decimal:
    return Decimal(q.numerator) / Decimal(q.denominator)


def exact_solve(M, b):
    n = len(M)
    A = [M[i][:] + [b[i]] for i in range(n)]
    for i in range(n):
        piv = next(r for r in range(i, n) if A[r][i] != 0)
        A[i], A[piv] = A[piv], A[i]
        for r in range(n):
            if r != i and A[r][i] != 0:
                f = A[r][i] / A[i][i]
                A[r] = [x - f * y for x, y in zip(A[r], A[i])]
    return [A[i][n] / A[i][i] for i in range(n)]


def exact_inverse(M):
    n = len(M)
    A = [M[i][:] + [F(1) if i == j else F(0) for j in range(n)] for i in range(n)]
    for i in range(n):
        piv = next(r for r in range(i, n) if A[r][i] != 0)
        A[i], A[piv] = A[piv], A[i]
        d = A[i][i]
        A[i] = [x / d for x in A[i]]
        for r in range(n):
            if r != i and A[r][i] != 0:
                f = A[r][i]
                A[r] = [x - f * y for x, y in zip(A[r], A[i])]
    return [row[n:] for row in A]


def jacobi_eigs(M, vectors=False):
    n = len(M)
    A = [row[:] for row in M]
    V = [[Decimal(1) if i == j else Decimal(0) for j in range(n)] for i in range(n)]
    for _ in range(100):
        off = sum(A[i][j] ** 2 for i in range(n) for j in range(i + 1, n))
        dg = sum(A[i][i] ** 2 for i in range(n))
        if off <= dg * Decimal(10) ** -160:
            break
        for p in range(n - 1):
            for q in range(p + 1, n):
                if A[p][q] == 0:
                    continue
                th = (A[q][q] - A[p][p]) / (2 * A[p][q])
                t = (1 if th >= 0 else -1) / (abs(th) + (th * th + 1).sqrt())
                c = 1 / (t * t + 1).sqrt(); s = t * c
                for k in range(n):
                    akp, akq = A[k][p], A[k][q]
                    A[k][p] = c * akp - s * akq; A[k][q] = s * akp + c * akq
                    vkp, vkq = V[k][p], V[k][q]
                    V[k][p] = c * vkp - s * vkq; V[k][q] = s * vkp + c * vkq
                for k in range(n):
                    apk, aqk = A[p][k], A[q][k]
                    A[p][k] = c * apk - s * aqk; A[q][k] = s * apk + c * aqk
    order = sorted(range(n), key=lambda i: A[i][i])
    vals = [A[i][i] for i in order]
    if vectors:
        return vals, [[V[k][i] for k in range(n)] for i in order]
    return vals


def scale_exponent(d: float) -> int:
    """The rule of SPEC-estimation EST-R-201: d = m 2^k, m in [0.5, 1); e = floor(k / 2); the scaled diagonal d 2^-2e lies in [0.5, 2)."""
    return math.frexp(d)[1] // 2


# --------------------------------------------------------------------------------------------------------------------------- the double-precision recipe

def normal_double(X, y):
    """The estimator's forming (EST-R-203): N-hat_ij = sum_k fl(a_ki a_kj), rows ascending, plain double sums; b-hat likewise."""
    m, n = len(X), len(X[0])
    N = [[0.0] * n for _ in range(n)]
    b = [0.0] * n
    for k in range(m):
        a = X[k]
        for i in range(n):
            for j in range(i + 1):
                N[i][j] += a[i] * a[j]
            b[i] += a[i] * y[k]
    for i in range(n):
        for j in range(i):
            N[j][i] = N[i][j]
    return N, b


def cholesky_double(N):
    """Demmel's Algorithm 2.1 in plain double (EST-R-204).  Returns (L, None) or (None, (index, pivot value))."""
    n = len(N)
    L = [[0.0] * n for _ in range(n)]
    for i in range(n):
        s = N[i][i]
        for k in range(i):
            s -= L[i][k] * L[i][k]
        if not (s > 0.0) or not math.isfinite(s):
            return None, (i, s)
        L[i][i] = math.sqrt(s)
        for j in range(i + 1, n):
            t = N[j][i]
            for k in range(i):
                t -= L[j][k] * L[i][k]
            L[j][i] = t / L[i][i]
    return L, None


# ----------------------------------------------------------------------------------------------------------------------------------- a dataset's figures

def analyse(cache: Path, name: str) -> dict:
    P = read_nist(cache, name)
    Xe, ye = design(name, P["rows"], True)
    Xd, yd = design(name, P["rows"], False)
    m, n = len(Xd), len(Xd[0])
    Nh, bh = normal_double(Xd, yd)
    e = [scale_exponent(Nh[i][i]) for i in range(n)]
    Nsd = [[math.ldexp(Nh[i][j], -(e[i] + e[j])) for j in range(n)] for i in range(n)]
    L, brk = cholesky_double(Nsd)
    out = dict(name=name, n=n, m=m, e=e, checksum=checksum(Xd, yd), cert=P["cert"], sd=P["sd"], rsd=P["rsd"])
    out["breakdown_index"] = None if brk is None else brk[0]
    out["breakdown_pivot"] = None if brk is None else brk[1]
    # exact quantities (the decimal data, the same power-of-two scaling as the code)
    Ne = [[sum(Xe[k][i] * Xe[k][j] for k in range(m)) for j in range(n)] for i in range(n)]
    be = [sum(Xe[k][i] * ye[k] for k in range(m)) for i in range(n)]
    x = exact_solve(Ne, be)
    Nse = [[Ne[i][j] * F(2) ** (-(e[i] + e[j])) for j in range(n)] for i in range(n)]
    Qse = exact_inverse(Nse)
    q1 = max(sum(abs(Qse[i][j]) for i in range(n)) for j in range(n))
    dd = [dec(Ne[i][i]).sqrt() for i in range(n)]
    A = [[dec(Ne[i][j]) / (dd[i] * dd[j]) for j in range(n)] for i in range(n)]
    evA = jacobi_eigs(A)
    evNs = jacobi_eigs([[dec(Nse[i][j]) for j in range(n)] for i in range(n)])
    out.update(
        kappa=float(evNs[-1] / evNs[0]), lam_min_Ns=float(evNs[0]), lam_max_Ns=float(evNs[-1]), lam_min_A=float(evA[0]), q1=float(dec(q1)),
        kappa_i=[float(dec(Ne[i][i] * F(2) ** (-2 * e[i]) * Qse[i][i])) for i in range(n)],      # N_s,ii Q_s,ii = N_ii (N^-1)_ii
        Dx=float(dec(sum(Ne[i][i] * x[i] * x[i] for i in range(n))).sqrt()),
        r=float(dec(sum(v * v for v in ye)).sqrt()),
        digits_exact_vs_cert=min(
            float(-((dec(x[i]) - Decimal(P["cert"][i])).copy_abs() / Decimal(P["cert"][i]).copy_abs()).log10())
            if dec(x[i]) != Decimal(P["cert"][i]) else 15.0 for i in range(n)))
    # the residual at the exact solution: Omega, sigma0 (against the certified residual standard deviation) and the magnitudes the bounds need
    rho = [ye[k] - sum(Xe[k][i] * x[i] for i in range(n)) for k in range(m)]
    omega = sum(v * v for v in rho)
    nu = m - n
    out["sigma0_exact"] = float(dec(omega / nu).sqrt())
    out["abs_AX_x"] = float(dec(sum((sum(abs(Xe[k][i]) * abs(x[i]) for i in range(n))) ** 2 for k in range(m))).sqrt())
    # --- the derived bounds -----------------------------------------------------------------------------------------------
    r, Dx = out["r"], out["Dx"]
    tN0 = theta_N(n, m, 0.0)
    tNt = theta_N(n, m, ETA_REP)
    out["theta_N_code"] = tN0
    out["theta_N_test"] = tNt
    out["lam_max_A"] = float(evA[-1])
    out["qdiag"] = [float(dec(Qse[i][i] * F(2) ** (-2 * e[i]))) for i in range(n)]     # Q_ii = (N^-1)_ii in the units of the parameters
    out["B_code"] = 4.0 * out["q1"] * (tN0 + math.sqrt(n) * gamma(m) * r / Dx) if Dx > 0 else math.inf
    out["B_test"] = 4.0 * out["q1"] * (tNt + math.sqrt(n) * (gamma(m) + ETA_REP) * r / Dx) + CERT_RELATIVE if Dx > 0 else math.inf
    out["certified_no_digit"] = out["B_code"] >= 0.5
    ainv = 1.0 / out["lam_min_A"]
    tau = tNt * ainv
    out["tau"] = tau
    out["B_Q"] = tau / (1.0 - tau) if tau < 1 else math.inf                       # |Q-hat_ii / Q_ii - 1| <= B_Q
    out["B_rho"] = 3.0 * out["B_Q"]                                               # |rho-hat_ij - rho_ij| <= B_rho
    # sigma0.  Omega(x-hat) = Omega + delta^2 (Pythagoras: the residual at the optimum is orthogonal to the columns), delta^2 = (x-hat - x)' N (x-hat - x)
    #   <= ||A^-1||_2 g0^2, g0 = Theta_N ||D x|| + sqrt(n) (gamma_m + eta_rep) ||r||  (N (x-hat - x) = Delta b - Delta N x-hat);  so
    #   sqrt(Omega-hat) - sqrt(Omega) <= min(delta, delta^2 / (2 sqrt(Omega))); the rounding of the residuals and of the sum of squares and the representation error add
    sq_omega = out["sigma0_exact"] * math.sqrt(nu)
    g0 = tNt * Dx + math.sqrt(n) * (gamma(m) + ETA_REP) * r
    delta = math.sqrt(ainv) * g0
    d_sqrt = delta if sq_omega == 0.0 else min(delta, delta * delta / (2.0 * sq_omega))
    out["delta"] = delta
    out["B_sigma"] = ((gamma(n + 1) + 2 * U + U * U) * (r + out["abs_AX_x"]) + 0.5 * gamma(m + 1) * sq_omega + d_sqrt) / math.sqrt(nu)
    # two solutions of the same problem in different units (EST-A-209): the sum of the two certified errors, the second with twice the representation error
    out["B_pair"] = (out["B_test"] - CERT_RELATIVE) + 4.0 * out["q1"] * (theta_N(n, m, 2 * ETA_REP) + math.sqrt(n) * (gamma(m) + 2 * ETA_REP) * r / Dx) if Dx > 0 else math.inf
    # the weakest direction: the eigenvector of the smallest eigenvalue of N_s (exact), its gap, and the perturbation bound on its angle
    evs, vecs = jacobi_eigs([[dec(Nse[i][j]) for j in range(n)] for i in range(n)], vectors=True)
    v = vecs[0]
    big = max(range(n), key=lambda i: abs(v[i]))
    if v[big] < 0:
        v = [-t for t in v]
    out["weakest_exact"] = [float(t) for t in v]
    out["gap"] = float(evs[0] / evs[1]) if n > 1 else 0.0                  # lambda_2(Q_s) / lambda_1(Q_s)
    out["tau_s"] = 2.0 * tN0 / out["lam_min_Ns"]                           # ||Q-hat_s - Q_s||_2 / ||Q_s||_2 <= tau_s (scaled diagonal below 2)
    out["cos_min"] = math.sqrt(max(0.0, 1.0 - (out["tau_s"] / (1.0 - out["gap"])) ** 2)) if (n > 1 and out["tau_s"] / (1.0 - out["gap"]) < 1.0) else 0.0
    rho_exact = []
    for i in range(n):
        for j in range(i + 1, n):
            rho_exact.append(float(dec(Qse[i][j]) / (dec(Qse[i][i]) * dec(Qse[j][j])).sqrt()))
    out["rho_exact"] = rho_exact
    return out


def predicted_unscaled_rule(cache: Path, name: str) -> dict:
    """The control of EST-A-209: the ratio of the smallest to the largest Cholesky pivot of the UNSCALED normal matrix, in double (the decision rule U)."""
    P = read_nist(cache, name)
    Xd, yd = design(name, P["rows"], False)
    Nh, _ = normal_double(Xd, yd)
    n = len(Nh)
    L, brk = cholesky_double(Nh)
    if brk is not None:
        return dict(ratio=None, breakdown=brk[0])
    piv = [L[i][i] ** 2 for i in range(n)]
    return dict(ratio=min(piv) / max(piv), pivots=piv, breakdown=None)


def equilibrated_pontius_rows(cache: Path):
    """Pontius with every design column divided by its own 2-norm in extended precision, rounded to double: the SAME problem in different units."""
    P = read_nist(cache, "Pontius")
    Xd, yd = design("Pontius", P["rows"], False)
    n = len(Xd[0])
    norms = [math.sqrt(sum(float(np.longdouble(Xd[k][j]) ** 2) for k in range(len(Xd)))) for j in range(n)]
    return [[Xd[k][j] / norms[j] for j in range(n)] for k in range(len(Xd))], yd, norms


# ------------------------------------------------------------------------------------------------------------------------------------ Demmel's example

DEMMEL_A = [[F(1), F(-11, 100), F(24, 100), F(-34, 100)], [F(-11, 100), F(1), F(7, 100), F(30, 100)],
            [F(24, 100), F(7, 100), F(1), F(65, 100)], [F(-34, 100), F(30, 100), F(65, 100), F(1)]]
DEMMEL_B = [F(42), F(-26), F(24), F(34)]
DEMMEL_D = [F(1), F(10) ** 5, F(10) ** -10, F(10) ** 15]
DEMMEL_P = [Decimal(".3641849059026662").copy_negate(), Decimal(".09299067506030982"), Decimal(".7002799484168281"), Decimal("-.6069020369959377")]


def demmel() -> dict:
    n = 4
    H = [[DEMMEL_D[i] * DEMMEL_A[i][j] * DEMMEL_D[j] for j in range(n)] for i in range(n)]
    x = exact_solve(H, DEMMEL_B)
    y = [DEMMEL_D[i] * x[i] for i in range(n)]
    nrm = sum(dec(v) ** 2 for v in y).sqrt()
    p = [dec(v) / nrm for v in y]
    # double matrix as the test builds it: H_ij in long double from the decimals, rounded once
    ld = np.longdouble
    Hd = [[float(ld(str(DEMMEL_A[i][j].numerator)) / ld(str(DEMMEL_A[i][j].denominator)) * ld(["1", "1e5", "1e-10", "1e15"][i]) * ld(["1", "1e5", "1e-10", "1e15"][j]))
           for j in range(n)] for i in range(n)]
    e = [scale_exponent(Hd[i][i]) for i in range(n)]
    Hs = [[F(Hd[i][j]) * F(2) ** (-(e[i] + e[j])) for j in range(n)] for i in range(n)]
    Qs = exact_inverse(Hs)
    q1 = max(sum(abs(Qs[i][j]) for i in range(n)) for j in range(n))
    dd = [dec(H[i][i]).sqrt() for i in range(n)]
    A = [[dec(H[i][j]) / (dd[i] * dd[j]) for j in range(n)] for i in range(n)]
    evA = jacobi_eigs(A)
    evH = jacobi_eigs([[dec(H[i][j]) for j in range(n)] for i in range(n)])
    evNs = jacobi_eigs([[dec(Hs[i][j]) for j in range(n)] for i in range(n)])
    eta_h = 1.002 * U
    tN = n * eta_h + theta_solve(n)
    dx = nrm
    return dict(e=e, p=[float(v) for v in p], kappa_A=float(evA[-1] / evA[0]), kappa_H=float(evH[-1] / evH[0]), kappa_Ns=float(evNs[-1] / evNs[0]),
                lam_min_A=float(evA[0]), lam_max_A=float(evA[-1]), q1=float(dec(q1)), theta_N=tN, B=4.0 * float(dec(q1)) * tN, B_exactA=tN / float(evA[0]),
                published_bound=1.5e-14, published_vector=[float(v) for v in DEMMEL_P], x=[float(dec(v)) for v in x])


# --------------------------------------------------------------------------------------------------------------------------------- synthetic problems

def syn_dependent() -> dict:
    """Three parameters a, b, c; c is a DUPLICATE of a; every scaled entry is a small dyadic number and every square root exact, so the third pivot is exactly 0."""
    a = [1.0, 1.0, 1.0, 1.0]; b = [1.0, 1.0, -1.0, -1.0]; c = a[:]
    X = [[a[k], b[k], c[k]] for k in range(4)]
    y = [1.0, 2.0, 3.0, 4.0]
    N, bb = normal_double(X, y)
    e = [scale_exponent(N[i][i]) for i in range(3)]
    Ns = [[math.ldexp(N[i][j], -(e[i] + e[j])) for j in range(3)] for i in range(3)]
    L, brk = cholesky_double(Ns)
    return dict(e=e, N=[row[:] for row in N], breakdown_index=brk[0], breakdown_pivot=brk[1], dependency=[1.0, 0.0])


def syn_nearly_dependent() -> dict:
    h = 2.0 ** -23
    a1 = [1.0, 1.0, 1.0, 1.0]; a2 = [1.0 + h, 1.0 - h, 1.0, 1.0]
    X = [[a1[k], a2[k]] for k in range(4)]
    y = [1.0, 0.0, 0.0, 0.0]
    N, b = normal_double(X, y)
    e = [scale_exponent(N[i][i]) for i in range(2)]
    Ns = [[F(N[i][j]) * F(2) ** (-(e[i] + e[j])) for j in range(2)] for i in range(2)]
    Qs = exact_inverse(Ns)
    q1 = max(sum(abs(Qs[i][j]) for i in range(2)) for j in range(2))
    ev = jacobi_eigs([[dec(Ns[i][j]) for j in range(2)] for i in range(2)])
    r = math.sqrt(sum(v * v for v in y))
    x = exact_solve([[F(N[i][j]) for j in range(2)] for i in range(2)], [F(v) for v in b])
    Dx = float(dec(sum(F(N[i][i]) * x[i] * x[i] for i in range(2))).sqrt())
    tN = theta_N(2, 4, 0.0)
    B = 4.0 * float(dec(q1)) * (tN + math.sqrt(2) * gamma(4) * r / Dx)
    return dict(e=e, kappa=float(ev[-1] / ev[0]), lam_min_Ns=float(ev[0]), q1=float(dec(q1)), B=B, theta_N=tN)


def tridiagonal(n: int) -> dict:
    """Rows e_k - e_{k-1} (k = 1 .. n+1): N = tridiag(2, -1) exactly; Q_ij = min(i,j) (n+1-max(i,j)) / (n+1); lambda_min = 2 - 2 cos(pi/(n+1))."""
    lam_min = 2.0 - 2.0 * math.cos(math.pi / (n + 1))
    lam_min_A = lam_min / 2.0                       # the diagonal is 2: A = N / 2
    return dict(n=n, m=n + 1, theta_s=theta_solve(n), lam_min_A=lam_min_A, B_col=theta_solve(n) / lam_min_A)


# --------------------------------------------------------------------------------------------------------------------------------------- the hatch

def hatch_filip(cache: Path) -> dict:
    """EST-A-210: Filip, certified yet beyond normal equations in double, with the five highest-order parameters ELIMINATED at their certified values.  The five are fixed at
    values that ARE the optimum (to the 15 printed digits), so the six that remain must come out as the certified B0 .. B5, with a condition number small enough to solve."""
    P = read_nist(cache, "Filip")
    Xe, ye = design("Filip", P["rows"], True)
    Xd, yd = design("Filip", P["rows"], False)
    m, nfull = len(Xd), 11
    E = [6, 7, 8, 9, 10]
    Fi = [0, 1, 2, 3, 4, 5]
    nf, ne = len(Fi), len(E)
    v_dec = [Decimal(P["cert"][j]) for j in E]
    v_exact = [F(t) for t in v_dec]
    v_dbl = [float(t) for t in v_dec]                                   # strtod: the nearest double
    # exact reduced problem
    rprime = [ye[k] - sum(Xe[k][E[i]] * v_exact[i] for i in range(ne)) for k in range(m)]
    Nff = [[sum(Xe[k][i] * Xe[k][j] for k in range(m)) for j in Fi] for i in Fi]
    bf = [sum(Xe[k][i] * rprime[k] for k in range(m)) for i in Fi]
    x = exact_solve(Nff, bf)
    # the double recipe: r'_k = r_k - sum_e a_ke v_e in E order, sequential; then the forming of N_FF and b_F as everywhere
    rp_d = []
    for k in range(m):
        acc = yd[k]
        for i in range(ne):
            acc = acc - Xd[k][E[i]] * v_dbl[i]
        rp_d.append(acc)
    XF = [[Xd[k][i] for i in Fi] for k in range(m)]
    Nh, bh = normal_double(XF, rp_d)
    e = [scale_exponent(Nh[i][i]) for i in range(nf)]
    Nsd = [[math.ldexp(Nh[i][j], -(e[i] + e[j])) for j in range(nf)] for i in range(nf)]
    L, brk = cholesky_double(Nsd)
    Nse = [[Nff[i][j] * F(2) ** (-(e[i] + e[j])) for j in range(nf)] for i in range(nf)]
    Qse = exact_inverse(Nse)
    q1 = max(sum(abs(Qse[i][j]) for i in range(nf)) for j in range(nf))
    dd = [dec(Nff[i][i]).sqrt() for i in range(nf)]
    A = [[dec(Nff[i][j]) / (dd[i] * dd[j]) for j in range(nf)] for i in range(nf)]
    evA = jacobi_eigs(A)
    evNs = jacobi_eigs([[dec(Nse[i][j]) for j in range(nf)] for i in range(nf)])
    Dx = float(dec(sum(Nff[i][i] * x[i] * x[i] for i in range(nf))).sqrt())
    r_orig = float(dec(sum(v * v for v in ye)).sqrt())
    r_mod = float(dec(sum(v * v for v in rprime)).sqrt())
    mag = float(dec(sum((sum(abs(Xe[k][E[i]]) * abs(v_exact[i]) for i in range(ne))) ** 2 for k in range(m))).sqrt())
    # D_E v from the FULL exact normal matrix diagonal
    dE = [dec(sum(Xe[k][j] * Xe[k][j] for k in range(m))).sqrt() for j in E]
    DEv = float(sum((dE[i] * dec(v_exact[i])) ** 2 for i in range(ne)).sqrt())
    tN0 = theta_N(nf, m, 0.0)
    tNt = theta_N(nf, m, ETA_REP)
    bterm0 = math.sqrt(nf) * (gamma(m) * r_mod + gamma(2 * ne) * (r_orig + mag)) / Dx
    bterm_t = math.sqrt(nf) * ((gamma(m) + ETA_REP) * r_mod + (gamma(2 * ne) + ETA_REP) * (r_orig + mag)) / Dx
    cert_term = math.sqrt(nf * ne) * CERT_RELATIVE * DEv / Dx
    out = dict(free=Fi, eliminated=E, n_free=nf, e=e, breakdown_index=None if brk is None else brk[0], kappa=float(evNs[-1] / evNs[0]), lam_min_A=float(evA[0]),
               q1=float(dec(q1)), Dx=Dx, r_orig=r_orig, r_mod=r_mod, mag=mag, DEv=DEv, dof=m - nf,
               B_code=4.0 * float(dec(q1)) * (tN0 + bterm0),
               B_test=4.0 * float(dec(q1)) * (tNt + bterm_t + cert_term) + CERT_RELATIVE,
               digits_exact_vs_cert=min(float(-((dec(x[i]) - Decimal(P["cert"][i])).copy_abs() / Decimal(P["cert"][i]).copy_abs()).log10())
                                        if dec(x[i]) != Decimal(P["cert"][i]) else 15.0 for i in range(nf)),
               checksum_reduced=checksum([[Xd[k][i] for i in range(nfull)] for k in range(m)], yd))
    return out


# --------------------------------------------------------------------------------------------------------------------------------------- the header

def hexf(x) -> str:
    if x is None:
        return "0.0"
    if isinstance(x, float) and math.isinf(x):
        return "HUGE_VAL" if x > 0 else "-HUGE_VAL"
    return float(x).hex()


def arr(vals, width, conv=hexf) -> str:
    vals = list(vals) + [0 if conv is str else 0.0] * (width - len(vals))
    return "{" + ", ".join(conv(v) for v in vals) + "}"


def build_header(rep: dict) -> str:
    L = [
        "// GENERATED by tools/estimation_sizing.py -- do not edit.",
        "//",
        "// The FROZEN FIGURES of SPEC-estimation 6 (L7 step 2), registered before any code of the estimator existed: the rows' bit checksums, the exact condition numbers, the",
        "// scale exponents, the derived bounds, the predictions of the refusals.  Every double is a hexadecimal float, so the header carries the generator's values exactly.",
        "#pragma once",
        "",
        "#include <cmath>",
        "",
        "namespace odl::estimation::registered {",
        "",
        "inline constexpr int kMaxN = 11;",
        "inline constexpr double kUnitRoundoff = 0x1p-53;",
        f"inline constexpr double kEtaRep = {hexf(rep['eta_rep'])};              // 2.01 u: a decimal datum rounded to double through long double, doubled for a product",
        f"inline constexpr double kCertifiedRelative = {hexf(rep['cert_relative'])};   // one unit of the 15th printed digit, worst case",
        "",
        "struct NistFigures {",
        "    const char* name;",
        "    int n, m;",
        "    unsigned long long checksum;      ///< sum of the bit patterns of every design entry and response, in row order, mod 2^64",
        "    int e[kMaxN];                     ///< the scale exponents (EST-R-201)",
        "    double kappa, lam_min_a, lam_max_a, q1, dx_norm, r_norm;",
        "    double theta_n_code, theta_n_test, b_code, b_test, b_q, b_rho, b_sigma, sigma0_exact;",
        "    double b_pair, gap, tau_s, cos_min;   ///< EST-A-209 (two units), EST-A-212 (the weakest direction's angle bound)",
        "    double kappa_i[kMaxN];",
        "    double qdiag[kMaxN];              ///< (N^-1)_ii in the units of the parameters",
        "    double weakest_exact[kMaxN];      ///< the exact eigenvector of the smallest eigenvalue of N_s, largest component positive",
        "    double rho_exact[kMaxN * (kMaxN - 1) / 2];   ///< the exact correlations, row-major upper triangle",
        "    int breakdown_index;              ///< -1: the Cholesky factorisation of the scaled matrix completes",
        "    double breakdown_pivot;",
        "};",
        "",
        "inline constexpr NistFigures kNist[] = {",
    ]
    for name, _, _ in NIST:
        d = rep["datasets"][name]
        bi = -1 if d["breakdown_index"] is None else d["breakdown_index"]
        L.append(f"    {{\"{name}\", {d['n']}, {d['m']}, {d['checksum']}ULL, {arr(d['e'], 11, str)},")
        L.append(f"     {hexf(d['kappa'])}, {hexf(d['lam_min_A'])}, {hexf(d['lam_max_A'])}, {hexf(d['q1'])}, {hexf(d['Dx'])}, {hexf(d['r'])},")
        L.append(f"     {hexf(d['theta_N_code'])}, {hexf(d['theta_N_test'])}, {hexf(d['B_code'])}, {hexf(d['B_test'])}, {hexf(d['B_Q'])}, {hexf(d['B_rho'])}, {hexf(d['B_sigma'])}, {hexf(d['sigma0_exact'])},")
        L.append(f"     {hexf(d['B_pair'])}, {hexf(d['gap'])}, {hexf(d['tau_s'])}, {hexf(d['cos_min'])},")
        L.append(f"     {arr(d['kappa_i'], 11)},")
        L.append(f"     {arr(d['qdiag'], 11)},")
        L.append(f"     {arr(d['weakest_exact'], 11)},")
        L.append(f"     {arr(d['rho_exact'], 55)},")
        L.append(f"     {bi}, {hexf(d['breakdown_pivot'])}}},")
    L += ["};", ""]
    ht = rep["hatch_filip"]
    L += [
        "/// Filip with B6 .. B10 ELIMINATED at their certified values (EST-A-210).",
        "struct HatchFigures { int n_free, dof; int e[kMaxN]; int breakdown_index; double kappa, lam_min_a, q1, dx_norm, r_orig, r_mod, magnitude, d_e_v, b_code, b_test, digits_exact_vs_certified; };",
        f"inline constexpr HatchFigures kHatchFilip = {{{ht['n_free']}, {ht['dof']}, {arr(ht['e'], 11, str)}, {-1 if ht['breakdown_index'] is None else ht['breakdown_index']},",
        f"    {hexf(ht['kappa'])}, {hexf(ht['lam_min_A'])}, {hexf(ht['q1'])}, {hexf(ht['Dx'])}, {hexf(ht['r_orig'])}, {hexf(ht['r_mod'])}, {hexf(ht['mag'])}, {hexf(ht['DEv'])},",
        f"    {hexf(ht['B_code'])}, {hexf(ht['B_test'])}, {hexf(ht['digits_exact_vs_cert'])}}};",
        "",
    ]
    ru = rep["rule_U"]
    L += [
        "/// The unscaled decision rule U of EST-A-209: min pivot / max pivot of the Cholesky factor of the UNSCALED normal matrix.",
        f"inline constexpr double kRuleUPontius = {hexf(ru['Pontius']['ratio'])};",
        f"inline constexpr double kRuleUPontiusEquilibrated = {hexf(ru['Pontius_equilibrated']['ratio'])};",
        "",
    ]
    dm = rep["demmel"]
    L += [
        "/// Demmel's worked example (LAPACK Working Note 14, page 4): H = D A D, D = diag(1, 1e5, 1e-10, 1e15), b = (42, -26, 24, 34).",
        "struct DemmelFigures { int e[4]; double p[4]; double kappa_a, lam_min_a, lam_max_a, kappa_h, kappa_ns, q1, theta_n, b, b_exact_a, published_bound; };",
        f"inline constexpr DemmelFigures kDemmel = {{{arr(dm['e'], 4, str)}, {arr(dm['p'], 4)},",
        f"    {hexf(dm['kappa_A'])}, {hexf(dm['lam_min_A'])}, {hexf(dm['lam_max_A'])}, {hexf(dm['kappa_H'])}, {hexf(dm['kappa_Ns'])}, {hexf(dm['q1'])}, {hexf(dm['theta_N'])}, {hexf(dm['B'])}, {hexf(dm['B_exactA'])}, {hexf(dm['published_bound'])}}};",
        "",
    ]
    sd, sn = rep["syn_dependent"], rep["syn_nearly_dependent"]
    L += [
        "/// SYN-dependent: three parameters, the third a DUPLICATE of the first (EST-A-211).",
        f"inline constexpr int kSynDependentBreakdownIndex = {sd['breakdown_index']};",
        f"inline constexpr double kSynDependentBreakdownPivot = {hexf(sd['breakdown_pivot'])};",
        "/// SYN-nearly-dependent: two parameters whose columns differ by 2^-23 in two entries (EST-A-211).",
        f"inline constexpr double kSynNearKappa = {hexf(sn['kappa'])};",
        f"inline constexpr double kSynNearLamMin = {hexf(sn['lam_min_Ns'])};",
        f"inline constexpr double kSynNearQ1 = {hexf(sn['q1'])};",
        f"inline constexpr double kSynNearB = {hexf(sn['B'])};",
        "",
        "/// The tridiagonal(2, -1) problems of EST-A-203: n, Theta_s(n), lambda_min(A), the per-column bound Theta_s / lambda_min(A).",
        "struct TridiagonalFigures { int n; double theta_s, lam_min_a, b_col; };",
        "inline constexpr TridiagonalFigures kTridiagonal[] = {",
    ]
    for n, t in rep["tridiagonal"].items():
        L.append(f"    {{{n}, {hexf(t['theta_s'])}, {hexf(t['lam_min_A'])}, {hexf(t['B_col'])}}},")
    L += ["};", "", "}  // namespace odl::estimation::registered", ""]
    return "\n".join(L)


# ---------------------------------------------------------------------------------------------------------------------------------- the specification's table

BEGIN = "<!-- BEGIN estimation_sizing: generated by tools/estimation_sizing.py --spec-table; do not edit by hand -->"
END = "<!-- END estimation_sizing -->"


def sci(x: float, digits: int = 3) -> str:
    if x == 0 or x is None:
        return "0"
    if math.isinf(x):
        return "∞"
    m, e = f"{x:.{digits - 1}e}".split("e")
    e = int(e)
    sup = str(e).translate(str.maketrans("-0123456789", "⁻⁰¹²³⁴⁵⁶⁷⁸⁹"))
    return f"{m} × 10{sup}" if e != 0 else m


def build_spec_table(rep: dict) -> str:
    L = [BEGIN, "",
         "**The registered figures, per dataset** (NIST StRD linear regression, read from the pinned cache; `m` rows, `n` parameters; the design recipe of §8.2; `u = 2⁻⁵³`). "
         "`e` are the scale exponents of `EST-R-201`; `κ` is the exact `κ₂(N_s)` of the power-of-two-scaled matrix; `κᵢ = Nᵢᵢ (N⁻¹)ᵢᵢ`; `q₁ = ‖Q_s‖₁`; `Θ_N` the forming-and-solving "
         "constant of `EST-R-206` at η = 0; `B`(code) the relative error the solve certifies; `B`(test) the same with the representation error of decimal data and the certified "
         "values' own last digit added; `B_Q` the bound on `|Q̂ᵢᵢ/Qᵢᵢ − 1|`; `B_σ` the bound on `|σ̂₀ − σ₀|`; the last column what the factorisation of the scaled matrix does.", "",
         "| dataset | `n × m` | `e` | `κ` | `min κᵢ` – `max κᵢ` | `q₁` | `Θ_N` | `B` (code) | `B` (test) | `B_Q` | `B_σ` | outcome |",
         "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for name, _, _ in NIST:
        d = rep["datasets"][name]
        if d["breakdown_index"] is not None:
            outcome = f"**EST-F-202 at `B{d['breakdown_index']}`**, pivot {sci(d['breakdown_pivot'], 2)}"
            Bc = Bt = BQ = Bs = "—"
        else:
            outcome = "completes"
            Bc, Bt, BQ, Bs = sci(d["B_code"]), sci(d["B_test"]), sci(d["B_Q"]), sci(d["B_sigma"])
        ks = d["kappa_i"]
        L.append(f"| {name} | {d['n']} × {d['m']} | {', '.join(str(v) for v in d['e'])} | {sci(d['kappa'])} | {sci(min(ks))} – {sci(max(ks))} | {sci(d['q1'])} | {sci(d['theta_N_code'])} | "
                 f"{Bc} | {Bt} | {BQ} | {Bs} | {outcome} |")
    ht = rep["hatch_filip"]
    L += ["",
          f"**The hatch (`EST-A-210`):** Filip with `B6 … B10` eliminated at their certified values — `n_free` = {ht['n_free']}, `e` = {', '.join(str(v) for v in ht['e'][:ht['n_free']])}, the factorisation completes, "
          f"`κ` = {sci(ht['kappa'])}, `q₁` = {sci(ht['q1'])}, `B` (code) = {sci(ht['B_code'])}, `B` (test) = {sci(ht['B_test'])}, ν = {ht['dof']}; the exact reduced solution agrees with the "
          f"certified `B0 … B5` to **{ht['digits_exact_vs_cert']:.1f} digits**.", ""]
    ru = rep["rule_U"]
    L += [f"**The unscaled decision `U` (`EST-A-209`)** — the smallest over the largest pivot of the Cholesky factor of the UNSCALED normal matrix, in double: Pontius {sci(ru['Pontius']['ratio'], 3)}; "
          f"the same problem with every design column divided by its own norm {sci(ru['Pontius_equilibrated']['ratio'], 3)}. `U` refuses below `n·2⁻⁵²` ({sci(3 * 2.0 ** -52, 3)} at n = 3).", ""]
    dm = rep["demmel"]
    L += [f"**Demmel's example (`EST-A-208`):** `e` = {', '.join(str(v) for v in dm['e'])}; **exact** κ₂(A) = {dm['kappa_A']:.4f} (λ_max = {dm['lam_max_A']:.4f}, λ_min = {dm['lam_min_A']:.4f}) — the page prints "
          f"“κ(A) ≈ 2.0”, which is **not** the 2-norm condition number of the printed A, and the gate does not use it; κ₂(H) = {sci(dm['kappa_H'])} (the page: ≈ 10⁵⁰), κ₂(N_s) = {dm['kappa_Ns']:.4f}, "
          f"`q₁` = {dm['q1']:.4f}, `Θ_N` = {sci(dm['theta_N'])} (η_H = 1.002 u for the matrix entries, none for b), `B` = {sci(dm['B'])} (and {sci(dm['B_exactA'])} with ‖A⁻¹‖₂ in place of 4 q₁); "
          f"the printed bound is {sci(dm['published_bound'], 2)}.", ""]
    sd, sn = rep["syn_dependent"], rep["syn_nearly_dependent"]
    L += [f"**Synthetic (`EST-A-211`):** `SYN-dependent` — `e` = {', '.join(str(v) for v in sd['e'])}, the third pivot is exactly {sd['breakdown_pivot']:g}, refusal at index {sd['breakdown_index']} "
          f"with the dependency `c ≈ 1·a + 0·b`; `SYN-nearly-dependent` — `e` = {', '.join(str(v) for v in sn['e'])}, κ = {sci(sn['kappa'])}, λ_min(N_s) = {sci(sn['lam_min_Ns'])}, "
          f"`q₁` = {sci(sn['q1'])}, `B` = {sn['B']:.3f} (≥ ½: EST-F-203).", "",
          "**The tridiagonal problems (`EST-A-203`),** rows `eₖ − eₖ₋₁`: `n`, `Θ_s(n)`, `λ_min(A) = (1 − cos(π/(n+1)))`, the per-column bound `B_col = Θ_s / λ_min(A)`:", "",
          "| `n` | `Θ_s` | `λ_min(A)` | `B_col` |", "|---|---|---|---|"]
    for n, t in rep["tridiagonal"].items():
        L.append(f"| {n} | {sci(t['theta_s'])} | {sci(t['lam_min_A'])} | {sci(t['B_col'])} |")
    L += ["", END]
    return "\n".join(L)


# ------------------------------------------------------------------------------------------------------------------------------------------ the report

def build_report(cache: Path) -> dict:
    rep = {"datasets": {}, "u": U, "eta_rep": ETA_REP, "cert_relative": CERT_RELATIVE}
    for name, _, _ in NIST:
        rep["datasets"][name] = analyse(cache, name)
    rep["rule_U"] = {"Pontius": predicted_unscaled_rule(cache, "Pontius")}
    Xq, yq, norms = equilibrated_pontius_rows(cache)
    Nq, _ = normal_double(Xq, yq)
    Lq, brk = cholesky_double(Nq)
    rep["rule_U"]["Pontius_equilibrated"] = dict(ratio=(min(Lq[i][i] ** 2 for i in range(3)) / max(Lq[i][i] ** 2 for i in range(3))) if brk is None else None, breakdown=None if brk is None else brk[0])
    rep["hatch_filip"] = hatch_filip(cache)
    rep["demmel"] = demmel()
    rep["syn_dependent"] = syn_dependent()
    rep["syn_nearly_dependent"] = syn_nearly_dependent()
    rep["tridiagonal"] = {n: tridiagonal(n) for n in (1, 2, 5, 10, 20, 40)}
    return rep


def print_report(rep: dict) -> None:
    print(f"u = 2^-53 = {rep['u']:.4e}; eta_rep = {rep['eta_rep']:.4e}; certified values' own uncertainty = {rep['cert_relative']:.0e}\n")
    print(f"{'dataset':9s} {'n':>2s} {'m':>3s} {'kappa(N_s)':>11s} {'lam_min(A)':>11s} {'||Q_s||_1':>10s} {'Theta_N':>9s} {'B (code)':>10s} {'B (test)':>10s} {'B_Q':>9s} {'B_sigma':>9s}  cert.digits  refusal")
    for name, d in rep["datasets"].items():
        ref = "-" if d["breakdown_index"] is None else f"EST-F-202 at B{d['breakdown_index']} (pivot {d['breakdown_pivot']:.3e})"
        if d["breakdown_index"] is None and d["certified_no_digit"]:
            ref = "EST-F-003"
        print(f"{name:9s} {d['n']:2d} {d['m']:3d} {d['kappa']:11.4e} {d['lam_min_A']:11.4e} {d['q1']:10.3e} {d['theta_N_code']:9.2e} {d['B_code']:10.3e} {d['B_test']:10.3e} "
              f"{d['B_Q']:9.2e} {d['B_sigma']:9.2e}  {d['digits_exact_vs_cert']:6.1f}      {ref}")
    print("\nscale exponents e (EST-R-201) and per-parameter kappa_i = N_ii (N^-1)_ii:")
    for name, d in rep["datasets"].items():
        print(f"  {name:9s} e = {d['e']}   kappa_i = {[f'{v:.4g}' for v in d['kappa_i']]}")
    print("\nrule U (the unscaled decision of EST-A-209): ratio of the smallest to the largest Cholesky pivot of the UNSCALED N:")
    for k, v in rep["rule_U"].items():
        print(f"  {k:24s} {v}")
    ht = rep["hatch_filip"]
    print(f"\nthe hatch (Filip, B6 .. B10 eliminated at the certified values): e = {ht['e']}, breakdown = {ht['breakdown_index']}, kappa(N_s) = {ht['kappa']:.4e}, ||Q_s||_1 = {ht['q1']:.3e}, "
          f"B(code) = {ht['B_code']:.3e}, B(test) = {ht['B_test']:.3e}, dof = {ht['dof']}, exact reduced solution vs certified: {ht['digits_exact_vs_cert']:.1f} digits")
    dm = rep["demmel"]
    print(f"\nDemmel's example: e = {dm['e']}, kappa(A) = {dm['kappa_A']:.4f} (lambda_max = {dm['lam_max_A']:.4f}), kappa(H) = {dm['kappa_H']:.4e}, kappa(N_s) = {dm['kappa_Ns']:.4f}, ||Q_s||_1 = {dm['q1']:.4e}, "
          f"Theta_N = {dm['theta_N']:.3e}, B = {dm['B']:.3e} (with 4 ||Q_s||_1) and {dm['B_exactA']:.3e} (with ||A^-1||_2), published bound {dm['published_bound']:.1e}")
    sd = rep["syn_dependent"]
    print(f"\nSYN-dependent: e = {sd['e']}, breakdown at parameter index {sd['breakdown_index']} with pivot {sd['breakdown_pivot']}, dependency c ~ {sd['dependency']} (a, b)")
    sn = rep["syn_nearly_dependent"]
    print(f"SYN-nearly-dependent: e = {sn['e']}, kappa(N_s) = {sn['kappa']:.4e}, lam_min(N_s) = {sn['lam_min_Ns']:.4e}, ||Q_s||_1 = {sn['q1']:.4e}, Theta_N = {sn['theta_N']:.3e}, B = {sn['B']:.3f}")
    print("\ntridiagonal (rows e_k - e_{k-1}): n, Theta_s, lambda_min(A), B_col = Theta_s / lambda_min(A):")
    for n, t in rep["tridiagonal"].items():
        print(f"  n = {n:3d}  Theta_s = {t['theta_s']:.3e}  lam_min(A) = {t['lam_min_A']:.4e}  B_col = {t['B_col']:.3e}")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="estimation_sizing.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--header", type=Path, default=None, help="write the C++ header of frozen figures")
    ap.add_argument("--verify", type=Path, default=None, help="exit 1 unless this committed header is exactly what the generator emits")
    ap.add_argument("--spec-table", action="store_true", help="print the specification's generated table block")
    ap.add_argument("--verify-spec", type=Path, default=None, help="exit 1 unless this specification's generated block is exactly what the generator emits")
    args = ap.parse_args(argv)
    try:
        rep = build_report(args.cache)
    except FileNotFoundError as exc:
        print(f"estimation_sizing.py: {exc}", file=sys.stderr)
        return 2
    if args.spec_table:
        print(build_spec_table(rep))
        return 0
    if args.verify_spec:
        doc = args.verify_spec.read_text(encoding="utf-8")
        i, j = doc.find(BEGIN), doc.find(END)
        ok = i >= 0 and j > i and doc[i:j + len(END)] == build_spec_table(rep)
        print("ok       the specification's generated block is exactly what the generator emits" if ok
              else "MISMATCH the specification's generated block differs from the generator's output (or is missing)", file=sys.stderr)
        return 0 if ok else 1
    if args.verify:
        text = build_header(rep)
        ok = args.verify.read_text(encoding="utf-8") == text
        print("ok       the committed header is exactly what the generator emits" if ok
              else "MISMATCH the committed header differs from the generator's output (a figure moved: the registration is not what it was)", file=sys.stderr)
        return 0 if ok else 1
    if args.header:
        text = build_header(rep)
        args.header.write_text(text, encoding="utf-8")
        print(f"wrote {len(text.splitlines())} lines to {args.header}", file=sys.stderr)
        return 0
    print_report(rep)
    return 0


if __name__ == "__main__":
    sys.exit(main())
