#include <odl/forcemodel/relativity_force.hpp>

#include <odl/core/units.hpp>

#include <cmath>

namespace odl::forcemodel {

namespace {

/// The matrix S(a) with S(a) x = a x x.
Mat3 skew(const Vec3& a) {
    Mat3 s;
    s.r[0][0] = 0.0;   s.r[0][1] = -a.z;  s.r[0][2] = a.y;
    s.r[1][0] = a.z;   s.r[1][1] = 0.0;   s.r[1][2] = -a.x;
    s.r[2][0] = -a.y;  s.r[2][1] = a.x;   s.r[2][2] = 0.0;
    return s;
}

Mat3 outer(const Vec3& a, const Vec3& b) {
    const double x[3] = {a.x, a.y, a.z}, y[3] = {b.x, b.y, b.z};
    Mat3 m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = x[i] * y[j];
    return m;
}

Mat3 axpy(double alpha, const Mat3& a, const Mat3& b) {     // alpha a + b
    Mat3 m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = alpha * a.r[i][j] + b.r[i][j];
    return m;
}

Mat3 scaled_identity(double s) {
    Mat3 m;
    m.r[0][0] = m.r[1][1] = m.r[2][2] = s;
    return m;
}

const char* term_name(relativity::Term t) {
    switch (t) {
        case relativity::Term::Schwarzschild: return "schwarzschild";
        case relativity::Term::LenseThirring: return "lense_thirring";
        case relativity::Term::DeSitter:      return "de_sitter";
    }
    return "?";
}

}  // namespace

Relativity::Relativity(relativity::Term term, const eph::Ephemeris& ephemeris, const time::LeapTable& leaps,
                       relativity::PpnParameters ppn, std::vector<SourceRecord> sources)
    : term_(term), ephemeris_(&ephemeris), leaps_(&leaps), ppn_(ppn), sources_(std::move(sources)) {}

dyn::ForceId Relativity::id() const { return dyn::ForceId{std::string("relativity.") + term_name(term_)}; }

const std::vector<dyn::ParameterId>& Relativity::consumes() const { return consumes_; }

odl::Result<dyn::ForceEvaluation, dyn::DynError>
Relativity::accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m, const Vec3& v_m_per_s,
                  const dyn::ParameterSet&, const dyn::ParameterRegistry& registry) const {
    const std::string name = id().name;
    const Vec3 r = r_m.metres();
    const Vec3 v = v_m_per_s;

    // THE CROSSING, by name, once: the L2 API takes a typed State in kilometres.
    const frames::State<frames::Frame::GCRS> sat{t, odl::km_from_metres(r), odl::km_from_metres(v)};
    // The Earth about the Sun, from the typed State of the Sun about the Earth, negated: no crossing, no bare double through a helper.
    auto sun = ephemeris_->geocentric_state(eph::Body::Sun, t, *leaps_);
    if (!sun) return odl::err(passthrough(name, sun.error()));
    const frames::State<frames::Frame::BCRS> earth{t, -1.0 * sun->position(), -1.0 * sun->velocity()};

    auto parts = relativity::Correction::by_term(sat, earth, relativity::Terms::only(term_), ppn_);
    if (!parts) return odl::err(passthrough(name, parts.error()));
    if (parts->size() != 1) return odl::err(dyn::DynError{"FMOD-F-002", "forcemodel '" + name + "': by_term returned " + std::to_string(parts->size()) + " terms for one"});
    const Vec3 a = (*parts)[0].a_m_s2;

    using relativity::Correction;
    const double c2 = Correction::kC * Correction::kC;
    const double K = Correction::kGmEarth / c2;
    const double gamma = ppn_.gamma(), beta = ppn_.beta();
    const double rn = r.norm();
    const double i3 = 1.0 / (rn * rn * rn), i4 = i3 / rn, i5 = i4 / rn, i6 = i5 / rn, i7 = i6 / rn;
    Mat3 da_dr, da_dv;

    switch (term_) {
        case relativity::Term::Schwarzschild: {
            // a = K [ p GM r r^-4  -  gamma v^2 r r^-3  +  q (r.v) v r^-3 ],  p = 2 (beta + gamma),  q = 2 (1 + gamma)
            const double p = 2.0 * (beta + gamma), q = 2.0 * (1.0 + gamma);
            const double v2 = v.dot(v), rv = r.dot(v);
            const Mat3 rr = outer(r, r), vv = outer(v, v), vr = outer(v, r), rvT = outer(r, v);
            Mat3 t1 = axpy(-4.0 * i6, rr, scaled_identity(i4));                        // I r^-4 - 4 r r^T r^-6
            Mat3 t2 = axpy(-3.0 * i5, rr, scaled_identity(i3));                        // I r^-3 - 3 r r^T r^-5
            Mat3 t3 = axpy(-3.0 * rv * i5, vr, axpy(i3, vv, Mat3{}));                  // v v^T r^-3 - 3 (r.v) v r^T r^-5
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j)
                    da_dr.r[i][j] = K * (p * Correction::kGmEarth * t1.r[i][j] - gamma * v2 * t2.r[i][j] + q * t3.r[i][j]);
            const Mat3 d2 = rvT;                                                       // r v^T
            const Mat3 d3 = axpy(1.0, vr, scaled_identity(rv));                        // v r^T + (r.v) I
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j)
                    da_dv.r[i][j] = K * i3 * (-2.0 * gamma * d2.r[i][j] + q * d3.r[i][j]);
            break;
        }
        case relativity::Term::LenseThirring: {
            // a = (1+gamma) K [ 3 (r.J)(r x v) r^-5 + (v x J) r^-3 ],  J along the GCRS z-axis (the module's stated convention)
            const Vec3 J{0.0, 0.0, Correction::kEarthAngularMomentumPerMass};
            const double u = r.dot(J);
            const Vec3 w = r.cross(v);
            const Vec3 vxJ = v.cross(J);
            const double kk = (1.0 + gamma) * K;
            const Mat3 wJ = outer(w, J), wr = outer(w, r), vJr = outer(vxJ, r);
            const Mat3 sv = skew(v), sr = skew(r), sJ = skew(J);
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) {
                    da_dr.r[i][j] = kk * (3.0 * i5 * (wJ.r[i][j] - u * sv.r[i][j]) - 15.0 * u * wr.r[i][j] * i7 - 3.0 * vJr.r[i][j] * i5);
                    da_dv.r[i][j] = kk * (3.0 * u * i5 * sr.r[i][j] - i3 * sJ.r[i][j]);
                }
            break;
        }
        case relativity::Term::DeSitter: {
            // a = (1 + 2 gamma) omega x v,  omega = Rdot x ( -GM_S R / (c^2 R^3) ): independent of the satellite's position, linear in its velocity
            const Vec3 R = odl::metres_from_km(earth.position());
            const Vec3 Rdot = odl::metres_from_km(earth.velocity());
            const double Rn = R.norm();
            const Vec3 inner = (-Correction::kGmSun / (c2 * Rn * Rn * Rn)) * R;
            const Vec3 omega = Rdot.cross(inner);
            const Mat3 so = skew(omega);
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j) da_dv.r[i][j] = (1.0 + 2.0 * gamma) * so.r[i][j];
            break;     // da_dr stays the zero matrix: exactly zero, not a forgotten block
        }
    }

    SourceId sid("relativity");
    sid.add("term", term_name(term_)).add("ppn", ppn_.is_general_relativity() ? "GR" : "non-GR").add("earth_about_sun", "de440s");
    dyn::Provenance prov{sid.str(), sha256_list(sources_, {"de440s-spk"}), false};
    return dyn::ForceEvaluation{
        frames::Acceleration<frames::Frame::GCRS>{a},
        dyn::StateJacobian::with_velocity(da_dr, da_dv),
        dyn::ParameterJacobian(registry),
        prov,
    };
}

}  // namespace odl::forcemodel
