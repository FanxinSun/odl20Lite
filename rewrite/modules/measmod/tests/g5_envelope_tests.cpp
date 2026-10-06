// g5_envelope_tests.cpp — SPEC-measmod.md §8.9, MEAS-A-101 (the envelope of G5, committed BEFORE the comparison code exists) and MEAS-A-101c (the term functions' closed forms).
//
// The envelope test computes, for the pass the registered rule chose, the centre c_i and half-width w_i of every normal point's envelope and the size of each assembly defect against
// it (model-versus-model differences). It FORMS NO RESIDUAL: the model's answer is read for its geometry and its modelled range; `residual_m()` and `observed_range_m()` are never
// called here, and the observed range is never printed. Its numbers are printed with their terms (the rule: each term printed with its basis).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "g5_envelope.hpp"

#include <cstdio>
#include <sstream>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using namespace odl::measmod::testing::g5;
using Catch::Matchers::WithinAbs;

TEST_CASE("MEAS-A-101c  the envelope's term functions at hand-computed values: the pole tide 25 sin e + 7 cos e mm, the zenith-delay term 3 mm m(e), the gradient term min(50 mm, 2 mm m_g), "
          "the mapping-function bands 3 / 12 / 48 mm, the Earth-orientation term with 30.92 mm per mas, the normal point's own error 3 (c/2) RMS / sqrt(n), and the sign of a range effect",
          "[measmod][g5][termfunctions]") {
    const double deg = std::numbers::pi / 180.0;
    SECTION("the pole tide: 7 mm at the horizon, 25 mm at the zenith, (25 + 7)/sqrt(2) mm at 45 degrees") {
        CHECK_THAT(pole_tide_term_m(0.0), WithinAbs(0.007, 1e-15));
        CHECK_THAT(pole_tide_term_m(90.0 * deg), WithinAbs(0.025, 1e-15));
        CHECK_THAT(pole_tide_term_m(45.0 * deg), WithinAbs(0.02262741699796952, 1e-15));
    }
    SECTION("the zenith-delay term and the mapping-function bands") {
        CHECK_THAT(zenith_delay_term_m(1.0), WithinAbs(0.003, 1e-18));
        CHECK_THAT(zenith_delay_term_m(2.5), WithinAbs(0.0075, 1e-18));
        CHECK_THAT(mapping_function_term_m(20.0 * deg), WithinAbs(0.003, 1e-18));
        CHECK_THAT(mapping_function_term_m(15.001 * deg), WithinAbs(0.003, 1e-18));     // the bands are tested either side of their edges, not on them: 15 * pi / 180 * 180 / pi is not exactly 15
        CHECK_THAT(mapping_function_term_m(14.999 * deg), WithinAbs(0.012, 1e-18));
        CHECK_THAT(mapping_function_term_m(10.001 * deg), WithinAbs(0.012, 1e-18));
        CHECK_THAT(mapping_function_term_m(9.999 * deg), WithinAbs(0.048, 1e-18));
    }
    SECTION("the gradient term: m_g(45 degrees) = 1.4080406249139064 and the 50 mm cap below about 11 degrees") {
        CHECK_THAT(gradient_term_m(45.0 * deg), WithinAbs(0.0028160812498278127, 1e-15));
        CHECK_THAT(gradient_term_m(30.0 * deg), WithinAbs(0.0068545937020212284, 1e-15));
        CHECK_THAT(gradient_term_m(15.0 * deg), WithinAbs(0.02760511006832864, 1e-15));
        CHECK_THAT(gradient_term_m(10.0 * deg), WithinAbs(0.050, 1e-18));
        CHECK_THAT(gradient_term_m(5.0 * deg), WithinAbs(0.050, 1e-18));
    }
    SECTION("the Earth-orientation term: 3 x 14.7 us x 0.465 mm/us = 20.5065 mm; 3 x 0.056 mas x 30.92 mm/mas = 5.19456 mm (the slip's 3 mm/mas would give 0.504 mm)") {
        CHECK_THAT(orientation_ut1_term_m(14.7e-6), WithinAbs(0.020506499999999997, 1e-15));
        CHECK_THAT(orientation_pole_term_m(5.6e-5), WithinAbs(0.00519456, 1e-15));
        CHECK(orientation_pole_term_m(5.6e-5) > 9.0 * 3.0 * 0.056e-3 * 3.0);          // ten times the slipped coefficient's value, within the second digit
    }
    SECTION("the normal point's own error: the RMS is a two-way time, the range is c/2 of it") {
        CHECK_THAT(normal_point_term_m(43.0, 17.0), WithinAbs(0.0046898176512524545, 1e-15));
        CHECK_THAT(normal_point_term_m(43.0, 68.0), WithinAbs(0.5 * 0.0046898176512524545, 1e-15));      // four times the points, half the error
    }
    SECTION("the sign of a range effect: a station raised toward the satellite makes the true range SHORTER, so observed minus modelled is negative") {
        CHECK_THAT(range_effect_of_displacement(Vec3{1.0, 0.0, 0.0}, Vec3{0.2, 0.0, 0.0}), WithinAbs(-0.2, 1e-18));
        CHECK_THAT(range_effect_of_displacement(Vec3{0.0, 0.0, 1.0}, Vec3{0.2, 0.0, 0.0}), WithinAbs(0.0, 1e-18));
        CHECK_THAT(unit_projection_m(Vec3{0.0, 0.0, 1.0}, Vec3{1.0, 2.0, 3.0}, Vec3{1.0, 2.0, 0.0}), WithinAbs(3.0, 1e-18));
    }
}

TEST_CASE("MEAS-A-101  G5's envelope, committed before the comparison code exists: for each normal point of the pass the registered rule chose, the centre (the omitted solid tide's signed range "
          "effect) and the half-width (the worst-case linear sum of twelve unsigned terms), each term printed with its basis, and the size of each assembly defect against the envelope; "
          "no residual is formed",
          "[measmod][g5][envelope]") {
    const PassChoice& pc = chosen();

    // ---- the pass and its inputs are the registered ones ------------------------------------------------------------------------------------------------------------------
    CHECK(pc.start_text == kChosenStart);
    CHECK(pc.normal_points == kChosenNormalPoints);
    CHECK(pc.skipped.empty());                                  // the builder's clause: no session had to be skipped
    REQUIRE(pc.observations.size() == static_cast<std::size_t>(kChosenNormalPoints));
    REQUIRE(pc.records.size() == pc.observations.size());
    for (const RangeObservation& o : pc.observations) CHECK(o.event == EpochEvent::GroundTransmit);

    const Envelope env = build_envelope();
    const std::vector<DefectRow> defects = assembly_defects();
    REQUIRE(env.points.size() == pc.observations.size());

    // ---- the report ------------------------------------------------------------------------------------------------------------------------------------------------------------
    {
        std::ostringstream o;
        o << "G5 ENVELOPE (MEAS-A-101), written to the log before any comparison code exists. Commit statements (the manager's ruling, plan/subplan_L6/L6-4.md):\n"
          << "  the tide helper: modules/measmod/tests/solid_tide.hpp, commit fbb60f4 -- TEST-SIDE ONLY; the model's Applied record still says \"station displacement: not modelled\" (ruling R2)\n"
          << "  the ephemeris the helper reads: de440s (manifest id de440s-spk), Ephemeris::geocentric_state, geometric GCRS positions of the Moon and the Sun at the tag epoch\n"
          << "  the pass-selection rule: commit 0a48ec0, which precedes its application (56e0053)\n"
          << "  the ocean-loading bound, TN36 chapter 7, section 7.1.2: \"Ocean tides cause a temporal variation of the ocean mass distribution and the associated load on the crust and produce "
             "time-varying deformations of the Earth that can reach 100 mm\"\n"
          << "THE PASS (the rule's order, most normal points first):\n";
        for (const std::string& r : pc.ranking) o << "  " << r << "\n";
        o << "  chosen: " << pc.start_text << " UTC, " << pc.normal_points << " normal points, sessions skipped by the builder clause: " << pc.skipped.size() << "\n";
        WARN(o.str());
    }
    {
        std::ostringstream o;
        char b[400];
        o << "THE INPUTS READ (each against what SPEC-measmod §8.9 registered):\n";
        o << "  DHF record covering the pass (pad 7090, target 51): " << env.dhf.line << "\n";
        std::snprintf(b, sizeof b, "    -> range bias %.1f mm, sigma %.1f mm, summed unsigned: %.1f mm (rule 6; NOT applied to any range)\n", env.dhf.value, env.dhf.sigma, env.dhf.value + env.dhf.sigma);
        o << b;
        std::snprintf(b, sizeof b, "  centre-of-mass row (7090, %02d %02d %04d .. %02d %02d %04d, %d nm): %.1f mm; the table's 532 nm entries run %.1f .. %.1f mm; rule 5: C = %.1f mm\n",
                      env.com_row.start_d, env.com_row.start_m, env.com_row.start_y, env.com_row.end_d, env.com_row.end_m, env.com_row.end_y, env.com_row.wavelength_nm, env.com_row.mm,
                      env.com_min_mm, env.com_max_mm, env.com_term_mm);
        o << b;
        std::snprintf(b, sizeof b, "  eccentricity (six determinations of SOD 70900513): scatter up %.1f north %.1f east %.1f mm, norm %.2f mm (rule 7, unprojected)\n", env.ecc_scatter_mm[0],
                      env.ecc_scatter_mm[1], env.ecc_scatter_mm[2], env.ecc_norm_mm);
        o << b;
        std::snprintf(b, sizeof b, "  C04 standard deviations: day 0 x %.6f\" y %.6f\" UT1 %.7f s; day 1 x %.6f\" y %.6f\" UT1 %.7f s -> sigma_UT1 %.2f us, sigma_pole %.4f mas (rule 9, corrected)\n",
                      env.sigmas_day0.x_arcsec, env.sigmas_day0.y_arcsec, env.sigmas_day0.ut1_s, env.sigmas_day1.x_arcsec, env.sigmas_day1.y_arcsec, env.sigmas_day1.ut1_s, env.sigma_ut1_s * 1e6,
                      env.sigma_pole_arcsec * 1e3);
        o << b;
        o << "  the products of the week (coordinate system, agency; time system asserted UTC), and each one's largest |g . (r_X - r_A)| over the pass's points (rule 4):\n";
        const auto& products = sp3_products();
        for (std::size_t x = 1; x < products.size(); ++x) {
            std::snprintf(b, sizeof b, "    %-6s %-6s %-6s  %8.1f mm\n", products[x].ac.c_str(), products[x].coordinate_sys.c_str(), products[x].agency.c_str(), env.orbit_by_product_m[x - 1].second * 1e3);
            o << b;
        }
        std::snprintf(b, sizeof b, "    primary %s: %s %s\n    O = %.1f mm (the maximum over the nine products and the points; factor 1)\n", products.front().ac.c_str(), products.front().coordinate_sys.c_str(),
                      products.front().agency.c_str(), env.orbit_max_m * 1e3);
        o << b;
        WARN(o.str());
    }
    {
        std::ostringstream o;
        char b[600];
        o << "THE ENVELOPE, per normal point (millimetres; c = centre, the signed range effect of the omitted solid tide; terms 1..10 unsigned; w = their sum; the window is [c - w, c + w]):\n";
        o << "   i  UTC s-of-day  elev deg  range km    c      U_h  pole  ocean orbit  CoM   DHF   ecc   ZTD  grad   map   UT1  pole    NP     w   gHat.v m/s\n";
        for (const NpEnvelope& p : env.points) {
            const EnvelopeTerms& t = p.terms;
            std::snprintf(b, sizeof b, "  %2zu  %11.3f  %8.2f  %8.1f %7.1f %6.1f %5.1f %6.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f %6.1f  %9.2f%s\n", p.index, p.seconds_of_day,
                          p.elevation_deg, p.range_m * 1e-3, p.centre_m * 1e3, t.u_h * 1e3, t.pole_tide * 1e3, t.ocean * 1e3, t.orbit * 1e3, t.com * 1e3, t.dhf * 1e3, t.ecc * 1e3, t.ztd * 1e3,
                          t.gradient * 1e3, t.mapping * 1e3, t.eop_ut1 * 1e3, t.eop_pole * 1e3, t.np_own * 1e3, p.w_m * 1e3, p.ghat_dot_station_velocity, p.beyond_mapping_table ? "  (below 6 deg: beyond the mapping table)" : "");
            o << b;
        }
        double wmin = 1e9, wmax = 0.0, emin = 1e9, emax = -1e9, cmin = 1e9, cmax = -1e9;
        for (const NpEnvelope& p : env.points) {
            wmin = std::min(wmin, p.w_m);
            wmax = std::max(wmax, p.w_m);
            emin = std::min(emin, p.elevation_deg);
            emax = std::max(emax, p.elevation_deg);
            cmin = std::min(cmin, p.centre_m);
            cmax = std::max(cmax, p.centre_m);
        }
        std::snprintf(b, sizeof b, "  elevations %.2f .. %.2f deg; w %.1f .. %.1f mm; c %.1f .. %.1f mm\n", emin, emax, wmin * 1e3, wmax * 1e3, cmin * 1e3, cmax * 1e3);
        o << b;
        o << "  PREDICTED before the computation (SPEC-measmod §8.9, second set; predictions, not criteria): elevations 15..70 deg, none below 10; w 210..370 mm (central 270); c within +-300 mm\n";
        WARN(o.str());
    }
    {
        std::ostringstream o;
        char b[700];
        o << "ASSEMBLY DEFECTS against the envelope (model minus model, metres; rho = |defect| / w; seen for certain rho > 2, may be seen 1 < rho <= 2, cannot be seen rho <= 1):\n";
        for (const DefectRow& d : defects) {
            double dmin = 1e30, dmax = 0.0, rmin = 1e30, rmax = 0.0;
            int certain = 0, maybe = 0, cannot = 0, refused = 0;
            std::string cannot_at;
            for (std::size_t i = 0; i < d.delta_m.size(); ++i) {
                switch (classify(d.delta_m[i], env.points[i].w_m)) {
                    case Seen::ForCertain: ++certain; break;
                    case Seen::MayBe: ++maybe; break;
                    case Seen::Cannot: ++cannot; break;
                    case Seen::Refused: ++refused; break;
                }
                if (d.delta_m[i]) {
                    const double a = std::abs(*d.delta_m[i]);
                    dmin = std::min(dmin, a);
                    dmax = std::max(dmax, a);
                    rmin = std::min(rmin, a / env.points[i].w_m);
                    rmax = std::max(rmax, a / env.points[i].w_m);
                    if (a / env.points[i].w_m <= 2.0) cannot_at += (cannot_at.empty() ? "" : ",") + std::to_string(i);
                }
            }
            std::snprintf(b, sizeof b, "  %-46s |defect| %10.4f .. %10.4f m   rho %8.2f .. %8.2f   certain %2d  may %2d  cannot %2d  refused %2d", d.name.c_str(), dmin, dmax, rmin, rmax, certain, maybe, cannot, refused);
            o << b << "\n";
            o << "      how: " << d.how << "\n";
            if (!cannot_at.empty()) o << "      G5 CANNOT BE RELIED ON to see it (rho <= 2) at points: " << cannot_at << "\n";
        }
        WARN(o.str());
    }

    // ---- assertions: the structure, and the defect machinery against closed forms ---------------------------------------------------------------------------------------
    for (const NpEnvelope& p : env.points) {
        INFO("normal point " << p.index);
        CHECK(std::isfinite(p.w_m));
        CHECK(p.w_m > 0.100);                                   // the ocean-loading term alone
        CHECK(std::isfinite(p.centre_m));
        CHECK(std::abs(p.centre_m) < 0.40);                     // the largest solid tide, radial 0.38 m plus horizontal 0.08 m, projected
        CHECK(p.elevation_deg > 0.0);
        CHECK(p.elevation_deg <= 90.0);
        CHECK_THAT(p.terms.sum(), WithinAbs(p.w_m, 1e-15));
    }
    CHECK(env.orbit_max_m > 0.0);                               // the products are not the same orbit
    {
        double biggest = 0.0;
        for (const auto& kv : env.orbit_by_product_m) biggest = std::max(biggest, kv.second);
        CHECK_THAT(biggest, WithinAbs(env.orbit_max_m, 1e-12));
    }

    {   // the target and the station: the model's geometry agrees with the ITRS-only geometry the products live in
        for (std::size_t i = 0; i < pc.observations.size(); ++i) {
            const RangeObservation& obs = pc.observations[i];
            INFO("normal point " << i);
            auto st = target().state_at(obs.epoch);
            REQUIRE(st.has_value());
            const Vec3 r = odl::metres_from_km(st->position());
            const Vec3 v = odl::metres_from_km(st->velocity());
            CHECK(r.norm() > 12.0e6);                           // LAGEOS-1: a = 12 270 km, e = 0.004
            CHECK(r.norm() < 12.5e6);
            CHECK(v.norm() > 5.5e3);                            // sqrt(GM / a) = 5.70 km/s
            CHECK(v.norm() < 5.9e3);
            // the elevation from the ITRS positions alone (no Earth orientation at all): the product's position, the registry's reference point, the geodetic vertical
            auto ra = sp3_position_itrs_m(sp3_products().front(), obs.epoch);
            REQUIRE(ra.has_value());
            const Vec3 d = *ra - obs.site.srp_itrs_m;
            const double lat = obs.site.srp_geodetic.latitude_rad, lon = obs.site.srp_geodetic.longitude_rad;
            const Vec3 up{std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat)};
            const double e_itrs = std::asin(up.dot(d) / d.norm());
            CHECK_THAT(env.points[i].elevation_deg * std::numbers::pi / 180.0, WithinAbs(e_itrs, 1e-4));   // the light-time geometry moves it by < 3e-5 rad
        }
    }
    {   // the centre of the envelope is the same in the ITRS: the tide's range effect does not depend on the frame it is evaluated in
        for (std::size_t i = 0; i < pc.observations.size(); ++i) {
            const RangeObservation& obs = pc.observations[i];
            auto o = orientation().at(obs.epoch);
            REQUIRE(o.has_value());
            auto moon = ephemeris_de440s().geocentric_state(odl::eph::Body::Moon, obs.epoch, leaps());
            auto sun = ephemeris_de440s().geocentric_state(odl::eph::Body::Sun, obs.epoch, leaps());
            REQUIRE(moon.has_value());
            REQUIRE(sun.has_value());
            const Vec3 d_itrs = solid_tide_step1_degree2(obs.site.srp_itrs_m, o->gcrs_to_itrs.apply(odl::metres_from_km(moon->position())),
                                                         o->gcrs_to_itrs.apply(odl::metres_from_km(sun->position())));
            const double c_itrs = range_effect_of_displacement(o->gcrs_to_itrs.apply(env.points[i].ghat), d_itrs);
            CHECK_THAT(c_itrs, WithinAbs(env.points[i].centre_m, 1e-8));
        }
    }
    {   // the defect machinery against first-order closed forms: the centre-of-mass sign is 2 delta; the eccentricity and the station velocity are the projected displacements;
        // the rotation is half the projected velocity times the flight time; the wrong event is the up light time times the range rate; UTC for TT is 69.184 s
        const double com_m = pc.observations.front().com.metres();
        for (std::size_t i = 0; i < pc.observations.size(); ++i) {
            const RangeObservation& obs = pc.observations[i];
            INFO("normal point " << i);
            const NpEnvelope& p = env.points[i];
            auto o = orientation().at(obs.epoch);
            REQUIRE(o.has_value());
            const EarthFixedStation station(obs.site.srp_itrs_m, obs.site.srp_geodetic, orientation());
            auto nominal = model_range(obs, station, target());
            REQUIRE(nominal.has_value());
            // (1) the sign: exactly twice the correction
            REQUIRE(defects[0].delta_m[i].has_value());
            CHECK_THAT(*defects[0].delta_m[i], WithinAbs(2.0 * com_m, 1e-9));
            // (3) the eccentricity: the marker is d away from the reference point; the range changes by -g . d (first order)
            const Vec3 d_ecc = o->gcrs_to_itrs.transpose().apply(obs.site.marker_itrs_m - obs.site.srp_itrs_m);
            REQUIRE(defects[2].delta_m[i].has_value());
            CHECK_THAT(*defects[2].delta_m[i], WithinAbs(-p.ghat.dot(d_ecc), 5e-5));
            // (2) the station velocity: the station sits at -shift from where it should
            auto ref = real_registry().site(obs.site.sod, utc(2015, 1, 1, 0, 0, 0.0));
            REQUIRE(ref.has_value());
            const Vec3 shift = o->gcrs_to_itrs.transpose().apply(obs.site.marker_itrs_m - ref->marker_itrs_m);
            REQUIRE(defects[1].delta_m[i].has_value());
            CHECK_THAT(*defects[1].delta_m[i], WithinAbs(p.ghat.dot(shift), 5e-5));
            CHECK(shift.norm() > 0.1);                          // eleven years of plate motion are not nothing
            CHECK(shift.norm() < 1.5);
            // (7) the rotation: the down leg's end moved by v x ToF, the range is the mean of the legs
            auto sk = station.at(obs.epoch);
            REQUIRE(sk.has_value());
            REQUIRE(defects[6].delta_m[i].has_value());
            const double expected_rotation = 0.5 * p.ghat.dot(sk->velocity_m_s) * nominal->time_of_flight_s();
            CHECK_THAT(*defects[6].delta_m[i], WithinAbs(expected_rotation, 5e-4));
            // (4) the wrong event: the range of the bounce-tagged point is the range one up-light-time EARLIER
            REQUIRE(defects[3].delta_m[i].has_value());
            RangeObservation plus = obs, minus = obs;
            plus.epoch = obs.epoch.add(odl::time::Duration::from_seconds(0.5));
            minus.epoch = obs.epoch.add(odl::time::Duration::from_seconds(-0.5));
            auto rp = model_range(plus, station, target());
            auto rm = model_range(minus, station, target());
            REQUIRE(rp.has_value());
            REQUIRE(rm.has_value());
            const double range_rate = rp->range_m() - rm->range_m();                 // over 1 s
            CHECK_THAT(*defects[3].delta_m[i], WithinAbs(-nominal->applied().up.light_time_s * range_rate, 5e-3));
            // (8) UTC read as TT is an epoch 69.184 s away (37 s of leap seconds + 32.184 s)
            auto tt = Epoch::from_calendar(TimeScale::TT, utc_calendar_of(obs.epoch, 9), leaps());
            REQUIRE(tt.has_value());
            CHECK_THAT(obs.epoch.difference(*tt).to_seconds(), WithinAbs(69.184, 1e-6));
            // (5), (6) the troposphere is the mean delay the model applied, which is positive and metres
            CHECK(defects[5].delta_m[i].value() > 1.0);
            CHECK(defects[5].delta_m[i].value() < 40.0);
            CHECK_THAT(*defects[4].delta_m[i], WithinAbs(-*defects[5].delta_m[i], 1e-15));
        }
    }
}

// ---- MEAS-A-101d (the L7 opening's record correction): the size, at G5's pass, of the backup combination's frame -----------------------------------------------------------------------

namespace {

/// The ITRS Center's 14-parameter model, as the pinned file's own note states it: X_S = X + T + D X + R x X, X in ITRF2020 and X_S in the other frame, each parameter P(t) = P(epoch) + rate (t - epoch).
struct Helmert {
    double epoch = 0.0;
    double t_mm[3] = {0, 0, 0}, d_ppb = 0.0, r_mas[3] = {0, 0, 0};
    double t_rate_mm[3] = {0, 0, 0}, d_rate_ppb = 0.0, r_rate_mas[3] = {0, 0, 0};
};

Helmert parse_itrf2014_row(const std::string& text) {
    const auto lines = g5::text_lines(text);
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        const auto a = g5::split_ws(lines[i]);
        if (a.size() < 9 || a[0] != "ITRF2014") continue;
        const auto b = g5::split_ws(lines[i + 1]);
        REQUIRE(b.size() >= 8);
        REQUIRE(b[0] == "rates");
        Helmert h;
        for (int k = 0; k < 3; ++k) {
            h.t_mm[k] = std::stod(a[1 + static_cast<std::size_t>(k)]);
            h.r_mas[k] = std::stod(a[5 + static_cast<std::size_t>(k)]);
            h.t_rate_mm[k] = std::stod(b[1 + static_cast<std::size_t>(k)]);
            h.r_rate_mas[k] = std::stod(b[5 + static_cast<std::size_t>(k)]);
        }
        h.d_ppb = std::stod(a[4]);
        h.epoch = std::stod(a[8]);
        h.d_rate_ppb = std::stod(b[4]);
        return h;
    }
    FAIL("the pinned transformation file has no ITRF2014 row");
    return {};
}

/// X_S - X (metres) for the position `x_m` at the decimal year `t`.
Vec3 helmert_difference_m(const Helmert& h, double t, const Vec3& x_m) {
    const double dt = t - h.epoch;
    const double mas = 4.84813681109536e-9;                                    // 1 mas in radians
    const Vec3 tr{(h.t_mm[0] + h.t_rate_mm[0] * dt) * 1e-3, (h.t_mm[1] + h.t_rate_mm[1] * dt) * 1e-3, (h.t_mm[2] + h.t_rate_mm[2] * dt) * 1e-3};
    const double d = (h.d_ppb + h.d_rate_ppb * dt) * 1e-9;
    const Vec3 rot{(h.r_mas[0] + h.r_rate_mas[0] * dt) * mas, (h.r_mas[1] + h.r_rate_mas[1] * dt) * mas, (h.r_mas[2] + h.r_rate_mas[2] * dt) * mas};
    return tr + d * x_m + rot.cross(x_m);
}

}  // namespace

TEST_CASE("MEAS-A-101d  the backup combination's frame at G5's pass: the ITRF2014 of its header against the ITRF2020 of SLRF2020, by the ITRS Center's own transformation parameters — its size along "
          "each normal point's line of sight, and the orbit term unchanged by it",
          "[measmod][g5][framediff]") {
    const Helmert h = parse_itrf2014_row(slurp(ODL_ITRF2020_TRANSFO_FILE));
    // the parse reads what the file prints (ITRF2014: -1.4 -0.9 1.4 mm, -0.42 ppb, no rotation, epoch 2015.0; rates 0.0 -0.1 0.2 mm/y, no scale rate, no rotation rate)
    CHECK(h.t_mm[0] == -1.4);
    CHECK(h.t_mm[1] == -0.9);
    CHECK(h.t_mm[2] == 1.4);
    CHECK(h.d_ppb == -0.42);
    CHECK((h.r_mas[0] == 0.0 && h.r_mas[1] == 0.0 && h.r_mas[2] == 0.0));
    CHECK(h.epoch == 2015.0);
    CHECK(h.t_rate_mm[0] == 0.0);
    CHECK(h.t_rate_mm[1] == -0.1);
    CHECK(h.t_rate_mm[2] == 0.2);
    CHECK(h.d_rate_ppb == 0.0);

    // a closed form (the machinery's power): at 2015.0 a satellite on the x axis at LAGEOS's radius, 12 273 km, is displaced by (-1.4 mm - 0.42e-9 x 1.2273e7 m, -0.9 mm, +1.4 mm) = (-6.5547, -0.9, +1.4) mm
    {
        const Vec3 d = helmert_difference_m(h, 2015.0, Vec3{1.2273e7, 0.0, 0.0});
        CHECK_THAT(d.x, WithinAbs(-1.4e-3 - 0.42e-9 * 1.2273e7, 1e-15));
        CHECK_THAT(d.y, WithinAbs(-0.9e-3, 1e-15));
        CHECK_THAT(d.z, WithinAbs(1.4e-3, 1e-15));
        // eleven years on the rates move only Ty and Tz, by -1.1 and +2.2 mm
        const Vec3 d2 = helmert_difference_m(h, 2026.0, Vec3{1.2273e7, 0.0, 0.0});
        CHECK_THAT(d2.y - d.y, WithinAbs(-0.1e-3 * 11.0, 1e-15));
        CHECK_THAT(d2.z - d.z, WithinAbs(0.2e-3 * 11.0, 1e-15));
        CHECK_THAT(d2.x - d.x, WithinAbs(0.0, 1e-15));
    }

    const g5::PassChoice& pc = g5::chosen();
    const auto& products = g5::sp3_products();
    REQUIRE(products.size() == 10);
    REQUIRE(products[1].ac == "ilrsb");
    const g5::Envelope env = g5::build_envelope();
    REQUIRE(env.points.size() == pc.observations.size());

    std::ostringstream o;
    char b[300];
    o << "THE BACKUP COMBINATION'S FRAME AT THE PASS (millimetres): the file's ITRF2014 row (ITRF2020 to ITRF2014; the standard model; P(t) = P(2015.0) + rate x (t - 2015.0)) at each tag's decimal year,\n"
         "applied to the primary product's position of L51 (the SLRF2020 / ITRF2020 frame) and projected on the station-to-satellite unit vector in the ITRS.\n"
         "   i  decimal year   |T(t)|   |D||X|   X_S - X along the line of sight   ilrsb spread   ilrsb corrected to ITRF2020\n";
    double worst_frame = 0.0, worst_bound = 0.0, worst_spread = 0.0, worst_corrected = 0.0;
    for (std::size_t i = 0; i < pc.observations.size(); ++i) {
        const RangeObservation& obs = pc.observations[i];
        auto ra = g5::sp3_position_itrs_m(products[0], obs.epoch);
        auto rb = g5::sp3_position_itrs_m(products[1], obs.epoch);
        REQUIRE(ra.has_value());
        REQUIRE(rb.has_value());
        const Vec3 d = *ra - obs.site.srp_itrs_m;
        const Vec3 ghat = (1.0 / d.norm()) * d;
        const auto cal = g5::utc_calendar_of(obs.epoch);
        const double year = cal.year + (g5::day_of_year(cal.year, cal.month, cal.day) - 1 + (cal.hour * 3600.0 + cal.minute * 60.0 + cal.second) / 86400.0) / 365.0;     // 2026 has 365 days
        const Vec3 frame = helmert_difference_m(h, year, *ra);
        const double along = ghat.dot(frame);
        const Vec3 t_only = helmert_difference_m(h, year, Vec3{0.0, 0.0, 0.0});
        const double bound = t_only.norm() + std::abs(h.d_ppb) * 1e-9 * ra->norm();           // |T(t)| + |D| |X|: nothing the line of sight can pick up exceeds it
        const double spread = std::abs(ghat.dot(*rb - *ra));
        const double corrected = std::abs(ghat.dot((*rb - frame) - *ra));                     // ilrsb's position taken from ITRF2014 to ITRF2020: minus X_S - X
        std::snprintf(b, sizeof b, "  %2zu  %11.5f  %7.3f  %7.3f  %31.3f  %13.3f  %27.3f\n", i, year, t_only.norm() * 1e3, std::abs(h.d_ppb) * 1e-9 * ra->norm() * 1e3, along * 1e3, spread * 1e3, corrected * 1e3);
        o << b;
        INFO("normal point " << i);
        CHECK(std::abs(along) <= bound);
        CHECK(spread + std::abs(along) <= env.orbit_max_m);                                    // the frame difference cannot lift the backup combination to the term
        CHECK(corrected <= env.orbit_max_m);
        worst_frame = std::max(worst_frame, std::abs(along));
        worst_bound = std::max(worst_bound, bound);
        worst_spread = std::max(worst_spread, spread);
        worst_corrected = std::max(worst_corrected, corrected);
    }
    std::snprintf(b, sizeof b, "  worst: the frame difference along the line of sight %.3f mm (its bound |T| + |D||X| %.3f mm); ilrsb's spread %.3f mm as the products stand, %.3f mm with it corrected to ITRF2020; "
                  "the orbit term %.3f mm (nsgf)\n", worst_frame * 1e3, worst_bound * 1e3, worst_spread * 1e3, worst_corrected * 1e3, env.orbit_max_m * 1e3);
    o << b;
    o << "  PREDICTED before the computation (analytic): the difference along the line of sight is at most |T(t)| 4.35 mm + |D||X| 5.15 mm = 9.5 mm, in practice 1 .. 8 mm; ilrsb's spread of 10.4 mm cannot "
         "become more than 19.9 mm; the term, 55.6 mm, is not changed\n";
    WARN(o.str());
    CHECK(worst_spread <= 0.0105);                                                             // the 10.4 mm the envelope record gives for ilrsb
    CHECK(env.orbit_by_product_m[0].first == "ilrsb");
    CHECK_THAT(worst_spread, WithinAbs(env.orbit_by_product_m[0].second, 1e-12));              // and this test's spread is the envelope's own
    CHECK(env.orbit_by_product_m.back().first == "nsgf");
    CHECK_THAT(env.orbit_max_m, WithinAbs(env.orbit_by_product_m.back().second, 1e-12));       // the term is nsgf's
}

