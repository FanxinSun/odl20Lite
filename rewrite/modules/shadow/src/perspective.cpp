// perspective.cpp — LI19's PPM.  SPEC-shadow.md §4.
//
// The paper prints the whole derivation and this file follows it, with three
// places where it does something else and says why: the branch test (eq 15), the
// intersections (eq 32-33), and the areas (eq 36-39).  Each is recorded at the
// point where it happens.

#include <odl/shadow/perspective.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <limits>
#include <vector>

namespace odl::shadow {
namespace {

using std::numbers::pi;

// --------------------------------------------------------------------------- //
// The Earth ellipsoid enters only as A = diag(a^-2, a^-2, b^-2), always through
// these two.  Keeping it a triple rather than a matrix keeps the symmetry that
// makes a == b degenerate to a sphere visible in the arithmetic itself.

struct Ellipsoid {
    double ia2, ib2;                                   // 1/a^2, 1/b^2
    [[nodiscard]] Vec3 apply(const Vec3& v) const noexcept {
        return Vec3{ia2 * v.x, ia2 * v.y, ib2 * v.z};
    }
};

/// M = A r r^T A^T - (r^T A r - 1) A   (LI19 eq 19).  Symmetric, so the quadratic
/// form is all that is ever needed and the matrix itself is never built.
struct Silhouetteform {
    Vec3 Ar;            // A r
    double rAr;         // r^T A r
    Ellipsoid e;
    [[nodiscard]] double operator()(const Vec3& g, const Vec3& h) const noexcept {
        return g.dot(Ar) * h.dot(Ar) - (rAr - 1.0) * g.dot(e.apply(h));
    }
};

// --------------------------------------------------------------------------- //
// A conic in its own standard frame: lam_x X^2 + lam_y Y^2 = C, with the sweep
// integral that Green's theorem needs.
//
//     (1/2) INTEGRAL (X dY - Y dX)
//
// is (1/2)(A B) dpsi on an ellipse (A cos psi, B sin psi) and (1/2) sigma (A B)
// dtau on a hyperbola (sigma A cosh tau, B sinh tau), because cos^2 + sin^2 = 1
// and cosh^2 - sinh^2 = 1 do the same job.  That one line is the whole of what
// LI19 eq 36-39 delegate to Hughes and Chraibi (2012) for the "elliptical arch":
// a chord and an arc bound a region whose area Green's theorem gives directly,
// so the four inside/outside x ellipse/hyperbola cases collapse into one sum and
// no second reference is needed.  SPEC-shadow SHDW-R-026.

struct Conic {
    bool hyperbolic = false;
    double A = 0.0, B = 0.0;    // semi-axes of the standard form
    bool swapped = false;       // true when the conic opens along Y rather than X

    /// The point at parameter `t` on branch `sigma` (sigma is ignored for an ellipse).
    [[nodiscard]] std::array<double, 2> point(double t, double sigma) const noexcept {
        double X, Y;
        if (!hyperbolic) { X = A * std::cos(t); Y = B * std::sin(t); }
        else             { X = sigma * A * std::cosh(t); Y = B * std::sinh(t); }
        return swapped ? std::array<double, 2>{Y, X} : std::array<double, 2>{X, Y};
    }

    /// The parameter of a point known to be on the conic.
    [[nodiscard]] double parameter(double x, double y) const noexcept {
        const double X = swapped ? y : x, Y = swapped ? x : y;
        return hyperbolic ? std::asinh(Y / B) : std::atan2(Y / B, X / A);
    }
    [[nodiscard]] double branch(double x, double y) const noexcept {
        const double X = swapped ? y : x;
        return X >= 0.0 ? 1.0 : -1.0;
    }

    /// (1/2) INTEGRAL (x dy - y dx) along the conic from t1 to t2 on branch sigma.
    /// The swap is a reflection, which flips the sign of the form.
    [[nodiscard]] double sweep(double t1, double t2, double sigma) const noexcept {
        const double s = 0.5 * A * B * (t2 - t1) * (hyperbolic ? sigma : 1.0);
        return swapped ? -s : s;
    }
};

/// (1/2) INTEGRAL (x dy - y dx) along the circle of centre (cx,cy) radius R from
/// th1 to th2.
double circle_sweep(double cx, double cy, double R, double th1, double th2) noexcept {
    return 0.5 * (R * R * (th2 - th1)
                  + R * (cx * (std::sin(th2) - std::sin(th1))
                         - cy * (std::cos(th2) - std::cos(th1))));
}

}  // namespace

namespace {

/// One silhouette, projected.  The atmosphere needs more than the area, so this
/// is what the core returns and `perspective_on` is what composes it.
struct Projected {
    double shaded = 0.0;        ///< blocked area of the solar disc, on the image plane
    double disc_area = 0.0;     ///< pi R0^2
    double R0 = 0.0;
    Silhouette kind = Silhouette::absent;
    /// Signed distance from the Sun's image centre, along the direction from the
    /// Earth's image centre (PEC) towards it, at which this silhouette's boundary
    /// is crossed.  LI19 §2.6 measures h along exactly this line.
    double u_boundary = std::numeric_limits<double>::quiet_NaN();
    bool centre_blocked = false;
};

odl::Result<Projected, ShadowError>
project(const Vec3& sun_m, const Vec3& sat_m, double equatorial_m, double polar_m, double gamma) {
    const auto fail = [](std::string_view id, std::string what) {
        return odl::Result<Projected, ShadowError>{odl::err(ShadowError{id, std::move(what)})};
    };
    if (!(equatorial_m > 0.0) || !(polar_m > 0.0)) return fail("SHDW-F-003", "the ellipsoid radii must both be positive");
    if (!(gamma > 0.0)) return fail("SHDW-F-003", "the image plane must be at a positive distance");

    const Vec3 r = sat_m, rs = sun_m;
    const Vec3 d{r.x - rs.x, r.y - rs.y, r.z - rs.z};
    const double dist = d.norm();
    if (!(dist > 0.0)) return fail("SHDW-F-001", "the satellite is at the Sun's centre");

    const Ellipsoid e{1.0 / (equatorial_m * equatorial_m), 1.0 / (polar_m * polar_m)};
    const double rAr = r.dot(e.apply(r));
    if (!(rAr > 1.0)) return fail("SHDW-F-004", "the satellite is on or inside the Earth ellipsoid, where no silhouette exists");

    // --- the ISF (LI19 eq 3-6).  u is built from the smallest component of n so
    //     that the cross product is never near-degenerate.
    const Vec3 n{d.x / dist, d.y / dist, d.z / dist};
    const double ax = std::abs(n.x), ay = std::abs(n.y), az = std::abs(n.z);
    const Vec3 seed = (ax <= ay && ax <= az) ? Vec3{1, 0, 0} : (ay <= az ? Vec3{0, 1, 0} : Vec3{0, 0, 1});
    Vec3 u = seed.cross(n);
    const double un = u.norm();
    u = Vec3{u.x / un, u.y / un, u.z / un};
    const Vec3 v = n.cross(u);

    const Silhouetteform M{e.apply(r), rAr, e};

    // --- existence of an image (LI19 eq 10-15).
    //
    // NOT AS THE PAPER TESTS IT.  Equation 15 compares Sun-to-satellite distances
    // against dS1 and dS2 to sort "no image" from "partial" from "full".  All
    // three are recovered here from ONE quantity that the area computation needs
    // anyway: a point x of the image plane is blocked exactly when the ray from
    // the satellite through it, continued FORWARDS, meets the ellipsoid.  From
    // eq 17 that ray's parameter satisfies p^2 g^T A g + 2 p g^T A r + (r^T A r
    // - 1) = 0, whose discriminant is g^T M g and whose roots share the sign of
    // -g^T A r, since their product (r^T A r - 1)/g^T A g is positive outside the
    // Earth.  So
    //
    //     BLOCKED(x)  <=>  g^T M g >= 0   AND   g^T A r < 0,     g = x - r
    //
    // exactly, with no separate case analysis.  The second factor is what eq 15
    // is really for: it selects the FORWARD nappe of the tangent cone, which is
    // the whole difference between the two branches of a hyperbolic silhouette.
    // It cannot cut a branch, because on g^T A r = 0 the form is -g^T A g (r^T A
    // r - 1) < 0, so the half-plane boundary never touches the blocked set.
    const auto blocked = [&](const Vec3& g) noexcept {
        return M(g, g) >= 0.0 && g.dot(M.Ar) < 0.0;
    };

    // --- the conic (LI19 eq 22).  x_b - r = -gamma n + alpha u + beta v.
    const double k0 = M(u, u), k1 = M(u, v), k2 = M(v, v);
    const double k3 = -2.0 * gamma * M(u, n), k4 = -2.0 * gamma * M(v, n);
    const double k5 = gamma * gamma * M(n, n);
    const double detB = k0 * k2 - k1 * k1;

    const double R0 = gamma * kSunRadiusM / dist;      // LI19 eq 28
    const double disc_area = pi * R0 * R0;

    Projected out;
    out.disc_area = disc_area;
    out.R0 = R0;

    // The line LI19 §2.6 measures depth along: from PEC, the Earth's centre
    // projected, towards the Sun's image centre, which is the ISF origin and so
    // sits at (alpha, beta) = (0, 0).  PEC's ray is g = -(gamma / r.n) r, so its
    // plane coordinates fall straight out of the basis.  `u` below is signed
    // distance from the Sun's image centre along that direction, increasing
    // AWAY from the Earth -- so the solid Earth's boundary comes first and the
    // atmosphere's after it, which is the order eq 40's h assumes.
    const double rn = r.dot(n);
    const auto at_u = [&](double uu, const auto& pa, const auto& pb) noexcept {
        const double al = uu * pa, be = uu * pb;
        return blocked(Vec3{-gamma * n.x + al * u.x + be * v.x,
                            -gamma * n.y + al * u.y + be * v.y,
                            -gamma * n.z + al * u.z + be * v.z});
    };
    const auto find_boundary = [&](void) noexcept {
        if (!(std::abs(rn) > 0.0)) return;
        const double pa = -(gamma / rn) * r.dot(u), pb = -(gamma / rn) * r.dot(v);
        const double len = std::hypot(pa, pb);
        if (!(len > 0.0)) return;
        const double ex = -pa / len, ey = -pb / len;      // PEC -> Sun's image centre
        double lo = -len;                                  // PEC itself
        if (!at_u(lo, ex, ey)) return;                     // not blocked there: no line to measure
        double hi = std::max(R0, len);
        for (int i = 0; i < 200 && at_u(hi, ex, ey); ++i) hi *= 2.0;
        if (at_u(hi, ex, ey)) return;
        for (int i = 0; i < 100; ++i) {
            const double mid = 0.5 * (lo + hi);
            at_u(mid, ex, ey) ? lo = mid : hi = mid;
        }
        out.u_boundary = 0.5 * (lo + hi);
    };

    // RELATIVE TO THE COEFFICIENTS' OWN SCALE, and it has to be: k0, k1 and k2
    // are quadratic forms of A = diag(a^-2, a^-2, b^-2), so they carry 1/R^4 and
    // run at 1e-27.  A threshold anchored to 1.0 calls every geometry a parabola
    // -- and the sweep that found it reported "worst difference 0.000e+00 over 41
    // geometries" while comparing none of them, which is why the count is
    // asserted wherever this is measured.
    const double scale = std::max(std::abs(k0 * k2), k1 * k1);
    if (!(scale > 0.0) || std::abs(detB) <= 1e-12 * scale)
        return fail("SHDW-F-005",
                    "the silhouette is a parabola, which LI19 §2.3 does not treat: \"an "
                    "instantaneous state in the variation from an ellipse to a hyperbola\"");

    out.kind = detB > 0.0 ? Silhouette::ellipse : Silhouette::hyperbola;

    // --- standard form (LI19 eq 23-26).  B = [[k0,k1],[k1,k2]], eigendecomposed.
    double lam1, lam2, q00, q10, q01, q11;
    {
        const double tr = k0 + k2;
        const double rad = std::sqrt(std::max(0.0, 0.25 * (k0 - k2) * (k0 - k2) + k1 * k1));
        lam1 = 0.5 * tr + rad;
        lam2 = 0.5 * tr - rad;
        if (std::abs(k1) > 0.0) {
            double ex = k1, ey = lam1 - k0;
            const double ln = std::hypot(ex, ey);
            ex /= ln; ey /= ln;
            q00 = ex; q10 = ey; q01 = -ey; q11 = ex;
        } else {
            q00 = 1.0; q10 = 0.0; q01 = 0.0; q11 = 1.0;
            lam1 = k0; lam2 = k2;
        }
    }
    // Omega = (1/4) delta^T B^-1 delta  (LI19 eq 31's Omega), C = Omega - k5.
    const double Omega = (k2 * k3 * k3 - 2.0 * k1 * k3 * k4 + k0 * k4 * k4) / (4.0 * detB);
    const double C = Omega - k5;

    // phi = Q^T chi + (1/2) D^-1 Q^T delta.  The circle's centre is phi(chi = 0).
    const double qd0 = q00 * k3 + q10 * k4, qd1 = q01 * k3 + q11 * k4;
    const double cx = 0.5 * qd0 / lam1, cy = 0.5 * qd1 / lam2;

    // phi -> (alpha, beta) -> the 3-D ray, so that `blocked` stays the authority.
    const auto ray = [&](double x, double y) noexcept {
        const double px = x - cx, py = y - cy;            // back to Q^T chi
        const double alpha = q00 * px + q01 * py;
        const double beta  = q10 * px + q11 * py;
        return Vec3{-gamma * n.x + alpha * u.x + beta * v.x,
                    -gamma * n.y + alpha * u.y + beta * v.y,
                    -gamma * n.z + alpha * u.z + beta * v.z};
    };

    // --- the conic in its own frame
    Conic conic;
    conic.hyperbolic = detB < 0.0;
    if (!conic.hyperbolic) {
        if (C / lam1 <= 0.0 || C / lam2 <= 0.0) {          // the conic is imaginary
            out.shaded = blocked(ray(cx, cy)) ? disc_area : 0.0; find_boundary(); return out;
        }
        conic.A = std::sqrt(C / lam1);
        conic.B = std::sqrt(C / lam2);
        conic.swapped = false;
    } else {
        if (C / lam1 > 0.0) { conic.A = std::sqrt(C / lam1); conic.B = std::sqrt(-C / lam2); conic.swapped = false; }
        else                { conic.A = std::sqrt(C / lam2); conic.B = std::sqrt(-C / lam1); conic.swapped = true;  }
    }

    // --- where the solar disc crosses the silhouette.
    //
    // NOT THROUGH LI19 eq 32-33.  That substitution is x = ((1-eta^2)/(1+eta^2))
    // R0 + tx/2, y = (2 eta/(1+eta^2)) R0 + ty/2, which is the Weierstrass
    // substitution eta = tan(theta/2) applied to the circle, and it has a pole:
    // theta = pi is not the image of any finite eta, so the point (-R0 + tx/2,
    // ty/2) is dropped from the root set of the quartic.  Parametrising the
    // circle by theta directly is the SAME equation -- a trigonometric polynomial
    // of degree two, hence at most four roots, the quartic's four -- with no
    // missing root and no Ferrari resolvent.
    const auto G = [&](double th) noexcept {
        const double x = cx + R0 * std::cos(th), y = cy + R0 * std::sin(th);
        return lam1 * x * x + lam2 * y * y - C;
    };
    constexpr int kSamples = 2048;      // >> 4 roots; a pair closer than 0.18 deg
                                        // is a tangency contributing no area
    std::vector<double> cross;
    double previous = G(0.0);
    for (int i = 1; i <= kSamples; ++i) {
        const double th = 2.0 * pi * i / kSamples;
        const double g = G(th);
        if ((previous < 0.0) != (g < 0.0)) {
            double lo = 2.0 * pi * (i - 1) / kSamples, hi = th;
            for (int k = 0; k < 80; ++k) {
                const double mid = 0.5 * (lo + hi);
                ((G(lo) < 0.0) != (G(mid) < 0.0)) ? hi = mid : lo = mid;
            }
            cross.push_back(0.5 * (lo + hi));
        }
        previous = g;
    }

    const auto on_circle = [&](double th) {
        return std::array<double, 2>{cx + R0 * std::cos(th), cy + R0 * std::sin(th)};
    };

    if (cross.empty()) {
        // No crossing: the disc is wholly blocked, wholly lit, or wholly contains
        // the silhouette.  The third is the PPM's version of an annular eclipse
        // and it is an area, not a branch of its own.
        const auto c = on_circle(0.0);
        if (blocked(ray(c[0], c[1]))) { out.shaded = disc_area; find_boundary(); return out; }
        if (!conic.hyperbolic) {
            const auto p = conic.point(0.0, 1.0);
            const bool inside_disc = std::hypot(p[0] - cx, p[1] - cy) <= R0;
            if (inside_disc && blocked(ray(0.0, 0.0)))
                { out.shaded = std::min(disc_area, pi * conic.A * conic.B); find_boundary(); return out; }
        }
        find_boundary(); return out;
    }
    if (cross.size() % 2 != 0) return fail("SHDW-F-006", "an odd number of solar-disc/silhouette crossings");

    // --- Green's theorem around the blocked part of the disc.  The cycle
    //     alternates: a circular arc that is blocked, then the conic arc that
    //     carries the boundary back.  The sign of the total depends on a
    //     traversal sense nothing here fixes, so the magnitude is taken; the
    //     region is connected and simply connected in every case this reaches.
    double total = 0.0;
    const std::size_t m = cross.size();
    for (std::size_t i = 0; i < m; ++i) {
        const double th1 = cross[i], th2 = cross[(i + 1) % m] + (i + 1 == m ? 2.0 * pi : 0.0);
        const auto mid = on_circle(0.5 * (th1 + th2));
        if (!blocked(ray(mid[0], mid[1]))) continue;
        total += circle_sweep(cx, cy, R0, th1, th2);

        // back along the conic from the arc's end to its start
        const auto p2 = on_circle(th2), p1 = on_circle(th1);
        const double t2 = conic.parameter(p2[0], p2[1]), t1 = conic.parameter(p1[0], p1[1]);
        const double sigma = conic.branch(p2[0], p2[1]);
        double a = t2, b = t1;
        if (!conic.hyperbolic) {                       // pick the way round that stays in the disc
            double span = b - a;
            while (span <= 0.0) span += 2.0 * pi;
            const auto q = conic.point(a + 0.5 * span, sigma);
            if (std::hypot(q[0] - cx, q[1] - cy) > R0) span -= 2.0 * pi;
            b = a + span;
        }
        total += conic.sweep(a, b, sigma);
    }
    double shaded = std::abs(total);
    if (shaded > disc_area) shaded = disc_area;
    out.shaded = shaded;
    find_boundary();
    return out;
}

}  // namespace

namespace detail {

odl::Result<PerspectiveResult, ShadowError>
perspective_on(const Vec3& sun_m, const Vec3& sat_m, double equatorial_m, double polar_m,
               Atmosphere atmosphere, double gamma) {
    auto solid = project(sun_m, sat_m, equatorial_m, polar_m, gamma);
    if (!solid) return odl::err(solid.error());

    PerspectiveResult out;
    out.silhouette = solid->kind;
    const double disc = solid->disc_area;
    const auto settle = [&](double lit_fraction) {
        out.fraction = std::clamp(lit_fraction, 0.0, 1.0);
        out.state = out.fraction >= 1.0 ? State::sunlight
                                        : (out.fraction <= 0.0 ? State::umbra : State::penumbra);
        return odl::Result<PerspectiveResult, ShadowError>{out};
    };

    if (atmosphere == Atmosphere::none) return settle(1.0 - solid->shaded / disc);

    // ----------------------------------------------------------------------- //
    // LI19 §2.6, eq 40-46.  The five cases are the paper's, and they are NOT a
    // mean reduction over the annulus: eq 43 and 45 pin one end of the average
    // at mu2 = 1 and mu1 = 0 respectively, so the average is one half only in
    // case c and only when the disc sits symmetrically.  An implementation that
    // takes one half throughout agrees with the paper in the middle of a pass
    // and disagrees at both ends of it, which is the shape of an error that
    // integrates into the along-track direction and cancels nowhere.
    auto toa = project(sun_m, sat_m, equatorial_m + kAtmosphereThicknessM,
                       polar_m + kAtmosphereThicknessM, gamma);
    if (!toa) return odl::err(toa.error());

    const double eps = 1e-12 * disc;
    if (toa->shaded <= eps) return settle(1.0);                        // case a, eq 42
    if (solid->shaded >= disc - eps) return settle(0.0);               // case e, eq 46

    const double uE = solid->u_boundary, uT = toa->u_boundary;
    if (!std::isfinite(uE) || !std::isfinite(uT) || !(uT > uE))
        return odl::err(ShadowError{"SHDW-F-008",
            "the depth into the atmosphere is not measurable along the Earth-centre-to-Sun-centre "
            "line that LI19 eq 40 defines it on"});
    const double H0 = uT - uE;
    const auto depth = [&](double u_at) { return std::clamp((u_at - uE) / H0, 0.0, 1.0); };
    const double R0 = solid->R0;
    const double G1 = std::max(-R0, uE), G2 = std::min(R0, uT);

    const bool solid_clear = solid->shaded <= eps;
    const bool disc_inside_toa = toa->shaded >= disc - eps;
    if (solid_clear && !disc_inside_toa)                               // case b, eq 43
        return settle((0.5 * (depth(G1) + 1.0) * toa->shaded + (disc - toa->shaded)) / disc);
    if (solid_clear && disc_inside_toa)                                // case c, eq 44
        return settle(0.5 * (depth(G1) + depth(G2)));
    if (!solid_clear && disc_inside_toa)                               // case d, eq 45
        return settle(0.5 * depth(G2) * (toa->shaded - solid->shaded) / disc);

    return odl::err(ShadowError{"SHDW-F-009",
        "the solar disc crosses the solid Earth's image and the atmosphere's image at once, which "
        "needs the disc to be wider than the atmosphere's image; LI19 Fig. 8's five cases do not "
        "cover it and this refuses rather than pick one of them"});
}

}  // namespace detail

odl::Result<PerspectiveResult, ShadowError>
perspective(const frames::Position<frames::Frame::ITRS>& sun,
            const frames::Position<frames::Frame::ITRS>& sat, Atmosphere atmosphere) {
    // gamma cancels: the silhouette's coefficients scale as gamma^2 under
    // (alpha,beta) -> gamma (alpha,beta) and R0 as gamma, so Fs is a ratio of
    // areas that are both quadratic in it.  One metre, and SHDW-A-010 proves it.
    return detail::perspective_on(sun.metres(), sat.metres(), kEarthEquatorialRadiusM,
                                  kEarthPolarRadiusM, atmosphere, 1.0);
}

}  // namespace odl::shadow
