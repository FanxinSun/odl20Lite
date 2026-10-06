#include <odl/estimation/normal_equations.hpp>

#include <odl/estimation/dense.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace odl::estimation {

namespace {

constexpr double kU = 0x1p-53;                       // the unit roundoff of IEEE binary64 under round-to-nearest (SPEC-estimation §3.4)
constexpr double kRangeLow = 0x1p-1000;              // NOT-A-UNIT-CROSSING: the exponent of the exact scaling's range, 2^-1000 (EST-F-206)
constexpr double kRangeHigh = 0x1p+1000;             // NOT-A-UNIT-CROSSING: the same range's upper end, 2^1000
constexpr int kPowerIterations = 100;                // EST-R-207
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] double gamma_k(std::size_t k) noexcept {
    const double ku = static_cast<double>(k) * kU;
    return ku / (1.0 - ku);
}

[[nodiscard]] double chi_n(std::size_t n) noexcept { return 1.0 / (1.0 - static_cast<double>(n + 1) * kU); }

/// Theta_s(n) = chi_n [ (3 n^2 + n) u + n^3 u^2 ]: the factorisation and the two substitutions, scaled 2-norm (SPEC-estimation §3.4).
[[nodiscard]] double theta_s(std::size_t n) noexcept {
    const double nn = static_cast<double>(n);
    return chi_n(n) * ((3.0 * nn * nn + nn) * kU + nn * nn * nn * kU * kU);
}

/// Theta_N(n, m) = n gamma_m (1 + gamma_m) + Theta_s(n): plus the forming of N.
[[nodiscard]] double theta_n(std::size_t n, std::size_t m) noexcept {
    const double g = gamma_k(m);
    return static_cast<double>(n) * g * (1.0 + g) + theta_s(n);
}

[[nodiscard]] std::string fmt(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

[[nodiscard]] std::string fmt17(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

/// The three entries of `values` of largest magnitude (indices into `values`), largest first; ties by lower index.
[[nodiscard]] std::vector<std::size_t> top_three(const std::vector<double>& values) {
    std::vector<std::size_t> idx(values.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return std::fabs(values[a]) > std::fabs(values[b]); });
    if (idx.size() > 3) idx.resize(3);
    return idx;
}

[[nodiscard]] std::string members(const std::vector<std::string>& names, const std::vector<double>& values) {
    std::string out;
    for (const std::size_t i : top_three(values)) {
        if (!out.empty()) out += ", ";
        out += names[i] + " (" + fmt(values[i]) + ")";
    }
    return out;
}

/// Rayleigh quotient of a symmetric matrix after `kPowerIterations` power iterations from (1,...,1)/sqrt(n); the final unit vector in `v`.
[[nodiscard]] double power_rayleigh(std::span<const double> m, std::size_t n, std::vector<double>& v) {
    v.assign(n, 1.0 / std::sqrt(static_cast<double>(n)));
    std::vector<double> w(n, 0.0);
    for (int it = 0; it < kPowerIterations; ++it) {
        multiply(m, n, v, w);
        double s = 0.0;
        for (const double t : w) s += t * t;
        const double nrm = std::sqrt(s);
        if (!(nrm > 0.0)) break;
        for (std::size_t i = 0; i < n; ++i) v[i] = w[i] / nrm;
    }
    multiply(m, n, v, w);
    double lam = 0.0;
    for (std::size_t i = 0; i < n; ++i) lam += v[i] * w[i];
    return lam;
}

void fix_sign(std::vector<double>& v) {
    std::size_t big = 0;
    for (std::size_t i = 1; i < v.size(); ++i)
        if (std::fabs(v[i]) > std::fabs(v[big])) big = i;
    if (!v.empty() && v[big] < 0.0)
        for (double& t : v) t = -t;
}

}  // namespace

int scale_exponent(double diagonal) noexcept {
    int k = 0;
    (void)std::frexp(diagonal, &k);
    return k >> 1;                                   // floor(k / 2): an arithmetic shift is a floor division by 2 for negative k, too (C++20)
}

// ------------------------------------------------------------------------------------------------------------------ Solution

odl::Result<double, EstimationRefusal> Solution::sigma0_squared() const {
    if (!weighted_rss) {
        return odl::err(EstimationRefusal{"EST-F-205", "the variance of unit weight is unavailable: no residual sum of squares was supplied (m = " +
                                                           std::to_string(observations) + ", free parameters " + std::to_string(free_parameters) + ")"});
    }
    if (degrees_of_freedom <= 0) {
        return odl::err(EstimationRefusal{"EST-F-205", "the variance of unit weight is undefined: nu = m - n_free = " + std::to_string(observations) + " - " +
                                                           std::to_string(free_parameters) + " = " + std::to_string(degrees_of_freedom) + " <= 0",
                                          {}, static_cast<double>(degrees_of_freedom)});
    }
    return *weighted_rss / static_cast<double>(degrees_of_freedom);
}

std::vector<double> Solution::formal_covariance() const { return cofactor; }

odl::Result<std::vector<double>, EstimationRefusal> Solution::scaled_covariance() const {
    const auto s2 = sigma0_squared();
    if (!s2) return odl::err(s2.error());
    std::vector<double> out(cofactor.size());
    for (std::size_t i = 0; i < cofactor.size(); ++i) out[i] = *s2 * cofactor[i];
    return out;
}

double Solution::correlation(std::size_t i, std::size_t j) const {
    if (i == j) return 1.0;
    std::size_t pi = free_index.size(), pj = free_index.size();
    for (std::size_t k = 0; k < free_index.size(); ++k) {
        if (free_index[k] == i) pi = k;
        if (free_index[k] == j) pj = k;
    }
    if (pi == free_index.size() || pj == free_index.size()) return 0.0;
    const std::size_t nf = free_index.size();
    const double r = scaled_cofactor[pi * nf + pj] / std::sqrt(scaled_cofactor[pi * nf + pi] * scaled_cofactor[pj * nf + pj]);
    return std::max(-1.0, std::min(1.0, r));
}

// ------------------------------------------------------------------------------------------------------- NormalEquations

odl::Result<NormalEquations, EstimationRefusal> NormalEquations::create(std::vector<std::string> names) {
    if (names.empty()) return odl::err(EstimationRefusal{"EST-F-204", "an estimator needs at least one parameter"});
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i].empty()) return odl::err(EstimationRefusal{"EST-F-204", "parameter " + std::to_string(i) + " has an empty name"});
        for (std::size_t j = 0; j < i; ++j)
            if (names[j] == names[i])
                return odl::err(EstimationRefusal{"EST-F-204", "the parameter name '" + names[i] + "' is used twice (indices " + std::to_string(j) + " and " +
                                                                   std::to_string(i) + ")", names[i]});
    }
    return NormalEquations(std::move(names));
}

odl::Result<NormalEquations, EstimationRefusal>
NormalEquations::from_normal_system(std::vector<std::string> names, std::span<const double> matrix, std::span<const double> rhs,
                                    std::size_t observations, std::optional<double> residual_square_sum) {
    auto made = create(std::move(names));
    if (!made) return odl::err(made.error());
    NormalEquations ne = std::move(*made);
    const std::size_t n = ne.names_.size();
    if (matrix.size() != n * n || rhs.size() != n)
        return odl::err(EstimationRefusal{"EST-F-204", "a normal system of " + std::to_string(n) + " parameters needs a " + std::to_string(n) + " x " + std::to_string(n) +
                                                           " matrix and a " + std::to_string(n) + "-vector; got " + std::to_string(matrix.size()) + " and " +
                                                           std::to_string(rhs.size()) + " values"});
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(rhs[i])) return odl::err(EstimationRefusal{"EST-F-204", "the right-hand side entry " + std::to_string(i) + " is not finite", ne.names_[i]});
        for (std::size_t j = 0; j <= i; ++j)
            if (!std::isfinite(matrix[i * n + j]))
                return odl::err(EstimationRefusal{"EST-F-204", "the matrix entry (" + std::to_string(i) + ", " + std::to_string(j) + ") is not finite"});
    }
    if (residual_square_sum && !std::isfinite(*residual_square_sum))
        return odl::err(EstimationRefusal{"EST-F-204", "the residual square sum is not finite"});
    ne.system_mode_ = true;
    ne.system_matrix_.assign(matrix.begin(), matrix.end());
    for (std::size_t i = 0; i < n; ++i)                                  // the lower triangle is read: mirror it
        for (std::size_t j = i + 1; j < n; ++j) ne.system_matrix_[i * n + j] = ne.system_matrix_[j * n + i];
    ne.system_rhs_.assign(rhs.begin(), rhs.end());
    ne.system_observations_ = observations;
    ne.system_residual_square_sum_ = residual_square_sum;
    return ne;
}

odl::Result<void, EstimationRefusal> NormalEquations::add(std::span<const double> row, double residual, double sigma) {
    const std::size_t n = names_.size();
    if (system_mode_)
        return odl::err(EstimationRefusal{"EST-F-204", "rows cannot be added to a system built by from_normal_system"});
    if (row.size() != n)
        return odl::err(EstimationRefusal{"EST-F-204", "observation row " + std::to_string(residuals_.size()) + " has " + std::to_string(row.size()) +
                                                           " entries; the estimator has " + std::to_string(n) + " parameters"});
    if (!(std::isfinite(sigma) && sigma > 0.0))
        return odl::err(EstimationRefusal{"EST-F-204", "observation row " + std::to_string(residuals_.size()) + ": sigma = " + fmt(sigma) +
                                                           " is not finite and positive (a weight is never defaulted)", {}, sigma});
    if (!std::isfinite(residual))
        return odl::err(EstimationRefusal{"EST-F-204", "observation row " + std::to_string(residuals_.size()) + ": the residual is not finite"});
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isfinite(row[i]))
            return odl::err(EstimationRefusal{"EST-F-204", "observation row " + std::to_string(residuals_.size()) + ": the partial with respect to '" + names_[i] +
                                                               "' is not finite", names_[i]});
    for (std::size_t i = 0; i < n; ++i) rows_.push_back(row[i] / sigma);
    residuals_.push_back(residual / sigma);
    return {};
}

odl::Result<FormedSystem, EstimationRefusal> NormalEquations::formed_system() const {
    const std::size_t n = names_.size();
    FormedSystem out;
    out.n = n;
    if (system_mode_) {
        out.matrix = system_matrix_;
        out.rhs = system_rhs_;
        return out;
    }
    out.matrix.assign(n * n, 0.0);
    out.rhs.assign(n, 0.0);
    const std::size_t m = residuals_.size();
    for (std::size_t k = 0; k < m; ++k) {                                // EST-R-203: rows in the order added, plain double sums
        const double* a = &rows_[k * n];
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j <= i; ++j) out.matrix[i * n + j] += a[i] * a[j];
            out.rhs[i] += a[i] * residuals_[k];
        }
    }
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) out.matrix[i * n + j] = out.matrix[j * n + i];
    return out;
}

odl::Result<Solution, EstimationRefusal> NormalEquations::solve(const SolveOptions& options) const {
    const std::size_t n = names_.size();

    // ------------------------------------------------------------------------------------------- the eliminations (EST-R-209)
    std::vector<unsigned char> elim(n, 0);
    std::vector<double> evalue(n, 0.0);
    if (system_mode_ && !options.eliminate.empty())
        return odl::err(EstimationRefusal{"EST-F-204", "an elimination needs the observation rows; this system was built by from_normal_system"});
    for (const Elimination& e : options.eliminate) {
        std::size_t at = n;
        for (std::size_t i = 0; i < n; ++i)
            if (names_[i] == e.name) at = i;
        if (at == n) return odl::err(EstimationRefusal{"EST-F-204", "the elimination names '" + e.name + "', which is not a parameter", e.name});
        if (elim[at]) return odl::err(EstimationRefusal{"EST-F-204", "the parameter '" + e.name + "' is eliminated twice", e.name});
        if (!std::isfinite(e.value)) return odl::err(EstimationRefusal{"EST-F-204", "the elimination of '" + e.name + "' has a value that is not finite", e.name});
        elim[at] = 1;
        evalue[at] = e.value;
    }
    std::vector<std::size_t> free_index;
    for (std::size_t i = 0; i < n; ++i)
        if (!elim[i]) free_index.push_back(i);
    const std::size_t nf = free_index.size();
    if (nf == 0) return odl::err(EstimationRefusal{"EST-F-204", "every parameter is eliminated; nothing is left to estimate"});
    const std::size_t m = observations();

    // ------------------------------------------------------------------------------------------- forming (EST-R-203, -209)
    std::vector<double> N(nf * nf, 0.0), b(nf, 0.0);
    std::vector<double> rprime;                       // the residual vector that forms b-hat (after elimination)
    double rho0 = 0.0, rhobar = 0.0, mu_e = 0.0;      // ||r~||, ||r~'||, || sum_e |a_ke| |v_e| ||
    std::size_t n_elim = 0;
    for (std::size_t i = 0; i < n; ++i) n_elim += elim[i];
    if (!system_mode_) {
        const std::size_t mm = residuals_.size();
        rprime.assign(mm, 0.0);
        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        for (std::size_t k = 0; k < mm; ++k) {
            const double* a = &rows_[k * n];
            double acc = residuals_[k];
            double mag = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                if (!elim[i]) continue;
                acc = acc - a[i] * evalue[i];
                mag += std::fabs(a[i]) * std::fabs(evalue[i]);
            }
            rprime[k] = acc;
            s0 += residuals_[k] * residuals_[k];
            s1 += acc * acc;
            s2 += mag * mag;
        }
        rho0 = std::sqrt(s0);
        rhobar = std::sqrt(s1);
        mu_e = std::sqrt(s2);
        for (std::size_t k = 0; k < mm; ++k) {
            const double* a = &rows_[k * n];
            for (std::size_t ii = 0; ii < nf; ++ii) {
                const double ai = a[free_index[ii]];
                for (std::size_t jj = 0; jj <= ii; ++jj) N[ii * nf + jj] += ai * a[free_index[jj]];
                b[ii] += ai * rprime[k];
            }
        }
    } else {
        for (std::size_t ii = 0; ii < nf; ++ii) {
            for (std::size_t jj = 0; jj <= ii; ++jj) N[ii * nf + jj] = system_matrix_[free_index[ii] * n + free_index[jj]];
            b[ii] = system_rhs_[free_index[ii]];
        }
    }
    for (std::size_t i = 0; i < nf; ++i)
        for (std::size_t j = i + 1; j < nf; ++j) N[i * nf + j] = N[j * nf + i];

    // ------------------------------------------------------------------------------------------- the diagonals (EST-F-201, -206)
    for (std::size_t i = 0; i < nf; ++i)
        for (std::size_t j = 0; j <= i; ++j)
            if (!std::isfinite(N[i * nf + j]))
                return odl::err(EstimationRefusal{"EST-F-206", "the normal matrix entry (" + names_[free_index[i]] + ", " + names_[free_index[j]] +
                                                                   ") is not finite: the information of the problem is outside what the exact scaling covers",
                                                  names_[free_index[i]], N[i * nf + j]});
    {
        std::string none;
        std::string first;
        double first_value = 0.0;
        for (std::size_t i = 0; i < nf; ++i) {
            const double d = N[i * nf + i];
            if (d <= 0.0) {
                if (first.empty()) { first = names_[free_index[i]]; first_value = d; }
                none += (none.empty() ? "" : ", ") + names_[free_index[i]];
            }
        }
        if (!none.empty())
            return odl::err(EstimationRefusal{"EST-F-201", "no information about " + none + " (the diagonal of the normal matrix is not positive: a zero column, or no rows)",
                                              first, first_value});
    }
    for (std::size_t i = 0; i < nf; ++i) {
        const double d = N[i * nf + i];
        if (d < kRangeLow || d > kRangeHigh)
            return odl::err(EstimationRefusal{"EST-F-206", "the diagonal of '" + names_[free_index[i]] + "' is " + fmt(d) +
                                                               ", outside [2^-1000, 2^1000]: the exact scaling does not cover it", names_[free_index[i]], d});   // NOT-A-UNIT-CROSSING: the message names the range 2^-1000 .. 2^1000
    }

    // ------------------------------------------------------------------------------------------- the scaling (EST-R-201)
    std::vector<int> e(nf, 0);
    for (std::size_t i = 0; i < nf; ++i) e[i] = scale_exponent(N[i * nf + i]);
    std::vector<double> Ns(nf * nf, 0.0), bs(nf, 0.0);
    for (std::size_t i = 0; i < nf; ++i) {
        bs[i] = std::ldexp(b[i], -e[i]);
        for (std::size_t j = 0; j <= i; ++j) Ns[i * nf + j] = std::ldexp(N[i * nf + j], -(e[i] + e[j]));
    }
    for (std::size_t i = 0; i < nf; ++i)
        for (std::size_t j = i + 1; j < nf; ++j) Ns[i * nf + j] = Ns[j * nf + i];

    // names of the free parameters, for the messages
    std::vector<std::string> fnames(nf);
    for (std::size_t i = 0; i < nf; ++i) fnames[i] = names_[free_index[i]];

    // ------------------------------------------------------------------------------------------- the factorisation (EST-R-204, EST-F-202)
    std::vector<double> L = Ns;
    const CholeskyResult ch = cholesky_lower(L, nf);
    if (!ch.ok) {
        const std::size_t j = ch.index;
        // the dependency (EST-R-208): z solves the leading j x j block against the row j of N_s; N_s[:, j] ~ sum z_k N_s[:, k]
        std::vector<double> lead(j * j, 0.0), z(j, 0.0);
        for (std::size_t r = 0; r < j; ++r)
            for (std::size_t c = 0; c <= r; ++c) lead[r * j + c] = L[r * nf + c];
        for (std::size_t r = 0; r < j; ++r) z[r] = Ns[j * nf + r];
        if (j > 0) {
            forward_substitute(lead, j, z);
            backward_substitute_transposed(lead, j, z);
        }
        ConditionReport report;
        report.weakest_direction.assign(n, 0.0);
        double nrm = 1.0;
        for (std::size_t r = 0; r < j; ++r) nrm += z[r] * z[r];
        nrm = std::sqrt(nrm);
        for (std::size_t r = 0; r < j; ++r) report.weakest_direction[free_index[r]] = -z[r] / nrm;
        report.weakest_direction[free_index[j]] = 1.0 / nrm;
        report.parameter_conditions.assign(n, kNaN);
        std::string dep;
        {
            std::vector<std::string> lead_names(fnames.begin(), fnames.begin() + static_cast<std::ptrdiff_t>(j));
            dep = j > 0 ? members(lead_names, z) : std::string("none: it has no predecessor");
        }
        return odl::err(EstimationRefusal{"EST-F-202", "the scaled factorisation breaks down at '" + fnames[j] + "' (pivot " + fmt(ch.pivot) +
                                                           " in the scaled variables): it is numerically a combination of the parameters before it; N_s[:, " + fnames[j] +
                                                           "] ~ sum z_k N_s[:, k] with the largest |z_k|: " + dep + "; the condition number is not available",
                                          fnames[j], ch.pivot, std::move(report)});
    }

    // ------------------------------------------------------------------------------------------- the solution (EST-R-205)
    std::vector<double> y = bs;
    forward_substitute(L, nf, y);
    backward_substitute_transposed(L, nf, y);

    std::vector<double> Qs(nf * nf, 0.0), q(nf, 0.0);
    for (std::size_t j = 0; j < nf; ++j) {
        std::fill(q.begin(), q.end(), 0.0);
        q[j] = 1.0;
        forward_substitute(L, nf, q);
        backward_substitute_transposed(L, nf, q);
        for (std::size_t i = j; i < nf; ++i) Qs[i * nf + j] = q[i];
    }
    for (std::size_t i = 0; i < nf; ++i)
        for (std::size_t j = i + 1; j < nf; ++j) Qs[i * nf + j] = Qs[j * nf + i];

    // ------------------------------------------------------------------------------------------- the condition report (EST-R-207)
    ConditionReport report;
    report.parameter_conditions.assign(n, kNaN);
    report.weakest_direction.assign(n, 0.0);
    bool finite = true;
    for (const double v : Qs) finite = finite && std::isfinite(v);
    for (const double v : y) finite = finite && std::isfinite(v);
    if (finite) {
        std::vector<double> vN, vQ;
        const double lam_n = power_rayleigh(Ns, nf, vN);
        const double lam_q = power_rayleigh(Qs, nf, vQ);
        fix_sign(vQ);
        report.condition_estimate = lam_n * lam_q;
        report.condition_upper = norm_one(Ns, nf) * norm_one(Qs, nf);
        for (std::size_t i = 0; i < nf; ++i) {
            report.parameter_conditions[free_index[i]] = Ns[i * nf + i] * Qs[i * nf + i];
            report.weakest_direction[free_index[i]] = vQ[i];
        }
    }

    // ------------------------------------------------------------------------------------------- what the solve certifies (EST-R-206, EST-F-203)
    double B = std::numeric_limits<double>::infinity();
    if (finite) {
        const double q1 = norm_one(Qs, nf);
        double X2 = 0.0;
        for (std::size_t i = 0; i < nf; ++i) X2 += Ns[i * nf + i] * y[i] * y[i];
        const double X = std::sqrt(X2);
        bool b_zero = true;
        for (const double t : bs) b_zero = b_zero && t == 0.0;
        if (b_zero) {
            B = 0.0;
        } else if (X > 0.0) {
            double bracket;
            if (system_mode_) {
                bracket = theta_s(nf);
            } else {
                const double g = gamma_k(m);
                bracket = theta_n(nf, m) + std::sqrt(static_cast<double>(nf)) * (g * rhobar + gamma_k(2 * n_elim) * (rho0 + mu_e)) / X;
            }
            B = 4.0 * q1 * bracket;
        }
    }
    if (!(B < 0.5)) {
        std::vector<std::string> all_names = names_;
        return odl::err(EstimationRefusal{"EST-F-203",
                                          "the solve certifies no digit: B = " + fmt(B) + " >= 1/2" + (finite ? "" : " (the computed cofactor or solution is not finite)") +
                                              "; kappa_est = " + fmt(report.condition_estimate) + ", kappa_up = " + fmt(report.condition_upper) +
                                              "; largest per-parameter condition numbers: " + members(all_names, [&] {
                                                  std::vector<double> v(n, 0.0);
                                                  for (std::size_t i = 0; i < n; ++i) v[i] = std::isnan(report.parameter_conditions[i]) ? 0.0 : report.parameter_conditions[i];
                                                  return v;
                                              }()) +
                                              "; weakest direction: " + members(all_names, report.weakest_direction),
                                          {}, B, std::move(report)});
    }

    // ------------------------------------------------------------------------------------------- the solution assembled
    Solution s;
    s.names = names_;
    s.x.assign(n, 0.0);
    s.eliminated = elim;
    s.scale_exponents.assign(n, 0);
    s.free_index = free_index;
    for (std::size_t i = 0; i < n; ++i)
        if (elim[i]) s.x[i] = evalue[i];
    for (std::size_t i = 0; i < nf; ++i) {
        s.x[free_index[i]] = std::ldexp(y[i], -e[i]);
        s.scale_exponents[free_index[i]] = e[i];
    }
    s.scaled_matrix = Ns;
    s.scaled_rhs = bs;
    s.scaled_solution = y;
    s.scaled_cofactor = Qs;
    s.cofactor.assign(n * n, 0.0);
    for (std::size_t i = 0; i < nf; ++i)
        for (std::size_t j = 0; j < nf; ++j) s.cofactor[free_index[i] * n + free_index[j]] = std::ldexp(Qs[i * nf + j], -(e[i] + e[j]));
    s.condition = std::move(report);
    s.certified_relative_error = B;
    s.observations = m;
    s.free_parameters = nf;
    s.degrees_of_freedom = static_cast<std::ptrdiff_t>(m) - static_cast<std::ptrdiff_t>(nf);

    // the residual (EST-R-210)
    if (!system_mode_) {
        double omega = 0.0;
        for (std::size_t k = 0; k < residuals_.size(); ++k) {
            const double* a = &rows_[k * n];
            double sk = 0.0;
            for (std::size_t i = 0; i < n; ++i) sk += a[i] * s.x[i];
            const double rho = residuals_[k] - sk;
            omega += rho * rho;
        }
        s.weighted_rss = omega;
    } else if (system_residual_square_sum_) {
        double bx = 0.0;
        for (std::size_t i = 0; i < n; ++i) bx += system_rhs_[i] * s.x[i];
        s.weighted_rss = *system_residual_square_sum_ - bx;
    }

    // the provenance (EST-R-212)
    {
        std::string p = "estimation=batch-normal-equations;scaling=powers-of-two;parameters=" + std::to_string(n) + ";free=" + std::to_string(nf) +
                        ";observations=" + std::to_string(m) + ";eliminated=";
        bool any = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (!elim[i]) continue;
            p += (any ? "," : "") + names_[i] + "=" + fmt17(evalue[i]);
            any = true;
        }
        if (!any) p += "none";
        p += ";exponents=";
        for (std::size_t i = 0; i < nf; ++i) p += (i ? "," : "") + std::to_string(e[i]);
        p += ";";
        s.provenance = std::move(p);
    }
    return s;
}

}  // namespace odl::estimation
