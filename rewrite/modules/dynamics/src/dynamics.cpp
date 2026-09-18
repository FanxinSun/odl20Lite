// dynamics.cpp — the plugin surface's behaviour, and the km<->m crossing.
//
// SPEC-dynamics.md.  There are exactly two crossing sites in this file and they
// are the only two in the tree outside `core/units.hpp`; `ci.sh` gate 10 keeps
// it that way.

#include <odl/dynamics/force_set.hpp>

#include <odl/core/units.hpp>

#include <string>

namespace odl::dyn {

odl::Result<const ParameterDeclaration*, DynError>
ParameterRegistry::declaration(const ParameterId& id) const {
    if (!issued(id)) {
        return odl::err(DynError{"DYN-F-001",
            "this ParameterId was not issued by this registry. A parameter's identity is the "
            "registry that issued it and its slot within it; an id from elsewhere refers to a "
            "parameter this registry knows nothing about, and reading it by position is the "
            "defect DYN-R-001 exists to prevent."});
    }
    return &decls_[id.slot_];
}

odl::Result<void, DynError> ParameterSet::set(const ParameterId& id, double v) {
    if (id.tag_ != tag_ || id.slot_ >= values_.size()) {
        return odl::err(DynError{"DYN-F-001",
            "this ParameterId was not issued by the registry this ParameterSet was built for."});
    }
    values_[id.slot_] = v;
    present_[id.slot_] = true;
    return {};
}

odl::Result<double, DynError> ParameterSet::value(const ParameterId& id) const {
    if (id.tag_ != tag_ || id.slot_ >= values_.size()) {
        return odl::err(DynError{"DYN-F-001",
            "this ParameterId was not issued by the registry this ParameterSet was built for."});
    }
    if (!present_[id.slot_]) {
        return odl::err(DynError{"DYN-F-002",
            "no value has been set for this parameter. A default of zero would be a value the "
            "caller never supplied, and for a drag coefficient it would silently remove the "
            "force (DYN-R-003)."});
    }
    return values_[id.slot_];
}

odl::Result<void, DynError>
ParameterJacobian::set_column(const ParameterId& id, const Vec3& d) {
    if (id.tag_ != tag_ || id.slot_ >= cols_.size()) {
        return odl::err(DynError{"DYN-F-003",
            "this ParameterId was not issued by the registry this Jacobian was built for; its "
            "width is " + std::to_string(cols_.size()) + "."});
    }
    cols_[id.slot_] = d;
    return {};
}

odl::Result<Vec3, DynError> ParameterJacobian::column(const ParameterId& id) const {
    if (id.tag_ != tag_ || id.slot_ >= cols_.size()) {
        return odl::err(DynError{"DYN-F-003",
            "this ParameterId was not issued by the registry this Jacobian was built for; its "
            "width is " + std::to_string(cols_.size()) + "."});
    }
    return cols_[id.slot_];
}

odl::Result<void, DynError> ForceSet::add(std::shared_ptr<const Force> f) {
    if (!f) return odl::err(DynError{"DYN-F-005", "a null force"});
    for (const auto& g : forces_) {
        if (g->id() == f->id()) {
            return odl::err(DynError{"DYN-F-005",
                "two forces registered with the identity '" + f->id().name + "'. The set "
                "attributes each contribution by identity (DYN-R-023), and two forces sharing "
                "one make the attribution a lie."});
        }
    }
    for (const auto& id : f->consumes()) {
        if (!registry_->issued(id)) {
            return odl::err(DynError{"DYN-F-001",
                "force '" + f->id().name + "' consumes a parameter this set's registry did not "
                "issue."});
        }
    }
    forces_.push_back(std::move(f));
    return {};
}

namespace {

/// CROSSING SITE 1 OF 2. A `State`'s position is in kilometres; a force's
/// `Position` is in metres.
[[nodiscard]] frames::Position<frames::Frame::GCRS>
field_position(const frames::State<frames::Frame::GCRS>& s) noexcept {
    return frames::Position<frames::Frame::GCRS>{field_position_m_from_state_km(s.position())};
}

/// The velocity crosses with it, by the same named function.
[[nodiscard]] Vec3 field_velocity(const frames::State<frames::Frame::GCRS>& s) noexcept {
    return field_position_m_from_state_km(s.velocity());
}

[[nodiscard]] bool finite(const Vec3& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace

odl::Result<std::vector<Contribution>, DynError>
ForceSet::contributions_at(const odl::time::Epoch& t,
                           const frames::State<frames::Frame::GCRS>& s,
                           const ParameterSet& params) const {
    const auto r = field_position(s);
    const auto v = field_velocity(s);
    std::vector<Contribution> out;
    out.reserve(forces_.size());
    for (const auto& f : forces_) {
        auto e = f->accel(t, r, v, params, *registry_);
        if (!e) return odl::err(e.error());
        const Vec3 a = e->acceleration.metres_per_second_squared();
        if (!finite(a)) {
            return odl::err(DynError{"DYN-F-006",
                "force '" + f->id().name + "' returned a non-finite acceleration."});
        }
        out.push_back(Contribution{f->id(), a});
    }
    return out;
}

odl::Result<StateDerivative, DynError>
ForceSet::derivative(const odl::time::Epoch& t,
                     const frames::State<frames::Frame::GCRS>& s,
                     const ParameterSet& params) const {
    auto terms = contributions_at(t, s, params);
    if (!terms) return odl::err(terms.error());
    Vec3 total{};
    for (const auto& c : *terms) {
        total.x += c.acceleration_m_s2.x;
        total.y += c.acceleration_m_s2.y;
        total.z += c.acceleration_m_s2.z;
    }
    // CROSSING SITE 2 OF 2. The summed acceleration is in m/s^2; the state is
    // integrated in km.
    return StateDerivative{s.velocity(), state_accel_km_s2_from_m_s2(total)};
}

odl::Result<ForceSet::Jacobians, DynError>
ForceSet::jacobians(const odl::time::Epoch& t,
                    const frames::State<frames::Frame::GCRS>& s,
                    const ParameterSet& params) const {
    const auto r = field_position(s);
    const auto v = field_velocity(s);
    Jacobians j{Mat3{}, Mat3{}, 0.0, ParameterJacobian{*registry_}};
    for (const auto& f : forces_) {
        auto e = f->accel(t, r, v, params, *registry_);
        if (!e) return odl::err(e.error());
        for (std::size_t a = 0; a < 3; ++a)
            for (std::size_t b = 0; b < 3; ++b)
                j.da_dr.r[a][b] += e->d_state.d_position().r[a][b];
        if (e->d_state.d_velocity()) {
            for (std::size_t a = 0; a < 3; ++a)
                for (std::size_t b = 0; b < 3; ++b)
                    j.da_dv.r[a][b] += (*e->d_state.d_velocity()).r[a][b];
        } else {
            // DECLARED ABSENT, with a bound. Summed rather than dropped: the
            // budget the caller is owed is the total of what every force left out.
            j.neglected_velocity_bound_per_s += e->d_state.neglected_velocity_bound_per_s();
        }
        for (const auto& id : f->consumes()) {
            auto col = e->d_parameters.column(id);
            if (!col) return odl::err(col.error());
            auto cur = j.da_dp.column(id);
            if (!cur) return odl::err(cur.error());
            auto put = j.da_dp.set_column(id, Vec3{cur->x + col->x, cur->y + col->y,
                                                   cur->z + col->z});
            if (!put) return odl::err(put.error());
        }
    }
    return j;
}

}  // namespace odl::dyn
