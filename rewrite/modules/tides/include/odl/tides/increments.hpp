#pragma once
// odl/tides/increments.hpp — what a tide model produces.
//
// PERT-R-001: a tide model produces INCREMENTS TO THE NORMALISED GEOPOTENTIAL
// COEFFICIENTS, never an acceleration.  The acceleration is `gravity`'s
// synthesis applied to the perturbed coefficients; computing it twice by two
// routes is how the two drift apart.
//
// PERT-R-003: this type and an acceleration have no conversion between them.  A
// caller adding a tide to a field and a caller adding an acceleration to a sum
// are doing different things.

#include <odl/core/result.hpp>
#include <odl/gravity/coefficients.hpp>

#include <string>
#include <vector>

namespace odl::tides {

using TidesError = odl::Diagnostic;
using gravity::TideSystem;

/// What produced an increment, and under what parameters.  PERT-R-062: a
/// perturbed field must be able to say which models made it.
struct ModelRecord {
    std::string model;       ///< "solid Earth tide, TN36-6 §6.2"
    std::string source;      ///< the manifest id or the clause of TN36
    std::string parameters;  ///< the Love-number column, the truncation, the epoch
};

/// Increments to C̄nm and S̄nm, dense to `max_degree`.
class TideIncrements {
public:
    TideIncrements(int max_degree, TideSystem system, ModelRecord record);

    void add(int n, int m, double dc, double ds) noexcept;
    [[nodiscard]] double dc(int n, int m) const noexcept;
    [[nodiscard]] double ds(int n, int m) const noexcept;

    [[nodiscard]] int max_degree() const noexcept { return max_degree_; }
    [[nodiscard]] TideSystem system() const noexcept { return system_; }
    [[nodiscard]] const std::vector<ModelRecord>& models() const noexcept { return models_; }

    /// PERT-F-001: two increments may be summed only if they agree about the
    /// permanent tide.  The refusal names both systems and the clause of
    /// TN36-6 §6.2.2 that reconciles them.
    [[nodiscard]] static odl::Result<TideIncrements, TidesError>
    sum(const std::vector<const TideIncrements*>& parts);

private:
    [[nodiscard]] static std::size_t index(int n, int m) noexcept {
        return gravity::CoefficientSet::index(n, m);
    }
    std::vector<double> dc_, ds_;
    int max_degree_;
    TideSystem system_;
    std::vector<ModelRecord> models_;
};

}  // namespace odl::tides
