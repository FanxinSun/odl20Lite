#pragma once
// odl/forcemodel/tides_force.hpp — the tides as ONE dyn::Force (SPEC-forcemodel.md FMOD-R-007).
//
// PERT-R-061: the increments of every configured model are SUMMED ONCE (`TideIncrements::sum`, which refuses a tide-system mismatch)
// and APPLIED ONCE, through the field's `acceleration_of` / `gradient_of` (SPEC-gravity §4.7) — a plugin per model would synthesise
// each separately and add the accelerations, which is the same number to round-off and not the same computation.  `by_model` returns each
// model's own acceleration, outside the Force interface, for the ranking and the truncation table.

#include <odl/dynamics/force.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/forcemodel/common.hpp>
#include <odl/forcemodel/earth_orientation.hpp>
#include <odl/gravity/field.hpp>
#include <odl/tides/ocean.hpp>
#include <odl/tides/pole.hpp>
#include <odl/tides/solid_earth.hpp>

#include <string>
#include <utility>
#include <vector>

namespace odl::forcemodel {

struct TidesOptions {
    bool solid = true;                                   ///< TN36-6 §6.2, three steps (degree 4)
    const tides::OceanTide* ocean = nullptr;             ///< FES2004 (borrowed); null = not included
    int ocean_degree = 0;                                ///< PERT-R-022a's criterion supplies a default; the caller states it
    bool solid_pole = true;                              ///< TN36-6 §6.4 (degree 2)
    const tides::OceanPoleTide* ocean_pole = nullptr;    ///< Desai's equilibrium model (borrowed); null = not included
    int ocean_pole_degree = 0;
    /// The solid Earth tide's target tide system.  The conventional field is ZERO-TIDE (GRAV-R-021), so the default is zero-tide; a
    /// different one is refused at synthesis (GRAV-F-008) and exists so that the refusal can be shown.
    gravity::TideSystem solid_target = gravity::TideSystem::ZeroTide;
    bool extrapolate_secular = false;                    ///< GRAV-F-006's override, for the conventional field and the wobble's secular pole
    std::vector<SourceRecord> sources;
};

class Tides final : public dyn::Force {
public:
    /// The model, the ephemeris, the leap table, and any tide model named in the options are borrowed and must outlive the plugin.
    Tides(const gravity::GravityModel& model, EarthOrientation orientation, const eph::Ephemeris& ephemeris,
          const time::LeapTable& leaps, TidesOptions options = {});

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s,
          const dyn::ParameterSet& params, const dyn::ParameterRegistry& registry) const override;

    struct ModelAcceleration {
        std::string name;        ///< solid_earth_tide, ocean_tide, solid_pole_tide, ocean_pole_tide
        Vec3 a_m_s2;             ///< GCRS
    };
    /// Each configured model's acceleration, synthesised by itself (the diagnostic route; NOT how `accel` forms its answer).
    [[nodiscard]] odl::Result<std::vector<ModelAcceleration>, dyn::DynError>
    by_model(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m) const;

    [[nodiscard]] std::string describe_orientation() const { return orientation_.describe(); }

private:
    struct Parts;
    [[nodiscard]] odl::Result<std::vector<std::pair<std::string, tides::TideIncrements>>, dyn::DynError>
    models_at(const odl::time::Epoch& t, const EarthOrientation::At& eo, const gravity::ConventionalField& field) const;

    const gravity::GravityModel* model_;
    EarthOrientation orientation_;
    const eph::Ephemeris* ephemeris_;
    const time::LeapTable* leaps_;
    TidesOptions options_;
    std::vector<dyn::ParameterId> consumes_;
};

}  // namespace odl::forcemodel
