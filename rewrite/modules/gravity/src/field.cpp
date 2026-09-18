#include <odl/gravity/field.hpp>

#include <odl/gravity/legendre.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace odl::gravity {

namespace {

// TN36-6 Table 6.2, at J2000.0, with the rates of (6.4).  The zonal C20 is the
// ZERO-TIDE value from seventeen years of SLR (Cheng et al. 2010); the file's is
// the tide-free value from four years of GRACE, and TN36-6 §6.1 says plainly
// that they differ significantly.  The difference is 8x the 2e-11 uncertainty
// the Conventions state, so the substitution is mandatory and not cosmetic.
constexpr double kC20ZeroTide = -0.48416948e-3;
constexpr double kC20Rate     = 11.6e-12;
constexpr double kC30         = 0.9571612e-6;
constexpr double kC30Rate     = 4.9e-12;
constexpr double kC40         = 0.5399659e-6;
constexpr double kC40Rate     = 4.7e-12;

// TN36-6 §6.1: the values (6.5) may use, stated there as adequate for 1e-14
// accuracy in that equation.  They are CONSTANTS in (6.5) and are deliberately
// not the epoch-propagated C20 above.
constexpr double kC20For65 = -0.48416948e-3;
constexpr double kC22For65 = 2.4393836e-6;
constexpr double kS22For65 = -1.4002737e-6;

constexpr double kSqrt3 = 1.7320508075688772935;

struct Spherical {
    double r = 0.0;
    double u = 0.0;      ///< sin(phi)
    double c = 0.0;      ///< cos(phi)
    double cl = 1.0;     ///< cos(lambda)
    double sl = 0.0;     ///< sin(lambda)
};

Spherical to_spherical(const Vec3& p) noexcept {
    Spherical s;
    const double rho = std::hypot(p.x, p.y);
    s.r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    s.u = p.z / s.r;
    s.c = rho / s.r;
    // GRAV-R-036.  On the polar axis the longitude is undefined and is set to
    // zero.  That is not a fudge: the horizontal acceleration at the pole comes
    // entirely from the order-1 terms, and evaluating them at lambda = 0 gives
    // the correct Cartesian components through the rotation below.
    if (rho > 0.0) {
        s.cl = p.x / rho;
        s.sl = p.y / rho;
    }
    return s;
}

}  // namespace

// --------------------------------------------------------------------------
// The recursion coefficients, from exact integers, once.
// --------------------------------------------------------------------------

RecursionTable::RecursionTable(int max_degree) {
    const std::size_t entries = CoefficientSet::index(max_degree, max_degree) + 1;
    a_.assign(entries, 0.0);
    b_.assign(entries, 0.0);
    pmm_.assign(static_cast<std::size_t>(max_degree) + 1, 0.0);

    // Pbar'_mm.  GRAV-R-033: m = 1 does NOT follow the general sectorial step,
    // because (2 - delta_0m) changes between m = 0 and m = 1.  The general step
    // would give sqrt(3/2) where the answer is sqrt(3) — a factor of sqrt(2) on
    // every order-1 term, C21 and S21 among them.
    pmm_[0] = 1.0;
    if (max_degree >= 1) pmm_[1] = kSqrt3;
    for (int m = 2; m <= max_degree; ++m) {
        pmm_[static_cast<std::size_t>(m)] =
            pmm_[static_cast<std::size_t>(m) - 1]
            * std::sqrt(static_cast<double>(2 * m + 1) / static_cast<double>(2 * m));
    }

    for (int m = 0; m <= max_degree; ++m) {
        for (int n = m + 2; n <= max_degree; ++n) {
            // Every product below is an exact integer in a double at degree
            // 2190: the largest is (2n+1)(n+m-1)(n-m-1) < 4.3e10 << 2^53.
            const double num_a = static_cast<double>(2 * n - 1) * static_cast<double>(2 * n + 1);
            const double den_a = static_cast<double>(n - m) * static_cast<double>(n + m);
            const double num_b = static_cast<double>(2 * n + 1)
                               * static_cast<double>(n + m - 1) * static_cast<double>(n - m - 1);
            const double den_b = static_cast<double>(2 * n - 3)
                               * static_cast<double>(n + m) * static_cast<double>(n - m);
            const std::size_t i = CoefficientSet::index(n, m);
            a_[i] = std::sqrt(num_a / den_a);
            b_[i] = std::sqrt(num_b / den_b);
        }
    }
}

// --------------------------------------------------------------------------
// Loading, and the conventional model
// --------------------------------------------------------------------------

odl::Result<GravityModel, GravityError>
GravityModel::load(const std::string& coefficient_path, const std::string& cache_root,
                   ScalingParameters scaling) {
    auto coeffs = CoefficientSet::load_egm2008(coefficient_path, cache_root);
    if (!coeffs) return odl::err(coeffs.error());

    Provenance prov;
    prov.coefficient_file = coeffs->source_path();
    prov.coefficient_sha256_declared_in = "manifest/manifest.json: egm2008-coefficients, "
                                          "member EGM2008_to2190_TideFree";
    prov.records = coeffs->record_count();
    prov.file_tide_system = coeffs->tide_system();
    prov.scaling_source = scaling.source();
    prov.gm_compatibility = scaling.compatibility();

    const int nmax = coeffs->max_degree();
    auto data = std::make_shared<ModelData>(ModelData{std::move(*coeffs), RecursionTable{nmax}});
    return GravityModel{std::move(data), std::move(scaling), std::move(prov)};
}

odl::Result<ConventionalField, GravityError>
GravityModel::conventional(const odl::time::Epoch& tt,
                           bool extrapolate_secular_terms_beyond_fit) const {
    auto pole = SecularPole::at(tt, extrapolate_secular_terms_beyond_fit);
    if (!pole) return odl::err(pole.error());

    const double t = SecularPole::years_from_2000(tt);
    const double c20 = kC20ZeroTide + kC20Rate * t;
    const double c30 = kC30 + kC30Rate * t;
    const double c40 = kC40 + kC40Rate * t;

    // TN36-6 (6.5).  xf, yf in radians.
    const double xf = pole->x_rad, yf = pole->y_rad;
    const double c21 =  kSqrt3 * xf * kC20For65 - xf * kC22For65 + yf * kS22For65;
    const double s21 = -kSqrt3 * yf * kC20For65 - yf * kC22For65 - xf * kS22For65;

    const CoefficientSet& base = d_->coefficients;
    std::vector<Substitution> subs{
        {2, 0, 'C', base.c(2, 0), c20, "TN36-6 Table 6.2 (zero tide, Cheng et al. 2010) with (6.4)"},
        {3, 0, 'C', base.c(3, 0), c30, "TN36-6 Table 6.2 with (6.4)"},
        {4, 0, 'C', base.c(4, 0), c40, "TN36-6 Table 6.2 with (6.4)"},
        {2, 1, 'C', base.c(2, 1), c21, "TN36-6 (6.5) with the secular pole of TN36-7 (21)"},
        {2, 1, 'S', base.s(2, 1), s21, "TN36-6 (6.5) with the secular pole of TN36-7 (21)"},
    };

    Provenance prov = prov_;
    prov.extrapolated_beyond_pole_fit = extrapolate_secular_terms_beyond_fit;
    return ConventionalField{d_, scaling_, std::move(prov), *pole, std::move(subs),
                             c20, c21, s21, c30, c40};
}

double ConventionalField::degree_amplitude(int n) const noexcept {
    if (n < 0 || n > max_degree()) return 0.0;
    if (n > 4) return d_->coefficients.degree_amplitude(n);
    double sum = 0.0;
    for (int m = 0; m <= n; ++m) {
        const double cc = c(n, m), ss = s(n, m);
        sum += cc * cc + ss * ss;
    }
    return std::sqrt(sum);
}

// --------------------------------------------------------------------------
// The synthesis
// --------------------------------------------------------------------------

namespace {

/// Everything the three public evaluators share.  Written once because the
/// potential and the acceleration must come from the same coefficients by the
/// same recursion, or GRAV-A-005's finite-difference check compares two
/// different fields and passes.
struct Synthesis {
    double potential = 0.0;
    Vec3 acceleration{};
    std::vector<Vec3> by_degree;
};

// The global scale factor, and the measurement that fixes it.
//
// THE SPECIFICATION'S §3.6 WAS HALF RIGHT AND IMPLEMENTATION FOUND THE OTHER
// HALF.  Carrying Pbar_nm unfactored underflows a double above 43.7 degrees of
// latitude at order 2190 — that much §3.6 measured correctly.  But the factored
// function it prescribed instead is NOT bounded: only the SECTORIAL seed
// Pbar'_mm is, at 10.277577.  Away from the sectorial diagonal,
//
//     max over m of Pbar'_{2190,m}(1) = 10^457.864, at m = 979
//
// computed from the closed form Pbar'_nm(1) = sqrt((2n+1)(2-delta_0m))
// sqrt((n+m)!/(n-m)!) / (2^m m!), which OVERFLOWS a double by 10^150.  The two
// representations fail in opposite directions and neither works alone.
//
// What works is both at once, which is Holmes and Featherstone's construction:
// scale every associated Legendre function by 10^-280, and fold the powers of
// cos(phi) back in through a HORNER NEST over order rather than forming
// cos^m(phi) as a number.  The scale puts the range 10^-279 to 10^178 inside a
// double with room at both ends, and the nest never multiplies two numbers that
// are separately unrepresentable.  There is no cancellation in the nest: the
// large intermediates are large precisely because they are about to be
// multiplied by many factors of cos(phi), and each step scales down rather than
// subtracting.
//
// 10^-280 is HF02's number and this tree could not obtain HF02.  It is used here
// because the measurement above says it is right — 10^457.9 - 280 = 10^177.9 at
// the top, 10^-280 for a unit result at the bottom — not because it was cited.
constexpr double kScale    = 1e-280;
constexpr double kUnscale  = 1e280;

Synthesis synthesise(const ConventionalField& f, const Vec3& pos_m, int N, int M,
                     bool want_by_degree) {
    const Spherical sph = to_spherical(pos_m);
    const double gm = f.scaling().gm_m3_s2();
    const double ae = f.scaling().ae_m();
    const double ratio = ae / sph.r;
    const double c = sph.c;
    const double u = sph.u;

    const auto n1 = static_cast<std::size_t>(N) + 1;
    // The Horner accumulators.  H1 and H3b carry the cos^m(phi) nest; H2 and
    // H3a carry the cos^(m-1)(phi) one, which is where the spherical gradient's
    // 1/(r cos phi) went.  GRAV-R-035: there is no division by cos(phi) here or
    // anywhere else, so the pole is an ordinary point.
    std::vector<double> H1(n1, 0.0), H2(n1, 0.0), H3a(n1, 0.0), H3b(n1, 0.0);
    std::vector<double> P(n1, 0.0), dP(n1, 0.0);

    // cos(m lambda) and sin(m lambda) for m = M down to 0, by the standard
    // downward pair recursion from the two highest orders.  These are the
    // ANGLES only: no power of cos(phi) is attached to them, because attaching
    // one is the thing that overflows.
    const double cl = sph.cl, sl = sph.sl;
    double cm = 1.0, sm = 0.0;
    for (int k = 1; k <= M; ++k) {
        const double nc = cm * cl - sm * sl;
        const double ns = sm * cl + cm * sl;
        cm = nc;
        sm = ns;
    }

    for (int m = M; m >= 0; --m) {
        legendre_column(f.recursion(), m, N, u, kScale, P.data(), dP.data());

        const double dm = static_cast<double>(m);
        for (int n = m; n <= N; ++n) {
            const auto un = static_cast<std::size_t>(n);
            const double C = f.c(n, m), S = f.s(n, m);
            const double p = P[un], dp = dP[un];
            const double w  = C * cm + S * sm;          // C cos(m lambda) + S sin(m lambda)
            const double wp = S * cm - C * sm;          // S cos(m lambda) - C sin(m lambda)
            H1[un]  = w * p          + c * H1[un];
            H3b[un] = w * dp         + c * H3b[un];
            if (m > 0) {
                H2[un]  = dm * wp * p        + c * H2[un];
                H3a[un] = -dm * u * w * p    + c * H3a[un];
            }
        }

        // Step the longitude pair down one order: a rotation by -lambda, which
        // is norm-preserving in exact arithmetic and drifts in floating point.
        // Over 2159 steps the drift is small but it costs one square root per
        // order to remove entirely, against 2.4 million terms in the sums, so
        // there is no reason to carry the doubt.
        if (m > 0) {
            const double pc = cm * cl + sm * sl;        // cos((m-1) lambda)
            const double ps = sm * cl - cm * sl;        // sin((m-1) lambda)
            const double norm = std::hypot(pc, ps);
            cm = pc / norm;
            sm = ps / norm;
        }
    }

    const double inv_r2 = 1.0 / (sph.r * sph.r);
    Synthesis out;
    if (want_by_degree) out.by_degree.reserve(n1);

    double ar = 0.0, ath = 0.0, aph = 0.0, v = 0.0;
    double power = 1.0;
    for (int n = 0; n <= N; ++n) {
        const auto un = static_cast<std::size_t>(n);
        // Unscale BEFORE applying (ae/r)^n.  The other order underflows: the
        // scaled degree-2190 sum is around 10^-289 and (ae/r)^2190 at 7331 km is
        // 10^-133, and their product is not a double.
        const double t1 = H1[un] * kUnscale;
        const double t2 = H2[un] * kUnscale;
        const double t3 = (H3a[un] + c * H3b[un]) * kUnscale;
        v += power * t1;
        const double ar_n = -(static_cast<double>(n) + 1.0) * power * t1 * gm * inv_r2;
        const double ath_n = power * t2 * gm * inv_r2;
        const double aph_n = power * t3 * gm * inv_r2;
        ar += ar_n;
        ath += ath_n;
        aph += aph_n;
        if (want_by_degree) {
            // NASA-TP (1.25), the orthogonal-spherical to body-fixed rotation.
            out.by_degree.push_back(Vec3{
                c * cl * ar_n - sl * ath_n - u * cl * aph_n,
                c * sl * ar_n + cl * ath_n - u * sl * aph_n,
                u * ar_n + c * aph_n});
        }
        power *= ratio;
    }

    out.potential = gm * v / sph.r;
    out.acceleration = Vec3{c * cl * ar - sl * ath - u * cl * aph,
                            c * sl * ar + cl * ath - u * sl * aph,
                            u * ar + c * aph};
    return out;
}

odl::Result<std::pair<int, int>, GravityError>
checked_extent(const ConventionalField& f, const frames::ItrsPosition& at,
               Degree degree, Order order) {
    const int N = degree.value(), M = order.value();
    if (M > N) {
        std::ostringstream m;
        m << "order " << M << " exceeds degree " << N
          << ". Order cannot exceed degree; requesting it usually means the two arguments were "
             "passed the other way round (GRAV-F-004).";
        return odl::err(GravityError{"GRAV-F-004", m.str()});
    }
    if (N > f.max_degree()) {
        std::ostringstream m;
        m << "requested degree " << N << "; this coefficient set carries " << f.max_degree();
        return odl::err(GravityError{"GRAV-F-004", m.str()});
    }
    const Vec3& p = at.metres();
    const double r2 = p.x * p.x + p.y * p.y + p.z * p.z;
    if (!std::isfinite(r2) || r2 <= 0.0) {
        std::ostringstream m;
        m.precision(17);
        m << "the field is not defined at this position: (" << p.x << ", " << p.y << ", " << p.z
          << ") m, r = " << std::sqrt(r2)
          << ". Returning zero, or the two-body term, would be a number where there is none "
             "(GRAV-F-001).";
        return odl::err(GravityError{"GRAV-F-001", m.str()});
    }
    return std::pair<int, int>{N, M};
}

}  // namespace

odl::Result<frames::ItrsAcceleration, GravityError>
ConventionalField::acceleration(const frames::ItrsPosition& at, Degree degree, Order order) const {
    auto extent = checked_extent(*this, at, degree, order);
    if (!extent) return odl::err(extent.error());
    const auto s = synthesise(*this, at.metres(), extent->first, extent->second, false);
    return frames::ItrsAcceleration{s.acceleration};
}

odl::Result<double, GravityError>
ConventionalField::potential(const frames::ItrsPosition& at, Degree degree, Order order) const {
    auto extent = checked_extent(*this, at, degree, order);
    if (!extent) return odl::err(extent.error());
    return synthesise(*this, at.metres(), extent->first, extent->second, false).potential;
}

odl::Result<std::vector<Vec3>, GravityError>
ConventionalField::acceleration_by_degree(const frames::ItrsPosition& at, Degree degree,
                                          Order order) const {
    auto extent = checked_extent(*this, at, degree, order);
    if (!extent) return odl::err(extent.error());
    return synthesise(*this, at.metres(), extent->first, extent->second, true).by_degree;
}

odl::Result<double, GravityError>
ConventionalField::truncation_rms(double radius_m, Degree degree) const {
    if (!(radius_m > 0.0) || !std::isfinite(radius_m)) {
        std::ostringstream m;
        m << "truncation RMS asked at radius " << radius_m << " m";
        return odl::err(GravityError{"GRAV-F-001", m.str()});
    }
    // GRAV-R-041.  From the coefficients alone, without evaluating the field:
    //   rms||a - a_N|| = (GM/r^2) sqrt( sum_{n>N} (ae/r)^{2n} sigma_n^2 (n+1)(2n+1) )
    // which follows from the 4pi normalisation — the mean square over the sphere
    // of the degree-n angular factor is sigma_n^2, the radial derivative
    // contributes (n+1)^2 and the horizontal gradient n(n+1).
    const double ratio = scaling().ae_m() / radius_m;
    // power must be (ae/r)^n at the TOP of each iteration, and the first
    // iteration is n = degree + 1.  Advancing it before the first use instead
    // put one extra factor of (ae/r) into every term, which is a clean 13 %
    // error at 7331 km and 48 % at 12270 km — visible only because the expected
    // values came from an independent evaluation rather than from this code.
    double power = 1.0;
    for (int k = 0; k < degree.value() + 1; ++k) power *= ratio;
    double total = 0.0;
    for (int n = degree.value() + 1; n <= max_degree(); ++n) {
        const double sig = degree_amplitude(n) * power;
        total += sig * sig * (static_cast<double>(n) + 1.0) * (2.0 * static_cast<double>(n) + 1.0);
        power *= ratio;
    }
    return scaling().gm_m3_s2() / (radius_m * radius_m) * std::sqrt(total);
}

}  // namespace odl::gravity
