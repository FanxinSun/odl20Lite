#!/usr/bin/env python3
"""penumbral_cancellation.py — the tool behind SHDW-R-031 and SHDW-R-033.

SPEC-shadow §3.4, §8 Coverage. NOT a gate: `SHDW-Q-005` defers limb darkening
because nothing in this plan yet samples inside a penumbra passage, so
asserting the MAGNITUDE of a deferred feature in CI would mean carrying a
two-body propagator in the test suite for something not built — over-building
a check for code that does not exist. But a number that is rightly excused
from the gate is not excused from being REPRODUCIBLE (plan §4 rule 3): this
script is what produced every figure R-031 and R-033 state, and running it
regenerates them, rather than leaving frozen numbers with no route back to
what made them (the defect `oracle/capture.sh` had, precisely, after the
merge with the predecessor's tree).

WHAT THIS MEASURES. `SHDW-A-016` establishes that a limb-darkened Sun moves Fs
by up to 1.97e-2 at LEO — the module's "seventh axis". That number is a PEAK,
at one instant. What a consumer that integrates over a whole eclipse passage
actually accumulates is a TIME integral, and the first version of that claim
(committed, then challenged) stated a cancellation figure — 99.9% — without
saying which passage produced it, or what would change it. This script states
the passage precisely and measures three families of geometry against it,
using an ACTUAL two-body Kepler propagator (Newton's method on Kepler's
equation; no small-angle or constant-rate shortcuts for the eccentric cases),
not hand-argument.

Run: `python3 tools/penumbral_cancellation.py`. Takes a few minutes; the
azimuthal ray-casting is vectorised with numpy for exactly that reason. Every
number printed under "=== SUMMARY ===" is a number quoted in SPEC-shadow.md
or PROVENANCE.md; the two should never drift apart, and if they do, this
script — not the spec — is what a reader re-runs to find out which is wrong.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

# --------------------------------------------------------------------------- #
# Constants. SHDW's own — kept numerically identical to conical.hpp /
# perspective.hpp rather than re-derived, since this tool exists to measure
# the MODULE's behaviour, not a different model of the same physics.

AU_M = 1.495978707e11
GM_EARTH = 3.986004415e14          # l2_floors.cpp's own value, EGM2008-consistent
R_EARTH_M = 6378137.0              # conical.hpp's kEarthRadiusM
R_SUN_M = 6.957e8                  # conical.hpp's kSunRadiusM
EDDINGTON_U = 0.6                  # SPEC-shadow SHDW-R-032: I(mu)/I(1) = 1 - u(1-mu), u = 3/5 exactly


# --------------------------------------------------------------------------- #
# The comparator: Fs by ray casting, EXACT in the radial direction (bisection
# on each blocked/unblocked edge), vectorised over azimuth. Identical in
# substance to `by_solid_angle` in modules/shadow/tests/perspective_tests.cpp
# and to the `Sky` class SHDW-A-016 uses — ported here rather than linked
# against, because this tool's whole point is to be runnable standalone, by
# hand, without a build.

@dataclass
class Sky:
    """The Sun's radial brightness, as the cumulative weight out to angle `a`
    from its own disc centre. u=0 is the uniform disc both shadow models
    assume; u=0.6 is Eddington's bolometric law (SHDW-R-032)."""

    a_s: float
    u: float

    def __post_init__(self) -> None:
        self.S = math.sin(self.a_s)
        self.k = math.cos(self.a_s)

    def _F(self, c: float) -> float:
        q = math.sqrt(max(0.0, (c - self.k) * (c + self.k)))
        return 0.5 * c * q - 0.5 * self.k * self.k * math.log(c + q)

    def W(self, a: float) -> float:
        c = math.cos(a)
        if self.u == 0.0:
            return 1.0 - c
        return (1.0 - self.u) * (1.0 - c) + (self.u / self.S) * (self._F(1.0) - self._F(c))


def fs(sun: np.ndarray, sat: np.ndarray, Re: float, u: float, nphi: int = 900) -> float:
    """Fs for a satellite at `sat`, Sun's centre at `sun` (both 3-vectors,
    metres, spherical occulter of radius Re): the fraction of the Sun's disc
    NOT blocked, weighted by brightness profile `u`. Exact in the radial
    direction (each blocked interval's ends found by bisection to double
    precision); the azimuthal sweep is what is discretised, vectorised here
    over ALL `nphi` directions AT ONCE, including the bisections themselves --
    a first version bisected one azimuth at a time in a Python-level loop and
    took 100 s for a single call, which does not survive being run by hand 28
    times.  A ray from the Sun's own centre outward to its limb crosses a
    single convex occulter's boundary AT MOST TWICE (enter, or enter-and-exit),
    so this only ever needs to track up to two crossings per azimuth, and does
    so for every azimuth in the same masked numpy operations."""
    to_sun = sun - sat
    ds = np.linalg.norm(to_sun)
    c = to_sun / ds
    sky = Sky(math.asin(R_SUN_M / ds), u)

    seed = np.array([1.0, 0.0, 0.0]) if abs(c[0]) <= abs(c[1]) and abs(c[0]) <= abs(c[2]) else (
        np.array([0.0, 1.0, 0.0]) if abs(c[1]) <= abs(c[2]) else np.array([0.0, 0.0, 1.0]))
    e1 = np.cross(seed, c)
    e1 /= np.linalg.norm(e1)
    e2 = np.cross(c, e1)

    rr = float(sat @ sat)
    Re2 = Re * Re

    def hit(al: np.ndarray, ph: np.ndarray) -> np.ndarray:
        ca, sa = np.cos(al), np.sin(al)
        cp, sp = np.cos(ph), np.sin(ph)
        wx = ca * c[0] + sa * (cp * e1[0] + sp * e2[0])
        wy = ca * c[1] + sa * (cp * e1[1] + sp * e2[1])
        wz = ca * c[2] + sa * (cp * e1[2] + sp * e2[2])
        wr = wx * sat[0] + wy * sat[1] + wz * sat[2]
        return (wr * wr - (rr - Re2) >= 0.0) & (wr < 0.0)

    kRad = 220
    ph = (np.arange(nphi) + 0.5) * (2.0 * math.pi / nphi)
    al_grid = sky.a_s * np.arange(0, kRad + 1) / kRad          # includes al=0
    AL, PH = np.meshgrid(al_grid, ph, indexing="xy")           # (nphi, kRad+1)
    blocked_grid = hit(AL, PH)
    state0 = blocked_grid[:, 0]

    def first_transition(after_state: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        """For each azimuth, the coarse-grid index of the first sample whose
        state differs from `after_state`'s current running state -- found by
        a vectorised argmax on a boolean "differs" mask -- and whether one
        exists at all. Used twice: once from al=0, once again scanning past
        the first found edge for a possible second one."""
        changed = blocked_grid != after_state[:, None]
        has = changed.any(axis=1)
        idx = np.where(has, np.argmax(changed, axis=1), kRad)
        return idx, has

    def bisect_vec(lo: np.ndarray, hi: np.ndarray, at_lo: np.ndarray, active: np.ndarray) -> np.ndarray:
        lo, hi = lo.copy(), hi.copy()
        for _ in range(55):
            mid = 0.5 * (lo + hi)
            cur = hit(mid, ph)
            take_hi = (cur == at_lo) & active
            lo = np.where(take_hi, mid, lo)
            hi = np.where(take_hi, hi, np.where(active, mid, hi))
        return 0.5 * (lo + hi)

    # --- first crossing, from al=0's state
    idx1, has1 = first_transition(state0)
    idx1c = np.clip(idx1, 1, kRad)
    lo1 = al_grid[idx1c - 1]
    hi1 = al_grid[idx1c]
    edge1 = bisect_vec(lo1, hi1, state0, has1)

    # --- second crossing, if any: scan the grid strictly past idx1 for a
    # state differing from the POST-first-crossing state (~state0)
    state_after1 = ~state0
    col = np.arange(kRad + 1)[None, :]
    past_first = col > idx1[:, None]
    changed2 = (blocked_grid != state_after1[:, None]) & past_first
    has2 = has1 & changed2.any(axis=1)
    idx2 = np.where(has2, np.argmax(changed2, axis=1), kRad)
    idx2c = np.clip(idx2, 1, kRad)
    lo2 = np.where(has2, al_grid[np.clip(idx2c - 1, 0, kRad)], 0.0)
    hi2 = np.where(has2, al_grid[idx2c], 0.0)
    edge2 = bisect_vec(lo2, hi2, state_after1, has2)

    # --- accumulate the WEIGHTED (brightness) blocked flux per azimuth.
    # state0 True (blocked at the Sun's own centre direction):
    #   0 crossings -> blocked all the way to a_s
    #   1 crossing  -> blocked from 0 to edge1
    #   2 crossings -> blocked 0..edge1, then unblocked, then BLOCKED AGAIN
    #                  edge2..a_s (the ray re-enters the disc; rare, but the
    #                  same generic edge machinery the C++ version used)
    # state0 False: mirror image (0, or blocked edge1..a_s, or edge1..edge2).
    Wa_s = sky.W(sky.a_s)
    W = np.vectorize(sky.W)
    Wedge1 = np.where(has1, W(edge1), 0.0)
    Wedge2 = np.where(has2, W(edge2), 0.0)

    blocked_flux = np.where(
        state0,
        np.where(has1, np.where(has2, Wedge1 + (Wa_s - Wedge2), Wedge1), Wa_s),
        np.where(has1, np.where(has2, Wedge2 - Wedge1, Wa_s - Wedge1), 0.0),
    )
    total = Wa_s
    return 1.0 - float(np.mean(blocked_flux)) / total


# --------------------------------------------------------------------------- #
# Two-body Kepler propagation. Deliberately NOT the constant-angular-rate
# shortcut a circular orbit would allow: the whole point of this tool is to
# measure what genuine Kepler dynamics do to the cancellation, so it always
# solves Kepler's equation, even at e=0.

@dataclass
class Orbit:
    a: float
    e: float
    peri_hat: np.ndarray   # periapsis direction (unit)
    q_hat: np.ndarray      # in-plane, perpendicular to peri_hat (unit)

    def __post_init__(self) -> None:
        self.n_mean = math.sqrt(GM_EARTH / self.a**3)

    def pos(self, t: float) -> np.ndarray:
        M = self.n_mean * t
        E = M
        for _ in range(80):
            f = E - self.e * math.sin(E) - M
            fp = 1.0 - self.e * math.cos(E)
            E -= f / fp
        r = self.a * (1.0 - self.e * math.cos(E))
        cosnu = (math.cos(E) - self.e) / (1.0 - self.e * math.cos(E))
        sinnu = math.sqrt(max(0.0, 1.0 - self.e**2)) * math.sin(E) / (1.0 - self.e * math.cos(E))
        return r * (cosnu * self.peri_hat + sinnu * self.q_hat)


@dataclass
class Result:
    ok: bool
    net: float = 0.0
    absint: float = 0.0
    worst_running: float = 0.0
    duration: float = 0.0
    cancel_pct: float = 0.0
    swing: float = 0.0
    note: str = ""


def study(a: float, e: float, beta: float, omega_deg: float, *,
         nphi_coarse: int = 150, nphi_fine: int = 450, n_samples: int = 401,
         coarse_n: int = 300) -> Result:
    """One traversal, fully specified by (a, e, beta, omega):

    beta  -- angle between the orbital plane and the Sun direction (0 = the
             shadow axis lies in the orbital plane).
    omega -- argument of periapsis measured from the eclipse-closest
             direction (0 = the eclipse sits exactly at periapsis or
             apoapsis, where the two-body problem's exact time symmetry about
             apsis passage makes the traversal symmetric regardless of e;
             SHDW-R-033's off-apsis case uses omega != 0).

    Returns the net and absolute time integrals of (limb - uniform) Fs across
    ONE side of the penumbral transition -- located by search, not assumed --
    plus the peak of the running integral and the cancellation percentage.
    """
    xhat = np.array([1.0, 0.0, 0.0])
    N = np.array([math.sin(beta), 0.0, math.cos(beta)])
    P0 = xhat - (xhat @ N) * N
    P0 /= np.linalg.norm(P0)
    antisolar_close = -P0
    q_of_N = np.cross(N, antisolar_close)
    om = math.radians(omega_deg)
    peri_hat = math.cos(om) * antisolar_close + math.sin(om) * q_of_N
    orbit_q = np.cross(N, peri_hat)
    orb = Orbit(a, e, peri_hat, orbit_q)
    sun = np.array([AU_M, 0.0, 0.0])

    def angle_from_antisolar(t: float) -> float:
        p = orb.pos(t)
        c = -(p @ xhat) / np.linalg.norm(p)
        return math.acos(max(-1.0, min(1.0, c)))

    # Locate the closest approach to the antisolar direction by a coarse scan
    # over a FULL period centred on t=0, then a ternary-search refinement.
    # Centred, not [0, 0.9*period]: an early version of this search used the
    # latter and silently missed the true minimum for every off-apsis case,
    # because the last ~degrees of true anomaly before an eclipse that sits
    # off-apsis can need MOST of the period to be reached (see the module
    # header and PROVENANCE.md SS25.11 for the two-body dynamics reason).
    Tp = 2.0 * math.pi / orb.n_mean
    M = 4000
    ts = -0.5 * Tp + Tp * np.arange(M) / (M - 1)
    angs = np.array([angle_from_antisolar(t) for t in ts])
    i0 = int(np.argmin(angs))
    lo, hi = ts[i0] - Tp / M, ts[i0] + Tp / M
    for _ in range(80):
        m1, m2 = lo + (hi - lo) / 3, hi - (hi - lo) / 3
        if angle_from_antisolar(m1) < angle_from_antisolar(m2):
            hi = m2
        else:
            lo = m1
    t0 = 0.5 * (lo + hi)

    def Fu(dt: float) -> float:
        return fs(sun, orb.pos(t0 + dt), R_EARTH_M, 0.0, nphi_coarse)

    f0 = Fu(0.0)
    if f0 > 1.0 - 1e-6:
        return Result(False, note=f"no eclipse at closest approach (Fs={f0:.6f})")

    half = 0.5 * Tp
    dts = -half + 2 * half * np.arange(coarse_n) / (coarse_n - 1)
    fv = np.array([Fu(dt) for dt in dts])
    i_centre = coarse_n // 2
    above = np.where(fv[i_centre:] > 1.0 - 1e-6)[0]
    if above.size == 0:
        return Result(False, note="window too narrow: never reaches full sunlight")
    i_full = i_centre + int(above[0])

    def bisect_cross(lo_t: float, hi_t: float, want_above: float) -> float:
        lo, hi = lo_t, hi_t
        for _ in range(60):
            m = 0.5 * (lo + hi)
            if Fu(m) < want_above:
                lo = m
            else:
                hi = m
        return hi

    dt_edge = bisect_cross(dts[max(0, i_full - 1)], dts[i_full], 1.0 - 1e-6)

    dt_umbra_out = 0.0
    if f0 < 1e-6:
        below = np.where(fv[i_centre:] > 1e-6)[0]
        j = i_centre + int(below[0]) if below.size else i_centre
        lo2, hi2 = dts[max(0, j - 1)], dts[j]
        for _ in range(60):
            m = 0.5 * (lo2 + hi2)
            if Fu(m) > 1e-6:
                hi2 = m
            else:
                lo2 = m
        dt_umbra_out = lo2

    span = dt_edge - dt_umbra_out
    dt_lo, dt_hi = dt_umbra_out - 0.15 * span, dt_edge + 0.15 * span

    tt = dt_lo + (dt_hi - dt_lo) * np.arange(n_samples) / (n_samples - 1)
    du = np.empty(n_samples)
    for i, dt in enumerate(tt):
        pos = orb.pos(t0 + dt)
        du[i] = fs(sun, pos, R_EARTH_M, EDDINGTON_U, nphi_fine) - fs(sun, pos, R_EARTH_M, 0.0, nphi_fine)

    net = float(np.trapezoid(du, tt))
    absint = float(np.trapezoid(np.abs(du), tt))
    running = np.concatenate([[0.0], np.cumsum(0.5 * (du[1:] + du[:-1]) * np.diff(tt))])
    worst = float(np.max(np.abs(running)))
    cancel = 100.0 * (1.0 - abs(net) / absint) if absint > 0 else float("nan")
    swing = worst / abs(net) if abs(net) > 1e-300 else float("inf")
    return Result(True, net, absint, worst, dt_hi - dt_lo, cancel, swing)


def report(label: str, r: Result) -> None:
    if not r.ok:
        print(f"[{label}]  {r.note}")
        return
    print(f"[{label}]")
    print(f"    duration {r.duration:.2f} s   net {r.net:+.4e} s   abs {r.absint:.4e} s   "
          f"peak running {r.worst_running:.4e} s   swing {r.swing:.1f}x   cancels {r.cancel_pct:.2f}%")


def main() -> int:
    r_leo = 7331.0e3
    a_e_leo = math.asin(R_EARTH_M / r_leo)

    print("=== SHDW-R-031: the ONE stated passage ===")
    print("LEO, r=7331km, circular, beta=0 (shadow axis in the orbital plane)\n")
    baseline = study(r_leo, 0.0, 0.0, 0.0)
    report("baseline", baseline)

    print("\n=== SHDW-R-033, family 1: orbital-plane tilt toward the eclipse cutoff ===")
    fracs = [0.0, 0.3, 0.6, 0.8, 0.9, 0.95, 0.97, 0.98, 0.99, 0.995, 0.998, 0.9995]
    for frac in fracs:
        r = study(r_leo, 0.0, frac * a_e_leo, 0.0, nphi_coarse=400, n_samples=1201)
        report(f"beta/a_e = {frac:.4f}", r)

    print("\n=== SHDW-R-033, family 2: eccentricity, eclipse AT an apse (should be UNCHANGED, exactly) ===")
    for e in [0.0, 0.3, 0.5, 0.7, 0.85]:
        a = r_leo / (1.0 - e)
        r = study(a, e, 0.0, 0.0)
        report(f"e = {e:.2f}, at periapsis", r)

    print("\n=== SHDW-R-033, family 3: eccentricity, eclipse OFF an apse (genuine radial velocity) ===")
    r_peri_safe = R_EARTH_M + 600.0e3
    for e in [0.5, 0.85]:
        a = r_peri_safe / (1.0 - e)
        for om in [45.0, 90.0]:
            r = study(a, e, 0.0, om)
            report(f"e = {e:.2f}, {om:.0f} deg from periapsis", r)

    print("\n=== resolution check on the two most extreme rows above (SHDW's own rule 5 diagnostic) ===")
    print("(kept modest deliberately: the point is to show the figure does not move across a")
    print(" refinement, which two steps already demonstrate; the original investigation went")
    print(" up to N=4801/nphi=1800 and is recorded in PROVENANCE.md SS25.11 for the full range)")
    for frac in [0.995, 0.9995]:
        print(f"  beta/a_e = {frac}:")
        for ns, nphi in [(401, 200), (801, 400)]:
            r = study(r_leo, 0.0, frac * a_e_leo, 0.0, nphi_coarse=200, nphi_fine=nphi, n_samples=ns)
            report(f"    N_samples={ns} nphi={nphi}", r)

    print("\n=== SUMMARY (the figures SPEC-shadow.md and PROVENANCE.md quote) ===")
    print(f"  baseline (beta=0):        cancels {baseline.cancel_pct:.2f}%, "
          f"net {baseline.net:.4e} s, abs {baseline.absint:.4e} s, swing {baseline.swing:.1f}x")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
