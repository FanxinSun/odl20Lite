#pragma once
// tools/forcemodel_fd_sizing.hpp — the finite-difference gate of the force plugins' Jacobians: its sizing, as the numbers it freezes (SPEC-forcemodel.md §6.2; plan L0 step 8, group C7).
//
// `run` is the whole tool: `forcemodel_fd_sizing [--scan] [--check] [--root DIR]`.  Exit 0 printed (and, with --check, every frozen number reproduced), 1 a frozen number was not reproduced, 2 an argument error
// or the coefficient file is missing or unreadable, 70 an error the tool did not anticipate.
//
// The pieces: Lemma 1's constant S3, Lemma 2's Phi_nm (which tools/forcemodel_comparator_header.cpp prints into its header), the bound F of each force, the registered noise bounds nu, the gate's bound
// eps(h) = h^2 F / 6 + nu / h, the power of the gate against the three wrong rows, and the looseness scan with its truncated power series.  All floating point is the Python's, operation for operation.

#include "gradient_reference.hpp"

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/tool.hpp>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace odl::tools::forcemodel_fd_sizing {

using odl::devkit::BigInt;
using odl::tools::gradient_reference::Poly;

inline constexpr double kEps = 0x1p-52;   // 2.0 ** -52
inline constexpr std::array<double, 5> kHs = {10.0, 30.0, 100.0, 300.0, 1000.0};   // position steps, m
inline constexpr double kHMax = 1000.0;
inline constexpr double kGM = 3.986004415e14;       // the field's (EGM2008, TT-compatible)
inline constexpr double kAE = 6378136.3;
inline constexpr double kC = 299792458.0;
inline constexpr double kGmeRel = 3.986004418e14;   // relativity's own (TN36-1)
inline constexpr double kGMS = 1.32712442099e20;
inline constexpr double kJEarth = 9.8e8;

/// sum() of three floats as CPython 3.12 and later computes it: the first is added to the int 0, the others by Neumaier's improved Kahan-Babuska summation (the compensation of the bits each addition
/// loses), and the compensation is added at the end unless it is zero or not finite.
[[nodiscard]] double py_sum3(double a, double b, double c);

/// L4's four reference points (tests/l4_ranking.cpp): name, geocentric radius in m.
struct ReferencePoint {
    const char* name;
    double r;
    double era_deg;   ///< the Earth-rotation angle at the reference epoch, degrees
    double phase;     ///< the fixture's orbit phase, rad
};
[[nodiscard]] const std::array<ReferencePoint, 4>& reference_points();

/// Lemma 1's constant: sum_{j=0..3} C(3,j) d^(falling 3-j) (q)_j.
[[nodiscard]] BigInt S3(int d, int q);
/// The l1 norm of a polynomial's coefficients.
[[nodiscard]] odl::devkit::Rational l1(const Poly& poly);
/// max over the three axes of the l1 norm of the derivative of the polynomial along that axis (the "max_i ||d_i q||" of Lemma 2).
[[nodiscard]] odl::devkit::Rational max_l1_gradient(const Poly& poly);
/// Lemma 2's Phi_nm (n >= 2): exact polynomial algebra, one square root; 0.0 for the zero polynomial ('S' with m = 0).
[[nodiscard]] double phi(int n, int m, char kind);
/// Phi for the point mass: S3(1, 3) = 96.
[[nodiscard]] double phi_point_mass();

/// One line of the coefficient file: C and S (the file's D exponents read as E).
struct Coefficient {
    int n;
    int m;
    double c;
    double s;
};
/// EGM2008's tide-free C, S for the lines up to degree `nmax`, in the file's order (the file starts at degree 2); std::nullopt when the file does not exist.  Throws std::runtime_error for a file that cannot be
/// read or a line that is not a coefficient line.
[[nodiscard]] std::optional<std::vector<Coefficient>> coefficients(const std::filesystem::path& file, int nmax);

/// The registered bound for Gravity(N = nmax): the point mass and every coefficient listed, times 1.01 (the conventional substitutions' difference from the file).
[[nodiscard]] double f_gravity(int nmax, double r_min, const std::vector<Coefficient>& coefs);
[[nodiscard]] double f_third_body(double mu, double d_min);
/// a_S = (GM/c^2) [ 4 GM x_i r^-4 - v^2 x_i r^-3 + 4 (x.v) v_i r^-3 ],  beta = gamma = 1.
[[nodiscard]] double f_schwarzschild(double r_min, double v);
/// a_LT = (1+gamma)(GM/c^2) [ 3 (x.J)(x x v)_i r^-5 + (v x J)_i r^-3 ], J along z.
[[nodiscard]] double f_lense_thirring(double r_min, double v);

// the registered noise bounds
[[nodiscard]] double nu_gravity(double a_norm);
[[nodiscard]] double nu_third_body(double mu, double d, double s);
[[nodiscard]] double nu_relativity(double a_norm);
[[nodiscard]] double nu_tides(double a_norm);
[[nodiscard]] double eps_h(double f, double nu, double h);

/// The point-mass tensor's three wrong rows against eps at the best step, for one geometry.
struct ControlPower {
    std::string name;
    double th;                                          ///< the Earth-rotation angle, rad
    std::array<std::pair<std::string, double>, 3> row;   ///< unrotated, transposed, sign flipped
};
[[nodiscard]] std::vector<ControlPower> control_power(const std::vector<Coefficient>& coefs);

/// One row of the sizing table.
struct TableRow {
    std::string name;
    double r;
    double f;
    double nu;
    std::array<double, 5> e;
    double rel;
};
[[nodiscard]] std::vector<TableRow> table(const std::vector<Coefficient>& coefs);

// the looseness scan: a power series in t truncated after t^3, and the exact third derivative of a_i along a line
inline constexpr int kOrder = 4;
struct Ser {
    std::array<double, kOrder> c{};
    Ser() = default;
    Ser(std::initializer_list<double> values);   // Ser([...]): the values, then zeros
};
[[nodiscard]] Ser operator+(const Ser& a, const Ser& b);
[[nodiscard]] Ser operator*(const Ser& a, const Ser& b);
/// (g(t))^alpha for g(0) > 0, by the standard recurrence.
[[nodiscard]] Ser power_series(const Ser& g, double alpha);
/// The polynomial along the line x0 + t e as a series.
[[nodiscard]] Ser poly_on_line(const Poly& poly, const std::array<double, 3>& x0, const std::array<double, 3>& e);

/// One term of the field: coef, the degree n, the polynomial q, its three derivatives, and s = 2n + 1.
struct FieldTerm {
    double coef;
    int n;
    Poly q;
    std::array<Poly, 3> dq;
    int s;
};
[[nodiscard]] std::vector<FieldTerm> field_terms(const std::vector<Coefficient>& coefs);
/// d_e^3 a_i at x0, for the field sum of terms.
[[nodiscard]] double third_derivative_a(const std::vector<FieldTerm>& terms, const std::array<double, 3>& x0, const std::array<double, 3>& e, int i);
/// The supremum of the TRUE |d_e^3 a_i| of the field (point mass included) over a grid of positions on the sphere of radius r, directions e, components i, and the three stencil points (-h, 0, +h) at h = 1000 m.
[[nodiscard]] double scan(int nmax, double r, const std::vector<Coefficient>& coefs, int grid_lat = 5, int grid_lon = 6, int grid_dir = 6);

/// What the tool freezes and where it reads: a test damages a frozen number to show the MISMATCH, or raises the floor of the controls' power.
struct Frozen {
    int d;
    int q;
    long long want;
};
struct Settings {
    std::filesystem::path root;
    std::vector<Frozen> s3 = {{1, 3, 96}, {1, 4, 180}, {2, 5, 420}, {0, 3, 60}, {3, 7, 1140}, {1, 5, 300}};
    double phi20_scale = 12000.0;   ///< Phi_20 = sqrt(5) * 12000, derived by hand
    double power_floor = 1e3;       ///< the registered requirement of the controls' predicted power
};
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.  `run_on` does the work of `settings` (--root overrides its root); `run` is `run_on` the real ones.
[[nodiscard]] int run_on(Settings settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
[[nodiscard]] std::filesystem::path default_root();

}  // namespace odl::tools::forcemodel_fd_sizing
