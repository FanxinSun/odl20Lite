#include <odl/forcemodel/tides_force.hpp>

#include <odl/core/units.hpp>
#include <odl/eop/fundamental_arguments.hpp>

#include <algorithm>
#include <sstream>

namespace odl::forcemodel {

namespace {

/// A view's storage: the increments' arrays in `CoefficientSet`'s packing.
struct Storage {
    std::vector<double> c, s;
    int degree = 0;
};

Storage storage_of(const tides::TideIncrements& inc) {
    Storage st;
    st.degree = inc.max_degree();
    const std::size_t n = gravity::CoefficientSet::index(st.degree, st.degree) + 1;
    st.c.assign(n, 0.0);
    st.s.assign(n, 0.0);
    for (int d = 0; d <= st.degree; ++d)
        for (int m = 0; m <= d; ++m) {
            st.c[gravity::CoefficientSet::index(d, m)] = inc.dc(d, m);
            st.s[gravity::CoefficientSet::index(d, m)] = inc.ds(d, m);
        }
    return st;
}

std::string models_text(const tides::TideIncrements& inc) {
    std::string out;
    for (const auto& r : inc.models()) {
        if (!out.empty()) out += " | ";
        out += r.model + " (" + r.source + "; " + r.parameters + ")";
    }
    std::replace(out.begin(), out.end(), ';', ',');     // §3.2: the provenance list is semicolon-separated
    return out;
}

}  // namespace

Tides::Tides(const gravity::GravityModel& model, EarthOrientation orientation, const eph::Ephemeris& ephemeris,
             const time::LeapTable& leaps, TidesOptions options)
    : model_(&model), orientation_(std::move(orientation)), ephemeris_(&ephemeris), leaps_(&leaps), options_(std::move(options)) {}

dyn::ForceId Tides::id() const { return dyn::ForceId{"tides"}; }

const std::vector<dyn::ParameterId>& Tides::consumes() const { return consumes_; }

odl::Result<std::vector<std::pair<std::string, tides::TideIncrements>>, dyn::DynError>
Tides::models_at(const odl::time::Epoch& t, const EarthOrientation::At& eo, const gravity::ConventionalField& field) const {
    const Mat3& m = eo.rotation.m;

    auto tt = t.two_part_jd(time::TimeScale::TT, *leaps_);
    if (!tt) return odl::err(passthrough("tides", tt.error()));
    auto ut1 = t.ut1_two_part_jd(time::Duration::from_seconds(eo.record.dut1), *leaps_);
    if (!ut1) return odl::err(passthrough("tides", ut1.error()));
    const eop::tides::Arguments args = eop::tides::arguments_at(tt->day, tt->fraction, ut1->day, ut1->fraction);

    std::vector<std::pair<std::string, tides::TideIncrements>> out;

    if (options_.solid) {
        auto sun = ephemeris_->geocentric_state(eph::Body::Sun, t, *leaps_);
        if (!sun) return odl::err(passthrough("tides", sun.error()));
        auto moon = ephemeris_->geocentric_state(eph::Body::Moon, t, *leaps_);
        if (!moon) return odl::err(passthrough("tides", moon.error()));
        // The Sun and Moon in the ITRS (PERT-R-012: step 1 is evaluated in the body-fixed frame), by the plugin's own rotation.
        const frames::Position<frames::Frame::ITRS> s_itrs{m.apply(odl::metres_from_km(sun->position()))};
        const frames::Position<frames::Frame::ITRS> m_itrs{m.apply(odl::metres_from_km(moon->position()))};
        auto inc = tides::SolidEarthTide::increments(s_itrs, m_itrs, args, tides::LoveNumbers::Anelastic, options_.solid_target);
        if (!inc) return odl::err(passthrough("tides", inc.error()));
        out.emplace_back("solid_earth_tide", std::move(*inc));
    }
    if (options_.ocean && options_.ocean_degree >= 2) {
        auto inc = options_.ocean->increments(args, options_.ocean_degree, options_.ocean_degree);
        if (!inc) return odl::err(passthrough("tides", inc.error()));
        out.emplace_back("ocean_tide", std::move(*inc));
    }
    if (options_.solid_pole || (options_.ocean_pole && options_.ocean_pole_degree >= 2)) {
        // TN36-7 (25): the wobble is measured from the SECULAR pole, which is gravity's definition (GRAV-R-029), consumed here.
        const tides::Wobble w = tides::Wobble::from(eo.record.xp, eo.record.yp, field.pole());
        if (options_.solid_pole) {
            auto inc = tides::SolidEarthPoleTide::increments(w);
            if (!inc) return odl::err(passthrough("tides", inc.error()));
            out.emplace_back("solid_pole_tide", std::move(*inc));
        }
        if (options_.ocean_pole && options_.ocean_pole_degree >= 2) {
            auto inc = options_.ocean_pole->increments(w, options_.ocean_pole_degree);
            if (!inc) return odl::err(passthrough("tides", inc.error()));
            out.emplace_back("ocean_pole_tide", std::move(*inc));
        }
    }
    if (out.empty()) return odl::err(dyn::DynError{"FMOD-F-002", "forcemodel 'tides': no tide model is configured"});
    return out;
}

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Tides::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3&,
             const dyn::ParameterSet&, const dyn::ParameterRegistry& registry) const {
    auto eo = orientation_.at(t);
    if (!eo) return odl::err(passthrough("tides", eo.error()));
    const Mat3& m = eo->rotation.m;
    const Mat3 mt = m.transpose();

    auto field = model_->conventional(t, options_.extrapolate_secular);
    if (!field) return odl::err(passthrough("tides", field.error()));

    auto parts = models_at(t, *eo, *field);
    if (!parts) return odl::err(parts.error());

    // PERT-R-061: summed ONCE ...
    std::vector<const tides::TideIncrements*> ptrs;
    for (const auto& p : *parts) ptrs.push_back(&p.second);
    auto total = tides::TideIncrements::sum(ptrs);
    if (!total) return odl::err(passthrough("tides", total.error()));

    // ... applied ONCE.
    const Storage st = storage_of(*total);
    auto view = gravity::CoefficientView::of(st.degree, st.c, st.s, total->system(), "tide increments");
    if (!view) return odl::err(passthrough("tides", view.error()));
    auto deg = gravity::Degree::of(st.degree);
    if (!deg) return odl::err(passthrough("tides", deg.error()));
    auto ord = gravity::Order::of(st.degree);
    if (!ord) return odl::err(passthrough("tides", ord.error()));

    const frames::ItrsPosition at{m.apply(r_m.metres())};
    auto a = field->acceleration_of(*view, at, *deg, *ord);
    if (!a) return odl::err(passthrough("tides", a.error()));
    auto g = field->gradient_of(*view, at, *deg, *ord);
    if (!g) return odl::err(passthrough("tides", g.error()));

    const Vec3 a_gcrs = mt.apply(a->metres_per_second_squared());
    const Mat3 da_dr = mt.times(g->per_second_squared()).times(m);

    SourceId sid("tides");
    sid.add("orientation", orientation_.describe())
       .add("system", gravity::name_of(total->system()) == std::string_view("zero tide") ? "zero-tide" : "tide-free")
       .add("degree", std::to_string(st.degree))
       .add("models", models_text(*total));
    dyn::Provenance prov{sid.str(),
                         sha256_list(options_.sources, {"egm2008-coefficients", "de440s-spk", "fes2004-ocean-tide", "desai-ocean-pole-tide"}),
                         false};
    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{a_gcrs},
        dyn::StateJacobian::no_velocity_dependence(da_dr, 0.0),
        dyn::ParameterJacobian(registry),
        prov,
    };
}

odl::Result<std::vector<Tides::ModelAcceleration>, dyn::DynError>
Tides::by_model(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m) const {
    auto eo = orientation_.at(t);
    if (!eo) return odl::err(passthrough("tides", eo.error()));
    const Mat3& m = eo->rotation.m;
    const Mat3 mt = m.transpose();
    auto field = model_->conventional(t, options_.extrapolate_secular);
    if (!field) return odl::err(passthrough("tides", field.error()));
    auto parts = models_at(t, *eo, *field);
    if (!parts) return odl::err(parts.error());
    const frames::ItrsPosition at{m.apply(r_m.metres())};
    std::vector<ModelAcceleration> out;
    for (const auto& p : *parts) {
        const Storage st = storage_of(p.second);
        auto view = gravity::CoefficientView::of(st.degree, st.c, st.s, p.second.system(), p.first);
        if (!view) return odl::err(passthrough("tides", view.error()));
        auto deg = gravity::Degree::of(st.degree);
        auto ord = gravity::Order::of(st.degree);
        if (!deg || !ord) return odl::err(passthrough("tides", deg ? ord.error() : deg.error()));
        auto a = field->acceleration_of(*view, at, *deg, *ord);
        if (!a) return odl::err(passthrough("tides:" + p.first, a.error()));
        out.push_back({p.first, mt.apply(a->metres_per_second_squared())});
    }
    return out;
}

}  // namespace odl::forcemodel
