#pragma once
// odl/dynamics/parameter.hpp — a parameter has an identity, not a position.
//
// SPEC-dynamics DYN-R-001 … DYN-R-005.
//
// `params[3]` meaning "the drag coefficient, by convention" is plan §5
// constraint 10 — *what a value means belongs in its type, never in the argument
// that produced it* — at the scale where it does the most damage.  The precedent
// is `Ephemeris::state`, which returned `State<Frame::BCRS>` for any centre: the
// frame was in the type as required, and the ORIGIN was a runtime argument the
// type did not carry, so a geocentric vector came back labelled barycentric.  A
// parameter vector indexed by convention is that defect with a dozen values
// instead of one, and with the convention recorded only in the prose of whichever
// force was implemented first.
//
// So: `ParameterId` is issued by a registry and cannot be made from an integer or
// a string; `ParameterSet` has no `operator[]` and no `data()`.  The dense layout
// the integrator needs exists, and it is private.

#include <odl/core/result.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace odl::dyn {

using DynError = odl::Diagnostic;

/// What a parameter MEANS.  Registration states this, so that two drag
/// coefficients on different spacecraft are different identities — a name alone
/// would collide, and a bare index would not even be asked (DYN-R-003).
enum class ParameterKind {
    drag_coefficient,
    srp_scale,
    empirical_acceleration,
    ballistic_coefficient,
    measurement_bias,
    other,
};

[[nodiscard]] constexpr std::string_view name_of(ParameterKind k) noexcept {
    switch (k) {
    case ParameterKind::drag_coefficient:       return "drag_coefficient";
    case ParameterKind::srp_scale:              return "srp_scale";
    case ParameterKind::empirical_acceleration: return "empirical_acceleration";
    case ParameterKind::ballistic_coefficient:  return "ballistic_coefficient";
    case ParameterKind::measurement_bias:       return "measurement_bias";
    case ParameterKind::other:                  return "other";
    }
    return "other";
}

struct ParameterDeclaration {
    ParameterKind kind = ParameterKind::other;
    std::string unit;      ///< the unit the VALUE is in, e.g. "1" or "m/s^2"
    std::string name;      ///< human, for diagnostics; never an identity on its own
    std::string subject;   ///< whose parameter it is — a spacecraft, a station
};

class ParameterRegistry;

/// A parameter's identity.  Issued by a registry; **not constructible from an
/// integer, a string, or anything a caller can make up** (DYN-R-002).  There is
/// no `ParameterId(std::size_t)` and adding one would be the defect this type
/// exists to prevent.
class ParameterId {
public:
    ParameterId() = delete;
    [[nodiscard]] friend bool operator==(const ParameterId&, const ParameterId&) noexcept = default;

private:
    friend class ParameterRegistry;
    struct Tag {};                      ///< identity of the issuing registry
    ParameterId(std::shared_ptr<const Tag> tag, std::size_t slot) noexcept
        : tag_(std::move(tag)), slot_(slot) {}

    std::shared_ptr<const Tag> tag_;
    std::size_t slot_ = 0;

    friend class ParameterSet;
    friend class ParameterJacobian;
    /// The sensitivity block's one internal consumer. The PUBLIC surface stays
    /// index-free (DYN-R-005); something has to map identity to storage, and it
    /// is here rather than in a caller's arithmetic.
    friend struct SensitivitySolution;
};

/// Issues identities.  Move-only: a registry is a thing, not a value, and two
/// copies of one would be two identities claiming to be the same.
class ParameterRegistry {
public:
    ParameterRegistry() : tag_(std::make_shared<ParameterId::Tag>()) {}
    ParameterRegistry(const ParameterRegistry&) = delete;
    ParameterRegistry& operator=(const ParameterRegistry&) = delete;
    ParameterRegistry(ParameterRegistry&&) noexcept = default;
    ParameterRegistry& operator=(ParameterRegistry&&) noexcept = default;

    [[nodiscard]] ParameterId declare(ParameterDeclaration d) {
        decls_.push_back(std::move(d));
        return ParameterId{tag_, decls_.size() - 1};
    }

    /// The width every Jacobian on this path takes (DYN-R-004).  There is no
    /// maximum, no compile-time extent, and nothing to raise later.
    [[nodiscard]] std::size_t size() const noexcept { return decls_.size(); }

    [[nodiscard]] bool issued(const ParameterId& id) const noexcept {
        return id.tag_ == tag_ && id.slot_ < decls_.size();
    }
    [[nodiscard]] odl::Result<const ParameterDeclaration*, DynError>
    declaration(const ParameterId& id) const;

    [[nodiscard]] std::shared_ptr<const ParameterId::Tag> tag() const noexcept { return tag_; }

private:
    std::shared_ptr<ParameterId::Tag> tag_;
    std::vector<ParameterDeclaration> decls_;
};

/// Values, addressed by identity.  **No `operator[]`, no `data()`, no `begin()`**
/// — the dense storage is here and it is private (DYN-R-002).
class ParameterSet {
public:
    explicit ParameterSet(const ParameterRegistry& r)
        : tag_(r.tag()), values_(r.size(), 0.0), present_(r.size(), false) {}

    [[nodiscard]] odl::Result<void, DynError> set(const ParameterId& id, double value);
    [[nodiscard]] odl::Result<double, DynError> value(const ParameterId& id) const;
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

private:
    std::shared_ptr<const ParameterId::Tag> tag_;
    std::vector<double> values_;
    std::vector<bool> present_;
};

}  // namespace odl::dyn
