#pragma once
// fixture.hpp — what the forcemodel tests share (SPEC-forcemodel.md §8, "The fixture"): the pinned data loaded once, and L4's four reference
// points exactly as tests/l4_ranking.cpp states them.

#include <catch2/catch_test_macros.hpp>

#include <erfa.h>

#include <odl/core/units.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/forcemodel/earth_orientation.hpp>
#include <odl/forcemodel/gravity_force.hpp>
#include <odl/forcemodel/relativity_force.hpp>
#include <odl/forcemodel/third_body_force.hpp>
#include <odl/forcemodel/tides_force.hpp>
#include <odl/forcemodel/truncation.hpp>
#include <odl/gravity/field.hpp>
#include <odl/gravity/scaling.hpp>
#include <odl/tides/ocean.hpp>
#include <odl/tides/pole.hpp>
#include <odl/thirdbody/attraction.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fx {

using namespace odl;

inline std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

inline const odl::time::LeapTable& leaps() {
    static const auto t = odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE),
                                                      odl::time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    REQUIRE(t.has_value());
    return *t;
}

inline const eph::Ephemeris& ephemeris() {
    static const auto e = eph::Ephemeris::open({ODL_DE440S_BSP}, {});
    REQUIRE(e.has_value());
    return *e;
}

inline const thirdbody::GravitationalParameters& gm() {
    static const auto g = thirdbody::GravitationalParameters::load(ODL_GM_DE440_TPC, ODL_MANIFEST_CACHE_ROOT);
    REQUIRE(g.has_value());
    return *g;
}

inline const gravity::GravityModel& model() {
    static const auto m = gravity::GravityModel::load(ODL_EGM2008_COEFFICIENTS, ODL_MANIFEST_CACHE_ROOT,
                                                      gravity::ScalingParameters::egm2008_tt_compatible());
    REQUIRE(m.has_value());
    return *m;
}

inline const eop::EopSeries& c04() {
    static const eop::EopSeries s = [] {
        auto r = eop::EopSeries::load_c04(slurp(ODL_C04_FILE), eop::EopProvenance{"eop-c04-20", "", "", ""}, leaps());
        if (!r.has_value()) FAIL("C04: " << r.error().message);
        return *r;
    }();
    return s;
}

inline const tides::OceanTide& fes() {
    static const auto o = tides::OceanTide::load(ODL_FES2004_CNM_SNM, ODL_MANIFEST_CACHE_ROOT);
    REQUIRE(o.has_value());
    return *o;
}

inline const tides::OceanPoleTide& desai() {
    static const auto o = tides::OceanPoleTide::load(ODL_DESAI_POLE_COEF, ODL_MANIFEST_CACHE_ROOT);
    REQUIRE(o.has_value());
    return *o;
}

/// What the tests tell the plugins they read (the manifest's hashes, as compile definitions).
inline std::vector<forcemodel::SourceRecord> sources() {
    return {{"egm2008-coefficients", ODL_EGM2008_SHA256}, {"naif-gm-de440", ODL_GM_DE440_SHA256},
            {"de440s-spk", ODL_DE440S_SHA256},           {"eop-c04-20", ODL_C04_SHA256},
            {"fes2004-ocean-tide", ODL_FES2004_SHA256},  {"desai-ocean-pole-tide", ODL_DESAI_SHA256}};
}

/// L4's four reference points, exactly tests/l4_ranking.cpp's: a circular orbit in the GCRS xy-plane, phase from +X.
struct Point {
    const char* name;
    double r_m;
    int year, month, day, hour;
    double phase_rad;
};

inline const std::array<Point, 4>& points() {
    static const std::array<Point, 4> p{{
        {"GPS 26561 km", 26561e3, 2023, 2, 20, 0, 0.7},
        {"LEO 300 km", 6378136.3 + 300e3, 2023, 6, 21, 8, 0.0},
        {"LEO 952.86 km", 6378136.3 + 952.86e3, 2023, 6, 21, 8, 0.0},
        {"sail 720 km", 6378136.3 + 720e3, 2019, 7, 1, 0, 1.2},
    }};
    return p;
}

inline odl::time::Epoch epoch_of(const Point& p) {
    odl::time::Calendar c;
    c.year = p.year; c.month = p.month; c.day = p.day; c.hour = p.hour; c.minute = 0; c.second = 0.0;
    auto e = odl::time::Epoch::from_calendar(odl::time::TimeScale::UTC, c, leaps());
    REQUIRE(e.has_value());
    return *e;
}

inline constexpr double kGmFixture = 3.986004415e14;     // the ranking's own, EGM2008's TT-compatible

struct Orbit {
    Vec3 r, v;
};

inline Orbit circular_orbit(double r_m, double phase_rad) {
    const Vec3 rh{std::cos(phase_rad), std::sin(phase_rad), 0.0};
    const Vec3 th{-std::sin(phase_rad), std::cos(phase_rad), 0.0};
    return Orbit{r_m * rh, std::sqrt(kGmFixture / r_m) * th};
}

inline frames::State<frames::Frame::GCRS> state_of(const Point& p) {
    const Orbit o = circular_orbit(p.r_m, p.phase_rad);
    return frames::State<frames::Frame::GCRS>{epoch_of(p), odl::km_from_metres(o.r), odl::km_from_metres(o.v)};
}

inline gravity::Degree deg(int n) { auto d = gravity::Degree::of(n); REQUIRE(d.has_value()); return *d; }
inline gravity::Order ord(int m) { auto o = gravity::Order::of(m); REQUIRE(o.has_value()); return *o; }

inline forcemodel::EarthOrientation orientation_series() {
    return forcemodel::EarthOrientation::from_series(c04(), eop::EopPolicy{}, leaps());
}

/// A fixed record at `t` with the celestial-pole offsets ZEROED, as FRAME-A-005 does, so that `eraC2t06a` is a like-for-like witness.
inline eop::EopRecord record_dxdy_zero(const odl::time::Epoch& t) {
    auto r = c04().at(t, eop::EopPolicy{});
    REQUIRE(r.has_value());
    eop::EopRecord rec = *r;
    rec.dx = 0.0;
    rec.dy = 0.0;
    return rec;
}

/// ONE rotation, as three rows, for an independent chain: `eraC2t06a` (GCRS -> terrestrial, dX = dY = 0 NOT applied by the call) assembled in the
/// test, not through `frames` (FMOD-A-002).  `record.dx, dy` must be zero for the comparison to be like-for-like.
inline odl::Mat3 erfa_matrix(const odl::time::Epoch& t, const eop::EopRecord& rec) {
    auto tt = t.two_part_jd(odl::time::TimeScale::TT, leaps());
    auto ut1 = t.ut1_two_part_jd(odl::time::Duration::from_seconds(rec.dut1), leaps());
    REQUIRE(tt.has_value());
    REQUIRE(ut1.has_value());
    double ref[3][3];
    eraC2t06a(tt->day, tt->fraction, ut1->day, ut1->fraction, rec.xp, rec.yp, ref);
    odl::Mat3 m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = ref[i][j];
    return m;
}

/// The Earth-rotation angle at `t`, degrees (the transposed-rotation control's power is 2 |sin| of it).
inline double era_degrees(const odl::time::Epoch& t, const eop::EopRecord& rec) {
    auto ut1 = t.ut1_two_part_jd(odl::time::Duration::from_seconds(rec.dut1), leaps());
    REQUIRE(ut1.has_value());
    return eraEra00(ut1->day, ut1->fraction) * 180.0 / 3.14159265358979323846;
}

/// The tide increments a configuration produces at `t`, built HERE from L2's models by the specification's own recipe, with the rotation
/// `m` GIVEN (the plugin's chain for the finite-difference gate, an independent one for the registry gate): the Sun and Moon in the
/// ITRS by the test's own crossing (`* 1000.0`), the six arguments from TT and UT1, the wobble from the field's secular pole, each model, the sum
/// ONCE.  Returns the models too, in the order the plugin's `by_model` reports them.
struct IndependentTides {
    std::vector<std::pair<std::string, odl::tides::TideIncrements>> parts;
    odl::tides::TideIncrements total;
};
inline IndependentTides independent_tides(const odl::time::Epoch& t, const eop::EopRecord& rec, const odl::Mat3& m,
                                          const odl::forcemodel::TidesOptions& o, const odl::gravity::ConventionalField& field) {
    auto tt = t.two_part_jd(odl::time::TimeScale::TT, leaps());
    auto ut1 = t.ut1_two_part_jd(odl::time::Duration::from_seconds(rec.dut1), leaps());
    REQUIRE(tt.has_value());
    REQUIRE(ut1.has_value());
    const auto args = odl::eop::tides::arguments_at(tt->day, tt->fraction, ut1->day, ut1->fraction);
    std::vector<std::pair<std::string, odl::tides::TideIncrements>> parts;
    if (o.solid) {
        auto sun = ephemeris().geocentric_state(odl::eph::Body::Sun, t, leaps());
        auto moon = ephemeris().geocentric_state(odl::eph::Body::Moon, t, leaps());
        REQUIRE(sun.has_value());
        REQUIRE(moon.has_value());
        const odl::frames::Position<odl::frames::Frame::ITRS> s{m.apply(1000.0 * sun->position())};
        const odl::frames::Position<odl::frames::Frame::ITRS> mo{m.apply(1000.0 * moon->position())};
        auto inc = odl::tides::SolidEarthTide::increments(s, mo, args, odl::tides::LoveNumbers::Anelastic, o.solid_target);
        REQUIRE(inc.has_value());
        parts.emplace_back("solid_earth_tide", std::move(*inc));
    }
    if (o.ocean && o.ocean_degree >= 2) {
        auto inc = o.ocean->increments(args, o.ocean_degree, o.ocean_degree);
        REQUIRE(inc.has_value());
        parts.emplace_back("ocean_tide", std::move(*inc));
    }
    const odl::tides::Wobble w = odl::tides::Wobble::from(rec.xp, rec.yp, field.pole());
    if (o.solid_pole) {
        auto inc = odl::tides::SolidEarthPoleTide::increments(w);
        REQUIRE(inc.has_value());
        parts.emplace_back("solid_pole_tide", std::move(*inc));
    }
    if (o.ocean_pole && o.ocean_pole_degree >= 2) {
        auto inc = o.ocean_pole->increments(w, o.ocean_pole_degree);
        REQUIRE(inc.has_value());
        parts.emplace_back("ocean_pole_tide", std::move(*inc));
    }
    REQUIRE(!parts.empty());
    std::vector<const odl::tides::TideIncrements*> ptrs;
    for (const auto& p : parts) ptrs.push_back(&p.second);
    auto total = odl::tides::TideIncrements::sum(ptrs);
    REQUIRE(total.has_value());
    return IndependentTides{std::move(parts), std::move(*total)};
}

/// The registered sets of SPEC-forcemodel §3.4: set A (the Sun, the Moon, `tides`, the three relativity terms) or set B (A and the five planets whose peak
/// tidal acceleration at LEO exceeds 1e-13 m s^-2), with `Gravity` at `gravity_degree` if it is positive.  The tides carry the ocean tide at
/// `degree_meeting_criterion` for the point's radius and the ocean pole tide to degree 10.
inline void add_registered_set(odl::dyn::ForceSet& fs, const Point& p, bool planets, int gravity_degree) {
    const auto eo = orientation_series();
    if (gravity_degree > 0) {
        forcemodel::GravityOptions go;
        go.extrapolate_secular = true;
        go.sources = sources();
        REQUIRE(fs.add(std::make_shared<forcemodel::Gravity>(model(), eo, deg(gravity_degree), ord(gravity_degree), go)).has_value());
    }
    std::vector<eph::Body> bodies{eph::Body::Sun, eph::Body::Moon};
    if (planets)
        for (auto b : {eph::Body::MercuryBarycentre, eph::Body::VenusBarycentre, eph::Body::MarsBarycentre, eph::Body::JupiterBarycentre,
                       eph::Body::SaturnBarycentre})
            bodies.push_back(b);
    auto tb = forcemodel::third_bodies(bodies, ephemeris(), gm(), leaps(), sources());
    REQUIRE(tb.has_value());
    for (auto& f : *tb) REQUIRE(fs.add(f).has_value());
    for (auto term : {relativity::Term::Schwarzschild, relativity::Term::LenseThirring, relativity::Term::DeSitter})
        REQUIRE(fs.add(std::make_shared<forcemodel::Relativity>(term, ephemeris(), leaps(), relativity::PpnParameters::general_relativity(), sources()))
                    .has_value());
    forcemodel::TidesOptions to;
    to.ocean = &fes();
    to.ocean_degree = fes().degree_meeting_criterion(p.r_m);
    to.ocean_pole = &desai();
    to.ocean_pole_degree = 10;
    to.extrapolate_secular = true;
    to.sources = sources();
    REQUIRE(fs.add(std::make_shared<forcemodel::Tides>(model(), eo, ephemeris(), leaps(), to)).has_value());
}

}  // namespace fx
