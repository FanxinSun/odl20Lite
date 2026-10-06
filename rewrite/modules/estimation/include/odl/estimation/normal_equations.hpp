#pragma once
// odl/estimation/normal_equations.hpp — the batch estimator: weighted linear least squares by NORMAL EQUATIONS SCALED BY DEFAULT
// (SPEC-estimation.md v0.3, L7 step 2; the manager's rulings R6 – R9 of 2026-10-06).
//
// WHAT THE SCALING IS FOR, stated first because the plan's own reason for it was corrected.  DEM89 (Demmel, LAPACK Working Note 14) proves that
// the UNSCALED Cholesky solution already meets the scaled error bound; with exact powers of two the two are the same computation bit for bit
// (EST-R-201, measured).  So the scaling cannot change what a solve returns.  It changes every DECISION read from the matrix: the condition
// number, the observability, the per-parameter diagnostics, the correlations — each of which moves by orders of magnitude with a parameter's
// unit when read from the unscaled matrix and does not when read from the scaled one.  That is why there is no unscaled path in this interface:
// nothing here factors or solves an unscaled matrix, and no option turns the scaling off.
//
// EVERY NUMBER A REFUSAL RESTS ON IS RETURNED (EstimationRefusal), and a solve whose DERIVED error bound certifies not even one digit is
// refused (EST-R-206): a returned value is never accompanied by a warning the caller may ignore.

#include <odl/core/result.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace odl::estimation {

/// The exact scale exponent of EST-R-201: with `diagonal = m 2^k`, `m` in [1/2, 1), the exponent is floor(k/2); the scaled diagonal
/// `diagonal * 2^(-2e)` lies in [1/2, 2).  The diagonal must be finite and positive.
[[nodiscard]] int scale_exponent(double diagonal) noexcept;

/// What a solution (or a refusal) reports about the matrix, in SCALED variables (EST-R-207).  Vectors are indexed by parameter.
struct ConditionReport {
    double condition_estimate = 0.0;                 ///< kappa_est = lambda_N lambda_Q, from below: <= kappa_2(N_s)
    double condition_upper = 0.0;                    ///< kappa_up = ||N_s||_1 ||N_s^-1||_1, from above: >= kappa_2(N_s); 0 when no inverse exists
    std::vector<double> parameter_conditions;        ///< kappa_i = N_ii (N^-1)_ii = (N_s)_ii (Q_s)_ii (unit-free, GEO1 10.5); NaN for an eliminated parameter
    std::vector<double> weakest_direction;           ///< unit 2-norm, scaled variables, largest-magnitude component positive; 0 for eliminated parameters
};

/// A refusal: the specification's own identifier (a literal EST-F-2nn), a message that names the offending values, the parameter it names,
/// the number the reason rests on, and whatever condition report could be computed.
struct EstimationRefusal : odl::Diagnostic {
    std::string parameter;                           ///< the parameter named; "" when the message names several or none
    double value = 0.0;                              ///< F-201: the diagonal; F-202: the pivot; F-203: B; F-205: nu; F-206: the diagonal
    ConditionReport condition;

    EstimationRefusal(std::string_view refusal_id, std::string text, std::string parameter_name = {}, double number = 0.0,
                      ConditionReport report = {})
        : odl::Diagnostic(refusal_id, std::move(text)),
          parameter(std::move(parameter_name)),
          value(number),
          condition(std::move(report)) {}
};

/// The one hatch (EST-R-209): a named parameter held at a stated value.
struct Elimination {
    std::string name;
    double value = 0.0;
};

struct SolveOptions {
    std::vector<Elimination> eliminate;
};

/// The normal equations as formed (EST-R-203): `matrix` is N-hat, n x n, full symmetric, row-major; `rhs` is b-hat.
struct FormedSystem {
    std::size_t n = 0;
    std::vector<double> matrix;
    std::vector<double> rhs;
};

struct Solution {
    std::vector<std::string> names;
    std::vector<double> x;                           ///< the correction in the parameters' own units; eliminated parameters at their stated values
    std::vector<unsigned char> eliminated;           ///< 1 for an eliminated parameter
    std::vector<int> scale_exponents;                ///< e_i; 0 for an eliminated parameter
    std::vector<std::size_t> free_index;             ///< the free parameters' indices, ascending
    std::vector<double> scaled_matrix;               ///< N_s over the FREE parameters, n_free x n_free row-major, full symmetric
    std::vector<double> scaled_rhs;                  ///< b_s
    std::vector<double> scaled_solution;             ///< y = S^-1 x
    std::vector<double> scaled_cofactor;             ///< Q_s = N_s^-1, exactly symmetric
    std::vector<double> cofactor;                    ///< Q = N^-1 in the parameters' units, n x n row-major, exactly symmetric; zero rows and columns for eliminated parameters
    ConditionReport condition;
    double certified_relative_error = 0.0;           ///< B of EST-R-206: the scaled relative error this solve certifies (< 1/2 or it is refused)
    std::size_t observations = 0;                    ///< m
    std::size_t free_parameters = 0;                 ///< n_free
    std::ptrdiff_t degrees_of_freedom = 0;           ///< nu = m - n_free
    std::optional<double> weighted_rss;              ///< Omega-hat; empty for a system built without a residual square sum
    std::string provenance;                          ///< EST-R-212

    /// sigma_0^2 = Omega-hat / nu (EST-F-205 when nu <= 0 or no residual information).
    [[nodiscard]] odl::Result<double, EstimationRefusal> sigma0_squared() const;
    /// The FORMAL covariance: sigma_0 = 1, the weights as stated.  Q.
    [[nodiscard]] std::vector<double> formal_covariance() const;
    /// The covariance SCALED by the fit's own variance of unit weight: sigma_0^2 Q (EST-F-205 when sigma_0^2 is unavailable).  Never a substitute for the formal one.
    [[nodiscard]] odl::Result<std::vector<double>, EstimationRefusal> scaled_covariance() const;
    /// The correlation from the SCALED cofactor (unit-free by construction), clamped to [-1, 1]; 1 on the diagonal; 0 against an eliminated parameter.
    [[nodiscard]] double correlation(std::size_t i, std::size_t j) const;
};

class NormalEquations {
public:
    /// An estimator over the named parameters (non-empty, unique names).
    [[nodiscard]] static odl::Result<NormalEquations, EstimationRefusal> create(std::vector<std::string> names);

    /// SYSTEM MODE (EST-R-213): an accumulated normal system (the LOWER triangle of `matrix`, n x n row-major, is read; `rhs`), for callers
    /// who hold one.  `observations` is m for the degrees of freedom; `residual_square_sum` is sum r^2 when the caller has it.
    [[nodiscard]] static odl::Result<NormalEquations, EstimationRefusal>
    from_normal_system(std::vector<std::string> names, std::span<const double> matrix, std::span<const double> rhs,
                       std::size_t observations, std::optional<double> residual_square_sum = std::nullopt);

    /// One scalar observation row: the partials, the residual (observed minus computed) and its standard deviation sigma > 0, which is never
    /// defaulted (EST-R-202).  Whitened on entry: fl(a/sigma), fl(r/sigma).
    [[nodiscard]] odl::Result<void, EstimationRefusal> add(std::span<const double> row, double residual, double sigma);

    [[nodiscard]] std::size_t parameters() const noexcept { return names_.size(); }
    [[nodiscard]] std::size_t observations() const noexcept { return system_mode_ ? system_observations_ : residuals_.size(); }
    [[nodiscard]] const std::vector<std::string>& names() const noexcept { return names_; }

    /// N-hat and b-hat as EST-R-203 forms them (no elimination).
    [[nodiscard]] odl::Result<FormedSystem, EstimationRefusal> formed_system() const;

    /// The solve, EST-R-201 … 212.  The only path: scale, factor the SCALED matrix, solve, unscale.
    [[nodiscard]] odl::Result<Solution, EstimationRefusal> solve(const SolveOptions& options = {}) const;

private:
    explicit NormalEquations(std::vector<std::string> names) : names_(std::move(names)) {}

    std::vector<std::string> names_;
    // rows mode
    std::vector<double> rows_;                       ///< m x n, whitened, row-major, in the order added
    std::vector<double> residuals_;                  ///< whitened
    // system mode
    bool system_mode_ = false;
    std::vector<double> system_matrix_;
    std::vector<double> system_rhs_;
    std::size_t system_observations_ = 0;
    std::optional<double> system_residual_square_sum_;
};

}  // namespace odl::estimation
