#pragma once
// tools/measmod_fd_sizing.hpp — the finite-difference gate's sizing, SPEC-measmod.md §6.2, as the numbers it freezes (plan L0 step 8, group C8): the frozen sizing, the amended range sizing, the angle sizing and the
// diurnal-aberration check, the truncated power series they scan with, and the 60-digit guards that check the series.
//
// `run` is the whole tool: `measmod_fd_sizing [--scan] [--check] [--root DIR]`.  Exit 0 printed (and, with --check, every frozen number reproduced), 1 a frozen number was not reproduced, 2 an argument error or an
// input file that is missing, cannot be read or is not the shape expected, 70 an error the tool did not anticipate.

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/tool.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace odl::tools::measmod_fd_sizing {

/// The five step sizes of the gate, metres.
inline constexpr std::array<double, 5> kSteps = {10.0, 30.0, 100.0, 300.0, 1000.0};

// ---- the frozen sizing -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

/// The sizing of one geometry as the Python's sizing() returns it: the noise bound nu, the truncation coefficient F, the three error columns at the five steps, the best step and its bound.
struct Sizing {
    double nu;
    double f;
    std::array<double, 5> truncation;
    std::array<double, 5> noise;
    std::array<double, 5> bound;
    double best_step;
    double best;
};

/// `range` is the geometry of a range (F = 1.1547005383792515 / rho^2) and else of an angle (F = 2 / (rho^3 cos^3 phi_max)); `rt` is the magnitude of the target's position.
[[nodiscard]] Sizing sizing(double rho, double rt, bool range, double phi_max_deg = 40.0);

// ---- the truncated power series (order 4: the third derivative is 6 times the coefficient of h^3) ---------------------------------------------------------------------------------------------------------

inline constexpr std::size_t kOrder = 4;

/// The Python's class S: c[0] + c[1] h + c[2] h^2 + c[3] h^3, with every operation the Python's, floating-point operation for floating-point operation (a float beside a series is the series [float, 0, 0, 0]).
struct Series {
    std::array<double, kOrder> c{};
    Series() = default;
    explicit Series(double constant) { c[0] = constant; }
    Series(double c0, double c1) {
        c[0] = c0;
        c[1] = c1;
    }
    explicit Series(const std::array<double, kOrder>& coefficients) : c(coefficients) {}
    [[nodiscard]] Series inv() const;
    [[nodiscard]] Series sqrt() const;
    [[nodiscard]] Series deriv() const;
    [[nodiscard]] Series integ(double c0) const;
};
[[nodiscard]] Series operator+(const Series& a, const Series& b);
[[nodiscard]] Series operator-(const Series& a, const Series& b);
[[nodiscard]] Series operator*(const Series& a, const Series& b);
[[nodiscard]] Series operator/(const Series& a, const Series& b);
[[nodiscard]] Series operator+(const Series& a, double b);
[[nodiscard]] Series operator+(double a, const Series& b);
[[nodiscard]] Series operator-(const Series& a, double b);
[[nodiscard]] Series operator-(double a, const Series& b);
[[nodiscard]] Series operator*(const Series& a, double b);
[[nodiscard]] Series operator*(double a, const Series& b);
[[nodiscard]] Series operator/(const Series& a, double b);
[[nodiscard]] Series operator/(double a, const Series& b);

/// arctan of a series: (q' / (1 + q^2)) integrated from atan(q_0).
[[nodiscard]] Series atan_series(const Series& q);

/// f''' of right ascension and declination, at rho = 1, for the line of sight g displaced along e (g_x > 0).
[[nodiscard]] std::array<double, 2> third_derivatives(const std::array<double, 3>& g, const std::array<double, 3>& e);

/// The exact series scan of the angle third derivatives for |declination| <= phi_deg: the largest |f'''| of right ascension and of declination.
[[nodiscard]] std::array<double, 2> scan(double phi_deg = 30.0);

// ---- series on three-vectors (the angle sizing's, and the diurnal check's, model of the reduction) -------------------------------------------------------------------------------------------------------

using Vec3 = std::array<double, 3>;
using SVec3 = std::array<Series, 3>;

/// A(n; beta) of MEAS-R-053, the aberration, on a series vector n and a float vector beta: [n sqrt(1 - beta^2) + (1 + (n.beta) / (1 + sqrt(1 - beta^2))) beta] / (1 + n.beta); D(n; beta) is A(n; -beta).
[[nodiscard]] SVec3 aberrate(const SVec3& n, const Vec3& beta);
/// The azimuth of a series direction (x north, y east, z up), by the branch that stays away from the singularity: arctan(y / x) for |x| >= |y| (the constant terms), else +-pi/2 - arctan(x / y).
[[nodiscard]] Series az_series_any(const SVec3& n);
/// The target's velocity v_hat (in units of c) and acceleration a_hat (in rho/c^2) of the angle scan's scenario, for a line of sight `los` (a unit vector): the speed 7.5 km/s across the line of sight and the third
/// axis (radial = 0), along the line of sight (radial = 1), or a mixture (sqrt(1 - radial^2) across, radial along); and 7.7 m/s^2 toward the geocentre (GM / (7.2e6 m)^2 over a range of 1.2e6 m).
[[nodiscard]] std::pair<Vec3, Vec3> scenario_vectors(const Vec3& los, double radial);

// ---- the amended range sizing -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

inline constexpr double kLight = 299792458.0;   // m/s
[[nodiscard]] double v_station_max();           // the equatorial surface speed, m/s
[[nodiscard]] double f_geo(double h, double rho, double speed);
/// The chain's bound of one evaluation of the range output (and |g . east|): three ulp of the angle at the axis distance, times the line of sight's east component, times 1/2 (event 2) or 1 (event 1).
[[nodiscard]] std::pair<double, double> nu_chain(int event, double r_perp, double el_deg, double az_deg);
[[nodiscard]] double eps_amended(std::size_t step, const std::array<double, 5>& f_trop, double rho, double speed, double nu);
/// TN36 eq. (9.9)'s mapping function in double precision (cphi is cos(latitude)).
[[nodiscard]] double fcula(double s, double cphi, double height, double t_c);
/// The zenith delay of TN36 eqs (9.3)-(9.7) in double precision.
[[nodiscard]] double zenith_total(double lat_rad, double height, double ps, double es, double lam);

/// The geodetic station inputs of the first normal point: the axis distance, the latitude and the height.
struct StationInputs {
    double r_perp, lat, height;
};

/// The atmosphere and the station of the amended sizing: axis distance, latitude, height, temperature in Celsius, the zenith delay.
struct Atmosphere {
    double r_perp, lat, height, t_c, z;
};

/// The stencil bound of the troposphere and Shapiro terms: the largest |third derivative| along any displacement and at each step (and of the Shapiro term alone, of the atmosphere alone).
struct TropScan {
    std::array<double, 5> sup;
    double sup_shapiro;
    double sup_atmosphere;
};
[[nodiscard]] TropScan trop_shapiro_scan(double rho, double el_deg, const Atmosphere& atm, double step_theta = 3.0, double step_phi = 4.0, double delta = 2000.0);

/// The largest ratio over the cone of the closed-form two-way range's third derivative, per step (60 digits).
[[nodiscard]] std::array<double, 5> coupling_scan(double rho, double el_deg, double az_deg, double speed, double radial, int event, double delta = 100.0);

// ---- the angle sizing and the diurnal check ----------------------------------------------------------------------------------------------------------------------------------------------------------------

/// eraRefco's A and B in 60 digits for the doubles passed (the optical case).
[[nodiscard]] std::pair<double, double> refco(double p_hpa, double t_c, double rh, double wl_um);
/// ERFA's own published case (800 hPa, 10 C, 90 %, 0.4 um) against its A and B, in units of ERFA's tolerances (1e-15 and 1e-18): the larger of the two.
[[nodiscard]] double refco_check(double a_erfa = 0.2264949956241415009e-3, double b_erfa = -0.2598658261729343970e-6);
/// The Decimal FCULa (60 digits) at the IERS FCUL_A prolog's printed case, minus the printed value (the text `printed`).  In the current Decimal context: the tool sets 60 digits.
[[nodiscard]] double iers_fcula_check(const char* printed = "3.800243667312344087");
/// F_pure(h) = 2 / [(rho - h)^3 cos^3(phi_max + asin(h/rho))], in m^-3 rad, at phi_max = 40 degrees (rho = 1.2e6 m unless given).
[[nodiscard]] double f_pure(double h, double rho = 1.2e6);
[[nodiscard]] double f_pure_at(double h, double phi_deg, double rho = 1.2e6);
/// The Python's ceil3: x rounded UP to three significant digits (with the 1e-9 allowance of its comparison).
[[nodiscard]] double ceil3(double x);
/// The unit vector with `first_deg` as right ascension (or azimuth) and `second_deg` as declination (or elevation).
[[nodiscard]] std::array<double, 3> unit_of_angles(double first_deg, double second_deg);
/// Unit displacement directions on a half-sphere at a step in degrees.
[[nodiscard]] std::vector<std::array<double, 3>> sphere_half(double step_deg);

/// The station and the atmosphere of the first normal point as the amended sizing uses them: from the registry section of measmod_reference (the station) and the weather of record 20 of the normal-point file.
[[nodiscard]] StationInputs station_inputs(const std::filesystem::path& root);
[[nodiscard]] Atmosphere atmosphere_inputs(const std::filesystem::path& root);

// ---- the numbers SPEC-measmod.md section 6.2 freezes, which `--check` reproduces ----------------------------------------------------------------------------------------------------------------------------
// They are the Python's literals, here fields of `Settings` (as in tools/forcemodel_fd_sizing): the defaults are the frozen figures and the tool is run on them; a test damages one and sees its check fire, at the check's
// own tolerance.  Some of them are also the INPUT of what is printed (a geometry's rho, F_trop in the modes without `--scan`, F_extra of a group): damaging those changes the table as well.

/// One geometry of the frozen sizing: its name, rho and |r_target| (inputs), range or angle, and the frozen B_pred and h*.
struct FrozenSizing {
    const char* name;
    double rho, rt;
    bool range;
    double b_pred, h_star;
};

/// One geometry of the amended range sizing: the stencil bound F_trop(sup) written down, the chain's bound of one evaluation for each epoch event, eps(h) for the rigid configuration and for the real chain in each
/// event, and B_pred of each.
struct FrozenAmended {
    std::array<double, 5> f_trop;
    double nu_chain_2, nu_chain_1;
    std::array<double, 5> eps_rigid, eps_real2, eps_real1;
    double b_rigid, b_real2, b_real1;
};

/// One group of the angle sizing: the fine scan's supremum, F_extra / rho^3 (1.05 x the fine scan, rounded up to three digits), eps(h) and B_pred.
struct FrozenAngleGroup {
    const char* name;
    double fine, f_extra;
    std::array<double, 5> eps;
    double b_pred;
};

/// The diurnal-aberration check: nu, the fine scan's F_extra and its frozen form, eps(h), B_pred, the two controls' largest row differences and their predicted powers at the five sizes.
struct FrozenDiurnal {
    double nu = 2.9944e-15;
    double fine_extra = 2.0815e-02;
    double f_extra = 2.19e-2;
    std::array<double, 5> eps = {3.2936e-16, 3.6904e-16, 3.0221e-15, 2.6961e-14, 3.0028e-13};
    double b_pred = 3.0028e-13;
    double delta_without = 1.4928e-12;    // rad/m: the largest |row without A' - row|, any output, axis, either sign of the motion
    double delta_reversed = 2.9857e-12;   // rad/m: the same for A'(.; -beta)
    std::array<double, 5> power_without = {4533.0, 4045.0, 494.0, 55.0, 5.0};   // |delta|/eps at the five sizes
    std::array<double, 5> power_reversed = {9065.0, 8090.0, 988.0, 111.0, 10.0};
};

struct Frozen {
    std::array<FrozenSizing, 3> sizing = {{{"range, LAGEOS-like", 6.6e6, 1.227e7, true, 4.42e-9, 86.0},
                                           {"range, LEO-like", 1.5e6, 6.92e6, true, 8.55e-8, 25.0},                      // target at 6.92e6 m: elevation 15 deg at rho 1.5e6 m
                                           {"angle, LEO-like, |phi| <= 40 deg", 1.2e6, 7.2e6, false, 4.29e-13, 18.0}}};   // target at 7.2e6 m: elevation 40 deg at rho 1.2e6 m
    /// the amended range sizing's two geometries: LAGEOS-like, then LEO-like
    std::array<FrozenAmended, 2> amended = {{{{9.6904e-20, 9.6905e-20, 9.6910e-20, 9.6924e-20, 9.6973e-20},
                                               3.7910e-08,
                                               7.5821e-08,
                                               {5.5924e-10, 1.9024e-10, 1.0007e-10, 4.1633e-10, 4.4255e-09},
                                               {4.3503e-09, 1.4539e-09, 4.7917e-10, 5.4270e-10, 4.4634e-09},
                                               {8.1413e-09, 2.7176e-09, 8.5827e-10, 6.6907e-10, 4.5013e-09},
                                               4.4255e-09,
                                               4.4634e-09,
                                               8.1413e-09},
                                              {{7.7299e-16, 7.7314e-16, 7.7365e-16, 7.7512e-16, 7.8028e-16},
                                               2.2981e-07,
                                               4.5961e-07,
                                               {2.8796e-10, 1.7024e-10, 8.8481e-10, 7.7232e-09, 8.5794e-08},
                                               {2.3269e-08, 7.8305e-09, 3.1829e-09, 8.4892e-09, 8.6024e-08},
                                               {4.6249e-08, 1.5491e-08, 5.4809e-09, 9.2553e-09, 8.6253e-08},
                                               8.5794e-08,
                                               8.6024e-08,
                                               8.6253e-08}}};
    /// the angle sizing's four groups: geometric, astrometric, refracted, ofdate (whose fine scan is that of astrometric: 0 here)
    std::array<FrozenAngleGroup, 4> angle_groups = {{{"geometric", 2.495171e-04, 2.62e-4, {5.4220e-16, 5.5271e-16, 4.3433e-15, 3.8693e-14, 4.3112e-13}, 4.3112e-13},
                                                     {"astrometric", 1.358137e-03, 1.43e-3, {5.4221e-16, 5.5281e-16, 4.3444e-15, 3.8703e-14, 4.3124e-13}, 4.3124e-13},
                                                     {"refracted", 9.271765e-02, 9.74e-2, {5.4314e-16, 5.6114e-16, 4.4370e-15, 3.9536e-14, 4.4049e-13}, 4.4049e-13},
                                                     {"ofdate", 0.0, 1.43e-3, {5.4221e-16, 5.5281e-16, 4.3444e-15, 3.8703e-14, 4.3124e-13}, 4.3124e-13}}};
    std::array<double, 5> angle_agreement = {1.0007e-15, 3.3494e-16, 1.0194e-16, 3.5370e-17, 1.2073e-17};   // 2 nu_a/h + 3 ds/(rho(1 - kappa) - h)^2, ds = 1e-6 m
    double angle_nu_a = 4.9928e-15;
    std::pair<double, double> angle_refraction = {2.484383e-04, -3.123796e-07};   // A and B of the first normal point's weather
    FrozenDiurnal diurnal;
    // what the checks of published and predicted values compare with
    const char* iers_fcula = "3.800243667312344087";   // the IERS FCUL_A prolog's printed value at its test case (lat 30.67166667 deg, H 2075 m, T 300.15 K, elevation 15 deg)
    double chain_ulp = 1.59e-7;                          // m: one ulp of the Earth-rotation angle at the station, predicted before it was measured
    double erfa_a = 0.2264949956241415009e-3;            // ERFA's published eraRefco case (t_erfa_c.c: 800 hPa, 10 C, 90 %, 0.4 um): A ...
    double erfa_b = -0.2598658261729343970e-6;           // ... and B
};

// ---- the tool -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Settings {
    std::filesystem::path root;
    Frozen frozen;
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::measmod_fd_sizing
