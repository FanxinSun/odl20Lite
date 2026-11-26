#pragma once
// odl/estimation/prior.hpp — the a priori constraint, isotropic and RTN (SPEC-estimation.md v0.5, L7 step 4; the manager's rulings R13 and R14 of 2026-10-06).
//
// A CONSTRAINT IS INFORMATION THE DATA DID NOT GIVE: a Gaussian prior with centre x_A and information matrix Lambda.  It is read as BRN52 §7.5 reads it, as pseudo-observations —
// rows (L, d) with L^T L = Lambda — that enter the normal equations exactly as data rows do (EST-R-203) and therefore BEFORE the scaling of EST-R-201 (EST-R-402).
//   isotropic:  Lambda = diag(sigma_i^-2) over the block's named parameters;
//   RTN:        Lambda = B^T diag(sigma_R^-2, sigma_T^-2, sigma_N^-2) B, B the matrix whose ROWS are e_R, e_T, e_N (FRAME-R-040: the matrix frames::rtn_basis returns),
//               over the three inertial components of one vector; B is computed once from the a priori state by the CALLER and held fixed (§3.7).
// A hard fix is the elimination of EST-R-209, never sigma -> 0.  This module does not depend on `frames`: the caller passes the nine doubles (EST-Q-008, ruled).
//
// The prior reaches the damped method (EST-R-404) as rows appended to the model's, so that the objective the gain ratio sees is the whole F_tot = (1/2)|r|^2 + (1/2)(x - x_A)^T Lambda (x - x_A);
// levenberg_marquardt.cpp is not changed.

#include <odl/estimation/levenberg_marquardt.hpp>
#include <odl/estimation/normal_equations.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace odl::estimation {

/// One block of a prior.  Built by the two factories, which validate (EST-R-401, EST-F-401 .. EST-F-403); the data members are public to be read, and every consumer validates again.
struct PriorBlock {
    std::vector<std::string> names;                  ///< isotropic: any number (>= 1); RTN: exactly three (the inertial x, y, z of one vector, in that order)
    std::vector<double> centre;                      ///< x_A, one per name, in the parameters' own units
    std::vector<double> sigma;                       ///< isotropic: one per name; RTN: (sigma_R, sigma_T, sigma_N)
    std::optional<std::array<double, 9>> basis;      ///< RTN: row-major, rows e_R, e_T, e_N (FRAME-R-040)

    /// An isotropic block: Lambda = diag(sigma^-2).  One sigma per name.
    [[nodiscard]] static odl::Result<PriorBlock, EstimationRefusal> isotropic(std::vector<std::string> names, std::vector<double> centre, std::vector<double> sigma);
    /// An RTN block over three named parameters; `basis` is row-major with the rows e_R, e_T, e_N.
    [[nodiscard]] static odl::Result<PriorBlock, EstimationRefusal> rtn(std::array<std::string, 3> names, std::array<double, 3> centre, const std::array<double, 9>& basis,
                                                                        std::array<double, 3> sigma_rtn);

    /// EST-R-401's validation (EST-F-401 .. EST-F-403).  The factories call it, and so does every consumer (add_prior, with_priors): a block assembled by hand is checked again, so no path
    /// carries an unvalidated block into a solve.
    [[nodiscard]] odl::Result<void, EstimationRefusal> validate() const;

    /// A readable name for the block in diagnostics: its kind and its parameters.
    [[nodiscard]] std::string label() const;

    /// The number of prior rows the block adds: isotropic, names.size(); RTN, 3.
    [[nodiscard]] std::size_t rows() const noexcept { return basis ? 3 : names.size(); }

    /// The whitened rows of the block over the parameters `all_names` (n of them): `rows()` rows of n entries (row-major) and, for a departure `d = x_A - x_lin` given per NAME of
    /// the block (in the block's order), the whitened residuals.  EST-R-402.  The names must be among `all_names` (the callers have checked).
    void whitened(const std::vector<std::string>& all_names, const std::vector<double>& departure, std::vector<double>& rows_out, std::vector<double>& residuals_out) const;
};

/// Validation shared by NormalEquations::add_prior and with_priors: every name of every block is among `names` and in no other block (EST-F-401, EST-F-405).
[[nodiscard]] odl::Result<void, EstimationRefusal> check_prior_blocks(const std::vector<std::string>& names, const std::vector<PriorBlock>& blocks);

/// EST-R-404: the model with the prior's rows appended (residual L(x_A - x), Jacobian L), so that the whole objective is the sum of squares the gain ratio is taken on.  A refusal at
/// construction (EST-F-401, -405); the returned model answers EST-F-404 the first time the wrapped model carries its own objective.
[[nodiscard]] odl::Result<LmModel, EstimationRefusal> with_priors(LmModel model, std::vector<std::string> names, std::vector<PriorBlock> blocks);

}  // namespace odl::estimation
