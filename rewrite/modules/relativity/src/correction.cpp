#include <odl/relativity/correction.hpp>

#include <odl/core/units.hpp>

#include <cmath>
#include <sstream>

namespace odl::relativity {

namespace {

/// The state's km and km/s into SI, through the ONE named crossing
/// (SPEC-frames FRAME-R-062).  Velocity too: `metres_from_km` is a scale, and
/// km/s to m/s is the same scale.
struct Si {
    Vec3 r;   ///< m
    Vec3 v;   ///< m/s
};

template <frames::Frame F>
Si si_of(const frames::State<F>& s) noexcept {
    return Si{odl::field_position_m_from_state_km(s.position()),
              odl::field_position_m_from_state_km(s.velocity())};
}

Vec3 schwarzschild(const Si& sat, const PpnParameters& ppn) noexcept {
    const double r = sat.r.norm();
    const double r3 = r * r * r;
    const double k = Correction::kGmEarth / (Correction::kC * Correction::kC * r3);
    const double a = 2.0 * (ppn.beta() + ppn.gamma()) * Correction::kGmEarth / r
                   - ppn.gamma() * sat.v.dot(sat.v);
    const double b = 2.0 * (1.0 + ppn.gamma()) * sat.r.dot(sat.v);
    return k * (a * sat.r + b * sat.v);
}

Vec3 lense_thirring(const Si& sat, const PpnParameters& ppn) noexcept {
    const Vec3 J{0.0, 0.0, Correction::kEarthAngularMomentumPerMass};
    const double r = sat.r.norm();
    const double r3 = r * r * r;
    const double k = (1.0 + ppn.gamma()) * Correction::kGmEarth
                   / (Correction::kC * Correction::kC * r3);
    const Vec3 first = (3.0 / (r * r)) * sat.r.dot(J) * sat.r.cross(sat.v);
    const Vec3 second = sat.v.cross(J);
    return k * (first + second);
}

Vec3 de_sitter(const Si& sat, const Si& earth, const PpnParameters& ppn) noexcept {
    // (1 + 2 gamma) { Rdot x (-GM_sun R / (c^2 R^3)) } x rdot.
    // The inner cross product is the geodesic precession RATE; crossing it with
    // the satellite velocity turns a precession into an acceleration.
    const double R = earth.r.norm();
    const double R3 = R * R * R;
    const Vec3 inner = (-Correction::kGmSun / (Correction::kC * Correction::kC * R3)) * earth.r;
    const Vec3 omega = earth.v.cross(inner);
    return (1.0 + 2.0 * ppn.gamma()) * omega.cross(sat.v);
}

odl::Result<int, RelativityError> checked(const frames::State<frames::Frame::GCRS>& sat,
                                          const frames::State<frames::Frame::BCRS>& earth) {
    const double r = sat.position().norm();
    const double R = earth.position().norm();
    if (!(r > 0.0) || !std::isfinite(r)) {
        std::ostringstream m;
        m << "the relativistic correction is not defined at a geocentric radius of " << r << " km";
        return odl::err(RelativityError{"PERT-F-001", m.str()});
    }
    if (!(R > 0.0) || !std::isfinite(R)) {
        std::ostringstream m;
        m << "the de Sitter term needs the Earth's position with respect to the SUN; "
             "the vector given has length " << R << " km. TN36-10 (10.12)'s R is heliocentric "
             "where its r is geocentric, and they appear in adjacent lines of one equation.";
        return odl::err(RelativityError{"PERT-F-001", m.str()});
    }
    return 0;
}

}  // namespace

odl::Result<PpnParameters, RelativityError> PpnParameters::of(double beta, double gamma) {
    if (!std::isfinite(beta) || !std::isfinite(gamma)) {
        std::ostringstream m;
        m << "PPN parameters must be finite; given beta = " << beta << ", gamma = " << gamma
          << ". General relativity is beta = gamma = 1 "
             "(PpnParameters::general_relativity()).";
        return odl::err(RelativityError{"PERT-F-009", m.str()});
    }
    return PpnParameters{beta, gamma};
}

odl::Result<std::vector<NamedAcceleration>, RelativityError>
Correction::by_term(const frames::State<frames::Frame::GCRS>& sat,
                    const frames::State<frames::Frame::BCRS>& earth_about_sun,
                    Terms terms, PpnParameters ppn) {
    auto ok = checked(sat, earth_about_sun);
    if (!ok) return odl::err(ok.error());
    const Si s = si_of(sat);
    const Si e = si_of(earth_about_sun);
    std::vector<NamedAcceleration> out;
    if (terms.schwarzschild) out.push_back({Term::Schwarzschild, schwarzschild(s, ppn)});
    if (terms.lense_thirring) out.push_back({Term::LenseThirring, lense_thirring(s, ppn)});
    if (terms.de_sitter) out.push_back({Term::DeSitter, de_sitter(s, e, ppn)});
    return out;
}

odl::Result<frames::Acceleration<frames::Frame::GCRS>, RelativityError>
Correction::acceleration(const frames::State<frames::Frame::GCRS>& sat,
                         const frames::State<frames::Frame::BCRS>& earth_about_sun,
                         Terms terms, PpnParameters ppn) {
    auto parts = by_term(sat, earth_about_sun, terms, ppn);
    if (!parts) return odl::err(parts.error());
    Vec3 total{};
    for (const auto& p : *parts) total = total + p.a_m_s2;
    return frames::Acceleration<frames::Frame::GCRS>{total};
}

}  // namespace odl::relativity
