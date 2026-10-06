#pragma once
// odl/forcemodel/truncation.hpp — the gravity field's truncation, chosen against the smallest force the model keeps
// (SPEC-forcemodel.md §3.4, ruling R4).
//
// THE CRITERION IS A FUNCTION, NOT A NUMBER (`PERT-R-022a`'s lesson).  The lowest degree N >= 2 at which the RMS over the sphere of the
// acceleration discarded by truncating at N — `truncation_rms`, GRAV-R-041's closed form, one instrument with the ocean tide's — is at most
// F_min, the smallest non-zero contribution, through the registry at that point and in the same run, of the registered forces OTHER THAN the
// truncated series.  A comparator from a family the field's degree cannot move.  F_min depends on what is registered, so the force that sets it
// is named and every table row says what it compared against.

#include <odl/core/result.hpp>
#include <odl/dynamics/force_set.hpp>
#include <odl/forcemodel/common.hpp>
#include <odl/gravity/field.hpp>

#include <string>
#include <vector>

namespace odl::forcemodel {

struct TruncationChoice {
    int degree = 0;                 ///< the lowest N >= 2 with truncation_rms(r, N) <= f_min
    double radius_m = 0.0;
    double f_min_m_s2 = 0.0;
    std::string compared_against;   ///< the force that set f_min
    double rms_at_degree = 0.0;     ///< truncation_rms(r, degree)
    double rms_below = 0.0;         ///< truncation_rms(r, degree - 1), above f_min unless degree is 2
};

/// FMOD-R-011.  Refuses a non-positive radius or comparator.  Returns the model's top degree when no lower degree meets the criterion.
[[nodiscard]] odl::Result<TruncationChoice, ForceModelError>
degree_meeting_criterion(const gravity::ConventionalField& field, double radius_m, double f_min_m_s2,
                         std::string compared_against = {});

struct SmallestContribution {
    std::string name;
    double value_m_s2 = 0.0;
};

/// FMOD-R-011, FMOD-F-003.  The smallest NON-ZERO norm among the contributions whose force name is not in `excluded`; refuses a set in which
/// none is non-zero.
[[nodiscard]] odl::Result<SmallestContribution, ForceModelError>
smallest_nonzero_contribution(const std::vector<dyn::Contribution>& contributions, const std::vector<std::string>& excluded);

/// The test orbit of §3.4: a circular orbit of inclination `inclination_rad` at `radius_m`, `samples` points uniform in argument of latitude over
/// one revolution with the Earth rotating beneath it (ascending node at longitude 0 at the start).  For each of `degrees`, the MAXIMUM over the
/// samples of the norm of the discarded acceleration `a_{max} - a_N`, formed from the per-degree contributions (one pass at each sample).
[[nodiscard]] odl::Result<std::vector<double>, ForceModelError>
orbit_tail_maxima(const gravity::ConventionalField& field, double radius_m, double inclination_rad,
                  const std::vector<int>& degrees, int samples = 96);

}  // namespace odl::forcemodel
