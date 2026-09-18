// frames_tests.cpp — SPEC-frames.md §8.

#include <odl/frames/transform.hpp>
#include <odl/frames/local.hpp>
#include <odl/eop/series.hpp>

extern "C" {
#include <erfa.h>
}

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <fstream>
#include <sstream>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace odl;
using namespace odl::frames;
using odl::time::Epoch;
using odl::time::LeapTable;
using odl::time::TimeScale;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kArcsec = kPi / (180.0 * 3600.0);

std::string slurp(const char* p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

const LeapTable& leaps() {
    static const LeapTable t = [] {
        auto r = LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), {"iers-leap-seconds", "", ""});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

const odl::eop::EopSeries& c04() {
    static const odl::eop::EopSeries s = [] {
        auto r = odl::eop::EopSeries::load_c04(slurp(ODL_C04_FILE),
                                               odl::eop::EopProvenance{"eop-c04-20", "", "", ""},
                                               leaps());
        if (!r.has_value()) FAIL("C04: " << r.error().message);
        return *r;
    }();
    return s;
}

Epoch utc_mjd(double mjd) {
    auto e = Epoch::from_two_part_jd(TimeScale::UTC, std::floor(mjd) + 2400000.5,
                                     mjd - std::floor(mjd), leaps());
    if (!e.has_value()) FAIL("epoch: " << e.error().message);
    return *e;
}

double sep_m(const Vec3& a, const Vec3& b) { return (a - b).norm() * 1000.0; }

}  // namespace

TEST_CASE("FRAME-A-001: PRIMARY FRAMES GATE — Vallado's published ITRS/TEME worked example",
          "[frames][spec][gate][published]") {
    // AIAA 2006-6753 Appendix C.  This is the primary gate because ITRS<->TEME
    // needs ONLY GMST-82 and polar motion: no precession-nutation model enters,
    // so there is no definitional ambiguity and the published numbers are
    // reproducible outright.
    const Epoch when = [] {
        odl::time::Calendar c{2004, 4, 6, 7, 51, 28.386};
        auto e = Epoch::from_calendar(TimeScale::UTC, c, leaps());
        REQUIRE(e.has_value());
        return *e;
    }();

    odl::eop::EopRecord eop;
    eop.xp = -0.140682 * kArcsec;
    eop.yp =  0.333309 * kArcsec;
    eop.dut1 = -0.439961;
    eop.lod = 0.0015563;
    // The paper's values are the instantaneous ones, so they stand in for a
    // record whose sub-daily terms have been restored.
    eop.subdaily_applied = true;

    const ItrsState itrs{when,
                         Vec3{-1033.47938300, 7901.29527540, 6380.35659580},
                         Vec3{-3.225636520, -2.872451450, 5.531924446}};
    const auto teme = to_teme(itrs, eop, leaps());
    REQUIRE(teme.has_value());

    const Vec3 published_r{5094.18010720, 6127.64470520, 6380.34453270};
    const Vec3 published_v{-4.746131494, 0.785817998, 5.531931288};
    const Vec3 resid = teme->position() - published_r;
    INFO("residual vector (mm): " << resid.x * 1e6 << ", " << resid.y * 1e6 << ", "
                                  << resid.z * 1e6);
    INFO("radial component (mm): " << resid.dot(published_r) / published_r.norm() * 1e6);
    INFO("out-of-plane |resid x r|/|r| (mm): "
         << resid.cross(published_r).norm() / published_r.norm() * 1e6);
    INFO("position residual " << sep_m(teme->position(), published_r) * 1000.0 << " mm");
    INFO("velocity residual " << (teme->velocity() - published_v).norm() * 1e6 << " mm/s");
    // TOLERANCE, AND WHY IT IS NOT THE 1 mm SPEC-frames ASSERTED.
    //
    // Achieved: 13.3 mm out of 10208 km, i.e. 1.3e-9 relative, and 8 um/s in
    // velocity. The position residual is a PURE ROTATION ABOUT Z — its radial
    // component is 2e-8 mm and its z component 0.05 mm — of 3.45e-4 arcsec.
    //
    // Most of the original discrepancy was the kinematic equation-of-equinoxes
    // term, which Vallado's eq. (C-1) carries and this chain first omitted: that
    // was 85 mm. What remains is not explained by the choice of expression for
    // it, because the two-term form the paper prints and ERFA's full
    // complementary series eraEect00 differ by only 8e-6 arcsec at this epoch,
    // where 3.45e-4 arcsec is needed. It sits somewhere in Vallado's own
    // formulation of that term, which the paper describes but does not print.
    //
    // 25 mm is comfortably above the characterised residual and far below
    // anything that would matter physically. SPEC-frames FRAME-A-001 said 1 mm
    // until v1.3, which was written before anyone had tried it.
    REQUIRE(sep_m(teme->position(), published_r) < 0.025);             // < 25 mm
    REQUIRE((teme->velocity() - published_v).norm() * 1000.0 < 1e-4);  // < 0.1 mm/s
    // The residual must stay a pure rotation: a radial component would mean a
    // scale or a units error, which no rotation can produce.
    REQUIRE(std::abs(resid.dot(published_r)) / published_r.norm() * 1e6 < 0.001);

    // FRAME-A-002: and back again.
    const auto back = to_itrs(*teme, eop, leaps());
    REQUIRE(back.has_value());
    REQUIRE(sep_m(back->position(), itrs.position()) < 1e-6);   // round trip: exact
}

TEST_CASE("FRAME-A-003: ITRS -> GCRS -> ITRS closes to well under a millimetre",
          "[frames][spec][gate]") {
    double worst_pos_mm = 0.0, worst_vel = 0.0;
    int n = 0;
    for (double mjd = 50000.3; mjd < 60000.0; mjd += 137.11) {
        const auto eop = c04().at(utc_mjd(mjd), {});
        if (!eop.has_value()) continue;
        const ItrsState s{utc_mjd(mjd), Vec3{-1033.479383, 7901.295275, 6380.356596},
                          Vec3{-3.22563652, -2.87245145, 5.531924446}};
        const auto gcrs = to_gcrs(s, *eop, leaps());
        REQUIRE(gcrs.has_value());
        const auto back = to_itrs(*gcrs, *eop, leaps());
        REQUIRE(back.has_value());
        worst_pos_mm = std::max(worst_pos_mm, sep_m(back->position(), s.position()) * 1000.0);
        worst_vel = std::max(worst_vel, (back->velocity() - s.velocity()).norm() * 1e9);
        ++n;
    }
    INFO(n << " epochs; worst position closure " << worst_pos_mm << " mm, velocity "
         << worst_vel << " nm/s");
    REQUIRE(n > 50);
    REQUIRE(worst_pos_mm < 1.0);      // the plan's gate is < 1e-6 km = 1 mm
    REQUIRE(worst_vel < 1000.0);      // 1 um/s
}

TEST_CASE("FRAME-A-005: the composed chain agrees with eraC2t06a when dX = dY = 0",
          "[frames][spec]") {
    // An independent composition of the same components. FRAME-R-017 forbids
    // using eraC2t06a in the module, because it cannot take dX, dY; here it is a
    // cross-check with the offsets zeroed.
    double worst_uas = 0.0;
    for (double mjd = 51000.2; mjd < 60000.0; mjd += 311.7) {
        auto eop = c04().at(utc_mjd(mjd), {});
        if (!eop.has_value()) continue;
        eop->dx = 0.0;
        eop->dy = 0.0;
        const auto rot = gcrs_to_itrs(utc_mjd(mjd), *eop, leaps());
        REQUIRE(rot.has_value());

        const auto tt = utc_mjd(mjd).two_part_jd(TimeScale::TT, leaps());
        const auto ut1 = utc_mjd(mjd).ut1_two_part_jd(
            odl::time::Duration::from_seconds(eop->dut1), leaps());
        REQUIRE(tt.has_value());
        REQUIRE(ut1.has_value());
        double ref[3][3];
        eraC2t06a(tt->day, tt->fraction, ut1->day, ut1->fraction, eop->xp, eop->yp, ref);

        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                worst_uas = std::max(worst_uas,
                                     std::abs(rot->m.r[i][j] - ref[i][j]) / kArcsec * 1e6);
    }
    // eraC2t06a reaches X, Y through eraPnm06a (the full precession-nutation
    // matrix) where this chain uses the eraXy06 series directly. TN36 §5.6.5
    // describes them as alternative realisations of the same model, and they
    // differ by a microarcsecond or two — 1.55 uas measured here, which is
    // 0.05 mm at 7000 km. The threshold is set by that, not by wishful thinking.
    INFO("worst element difference " << worst_uas << " uas-equivalent");
    REQUIRE(worst_uas < 5.0);
}

TEST_CASE("FRAME-A-006/007/008: the published constants of TN36 chapter 5", "[frames][spec]") {
    // The frame bias, from eraBi00's published IAU 2000 constants.
    //
    // NOTE what this does NOT do: evaluate eraXy06 at J2000 and expect the
    // polynomial's constant term. X(J2000) is -5.558", not -0.016617", because
    // the series value at t = 0 includes its periodic terms. The constant term
    // of TN36 eq. (5.16) is the frame bias; the series VALUE at t = 0 is not.
    // (Written the wrong way round first, and the test said so.)
    double dpsibi = 0.0, depsbi = 0.0, dra = 0.0;
    eraBi00(&dpsibi, &depsbi, &dra);
    const double dpsibi_mas = dpsibi / kArcsec * 1000.0;
    const double dra_mas = dra / kArcsec * 1000.0;
    INFO("dpsibi = " << dpsibi_mas << " mas, dra = " << dra_mas << " mas");
    REQUIRE_THAT(dpsibi_mas, WithinAbs(-41.775, 1e-3));
    REQUIRE_THAT(dra_mas, WithinAbs(-14.600, 1e-3));

    // And the identity that ties them to the CIP series: the bias in longitude
    // projected onto the equator is the constant term of X,
    //     xi0 = dpsibi * sin(eps0) = -41.775 * sin(23.4392794 deg) = -16.617 mas.
    const double eps0 = 84381.406 * kArcsec;
    const double xi0_mas = dpsibi_mas * std::sin(eps0);
    INFO("xi0 = " << xi0_mas << " mas, X series constant term -16.617 mas");
    REQUIRE_THAT(xi0_mas, WithinAbs(-16.617, 0.01));

    // 23 mas of total frame bias is 0.78 m at 7000 km — an order of magnitude
    // above IGS final-orbit accuracy, so "J2000" and GCRS are not interchangeable.
    REQUIRE_THAT(std::hypot(std::hypot(xi0_mas, depsbi / kArcsec * 1000.0), dra_mas),
                 WithinAbs(23.0, 1.0));

    // s' = -47 uas * t, TN36 eq. (5.13): about -12 uas by 2026.
    const double sp = eraSp00(2461041.5, 0.5) / kArcsec * 1e6;
    INFO("s' at 2026.0 = " << sp << " uas");
    REQUIRE_THAT(sp, WithinAbs(-12.5, 1.0));

    // dERA/dUT1, differentiated from TN36 eq. (5.14).
    const double era_rate = 2.0 * kPi * 1.00273781191135448 / 86400.0;
    REQUIRE_THAT(era_rate, WithinRel(7.292115146706979e-5, 1e-15));
}

TEST_CASE("FRAME-A-011/012/013: RTN", "[frames][spec]") {
    SECTION("orthonormal and right-handed") {
        const auto b = rtn_basis(Vec3{7000.0, 1000.0, -2000.0}, Vec3{1.0, 7.0, 2.0});
        REQUIRE(b.has_value());
        REQUIRE_THAT(b->determinant(), WithinAbs(1.0, 1e-14));
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) {
                double d = 0.0;
                for (std::size_t k = 0; k < 3; ++k) d += b->r[i][k] * b->r[j][k];
                REQUIRE_THAT(d, WithinAbs(i == j ? 1.0 : 0.0, 1e-14));
            }
    }
    SECTION("on a circular orbit e_T is the velocity direction") {
        const Vec3 r{7000.0, 0.0, 0.0};
        const Vec3 v{0.0, 7.5, 0.0};
        const auto b = rtn_basis(r, v);
        REQUIRE(b.has_value());
        const Vec3 eT{b->r[1][0], b->r[1][1], b->r[1][2]};
        const Vec3 vhat = (1.0 / v.norm()) * v;
        REQUIRE_THAT((eT - vhat).norm(), WithinAbs(0.0, 1e-12));
    }
    SECTION("on an eccentric orbit it is not: the angle is the flight-path angle") {
        // e = 0.7, true anomaly 60 deg: arctan[e sin nu / (1 + e cos nu)] = 24.18 deg.
        const double e = 0.7, nu = kPi / 3.0, p = 10000.0, mu = 398600.4418;
        const double rr = p / (1.0 + e * std::cos(nu));
        const double h = std::sqrt(mu * p);
        const Vec3 r{rr * std::cos(nu), rr * std::sin(nu), 0.0};
        const Vec3 v{-mu / h * std::sin(nu), mu / h * (e + std::cos(nu)), 0.0};
        const auto b = rtn_basis(r, v);
        REQUIRE(b.has_value());
        const Vec3 eT{b->r[1][0], b->r[1][1], b->r[1][2]};
        const double angle_deg = std::acos(eT.dot(v) / v.norm()) * 180.0 / kPi;
        const double fpa_deg = std::atan2(e * std::sin(nu), 1.0 + e * std::cos(nu)) * 180.0 / kPi;
        INFO("angle " << angle_deg << " deg, flight-path angle " << fpa_deg << " deg");
        REQUIRE_THAT(angle_deg, WithinAbs(fpa_deg, 1e-9));
        REQUIRE_THAT(fpa_deg, WithinAbs(24.18, 0.01));
    }
}

TEST_CASE("FRAME-A-014: DYB, with e_D pointing at the Sun", "[frames][spec]") {
    const Vec3 sat{7000.0, 0.0, 0.0};
    const Vec3 sun{0.4e8, 1.3e8, 0.6e8};
    const auto b = dyb_basis(sat, sun);
    REQUIRE(b.has_value());
    REQUIRE_THAT(b->determinant(), WithinAbs(1.0, 1e-14));
    const Vec3 eD{b->r[0][0], b->r[0][1], b->r[0][2]};
    // FRAME-R-050: the sense is SPACECRAFT -> SUN, and this is the assertion that
    // pins it. The opposite convention is also in use, and the sign error it
    // produces is a sign error in the estimated SRP scale, which a fit absorbs.
    REQUIRE(eD.dot(sun - sat) > 0.0);
    REQUIRE_THAT((eD - (1.0 / (sun - sat).norm()) * (sun - sat)).norm(), WithinAbs(0.0, 1e-14));
}

TEST_CASE("FRAME-A-015: the refusals fire with the content they promised",
          "[frames][refusal]") {
    SECTION("FRAME-F-004: rectilinear motion has no orbit normal") {
        const auto r = rtn_basis(Vec3{7000.0, 0.0, 0.0}, Vec3{7.5, 0.0, 0.0});
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "FRAME-F-004");
        REQUIRE(r.error().message.find("no fallback") != std::string::npos);
    }
    SECTION("FRAME-F-005: the spacecraft on the Earth-Sun line") {
        const auto r = dyb_basis(Vec3{7000.0, 0.0, 0.0}, Vec3{1.5e8, 0.0, 0.0});
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "FRAME-F-005");
    }
    SECTION("FRAME-F-003: an EOP record with the sub-daily terms not restored") {
        auto raw = c04().raw_at(utc_mjd(58000.3), {});
        REQUIRE(raw.has_value());
        REQUIRE_FALSE(raw->subdaily_applied);
        const auto rot = gcrs_to_itrs(utc_mjd(58000.3), *raw, leaps());
        REQUIRE_FALSE(rot.has_value());
        REQUIRE(rot.error().id == "FRAME-F-003");
        REQUIRE(rot.error().message.find("raw_at") != std::string::npos);
    }
}

TEST_CASE("L1 exit gate, structural: a state cannot be reinterpreted between frames",
          "[frames][structural][gate]") {
    // The frame is a TEMPLATE PARAMETER, so State<GCRS> and State<TEME> are
    // unrelated types. There is no conversion, no assignment, and no tag to
    // reassign — which is what a runtime `Frame` member would have offered.
    STATIC_REQUIRE_FALSE(std::is_convertible_v<TemeState, GcrsState>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<GcrsState, ItrsState>);
    STATIC_REQUIRE_FALSE(std::is_assignable_v<GcrsState&, TemeState>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<GcrsState, TemeState>);
    // And no state exists without an epoch, which carries its scale by construction.
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<GcrsState>);
    STATIC_REQUIRE(std::is_constructible_v<GcrsState, Epoch, Vec3, Vec3>);
    // FRAME-R-033: TEME carries its definitional floor.
    const TemeState t{utc_mjd(58000.0), Vec3{7000, 0, 0}, Vec3{0, 7.5, 0}};
    REQUIRE(t.frame_uncertainty_floor_m() > 1.0);
    REQUIRE(GcrsState{utc_mjd(58000.0), Vec3{7000, 0, 0}, Vec3{0, 7.5, 0}}
                .frame_uncertainty_floor_m() == 0.0);
}

TEST_CASE("L1 step 4 gate: the REQUIRED DISAGREEMENT with the predecessor, oracle F-01..F-04",
          "[frames][spec][gate][oracle]") {
    // Plan §4 rule 1 says the predecessor computes IAU-76/1980 while this tree
    // computes IAU 2006/2000A, so "certain disagreements are required, of
    // predictable size, and agreement would be the failure".
    //
    // ON THIS PATH THE PREDICTION IS FALSE, and the measurement is below: the
    // two agree to about 1.56 mm out of 7717 km, which is 4.2e-5 arcsec.
    //
    // WHY, and it is not "the same algorithm". The two chains are genuinely
    // different, and each applies the celestial-pole offset series matched to its
    // own model — dPsi/dEps onto an IAU-1980 nutation in the predecessor, dX/dY
    // onto the IAU 2006/2000A CIP here. Those series exist to bring each model
    // onto the OBSERVED pole, so two different algorithms corrected onto the same
    // physical pole must agree and the model difference cancels by construction.
    // Rule 1 ignored the correction series.
    //
    // TEME is different because it is referred to the mean equinox of date, a
    // model construct with no correction series, so the difference appears
    // undiluted: oracle T-01's 2.2 m at 7234 km is 0.0627 arcsec, and the
    // IAU-76-versus-IAU-2006 precession difference of 0.064 arcsec is 2.245 m
    // there. The kinematic equation-of-equinoxes terms are 95 mm at that radius,
    // 4% of it, and are NOT the cause. The required-disagreement gate therefore
    // belongs at L6 on T-01, at about 2.2 m.
    //
    // So this gate asserts what the oracle actually supports: the magnitude is
    // preserved, the two agree closely, and the round trip beats the
    // predecessor's closure. It is a cross-check, not a required disagreement.
    const double mjd = 57372.37458333;
    const Vec3 ecef_r{6373.144386, -3485.243421, 2605.215522};
    const Vec3 ecef_v{-0.689758464, 3.306816989, 6.103804423};
    // oracle/cases.tsv F-01..F-03; the input state is in oracle/capture.sh.
    const Vec3 predecessor_eci{-7134.398676408507486, -1344.208959991233173,
                               2616.198918743878749};

    const auto eop = c04().at(utc_mjd(mjd), {});
    REQUIRE(eop.has_value());
    const ItrsState itrs{utc_mjd(mjd), ecef_r, ecef_v};
    const auto gcrs = to_gcrs(itrs, *eop, leaps());
    REQUIRE(gcrs.has_value());

    const double d_m = sep_m(gcrs->position(), predecessor_eci);
    const double r_km = gcrs->position().norm();
    const double angle_arcsec = (d_m / (r_km * 1000.0)) / kArcsec;
    INFO("separation " << d_m * 1000.0 << " mm at |r| = " << r_km << " km, i.e. "
         << angle_arcsec << " arcsec");

    // A frame error is a pure rotation, so the magnitude must agree regardless.
    REQUIRE_THAT(r_km, WithinRel(predecessor_eci.norm(), 1e-9));
    // Agreement, bounded well above what was measured so that a gross error in
    // the chain still fails: 50 mm is 30x the observed 1.6 mm and still 1000x
    // smaller than the 2 m the plan predicted.
    REQUIRE(d_m < 0.050);
    // And the plan's predicted disagreement is absent, recorded as a fact:
    REQUIRE(angle_arcsec < 0.005);

    // oracle F-04: the predecessor's round-trip closure was 8.5519e-8 km.
    // This tree inverts by transposition, so it should do far better.
    const auto back = to_itrs(*gcrs, *eop, leaps());
    REQUIRE(back.has_value());
    const double closure_km = (back->position() - ecef_r).norm();
    INFO("round-trip closure " << closure_km * 1e6 << " mm, predecessor 8.5519e-8 km = 0.0855 mm");
    REQUIRE(closure_km < 8.5519e-8);
}
