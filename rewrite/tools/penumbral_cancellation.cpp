// penumbral_cancellation.cpp — the tool behind SHDW-R-031 and SHDW-R-033.
//
// Plan L0 step 8, group C10, ported to C++ (the user's directive of 2026-10-06) from tools/penumbral_cancellation.py (deleted by the same commit: `git show fb4b960:rewrite/tools/penumbral_cancellation.py`).
//
// SPEC-shadow §3.4, §8 Coverage. NOT a gate: `SHDW-Q-005` defers limb darkening because nothing in this plan yet samples inside a penumbra passage, so asserting the MAGNITUDE of a deferred feature in CI
// would mean carrying a two-body propagator in the test suite for something not built — over-building a check for code that does not exist. But a number that is rightly excused from the gate is not excused
// from being REPRODUCIBLE (plan §4 rule 3): this tool is what produced every figure R-031 and R-033 state, and running it regenerates them, rather than leaving frozen numbers with no route back to what made
// them (the defect `oracle/capture.sh` had, precisely, after the merge with the predecessor's tree).
//
// WHAT THIS MEASURES. `SHDW-A-016` establishes that a limb-darkened Sun moves Fs by up to 1.97e-2 at LEO — the module's "seventh axis". That number is a PEAK, at one instant. What a consumer that integrates
// over a whole eclipse passage actually accumulates is a TIME integral, and the first version of that claim (committed, then challenged) stated a cancellation figure — 99.9% — without saying which passage
// produced it, or what would change it. This tool states the passage precisely and measures three families of geometry against it, using an ACTUAL two-body Kepler propagator (Newton's method on Kepler's
// equation; no small-angle or constant-rate shortcuts for the eccentric cases), not hand-argument.
//
// Run: `penumbral_cancellation`. Takes a few minutes. Every number printed under "=== SUMMARY ===" is a number quoted in SPEC-shadow.md or PROVENANCE.md; the two should never drift apart, and if they do, this
// tool — not the spec — is what a reader re-runs to find out which is wrong.
//
// THE PROOF OF THE PORT (registered before any line of this file was written: C10_proof_registration.txt in the group's report files).  The Python's output was never kept; what is recorded is what the tree
// QUOTES from it (PROVENANCE 25.10 and 25.11, SPEC-shadow SHDW-R-031 and SHDW-R-033), and control K-D1 (a script written before the registration, tested on a perfect and on four corrupted outputs)
// compares those 45 figures with the port's printed ones, each at the precision it is quoted at.  C10_proof_result.txt holds the result as it came: 40 of the 45 are reproduced, and the five that differ in
// their last quoted digits are not an effect of arithmetic (six variants of it printed the same bytes: C10_sensitivity.txt) nor of the radial treatment (a second method, the blocked arc of each azimuth in closed
// form, agrees with the sweep to 1e-11 and prints the same output: C10_penumbral_exploration_facts.txt).  They are an effect of the azimuthal resolution of the study: the table of PROVENANCE 25.11 is reproduced,
// all twelve rows, by nphi_fine = 400, where this tool (as the Python) defaults to 450, and the near-grazing rows converge only at nphi_fine >= 1800.  The maintainer's ruling of 2026-10-10: the port stands as
// it is with the defaults it was given, and the records are corrected (PROVENANCE 25.10 and 25.11, section 41.21).  What the tests pin is SPEC-shadow's quotes at their stated precision, this tool's own output at
// its defaults (tests/devtools/penumbral_cancellation_recorded.hpp, the first record), the second method, and the convergence of the 0.9995 row.
//
// NUMPY.  The Python vectorised the ray casting with NumPy; the port uses plain loops that compute, element by element, the SAME operations in the SAME order (no fused multiply-add: the tool is compiled
// with -ffp-contract=off), and reproduces the three places where NumPy's arithmetic is not a plain loop: np.mean and the sum inside np.trapezoid add PAIRWISE (eight accumulators, blocks of 128: pairwise_sum),
// np.cumsum adds in order, and sum() of Python floats is CPython 3.12's compensated sum (devkit py_sum).  What it cannot reproduce bit for bit, stated here and in the registration: NumPy's float64 sin and cos
// (SIMD, a few units in the last place from the C library's) and its BLAS dot products (np.linalg.norm, `@`): the port calls the C library and sums the three products in order.  Differences of that size
// can move a printed figure only where it sits at a rounding edge.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * The Python took no arguments and ignored any it was given; this prints its help for -h and REFUSES any other argument (exit 2).
//   * The cos and sin of the azimuth grid and of the radial grid are computed once per grid line, not once per cell: the same function of the same argument, so the same doubles.
//   * Azimuths without a crossing take no part in the bisection and in W(edge): their values were computed by the Python and discarded by np.where.
//   * `Settings::study` is a seam for the tests of the printing (empty means the real study).
//   * study() is cut into orbit_for, locate_closest_approach, find_window, sample_times and summarize, so that each can be tested; the cut changes no arithmetic (the default output is the same bytes).

#include "penumbral_cancellation.hpp"

#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dk = odl::devkit;
using odl::devkit::Streams;

namespace odl::tools::penumbral_cancellation {

namespace {

constexpr int kOk = 0;
constexpr int kArgument = 2;
constexpr int kInternal = 70;

constexpr int kRad = 220;   // radial samples of the Sun's disc, from its centre to its limb

constexpr std::size_t kPairwiseBlock = 128;

double pairwise(const double* a, std::size_t n) {
    if (n < 8) {
        double res = -0.0;   // numpy starts with -0 to preserve -0 values
        for (std::size_t i = 0; i < n; ++i) res += a[i];
        return res;
    }
    if (n <= kPairwiseBlock) {
        // a block with 8 accumulators
        std::array<double, 8> r{};
        for (std::size_t j = 0; j < 8; ++j) r.at(j) = a[j];
        std::size_t i = 8;
        for (; i < n - (n % 8); i += 8) {
            for (std::size_t j = 0; j < 8; ++j) r.at(j) += a[i + j];
        }
        double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; ++i) res += a[i];   // the rest, after the last multiple of eight
        return res;
    }
    // divide by two but avoid non-multiples of the unroll factor
    std::size_t n2 = n / 2;
    n2 -= n2 % 8;
    return pairwise(a, n2) + pairwise(a + n2, n - n2);
}

// "+.4e": Python's plus flag writes the sign of a non-negative number
std::string plus_e4(double v) {
    std::string s = dk::py_format_e(v, 4);
    if (s.empty() || s.front() != '-') s.insert(s.begin(), '+');
    return s;
}

}  // namespace

double pairwise_sum(std::span<const double> a) { return pairwise(a.data(), a.size()); }

double np_sum(std::span<const double> a) { return 0.0 + pairwise(a.data(), a.size()); }

double np_mean(std::span<const double> a) { return np_sum(a) / static_cast<double>(a.size()); }

double np_trapezoid(std::span<const double> y, std::span<const double> x) {
    if (x.size() != y.size()) throw std::invalid_argument("np_trapezoid: x and y differ in length");
    if (y.size() < 2) return 0.0;
    std::vector<double> terms(y.size() - 1);
    for (std::size_t i = 0; i + 1 < y.size(); ++i) {
        const double d = x[i + 1] - x[i];
        const double s = y[i + 1] + y[i];
        terms[i] = (d * s) / 2.0;
    }
    return np_sum(terms);
}

std::vector<double> np_cumsum(std::span<const double> a) {
    std::vector<double> out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = i == 0 ? a[0] : out[i - 1] + a[i];   // the first element is copied, not added to 0.0 (a -0.0 stays), the others added in order
    return out;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

double norm3(const Vec3& a) { return std::sqrt(dot3(a, a)); }

// ------------------------------------------------------------------------------------------------------------------------------------------ the comparator
// Fs by ray casting, EXACT in the radial direction (bisection on each blocked/unblocked edge).  Identical in substance to `by_solid_angle` in modules/shadow/tests/perspective_tests.cpp and to the `Sky` class
// SHDW-A-016 uses — ported here rather than linked against, because this tool's whole point is to be runnable standalone, without a build of the module.

Sky::Sky(double a_s_, double u_) : a_s(a_s_), u(u_), S(dk::py_sin(a_s_)), k(dk::py_cos(a_s_)) {}

double Sky::F(double c) const {
    const double q = dk::py_sqrt(std::max(0.0, (c - k) * (c + k)));
    return 0.5 * c * q - 0.5 * k * k * dk::py_log(c + q);
}

double Sky::W(double a) const {
    const double c = dk::py_cos(a);
    if (u == 0.0) return 1.0 - c;
    return (1.0 - u) * (1.0 - c) + (u / S) * (F(1.0) - F(c));
}

double fs(const Vec3& sun, const Vec3& sat, double re, double u, int nphi) {
    if (nphi < 1) throw std::invalid_argument("fs: nphi must be at least 1");
    const Vec3 to_sun = {sun[0] - sat[0], sun[1] - sat[1], sun[2] - sat[2]};
    const double ds = norm3(to_sun);
    const Vec3 c = {to_sun[0] / ds, to_sun[1] / ds, to_sun[2] / ds};
    const Sky sky(dk::py_asin(kRSunM / ds), u);

    const Vec3 seed = (std::fabs(c[0]) <= std::fabs(c[1]) && std::fabs(c[0]) <= std::fabs(c[2])) ? Vec3{1.0, 0.0, 0.0}
                      : (std::fabs(c[1]) <= std::fabs(c[2]))                                        ? Vec3{0.0, 1.0, 0.0}
                                                                                                    : Vec3{0.0, 0.0, 1.0};
    Vec3 e1 = cross(seed, c);
    const double n1 = norm3(e1);
    e1 = {e1[0] / n1, e1[1] / n1, e1[2] / n1};
    const Vec3 e2 = cross(c, e1);

    const double rr = dot3(sat, sat);
    const double Re2 = re * re;
    const double rr_minus_Re2 = rr - Re2;

    const std::size_t n = static_cast<std::size_t>(nphi);
    const std::size_t cols = static_cast<std::size_t>(kRad) + 1;

    // the azimuth of each direction, and the part of the direction's components that does not depend on the radial angle
    const double step = 2.0 * std::numbers::pi / nphi;
    std::vector<double> ph(n);
    std::vector<double> t0(n);
    std::vector<double> t1(n);
    std::vector<double> t2(n);
    for (std::size_t j = 0; j < n; ++j) {
        ph[j] = (static_cast<double>(j) + 0.5) * step;
        const double cp = dk::py_cos(ph[j]);
        const double sp = dk::py_sin(ph[j]);
        t0[j] = cp * e1[0] + sp * e2[0];
        t1[j] = cp * e1[1] + sp * e2[1];
        t2[j] = cp * e1[2] + sp * e2[2];
    }
    // the radial grid, including al = 0
    std::vector<double> al_grid(cols);
    std::vector<double> ca(cols);
    std::vector<double> sa(cols);
    for (std::size_t i = 0; i < cols; ++i) {
        al_grid[i] = (sky.a_s * static_cast<double>(i)) / kRad;
        ca[i] = dk::py_cos(al_grid[i]);
        sa[i] = dk::py_sin(al_grid[i]);
    }

    // is the direction (al, azimuth j) blocked: it points at the occulter and away from the satellite's zenith
    const auto hit = [&](double cos_al, double sin_al, std::size_t j) {
        const double wx = cos_al * c[0] + sin_al * t0[j];
        const double wy = cos_al * c[1] + sin_al * t1[j];
        const double wz = cos_al * c[2] + sin_al * t2[j];
        const double wr = wx * sat[0] + wy * sat[1] + wz * sat[2];
        return (wr * wr - rr_minus_Re2 >= 0.0) && (wr < 0.0);
    };

    std::vector<unsigned char> blocked(n * cols);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < cols; ++i) blocked.at(j * cols + i) = hit(ca.at(i), sa.at(i), j) ? 1 : 0;
    }
    const auto cell = [&](std::size_t j, std::size_t i) { return blocked[j * cols + i] != 0; };

    // bisect_vec for ONE azimuth: 55 halvings between lo and hi, the state at lo being `at_lo`
    const auto bisect = [&](double lo, double hi, bool at_lo, std::size_t j) {
        for (int it = 0; it < 55; ++it) {
            const double mid = 0.5 * (lo + hi);
            const bool cur = hit(dk::py_cos(mid), dk::py_sin(mid), j);
            if (cur == at_lo) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        return 0.5 * (lo + hi);
    };

    // --- first crossing, from al=0's state; second crossing, if any: the grid strictly past the first, for a state differing from the POST-first-crossing state
    //
    // A ray from the Sun's own centre outward to its limb crosses a single convex occulter's boundary AT MOST TWICE (enter, or enter-and-exit), so this only ever needs to track up to two crossings per azimuth.
    // state0 True (blocked at the Sun's own centre direction):
    //   0 crossings -> blocked all the way to a_s
    //   1 crossing  -> blocked from 0 to edge1
    //   2 crossings -> blocked 0..edge1, then unblocked, then BLOCKED AGAIN edge2..a_s (the ray re-enters the disc; rare, but the same generic edge machinery the C++ version of the module used)
    // state0 False: mirror image (0, or blocked edge1..a_s, or edge1..edge2).
    const double Wa_s = sky.W(sky.a_s);
    std::vector<double> flux(n);
    for (std::size_t j = 0; j < n; ++j) {
        const bool state0 = cell(j, 0);
        // the first sample past al = 0 (the sample 0 is state0's own) whose state differs from state0; idx1 == 0: there is none
        std::size_t idx1 = 0;
        for (std::size_t i = 1; i < cols; ++i) {
            if (cell(j, i) != state0) {
                idx1 = i;
                break;
            }
        }
        if (idx1 == 0) {   // no crossing: blocked all the way to a_s, or not at all
            flux[j] = state0 ? Wa_s : 0.0;
            continue;
        }
        const double Wedge1 = sky.W(bisect(al_grid[idx1 - 1], al_grid[idx1], state0, j));

        // the first sample strictly past the first crossing (the sample idx1 is the state after it) whose state differs from the state after the first crossing; idx2 == 0: there is none
        const bool state_after1 = !state0;
        std::size_t idx2 = 0;
        for (std::size_t i = idx1 + 1; i < cols; ++i) {
            if (cell(j, i) != state_after1) {
                idx2 = i;
                break;
            }
        }
        if (idx2 == 0) {   // one crossing: blocked from 0 to edge1 (state0), or from edge1 to a_s
            flux[j] = state0 ? Wedge1 : Wa_s - Wedge1;
            continue;
        }
        const double Wedge2 = sky.W(bisect(al_grid[idx2 - 1], al_grid[idx2], state_after1, j));
        flux[j] = state0 ? Wedge1 + (Wa_s - Wedge2) : Wedge2 - Wedge1;
    }
    const double total = Wa_s;
    return 1.0 - np_mean(flux) / total;
}

// ------------------------------------------------------------------------------------------------------------------------------------------ two-body Kepler propagation
// Deliberately NOT the constant-angular-rate shortcut a circular orbit would allow: the whole point of this tool is to measure what genuine Kepler dynamics do to the cancellation, so it always solves
// Kepler's equation, even at e=0.

Orbit::Orbit(double a_, double e_, const Vec3& peri_hat_, const Vec3& q_hat_)
    : a(a_), e(e_), peri_hat(peri_hat_), q_hat(q_hat_), n_mean(dk::py_sqrt(kGmEarth / dk::py_pow(a_, 3.0))) {}

Vec3 Orbit::pos(double t) const {
    const double M = n_mean * t;
    double E = M;
    for (int i = 0; i < 80; ++i) {
        const double f = E - e * dk::py_sin(E) - M;
        const double fp = 1.0 - e * dk::py_cos(E);
        E -= f / fp;
    }
    const double r = a * (1.0 - e * dk::py_cos(E));
    const double cosnu = (dk::py_cos(E) - e) / (1.0 - e * dk::py_cos(E));
    const double sinnu = dk::py_sqrt(std::max(0.0, 1.0 - dk::py_pow(e, 2.0))) * dk::py_sin(E) / (1.0 - e * dk::py_cos(E));
    return {r * (cosnu * peri_hat[0] + sinnu * q_hat[0]), r * (cosnu * peri_hat[1] + sinnu * q_hat[1]), r * (cosnu * peri_hat[2] + sinnu * q_hat[2])};
}

double Orbit::period() const { return 2.0 * std::numbers::pi / n_mean; }

// ------------------------------------------------------------------------------------------------------------------------------------------ one traversal, in its pieces
// study() was one function in the Python; the port keeps its operations and their order and cuts it where a test can take hold: the orbit of the traversal, the closest approach, the window, the sample times and
// the integrals.  The cut changes no arithmetic: the default output of the tool is the same bytes before and after (C10_proof_registration.txt, AMENDMENT 2, R1).

Orbit orbit_for(double a, double e, double beta, double omega_deg) {
    const Vec3 xhat = {1.0, 0.0, 0.0};
    const Vec3 N = {dk::py_sin(beta), 0.0, dk::py_cos(beta)};
    const double xn = dot3(xhat, N);
    Vec3 P0 = {xhat[0] - xn * N[0], xhat[1] - xn * N[1], xhat[2] - xn * N[2]};
    const double p0n = norm3(P0);
    P0 = {P0[0] / p0n, P0[1] / p0n, P0[2] / p0n};
    const Vec3 antisolar_close = {-P0[0], -P0[1], -P0[2]};
    const Vec3 q_of_N = cross(N, antisolar_close);
    const double om = dk::py_radians(omega_deg);
    const double com = dk::py_cos(om);
    const double som = dk::py_sin(om);
    const Vec3 peri_hat = {com * antisolar_close[0] + som * q_of_N[0], com * antisolar_close[1] + som * q_of_N[1], com * antisolar_close[2] + som * q_of_N[2]};
    const Vec3 orbit_q = cross(N, peri_hat);
    return Orbit(a, e, peri_hat, orbit_q);
}

double angle_from_antisolar(const Orbit& orb, double t) {
    const Vec3 xhat = {1.0, 0.0, 0.0};
    const Vec3 p = orb.pos(t);
    const double c = -dot3(p, xhat) / norm3(p);
    return dk::py_acos(std::max(-1.0, std::min(1.0, c)));
}

// Locate the closest approach to the antisolar direction by a coarse scan over a FULL period centred on t=0, then a ternary-search refinement.  Centred, not [0, 0.9*period]: an early version of this
// search used the latter and silently missed the true minimum for every off-apsis case, because the last ~degrees of true anomaly before an eclipse that sits off-apsis can need MOST of the period to be
// reached (see the module header and PROVENANCE.md SS25.11 for the two-body dynamics reason).
double locate_closest_approach(const Orbit& orb) {
    const double Tp = orb.period();
    constexpr std::size_t M = 4000;
    std::vector<double> ts(M);
    std::vector<double> angs(M);
    for (std::size_t i = 0; i < M; ++i) {
        ts.at(i) = -0.5 * Tp + (Tp * static_cast<double>(i)) / static_cast<double>(M - 1);
        angs.at(i) = angle_from_antisolar(orb, ts[i]);
    }
    std::size_t i0 = 0;
    for (std::size_t i = 1; i < angs.size(); ++i) {
        if (angs[i] < angs[i0]) i0 = i;   // np.argmin: the first of equal minima
    }
    double lo = ts[i0] - Tp / static_cast<double>(M);
    double hi = ts[i0] + Tp / static_cast<double>(M);
    for (int it = 0; it < 80; ++it) {
        const double m1 = lo + (hi - lo) / 3;
        const double m2 = hi - (hi - lo) / 3;
        if (angle_from_antisolar(orb, m1) < angle_from_antisolar(orb, m2)) {
            hi = m2;
        } else {
            lo = m1;
        }
    }
    return 0.5 * (lo + hi);
}

Window find_window(const Orbit& orb, double t0, const StudyOptions& options) {
    const Vec3 sun = kSunPosition;
    const auto Fu = [&](double dt) { return fs(sun, orb.pos(t0 + dt), kREarthM, 0.0, options.nphi_coarse); };

    Window w;
    const double f0 = Fu(0.0);
    if (f0 > 1.0 - 1e-6) {
        w.note = "no eclipse at closest approach (Fs=" + dk::py_format_f(f0, 6) + ")";
        return w;
    }

    const double half = 0.5 * orb.period();
    const int coarse_n = options.coarse_n;
    std::vector<double> dts(static_cast<std::size_t>(coarse_n));
    std::vector<double> fv(static_cast<std::size_t>(coarse_n));
    for (int i = 0; i < coarse_n; ++i) {
        dts[static_cast<std::size_t>(i)] = -half + ((2 * half) * static_cast<double>(i)) / (coarse_n - 1);
        fv[static_cast<std::size_t>(i)] = Fu(dts[static_cast<std::size_t>(i)]);
    }
    const std::size_t i_centre = static_cast<std::size_t>(coarse_n / 2);
    std::size_t i_full = 0;
    bool found_full = false;
    for (std::size_t k = i_centre; k < fv.size(); ++k) {
        if (fv[k] > 1.0 - 1e-6) {
            i_full = k;
            found_full = true;
            break;
        }
    }
    if (!found_full) {
        w.note = "window too narrow: never reaches full sunlight";
        return w;
    }

    const auto bisect_cross = [&](double lo_t, double hi_t, double want_above) {
        double l = lo_t;
        double h = hi_t;
        for (int it = 0; it < 60; ++it) {
            const double m = 0.5 * (l + h);
            if (Fu(m) < want_above) {
                l = m;
            } else {
                h = m;
            }
        }
        return h;
    };

    const double dt_edge = bisect_cross(dts[i_full >= 1 ? i_full - 1 : 0], dts[i_full], 1.0 - 1e-6);

    double dt_umbra_out = 0.0;
    if (f0 < 1e-6) {
        std::size_t j = i_centre;
        for (std::size_t k = i_centre; k < fv.size(); ++k) {
            if (fv[k] > 1e-6) {
                j = k;
                break;
            }
        }
        double lo2 = dts[j >= 1 ? j - 1 : 0];
        double hi2 = dts[j];
        for (int it = 0; it < 60; ++it) {
            const double m = 0.5 * (lo2 + hi2);
            if (Fu(m) > 1e-6) {
                hi2 = m;
            } else {
                lo2 = m;
            }
        }
        dt_umbra_out = lo2;
    }

    const double span = dt_edge - dt_umbra_out;
    w.dt_lo = dt_umbra_out - 0.15 * span;
    w.dt_hi = dt_edge + 0.15 * span;
    w.ok = true;
    return w;
}

std::vector<double> sample_times(double dt_lo, double dt_hi, int n_samples) {
    std::vector<double> tt(static_cast<std::size_t>(n_samples));
    for (int i = 0; i < n_samples; ++i) {
        const std::size_t k = static_cast<std::size_t>(i);
        tt[k] = dt_lo + ((dt_hi - dt_lo) * static_cast<double>(i)) / (n_samples - 1);
    }
    return tt;
}

Result summarize(std::span<const double> du, std::span<const double> tt, double duration) {
    if (du.size() != tt.size()) throw std::invalid_argument("summarize: du and tt differ in length");
    Result r;
    r.ok = true;
    r.net = np_trapezoid(du, tt);
    std::vector<double> abs_du(du.size());
    for (std::size_t i = 0; i < du.size(); ++i) abs_du[i] = std::fabs(du[i]);
    r.absint = np_trapezoid(abs_du, tt);
    std::vector<double> increments;
    if (du.size() >= 2) {
        increments.resize(du.size() - 1);
        for (std::size_t i = 0; i + 1 < du.size(); ++i) increments[i] = (0.5 * (du[i + 1] + du[i])) * (tt[i + 1] - tt[i]);
    }
    const std::vector<double> sums = np_cumsum(increments);
    double worst = 0.0;   // np.max(np.abs(running)), running = [0.0] + cumsum
    for (const double s : sums) worst = std::max(worst, std::fabs(s));
    r.worst_running = worst;
    r.duration = duration;
    r.cancel_pct = r.absint > 0 ? 100.0 * (1.0 - std::fabs(r.net) / r.absint) : std::numeric_limits<double>::quiet_NaN();
    r.swing = std::fabs(r.net) > 1e-300 ? r.worst_running / std::fabs(r.net) : std::numeric_limits<double>::infinity();
    return r;
}

Result study(double a, double e, double beta, double omega_deg, const StudyOptions& options) {
    const Orbit orb = orbit_for(a, e, beta, omega_deg);
    const double t0 = locate_closest_approach(orb);
    const Window w = find_window(orb, t0, options);
    if (!w.ok) {
        Result r;
        r.ok = false;
        r.note = w.note;
        return r;
    }
    const Vec3 sun = kSunPosition;
    const std::vector<double> tt = sample_times(w.dt_lo, w.dt_hi, options.n_samples);
    std::vector<double> du(tt.size());
    for (std::size_t i = 0; i < tt.size(); ++i) {
        const Vec3 pos = orb.pos(t0 + tt[i]);
        du[i] = fs(sun, pos, kREarthM, kEddingtonU, options.nphi_fine) - fs(sun, pos, kREarthM, 0.0, options.nphi_fine);
    }
    return summarize(du, tt, w.dt_hi - w.dt_lo);
}

void report(std::ostream& out, const std::string& label, const Result& r) {
    if (!r.ok) {
        out << '[' << label << "]  " << r.note << '\n';
        return;
    }
    out << '[' << label << "]\n";
    out << "    duration " << dk::py_format_f(r.duration, 2) << " s   net " << plus_e4(r.net) << " s   abs " << dk::py_format_e(r.absint, 4) << " s   peak running "
        << dk::py_format_e(r.worst_running, 4) << " s   swing " << dk::py_format_f(r.swing, 1) << "x   cancels " << dk::py_format_f(r.cancel_pct, 2) << "%\n";
}

Settings default_settings() { return Settings{}; }

namespace {

const char kUsageText[] = "usage: penumbral_cancellation [-h]\n";

const char kHelpText[] =
    "\n"
    "The tool behind SHDW-R-031 and SHDW-R-033 (SPEC-shadow 3.4, 8 Coverage): Fs of a limb-darkened Sun against a uniform one along ONE stated eclipse passage, with an actual two-body Kepler propagator,\n"
    "for the baseline, a tilt of the orbital plane toward the eclipse cutoff, and eccentric orbits at and off an apse, with the resolution checks and the SUMMARY that SPEC-shadow and PROVENANCE quote.\n"
    "Takes a few minutes.  Not a gate (SHDW-Q-005).\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "\n"
    "exit codes: 0 done   2 an argument error   70 an error the tool did not anticipate\n";

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        for (const std::string& arg : argv) {
            if (arg == "-h" || arg == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            io.err << kUsageText << kTool << ": error: unrecognized arguments: " << arg << '\n';
            return kArgument;
        }
        std::ostream& out = io.out;
        const auto run_study = [&](double a, double e, double beta, double omega_deg, const StudyOptions& options) {
            return settings.study ? settings.study(a, e, beta, omega_deg, options) : study(a, e, beta, omega_deg, options);
        };

        const double r_leo = 7331.0e3;
        const double a_e_leo = dk::py_asin(kREarthM / r_leo);

        out << "=== SHDW-R-031: the ONE stated passage ===\n";
        out << "LEO, r=7331km, circular, beta=0 (shadow axis in the orbital plane)\n\n";
        const Result baseline = run_study(r_leo, 0.0, 0.0, 0.0, StudyOptions{});
        report(out, "baseline", baseline);

        out << "\n=== SHDW-R-033, family 1: orbital-plane tilt toward the eclipse cutoff ===\n";
        for (const double frac : {0.0, 0.3, 0.6, 0.8, 0.9, 0.95, 0.97, 0.98, 0.99, 0.995, 0.998, 0.9995}) {
            StudyOptions options;
            options.nphi_coarse = 400;
            options.n_samples = 1201;
            const Result r = run_study(r_leo, 0.0, frac * a_e_leo, 0.0, options);
            report(out, "beta/a_e = " + dk::py_format_f(frac, 4), r);
        }

        out << "\n=== SHDW-R-033, family 2: eccentricity, eclipse AT an apse (should be UNCHANGED, exactly) ===\n";
        for (const double e : {0.0, 0.3, 0.5, 0.7, 0.85}) {
            const double a = r_leo / (1.0 - e);
            const Result r = run_study(a, e, 0.0, 0.0, StudyOptions{});
            report(out, "e = " + dk::py_format_f(e, 2) + ", at periapsis", r);
        }

        out << "\n=== SHDW-R-033, family 3: eccentricity, eclipse OFF an apse (genuine radial velocity) ===\n";
        const double r_peri_safe = kREarthM + 600.0e3;
        for (const double e : {0.5, 0.85}) {
            const double a = r_peri_safe / (1.0 - e);
            for (const double om : {45.0, 90.0}) {
                const Result r = run_study(a, e, 0.0, om, StudyOptions{});
                report(out, "e = " + dk::py_format_f(e, 2) + ", " + dk::py_format_f(om, 0) + " deg from periapsis", r);
            }
        }

        // The next three printed lines are the Python's text and are kept as it wrote them, because the output is the tool's first record.  What they say is not what the two steps below show: the 0.9995 row prints
        // 48.37 and 48.32 and the 0.995 row 74.68 and 74.67 (they converge at nphi_fine >= 1800 only, to 48.2927 and 74.6745): PROVENANCE section 41.21 and section 25.11 (corrected 2026-10-10) say so.
        out << "\n=== resolution check on the two most extreme rows above (SHDW's own rule 5 diagnostic) ===\n";
        out << "(kept modest deliberately: the point is to show the figure does not move across a\n";
        out << " refinement, which two steps already demonstrate; the original investigation went\n";
        out << " up to N=4801/nphi=1800 and is recorded in PROVENANCE.md SS25.11 for the full range)\n";
        for (const double frac : {0.995, 0.9995}) {
            out << "  beta/a_e = " << dk::py_float_repr(frac) << ":\n";
            for (const auto& [ns, nphi] : {std::pair{401, 200}, std::pair{801, 400}}) {
                StudyOptions options;
                options.nphi_coarse = 200;
                options.nphi_fine = nphi;
                options.n_samples = ns;
                const Result r = run_study(r_leo, 0.0, frac * a_e_leo, 0.0, options);
                report(out, "    N_samples=" + std::to_string(ns) + " nphi=" + std::to_string(nphi), r);
            }
        }

        out << "\n=== resolution check on the BASELINE row too (manager's finding, 2026-09-22) ===\n";
        out << "this is the ROW THE EXTREME-ROW CHECK ABOVE DID NOT COVER, and it is the most\n";
        out << "resolution-sensitive of any row in this file: cancel is 99.9%, so net is a ~0.1%\n";
        out << "residual of two integrals each ~abs in size, and a tiny relative shift in either\n";
        out << "integral is a large relative shift in their difference. swing = abs/net inherits\n";
        out << "that sensitivity directly. abs and cancel_pct do NOT share it -- watch them hold\n";
        out << "still while swing moves.\n";
        std::vector<Result> baseline_runs;
        for (const auto& [ns, nphi] : {std::pair{201, 100}, std::pair{401, 200}, std::pair{801, 400}, std::pair{1601, 800}, std::pair{3201, 1600}}) {
            StudyOptions options;
            options.nphi_coarse = 150;
            options.nphi_fine = nphi;
            options.n_samples = ns;
            const Result r = run_study(r_leo, 0.0, 0.0, 0.0, options);
            baseline_runs.push_back(r);
            report(out, "  N_samples=" + std::to_string(ns) + " nphi=" + std::to_string(nphi), r);
        }
        std::vector<double> swings;
        std::vector<double> nets;
        std::vector<double> abss;
        for (const Result& r : baseline_runs) {
            swings.push_back(r.swing);
            nets.push_back(r.net);
            abss.push_back(r.absint);
        }
        // Python's min() and max() of a list that is never empty here (five runs): the first element, replaced by a later one only when that is strictly smaller or larger
        const auto minimum = [](const std::vector<double>& v) { double m = v.at(0); for (const double x : v) { if (x < m) m = x; } return m; };
        const auto maximum = [](const std::vector<double>& v) { double m = v.at(0); for (const double x : v) { if (x > m) m = x; } return m; };
        const double count = static_cast<double>(baseline_runs.size());
        const double swing_spread_pct = 100.0 * (maximum(swings) - minimum(swings)) / (dk::py_sum(swings) / count);
        const double net_spread_pct = 100.0 * (maximum(nets) - minimum(nets)) / std::fabs(dk::py_sum(nets) / count);
        const double abs_spread_pct = 100.0 * (maximum(abss) - minimum(abss)) / (dk::py_sum(abss) / count);
        // NOT a plain mean over all five points: the two coarsest are the least converged and would bias a naive average low, toward the same under-resolved answer the tool's own default settings gave on the
        // first pass (367x) and that a first version of THIS summary line reported by averaging everything together.  What estimates convergence is the pair of FINEST resolutions agreeing with each other, not
        // the whole sweep's mean -- so that is what is reported.
        const double finest_a = swings[swings.size() - 2];
        const double finest_b = swings[swings.size() - 1];
        const double finest_agreement_pct = 100.0 * std::fabs(finest_b - finest_a) / finest_b;
        const double converged_estimate = finest_b;
        out << "  across these " << baseline_runs.size() << " resolutions: swing spans " << dk::py_format_f(minimum(swings), 1) << "x to " << dk::py_format_f(maximum(swings), 1) << "x ("
            << dk::py_format_f(swing_spread_pct, 1) << "% spread); net spans " << dk::py_format_f(net_spread_pct, 1) << "% spread; abs spans only " << dk::py_format_f(abs_spread_pct, 2)
            << "% spread -- confirming net (and hence swing) is the sensitive one, not abs.\n";
        out << "  the FINEST two resolutions agree to " << dk::py_format_f(finest_agreement_pct, 2) << "%, well inside the spread of the coarser pairs -- that is convergence, not noise. Converged estimate: ~"
            << dk::py_format_f(converged_estimate, 0)
            << "x. This is closer to the ORIGINAL figure this tool was built to check (376x) than to this tool's own default-resolution output (367x) -- the default was under-resolved, not the "
               "earlier figure wrong.\n";

        out << "\n=== SUMMARY (the figures SPEC-shadow.md and PROVENANCE.md quote) ===\n";
        out << "  baseline (beta=0):        cancels " << dk::py_format_f(baseline.cancel_pct, 2) << "%, net " << dk::py_format_e(baseline.net, 4) << " s, abs " << dk::py_format_e(baseline.absint, 4)
            << " s (stable to 4 figures), swing ~" << dk::py_format_f(converged_estimate, 0) << "x (from the finest-resolution pair above, NOT from this call's own default-resolution run of "
            << dk::py_format_f(baseline.swing, 0) << "x -- see the baseline resolution check for why the two differ and which one to trust)\n";
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::penumbral_cancellation

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::penumbral_cancellation::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
