// forcemodel_fd_sizing.cpp — the finite-difference gate of the force plugins' Jacobians: its sizing, as the numbers it freezes.  SPEC-forcemodel.md §6.2.
//
// Plan L0 step 8, group C7, ported to C++ (the user's directive of 2026-10-06) from tools/forcemodel_fd_sizing.py, on tools/gradient_reference.cpp's exact polynomials and the devkit's BigInt and Rational.
//
// WRITTEN 2026-10-06, BEFORE ANY RUN OF THE GATE (plan §4 rule 7), in L6's form (SPEC-measmod §6.2, STM-P-1).  A central difference of a plugin's acceleration carries two errors,
//
//     truncation   T(h) = h^2 F / 6    F a bound of |d^3 a_i / d x_j^3| over the whole stencil [x - h e_j, x + h e_j]
//     noise        N(h) = nu / h       nu a bound of ONE evaluation's round-off, over every point of the stencil
//
//     bound        eps(h) = T(h) + N(h)
//
// F IS A BOUND, NOT AN ESTIMATE, and it is proved from three elementary facts, each used once:
//
//   Lemma 1 (Leibniz).  For a homogeneous polynomial P of degree d with the l1 norm ||P|| of its coefficients, and q > 0,
//         | d_e^3 ( P(x) r^-q ) |  <=  ||P|| r^(d-q-3) S3(d, q),      S3(d, q) = sum_{j=0..3} C(3,j) d^(falling 3-j) (q)_j
//       because |d_e^k P| <= ||P|| d^(falling k) r^(d-k)  (each monomial is a product of d coordinates, each bounded by r, each derivative of a linear factor bounded by 1) and |d_e^j r^-q| <= (q)_j r^(-q-j)
//       (the Gegenbauer function C_j^(q/2) peaks at +-1).
//   Lemma 2 (the potential of one normalised coefficient).  V_nm = GM a_e^n N_nm q(x) / r^(2n+1), q the harmonic polynomial of GRAV §4.7 / gradient_reference, so
//         a_i = d_i V = GM a_e^n N_nm [ (d_i q) r^-(2n+1) - (2n+1) q x_i r^-(2n+3) ]
//       and Lemma 1 applied to the two terms gives
//         | d_e^3 a_i |  <=  GM a_e^n Phi_nm r^-(n+5),
//         Phi_nm = N_nm max_i [ ||d_i q|| S3(n-1, 2n+1) + (2n+1) ||q|| S3(n+1, 2n+3) ]            (n >= 1)
//       and for the point mass (n = 0, q = 1, s = 1)   Phi_0 = S3(1, 3) = 96.
//   Lemma 3 (the stencil).  Every exponent above is negative, so the bound is largest at the smallest radius the stencil reaches, r_min = r - h_max.
//
// THE LOOSENESS is measured by --scan: the true supremum of |d_e^3 a_i| over directions, positions on the sphere and the stencil, by exact power series along the line (no finite differences), and F over it
// is printed; the gate's POWER -- the smallest relative defect it can see -- is eps(h) / ||row|| at the best step, printed with it.
//
//   forcemodel_fd_sizing [--scan] [--check] [--root DIR]
//     (no flag)  print the sizing;  --check  every frozen number reproduced;  --scan  the looseness scan (about a minute in Python; a second here)
//   exit 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced   2 an argument error, or the coefficient file is missing or unreadable   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's product is its standard output, and two records of it exist outside the tree, tied to it by the hashes PROVENANCE 40.6 records: the output of `--check` (2,088 bytes) and of
// `--scan` (2,502 bytes; its first 2,088 bytes are the other file).  The port printed both BYTE FOR BYTE, against an EMPTY substitution list registered before the comparison (the output names no tool).
// (C7_proof_registration.txt, in this group's report files, holds the registration and the results.)  What no record holds -- the MISMATCH text of a frozen number, the refusals -- stands on tests.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * Every floating-point operation is the Python's, in the Python's order, and none is fused (-ffp-contract=off): float ** int is the C library's pow() (called through a function pointer the compiler cannot
//     see through, so that it is never folded to a multiplication), math.radians(x) is x * (pi / 180) and math.degrees(x) is x * (180 / pi) as CPython computes them, and sum() of three floats is
//     CPython 3.12's compensated (Neumaier) sum.  The coefficient file's lines are kept in the file's order (a dict's order), and so are the polynomials' terms (gradient_reference.cpp's Poly).
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this tool was built from.
//   * A coefficient file that does not exist: the Python's docstring promised exit 2 and its code raised FileNotFoundError (a traceback, exit 1) after printing the first eight lines; this prints those eight lines,
//     says which file is missing, and exits 2.  A file that cannot be read, or a line that is not a coefficient line: the same, exit 2.
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--root=DIR` is taken).  `-h` prints this tool's own text.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "forcemodel_fd_sizing.hpp"

#include <math.h>   // NOLINT(modernize-deprecated-headers): ::pow, the C library's, whose address the port takes

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>

namespace dk = odl::devkit;

namespace odl::tools::forcemodel_fd_sizing {

using dk::Rational;
using dk::Streams;
using odl::tools::gradient_reference::norm_squared;
using odl::tools::gradient_reference::p_der;
using odl::tools::gradient_reference::solid_polynomial;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "forcemodel_fd_sizing";
constexpr const char kEgmRelative[] = "data/cache/egm2008-coefficients/extracted/EGM2008_to2190_TideFree";

/// float ** float of the Python: the C library's pow().  Called through a pointer the compiler cannot see through: GCC folds pow(x, 2.0) to x * x, which glibc's pow does not promise to equal in the last place.
double libm_pow(double base, double exponent) {
    static double (*volatile call)(double, double) = &::pow;
    return call(base, exponent);
}

/// math.radians and math.degrees as CPython computes them.
constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

[[nodiscard]] std::string fixed(double v, int width, int precision) {
    std::array<char, 64> buf{};
    (void)std::snprintf(buf.data(), buf.size(), "%*.*f", width, precision, v);   // (a number too long for the buffer is cut, and the buffer is always terminated)
    return std::string(buf.data());
}

[[nodiscard]] std::string scientific(double v, int width, int precision) {
    std::array<char, 64> buf{};
    (void)std::snprintf(buf.data(), buf.size(), "%*.*e", width, precision, v);   // (a number too long for the buffer is cut, and the buffer is always terminated)
    return std::string(buf.data());
}

[[nodiscard]] double radians(double degrees) { return degrees * kDegToRad; }
[[nodiscard]] double degrees(double rad) { return rad * kRadToDeg; }

}  // namespace

// sum() of three floats (CPython 3.12 and later): the first is added to the int 0, the rest by Neumaier's improved Kahan-Babuska summation, and the compensation is added at the end.
double py_sum3(double a, double b, double c) {
    double result = 0.0 + a;
    double compensation = 0.0;
    for (const double x : {b, c}) {
        const double t = result + x;
        if (std::fabs(result) >= std::fabs(x)) compensation += (result - t) + x;
        else compensation += (x - t) + result;
        result = t;
    }
    if (compensation != 0.0 && std::isfinite(compensation)) result += compensation;
    return result;
}

const std::array<ReferencePoint, 4>& reference_points() {
    // the Earth-rotation angle (ERA) at the four reference epochs, from the IERS definition 2 pi (0.7790572732640 + 1.00273781191135448 (JD_UT1 - 2451545)), |UT1 - UTC| < 0.9 s changes it by under 0.004 deg: the
    // epochs of tests/l4_ranking.cpp (GPS 2023-02-20 00h; LEO 2023-06-21 08h; sail 2019-07-01 00h); the phase is the fixture's orbit phase
    static const std::array<ReferencePoint, 4> points = {{{"GPS", 26561e3, 149.377, 0.7},
                                                          {"LEO 300 km", 6378136.3 + 300e3, 28.965, 0.0},
                                                          {"LEO 952.86 km", 6378136.3 + 952.86e3, 28.965, 0.0},
                                                          {"sail 720 km", 6378136.3 + 720e3, 278.513, 1.2}}};
    return points;
}

BigInt S3(int d, int q) {
    const auto falling = [](int base, int k) {
        BigInt out(1);
        for (int i = 0; i < k; ++i) out *= BigInt(base - i);
        return out;
    };
    const auto rising = [](int base, int k) {
        BigInt out(1);
        for (int i = 0; i < k; ++i) out *= BigInt(base + i);
        return out;
    };
    const std::array<int, 4> choose3 = {1, 3, 3, 1};   // C(3, j)
    BigInt total(0);
    for (int j = 0; j < 4; ++j) total += BigInt(choose3[static_cast<std::size_t>(j)]) * falling(d, 3 - j) * rising(q, j);
    return total;
}

Rational l1(const Poly& poly) {
    Rational total(0);
    for (const Poly::Term& term : poly.terms()) total += term.second.abs();
    return total;
}

Rational max_l1_gradient(const Poly& poly) { return std::max({l1(p_der(poly, 0)), l1(p_der(poly, 1)), l1(p_der(poly, 2))}); }

double phi(int n, int m, char kind) {
    const Poly q = solid_polynomial(n, m, kind);
    if (q.empty()) return 0.0;
    const int s = 2 * n + 1;
    const Rational dq = max_l1_gradient(q);
    const Rational inner = dq * Rational(S3(n - 1, s)) + Rational(s) * l1(q) * Rational(S3(n + 1, s + 2));
    return std::sqrt(norm_squared(n, m).to_double()) * inner.to_double();
}

double phi_point_mass() { return Rational(S3(1, 3)).to_double(); }

namespace {

/// float(S3(d, q)): the Python multiplies a float by the int.
double s3_as_double(int d, int q) { return Rational(S3(d, q)).to_double(); }

}  // namespace

std::optional<std::vector<Coefficient>> coefficients(const std::filesystem::path& file, int nmax) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return std::nullopt;
    if (!std::filesystem::is_regular_file(file, ec)) throw std::runtime_error(file.string() + " is not a regular file");
    std::ifstream in(file);
    if (!in) throw std::runtime_error("cannot read " + file.string());
    std::vector<Coefficient> out;
    const auto bad_line = [&](std::size_t number) { return std::runtime_error(file.string() + ": line " + std::to_string(number) + " is not a coefficient line"); };
    const auto integer = [&](const std::string& token, std::size_t number) {
        int value = 0;
        const auto res = std::from_chars(token.data(), token.data() + token.size(), value);
        if (res.ec != std::errc() || res.ptr != token.data() + token.size()) throw bad_line(number);
        return value;
    };
    const auto real = [&](std::string token, std::size_t number) {
        std::replace(token.begin(), token.end(), 'D', 'E');
        char* end = nullptr;
        const double value = std::strtod(token.c_str(), &end);
        if (token.empty() || end != token.c_str() + token.size()) throw bad_line(number);
        return value;
    };
    std::string line;
    for (std::size_t number = 1; std::getline(in, line); ++number) {
        const std::vector<std::string> f = dk::split_py(line);
        if (f.size() < 2) throw bad_line(number);
        const int n = integer(f[0], number);
        const int m = integer(f[1], number);
        if (n > nmax) break;
        if (f.size() < 4) throw bad_line(number);
        out.push_back({n, m, real(f[2], number), real(f[3], number)});
    }
    return out;
}

double f_gravity(int /*nmax*/, double r_min, const std::vector<Coefficient>& coefs) {
    double total = kGM * phi_point_mass() / libm_pow(r_min, 5.0);
    for (const Coefficient& cf : coefs) {
        total += kGM * libm_pow(kAE, cf.n) * (std::fabs(cf.c) * phi(cf.n, cf.m, 'C') + std::fabs(cf.s) * (cf.m > 0 ? phi(cf.n, cf.m, 'S') : 0.0)) / libm_pow(r_min, cf.n + 5);
    }
    return 1.01 * total;
}

double f_third_body(double mu, double d_min) { return mu * phi_point_mass() / libm_pow(d_min, 5.0); }

double f_schwarzschild(double r_min, double v) {
    const double k = kGmeRel / libm_pow(kC, 2.0);
    const double t1 = 4.0 * kGmeRel * k * s3_as_double(1, 4) / libm_pow(r_min, 6.0);
    const double t2 = k * v * v * s3_as_double(1, 3) / libm_pow(r_min, 5.0);
    const double t3 = 4.0 * k * v * (std::sqrt(3.0) * v) * s3_as_double(1, 3) / libm_pow(r_min, 5.0);
    return t1 + t2 + t3;
}

double f_lense_thirring(double r_min, double v) {
    const double k = 2.0 * kGmeRel / libm_pow(kC, 2.0);
    const double p1 = 3.0 * kJEarth * (std::sqrt(3.0) * v);   // ||(x.J)(x x v)_i||_1 <= |J| * ||z (x x v)_i||_1 <= |J| sqrt(3)|v| (z and two terms)
    return k * (p1 * s3_as_double(2, 5) / libm_pow(r_min, 6.0) + kJEarth * v * s3_as_double(0, 3) / libm_pow(r_min, 6.0));
}

double nu_gravity(double a_norm) { return 64.0 * kEps * a_norm; }
double nu_third_body(double mu, double d, double s) { return 16.0 * kEps * mu * (1.0 / libm_pow(d, 2.0) + 1.0 / libm_pow(s, 2.0)); }
double nu_relativity(double a_norm) { return 32.0 * kEps * a_norm; }
double nu_tides(double a_norm) { return 128.0 * kEps * a_norm; }
double eps_h(double f, double nu, double h) { return h * h * f / 6.0 + nu / h; }

namespace {

/// min over the five steps of eps(h): the smallest relative defect the sizing can see
double best_eps(double f, double nu) {
    double best = std::numeric_limits<double>::infinity();
    for (const double h : kHs) best = std::min(best, eps_h(f, nu, h));
    return best;
}

}  // namespace

std::vector<ControlPower> control_power(const std::vector<Coefficient>& coefs) {
    std::vector<ControlPower> out;
    for (const ReferencePoint& point : reference_points()) {
        const double r = point.r;
        const double mu_r3 = kGM / libm_pow(r, 3.0);
        const double th = radians(point.era_deg);
        const double ph = point.phase;
        using Vec = std::array<double, 3>;
        const Vec ug = {std::cos(ph), std::sin(ph), 0.0};   // unit vector, GCRS
        const auto rotz = [](double a, const Vec& v) { return Vec{std::cos(a) * v[0] - std::sin(a) * v[1], std::sin(a) * v[0] + std::cos(a) * v[1], v[2]}; };
        using Mat = std::array<std::array<double, 3>, 3>;
        const auto tensor = [&](const Vec& u) {
            Mat g{};
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = 0; j < 3; ++j) g[i][j] = mu_r3 * (3.0 * u[i] * u[j] - (i == j ? 1.0 : 0.0));
            }
            return g;
        };
        const Mat g_right = tensor(ug);
        const Vec ui = rotz(-th, ug);                       // GCRS -> ITRS is R3(+ERA): the vector's ITRS direction is its GCRS angle minus ERA
        const Mat g_unrot = tensor(ui);                     // the ITRS tensor used as if it were GCRS
        const Vec uw = rotz(-2.0 * th, ug);                 // the transposed rotation applied to the point-mass tensor: direction rotated by 2 ERA
        const Mat g_transposed = tensor(uw);
        Mat g_sign{};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) g_sign[i][j] = -g_right[i][j];
        }
        const double f = f_gravity(4, r - kHMax, coefs);
        const double nu = nu_gravity(kGM / libm_pow(r, 2.0));
        const double best = best_eps(f, nu);
        ControlPower row;
        row.name = point.name;
        row.th = th;
        const std::array<std::pair<const char*, const Mat*>, 3> wrong = {{{"unrotated", &g_unrot}, {"transposed", &g_transposed}, {"sign flipped", &g_sign}}};
        for (std::size_t w = 0; w < 3; ++w) {
            const Mat& g = *wrong[w].second;
            double biggest = 0.0;   // (below every fabs)
            for (std::size_t i = 0; i < 3; ++i) {
                for (std::size_t j = 0; j < 3; ++j) biggest = std::max(biggest, std::fabs(g[i][j] - g_right[i][j]));
            }
            row.row[w] = {wrong[w].first, biggest / best};
        }
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<TableRow> table(const std::vector<Coefficient>& coefs) {
    std::vector<TableRow> rows;
    for (const ReferencePoint& point : reference_points()) {
        const double r = point.r;
        const double a = kGM / libm_pow(r, 2.0);
        const double rmin = r - kHMax;
        const double f = f_gravity(4, rmin, coefs);
        const double nu = nu_gravity(a);
        TableRow row;
        row.name = point.name;
        row.r = r;
        row.f = f;
        row.nu = nu;
        for (std::size_t k = 0; k < kHs.size(); ++k) row.e[k] = eps_h(f, nu, kHs[k]);
        const double g_scale = 2.0 * kGM / libm_pow(r, 3.0);   // the largest eigenvalue of the point-mass tensor
        row.rel = *std::min_element(row.e.begin(), row.e.end()) / g_scale;
        rows.push_back(std::move(row));
    }
    return rows;
}

// ============================================================================================================ the looseness scan: the TRUE supremum of |d_e^3 a_i| by exact power series along the line

Ser::Ser(std::initializer_list<double> values) {
    std::size_t i = 0;
    for (const double v : values) {
        if (i < c.size()) c[i++] = v;
    }
}

Ser operator+(const Ser& a, const Ser& b) {
    Ser out;
    for (std::size_t i = 0; i < out.c.size(); ++i) out.c[i] = a.c[i] + b.c[i];
    return out;
}

Ser operator*(const Ser& a, const Ser& b) {
    Ser out;
    for (std::size_t i = 0; i < out.c.size(); ++i) {
        for (std::size_t j = 0; j < out.c.size() - i; ++j) out.c[i + j] += a.c[i] * b.c[j];
    }
    return out;
}

Ser power_series(const Ser& g, double alpha) {
    Ser out;
    out.c[0] = libm_pow(g.c[0], alpha);
    for (std::size_t k = 1; k < out.c.size(); ++k) {
        double s = 0.0;
        for (std::size_t j = 1; j <= k; ++j) s += (alpha * static_cast<double>(j) - static_cast<double>(k - j)) * g.c[j] * out.c[k - j];
        out.c[k] = s / (static_cast<double>(k) * g.c[0]);
    }
    return out;
}

Ser poly_on_line(const Poly& poly, const std::array<double, 3>& x0, const std::array<double, 3>& e) {
    const std::array<Ser, 3> coords = {Ser{x0[0], e[0]}, Ser{x0[1], e[1]}, Ser{x0[2], e[2]}};
    Ser total{0.0};
    for (const Poly::Term& term : poly.terms()) {
        Ser value{term.second.to_double()};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            for (int p = 0; p < term.first[axis]; ++p) value = value * coords[axis];
        }
        total = total + value;
    }
    return total;
}

std::vector<FieldTerm> field_terms(const std::vector<Coefficient>& coefs) {
    std::vector<FieldTerm> terms;
    Poly one;
    one.set({0, 0, 0}, Rational(1));
    // point mass: V = GM / r  <->  q = 1, s = 1
    terms.push_back(FieldTerm{kGM, 0, one, {Poly{}, Poly{}, Poly{}}, 1});
    for (const Coefficient& cf : coefs) {
        for (const char kind : {'C', 'S'}) {
            const double val = kind == 'C' ? cf.c : cf.s;
            if (val == 0.0 || (kind == 'S' && cf.m == 0)) continue;
            Poly q = solid_polynomial(cf.n, cf.m, kind);
            std::array<Poly, 3> dq = {p_der(q, 0), p_der(q, 1), p_der(q, 2)};
            const double norm = std::sqrt(norm_squared(cf.n, cf.m).to_double());
            terms.push_back(FieldTerm{kGM * libm_pow(kAE, cf.n) * norm * val, cf.n, std::move(q), std::move(dq), 2 * cf.n + 1});
        }
    }
    return terms;
}

double third_derivative_a(const std::vector<FieldTerm>& terms, const std::array<double, 3>& x0, const std::array<double, 3>& e, int i) {
    const std::size_t axis = static_cast<std::size_t>(i);
    const Ser r2{libm_pow(x0[0], 2.0) + libm_pow(x0[1], 2.0) + libm_pow(x0[2], 2.0), 2.0 * py_sum3(x0[0] * e[0], x0[1] * e[1], x0[2] * e[2]), 1.0};
    double total = 0.0;
    const Ser xi{x0[axis], e[axis]};
    for (const FieldTerm& term : terms) {
        const Ser qs = poly_on_line(term.q, x0, e);
        const Ser dqs = poly_on_line(term.dq[axis], x0, e);
        const double s = static_cast<double>(term.s);
        const Ser a = dqs * power_series(r2, -s / 2.0) + qs * xi * power_series(r2, -(s + 2.0) / 2.0) * Ser{-s};
        total += term.coef * 6.0 * a.c[3];
    }
    return total;
}

double scan(int /*nmax*/, double r, const std::vector<Coefficient>& coefs, int grid_lat, int grid_lon, int grid_dir) {
    const std::vector<FieldTerm> terms = field_terms(coefs);
    constexpr double pi = std::numbers::pi;
    double best = 0.0;
    for (int ia = 0; ia < grid_lat; ++ia) {
        const double lat = (-pi / 2) + pi * (ia + 0.5) / grid_lat;
        for (int ib = 0; ib < grid_lon; ++ib) {
            const double lon = 2 * pi * (ib + 0.5) / grid_lon;
            const std::array<double, 3> x = {r * std::cos(lat) * std::cos(lon), r * std::cos(lat) * std::sin(lon), r * std::sin(lat)};
            for (int ic = 0; ic < grid_dir; ++ic) {
                const double th = pi * (ic + 0.5) / grid_dir;
                for (int idr = 0; idr < 2 * grid_dir; ++idr) {
                    const double ph = pi * (idr + 0.5) / grid_dir;
                    const std::array<double, 3> e = {std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)};
                    for (const double step : {-kHMax, 0.0, kHMax}) {
                        const std::array<double, 3> x0 = {x[0] + step * e[0], x[1] + step * e[1], x[2] + step * e[2]};
                        for (int i = 0; i < 3; ++i) best = std::max(best, std::fabs(third_derivative_a(terms, x0, e, i)));
                    }
                }
            }
        }
    }
    return best;
}

// ============================================================================================================ the tool

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

Settings default_settings() {
    Settings settings;
    settings.root = default_root();
    return settings;
}

namespace {

const char kUsageText[] = "usage: forcemodel_fd_sizing [-h] [--scan] [--check] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "The finite-difference gate of the force plugins' Jacobians: its sizing, as the numbers it freezes (SPEC-forcemodel.md §6.2): the bound F of the third derivative of each force over the whole\n"
    "stencil, the registered noise bound nu, eps(h) = h^2 F / 6 + nu / h, and the power of the gate against its three wrong rows.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --scan       also run the looseness scan: the true supremum of |d_e^3 a_i| by exact power series, against F\n"
    "  --check      every frozen number reproduced (exit 1 if one is not)\n"
    "  --root ROOT  the tree whose data/cache holds the coefficient file (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced\n"
    "            2 an argument error, or the coefficient file is missing or unreadable\n";

}  // namespace

int run_on(Settings settings, const std::vector<std::string>& argv, Streams io) {
    try {
        bool scan_flag = false;
        bool check = false;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument --root: expected one argument");
                    value = argv[++i];
                }
                settings.root = value;
            } else if (opt == "--scan" && !has_value) {
                scan_flag = true;
            } else if (opt == "--check" && !has_value) {
                check = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        const std::filesystem::path egm = settings.root / kEgmRelative;
        bool failed = false;   // a frozen number not reproduced, or a control below the floor: what --check turns into exit status 1 (the Python kept the names of the failures and never printed them)

        // the frozen number of one line: ok, or MISMATCH with the frozen value beside it
        const auto expect = [&](const std::string& name, const std::string& got, const std::string& want, bool ok) {
            io.out << (ok ? "ok       " : "MISMATCH ") << name << ": " << got << (ok ? "" : "  (frozen " + want + ")") << '\n';
            if (!ok) failed = true;
        };

        // Lemma 1's constants, by hand: S3(1,3) = 3*1*12 + 60 = 96;  S3(1,4) = 3*1*20 + 120 = 180;  S3(2,5) = 6*5 + 6*30 + 210 = 420;
        // S3(0,3) = (3)_3 = 60;  S3(3,7) = 6 + 126 + 504 + 504 = 1140;  S3(1,5) = 90 + 210 = 300
        for (const Frozen& frozen : settings.s3) {
            const BigInt got = S3(frozen.d, frozen.q);
            const BigInt want(frozen.want);
            expect("S3(" + std::to_string(frozen.d) + "," + std::to_string(frozen.q) + ")", got.to_decimal(), want.to_decimal(), got == want);
        }
        // Phi_20 = sqrt(5) (2 * 300 + 10 * 1140), derived by hand from q = (2z^2 - x^2 - y^2)/2, ||q|| = 2, max_i ||d_i q|| = 2
        {
            const double got = phi(2, 0, 'C');
            const double want = std::sqrt(5.0) * settings.phi20_scale;
            expect("Phi_20", dk::py_float_repr(got), dk::py_float_repr(want), std::fabs(got - want) <= 1e-12 * std::fabs(want));
        }
        {
            const double got = phi_point_mass();
            expect("Phi_point_mass", dk::py_float_repr(got), "96.0", got == 96.0);
        }

        std::optional<std::vector<Coefficient>> coefs;
        try {
            coefs = coefficients(egm, 4);
        } catch (const std::runtime_error& exc) {
            io.err << kTool << ": error: " << exc.what() << '\n';
            return kArgument;
        }
        if (!coefs) {
            io.err << kTool << ": error: the coefficient file does not exist: " << egm.string() << '\n';
            return kArgument;
        }
        const std::vector<TableRow> rows = table(*coefs);
        io.out << "\nGravity(N = 4): rigorous F at r_min = r - 1000 m, the registered nu = 64 eps |a|, eps(h), and the best relative defect it can see\n";
        {
            std::string head = "  " + dk::pad_right("point", 16) + " " + dk::pad_left("r (m)", 12) + " " + dk::pad_left("F (m^-2 s^-2)", 16) + " " + dk::pad_left("nu (m s^-2)", 13) + " ";
            for (std::size_t k = 0; k < kHs.size(); ++k) head += (k == 0 ? "" : " ") + dk::pad_left("eps(" + dk::py_format_g(kHs[k]) + "m)", 11);
            io.out << head << "   min eps / |G|\n";
        }
        for (const TableRow& row : rows) {
            std::string line = "  " + dk::pad_right(row.name, 16) + " " + fixed(row.r, 12, 1) + " " + scientific(row.f, 16, 4) + " " + scientific(row.nu, 13, 3) + " ";
            for (std::size_t k = 0; k < row.e.size(); ++k) line += (k == 0 ? "" : " ") + scientific(row.e[k], 11, 3);
            io.out << line << "   " << scientific(row.rel, 0, 2) << '\n';
        }
        io.out << "\nGravity(N = 4): the three wrong rows' predicted power, max |wrong - right| / (eps at the best step); the registered requirement is >= 1e3\n";
        for (const ControlPower& power : control_power(*coefs)) {
            std::string line = "  " + dk::pad_right(power.name, 16) + " ERA " + fixed(degrees(power.th), 8, 3) + " deg  |sin ERA| = " + fixed(std::fabs(std::sin(power.th)), 0, 3) + "  ";
            for (std::size_t k = 0; k < power.row.size(); ++k) line += (k == 0 ? "" : "  ") + power.row[k].first + ": " + scientific(power.row[k].second, 0, 2);
            io.out << line << '\n';
            for (const auto& [label, v] : power.row) {
                if (v < settings.power_floor) failed = true;
            }
        }
        io.out << "\nThird body (mu / d_min^5 x 96), the Sun and the Moon at a LEO point:\n";
        const std::array<std::tuple<const char*, double, double>, 2> bodies = {{{"Sun", kGMS, 1.4959787e11}, {"Moon", 4.9028e12, 3.844e8}}};
        for (const auto& [body, mu, s] : bodies) {
            const double f = f_third_body(mu, s - 7.0e6 - kHMax);
            const double nu = nu_third_body(mu, s, s);
            const double best = best_eps(f, nu);
            const double g = 2 * mu / libm_pow(s, 3.0);
            io.out << "  " << dk::pad_right(body, 5) << " F = " << scientific(f, 0, 3) << "  nu = " << scientific(nu, 0, 3) << "  |G| ~ " << scientific(g, 0, 3) << "  best eps = " << scientific(best, 0, 3)
                   << "  relative " << scientific(best / g, 0, 2) << '\n';
        }
        io.out << "\nRelativity at the LEO 300 km point (v = sqrt(GM/r)):\n";
        {
            const double r = reference_points()[1].r;
            const double v = std::sqrt(kGM / r);
            const double fs = f_schwarzschild(r - kHMax, v);
            const double fl = f_lense_thirring(r - kHMax, v);
            const double a_s = 3.0 * libm_pow(kGmeRel, 2.0) / (libm_pow(kC, 2.0) * libm_pow(r, 3.0));
            const double a_l = 2.0 * kGmeRel * kJEarth * v / (libm_pow(kC, 2.0) * libm_pow(r, 3.0));
            io.out << "  Schwarzschild  F = " << scientific(fs, 0, 3) << "  nu = " << scientific(nu_relativity(a_s), 0, 3) << "   Lense-Thirring  F = " << scientific(fl, 0, 3) << "  nu = "
                   << scientific(nu_relativity(a_l), 0, 3) << "   de Sitter  F = 0 (d a / d r = 0 identically)\n";
        }

        if (scan_flag) {
            io.out << "\nLooseness, the TRUE supremum of |d_e^3 a_i| against F (point mass + degrees 2..4 of EGM2008; coarse grid, exact series):\n";
            for (const ReferencePoint& point : reference_points()) {
                const double truth = scan(4, point.r, *coefs);
                const double f = f_gravity(4, point.r - kHMax, *coefs);
                io.out << "  " << dk::pad_right(point.name, 16) << " true sup " << scientific(truth, 0, 3) << "   F " << scientific(f, 0, 3) << "   F / true sup = " << fixed(f / truth, 0, 1) << '\n';
            }
        }
        if (check) return failed ? kFailed : kOk;
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::forcemodel_fd_sizing

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::forcemodel_fd_sizing::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
