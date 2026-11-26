#include <odl/estimation/prior.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace odl::estimation {

namespace {

constexpr double kSigmaLow = 0x1p-500;               // NOT-A-UNIT-CROSSING: the lower end of EST-R-401's sigma range, 2^-500 (sigma^-2 and every whitened entry stay normal doubles)
constexpr double kSigmaHigh = 0x1p+500;              // NOT-A-UNIT-CROSSING: the upper end of the same range, 2^500
constexpr double kOrthonormal = 1.0e-12;             // NOT-A-UNIT-CROSSING: EST-R-401's tolerance on max |B B^T - I| (a dimensionless matrix entry, not a unit change)

[[nodiscard]] std::string fmt(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

[[nodiscard]] std::string block_label(const PriorBlock& b) {
    std::string s = b.basis ? "RTN block (" : "isotropic block (";
    for (std::size_t i = 0; i < b.names.size(); ++i) s += (i ? ", " : "") + b.names[i];
    return s + ")";
}

[[nodiscard]] odl::Result<void, EstimationRefusal> refuse(const char* id, std::string text, std::string parameter = {}, double value = 0.0) {
    return odl::err(EstimationRefusal{id, std::move(text), std::move(parameter), value});
}

}  // namespace

std::string PriorBlock::label() const { return block_label(*this); }

odl::Result<void, EstimationRefusal> PriorBlock::validate() const {
    const std::string label = block_label(*this);
    // EST-F-401: the names
    if (names.empty()) return refuse("EST-F-401", "a prior block names no parameter");
    if (basis && names.size() != 3)
        return refuse("EST-F-401", label + ": an RTN block needs exactly three parameters (the inertial x, y, z of one vector); " + std::to_string(names.size()) + " were named");
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i].empty()) return refuse("EST-F-401", label + ": parameter " + std::to_string(i) + " has an empty name");
        for (std::size_t j = 0; j < i; ++j)
            if (names[j] == names[i]) return refuse("EST-F-401", label + ": the parameter '" + names[i] + "' is named twice (positions " + std::to_string(j) + " and " + std::to_string(i) + ")", names[i]);
    }
    // EST-F-402: the centres and the sigmas
    const std::size_t want_sigma = basis ? 3 : names.size();
    if (centre.size() != names.size())
        return refuse("EST-F-402", label + ": " + std::to_string(centre.size()) + " centres for " + std::to_string(names.size()) + " parameters", {}, static_cast<double>(centre.size()));
    if (sigma.size() != want_sigma)
        return refuse("EST-F-402", label + ": " + std::to_string(sigma.size()) + " standard deviations where " + std::to_string(want_sigma) + " are needed", {}, static_cast<double>(sigma.size()));
    for (std::size_t i = 0; i < centre.size(); ++i)
        if (!std::isfinite(centre[i])) return refuse("EST-F-402", label + ": the centre of '" + names[i] + "' is not finite", names[i], centre[i]);
    for (std::size_t i = 0; i < sigma.size(); ++i) {
        const double s = sigma[i];
        const std::string who = basis ? std::string(i == 0 ? "sigma_R" : i == 1 ? "sigma_T" : "sigma_N") : "sigma of '" + names[i] + "'";
        if (!std::isfinite(s)) return refuse("EST-F-402", label + ": " + who + " is not finite", basis ? std::string{} : names[i], s);
        if (!(s > 0.0)) return refuse("EST-F-402", label + ": " + who + " = " + fmt(s) + " is not positive (a hard fix is elimination, EST-R-209, never sigma = 0)", basis ? std::string{} : names[i], s);
        if (s < kSigmaLow || s > kSigmaHigh)
            return refuse("EST-F-402", label + ": " + who + " = " + fmt(s) + " is outside [2^-500, 2^500]", basis ? std::string{} : names[i], s);   // NOT-A-UNIT-CROSSING: the message names the range 2^-500 .. 2^500
    }
    // EST-F-403: the basis
    if (basis) {
        const std::array<double, 9>& B = *basis;
        for (std::size_t i = 0; i < 9; ++i)
            if (!std::isfinite(B[i])) return refuse("EST-F-403", label + ": the basis entry " + std::to_string(i) + " is not finite");
        double worst = 0.0;
        std::size_t at = 0;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) {
                double s = 0.0;
                for (std::size_t k = 0; k < 3; ++k) s += B[3 * i + k] * B[3 * j + k];
                const double dev = std::fabs(s - (i == j ? 1.0 : 0.0));
                if (dev > worst) {
                    worst = dev;
                    at = 3 * i + j;
                }
            }
        if (!(worst <= kOrthonormal))
            return refuse("EST-F-403", label + ": the basis is not orthonormal: max |B B^T - I| = " + fmt(worst) + " at entry (" + std::to_string(at / 3) + ", " + std::to_string(at % 3) + ") exceeds 1e-12", {}, worst);
    }
    return {};
}

odl::Result<PriorBlock, EstimationRefusal> PriorBlock::isotropic(std::vector<std::string> names, std::vector<double> centre, std::vector<double> sigma) {
    PriorBlock b;
    b.names = std::move(names);
    b.centre = std::move(centre);
    b.sigma = std::move(sigma);
    if (auto v = b.validate(); !v) return odl::err(v.error());
    return b;
}

odl::Result<PriorBlock, EstimationRefusal> PriorBlock::rtn(std::array<std::string, 3> names, std::array<double, 3> centre, const std::array<double, 9>& basis, std::array<double, 3> sigma_rtn) {
    PriorBlock b;
    b.names.assign(names.begin(), names.end());
    b.centre.assign(centre.begin(), centre.end());
    b.sigma.assign(sigma_rtn.begin(), sigma_rtn.end());
    b.basis = basis;
    if (auto v = b.validate(); !v) return odl::err(v.error());
    return b;
}

void PriorBlock::whitened(const std::vector<std::string>& all_names, const std::vector<double>& departure, std::vector<double>& rows_out, std::vector<double>& residuals_out) const {
    const std::size_t n = all_names.size();
    std::vector<std::size_t> at(names.size(), 0);
    for (std::size_t j = 0; j < names.size(); ++j)
        for (std::size_t i = 0; i < n; ++i)
            if (all_names[i] == names[j]) at[j] = i;
    const std::size_t r = rows();
    rows_out.assign(r * n, 0.0);
    residuals_out.assign(r, 0.0);
    if (!basis) {                                                        // EST-R-402: the row (e_i / sigma_i), the residual fl(d_i / sigma_i)
        for (std::size_t i = 0; i < r; ++i) {
            rows_out[i * n + at[i]] = 1.0 / sigma[i];
            residuals_out[i] = departure[i] / sigma[i];
        }
        return;
    }
    const std::array<double, 9>& B = *basis;                             // EST-R-402: three rows, entries fl(B_kj / sigma_k), residual fl(fl(sum_j B_kj d_j) / sigma_k), j = x, y, z
    for (std::size_t k = 0; k < 3; ++k) {
        for (std::size_t j = 0; j < 3; ++j) rows_out[k * n + at[j]] = B[3 * k + j] / sigma[k];
        double acc = B[3 * k] * departure[0];
        acc += B[3 * k + 1] * departure[1];
        acc += B[3 * k + 2] * departure[2];
        residuals_out[k] = acc / sigma[k];
    }
}

odl::Result<void, EstimationRefusal> check_prior_blocks(const std::vector<std::string>& names, const std::vector<PriorBlock>& blocks) {
    std::vector<std::ptrdiff_t> owner(names.size(), -1);
    for (std::size_t bi = 0; bi < blocks.size(); ++bi) {
        if (auto v = blocks[bi].validate(); !v) return odl::err(v.error());
        for (const std::string& nm : blocks[bi].names) {
            std::size_t at = names.size();
            for (std::size_t i = 0; i < names.size(); ++i)
                if (names[i] == nm) at = i;
            if (at == names.size())
                return refuse("EST-F-401", block_label(blocks[bi]) + " (block " + std::to_string(bi) + "): the parameter '" + nm + "' is not one the estimator holds", nm);
            if (owner[at] >= 0)
                return refuse("EST-F-405", "the parameter '" + nm + "' is constrained by two blocks: block " + std::to_string(owner[at]) + " (" + block_label(blocks[static_cast<std::size_t>(owner[at])]) +
                                              ") and block " + std::to_string(bi) + " (" + block_label(blocks[bi]) + "); no silent combination: build one block", nm);
            owner[at] = static_cast<std::ptrdiff_t>(bi);
        }
    }
    return {};
}

odl::Result<LmModel, EstimationRefusal> with_priors(LmModel model, std::vector<std::string> names, std::vector<PriorBlock> blocks) {
    if (auto v = check_prior_blocks(names, blocks); !v) return odl::err(v.error());
    // the index of each block's names among the call's parameters, once
    std::vector<std::vector<std::size_t>> at(blocks.size());
    for (std::size_t bi = 0; bi < blocks.size(); ++bi)
        for (const std::string& nm : blocks[bi].names)
            for (std::size_t i = 0; i < names.size(); ++i)
                if (names[i] == nm) at[bi].push_back(i);
    return LmModel([model = std::move(model), names = std::move(names), blocks = std::move(blocks), at = std::move(at)](std::span<const double> x,
                                                                                                                         bool with_jacobian) -> odl::Result<LmEvaluation, EstimationRefusal> {
        auto inner = model(x, with_jacobian);
        if (!inner) return odl::err(inner.error());
        LmEvaluation ev = std::move(*inner);
        if (ev.objective)
            return odl::err(EstimationRefusal{"EST-F-404", "the wrapped model carries its own objective (" + fmt(*ev.objective) +
                                                               "); a prior cannot be added to it under a registered convention (EST-R-404)", {}, *ev.objective});
        const std::size_t n = names.size();
        for (std::size_t bi = 0; bi < blocks.size(); ++bi) {
            const PriorBlock& b = blocks[bi];
            std::vector<double> departure(b.names.size());
            for (std::size_t j = 0; j < departure.size(); ++j) departure[j] = b.centre[j] - x[at[bi][j]];
            std::vector<double> rows, res;
            b.whitened(names, departure, rows, res);
            for (const double v : res) ev.residual.push_back(v);
            if (with_jacobian)
                for (const double v : rows) ev.jacobian.push_back(v);
        }
        (void)n;
        return ev;
    });
}

}  // namespace odl::estimation
