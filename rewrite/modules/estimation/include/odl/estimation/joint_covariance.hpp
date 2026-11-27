#pragma once
// odl/estimation/joint_covariance.hpp — the joint covariance over the state and the registered parameters, and the two-parameter confidence regions
// (SPEC-estimation.md v0.6, L7 step 5; the manager's ruling R15 of 2026-10-06; EST-Q-009 ruled 2026-11-27).
//
// NOTHING NEW IS SOLVED HERE.  Step 5 reads what step 2's `Solution` already holds (the cofactor Q in the parameters' units, the scaled cofactor Q_s, Omega-hat and nu) and adds no
// factorisation: a 2 x 2 inverse is its adjugate over its determinant, and the eigen-structure of a 2 x 2 is closed-form.  The module stays `core`-only (EST-Q-008): it takes
// NAMES, never a dyn::ParameterId; the integration test builds the layout from L3's registry in issue order.
//
// FORMAL AND SCALED ARE NEVER CONFLATED (EST-R-505): the mode is an explicit argument with no default.  known_sigma0 -> the formal covariance (sigma_0^2 Q) and chi^2_2 = -2 ln(1-p);
// estimated_sigma0 -> the scaled covariance (sigma-hat_0^2 Q) and 2F_p(2, nu) = nu((1-p)^(-2/nu) - 1).  c^2 = 1 is the 39.35 % contour in two dimensions, not the 68.27 % one.
//
// THE REGION IS FIRST-ORDER (EST-R-506): exact for a linear model, no better than the linearisation for a nonlinear one; no claim is made about an orbit fit's region here.

#include <odl/estimation/normal_equations.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace odl::estimation {

enum class ConfidenceMode { known_sigma0, estimated_sigma0 };

/// chi^2_2(p) = -2 log1p(-p), p in the OPEN interval (0, 1) (EST-R-501, EST-F-501).
[[nodiscard]] odl::Result<double, EstimationRefusal> chi2_2_critical(double p);
/// 2F_p(2, nu) = nu expm1(-(2/nu) log1p(-p)), p in (0, 1), nu >= 1.  This is 2F, not F: F is half of it.
[[nodiscard]] odl::Result<double, EstimationRefusal> two_f_2_nu_critical(double p, std::ptrdiff_t nu);
/// chi^2_2(p) in the known mode, 2F_p(2, nu) in the estimated mode (nu is read in the estimated mode only).
[[nodiscard]] odl::Result<double, EstimationRefusal> confidence_critical_value(ConfidenceMode mode, double p, std::ptrdiff_t nu);
/// The tail forms (EST-R-507), alpha = 1 - p in (0, 1): -2 ln(alpha), and nu expm1(-(2/nu) ln(alpha)).
[[nodiscard]] odl::Result<double, EstimationRefusal> chi2_2_critical_from_tail(double alpha);
[[nodiscard]] odl::Result<double, EstimationRefusal> two_f_2_nu_critical_from_tail(double alpha, std::ptrdiff_t nu);

/// The marginal two-parameter ellipse (EST-R-502): (theta - centre)^T C^-1 (theta - centre) <= level.
struct ConfidenceEllipse2D {
    std::string name_i, name_j;
    ConfidenceMode mode = ConfidenceMode::known_sigma0;
    double p = 0.0, level = 0.0;                     ///< the coverage p and c^2
    std::array<double, 2> centre{};                  ///< theta-hat_ij
    std::array<double, 3> covariance{};              ///< C_ii, C_ij, C_jj of the MODE
    double semi_major = 0.0, semi_minor = 0.0;       ///< a >= b, sqrt(lambda c^2)
    double angle = 0.0;                              ///< of the major axis from the i axis, radians, in (-pi/2, pi/2]

    [[nodiscard]] double quadratic(std::array<double, 2> displacement) const noexcept;   ///< v^T C^-1 v
    [[nodiscard]] bool contains(std::array<double, 2> point) const noexcept;             ///< quadratic(point - centre) <= level
    [[nodiscard]] std::array<double, 2> boundary_point(double t) const noexcept;         ///< centre + R(angle) (a cos t, b sin t)
};

/// The ellipse of the pair (name_i, name_j) of `solution` for `mode` and level `p`.  `known_sigma0` (finite, > 0) multiplies the formal block in the known mode only.
/// EST-F-501 (p, nu, sigma_0), EST-F-502 (an unknown, repeated or eliminated parameter), EST-F-503 (no sigma-hat_0^2), EST-F-504 (a degenerate block).
[[nodiscard]] odl::Result<ConfidenceEllipse2D, EstimationRefusal>
two_parameter_ellipse(const Solution& solution, const std::string& name_i, const std::string& name_j, ConfidenceMode mode, double p, double known_sigma0 = 1.0);

/// The joint parameter vector: the six state parameters (position then velocity), then the force parameters in the order L3's registry issued them (EST-R-503).
struct JointParameterLayout {
    std::array<std::string, 6> state_names;
    std::vector<std::string> force_names;
    /// EST-F-505: a state name or force name empty, or a name repeated.
    [[nodiscard]] static odl::Result<JointParameterLayout, EstimationRefusal> create(std::array<std::string, 6> state, std::vector<std::string> forces);
    [[nodiscard]] std::vector<std::string> all() const;
};

/// The report of EST-R-504, over the whole parameter vector in layout order.
struct JointCovariance {
    std::vector<std::string> names;
    std::vector<double> formal;                      ///< Q, n x n row-major
    std::optional<std::vector<double>> scaled;       ///< sigma-hat_0^2 Q; absent when nu <= 0 or the solution carries no residual square sum
    std::vector<double> correlation;                 ///< from the SCALED cofactor, unit-free, n x n; 1 on the diagonal
    std::vector<unsigned char> eliminated;           ///< 1 for an eliminated parameter (its row and column of Q are exactly zero)

    /// sqrt(C_ii - 2 C_ij + C_jj) (BRN52 8.3.2) of the mode (the known mode with sigma_0 = 1).  EST-F-502, -503, -504.
    [[nodiscard]] odl::Result<double, EstimationRefusal> difference_sd(const std::string& i, const std::string& j, ConfidenceMode mode) const;
};

/// EST-R-503 / -504: the layout must equal `solution.names` element for element (EST-F-505).
[[nodiscard]] odl::Result<JointCovariance, EstimationRefusal> joint_covariance(const Solution& solution, const JointParameterLayout& layout);

}  // namespace odl::estimation
