// l6_exit_gate.cpp — L6's own exit gate (PLAN.md §3.7): "every format round-trips, and a measurement model returns residual and partials for all three observation types."
// SPEC-measmod.md MEAS-A-102 (the first half, the formats) and MEAS-A-102b (the second half, the three observation types).
//
// Linked here, not inside modules/io/tests or modules/measmod/tests, for the reason tests/l5_exit_gate.cpp states for L5: the property under test spans the layer — every reader and
// writer of `io`, and every model of `measmod` — and is checked once, in one place, on the layer's real inputs.
//
// FIXTURES, with their rank. REAL pinned files where the manifest holds one: the January-2026 LAGEOS-1 normal-point file (CRD), the ten products of the ILRS week 260103 (SP3), SLRF2020 and the
// eccentricity file (SINEX), the ACS3 Horizons table, the ACS3 element set (TLE). PUBLISHED: STR3's own sample element set (TLE) and CPF2's own printed sample (CPF). BUILT AT THE
// DOCUMENTED COLUMNS, not a printed example (as IOFM-A-013f and -013h are, from which they are copied): the IOD line and the ANTEX antenna — the tree pins no real file of either format.
// The three observation types are each made from a real-format source: a CRD normal point, a position from an SP3 record and from a Horizons record, and an angle pair from an IOD line.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "g5_inputs.hpp"

#include <odl/io/antex.hpp>
#include <odl/io/cpf.hpp>
#include <odl/io/crd.hpp>
#include <odl/io/horizons.hpp>
#include <odl/io/iod.hpp>
#include <odl/io/sinex.hpp>
#include <odl/io/sp3.hpp>
#include <odl/io/tle.hpp>
#include <odl/measmod/angles.hpp>
#include <odl/measmod/position.hpp>
#include <odl/measmod/range.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <type_traits>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using Catch::Matchers::WithinAbs;
using odl::Vec3;

namespace {

namespace io = odl::io;

/// The position part (the first three columns) of a partials row: its norm and its dot product with another row's.
double position_part_norm(const std::array<double, 6>& row) { return Vec3{row[0], row[1], row[2]}.norm(); }
double position_part_dot(const std::array<double, 6>& a, const std::array<double, 6>& b) { return Vec3{a[0], a[1], a[2]}.dot(Vec3{b[0], b[1], b[2]}); }

/// The first line of text after the name line of a TLE file, and the one after it.
std::pair<std::string, std::string> tle_lines_of(const std::string& text) {
    const auto lines = g5::text_lines(text);
    std::vector<std::string> kept;
    for (const std::string& l : lines)
        if (!l.empty() && (l[0] == '1' || l[0] == '2') && l.size() >= 69) kept.push_back(l.substr(0, 69));
    REQUIRE(kept.size() == 2);
    return {kept[0], kept[1]};
}

}  // namespace

TEST_CASE("MEAS-A-102  the L6 exit gate, first half: every format round-trips — CRD, IOD, SP3, SINEX, CPF, TLE, ANTEX and Horizons: read(write(read(x))) == read(x) on the real pinned files "
          "and the published or column-verified fixtures, the work done counted",
          "[l6][exitgate][formats]") {
    SECTION("CRD: the real January-2026 LAGEOS-1 normal-point file (807 sessions)") {
        auto a = io::read_crd(slurp(ODL_LAGEOS1_NP_FILE));
        REQUIRE(a.has_value());
        const auto nps = std::count_if(a->ranges.begin(), a->ranges.end(), [](const io::CrdRangeRecord& r) { return r.kind == io::CrdRecordKind::NormalPointRange; });
        CHECK(a->sessions.size() == 807);
        CHECK(nps == 4628);
        auto text = io::write_crd(*a);
        REQUIRE(text.has_value());
        auto b = io::read_crd(*text);
        REQUIRE(b.has_value());
        CHECK(*b == *a);
    }
    SECTION("SP3: the ten real products of the ILRS week 260103 — the primary combination and the nine others the reader needed rules for (IOFM-R-013 .. -016)") {
        struct Src { const char* name; const char* path; std::size_t epochs; };
        const Src sources[] = {{"ilrsa", ODL_ILRS_SP3_FILE, 5040}, {"ilrsb", ODL_SP3_ILRSB_FILE, 5040}, {"asi", ODL_SP3_ASI_FILE, 5041}, {"bkg", ODL_SP3_BKG_FILE, 5040},
                               {"cnes", ODL_SP3_CNES_FILE, 5041}, {"dgfi", ODL_SP3_DGFI_FILE, 5041}, {"esa", ODL_SP3_ESA_FILE, 5041}, {"gfz", ODL_SP3_GFZ_FILE, 5041},
                               {"jcet", ODL_SP3_JCET_FILE, 5040}, {"nsgf", ODL_SP3_NSGF_FILE, 5043}};
        std::size_t total = 0;
        for (const Src& s : sources) {
            INFO(s.name);
            auto a = io::read_sp3(slurp(s.path));
            REQUIRE(a.has_value());
            CHECK(a->epochs.size() == s.epochs);
            total += a->epochs.size();
            auto text = io::write_sp3(*a);
            REQUIRE(text.has_value());
            auto b = io::read_sp3(*text);
            REQUIRE(b.has_value());
            CHECK(*b == *a);
            CHECK(b->epoch_lines_by_fields == 0);          // what was written is in SP3D's own layout, clock and all
            CHECK(b->clock_fields_absent == 0);
        }
        CHECK(total == 50408);
    }
    SECTION("SINEX: SLRF2020 and the eccentricity file, real") {
        for (const char* path : {ODL_SLRF2020_FILE, ODL_SLR_ECC_FILE}) {
            INFO(path);
            auto a = io::read_sinex(slurp(path));
            REQUIRE(a.has_value());
            CHECK_FALSE(a->blocks.empty());
            auto text = io::write_sinex(*a);
            REQUIRE(text.has_value());
            auto b = io::read_sinex(*text);
            REQUIRE(b.has_value());
            CHECK(*b == *a);
        }
    }
    SECTION("TLE: the real ACS3 element set, and STR3's own published sample (satellite 88888)") {
        const auto [real1, real2] = tle_lines_of(slurp(ODL_ACS3_TLE_FILE));
        const std::pair<std::string, std::string> published{"1 88888U          80275.98708465  .00073094  13844-3  66816-4 0    87",
                                                            "2 88888  72.8435 115.9689 0086731  52.6988 110.5714 16.05824518  1058"};
        for (const auto& [l1, l2] : {std::pair<std::string, std::string>{real1, real2}, published}) {
            INFO(l1);
            auto a = io::read_tle(l1, l2);
            REQUIRE(a.has_value());
            auto lines = io::write_tle(*a);
            REQUIRE(lines.has_value());
            auto b = io::read_tle(lines->first, lines->second);
            REQUIRE(b.has_value());
            CHECK(*b == *a);
        }
    }
    SECTION("Horizons: the real ACS3 vector table (5 records)") {
        auto a = io::read_horizons(slurp(ODL_ACS3_HORIZONS_FILE));
        REQUIRE(a.has_value());
        CHECK(a->states.size() == 5);
        auto text = io::write_horizons(*a);
        REQUIRE(text.has_value());
        auto b = io::read_horizons(*text);
        REQUIRE(b.has_value());
        CHECK(*b == *a);
    }
    SECTION("CPF: CPF2's own printed sample") {
        const char* sample =
            "H1 CPF 2 AIU 2005 11 16 4 320 1 gps35\n"
            "H2 9305401 3535 22779 2005 11 15 23 59 47 2005 11 20 23 29 47 900 1 1 0 0 0 1\n"
            "H9\n"
            "10 0 53689 86387.000000 0 -13785362.868 -12150743.695 19043830.747\n"
            "10 0 53690    887.000000 0 -13656536.158 -14288496.731 17628980.237\n";
        auto a = io::read_cpf(sample);
        REQUIRE(a.has_value());
        CHECK(a->positions.size() == 2);
        auto text = io::write_cpf(*a);
        REQUIRE(text.has_value());
        auto b = io::read_cpf(*text);
        REQUIRE(b.has_value());
        CHECK(*b == *a);
    }
    SECTION("IOD: a line built at the documented columns (as IOFM-A-013f)") {
        std::string line(80, ' ');
        auto put = [&](int first, const std::string& s) {
            for (std::size_t i = 0; i < s.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = s[i];
        };
        put(1, "12345 98067A");
        put(17, "2420");
        put(22, "G");
        put(24, "20260115");
        put(32, "2213");
        put(36, "45678");
        put(42, "56");
        put(45, "2");
        put(46, "5");
        put(48, "0917234+123456");
        put(63, "25");
        put(66, "S");
        put(67, "+056");
        put(72, "05");
        put(75, "008500");
        auto a = io::read_iod(line);
        REQUIRE(a.has_value());
        auto text = io::write_iod(*a);
        REQUIRE(text.has_value());
        auto b = io::read_iod(*text);
        REQUIRE(b.has_value());
        CHECK(*b == *a);
    }
    SECTION("ANTEX: an antenna built at the documented columns (as IOFM-A-013h)") {
        const char* fixture =
            "       1.4           G                                      ANTEX VERSION / SYST\n"
            "A                                                           PCV TYPE / REFANT\n"
            "                                                            END OF HEADER\n"
            "                                                            START OF ANTENNA\n"
            "BLOCK IIA           G01                                     TYPE / SERIAL NO\n"
            "   0.0                                                      DAZI\n"
            "   0.0  10.0   5.0                                          ZEN1 / ZEN2 / DZEN\n"
            "     1                                                      # OF FREQUENCIES\n"
            "   G01                                                      START OF FREQUENCY\n"
            "      1.00      2.00    880.00                              NORTH / EAST / UP\n"
            "   NOAZI    1.20    0.90    0.10\n"
            "   G01                                                      END OF FREQUENCY\n"
            "                                                            END OF ANTENNA\n";
        auto a = io::read_antex(fixture);
        REQUIRE(a.has_value());
        CHECK(a->antennas.size() == 1);
        auto text = io::write_antex(*a);
        REQUIRE(text.has_value());
        auto b = io::read_antex(*text);
        REQUIRE(b.has_value());
        CHECK(*b == *a);
    }
}

TEST_CASE("MEAS-A-102b  the L6 exit gate, second half: a measurement model returns residual and partials for all three observation types — a range from a real CRD normal point (1 x 6), an "
          "ephemeris position from a real SP3 record and a real Horizons record (3 x 6), an angle pair from an IOD line, right ascension and declination and azimuth and elevation (2 x 6) — "
          "through the one interface",
          "[l6][exitgate][models]") {
    // ---- the interface's one shape, at compile time (MEAS-R-062) -------------------------------------------------------------------------------------------------------------
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const ModelledRange&>().partials())>, Partials<odl::frames::Frame::GCRS, 1>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const ModelledPosition&>().partials())>, Partials<odl::frames::Frame::GCRS, 3>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const ModelledRaDec&>().partials())>, Partials<odl::frames::Frame::GCRS, 2>>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::declval<const ModelledAzEl&>().partials())>, Partials<odl::frames::Frame::GCRS, 2>>);
    static_assert(Partials<odl::frames::Frame::GCRS, 1>::rows == 1 && Partials<odl::frames::Frame::GCRS, 3>::rows == 3 && Partials<odl::frames::Frame::GCRS, 2>::rows == 2);
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(Partials<odl::frames::Frame::GCRS, 1>{}.d[0])>> == 6);

    const g5::PassChoice& pc = g5::chosen();
    const RangeObservation& obs = pc.observations.front();
    const EarthFixedStation station(obs.site.srp_itrs_m, obs.site.srp_geodetic, g5::orientation());

    // ---- (1) a range: the first normal point of the real pass, the real orbit through the real chain -----------------------------------------------------------------------
    {
        auto m = model_range(obs, station, g5::target());
        REQUIRE(m.has_value());
        CHECK(std::isfinite(m->residual_m()));
        CHECK(m->residual_m() == m->observed_range_m() - m->range_m());
        CHECK(std::abs(m->residual_m()) < 1.0);                       // a plausibility bound, not G5's criterion
        const auto& p = m->partials();
        REQUIRE(p.d.size() == 1);
        REQUIRE(p.d[0].size() == 6);
        for (double v : p.d[0]) CHECK(std::isfinite(v));
        CHECK_THAT(position_part_norm(p.d[0]), WithinAbs(1.0, 1e-3));      // the line of sight, unit to the light-time terms
        CHECK((p.d[0][3] == 0.0 && p.d[0][4] == 0.0 && p.d[0][5] == 0.0));                  // the velocity columns are zero to first order (MEAS-R-060)
        CHECK(m->bounce_tt() != m->transmit_tt());
        WARN("range, from the first normal point of the chosen real pass (" << pc.start_text << " UTC): residual " << m->residual_m() * 1e3 << " mm (observed minus modelled; the omitted solid tide is about -200 mm), "
             "partials row 1 x 6 = (" << p.d[0][0] << ", " << p.d[0][1] << ", " << p.d[0][2] << ", " << p.d[0][3] << ", " << p.d[0][4] << ", " << p.d[0][5] << ")");
    }

    // ---- (2a) a position from a real SP3 record: the observation in the ITRS, the model's target rotated to it by the same chain: the residual is the arithmetic of the round trip -----
    {
        const auto& sp3 = real_sp3();
        const auto& epoch = sp3.epochs[1000];
        auto pobs = position_observation(sp3.header, epoch.epoch, epoch.satellites.front().position, ReferencePoint::CentreOfMass, leaps());
        REQUIRE(pobs.has_value());
        auto m = model_position(*pobs, g5::target(), g5::orientation());
        REQUIRE(m.has_value());
        const Vec3 res = m->residual_m();
        CHECK(res.norm() < 1e-6);
        const auto& p = m->partials();
        REQUIRE(p.d.size() == 3);
        for (std::size_t i = 0; i < 3; ++i) {
            REQUIRE(p.d[i].size() == 6);
            CHECK_THAT(position_part_norm(p.d[i]), WithinAbs(1.0, 1e-12));  // [M 0]: the rows of a rotation are unit vectors
            CHECK((p.d[i][3] == 0.0 && p.d[i][4] == 0.0 && p.d[i][5] == 0.0));
        }
        CHECK_THAT(position_part_dot(p.d[0], p.d[1]), WithinAbs(0.0, 1e-12));
        WARN("position, from the real SP3 record " << 1000 << " (ITRS): residual " << res.norm() << " m (the round trip of the same chain), partials 3 x 6 = [M 0], row norms "
             << position_part_norm(p.d[0]) << ", " << position_part_norm(p.d[1]) << ", " << position_part_norm(p.d[2]));
    }
    // ---- (2b) a position from a real Horizons record (GCRS): against a two-body propagation of the table's first state --------------------------------------------------------
    {
        const auto& table = acs3_table();
        REQUIRE(table.states.size() == 5);
        auto first = position_observation(table.states.front(), ReferencePoint::CentreOfMass, leaps());
        auto later = position_observation(table.states[2], ReferencePoint::CentreOfMass, leaps());
        REQUIRE(first.has_value());
        REQUIRE(later.has_value());
        const TwoBodyTrajectory two_body(first->epoch, first->position_m, odl::metres_from_km(table.states.front().velocity_km_s));
        auto m = model_position(*later, two_body, g5::orientation());
        REQUIRE(m.has_value());
        const double miss = m->residual_m().norm();
        CHECK(std::isfinite(miss));
        CHECK(miss > 1.0);                                             // a different model of the same orbit is not the table
        CHECK(miss < 1.0e6);                                           // and it is still the same orbit
        const auto& p = m->partials();
        REQUIRE(p.d.size() == 3);
        CHECK(p.d[0][0] == 1.0);                                       // the identity in the GCRS
        CHECK(p.d[1][1] == 1.0);
        CHECK(p.d[2][2] == 1.0);
        CHECK(m->applied().frame == PositionFrame::GCRS);
        WARN("position, from the real Horizons record 2 (GCRS) against a two-body propagation of record 0: residual " << miss << " m, partials 3 x 6 = [I 0]");
    }

    // ---- (3) angles from IOD lines: an optical observer at the SLR station's reference point (a test input), LAGEOS-1 through the real orbit and chain --------------------------
    {
        OpticalRegistry sites;
        auto added = sites.add(OpticalStationNumber{9999}, obs.site.srp_geodetic.latitude_rad, obs.site.srp_geodetic.longitude_rad, obs.site.srp_geodetic.height_m,
                               "the Yarragadee SLR reference point of the registry, used as an optical observer: a test input of the exit gate, not a real observer");
        REQUIRE(added.has_value());
        auto iod_at = [&](char format, char epoch_code, const std::string& angles14) {
            std::string line(80, ' ');
            auto put = [&](int first, const std::string& s) {
                for (std::size_t i = 0; i < s.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = s[i];
            };
            put(1, "12345 98067A");
            put(17, "9999");
            put(24, "20260102");
            put(32, "0420");
            put(36, "00000");
            line[44] = format;
            line[45] = epoch_code;
            put(48, angles14);
            put(63, "25");
            auto o = io::read_iod(line);
            if (!o) FAIL("the exit gate's IOD line did not read: " << o.error().id << " " << o.error().message);
            return *o;
        };
        const EphemerisEarthMotion motion(ephemeris_de440s(), leaps());

        auto radec_obs = angle_observation(iod_at('2', '5', "0917234+123456"), sites, AngleReduction::Astrometric, std::nullopt, std::nullopt, leaps());
        REQUIRE(radec_obs.has_value());
        auto rd = model_radec(*radec_obs, station, g5::target(), motion);
        REQUIRE(rd.has_value());
        CHECK(std::isfinite(rd->residual_ra_rad()));
        CHECK(std::isfinite(rd->residual_dec_rad()));
        CHECK(std::abs(rd->residual_ra_rad()) <= std::numbers::pi);
        CHECK(rd->residual_dec_rad() == rd->observed_dec_rad() - rd->dec_rad());
        {
            const auto& p = rd->partials();
            REQUIRE(p.d.size() == 2);
            for (std::size_t i = 0; i < 2; ++i) {
                REQUIRE(p.d[i].size() == 6);
                const double along = position_part_norm(p.d[i]) * rd->applied().range_m;     // rad/m x m: of order 1 (1/cos(dec) for the right ascension)
                CHECK(along > 0.3);
                CHECK(along < 5.0);
                CHECK((p.d[i][3] == 0.0 && p.d[i][4] == 0.0 && p.d[i][5] == 0.0));
            }
            WARN("angles, from an IOD line (format 2, ICRF), Astrometric: residual right ascension " << rd->residual_ra_rad() * 206264.80624709636 << " arcsec, declination "
                 << rd->residual_dec_rad() * 206264.80624709636 << " arcsec (the line's angles are not LAGEOS-1's: only the interface is exercised), partials 2 x 6, range " << rd->applied().range_m / 1e3 << " km");
        }

        auto azel_obs = angle_observation(iod_at('5', ' ', "1803000+450000"), sites, AngleReduction::ApparentRefracted, std::nullopt,
                                          AngleAtmosphere{Atmosphere{1013.25, 15.0, 0.5}, 0.532}, leaps());
        REQUIRE(azel_obs.has_value());
        auto ae = model_azel(*azel_obs, station, g5::target(), g5::orientation());
        REQUIRE(ae.has_value());
        CHECK(std::isfinite(ae->residual_azimuth_rad()));
        CHECK(std::isfinite(ae->residual_elevation_rad()));
        CHECK(ae->residual_elevation_rad() == ae->observed_elevation_rad() - ae->elevation_rad());
        CHECK(ae->elevation_rad() > 0.1);                              // the pass is well above the horizon at 04:20 UTC
        {
            const auto& p = ae->partials();
            REQUIRE(p.d.size() == 2);
            for (std::size_t i = 0; i < 2; ++i) {
                REQUIRE(p.d[i].size() == 6);
                for (double v : p.d[i]) CHECK(std::isfinite(v));
                const double along = position_part_norm(p.d[i]) * ae->applied().range_m;
                CHECK(along > 0.3);
                CHECK(along < 5.0);
            }
            WARN("angles, from an IOD line (format 5, azimuth and elevation), ApparentRefracted: modelled azimuth " << ae->azimuth_rad() * 180.0 / std::numbers::pi << " deg, elevation "
                 << ae->elevation_rad() * 180.0 / std::numbers::pi << " deg, refraction " << ae->applied().refraction_rad * 206264.80624709636 << " arcsec; residual azimuth "
                 << ae->residual_azimuth_rad() * 180.0 / std::numbers::pi << " deg, elevation " << ae->residual_elevation_rad() * 180.0 / std::numbers::pi << " deg, partials 2 x 6");
        }
    }
}
