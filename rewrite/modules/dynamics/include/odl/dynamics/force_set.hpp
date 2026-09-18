#pragma once
// odl/dynamics/force_set.hpp — where accelerations are summed, and where the
// kilometre meets the metre.
//
// SPEC-dynamics DYN-R-010, DYN-R-023.
//
// THE CROSSING HAS EXACTLY TWO SITES AND THEY ARE BOTH IN THIS MODULE'S .cpp.
// `SPEC-frames` puts `State` in kilometres and `Position`/`Acceleration` in
// metres, each following the units its normative source publishes.
// `core/units.hpp` named the crossing before there was a caller; this is the
// first caller, and `FRAME-R-062`'s *no other site performs it* is enforced from
// here on by `tools/unitcheck.py`, which is `ci.sh` gate 10.
//
// THE SET REPORTS WHAT IT SUMMED (DYN-R-023).  `tests/l2_floors.cpp` had to
// reconstruct L2's acceleration comparison by hand across three specifications,
// and both the manager and the executor reconstructed it wrong in opposite
// directions before it became contradictory enough to force a measurement.  L4
// has a dozen accelerations to rank.  The sum already visits every force, so
// reporting the terms costs the vector that holds them, and L4 inherits the
// ranking instead of rebuilding it.

#include <odl/dynamics/force.hpp>

#include <memory>
#include <vector>

namespace odl::dyn {

/// One force's share of the total, attributed.
struct Contribution {
    ForceId force;
    Vec3 acceleration_m_s2;
};

/// The derivative of a `State`, in the units a `State` is integrated in.
struct StateDerivative {
    Vec3 velocity_km_s;          ///< dr/dt
    Vec3 acceleration_km_s2;     ///< dv/dt — THE CROSSING happened to produce this
};

class ForceSet {
public:
    explicit ForceSet(const ParameterRegistry& registry) : registry_(&registry) {}

    [[nodiscard]] odl::Result<void, DynError> add(std::shared_ptr<const Force> f);
    [[nodiscard]] std::size_t size() const noexcept { return forces_.size(); }

    /// Every force's acceleration at a point, in m/s^2, attributed — plus the
    /// total.  No crossing happens here: this is the field side of the boundary.
    [[nodiscard]] odl::Result<std::vector<Contribution>, DynError>
    contributions_at(const odl::time::Epoch& t,
                     const frames::State<frames::Frame::GCRS>& s,
                     const ParameterSet& params) const;

    /// The state derivative, in km and km/s. **This is the only function in the
    /// tree that turns an acceleration into a state derivative**, and the
    /// conversion is `state_accel_km_s2_from_m_s2` by name.
    [[nodiscard]] odl::Result<StateDerivative, DynError>
    derivative(const odl::time::Epoch& t,
               const frames::State<frames::Frame::GCRS>& s,
               const ParameterSet& params) const;

    /// The summed Jacobians, for the STM at step 3 and the registry at step 4.
    struct Jacobians {
        Mat3 da_dr{};
        Mat3 da_dv{};
        double neglected_velocity_bound_per_s = 0.0;  ///< summed over declaring forces
        ParameterJacobian da_dp;
    };
    [[nodiscard]] odl::Result<Jacobians, DynError>
    jacobians(const odl::time::Epoch& t,
              const frames::State<frames::Frame::GCRS>& s,
              const ParameterSet& params) const;

private:
    const ParameterRegistry* registry_;
    std::vector<std::shared_ptr<const Force>> forces_;
};

}  // namespace odl::dyn
