#include <odl/thirdbody/attraction.hpp>

#include <odl/core/units.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace odl::thirdbody {

namespace {

// THE SECOND NUMBERING SCHEME.  `body.hpp` warns that CALCEPH's classic scheme
// and the NAIF scheme disagree on exactly the ids this module needs — classic
// 10 is the MOON and NAIF 10 is the SUN — and gm_de440.tpc is in NAIF ids where
// everything else in this tree is in classic ones.  So the mapping is written
// out here rather than reused, and the loader then CHECKS it against TN36-1
// (see `cross_check`): a 10/11 confusion makes GM_sun equal GM_moon, which is
// wrong by a factor of 2.7e7 and cannot survive the check.
//
// The planets are their SYSTEM BARYCENTRES (NAIF 1..9), matching the positions
// `Body::*Barycentre` asks the ephemeris for.  Using BODY499_GM — Mars alone —
// with the Mars barycentre's position would pair a mass with a position that is
// not its own.
constexpr int naif_id(eph::Body b) noexcept {
    switch (b) {
        case eph::Body::MercuryBarycentre: return 1;
        case eph::Body::VenusBarycentre:   return 2;
        case eph::Body::MarsBarycentre:    return 4;
        case eph::Body::JupiterBarycentre: return 5;
        case eph::Body::SaturnBarycentre:  return 6;
        case eph::Body::UranusBarycentre:  return 7;
        case eph::Body::NeptuneBarycentre: return 8;
        case eph::Body::PlutoBarycentre:   return 9;
        case eph::Body::Sun:               return 10;
        case eph::Body::Moon:              return 301;
        case eph::Body::Earth:             return 399;
        case eph::Body::EarthMoonBarycentre: return 3;
        case eph::Body::SolarSystemBarycentre: return 0;
    }
    return -1;
}

// TN36-1, for the cross-check only.
constexpr double kGmSunTn36 = 1.32712442099e20;      ///< m^3/s^2
constexpr double kGmEarthTn36 = 3.986004418e14;      ///< m^3/s^2
constexpr double kMoonEarthMassRatio = 0.0123000371;
constexpr double kLb = 1.550519768e-8;               ///< TN36-1, TDB/TCB rate

const std::regex& gm_line() {
    static const std::regex re(R"(BODY(\d+)_GM\s*=\s*\(\s*([-+0-9.]+[EeDd][-+]?\d+)\s*\))");
    return re;
}

double fortran_double(std::string s) {
    for (char& c : s) {
        if (c == 'D' || c == 'd') c = 'E';
    }
    return std::stod(s);
}

}  // namespace

odl::Result<GravitationalParameters, ThirdBodyError>
GravitationalParameters::load(const std::string& path, const std::string& cache_root) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(fs::path(path), ec);
    const fs::path root = fs::weakly_canonical(fs::path(cache_root), ec);
    const auto rel = canonical.lexically_relative(root);
    if (root.empty() || rel.empty() || *rel.begin() == "..") {
        std::ostringstream m;
        m << "the gravitational-parameter file is outside the manifest cache and will not be "
             "read.\n  requested  " << canonical.string()
          << "\n  cache root " << root.string()
          << "\n  Every external input is declared with a URL and a SHA-256 (plan rule R11).";
        return odl::err(ThirdBodyError{"PERT-F-010", m.str()});
    }

    std::ifstream f(canonical);
    if (!f) {
        return odl::err(ThirdBodyError{"PERT-F-010", "cannot open " + canonical.string()});
    }
    GravitationalParameters out;
    out.path_ = canonical.string();
    std::string line;
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, gm_line())) {
            out.by_naif_.emplace_back(std::stoi(m[1].str()), fortran_double(m[2].str()));
        }
    }
    if (out.by_naif_.size() < 20) {
        std::ostringstream m;
        m << "only " << out.by_naif_.size() << " BODY<id>_GM assignments were found in "
          << canonical.string()
          << "; gm_de440.tpc carries more than twenty. Either the file is not what it says or "
             "the parse is reading the wrong thing, and a short parse that still returns the Sun "
             "would be invisible.";
        return odl::err(ThirdBodyError{"PERT-F-012", m.str()});
    }

    // CROSS-CHECK AGAINST TN36-1, which is pinned independently of this file.
    // It catches a NAIF/classic id confusion (Sun against Moon is a factor of
    // 2.7e7) and a km/m slip (a factor of 1e9).
    auto value = [&](int id) -> double {
        for (const auto& [k, v] : out.by_naif_) {
            if (k == id) return v * 1e9;              // km^3/s^2 -> m^3/s^2
        }
        return 0.0;
    };
    const double sun = value(10), earth = value(399), moon = value(301);
    const double sun_rel = std::abs(sun - kGmSunTn36) / kGmSunTn36;
    const double moon_want = kMoonEarthMassRatio * earth;
    const double moon_rel = std::abs(moon - moon_want) / moon_want;
    // The Sun's GM disagrees with TN36-1 by almost exactly L_B = 1.5505e-8 —
    // the TDB/TCB rate — because the two are compatible with different time
    // scales. That is a convention difference and not an error, so the band is
    // set to admit it and nothing wider.
    if (!(sun_rel < 3.0 * kLb) || !(moon_rel < 1e-6)) {
        std::ostringstream m;
        m.precision(12);
        m << "the parsed gravitational parameters do not agree with TN36-1.\n"
             "  GM_sun  parsed " << sun << " m^3/s^2, TN36-1 " << kGmSunTn36
          << ", relative difference " << sun_rel << " (expected about L_B = " << kLb << ")\n"
             "  GM_moon parsed " << moon << " m^3/s^2, against mu*GM_earth = " << moon_want
          << ", relative difference " << moon_rel << "\n"
             "  The likely causes are a NAIF/classic id confusion — classic 10 is the MOON and "
             "NAIF 10 is the SUN — or km against m.";
        return odl::err(ThirdBodyError{"PERT-F-012", m.str()});
    }
    return out;
}

odl::Result<double, ThirdBodyError> GravitationalParameters::gm_m3_s2(eph::Body b) const {
    const int id = naif_id(b);
    for (const auto& [k, v] : by_naif_) {
        if (k == id) return v * 1e9;
    }
    std::ostringstream m;
    m << "no gravitational parameter is pinned for " << eph::name_of(b) << " (NAIF id " << id
      << ").\n  searched " << path_ << ", which carries " << by_naif_.size() << " bodies.\n"
         "  A third-body term with a guessed GM is a wrong number of the right size, which is "
         "worse than no term at all.";
    return odl::err(ThirdBodyError{"PERT-F-012", m.str()});
}

Vec3 Attraction::pair_direct(const Vec3& sat_m, const Vec3& body_m, double gm) noexcept {
    const Vec3 d = body_m - sat_m;
    const double dn = d.norm(), bn = body_m.norm();
    return gm * ((1.0 / (dn * dn * dn)) * d - (1.0 / (bn * bn * bn)) * body_m);
}

Vec3 Attraction::pair_stable(const Vec3& sat_m, const Vec3& body_m, double gm) noexcept {
    // Derived here, not cited. With d = r - r_b and |d|^2 = |r_b|^2 (1 + q),
    //   a = -GM/|d|^3 [ r + f(q) r_b ],  f(q) = (1+q)^(3/2) - 1,
    // and (1+q)^(3/2) - 1 is evaluated in the form that does not subtract two
    // near-equal numbers when q is small:
    //   f(q) = q(3 + 3q + q^2) / (1 + (1+q)^(3/2)),
    // because [(1+q)^(3/2) - 1][(1+q)^(3/2) + 1] = (1+q)^3 - 1 = q(3 + 3q + q^2).
    const double bn2 = body_m.dot(body_m);
    const double q = (sat_m.dot(sat_m) - 2.0 * sat_m.dot(body_m)) / bn2;
    const double root = std::sqrt((1.0 + q) * (1.0 + q) * (1.0 + q));
    const double f = q * (3.0 + 3.0 * q + q * q) / (1.0 + root);
    const Vec3 d = sat_m - body_m;
    const double dn = d.norm();
    return (-gm / (dn * dn * dn)) * (sat_m + f * body_m);
}

odl::Result<std::vector<NamedAcceleration>, ThirdBodyError>
Attraction::by_body(const frames::Position<frames::Frame::GCRS>& sat,
                    const std::vector<eph::Body>& bodies, const eph::Ephemeris& ephemeris,
                    const GravitationalParameters& gm, const odl::time::Epoch& when,
                    const odl::time::LeapTable& leaps) {
    const bool has_sun = std::find(bodies.begin(), bodies.end(), eph::Body::Sun) != bodies.end();
    const bool has_moon = std::find(bodies.begin(), bodies.end(), eph::Body::Moon) != bodies.end();
    if (!has_sun || !has_moon) {
        std::ostringstream m;
        m << "the Sun and the Moon are not optional third bodies for an Earth satellite "
             "(PERT-R-052); the list given "
          << (has_sun ? "has the Sun but not the Moon" : has_moon ? "has the Moon but not the Sun"
                                                                  : "has neither")
          << ". Their accelerations at 7331 km are about 5e-6 and 1e-5 m/s^2, which is four "
             "orders of magnitude above anything else in L2.";
        return odl::err(ThirdBodyError{"PERT-F-013", m.str()});
    }
    std::vector<NamedAcceleration> out;
    out.reserve(bodies.size());
    for (eph::Body b : bodies) {
        auto mu = gm.gm_m3_s2(b);
        if (!mu) return odl::err(mu.error());
        // ONE call, centred on the Earth.  Differencing two barycentric vectors
        // would subtract two 1.5e8 km quantities to get a 3.8e5 km one.
        auto st = ephemeris.geocentric_state(b, when, leaps);
        if (!st) return odl::err(ThirdBodyError{st.error().id, st.error().message});
        const Vec3 body_m = odl::field_position_m_from_state_km(st->position());
        out.push_back({b, pair_stable(sat.metres(), body_m, *mu)});
    }
    return out;
}

odl::Result<frames::Acceleration<frames::Frame::GCRS>, ThirdBodyError>
Attraction::acceleration(const frames::Position<frames::Frame::GCRS>& sat,
                         const std::vector<eph::Body>& bodies, const eph::Ephemeris& ephemeris,
                         const GravitationalParameters& gm, const odl::time::Epoch& when,
                         const odl::time::LeapTable& leaps) {
    auto parts = by_body(sat, bodies, ephemeris, gm, when, leaps);
    if (!parts) return odl::err(parts.error());
    Vec3 total{};
    for (const auto& p : *parts) total = total + p.a_m_s2;
    return frames::Acceleration<frames::Frame::GCRS>{total};
}

}  // namespace odl::thirdbody
