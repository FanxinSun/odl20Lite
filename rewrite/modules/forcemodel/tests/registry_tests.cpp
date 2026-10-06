// registry_tests.cpp — SPEC-forcemodel.md §8, FMOD-A-001 … -008 and -030: the plugins through the registry, each wrapper's rotation and crossing
// checked INDEPENDENTLY of the wrapper, L2's gated values reproduced through the registry, the declarations, the refusals, and the size of the
// Earth-orientation handling over an arc.
//
// EVERY tolerance, case list and case count below is registered in the specification (commit 4f6f5b6) BEFORE this file existed.
#include "fixture.hpp"

#include <odl/relativity/correction.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

using namespace odl;

namespace {

constexpr double kEps = 2.220446049250313e-16;            // 2^-52
constexpr double kRotationBound = 3.0e-10;                // FMOD-P-3: from FRAME-A-005's 5 uas

/// One registry at one point.  Not movable: the ForceSet holds a pointer to the registry.
struct Set {
    dyn::ParameterRegistry reg;
    dyn::ForceSet fs{reg};
};

std::shared_ptr<forcemodel::Gravity> make_gravity(const forcemodel::EarthOrientation& eo, int n, int m) {
    forcemodel::GravityOptions o;
    o.extrapolate_secular = true;             // the epochs are after 2017; recorded in the provenance
    o.sources = fx::sources();
    return std::make_shared<forcemodel::Gravity>(fx::model(), eo, fx::deg(n), fx::ord(m), o);
}

forcemodel::TidesOptions default_tides(const fx::Point& p) {
    forcemodel::TidesOptions o;
    o.ocean = &fx::fes();
    o.ocean_degree = fx::fes().degree_meeting_criterion(p.r_m);
    o.ocean_pole = &fx::desai();
    o.ocean_pole_degree = 10;
    o.extrapolate_secular = true;
    o.sources = fx::sources();
    return o;
}

const std::vector<eph::Body>& planets_b() {
    static const std::vector<eph::Body> v{eph::Body::Sun, eph::Body::Moon, eph::Body::MercuryBarycentre, eph::Body::VenusBarycentre,
                                          eph::Body::MarsBarycentre, eph::Body::JupiterBarycentre, eph::Body::SaturnBarycentre};
    return v;
}

void build_full(Set& s, const fx::Point& p, int degree = 36) {
    const auto eo = fx::orientation_series();
    REQUIRE(s.fs.add(make_gravity(eo, degree, degree)).has_value());
    auto tb = forcemodel::third_bodies(planets_b(), fx::ephemeris(), fx::gm(), fx::leaps(), fx::sources());
    REQUIRE(tb.has_value());
    for (auto& f : *tb) REQUIRE(s.fs.add(f).has_value());
    for (auto term : {relativity::Term::Schwarzschild, relativity::Term::LenseThirring, relativity::Term::DeSitter})
        REQUIRE(s.fs.add(std::make_shared<forcemodel::Relativity>(term, fx::ephemeris(), fx::leaps(), relativity::PpnParameters::general_relativity(),
                                                                   fx::sources())).has_value());
    REQUIRE(s.fs.add(std::make_shared<forcemodel::Tides>(fx::model(), eo, fx::ephemeris(), fx::leaps(), default_tides(p))).has_value());
}

dyn::ForceEvaluation evaluate(const dyn::Force& f, const odl::time::Epoch& t, const Vec3& r, const Vec3& v) {
    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);
    auto ev = f.accel(t, frames::Position<frames::Frame::GCRS>{r}, v, params, reg);
    if (!ev) FAIL("accel refused: " << ev.error().id << ": " << ev.error().message);
    return *ev;
}

Vec3 acc(const dyn::ForceEvaluation& e) { return e.acceleration.metres_per_second_squared(); }

}  // namespace

TEST_CASE("FMOD-A-001: the registry carries every plugin, attributed by the stated names, at L4's four points", "[forcemodel][registry]") {
    const std::set<std::string> expected{"gravity", "third_body.sun", "third_body.moon", "third_body.mercury", "third_body.venus", "third_body.mars",
                                         "third_body.jupiter", "third_body.saturn", "tides", "relativity.schwarzschild",
                                         "relativity.lense_thirring", "relativity.de_sitter"};
    REQUIRE(expected.size() == 12);
    int total = 0;
    for (const auto& p : fx::points()) {
        Set s;
        build_full(s, p);
        REQUIRE(s.fs.size() == 12);
        dyn::ParameterSet params(s.reg);
        auto c = s.fs.contributions_at(fx::epoch_of(p), fx::state_of(p), params);
        REQUIRE(c.has_value());
        REQUIRE(c->size() == 12);
        std::set<std::string> got;
        for (const auto& x : *c) {
            got.insert(x.force.name);
            const double n = x.acceleration_m_s2.norm();
            CHECK(std::isfinite(n));
            CHECK(n > 0.0);
            ++total;
        }
        CHECK(got == expected);
    }
    REQUIRE(total == 48);
    WARN("FMOD-A-001: " << total << " attributed contributions (4 points x 12 plugins), every one finite and non-zero, the names as stated");
}

TEST_CASE("FMOD-A-002: Gravity's rotation against eraC2t06a assembled in the test, and the transposed-rotation control", "[forcemodel][registry][gravity]") {
    int cases = 0;
    double worst = 0.0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        const eop::EopRecord rec = fx::record_dxdy_zero(t);
        const Mat3 rp = fx::erfa_matrix(t, rec);                          // the independent chain
        forcemodel::GravityOptions go;
        go.extrapolate_secular = true;
        go.sources = fx::sources();
        const forcemodel::Gravity plugin(fx::model(), forcemodel::EarthOrientation::from_record(rec, fx::leaps()), fx::deg(36), fx::ord(36), go);
        const Vec3 a_plugin = acc(evaluate(plugin, t, o.r, o.v));

        auto field = fx::model().conventional(t, true);
        REQUIRE(field.has_value());
        auto a_itrs = field->acceleration(frames::ItrsPosition{rp.apply(o.r)}, fx::deg(36), fx::ord(36));
        REQUIRE(a_itrs.has_value());
        const Vec3 a_ref = rp.transpose().apply(a_itrs->metres_per_second_squared());
        const double rel = (a_plugin - a_ref).norm() / a_ref.norm();
        worst = std::max(worst, rel);
        CHECK(rel <= kRotationBound);

        // The control: the transposed rotation, forward and back (the defect FMOD-P-3's bound is there to catch).
        auto a_wrong_itrs = field->acceleration(frames::ItrsPosition{rp.transpose().apply(o.r)}, fx::deg(36), fx::ord(36));
        REQUIRE(a_wrong_itrs.has_value());
        const Vec3 a_wrong = rp.apply(a_wrong_itrs->metres_per_second_squared());
        const double power = (a_wrong - a_plugin).norm() / (a_ref.norm() * kRotationBound);
        CHECK(power >= 1.0e3);
        const double era = fx::era_degrees(t, rec);
        WARN("FMOD-A-002 " << p.name << ": |plugin - independent| / |a| = " << rel << " (bound 3e-10); the transposed control differs by "
             << (a_wrong - a_plugin).norm() / a_ref.norm() << " of |a| = " << power << " x the bound; ERA " << era << " deg, 2|sin| = "
             << 2.0 * std::abs(std::sin(era * 3.14159265358979323846 / 180.0)));
        ++cases;
    }
    REQUIRE(cases == 4);
    WARN("FMOD-A-002: worst relative difference " << worst << " over " << cases << " points (bound 3e-10)");
}

TEST_CASE("FMOD-A-003: L2's gated values reproduced through the plugin -- GRAV-A-012's pole anchor and GRAV-A-027's J2 closed form",
          "[forcemodel][registry][gravity]") {
    const auto& p = fx::points()[1];
    const auto t = fx::epoch_of(p);
    const eop::EopRecord rec = fx::record_dxdy_zero(t);
    const Mat3 rp = fx::erfa_matrix(t, rec);
    const forcemodel::EarthOrientation eo = forcemodel::EarthOrientation::from_record(rec, fx::leaps());
    auto field = fx::model().conventional(t, true);
    REQUIRE(field.has_value());

    // (a) GRAV-A-012: |a| on the polar axis at r = b, WGS 84's semi-minor axis, at full degree 2190, order 2159.
    {
        constexpr double kB = 6356752.3142;
        const Vec3 x_itrs{0.0, 0.0, kB};
        const Vec3 r_gcrs = rp.transpose().apply(x_itrs);
        forcemodel::GravityOptions go;
        go.extrapolate_secular = true;
        const forcemodel::Gravity plugin(fx::model(), eo, fx::deg(2190), fx::ord(2159), go);
        const Vec3 a_plugin = acc(evaluate(plugin, t, r_gcrs, Vec3{0.0, 0.0, 0.0}));
        auto a_direct = field->acceleration(frames::ItrsPosition{x_itrs}, fx::deg(2190), fx::ord(2159));
        REQUIRE(a_direct.has_value());
        const Vec3 a_ref = rp.transpose().apply(a_direct->metres_per_second_squared());
        WARN("FMOD-A-003 (a): |a| at the pole through the plugin = " << a_plugin.norm() << " m/s^2 (GRAV-A-012: 9.8321849379 +- 0.005); "
             "against the field's direct value " << (a_plugin - a_ref).norm() / a_ref.norm());
        CHECK(std::abs(a_plugin.norm() - 9.8321849379) <= 0.005);
        CHECK((a_plugin - a_ref).norm() <= kRotationBound * a_ref.norm());
    }
    // (b) GRAV-A-027: the field truncated to degree 2, order 0 against the exact J2-only closed form, three latitudes.
    {
        const double gm = field->scaling().gm_m3_s2(), ae = field->scaling().ae_m();
        const double j2 = -std::sqrt(5.0) * field->c(2, 0);
        constexpr double kR = 7331e3;
        const double lat_lon[3][2] = {{0.0, 20.0}, {45.0, -75.0}, {80.0, 140.0}};
        const forcemodel::Gravity plugin(fx::model(), eo, fx::deg(2), fx::ord(0), forcemodel::GravityOptions{true, fx::sources()});
        int cases = 0;
        for (const auto& ll : lat_lon) {
            const double la = ll[0] * 3.14159265358979323846 / 180.0, lo = ll[1] * 3.14159265358979323846 / 180.0;
            const Vec3 x{kR * std::cos(la) * std::cos(lo), kR * std::cos(la) * std::sin(lo), kR * std::sin(la)};
            const double z2 = x.z * x.z / (kR * kR);
            const double f = 1.5 * j2 * (ae / kR) * (ae / kR);
            const Vec3 a_closed{-gm / (kR * kR * kR) * x.x * (1.0 + f * (1.0 - 5.0 * z2)),
                                -gm / (kR * kR * kR) * x.y * (1.0 + f * (1.0 - 5.0 * z2)),
                                -gm / (kR * kR * kR) * x.z * (1.0 + f * (3.0 - 5.0 * z2))};
            const Vec3 a_ref = rp.transpose().apply(a_closed);
            const Vec3 a_plugin = acc(evaluate(plugin, t, rp.transpose().apply(x), Vec3{0.0, 0.0, 0.0}));
            const double rel = (a_plugin - a_ref).norm() / a_ref.norm();
            WARN("FMOD-A-003 (b): lat " << ll[0] << " deg: |plugin(2,0) - J2 closed form| / |a| = " << rel << " (bound 3e-10)");
            CHECK(rel <= kRotationBound);
            ++cases;
        }
        REQUIRE(cases == 3);
    }
}

TEST_CASE("FMOD-A-004: third bodies -- equal to by_body, to pair_direct in metres, the indirect term in, PERT-F-013 enforced",
          "[forcemodel][registry][thirdbody]") {
    int cases = 0;
    double worst_direct = 0.0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        auto plugins = forcemodel::third_bodies(planets_b(), fx::ephemeris(), fx::gm(), fx::leaps(), fx::sources());
        REQUIRE(plugins.has_value());
        REQUIRE(plugins->size() == 7);
        for (const auto& plugin : *plugins) {
            const eph::Body body = plugin->body();
            const Vec3 a_plugin = acc(evaluate(*plugin, t, o.r, o.v));

            // equal to by_body of {Sun, Moon, this body} at this body's entry, 0 ulp
            std::vector<eph::Body> list{eph::Body::Sun, eph::Body::Moon};
            if (body != eph::Body::Sun && body != eph::Body::Moon) list.push_back(body);
            auto parts = thirdbody::Attraction::by_body(frames::Position<frames::Frame::GCRS>{o.r}, list, fx::ephemeris(), fx::gm(), t, fx::leaps());
            REQUIRE(parts.has_value());
            bool found = false;
            for (const auto& q : *parts)
                if (q.body == body) {
                    found = true;
                    CHECK(a_plugin.x == q.a_m_s2.x);
                    CHECK(a_plugin.y == q.a_m_s2.y);
                    CHECK(a_plugin.z == q.a_m_s2.z);
                }
            REQUIRE(found);

            // equal to pair_direct, the expression as written, in metres (the test's own crossing), within FMOD-P-6
            auto st = fx::ephemeris().geocentric_state(body, t, fx::leaps());
            auto mu = fx::gm().gm_m3_s2(body);
            REQUIRE(st.has_value());
            REQUIRE(mu.has_value());
            const Vec3 s = 1000.0 * st->position();
            const Vec3 d = s - o.r;
            const double bound = 16.0 * kEps * *mu * (1.0 / (s.norm() * s.norm()) + 1.0 / (d.norm() * d.norm()));
            const Vec3 a_direct_form = thirdbody::Attraction::pair_direct(o.r, s, *mu);
            const double diff = (a_plugin - a_direct_form).norm();
            worst_direct = std::max(worst_direct, diff / bound);
            CHECK(diff <= bound);

            // PERT-A-014: the indirect term is in -- the plugin minus the direct attraction alone is -mu s/|s|^3
            const Vec3 direct_only = (*mu / (d.norm() * d.norm() * d.norm())) * d;
            const Vec3 indirect = (-*mu / (s.norm() * s.norm() * s.norm())) * s;
            CHECK((a_plugin - direct_only - indirect).norm() <= bound);
            ++cases;
        }
    }
    REQUIRE(cases == 28);
    WARN("FMOD-A-004: " << cases << " (point, body) cases; 0 ulp against by_body; worst |plugin - pair_direct| / bound = " << worst_direct);

    // PERT-F-013 and the list's record
    auto no_moon = forcemodel::third_bodies({eph::Body::Sun}, fx::ephemeris(), fx::gm(), fx::leaps());
    REQUIRE(!no_moon.has_value());
    CHECK(no_moon.error().id == "FMOD-F-001");
    CHECK(no_moon.error().message.find("Moon") != std::string::npos);
    auto no_sun = forcemodel::third_bodies({eph::Body::Moon, eph::Body::JupiterBarycentre}, fx::ephemeris(), fx::gm(), fx::leaps());
    REQUIRE(!no_sun.has_value());
    CHECK(no_sun.error().id == "FMOD-F-001");
    CHECK(no_sun.error().message.find("Sun") != std::string::npos);
    auto not_a_body = forcemodel::third_bodies({eph::Body::Sun, eph::Body::Moon, eph::Body::Earth}, fx::ephemeris(), fx::gm(), fx::leaps());
    REQUIRE(!not_a_body.has_value());
    CHECK(not_a_body.error().id == "FMOD-F-001");
    auto ok = forcemodel::third_bodies(planets_b(), fx::ephemeris(), fx::gm(), fx::leaps());
    REQUIRE(ok.has_value());
    const char* names[7] = {"third_body.sun", "third_body.moon", "third_body.mercury", "third_body.venus", "third_body.mars", "third_body.jupiter",
                            "third_body.saturn"};
    for (std::size_t i = 0; i < 7; ++i) CHECK((*ok)[i]->id().name == names[i]);       // the stated list, in order (PERT-R-052)
}

namespace {

/// (10.12) by hand, in metres, general relativity (beta = gamma = 1): the test's own formulas and its own crossing.
Vec3 hand_term(relativity::Term term, const Vec3& r, const Vec3& v, const Vec3& earth_r_m, const Vec3& earth_v_m_s) {
    using relativity::Correction;
    const double c2 = Correction::kC * Correction::kC;
    const double gme = Correction::kGmEarth;
    const double rn = r.norm();
    switch (term) {
        case relativity::Term::Schwarzschild:
            return (gme / (c2 * rn * rn * rn)) * ((4.0 * gme / rn - v.dot(v)) * r + 4.0 * r.dot(v) * v);
        case relativity::Term::LenseThirring: {
            const Vec3 J{0.0, 0.0, Correction::kEarthAngularMomentumPerMass};
            return (2.0 * gme / (c2 * rn * rn * rn)) * ((3.0 / (rn * rn)) * r.dot(J) * r.cross(v) + v.cross(J));
        }
        case relativity::Term::DeSitter: {
            const double R = earth_r_m.norm();
            const Vec3 inner = (-Correction::kGmSun / (c2 * R * R * R)) * earth_r_m;
            const Vec3 omega = earth_v_m_s.cross(inner);
            return 3.0 * omega.cross(v);
        }
    }
    return {};
}

}  // namespace

TEST_CASE("FMOD-A-005: relativity -- 0 ulp against L2, (10.12) by hand in metres, the R3 bands, and the km-slip control", "[forcemodel][registry][relativity]") {
    using relativity::Correction;
    int cases = 0;
    double worst = 0.0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        auto sun = fx::ephemeris().geocentric_state(eph::Body::Sun, t, fx::leaps());
        REQUIRE(sun.has_value());
        const Vec3 earth_r = -1.0 * (1000.0 * sun->position());               // the test's own crossing and sign
        const Vec3 earth_v = -1.0 * (1000.0 * sun->velocity());
        const frames::State<frames::Frame::GCRS> sat{t, odl::km_from_metres(o.r), odl::km_from_metres(o.v)};
        const frames::State<frames::Frame::BCRS> earth{t, -1.0 * sun->position(), -1.0 * sun->velocity()};
        const double c2 = Correction::kC * Correction::kC;
        const double r = o.r.norm(), v = o.v.norm();
        // R3's closed forms for this circular equatorial orbit
        const double a_s = Correction::kGmEarth * (4.0 * Correction::kGmEarth / r - v * v) / (c2 * r * r);
        const double a_lt = 2.0 * Correction::kGmEarth * Correction::kEarthAngularMomentumPerMass * v / (c2 * r * r * r);
        const double a_c = 3.0 * (Correction::kGmSun / (c2 * 1.495978707e11 * 1.495978707e11)) * std::sqrt(Correction::kGmSun / 1.495978707e11) * v;
        for (auto term : {relativity::Term::Schwarzschild, relativity::Term::LenseThirring, relativity::Term::DeSitter}) {
            const forcemodel::Relativity plugin(term, fx::ephemeris(), fx::leaps(), relativity::PpnParameters::general_relativity(), fx::sources());
            const Vec3 a_plugin = acc(evaluate(plugin, t, o.r, o.v));

            // (i) equal to L2 fed the same typed inputs, 0 ulp
            auto l2 = Correction::by_term(sat, earth, relativity::Terms::only(term));
            REQUIRE(l2.has_value());
            REQUIRE(l2->size() == 1);
            CHECK(a_plugin.x == (*l2)[0].a_m_s2.x);
            CHECK(a_plugin.y == (*l2)[0].a_m_s2.y);
            CHECK(a_plugin.z == (*l2)[0].a_m_s2.z);

            // (ii) equal to (10.12) evaluated by hand in metres, within FMOD-P-5
            const Vec3 hand = hand_term(term, o.r, o.v, earth_r, earth_v);
            const double rel = (a_plugin - hand).norm() / a_plugin.norm();
            worst = std::max(worst, rel);
            CHECK(rel <= 64.0 * kEps);

            // (iii) inside R3's per-term closed-form band
            const double a = a_plugin.norm();
            switch (term) {
                case relativity::Term::Schwarzschild: CHECK(a >= a_s * (1.0 - 1e-6)); CHECK(a <= a_s * (1.0 + 1e-6)); break;
                case relativity::Term::LenseThirring: CHECK(a >= a_lt * (1.0 - 1e-6)); CHECK(a <= a_lt * (1.0 + 1e-6)); break;
                case relativity::Term::DeSitter:      CHECK(a >= 0.85 * a_c); CHECK(a <= 1.06 * a_c); break;
            }

            // (iv) the km-slip control: the crossing applied twice, and not at all, on the SATELLITE's state
            for (double slip : {1.0e-3, 1.0e3}) {
                const frames::State<frames::Frame::GCRS> wrong{t, slip * sat.position(), slip * sat.velocity()};
                auto bad = Correction::by_term(wrong, earth, relativity::Terms::only(term));
                REQUIRE(bad.has_value());
                CHECK((a_plugin - (*bad)[0].a_m_s2).norm() / a_plugin.norm() >= 1.0e3 * 64.0 * kEps);
            }
            ++cases;
        }
    }
    REQUIRE(cases == 12);
    WARN("FMOD-A-005: " << cases << " (point, term) cases; 0 ulp against L2; worst relative difference from (10.12) by hand " << worst
         << " (bound 64 eps = " << 64.0 * kEps << ")");
}

TEST_CASE("FMOD-A-006: tides -- ONE plugin, summed once; the independent construction; every model non-zero", "[forcemodel][registry][tides]") {
    int cases = 0;
    double worst_sum = 0.0, worst_independent = 0.0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        const eop::EopRecord rec = fx::record_dxdy_zero(t);
        const Mat3 rp = fx::erfa_matrix(t, rec);
        forcemodel::TidesOptions opts = default_tides(p);
        REQUIRE(opts.ocean_degree >= 2);
        REQUIRE(opts.ocean_degree <= 100);
        const forcemodel::Tides plugin(fx::model(), forcemodel::EarthOrientation::from_record(rec, fx::leaps()), fx::ephemeris(), fx::leaps(), opts);
        const dyn::ForceEvaluation ev = evaluate(plugin, t, o.r, o.v);
        const Vec3 a_plugin = acc(ev);

        // the by-model route sums to the total
        auto bm = plugin.by_model(t, frames::Position<frames::Frame::GCRS>{o.r});
        REQUIRE(bm.has_value());
        REQUIRE(bm->size() == 4);
        Vec3 sum{};
        double sum_abs = 0.0;
        for (const auto& m : *bm) {
            const double n = m.a_m_s2.norm();
            CHECK(n > 0.0);
            CHECK(n < 1.0e-5);
            sum = sum + m.a_m_s2;
            sum_abs += n;
        }
        const double ds = (a_plugin - sum).norm() / (128.0 * kEps * sum_abs);
        worst_sum = std::max(worst_sum, ds);
        CHECK((a_plugin - sum).norm() <= 128.0 * kEps * sum_abs);

        // the independent construction: L2's models by the recipe, the independent chain, the sum once, the field's view synthesis
        auto field = fx::model().conventional(t, true);
        REQUIRE(field.has_value());
        const fx::IndependentTides ind = fx::independent_tides(t, rec, rp, opts, *field);
        const int dmax = ind.total.max_degree();
        std::vector<double> c(gravity::CoefficientSet::index(dmax, dmax) + 1, 0.0), s(c.size(), 0.0);
        for (int n = 0; n <= dmax; ++n)
            for (int m = 0; m <= n; ++m) {
                c[gravity::CoefficientSet::index(n, m)] = ind.total.dc(n, m);
                s[gravity::CoefficientSet::index(n, m)] = ind.total.ds(n, m);
            }
        auto view = gravity::CoefficientView::of(dmax, c, s, ind.total.system());
        REQUIRE(view.has_value());
        auto a_itrs = field->acceleration_of(*view, frames::ItrsPosition{rp.apply(o.r)}, fx::deg(dmax), fx::ord(dmax));
        REQUIRE(a_itrs.has_value());
        const Vec3 a_ref = rp.transpose().apply(a_itrs->metres_per_second_squared());
        const double rel = (a_plugin - a_ref).norm() / a_ref.norm();
        worst_independent = std::max(worst_independent, rel);
        CHECK(rel <= 1.0e-8);

        // the provenance carries every model's record (PERT-R-062)
        REQUIRE(ev.provenance.has_value());
        const std::string& id = ev.provenance->source_id;
        CHECK(id.rfind("forcemodel=tides;", 0) == 0);
        CHECK(id.find("solid Earth tide") != std::string::npos);
        CHECK(id.find("ocean tide (FES2004)") != std::string::npos);
        CHECK(id.find("solid Earth pole tide") != std::string::npos);
        CHECK(id.find("ocean pole tide") != std::string::npos);
        WARN("FMOD-A-006 " << p.name << ": ocean degree " << opts.ocean_degree << ", |a_tides| = " << a_plugin.norm() << " m/s^2 (models: "
             << (*bm)[0].a_m_s2.norm() << " solid, " << (*bm)[1].a_m_s2.norm() << " ocean, " << (*bm)[2].a_m_s2.norm() << " solid pole, "
             << (*bm)[3].a_m_s2.norm() << " ocean pole); by-model sum " << ds << " of its bound; independent construction " << rel << " (bound 1e-8)");
        ++cases;
    }
    REQUIRE(cases == 4);
    WARN("FMOD-A-006: worst by-model-sum / bound " << worst_sum << "; worst independent-construction relative difference " << worst_independent);
}

TEST_CASE("FMOD-A-007: declarations -- names, no parameters, the velocity blocks, the provenance", "[forcemodel][registry]") {
    const auto& p = fx::points()[1];
    const auto t = fx::epoch_of(p);
    const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
    Set s;
    build_full(s, p);

    const auto series = fx::orientation_series();
    const eop::EopRecord rec = fx::record_dxdy_zero(t);
    const auto fixed = forcemodel::EarthOrientation::from_record(rec, fx::leaps());
    CHECK(series.describe() == "series");
    CHECK(fixed.describe() == "fixed record");
    const forcemodel::Gravity g_series(fx::model(), series, fx::deg(8), fx::ord(8));
    const forcemodel::Gravity g_fixed(fx::model(), fixed, fx::deg(8), fx::ord(8));
    CHECK(g_series.describe_orientation() == "series");
    CHECK(g_fixed.describe_orientation() == "fixed record");

    // the expected provenance, key order and hashes
    const std::string egm = std::string("egm2008-coefficients=") + ODL_EGM2008_SHA256;
    const std::string gm_eph = std::string("naif-gm-de440=") + ODL_GM_DE440_SHA256 + ",de440s-spk=" + ODL_DE440S_SHA256;
    const std::string eph_only = std::string("de440s-spk=") + ODL_DE440S_SHA256;
    const std::string tides_hash = egm + ",de440s-spk=" + ODL_DE440S_SHA256 + ",fes2004-ocean-tide=" + ODL_FES2004_SHA256 +
                                   ",desai-ocean-pole-tide=" + ODL_DESAI_SHA256;
    auto keys_of = [](const std::string& id) {
        std::vector<std::string> keys;
        std::stringstream ss(id);
        std::string item;
        while (std::getline(ss, item, ';')) keys.push_back(item.substr(0, item.find('=')));
        return keys;
    };

    struct Row { std::string name; bool velocity_present; std::vector<std::string> keys; std::string sha; };
    const std::vector<Row> rows{
        {"gravity", false, {"forcemodel", "model", "system", "N", "M", "secular_pole", "orientation"}, egm},
        {"third_body.sun", false, {"forcemodel", "body", "gm", "ephemeris"}, gm_eph},
        {"third_body.moon", false, {"forcemodel", "body", "gm", "ephemeris"}, gm_eph},
        {"third_body.jupiter", false, {"forcemodel", "body", "gm", "ephemeris"}, gm_eph},
        {"relativity.schwarzschild", true, {"forcemodel", "term", "ppn", "earth_about_sun"}, eph_only},
        {"relativity.lense_thirring", true, {"forcemodel", "term", "ppn", "earth_about_sun"}, eph_only},
        {"relativity.de_sitter", true, {"forcemodel", "term", "ppn", "earth_about_sun"}, eph_only},
        {"tides", false, {"forcemodel", "orientation", "system", "degree", "models"}, tides_hash},
    };

    // rebuild the plugins singly so that each evaluation is visible; the set above proves they coexist
    std::vector<std::shared_ptr<dyn::Force>> plugins;
    plugins.push_back(make_gravity(series, 36, 36));
    auto tb = forcemodel::third_bodies(planets_b(), fx::ephemeris(), fx::gm(), fx::leaps(), fx::sources());
    REQUIRE(tb.has_value());
    for (auto& f : *tb) plugins.push_back(f);
    for (auto term : {relativity::Term::Schwarzschild, relativity::Term::LenseThirring, relativity::Term::DeSitter})
        plugins.push_back(std::make_shared<forcemodel::Relativity>(term, fx::ephemeris(), fx::leaps(), relativity::PpnParameters::general_relativity(), fx::sources()));
    plugins.push_back(std::make_shared<forcemodel::Tides>(fx::model(), series, fx::ephemeris(), fx::leaps(), default_tides(p)));

    int matched = 0;
    for (const auto& plugin : plugins) {
        CHECK(plugin->consumes().empty());
        for (const auto& row : rows) {
            if (row.name != plugin->id().name) continue;
            ++matched;
            const dyn::ForceEvaluation ev = evaluate(*plugin, t, o.r, o.v);
            CHECK(ev.d_state.d_velocity().has_value() == row.velocity_present);
            if (!row.velocity_present) CHECK(ev.d_state.neglected_velocity_bound_per_s() == 0.0);
            REQUIRE(ev.provenance.has_value());
            CHECK(keys_of(ev.provenance->source_id) == row.keys);
            CHECK(ev.provenance->verified_against_issuer == false);
            CHECK(ev.provenance->source_sha256 == row.sha);
        }
    }
    REQUIRE(matched == 8);

    // a plugin told nothing says so
    const forcemodel::Gravity unsupplied(fx::model(), fixed, fx::deg(4), fx::ord(4), forcemodel::GravityOptions{true, {}});
    const dyn::ForceEvaluation ev0 = evaluate(unsupplied, t, o.r, o.v);
    REQUIRE(ev0.provenance.has_value());
    CHECK(ev0.provenance->source_sha256 == "egm2008-coefficients=unsupplied");

    // FMOD-R-010: two plugins of different degree answer independently when interleaved
    const forcemodel::Gravity g4(fx::model(), fixed, fx::deg(4), fx::ord(4), forcemodel::GravityOptions{true, {}});
    const forcemodel::Gravity g36(fx::model(), fixed, fx::deg(36), fx::ord(36), forcemodel::GravityOptions{true, {}});
    const Vec3 a4 = acc(evaluate(g4, t, o.r, o.v)), a36 = acc(evaluate(g36, t, o.r, o.v));
    for (int k = 0; k < 3; ++k) {
        const Vec3 b36 = acc(evaluate(g36, t, o.r, o.v)), b4 = acc(evaluate(g4, t, o.r, o.v));
        CHECK(b4.x == a4.x); CHECK(b4.y == a4.y); CHECK(b4.z == a4.z);
        CHECK(b36.x == a36.x); CHECK(b36.y == a36.y); CHECK(b36.z == a36.z);
    }
    CHECK((a4 - a36).norm() > 0.0);
}

TEST_CASE("FMOD-A-008: refusals pass through with their own identifier and message", "[forcemodel][registry]") {
    const auto& p = fx::points()[1];
    const auto t = fx::epoch_of(p);
    const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);
    auto passes = [](const auto& source, const odl::Result<dyn::ForceEvaluation, dyn::DynError>& via, const char* plugin) {
        REQUIRE(!via.has_value());
        CHECK(via.error().id == source.id);
        CHECK(via.error().message.find(source.message) != std::string::npos);
        CHECK(via.error().message.find(plugin) != std::string::npos);
    };

    // (a) an epoch the EOP series does not cover: a GPS week well past the file's end
    {
        auto far = odl::time::Epoch::from_gps_week(2700, 0.0);
        REQUIRE(far.has_value());
        auto direct = fx::c04().at(*far, eop::EopPolicy{});
        REQUIRE(!direct.has_value());
        const forcemodel::Gravity g(fx::model(), fx::orientation_series(), fx::deg(8), fx::ord(8), forcemodel::GravityOptions{true, {}});
        passes(direct.error(), g.accel(*far, frames::Position<frames::Frame::GCRS>{o.r}, o.v, params, reg), "gravity");
    }
    // (b) a fixed record whose sub-daily terms were not applied: FRAME-F-003, as frames itself says it
    {
        eop::EopRecord rec = fx::record_dxdy_zero(t);
        rec.subdaily_applied = false;
        auto direct = frames::gcrs_to_itrs(t, rec, fx::leaps());
        REQUIRE(!direct.has_value());
        CHECK(direct.error().id == "FRAME-F-003");
        const forcemodel::Gravity g(fx::model(), forcemodel::EarthOrientation::from_record(rec, fx::leaps()), fx::deg(8), fx::ord(8),
                                    forcemodel::GravityOptions{true, {}});
        passes(direct.error(), g.accel(t, frames::Position<frames::Frame::GCRS>{o.r}, o.v, params, reg), "gravity");
    }
    // (c) the secular-pole override: refused without it (GRAV-F-006), accepted with it and recorded
    {
        const eop::EopRecord rec = fx::record_dxdy_zero(t);
        auto direct = fx::model().conventional(t, false);
        REQUIRE(!direct.has_value());
        CHECK(direct.error().id == "GRAV-F-006");
        const forcemodel::Gravity without(fx::model(), forcemodel::EarthOrientation::from_record(rec, fx::leaps()), fx::deg(8), fx::ord(8),
                                          forcemodel::GravityOptions{false, {}});
        passes(direct.error(), without.accel(t, frames::Position<frames::Frame::GCRS>{o.r}, o.v, params, reg), "gravity");
        const forcemodel::Gravity with(fx::model(), forcemodel::EarthOrientation::from_record(rec, fx::leaps()), fx::deg(8), fx::ord(8),
                                       forcemodel::GravityOptions{true, {}});
        const dyn::ForceEvaluation ev = evaluate(with, t, o.r, o.v);
        REQUIRE(ev.provenance.has_value());
        CHECK(ev.provenance->source_id.find("secular_pole=extrapolated") != std::string::npos);
    }
    // (d) a degree or order beyond the model is not constructible: GRAV-F-004 at the typed factory
    CHECK(!gravity::Degree::of(2191).has_value());
    CHECK(!gravity::Order::of(2160).has_value());
    // (e) the ephemeris refuses an epoch outside its coverage: DE440s ends in 2650; the Sun plugin passes the refusal on
    {
        auto far = odl::time::Epoch::from_two_part_jd(odl::time::TimeScale::TT, 2800000.5, 0.0, fx::leaps());
        REQUIRE(far.has_value());
        auto direct = fx::ephemeris().geocentric_state(eph::Body::Sun, *far, fx::leaps());
        REQUIRE(!direct.has_value());
        const forcemodel::ThirdBody sun(eph::Body::Sun, fx::ephemeris(), fx::gm(), fx::leaps());
        passes(direct.error(), sun.accel(*far, frames::Position<frames::Frame::GCRS>{o.r}, o.v, params, reg), "third_body.sun");
    }
    // (f) the solid tide requested in the tide-free system against the zero-tide field: GRAV-F-008, from the synthesis
    {
        const eop::EopRecord rec = fx::record_dxdy_zero(t);
        forcemodel::TidesOptions to;
        to.solid = true;
        to.solid_pole = false;
        to.solid_target = gravity::TideSystem::TideFree;
        to.extrapolate_secular = true;
        const forcemodel::Tides tides(fx::model(), forcemodel::EarthOrientation::from_record(rec, fx::leaps()), fx::ephemeris(), fx::leaps(), to);
        auto via = tides.accel(t, frames::Position<frames::Frame::GCRS>{o.r}, o.v, params, reg);
        REQUIRE(!via.has_value());
        CHECK(via.error().id == "GRAV-F-008");
        CHECK(via.error().message.find("tides") != std::string::npos);
    }
}

TEST_CASE("FMOD-A-030: the size of the Earth-orientation handling over a 24-hour arc, for the exit gate to quote", "[forcemodel][registry][gravity]") {
    const auto& p = fx::points()[0];
    const auto t0 = fx::epoch_of(p);
    const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
    const auto t_mid = t0.add(odl::time::Duration::from_seconds(12.0 * 3600.0));
    auto mid = fx::c04().at(t_mid, eop::EopPolicy{});
    REQUIRE(mid.has_value());
    const forcemodel::Gravity from_series(fx::model(), fx::orientation_series(), fx::deg(36), fx::ord(36), forcemodel::GravityOptions{true, {}});
    const forcemodel::Gravity from_fixed(fx::model(), forcemodel::EarthOrientation::from_record(*mid, fx::leaps()), fx::deg(36), fx::ord(36),
                                         forcemodel::GravityOptions{true, {}});
    double worst_abs = 0.0, worst_rel = 0.0;
    int n = 0;
    for (int k = 0; k <= 24; ++k) {
        const auto t = t0.add(odl::time::Duration::from_seconds(3600.0 * k));
        const Vec3 a = acc(evaluate(from_series, t, o.r, o.v)), b = acc(evaluate(from_fixed, t, o.r, o.v));
        worst_abs = std::max(worst_abs, (a - b).norm());
        worst_rel = std::max(worst_rel, (a - b).norm() / a.norm());
        ++n;
    }
    REQUIRE(n == 25);
    CHECK(std::isfinite(worst_abs));
    WARN("FMOD-A-030: at the GPS point, Gravity (36, 36) from the series against the same from the record of the arc's middle epoch, 25 epochs over "
         "24 h with the same GCRS position: the largest difference in the GCRS acceleration is " << worst_abs << " m/s^2, " << worst_rel
         << " of |a|; REPORTED, not a criterion (finding F4: the exit gate's fits state this for each plugin)");
}
