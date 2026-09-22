#pragma once
// odl/drag/drag.hpp — atmospheric drag, over L2's atmosphere and L3's force
// plugin surface.
//
// SPEC-drag.md.  THE FIRST FORCE THAT CONSUMES A REGISTERED PARAMETER
// (`ParameterKind::drag_coefficient`, registered at L3 step 1 before this
// module existed) AND THE FIRST FORCE THAT CONSUMES ANOTHER LAYER'S
// PROVENANCE: `atmosphere::DragDensity` carries a verification flag and a
// snapshot identity (`SPEC-atmosphere` ATMO-R-031), and a force that drops
// them on the way to its own result makes that mechanism decorative. Both
// survive here: `DragResult::atmosphere_record` and `require_verified`.

#include <odl/atmosphere/atmosphere.hpp>
#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/force.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/eop/record.hpp>
#include <odl/frames/state.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

namespace odl::drag {

using DragError = odl::Diagnostic;

namespace detail {
/// Gregorian day-of-year, 1-366. Exposed only so SPEC-drag DRAG-A-009 can
/// check the leap-year rule's own three exceptions (1900, 2000, 2100)
/// directly: those years are outside what an `Epoch`/`LeapTable` can
/// represent in UTC (before 1972, or past any realistic table expiry), so
/// routing them through `acceleration` is not possible. NOT part of this
/// module's public contract — `acceleration` and `Drag` are.
[[nodiscard]] int day_of_year(int year, int month, int day) noexcept;
}  // namespace detail

/// SPEC-drag DRAG-R-001: a = -0.5 * rho * C_D * (A/m) * v_rel * |v_rel|, v_rel
/// the satellite's velocity relative to a RIGIDLY CO-ROTATING atmosphere.
/// `atmosphere_record` is `for_drag`'s own `EvaluationRecord`, carried forward
/// unchanged -- not summarised, not re-derived -- so a caller inspecting a
/// `DragResult` sees exactly what the atmosphere model that produced it saw.
struct DragResult {
    Vec3 acceleration_m_s2;                        ///< GCRS frame
    atmosphere::EvaluationRecord atmosphere_record;
};

/// The core computation. `r_gcrs_m`/`v_gcrs_m_per_s` are the satellite's GCRS
/// state; `eop`/`leaps` are what `frames::to_itrs` needs to find the
/// co-rotating frame and the geodetic point under the satellite (DRAG-R-002).
/// `require_verified`: refuse (`DRAG-F-001`, naming `ATMO-F-015`'s own reason)
/// rather than compute silently, if `sw`'s verification flag is not
/// `verified_against_issuer` -- SPEC-atmosphere's flag, reachable through this
/// force and not only through a direct call to `atmosphere::for_drag`.
[[nodiscard]] odl::Result<DragResult, DragError>
acceleration(const odl::time::Epoch& t, const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s,
            double c_d, double area_m2, double mass_kg, const atmosphere::SpaceWeather& sw,
            const odl::eop::EopRecord& eop, const odl::time::LeapTable& leaps,
            bool require_verified = false);

/// `dyn::Force`'s frozen surface (SPEC-dynamics), so drag plugs into the
/// integrator like any other force. `consumes()` declares exactly one
/// parameter, `ParameterKind::drag_coefficient` (registered at L3, not by
/// this module -- DYN-F-002 refuses a missing value rather than defaulting
/// C_D to a constant, which is the whole point of it being registered).
/// `area_m2`, `mass_kg`, `sw`, `eop` and `leaps` are bound at construction:
/// the `Force` interface has no room for them (frozen before this module
/// existed), so a `Drag` instance is built for one satellite's fixed
/// properties and one EOP/space-weather source, the same shape every other
/// force with fixed physical constants (radii, areas) already takes.
class Drag final : public dyn::Force {
public:
    Drag(dyn::ParameterId c_d_id, double area_m2, double mass_kg,
        atmosphere::SpaceWeather sw, odl::eop::EopRecord eop, odl::time::LeapTable leaps);

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
         const Vec3& v_m_per_s, const dyn::ParameterSet& params,
         const dyn::ParameterRegistry& registry) const override;

private:
    dyn::ParameterId c_d_id_;
    double area_m2_;
    double mass_kg_;
    atmosphere::SpaceWeather sw_;
    odl::eop::EopRecord eop_;
    odl::time::LeapTable leaps_;
    std::vector<dyn::ParameterId> consumes_;
};

}  // namespace odl::drag
