#pragma once
// odl/drag/drag.hpp — atmospheric drag, over L2's atmosphere and L3's force
// plugin surface.
//
// SPEC-drag.md.  THE FIRST FORCE THAT CONSUMES A REGISTERED PARAMETER
// (`ParameterKind::drag_coefficient`, registered at L3 step 1 before this
// module existed) AND THE FIRST FORCE THAT CONSUMES ANOTHER LAYER'S
// PROVENANCE: `atmosphere::DragDensity` carries a verification flag and a
// snapshot identity (`SPEC-atmosphere` ATMO-R-031), and a force that drops
// them on the way to its own result makes that mechanism decorative.
//
// THE PROVENANCE REACHES BOTH SURFACES THIS MODULE HAS, not only the free
// function's own return value: `DragResult::atmosphere_record` for a caller
// of `acceleration()` directly, and `dyn::ForceEvaluation::provenance`
// (amended additively into `SPEC-dynamics`'s frozen surface, DYN-Q-001's own
// terms) for a caller going through the `Force` plugin -- L7's estimator,
// which only ever sees a `ForceEvaluation`, is exactly the consumer the first
// draft's plugin-only silence would have left with no snapshot identity and
// no verification flag at all. `require_verified` is likewise reachable
// through both: a plain argument on `acceleration()`, and a construction-time
// option on `Drag` (§ below) so a consumer who needs verified inputs sets it
// once, not on every call.

#include <odl/atmosphere/atmosphere.hpp>
#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/force.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/eop/record.hpp>
#include <odl/frames/state.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace odl::drag {

/// A drag refusal. `DRAG-F-003` covers three internally distinct causes
/// (the GCRS<->ITRS transform, the epoch-to-UTC-calendar conversion, the
/// GCRS<->ITRS rotation) under one id, matching `DRAG-F-002`'s own
/// precedent for mass/area -- but `cause`, when present, carries the
/// UNDERLYING module's own `Diagnostic` (its own id and message) as DATA, not
/// only folded into `message`'s free text (plan §5 constraint 10: what a
/// value means belongs in its type). `DragError` is this module's own error
/// type, not `dyn::DynError` (`SPEC-dynamics`'s frozen, shared one, used
/// unchanged by `Drag::accel`'s own return type) -- free to carry this
/// because nothing outside this module reaches into it.
struct DragError {
    std::string_view id;
    std::string message;
    std::optional<odl::Diagnostic> cause;

    DragError(std::string_view refusal_id, std::string text)
        : id(refusal_id), message(std::move(text)) {}
    DragError(std::string_view refusal_id, std::string text, odl::Diagnostic underlying)
        : id(refusal_id), message(std::move(text)), cause(std::move(underlying)) {}
};

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
///
/// `require_verified` (default `false`, so every existing construction call
/// stays valid unchanged): when `true`, `accel()` refuses `DRAG-F-001`
/// rather than computing, on a sample whose `verification` is not
/// `verified_against_issuer` -- the SAME refusal `acceleration()`'s own
/// `require_verified` argument names, now reachable through the plugin a
/// caller with no access to the free function actually uses (DRAG-A-007).
class Drag final : public dyn::Force {
public:
    Drag(dyn::ParameterId c_d_id, double area_m2, double mass_kg,
        atmosphere::SpaceWeather sw, odl::eop::EopRecord eop, odl::time::LeapTable leaps,
        bool require_verified = false);

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
    bool require_verified_;
    std::vector<dyn::ParameterId> consumes_;
};

}  // namespace odl::drag
