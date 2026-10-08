// measmod_fd_sizing.cpp — the finite-difference gate's sizing, SPEC-measmod.md §6.2, as the numbers it freezes.
//
// Plan L0 step 8, group C8, ported to C++ (the user's directive of 2026-10-06) from tools/measmod_fd_sizing.py (deleted by the same commit: `git show bc7cd6d:rewrite/tools/measmod_fd_sizing.py`), on tools/measmod_reference.cpp's registry section, closed-form light time and cosine (as the Python
// imported measmod_reference), the devkit's Decimal and pymath.
//
// WRITTEN 2026-10-06, BEFORE ANY RUN OF THE GATE (plan §4 rule 7).  A central difference of the measurement model's output carries three errors (the argument of SPEC-stm `STM-P-1`, applied to measmod):
//
//     truncation   T(h) = h^2 F / 6      F the third derivative of the output along the displacement direction
//     noise        N(h) = nu / h         nu the bound of ONE evaluation's round-off (3 ulp of the largest operand)
//     light-time tolerance               absent: the iteration runs to the double-precision fixed point
//
//     bound        eps(h) = T(h) + N(h);  band prediction B_pred = max over the five sizes of eps(h)
//
// Range: F = 3 mu (1 - mu^2) / rho^2, at most 1.1547 / rho^2 (mu = cos of the angle between the displacement and the line of sight).  Angle: F_a = 2 / (rho^3 cos^3 phi_max).  The right-ascension coefficient is
// the analytic 2 Im[(w1/w0)^3] with w0 = n_x + i n_y, so it is bounded by 2/cos^3(dec); `--scan` confirms it, and the declination coefficient, by an exact truncated power series over displacement directions and
// lines of sight (no finite differences, no mpmath: the series coefficients are O(1) and carry no cancellation).
//
// AMENDED 2026-10-06, AFTER THE GATE'S FIRST RUN MISSED ITS PER-SIZE BOUND (SPEC-measmod.md §6.2, "Amendment"; plan/subplan_L6/L6-4.md, the manager's ruling).  The text above is the sizing as it was frozen and run
// once; it is kept as it was.  The cause of the miss was in the sizing, and the amendment changes the sizing only, by two first-principles terms it left out (the section "THE AMENDED RANGE SIZING" below):
//   (i)  nu gains the Earth-rotation angle's double-precision floor of the real orientation chain (3 x 2^-45 rad x the station's axis distance, carried through the range as it enters for each epoch event) -- in the
//        real-chain configuration only;
//   (ii) F becomes a bound over the stencil [x-h, x+h] for the whole modelled range: geometry (with the light-time coupling), troposphere through the mapping function's derivatives, and the Shapiro term.
//
//   measmod_fd_sizing [--scan] [--check] [--root DIR]
//     (no flag)  print the frozen sizing and the amended one;   --check  every frozen number reproduced (fast);
//     --scan     the exact angle-series scan AND the amended range scans (the stencil bounds; ten seconds in Python)
//   exit 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced   2 an argument error, or an input file that is missing, cannot be read or is not the shape expected
//   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's `--scan --check` output was recorded by L6's report (round9/fd_sizing_angle_scan_check.out, 9,872 bytes, 106 lines, sha256 c6389b08...) before the Python's last section, the
// diurnal-aberration check, was added; an earlier run (fd_sizing_amended_scan_check.out) is its first 68 lines and its last.  The port printed lines 1 to 105 BYTE FOR BYTE and the last line, against an EMPTY
// substitution list registered before the comparison (the output names no tool).  The diurnal section has NO record; what stands for it is registered too: its table is the closed forms of the specification computed
// apart by bc (and its eps column IS the frozen FROZEN_DIURNAL of the Python), the rest is compared with the frozen literals the tool's own --check holds and with SPEC-measmod §6.2 (vi).  (C8_proof_registration.txt,
// C-2, holds the registration and the result.)  The modes without a record (no flag, --check alone, --scan alone) stand on tests.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * Every floating-point operation is the Python's, in the Python's order, and none is fused (-ffp-contract=off): float ** int is the C library's pow() (devkit pymath: never folded to a multiplication),
//     math.radians(x) is x * (pi / 180) and math.degrees(x) is x * (180 / pi) as CPython computes them, sum() of floats is CPython 3.12's compensated (Neumaier) sum, math.ulp is its own, math.hypot is the
//     correctly rounded norm (CPython's is within an ulp of it and equal to it but for a few cases in a million), and the sines and cosines are the C library's, called one by one.
//   * The Decimal arithmetic is the devkit's, at 60 digits as the Python set it when it imported measmod_reference; the real power Decimal(10) ** x of eraRefco is exp(x ln 10) in 83 digits (libmpdec's
//     rule as remembered; the last digit of a 60-digit value does not reach a double).
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this tool was built from.
//   * The figures SPEC-measmod.md 6.2 freezes -- the Python's literals CASES, FROZEN_F_TROP, AMENDED_FROZEN, FROZEN_ANGLE and FROZEN_DIURNAL, the IERS FCUL_A and ERFA eraRefco published values and the 1.59e-7 m predicted
//     before it was measured -- are fields of `Settings::frozen` (as in tools/forcemodel_fd_sizing), with the Python's values as their defaults, so that a test damages one and sees its comparison fire at its tolerance.
//     The output with the defaults is the same bytes as before the fields existed (`--scan --check`: sha256 565c4923..., checked the day the fields were made).
//   * A missing or unreadable or ill-formed input file: the Python caught FileNotFoundError only, printing "missing input: ..." (exit 2) and dying with a traceback on any other; this REFUSES them all the same
//     way, exit 2, naming the file.
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--root=DIR` is taken).  `-h` prints this tool's own text.

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/fs.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_fd_sizing.hpp"
#include "measmod_reference.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace dk = odl::devkit;
namespace mr = odl::tools::measmod_reference;

namespace odl::tools::measmod_fd_sizing {

using dk::Decimal;
using dk::DecimalContext;
using dk::LocalContext;
using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "measmod_fd_sizing";
constexpr double kPi = std::numbers::pi;

// the Python's max(a, b): b only when it is greater than a (so a NaN never wins)
double py_max(double a, double b) { return b > a ? b : a; }

// the Python's f-string formats of a float: {v:.Pe}, {v:.Pf}, {v:.Pg}, and {v:W.Pf}
std::string fe(double v, int precision) { return dk::py_format_e(v, precision); }
std::string ff(double v, int precision) { return dk::py_format_f(v, precision); }
std::string fg(double v, int precision) { return dk::py_format_g(v, precision); }
std::string fwf(double v, std::size_t width, int precision) { return dk::pad_left(dk::py_format_f(v, precision), width); }

// '%.5e' % x for each of the five
std::string list_of_e(const std::array<double, 5>& values, int precision) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) out += (i > 0 ? ", '" : "'") + fe(values[i], precision) + "'";
    return out + "]";
}

double sum3(double a, double b, double c) {
    const std::array<double, 3> items = {a, b, c};
    return dk::py_sum(items);
}

double norm3(const Vec3& v) { return dk::py_sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }   // the Python's _norm

}  // namespace

// =====================================================================================================================================================================================================
// THE FROZEN SIZING
// =====================================================================================================================================================================================================

// (the geometries and the figures SPEC-measmod.md §6.2 freezes -- name, rho, |r_target|, kind, B_pred, h* -- are Frozen::sizing, in the header)

Sizing sizing(double rho, double rt, bool range, double phi_max_deg) {
    Sizing out{};
    double f = 0.0;
    if (range) {
        out.nu = 3 * dk::py_ulp(rt);
        f = 1.1547005383792515 / dk::py_pow(rho, 2);
    } else {
        out.nu = 3 * (dk::py_ulp(2 * kPi) + dk::py_ulp(rt) / rho);
        f = 2 / (dk::py_pow(rho, 3) * dk::py_pow(dk::py_cos(dk::py_radians(phi_max_deg)), 3));
    }
    out.f = f;
    for (std::size_t i = 0; i < kSteps.size(); ++i) {
        const double h = kSteps[i];
        out.truncation[i] = h * h * f / 6;
        out.noise[i] = out.nu / h;
        out.bound[i] = out.truncation[i] + out.noise[i];
    }
    const double hstar = dk::py_pow(3 * out.nu / f, 1.0 / 3.0);
    out.best_step = hstar;
    out.best = dk::py_pow(hstar, 2) * f / 6 + out.nu / hstar;
    return out;
}

// =====================================================================================================================================================================================================
// THE EXACT SERIES SCAN (truncated power series in h)
// =====================================================================================================================================================================================================

Series operator+(const Series& a, const Series& b) {
    Series r;
    for (std::size_t i = 0; i < kOrder; ++i) r.c[i] = a.c[i] + b.c[i];
    return r;
}

Series operator-(const Series& a, const Series& b) {
    Series r;
    for (std::size_t i = 0; i < kOrder; ++i) r.c[i] = a.c[i] - b.c[i];
    return r;
}

Series operator*(const Series& a, const Series& b) {
    Series r;
    for (std::size_t i = 0; i < kOrder; ++i) {
        for (std::size_t j = 0; j < kOrder - i; ++j) r.c[i + j] += a.c[i] * b.c[j];
    }
    return r;
}

Series Series::inv() const {
    Series r;
    r.c[0] = 1.0 / c[0];
    for (std::size_t n = 1; n < kOrder; ++n) {
        std::vector<double> terms;
        for (std::size_t k = 1; k <= n; ++k) terms.push_back(c[k] * r.c[n - k]);
        r.c[n] = -dk::py_sum(terms) / c[0];
    }
    return r;
}

Series operator/(const Series& a, const Series& b) { return a * b.inv(); }

Series Series::sqrt() const {
    Series r;
    r.c[0] = dk::py_sqrt(c[0]);
    for (std::size_t n = 1; n < kOrder; ++n) {
        std::vector<double> terms;
        for (std::size_t k = 1; k < n; ++k) terms.push_back(r.c[k] * r.c[n - k]);
        r.c[n] = (c[n] - dk::py_sum(terms)) / (2 * r.c[0]);   // (an empty sum is the int 0: c[n] - 0)
    }
    return r;
}

Series Series::deriv() const {
    Series r;
    for (std::size_t k = 1; k < kOrder; ++k) r.c[k - 1] = static_cast<double>(k) * c[k];
    return r;
}

Series Series::integ(double c0) const {
    Series r;
    r.c[0] = c0;
    for (std::size_t k = 0; k + 1 < kOrder; ++k) r.c[k + 1] = c[k] / static_cast<double>(k + 1);
    return r;
}

Series operator+(const Series& a, double b) { return a + Series(b); }
Series operator+(double a, const Series& b) { return b + Series(a); }
Series operator-(const Series& a, double b) { return a - Series(b); }
Series operator-(double a, const Series& b) { return Series(a) - b; }
Series operator*(const Series& a, double b) { return a * Series(b); }
Series operator*(double a, const Series& b) { return b * Series(a); }
Series operator/(const Series& a, double b) { return a * Series(b).inv(); }
Series operator/(double a, const Series& b) { return Series(a) / b; }

Series atan_series(const Series& q) { return (q.deriv() / (Series(1.0) + q * q)).integ(dk::py_atan(q.c[0])); }

std::array<double, 2> third_derivatives(const std::array<double, 3>& g, const std::array<double, 3>& e) {
    const Series x(g[0], e[0]);
    const Series y(g[1], e[1]);
    const Series z(g[2], e[2]);
    const Series ra = atan_series(y / x);
    const Series dec = atan_series(z / (x * x + y * y).sqrt());
    return {6 * ra.c[3], 6 * dec.c[3]};
}

std::array<double, 2> scan(double phi_deg) {
    (void)phi_deg;   // (the Python's parameter is unused as well: the declinations are the fixed list)
    std::array<double, 2> best = {0.0, 0.0};
    for (const int dec_deg : {-30, -20, -10, 0, 10, 20, 30}) {
        const double dec = dk::py_radians(static_cast<double>(dec_deg));
        for (int k = 0; k < 12; ++k) {
            const double ra = 2 * kPi * k / 12 + 0.3;
            const Vec3 g = {dk::py_cos(dec) * dk::py_cos(ra), dk::py_cos(dec) * dk::py_sin(ra), dk::py_sin(dec)};
            if (g[0] <= 0) continue;
            for (int it = 0; it < 90; ++it) {
                const double th = kPi * (it + 0.5) / 90;
                for (int jt = 0; jt < 180; ++jt) {
                    const double ph = 2 * kPi * jt / 180;
                    const Vec3 e = {dk::py_sin(th) * dk::py_cos(ph), dk::py_sin(th) * dk::py_sin(ph), dk::py_cos(th)};
                    const std::array<double, 2> d3 = third_derivatives(g, e);
                    best[0] = py_max(best[0], std::fabs(d3[0]));
                    best[1] = py_max(best[1], std::fabs(d3[1]));
                }
            }
        }
    }
    return best;
}

// =====================================================================================================================================================================================================
// THE AMENDED RANGE SIZING — 2026-10-06, MADE AFTER THE FIRST RUN OF THE GATE MISSED CRITERION (b) (post hoc, and labelled so)
// =====================================================================================================================================================================================================
// The first run missed (b) in 7 of 8 cases because the sizing above left out two terms, each derivable from first principles and neither read off the measurement:
//   (i)  the real orientation chain's Earth-rotation angle is formed unreduced (2 pi (f + 0.779... + 0.0027378 t), about 171 rad for 2019-2039), so its last bit is 2^-45 rad: at the station's distance from the
//        Earth's axis a position noise of 2^-45 rad x R_perp = 1.59e-7 m per evaluation.  The frozen sizing's own convention is three ulp: nu_chain = 3 x 2^-45 x R_perp, carried through the range as it enters --
//        in event 2 the up leg's station epoch is the tag, the same in every evaluation, so only the down leg enters (weight 1/2 of the range); in event 1 both legs move (weights 1/2 + 1/2) -- times |g . east|,
//        the cosine between the line of sight and the direction of the error.
//   (ii) F is a bound over the whole stencil [x-h, x+h], for the whole modelled range: a central difference's error is h^2/6 f'''(xi) at some xi inside it, so (b) is a bound only if F bounds |f'''| everywhere in it:
//            F(h) = F_geo(h) + 1.01 F_trop(h)
//            F_geo(h) = 1.1547005 (1 + kappa)^3 / (rho (1 - kappa) - h)^2,  kappa = (v_target + 2 v_station,max) / c
//                       the analytic maximum of 3 mu (1 - mu^2) over the smallest distance of the stencil, with the light-time coupling's v/c (VERIFIED below against the closed-form two-way light time in 60 digits
//                       over the cone of directions where the maximum lives)
//            F_trop(h)  the supremum over every unit displacement direction and every point of the stencil of |d^3/dxi^3| of the leg's zenith delay x FCULa mapping function plus Shapiro term, by third
//                       differences of the model's own formulas (an independent implementation: TN36 equations 9.3-9.10 and 11.17), the 1.01 being the allowance for the grid's discretisation.
// The geometries, the azimuth rule, the sizes, the velocity directions and the criteria (a), (b), (c) are the frozen ones.

namespace {

constexpr double kUEra = 1.0 / 35184372088832.0;   // 2.0 ** -45: one ulp of an Earth-rotation angle between 128 and 256 rad (2019-2039); a power of two, so the division is exact
constexpr double kRStation = 6.373e6;               // m: the station's geocentric distance, used only for the size of the Shapiro term
// the first normal point's surface weather (record 20 of lageos1_202601.np2) and the transmit wavelength of its configuration
constexpr double kPHpa = 976.70;
constexpr double kTKelvin = 310.30;
constexpr double kRhPercent = 12.0;
constexpr double kLambdaUm = 0.532;

double k_shapiro() { return 2.0 * 3.986004415e14 / dk::py_pow(kLight, 2); }   // m: 2 GM/c^2 (TN36 eq. 11.17, GM = 3.986004415e14 m^3/s^2)

struct AmendedCase {
    const char* name;
    double rho, el_deg, az_deg, speed, nu;
};

// name, rho, elevation (deg), the azimuth the frozen geometry rule selects (deg), the target's speed (m/s), the frozen noise bound nu (m)
const std::array<AmendedCase, 2>& amended_cases() {
    static const std::array<AmendedCase, 2> cases = {{{"LAGEOS-like", 6.6e6, 52.0, 105.0, 5700.0, 3 * dk::py_ulp(1.227e7)}, {"LEO-like", 1.5e6, 15.0, 0.0, 7500.0, 3 * dk::py_ulp(6.92e6)}}};
    return cases;
}

constexpr std::array<double, 6> kXis = {0.0, 10.0, 30.0, 100.0, 300.0, 1000.0};   // the stencil's points (and their mirror images): the sizes of the gate

// (F_trop(h), m^-2, the scan's result written down -- the section's own `--scan` recomputes it and `--check` compares -- and THE AMENDED NUMBERS, frozen 2026-10-06 BEFORE THE GATE'S SECOND RUN (the section's own
// output, 4 significant digits): the chain's bound of one evaluation of the range output for each epoch event, eps(h) for the rigid configuration (nu as first frozen) and for the real chain (nu + nu_chain) in
// each event, and B_pred = max eps -- are Frozen::amended, in the header, by case in the order of amended_cases().)

double ten_pow(double x) { return dk::py_pow(10.0, x); }

}  // namespace

double v_station_max() { return 7.2921150e-5 * 6378137.0; }   // m/s: the equatorial surface speed, an upper bound of any station's

double fcula(double s, double cphi, double height, double t_c) {
    // TN36 Table 9.1 / eq. (9.9), as printed
    const double a1 = 12100.8e-7 + 1729.5e-9 * t_c + 319.1e-7 * cphi - 1847.8e-11 * height;
    const double a2 = 30496.5e-7 + 234.6e-8 * t_c - 103.5e-6 * cphi - 185.6e-10 * height;
    const double a3 = 6877.7e-5 + 197.2e-7 * t_c - 345.8e-5 * cphi + 106.0e-9 * height;
    return (1 + a1 / (1 + a2 / (1 + a3))) / (s + a1 / (s + a2 / (s + a3)));
}

namespace {

// the same with Decimal arithmetic (the Python's `num=Decimal`)
Decimal fcula_decimal(const Decimal& s, const Decimal& cphi, const Decimal& height, const Decimal& t_c) {
    const auto n = [](const char* text) { return Decimal::from_string(text); };
    const Decimal a1 = n("12100.8e-7") + n("1729.5e-9") * t_c + n("319.1e-7") * cphi - n("1847.8e-11") * height;
    const Decimal a2 = n("30496.5e-7") + n("234.6e-8") * t_c - n("103.5e-6") * cphi - n("185.6e-10") * height;
    const Decimal a3 = n("6877.7e-5") + n("197.2e-7") * t_c - n("345.8e-5") * cphi + n("106.0e-9") * height;
    return (Decimal(1) + a1 / (Decimal(1) + a2 / (Decimal(1) + a3))) / (s + a1 / (s + a2 / (s + a3)));
}

}  // namespace

double zenith_total(double lat_rad, double height, double ps, double es, double lam) {
    // TN36 eqs (9.3)-(9.7) in double precision (the sizing needs it to a part in 10^3 only)
    const double fs = 1 - 0.00266 * dk::py_cos(2 * lat_rad) - 0.00000028 * height;
    const double sig2 = dk::py_pow(1 / lam, 2);
    const double fh = 1e-2 * (19990.975 * (238.0185 + sig2) / dk::py_pow(238.0185 - sig2, 2) + 579.55174 * (57.362 + sig2) / dk::py_pow(57.362 - sig2, 2)) * (1 + 0.534e-6 * (375 - 450));
    const double fnh = 0.003101 * (295.235 + 3 * 2.6422 * sig2 + 5 * -0.032380 * dk::py_pow(sig2, 2) + 7 * 0.004028 * dk::py_pow(sig2, 3));
    return 0.002416579 * fh / fs * ps + 1e-4 * (5.316 * fnh - 3.759 * fh) * es / fs;
}

// The station's axis distance, geodetic latitude and ellipsoidal height of the first normal point, from the independent registry reference.
StationInputs station_inputs(const std::filesystem::path& root) {
    const mr::Values r = mr::registry_section(root);
    const auto get = [&](const std::string& key) {
        for (const auto& [name, value] : r) {
            if (name == key) return value;
        }
        throw mr::MissingInput("the registry section holds no " + key);
    };
    return StationInputs{dk::py_hypot(get("yarl_srp_x_m"), get("yarl_srp_y_m")), get("yarl_srp_lat_rad"), get("yarl_srp_h_m")};
}

Atmosphere atmosphere_inputs(const std::filesystem::path& root) {
    const StationInputs station = station_inputs(root);
    const double t_c = kTKelvin - 273.15;
    const double e = (kRhPercent / 100.0) * 6.11 * ten_pow(7.5 * t_c / (237.3 + t_c));
    return Atmosphere{station.r_perp, station.lat, station.height, t_c, zenith_total(station.lat, station.height, kPHpa, e, kLambdaUm)};
}

// The Decimal FCULa at the IERS FCUL_A prolog's printed case (lat 30.67166667 deg, H 2075 m, T 300.15 K, elevation 15 deg): the printed 3.800243667312344087.
double iers_fcula_check(const char* printed) {
    const Decimal lat = Decimal::from_string("30.67166667") * mr::pi() / Decimal(180);
    const Decimal sin15 = (Decimal(6).sqrt() - Decimal(2).sqrt()) / Decimal(4);
    return (fcula_decimal(sin15, mr::d_cos(lat), Decimal(2075), Decimal::from_string("300.15") - Decimal::from_string("273.15")) - Decimal::from_string(printed)).to_double();
}

double f_geo(double h, double rho, double speed) {
    const double kappa = (speed + 2 * v_station_max()) / kLight;
    return 1.1547005383792515 * dk::py_pow(1 + kappa, 3) / dk::py_pow(rho * (1 - kappa) - h, 2);
}

std::pair<double, double> nu_chain(int event, double r_perp, double el_deg, double az_deg) {
    // the chain's bound of one evaluation of the range output: three ulp of the angle at the axis distance, times the cosine between the line of sight and the error's direction (east), times 1/2 for the one leg
    // that moves in event 2 and 1 for the two legs that move in event 1
    const double g_east = std::fabs(dk::py_cos(dk::py_radians(el_deg)) * dk::py_cos(dk::py_radians(az_deg)));
    return {3 * kUEra * r_perp * g_east * (event == 2 ? 0.5 : 1.0), g_east};
}

double eps_amended(std::size_t step, const std::array<double, 5>& f_trop, double rho, double speed, double nu) {
    const double h = kSteps[step];
    return h * h * (f_geo(h, rho, speed) + 1.01 * f_trop[step]) / 6 + nu / h;
}

// ---- the stencil bound of the troposphere and Shapiro terms: third differences of the model's own formulas, over every direction --------------------------------------------------------------------------

TropScan trop_shapiro_scan(double rho, double el_deg, const Atmosphere& atm, double step_theta, double step_phi, double delta) {
    // sup over unit displacement directions and the stencil's points of |d3/dxi3| [ZTD m(sin e) + Shapiro] along the displacement, for one leg whose line of sight makes `el_deg` with the horizon at slant range `rho`
    const double cphi = dk::py_cos(atm.lat);
    const double el = dk::py_radians(el_deg);
    const Vec3 p0 = {rho * dk::py_cos(el), 0.0, rho * dk::py_sin(el)};   // target relative to the station, local frame (up = z)
    const double shapiro_constant = k_shapiro();

    const auto total = [&](const Vec3& p, bool with_atm, bool with_shapiro) {
        const double r = norm3(p);
        double out = 0.0;
        if (with_atm) out += atm.z * fcula(p[2] / r, cphi, atm.height, atm.t_c);
        if (with_shapiro) {
            const double r2 = norm3({p[0], p[1], p[2] + kRStation});
            out += shapiro_constant * dk::py_log1p(2 * r / (kRStation + r2 - r));
        }
        return out;
    };
    const auto third = [&](const Vec3& e, double xi, bool with_atm, bool with_shapiro) {
        std::array<double, 4> f{};
        std::size_t index = 0;
        for (const int k : {-2, -1, 1, 2}) {
            const double t = xi + k * delta;
            f[index++] = total({p0[0] + t * e[0], p0[1] + t * e[1], p0[2] + t * e[2]}, with_atm, with_shapiro);
        }
        return (f[3] - 2 * f[2] + 2 * f[1] - f[0]) / (2 * dk::py_pow(delta, 3));
    };

    TropScan out{};   // sup of every step, 0.0
    const int n_th = static_cast<int>(180 / step_theta) + 1;
    const int n_ph = static_cast<int>(360 / step_phi);
    for (int it = 0; it < n_th; ++it) {
        const double th = dk::py_radians(step_theta * it);
        for (int jp = 0; jp < n_ph; ++jp) {
            const double ph = dk::py_radians(step_phi * jp);
            const Vec3 e = {dk::py_sin(th) * dk::py_cos(ph), dk::py_sin(th) * dk::py_sin(ph), dk::py_cos(th)};
            for (const double sign : {1.0, -1.0}) {
                for (const double xi_abs : kXis) {
                    const double xi = sign * xi_abs;
                    const double v = std::fabs(third(e, xi, true, true));
                    for (std::size_t s = 0; s < kSteps.size(); ++s) {
                        if (xi_abs <= kSteps[s]) out.sup[s] = py_max(out.sup[s], v);
                    }
                    if (xi_abs == 1000.0 || xi_abs == 0.0) {
                        out.sup_shapiro = py_max(out.sup_shapiro, std::fabs(third(e, xi, false, true)));
                        out.sup_atmosphere = py_max(out.sup_atmosphere, std::fabs(third(e, xi, true, false)));
                    }
                }
            }
        }
    }
    return out;
}

// ---- the closed-form two-way light time with both bodies in uniform motion (60 digits): the geometry and the coupling, over the cone where the maximum lives -------------------------------------------------

std::array<double, 5> coupling_scan(double rho, double el_deg, double az_deg, double speed, double radial, int event, double delta) {
    // sup over the cone of displacement directions (mu = e.g in 0.45 ... 0.70, 24 azimuths) and the stencil's points of the third derivative of the closed-form two-way range, for each h
    const double el = dk::py_radians(el_deg);
    const double az = dk::py_radians(az_deg);
    const Vec3 g = {dk::py_cos(el) * dk::py_cos(az), dk::py_cos(el) * dk::py_sin(az), dk::py_sin(el)};
    const Vec3 up = {0.0, 0.0, 1.0};
    Vec3 across = {g[1] * up[2] - g[2] * up[1], g[2] * up[0] - g[0] * up[2], g[0] * up[1] - g[1] * up[0]};
    const double n = norm3(across);
    for (double& x : across) x = x / n;
    Vec3 vt{};
    for (std::size_t i = 0; i < 3; ++i) vt[i] = speed * (dk::py_sqrt(1.0 - radial * radial) * across[i] + radial * g[i]);
    const Vec3 s0 = {0.0, 0.0, 0.0};
    const Vec3 vs = {v_station_max(), 0.0, 0.0};
    mr::Vec r0;
    for (std::size_t i = 0; i < 3; ++i) r0[i] = Decimal::from_double(rho * g[i]);
    Vec3 a_ax{};
    for (std::size_t i = 0; i < 3; ++i) a_ax[i] = up[i] - g[2] * g[i];   // up minus its component along g
    const double na = norm3(a_ax);
    for (double& x : a_ax) x = x / na;
    const Vec3 b_ax = {g[1] * a_ax[2] - g[2] * a_ax[1], g[2] * a_ax[0] - g[0] * a_ax[2], g[0] * a_ax[1] - g[1] * a_ax[0]};
    const Decimal c(299792458);
    const Decimal dl = Decimal::from_double(delta);
    // the doubles of the station and the target velocity, as Decimals once (the Python converted them at every call, to the same values)
    mr::Vec s0d;
    mr::Vec vsd;
    mr::Vec vtd;
    for (std::size_t i = 0; i < 3; ++i) {
        s0d[i] = Decimal::from_double(s0[i]);
        vsd[i] = Decimal::from_double(vs[i]);
        vtd[i] = Decimal::from_double(vt[i]);
    }
    const auto y = [&](const mr::Vec& r) {
        const auto [tu, td] = mr::lt_closed_form(event, s0d, vsd, r, vtd);
        return c * (tu + td) / Decimal(2);
    };

    std::array<double, 5> sup{};
    for (const double mu : {0.45, 0.50, 0.55, 1 / dk::py_sqrt(3), 0.60, 0.65, 0.70}) {
        const double sn = dk::py_sqrt(1 - mu * mu);
        for (int k = 0; k < 24; ++k) {
            const double ph = dk::py_radians(15.0 * k);
            Vec3 e{};
            for (std::size_t i = 0; i < 3; ++i) e[i] = mu * g[i] + sn * (dk::py_cos(ph) * a_ax[i] + dk::py_sin(ph) * b_ax[i]);
            mr::Vec ed;
            for (std::size_t i = 0; i < 3; ++i) ed[i] = Decimal::from_double(e[i]);
            for (const double sign : {1.0, -1.0}) {
                for (const double xi_abs : kXis) {
                    const Decimal xi = Decimal::from_double(sign * xi_abs);
                    std::array<Decimal, 4> ys;
                    std::size_t index = 0;
                    for (const int kk : {-2, -1, 1, 2}) {
                        const Decimal t = xi + Decimal(kk) * dl;
                        ys[index++] = y({r0[0] + t * ed[0], r0[1] + t * ed[1], r0[2] + t * ed[2]});
                    }
                    const double d3 = std::fabs(((ys[3] - Decimal(2) * ys[2] + Decimal(2) * ys[1] - ys[0]) / (Decimal(2) * dl.pow(Decimal(3)))).to_double());
                    for (std::size_t s = 0; s < kSteps.size(); ++s) {
                        if (xi_abs <= kSteps[s]) sup[s] = py_max(sup[s], d3);
                    }
                }
            }
        }
    }
    return sup;
}

namespace {

// The amended section: print it; with `scan` recompute the stencil bounds (slow) and verify the closed form bounds the coupled geometry; with `check` fail on a miss.
int amended(Streams& io, const std::filesystem::path& root, bool scan_flag, bool check, const Frozen& frozen) {
    int bad = 0;
    const Atmosphere atm = atmosphere_inputs(root);
    const double floor_one = kUEra * atm.r_perp;
    io.out << "\n=== THE AMENDED RANGE SIZING (2026-10-06, made after the first run's miss) ===\n";
    io.out << "the chain's floor: 2^-45 rad = " << fe(kUEra, 4) << " rad, axis distance " << ff(atm.r_perp, 1) << " m, one ulp = " << fe(floor_one, 4) << " m, three ulp = " << fe(3 * floor_one, 4) << " m;  ZTD = "
           << ff(atm.z, 6) << " m, lat " << ff(dk::py_degrees(atm.lat), 4) << " deg, H " << ff(atm.height, 1) << " m, t = " << ff(atm.t_c, 2) << " C\n";
    if (check) {
        const double d = iers_fcula_check(frozen.iers_fcula);
        bool ok = std::fabs(d) < 1e-12;
        io.out << "the Decimal FCULa at the IERS FCUL_A printed case differs from the printed 3.800243667312344087 by " << fe(d, 2) << ": " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = std::fabs(floor_one / frozen.chain_ulp - 1) < 0.01;
        io.out << "one ulp of the chain's angle at the station = " << fe(floor_one, 3) << " m against the 1.59e-7 m predicted before it was measured: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
    }
    for (std::size_t case_index = 0; case_index < amended_cases().size(); ++case_index) {
        const AmendedCase& cs = amended_cases()[case_index];
        const FrozenAmended& fa = frozen.amended[case_index];
        const std::string name = cs.name;
        const double rho = cs.rho;
        const double speed = cs.speed;
        const double nu = cs.nu;
        std::array<double, 5> f_trop{};
        double f_shap = std::numeric_limits<double>::quiet_NaN();
        double f_atm = f_shap;
        if (scan_flag) {
            const TropScan found = trop_shapiro_scan(rho, cs.el_deg, atm);
            f_trop = found.sup;
            f_shap = found.sup_shapiro;
            f_atm = found.sup_atmosphere;
        } else {
            f_trop = fa.f_trop;
        }
        io.out << "--- " << name << ": rho = " << fg(rho, 3) << " m, elevation " << ff(cs.el_deg, 0) << " deg, azimuth " << ff(cs.az_deg, 0) << " deg, speed " << ff(speed, 0) << " m/s, nu(frozen) = " << fg(nu, 4)
               << " m\n";
        if (scan_flag) {
            io.out << "    Shapiro alone: sup |d3| = " << fe(f_shap, 3) << " m^-2 = " << fe(f_shap / f_geo(0.0, rho, speed), 2) << " of F_geo;  atmosphere alone: " << fe(f_atm, 3) << " = "
                   << fe(f_atm / f_geo(0.0, rho, speed), 2) << " of F_geo\n";
        }
        io.out << "    h        F_geo        F_trop(sup)   F(h)=F_geo+1.01 F_trop   eps_rigid(h)      eps_real event 2      eps_real event 1\n";
        const auto [nu2, ge] = nu_chain(2, atm.r_perp, cs.el_deg, cs.az_deg);
        const double nu1 = nu_chain(1, atm.r_perp, cs.el_deg, cs.az_deg).first;
        for (std::size_t s = 0; s < kSteps.size(); ++s) {
            const double h = kSteps[s];
            const double fg_h = f_geo(h, rho, speed);
            const double ft = f_trop[s];
            io.out << "    " << fwf(h, 6, 0) << "   " << fe(fg_h, 5) << "  " << fe(ft, 4) << "   " << fe(fg_h + 1.01 * ft, 5) << "   " << fe(eps_amended(s, f_trop, rho, speed, nu), 4) << "   "
                   << fe(eps_amended(s, f_trop, rho, speed, nu + nu2), 4) << "        " << fe(eps_amended(s, f_trop, rho, speed, nu + nu1), 4) << '\n';
        }
        const std::array<std::pair<const char*, double>, 3> labels = {{{"rigid", 0.0}, {"real, event 2", nu2}, {"real, event 1", nu1}}};
        for (const auto& [label, nu_extra] : labels) {
            double b = eps_amended(0, f_trop, rho, speed, nu + nu_extra);
            for (std::size_t s = 1; s < kSteps.size(); ++s) b = py_max(b, eps_amended(s, f_trop, rho, speed, nu + nu_extra));
            io.out << "    " << dk::pad_right(label, 14) << ": nu = " << fe(nu + nu_extra, 4) << " m   B_pred = " << fe(b, 4) << "   window [" << fe(b / 10, 3) << ", " << fe(b * 10, 3) << "]\n";
        }
        io.out << "    |g . east| = " << ff(ge, 4) << ": nu_chain event 2 = " << fe(nu2, 4) << " m, event 1 = " << fe(nu1, 4) << " m\n";
        if (scan_flag) {
            for (const auto& [radial, rname] : {std::pair<double, const char*>{0.0, "across"}, std::pair<double, const char*>{1.0, "along"}}) {
                for (const int event : {2, 1}) {
                    const std::array<double, 5> sup = coupling_scan(rho, cs.el_deg, cs.az_deg, speed, radial, event);
                    double worst = sup[0] / f_geo(kSteps[0], rho, speed);
                    for (std::size_t s = 1; s < kSteps.size(); ++s) worst = py_max(worst, sup[s] / f_geo(kSteps[s], rho, speed));
                    const bool ok = worst <= 1.0;
                    io.out << "    closed-form two-way light time, " << dk::pad_right(rname, 6) << " event " << event << ": max over the cone of |y'''| / F_geo(h) = " << ff(worst, 6)
                           << "  (the formula bounds the coupled geometry: " << (ok ? "yes" : "NO") << ")\n";
                    bad += ok ? 0 : 1;
                }
            }
        }
        if (check) {
            const std::array<double, 5>& lit = fa.f_trop;
            bool ok = true;
            for (std::size_t s = 0; s < kSteps.size(); ++s) ok = ok && std::fabs(f_trop[s] / lit[s] - 1) < 0.02;
            io.out << (scan_flag ? "    frozen F_trop reproduced:" : "    frozen F_trop in use:") << ' ' << (ok ? "ok" : "NOT REPRODUCED") << '\n';
            bad += ok ? 0 : 1;
            std::array<double, 5> rigid{};
            std::array<double, 5> real2{};
            std::array<double, 5> real1{};
            for (std::size_t s = 0; s < kSteps.size(); ++s) {
                rigid[s] = eps_amended(s, lit, rho, speed, nu);
                real2[s] = eps_amended(s, lit, rho, speed, nu + nu2);
                real1[s] = eps_amended(s, lit, rho, speed, nu + nu1);
            }
            ok = true;
            for (std::size_t s = 0; s < kSteps.size(); ++s) {
                ok = ok && std::fabs(rigid[s] / fa.eps_rigid[s] - 1) < 0.002;
                ok = ok && std::fabs(real2[s] / fa.eps_real2[s] - 1) < 0.002;
                ok = ok && std::fabs(real1[s] / fa.eps_real1[s] - 1) < 0.002;
            }
            ok = ok && std::fabs(nu2 / fa.nu_chain_2 - 1) < 0.002 && std::fabs(nu1 / fa.nu_chain_1 - 1) < 0.002;
            const auto largest = [](const std::array<double, 5>& v) {
                double m = v[0];
                for (std::size_t s = 1; s < v.size(); ++s) m = py_max(m, v[s]);
                return m;
            };
            ok = ok && std::fabs(largest(rigid) / fa.b_rigid - 1) < 0.002 && std::fabs(largest(real2) / fa.b_real2 - 1) < 0.002 && std::fabs(largest(real1) / fa.b_real1 - 1) < 0.002;
            io.out << "    the frozen amended eps(h), nu_chain and B_pred reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
            bad += ok ? 0 : 1;
        }
    }
    return bad;
}

}  // namespace

// =====================================================================================================================================================================================================
// THE ANGLE SIZING — 2026-10-06, WRITTEN AND COMMITTED BEFORE THE ANGLE GATE'S FIRST RUN (item 6; SPEC-measmod.md §6.2 (iv), "The angle gate")
// =====================================================================================================================================================================================================
// Amendment A1 (iv) said the angle gate takes the same two terms before it first runs: (i) the chain's floor is ZERO — an angle observation puts the observer, the Earth's orientation and the Earth's motion at the
// OBSERVATION epoch, which no variation of the target moves, so the floor cancels in every difference (MEAS-A-089 shows the epochs asked); (ii) F_a is a bound over the stencil for the whole modelled angle: the
// geometry, the light-time coupling, the reduction's aberration, and the refraction.
//
//   F_a(h)  =  F_pure(h)  +  1.01 F_extra
//   F_pure(h) = 2 / [ (rho - h)^3 cos^3(phi_max + asin(h/rho)) ]          the pure geometric direction, a THEOREM: the third derivative of right ascension along a unit displacement is 2 Im[(w1/w0)^3],
//                                                                          |w1| <= 1, |w0| = rho' cos(phi'), rho' >= rho - h and |phi'| <= phi_max + asin(h/rho) over the stencil; declination's is smaller
//   F_extra   = sup | d3y(the full modelled angle) - d3y(the pure direction of the same displaced target) | over the class, d3 the third derivative along the displacement, by the exact power series of the MODEL
//               ITSELF (the light-time coupling re-solved for every displacement; the aberration; the diurnal aberration, the local frame and the refraction), at the stencil's centre and its two ends; the 1.01 allows
//               for the grid.  The two-method guard: the series is checked against 60-digit central third differences of an independent implementation before it is used.
//
// The class (the geometry rule of the gate, SPEC §6.2 "The angle gate"): |declination| <= 39.5 deg of the geometric direction in the observation's frame (<= 40 deg after the aberration's 0.012 deg), and for
// azimuth/elevation an elevation of 20 to 39.5 deg; rho = 1.2e6 m at emission; the target at 7.5 km/s across the line of sight and along it.

namespace {

constexpr double kRhoA = 1.2e6;                                  // m: the slant range at emission
constexpr double kVTargetA = 7500.0;                             // m/s
const double kPhiMaxA = dk::py_radians(40.0);
constexpr double kBetaEMax = 30.29e3 / kLight;                   // the Earth's speed at the January perihelion over c (the largest it is)
double beta_d_max() { return v_station_max() / kLight; }         // the equatorial station's v/c: 1.55e-6, above any station's
constexpr double kGmEarth = 3.986004415e14;
double nu_a() { return 3 * (dk::py_ulp(2 * kPi) + dk::py_ulp(7.2e6) / kRhoA); }   // rad: as frozen (the output's own ulp and the operand's, as an angle)
constexpr double kDeltaSMax = 1.0e-6;                            // m: the largest difference between the real chain's observer and the rigid configuration's at the observation epoch (a precondition the gate asserts)

// the first normal point's weather (record 20 of lageos1_202601.np2, as used above): pressure, temperature, humidity and wavelength -> eraRefco's A and B (60 digits)
struct AtmA {
    double p_hpa, t_c, rh, wl_um;
};
AtmA atm_a() { return AtmA{kPHpa, kTKelvin - 273.15, kRhPercent / 100.0, kLambdaUm}; }

}  // namespace

std::pair<double, double> refco(double p_hpa, double t_c, double rh, double wl_um) {
    // eraRefco's A and B, from the model as ERFA documents and implements it (Gill's saturation pressure, Crane's vapour pressure, the IAG refractivity, Stone's beta, Green's constants), in 60 digits for the
    // doubles passed.  The optical case only (wavelength <= 100 micrometres).
    const auto n = [](const char* text) { return Decimal::from_string(text); };
    const Decimal p = Decimal::from_double(p_hpa);
    const Decimal t = Decimal::from_double(t_c);
    const Decimal r = Decimal::from_double(rh);
    const Decimal w = Decimal::from_double(wl_um);
    const Decimal ps = (Decimal(10).pow((n("0.7859") + n("0.03477") * t) / (Decimal(1) + n("0.00412") * t))) * (Decimal(1) + p * (n("4.5e-6") + n("6e-10") * t * t));
    const Decimal pw = r * ps / (Decimal(1) - (Decimal(1) - r) * ps / p);
    const Decimal tk = t + n("273.15");
    const Decimal wlsq = w * w;
    const Decimal gamma = (((n("77.53484e-6") + (n("4.39108e-7") + n("3.666e-9") / wlsq) / wlsq) * p) - n("11.2684e-6") * pw) / tk;
    const Decimal beta = n("4.4474e-6") * tk;
    return {(gamma * (Decimal(1) - beta)).to_double(), (-gamma * (beta - gamma / Decimal(2))).to_double()};
}

double refco_check(double a_erfa, double b_erfa) {
    // ERFA's own published case (t_erfa_c.c: 800 hPa, 10 C, 90 %, 0.4 um): the difference from its A = 0.2264949956241415009e-3, B = -0.2598658261729343970e-6, in units of ERFA's tolerances
    const auto [a, b] = refco(800.0, 10.0, 0.9, 0.4);
    return py_max(std::fabs(a - a_erfa) / 1e-15, std::fabs(b - b_erfa) / 1e-18);
}

namespace {

// ---- series on 3-vectors ---------------------------------------------------------------------------------------------------------------------------------------------------

Series v_dot(const SVec3& a, const SVec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Series v_dot(const SVec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

SVec3 v_unit(const SVec3& a) {
    const Series n = v_dot(a, a).sqrt();
    return {a[0] / n, a[1] / n, a[2] / n};
}

}  // namespace

// A(n; beta) of MEAS-R-053 on a series vector n and a float vector beta; D(n; beta) is A(n; -beta).
SVec3 aberrate(const SVec3& n, const Vec3& beta) {
    const double inv_gamma = dk::py_sqrt(1.0 - sum3(beta[0] * beta[0], beta[1] * beta[1], beta[2] * beta[2]));
    const Series nb = v_dot(n, beta);
    const Series w = 1.0 + nb / (1.0 + inv_gamma);
    const Series q = 1.0 + nb;
    SVec3 out;
    for (std::size_t i = 0; i < 3; ++i) out[i] = (n[i] * inv_gamma + w * beta[i]) / q;
    return out;
}

namespace {

// F(z) = z + a tan z + b tan^3 z and its first three derivatives, at the float z.
std::array<double, 3> f_derivs(double z, double a, double b) {
    const double t = dk::py_tan(z);
    const double s2 = 1.0 + t * t;
    const double f1 = 1.0 + a * s2 + 3.0 * b * t * t * s2;
    const double f2 = 2.0 * a * t * s2 + 6.0 * b * t * s2 * s2 + 6.0 * b * dk::py_pow(t, 3) * s2;
    const double f3 = 2.0 * a * s2 * (s2 + 2 * t * t) + 6.0 * b * s2 * s2 * (s2 + 4 * t * t) + 6.0 * b * t * t * s2 * (3 * s2 + 2 * t * t);
    return {f1, f2, f3};
}

// z_o as a series, from z_v = z_o + a tan z_o + b tan^3 z_o: Newton on the constant term, then the cubic Taylor polynomial of the inverse function composed with the series.
Series refract_series(const Series& zv, double a, double b) {
    const double z0 = zv.c[0];
    double zo = z0;
    for (int iteration = 0; iteration < 60; ++iteration) {
        const double t = dk::py_tan(zo);
        const double g = zo + a * t + b * dk::py_pow(t, 3) - z0;
        const double f1 = f_derivs(zo, a, b)[0];
        const double step = g / f1;
        zo -= step;
        if (std::fabs(step) < 1e-16) break;
    }
    const std::array<double, 3> derivatives = f_derivs(zo, a, b);
    const double f1 = derivatives[0];
    const double f2 = derivatives[1];
    const double f3 = derivatives[2];
    const double g1 = 1.0 / f1;
    const double g2 = -f2 / dk::py_pow(f1, 3);
    const double g3 = -f3 / dk::py_pow(f1, 4) + 3.0 * f2 * f2 / dk::py_pow(f1, 5);
    const Series d = zv - z0;   // a series without constant term
    return Series(zo) + g1 * d + (0.5 * g2) * (d * d) + (g3 / 6.0) * (d * d * d);
}

// The unit direction (a series vector in s = displacement / rho along the unit vector e, about the stencil point xi0 / rho) from the origin observer to a target that was at `los` (unit vector, in units of rho) at its
// nominal emission epoch, moving at v_hat (in units of c) with acceleration a_hat (rho/c^2), displaced TIME-FIXED by (xi0 + s) e, the light time re-solved.  Also returns the displaced position itself (the pure
// direction's source).
std::pair<SVec3, SVec3> emission_direction(const Vec3& los, const Vec3& e, double xi0, const Vec3& v_hat, const Vec3& a_hat, int passes = 3) {
    SVec3 base;
    for (std::size_t i = 0; i < 3; ++i) base[i] = Series(los[i] + xi0 * e[i], e[i]);
    double u0 = 0.0;
    for (int iteration = 0; iteration < 80; ++iteration) {   // the constant term of the light-time shift, tau - tau_nominal, in units rho/c
        Vec3 r{};
        for (std::size_t i = 0; i < 3; ++i) r[i] = los[i] + xi0 * e[i] - v_hat[i] * u0 + 0.5 * a_hat[i] * u0 * u0;
        const double un = dk::py_sqrt(sum3(r[0] * r[0], r[1] * r[1], r[2] * r[2])) - 1.0;
        if (std::fabs(un - u0) < 1e-18) {
            u0 = un;
            break;
        }
        u0 = un;
    }
    Series u(u0);
    const auto residual = [&](const Series& shift) {
        SVec3 r;
        for (std::size_t i = 0; i < 3; ++i) r[i] = base[i] - v_hat[i] * shift + (0.5 * a_hat[i]) * (shift * shift);
        return r;
    };
    for (int pass = 0; pass < passes; ++pass) {
        const SVec3 r = residual(u);
        u = v_dot(r, r).sqrt() - 1.0;
    }
    const SVec3 r = residual(u);
    const Series inv = 1.0 / (1.0 + u);
    return {SVec3{r[0] * inv, r[1] * inv, r[2] * inv}, base};
}

// Right ascension and declination, as two series, of a series direction with n_x > 0.
std::array<Series, 2> ra_dec_series(const SVec3& n) {
    const Series h = (n[0] * n[0] + n[1] * n[1]).sqrt();
    return {atan_series(n[1] / n[0]), atan_series(n[2] / h)};
}

enum class Kind { Geometric, Astrometric, Refracted };

// The (first, second) output series of the full modelled angle (`kind`: "geometric", "astrometric", "refracted") and of the pure geometric direction of the same displaced target.
// beta: the Earth's velocity over c (astrometric) or the observer's (refracted), a float vector; atm_ab: (A, B) of the refraction.  The local frame of "refracted" is x = north, y = east, z = up.
struct Outputs {
    std::array<Series, 2> full;
    std::array<Series, 2> pure;
};

Outputs outputs_series(Kind kind, const Vec3& los, const Vec3& e, double xi0, const Vec3& v_hat, const Vec3& a_hat, const Vec3& beta, const std::pair<double, double>& atm_ab) {
    const auto [n_geo, displaced] = emission_direction(los, e, xi0, v_hat, a_hat);
    const std::array<Series, 2> pure = ra_dec_series(v_unit(displaced));
    if (kind == Kind::Geometric) return Outputs{ra_dec_series(n_geo), pure};
    if (kind == Kind::Astrometric) return Outputs{ra_dec_series(aberrate(n_geo, Vec3{-beta[0], -beta[1], -beta[2]})), pure};
    const SVec3 n_app = aberrate(n_geo, beta);
    const Series az = atan_series(n_app[1] / n_app[0]);
    const Series zv = atan_series((n_app[0] * n_app[0] + n_app[1] * n_app[1]).sqrt() / n_app[2]);
    const Series zo = refract_series(zv, atm_ab.first, atm_ab.second);
    return Outputs{{az, kPi / 2 - zo}, pure};
}

std::array<double, 2> third_of(const std::array<Series, 2>& pair) { return {6.0 * pair[0].c[3], 6.0 * pair[1].c[3]}; }

// Unit displacement directions on a half-sphere (the third derivative is odd in the direction, so its modulus is even).
std::vector<Vec3> sphere_half_directions(double step_deg) {
    std::vector<Vec3> out;
    const int n_th = static_cast<int>(std::nearbyint(180.0 / step_deg));
    const int n_ph = static_cast<int>(std::nearbyint(360.0 / step_deg));
    for (int it = 0; it <= n_th; ++it) {
        const double th = dk::py_radians(step_deg * it);
        for (int jp = 0; jp < n_ph; ++jp) {
            const double ph = dk::py_radians(step_deg * jp);
            const Vec3 e = {dk::py_sin(th) * dk::py_cos(ph), dk::py_sin(th) * dk::py_sin(ph), dk::py_cos(th)};
            if (e[2] > 1e-12 || (std::fabs(e[2]) <= 1e-12 && (e[0] > 1e-12 || (std::fabs(e[0]) <= 1e-12 && e[1] > 0)))) out.push_back(e);
        }
    }
    return out;
}

}  // namespace

// v_hat and a_hat for the scaled scenario: the speed across the line of sight (radial = 0: perpendicular to `los` and to the third axis) or along it (radial = 1).
std::pair<Vec3, Vec3> scenario_vectors(const Vec3& los, double radial) {
    const Vec3 up = {0.0, 0.0, 1.0};
    Vec3 across = {los[1] * up[2] - los[2] * up[1], los[2] * up[0] - los[0] * up[2], los[0] * up[1] - los[1] * up[0]};
    const double na = dk::py_sqrt(sum3(across[0] * across[0], across[1] * across[1], across[2] * across[2]));
    for (double& x : across) x = x / na;
    const double sp = kVTargetA / kLight;
    Vec3 v_hat{};
    for (std::size_t i = 0; i < 3; ++i) v_hat[i] = sp * (dk::py_sqrt(1 - radial * radial) * across[i] + radial * los[i]);
    const double a_mag = kGmEarth / dk::py_pow(7.2e6, 2);
    Vec3 a_hat{};
    for (std::size_t i = 0; i < 3; ++i) a_hat[i] = -a_mag * kRhoA / dk::py_pow(kLight, 2) * los[i];   // a representative acceleration, 7.7 m/s^2 toward the geocentre
    return {v_hat, a_hat};
}

namespace {

// The scan: for each line of sight, velocity direction, displacement direction, centre of the stencil and beta, the third derivatives of the full modelled angle and of the pure direction.  Returns (sup |full -
// pure| per output, sup |pure| per output, the worst ratio of a pure third derivative to F_pure at its stencil point, sup |full| per output).
struct AngleScan {
    std::array<double, 2> sup_diff{};
    std::array<double, 2> sup_pure{};
    double worst_ratio = 0.0;
    std::array<double, 2> sup_full{};
};

AngleScan angle_scan(Kind kind, const std::vector<Vec3>& los_list, const std::vector<Vec3>& e_list, const std::vector<Vec3>& beta_list, const std::pair<double, double>& atm_ab) {
    AngleScan out;
    for (const Vec3& los : los_list) {
        for (const double radial : {0.0, 1.0}) {
            const auto [v_hat, a_hat] = scenario_vectors(los, radial);
            for (const Vec3& e : e_list) {
                for (const double xi : {0.0, 1000.0, -1000.0}) {
                    for (const Vec3& beta : beta_list) {
                        const Outputs result = outputs_series(kind, los, e, xi / kRhoA, v_hat, a_hat, beta, atm_ab);
                        const std::array<double, 2> tf = third_of(result.full);
                        const std::array<double, 2> tp = third_of(result.pure);
                        for (std::size_t k = 0; k < 2; ++k) {
                            out.sup_diff[k] = py_max(out.sup_diff[k], std::fabs(tf[k] - tp[k]));
                            out.sup_pure[k] = py_max(out.sup_pure[k], std::fabs(tp[k]));
                            out.sup_full[k] = py_max(out.sup_full[k], std::fabs(tf[k]));
                            out.worst_ratio = py_max(out.worst_ratio, std::fabs(tp[k]) / (f_pure(std::fabs(xi)) * dk::py_pow(kRhoA, 3)));
                        }
                    }
                }
            }
        }
    }
    return out;
}

std::vector<Vec3> beta_dirs(double mag) { return {{mag, 0.0, 0.0}, {0.0, mag, 0.0}, {0.0, 0.0, mag}}; }

// ---- the two-method guard: 60-digit central third differences of an independent implementation (Decimal) -------------------------------------------------------------------------------------------

// arctan in 60 digits: halving the argument four times, then the Taylor series.
Decimal d_atan(const Decimal& x) {
    Decimal y = x;
    int k = 0;
    for (int halving = 0; halving < 4; ++halving) {
        y = y / (Decimal(1) + (Decimal(1) + y * y).sqrt());
        ++k;
    }
    Decimal term = y;
    Decimal total = y;
    std::int64_t n = 0;
    const Decimal y2 = y * y;
    const Decimal tiny = Decimal::from_string("1e-70");
    while (term.abs() > tiny) {
        ++n;
        term = -term * y2;
        total = total + term / Decimal(2 * n + 1);
    }
    return total * Decimal(1 << k);
}

Decimal d_tan(const Decimal& z) {
    Decimal s = z;
    Decimal term = z;
    std::int64_t n = 0;
    const Decimal z2 = z * z;
    const Decimal tiny = Decimal::from_string("1e-70");
    while (term.abs() > tiny) {
        ++n;
        term = -term * z2 / Decimal((2 * n) * (2 * n + 1));
        s = s + term;
    }
    return s / mr::d_cos(z);
}

// The same two outputs, independently, in 60-digit arithmetic and by iteration of the light time (no series): the displaced target at xi (a Decimal, in rho units; the sample points of the guard are formed in
// Decimal, so that their spacing is exact).
std::array<Decimal, 2> decimal_outputs(Kind kind, const Vec3& los, const Vec3& e, const Decimal& xi, const Vec3& v_hat, const Vec3& a_hat, const Vec3& beta, const std::pair<double, double>& atm_ab) {
    const auto to_vec = [](const Vec3& v) { return mr::Vec{Decimal::from_double(v[0]), Decimal::from_double(v[1]), Decimal::from_double(v[2])}; };
    const mr::Vec los_d = to_vec(los);
    const mr::Vec e_d = to_vec(e);
    const mr::Vec v_d = to_vec(v_hat);
    const mr::Vec a_d = to_vec(a_hat);
    const Decimal half = Decimal::from_string("0.5");
    const auto residual = [&](const Decimal& u) {
        mr::Vec r;
        for (std::size_t i = 0; i < 3; ++i) r[i] = los_d[i] + xi * e_d[i] - v_d[i] * u + half * a_d[i] * u * u;
        return r;
    };
    const auto sum_of_squares = [](const mr::Vec& r) { return Decimal(0) + r[0] * r[0] + r[1] * r[1] + r[2] * r[2]; };   // sum(x * x for x in r): the int 0, then each in turn
    Decimal u(0);
    for (int iteration = 0; iteration < 40; ++iteration) u = sum_of_squares(residual(u)).sqrt() - Decimal(1);
    const mr::Vec r = residual(u);
    mr::Vec n;
    for (std::size_t i = 0; i < 3; ++i) n[i] = r[i] / (Decimal(1) + u);
    const auto ab = [&](const mr::Vec& nv, const Vec3& bv) {
        const mr::Vec b = {Decimal::from_double(bv[0]), Decimal::from_double(bv[1]), Decimal::from_double(bv[2])};
        const Decimal bb = sum_of_squares(b);
        const Decimal ig = (Decimal(1) - bb).sqrt();
        const Decimal nb = Decimal(0) + nv[0] * b[0] + nv[1] * b[1] + nv[2] * b[2];
        const Decimal w = Decimal(1) + nb / (Decimal(1) + ig);
        mr::Vec out;
        for (std::size_t i = 0; i < 3; ++i) out[i] = (nv[i] * ig + w * b[i]) / (Decimal(1) + nb);
        return out;
    };
    mr::Vec m = n;
    if (kind == Kind::Astrometric) m = ab(n, Vec3{-beta[0], -beta[1], -beta[2]});
    else if (kind == Kind::Refracted) m = ab(n, beta);
    const Decimal hh = (m[0].pow(Decimal(2)) + m[1].pow(Decimal(2))).sqrt();
    if (kind == Kind::Refracted) {
        const Decimal zv = d_atan(hh / m[2]);
        const Decimal a = Decimal::from_double(atm_ab.first);
        const Decimal b = Decimal::from_double(atm_ab.second);
        Decimal zo = zv;
        for (int iteration = 0; iteration < 40; ++iteration) {
            const Decimal t = d_tan(zo);
            const Decimal g = zo + a * t + b * t.pow(Decimal(3)) - zv;
            const Decimal dg = Decimal(1) + a * (Decimal(1) + t * t) + Decimal(3) * b * t * t * (Decimal(1) + t * t);
            zo = zo - g / dg;
        }
        return {d_atan(m[1] / m[0]), mr::pi() / Decimal(2) - zo};
    }
    return {d_atan(m[1] / m[0]), d_atan(m[2] / hh)};
}

// The largest relative disagreement between the series' third derivative and the 60-digit central third difference, over a few cases of every kind.
double series_guard(const std::pair<double, double>& atm_ab) {
    struct Case {
        Kind kind;
        Vec3 los, e, beta;
    };
    const std::array<Case, 4> cases = {{{Kind::Geometric, unit_of_angles(30.0, 35.0), {0.3, -0.5, 0.8124038404635961}, {0.0, 0.0, 0.0}},
                                        {Kind::Astrometric, unit_of_angles(-20.0, -30.0), {-0.6, 0.48, 0.64}, {7.0e-5, -5.0e-5, 4.0e-5}},
                                        {Kind::Refracted, unit_of_angles(10.0, 25.0), {0.36, 0.48, -0.8}, {0.0, 1.5e-6, 4.0e-7}},
                                        {Kind::Refracted, unit_of_angles(-40.0, 38.0), {0.8, 0.0, 0.6}, {1.0e-6, -1.0e-6, 0.0}}}};
    double worst = 0.0;
    for (const Case& cs : cases) {
        Vec3 e{};
        const double norm = dk::py_sqrt(sum3(cs.e[0] * cs.e[0], cs.e[1] * cs.e[1], cs.e[2] * cs.e[2]));
        for (std::size_t i = 0; i < 3; ++i) e[i] = cs.e[i] / norm;
        const auto [v_hat, a_hat] = scenario_vectors(cs.los, 0.3);
        for (const double xi : {0.0, 8.0e-4}) {
            const Outputs result = outputs_series(cs.kind, cs.los, e, xi, v_hat, a_hat, cs.beta, atm_ab);
            const std::array<double, 2> t_series = third_of(result.full);
            const double delta = 2.0e-5;   // in rho units: 24 m (the 4-point difference converges as delta^2: checked at 2e-4, 1e-4, 5e-5)
            std::array<std::array<Decimal, 2>, 4> ys;
            std::size_t index = 0;
            for (const int k : {-2, -1, 1, 2}) ys[index++] = decimal_outputs(cs.kind, cs.los, e, Decimal::from_double(xi) + Decimal(k) * Decimal::from_double(delta), v_hat, a_hat, cs.beta, atm_ab);
            for (std::size_t out = 0; out < 2; ++out) {
                const Decimal third = (ys[3][out] - Decimal(2) * ys[2][out] + Decimal(2) * ys[1][out] - ys[0][out]) / (Decimal(2) * Decimal::from_double(delta).pow(Decimal(3)));
                const double third_double = third.to_double();
                worst = py_max(worst, std::fabs(third_double - t_series[out]) / py_max(std::fabs(third_double), 1e-3));
            }
        }
    }
    return worst;
}

// ---- the frozen numbers of the angle sizing (the section's own output): Frozen::angle_groups, angle_agreement, angle_nu_a, angle_refraction, in the header ----------------------------------------------------------
// THE ANGLE GATE'S FROZEN NUMBERS, 2026-10-06, BEFORE THE ANGLE GATE EXISTS (the section's own output, 5 significant digits; SPEC-measmod.md §6.2 (v) holds the same).
//   f_extra : F_extra / rho^3 = 1.05 x the FINE scan below (a 10-degree grid of displacement directions over the half-sphere, 25 lines of sight, the stencil's centre and both ends, both velocity directions, the
//             aberration vector along each axis), rounded up to three digits.  The 15-degree grid the check uses gives 2.9 %, 1.6 % and 1.0 % less (Geometric, Astrometric, refracted) than the 10-degree one, and a
//             20-degree grid up to 4.5 % less: 1.05 allows for the grid.
//   fine    : the fine scan's own supremum of |full - pure| over both outputs (not for "ofdate").
using AngleGroup = FrozenAngleGroup;

// F_a(h) of a group over the five sizes: F_pure(h) + F_extra / rho^3.
std::array<double, 5> f_a_table(const AngleGroup& group) {
    std::array<double, 5> out{};
    for (std::size_t i = 0; i < 5; ++i) out[i] = f_pure(kSteps[i]) + group.f_extra / dk::py_pow(kRhoA, 3);
    return out;
}

std::array<double, 5> eps_a_table(const AngleGroup& group) {
    const std::array<double, 5> f = f_a_table(group);
    std::array<double, 5> out{};
    for (std::size_t i = 0; i < 5; ++i) out[i] = kSteps[i] * kSteps[i] * f[i] / 6 + nu_a() / kSteps[i];
    return out;
}

std::array<double, 5> agreement_table() {
    const double kappa = kVTargetA / kLight;
    std::array<double, 5> out{};
    for (std::size_t i = 0; i < 5; ++i) out[i] = 2 * nu_a() / kSteps[i] + 3 * kDeltaSMax / dk::py_pow(kRhoA * (1 - kappa) - kSteps[i], 2);
    return out;
}

double largest(const std::array<double, 5>& v) {
    double m = v[0];
    for (std::size_t i = 1; i < v.size(); ++i) m = py_max(m, v[i]);
    return m;
}

double largest2(const std::array<double, 2>& v) { return py_max(v[0], v[1]); }

Kind kind_of(const std::string& name) { return name == "geometric" ? Kind::Geometric : name == "astrometric" ? Kind::Astrometric : Kind::Refracted; }

int angle_report(Streams& io, bool scan_flag, bool check, const Frozen& frozen) {
    int bad = 0;
    const AtmA atm = atm_a();
    const auto [a_ref, b_ref] = refco(atm.p_hpa, atm.t_c, atm.rh, atm.wl_um);
    io.out << "\n=== THE ANGLE SIZING (item 6: written and committed before the angle gate's first run) ===\n";
    io.out << "nu_a = 3 [ulp(2 pi) + ulp(7.2e6 m)/rho] = " << fe(nu_a(), 4) << " rad;  the chain's floor: ZERO (the observer, the orientation and the Earth's motion are evaluated at the observation epoch only)\n";
    io.out << "the refraction of the first normal point's weather (" << dk::py_float_repr(atm.p_hpa) << " hPa, " << ff(atm.t_c, 2) << " C, rh " << dk::py_float_repr(atm.rh) << ", " << dk::py_float_repr(atm.wl_um)
           << " um): A = " << fe(a_ref, 6) << ", B = " << fe(b_ref, 6) << " rad\n";
    io.out << "    h        F_pure(h) (m^-3)    h^2 F_pure/6      nu_a/h       agreement tolerance\n";
    const std::array<double, 5> ag = agreement_table();
    for (std::size_t i = 0; i < kSteps.size(); ++i) {
        const double h = kSteps[i];
        io.out << "    " << fwf(h, 6, 0) << "   " << fe(f_pure(h), 5) << "    " << fe(h * h * f_pure(h) / 6, 4) << "   " << fe(nu_a() / h, 4) << "   " << fe(ag[i], 4) << '\n';
    }
    for (const AngleGroup& group : frozen.angle_groups) {
        const std::array<double, 5> eps = eps_a_table(group);
        io.out << "    " << dk::pad_right(group.name, 12) << ": F_extra/rho^3 = " << fg(group.f_extra, 3) << "  F_a(h) = " << list_of_e(f_a_table(group), 5) << '\n';
        io.out << "                  eps(h) = " << list_of_e(eps, 4) << "   B_pred = " << fe(largest(eps), 4) << "  window [" << fe(largest(eps) / 10, 3) << ", " << fe(largest(eps) * 10, 3) << "]\n";
    }
    if (check) {
        const double d = refco_check(frozen.erfa_a, frozen.erfa_b);
        bool ok = d < 1.0;
        io.out << "the 60-digit eraRefco against ERFA's published case: " << ff(d, 3) << " of ERFA's tolerances: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = std::fabs(a_ref / frozen.angle_refraction.first - 1) < 1e-6 && std::fabs(b_ref / frozen.angle_refraction.second - 1) < 1e-6;
        io.out << "the frozen A and B of the first normal point's weather reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = std::fabs(nu_a() / frozen.angle_nu_a - 1) < 1e-4;
        io.out << "the frozen nu_a reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = true;
        for (const AngleGroup& group : frozen.angle_groups) {
            const std::array<double, 5> eps = eps_a_table(group);
            bool group_ok = std::fabs(largest(eps) / group.b_pred - 1) < 1e-3;
            for (std::size_t i = 0; i < 5; ++i) group_ok = group_ok && std::fabs(eps[i] / group.eps[i] - 1) < 1e-3;
            ok = ok && group_ok;
        }
        io.out << "the frozen eps(h) and B_pred of the four groups reproduced from F_pure and the frozen F_extra: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = true;
        for (std::size_t i = 0; i < 5; ++i) ok = ok && std::fabs(ag[i] / frozen.angle_agreement[i] - 1) < 1e-3;
        io.out << "the frozen agreement tolerances reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = true;
        for (const AngleGroup& group : frozen.angle_groups) {
            if (std::string(group.name) == "ofdate") continue;   // (the fine scan has three groups)
            ok = ok && std::fabs(ceil3(1.05 * group.fine) / group.f_extra - 1) < 1e-9;
        }
        io.out << "the frozen F_extra are 1.05 x the fine scan, rounded up to three digits: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
    }
    if (!scan_flag) return bad;
    // the lines of sight of the scan, by the class of the gate: declinations -39.5 .. 39.5 for right ascension and declination, elevations 20, 30, 39.5 for azimuth and elevation
    std::vector<Vec3> radec;
    for (const double dec : {-39.5, 0.0, 39.5}) {
        for (const double ra : {-45.0, 45.0}) radec.push_back(unit_of_angles(ra, dec));
    }
    std::vector<Vec3> azel;
    for (const double el : {20.0, 30.0, 39.5}) {
        for (const double az : {-45.0, 0.0, 45.0}) azel.push_back(unit_of_angles(az, el));
    }
    const std::vector<Vec3> e_list = sphere_half_directions(15.0);
    io.out << "the series guard against 60-digit third differences ... ";
    io.out.flush();
    const double g = series_guard({a_ref, b_ref});
    io.out << "worst relative disagreement " << fe(g, 2) << ": " << (g < 1e-6 ? "ok" : "NOT AGREED") << '\n';
    bad += g < 1e-6 ? 0 : 1;
    struct Run {
        const char* kind;
        const std::vector<Vec3>* los_list;
        std::vector<Vec3> beta_list;
    };
    const std::array<Run, 3> runs = {{{"geometric", &radec, {{0.0, 0.0, 0.0}}}, {"astrometric", &radec, beta_dirs(kBetaEMax)}, {"refracted", &azel, beta_dirs(beta_d_max())}}};
    for (const Run& run : runs) {
        const std::string kind = run.kind;
        const AngleGroup* group = nullptr;
        for (const AngleGroup& candidate : frozen.angle_groups) {
            if (kind == candidate.name) group = &candidate;
        }
        const AngleScan found = angle_scan(kind_of(kind), *run.los_list, e_list, run.beta_list, {a_ref, b_ref});
        io.out << "--- " << kind << " (check grid: 15 degrees, " << run.los_list->size() << " lines of sight): sup |full - pure| = (" << fe(found.sup_diff[0], 4) << ", " << fe(found.sup_diff[1], 4)
               << ") /rho^3;  sup |pure| = (" << ff(found.sup_pure[0], 4) << ", " << ff(found.sup_pure[1], 4) << "), worst ratio to F_pure " << ff(found.worst_ratio, 5) << ";  sup |full| = ("
               << ff(found.sup_full[0], 4) << ", " << ff(found.sup_full[1], 4) << ")\n";
        const double mine = largest2(found.sup_diff);
        bool ok = mine <= group->f_extra && mine >= 0.85 * group->fine;
        io.out << "    below the frozen F_extra " << fg(group->f_extra, 3) << " and within 15 % of the fine scan's " << fg(group->fine, 4) << ": " << (ok ? "yes" : "NO") << '\n';
        bad += ok ? 0 : 1;
        ok = found.worst_ratio <= 1.0;
        io.out << "    the pure geometric formula bounds the pure third derivatives of the scan: " << (ok ? "yes" : "NO") << '\n';
        bad += ok ? 0 : 1;
        double full_bound = f_pure(kSteps[0]) * dk::py_pow(kRhoA, 3) + group->f_extra;
        for (std::size_t i = 1; i < kSteps.size(); ++i) full_bound = py_max(full_bound, f_pure(kSteps[i]) * dk::py_pow(kRhoA, 3) + group->f_extra);
        ok = largest2(found.sup_full) <= full_bound;
        io.out << "    the full third derivative stays below F_a (the largest of the five sizes, " << ff(full_bound, 4) << "): " << (ok ? "yes" : "NO") << '\n';
        bad += ok ? 0 : 1;
    }
    return bad;
}

// =====================================================================================================================================================================================================
// THE DIURNAL-ABERRATION CHECK (MEAS-A-098b) — 2026-10-06, REGISTERED AFTER THE ANGLE GATE'S ONE RUN AND BEFORE THIS CHECK HAS RUN
// =====================================================================================================================================================================================================
// The angle gate's control "without the diurnal aberration's Jacobian A'(.; beta_d)" asked 10^2 epsilon and gave 0.47: at the geometry the rule selected (a line of sight due north, a station moving east) the
// Jacobian's first-order effect on an angular displacement, the scalar (1 - n.beta), is zero, and the control had no power.  The manager's ruling (7baf3ad, 2026-10-06): the run stands; the claim — that G1
// catches a missing or mis-wired A'(.; beta_d) in the azimuth/elevation row — moves to a NEW check at a closed-form geometry with the station moving along the line of sight, sized by the same procedure
// EVALUATED AT THIS GEOMETRY (not the same numbers carried over), with each control's power evaluated at this geometry and written down before it runs.
//
// Geometry (SPEC-measmod 6.2 (vi)): the identity Earth orientation, a site on the equator at longitude 0 (up = x, east = y, north = z of the GCRS axes), the observer at rest in position at (6 378 137, 0, 0) m with
// the inertial velocity of the equatorial surface, 465.1 m/s, ALONG THE LINE OF SIGHT (toward the target, n.beta = +beta; or away, -beta); the target static (no light-time coupling to confuse the check) at
// azimuth 90 deg, elevation 30 deg, slant range 1.2e6 m; the atmosphere the first normal point's.  Because beta is parallel to n, A(n; +-beta) = n: the apparent direction IS the geometric one, and the right row,
// the row without A' and the row with A'(.; -beta) share one base direction and differ only in the Jacobian — which is what the check isolates.
//
// The sizing, the procedure of the angle sizing evaluated here:
//   nu    = 3 [ulp(output) + ulp(operand)/rho]   output: the azimuth, 1.5708 rad (ulp 2^-52, not ulp(2 pi): THIS geometry's); operand: the target's largest position component
//   F_a(h) = F_pure(h; phi = 30 deg) + 1.05 F_extra/rho^3, F_pure(h; phi) = 2/[(rho - h)^3 cos^3(phi + asin(h/rho))], F_extra by the exact series of the model at THIS line of sight
//   eps(h) = h^2 F_a(h)/6 + nu/h;   B_pred = max eps;   the controls' power: |Delta row| / eps at each size, from the series' first derivatives (an implementation independent of the C++ rows)

constexpr double kDiAzDeg = 90.0;
constexpr double kDiElDeg = 30.0;
constexpr double kDiRho = 1.2e6;
double di_speed() { return v_station_max(); }   // m/s: omega_E a = 465.1, the largest speed of any ground station

double di_nu() {
    const Vec3 los = unit_of_angles(kDiAzDeg, kDiElDeg);
    const double r_largest = 6378137.0 + kDiRho * los[2];   // the target's largest position component: the station's x plus rho sin(el)
    return 3 * (dk::py_ulp(dk::py_radians(kDiAzDeg)) + dk::py_ulp(r_largest) / kDiRho);
}

}  // namespace

// The azimuth of a series direction (x north, y east, z up), by the branch that stays away from the singularity: arctan(y/x) for |x| >= |y|, else +-pi/2 - arctan(x/y).
Series az_series_any(const SVec3& n) {
    if (std::fabs(n[0].c[0]) >= std::fabs(n[1].c[0])) return atan_series(n[1] / n[0]);
    const double sgn = n[1].c[0] > 0 ? 1.0 : -1.0;
    return sgn * (kPi / 2) - atan_series(n[0] / n[1]);
}

namespace {

enum class Variant { Right, Without, Reversed };

// The (azimuth, elevation) series of the diurnal check's model — variant "right" (A' of the model), "without" (the identity Jacobian) or "reversed" (A'(.; -beta)) — and the pure direction's series, for the static
// target displaced along the unit vector e (local axes) about the stencil point xi0 (units of rho).
Outputs di_outputs(Variant variant, double sign, const Vec3& e, double xi0, const std::pair<double, double>& atm_ab) {
    const Vec3 los = unit_of_angles(kDiAzDeg, kDiElDeg);
    SVec3 base;
    for (std::size_t i = 0; i < 3; ++i) base[i] = Series(los[i] + xi0 * e[i], e[i]);
    const SVec3 n_geo = v_unit(base);
    Vec3 beta{};
    for (std::size_t i = 0; i < 3; ++i) beta[i] = sign * (di_speed() / kLight) * los[i];
    SVec3 n_app;
    if (variant == Variant::Right) {
        n_app = aberrate(n_geo, beta);
    } else if (variant == Variant::Reversed) {
        n_app = aberrate(n_geo, Vec3{-beta[0], -beta[1], -beta[2]});
    } else {   // "without": the identity Jacobian, n_app = n_geo + (A(n0; beta) - n0), a constant shift; defined at the centre of the stencil only
        if (xi0 != 0.0) throw std::logic_error("di_outputs: the identity Jacobian is defined at the centre of the stencil only");
        const SVec3 a0 = aberrate(SVec3{Series(los[0]), Series(los[1]), Series(los[2])}, beta);
        for (std::size_t i = 0; i < 3; ++i) n_app[i] = n_geo[i] + (a0[i].c[0] - los[i]);
    }
    const Series zv = atan_series((n_app[0] * n_app[0] + n_app[1] * n_app[1]).sqrt() / n_app[2]);
    const Series zo = refract_series(zv, atm_ab.first, atm_ab.second);
    Outputs out;
    out.full = {az_series_any(n_app), kPi / 2 - zo};
    const Series h_pure = (n_geo[0] * n_geo[0] + n_geo[1] * n_geo[1]).sqrt();
    out.pure = {az_series_any(n_geo), atan_series(n_geo[2] / h_pure)};
    return out;
}

// sup |third derivative of the full angle - that of the pure direction| over the displacement directions, the stencil's centre and ends, both signs of the observer's motion; and the largest pure third
// derivative against F_pure(h; 30 deg).
struct DiScan {
    std::array<double, 2> sup_diff{};
    std::array<double, 2> sup_pure{};
    double worst_ratio = 0.0;
};

DiScan di_extra_scan(const std::pair<double, double>& atm_ab, double step_deg = 10.0) {
    DiScan out;
    for (const Vec3& e : sphere_half_directions(step_deg)) {
        for (const double xi : {0.0, 1000.0, -1000.0}) {
            for (const double sign : {1.0, -1.0}) {
                const Outputs result = di_outputs(Variant::Right, sign, e, xi / kDiRho, atm_ab);
                const std::array<double, 2> tf = third_of(result.full);
                const std::array<double, 2> tp = third_of(result.pure);
                for (std::size_t k = 0; k < 2; ++k) {
                    out.sup_diff[k] = py_max(out.sup_diff[k], std::fabs(tf[k] - tp[k]));
                    out.sup_pure[k] = py_max(out.sup_pure[k], std::fabs(tp[k]));
                    out.worst_ratio = py_max(out.worst_ratio, std::fabs(tp[k]) / (f_pure_at(std::fabs(xi), kDiElDeg) * dk::py_pow(kDiRho, 3)));
                }
            }
        }
    }
    return out;
}

// The row of the model (variant "right") or of a wrong row, in rad per m: [axis][output], from the first coefficient of the exact series.
std::array<std::array<double, 2>, 3> di_first_coeffs(Variant variant, double sign, const std::pair<double, double>& atm_ab) {
    // the GCRS axes of the test's identity orientation (x = up, y = east, z = north) in (north, east, up)
    const std::array<Vec3, 3> axes = {{{0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}}};
    std::array<std::array<double, 2>, 3> out{};
    for (std::size_t k = 0; k < 3; ++k) {
        const Outputs result = di_outputs(variant, sign, axes[k], 0.0, atm_ab);
        out[k] = {result.full[0].c[1] / kDiRho, result.full[1].c[1] / kDiRho};
    }
    return out;
}

// THE DIURNAL CHECK'S FROZEN NUMBERS, 2026-10-06, BEFORE ITS TEST CODE EXISTS (the section's own output; SPEC-measmod.md 6.2 (vi) holds the same): Frozen::diurnal, in the header.
//   nu       : 3 [ulp(1.5708) + ulp(6.98e6 m)/rho], THIS geometry's output and operand (the angle gate's nu_a used ulp(2 pi) for any azimuth)
//   fine_extra / f_extra : F_extra/rho^3 from the exact series of the model at this line of sight (10-degree grid, centre and both ends, both signs of the observer's motion), and 1.05 x it rounded up to three
//             digits; the elevation output carries it (the refraction's share, 1.2 % of F_pure(30 deg)); the azimuth's own is 9 x 10^-6
//   power    : the controls' predicted violation of (b) at the smallest eps, |wrong row - right row| / eps(10 m), from the series' first derivatives

int diurnal_report(Streams& io, bool scan_flag, bool check, const Frozen& frozen) {
    int bad = 0;
    const AtmA atm = atm_a();
    const auto [a_ref, b_ref] = refco(atm.p_hpa, atm.t_c, atm.rh, atm.wl_um);
    const double nu = di_nu();
    const FrozenDiurnal& dz = frozen.diurnal;
    const double f_extra = dz.f_extra;
    io.out << "\n=== THE DIURNAL-ABERRATION CHECK (MEAS-A-098b; registered after the angle gate's one run, before this check has run) ===\n";
    io.out << "geometry: azimuth " << ff(kDiAzDeg, 0) << " deg, elevation " << ff(kDiElDeg, 0) << " deg, rho " << fg(kDiRho, 3) << " m, the observer moving at " << ff(di_speed(), 4)
           << " m/s along the line of sight; beta = " << fe(di_speed() / kLight, 6) << '\n';
    io.out << "nu (this geometry) = 3 [ulp(az = 1.5708 rad) + ulp(target's largest component)/rho] = " << fe(nu, 4) << " rad   (the angle gate's nu_a, with ulp(2 pi), was " << fe(nu_a(), 4) << ")\n";
    std::array<double, 5> eps{};
    for (std::size_t i = 0; i < 5; ++i) {
        const double h = kSteps[i];
        eps[i] = h * h * (f_pure_at(h, kDiElDeg) + f_extra / dk::py_pow(kDiRho, 3)) / 6 + nu / h;
    }
    io.out << "F_extra/rho^3 frozen = " << fg(f_extra, 3) << "  (1.05 x the fine scan's " << fe(dz.fine_extra, 4) << ", rounded up)\n";
    io.out << "    h        F_pure(h; 30 deg)    F_a(h)               eps(h)\n";
    for (std::size_t i = 0; i < 5; ++i) {
        const double h = kSteps[i];
        io.out << "    " << fwf(h, 6, 0) << "   " << fe(f_pure_at(h, kDiElDeg), 5) << "    " << fe(f_pure_at(h, kDiElDeg) + f_extra / dk::py_pow(kDiRho, 3), 5) << "    " << fe(eps[i], 4) << '\n';
    }
    io.out << "    B_pred = max eps = " << fe(largest(eps), 4) << ", window [" << fe(largest(eps) / 10, 3) << ", " << fe(largest(eps) * 10, 3) << "]\n";
    // the controls' power at THIS geometry, from the first coefficients of the exact series (independent of the C++ rows)
    double delta_without = 0.0;
    double delta_reversed = 0.0;
    for (const auto& [sign, label] : {std::pair<double, const char*>{+1.0, "toward"}, std::pair<double, const char*>{-1.0, "away"}}) {
        const auto right = di_first_coeffs(Variant::Right, sign, {a_ref, b_ref});
        for (const Variant variant : {Variant::Without, Variant::Reversed}) {
            const auto wrong = di_first_coeffs(variant, sign, {a_ref, b_ref});
            double worst_abs = std::fabs(wrong[0][0] - right[0][0]);
            for (std::size_t k = 0; k < 3; ++k) {
                for (std::size_t o = 0; o < 2; ++o) {
                    if (k == 0 && o == 0) continue;
                    worst_abs = py_max(worst_abs, std::fabs(wrong[k][o] - right[k][o]));
                }
            }
            double& slot = variant == Variant::Without ? delta_without : delta_reversed;
            slot = py_max(slot, worst_abs);
            std::array<double, 5> per_size{};
            for (std::size_t i = 0; i < 5; ++i) per_size[i] = worst_abs / eps[i];
            std::size_t at = 0;
            for (std::size_t i = 1; i < 5; ++i) {
                if (per_size[i] > per_size[at]) at = i;   // list.index(max(...)): the first of the largest
            }
            std::string sizes = "[";
            for (std::size_t i = 0; i < 5; ++i) sizes += (i > 0 ? ", '" : "'") + ff(per_size[i], 0) + "'";
            sizes += "]";
            io.out << "    observer moving " << dk::pad_right(label, 6) << " the target: row " << dk::pad_right(variant == Variant::Without ? "without" : "reversed", 9) << ": max |wrong - right| = " << fe(worst_abs, 4)
                   << " rad/m = " << ff(largest(per_size), 0) << " eps (at h = " << ff(kSteps[at], 0) << " m)  [per size: " << sizes << "]\n";
        }
        io.out << "        the model's row (rad/m): ";
        const char* names[3] = {"x", "y", "z"};
        for (std::size_t k = 0; k < 3; ++k) io.out << (k > 0 ? ", " : "") << names[k] << ": (" << fe(right[k][0], 4) << ", " << fe(right[k][1], 4) << ")";
        io.out << '\n';
    }
    if (check) {
        bool ok = std::fabs(nu / dz.nu - 1) < 1e-4;
        io.out << "the frozen nu of this geometry reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = std::fabs(largest(eps) / dz.b_pred - 1) < 1e-3;
        for (std::size_t i = 0; i < 5; ++i) ok = ok && std::fabs(eps[i] / dz.eps[i] - 1) < 1e-3;
        io.out << "the frozen eps(h) and B_pred of this geometry reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = ceil3(1.05 * dz.fine_extra) == dz.f_extra || std::fabs(ceil3(1.05 * dz.fine_extra) / dz.f_extra - 1) < 1e-9;
        io.out << "the frozen F_extra is 1.05 x the fine scan, rounded up to three digits: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        ok = std::fabs(delta_without / dz.delta_without - 1) < 1e-3 && std::fabs(delta_reversed / dz.delta_reversed - 1) < 1e-3;
        io.out << "the frozen row differences (without A', A' reversed) reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
        // the literals are whole numbers of epsilon: agree to the rounding (0.6) or to 0.2 %, whichever is larger
        ok = true;
        for (std::size_t i = 0; i < 5; ++i) {
            ok = ok && std::fabs(delta_without / eps[i] - dz.power_without[i]) <= py_max(0.6, 2e-3 * dz.power_without[i]);
            ok = ok && std::fabs(delta_reversed / eps[i] - dz.power_reversed[i]) <= py_max(0.6, 2e-3 * dz.power_reversed[i]);
        }
        io.out << "the frozen predicted powers of the two controls (in eps, at the five sizes) reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
        bad += ok ? 0 : 1;
    }
    if (scan_flag) {
        const DiScan found = di_extra_scan({a_ref, b_ref});
        io.out << "series scan (10-degree grid, ends and centre, both signs): sup |full - pure| = (" << fe(found.sup_diff[0], 4) << ", " << fe(found.sup_diff[1], 4) << ") /rho^3;  sup |pure| = ("
               << ff(found.sup_pure[0], 4) << ", " << ff(found.sup_pure[1], 4) << "), worst ratio to F_pure(h; 30 deg) " << ff(found.worst_ratio, 12) << '\n';
        bool ok = found.worst_ratio <= 1.0 + 1e-9;
        io.out << "    the pure geometric formula at phi = 30 deg bounds the pure third derivatives of the scan (the north axis at the centre attains it exactly): " << (ok ? "yes" : "NO") << '\n';
        bad += ok ? 0 : 1;
        if (check) {
            ok = std::fabs(largest2(found.sup_diff) / dz.fine_extra - 1) < 1e-3;
            io.out << "    the frozen fine scan " << fe(dz.fine_extra, 4) << " reproduced: " << (ok ? "ok" : "NOT REPRODUCED") << '\n';
            bad += ok ? 0 : 1;
        }
        double full_bound = f_pure_at(kSteps[0], kDiElDeg) * dk::py_pow(kDiRho, 3) + f_extra;
        for (std::size_t i = 1; i < kSteps.size(); ++i) full_bound = py_max(full_bound, f_pure_at(kSteps[i], kDiElDeg) * dk::py_pow(kDiRho, 3) + f_extra);
        // the largest full third derivative over the scan stays below the largest F_a
        io.out << "    F_a (the largest of the five sizes) = " << ff(full_bound, 4) << " /rho^3 against the pure sup " << ff(largest2(found.sup_pure), 4) << '\n';
    }
    return bad;
}

}  // namespace

double f_pure(double h, double rho) { return 2.0 / (dk::py_pow(rho - h, 3) * dk::py_pow(dk::py_cos(kPhiMaxA + dk::py_asin(h / rho)), 3)); }

double f_pure_at(double h, double phi_deg, double rho) { return 2.0 / (dk::py_pow(rho - h, 3) * dk::py_pow(dk::py_cos(dk::py_radians(phi_deg) + dk::py_asin(h / rho)), 3)); }

double ceil3(double x) {
    const int e = static_cast<int>(std::floor(dk::py_log10(x)));
    const double sc = dk::py_pow(10.0, e - 2);   // (exact for e - 2 up to 22, where the Python's int is exact as well; for a negative exponent it is the float the Python's int ** negative int makes)
    return std::ceil(x / sc - 1e-9) * sc;
}

std::array<double, 3> unit_of_angles(double first_deg, double second_deg) {
    // the unit vector with 'right ascension' first_deg and 'declination' second_deg (for the local frame: azimuth from north and elevation: x = north, y = east, z = up)
    const double f = dk::py_radians(first_deg);
    const double d = dk::py_radians(second_deg);
    return {dk::py_cos(d) * dk::py_cos(f), dk::py_cos(d) * dk::py_sin(f), dk::py_sin(d)};
}

std::vector<std::array<double, 3>> sphere_half(double step_deg) { return sphere_half_directions(step_deg); }

// =====================================================================================================================================================================================================
// THE TOOL
// =====================================================================================================================================================================================================

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

const char kUsageText[] = "usage: measmod_fd_sizing [-h] [--scan] [--check] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "The finite-difference gate's sizing, SPEC-measmod.md section 6.2, as the numbers it freezes: the frozen sizing, the amended range sizing, the angle sizing and the diurnal-aberration check.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --scan       the exact series scan of the angle third derivatives and the stencil bounds of the amended range sizing\n"
    "  --check      exit 1 unless every number SPEC-measmod.md section 6.2 freezes is reproduced\n"
    "  --root ROOT  the tree whose data/cache holds the station files (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 printed (and, with --check, every frozen number reproduced)   1 a frozen number was not reproduced\n"
    "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        bool scan_flag = false;
        bool check = false;
        std::filesystem::path root = settings.root;
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
            if (opt == "--scan" && !has_value) {
                scan_flag = true;
            } else if (opt == "--check" && !has_value) {
                check = true;
            } else if (opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument --root: expected one argument");
                    value = argv[++i];
                }
                root = value;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        // the Python's own state: its default decimal context, and the precision measmod_reference set when it was imported (before any number of this tool is a Decimal)
        LocalContext fresh;
        fresh.context() = DecimalContext{};
        fresh.context().prec = mr::kPrecision;

        int bad = 0;
        for (const FrozenSizing& cs : settings.frozen.sizing) {
            const Sizing s = sizing(cs.rho, cs.rt, cs.range);
            io.out << "--- " << cs.name << ": rho = " << fg(cs.rho, 3) << " m, F = " << fg(s.f, 4) << ", nu = " << fg(s.nu, 3) << '\n';
            for (std::size_t i = 0; i < kSteps.size(); ++i) {
                io.out << "    h = " << fwf(kSteps[i], 6, 0) << "   T = " << fe(s.truncation[i], 2) << "   N = " << fe(s.noise[i], 2) << "   eps = " << fe(s.bound[i], 2) << '\n';
            }
            const double top = largest(s.bound);
            io.out << "    h* = " << ff(s.best_step, 0) << ", best = " << fe(s.best, 1) << ", B_pred = " << fg(top, 3) << ", window [" << fg(top / 10, 2) << ", " << fg(top * 10, 2) << "]\n";
            if (check) {
                const bool ok = std::fabs(top / cs.b_pred - 1) < 0.005 && std::fabs(s.best_step - cs.h_star) < 1.0;
                io.out << "    frozen B_pred " << fg(cs.b_pred, 3) << ", h* " << ff(cs.h_star, 0) << ": " << (ok ? "reproduced" : "NOT REPRODUCED") << '\n';
                bad += ok ? 0 : 1;
            }
        }
        const double machine = std::ldexp(1.0, -52);   // 2.0 ** -52
        io.out << "--- if only machine round-off mattered: eps^(2/3) = " << fe(dk::py_pow(machine, 2.0 / 3.0), 2) << '\n';
        if (scan_flag) {
            const std::array<double, 2> found = scan();
            const double bound = 2 / dk::py_pow(dk::py_cos(dk::py_radians(30.0)), 3);
            io.out << "--- series scan, |dec| <= 30 deg: max |f'''| RA " << ff(found[0], 4) << ", Dec " << ff(found[1], 4) << " (rho = 1); analytic RA bound 2/cos^3 = " << ff(bound, 4) << '\n';
            if (check) {
                const bool ok = found[0] <= bound * 1.001 && found[1] <= bound;
                io.out << "    the frozen angle coefficient 2/(rho^3 cos^3 phi_max) bounds both: " << (ok ? "yes" : "NO") << '\n';
                bad += ok ? 0 : 1;
            }
        }
        try {
            bad += amended(io, root, scan_flag, check, settings.frozen);
            bad += angle_report(io, scan_flag, check, settings.frozen);
            bad += diurnal_report(io, scan_flag, check, settings.frozen);
        } catch (const mr::MissingInput& exc) {
            io.err << "missing input: " << exc.what() << '\n';
            return kArgument;
        }
        if (check) io.out << (bad == 0 ? std::string("ok       every frozen number reproduced") : "FAILED   " + std::to_string(bad) + " frozen number(s) not reproduced") << '\n';
        return bad != 0 ? kFailed : kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::measmod_fd_sizing

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::measmod_fd_sizing::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
