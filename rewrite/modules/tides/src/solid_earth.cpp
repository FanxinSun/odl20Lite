#include <odl/tides/solid_earth.hpp>

#include <odl/gravity/field.hpp>
#include <odl/gravity/legendre.hpp>

#include "solid_tide_tables.hpp"

#include <array>
#include <cmath>
#include <sstream>
#include <vector>

namespace odl::tides {

namespace {

using tables::SolidTideTerm;

// TN36-1, and NOT EGM2008's scaling pair (PERT-R-009).
constexpr double kRe = 6378136.6;                 ///< m, TN36-1's a_E
constexpr double kMoonEarthMassRatio = 0.0123000371;
constexpr double kGmSunOverGmEarth = 1.32712442099e20 / 3.986004418e14;

/// TN36-6 Table 6.3, both columns.  Index [n-2][m].
struct LoveSet {
    std::array<std::array<double, 4>, 2> k_real{};   ///< degrees 2 and 3
    std::array<std::array<double, 4>, 2> k_imag{};
    std::array<double, 3> k_plus{};                  ///< k2m^(+), m = 0,1,2
};

LoveSet love_set(LoveNumbers which) noexcept {
    LoveSet s;
    if (which == LoveNumbers::Anelastic) {
        s.k_real[0] = {0.30190, 0.29830, 0.30102, 0.0};
        s.k_imag[0] = {-0.00000, -0.00144, -0.00130, 0.0};
        s.k_plus = {-0.00089, -0.00080, -0.00057};
    } else {
        s.k_real[0] = {0.29525, 0.29470, 0.29801, 0.0};
        s.k_imag[0] = {0.0, 0.0, 0.0, 0.0};
        s.k_plus = {-0.00087, -0.00079, -0.00057};
    }
    // Degree 3 is real in both columns; Table 6.3 gives 0.093, 0.093, 0.093, 0.094.
    s.k_real[1] = {0.093, 0.093, 0.093, 0.094};
    s.k_imag[1] = {0.0, 0.0, 0.0, 0.0};
    return s;
}

struct Band {
    const SolidTideTerm* rows;
    std::size_t count;
    double amp_scale;
};

Band band_of(int m) noexcept {
    switch (m) {
        case 0: return {tables::kSolidTideZonal, std::size(tables::kSolidTideZonal),
                        tables::kAmplitudeScale};
        case 1: return {tables::kSolidTideDiurnal, std::size(tables::kSolidTideDiurnal),
                        tables::kAmplitudeScale};
        default: return {tables::kSolidTideSemidiurnal, std::size(tables::kSolidTideSemidiurnal),
                         tables::kAmplitudeScale};
    }
}

/// theta_f = m (theta_g + 180 deg) - sum_j N_j F_j, TN36-6 below (6.8e).
/// gamma is theta_g + pi, so the first term is m * gamma exactly.
double theta_of(const SolidTideTerm& t, const eop::tides::Arguments& a) noexcept {
    const std::array<double, 5> F{a.l, a.lp, a.F, a.D, a.Om};
    double s = static_cast<double>(t.doodson[0]) * a.gamma;
    for (std::size_t j = 0; j < 5; ++j) s -= static_cast<double>(t.delaunay[j]) * F[j];
    return s;
}

/// One body's contribution to (6.6)'s sum, for one (n, m): returns
/// (A, B) with A = sum cos(m lambda) * ..., B = sum sin(m lambda) * ...
struct AB { double a = 0.0, b = 0.0; };

AB body_sum(const Vec3& body_m, double gm_ratio, int n, int m,
            const gravity::RecursionTable& rec) noexcept {
    const double r = body_m.norm();
    const double rho = std::hypot(body_m.x, body_m.y);
    const double u = body_m.z / r;
    const double c = rho / r;
    double cl = 1.0, sl = 0.0;
    if (rho > 0.0) { cl = body_m.x / rho; sl = body_m.y / rho; }

    std::array<double, 8> P{}, dP{};
    gravity::legendre_column(rec, m, n, u, 1.0, P.data(), dP.data());
    // Pbar_nm = Pbar'_nm * cos^m(phi): at degree 3 there is no range problem and
    // the factored form is not needed, so the power is formed directly.
    const double pbar = P[static_cast<std::size_t>(n)] * std::pow(c, static_cast<double>(m));

    // cos(m lambda), sin(m lambda)
    double cm = 1.0, sm = 0.0;
    for (int k = 0; k < m; ++k) {
        const double nc = cm * cl - sm * sl;
        sm = sm * cl + cm * sl;
        cm = nc;
    }
    const double scale = gm_ratio * std::pow(kRe / r, static_cast<double>(n) + 1.0) * pbar;
    return AB{scale * cm, scale * sm};
}

}  // namespace

std::size_t SolidEarthTide::constituents(int band) noexcept { return band_of(band).count; }

double SolidEarthTide::theta_f(int band, std::size_t row,
                               const eop::tides::Arguments& args) noexcept {
    const Band b = band_of(band);
    return row < b.count ? theta_of(b.rows[row], args) : 0.0;
}

double SolidEarthTide::permanent_c20(LoveNumbers love) noexcept {
    return kA0 * kH0 * love_set(love).k_real[0][0];
}

odl::Result<TideIncrements, TidesError>
SolidEarthTide::step1(const frames::Position<frames::Frame::ITRS>& sun,
                      const frames::Position<frames::Frame::ITRS>& moon, LoveNumbers love) {
    if (!(sun.norm_m() > 0.0) || !(moon.norm_m() > 0.0)) {
        return odl::err(TidesError{"PERT-F-001",
                                   "the solid Earth tide needs non-degenerate Sun and Moon "
                                   "positions; one of them has zero length"});
    }
    const LoveSet k = love_set(love);
    const gravity::RecursionTable rec{8};

    std::ostringstream p;
    p << name_of(love) << " Love numbers, TN36-6 Table 6.3";
    TideIncrements out{4, TideSystem::TideFree,
                       ModelRecord{"solid Earth tide, step 1", "TN36-6 (6.6), (6.7)", p.str()}};

    for (int n = 2; n <= 3; ++n) {
        for (int m = 0; m <= n; ++m) {
            AB s{};
            for (auto [pos, ratio] : {std::pair{moon.metres(), kMoonEarthMassRatio},
                                      std::pair{sun.metres(), kGmSunOverGmEarth}}) {
                const AB t = body_sum(pos, ratio, n, m, rec);
                s.a += t.a;
                s.b += t.b;
            }
            // (kR + i kI)(A - iB) / (2n+1)
            const double kr = k.k_real[static_cast<std::size_t>(n - 2)][static_cast<std::size_t>(m)];
            const double ki = k.k_imag[static_cast<std::size_t>(n - 2)][static_cast<std::size_t>(m)];
            const double d = 2.0 * n + 1.0;
            out.add(n, m, (kr * s.a + ki * s.b) / d, (kr * s.b - ki * s.a) / d);
        }
    }
    // (6.7): the degree-4 changes the degree-2 tides make, through k2m^(+).
    for (int m = 0; m <= 2; ++m) {
        AB s{};
        for (auto [pos, ratio] : {std::pair{moon.metres(), kMoonEarthMassRatio},
                                  std::pair{sun.metres(), kGmSunOverGmEarth}}) {
            const AB t = body_sum(pos, ratio, 2, m, rec);
            s.a += t.a;
            s.b += t.b;
        }
        const double kp = k.k_plus[static_cast<std::size_t>(m)];
        out.add(4, m, kp * s.a / 5.0, kp * s.b / 5.0);
    }
    return out;
}

odl::Result<TideIncrements, TidesError>
SolidEarthTide::step2_one(int band, std::size_t row, double theta_f) {
    const Band b = band_of(band);
    if (band < 0 || band > 2 || row >= b.count) {
        std::ostringstream m;
        m << "no constituent " << row << " in band " << band << "; the bands carry "
          << std::size(tables::kSolidTideZonal) << ", " << std::size(tables::kSolidTideDiurnal)
          << " and " << std::size(tables::kSolidTideSemidiurnal) << " constituents.";
        return odl::err(TidesError{"PERT-F-006", m.str()});
    }
    const SolidTideTerm& t = b.rows[row];
    const double ip = t.amp_ip * b.amp_scale;
    const double op = t.amp_op * b.amp_scale;
    const double ct = std::cos(theta_f), st = std::sin(theta_f);

    TideIncrements out{2, TideSystem::TideFree,
                       ModelRecord{"solid Earth tide, step 2, one constituent",
                                   "TN36-6 (6.8a)/(6.8b)", ""}};
    if (band == 0) {
        // (6.8a): Re sum (A0 dk H) e^{i theta} = ip cos - op sin.
        out.add(2, 0, ip * ct - op * st, 0.0);
    } else if (band == 1) {
        // (6.8b) with eta_1 = -i:
        //   dC - i dS = -i (ip + i op)(cos + i sin)
        //             = (ip sin + op cos) - i (ip cos - op sin)
        out.add(2, 1, ip * st + op * ct, ip * ct - op * st);
    } else {
        // (6.8b) with eta_2 = 1.
        out.add(2, 2, ip * ct - op * st, -(ip * st + op * ct));
    }
    return out;
}

TideIncrements SolidEarthTide::step2(const eop::tides::Arguments& args) {
    TideIncrements out{2, TideSystem::TideFree,
                       ModelRecord{"solid Earth tide, step 2",
                                   "TN36-6 Tables 6.5a, 6.5b, 6.5c", ""}};
    for (int band = 0; band <= 2; ++band) {
        const Band b = band_of(band);
        for (std::size_t i = 0; i < b.count; ++i) {
            auto one = step2_one(band, i, theta_of(b.rows[i], args));
            if (!one) continue;                      // unreachable: the index is in range
            for (int m = 0; m <= 2; ++m) out.add(2, m, one->dc(2, m), one->ds(2, m));
        }
    }
    return out;
}

odl::Result<TideIncrements, TidesError>
SolidEarthTide::increments(const frames::Position<frames::Frame::ITRS>& sun,
                           const frames::Position<frames::Frame::ITRS>& moon,
                           const eop::tides::Arguments& args, LoveNumbers love,
                           TideSystem target) {
    auto s1 = step1(sun, moon, love);
    if (!s1) return odl::err(s1.error());
    const TideIncrements s2 = step2(args);

    std::ostringstream p;
    p.precision(10);
    p << name_of(love) << " Love numbers; " << constituents(0) << " + " << constituents(1)
      << " + " << constituents(2) << " step-2 constituents; target " << gravity::name_of(target);
    TideIncrements out{4, target,
                       ModelRecord{"solid Earth tide, TN36-6 §6.2, three steps",
                                   "TN36-6 (6.6), (6.7), (6.8), (6.13)", p.str()}};
    for (int n = 0; n <= 4; ++n) {
        for (int m = 0; m <= n; ++m) out.add(n, m, s1->dc(n, m) + s2.dc(n, m),
                                             s1->ds(n, m) + s2.ds(n, m));
    }
    // STEP 3.  With a zero-tide background field the permanent part must be
    // removed, TN36-6 (6.13).  Leaving it in counts the permanent deformation
    // twice, once in the field's own C20 and once here.
    if (target == TideSystem::ZeroTide) out.add(2, 0, -permanent_c20(love), 0.0);
    return out;
}

}  // namespace odl::tides
