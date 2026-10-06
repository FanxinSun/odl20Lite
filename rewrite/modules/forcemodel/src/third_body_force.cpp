#include <odl/forcemodel/third_body_force.hpp>

#include <odl/core/units.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace odl::forcemodel {

std::string_view third_body_name(eph::Body b) noexcept {
    switch (b) {
        case eph::Body::Sun:               return "sun";
        case eph::Body::Moon:              return "moon";
        case eph::Body::MercuryBarycentre: return "mercury";
        case eph::Body::VenusBarycentre:   return "venus";
        case eph::Body::MarsBarycentre:    return "mars";
        case eph::Body::JupiterBarycentre: return "jupiter";
        case eph::Body::SaturnBarycentre:  return "saturn";
        case eph::Body::UranusBarycentre:  return "uranus";
        case eph::Body::NeptuneBarycentre: return "neptune";
        case eph::Body::PlutoBarycentre:   return "pluto";
        default: return {};
    }
}

ThirdBody::ThirdBody(eph::Body body, const eph::Ephemeris& ephemeris, const thirdbody::GravitationalParameters& gm,
                     const time::LeapTable& leaps, std::vector<SourceRecord> sources)
    : body_(body), ephemeris_(&ephemeris), gm_(&gm), leaps_(&leaps), sources_(std::move(sources)) {}

dyn::ForceId ThirdBody::id() const { return dyn::ForceId{"third_body." + std::string(third_body_name(body_))}; }

const std::vector<dyn::ParameterId>& ThirdBody::consumes() const { return consumes_; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
ThirdBody::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3&,
                 const dyn::ParameterSet&, const dyn::ParameterRegistry& registry) const {
    const std::string name = id().name;
    // The acceleration and the Jacobian both need the body's geocentric position in metres: one ephemeris call (the named crossing at the
    // ephemeris boundary, as `Attraction` does).
    auto mu = gm_->gm_m3_s2(body_);
    if (!mu) return odl::err(passthrough(name, mu.error()));
    auto st = ephemeris_->geocentric_state(body_, t, *leaps_);
    if (!st) return odl::err(passthrough(name, st.error()));
    const Vec3 s = odl::field_position_m_from_state_km(st->position());

    // The acceleration: L2's own function for one body-and-satellite pair, `Attraction::pair_stable` -- the very function `by_body` applies to
    // each body of its list.  `by_body` itself is not called, because it refuses (PERT-F-013) any list without both the Sun and the Moon, and a
    // plugin is ONE body; the registry-level rule is the factory's.  FMOD-A-004 holds the result to `by_body` of {Sun, Moon, this body}, 0 ulp.
    const Vec3 a = thirdbody::Attraction::pair_stable(r_m.metres(), s, *mu);

    // The Jacobian: the tidal tensor.
    const Vec3 d = s - r_m.metres();
    const double dn = d.norm();
    const double i3 = 1.0 / (dn * dn * dn);
    const double i5 = i3 / (dn * dn);
    const double dd[3] = {d.x, d.y, d.z};
    Mat3 da_dr;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            da_dr.r[i][j] = *mu * (3.0 * dd[i] * dd[j] * i5 - (i == j ? i3 : 0.0));

    SourceId sid("third_body");
    sid.add("body", third_body_name(body_)).add("gm", "gm_de440.tpc").add("ephemeris", "de440s");
    dyn::Provenance prov{sid.str(), sha256_list(sources_, {"naif-gm-de440", "de440s-spk"}), false};
    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{a},
        dyn::StateJacobian::no_velocity_dependence(da_dr, 0.0),
        dyn::ParameterJacobian(registry),
        prov,
    };
}

odl::Result<std::vector<std::shared_ptr<ThirdBody>>, ForceModelError>
third_bodies(const std::vector<eph::Body>& bodies, const eph::Ephemeris& ephemeris,
             const thirdbody::GravitationalParameters& gm, const time::LeapTable& leaps,
             const std::vector<SourceRecord>& sources) {
    const bool sun = std::find(bodies.begin(), bodies.end(), eph::Body::Sun) != bodies.end();
    const bool moon = std::find(bodies.begin(), bodies.end(), eph::Body::Moon) != bodies.end();
    if (!sun || !moon) {
        std::ostringstream m;
        m << "third-body attraction requested without " << (!sun && !moon ? "the Sun or the Moon" : (!sun ? "the Sun" : "the Moon"))
          << ". Their accelerations at 7331 km are about 3e-7 (Sun) and 1e-6 (Moon) m s^-2; treating them as optional like the planets "
             "would remove the largest non-gravitational-model terms (PERT-F-013, FMOD-F-001).";
        return odl::err(ForceModelError{"FMOD-F-001", m.str()});
    }
    std::vector<std::shared_ptr<ThirdBody>> out;
    for (eph::Body b : bodies) {
        if (third_body_name(b).empty()) {
            return odl::err(ForceModelError{"FMOD-F-001", std::string("'") + std::string(eph::name_of(b)) +
                                                           "' is not a third body (the Earth and the barycentres are not)"});
        }
        out.push_back(std::make_shared<ThirdBody>(b, ephemeris, gm, leaps, sources));
    }
    return out;
}

}  // namespace odl::forcemodel
