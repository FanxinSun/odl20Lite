#pragma once
// odl/dynamics/force.hpp — the one interface every force implements.
//
// SPEC-dynamics.md.  Frozen BEFORE any force exists, because retrofitting it is
// how the predecessor ended up unable to estimate drag at all, and gated with a
// TRIVIAL force so the surface is not shaped by its first client — a trivial
// force exercises the surface without negotiating with it.
//
// TWO NAMED 3x3 BLOCKS, NOT ONE 3x6 (DYN-R-022).  A zero block and a forgotten
// block are indistinguishable, and a silently-zero derivative reading as a
// correct answer IS the predecessor's drag defect.  So `absent` and `zero` are
// different states here, and a force declaring no velocity dependence must say
// how large the term it is neglecting can be (DYN-R-027).
//
// THAT IS NOT A FORMALITY.  Solar radiation pressure is the force a conventional
// model declares to have no velocity dependence, and the declaration is false:
// aberration makes it depend on the spacecraft's own velocity, with
// da/dv ~ a_SRP/c.  The term is ALTITUDE-INDEPENDENT while drag's decays, so they
// cross; measured against this tree's own atmosphere model, drag's derivative is
// 1078x SRP's at 953 km and 0.20x it at GNSS altitude (DYN-P-3, DYN-P-4).  SRP's
// aberration term is the DOMINANT velocity dependence in exactly the regime L4
// is aimed at.

#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/frames/state.hpp>
#include <odl/frames/vector.hpp>
#include <odl/time/epoch.hpp>

#include <optional>
#include <string>
#include <vector>

namespace odl::dyn {

/// d(acceleration)/d(state), as two things that are not the same thing.
class StateJacobian {
public:
    /// The force depends on velocity and here is the derivative.
    [[nodiscard]] static StateJacobian with_velocity(Mat3 da_dr, Mat3 da_dv) noexcept {
        StateJacobian j; j.da_dr_ = da_dr; j.da_dv_ = da_dv; return j;
    }
    /// The force does not depend on velocity, AND HERE IS HOW LARGE THE TERM IT
    /// NEGLECTS CAN BE, in s^-1.  The bound is not optional: without it "no
    /// velocity dependence" comes to mean "nobody looked" (DYN-R-027).
    [[nodiscard]] static StateJacobian no_velocity_dependence(Mat3 da_dr,
                                                              double neglected_bound_per_s) noexcept {
        StateJacobian j; j.da_dr_ = da_dr; j.neglected_ = neglected_bound_per_s; return j;
    }

    [[nodiscard]] const Mat3& d_position() const noexcept { return da_dr_; }
    /// Absent means DECLARED ABSENT, with a bound, not "zero" and not "forgotten".
    [[nodiscard]] const std::optional<Mat3>& d_velocity() const noexcept { return da_dv_; }
    [[nodiscard]] double neglected_velocity_bound_per_s() const noexcept { return neglected_; }

private:
    StateJacobian() = default;
    Mat3 da_dr_{};
    std::optional<Mat3> da_dv_{};
    double neglected_ = 0.0;
};

/// d(acceleration)/d(parameters).  Width comes from the registry; columns are
/// addressed BY IDENTITY (DYN-R-004, DYN-R-005).  There is no `column(size_t)`.
class ParameterJacobian {
public:
    explicit ParameterJacobian(const ParameterRegistry& r)
        : tag_(r.tag()), cols_(r.size(), Vec3{}) {}

    [[nodiscard]] odl::Result<void, DynError> set_column(const ParameterId& id, const Vec3& d);
    [[nodiscard]] odl::Result<Vec3, DynError> column(const ParameterId& id) const;
    [[nodiscard]] std::size_t width() const noexcept { return cols_.size(); }

    /// The dense block the integrator consumes. It sees a width and doubles, and
    /// nothing about what any of them MEAN (DYN-R-026).
    [[nodiscard]] const std::vector<Vec3>& dense() const noexcept { return cols_; }

private:
    std::shared_ptr<const ParameterId::Tag> tag_;
    std::vector<Vec3> cols_;
};

/// What a force returns.
struct ForceEvaluation {
    frames::Acceleration<frames::Frame::GCRS> acceleration;
    StateJacobian d_state;
    ParameterJacobian d_parameters;
};

/// A force's own identity, so the registry can attribute contributions.
struct ForceId {
    std::string name;
    [[nodiscard]] friend bool operator==(const ForceId&, const ForceId&) = default;
};

/// THE SURFACE.
///
/// A force receives a `Position` and `Velocity` in METRES and returns an
/// `Acceleration` in m/s^2.  **It never sees kilometres** (DYN-R-011): the
/// crossing happens at exactly two sites, both in `force_set.cpp`, both naming
/// `core/units.hpp`.
class Force {
public:
    virtual ~Force() = default;

    [[nodiscard]] virtual ForceId id() const = 0;

    /// The parameters this force consumes, declared up front so that a missing
    /// value is a refusal naming the force rather than a zero (DYN-F-002).
    [[nodiscard]] virtual const std::vector<ParameterId>& consumes() const = 0;

    /// `t` is an `Epoch`, not a double: an integrator carrying a bare `double t`
    /// has put the time scale in a comment (DYN-R-012).
    [[nodiscard]] virtual odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch& t,
          const frames::Position<frames::Frame::GCRS>& r_m,
          const Vec3& v_m_per_s,
          const ParameterSet& params,
          const ParameterRegistry& registry) const = 0;
};

}  // namespace odl::dyn
